#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Bedvit";
const char* password = "Jemma2020";

const char* serverUrl = "https://api.teclab.bushidolatina.com/sessions/c7785909-daee-4073-895a-751b24b7c790/hit";

const int sensorPin = 34;
const int hitThreshold = 1000;

bool hitActive = false;

void setup() {
  Serial.begin(115200);
  randomSeed(micros());
  delay(1000);

  Serial.println("");
  Serial.println("Booting ESP32...");

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("FSR + WiFi hit detection ready");
}

void loop() {
  int value = analogRead(sensorPin);

  Serial.print("Sensor value: ");
  Serial.println(value);

  if (value < hitThreshold && !hitActive) {
    Serial.println("COLPO RILEVATO");

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      int force = random(300, 2500);
      String jsonPayload = "{\"force_n\":" + String(force) + ",\"zone\":\"center\"}";

      int httpCode = http.POST(jsonPayload);

      Serial.print("HTTP code: ");
      Serial.println(httpCode);

      if (httpCode > 0) {
        String payload = http.getString();
        Serial.print("Server response: ");
        Serial.println(payload);
      } else {
        Serial.print("HTTP request failed: ");
        Serial.println(http.errorToString(httpCode));
      }

      http.end();
    } else {
      Serial.println("WiFi NOT connected");
    }

    hitActive = true;
  }

  if (value > hitThreshold) {
    hitActive = false;
  }

  delay(20);
}