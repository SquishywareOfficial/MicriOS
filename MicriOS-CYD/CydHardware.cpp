#include "CydHardware.h"

#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

namespace CydHardware {
namespace {
constexpr const char* PREF_NAMESPACE = "cyd";
constexpr const char* CAL_VALID_KEY = "cal";
constexpr const char* CAL_LAYOUT_KEY = "calver";
constexpr uint8_t CAL_LAYOUT_VERSION = 3;
constexpr const char* BRIGHTNESS_KEY = "bright";
constexpr uint8_t DEFAULT_BRIGHTNESS = 13;
constexpr uint16_t TOUCH_MIN_PRESSURE = 120;

SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
Preferences prefs;
Calibration currentCalibration;
bool touchReady = false;
uint8_t activeDisplayRotation = PREFERRED_DISPLAY_ROTATION;
bool displayRotationSelected = false;
bool backlightReady = false;

int16_t clampCoordinate(int32_t value, int16_t maximum) {
  if (value < 0) return 0;
  if (value > maximum) return maximum;
  return static_cast<int16_t>(value);
}

int16_t mapRaw(int16_t value, int16_t rawAtZero, int16_t rawAtMaximum,
               int16_t maximum) {
  const int32_t denominator = static_cast<int32_t>(rawAtMaximum) - rawAtZero;
  if (denominator == 0) return 0;
  const int32_t mapped =
      (static_cast<int32_t>(value) - rawAtZero) * maximum / denominator;
  return clampCoordinate(mapped, maximum);
}

int16_t averagePair(int16_t first, int16_t second) {
  return static_cast<int16_t>((static_cast<int32_t>(first) + second) / 2);
}

int16_t extrapolateRaw(int16_t rawAtTargetStart,
                       int16_t rawAtTargetEnd,
                       int16_t targetStart,
                       int16_t targetEnd,
                       int16_t desiredCoordinate) {
  const int32_t targetSpan = targetEnd - targetStart;
  if (targetSpan == 0) return rawAtTargetStart;
  return static_cast<int16_t>(
      rawAtTargetStart +
      (static_cast<int32_t>(rawAtTargetEnd - rawAtTargetStart) *
       (desiredCoordinate - targetStart)) /
          targetSpan);
}

void loadCalibration() {
  prefs.begin(PREF_NAMESPACE, true);
  const uint8_t savedLayout = prefs.getUChar(CAL_LAYOUT_KEY, 0);
  currentCalibration.valid = prefs.getBool(CAL_VALID_KEY, false) &&
                             savedLayout == CAL_LAYOUT_VERSION;
  currentCalibration.screenX0Raw = prefs.getShort("x0", 3800);
  currentCalibration.screenXMaxRaw = prefs.getShort("x1", 250);
  currentCalibration.screenY0Raw = prefs.getShort("y0", 250);
  currentCalibration.screenYMaxRaw = prefs.getShort("y1", 3800);
  currentCalibration.screenXUsesRawY = prefs.getBool("swap", false);
  prefs.end();
}

void configureOptionalButton(int pin) {
  if (pin >= 0) pinMode(pin, INPUT_PULLUP);
}

bool optionalButtonDown(int pin) {
  return pin >= 0 && digitalRead(pin) == LOW;
}
}  // namespace

void begin() {
  pinMode(PIN_BOOT, INPUT_PULLUP);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  setRgb(0, 0, 0);

  configureOptionalButton(MICRI_CYD_B1_PIN);
  configureOptionalButton(MICRI_CYD_B2_PIN);

  loadCalibration();
}

void beginBacklight() {
  // Attach after TFT_eSPI::init(). Some TFT_eSPI setups drive TFT_BL as a
  // normal GPIO during init, which silently detaches or overrides LEDC PWM.
  backlightReady = ledcAttach(PIN_BACKLIGHT, 5000, 8);
  Serial.printf("[cyd] backlight pwm %s pin=%u\n",
                backlightReady ? "ready" : "failed", PIN_BACKLIGHT);
  setBrightness(loadBrightness());
}

void applyDisplayOrientation(TFT_eSPI& tft) {
  if (displayRotationSelected) {
    tft.setRotation(activeDisplayRotation);
    return;
  }

  constexpr uint8_t CANDIDATES[] = {PREFERRED_DISPLAY_ROTATION, 1, 2, 0};
  for (const uint8_t candidate : CANDIDATES) {
    tft.setRotation(candidate);
    if (tft.width() == SCREEN_WIDTH && tft.height() == SCREEN_HEIGHT) {
      activeDisplayRotation = candidate;
      displayRotationSelected = true;
      Serial.printf("[cyd] display rotation=%u size=%dx%d\n",
                    activeDisplayRotation, tft.width(), tft.height());
      return;
    }
  }

  activeDisplayRotation = PREFERRED_DISPLAY_ROTATION;
  displayRotationSelected = true;
  tft.setRotation(activeDisplayRotation);
  Serial.printf("[cyd] warning: landscape size=%dx%d after rotation=%u\n",
                tft.width(), tft.height(), activeDisplayRotation);
}

uint8_t displayRotation() {
  return activeDisplayRotation;
}

bool beginTouch() {
  touchSpi.begin(PIN_TOUCH_CLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  touchReady = touch.begin(touchSpi);
  touch.setRotation(0);
  Serial.print("[cyd] touch ");
  Serial.println(touchReady ? "ready" : "failed");
  return touchReady;
}

RawTouchSample readRawTouch() {
  RawTouchSample sample;
  if (!touchReady || !touch.tirqTouched() || !touch.touched()) return sample;
  const TS_Point point = touch.getPoint();
  sample.pressure = point.z;
  sample.down = point.z >= TOUCH_MIN_PRESSURE;
  sample.x = point.x;
  sample.y = point.y;
  return sample;
}

TouchUi::TouchSample readTouch() {
  TouchUi::TouchSample sample;
  const RawTouchSample raw = readRawTouch();
  sample.down = raw.down;
  if (!raw.down) return sample;

  const int16_t rawForX = currentCalibration.screenXUsesRawY ? raw.y : raw.x;
  const int16_t rawForY = currentCalibration.screenXUsesRawY ? raw.x : raw.y;
  sample.point.x = mapRaw(rawForX, currentCalibration.screenX0Raw,
                         currentCalibration.screenXMaxRaw, SCREEN_WIDTH - 1);
  sample.point.y = mapRaw(rawForY, currentCalibration.screenY0Raw,
                         currentCalibration.screenYMaxRaw, SCREEN_HEIGHT - 1);
  return sample;
}

bool hasCalibration() {
  return currentCalibration.valid;
}

Calibration calibration() {
  return currentCalibration;
}

void saveCalibration(const RawTouchSample samples[4],
                     const TouchUi::Point targets[4]) {
  const int16_t leftRawX = averagePair(samples[0].x, samples[3].x);
  const int16_t rightRawX = averagePair(samples[1].x, samples[2].x);
  const int16_t leftRawY = averagePair(samples[0].y, samples[3].y);
  const int16_t rightRawY = averagePair(samples[1].y, samples[2].y);
  const int32_t horizontalX = rightRawX - leftRawX;
  const int32_t horizontalY = rightRawY - leftRawY;
  currentCalibration.screenXUsesRawY =
      (horizontalY < 0 ? -horizontalY : horizontalY) >
      (horizontalX < 0 ? -horizontalX : horizontalX);

  const int16_t topRawX = averagePair(samples[0].x, samples[1].x);
  const int16_t bottomRawX = averagePair(samples[3].x, samples[2].x);
  const int16_t topRawY = averagePair(samples[0].y, samples[1].y);
  const int16_t bottomRawY = averagePair(samples[3].y, samples[2].y);

  const int16_t xStartRaw = currentCalibration.screenXUsesRawY ? leftRawY : leftRawX;
  const int16_t xEndRaw = currentCalibration.screenXUsesRawY ? rightRawY : rightRawX;
  const int16_t yStartRaw = currentCalibration.screenXUsesRawY ? topRawX : topRawY;
  const int16_t yEndRaw = currentCalibration.screenXUsesRawY ? bottomRawX : bottomRawY;

  currentCalibration.screenX0Raw = extrapolateRaw(
      xStartRaw, xEndRaw, targets[0].x, targets[1].x, 0);
  currentCalibration.screenXMaxRaw = extrapolateRaw(
      xStartRaw, xEndRaw, targets[0].x, targets[1].x, SCREEN_WIDTH - 1);
  currentCalibration.screenY0Raw = extrapolateRaw(
      yStartRaw, yEndRaw, targets[0].y, targets[3].y, 0);
  currentCalibration.screenYMaxRaw = extrapolateRaw(
      yStartRaw, yEndRaw, targets[0].y, targets[3].y, SCREEN_HEIGHT - 1);
  currentCalibration.valid = true;

  prefs.begin(PREF_NAMESPACE, false);
  prefs.putBool(CAL_VALID_KEY, true);
  prefs.putUChar(CAL_LAYOUT_KEY, CAL_LAYOUT_VERSION);
  prefs.putShort("x0", currentCalibration.screenX0Raw);
  prefs.putShort("x1", currentCalibration.screenXMaxRaw);
  prefs.putShort("y0", currentCalibration.screenY0Raw);
  prefs.putShort("y1", currentCalibration.screenYMaxRaw);
  prefs.putBool("swap", currentCalibration.screenXUsesRawY);
  prefs.end();
}

void clearCalibration() {
  prefs.begin(PREF_NAMESPACE, false);
  prefs.remove(CAL_VALID_KEY);
  prefs.remove(CAL_LAYOUT_KEY);
  prefs.remove("x0");
  prefs.remove("x1");
  prefs.remove("y0");
  prefs.remove("y1");
  prefs.remove("swap");
  prefs.end();
  currentCalibration = Calibration();
}

uint8_t loadBrightness() {
  prefs.begin(PREF_NAMESPACE, true);
  const uint8_t level = prefs.getUChar(BRIGHTNESS_KEY, DEFAULT_BRIGHTNESS);
  prefs.end();
  return constrain(level, 1, 16);
}

void saveBrightness(uint8_t level) {
  level = constrain(level, 1, 16);
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putUChar(BRIGHTNESS_KEY, level);
  prefs.end();
  setBrightness(level);
}

void setBrightness(uint8_t level) {
  level = constrain(level, 1, 16);
  const uint8_t duty = static_cast<uint8_t>(8 + (level - 1) * 247 / 15);
  if (!backlightReady) {
    backlightReady = ledcAttach(PIN_BACKLIGHT, 5000, 8);
  }
  if (!ledcWrite(PIN_BACKLIGHT, duty)) {
    Serial.printf("[cyd] backlight write failed level=%u duty=%u\n", level,
                  duty);
  }
}

void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  analogWrite(PIN_LED_RED, 255 - red);
  analogWrite(PIN_LED_GREEN, 255 - green);
  analogWrite(PIN_LED_BLUE, 255 - blue);
}

void setStatusLed(bool on) {
  digitalWrite(PIN_LED_BLUE, on ? LOW : HIGH);
}

bool physicalB1Down() {
  return optionalButtonDown(MICRI_CYD_B1_PIN);
}

bool physicalB2Down() {
  return optionalButtonDown(MICRI_CYD_B2_PIN);
}

bool forceCalibrationRequested() {
  return digitalRead(PIN_BOOT) == LOW;
}

}  // namespace CydHardware
