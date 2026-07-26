#pragma once

#include <stdint.h>

#include "src/shared/logic/BatterySessionLogic.h"

class TFT_eSPI;

namespace TDisplayPower {

constexpr uint8_t BACKLIGHT_PIN = 4;
constexpr uint8_t BATTERY_DIVIDER_ENABLE_PIN = 14;
constexpr uint8_t BATTERY_ADC_PIN = 34;
constexpr uint16_t LOW_BATTERY_MV = 3500;
constexpr uint16_t CRITICAL_BATTERY_MV = 3350;
constexpr uint16_t BATTERY_RECOVERY_MV = 3650;
constexpr uint16_t DEFAULT_FULL_VOLTAGE_MV = 4200;
constexpr uint16_t MIN_FULL_VOLTAGE_MV = 3800;
constexpr uint16_t MAX_FULL_VOLTAGE_MV = 4400;
constexpr uint8_t MIN_BRIGHTNESS_LEVEL = 1;
constexpr uint8_t MAX_BRIGHTNESS_LEVEL = 10;

struct BatteryReading {
  bool valid = false;
  uint16_t millivolts = 0;
  uint8_t percent = 0;
  uint32_t sampledAtMs = 0;
};

// Releases RTC pin holds left by deep sleep before the display is initialized.
void prepareAfterWake();

// Samples the switched 1:2 battery/charger divider and disables it afterward.
BatteryReading readBattery();
BatteryReading latestBatteryReading();
BatterySessionLogic::Snapshot batterySessionSnapshot();
uint64_t bootUptimeSeconds();
uint32_t bootUptimeMinutes();
void finalizeBatterySession(BatterySessionLogic::EndReason reason);
void resetBatteryTelemetry();

bool isBatteryInstalled();
void setBatteryInstalled(bool installed);

uint16_t loadFullVoltageMv();
void saveFullVoltageMv(uint16_t millivolts);

uint8_t loadBrightnessLevel();
void saveBrightnessLevel(uint8_t level);
void applyBrightnessLevel(uint8_t level);

// Powers down radios and the display. RST/EN is the reliable wake mechanism:
// B2/GPIO35 has no pull-up on the classic T-Display and floats in deep sleep.
[[noreturn]] void enterDeepSleep(
    TFT_eSPI& tft,
    BatterySessionLogic::EndReason reason =
        BatterySessionLogic::EndReason::ManualSleep);

}  // namespace TDisplayPower
