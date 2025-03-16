#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <FirebaseESP32.h>
#include <WiFi.h>

// Wi-Fi ve Firebase bilgileri
#define WIFI_SSID "BELIZE" // Wi-Fi SSID'nizi buraya yazın
#define WIFI_PASSWORD "bilecikbelize" // Wi-Fi şifrenizi buraya yazın
#define FIREBASE_HOST "" // Firebase Realtime Database URL'si
#define FIREBASE_AUTH "" // Firebase Authentication Token



// Firebase nesneleri
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// ADXL345 sensör nesnesi
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Sabitler
const float zThreshold = 2.00;          // Z ekseni ani değişim eşiği
const float zStableMin = 7.5;           // Z sabit alt sınırı
const float zStableMax = 8.5;           // Z sabit üst sınırı
const float xyStableMin = -0.5;         // X ve Y sabit alt sınırı
const float xyStableMax = 1.0;          // X ve Y sabit üst sınırı
const unsigned long stableDuration = 50000; // Sabit durma süresi (ms)

// Değişkenler
bool fallDetected = false;              // Düşme algılandı mı?
unsigned long stableStartTime = 0;      // Sabit durma başlangıç zamanı

void setup() {
  Serial.begin(115200);

  // Wi-Fi bağlantısı
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Wi-Fi'ye bağlanılıyor...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi bağlandı!");

  // Firebase yapılandırması
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&firebaseConfig, &firebaseAuth);

  // ADXL345 başlatma
  if (!accel.begin()) {
    Serial.println("ADXL345 bulunamadı! Bağlantıları kontrol edin.");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_16_G); // Hassasiyet aralığı 16G
  Serial.println("ADXL345 başarıyla başlatıldı!");
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);

  // Sensör değerlerini oku
  float xValue = event.acceleration.x;
  float yValue = event.acceleration.y;
  float zValue = event.acceleration.z;

  // Z ekseninde ani bir değişim kontrolü
  if (abs(zValue) > zThreshold && !fallDetected) {
    Serial.println("Ani Z değişimi tespit edildi!");
    fallDetected = true; // Düşme algılaması başlat
    stableStartTime = 0; // Sabit durma başlangıcını sıfırla
  }

  // Eğer düşme algılandıysa sabit durma kontrolü yap
  if (fallDetected) {
    if ((zValue >= zStableMin && zValue <= zStableMax) &&
        (xValue >= xyStableMin && xValue <= xyStableMax) &&
        (yValue >= xyStableMin && yValue <= xyStableMax)) {
      if (stableStartTime == 0) {
        stableStartTime = millis(); // Sabit durma başlangıç zamanını ayarla
      } else if (millis() - stableStartTime >= stableDuration) {
        // Belirtilen süre boyunca sabit durumdaysa düşme algılandı
        Serial.println("Düşme Algılandı ve Sabit Durma Tespit Edildi!");
        Firebase.setString(firebaseData, "/fall_detected", "Düşme Algılandı ve Sabitlenme Tespit Edildi!");
        fallDetected = false; // Düşme durumunu sıfırla
      }
    } else {
      // Sabit durma bozulduysa başlangıç zamanını sıfırla
      stableStartTime = 0;
    }
  }

  // Firebase'e sensör değerlerini gönder
  Firebase.setFloat(firebaseData, "/acceleration/x", xValue);
  Firebase.setFloat(firebaseData, "/acceleration/y", yValue);
  Firebase.setFloat(firebaseData, "/acceleration/z", (zValue >= zStableMin && zValue <= zStableMax) ? 0.0 : zValue);

  // Seri monitöre sensör değerlerini yazdır
  Serial.print("X: ");
  Serial.print(xValue);
  Serial.print(" m/s^2, Y: ");
  Serial.print(yValue);
  Serial.print(" m/s^2, Z: ");
  Serial.print(zValue);
  Serial.println(" m/s^2");

  delay(100); // 100 ms bekleme
}
