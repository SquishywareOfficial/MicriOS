#include "AutoLaunchSettingsApp.h"

#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr const char* LEGACY_DEBUG_PREF_NS = "debug";
constexpr const char* LEGACY_AUTO_APP_KEY = "auto_app";
constexpr const char* LEGACY_AUTO_MINER_KEY = "auto_miner";
constexpr uint8_t MAIN_COUNT = 4;

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}

String normalizeLegacyAppTitle(const String& title) {
  if (title.equalsIgnoreCase("Femto Miner")) return "Micri Miner";
  if (title.equalsIgnoreCase("Femto Casino")) return "Micri Casino";
  if (title.equalsIgnoreCase("Femto Field")) return "Micri Field";
  if (title.equalsIgnoreCase("Femto Clock")) return "Micri Clock";
  return title;
}
}

namespace AutoLaunchSettings {

Config load() {
  Preferences prefs;
  prefs.begin(PREF_NS, true);
  Config config;
  config.enabled = prefs.getBool(KEY_ENABLED, false);
  config.autoRun = prefs.getBool(KEY_AUTORUN, false);
  config.appTitle = prefs.getString(KEY_APP, "");
  prefs.end();
  config.appTitle.trim();
  const String normalizedTitle = normalizeLegacyAppTitle(config.appTitle);
  if (normalizedTitle != config.appTitle) {
    config.appTitle = normalizedTitle;
    Preferences writePrefs;
    writePrefs.begin(PREF_NS, false);
    writePrefs.putString(KEY_APP, config.appTitle);
    writePrefs.end();
  }
  return config;
}

void save(const Config& config) {
  Preferences prefs;
  prefs.begin(PREF_NS, false);
  prefs.putBool(KEY_ENABLED, config.enabled);
  prefs.putBool(KEY_AUTORUN, config.autoRun);
  prefs.putString(KEY_APP, config.appTitle);
  prefs.end();
}

void setEnabled(bool enabled) {
  Config config = load();
  config.enabled = enabled;
  save(config);
}

void setAppTitle(const char* title) {
  Config config = load();
  config.appTitle = title == nullptr ? "" : title;
  save(config);
}

void setAutoRun(bool enabled) {
  Config config = load();
  config.autoRun = enabled;
  save(config);
}

void clear() {
  Preferences prefs;
  prefs.begin(PREF_NS, false);
  prefs.clear();
  prefs.end();
}

void migrateLegacyDebugPrefs() {
  Preferences prefs;
  prefs.begin(PREF_NS, true);
  const bool hasApp = prefs.isKey(KEY_APP);
  const bool hasEnabled = prefs.isKey(KEY_ENABLED);
  const bool hasAutoRun = prefs.isKey(KEY_AUTORUN);
  prefs.end();

  Preferences legacy;
  legacy.begin(LEGACY_DEBUG_PREF_NS, true);
  String legacyApp = legacy.getString(LEGACY_AUTO_APP_KEY, "");
  const bool legacyMinerAutoRun = legacy.getBool(LEGACY_AUTO_MINER_KEY, false);
  legacy.end();
  legacyApp.trim();

  if ((!hasApp && legacyApp.length() > 0) || (!hasEnabled && legacyApp.length() > 0) || (!hasAutoRun && legacyMinerAutoRun)) {
    Config config = load();
    if (!hasApp && legacyApp.length() > 0) config.appTitle = normalizeLegacyAppTitle(legacyApp);
    if (!hasEnabled && legacyApp.length() > 0) config.enabled = true;
    if (!hasAutoRun && legacyMinerAutoRun) config.autoRun = true;
    save(config);
  }
}

}  // namespace AutoLaunchSettings

AutoLaunchSettingsApp::AutoLaunchSettingsApp(uint32_t width, uint32_t height)
    : App("Autolaunch", width, height) {}

void AutoLaunchSettingsApp::setLaunchableApps(App* const* apps, uint8_t count) {
  apps_ = apps;
  appCount_ = count;
}

bool AutoLaunchSettingsApp::hasCustomOverlay() const {
  return true;
}

uint16_t AutoLaunchSettingsApp::runningRenderIntervalMs() const {
  return 500;
}

bool AutoLaunchSettingsApp::startsRunningImmediately() const {
  return true;
}

void AutoLaunchSettingsApp::onAppReset() {
  mode_ = Mode::Main;
  mainIndex_ = 0;
  appIndex_ = 0;
  message_ = "";
  rendered_ = false;
  loadConfig();
  syncAppIndexToConfig();
  markDirty();
}

void AutoLaunchSettingsApp::loadConfig() {
  config_ = AutoLaunchSettings::load();
  if (config_.appTitle.length() == 0 && appCount_ > 0 && apps_[0] != nullptr) {
    config_.appTitle = apps_[0]->appTitle();
  }
  updateMainLabels();
}

void AutoLaunchSettingsApp::saveConfig() {
  AutoLaunchSettings::save(config_);
  updateMainLabels();
}

void AutoLaunchSettingsApp::markDirty() {
  dirty_ = true;
}

