#include "ClockApp.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint32_t SPLASH_MS = 900;
constexpr uint32_t CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t CONNECTED_MS = 800;
constexpr uint32_t SYNC_TIMEOUT_MS = 12000;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;
constexpr const char* HOSTNAME = "MicriDeck-TDisplay";

int16_t handX(int16_t cx, float angle, int16_t length) {
  return cx + static_cast<int16_t>(cosf(angle) * length);
}

int16_t handY(int16_t cy, float angle, int16_t length) {
  return cy + static_cast<int16_t>(sinf(angle) * length);
}

template <typename Canvas>
void drawCentered(Canvas& tft, const String& text, int y, uint8_t size, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(size);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, (TDisplayUi::W - tft.textWidth(text)) / 2, y);
}
}

ClockApp::ClockApp(uint32_t width, uint32_t height)
    : App("Micri Clock", width, height) {}

bool ClockApp::startsRunningImmediately() const {
  return true;
}

bool ClockApp::hasCustomOverlay() const {
  return true;
}

void ClockApp::markDirty() {
  dirty_ = true;
}

void ClockApp::onAppReset() {
  clock_.begin();
  wifi_.init();
  mode_ = Mode::Splash;
  modeStartedAtMs_ = millis();
  currentSlot_ = 0;
  currentSsid_ = "";
  renderedOnce_ = false;
  markDirty();
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
    markDirty();
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
    markDirty();
    return true;
  }

  stopWifi();
  mode_ = anyProfile ? Mode::ConnectFailed : Mode::NoProfiles;
  markDirty();
  return false;
}

void ClockApp::beginSync() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  mode_ = Mode::Syncing;
  modeStartedAtMs_ = millis();
  markDirty();
}

void ClockApp::enterClock() {
  stopWifi();
  mode_ = Mode::Clock;
  modeStartedAtMs_ = millis();
  markDirty();
}

void ClockApp::handleClockAction(ClockLogic::Action action) {
  if (action == ClockLogic::Action::RequestSync) {
    beginRetry();
  } else if (action == ClockLogic::Action::RequestExit) {
    clock_.saveSettings();
    requestExitToMenu();
  }
}

void ClockApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  const uint32_t nowMs = millis();

  if (mode_ == Mode::Splash) {
    if (b2.click) {
      requestExitToMenu();
      return;
    }
    if ((nowMs - modeStartedAtMs_) >= SPLASH_MS) {
      beginRetry();
    }
    return;
  }

  if (mode_ == Mode::NoProfiles || mode_ == Mode::ConnectFailed || mode_ == Mode::SyncFailed) {
    if (b2.click) {
      requestExitToMenu();
      return;
    }
    if (hasUsableTime() && ((nowMs - modeStartedAtMs_) > 1400)) {
      enterClock();
      return;
    }
    if (b1.click && mode_ != Mode::NoProfiles) {
      if (mode_ == Mode::SyncFailed && WiFi.status() == WL_CONNECTED) {
        beginSync();
      } else {
        beginRetry();
      }
    }
    return;
  }

  if (mode_ == Mode::Connecting) {
    if (b2.click) {
      stopWifi();
      requestExitToMenu();
      return;
    }
    if (WiFi.status() == WL_CONNECTED) {
      mode_ = Mode::Connected;
      modeStartedAtMs_ = nowMs;
      markDirty();
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
      markDirty();
    }
    return;
  }

  if (clock_.view() == ClockLogic::View::Timezone) {
    if (b1.click) {
      clock_.nextZone();
      markDirty();
    }
    if (b2.click) {
      clock_.previousZone();
      markDirty();
    }
    if (b1.longPress) {
      clock_.closeTimezone();
      markDirty();
    }
    return;
  }

  if (clock_.view() == ClockLogic::View::Options) {
    if (b1.click) {
      clock_.nextOption();
      markDirty();
    }
    if (b2.click) {
      clock_.previousOption();
      markDirty();
    }
    if (b1.longPress) {
      handleClockAction(clock_.activateOption());
      markDirty();
    }
    return;
  }

  if (b1.click) {
    clock_.nextFace();
    markDirty();
  }
  if (b2.click) {
    clock_.previousFace();
    markDirty();
  }
  if (b1.longPress && clock_.face() == ClockLogic::Face::Options) {
    handleClockAction(clock_.selectFace());
    markDirty();
  }
}

