#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <NimBLEUUID.h>

#include "App.h"
#include "src/apps/CounterApp.h"
#include "src/apps/MouseEmulatorApp.h"
#include "src/apps/DiceRollerApp.h"
#include "src/apps/CoinFlipperApp.h"
#include "src/apps/RandomNumberApp.h"
#include "src/apps/ScreenSaverApp.h"
#include "src/apps/StopwatchApp.h"
#include "src/apps/CountdownApp.h"
#include "src/apps/ClockApp.h"
#include "src/apps/MicriMinerApp.h"
#include "src/apps/DistributedMinerApp.h"
#include "src/apps/MetronomeApp.h"
#include "src/apps/WiFiSetupApp.h"
#include "src/apps/EspContactsApp.h"
#include "src/apps/CommunicatorApp.h"
#include "src/apps/CreditsApp.h"
#include "src/apps/OptionsApp.h"
#include "src/apps/AutoLaunchSettingsApp.h"
#include "src/apps/PetSimulatorApp.h"
#include "src/apps/ReadingApp.h"

#include "src/games/AlienRaidersGame.h"
#include "src/games/Breakout76Game.h"
#include "src/games/CaveChopperGame.h"
#include "src/games/CityRacerGame.h"
#include "src/games/MicriCasinoGame.h"
#include "src/games/MicriFieldGame.h"
#include "src/games/FishingFlickGame.h"
#include "src/games/KnifeThrowGame.h"
#include "src/games/MazeRunnerGame.h"
#include "src/games/MiniLanderGame.h"
#include "src/games/NeedSpeedGame.h"
#include "src/games/NoonShooterGame.h"
#include "src/games/NuclearReactorGame.h"
#include "src/games/PipeManiaGame.h"
#include "src/games/SimonGame.h"
#include "src/games/TinyGolfGame.h"
#include "src/games/TongItsGame.h"
#include "src/games/TowerStackerGame.h"

#include "TDisplayUi.h"
#include "TDisplayFramebuffer.h"
#include "src/shared/Version.h"

static NimBLEUUID nimbleUuidLinkAnchor(static_cast<uint16_t>(0x1812));

TFT_eSPI tft = TFT_eSPI();

constexpr uint8_t BUTTON_1 = 0;
constexpr uint8_t BUTTON_2 = 35;

constexpr uint16_t SCREEN_WIDTH = 240;
constexpr uint16_t SCREEN_HEIGHT = 135;
constexpr uint8_t VISIBLE_MENU_ROWS = 6;
constexpr uint16_t MENU_ROW_Y = 42;
constexpr uint16_t MENU_ROW_H = 13;
constexpr uint16_t MENU_LEFT = 12;
constexpr uint16_t MENU_RIGHT = SCREEN_WIDTH - 12;
constexpr uint16_t BOOT_SPLASH_MS = 2000;
constexpr uint16_t AUTO_LAUNCH_NOTICE_MS = 2000;
constexpr uint16_t APP_RUNNING_RENDER_MS = 33;
constexpr uint16_t APP_STATIC_RENDER_MS = 2000;

CounterApp counterApp(SCREEN_WIDTH, SCREEN_HEIGHT);
MouseEmulatorApp mouseEmulatorApp(SCREEN_WIDTH, SCREEN_HEIGHT);
DiceRollerApp diceRollerApp(SCREEN_WIDTH, SCREEN_HEIGHT);
CoinFlipperApp coinFlipperApp(SCREEN_WIDTH, SCREEN_HEIGHT);
RandomNumberApp randomNumberApp(SCREEN_WIDTH, SCREEN_HEIGHT);
ScreenSaverApp screenSaverApp(SCREEN_WIDTH, SCREEN_HEIGHT);
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
OptionsApp optionsApp(SCREEN_WIDTH, SCREEN_HEIGHT);
AutoLaunchSettingsApp autoLaunchSettingsApp(SCREEN_WIDTH, SCREEN_HEIGHT);
PetSimulatorApp petSimulatorApp(SCREEN_WIDTH, SCREEN_HEIGHT);
ReadingApp readingApp(SCREEN_WIDTH, SCREEN_HEIGHT);

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

