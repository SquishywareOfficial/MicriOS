#include "ClockApp.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

namespace {
constexpr uint32_t SPLASH_MS = 900;
constexpr uint32_t CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t CONNECTED_MS = 800;
constexpr uint32_t SYNC_TIMEOUT_MS = 12000;
constexpr uint32_t TIMEZONE_IDLE_RETURN_MS = 2200;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;
constexpr const char* HOSTNAME = "MicriDeck-C3";

int16_t handX(int16_t cx, float angle, int16_t length) {
  return cx + static_cast<int16_t>(cosf(angle) * length);
}

int16_t handY(int16_t cy, float angle, int16_t length) {
  return cy + static_cast<int16_t>(sinf(angle) * length);
}
}

ClockApp::ClockApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Micri Clock", width, height) {
  (void)left;
}

bool ClockApp::startsRunningImmediately() const {
  return true;
}

bool ClockApp::hasCustomOverlay() const {
  return true;
}

void ClockApp::onAppReset() {
  clock_.begin();
  wifi_.init();
  mode_ = Mode::Splash;
  modeStartedAtMs_ = millis();
  timezoneIdleMs_ = 0;
  currentSlot_ = 0;
  currentSsid_ = "";
  if (!hasSync_ && timeIsValid()) {
    noteSync();
  }
  stopWifi();
}

bool ClockApp::timeIsValid() const {
  return time(nullptr) >= VALID_TIME_THRESHOLD;
}

bool ClockApp::hasUsableTime() const {
  return timeIsValid() || hasSync_;
}

void ClockApp::noteSync() {
  lastSyncEpoch_ = time(nullptr);
  lastSyncMillis_ = millis();
  hasSync_ = lastSyncEpoch_ >= VALID_TIME_THRESHOLD;
}

time_t ClockApp::currentTime() const {
  if (timeIsValid()) {
    return time(nullptr);
  }
  if (!hasSync_) {
    return 0;
  }
  return lastSyncEpoch_ + static_cast<time_t>((millis() - lastSyncMillis_) / 1000);
}

uint32_t ClockApp::syncAgeSeconds() const {
  if (!hasSync_) {
    return 0;
  }
  return (millis() - lastSyncMillis_) / 1000;
}

void ClockApp::syncLabel(char* out, size_t outSize) const {
  if (!hasSync_) {
    snprintf(out, outSize, "no sync");
    return;
  }
  ClockLogic::formatSyncAge(syncAgeSeconds(), out, outSize);
}

void ClockApp::stopWifi() {
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
}

void ClockApp::beginRetry() {
  wifi_.loadProfiles();
  currentSlot_ = 0;
  currentSsid_ = "";
  if (!beginNextProfile(0)) {
    modeStartedAtMs_ = millis();
  }
}

bool ClockApp::beginNextProfile(uint8_t startSlot) {
  bool anyProfile = false;
  for (uint8_t slot = 0; slot < WiFiLogic::MAX_PROFILES; slot++) {
    if (!wifi_.isProfileUsed(slot)) {
      continue;
    }
    anyProfile = true;
    if (slot < startSlot) {
      continue;
    }

    currentSlot_ = slot;
    currentSsid_ = wifi_.getSsid(slot);
    const String pass = wifi_.getPass(slot);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(HOSTNAME);
    WiFi.disconnect(false, false);
    WiFi.begin(currentSsid_.c_str(), pass.c_str());
    mode_ = Mode::Connecting;
    modeStartedAtMs_ = millis();
    return true;
  }

  stopWifi();
  mode_ = anyProfile ? Mode::ConnectFailed : Mode::NoProfiles;
  return false;
}

void ClockApp::beginSync() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  mode_ = Mode::Syncing;
  modeStartedAtMs_ = millis();
}

void ClockApp::enterClock() {
  stopWifi();
  mode_ = Mode::Clock;
  modeStartedAtMs_ = millis();
}

void ClockApp::handleClockAction(ClockLogic::Action action) {
  if (action == ClockLogic::Action::RequestSync) {
    beginRetry();
  } else if (action == ClockLogic::Action::RequestExit) {
    clock_.saveSettings();
    requestExitToMenu();
  }
}

void ClockApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  const uint32_t nowMs = millis();

  if (mode_ == Mode::Splash) {
    if (input.longPress) {
      requestExitToMenu();
      return;
    }
    if ((nowMs - modeStartedAtMs_) >= SPLASH_MS) {
      beginRetry();
    }
    return;
  }

  if (mode_ == Mode::NoProfiles || mode_ == Mode::ConnectFailed || mode_ == Mode::SyncFailed) {
    if (input.longPress) {
      requestExitToMenu();
      return;
    }
    if (hasUsableTime() && ((nowMs - modeStartedAtMs_) > 1400)) {
      enterClock();
      return;
    }
    if (input.click && mode_ != Mode::NoProfiles) {
      if (mode_ == Mode::SyncFailed && WiFi.status() == WL_CONNECTED) {
        beginSync();
      } else {
        beginRetry();
      }
    }
    return;
  }

  if (mode_ == Mode::Connecting) {
    if (input.longPress) {
      stopWifi();
      requestExitToMenu();
      return;
    }
    if (WiFi.status() == WL_CONNECTED) {
      mode_ = Mode::Connected;
      modeStartedAtMs_ = nowMs;
      return;
    }
    if ((nowMs - modeStartedAtMs_) >= CONNECT_TIMEOUT_MS) {
      beginNextProfile(currentSlot_ + 1);
    }
    return;
  }

  if (mode_ == Mode::Connected) {
    if ((nowMs - modeStartedAtMs_) >= CONNECTED_MS) {
      beginSync();
    }
    return;
  }

  if (mode_ == Mode::Syncing) {
    if (timeIsValid()) {
      noteSync();
      enterClock();
      return;
    }
    if ((nowMs - modeStartedAtMs_) >= SYNC_TIMEOUT_MS) {
      mode_ = Mode::SyncFailed;
      modeStartedAtMs_ = nowMs;
    }
    return;
  }

  if (clock_.view() == ClockLogic::View::Timezone) {
    timezoneIdleMs_ += deltaMs;
    if (input.click) {
      clock_.nextZone();
      timezoneIdleMs_ = 0;
    }
    if (input.longPress) {
      clock_.previousZone();
      timezoneIdleMs_ = 0;
    }
    if (timezoneIdleMs_ >= TIMEZONE_IDLE_RETURN_MS) {
      clock_.closeTimezone();
    }
    return;
  }

  timezoneIdleMs_ = 0;
  if (clock_.view() == ClockLogic::View::Options) {
    if (input.click) {
      clock_.nextOption();
    }
    if (input.longPress) {
      handleClockAction(clock_.activateOption());
    }
    return;
  }

  if (input.click) {
    clock_.nextFace();
  }
  if (input.longPress) {
    if (clock_.face() == ClockLogic::Face::Options) {
      handleClockAction(clock_.selectFace());
    } else {
      clock_.previousFace();
    }
  }
}

void ClockApp::drawFit(U8G2& u8g2, int x, int y, const char* text) const {
  u8g2.setFont(u8g2_font_5x8_tr);
  if (u8g2.getStrWidth(text) > static_cast<int>(width - x - 3)) {
    u8g2.setFont(u8g2_font_4x6_tr);
  }
  u8g2.drawStr(x, y, text);
}

void ClockApp::drawNetworkStatus(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);

  if (mode_ == Mode::Splash) {
    u8g2.drawStr(8, 12, "Micri");
    u8g2.drawStr(8, 24, "Clock");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 38, "Testing WiFi");
    return;
  }

  if (mode_ == Mode::NoProfiles) {
    u8g2.drawStr(3, 9, "No WiFi");
    u8g2.drawStr(3, 20, "Saved");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 31, hasUsableTime() ? "Offline" : "Use WiFi");
    u8g2.drawStr(3, 38, hasUsableTime() ? "Clock..." : "Settings");
    return;
  }

  if (mode_ == Mode::Connecting) {
    u8g2.drawStr(3, 9, "Trying");
    drawFit(u8g2, 3, 21, currentSsid_.c_str());
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 38, "Hold menu");
    return;
  }

  if (mode_ == Mode::Connected) {
    u8g2.drawStr(3, 12, "CONNECTED");
    drawFit(u8g2, 3, 26, currentSsid_.c_str());
    return;
  }

  if (mode_ == Mode::Syncing) {
    u8g2.drawStr(3, 13, "Syncing");
    u8g2.drawStr(3, 26, "time...");
    return;
  }

  if (mode_ == Mode::ConnectFailed) {
    u8g2.drawStr(3, 9, "Connect");
    u8g2.drawStr(3, 20, "Failed");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 31, hasUsableTime() ? "Offline" : "Tap retry");
    u8g2.drawStr(3, 38, hasUsableTime() ? "Clock..." : "Hold menu");
    return;
  }

  u8g2.drawStr(3, 9, "Time Sync");
  u8g2.drawStr(3, 20, "Failed");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 31, hasUsableTime() ? "Offline" : "Tap retry");
  u8g2.drawStr(3, 38, hasUsableTime() ? "Clock..." : "Hold menu");
}