bool ClockApp::shouldRedraw(time_t now) {
  if (!renderedOnce_ || dirty_ || mode_ != renderedMode_) {
    return true;
  }
  if (mode_ != Mode::Clock) {
    return false;
  }
  if (clock_.view() != renderedView_) return true;
  if (clock_.face() != renderedFace_) return true;
  if (clock_.option() != renderedOption_) return true;
  if (clock_.zoneIndex() != renderedZone_) return true;
  if (clock_.savedZoneCount() != renderedZoneCount_) return true;
  if (clock_.activeZoneSlot() != renderedActiveZoneSlot_) return true;
  if (clock_.view() != ClockLogic::View::Face || clock_.face() == ClockLogic::Face::Options) {
    return false;
  }
  return now != renderedSecond_;
}

bool ClockApp::needsFullRedraw() const {
  if (!renderedOnce_ || dirty_ || mode_ != renderedMode_) {
    return true;
  }
  if (mode_ != Mode::Clock) {
    return true;
  }
  if (clock_.view() != renderedView_) return true;
  if (clock_.face() != renderedFace_) return true;
  if (clock_.option() != renderedOption_) return true;
  if (clock_.zoneIndex() != renderedZone_) return true;
  if (clock_.savedZoneCount() != renderedZoneCount_) return true;
  if (clock_.activeZoneSlot() != renderedActiveZoneSlot_) return true;
  return false;
}

void ClockApp::noteRendered(time_t now) {
  renderedOnce_ = true;
  dirty_ = false;
  renderedMode_ = mode_;
  renderedView_ = clock_.view();
  renderedFace_ = clock_.face();
  renderedOption_ = clock_.option();
  renderedZone_ = clock_.zoneIndex();
  renderedZoneCount_ = clock_.savedZoneCount();
  renderedActiveZoneSlot_ = clock_.activeZoneSlot();
  renderedSecond_ = now;
}

