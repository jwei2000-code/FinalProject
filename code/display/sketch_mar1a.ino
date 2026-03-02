#include <SwitecX25.h>
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>

// --- Configuration ---
#define LED_PIN     D9
#define NUM_LEDS    7
#define STEPS       945
#define IN_ZONE_POS 800
#define READY_POS   100

// IMPORTANT: use ESP32 GPIO numbers that you wired through a driver (ULN2003/transistors)
// XIAO ESP32C3 example pins (change to your actual wiring)
#define COIL1 4
#define COIL2 5
#define COIL3 6
#define COIL4 7

SwitecX25 motor(STEPS, COIL1, COIL2, COIL3, COIL4);
Adafruit_NeoPixel ring(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int focusLevel = 0;

void setup() {
  Serial.begin(115200);

  ring.begin();
  ring.setBrightness(50);
  ring.show();

  motor.setPosition(READY_POS);
}

void loop() {
  motor.update();

  if (focusLevel >= 70) {
    motor.setPosition(IN_ZONE_POS);
    pulseLEDs(255, 120, 0);
  } else {
    motor.setPosition(READY_POS);
    ring.clear();
    ring.show();
  }
}

void pulseLEDs(uint8_t r, uint8_t g, uint8_t b) {
  float s = sin(millis() / 500.0f);
  uint8_t brightness = (uint8_t)((s + 1.0f) * 127.0f);

  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, ring.Color((r * brightness) / 255, (g * brightness) / 255, (b * brightness) / 255));
  }
  ring.show();
}