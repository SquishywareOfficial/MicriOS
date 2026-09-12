#include "TDisplayIdleSettings.h"

#include <Preferences.h>
#include <stdio.h>

namespace TDisplayIdleSettings {
namespace {

constexpr char PREFERENCES_NAMESPACE[] = "displayidle";
constexpr char SCREEN_SAVER_KEY[] = "savermin";
constexpr char DIM_KEY[] = "dimmin";
constexpr char SCREEN_OFF_KEY[] = "offmin";
constexpr char SELECTED_SAVER_KEY[] = "saver";

constexpr uint16_t SCREEN_SAVER_PRESETS[] = {0, 1, 2, 5, 10, 15, 30};
constexpr uint16_t DIM_PRESETS[] = {0, 1, 2, 5, 10, 15, 30};
constexpr uint16_t SCREEN_OFF_PRESETS[] = {0, 2, 5, 10, 15, 30, 60};

template <size_t N>
uint16_t validatedPreset(uint16_t value, const uint16_t (&presets)[N],
                         uint16_t fallback) {
  for (uint16_t preset : presets) {
    if (preset == value) {
      return value;
    }
  }
  return fallback;
}

template <size_t N>
uint16_t nextPreset(uint16_t current, const uint16_t (&presets)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (presets[i] == current) {
      return presets[(i + 1) % N];
    }
  }
  return presets[0];
}

}  // namespace

Config load() {
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, true);
  Config config;
  config.screenSaverMinutes = validatedPreset(
      preferences.getUShort(SCREEN_SAVER_KEY, DEFAULT_SCREEN_SAVER_MINUTES),
      SCREEN_SAVER_PRESETS, DEFAULT_SCREEN_SAVER_MINUTES);
  config.dimMinutes = validatedPreset(
      preferences.getUShort(DIM_KEY, DEFAULT_DIM_MINUTES), DIM_PRESETS,
      DEFAULT_DIM_MINUTES);
  config.screenOffMinutes = validatedPreset(
      preferences.getUShort(SCREEN_OFF_KEY, DEFAULT_SCREEN_OFF_MINUTES),
      SCREEN_OFF_PRESETS, DEFAULT_SCREEN_OFF_MINUTES);
  config.selectedSaver = preferences.getUChar(SELECTED_SAVER_KEY, 0);
  preferences.end();
  return config;
}

void saveScreenSaverMinutes(uint16_t minutes) {
  minutes = validatedPreset(minutes, SCREEN_SAVER_PRESETS,
                            DEFAULT_SCREEN_SAVER_MINUTES);
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putUShort(SCREEN_SAVER_KEY, minutes);
  preferences.end();
}

void saveDimMinutes(uint16_t minutes) {
  minutes = validatedPreset(minutes, DIM_PRESETS, DEFAULT_DIM_MINUTES);
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putUShort(DIM_KEY, minutes);
  preferences.end();
}

void saveScreenOffMinutes(uint16_t minutes) {
  minutes = validatedPreset(minutes, SCREEN_OFF_PRESETS,
                            DEFAULT_SCREEN_OFF_MINUTES);
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putUShort(SCREEN_OFF_KEY, minutes);
  preferences.end();
}

void saveSelectedSaver(uint8_t saver) {
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putUChar(SELECTED_SAVER_KEY, saver);
  preferences.end();
}

uint16_t nextScreenSaverMinutes(uint16_t current) {
  return nextPreset(current, SCREEN_SAVER_PRESETS);
}

uint16_t nextDimMinutes(uint16_t current) {
  return nextPreset(current, DIM_PRESETS);
}

uint16_t nextScreenOffMinutes(uint16_t current) {
  return nextPreset(current, SCREEN_OFF_PRESETS);
}

void formatDuration(uint16_t minutes, char* output, size_t outputSize) {
  if (minutes == 0) {
    snprintf(output, outputSize, "Off");
  } else if (minutes < 60) {
    snprintf(output, outputSize, "%u min", minutes);
  } else {
    snprintf(output, outputSize, "%u hour", minutes / 60);
  }
}

}  // namespace TDisplayIdleSettings