template <typename Canvas>
void ClockApp::drawNetworkStatus(Canvas& tft) {
  TDisplayUi::clear(tft);

  if (mode_ == Mode::Splash) {
    TDisplayUi::header(tft, "Micri Clock", TFT_CYAN);
    drawCentered(tft, "Testing", 48, 3, TFT_WHITE);
    drawCentered(tft, "saved WiFi", 82, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B2 back");
    return;
  }

  if (mode_ == Mode::NoProfiles) {
    TDisplayUi::header(tft, "Micri Clock", TFT_RED, "NO WIFI");
    drawCentered(tft, "No WiFi Saved", 46, 2, TFT_RED);
    drawCentered(tft, hasUsableTime() ? "Offline Clock..." : "Use WiFi Settings", 78, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "B2 menu");
    return;
  }

  if (mode_ == Mode::Connecting) {
    TDisplayUi::header(tft, "Trying Network", TFT_YELLOW);
    drawCentered(tft, TDisplayUi::fitText(tft, currentSsid_, 210), 55, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "B2 cancel");
    return;
  }

  if (mode_ == Mode::Connected) {
    TDisplayUi::header(tft, "Micri Clock", TFT_GREEN, "OK");
    drawCentered(tft, "CONNECTED", 48, 3, TFT_GREEN);
    drawCentered(tft, TDisplayUi::fitText(tft, currentSsid_, 210), 88, 1, TFT_LIGHTGREY);
    return;
  }

  if (mode_ == Mode::Syncing) {
    TDisplayUi::header(tft, "Micri Clock", TFT_CYAN);
    drawCentered(tft, "Syncing", 48, 3, TFT_CYAN);
    drawCentered(tft, "NTP time", 84, 2, TFT_WHITE);
    return;
  }

  if (mode_ == Mode::ConnectFailed) {
    TDisplayUi::header(tft, "Micri Clock", TFT_RED, "FAIL");
    drawCentered(tft, "Connect Failed", 48, 2, TFT_RED);
    drawCentered(tft, hasUsableTime() ? "Offline Clock..." : "Check WiFi Settings", 78, 1, TFT_WHITE);
    TDisplayUi::footer(tft, hasUsableTime() ? "B2 menu" : "B1 retry  B2 menu");
    return;
  }

  TDisplayUi::header(tft, "Micri Clock", TFT_RED, "NTP");
  drawCentered(tft, "Time Sync Failed", 48, 2, TFT_RED);
  drawCentered(tft, hasUsableTime() ? "Offline Clock..." : "Internet may be down", 78, 1, TFT_WHITE);
  TDisplayUi::footer(tft, hasUsableTime() ? "B2 menu" : "B1 retry  B2 menu");
}

template <typename Canvas>
void ClockApp::drawDigital(Canvas& tft, time_t now) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Digital Clock", TFT_CYAN, clock_.showOffset() ? clock_.zoneLabel() : nullptr);

  char timeBuf[16];
  clock_.formatTime(now, timeBuf, sizeof(timeBuf), true);
  TDisplayUi::largeValue(tft, timeBuf, clock_.showDate() ? 51 : 58, TFT_GREEN);

  if (clock_.showDate()) {
    char dateBuf[16];
    clock_.formatLongDate(now, dateBuf, sizeof(dateBuf));
    drawCentered(tft, dateBuf, 96, 2, TFT_LIGHTGREY);
  }
  char footer[42];
  if (hasSync_) {
    char status[14];
    syncLabel(status, sizeof(status));
    snprintf(footer, sizeof(footer), "%s  B1 next  B2 prev", status);
  } else {
    snprintf(footer, sizeof(footer), "B1 next  B2 prev  options face");
  }
  TDisplayUi::footer(tft, footer);
}

template <typename Canvas>
void ClockApp::drawAnalog(Canvas& tft, time_t now) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Analog Clock", TFT_ORANGE, clock_.showOffset() ? clock_.zoneLabel() : nullptr);
  const ClockLogic::TimeParts parts = clock_.localParts(now);
  const int cx = 120;
  const int cy = 73;
  const int radius = 39;
  const float secondAngle = (parts.second * 6.0f - 90.0f) * DEG_TO_RAD;
  const float minuteAngle = (parts.minute * 6.0f - 90.0f) * DEG_TO_RAD;
  const float hourAngle = (((parts.hour % 12) + parts.minute / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;

  tft.drawCircle(cx, cy, radius, TFT_WHITE);
  tft.drawCircle(cx, cy, radius - 1, TFT_DARKGREY);
  for (uint8_t i = 0; i < 12; i++) {
    const float tick = (i * 30.0f - 90.0f) * DEG_TO_RAD;
    tft.drawLine(handX(cx, tick, radius - 5), handY(cy, tick, radius - 5),
                 handX(cx, tick, radius - 1), handY(cy, tick, radius - 1), TFT_LIGHTGREY);
  }
  tft.drawLine(cx, cy, handX(cx, hourAngle, 20), handY(cy, hourAngle, 20), TFT_ORANGE);
  tft.drawLine(cx, cy, handX(cx, minuteAngle, 31), handY(cy, minuteAngle, 31), TFT_CYAN);
  tft.drawLine(cx, cy, handX(cx, secondAngle, 34), handY(cy, secondAngle, 34), TFT_RED);
  tft.fillCircle(cx, cy, 3, TFT_WHITE);
  char footer[36];
  if (hasSync_) {
    char status[14];
    syncLabel(status, sizeof(status));
    snprintf(footer, sizeof(footer), "%s  B1 next  B2 prev", status);
  } else {
    snprintf(footer, sizeof(footer), "B1 next  B2 prev");
  }
  TDisplayUi::footer(tft, footer);
}

template <typename Canvas>
void ClockApp::drawUnix(Canvas& tft, time_t now) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Unix Time", TFT_MAGENTA, "UTC");
  char unixBuf[16];
  snprintf(unixBuf, sizeof(unixBuf), "%lu", static_cast<unsigned long>(now));
  TDisplayUi::largeValue(tft, unixBuf, 55, TFT_MAGENTA);
  char footer[36];
  if (hasSync_) {
    char status[14];
    syncLabel(status, sizeof(status));
    snprintf(footer, sizeof(footer), "%s  UTC", status);
  } else {
    snprintf(footer, sizeof(footer), "Always UTC  B1 next  B2 prev");
  }
  TDisplayUi::footer(tft, footer);
}

