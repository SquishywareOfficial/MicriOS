#include <NimBLEUUID.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_timer.h>

#include "App.h"
#include "CydFramebuffer.h"
#include "CydHardware.h"
#include "CydIcons.h"
#include "CydKidModeUi.h"
#include "CydUi.h"
#include "src/shared/Version.h"
#include "src/shared/logic/KidModeStorage.h"
#include "src/shared/logic/SystemControls.h"
#include "src/shared/logic/TouchUi.h"

#include "src/apps/AutoLaunchSettingsApp.h"
#include "src/apps/ClockApp.h"
#include "src/apps/CoinFlipperApp.h"
#include "src/apps/CommunicatorApp.h"
#include "src/apps/CountdownApp.h"
#include "src/apps/CounterApp.h"
#include "src/apps/CreditsApp.h"
#include "src/apps/CydDeviceSettingsApp.h"
#include "src/apps/DiceRollerApp.h"
#include "src/apps/DistributedMinerApp.h"
#include "src/apps/EspContactsApp.h"
#include "src/apps/MetronomeApp.h"
#include "src/apps/MediaPlayerApp.h"
#include "src/apps/MicriMinerApp.h"
#include "src/apps/MouseEmulatorApp.h"
#include "src/apps/OptionsApp.h"
#include "src/apps/PetSimulatorApp.h"
#include "src/apps/RandomNumberApp.h"
#include "src/apps/ReadingApp.h"
#include "src/apps/ScreenSaverApp.h"
#include "src/apps/StopwatchApp.h"
#include "src/apps/WiFiSetupApp.h"

#include "src/games/AlienRaidersGame.h"
#include "src/games/Breakout76Game.h"
#include "src/games/CaveChopperGame.h"
#include "src/games/CityRacerGame.h"
#include "src/games/FishingFlickGame.h"
#include "src/games/KnifeThrowGame.h"
#include "src/games/MazeRunnerGame.h"
#include "src/games/MicriCasinoGame.h"
#include "src/games/MicriFieldGame.h"
#include "src/games/MiniLanderGame.h"
#include "src/games/NeedSpeedGame.h"
#include "src/games/NoonShooterGame.h"
#include "src/games/NuclearReactorGame.h"
#include "src/games/PipeManiaGame.h"
#include "src/games/SimonGame.h"
#include "src/games/TinyGolfGame.h"
#include "src/games/TongItsGame.h"
#include "src/games/TowerStackerGame.h"

static NimBLEUUID nimbleUuidLinkAnchor(static_cast<uint16_t>(0x1812));

TFT_eSPI tft;

constexpr uint16_t SCREEN_WIDTH = CydHardware::SCREEN_WIDTH;
constexpr uint16_t SCREEN_HEIGHT = CydHardware::CONTENT_HEIGHT;
constexpr uint16_t BOOT_SPLASH_MS = 2000;
constexpr uint16_t CUSTOM_BOOT_SPLASH_MS = 2500;
constexpr uint16_t AUTO_LAUNCH_NOTICE_MS = 2000;

class CydSystemControlProvider final : public SystemControls::Provider {
 public:
  void begin() {
    brightnessLevel_ = CydHardware::loadBrightness();
    volumePercent_ = CydHardware::loadAudioVolume();
    volumeMuted_ = CydHardware::loadAudioMuted();
  }

  void refreshBrightness() {
    brightnessLevel_ = CydHardware::loadBrightness();
  }

  void restoreBrightness() {
    refreshBrightness();
    CydHardware::setBrightness(brightnessLevel_);
  }

  SystemControls::State state(SystemControls::Control control) const override {
    if (control == SystemControls::Control::Brightness) {
      return {true, true,
              SystemControls::percentFromLevel(brightnessLevel_, 1, 16)};
    }
    if (control == SystemControls::Control::Volume) {
      return {true, true, volumePercent_, volumeMuted_};
    }
    return {};
  }

  void preview(SystemControls::Control control, uint8_t percent) override {
    if (control == SystemControls::Control::Brightness) {
      const uint8_t next = SystemControls::levelFromPercent(percent, 1, 16);
      if (next == brightnessLevel_) return;
      brightnessLevel_ = next;
      CydHardware::setBrightness(brightnessLevel_);
    } else if (control == SystemControls::Control::Volume) {
      volumePercent_ = SystemControls::clampPercent(percent);
      CydHardware::previewAudioVolume(volumePercent_);
    }
  }

  void commit(SystemControls::Control control, uint8_t percent) override {
    preview(control, percent);
    if (control == SystemControls::Control::Brightness) {
      CydHardware::saveBrightness(brightnessLevel_);
    } else if (control == SystemControls::Control::Volume) {
      CydHardware::saveAudioVolume(volumePercent_);
    }
  }

  void setMuted(SystemControls::Control control, bool muted) override {
    if (control != SystemControls::Control::Volume) return;
    volumeMuted_ = muted;
    CydHardware::saveAudioMuted(volumeMuted_);
  }

 private:
  uint8_t brightnessLevel_ = 13;
  uint8_t volumePercent_ = 15;
  bool volumeMuted_ = false;
};

KidMode::Storage kidModeStorage;
KidMode::Service kidModeService;

CounterApp counterApp(SCREEN_WIDTH, SCREEN_HEIGHT);
MouseEmulatorApp mouseEmulatorApp(SCREEN_WIDTH, SCREEN_HEIGHT);
DiceRollerApp diceRollerApp(SCREEN_WIDTH, SCREEN_HEIGHT);
CoinFlipperApp coinFlipperApp(SCREEN_WIDTH, SCREEN_HEIGHT);
RandomNumberApp randomNumberApp(SCREEN_WIDTH, SCREEN_HEIGHT);
ScreenSaverApp screenSaverApp(SCREEN_WIDTH, CydHardware::SCREEN_HEIGHT);
StopwatchApp stopwatchApp(SCREEN_WIDTH, SCREEN_HEIGHT);
ClockApp clockApp(SCREEN_WIDTH, SCREEN_HEIGHT);
MicriMinerApp micriMinerApp(SCREEN_WIDTH, SCREEN_HEIGHT);
DistributedMinerApp distributedMinerApp(SCREEN_WIDTH, SCREEN_HEIGHT);
CountdownApp countdownApp(SCREEN_WIDTH, SCREEN_HEIGHT);
MetronomeApp metronomeApp(SCREEN_WIDTH, SCREEN_HEIGHT);
WiFiSetupApp wifiSetupApp(SCREEN_WIDTH, SCREEN_HEIGHT);
EspContactsApp espContactsApp(SCREEN_WIDTH, SCREEN_HEIGHT);
CommunicatorApp communicatorApp(SCREEN_WIDTH, SCREEN_HEIGHT);
CreditsApp creditsApp(SCREEN_WIDTH, SCREEN_HEIGHT);
OptionsApp optionsApp(SCREEN_WIDTH, SCREEN_HEIGHT, kidModeService,
                      kidModeStorage);
