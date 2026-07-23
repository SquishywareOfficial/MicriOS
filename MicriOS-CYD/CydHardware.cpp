#include "CydHardware.h"

#include <Preferences.h>
#include <TFT_eSPI.h>

namespace CydHardware {
namespace {
constexpr const char* PREF_NAMESPACE = "cyd";
constexpr const char* CAL_VALID_KEY = "cal";
constexpr const char* CAL_LAYOUT_KEY = "calver";
constexpr uint8_t CAL_LAYOUT_VERSION = 3;
constexpr const char* BRIGHTNESS_KEY = "bright";
constexpr uint8_t DEFAULT_BRIGHTNESS = 13;
constexpr const char* AUDIO_VOLUME_KEY = "volume";
constexpr uint8_t DEFAULT_AUDIO_VOLUME = 15;
constexpr const char* AUDIO_MUTED_KEY = "muted";
constexpr uint16_t TOUCH_MIN_PRESSURE = 400;
constexpr uint32_t BACKLIGHT_PWM_HZ = 25000;
constexpr uint32_t BEEP_FREQUENCY_HZ = 720;
constexpr uint32_t BEEP_DURATION_MS = 75;

Preferences prefs;
Calibration currentCalibration;
bool touchReady = false;
uint8_t activeDisplayRotation = PREFERRED_DISPLAY_ROTATION;
bool displayRotationSelected = false;
bool backlightReady = false;
uint8_t currentAudioVolume = DEFAULT_AUDIO_VOLUME;
bool currentAudioMuted = false;

// The command sequence and best-two sampling follow Paul Stoffregen's
// MIT-licensed XPT2046_Touchscreen driver. Bit-banging this target-local bus
// leaves the ESP32 VSPI host available for the CYD microSD slot.
uint8_t touchTransfer8(uint8_t output) {
  uint8_t input = 0;
  for (int8_t bit = 7; bit >= 0; --bit) {
    digitalWrite(PIN_TOUCH_MOSI, (output >> bit) & 1U);
    digitalWrite(PIN_TOUCH_CLK, HIGH);
    input = static_cast<uint8_t>((input << 1) |
                                 (digitalRead(PIN_TOUCH_MISO) ? 1 : 0));
    digitalWrite(PIN_TOUCH_CLK, LOW);
  }
  return input;
}

uint16_t touchTransfer16(uint16_t output) {
  return static_cast<uint16_t>(touchTransfer8(output >> 8) << 8) |
         touchTransfer8(output & 0xff);
}

int16_t bestTwoAverage(int16_t first, int16_t second, int16_t third) {
  const int16_t firstSecond = abs(first - second);
  const int16_t firstThird = abs(first - third);
  const int16_t secondThird = abs(second - third);
  if (firstSecond <= firstThird && firstSecond <= secondThird) {
    return (first + second) >> 1;
  }
  if (firstThird <= firstSecond && firstThird <= secondThird) {
    return (first + third) >> 1;
  }
  return (second + third) >> 1;
}

RawTouchSample readTouchController() {
  RawTouchSample sample;
  if (!touchReady || digitalRead(PIN_TOUCH_IRQ) != LOW) return sample;

  int16_t data[6] = {};
  digitalWrite(PIN_TOUCH_CS, LOW);
  touchTransfer8(0xB1);
  const int16_t z1 = touchTransfer16(0xC1) >> 3;
  int16_t pressure = z1 + 4095;
  const int16_t z2 = touchTransfer16(0x91) >> 3;
  pressure -= z2;
  if (pressure >= TOUCH_MIN_PRESSURE) {
    touchTransfer16(0x91);
    data[0] = touchTransfer16(0xD1) >> 3;
    data[1] = touchTransfer16(0x91) >> 3;
    data[2] = touchTransfer16(0xD1) >> 3;
    data[3] = touchTransfer16(0x91) >> 3;
  }
  data[4] = touchTransfer16(0xD0) >> 3;
  data[5] = touchTransfer16(0) >> 3;
  digitalWrite(PIN_TOUCH_CS, HIGH);

  if (pressure < TOUCH_MIN_PRESSURE) return sample;
  const int16_t measuredX = bestTwoAverage(data[0], data[2], data[4]);
  const int16_t measuredY = bestTwoAverage(data[1], data[3], data[5]);

  // Match XPT2046_Touchscreen rotation 0 so existing calibration remains
  // valid. Software SPI leaves hardware VSPI available for the CYD SD slot.
  sample.down = true;
  sample.x = 4095 - measuredY;
  sample.y = measuredX;
  sample.pressure = pressure;
  return sample;
}

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
  holdAudioIdle();