enum class MenuView {
  Root,
  Games,
  Apps,
  Settings
};

enum class MenuAction {
  OpenGames,
  OpenApps,
  OpenSettings,
  Launch,
  Back
};

struct MenuEntry {
  const char* label;
  MenuAction action;
  App* app;
};

MenuEntry rootMenu[] = {
    {"Games", MenuAction::OpenGames, nullptr},
    {"Apps", MenuAction::OpenApps, nullptr},
    {"Settings", MenuAction::OpenSettings, nullptr},
    {nullptr, MenuAction::Launch, &creditsApp},
};

MenuEntry gamesMenu[] = {
    {nullptr, MenuAction::Launch, &alienRaidersGame},
    {nullptr, MenuAction::Launch, &breakout76Game},
    {nullptr, MenuAction::Launch, &caveChopperGame},
    {nullptr, MenuAction::Launch, &cityRacerGame},
    {nullptr, MenuAction::Launch, &micriCasinoGame},
    {nullptr, MenuAction::Launch, &micriFieldGame},
    {nullptr, MenuAction::Launch, &fishingFlickGame},
    {nullptr, MenuAction::Launch, &knifeThrowGame},
    {nullptr, MenuAction::Launch, &mazeCollectorGame},
    {nullptr, MenuAction::Launch, &mazeRunnerGame},
    {nullptr, MenuAction::Launch, &miniLanderGame},
    {nullptr, MenuAction::Launch, &needSpeedGame},
    {nullptr, MenuAction::Launch, &noonShooterGame},
    {nullptr, MenuAction::Launch, &petSimulatorApp},
    {nullptr, MenuAction::Launch, &pipeManiaGame},
    {nullptr, MenuAction::Launch, &nuclearReactorGame},
    {nullptr, MenuAction::Launch, &simonGame},
    {nullptr, MenuAction::Launch, &tinyGolfGame},
    {nullptr, MenuAction::Launch, &tongItsGame},
    {nullptr, MenuAction::Launch, &towerStackerGame},
    {"Back", MenuAction::Back, nullptr},
};

MenuEntry appsMenu[] = {
    {nullptr, MenuAction::Launch, &clockApp},
    {nullptr, MenuAction::Launch, &stopwatchApp},
    {nullptr, MenuAction::Launch, &countdownApp},
    {nullptr, MenuAction::Launch, &counterApp},
    {nullptr, MenuAction::Launch, &diceRollerApp},
    {nullptr, MenuAction::Launch, &coinFlipperApp},
    {nullptr, MenuAction::Launch, &randomNumberApp},
    {nullptr, MenuAction::Launch, &screenSaverApp},
    {nullptr, MenuAction::Launch, &metronomeApp},
    {nullptr, MenuAction::Launch, &micriMinerApp},
    {nullptr, MenuAction::Launch, &distributedMinerApp},
    {nullptr, MenuAction::Launch, &espContactsApp},
    {nullptr, MenuAction::Launch, &communicatorApp},
    {nullptr, MenuAction::Launch, &mouseEmulatorApp},
    {nullptr, MenuAction::Launch, &readingApp},
    {"Back", MenuAction::Back, nullptr},
};

MenuEntry settingsMenu[] = {
    {nullptr, MenuAction::Launch, &optionsApp},
    {nullptr, MenuAction::Launch, &autoLaunchSettingsApp},
    {nullptr, MenuAction::Launch, &wifiSetupApp},
    {"Back", MenuAction::Back, nullptr},
};

App* autoLaunchChoices[] = {
    &alienRaidersGame,
    &breakout76Game,
    &caveChopperGame,
    &cityRacerGame,
    &micriCasinoGame,
    &micriFieldGame,
    &fishingFlickGame,
    &knifeThrowGame,
    &mazeCollectorGame,
    &mazeRunnerGame,
    &miniLanderGame,
    &needSpeedGame,
    &noonShooterGame,
    &petSimulatorApp,
    &pipeManiaGame,
    &nuclearReactorGame,
    &simonGame,
    &tinyGolfGame,
    &tongItsGame,
    &towerStackerGame,
    &clockApp,
    &stopwatchApp,
    &countdownApp,
    &counterApp,
    &diceRollerApp,
    &coinFlipperApp,
    &randomNumberApp,
    &screenSaverApp,
    &metronomeApp,
    &micriMinerApp,
    &distributedMinerApp,
    &espContactsApp,
    &communicatorApp,
    &mouseEmulatorApp,
    &readingApp,
};
constexpr uint8_t AUTO_LAUNCH_CHOICE_COUNT = sizeof(autoLaunchChoices) / sizeof(autoLaunchChoices[0]);

