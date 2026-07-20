#pragma once

#include <Arduino.h>

#include "src/shared/logic/TouchUi.h"

class TFT_eSPI;

namespace CydHardware {

constexpr int16_t SCREEN_WIDTH = 320;
constexpr int16_t SCREEN_HEIGHT = 240;
constexpr int16_t CONTENT_HEIGHT = 208;
constexpr uint8_t PREFERRED_DISPLAY_ROTATION = 1;

constexpr uint8_t PIN_TOUCH_CLK = 25;
constexpr uint8_t PIN_TOUCH_MOSI = 32;
constexpr uint8_t PIN_TOUCH_CS = 33;
constexpr uint8_t PIN_TOUCH_IRQ = 36;
constexpr uint8_t PIN_TOUCH_MISO = 39;

constexpr uint8_t PIN_BACKLIGHT = 21;
constexpr uint8_t PIN_LED_RED = 4;
constexpr uint8_t PIN_LED_GREEN = 16;
constexpr uint8_t PIN_LED_BLUE = 17;
constexpr uint8_t PIN_BOOT = 0;

#ifndef MICRI_CYD_B1_PIN
#define MICRI_CYD_B1_PIN -1
#endif

#ifndef MICRI_CYD_B2_PIN
#define MICRI_CYD_B2_PIN -1
#endif

struct RawTouchSample {
  bool down = false;
  int16_t x = 0;
  int16_t y = 0;
  int16_t pressure = 0;
};

struct Calibration {
  int16_t screenX0Raw = 3800;
  int16_t screenXMaxRaw = 250;
  int16_t screenY0Raw = 250;
  int16_t screenYMaxRaw = 3800;
  bool screenXUsesRawY = false;
  bool valid = false;
};

void begin();
void beginBacklight();
void applyDisplayOrientation(TFT_eSPI& tft);
uint8_t displayRotation();
bool beginTouch();
RawTouchSample readRawTouch();
TouchUi::TouchSample readTouch();

bool hasCalibration();
Calibration calibration();
void saveCalibration(const RawTouchSample samples[4],
                     const TouchUi::Point targets[4]);
void clearCalibration();

uint8_t loadBrightness();
void saveBrightness(uint8_t level);
void setBrightness(uint8_t level);

void setRgb(uint8_t red, uint8_t green, uint8_t blue);
void setStatusLed(bool on);

bool physicalB1Down();
bool physicalB2Down();
bool forceCalibrationRequested();

}  // namespace CydHardware