void ClockApp::drawDigital(U8G2& u8g2, time_t now) {
  char timeBuf[14];
  clock_.formatTime(now, timeBuf, sizeof(timeBuf), clock_.use24Hour());

  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_7x13B_tr);
  if (u8g2.getStrWidth(timeBuf) > static_cast<int>(width - 4)) {
    u8g2.setFont(u8g2_font_6x10_tr);
  }
  u8g2.drawStr((width + 2 - u8g2.getStrWidth(timeBuf)) / 2, clock_.showDate() ? 22 : 25, timeBuf);

  u8g2.setFont(u8g2_font_4x6_tr);
  if (clock_.showDate()) {
    char dateBuf[12];
    clock_.formatDate(now, dateBuf, sizeof(dateBuf));
    u8g2.drawStr(3, 8, dateBuf);
  }
  if (clock_.showOffset()) {
    char status[14];
    if (hasSync_ && ((millis() / 2000) % 2) == 1) {
      syncLabel(status, sizeof(status));
      u8g2.drawStr(3, 38, status);
    } else {
      u8g2.drawStr(3, 38, clock_.zoneLabel());
    }
  }
}

void ClockApp::drawAnalog(U8G2& u8g2, time_t now) {
  const ClockLogic::TimeParts parts = clock_.localParts(now);
  const int cx = 35;
  const int cy = 20;
  const int radius = 15;
  const float minuteAngle = (parts.minute * 6.0f - 90.0f) * DEG_TO_RAD;
  const float hourAngle = (((parts.hour % 12) + parts.minute / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;

  u8g2.drawCircle(cx, cy, radius);
  u8g2.drawPixel(cx, cy);
  u8g2.drawLine(cx, cy, handX(cx, hourAngle, 8), handY(cy, hourAngle, 8));
  u8g2.drawLine(cx, cy, handX(cx, minuteAngle, 12), handY(cy, minuteAngle, 12));

  u8g2.setFont(u8g2_font_4x6_tr);
  if (clock_.showOffset()) {
    char status[14];
    if (hasSync_ && ((millis() / 2000) % 2) == 1) {
      syncLabel(status, sizeof(status));
      u8g2.drawStr(3, 38, status);
    } else {
      u8g2.drawStr(3, 38, clock_.zoneLabel());
    }
  } else {
    u8g2.drawStr(3, 38, "Tap face Hold-");
  }
}

void ClockApp::drawUnix(U8G2& u8g2, time_t now) {
  char unixBuf[16];
  snprintf(unixBuf, sizeof(unixBuf), "%lu", static_cast<unsigned long>(now));
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 10, "UNIX UTC");
  u8g2.drawStr((width + 2 - u8g2.getStrWidth(unixBuf)) / 2, 25, unixBuf);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap face Hold-");
}

void ClockApp::drawOptionsFace(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(13, 15, "Options");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 31, "Hold open");
  u8g2.drawStr(3, 38, "Tap next");
}

void ClockApp::drawOptions(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 8, "Clock Options");
  drawFit(u8g2, 3, 22, clock_.optionLabel());
  char zonePos[8];
  if (clock_.option() == ClockLogic::Option::NextZone || clock_.option() == ClockLogic::Option::Timezone ||
      clock_.option() == ClockLogic::Option::AddZone ||
      clock_.option() == ClockLogic::Option::DeleteZone) {
    clock_.formatZonePosition(zonePos, sizeof(zonePos));
    drawFit(u8g2, 3, 31, zonePos);
    drawFit(u8g2, 23, 31, clock_.optionValue());
  } else {
    drawFit(u8g2, 3, 31, clock_.optionValue());
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap next Hold");
}

void ClockApp::drawTimezone(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Timezone");
  drawFit(u8g2, 3, 23, clock_.pickerZoneLabel());
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap+ Hold-");
}

void ClockApp::drawRunning(U8G2& u8g2) {
  if (mode_ != Mode::Clock) {
    drawNetworkStatus(u8g2);
    return;
  }

  if (clock_.view() == ClockLogic::View::Options) {
    drawOptions(u8g2);
    return;
  }
  if (clock_.view() == ClockLogic::View::Timezone) {
    drawTimezone(u8g2);
    return;
  }

  const time_t now = currentTime();
  switch (clock_.face()) {
    case ClockLogic::Face::Digital:
      drawDigital(u8g2, now);
      break;
    case ClockLogic::Face::Analog:
      drawAnalog(u8g2, now);
      break;
    case ClockLogic::Face::UnixTime:
      drawUnix(u8g2, now);
      break;
    case ClockLogic::Face::Options:
    default:
      drawOptionsFace(u8g2);
      break;
  }
}