constexpr uint8_t ROOT_MENU_COUNT = sizeof(rootMenu) / sizeof(rootMenu[0]);
constexpr uint8_t GAMES_MENU_COUNT = sizeof(gamesMenu) / sizeof(gamesMenu[0]);
constexpr uint8_t APPS_MENU_COUNT = sizeof(appsMenu) / sizeof(appsMenu[0]);
constexpr uint8_t SETTINGS_MENU_COUNT = sizeof(settingsMenu) / sizeof(settingsMenu[0]);

SingleButton menuButton1;
SingleButton menuButton2;
MenuView currentMenu = MenuView::Root;
MenuView activeReturnMenu = MenuView::Root;
uint8_t menuIndex = 0;
uint8_t scrollOffset = 0;
bool menuSelectArmed = false;
bool menuDirty = true;
bool menuRendered = false;
MenuView renderedMenuView = MenuView::Root;
uint8_t renderedMenuIndex = 255;
uint8_t renderedScrollOffset = 255;
uint8_t renderedMenuCount = 0;
bool renderedMenuSelectArmed = false;
TDisplayUi::TextSize renderedMenuTextSize = TDisplayUi::TextSize::Compact;
bool menuInputLockedUntilRelease = false;
bool bootSplashActive = true;
bool bootSkipReleasePending = false;
bool autoLaunchAttempted = false;
bool autoLaunchNoticeActive = false;
bool autoLaunchNoticeDrawn = false;
bool pendingAutoLaunchAutoRun = false;
uint32_t bootStartedAtMs = 0;
uint32_t autoLaunchNoticeStartedAtMs = 0;
App* activeApp = nullptr;
bool appRenderDue = false;
uint32_t nextAppRenderMs = 0;
AppPhase lastRenderedAppPhase = AppPhase::Start;
String serialCommand;
String pendingAutoLaunchTitle;

bool isButton1Down() { return digitalRead(BUTTON_1) == LOW; }
bool isButton2Down() { return digitalRead(BUTTON_2) == LOW; }

MenuEntry* currentMenuEntries() {
  switch (currentMenu) {
    case MenuView::Games:
      return gamesMenu;
    case MenuView::Apps:
      return appsMenu;
    case MenuView::Settings:
      return settingsMenu;
    case MenuView::Root:
    default:
      return rootMenu;
  }
}

uint8_t currentMenuCount() {
  switch (currentMenu) {
    case MenuView::Games:
      return GAMES_MENU_COUNT;
    case MenuView::Apps:
      return APPS_MENU_COUNT;
    case MenuView::Settings:
      return SETTINGS_MENU_COUNT;
    case MenuView::Root:
    default:
      return ROOT_MENU_COUNT;
  }
}

const char* currentMenuTitle() {
  switch (currentMenu) {
    case MenuView::Games:
      return "Games";
    case MenuView::Apps:
      return "Apps";
    case MenuView::Settings:
      return "Settings";
    case MenuView::Root:
    default:
      return "MicriOS";
  }
}

const char* entryTitle(const MenuEntry& entry) {
  return entry.app != nullptr ? entry.app->appTitle() : entry.label;
}

bool startsWithIgnoreCase(const String& value, const char* prefix) {
  String lowerValue = value;
  String lowerPrefix(prefix);
  lowerValue.toLowerCase();
  lowerPrefix.toLowerCase();
  return lowerValue.startsWith(lowerPrefix);
}

App* findAppInMenu(MenuEntry* entries, uint8_t count, const String& title) {
  for (uint8_t i = 0; i < count; i++) {
    if (entries[i].app != nullptr && title.equalsIgnoreCase(entries[i].app->appTitle())) {
      return entries[i].app;
    }
  }
  return nullptr;
}

