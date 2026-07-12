#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <HIDTypes.h>

#include "src/shared/Version.h"
#include "src/shared/logic/MinerClusterLogic.h"
#include "src/shared/logic/MouseEmulatorLogic.h"
#include "src/shared/logic/MouseIdentityLogic.h"

constexpr uint8_t LED_PIN = 8;
constexpr uint8_t BUTTON_PIN = 9;
constexpr bool LED_ACTIVE_LOW = true;

constexpr uint32_t LONG_HOLD_MS = 5000;
constexpr uint32_t TAP_GAP_MS = 450;
constexpr uint32_t DEBOUNCE_MS = 25;

constexpr uint16_t MENU_ON_MS = 2000;
constexpr uint16_t MENU_SELECT_MS = 2000;
constexpr uint16_t SHORT_BLINK_MS = 120;
constexpr uint16_t APP_MARKER_ON_MS = 700;
constexpr uint16_t APP_MARKER_STATE_GAP_MS = 180;
constexpr uint16_t STATE_WINDOW_MS = 5000;

constexpr int MOUSE_VERTICAL_WIGGLE_LIMIT = 20;
constexpr int MOUSE_VERTICAL_WIGGLE_MIN_STEPS = 8;
constexpr int MOUSE_VERTICAL_WIGGLE_MAX_STEPS = 28;
constexpr int MOUSE_STEPS_PER_TICK = 24;

constexpr const char* HEADLESS_PREF_NS = "headless";
constexpr const char* AUTO_APP_KEY = "auto_app";
constexpr const char* MOUSE_ADDR_KEY = "mouse_addr";

enum class HeadlessApp : uint8_t {
  Mouse = 1,
  Miner = 2,
};

enum class Mode : uint8_t {
  Menu,
  Mouse,
  Miner,
};

enum class ButtonEvent : uint8_t {
  None,
  Hold5s,
  OneTap,
  TwoTaps,
  ThreeTaps,
};

struct LedStep {
  bool on;
  uint16_t durationMs;
  bool selectWindow;
};

class LedProgram {
 public:
  void clear(uint32_t nowMs) {
    count_ = 0;
    index_ = 0;
    startedAtMs_ = nowMs;
    done_ = true;
  }

  void start(const LedStep* steps, uint8_t count, uint32_t nowMs) {
    count_ = min<uint8_t>(count, MAX_STEPS);
    for (uint8_t i = 0; i < count_; ++i) steps_[i] = steps[i];
    index_ = 0;
    startedAtMs_ = nowMs;
    done_ = count_ == 0;
    if (!done_) setLed(steps_[0].on);
  }

  void update(uint32_t nowMs) {
    if (done_ || count_ == 0) return;
    if (nowMs - startedAtMs_ < steps_[index_].durationMs) return;
    index_++;
    if (index_ >= count_) {
      done_ = true;
      return;
    }
    startedAtMs_ = nowMs;
    setLed(steps_[index_].on);
  }

  bool done() const { return done_; }
  bool inSelectWindow() const {
    return !done_ && count_ > 0 && index_ < count_ && steps_[index_].selectWindow;
  }

 private:
  static constexpr uint8_t MAX_STEPS = 72;
  LedStep steps_[MAX_STEPS] = {};
  uint8_t count_ = 0;
  uint8_t index_ = 0;
  uint32_t startedAtMs_ = 0;
  bool done_ = true;

  void setLed(bool on);
};

class ButtonTracker {
 public:
  void begin(bool down, uint32_t nowMs) {
    rawDown_ = down;
    stableDown_ = down;
    lastRawChangeMs_ = nowMs;
    pressedAtMs_ = down ? nowMs : 0;
  }

  ButtonEvent update(bool down, uint32_t nowMs) {
    if (down != rawDown_) {
      rawDown_ = down;
      lastRawChangeMs_ = nowMs;
    }

    if (rawDown_ != stableDown_ && nowMs - lastRawChangeMs_ >= DEBOUNCE_MS) {
      stableDown_ = rawDown_;
      if (stableDown_) {
        pressedAtMs_ = nowMs;
        holdEmitted_ = false;
      } else {
        if (!holdEmitted_) {
          tapCount_++;
          lastTapMs_ = nowMs;
        }
        pressedAtMs_ = 0;
        holdEmitted_ = false;
      }
    }

    if (stableDown_ && !holdEmitted_ && pressedAtMs_ != 0 && nowMs - pressedAtMs_ >= LONG_HOLD_MS) {
      holdEmitted_ = true;
      tapCount_ = 0;
      return ButtonEvent::Hold5s;
    }

    if (!stableDown_ && tapCount_ > 0 && nowMs - lastTapMs_ >= TAP_GAP_MS) {
      const uint8_t taps = tapCount_;
      tapCount_ = 0;
      if (taps == 1) return ButtonEvent::OneTap;
      if (taps == 2) return ButtonEvent::TwoTaps;
      if (taps == 3) return ButtonEvent::ThreeTaps;
      return ButtonEvent::None;
    }

    return ButtonEvent::None;
  }