AutoLaunchSettingsApp autoLaunchSettingsApp(SCREEN_WIDTH, SCREEN_HEIGHT);
CydDeviceSettingsApp cydDeviceSettingsApp(SCREEN_WIDTH, SCREEN_HEIGHT);
PetSimulatorApp petSimulatorApp(SCREEN_WIDTH, SCREEN_HEIGHT);
ReadingApp readingApp(SCREEN_WIDTH, SCREEN_HEIGHT);
MediaPlayerApp mediaPlayerApp(SCREEN_WIDTH, SCREEN_HEIGHT);

AlienRaidersGame alienRaidersGame(SCREEN_WIDTH, SCREEN_HEIGHT);
MicriCasinoGame micriCasinoGame(SCREEN_WIDTH, SCREEN_HEIGHT);
Breakout76Game breakout76Game(SCREEN_WIDTH, SCREEN_HEIGHT);
CaveChopperGame caveChopperGame(SCREEN_WIDTH, SCREEN_HEIGHT);
CityRacerGame cityRacerGame(SCREEN_WIDTH, SCREEN_HEIGHT);
MicriFieldGame micriFieldGame(SCREEN_WIDTH, SCREEN_HEIGHT);
FishingFlickGame fishingFlickGame(SCREEN_WIDTH, SCREEN_HEIGHT);
KnifeThrowGame knifeThrowGame(SCREEN_WIDTH, SCREEN_HEIGHT);
MazeRunnerGame mazeRunnerGame(SCREEN_WIDTH, SCREEN_HEIGHT, 0);
MazeRunnerGame mazeCollectorGame(SCREEN_WIDTH, SCREEN_HEIGHT, 0, true);
MiniLanderGame miniLanderGame(SCREEN_WIDTH, SCREEN_HEIGHT);
NeedSpeedGame needSpeedGame(SCREEN_WIDTH, SCREEN_HEIGHT);
NoonShooterGame noonShooterGame(SCREEN_WIDTH, SCREEN_HEIGHT);
NuclearReactorGame nuclearReactorGame(SCREEN_WIDTH, SCREEN_HEIGHT);
PipeManiaGame pipeManiaGame(SCREEN_WIDTH, SCREEN_HEIGHT);
SimonGame simonGame(SCREEN_WIDTH, SCREEN_HEIGHT);
TinyGolfGame tinyGolfGame(SCREEN_WIDTH, SCREEN_HEIGHT);
TongItsGame tongItsGame(SCREEN_WIDTH, SCREEN_HEIGHT);
TowerStackerGame towerStackerGame(SCREEN_WIDTH, SCREEN_HEIGHT);

enum class MenuView : uint8_t { Root, Games, Apps, Settings };
enum class MenuAction : uint8_t { OpenGames, OpenApps, OpenSettings, Launch };

struct MenuEntry {
  const char* label;
  MenuAction action;
  App* app;
  CydIcons::Icon icon;
};

MenuEntry rootMenu[] = {
    {"Games", MenuAction::OpenGames, nullptr, CydIcons::Icon::Games},
    {"Apps", MenuAction::OpenApps, nullptr, CydIcons::Icon::Apps},
    {"Settings", MenuAction::OpenSettings, nullptr, CydIcons::Icon::Settings},
    {"About", MenuAction::Launch, &creditsApp, CydIcons::Icon::About},
};

MenuEntry gamesMenu[] = {
    {nullptr, MenuAction::Launch, &alienRaidersGame, CydIcons::Icon::AlienRaiders},
    {nullptr, MenuAction::Launch, &breakout76Game, CydIcons::Icon::Breakout},
    {nullptr, MenuAction::Launch, &caveChopperGame, CydIcons::Icon::CaveChopper},
    {nullptr, MenuAction::Launch, &cityRacerGame, CydIcons::Icon::CityRacer},
    {nullptr, MenuAction::Launch, &micriCasinoGame, CydIcons::Icon::Casino},
    {nullptr, MenuAction::Launch, &micriFieldGame, CydIcons::Icon::Field},
    {nullptr, MenuAction::Launch, &fishingFlickGame, CydIcons::Icon::Fishing},
    {nullptr, MenuAction::Launch, &knifeThrowGame, CydIcons::Icon::Knife},
    {nullptr, MenuAction::Launch, &mazeCollectorGame, CydIcons::Icon::MazeCollector},
    {nullptr, MenuAction::Launch, &mazeRunnerGame, CydIcons::Icon::MazeRunner},
    {nullptr, MenuAction::Launch, &miniLanderGame, CydIcons::Icon::Lander},
    {nullptr, MenuAction::Launch, &needSpeedGame, CydIcons::Icon::Speed},
    {nullptr, MenuAction::Launch, &noonShooterGame, CydIcons::Icon::Shooter},
    {nullptr, MenuAction::Launch, &petSimulatorApp, CydIcons::Icon::Pet},
    {nullptr, MenuAction::Launch, &pipeManiaGame, CydIcons::Icon::Pipe},
    {nullptr, MenuAction::Launch, &nuclearReactorGame, CydIcons::Icon::Reactor},
    {nullptr, MenuAction::Launch, &simonGame, CydIcons::Icon::Simon},
    {nullptr, MenuAction::Launch, &tinyGolfGame, CydIcons::Icon::Golf},
    {nullptr, MenuAction::Launch, &towerStackerGame, CydIcons::Icon::Tower},
};

MenuEntry appsMenu[] = {
    {nullptr, MenuAction::Launch, &clockApp, CydIcons::Icon::Clock},
    {nullptr, MenuAction::Launch, &stopwatchApp, CydIcons::Icon::Stopwatch},
    {nullptr, MenuAction::Launch, &countdownApp, CydIcons::Icon::Countdown},
    {nullptr, MenuAction::Launch, &counterApp, CydIcons::Icon::Counter},
    {nullptr, MenuAction::Launch, &diceRollerApp, CydIcons::Icon::Dice},
    {nullptr, MenuAction::Launch, &coinFlipperApp, CydIcons::Icon::Coin},
    {nullptr, MenuAction::Launch, &randomNumberApp, CydIcons::Icon::Random},
    {nullptr, MenuAction::Launch, &screenSaverApp, CydIcons::Icon::Screensaver},
    {nullptr, MenuAction::Launch, &metronomeApp, CydIcons::Icon::Metronome},
    {nullptr, MenuAction::Launch, &micriMinerApp, CydIcons::Icon::Bitcoin},
    {nullptr, MenuAction::Launch, &distributedMinerApp, CydIcons::Icon::Cluster},
    {nullptr, MenuAction::Launch, &espContactsApp, CydIcons::Icon::Contacts},
    {nullptr, MenuAction::Launch, &communicatorApp, CydIcons::Icon::Communicator},
    {nullptr, MenuAction::Launch, &mouseEmulatorApp, CydIcons::Icon::Mouse},
    {nullptr, MenuAction::Launch, &readingApp, CydIcons::Icon::Reading},
    {nullptr, MenuAction::Launch, &mediaPlayerApp, CydIcons::Icon::Media},
};

