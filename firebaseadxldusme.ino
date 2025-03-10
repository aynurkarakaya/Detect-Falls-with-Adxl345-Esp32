
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>

#define WIFI_SSID "EVWİFİ" // Wi-Fi SSID'nizi buraya yazın
#define WIFI_PASSWORD "ucak89012" // Wi-Fi şifrenizi buraya yazın
#define FIREBASE_HOST "/" // Firebase Realtime Database URL'si
#define FIREBASE_AUTH "" // Firebase Authentication Token

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// ADXL345 Sensör Objesi
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// İvme değişim eşik değeri
const float threshold = 2.00; // m/s^2
// Düşme algılama süresi (ms)
const unsigned long fallDuration = 30;

// Sabit değer sınırları
const float zStableMin = 7.5;
const float zStableMax = 8.5;
const float xyStableMin = -0.5;
const float xyStableMax = 1.0;

// NTP sunucusu ve saat dilimi
const char* ntpServer = "pool.ntp.org"; // NTP sunucusu
const long gmtOffset_sec = 3 * 3600;    // Türkiye saat farkı (GMT+3)
const int daylightOffset_sec = 0;      // Yaz saati farkı (yok)

void setup() {
  Serial.begin(115200); // Seri haberleşme başlat
  
  // Wi-Fi bağlantısı
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Wi-Fi'ye bağlanılıyor...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi bağlandı!");

  // NTP sunucusundan zaman ayarı
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

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
  static sensors_event_t previousEvent; // Önceki ölçüm
  sensors_event_t currentEvent;        // Şimdiki ölçüm
  static unsigned long fallStartTime = 0; // Düşme süresinin başlangıcı
  static bool falling = false; // Düşme durum bayrağı
  static bool fallDetected = false; // Düşme algılama durumu

  // Şimdiki ivme ölçümünü al
  accel.getEvent(&currentEvent);

  // İki ölçüm arasındaki farkları hesapla
  float deltaX = abs(currentEvent.acceleration.x - previousEvent.acceleration.x);
  float deltaY = abs(currentEvent.acceleration.y - previousEvent.acceleration.y);
  float deltaZ = abs(currentEvent.acceleration.z - previousEvent.acceleration.z);

  // Firebase'e gönderilecek değerler
  float xValue = currentEvent.acceleration.x;
  float yValue = currentEvent.acceleration.y;
  float zValue = currentEvent.acceleration.z;

  // Eşik değer kontrolü (tüm eksenlerde değişim olmalı)
  if (deltaX > threshold && deltaY > threshold && deltaZ > threshold) {
    if (!falling) {
      // Düşme yeni başlamışsa zamanı kaydet
      fallStartTime = millis();
      falling = true;
    } else if (millis() - fallStartTime > fallDuration) {
      // Eşik değer süre boyunca aşıldıysa düşme algılandı
      Serial.println("Düşme Algılandı!");

      // Türkiye saatini al
      time_t now;
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Zaman alınamadı!");
        return;
      }
      char timeString[25];
      strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);

      // Firebase'e düşme bilgisi gönder
      Firebase.setBool(firebaseData, "/fall_detected", true); // Düşme durumu
      Firebase.setString(firebaseData, "/fall_time", timeString);

      fallDetected = true;
      falling = false; // Bayrağı sıfırla
    }
  } else {
    falling = false; // Eşik aşılmadıysa düşme durumunu sıfırla
    if (fallDetected) {
      // Düşme durumu resetlenir
      Firebase.setBool(firebaseData, "/fall_detected", false);
      fallDetected = false;
    }
  }

  // Z ekseni kontrolü (2.12 veya 7.5 ile 8.5 arasında olduğunda sıfırla) 2.12 offset, gürültü
  if ((zValue >= zStableMin && zValue <= zStableMax) || zValue == 2.12) {
    zValue = 0.0; // Z değeri bu aralıktaysa sıfırlanır
  }

  // Firebase'e sensör değerlerini gönder
  Firebase.setFloat(firebaseData, "/acceleration/x", xValue);
  Firebase.setFloat(firebaseData, "/acceleration/y", yValue);
  Firebase.setFloat(firebaseData, "/acceleration/z", zValue);

  // Seri monitöre sensör değerlerini yazdır
  Serial.print("X: ");
  Serial.print(xValue);
  Serial.print(" m/s^2, Y: ");
  Serial.print(yValue);
  Serial.print(" m/s^2, Z: ");
  Serial.print(zValue);
  Serial.println(" m/s^2");

  // Şimdiki ölçümü bir sonraki döngü için önceki olarak kaydet
  previousEvent = currentEvent;

  delay(100); // 100 ms bekleme
}