 private:
  bool rawDown_ = false;
  bool stableDown_ = false;
  bool holdEmitted_ = false;
  uint8_t tapCount_ = 0;
  uint32_t lastRawChangeMs_ = 0;
  uint32_t pressedAtMs_ = 0;
  uint32_t lastTapMs_ = 0;
};

namespace {
constexpr uint8_t HID_COUNTRY_NOT_LOCALIZED = 0x00;
constexpr uint8_t HID_INFO_NORMALLY_CONNECTABLE = 0x02;

const uint8_t HID_REPORT_DESCRIPTOR[] = {
    USAGE_PAGE(1), 0x01, USAGE(1), 0x02, COLLECTION(1), 0x01, USAGE(1), 0x01, COLLECTION(1), 0x00,
    REPORT_ID(1), 0x01, USAGE_PAGE(1), 0x09, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x05,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01, REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x05,
    HIDINPUT(1), 0x02, REPORT_SIZE(1), 0x03, REPORT_COUNT(1), 0x01, HIDINPUT(1), 0x03,
    USAGE_PAGE(1), 0x01, USAGE(1), 0x30, USAGE(1), 0x31, USAGE(1), 0x38,
    LOGICAL_MINIMUM(1), 0x81, LOGICAL_MAXIMUM(1), 0x7f, REPORT_SIZE(1), 0x08, REPORT_COUNT(1), 0x03,
    HIDINPUT(1), 0x06, USAGE_PAGE(1), 0x0c, USAGE(2), 0x38, 0x02,
    LOGICAL_MINIMUM(1), 0x81, LOGICAL_MAXIMUM(1), 0x7f, REPORT_SIZE(1), 0x08, REPORT_COUNT(1), 0x01,
    HIDINPUT(1), 0x06, END_COLLECTION(0), END_COLLECTION(0)
};
}  // namespace

class HeadlessBleMouse : public NimBLEServerCallbacks {
 public:
  void begin() {
    if (started_) return;
    const MouseIdentityProfile& identity = MouseIdentityLogic::profile(0);
    NimBLEDevice::init(identity.deviceName);
    applySavedAddress();
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(0x03);
    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(this, false);
    server_->advertiseOnDisconnect(true);
    hid_ = new NimBLEHIDDevice(server_);
    inputMouse_ = hid_->getInputReport(1);
    hid_->setManufacturer(identity.manufacturer);
    hid_->setPnp(0x02, identity.vendorId, identity.productId, 0x0110);
    hid_->setHidInfo(HID_COUNTRY_NOT_LOCALIZED, HID_INFO_NORMALLY_CONNECTABLE);
    hid_->setReportMap((uint8_t*)HID_REPORT_DESCRIPTOR, sizeof(HID_REPORT_DESCRIPTOR));
    hid_->setBatteryLevel(100);
    server_->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(HID_MOUSE);
    adv->addServiceUUID(hid_->getHidService()->getUUID());
    adv->setName(identity.deviceName);
    adv->addTxPower();
    adv->start();
    started_ = true;
    connected_ = false;
  }

  void end() {
    if (!started_) return;
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::deinit(true);
    started_ = false;
    connected_ = false;
    server_ = nullptr;
    hid_ = nullptr;
    inputMouse_ = nullptr;
  }

  bool clearPairing() {
    if (!started_) begin();
    clearingPairing_ = true;
    NimBLEDevice::stopAdvertising();
    if (server_) {
      const auto peers = server_->getPeerDevices();
      for (uint16_t handle : peers) {
        server_->disconnect(handle);
      }
      const uint32_t disconnectStartedAt = millis();
      while (server_->getConnectedCount() > 0 && millis() - disconnectStartedAt < 1000) {
        delay(20);
      }
    }
    const int bondsBefore = NimBLEDevice::getNumBonds();
    const bool cleared = NimBLEDevice::deleteAllBonds();
    const bool addressRotated = rotateSavedAddress();
    NimBLEDevice::deinit(true);
    started_ = false;
    connected_ = false;
    server_ = nullptr;
    hid_ = nullptr;
    inputMouse_ = nullptr;
    delay(250);
    clearingPairing_ = false;
    begin();
    Serial.print("[headless] mouse bonds cleared=");
    Serial.print(cleared ? "yes" : "no");
    Serial.print(" count=");
    Serial.print(bondsBefore);
    Serial.print(" address_rotated=");
    Serial.println(addressRotated ? "yes" : "no");
    return (cleared || bondsBefore == 0) && addressRotated;
  }