MenuEntry settingsMenu[] = {
    {nullptr, MenuAction::Launch, &cydDeviceSettingsApp, CydIcons::Icon::Device},
    {nullptr, MenuAction::Launch, &optionsApp, CydIcons::Icon::Options},
    {nullptr, MenuAction::Launch, &autoLaunchSettingsApp, CydIcons::Icon::Autolaunch},
    {nullptr, MenuAction::Launch, &wifiSetupApp, CydIcons::Icon::Wifi},
};

App* autoLaunchChoices[] = {
    &alienRaidersGame, &breakout76Game, &caveChopperGame, &cityRacerGame,
    &micriCasinoGame, &micriFieldGame, &fishingFlickGame, &knifeThrowGame,
    &mazeCollectorGame, &mazeRunnerGame, &miniLanderGame, &needSpeedGame,
    &noonShooterGame, &petSimulatorApp, &pipeManiaGame, &nuclearReactorGame,
    &simonGame, &tinyGolfGame, &tongItsGame, &towerStackerGame,
    &clockApp, &stopwatchApp, &countdownApp, &counterApp, &diceRollerApp,
    &coinFlipperApp, &randomNumberApp, &screenSaverApp, &metronomeApp,
    &micriMinerApp, &distributedMinerApp, &espContactsApp, &communicatorApp,
    &mouseEmulatorApp, &readingApp, &mediaPlayerApp,
};

constexpr uint16_t ROOT_COUNT = sizeof(rootMenu) / sizeof(rootMenu[0]);
constexpr uint16_t GAMES_COUNT = sizeof(gamesMenu) / sizeof(gamesMenu[0]);
constexpr uint16_t APPS_COUNT = sizeof(appsMenu) / sizeof(appsMenu[0]);
constexpr uint16_t SETTINGS_COUNT = sizeof(settingsMenu) / sizeof(settingsMenu[0]);
constexpr uint8_t AUTO_LAUNCH_COUNT =
    sizeof(autoLaunchChoices) / sizeof(autoLaunchChoices[0]);

MenuView currentMenu = MenuView::Root;
MenuView activeReturnMenu = MenuView::Root;
uint16_t menuPage = 0;
bool menuDirty = true;
bool menuTouchLockedUntilRelease = false;
App* activeApp = nullptr;
bool returnToCasinoAfterTongIts = false;
bool appRenderDue = false;
uint32_t nextAppRenderMs = 0;
AppPhase lastRenderedAppPhase = AppPhase::Start;

TouchUi::Tracker touchTracker;
TouchUi::TouchSample touchSample;
TouchUi::TouchEvent touchEvent;

bool bootSplashActive = true;
bool customBootSplashActive = false;
bool bootReleasePending = false;
bool autoLaunchAttempted = false;
bool autoLaunchNoticeActive = false;
uint32_t bootStartedAtMs = 0;
uint32_t customBootSplashStartedAtMs = 0;
uint32_t autoLaunchNoticeStartedAtMs = 0;
String pendingAutoLaunchTitle;
bool pendingAutoLaunchAutoRun = false;
String serialCommand;
CydSystemControlProvider systemControlProvider;
SystemControls::Control openSystemControl = SystemControls::Control::None;
bool systemControlAdjusting = false;
bool systemBarDirty = true;
bool lastImmersiveMode = false;
bool immersiveExitVisible = false;
uint32_t immersiveExitHideAtMs = 0;
uint8_t pendingSystemControlPercent = 0;
uint32_t systemControlLastInteractionMs = 0;
bool systemControlOverlayDrawn = false;
uint32_t lastSystemControlDrawMs = 0;
KidMode::SplashSettings bootChildSplashSettings;

enum class ChildShellScreen : uint8_t {
  None,
  Pin,
  Duration,
  Expired
};

ChildShellScreen childShellScreen = ChildShellScreen::None;
KidMode::PinEntry childShellPin;
KidMode::AuthLockout childShellLockout;
TouchUi::ControlCapture childShellCapture;
String childShellPrompt = "Parent PIN required";
String childTimeLabel;
bool childShellDirty = false;
uint32_t childRenderedCountdown = UINT32_MAX;
uint64_t childExpiredSleepAtUs = 0;
uint8_t childBadgeTapCount = 0;
uint32_t childBadgeFirstTapMs = 0;

constexpr uint8_t CHILD_BADGE_LOCK_TAPS = 3;
constexpr uint32_t CHILD_BADGE_TAP_WINDOW_MS = 1500;

MenuEntry* currentEntries() {
  switch (currentMenu) {
    case MenuView::Games: return gamesMenu;
    case MenuView::Apps: return appsMenu;
    case MenuView::Settings: return settingsMenu;
    default: return rootMenu;
  }
}

uint16_t currentCount() {
  switch (currentMenu) {
    case MenuView::Games: return GAMES_COUNT;
    case MenuView::Apps: return APPS_COUNT;
    case MenuView::Settings: return SETTINGS_COUNT;
    default: return ROOT_COUNT;
  }
}

const char* currentTitle() {
  switch (currentMenu) {
    case MenuView::Games: return "Games";
    case MenuView::Apps: return "Apps";
    case MenuView::Settings: return "Settings";
    default: return "MicriOS";
  }
}

const char* displayedSystemBarTitle() {
  return activeApp == nullptr ? currentTitle() : activeApp->appTitle();
}

bool systemBarShowsBack() {
  return activeApp != nullptr || currentMenu != MenuView::Root;
}

bool activeAppIsImmersive() {
  return activeApp != nullptr && activeApp->prefersImmersiveMode();
}

void closeSystemControl() {
  openSystemControl = SystemControls::Control::None;
  systemControlAdjusting = false;
  systemControlOverlayDrawn = false;
  systemBarDirty = true;
}

void openSystemControlOverlay(SystemControls::Control control,
                              uint32_t nowMs) {
  if (control == SystemControls::Control::Brightness) {
    systemControlProvider.refreshBrightness();
  }
  openSystemControl = control;
  const auto state = systemControlProvider.state(control);
  pendingSystemControlPercent =
      state.percent < 0 ? 0 : SystemControls::clampPercent(state.percent);
  systemControlLastInteractionMs = nowMs;
  systemControlOverlayDrawn = false;
  systemBarDirty = true;
}

