/**
 * Arduino BLE server test — exactly the classic Arduino-ESP32 BLE example
 * pattern (BLEDevice / BLEServer). Uses the built-in BLE library (Bluedroid
 * host stack). If THIS boots and advertises on the board, the radio works
 * and we can build the full map-render receiver on top of it.
 */
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define SERVICE_UUID "5a7e1000-2b2f-4f66-9f9a-5c0f8e1a2b3c"
#define CHAR_UUID    "5a7e1001-2b2f-4f66-9f9a-5c0f8e1a2b3c"

static int writeCount = 0;

class CharCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    std::string val = c->getValue();
    writeCount++;
    Serial.printf("WROTE[%d] %d bytes\n", writeCount, (int)val.size());
    Serial.flush();
  }
};

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Arduino BLE starting...");
  Serial.flush();

  BLEDevice::init("EINK-MAP");

  BLEServer *server = BLEDevice::createServer();
  BLEService *service = server->createService(SERVICE_UUID);
  BLECharacteristic *chr = service->createCharacteristic(
      CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  chr->setCallbacks(new CharCB());
  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("Advertising as EINK-MAP (Arduino BLE)");
  Serial.flush();
}

void loop() {
  delay(1000);
}