App* findAppByTitle(const String& title) {
  App* app = findAppInMenu(rootMenu, ROOT_MENU_COUNT, title);
  if (app != nullptr) return app;
  app = findAppInMenu(gamesMenu, GAMES_MENU_COUNT, title);
  if (app != nullptr) return app;
  app = findAppInMenu(appsMenu, APPS_MENU_COUNT, title);
  if (app != nullptr) return app;
  return findAppInMenu(settingsMenu, SETTINGS_MENU_COUNT, title);
}

void printAppsInMenu(MenuEntry* entries, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    if (entries[i].app != nullptr) {
      Serial.print("  ");
      Serial.println(entries[i].app->appTitle());
    }
  }
}

void printAppList() {
  Serial.println("[debug] launchable apps:");
  printAppsInMenu(rootMenu, ROOT_MENU_COUNT);
  printAppsInMenu(gamesMenu, GAMES_MENU_COUNT);
  printAppsInMenu(appsMenu, APPS_MENU_COUNT);
  printAppsInMenu(settingsMenu, SETTINGS_MENU_COUNT);
}

void saveAutoLaunchTitle(const String& title) {
  AutoLaunchSettings::Config config = AutoLaunchSettings::load();
  config.appTitle = title;
  config.enabled = title.length() > 0;
  AutoLaunchSettings::save(config);
}

void clearAutoLaunchTitle() {
  AutoLaunchSettings::setEnabled(false);
}

void printAutoLaunchStatus() {
  const AutoLaunchSettings::Config config = AutoLaunchSettings::load();
  Serial.print("[debug] autolaunch_enabled=");
  Serial.println(config.enabled ? "on" : "off");
  Serial.print("[debug] autolaunch_app=");
  if (config.appTitle.length() == 0) {
    Serial.println("(none)");
  } else {
    Serial.println(config.appTitle);
  }
  Serial.print("[debug] autolaunch_autorun=");
  Serial.println(config.autoRun ? "on" : "off");
}

void markMenuDirty() {
  menuDirty = true;
}

void invalidateMenuRender() {
  menuRendered = false;
  renderedMenuIndex = 255;
  renderedScrollOffset = 255;
}

void openMenu(MenuView view) {
  currentMenu = view;
  menuIndex = 0;
  scrollOffset = 0;
  menuSelectArmed = false;
  invalidateMenuRender();
  markMenuDirty();
}

void launchApp(App& app, uint32_t nowMs, bool fromAutoLaunch = false, bool autoRun = false) {
  invalidateMenuRender();
  activeReturnMenu = currentMenu;
  activeApp = &app;
  if (&app == &micriMinerApp) {
    micriMinerApp.setDebugAutoStart(fromAutoLaunch && autoRun);
  }
  activeApp->begin(nowMs, false, false);
  menuSelectArmed = false;
  appRenderDue = true;
  nextAppRenderMs = 0;
  lastRenderedAppPhase = activeApp->phase();
  tft.fillScreen(TFT_BLACK);
}

bool launchAppByTitle(const String& title, uint32_t nowMs, bool fromAutoLaunch = false, bool autoRun = false) {
  App* app = findAppByTitle(title);
  if (app == nullptr) {
    Serial.print("[debug] app not found: ");
    Serial.println(title);
    return false;
  }
  if (activeApp != nullptr) {
    Serial.println("[debug] cannot launch while another app is active");
    return false;
  }
  Serial.print("[debug] launching ");
  Serial.println(app->appTitle());
  launchApp(*app, nowMs, fromAutoLaunch, autoRun);
  return true;
}

void selectMenuEntry(uint32_t nowMs) {
  const MenuEntry& entry = currentMenuEntries()[menuIndex];
  switch (entry.action) {
    case MenuAction::OpenGames:
      openMenu(MenuView::Games);
      break;
    case MenuAction::OpenApps:
      openMenu(MenuView::Apps);
      break;
    case MenuAction::OpenSettings:
      openMenu(MenuView::Settings);
      break;
    case MenuAction::Back:
      openMenu(MenuView::Root);
      break;
    case MenuAction::Launch:
      if (entry.app != nullptr) {
        launchApp(*entry.app, nowMs);
      }
      break;
  }
}