void drawSystemBarIfDirty() {
  if (!systemBarDirty) return;
  const uint32_t nowMs = millis();
  if (systemControlAdjusting &&
      nowMs - lastSystemControlDrawMs < 32) return;
  systemBarDirty = false;
  if (activeAppIsImmersive()) {
    if (immersiveExitVisible) CydUi::immersiveExitBar(tft);
    return;
  }
  if (openSystemControl != SystemControls::Control::None) {
    const auto state = systemControlProvider.state(openSystemControl);
    if (!systemControlOverlayDrawn) {
      CydUi::systemControlOverlay(tft, openSystemControl, state);
      systemControlOverlayDrawn = true;
    } else {
      CydUi::systemControlTrack(tft, state);
    }
    lastSystemControlDrawMs = nowMs;
    return;
  }
  CydUi::systemBar(
      tft, displayedSystemBarTitle(), systemBarShowsBack(),
      activeApp == nullptr ? "Back" : "Exit",
      systemControlProvider.state(SystemControls::Control::Brightness),
      systemControlProvider.state(SystemControls::Control::Volume),
      systemControlProvider.state(SystemControls::Control::Battery),
      childTimeLabel.length() == 0 ? nullptr : childTimeLabel.c_str());
}

bool updateImmersiveChromeTouch(uint32_t nowMs) {
  if (!activeAppIsImmersive()) return false;

  if (immersiveExitVisible && nowMs >= immersiveExitHideAtMs &&
      !touchSample.down) {
    immersiveExitVisible = false;
    appRenderDue = true;
    return false;
  }

  if (immersiveExitVisible) {
    if (touchEvent.tap &&
        CydUi::immersiveExitRect().contains(touchEvent.point)) {
      activeApp->exitToMenu();
      return true;
    }
    if (touchEvent.tap) {
      immersiveExitVisible = false;
      appRenderDue = true;
      return true;
    }
    return touchSample.down && touchSample.point.y >= CydUi::H;
  }

  // The lower edge is a hidden reveal gesture, not a persistent overlay.
  if (touchEvent.tap && touchEvent.point.y >= CydUi::H) {
    immersiveExitVisible = true;
    immersiveExitHideAtMs = nowMs + 2500;
    systemBarDirty = true;
    return true;
  }
  return false;
}

bool updateSystemBarTouch(uint32_t nowMs) {
  const auto bar = CydUi::systemBarRect();

  if (childBadgeTapCount > 0 &&
      nowMs - childBadgeFirstTapMs > CHILD_BADGE_TAP_WINDOW_MS) {
    childBadgeTapCount = 0;
  }

  if (openSystemControl != SystemControls::Control::None) {
    const auto state = systemControlProvider.state(openSystemControl);
    const uint32_t timeout = state.implemented
                                 ? SystemControls::OVERLAY_TIMEOUT_MS
                                 : SystemControls::UNIMPLEMENTED_TIMEOUT_MS;
    if (!touchSample.down &&
        nowMs - systemControlLastInteractionMs >= timeout) {
      closeSystemControl();
      return false;
    }

    if (touchEvent.tap &&
        CydUi::systemControlCloseRect().contains(touchEvent.point)) {
      closeSystemControl();
      return true;
    }

    if (openSystemControl == SystemControls::Control::Volume &&
        touchEvent.tap &&
        CydUi::systemControlOverlayIconRect().contains(touchEvent.point)) {
      systemControlProvider.setMuted(SystemControls::Control::Volume,
                                     !state.muted);
      systemControlLastInteractionMs = nowMs;
      systemControlOverlayDrawn = false;
      systemBarDirty = true;
      return true;
    }

    if (state.implemented && state.adjustable) {
      const auto track = CydUi::systemControlTrackRect();
      if (touchSample.down && track.contains(touchSample.point)) {
        pendingSystemControlPercent = SystemControls::percentFromTrack(
            touchSample.point.x, track.x + 1, track.w - 2);
        systemControlProvider.preview(openSystemControl,
                                      pendingSystemControlPercent);
        systemControlAdjusting = true;
        systemControlLastInteractionMs = nowMs;
        systemBarDirty = true;
        return true;
      }
      if (touchEvent.released && systemControlAdjusting) {
        systemControlProvider.commit(openSystemControl,
                                     pendingSystemControlPercent);
        systemControlAdjusting = false;
        systemControlLastInteractionMs = nowMs;
        systemBarDirty = true;
        return true;
      }
    } else if (touchEvent.tap && bar.contains(touchEvent.point)) {
      closeSystemControl();
      return true;
    }

    return (touchSample.down && bar.contains(touchSample.point)) ||
           (touchEvent.released && bar.contains(touchEvent.point));
  }

  if (!touchEvent.tap) return false;
  if (childTimeLabel.length() != 0 &&
      CydUi::childModeBadgeRect().contains(touchEvent.point)) {
    if (childBadgeTapCount == 0) childBadgeFirstTapMs = nowMs;
    ++childBadgeTapCount;
    if (childBadgeTapCount >= CHILD_BADGE_LOCK_TAPS) {
      childBadgeTapCount = 0;
      kidModeService.lockNow();
    }
    return true;
  }

  // Any other system-bar action abandons an incomplete lock gesture.
  childBadgeTapCount = 0;
  const SystemControls::Control controls[] = {
      SystemControls::Control::Brightness,
      SystemControls::Control::Volume,
      SystemControls::Control::Battery};
  for (const auto control : controls) {
    if (CydUi::systemControlRect(control).contains(touchEvent.point)) {
      openSystemControlOverlay(control, nowMs);
      return true;
    }
  }
  return false;
}

const char* entryTitle(const MenuEntry& entry) {
  return entry.app == nullptr ? entry.label : entry.app->appTitle();
}

App* findAppIn(MenuEntry* entries, uint16_t count, const String& title) {
  for (uint16_t i = 0; i < count; ++i) {
    if (entries[i].app != nullptr &&
        title.equalsIgnoreCase(entries[i].app->appTitle())) return entries[i].app;
  }
  return nullptr;
}

App* findAppByTitle(const String& title) {
  App* app = findAppIn(rootMenu, ROOT_COUNT, title);
  if (app == nullptr) app = findAppIn(gamesMenu, GAMES_COUNT, title);
  if (app == nullptr) app = findAppIn(appsMenu, APPS_COUNT, title);
  if (app == nullptr) app = findAppIn(settingsMenu, SETTINGS_COUNT, title);
  return app;
}

void openMenu(MenuView view) {
  closeSystemControl();
  currentMenu = view;
  menuPage = 0;
  menuDirty = true;
  systemBarDirty = true;
}

void launchApp(App& app, uint32_t nowMs, bool fromAutoLaunch = false,
               bool autoRun = false) {
  closeSystemControl();
  activeReturnMenu = currentMenu;
  activeApp = &app;
  CydHardware::applyDisplayOrientation(tft);
  if (CydFramebuffer::acquire(tft, SCREEN_WIDTH, SCREEN_HEIGHT) == nullptr) {
    Serial.println("[cyd] warning: content framebuffer unavailable");
  }
  if (&app == &micriMinerApp) {
    micriMinerApp.setDebugAutoStart(fromAutoLaunch && autoRun);
  }
  activeApp->begin(nowMs, false, false);
  appRenderDue = true;
  nextAppRenderMs = 0;
  lastRenderedAppPhase = activeApp->phase();
  lastImmersiveMode = activeApp->prefersImmersiveMode();
  immersiveExitVisible = false;
  tft.fillScreen(TFT_BLACK);
  systemBarDirty = true;
}

