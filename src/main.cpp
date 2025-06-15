#define BLYNK_TEMPLATE_ID "TMPL6CB8Y9XWh"
#define BLYNK_TEMPLATE_NAME "Capstone"
#define BLYNK_AUTH_TOKEN "YvbN0L4RsU3QP1kQECAUoaeskRVENNO-"

#include "model.h" 
#include <tflm_esp32.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "esp_log.h"
#include <eloquent_tinyml.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HTTPClient.h>

const char* ssid = "dehani";
const char* password = "12345678";
const char* lambda_url = "https://e6zpz6oywrqmlwmfmfywklxz340ozafa.lambda-url.us-east-1.on.aws/";
const char* apiKey = "689bd19c7d61c8c14869750378aaa033";
const float latitude = -8.036446882144972;
const float longitude = 112.73506632854176;

float suhu = 0.0, kelembaban = 0.0, angin = 0.0;
float hujan = 0.0;
float temp1, temp2, temp3, hum1, hum2, hum3, soil1, soil2, soil3;
float tempFinal = -1, humFinal = -1, soilFinal = -1;

// Tensor Arena dan interpreter
constexpr int ARENA_SIZE = (8 * 1024);
Eloquent::TF::Sequential<TF_NUM_OPS, ARENA_SIZE> tf;

#define DHTPIN1 27
#define DHTPIN2 26
#define DHTPIN3 25
#define DHTTYPE DHT22

#define SOIL1 33
#define SOIL2 32
#define SOIL3 35
#define PUMP_PIN 21

#ifndef MODEL_H_INCLUDED
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);
#endif

bool isPumpOn = false;
unsigned long pumpStartTime = 0;
unsigned long pumpDurationMs = 0;

BlynkTimer timer;
unsigned long lastTimeSync = 0;
const unsigned long timeResyncInterval = 3600000;

bool syncTimeNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = 0;
  int retry = 0;
  while (time(&now) < 100000 && retry < 10) {
    delay(500);
    retry++;
  }
  if (time(&now) > 100000) {
    lastTimeSync = millis();
    return true;
  } else {
    return false;
  }
}

void swap(float &a, float &b) { float temp = a; a = b; b = temp; }

float voter(float a, float b, float c) {
  float values[3]; int count = 0;
  if (!isnan(a)) values[count++] = a;
  if (!isnan(b)) values[count++] = b;
  if (!isnan(c)) values[count++] = c;

  if (count == 3) {
    if (values[0] > values[1]) swap(values[0], values[1]);
    if (values[1] > values[2]) swap(values[1], values[2]);
    if (values[0] > values[1]) swap(values[0], values[1]);
    return values[1];
  } else if (count == 2) {
    return (values[0] + values[1]) / 2.0;
  } else if (count == 1) {
    return values[0];
  } else {
    return NAN;
  }
}

String safeJsonEntry(String key, float value, int decimals = 1) {
  if (isnan(value)) return "\"" + key + "\":null";
  else return "\"" + key + "\":" + String(value, decimals);
}

void ambilDataCuaca() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude) +
                 "&lon=" + String(longitude) + "&appid=" + apiKey + "&units=metric";
    http.begin(url);
    http.setTimeout(5000);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        suhu = doc["main"]["temp"];
        kelembaban = doc["main"]["humidity"];
        angin = doc["wind"]["speed"];
        hujan = doc.containsKey("rain") && doc["rain"].containsKey("1h") ? doc["rain"]["1h"] : 0;

        Serial.println("\n=== Data Cuaca dari API ===");
        Serial.print("Suhu: "); Serial.println(suhu);
        Serial.print("Kelembaban: "); Serial.println(kelembaban);
        Serial.print("Angin: "); Serial.println(angin);
        Serial.print("Hujan: "); Serial.println(hujan);
      }
    }
    http.end();
  }
}

float norm(float x, float minVal, float maxVal) {
  float n = (x - minVal) / (maxVal - minVal);
  return constrain(n, 0.0, 1.0);
}