void AutoLaunchSettingsApp::updateMainLabels() {
  snprintf(mainLabels_[0], sizeof(mainLabels_[0]), "Enabled: %s", config_.enabled ? "On" : "Off");
  snprintf(mainLabels_[1], sizeof(mainLabels_[1]), "App: %s", config_.appTitle.length() == 0 ? "(none)" : config_.appTitle.c_str());
  snprintf(mainLabels_[2], sizeof(mainLabels_[2]), "Auto-run: %s", config_.autoRun ? "On" : "Off");
  snprintf(mainLabels_[3], sizeof(mainLabels_[3]), "Back");
}

const char* AutoLaunchSettingsApp::mainLabel(uint8_t index) const {
  if (index >= MAIN_COUNT) return "";
  return mainLabels_[index];
}

const char* AutoLaunchSettingsApp::appLabel(uint8_t index) const {
  if (index == appCount_) return "Back";
  if (apps_ == nullptr || index >= appCount_ || apps_[index] == nullptr) return "";
  return apps_[index]->appTitle();
}

void AutoLaunchSettingsApp::syncAppIndexToConfig() {
  appIndex_ = 0;
  if (apps_ == nullptr || appCount_ == 0) return;
  for (uint8_t i = 0; i < appCount_; i++) {
    if (apps_[i] != nullptr && config_.appTitle.equalsIgnoreCase(apps_[i]->appTitle())) {
      appIndex_ = i;
      return;
    }
  }
}

void AutoLaunchSettingsApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;

  if (mode_ == Mode::Message) {
    if (b1.click || b2.click || b1.longPress) {
      mode_ = Mode::Main;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::SelectApp) {
    const uint8_t count = appCount_ + 1;
    if (b1.click && count > 0) {
      appIndex_ = (appIndex_ + 1) % count;
      markDirty();
    }
    if (b2.click && count > 0) {
      appIndex_ = appIndex_ == 0 ? count - 1 : appIndex_ - 1;
      markDirty();
    }
    if (b1.longPress) {
      if (appIndex_ == appCount_) {
        mode_ = Mode::Main;
      } else if (apps_ != nullptr && appIndex_ < appCount_ && apps_[appIndex_] != nullptr) {
        config_.appTitle = apps_[appIndex_]->appTitle();
        saveConfig();
        message_ = "App saved";
        mode_ = Mode::Message;
      }
      markDirty();
    }
    return;
  }

  if (b2.click) {
    requestExitToMenu();
    return;
  }

  if (b1.click) {
    mainIndex_ = (mainIndex_ + 1) % MAIN_COUNT;
    markDirty();
  }

  if (b1.longPress) {
    if (mainIndex_ == 0) {
      config_.enabled = !config_.enabled;
      saveConfig();
      message_ = config_.enabled ? "Autolaunch on" : "Autolaunch off";
      mode_ = Mode::Message;
    } else if (mainIndex_ == 1) {
      syncAppIndexToConfig();
      mode_ = Mode::SelectApp;
    } else if (mainIndex_ == 2) {
      config_.autoRun = !config_.autoRun;
      saveConfig();
      message_ = config_.autoRun ? "Auto-run on" : "Auto-run off";
      mode_ = Mode::Message;
    } else {
      requestExitToMenu();
    }
    markDirty();
  }
}

void AutoLaunchSettingsApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) return;
  dirty_ = false;
  rendered_ = true;

  const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
  if (mode_ == Mode::Main) {
    updateMainLabels();
    const TDisplayUi::MenuStyle style = TDisplayUi::menuStyle(textSize);
    const uint8_t first = TDisplayUi::menuScrollOffset(mainIndex_, MAIN_COUNT, style);
    drawBuffered(tft, width, height, [&](auto& canvas) {
      TDisplayUi::menuFrame(canvas, "Autolaunch", mainIndex_, MAIN_COUNT, first,
                            [this](uint8_t index) -> const char* { return mainLabel(index); },
                            "B1 next/open  B2 back", textSize, TFT_CYAN);
    });
    renderedMode_ = mode_;
    return;
  }

  if (mode_ == Mode::SelectApp) {
    const uint8_t count = appCount_ + 1;
    const TDisplayUi::MenuStyle style = TDisplayUi::menuStyle(textSize);
    const uint8_t first = TDisplayUi::menuScrollOffset(appIndex_, count, style);
    drawBuffered(tft, width, height, [&](auto& canvas) {
      TDisplayUi::menuFrame(canvas, "Choose App", appIndex_, count, first,
                            [this](uint8_t index) -> const char* { return appLabel(index); },
                            "B1 next/open  B2 prev", textSize, TFT_CYAN);
    });
    renderedMode_ = mode_;
    return;
  }

  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::menuShell(canvas, "Autolaunch", 0, 0, textSize);
    TDisplayUi::centered(canvas, message_, 52, 2, TFT_GREEN);
    TDisplayUi::centered(canvas, "Auto-run affects apps that support it.", 82, 1, TFT_LIGHTGREY);
    TDisplayUi::menuFooter(canvas, "B1 continue  B2 back", textSize);
  });
  renderedMode_ = mode_;
}
