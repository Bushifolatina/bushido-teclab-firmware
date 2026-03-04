#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Bedvit";
const char* password = "Jemma2020";

const char* serverUrl = "https://api.teclab.bushidolatina.com/health";

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println("Booting ESP32...");

  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    http.begin(serverUrl);

    int httpCode = http.GET();

    Serial.print("HTTP code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {

      String payload = http.getString();

      Serial.print("Response: ");
      Serial.println(payload);

    }

    http.end();

  } else {

    Serial.println("WiFi NOT connected");

  }

  delay(5000);

}