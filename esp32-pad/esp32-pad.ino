void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 OK");
}

void loop() {
  delay(1000);
  Serial.println("Alive...");
}