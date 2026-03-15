#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// --------------------
// WiFi / Backend
// --------------------
const char* ssid = "Bedvit";
const char* password = "Jemma2020";
const char* serverUrl = "https://api.teclab.bushidolatina.com/device/hit";

const char* deviceId = "COLP-0001";
const char* bleDeviceName = "BushidoPad Pro";

// --------------------
// Sensore
// --------------------
const int sensorPin = 34;
const int hitThreshold = 1000;

const unsigned long hitCooldownMs = 1200;
const unsigned long debugInterval = 1000;

bool hitActive = false;
unsigned long lastHitTime = 0;
unsigned long lastDebugTime = 0;

// --------------------
// BLE UUID custom
// --------------------
#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define STATUS_CHAR_UUID    "19b10001-e8f2-537e-4f6c-d104768a1214"
#define HIT_CHAR_UUID       "19b10002-e8f2-537e-4f6c-d104768a1214"

BLEServer* pServer = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pHitCharacteristic = nullptr;

bool bleClientConnected = false;
int totalHits = 0;

// --------------------
// BLE Callbacks
// --------------------
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleClientConnected = true;
    Serial.println("BLE client CONNECTED");
    if (pStatusCharacteristic) {
      pStatusCharacteristic->setValue("connected");
      pStatusCharacteristic->notify();
    }
  }

  void onDisconnect(BLEServer* pServer) override {
    bleClientConnected = false;
    Serial.println("BLE client DISCONNECTED");

    // Riavvia advertising
    BLEDevice::startAdvertising();

    if (pStatusCharacteristic) {
      pStatusCharacteristic->setValue("idle");
    }
  }
};

void setupBLE() {
  BLEDevice::init(bleDeviceName);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pStatusCharacteristic = pService->createCharacteristic(
    STATUS_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pStatusCharacteristic->addDescriptor(new BLE2902());
  pStatusCharacteristic->setValue("idle");

  pHitCharacteristic = pService->createCharacteristic(
    HIT_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pHitCharacteristic->addDescriptor(new BLE2902());
  pHitCharacteristic->setValue("0");

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE READY");
  Serial.print("BLE Name: ");
  Serial.println(bleDeviceName);
}

void connectWiFi() {
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
}

void sendHitToBackend(int forceValue, const String& zoneValue) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi NOT CONNECTED");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{\"deviceId\":\"" + String(deviceId) +
    "\",\"force_n\":" + String(forceValue) +
    ",\"zone\":\"" + zoneValue + "\"}";

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
}

void notifyBleHit(int forceValue) {
  Serial.println("BLE HIT SENT");
  totalHits++;

  if (pStatusCharacteristic) {
    pStatusCharacteristic->setValue("hit");
    pStatusCharacteristic->notify();
  }

  if (pHitCharacteristic) {
    String hitPayload =
      String("{\"deviceId\":\"") + deviceId +
      "\",\"force_n\":" + String(forceValue) +
      ",\"hits\":" + String(totalHits) + "}";

    pHitCharacteristic->setValue(hitPayload.c_str());
    pHitCharacteristic->notify();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("BOOT ESP32");

  connectWiFi();
  setupBLE();

  Serial.println("Pad Ready");
}

void loop() {
  int value = analogRead(sensorPin);
  unsigned long now = millis();

  if (now - lastDebugTime > debugInterval) {
    lastDebugTime = now;
    Serial.print("Sensor: ");
    Serial.println(value);
  }

  if (value < hitThreshold && !hitActive && (now - lastHitTime > hitCooldownMs)) {
    Serial.println("COLPO RILEVATO");

    int force = random(300, 2500);
    String zone = "center";

    sendHitToBackend(force, zone);
    notifyBleHit(force);

    hitActive = true;
    lastHitTime = now;
  }

  if (value > hitThreshold) {
    hitActive = false;
  }

  delay(20);
}