bool launchAppByTitle(const String& title, uint32_t nowMs,
                      bool fromAutoLaunch = false, bool autoRun = false) {
  App* app = findAppByTitle(title);
  if (app == nullptr || activeApp != nullptr) return false;
  launchApp(*app, nowMs, fromAutoLaunch, autoRun);
  return true;
}

void activateEntry(uint16_t index, uint32_t nowMs) {
  if (index >= currentCount()) return;
  const MenuEntry& entry = currentEntries()[index];
  switch (entry.action) {
    case MenuAction::OpenGames: openMenu(MenuView::Games); break;
    case MenuAction::OpenApps: openMenu(MenuView::Apps); break;
    case MenuAction::OpenSettings: openMenu(MenuView::Settings); break;
    case MenuAction::Launch:
      if (entry.app != nullptr) launchApp(*entry.app, nowMs);
      break;
  }
}

TouchUi::GridMetrics currentGrid() {
  return currentMenu == MenuView::Root ? CydUi::rootGrid() : CydUi::submenuGrid();
}

void changePage(int8_t direction) {
  const uint16_t pages = TouchUi::pageCount(currentCount(), currentGrid().capacity());
  if (pages <= 1) return;
  if (direction > 0) menuPage = (menuPage + 1) % pages;
  else menuPage = menuPage == 0 ? pages - 1 : menuPage - 1;
  menuDirty = true;
}

void updateMenuTouch(uint32_t nowMs) {
  if (menuTouchLockedUntilRelease) {
    if (!touchSample.down) menuTouchLockedUntilRelease = false;
    return;
  }

  if (touchEvent.swipe == TouchUi::SwipeDirection::Left) changePage(1);
  if (touchEvent.swipe == TouchUi::SwipeDirection::Right) changePage(-1);
  if (!touchEvent.tap) return;

  if (currentMenu != MenuView::Root && CydUi::backRect().contains(touchEvent.point)) {
    openMenu(MenuView::Root);
    return;
  }
  if (CydUi::menuPreviousPageRect().contains(touchEvent.point)) {
    changePage(-1);
    return;
  }
  if (CydUi::menuNextPageRect().contains(touchEvent.point)) {
    changePage(1);
    return;
  }

  const auto grid = currentGrid();
  const uint16_t offset = TouchUi::pageOffset(menuPage, grid.capacity());
  const int16_t index = TouchUi::cardAt(grid, currentCount(), offset,
                                        touchEvent.point);
  if (index >= 0) activateEntry(static_cast<uint16_t>(index), nowMs);
}

uint16_t cardAccent(uint16_t index) {
  constexpr uint16_t COLORS[] = {TFT_CYAN, TFT_GREEN, TFT_ORANGE,
                                 TFT_MAGENTA, TFT_YELLOW, TFT_SKYBLUE};
  return COLORS[index % (sizeof(COLORS) / sizeof(COLORS[0]))];
}

void drawMenu() {
  if (menuDirty) {
    menuDirty = false;
    const auto grid = currentGrid();
    const uint16_t count = currentCount();
    const uint16_t pages = TouchUi::pageCount(count, grid.capacity());
    if (menuPage >= pages) menuPage = 0;
    const uint16_t offset = TouchUi::pageOffset(menuPage, grid.capacity());

    CydFramebuffer::draw(tft, SCREEN_WIDTH, SCREEN_HEIGHT, [&](auto& canvas) {
      CydUi::clear(canvas);
      CydUi::header(
          canvas, currentTitle(), TFT_GREEN,
          currentMenu == MenuView::Root
              ? BuildInfo::BUILD_TEXT
              : (pages > 1 ? "" : nullptr));
      for (uint8_t slot = 0; slot < grid.capacity(); ++slot) {
        const uint16_t index = offset + slot;
        if (index >= count) break;
        CydUi::touchCard(canvas, grid.cardRect(slot),
                         entryTitle(currentEntries()[index]),
                         currentEntries()[index].icon, cardAccent(index));
      }
      CydUi::menuPager(canvas, menuPage, pages);
    });
  }
  drawSystemBarIfDirty();
}

TouchUi::ControlProfile controlProfileFor(const App* app) {
  using TouchUi::ControlMode;
  using TouchUi::LogicalButton;
  if (app == &alienRaidersGame) {
    return {ControlMode::SplitButtons, LogicalButton::B1, LogicalButton::B2,
            TouchUi::SplitAxis::Y};
  }
  if (app == &cityRacerGame) {
    return {ControlMode::SplitButtons, LogicalButton::B1, LogicalButton::B2,
            TouchUi::SplitAxis::Y};
  }
  if (app == &breakout76Game) {
    return {ControlMode::SplitButtons, LogicalButton::B2, LogicalButton::B1};
  }
  if (app == &micriCasinoGame || app == &tongItsGame ||
      app == &mazeRunnerGame || app == &mazeCollectorGame ||
      app == &pipeManiaGame || app == &petSimulatorApp ||
      app == &clockApp || app == &stopwatchApp ||
      app == &countdownApp || app == &counterApp || app == &diceRollerApp ||
      app == &coinFlipperApp || app == &randomNumberApp ||
      app == &screenSaverApp || app == &metronomeApp ||
      app == &micriMinerApp || app == &distributedMinerApp ||
      app == &espContactsApp || app == &communicatorApp ||
      app == &mouseEmulatorApp || app == &readingApp ||
      app == &mediaPlayerApp ||
      app == &wifiSetupApp || app == &optionsApp || app == &creditsApp ||
      app == &autoLaunchSettingsApp || app == &cydDeviceSettingsApp) {
    return {ControlMode::Direct, LogicalButton::None, LogicalButton::None};
  }
  return {ControlMode::OneButton, LogicalButton::B1, LogicalButton::B2};
}

void exitActiveApp(uint32_t nowMs) {
  closeSystemControl();
  activeApp->clearExitRequest();
  activeApp = nullptr;
  currentMenu = activeReturnMenu;
  menuPage = 0;
  menuDirty = true;
  menuTouchLockedUntilRelease = true;
  CydHardware::applyDisplayOrientation(tft);
  tft.fillScreen(TFT_BLACK);
  touchTracker.reset(touchSample, nowMs);
  systemBarDirty = true;
  lastImmersiveMode = false;
  immersiveExitVisible = false;
}

void refreshChildTimeLabel(uint64_t nowUs) {
  String next;
  if (kidModeService.isUnlimited()) {
    next = "Kid INF";
  } else if (kidModeService.state() == KidMode::State::Timed) {
    const uint32_t seconds = kidModeService.remainingSeconds(nowUs);
    if (seconds < 60) {
      next = "Kid <1m";
    } else {
      const uint32_t minutes = (seconds + 59U) / 60U;
      if (minutes == 120) next = "Kid 2h";
      else if (minutes == 60) next = "Kid 1h";
      else next = String("Kid ") + String(minutes) + "m";
    }
  }
  if (next != childTimeLabel) {
    childTimeLabel = next;
    systemBarDirty = true;
  }
}

