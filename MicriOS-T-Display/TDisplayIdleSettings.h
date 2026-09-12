#pragma once

#include <stddef.h>
#include <stdint.h>

namespace TDisplayIdleSettings {

constexpr uint16_t DEFAULT_SCREEN_SAVER_MINUTES = 5;
constexpr uint16_t DEFAULT_DIM_MINUTES = 0;
constexpr uint16_t DEFAULT_SCREEN_OFF_MINUTES = 15;

struct Config {
  uint16_t screenSaverMinutes = DEFAULT_SCREEN_SAVER_MINUTES;
  uint16_t dimMinutes = DEFAULT_DIM_MINUTES;
  uint16_t screenOffMinutes = DEFAULT_SCREEN_OFF_MINUTES;
  uint8_t selectedSaver = 0;
};

Config load();
void saveScreenSaverMinutes(uint16_t minutes);
void saveDimMinutes(uint16_t minutes);
void saveScreenOffMinutes(uint16_t minutes);
void saveSelectedSaver(uint8_t saver);

uint16_t nextScreenSaverMinutes(uint16_t current);
uint16_t nextDimMinutes(uint16_t current);
uint16_t nextScreenOffMinutes(uint16_t current);
void formatDuration(uint16_t minutes, char* output, size_t outputSize);

}  // namespace TDisplayIdleSettings
