/*
  PROJECT: SMART CITY LIGHT & MONITORING SYSTEM 
  ------------------------------------------------------------------
  Sistem PJU Pintar berbasis IoT dengan keamanan data dan multitasking.
  
  FITUR UTAMA:
  1. 🔐 Security (Confidentiality): Autentikasi User Email/Password (Bukan Anonymous).
  2. ⚡ Multitasking (FreeRTOS): 
     - Core 0: Pembacaan sensor intensif (Ultrasonic, MQ2, Hujan) tanpa blocking.
     - Core 1: Komunikasi WiFi, Firebase, dan Logika Kontrol Utama.
  3. 📡 Availability & Reliability: Strategi Auto-Reconnect WiFi & Token Refresh.
  4. 💡 Adaptive Lighting:
     - Mode Otomatis: Menyesuaikan kecerahan berdasarkan Lux, Hujan, & Traffic.
     - Mode Darurat: Lampu MAX + Buzzer jika terdeteksi asap/api (MQ2).
     - Mode Manual: Kontrol PWM via Web Dashboard.
  5. ☁️ Realtime Database: Sinkronisasi data sensor & log aktivitas ke Firebase.
  
  HARDWARE:
  - ESP32 Dev Module
  - Sensor: BH1750 (Lux), HC-SR04 (Ultrasonic), Rain Sensor, MQ2 (Gas/Asap).
  - Actuator: LED Driver (PWM), Buzzer.

  CREATED BY: Azkal Azkiya Alfiandri, 
              Khrisna Wahyu Wibisono, 
              Lutfia Rahmah Tunnisa
  DATE: December 2025
*/

#include <Wire.h>
#include <BH1750.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// --- PENTING: LIBRARY TAMBAHAN UNTUK TOKEN (SECURITY) ---
#include <addons/TokenHelper.h> 
#include <addons/RTDBHelper.h>

// ==========================================
// 1. KONFIGURASI RAHASIA 
// --- PENTING: EDIT KONFIGURASI WIFI DAN DATABASE PADA FILE esp_config.h ---
// ==========================================
#include "esp_config.h" // 
#define WIFI_SSID     SECRET_WIFI_SSID
#define WIFI_PASSWORD SECRET_WIFI_PASS

#define API_KEY       SECRET_API_KEY
#define DATABASE_URL  SECRET_DB_URL

#define USER_EMAIL    SECRET_EMAIL
#define USER_PASSWORD SECRET_PASS

// ==========================================
// 2. DEFINISI PIN & VARIABEL
// ==========================================
const int PIN_LED = 16;       // LED 1
const int PIN_LED2 = 17;      // LED 2
const int PIN_HUJAN = 35;     // A0 Rain Sensor
const int PIN_TRIG = 19;      // TRIG ULTRASONIC
const int PIN_ECHO = 18;      // ECHO ULTRASONIC
const int PIN_MQ2 = 34;       // Sensor Asap
const int PIN_BUZZER = 4;     // Alarm

// Setting Logika
const int FADE_SPEED = 40;          // Kecepatan redup (makin besar makin cepat)
const int JARAK_DETEKSI = 8;        // cm (Jarak deteksi kendaraan)
const int MQ2_AMBANG_ATAS = 1800;   // Lampu nyala MAX jika nilai tembus angka ini
const int MQ2_AMBANG_BAWAH = 1500; // Lampu baru normal kembali jika nilai TURUN di bawah ini

// Variabel Shared (Diakses oleh 2 Core Processor)
volatile int shared_jarakCm = 999;
volatile int shared_mq2Value = 0;
volatile bool shared_adaAsap = false;
volatile int shared_vehicleCount = 0;
volatile bool shared_isHujan = false;

// Objek
BH1750 lightMeter;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Variabel Kontrol
bool modeOtomatis = true;
int manualValue = 0;
int targetKecerahan = 0;
int kecerahanSaatIni = 0;
String statusSistem = "Booting...";
unsigned long sendDataPrevMillis = 0;
float lux = 0;