void beginChildShellScreen(ChildShellScreen screen, uint64_t nowUs) {
  childShellScreen = screen;
  childShellPin.clear();
  childShellLockout.clear();
  childShellCapture.reset();
  childShellPrompt = "Parent PIN required";
  childRenderedCountdown = UINT32_MAX;
  childExpiredSleepAtUs =
      screen == ChildShellScreen::Expired ? nowUs + 10ULL * 1000000ULL : 0;
  childShellDirty = true;
  menuTouchLockedUntilRelease = true;
  closeSystemControl();
  tft.fillScreen(TFT_BLACK);
}

void forceChildLock(bool expired, uint32_t nowMs, uint64_t nowUs) {
  childBadgeTapCount = 0;
  if (activeApp != nullptr) {
    activeApp->exitToMenu();
    exitActiveApp(nowMs);
  }
  autoLaunchNoticeActive = false;
  pendingAutoLaunchTitle = "";
  returnToCasinoAfterTongIts = false;
  childTimeLabel = "";
  systemBarDirty = true;
  CydHardware::setRgb(0, 0, 0);
  CydHardware::holdAudioIdle();
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  beginChildShellScreen(expired ? ChildShellScreen::Expired
                                : ChildShellScreen::Pin,
                        nowUs);
  if (expired) CydHardware::playNotificationBeep();
}

void drawChildShell(uint64_t nowUs) {
  if (!childShellDirty) return;
  childShellDirty = false;
  CydFramebuffer::draw(tft, CydHardware::SCREEN_WIDTH,
                       CydHardware::SCREEN_HEIGHT, [&](auto& canvas) {
    if (childShellScreen == ChildShellScreen::Pin) {
      if (childShellLockout.active(nowUs)) {
        CydKidModeUi::drawLockout(
            canvas, childShellLockout.remainingSeconds(nowUs), true);
      } else {
        CydKidModeUi::drawPin(canvas, "Child Mode", childShellPrompt.c_str(),
                              childShellPin, "Unlock",
                              childShellCapture.activeId(), true);
      }
    } else if (childShellScreen == ChildShellScreen::Duration) {
      CydKidModeUi::drawDurations(canvas, childShellCapture.activeId(), true);
    } else if (childShellScreen == ChildShellScreen::Expired) {
      const uint32_t remaining = nowUs >= childExpiredSleepAtUs
                                     ? 0
                                     : static_cast<uint32_t>(
                                           (childExpiredSleepAtUs - nowUs +
                                            999999ULL) /
                                           1000000ULL);
      CydKidModeUi::drawExpired(
          canvas, remaining,
          childShellCapture.activeId() == 1);
    }
  });
}

void completeChildUnlock(uint32_t nowMs, uint64_t nowUs) {
  // The expired-session fallback dims the PWM without changing the saved
  // preference. Restore that persisted level before returning to normal UI.
  systemControlProvider.restoreBrightness();
  childShellScreen = ChildShellScreen::None;
  childShellPin.clear();
  childShellLockout.clear();
  childShellCapture.reset();
  childShellDirty = false;
  childRenderedCountdown = UINT32_MAX;
  childExpiredSleepAtUs = 0;
  currentMenu = MenuView::Root;
  menuPage = 0;
  menuDirty = true;
  menuTouchLockedUntilRelease = true;
  CydHardware::applyDisplayOrientation(tft);
  tft.fillScreen(TFT_BLACK);
  touchTracker.reset(touchSample, nowMs);
  refreshChildTimeLabel(nowUs);
  systemBarDirty = true;
}

void handleChildPinAction(int16_t id, uint64_t nowUs) {
  if (id >= 0 && id <= 9) {
    childShellPin.append(static_cast<uint8_t>(id));
    return;
  }
  if (id == CydKidModeUi::PIN_BACKSPACE) {
    childShellPin.backspace();
    return;
  }
  if (id != CydKidModeUi::PIN_SUBMIT || !childShellPin.complete()) return;

  if (kidModeStorage.verify(childShellPin.value())) {
    childShellPin.clear();
    childShellLockout.clear();
    childShellCapture.reset();
    childShellScreen = ChildShellScreen::Duration;
    childShellDirty = true;
    return;
  }

  childShellPin.clear();
  childShellCapture.reset();
  childShellLockout.fail(nowUs);
  childRenderedCountdown = UINT32_MAX;
  childShellDirty = true;
  CydHardware::playNotificationBeep();
}

bool updateChildShell(uint32_t nowMs, uint64_t nowUs) {
  if (childShellScreen == ChildShellScreen::None) return false;

  if (childShellScreen == ChildShellScreen::Pin) {
    const uint32_t seconds = childShellLockout.remainingSeconds(nowUs);
    if (seconds != childRenderedCountdown) {
      childRenderedCountdown = seconds;
      childShellDirty = true;
    }
  } else if (childShellScreen == ChildShellScreen::Expired) {
    const uint32_t seconds = nowUs >= childExpiredSleepAtUs
                                 ? 0
                                 : static_cast<uint32_t>(
                                       (childExpiredSleepAtUs - nowUs +
                                        999999ULL) /
                                       1000000ULL);
    if (seconds != childRenderedCountdown) {
      childRenderedCountdown = seconds;
      childShellDirty = true;
    }
    if (nowUs >= childExpiredSleepAtUs && !touchSample.down) {
      drawChildShell(nowUs);
      if (!CydHardware::enterChildLockDeepSleep()) {
        childShellPrompt = "Sleep unavailable - parent PIN required";
        childShellScreen = ChildShellScreen::Pin;
        childShellCapture.reset();
        childShellDirty = true;
      }
    }
  }

  const TouchUi::Point point = TouchUi::currentPoint(touchSample, touchEvent);
  int16_t hit = TouchUi::NO_CONTROL;
  if (childShellScreen == ChildShellScreen::Pin &&
      !childShellLockout.active(nowUs)) {
    hit = CydKidModeUi::pinHit(point);
  } else if (childShellScreen == ChildShellScreen::Duration) {
    hit = CydKidModeUi::durationHit(point);
  } else if (childShellScreen == ChildShellScreen::Expired &&
             CydKidModeUi::EXPIRED_UNLOCK.contains(point)) {
    hit = 1;
  }

  const auto result = childShellCapture.update(touchSample, touchEvent, hit);
  if (result.pressed || result.released) childShellDirty = true;
  if (result.activated) {
    if (childShellScreen == ChildShellScreen::Pin) {
      handleChildPinAction(result.id, nowUs);
    } else if (childShellScreen == ChildShellScreen::Duration) {
      constexpr uint16_t MINUTES[] = {10, 20, 30, 60, 120};
      const bool unlocked = result.id == 5
                                ? kidModeService.unlockUnlimited()
                                : (result.id >= 0 && result.id < 5 &&
                                   kidModeService.unlockForMinutes(
                                       MINUTES[result.id], nowUs));
      if (unlocked) completeChildUnlock(nowMs, nowUs);
    } else if (childShellScreen == ChildShellScreen::Expired) {
      childShellScreen = ChildShellScreen::Pin;
      childShellPrompt = "Parent PIN required";
      childShellCapture.reset();
      childShellDirty = true;
    }
  }

  drawChildShell(nowUs);
  return true;
}