void moveMenu(int8_t direction) {
  const uint8_t count = currentMenuCount();
  if (direction > 0) {
    menuIndex = (menuIndex + 1) % count;
  } else if (menuIndex == 0) {
    menuIndex = count - 1;
  } else {
    menuIndex--;
  }
  menuSelectArmed = false;
  markMenuDirty();
}

void updateMenu(uint32_t nowMs, bool b1, bool b2) {
  if (menuInputLockedUntilRelease) {
    menuButton1.reset(b1, nowMs);
    menuButton2.reset(b2, nowMs);
    if (!b1 && !b2) {
      menuInputLockedUntilRelease = false;
      menuButton1.reset(false, nowMs);
      menuButton2.reset(false, nowMs);
    }
    return;
  }

  const ButtonInput input1 = menuButton1.update(b1, nowMs);
  const ButtonInput input2 = menuButton2.update(b2, nowMs);

  if (input1.longPress) {
    menuSelectArmed = true;
    markMenuDirty();
  }
  if (input1.click && !menuSelectArmed) {
    moveMenu(1);
  }
  if (menuSelectArmed && input1.released) {
    selectMenuEntry(nowMs);
    menuButton1.reset(false, nowMs);
  }

  if (input2.click) {
    moveMenu(-1);
  }
  if (input2.longPress && currentMenu != MenuView::Root) {
    openMenu(MenuView::Root);
    menuButton2.reset(b2, nowMs);
  }
}

void exitActiveAppToMenu(uint32_t nowMs, bool b1, bool b2) {
  activeApp->clearExitRequest();
  activeApp = nullptr;
  currentMenu = activeReturnMenu;
  menuSelectArmed = false;
  menuInputLockedUntilRelease = true;
  menuButton1.reset(b1, nowMs);
  menuButton2.reset(b2, nowMs);
  markMenuDirty();
  invalidateMenuRender();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
}

void renderActiveAppIfDue(uint32_t nowMs) {
  const AppPhase currentPhase = activeApp->phase();
  const uint16_t interval = currentPhase == AppPhase::Running ? activeApp->runningRenderIntervalMs() : activeApp->staticRenderIntervalMs();
  const bool timedRenderEnabled = interval > 0;
  const bool wantsImmediate = activeApp->wantsImmediateRender();
  if (!wantsImmediate && !appRenderDue && currentPhase == lastRenderedAppPhase &&
      (!timedRenderEnabled || nowMs < nextAppRenderMs)) {
    return;
  }
  activeApp->render(tft);
  appRenderDue = false;
  lastRenderedAppPhase = currentPhase;
  nextAppRenderMs = timedRenderEnabled ? nowMs + interval : UINT32_MAX;
}

void drawAutoLaunchNotice() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.drawRoundRect(5, 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10, 8, TFT_DARKGREY);
  drawCenteredText("Autolaunching", 28, 2, TFT_CYAN);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String title = TDisplayUi::fitText(tft, pendingAutoLaunchTitle, SCREEN_WIDTH - 24);
  tft.drawString(title, (SCREEN_WIDTH - tft.textWidth(title)) / 2, 62);

  drawCenteredText("B1 cancel", 112, 1, TFT_LIGHTGREY);
}

bool beginAutoLaunchNotice(uint32_t nowMs) {
  if (autoLaunchAttempted) return false;
  autoLaunchAttempted = true;
  const AutoLaunchSettings::Config config = AutoLaunchSettings::load();
  if (!config.enabled || config.appTitle.length() == 0) return false;
  pendingAutoLaunchTitle = config.appTitle;
  pendingAutoLaunchAutoRun = config.autoRun;
  autoLaunchNoticeActive = true;
  autoLaunchNoticeDrawn = false;
  autoLaunchNoticeStartedAtMs = nowMs;
  Serial.print("[debug] autolaunch notice: ");
  Serial.println(pendingAutoLaunchTitle);
  return true;
}

