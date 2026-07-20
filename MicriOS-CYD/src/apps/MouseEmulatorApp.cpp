#include "MouseEmulatorApp.h"
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUUID.h>
#include <HIDTypes.h>

#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include "../shared/logic/MouseIdentityLogic.h"

namespace {
constexpr uint8_t HID_COUNTRY_NOT_LOCALIZED = 0x00;
constexpr uint8_t HID_INFO_NORMALLY_CONNECTABLE = 0x02;
constexpr int VERTICAL_WIGGLE_LIMIT = 20;
constexpr int VERTICAL_WIGGLE_MIN_STEPS = 8;
constexpr int VERTICAL_WIGGLE_MAX_STEPS = 28;
constexpr TouchUi::Rect PROFILE_ROWS[5] = {
    {12, 38, 143, 31}, {165, 38, 143, 31},
    {12, 75, 143, 31}, {165, 75, 143, 31},
    {12, 112, 143, 31},
};
constexpr TouchUi::Rect PROFILE_START = {165, 112, 143, 31};
constexpr TouchUi::Rect ACTIVE_TOGGLE = {12, 151, 143, 43};
constexpr TouchUi::Rect PAIR_AGAIN = {165, 151, 143, 43};

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

void keepNimBLEUuidLinked() {
  static NimBLEUUID hidServiceUuid(static_cast<uint16_t>(0x1812));
  (void)hidServiceUuid;
}

class NimbleMouse : public NimBLEServerCallbacks {
  public:
    void begin(const MouseIdentityProfile& identity, uint8_t profileIndex) {
      if (started_ && currentProfileIndex_ == profileIndex) return;
      if (started_) end();
      keepNimBLEUuidLinked();
      NimBLEDevice::init(identity.deviceName);
      NimBLEDevice::setSecurityAuth(true, false, true);
      NimBLEDevice::setSecurityIOCap(0x03); // No input/output: pair without a PIN.
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
      currentProfileIndex_ = profileIndex;
      started_ = true;
    }
    void end() {
      if (!started_) return;
      NimBLEDevice::stopAdvertising();
      NimBLEDevice::deinit(true);
      started_ = false;
      connected_ = false;
      currentProfileIndex_ = 255;
      server_ = nullptr;
      hid_ = nullptr;
      inputMouse_ = nullptr;
    }
    bool isConnected() const { return started_ && connected_ && server_ && server_->getConnectedCount() > 0; }
    void clearPairing() {
      if (!started_) return;
      NimBLEDevice::deleteAllBonds();
      connected_ = false;
      NimBLEDevice::startAdvertising();
    }
    void move(signed char x, signed char y) {
      if (!isConnected()) return;
      uint8_t report[] = {0, (uint8_t)x, (uint8_t)y, 0, 0};
      inputMouse_->notify(report, sizeof(report));
    }
  private:
    void onConnect(NimBLEServer* s, NimBLEConnInfo& c) override { connected_ = true; }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& c, int r) override {
      connected_ = false;
      NimBLEDevice::startAdvertising();
    }
    bool started_ = false, connected_ = false;
    uint8_t currentProfileIndex_ = 255;
    NimBLEServer* server_ = nullptr;
    NimBLEHIDDevice* hid_ = nullptr;
    NimBLECharacteristic* inputMouse_ = nullptr;
};
NimbleMouse bleMouse;
}

MouseEmulatorApp::MouseEmulatorApp(uint32_t width, uint32_t height)
    : App("Mouse Emulator", width, height) {}

bool MouseEmulatorApp::handleTouch(const TouchUi::TouchSample& sample,
                                   const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (phase() == AppPhase::Start) {
    hit = TouchUi::hitRectList(PROFILE_ROWS, 5, point);
    if (hit == TouchUi::NO_CONTROL && PROFILE_START.contains(point)) hit = 5;
  } else if (phase() == AppPhase::Running) {
    if (ACTIVE_TOGGLE.contains(point)) hit = 10;
    else if (PAIR_AGAIN.contains(point)) hit = 11;
  }

  const TouchUi::CaptureResult result = touchCapture_.update(sample, event, hit);
  if (!result.activated) return result.consumed;
  if (result.id >= 0 && result.id < 5) {
    profileIndex_ = static_cast<uint8_t>(result.id);
    MouseIdentityLogic::saveIndex(profileIndex_);
  } else if (result.id == 5) {
    MouseIdentityLogic::saveIndex(profileIndex_);
    startRunning();
  } else if (result.id == 10) {
    logic_.toggleEnabled(millis());
    uiInitialized_ = false;
  } else if (result.id == 11) {
    bleMouse.clearPairing();
    uiInitialized_ = false;
  }
  return true;
}

