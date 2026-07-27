#include "TDisplayPower.h"

#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_bt.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>

namespace TDisplayPower {
namespace {

constexpr uint8_t BATTERY_SAMPLE_COUNT = 24;
constexpr char POWER_PREFERENCES_NAMESPACE[] = "power";
constexpr char BATTERY_INSTALLED_KEY[] = "battery";
constexpr char FULL_VOLTAGE_KEY[] = "fullmv";
constexpr char BRIGHTNESS_KEY[] = "bright";
constexpr char CURRENT_SESSION_KEY[] = "bcur";
constexpr char LAST_SESSION_KEY[] = "blast";
constexpr uint8_t DEFAULT_BRIGHTNESS_LEVEL = 8;
constexpr uint32_t BACKLIGHT_PWM_FREQUENCY = 5000;
constexpr uint8_t BACKLIGHT_PWM_RESOLUTION = 8;
constexpr uint8_t BRIGHTNESS_DUTY[MAX_BRIGHTNESS_LEVEL] = {
    8, 14, 22, 34, 50, 72, 100, 136, 188, 255,
};
bool backlightPwmAttached = false;
bool screenStandbyActive = false;
bool batteryTrackerLoaded = false;
BatterySessionLogic::Tracker batteryTracker;
BatteryReading cachedBatteryReading;

struct VoltagePoint {
  uint16_t millivolts;
  uint8_t percent;
};

constexpr VoltagePoint VOLTAGE_CURVE[] = {
    {3300, 0}, {3500, 3}, {3600, 6}, {3700, 12}, {3750, 20},
    {3800, 30}, {3850, 40}, {3900, 50}, {3950, 60}, {4000, 70},
    {4050, 80}, {4100, 90}, {4150, 96}, {4200, 100},
};

uint8_t estimatePercent(uint16_t millivolts, uint16_t fullVoltageMv) {
  fullVoltageMv =
      constrain(fullVoltageMv, MIN_FULL_VOLTAGE_MV, MAX_FULL_VOLTAGE_MV);
  if (millivolts >= fullVoltageMv) {
    return 100;
  }

  // Map the user-observed full voltage onto the canonical 4.20 V LiPo curve.
  // This calibrates the displayed percentage only; safety thresholds continue
  // to compare the raw measured voltage.
  constexpr uint16_t EMPTY_REFERENCE_MV = 3300;
  if (millivolts > EMPTY_REFERENCE_MV &&
      fullVoltageMv > EMPTY_REFERENCE_MV) {
    const uint32_t scaledRange =
        static_cast<uint32_t>(millivolts - EMPTY_REFERENCE_MV) *
        (DEFAULT_FULL_VOLTAGE_MV - EMPTY_REFERENCE_MV);
    millivolts = EMPTY_REFERENCE_MV +
                 scaledRange / (fullVoltageMv - EMPTY_REFERENCE_MV);
  }

  if (millivolts <= VOLTAGE_CURVE[0].millivolts) {
    return VOLTAGE_CURVE[0].percent;
  }

  constexpr size_t pointCount = sizeof(VOLTAGE_CURVE) / sizeof(VOLTAGE_CURVE[0]);
  for (size_t i = 1; i < pointCount; ++i) {
    if (millivolts <= VOLTAGE_CURVE[i].millivolts) {
      const VoltagePoint& low = VOLTAGE_CURVE[i - 1];
      const VoltagePoint& high = VOLTAGE_CURVE[i];
      const uint32_t numerator =
          static_cast<uint32_t>(millivolts - low.millivolts) * (high.percent - low.percent);
      return low.percent + numerator / (high.millivolts - low.millivolts);
    }
  }
  return 100;
}

void holdRtcOutputLow(gpio_num_t pin) {
  rtc_gpio_init(pin);
  rtc_gpio_set_direction(pin, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level(pin, 0);
  rtc_gpio_hold_en(pin);
}

void stopRadios() {
  // esp_now_deinit() can dereference uninitialized WiFi state on this
  // Arduino-ESP32/IDF build. Only tear ESP-NOW down after WiFi has actually
  // been started; deinit itself safely handles "WiFi active, ESP-NOW absent".
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    esp_now_deinit();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

#if CONFIG_BT_ENABLED
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    esp_bt_controller_disable();
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    esp_bt_controller_deinit();
  }
#endif
}

void persistTelemetryUpdates() {
  BatterySessionLogic::PersistenceUpdate update;
  while (batteryTracker.takePersistenceUpdate(update)) {
    Preferences preferences;
    preferences.begin(POWER_PREFERENCES_NAMESPACE, false);
    if (update.action ==
        BatterySessionLogic::PersistenceAction::SaveCurrent) {
      preferences.putBytes(CURRENT_SESSION_KEY, &update.record,
                           sizeof(update.record));
    } else if (update.action ==
               BatterySessionLogic::PersistenceAction::FinalizeToLast) {
      preferences.putBytes(LAST_SESSION_KEY, &update.record,
                           sizeof(update.record));
      preferences.remove(CURRENT_SESSION_KEY);
    }
    preferences.end();
  }
}

bool loadSessionRecord(Preferences& preferences, const char* key,
                       BatterySessionLogic::SessionRecord& record) {
  if (preferences.getBytesLength(key) != sizeof(record)) {
    return false;
  }
  return preferences.getBytes(key, &record, sizeof(record)) == sizeof(record) &&
         BatterySessionLogic::isValid(record);
}

void ensureBatteryTrackerLoaded() {
  if (batteryTrackerLoaded) {
    return;
  }

  BatterySessionLogic::SessionRecord current;
  BatterySessionLogic::SessionRecord last;
  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, true);
  const bool hasCurrent =
      loadSessionRecord(preferences, CURRENT_SESSION_KEY, current);
  const bool hasLast = loadSessionRecord(preferences, LAST_SESSION_KEY, last);
  preferences.end();