void updateAutoLaunchNotice(uint32_t nowMs, bool b1, bool b2) {
  if (!autoLaunchNoticeDrawn) {
    drawAutoLaunchNotice();
    autoLaunchNoticeDrawn = true;
  }

  if (b1) {
    Serial.println("[debug] autolaunch canceled by B1");
    autoLaunchNoticeActive = false;
    pendingAutoLaunchTitle = "";
    menuInputLockedUntilRelease = true;
    menuButton1.reset(b1, nowMs);
    menuButton2.reset(b2, nowMs);
    markMenuDirty();
    tft.fillScreen(TFT_BLACK);
    return;
  }

  if (nowMs - autoLaunchNoticeStartedAtMs >= AUTO_LAUNCH_NOTICE_MS) {
    autoLaunchNoticeActive = false;
    const String title = pendingAutoLaunchTitle;
    const bool autoRun = pendingAutoLaunchAutoRun;
    pendingAutoLaunchTitle = "";
    launchAppByTitle(title, nowMs, true, autoRun);
    if (activeApp == nullptr) {
      markMenuDirty();
      tft.fillScreen(TFT_BLACK);
    }
  }
}

void handleSerialCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command.equalsIgnoreCase("help")) {
    Serial.println("[debug] commands:");
    Serial.println("  auto");
    Serial.println("  auto list");
    Serial.println("  auto on");
    Serial.println("  auto off");
    Serial.println("  auto <app title>");
    Serial.println("  autorun on|off");
    Serial.println("  launch <app title>");
    Serial.println("  miner start");
    Serial.println("  miner stop");
    Serial.println("  miner stats");
    Serial.println("  miner logs on|off");
    Serial.println("  miner autostart on|off (alias)");
    Serial.println("  cluster start");
    Serial.println("  cluster stop");
    Serial.println("  cluster stats");
    Serial.println("  cluster pair");
    Serial.println("  cluster reset");
    Serial.println("  cluster local on|off");
    return;
  }

  if (command.equalsIgnoreCase("auto")) {
    printAutoLaunchStatus();
    return;
  }
  if (command.equalsIgnoreCase("auto list")) {
    printAppList();
    return;
  }
  if (command.equalsIgnoreCase("auto off")) {
    clearAutoLaunchTitle();
    Serial.println("[debug] autolaunch disabled");
    return;
  }
  if (command.equalsIgnoreCase("auto on")) {
    AutoLaunchSettings::Config config = AutoLaunchSettings::load();
    if (config.appTitle.length() == 0) {
      Serial.println("[debug] no autolaunch app selected");
      return;
    }
    config.enabled = true;
    AutoLaunchSettings::save(config);
    Serial.println("[debug] autolaunch enabled");
    return;
  }
  if (startsWithIgnoreCase(command, "auto ")) {
    String title = command.substring(5);
    title.trim();
    App* app = findAppByTitle(title);
    if (app == nullptr) {
      Serial.print("[debug] app not found, not saved: ");
      Serial.println(title);
      return;
    }
    saveAutoLaunchTitle(app->appTitle());
    Serial.print("[debug] autolaunch saved: ");
    Serial.println(app->appTitle());
    return;
  }

  if (command.equalsIgnoreCase("autorun on")) {
    AutoLaunchSettings::setAutoRun(true);
    Serial.println("[debug] autolaunch autorun on");
    return;
  }
  if (command.equalsIgnoreCase("autorun off")) {
    AutoLaunchSettings::setAutoRun(false);
    Serial.println("[debug] autolaunch autorun off");
    return;
  }

  if (startsWithIgnoreCase(command, "launch ")) {
    String title = command.substring(7);
    title.trim();
    launchAppByTitle(title, millis());
    return;
  }

  if (command.equalsIgnoreCase("miner start")) {
    if (activeApp == nullptr) {
      launchApp(micriMinerApp, millis());
    }
    if (activeApp == &micriMinerApp) {
      micriMinerApp.debugStartMining();
    } else {
      Serial.println("[miner] another app is active");
    }
    return;
  }
  if (command.equalsIgnoreCase("miner stop")) {
    micriMinerApp.debugStopMining();
    return;
  }
  if (command.equalsIgnoreCase("miner stats")) {
    micriMinerApp.debugPrintStats();
    return;
  }
  if (command.equalsIgnoreCase("miner logs on")) {
    micriMinerApp.setEmitDebugLogs(true);
    Serial.println("[miner] debug logs on");
    return;
  }
  if (command.equalsIgnoreCase("miner logs off")) {
    micriMinerApp.setEmitDebugLogs(false);
    Serial.println("[miner] debug logs off");
    return;
  }
  if (command.equalsIgnoreCase("miner autostart on")) {
    AutoLaunchSettings::setAutoRun(true);
    Serial.println("[debug] autolaunch autorun on");
    return;
  }
  if (command.equalsIgnoreCase("miner autostart off")) {
    AutoLaunchSettings::setAutoRun(false);
    Serial.println("[debug] autolaunch autorun off");
    return;
  }

  if (command.equalsIgnoreCase("cluster start")) {
    if (activeApp == nullptr) {
      launchApp(distributedMinerApp, millis());
    }
    if (activeApp == &distributedMinerApp) {
      distributedMinerApp.debugStartCluster();
    } else {
      Serial.println("[cluster] another app is active");
    }
    return;
  }
  if (command.equalsIgnoreCase("cluster stop")) {
    distributedMinerApp.debugStopCluster();
    return;
  }
  if (command.equalsIgnoreCase("cluster stats")) {
    distributedMinerApp.debugPrintStats();
    return;
  }
  if (command.equalsIgnoreCase("cluster pair")) {
    if (activeApp == nullptr) {
      launchApp(distributedMinerApp, millis());
    }
    distributedMinerApp.debugStartPairing();
    return;
  }
  if (command.equalsIgnoreCase("cluster reset")) {
    if (activeApp == nullptr) {
      launchApp(distributedMinerApp, millis());
    }
    distributedMinerApp.debugResetCluster();
    return;
  }
  if (command.equalsIgnoreCase("cluster local on")) {
    distributedMinerApp.debugSetLocalMining(true);
    return;
  }
  if (command.equalsIgnoreCase("cluster local off")) {
    distributedMinerApp.debugSetLocalMining(false);
    return;
  }

  Serial.print("[debug] unknown command: ");
  Serial.println(command);
}