void MouseEmulatorApp::ensureProfileLoaded() {
  if (profileLoaded_) return;
  profileIndex_ = MouseIdentityLogic::loadIndex();
  profileLoaded_ = true;
}

void MouseEmulatorApp::cycleProfile() {
  ensureProfileLoaded();
  profileIndex_ = (profileIndex_ + 1) % MouseIdentityLogic::PROFILE_COUNT;
  MouseIdentityLogic::saveIndex(profileIndex_);
}

bool MouseEmulatorApp::updateStart(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  ensureProfileLoaded();
  if (b1.click) {
    cycleProfile();
  } else if (b1.longPress || b2.click) {
    MouseIdentityLogic::saveIndex(profileIndex_);
    startRunning();
  }
  return true;
}

void MouseEmulatorApp::onAppReset() {
  ensureProfileLoaded();
  logic_.reset(millis());
  bleMouse.begin(MouseIdentityLogic::profile(profileIndex_), profileIndex_);
  lastClockUpdate_ = millis();
  uiInitialized_ = false;
  touchCapture_.reset();
}

void MouseEmulatorApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  if (b1.click) logic_.toggleEnabled(millis());

  uint32_t now = millis();
  bool connected = bleMouse.isConnected();

  if (logic_.shouldStartMove(now, connected)) {
    logic_.startMove(now, 500, 5000);
  } else if (logic_.getPhase() == MouseEmulatorLogic::Forward) {
    if (!connected) {
      logic_.setPhase(MouseEmulatorLogic::Idle);
      logic_.setMoving(false);
      logic_.setLastJiggleTime(now);
    } else {
      int steps = min(stepsPerTick_, logic_.remainingSteps());
      for (int i = 0; i < steps; ++i) sendHumanizedStep();
      logic_.remainingSteps() -= steps;
      if (logic_.remainingSteps() <= 0) {
        logic_.setWaitUntil(now + random(400, 1200));
        logic_.setPhase(MouseEmulatorLogic::Waiting);
      }
    }
  } else if (logic_.getPhase() == MouseEmulatorLogic::Waiting) {
    if (now >= logic_.getWaitUntil()) {
      logic_.setPhase(MouseEmulatorLogic::Back);
      logic_.remainingSteps() = logic_.getLastMovementPixels();
      logic_.movementDirection() = -1;
    }
  } else if (logic_.getPhase() == MouseEmulatorLogic::Back) {
    if (!connected) {
        logic_.setPhase(MouseEmulatorLogic::Idle);
        logic_.setMoving(false);
        logic_.setLastJiggleTime(now);
    } else {
      int steps = min(stepsPerTick_, logic_.remainingSteps());
      for (int i = 0; i < steps; ++i) sendHumanizedStep();
      logic_.remainingSteps() -= steps;
      if (logic_.remainingSteps() <= 0) {
        logic_.setPhase(MouseEmulatorLogic::Idle);
        logic_.setMoving(false);
        logic_.setLastJiggleTime(now);
        logic_.calculateNextInterval();
      }
    }
  }

  if (!logic_.isMoving() && (now - lastClockUpdate_ >= 1000)) {
    lastClockUpdate_ = now;
  }
}

void MouseEmulatorApp::sendHumanizedStep() {
  int y = 0;
  logic_.verticalJitterCountdown()--;
  if (logic_.verticalJitterCountdown() <= 0) {
    if (logic_.verticalOffset() >= VERTICAL_WIGGLE_LIMIT) y = -1;
    else if (logic_.verticalOffset() <= -VERTICAL_WIGGLE_LIMIT) y = 1;
    else y = random(0, 2) == 0 ? -1 : 1;
    logic_.verticalOffset() += y;
    logic_.verticalJitterCountdown() = random(VERTICAL_WIGGLE_MIN_STEPS, VERTICAL_WIGGLE_MAX_STEPS);
  }
  bleMouse.move((signed char)logic_.movementDirection(), (signed char)y);
  delayMicroseconds(200);
}