  bool isStarted() const { return started_; }
  bool isConnected() const { return started_ && connected_ && server_ && server_->getConnectedCount() > 0; }

  void move(signed char x, signed char y) {
    if (!isConnected()) return;
    uint8_t report[] = {0, static_cast<uint8_t>(x), static_cast<uint8_t>(y), 0, 0};
    inputMouse_->notify(report, sizeof(report));
  }

 private:
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { connected_ = true; }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    connected_ = false;
    if (!clearingPairing_) NimBLEDevice::startAdvertising();
  }

  static void makeRandomStaticAddress(uint8_t address[6]) {
    for (uint8_t i = 0; i < 6; ++i) {
      address[i] = static_cast<uint8_t>(esp_random() & 0xff);
    }
    // BLE random static addresses must have the two most significant bits set.
    address[5] = static_cast<uint8_t>((address[5] & 0x3f) | 0xc0);
  }

  bool loadSavedAddress(uint8_t address[6]) const {
    Preferences prefs;
    prefs.begin(HEADLESS_PREF_NS, true);
    const size_t length = prefs.getBytesLength(MOUSE_ADDR_KEY);
    const bool ok = length == 6 && prefs.getBytes(MOUSE_ADDR_KEY, address, 6) == 6;
    prefs.end();
    return ok;
  }

  bool rotateSavedAddress() const {
    uint8_t address[6];
    makeRandomStaticAddress(address);
    Preferences prefs;
    prefs.begin(HEADLESS_PREF_NS, false);
    const size_t written = prefs.putBytes(MOUSE_ADDR_KEY, address, 6);
    prefs.end();
    return written == 6;
  }

  void applySavedAddress() const {
    uint8_t address[6];
    if (!loadSavedAddress(address)) return;
    const bool addressSet = NimBLEDevice::setOwnAddr(address);
    const bool typeSet = NimBLEDevice::setOwnAddrType(0x01);
    Serial.print("[headless] mouse random address=");
    Serial.println(addressSet && typeSet ? "enabled" : "failed");
  }

  bool started_ = false;
  bool connected_ = false;
  bool clearingPairing_ = false;
  NimBLEServer* server_ = nullptr;
  NimBLEHIDDevice* hid_ = nullptr;
  NimBLECharacteristic* inputMouse_ = nullptr;
};

MinerCluster::SlaveEngine slave;
MouseEmulatorLogic mouseLogic;
HeadlessBleMouse bleMouse;
LedProgram ledProgram;
ButtonTracker buttonTracker;

Mode mode = Mode::Menu;
HeadlessApp menuSlot = HeadlessApp::Mouse;
uint32_t lastStatsAtMs = 0;
uint32_t lastMiningSeenAtMs = 0;
bool ledOn = false;

bool buttonDown() {
  return digitalRead(BUTTON_PIN) == LOW;
}