void pollSerialCommands() {
  while (Serial.available() > 0) {
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

void drawCenteredText(const char* text, int16_t y, uint8_t size, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(size);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, (SCREEN_WIDTH - tft.textWidth(text)) / 2, y);
}

void drawBootSplash() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRoundRect(5, 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10, 8, TFT_DARKGREY);
  tft.drawFastHLine(24, 31, SCREEN_WIDTH - 48, TFT_CYAN);
  tft.drawFastHLine(42, 101, SCREEN_WIDTH - 84, TFT_ORANGE);

  drawCenteredText("MicriDeck", 42, 3, TFT_WHITE);
  drawCenteredText("T-Display", 72, 2, TFT_CYAN);
  String versionLine = String("MicriOS ") + BuildInfo::BUILD_TEXT;
  drawCenteredText(versionLine.c_str(), 114, 1, TFT_LIGHTGREY);
}

template <typename Canvas>
void drawMenuFrame(Canvas& canvas, uint8_t count, TDisplayUi::TextSize textSize) {
  const char* footer = menuSelectArmed
                           ? (currentMenuEntries()[menuIndex].action == MenuAction::Back ? "Release: back" : "Release: open")
                           : "B1 next/open  B2 prev/back";
  TDisplayUi::menuFrame(canvas, currentMenuTitle(), menuIndex, count, scrollOffset,
                        [](uint8_t index) -> const char* {
                          return entryTitle(currentMenuEntries()[index]);
                        },
                        footer, textSize, TFT_GREEN);
}