void MouseEmulatorApp::drawRunning(TFT_eSPI& tft) {
  bool currentConn = bleMouse.isConnected();
  bool enabled = logic_.isEnabled();
  const int8_t connectionPhase = currentConn ? static_cast<int8_t>((millis() / 5000) % 2) : 2;

  int32_t countdownValue = INT32_MIN;
  if (!currentConn || !enabled) {
    countdownValue = -1;
  } else if (logic_.isMoving()) {
    countdownValue = -2;
  } else {
    uint32_t elapsed = millis() - logic_.getLastJiggleTime();
    long remaining = 0;
    if (logic_.getTargetIntervalMs() > elapsed) remaining = (logic_.getTargetIntervalMs() - elapsed) / 1000;
    countdownValue = remaining;
  }

  if (uiInitialized_ &&
      currentConn == lastKnownConnection_ &&
      enabled == lastKnownEnabled_ &&
      connectionPhase == lastConnectionPhase_ &&
      countdownValue == lastCountdownValue_ &&
      logic_.getLastMovementPixels() == lastMovementPixels_) {
    return;
  }

  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    drawDashboard(canvas, currentConn, enabled, connectionPhase, countdownValue);
  });

  lastKnownConnection_ = currentConn;
  lastKnownEnabled_ = enabled;
  lastConnectionPhase_ = connectionPhase;
  lastCountdownValue_ = countdownValue;
  lastMovementPixels_ = logic_.getLastMovementPixels();
  uiInitialized_ = true;
}

template <typename Canvas>
void MouseEmulatorApp::drawDashboard(Canvas& tft, bool connected, bool enabled, int8_t connectionPhase, int32_t countdownValue) {
  const MouseIdentityProfile& profile = MouseIdentityLogic::profile(profileIndex_);
  CydUi::header(tft, "Mouse Emulator", TFT_ORANGE);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Advertises as:", 10, 36);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(profile.shortName, 10, 49);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Host", 10, 78);
  tft.drawString("Core", 10, 94);
  tft.drawString("Next", 10, 110);

  if (connected) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(connectionPhase == 0 ? "PAIRED" : "READY", 70, 78);
  } else {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("WAITING TO PAIR", 70, 78);
  }

  tft.setTextColor(enabled ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.drawString(enabled ? "ACTIVE" : "PAUSED", 70, 94);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  if (logic_.getLastMovementPixels() > 0) tft.drawString(String(logic_.getLastMovementPixels()) + " px", 154, 94);
  else tft.drawString("no run", 154, 94);

  if (countdownValue == -1) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("STANDBY", 70, 110);
  } else if (countdownValue == -2) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("SWEEPING", 70, 110);
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(countdownValue) + " seconds", 70, 110);
  }

  CydUi::touchAction(tft, ACTIVE_TOGGLE, enabled ? "Pause" : "Resume",
                     enabled ? TFT_ORANGE : TFT_GREEN);
  CydUi::touchAction(tft, PAIR_AGAIN, "Pair Again", TFT_CYAN, false);
}

void MouseEmulatorApp::drawStart(TFT_eSPI& tft) {
    ensureProfileLoaded();
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
      CydUi::header(canvas, "Choose Mouse", TFT_ORANGE);
      for (uint8_t i = 0; i < MouseIdentityLogic::PROFILE_COUNT; ++i) {
        const MouseIdentityProfile& profile = MouseIdentityLogic::profile(i);
        CydUi::touchAction(canvas, PROFILE_ROWS[i], profile.shortName,
                           i == profileIndex_ ? TFT_ORANGE : TFT_LIGHTGREY,
                           i == profileIndex_);
      }
      CydUi::touchAction(canvas, PROFILE_START, "Start", TFT_GREEN);
    });
}

bool MouseEmulatorApp::hasCustomOverlay() const { return true; }

void MouseEmulatorApp::onAppExit() {
  bleMouse.end();
}
