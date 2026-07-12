#pragma once

#include <Arduino.h>

#include "../../App.h"

namespace AutoLaunchSettings {

constexpr const char* PREF_NS = "autolaunch";
constexpr const char* KEY_ENABLED = "enabled";
constexpr const char* KEY_APP = "app";
constexpr const char* KEY_AUTORUN = "autorun";

struct Config {
  bool enabled = false;
  bool autoRun = false;
  String appTitle;
};

Config load();
void save(const Config& config);
void setEnabled(bool enabled);
void setAppTitle(const char* title);
void setAutoRun(bool enabled);
void clear();
void migrateLegacyDebugPrefs();

}  // namespace AutoLaunchSettings

class AutoLaunchSettingsApp : public App {
public:
  AutoLaunchSettingsApp(uint32_t width, uint32_t height);

  void setLaunchableApps(App* const* apps, uint8_t count);
  bool hasCustomOverlay() const override;
  uint16_t runningRenderIntervalMs() const override;

protected:
  void onAppReset() override;
  void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
  void drawRunning(TFT_eSPI& tft) override;
  bool startsRunningImmediately() const override;

private:
  enum class Mode : uint8_t {
    Main,
    SelectApp,
    Message
  };

  void loadConfig();
  void saveConfig();
  void markDirty();
  void updateMainLabels();
  const char* mainLabel(uint8_t index) const;
  const char* appLabel(uint8_t index) const;
  void syncAppIndexToConfig();

  App* const* apps_ = nullptr;
  uint8_t appCount_ = 0;
  AutoLaunchSettings::Config config_;
  Mode mode_ = Mode::Main;
  uint8_t mainIndex_ = 0;
  uint8_t appIndex_ = 0;
  const char* message_ = "";
  bool dirty_ = true;
  bool rendered_ = false;
  Mode renderedMode_ = Mode::Main;
  char mainLabels_[4][34] = {};
};
