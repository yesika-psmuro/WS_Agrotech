#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>

// ---- KONFIGURASI WIFI & THINGSPEAK ----
const char* ssid = "UGMURO-IoT";
const char* password = "Esteh5000";

const char* thingSpeakApiKey = "M0I2JLKS09IMQ8L7"; // Ganti dengan API Key ThingSpeak
const char* thingSpeakUrl = "http://api.thingspeak.com/update";

// ---- KONFIGURASI PIN & OBJEK ----
#define DHTPIN 4
#define DHTTYPE DHT21
#define RELAY1_PIN 26
#define RELAY2_PIN 25
#define RELAY3_PIN 17
const int soilPin = 36;

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);
AsyncWebServer server(80);

// ---- VARIABEL GLOBAL ----
float suhu, kelembapanUdara;
int soilValue, kelembapanTanah;

// ---- SETUP ----
void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(3, 0); lcd.print("Selamat Datang!");
  lcd.setCursor(0, 1); lcd.print("WS Agroteknologi IoT");
  lcd.setCursor(3, 3); lcd.print("-- UG MURO --");
  delay(5000); lcd.clear(); delay(2000);

  pinMode(soilPin, INPUT);
  dht.begin();
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected. IP: " + WiFi.localIP().toString());

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Kontrol relay via web
  server.on("/fan", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      digitalWrite(RELAY1_PIN, state == "1" ? HIGH : LOW);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing state");
    }
  });

  server.on("/pump", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      digitalWrite(RELAY3_PIN, state == "1" ? HIGH : LOW);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing state");
    }
  });

  // Endpoint data sensor (JSON dengan nama field baru)
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"suhu\":" + String(suhu, 1) + ",";
    json += "\"kelembapanUdara\":" + String(kelembapanUdara, 1) + ",";
    json += "\"kelembapanTanah\":" + String(kelembapanTanah);
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

// ---- FUNGSI KIRIM THINGSPEAK ----
void sendToThingSpeak(float suhu, float kelembapanUdara, int kelembapanTanah) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(thingSpeakUrl) +
                 "?api_key=" + thingSpeakApiKey +
                 "&field1=" + String(suhu) +
                 "&field2=" + String(kelembapanUdara) +
                 "&field3=" + String(kelembapanTanah);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.println("ThingSpeak Response Code: " + String(httpCode));
    } else {
      Serial.println("Failed to send data to ThingSpeak.");
    }

    http.end();
  } else {
    Serial.println("WiFi not connected. Can't send data to ThingSpeak.");
  }
}

// ---- LOOP UTAMA ----
void loop() {
  suhu = dht.readTemperature();
  kelembapanUdara = dht.readHumidity();
  soilValue = analogRead(soilPin);
  kelembapanTanah = map(soilValue, 2048, 0, 0, 100);
  kelembapanTanah = constrain(kelembapanTanah, 0, 100);

  // Tampilkan ke LCD dengan label baru
  lcd.setCursor(5, 0); lcd.print("Monitoring");
  lcd.setCursor(0, 1); lcd.print("Suhu : "); lcd.setCursor(8, 1); lcd.print(suhu); lcd.setCursor(17, 1); lcd.print("C");
  lcd.setCursor(0, 2); lcd.print("K.Udara: "); lcd.setCursor(8, 2); lcd.print(kelembapanUdara); lcd.setCursor(17, 2); lcd.print("%");
  lcd.setCursor(0, 3); lcd.print("K.Tanah: "); lcd.setCursor(9, 3); lcd.print("     "); lcd.setCursor(9, 3); lcd.print(kelembapanTanah); lcd.setCursor(17, 3); lcd.print("%");

  // Otomatisasi relay
  if (suhu > 24 && kelembapanUdara > 70)
    digitalWrite(RELAY1_PIN, HIGH);
  else
    digitalWrite(RELAY1_PIN, LOW);

  if (kelembapanTanah < 30)
    digitalWrite(RELAY3_PIN, HIGH);
  else
    digitalWrite(RELAY3_PIN, LOW);

  // Kirim data ke ThingSpeak
  sendToThingSpeak(suhu, kelembapanUdara, kelembapanTanah);

  delay(15000); // delay 15 detik sesuai batas ThingSpeak
}
