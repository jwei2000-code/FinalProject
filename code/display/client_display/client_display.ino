#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <SwitecX25.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

static BLEUUID serviceUUID("30e2772d-6dda-4baa-a391-ec111e9b1a84");
static BLEUUID    charUUID("0e26f354-8aae-46ea-9dce-195ce2227fad");

#define STEPS           630
#define NOT_FOCUSED_POS 30
#define FOCUSED_POS     600
#define COIL1 4
#define COIL2 5
#define COIL3 6
#define COIL4 7
#define LED_PIN  D9
#define NUM_LEDS 7

SwitecX25 motor(STEPS, COIL1, COIL2, COIL4, COIL3);
Adafruit_NeoPixel ring(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
static BLEScan* pBLEScan;

volatile int focusScore = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastScanStart = 0;
int lastMotorPos = NOT_FOCUSED_POS;

inline int clampPos(int p) { return constrain(p, 0, STEPS - 1); }

void pulseLEDs(uint8_t r, uint8_t g, uint8_t b) {
  float s = sin(millis() / 500.0f);
  uint8_t brightness = (uint8_t)((s + 1.0f) * 127.0f);
  for (int i = 0; i < NUM_LEDS; i++)
    ring.setPixelColor(i, ring.Color((r*brightness)/255, (g*brightness)/255, (b*brightness)/255));
  ring.show();
}

void solidLEDs(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++)
    ring.setPixelColor(i, ring.Color(r, g, b));
  ring.show();
}

static void notifyCallback(BLERemoteCharacteristic* p, uint8_t* pData, size_t length, bool isNotify) {
  focusScore = String((char*)pData).substring(0, length).toInt();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* p) {}
  void onDisconnect(BLEClient* p) {
    connected = false;
    focusScore = 0;
  }
};

bool connectToServer() {
  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  pClient->connect(myDevice);
  pClient->setMTU(517);
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (!pRemoteService) { pClient->disconnect(); return false; }
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (!pRemoteCharacteristic) { pClient->disconnect(); return false; }
  if (pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  connected = true;
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      pBLEScan->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

void startScan() {
  pBLEScan->clearResults();
  pBLEScan->start(5, false);
  lastScanStart = millis();
}

void setup() {
  Serial.begin(115200);
  ring.begin(); ring.setBrightness(50); ring.clear(); ring.show();

  motor.setPosition(0);
  for (int i = 0; i < 6000; i++) { motor.update(); delayMicroseconds(800); }
  motor.setPosition(clampPos(NOT_FOCUSED_POS));
  for (int i = 0; i < 3000; i++) { motor.update(); delayMicroseconds(800); }

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  startScan();
}

void loop() {
  motor.update();

  unsigned long now = millis();

  if (doConnect) {
    doConnect = false;
    if (connectToServer()) {
      Serial.println("Connected!");
    } else {
      Serial.println("Failed, retrying...");
      startScan();
    }
  }

  if (!connected && (now - lastScanStart >= 6000)) {
    startScan();
  }

  int targetPos = (focusScore >= 30) ? clampPos(FOCUSED_POS) : clampPos(NOT_FOCUSED_POS);
  if (targetPos != lastMotorPos) {
    motor.setPosition(targetPos);
    lastMotorPos = targetPos;
  }

  if (now - lastLEDUpdate >= 30) {
    lastLEDUpdate = now;
    if (focusScore >= 30) pulseLEDs(0, 255, 0);
    else solidLEDs(0, 0, 255);
  }
}