TaskHandle_t TaskSensorHandle;

// ==========================================
// 3. TASK CORE 0: SENSOR READING (Kecepatan Tinggi)
// ==========================================
void TaskSensorCode( void * pvParameters ) {
  bool lastStateGerak = false;
  
  // Setup Pin
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  // MQ2 & Hujan adalah Input Analog, tidak wajib pinMode tapi boleh ada
  pinMode(PIN_MQ2, INPUT); 
  pinMode(PIN_HUJAN, INPUT);

  for(;;) { 
    // ----------------------------------------
    // 1. BACA SENSOR JARAK (Ultrasonic)
    // ----------------------------------------
    digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    
    long duration = pulseIn(PIN_ECHO, HIGH, 15000); 
    int jarak = (duration == 0) ? 999 : (duration * 0.034 / 2);
    shared_jarakCm = jarak; 

    // Logika Traffic Counter
    bool adaGerakan = (jarak > 0 && jarak < JARAK_DETEKSI);
    if (adaGerakan && !lastStateGerak) {
      shared_vehicleCount++;
      //Serial.printf("Traffic: %d\n", shared_vehicleCount);
    }
    lastStateGerak = adaGerakan;

    // ----------------------------------------
    // 2. BACA SENSOR ASAP (DENGAN HISTERESIS)
    // ----------------------------------------
    // Ambil rata-rata 5x bacaan agar lebih stabil (Smoothing)
    long totalMq2 = 0;
    for(int i=0; i<5; i++){
      totalMq2 += analogRead(PIN_MQ2);
      delay(2); // Jeda dikit
    }
    int rataRataMq2 = totalMq2 / 5;
    shared_mq2Value = rataRataMq2;

    // LOGIKA STABIL (ANTI-KEDIP)
    if (rataRataMq2 > MQ2_AMBANG_ATAS) {
      shared_adaAsap = true;  // Asap tebal -> MODE BAHAYA AKTIF
    } 
    else if (rataRataMq2 < MQ2_AMBANG_BAWAH) {
      shared_adaAsap = false; // Udara bersih -> MODE BAHAYA MATI
    }
    // Jika nilai di tengah-tengah (misal 1600), status TIDAK BERUBAH.

    // ----------------------------------------
    // 3. BACA SENSOR LAIN & AKSI
    // ----------------------------------------
    shared_isHujan = (analogRead(PIN_HUJAN) < 2500);

    // Kontrol Buzzer Langsung
    if (shared_adaAsap) digitalWrite(PIN_BUZZER, HIGH);
    else digitalWrite(PIN_BUZZER, LOW);

    // Jeda Task
    vTaskDelay(20 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// 4. SETUP (CORE 1)
// ==========================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Init Hardware
  if (lightMeter.begin()) Serial.println(F("BH1750 OK"));
  pinMode(PIN_HUJAN, INPUT);
  pinMode(PIN_MQ2, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  
  ledcAttach(PIN_LED, 5000, 8);
  ledcAttach(PIN_LED2, 5000, 8);

  // Jalankan Task Sensor di Core 0
  xTaskCreatePinnedToCore(TaskSensorCode, "TaskSensor", 10000, NULL, 1, &TaskSensorHandle, 0);

  // Koneksi WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(300);
  }
  Serial.println("\nConnected!");

  // --- IMPLEMENTASI KEAMANAN (CIA TRIAD) ---
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // 1. CONFIDENTIALITY: Menggunakan User Email/Pass (Bukan Anonim)
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // 2. AVAILABILITY: Token Callback untuk refresh otomatis
  config.token_status_callback = tokenStatusCallback; 
  
  // Setting Timeout (Agar tidak hang jika internet lemot)
  config.timeout.wifiReconnect = 10000;
  config.timeout.socketConnection = 10000;

  // Mulai Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// ==========================================
// 5. LOOP UTAMA (CORE 1): LOGIKA & INTERNET
// ==========================================
void loop() {
  // Hanya jalankan jika Firebase Siap & Token Valid
  if (Firebase.ready()) {
    signupOK = true; // Flag penanda koneksi aman
    
    // Baca Lux (I2C tetap di Core utama agar aman)
    lux = lightMeter.readLightLevel();

    // Ambil data terbaru dari Core 0
    int jarakNow = shared_jarakCm;
    bool asapNow = shared_adaAsap;
    bool hujanNow = shared_isHujan;
    bool gerakNow = (jarakNow < JARAK_DETEKSI);

    // --- A. BACA PERINTAH DARI WEB ---
    if (Firebase.RTDB.getBool(&fbdo, "/control/modeOtomatis")) modeOtomatis = fbdo.boolData();
    if (Firebase.RTDB.getInt(&fbdo, "/control/manualValue")) manualValue = fbdo.intData();
    
    // Cek Reset Counter
    if (Firebase.RTDB.getBool(&fbdo, "/control/resetCount")) {
      if (fbdo.boolData()) {
        shared_vehicleCount = 0;
        Firebase.RTDB.setBool(&fbdo, "/control/resetCount", false);
        Serial.println("Counter Reset by Web");
      }
    }

    // --- B. LOGIKA PENENTUAN CAHAYA ---
    if (modeOtomatis) {
      if (asapNow) {
         statusSistem = "BAHAYA API";
         targetKecerahan = 255; // Nyala Maksimal saat darurat
      }
      else if (lux <= 20) { // MALAM
        statusSistem = "MALAM";
        targetKecerahan = (hujanNow || gerakNow) ? 255 : 100;
        if(hujanNow) statusSistem += " (HUJAN)";
        else if(gerakNow) statusSistem += " (GERAK)";
        else statusSistem += " (SEPI)";
      } 
      else if (lux > 20 && lux <= 200) { // SORE
        statusSistem = "PAGI/SORE";
        targetKecerahan = (hujanNow) ? 150 : (gerakNow ? 120 : 50);
      } 
      else { // SIANG
        statusSistem = "SIANG";
        targetKecerahan = 0;
      }
    } else {
      statusSistem = "MANUAL WEB";
      targetKecerahan = manualValue;
    }

    // --- C. EKSEKUSI PWM (FAST FADING) ---
    if (kecerahanSaatIni < targetKecerahan) {
      kecerahanSaatIni += FADE_SPEED;
      if (kecerahanSaatIni > targetKecerahan) kecerahanSaatIni = targetKecerahan;
    } else if (kecerahanSaatIni > targetKecerahan) {
      kecerahanSaatIni -= FADE_SPEED;
      if (kecerahanSaatIni < targetKecerahan) kecerahanSaatIni = targetKecerahan;
    }
    ledcWrite(PIN_LED, kecerahanSaatIni);
    ledcWrite(PIN_LED2, kecerahanSaatIni);

    // --- D. KIRIM DATA KE DATABASE ---
    if (millis() - sendDataPrevMillis > 2000) {
      sendDataPrevMillis = millis();
      
      FirebaseJson json;
      json.set("lux", lux);
      json.set("jarak", jarakNow);
      json.set("status", statusSistem);
      json.set("led_pwm", kecerahanSaatIni);
      json.set("traffic", shared_vehicleCount);
      json.set("hujan", hujanNow ? "BASAH" : "KERING");
      json.set("asap_val", shared_mq2Value);
      json.set("alert_asap", asapNow ? "BAHAYA" : "AMAN");
      
      // Kirim Data Live
      Firebase.RTDB.setJSON(&fbdo, "/live", &json);
      
      // Simpan Log (Opsional: Bisa dikurangi frekuensinya agar hemat database)
      // Firebase.RTDB.pushJSON(&fbdo, "/logs", &json); 
    }
  }
}