void drawMenu() {
  if (!menuDirty) {
    return;
  }
  menuDirty = false;

  const uint8_t count = currentMenuCount();
  const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
  const uint8_t oldIndex = renderedMenuIndex;
  const uint8_t oldScroll = renderedScrollOffset;
  const TDisplayUi::TextSize oldTextSize = renderedMenuTextSize;
  scrollOffset = TDisplayUi::menuScrollOffset(menuIndex, count, TDisplayUi::menuStyle(textSize));

  tft.setRotation(1);
  const bool canRedrawRows =
      menuRendered &&
      renderedMenuView == currentMenu &&
      renderedMenuCount == count &&
      renderedMenuSelectArmed == menuSelectArmed &&
      oldTextSize == textSize &&
      oldScroll == scrollOffset &&
      oldIndex != 255 &&
      oldIndex != menuIndex;

  if (canRedrawRows) {
    if (oldIndex >= scrollOffset && oldIndex < scrollOffset + TDisplayUi::menuStyle(textSize).visibleRows) {
      const uint8_t oldRow = oldIndex - scrollOffset;
      TDisplayUi::menuRow(tft, oldRow, entryTitle(currentMenuEntries()[oldIndex]), false, textSize, TFT_GREEN);
    }
    if (menuIndex >= scrollOffset && menuIndex < scrollOffset + TDisplayUi::menuStyle(textSize).visibleRows) {
      const uint8_t newRow = menuIndex - scrollOffset;
      TDisplayUi::menuRow(tft, newRow, entryTitle(currentMenuEntries()[menuIndex]), true, textSize, TFT_GREEN);
    }
    TDisplayUi::menuCounter(tft, menuIndex, count);
    renderedMenuIndex = menuIndex;
    renderedScrollOffset = scrollOffset;
    return;
  }

  TDisplayFramebuffer::draw(tft, SCREEN_WIDTH, SCREEN_HEIGHT,
                            [&](auto& canvas) { drawMenuFrame(canvas, count, textSize); });
  menuRendered = true;
  renderedMenuView = currentMenu;
  renderedMenuIndex = menuIndex;
  renderedScrollOffset = scrollOffset;
  renderedMenuCount = count;
  renderedMenuSelectArmed = menuSelectArmed;
  renderedMenuTextSize = textSize;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  autoLaunchSettingsApp.setLaunchableApps(autoLaunchChoices, AUTO_LAUNCH_CHOICE_COUNT);
  AutoLaunchSettings::migrateLegacyDebugPrefs();
  const uint32_t nowMs = millis();
  bootStartedAtMs = nowMs;
  menuButton1.reset(isButton1Down(), nowMs);
  menuButton2.reset(isButton2Down(), nowMs);
  printAutoLaunchStatus();
  drawBootSplash();
  markMenuDirty();
}

void loop() {
  uint32_t nowMs = millis();
  bool b1 = isButton1Down();
  bool b2 = isButton2Down();
  pollSerialCommands();

  if (bootSplashActive && (b1 || b2 || (nowMs - bootStartedAtMs) >= BOOT_SPLASH_MS)) {
    bootSplashActive = false;
    bootSkipReleasePending = b1 || b2;
    menuButton1.reset(b1, nowMs);
    menuButton2.reset(b2, nowMs);
    markMenuDirty();
    tft.fillScreen(TFT_BLACK);
  }

  if (bootSplashActive) {
    delay(10);
    return;
  }

  if (bootSkipReleasePending) {
    menuButton1.reset(b1, nowMs);
    menuButton2.reset(b2, nowMs);
    if (!b1 && !b2) {
      bootSkipReleasePending = false;
      menuButton1.reset(false, nowMs);
      menuButton2.reset(false, nowMs);
    }
    delay(10);
    return;
  }

  if (!autoLaunchAttempted && activeApp == nullptr) {
    if (beginAutoLaunchNotice(nowMs)) {
      delay(10);
      return;
    }
  }

  if (autoLaunchNoticeActive) {
    updateAutoLaunchNotice(nowMs, b1, b2);
    delay(10);
    return;
  }

  if (activeApp == nullptr) {
    updateMenu(nowMs, b1, b2);
    drawMenu();
  } else {
    activeApp->tick(nowMs, b1, b2);
    if (activeApp->shouldExitToMenu()) {
        exitActiveAppToMenu(nowMs, b1, b2);
    } else {
        renderActiveAppIfDue(nowMs);
    }
  }
  delay(10);
}