  batteryTracker.restore(hasCurrent ? &current : nullptr,
                         hasLast ? &last : nullptr);
  batteryTrackerLoaded = true;
}

}  // namespace

void prepareAfterWake() {
  screenStandbyActive = false;
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  Serial.printf("[power] reset reason=%d wake cause=%d\n",
                static_cast<int>(esp_reset_reason()),
                static_cast<int>(wakeCause));

  gpio_deep_sleep_hold_dis();
  rtc_gpio_hold_dis(static_cast<gpio_num_t>(BACKLIGHT_PIN));
  rtc_gpio_hold_dis(static_cast<gpio_num_t>(BATTERY_DIVIDER_ENABLE_PIN));
  rtc_gpio_deinit(static_cast<gpio_num_t>(BACKLIGHT_PIN));
  rtc_gpio_deinit(static_cast<gpio_num_t>(BATTERY_DIVIDER_ENABLE_PIN));

  pinMode(BATTERY_DIVIDER_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_DIVIDER_ENABLE_PIN, LOW);
  ensureBatteryTrackerLoaded();
}

BatteryReading readBattery() {
  BatteryReading reading;

  pinMode(BATTERY_DIVIDER_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_DIVIDER_ENABLE_PIN, HIGH);
  delay(8);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  uint32_t sampleTotalMv = 0;
  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; ++i) {
    sampleTotalMv += analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(2);
  }

  digitalWrite(BATTERY_DIVIDER_ENABLE_PIN, LOW);

  const uint32_t dividerMv = sampleTotalMv / BATTERY_SAMPLE_COUNT;
  const uint32_t railMv = dividerMv * 2U;
  reading.sampledAtMs = millis();
  if (railMv >= 2500U && railMv <= 5000U) {
    reading.valid = true;
    reading.millivolts = static_cast<uint16_t>(railMv);
    reading.percent =
        estimatePercent(reading.millivolts, loadFullVoltageMv());
    cachedBatteryReading = reading;
    ensureBatteryTrackerLoaded();
    batteryTracker.observeVoltage(reading.millivolts, esp_timer_get_time());
    persistTelemetryUpdates();
  }
  return reading;
}

BatteryReading latestBatteryReading() {
  return cachedBatteryReading;
}

BatterySessionLogic::Snapshot batterySessionSnapshot() {
  ensureBatteryTrackerLoaded();
  return batteryTracker.snapshot(esp_timer_get_time());
}

uint32_t bootUptimeMinutes() {
  return static_cast<uint32_t>(bootUptimeSeconds() / 60ULL);
}

uint64_t bootUptimeSeconds() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
}

void finalizeBatterySession(BatterySessionLogic::EndReason reason) {
  ensureBatteryTrackerLoaded();
  batteryTracker.finish(reason, esp_timer_get_time());
  persistTelemetryUpdates();
}

void resetBatteryTelemetry() {
  batteryTracker.reset();
  batteryTrackerLoaded = true;
  cachedBatteryReading = BatteryReading{};

  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, false);
  preferences.remove(CURRENT_SESSION_KEY);
  preferences.remove(LAST_SESSION_KEY);
  preferences.end();
}

bool isBatteryInstalled() {
  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, true);
  const bool installed = preferences.getBool(BATTERY_INSTALLED_KEY, false);
  preferences.end();
  return installed;
}

void setBatteryInstalled(bool installed) {
  if (!installed) {
    finalizeBatterySession(BatterySessionLogic::EndReason::Disabled);
  }
  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, false);
  preferences.putBool(BATTERY_INSTALLED_KEY, installed);
  preferences.end();
}