void readSensorsAndPredict() {
  temp1 = dht1.readTemperature() + 0.1;
  temp2 = dht2.readTemperature() + 0.6;
  temp3 = dht3.readTemperature() + 0.6;

  hum1 = dht1.readHumidity() - 14.1;
  hum2 = dht2.readHumidity() - 13.3;
  hum3 = dht3.readHumidity() - 5.6;

  int soilRaw1 = analogRead(SOIL1);
  int soilRaw2 = analogRead(SOIL2);
  int soilRaw3 = analogRead(SOIL3);
  soil1 = (soilRaw1 < 100) ? NAN : (float)(4095 - soilRaw1) / 4095.0 * 100.0;
  soil2 = (soilRaw2 < 100) ? NAN : (float)(4095 - soilRaw2) / 4095.0 * 100.0;
  soil3 = (soilRaw3 < 100) ? NAN : (float)(4095 - soilRaw3) / 4095.0 * 100.0;

  tempFinal = voter(temp1, temp2, temp3);
  humFinal = voter(hum1, hum2, hum3);
  soilFinal = voter(soil1, soil2, soil3);

  if (isnan(tempFinal) || isnan(humFinal) || isnan(soilFinal)) return;

  Serial.println("\n=== DATA SENSOR & MODEL ===");
  Serial.printf("Temp1: %.2f, Temp2: %.2f, Temp3: %.2f, TempFinal: %.2f\n", temp1, temp2, temp3, tempFinal);
  Serial.printf("Hum1: %.2f, Hum2: %.2f, Hum3: %.2f, HumFinal: %.2f\n", hum1, hum2, hum3, humFinal);
  Serial.printf("Soil1: %.2f, Soil2: %.2f, Soil3: %.2f, SoilFinal: %.2f\n", soil1, soil2, soil3, soilFinal);

  float input[TF_NUM_INPUTS] = {
    norm(suhu, 25.55, 30.45),
    norm(kelembaban, 94, 97),
    norm(angin, 0.57, 0.8),
    norm(hujan, 0.0, 3.65),
    norm(tempFinal, 25.07, 30.5),
    norm(humFinal, 58.3, 85.85),
    norm(soilFinal, 21.78, 47.61)
  };

  if (!tf.predict(input).isOk()) {
    Serial.println(tf.exception.toCString());
    return;
  }

  float durasi = tf.output(0);
  Serial.print("\nPrediksi durasi penyiraman: ");
  Serial.print(durasi);
  Serial.println(" detik");

  Blynk.virtualWrite(V10, tempFinal);
  Blynk.virtualWrite(V11, humFinal);
  Blynk.virtualWrite(V12, soilFinal);
  Blynk.virtualWrite(V15, durasi);

  pumpDurationMs = durasi * 1000;
  digitalWrite(PUMP_PIN, HIGH);
  pumpStartTime = millis();
  isPumpOn = true;

}

void kirimDataKeLambda() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    syncTimeNTP();
    return;
  }

  char timeBuffer[30];
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(lambda_url);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += safeJsonEntry("rawsuhu1", temp1) + ",";
    jsonData += safeJsonEntry("rawsuhu2", temp2) + ",";
    jsonData += safeJsonEntry("rawsuhu3", temp3) + ",";
    jsonData += safeJsonEntry("finalsuhu", tempFinal) + ",";
    jsonData += safeJsonEntry("rawhum1", hum1) + ",";
    jsonData += safeJsonEntry("rawhum2", hum2) + ",";
    jsonData += safeJsonEntry("rawhum3", hum3) + ",";
    jsonData += safeJsonEntry("finalhum", humFinal) + ",";
    jsonData += safeJsonEntry("rawsoil1", soil1, 2) + ",";
    jsonData += safeJsonEntry("rawsoil2", soil2, 2) + ",";
    jsonData += safeJsonEntry("rawsoil3", soil3, 2) + ",";
    jsonData += safeJsonEntry("finalsoil", soilFinal, 2) + ",";
    jsonData += safeJsonEntry("suhuA", suhu, 2) + ",";
    jsonData += safeJsonEntry("humidA", kelembaban, 2) + ",";
    jsonData += safeJsonEntry("anginA", angin, 2) + ",";
    jsonData += safeJsonEntry("hujanA", hujan, 2);
    jsonData += "}";

    Serial.println("\n=== Kirim Data ke Lambda ===");
    Serial.println(jsonData);

    int httpCode = http.POST(jsonData);
    if (httpCode > 0) {
      Serial.print("Data terkirim ke Lambda, HTTP code: ");
      Serial.println(httpCode);
      String payload = http.getString();
      Serial.print("Response: ");
      Serial.println(payload);
    } else {
      Serial.print("Gagal kirim data ke Lambda, error: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end();

  } else {
    Serial.println("WiFi tidak terkoneksi, tidak bisa kirim data ke Lambda");
  } 
  }

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  tf.setNumInputs(TF_NUM_INPUTS);
  tf.setNumOutputs(TF_NUM_OUTPUTS);
  tf.resolver.AddFullyConnected();

  while (!tf.begin(modelANN).isOk()) {
    Serial.println(tf.exception.toCString());
    delay(1000);
  }

  Serial.println("\nModel siap dipakai!");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(1000);

  syncTimeNTP();
  dht1.begin(); dht2.begin(); dht3.begin();

  timer.setInterval(120000L, ambilDataCuaca);         
  timer.setInterval(120000L, readSensorsAndPredict);  
  timer.setInterval(120000L, kirimDataKeLambda);      
}

void loop() {
  Blynk.run();
  timer.run();

  if (millis() - lastTimeSync >= timeResyncInterval) {
    syncTimeNTP();
  }

    if (isPumpOn && millis() - pumpStartTime >= pumpDurationMs) {
    digitalWrite(PUMP_PIN, LOW);
    isPumpOn = false;
    Serial.println("Penyiraman selesai.");
  }
}