bool enforceChildMode(uint32_t nowMs, uint64_t nowUs) {
  const KidMode::TickEvent event = kidModeService.tick(nowUs);
  if (event == KidMode::TickEvent::RemainingChanged) {
    refreshChildTimeLabel(nowUs);
  } else if (event == KidMode::TickEvent::Expired) {
    forceChildLock(true, nowMs, nowUs);
  }

  if (kidModeService.isUnlocked()) {
    if (childTimeLabel.length() == 0) refreshChildTimeLabel(nowUs);
  } else if (!kidModeService.enabled() && childTimeLabel.length() != 0) {
    childTimeLabel = "";
    systemBarDirty = true;
  }

  if (kidModeService.isLocked() &&
      childShellScreen == ChildShellScreen::None) {
    forceChildLock(false, nowMs, nowUs);
  }
  return updateChildShell(nowMs, nowUs);
}

void renderActiveAppIfDue(uint32_t nowMs) {
  const AppPhase phase = activeApp->phase();
  const bool immersive = activeApp->prefersImmersiveMode();
  if (phase != lastRenderedAppPhase || immersive != lastImmersiveMode) {
    appRenderDue = true;
    systemBarDirty = true;
    if (immersive != lastImmersiveMode) {
      closeSystemControl();
      immersiveExitVisible = false;
    }
  }
  const uint16_t interval = phase == AppPhase::Running
                                ? activeApp->runningRenderIntervalMs()
                                : activeApp->staticRenderIntervalMs();
  if (!activeApp->wantsImmediateRender() && !appRenderDue &&
      phase == lastRenderedAppPhase &&
      (interval == 0 || nowMs < nextAppRenderMs)) return;

  activeApp->render(tft);
  CydHardware::applyDisplayOrientation(tft);
  appRenderDue = false;
  lastRenderedAppPhase = phase;
  lastImmersiveMode = immersive;
  nextAppRenderMs = interval == 0 ? UINT32_MAX : nowMs + interval;
}

void drawCentered(const String& text, int16_t y, uint8_t size, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(size);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, (SCREEN_WIDTH - tft.textWidth(text)) / 2, y);
}

void drawBootSplash() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRoundRect(18, 16, 284, 192, 12, TFT_DARKGREY);
  tft.drawFastHLine(55, 62, 210, TFT_CYAN);
  drawCentered("MicriDeck", 76, 4, TFT_WHITE);
  drawCentered("CYD", 126, 3, TFT_CYAN);
  drawCentered(String("MicriOS ") + BuildInfo::BUILD_TEXT, 176, 1,
               TFT_LIGHTGREY);
}

void beginCustomBootSplash(uint32_t nowMs) {
  customBootSplashActive = true;
  customBootSplashStartedAtMs = nowMs;
  bootReleasePending = touchSample.down;
  touchTracker.reset(touchSample, nowMs);
  CydKidModeUi::drawCustomSplash(tft, bootChildSplashSettings,
                                 CydHardware::SCREEN_HEIGHT, true);
}

void drawAutoLaunchNotice() {
  tft.fillScreen(TFT_BLACK);
  CydUi::header(tft, "Autolaunch", TFT_CYAN);
  CydUi::centered(tft, "Launching", 65, 2, TFT_LIGHTGREY);
  CydUi::centered(tft, pendingAutoLaunchTitle, 100, 3, TFT_WHITE);
  CydUi::centered(tft, "Tap to cancel", 164, 2, TFT_YELLOW);
}

bool beginAutoLaunch(uint32_t nowMs) {
  if (autoLaunchAttempted) return false;
  autoLaunchAttempted = true;
  const AutoLaunchSettings::Config config = AutoLaunchSettings::load();
  if (!config.enabled || config.appTitle.length() == 0) return false;
  pendingAutoLaunchTitle = config.appTitle;
  pendingAutoLaunchAutoRun = config.autoRun;
  autoLaunchNoticeStartedAtMs = nowMs;
  autoLaunchNoticeActive = true;
  drawAutoLaunchNotice();
  return true;
}

void updateAutoLaunch(uint32_t nowMs) {
  if (touchEvent.pressed || CydHardware::physicalB1Down() ||
      CydHardware::physicalB2Down()) {
    autoLaunchNoticeActive = false;
    pendingAutoLaunchTitle = "";
    menuTouchLockedUntilRelease = true;
    menuDirty = true;
    return;
  }
  if (nowMs - autoLaunchNoticeStartedAtMs < AUTO_LAUNCH_NOTICE_MS) return;
  autoLaunchNoticeActive = false;
  const String title = pendingAutoLaunchTitle;
  pendingAutoLaunchTitle = "";
  if (!launchAppByTitle(title, nowMs, true, pendingAutoLaunchAutoRun)) {
    menuDirty = true;
  }
}

bool collectCalibrationPoint(CydHardware::RawTouchSample& result) {
  uint32_t deadline = millis() + 15000;
  while (millis() < deadline && CydHardware::readRawTouch().down) delay(5);
  while (millis() < deadline) {
    CydHardware::RawTouchSample sample = CydHardware::readRawTouch();
    if (!sample.down) {
      delay(5);
      continue;
    }
    int32_t x = 0, y = 0, z = 0;
    uint8_t count = 0;
    while (sample.down && count < 24) {
      x += sample.x;
      y += sample.y;
      z += sample.pressure;
      ++count;
      delay(8);
      sample = CydHardware::readRawTouch();
    }
    if (count >= 5) {
      result.down = true;
      result.x = x / count;
      result.y = y / count;
      result.pressure = z / count;
      return true;
    }
  }
  return false;
}

void runTouchCalibration() {
  const TouchUi::Point targets[4] = {{28, 52}, {291, 52},
                                     {291, 181}, {28, 181}};
  CydHardware::RawTouchSample samples[4];
  Serial.println("[cyd] touch calibration start");
  for (uint8_t i = 0; i < 4; ++i) {
    tft.fillScreen(TFT_BLACK);
    CydUi::header(tft, "Touch Calibration", TFT_YELLOW);
    CydUi::centered(tft, String("Touch target ") + String(i + 1) + "/4",
                    82, 2, TFT_WHITE);
    tft.drawCircle(targets[i].x, targets[i].y, 10, TFT_YELLOW);
    tft.drawFastHLine(targets[i].x - 15, targets[i].y, 31, TFT_YELLOW);
    tft.drawFastVLine(targets[i].x, targets[i].y - 15, 31, TFT_YELLOW);
    if (!collectCalibrationPoint(samples[i])) {
      --i;
    } else {
      Serial.printf("[cyd] calibration point %u raw=%d,%d z=%d\n", i + 1,
                    samples[i].x, samples[i].y, samples[i].pressure);
    }
  }
  CydHardware::saveCalibration(samples, targets);
  Serial.println("[cyd] touch calibration saved");
  tft.fillScreen(TFT_BLACK);
  CydUi::centered(tft, "Calibration saved", 95, 3, TFT_GREEN);
  delay(700);
}