void setLedRaw(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, LED_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void LedProgram::setLed(bool on) {
  setLedRaw(on);
}

bool loadAutolaunchApp(HeadlessApp& app) {
  Preferences prefs;
  prefs.begin(HEADLESS_PREF_NS, true);
  const uint8_t savedApp = prefs.getUChar(AUTO_APP_KEY, 0);
  prefs.end();
  if (savedApp == static_cast<uint8_t>(HeadlessApp::Mouse)) {
    app = HeadlessApp::Mouse;
    return true;
  }
  if (savedApp == static_cast<uint8_t>(HeadlessApp::Miner)) {
    app = HeadlessApp::Miner;
    return true;
  }
  return false;
}

void saveAutolaunchApp(HeadlessApp app) {
  Preferences prefs;
  prefs.begin(HEADLESS_PREF_NS, false);
  prefs.putUChar(AUTO_APP_KEY, static_cast<uint8_t>(app));
  prefs.end();
  Serial.print("[headless] autolaunch=");
  Serial.println(app == HeadlessApp::Mouse ? "Mouse Emulator" : "Slave Miner");
}

void clearAutolaunchApp() {
  Preferences prefs;
  prefs.begin(HEADLESS_PREF_NS, false);
  const bool removed = prefs.remove(AUTO_APP_KEY);
  prefs.end();
  Serial.print("[headless] autolaunch cleared=");
  Serial.println(removed ? "yes" : "already-clear");
}

void addStep(LedStep* steps, uint8_t& count, bool on, uint16_t durationMs, bool selectWindow = false) {
  steps[count++] = {on, durationMs, selectWindow};
}

void addBlinks(LedStep* steps, uint8_t& count, uint8_t blinks) {
  for (uint8_t i = 0; i < blinks; ++i) {
    addStep(steps, count, false, SHORT_BLINK_MS);
    addStep(steps, count, true, SHORT_BLINK_MS);
  }
}

void addBlinkingWindow(LedStep* steps, uint8_t& count, uint16_t intervalMs) {
  uint32_t elapsed = 0;
  bool on = true;
  while (elapsed < STATE_WINDOW_MS && count < 70) {
    const uint16_t duration = static_cast<uint16_t>(min<uint32_t>(intervalMs, STATE_WINDOW_MS - elapsed));
    addStep(steps, count, on, duration);
    elapsed += duration;
    on = !on;
  }
}

void startMenuPattern(uint32_t nowMs) {
  LedStep steps[8] = {};
  uint8_t count = 0;
  addStep(steps, count, true, MENU_ON_MS);
  addBlinks(steps, count, menuSlot == HeadlessApp::Mouse ? 1 : 2);
  addStep(steps, count, false, MENU_SELECT_MS, true);
  ledProgram.start(steps, count, nowMs);
}

void flashAutolaunchCleared() {
  for (uint8_t i = 0; i < 3; ++i) {
    setLedRaw(false);
    delay(SHORT_BLINK_MS);
    setLedRaw(true);
    delay(SHORT_BLINK_MS);
  }
  setLedRaw(false);
  delay(220);
}

void appendAppMarker(LedStep* steps, uint8_t& count, HeadlessApp app) {
  addStep(steps, count, true, APP_MARKER_ON_MS);
  addBlinks(steps, count, app == HeadlessApp::Mouse ? 1 : 2);
  addStep(steps, count, true, 240);
  addStep(steps, count, false, APP_MARKER_STATE_GAP_MS);
}

void startMouseLedPattern(uint32_t nowMs) {
  LedStep steps[16] = {};
  uint8_t count = 0;
  appendAppMarker(steps, count, HeadlessApp::Mouse);

  const bool connected = bleMouse.isConnected();
  const bool started = bleMouse.isStarted();
  if (connected && mouseLogic.isEnabled()) {
    addStep(steps, count, true, STATE_WINDOW_MS);
  } else if (connected && !mouseLogic.isEnabled()) {
    addStep(steps, count, true, STATE_WINDOW_MS);
    addBlinks(steps, count, 1);
  } else if (!started) {
    addStep(steps, count, true, STATE_WINDOW_MS);
    addBlinks(steps, count, 2);
  } else {
    addStep(steps, count, true, STATE_WINDOW_MS);
    addBlinks(steps, count, 3);
  }
  addStep(steps, count, false, 220);
  ledProgram.start(steps, count, nowMs);
}

void startMinerLedPattern(uint32_t nowMs) {
  LedStep steps[72] = {};
  uint8_t count = 0;
  appendAppMarker(steps, count, HeadlessApp::Miner);

  const MinerCluster::SlaveStats stats = slave.stats();
  if (stats.state == MinerCluster::SlaveState::Mining) {
    lastMiningSeenAtMs = nowMs;
  }

  if (nowMs - lastMiningSeenAtMs < 3000) {
    addStep(steps, count, true, STATE_WINDOW_MS);
  } else if (stats.state == MinerCluster::SlaveState::Error) {
    addBlinkingWindow(steps, count, 120);
  } else if (!stats.paired || stats.state == MinerCluster::SlaveState::Searching) {
    addBlinkingWindow(steps, count, 700);
  } else {
    addBlinkingWindow(steps, count, 2000);
  }

  addStep(steps, count, false, 220);
  ledProgram.start(steps, count, nowMs);
}

void stopCurrentApp() {
  if (mode == Mode::Mouse) {
    bleMouse.end();
  } else if (mode == Mode::Miner) {
    slave.stop();
  }
}

void enterMenu(uint32_t nowMs) {
  stopCurrentApp();
  mode = Mode::Menu;
  menuSlot = HeadlessApp::Mouse;
  ledProgram.clear(nowMs);
  setLedRaw(false);
  Serial.println("[headless] menu");
}

void enterMouse(uint32_t nowMs) {
  stopCurrentApp();
  mode = Mode::Mouse;
  mouseLogic.reset(nowMs);
  bleMouse.begin();
  ledProgram.clear(nowMs);
  Serial.println("[headless] app=Mouse Emulator device=Logitech Signature M650");
}

void enterMiner(uint32_t nowMs) {
  stopCurrentApp();
  mode = Mode::Miner;
  lastMiningSeenAtMs = 0;
  slave.begin();
  ledProgram.clear(nowMs);
  Serial.println("[headless] app=Slave Miner");
}

void enterApp(HeadlessApp app, uint32_t nowMs) {
  if (app == HeadlessApp::Mouse) enterMouse(nowMs);
  else enterMiner(nowMs);
}

HeadlessApp currentApp() {
  return mode == Mode::Mouse ? HeadlessApp::Mouse : HeadlessApp::Miner;
}

void sendHumanizedMouseStep() {
  int y = 0;
  mouseLogic.verticalJitterCountdown()--;
  if (mouseLogic.verticalJitterCountdown() <= 0) {
    if (mouseLogic.verticalOffset() >= MOUSE_VERTICAL_WIGGLE_LIMIT) y = -1;
    else if (mouseLogic.verticalOffset() <= -MOUSE_VERTICAL_WIGGLE_LIMIT) y = 1;
    else y = random(0, 2) == 0 ? -1 : 1;
    mouseLogic.verticalOffset() += y;
    mouseLogic.verticalJitterCountdown() = random(MOUSE_VERTICAL_WIGGLE_MIN_STEPS, MOUSE_VERTICAL_WIGGLE_MAX_STEPS);
  }
  bleMouse.move(static_cast<signed char>(mouseLogic.movementDirection()), static_cast<signed char>(y));
  delayMicroseconds(200);
}

void updateMouse(uint32_t nowMs) {
  const bool connected = bleMouse.isConnected();

  if (mouseLogic.shouldStartMove(nowMs, connected)) {
    mouseLogic.startMove(nowMs, 500, 5000);
  } else if (mouseLogic.getPhase() == MouseEmulatorLogic::Forward) {
    if (!connected) {
      mouseLogic.setPhase(MouseEmulatorLogic::Idle);
      mouseLogic.setMoving(false);
      mouseLogic.setLastJiggleTime(nowMs);
    } else {
      const int steps = min(MOUSE_STEPS_PER_TICK, mouseLogic.remainingSteps());
      for (int i = 0; i < steps; ++i) sendHumanizedMouseStep();
      mouseLogic.remainingSteps() -= steps;
      if (mouseLogic.remainingSteps() <= 0) {
        mouseLogic.setWaitUntil(nowMs + random(400, 1200));
        mouseLogic.setPhase(MouseEmulatorLogic::Waiting);
      }
    }
  } else if (mouseLogic.getPhase() == MouseEmulatorLogic::Waiting) {
    if (nowMs >= mouseLogic.getWaitUntil()) {
      mouseLogic.setPhase(MouseEmulatorLogic::Back);
      mouseLogic.remainingSteps() = mouseLogic.getLastMovementPixels();
      mouseLogic.movementDirection() = -1;
    }
  } else if (mouseLogic.getPhase() == MouseEmulatorLogic::Back) {
    if (!connected) {
      mouseLogic.setPhase(MouseEmulatorLogic::Idle);
      mouseLogic.setMoving(false);
      mouseLogic.setLastJiggleTime(nowMs);
    } else {
      const int steps = min(MOUSE_STEPS_PER_TICK, mouseLogic.remainingSteps());
      for (int i = 0; i < steps; ++i) sendHumanizedMouseStep();
      mouseLogic.remainingSteps() -= steps;
      if (mouseLogic.remainingSteps() <= 0) {
        mouseLogic.setPhase(MouseEmulatorLogic::Idle);
        mouseLogic.setMoving(false);
        mouseLogic.setLastJiggleTime(nowMs);
        mouseLogic.calculateNextInterval();
      }
    }
  }
}

void handleAppButton(ButtonEvent event, uint32_t nowMs) {
  if (event == ButtonEvent::Hold5s) {
    enterMenu(nowMs);
  } else if (event == ButtonEvent::TwoTaps) {
    saveAutolaunchApp(currentApp());
  } else if (event == ButtonEvent::ThreeTaps && mode == Mode::Mouse) {
    bleMouse.clearPairing();
    mouseLogic.reset(nowMs);
    ledProgram.clear(nowMs);
  } else if (event == ButtonEvent::ThreeTaps && mode == Mode::Miner) {
    slave.clearPairing();
    Serial.println("[headless] miner pairing cleared");
  }
}

void updateMenu(ButtonEvent event, uint32_t nowMs) {
  if (event == ButtonEvent::TwoTaps) {
    clearAutolaunchApp();
    ledProgram.clear(nowMs);
    flashAutolaunchCleared();
    startMenuPattern(millis());
    return;
  }

  if (event == ButtonEvent::OneTap && ledProgram.inSelectWindow()) {
    enterApp(menuSlot, nowMs);
    return;
  }

  ledProgram.update(nowMs);
  if (ledProgram.done()) {
    menuSlot = menuSlot == HeadlessApp::Mouse ? HeadlessApp::Miner : HeadlessApp::Mouse;
    startMenuPattern(nowMs);
  }
}

void updateLedForMode(uint32_t nowMs) {
  ledProgram.update(nowMs);
  if (!ledProgram.done()) return;
  if (mode == Mode::Mouse) startMouseLedPattern(nowMs);
  else if (mode == Mode::Miner) startMinerLedPattern(nowMs);
}

void printStats(uint32_t nowMs) {
  if (nowMs - lastStatsAtMs < 2000) return;
  lastStatsAtMs = nowMs;

  Serial.print("[headless] build=");
  Serial.print(BuildInfo::BUILD_TEXT);
  Serial.print(" mode=");

  if (mode == Mode::Mouse) {
    Serial.print("mouse connected=");
    Serial.print(bleMouse.isConnected() ? "yes" : "no");
    Serial.print(" enabled=");
    Serial.print(mouseLogic.isEnabled() ? "yes" : "no");
    Serial.print(" moving=");
    Serial.println(mouseLogic.isMoving() ? "yes" : "no");
  } else if (mode == Mode::Miner) {
    const MinerCluster::SlaveStats stats = slave.stats();
    Serial.print("miner state=");
    Serial.print(MinerCluster::slaveStateLabel(stats.state));
    Serial.print(" paired=");
    Serial.print(stats.paired ? "yes" : "no");
    Serial.print(" channel=");
    Serial.print(stats.channel);
    Serial.print(" khps=");
    Serial.print(stats.hashrate / 1000.0f, 1);
    Serial.print(" jobs=");
    Serial.print(stats.jobs);
    Serial.print(" completed=");
    Serial.print(stats.completed);
    Serial.print(" total_kh=");
    Serial.print(static_cast<unsigned long>(stats.totalHashes / 1000ULL));
    if (stats.lastError[0]) {
      Serial.print(" error=");
      Serial.print(stats.lastError);
    }
    Serial.println();
  } else {
    Serial.print("menu slot=");
    Serial.println(menuSlot == HeadlessApp::Mouse ? "mouse" : "miner");
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setLedRaw(false);
  Serial.begin(115200);
  delay(300);
  randomSeed(static_cast<uint32_t>(esp_random()));
  buttonTracker.begin(buttonDown(), millis());

  Serial.println();
  Serial.print("[headless] Micri C3 Headless ");
  Serial.println(BuildInfo::BUILD_TEXT);
  Serial.println("[headless] apps=Mouse Emulator, Slave Miner");
  Serial.println("[headless] hold BOOT 5s in app to return to LED menu");

  HeadlessApp autolaunchApp = HeadlessApp::Mouse;
  if (loadAutolaunchApp(autolaunchApp)) {
    enterApp(autolaunchApp, millis());
  } else {
    Serial.println("[headless] no saved autolaunch; starting LED menu");
    enterMenu(millis());
  }
}

void loop() {
  const uint32_t nowMs = millis();
  const ButtonEvent event = buttonTracker.update(buttonDown(), nowMs);

  if (mode == Mode::Menu) {
    updateMenu(event, nowMs);
  } else {
    handleAppButton(event, nowMs);
    if (mode == Mode::Mouse) {
      updateMouse(nowMs);
    } else if (mode == Mode::Miner) {
      slave.update();
    }
    updateLedForMode(nowMs);
  }

  printStats(nowMs);
  delay(1);
}