template <typename Canvas>
void ClockApp::drawOptionsFace(Canvas& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Clock", TFT_YELLOW);
  drawCentered(tft, "Options", 50, 3, TFT_YELLOW);
  drawCentered(tft, "hold B1 to open", 88, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 next/open  B2 prev");
}

template <typename Canvas>
void ClockApp::drawOptions(Canvas& tft) {
  TDisplayUi::clear(tft);
  char zonePos[8];
  const bool zoneOption = clock_.option() == ClockLogic::Option::NextZone ||
                          clock_.option() == ClockLogic::Option::Timezone ||
                          clock_.option() == ClockLogic::Option::AddZone ||
                          clock_.option() == ClockLogic::Option::DeleteZone;
  if (zoneOption) {
    clock_.formatZonePosition(zonePos, sizeof(zonePos));
  }
  TDisplayUi::header(tft, "Clock Options", TFT_YELLOW, zoneOption ? zonePos : clock_.optionValue());
  TDisplayUi::row(tft, 49, clock_.optionLabel(), true, TFT_YELLOW);
  drawCentered(tft, clock_.optionValue(), 86, 2, TFT_WHITE);
  TDisplayUi::footer(tft, "B1 next/hold select  B2 prev");
}

template <typename Canvas>
void ClockApp::drawTimezone(Canvas& tft) {
  TDisplayUi::clear(tft);
  char zonePos[8];
  clock_.formatZonePosition(zonePos, sizeof(zonePos));
  TDisplayUi::header(tft, "Timezone", TFT_CYAN, zonePos);
  drawCentered(tft, clock_.pickerZoneLabel(), 54, 3, TFT_CYAN);
  TDisplayUi::footer(tft, "B1 next  B2 prev  B1 hold done");
}

void ClockApp::drawClockFooter(TFT_eSPI& tft, bool utc) {
  char footer[42];
  if (hasSync_) {
    char status[14];
    syncLabel(status, sizeof(status));
    snprintf(footer, sizeof(footer), utc ? "%s  UTC" : "%s  B1 next  B2 prev", status);
  } else if (utc) {
    snprintf(footer, sizeof(footer), "Always UTC  B1 next  B2 prev");
  } else if (clock_.face() == ClockLogic::Face::Digital) {
    snprintf(footer, sizeof(footer), "B1 next  B2 prev  options face");
  } else {
    snprintf(footer, sizeof(footer), "B1 next  B2 prev");
  }
  TDisplayUi::footer(tft, footer);
}

void ClockApp::drawDigitalTick(TFT_eSPI& tft, time_t now) {
  tft.fillRect(0, TDisplayUi::HEADER_H, TDisplayUi::W,
               TDisplayUi::H - TDisplayUi::HEADER_H - TDisplayUi::FOOTER_H, TFT_BLACK);

  char timeBuf[16];
  clock_.formatTime(now, timeBuf, sizeof(timeBuf), true);
  TDisplayUi::largeValue(tft, timeBuf, clock_.showDate() ? 51 : 58, TFT_GREEN);

  if (clock_.showDate()) {
    char dateBuf[16];
    clock_.formatLongDate(now, dateBuf, sizeof(dateBuf));
    drawCentered(tft, dateBuf, 96, 2, TFT_LIGHTGREY);
  }
  drawClockFooter(tft);
}

void ClockApp::drawAnalogTick(TFT_eSPI& tft, time_t now) {
  tft.fillRect(0, TDisplayUi::HEADER_H, TDisplayUi::W,
               TDisplayUi::H - TDisplayUi::HEADER_H - TDisplayUi::FOOTER_H, TFT_BLACK);

  const ClockLogic::TimeParts parts = clock_.localParts(now);
  const int cx = 120;
  const int cy = 73;
  const int radius = 39;
  const float secondAngle = (parts.second * 6.0f - 90.0f) * DEG_TO_RAD;
  const float minuteAngle = (parts.minute * 6.0f - 90.0f) * DEG_TO_RAD;
  const float hourAngle = (((parts.hour % 12) + parts.minute / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;

  tft.drawCircle(cx, cy, radius, TFT_WHITE);
  tft.drawCircle(cx, cy, radius - 1, TFT_DARKGREY);
  for (uint8_t i = 0; i < 12; i++) {
    const float tick = (i * 30.0f - 90.0f) * DEG_TO_RAD;
    tft.drawLine(handX(cx, tick, radius - 5), handY(cy, tick, radius - 5),
                 handX(cx, tick, radius - 1), handY(cy, tick, radius - 1), TFT_LIGHTGREY);
  }
  tft.drawLine(cx, cy, handX(cx, hourAngle, 20), handY(cy, hourAngle, 20), TFT_ORANGE);
  tft.drawLine(cx, cy, handX(cx, minuteAngle, 31), handY(cy, minuteAngle, 31), TFT_CYAN);
  tft.drawLine(cx, cy, handX(cx, secondAngle, 34), handY(cy, secondAngle, 34), TFT_RED);
  tft.fillCircle(cx, cy, 3, TFT_WHITE);
  drawClockFooter(tft);
}

void ClockApp::drawUnixTick(TFT_eSPI& tft, time_t now) {
  tft.fillRect(0, TDisplayUi::HEADER_H, TDisplayUi::W,
               TDisplayUi::H - TDisplayUi::HEADER_H - TDisplayUi::FOOTER_H, TFT_BLACK);

  char unixBuf[16];
  snprintf(unixBuf, sizeof(unixBuf), "%lu", static_cast<unsigned long>(now));
  TDisplayUi::largeValue(tft, unixBuf, 55, TFT_MAGENTA);
  drawClockFooter(tft, true);
}

void ClockApp::drawSecondTick(TFT_eSPI& tft, time_t now) {
  if (mode_ != Mode::Clock || clock_.view() != ClockLogic::View::Face) {
    return;
  }
  switch (clock_.face()) {
    case ClockLogic::Face::Digital:
      drawDigitalTick(tft, now);
      break;
    case ClockLogic::Face::Analog:
      drawAnalogTick(tft, now);
      break;
    case ClockLogic::Face::UnixTime:
      drawUnixTick(tft, now);
      break;
    case ClockLogic::Face::Options:
    default:
      break;
  }
}

template <typename Canvas>
void ClockApp::drawClockFrame(Canvas& tft, time_t now) {
  if (mode_ != Mode::Clock) {
    drawNetworkStatus(tft);
  } else if (clock_.view() == ClockLogic::View::Options) {
    drawOptions(tft);
  } else if (clock_.view() == ClockLogic::View::Timezone) {
    drawTimezone(tft);
  } else {
    switch (clock_.face()) {
      case ClockLogic::Face::Digital:
        drawDigital(tft, now);
        break;
      case ClockLogic::Face::Analog:
        drawAnalog(tft, now);
        break;
      case ClockLogic::Face::UnixTime:
        drawUnix(tft, now);
        break;
      case ClockLogic::Face::Options:
      default:
        drawOptionsFace(tft);
        break;
    }
  }
}

void ClockApp::drawRunning(TFT_eSPI& tft) {
  const time_t now = currentTime();
  if (!shouldRedraw(now)) {
    return;
  }

  TDisplayFramebuffer::draw(tft, TDisplayUi::W, TDisplayUi::H,
                            [&](auto& canvas) { drawClockFrame(canvas, now); });

  noteRendered(now);
}