void handleSerialCommand(String command) {
  command.trim();
  if (command.equalsIgnoreCase("auto off")) {
    AutoLaunchSettings::setEnabled(false);
    Serial.println("[cyd] autolaunch disabled");
  } else if (command.startsWith("auto ")) {
    String title = command.substring(5);
    App* app = findAppByTitle(title);
    if (app != nullptr) {
      AutoLaunchSettings::Config config = AutoLaunchSettings::load();
      config.appTitle = app->appTitle();
      config.enabled = true;
      AutoLaunchSettings::save(config);
      Serial.println("[cyd] autolaunch saved");
    }
  } else if (command.startsWith("launch ")) {
    launchAppByTitle(command.substring(7), millis());
  } else if (command.equalsIgnoreCase("touch calibrate")) {
    CydHardware::clearCalibration();
    ESP.restart();
  } else {
    Serial.println("[cyd] commands: auto <app>, auto off, launch <app>, touch calibrate");
  }
}

void pollSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      handleSerialCommand(serialCommand);
      serialCommand = "";
    } else if (serialCommand.length() < 120) {
      serialCommand += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[cyd] setup start");
  CydHardware::begin();
  systemControlProvider.begin();
  const KidMode::Storage::Config kidConfig = kidModeStorage.load();
  kidModeService.begin(kidConfig.enabled);
  bootChildSplashSettings = kidModeStorage.loadSplash();
  tft.init();
  CydHardware::applyDisplayOrientation(tft);
  tft.invertDisplay(true);
  CydHardware::beginBacklight();
  tft.fillScreen(TFT_BLACK);
  if (!CydHardware::beginTouch()) Serial.println("[cyd] touch unavailable");
  if (!CydHardware::hasCalibration() || CydHardware::forceCalibrationRequested()) {
    runTouchCalibration();
  }

  autoLaunchSettingsApp.setLaunchableApps(autoLaunchChoices, AUTO_LAUNCH_COUNT);
  AutoLaunchSettings::migrateLegacyDebugPrefs();
  touchSample = CydHardware::readTouch();
  touchTracker.reset(touchSample, millis());
  bootStartedAtMs = millis();
  drawBootSplash();
  Serial.println("[cyd] setup complete");
}

void loop() {
  const uint32_t nowMs = millis();
  const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
  pollSerial();
  touchSample = CydHardware::readTouch();
  touchEvent = touchTracker.update(touchSample, nowMs);

  if (bootSplashActive) {
    if (touchEvent.pressed || CydHardware::physicalB1Down() ||
        CydHardware::physicalB2Down() || nowMs - bootStartedAtMs >= BOOT_SPLASH_MS) {
      bootSplashActive = false;
      if (kidModeService.enabled() && bootChildSplashSettings.enabled) {
        beginCustomBootSplash(nowMs);
      } else {
        bootReleasePending = touchSample.down;
        menuDirty = true;
        tft.fillScreen(TFT_BLACK);
      }
    }
    delay(4);
    return;
  }


  if (customBootSplashActive) {
    if (bootReleasePending) {
      if (!touchSample.down) {
        bootReleasePending = false;
        touchTracker.reset(touchSample, nowMs);
      }
      delay(4);
      return;
    }
    if (touchEvent.pressed ||
        nowMs - customBootSplashStartedAtMs >= CUSTOM_BOOT_SPLASH_MS) {
      customBootSplashActive = false;
      bootReleasePending = touchSample.down;
      menuDirty = true;
      tft.fillScreen(TFT_BLACK);
      touchTracker.reset(touchSample, nowMs);
    }
    delay(4);
    return;
  }

  if (bootReleasePending) {
    if (!touchSample.down) bootReleasePending = false;
    delay(4);
    return;
  }

  if (enforceChildMode(nowMs, nowUs)) {
    delay(4);
    return;
  }

  if (!autoLaunchAttempted && activeApp == nullptr && beginAutoLaunch(nowMs)) {
    delay(4);
    return;
  }
  if (autoLaunchNoticeActive) {
    updateAutoLaunch(nowMs);
    delay(4);
    return;
  }

  if (activeApp == nullptr) {
    const bool systemBarConsumed = updateSystemBarTouch(nowMs);
    if (!systemBarConsumed) updateMenuTouch(nowMs);
    drawMenu();
    delay(4);
    return;
  }

  const bool immersiveChromeConsumed = updateImmersiveChromeTouch(nowMs);
  const bool systemBarConsumed = immersiveChromeConsumed
                                     ? true
                                     : updateSystemBarTouch(nowMs);
  if (!systemBarConsumed && touchEvent.tap &&
      CydUi::backRect().contains(touchEvent.point)) {
    activeApp->exitToMenu();
  }

  const bool directTouchConsumed =
      !systemBarConsumed && activeApp->handleTouch(touchSample, touchEvent);
  const bool gameplayTouchEnabled = activeApp->phase() == AppPhase::Running;
  const TouchUi::TouchSample appTouchSample =
      (systemBarConsumed || directTouchConsumed || !gameplayTouchEnabled)
          ? TouchUi::TouchSample{}
          : touchSample;

  TouchUi::LogicalInputState input = TouchUi::mapGameTouch(
      appTouchSample, {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT},
      controlProfileFor(activeApp));
  input.b1Down = input.b1Down || CydHardware::physicalB1Down();
  input.b2Down = input.b2Down || CydHardware::physicalB2Down();
  activeApp->tick(nowMs, input.b1Down, input.b2Down);

  if (activeApp == &micriCasinoGame &&
      micriCasinoGame.consumeTongItsLaunchRequest()) {
    activeApp->exitToMenu();
    exitActiveApp(nowMs);
    returnToCasinoAfterTongIts = true;
    launchApp(tongItsGame, nowMs);
    drawSystemBarIfDirty();
    delay(4);
    return;
  }

  if (activeApp->shouldExitToMenu()) {
    if (activeApp == &tongItsGame && returnToCasinoAfterTongIts) {
      returnToCasinoAfterTongIts = false;
      exitActiveApp(nowMs);
      launchApp(micriCasinoGame, nowMs);
      micriCasinoGame.resumeHubFromTongIts();
      appRenderDue = true;
    } else {
      returnToCasinoAfterTongIts = false;
      exitActiveApp(nowMs);
    }
  } else {
    renderActiveAppIfDue(nowMs);
  }
  drawSystemBarIfDirty();
  delay(4);
}
