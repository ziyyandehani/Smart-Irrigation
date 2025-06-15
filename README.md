# Smart-Irrigation
Sistem irigasi cerdas berbasis AI yang ditanam di perangkat embedded dan terhubung ke cloud dengan toleransi kesalahan sensor.

## Fitur Utama
📊 Mengambil data dari sensor lokal (suhu, kelembaban udara, kelembaban tanah).
🌤️ Mengambil data cuaca dari API (suhu, kelembaban, angin, curah hujan).
🧠 Model Artificial Neural Network (ANN) untuk memprediksi durasi penyiraman.
🚿 Mengontrol pompa air otomatis via relay berdasarkan output model.
🔄 Pengambilan dan pengiriman data secara periodik.
☁️ Penyimpanan data ke AWS Lambda + PostgreSQL di AWS RDS.
📱 Monitoring real-time via Blynk Dashboard.

## Teknologi yang Digunakan
ESP32
Sensor DHT22 + YL-69 (dengan TMR untuk fault tolerance)
TensorFlow & TensorFlow Lite
AWS EC2, Lambda, PostgreSQL
Blynk IoT
Python (data preprocessing dan training)
Excel untuk dataset awal

## Cara Kerja Singkat
ESP32 membaca data sensor setiap 10 detik.
Data cuaca diambil dari API setiap 10 menit.
Model AI memprediksi durasi penyiraman.
Jika durasi > 0, pompa aktif selama durasi tersebut.
Semua data dikirim ke server AWS (via Lambda) setiap menit.

## Dataset & Model
Dataset: kombinasi data sensor + data cuaca + durasi penyiraman (target)
Model: ANN dengan 7 input & 1 output (durasi penyiraman dalam detik)

## Catatan
Model dikonversi ke C header dengan everywhereml di file ipynb pada model/
Model disisipkan langsung di ESP32 (model.h)
Disarankan menggunakan PlatformIO karena Arduino IDE tidak dapat digunakan karena library tflm bertabrakan dengan library bawaan board ESP dari Arduino.

--------------------------------------------------------------------------------------------------------------------------------------------------------
## Struktur
- `model/Model_Training.ipynb` : Notebook pelatihan model ANN menggunakan TensorFlow
- `dataset/Dataset_FIX.xlsx` : Dataset hasil pengukuran lapangan sebanyak 78 data berlabel
- `src/main.cpp` : Program utama untuk ESP32, menggunakan output model AI
- `include/model.h` : File header berisi model TensorFlow Lite (.tflite) yang dikonversi ke array
- `lib/` : Library yang dibutuhkan

## Cara Pakai
1. Jalankan `Model_Training.ipynb` untuk melatih model baru
2. Konversi model ke TFLite lalu salin ke `model.h`
3. Upload ke ESP32

=========================================================================================================================================================
## Author
Ziyyan Dehani Safti Ajly