uint16_t loadFullVoltageMv() {
  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, true);
  uint16_t millivolts =
      preferences.getUShort(FULL_VOLTAGE_KEY, DEFAULT_FULL_VOLTAGE_MV);
  preferences.end();
  return constrain(millivolts, MIN_FULL_VOLTAGE_MV, MAX_FULL_VOLTAGE_MV);
}

void saveFullVoltageMv(uint16_t millivolts) {
  millivolts =
      constrain(millivolts, MIN_FULL_VOLTAGE_MV, MAX_FULL_VOLTAGE_MV);
  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, false);
  preferences.putUShort(FULL_VOLTAGE_KEY, millivolts);
  preferences.end();
}

uint8_t loadBrightnessLevel() {
  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, true);
  uint8_t level = preferences.getUChar(BRIGHTNESS_KEY, DEFAULT_BRIGHTNESS_LEVEL);
  preferences.end();
  return constrain(level, MIN_BRIGHTNESS_LEVEL, MAX_BRIGHTNESS_LEVEL);
}

void saveBrightnessLevel(uint8_t level) {
  level = constrain(level, MIN_BRIGHTNESS_LEVEL, MAX_BRIGHTNESS_LEVEL);
  Preferences preferences;
  preferences.begin(POWER_PREFERENCES_NAMESPACE, false);
  preferences.putUChar(BRIGHTNESS_KEY, level);
  preferences.end();
}

void applyBrightnessLevel(uint8_t level) {
  level = constrain(level, MIN_BRIGHTNESS_LEVEL, MAX_BRIGHTNESS_LEVEL);
  if (!backlightPwmAttached) {
    backlightPwmAttached =
        ledcAttach(BACKLIGHT_PIN, BACKLIGHT_PWM_FREQUENCY, BACKLIGHT_PWM_RESOLUTION);
  }
  if (backlightPwmAttached) {
    ledcWrite(BACKLIGHT_PIN, BRIGHTNESS_DUTY[level - 1]);
  }
}

void enterScreenStandby(TFT_eSPI& tft) {
  if (screenStandbyActive) {
    return;
  }

  if (!backlightPwmAttached) {
    backlightPwmAttached =
        ledcAttach(BACKLIGHT_PIN, BACKLIGHT_PWM_FREQUENCY, BACKLIGHT_PWM_RESOLUTION);
  }
  if (backlightPwmAttached) {
    ledcWrite(BACKLIGHT_PIN, 0);
  } else {
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);
  }

  tft.writecommand(TFT_DISPOFF);
  delay(20);
  tft.writecommand(TFT_SLPIN);
  delay(120);
  screenStandbyActive = true;
}

void exitScreenStandby(TFT_eSPI& tft) {
  if (!screenStandbyActive) {
    return;
  }

  tft.writecommand(TFT_SLPOUT);
  delay(120);
  tft.writecommand(TFT_DISPON);
  delay(20);
  tft.fillScreen(TFT_BLACK);
  screenStandbyActive = false;
  applyBrightnessLevel(loadBrightnessLevel());
}

bool isScreenStandbyActive() {
  return screenStandbyActive;
}

[[noreturn]] void enterDeepSleep(
    TFT_eSPI& tft, BatterySessionLogic::EndReason reason) {
  if (reason == BatterySessionLogic::EndReason::ManualSleep &&
      isBatteryInstalled()) {
    readBattery();
  }
  finalizeBatterySession(reason);
  stopRadios();

  // Put the panel to sleep before removing its backlight.
  tft.writecommand(TFT_DISPOFF);
  delay(20);
  tft.writecommand(TFT_SLPIN);
  delay(120);

  if (backlightPwmAttached) {
    ledcDetach(BACKLIGHT_PIN);
    backlightPwmAttached = false;
  }
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW);
  pinMode(BATTERY_DIVIDER_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_DIVIDER_ENABLE_PIN, LOW);
  SPI.end();

  holdRtcOutputLow(static_cast<gpio_num_t>(BACKLIGHT_PIN));
  holdRtcOutputLow(static_cast<gpio_num_t>(BATTERY_DIVIDER_ENABLE_PIN));
  gpio_deep_sleep_hold_en();

  // Do not arm B2/GPIO35: it is an input-only pin with no internal pull-up,
  // and the classic T-Display schematic does not provide an external one.
  // A floating active-low ext0 input wakes the ESP32 immediately. GPIO0/B1 has
  // a pull-up, but is a boot-strapping pin, so RST/EN is the safe software-only
  // wake mechanism for this board revision.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  Serial.flush();
  esp_deep_sleep_start();
  while (true) {
    delay(1000);
  }
}

}  // namespace TDisplayPower