  configureOptionalButton(MICRI_CYD_B1_PIN);
  configureOptionalButton(MICRI_CYD_B2_PIN);

  loadCalibration();
}

void beginBacklight() {
  // Attach after TFT_eSPI::init(). Some TFT_eSPI setups drive TFT_BL as a
  // normal GPIO during init, which silently detaches or overrides LEDC PWM.
  backlightReady = ledcAttach(PIN_BACKLIGHT, BACKLIGHT_PWM_HZ, 8);
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
  pinMode(PIN_TOUCH_CLK, OUTPUT);
  pinMode(PIN_TOUCH_MOSI, OUTPUT);
  pinMode(PIN_TOUCH_MISO, INPUT);
  pinMode(PIN_TOUCH_CS, OUTPUT);
  pinMode(PIN_TOUCH_IRQ, INPUT);
  digitalWrite(PIN_TOUCH_CLK, LOW);
  digitalWrite(PIN_TOUCH_MOSI, LOW);
  digitalWrite(PIN_TOUCH_CS, HIGH);
  touchReady = true;
  Serial.print("[cyd] touch ");
  Serial.println("ready (software SPI)");
  return touchReady;
}

RawTouchSample readRawTouch() {
  return readTouchController();
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
    backlightReady = ledcAttach(PIN_BACKLIGHT, BACKLIGHT_PWM_HZ, 8);
  }
  if (!ledcWrite(PIN_BACKLIGHT, duty)) {
    Serial.printf("[cyd] backlight write failed level=%u duty=%u\n", level,
                  duty);
  }
}

uint8_t loadAudioVolume() {
  prefs.begin(PREF_NAMESPACE, true);
  currentAudioVolume =
      constrain(prefs.getUChar(AUDIO_VOLUME_KEY, DEFAULT_AUDIO_VOLUME), 0, 100);
  prefs.end();
  return currentAudioVolume;
}

void previewAudioVolume(uint8_t percent) {
  currentAudioVolume = constrain(percent, 0, 100);
}

void saveAudioVolume(uint8_t percent) {
  previewAudioVolume(percent);
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putUChar(AUDIO_VOLUME_KEY, currentAudioVolume);
  prefs.end();
}

uint8_t audioVolume() {
  return currentAudioVolume;
}

bool loadAudioMuted() {
  prefs.begin(PREF_NAMESPACE, true);
  currentAudioMuted = prefs.getBool(AUDIO_MUTED_KEY, false);
  prefs.end();
  return currentAudioMuted;
}

void saveAudioMuted(bool muted) {
  currentAudioMuted = muted;
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putBool(AUDIO_MUTED_KEY, currentAudioMuted);
  prefs.end();
}

bool audioMuted() {
  return currentAudioMuted;
}

void holdAudioIdle() {
  // The CYD's SC8002B amplifier is permanently enabled and AC-coupled to
  // GPIO26. Keep the DAC-side input low impedance when I2S is not using it so
  // the amplifier cannot turn a floating pin into an audible whine.
  dacDisable(PIN_AUDIO_DAC);
  pinMode(PIN_AUDIO_DAC, OUTPUT);
  digitalWrite(PIN_AUDIO_DAC, LOW);
}

void playNotificationBeep() {
  if (currentAudioMuted || currentAudioVolume == 0) return;
  if (!ledcAttach(PIN_AUDIO_DAC, BEEP_FREQUENCY_HZ, 8)) return;
  const uint8_t duty = static_cast<uint8_t>(
      5 + static_cast<uint16_t>(currentAudioVolume) * 45 / 100);
  ledcWrite(PIN_AUDIO_DAC, duty);
  delay(BEEP_DURATION_MS);
  ledcWrite(PIN_AUDIO_DAC, 0);
  ledcDetach(PIN_AUDIO_DAC);
  holdAudioIdle();
}

bool enterChildLockDeepSleep() {
  // The complete CYD firmware already sits close to the classic ESP32's IRAM
  // ceiling. ESP-IDF's ext0/deep-sleep path adds about 1.8 KB of IRAM and
  // overflows this all-app image, so use the documented locked fallback until
  // enough IRAM can be reclaimed. The shell keeps polling touch for a parent.
  Serial.println("[kidmode] deep sleep unavailable; using dim locked mode");
  dimForLockedFallback();
  return false;
}

void dimForLockedFallback() {
  setBrightness(1);
  setRgb(0, 0, 0);
  holdAudioIdle();
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
