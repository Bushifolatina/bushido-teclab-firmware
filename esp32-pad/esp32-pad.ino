#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char* ssid = "Bedvit";
const char* password = "Jemma2020";

const char* serverUrl = "https://api.teclab.bushidolatina.com/device/hit";

const char* deviceId = "COLP-0001";

const int sensorPin = 34;
const int hitThreshold = 1000;

const unsigned long hitCooldownMs = 1200;
const unsigned long debugInterval = 1000;

bool hitActive = false;
unsigned long lastHitTime = 0;
unsigned long lastDebugTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("BOOT ESP32");

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi CONNECTED");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("Pad Ready");
}

void loop() {
  int value = analogRead(sensorPin);
  unsigned long now = millis();

if (now - lastDebugTime > debugInterval) {
  lastDebugTime = now;
}

  if (value < hitThreshold && !hitActive && (now - lastHitTime > hitCooldownMs)) {
    Serial.println("COLPO RILEVATO");

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure();

      HTTPClient http;
      http.begin(client, serverUrl);
      http.addHeader("Content-Type", "application/json");

      int force = random(300, 2500);

      String payload = "{\"deviceId\":\"" + String(deviceId) + "\",\"force_n\":" + String(force) + ",\"zone\":\"center\"}";

      int httpCode = http.POST(payload);

      Serial.print("HTTP: ");
      Serial.println(httpCode);

      if (httpCode > 0) {
        String response = http.getString();
        Serial.print("Server: ");
        Serial.println(response);
      } else {
        Serial.print("HTTP ERROR: ");
        Serial.println(http.errorToString(httpCode));
      }

      http.end();
    } else {
      Serial.println("WiFi NOT CONNECTED");
    }

    hitActive = true;
    lastHitTime = now;
  }

  if (value > hitThreshold) {
    hitActive = false;
  }

  delay(20);
}