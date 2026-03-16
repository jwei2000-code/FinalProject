#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ── BLE UUIDs ──────────────────────────────────────────────────
#define SERVICE_UUID        "30e2772d-6dda-4baa-a391-ec111e9b1a84"
#define CHARACTERISTIC_UUID "0e26f354-8aae-46ea-9dce-195ce2227fad"

// ── MPU6500 ────────────────────────────────────────────────────
#define MPU_ADDR 0x68

// ── Moving average filter ──────────────────────────────────────
const int FILTER_SIZE = 10;
float gyroReadings[FILTER_SIZE] = {0};
int filterIndex = 0;
float filterSum = 0.0;

// ── BLE globals ────────────────────────────────────────────────
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer)    { deviceConnected = true;  Serial.println("Client connected!"); }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; Serial.println("Client disconnected!"); }
};

// ── MPU helpers (exactly from your working code) ───────────────
void mpuWrite(byte reg, byte val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission(true);
}

byte mpuRead(byte reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

// ── Moving average DSP ─────────────────────────────────────────
float movingAverage(float newVal) {
  filterSum -= gyroReadings[filterIndex];
  gyroReadings[filterIndex] = newVal;
  filterSum += newVal;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;
  return filterSum / FILTER_SIZE;
}

// ── Map smoothed gyro magnitude to focus score 0-100 ──────────
int calcFocusScore(float smoothedMag) {
  const float MAX_MAG = 150.0;
  return (int)(constrain(smoothedMag / MAX_MAG, 0.0, 1.0) * 100.0);
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // 
  
  // ── MPU init (same as your working code) ────────────────────
  Wire.begin(6, 7);
  Wire.setClock(100000);
  delay(250);

  Serial.println("Initializing MPU6500...");
  mpuWrite(0x6B, 0x00); delay(100);
  mpuWrite(0x1C, 0x10);
  mpuWrite(0x1B, 0x08);
  mpuWrite(0x1A, 0x04);

  Serial.print("WHO_AM_I: 0x");
  Serial.println(mpuRead(0x75), HEX);
  Serial.println("MPU Init done!");

  // ── BLE init ────────────────────────────────────────────────
  BLEDevice::init("DeepWorkHalo_Sensor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("0");
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising as 'DeepWorkHalo_Sensor'");
}

void loop() {
  // ── Read MPU6500 ────────────────────────────────────────────
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14);

  if (Wire.available() < 14) {
    Serial.println("MPU read error!");
    delay(200);
    return;
  }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  int16_t rawTemp = (Wire.read() << 8) | Wire.read();
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  float accX  = ax / 4096.0;
  float accY  = ay / 4096.0;
  float accZ  = az / 4096.0;
  float gyroX = gx / 65.5;
  float gyroY = gy / 65.5;
  float gyroZ = gz / 65.5;
  float tempC = (rawTemp / 333.87) + 21.0;

  float rawMag      = sqrt(gyroX*gyroX + gyroY*gyroY + gyroZ*gyroZ);
  float smoothedMag = movingAverage(rawMag);
  int   focusScore  = calcFocusScore(smoothedMag);

  // Print same format as your working code + DSP info
  Serial.print("Acc(g): ");
  Serial.print(accX, 3); Serial.print(", ");
  Serial.print(accY, 3); Serial.print(", ");
  Serial.print(accZ, 3);
  Serial.print("  |  Gyro(°/s): ");
  Serial.print(gyroX, 2); Serial.print(", ");
  Serial.print(gyroY, 2); Serial.print(", ");
  Serial.print(gyroZ, 2);
  Serial.print("  |  Temp: ");
  Serial.print(tempC, 1); Serial.print("°C");
  Serial.print("  |  RawMag: ");   Serial.print(rawMag, 2);
  Serial.print("  |  Smoothed: "); Serial.print(smoothedMag, 2);
  Serial.print("  |  Score: ");    Serial.println(focusScore);

  // ── Transmit over BLE ────────────────────────────────────────
  if (deviceConnected) {
    String val = String(focusScore);
    pCharacteristic->setValue(val.c_str());
    pCharacteristic->notify();
  }

  // ── Reconnect handling ───────────────────────────────────────
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Restarting advertising...");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(200);
}
