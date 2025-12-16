# 💡 IoT Smart Light Dashboard

------------------------------------------------------------------

Dashboard berbasis web untuk memantau dan mengontrol sistem Penerangan Jalan Umum (PJU) Pintar berbasis IoT (ESP32). Proyek ini menggunakan **Firebase Realtime Database** untuk sinkronisasi data sensor dan kontrol lampu secara *real-time*.

## ✨ Fitur Utama

* **Real-time Monitoring:** Memantau intensitas cahaya (Lux), status hujan, dan kepadatan lalu lintas.
* **Dual Mode Control:**
    * **Otomatis:** Lampu menyesuaikan sendiri berdasarkan sensor.
    * **Manual:** Kontrol intensitas lampu (PWM) menggunakan slider via dashboard.
* **Power Estimator:** Estimasi penggunaan daya (Watt) berdasarkan output lampu.
* **Log Aktivitas:** Tabel riwayat data sensor yang terupdate otomatis.
* **Sistem Login:** Keamanan akses dashboard menggunakan Firebase Authentication.

## 📂 Struktur Folder

Repository ini dipisahkan menjadi dua bagian utama untuk memudahkan pengembangan dan kerapian kode:

root/
├── firmware/           # Kode C++ untuk ESP32 (Arduino IDE)
│   ├── ESP32-IoT-FlashCode.ino
│   └── esp_config.h
│
├── web-dashboard/      # Kode Interface Website
│   ├── dashboard.html
│   ├── analytics.html
│   ├── settings.html
│   └── config.js
│
├── circuit-schematic-diagram.png # Diagram Wiring untuk ESP32
│
└── README.md           # Dokumentasi Proyek

## 🚀 Cara Instalasi & Menjalankan

Karena proyek ini menggunakan **ES Modules** (`import/export`), Anda **tidak bisa** membukanya hanya dengan double-click file HTML. Anda harus menjalankannya menggunakan local server.

### 1. Clone atau Download Repository
Download source code proyek ini ke komputer Anda.

### 2. Konfigurasi Firebase (PENTING!)
Agar dashboard dapat terhubung ke database Anda sendiri:

1.  Cari file bernama `config.js` di dalam folder proyek.
2.  Buka file `config.js` tersebut, lalu masukkan API Key dari Firebase Console Anda:

    ```javascript
    export const firebaseConfig = {
        apiKey: "MASUKKAN_API_KEY_ANDA_DISINI",
        authDomain: "project-id.firebaseapp.com",
        databaseURL: "[https://project-id.firebaseio.com](https://project-id.firebaseio.com)",
        projectId: "project-id",
        appId: "app-id"
    };
    ```
    > ⚠️ **Catatan:** File `config.js` sudah dimasukkan ke dalam `.gitignore` agar kunci rahasia Anda tidak ikut ter-upload ke GitHub publik.

### 3. Konfigurasi Keamanan (esp_config.h)
Agar password WiFi dan API Key tidak bocor ke publik, kami menggunakan file terpisah:

1.  Masuk ke folder firmware/.
2.  Buka file `esp_config.h` tersebut, lalu masukkan WIFI Information dan API Key dari Firebase Console Anda:

Buka file tersebut dan isi dengan data Anda:
// firmware/esp_config.h
#define SECRET_WIFI_SSID "NAMA_WIFI_ANDA"
#define SECRET_WIFI_PASS "PASSWORD_WIFI_ANDA"

#define SECRET_API_KEY   "API_KEY_FIREBASE"
#define SECRET_DB_URL    "URL_DATABASE_FIREBASE"

#define SECRET_EMAIL     "admin@project.com" // Email User Auth
#define SECRET_PASS      "admin123"          // Password User Auth

### 4. Wiring (Skema Kabel)
Lihat pada file circuit-schematic-diagram.png untuk lebih detail
Komponen	    | Pin ESP32
LED 1 (PWM)	    |   GPIO 16
LED 2 (PWM)	    |   GPIO 17
Buzzer	        |   GPIO 4
Ultrasonic Trig	|   GPIO 19
Ultrasonic Echo	|   GPIO 18
Sensor Hujan	|   GPIO 35
Sensor MQ2	    |   GPIO 34
BH1750 (SDA)	|   GPIO 21
BH1750 (SCL)	|   GPIO 22

### 5. Menjalankan Dashboard
Gunakan ekstensi **Live Server** di VS Code atau server lokal lainnya (seperti Python SimpleHTTPServer atau Node http-server).

* **Via VS Code:** Klik kanan pada file `dashboard.html` lalu pilih **"Open with Live Server"**.
* Akses di browser: `http://127.0.0.1:5500/dashboard.html`

## 🔐 Akun Login (Demo)

Jika Anda menggunakan database default atau ingin mencoba tampilan UI, gunakan kredensial berikut (pastikan akun ini sudah dibuat di Firebase Authentication menu):

| Field | Value |
| :--- | :--- |
| **Email** | `admin@gmail.com` |
| **Password** | `admin123` |

## 🛠️ Teknologi yang Digunakan

* **Frontend:** HTML5, CSS3 (Custom Design), JavaScript (ES6 Modules).
* **Backend:** Firebase Realtime Database & Firebase Authentication.
* **Library:** Chart.js (untuk visualisasi grafik).
* **Hardware (Target):** ESP32, Sensor LDR, Sensor Hujan, & LED Driver (PWM).

---
CREATED BY: Azkal Azkiya Alfiandri, 
            Khrisna Wahyu Wibisono, 
            Lutfia Rahmah Tunnisa
DATE: December 2025
© 2025 IoT Smart Light Project.