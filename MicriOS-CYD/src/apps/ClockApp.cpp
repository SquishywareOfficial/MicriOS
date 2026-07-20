#include "ClockApp.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

#include "../../CydFramebuffer.h"
#include "../../CydUi.h"

namespace {
constexpr uint32_t SPLASH_MS = 900;
constexpr uint32_t CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t CONNECTED_MS = 800;
constexpr uint32_t SYNC_TIMEOUT_MS = 12000;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;
constexpr const char* HOSTNAME = "MicriDeck-Cyd";
constexpr uint8_t CLOCK_ROWS = 4;
constexpr uint8_t OPTION_PAGE_COUNT =
    (ClockLogic::OPTION_COUNT + CLOCK_ROWS - 1) / CLOCK_ROWS;
constexpr uint8_t ZONE_PAGE_COUNT =
    (ClockLogic::ZONE_COUNT + CLOCK_ROWS - 1) / CLOCK_ROWS;
constexpr TouchUi::Rect NETWORK_CANCEL = {20, 151, 132, 43};
constexpr TouchUi::Rect NETWORK_RETRY = {168, 151, 132, 43};
constexpr TouchUi::Rect PAGE_PREV = {12, 174, 58, 28};
constexpr TouchUi::Rect PAGE_NEXT = {76, 174, 58, 28};
constexpr TouchUi::Rect PAGE_DONE = {174, 168, 134, 34};

TouchUi::Rect faceTab(uint8_t index) {
  return {static_cast<int16_t>(7 + index * 78), 176, 72, 28};
}

TouchUi::Rect clockRow(uint8_t row) {
  return {12, static_cast<int16_t>(42 + row * 32), 296, 29};
}

const char* faceLabel(uint8_t index) {
  static const char* LABELS[] = {"Digital", "Analog", "Unix", "Settings"};
  return index < 4 ? LABELS[index] : "";
}

template <typename Canvas>
void drawFaceTabs(Canvas& canvas, ClockLogic::Face face, int16_t pressed) {
  for (uint8_t i = 0; i < ClockLogic::FACE_COUNT; ++i) {
    CydUi::touchTab(canvas, faceTab(i), faceLabel(i),
                    static_cast<uint8_t>(face) == i,
                    i == 3 ? TFT_YELLOW : TFT_CYAN, pressed == i);
  }
}

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
  tft.drawString(text, (CydUi::W - tft.textWidth(text)) / 2, y);
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

bool ClockApp::handleTouch(const TouchUi::TouchSample& sample,
                           const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (mode_ != Mode::Clock) {
    if (NETWORK_CANCEL.contains(point)) hit = 0;
    if ((mode_ == Mode::ConnectFailed || mode_ == Mode::SyncFailed) &&
        NETWORK_RETRY.contains(point)) hit = 1;
  } else if (clock_.view() == ClockLogic::View::Face) {
    for (uint8_t i = 0; i < ClockLogic::FACE_COUNT; ++i) {
      if (faceTab(i).contains(point)) hit = i;
    }
  } else if (clock_.view() == ClockLogic::View::Options) {
    for (uint8_t row = 0; row < CLOCK_ROWS; ++row) {
      const uint8_t index = optionPage_ * CLOCK_ROWS + row;
      if (index < ClockLogic::OPTION_COUNT && clockRow(row).contains(point)) {
        hit = row;
      }
    }
    if (PAGE_PREV.contains(point)) hit = 4;
    if (PAGE_NEXT.contains(point)) hit = 5;
  } else {
    for (uint8_t row = 0; row < CLOCK_ROWS; ++row) {
      const uint8_t index = zonePage_ * CLOCK_ROWS + row;
      if (index < ClockLogic::ZONE_COUNT && clockRow(row).contains(point)) {
        hit = row;
      }
    }
    if (PAGE_PREV.contains(point)) hit = 4;
    if (PAGE_NEXT.contains(point)) hit = 5;
    if (PAGE_DONE.contains(point)) hit = 6;
  }

  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) markDirty();
  if (!result.activated) return result.consumed;

  if (mode_ != Mode::Clock) {
    if (result.id == 0) {
      stopWifi();
      requestExitToMenu();
    } else if (mode_ == Mode::SyncFailed && WiFi.status() == WL_CONNECTED) {
      beginSync();
    } else {
      beginRetry();
    }
  } else if (clock_.view() == ClockLogic::View::Face) {
    clock_.setFace(static_cast<ClockLogic::Face>(result.id));
    if (result.id == static_cast<int16_t>(ClockLogic::Face::Options)) {
      clock_.selectFace();
      optionPage_ = 0;
    }
  } else if (clock_.view() == ClockLogic::View::Options) {
    if (result.id < CLOCK_ROWS) {
      const uint8_t index = optionPage_ * CLOCK_ROWS + result.id;
      if (index < ClockLogic::OPTION_COUNT) {
        clock_.setOption(static_cast<ClockLogic::Option>(index));
        handleClockAction(clock_.activateOption());
        if (clock_.view() == ClockLogic::View::Timezone) {
          zonePage_ = clock_.pickerZoneIndex() / CLOCK_ROWS;
        }
      }
    } else if (result.id == 4) {
      optionPage_ = optionPage_ == 0 ? OPTION_PAGE_COUNT - 1 : optionPage_ - 1;
    } else if (result.id == 5) {
      optionPage_ = (optionPage_ + 1) % OPTION_PAGE_COUNT;
    }
  } else {
    if (result.id < CLOCK_ROWS) {
      const uint8_t index = zonePage_ * CLOCK_ROWS + result.id;
      if (index < ClockLogic::ZONE_COUNT) clock_.setPickerZone(index);
    } else if (result.id == 4) {
      zonePage_ = zonePage_ == 0 ? ZONE_PAGE_COUNT - 1 : zonePage_ - 1;
    } else if (result.id == 5) {
      zonePage_ = (zonePage_ + 1) % ZONE_PAGE_COUNT;
    } else if (result.id == 6) {
      clock_.closeTimezone();
    }
  }
  touchCapture_.reset();
  markDirty();
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
  optionPage_ = 0;
  zonePage_ = 0;
  touchCapture_.reset();
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
  CydUi::clear(tft);

  if (mode_ == Mode::Splash) {
    CydUi::header(tft, "Micri Clock", TFT_CYAN);
    drawCentered(tft, "Testing", 48, 3, TFT_WHITE);
    drawCentered(tft, "saved WiFi", 82, 2, TFT_LIGHTGREY);
    CydUi::touchAction(tft, NETWORK_CANCEL, "Cancel", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 0);
    return;
  }

  if (mode_ == Mode::NoProfiles) {
    CydUi::header(tft, "Micri Clock", TFT_RED, "NO WIFI");
    drawCentered(tft, "No WiFi Saved", 46, 2, TFT_RED);
    drawCentered(tft, hasUsableTime() ? "Offline Clock..." : "Use WiFi Settings", 78, 2, TFT_WHITE);
    CydUi::touchAction(tft, NETWORK_CANCEL, "Exit", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 0);
    return;
  }

  if (mode_ == Mode::Connecting) {
    CydUi::header(tft, "Trying Network", TFT_YELLOW);
    drawCentered(tft, CydUi::fitText(tft, currentSsid_, 210), 55, 2, TFT_WHITE);
    CydUi::touchAction(tft, NETWORK_CANCEL, "Cancel", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 0);
    return;
  }

  if (mode_ == Mode::Connected) {
    CydUi::header(tft, "Micri Clock", TFT_GREEN, "OK");
    drawCentered(tft, "CONNECTED", 48, 3, TFT_GREEN);
    drawCentered(tft, CydUi::fitText(tft, currentSsid_, 210), 88, 1, TFT_LIGHTGREY);
    CydUi::touchAction(tft, NETWORK_CANCEL, "Cancel", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 0);
    return;
  }

  if (mode_ == Mode::Syncing) {
    CydUi::header(tft, "Micri Clock", TFT_CYAN);
    drawCentered(tft, "Syncing", 48, 3, TFT_CYAN);
    drawCentered(tft, "NTP time", 84, 2, TFT_WHITE);
    CydUi::touchAction(tft, NETWORK_CANCEL, "Cancel", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 0);
    return;
  }

  if (mode_ == Mode::ConnectFailed) {
    CydUi::header(tft, "Micri Clock", TFT_RED, "FAIL");
    drawCentered(tft, "Connect Failed", 48, 2, TFT_RED);
    drawCentered(tft, hasUsableTime() ? "Offline Clock..." : "Check WiFi Settings", 78, 1, TFT_WHITE);
    CydUi::touchAction(tft, NETWORK_CANCEL, "Exit", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 0);
    CydUi::touchAction(tft, NETWORK_RETRY, "Retry", TFT_GREEN, true,
                       touchCapture_.activeId() == 1);
    return;
  }

  CydUi::header(tft, "Micri Clock", TFT_RED, "NTP");
  drawCentered(tft, "Time Sync Failed", 48, 2, TFT_RED);
  drawCentered(tft, hasUsableTime() ? "Offline Clock..." : "Internet may be down", 78, 1, TFT_WHITE);
  CydUi::touchAction(tft, NETWORK_CANCEL, "Exit", TFT_LIGHTGREY, false,
                     touchCapture_.activeId() == 0);
  CydUi::touchAction(tft, NETWORK_RETRY, "Retry", TFT_GREEN, true,
                     touchCapture_.activeId() == 1);
}

template <typename Canvas>
void ClockApp::drawDigital(Canvas& tft, time_t now) {
  CydUi::clear(tft);
  CydUi::header(tft, "Digital Clock", TFT_CYAN, clock_.showOffset() ? clock_.zoneLabel() : nullptr);

  char timeBuf[16];
  clock_.formatTime(now, timeBuf, sizeof(timeBuf), true);
  CydUi::largeValue(tft, timeBuf, clock_.showDate() ? 67 : 79, TFT_GREEN);

  if (clock_.showDate()) {
    char dateBuf[16];
    clock_.formatLongDate(now, dateBuf, sizeof(dateBuf));
    drawCentered(tft, dateBuf, 139, 2, TFT_LIGHTGREY);
  }
  drawFaceTabs(tft, clock_.face(), touchCapture_.activeId());
}

template <typename Canvas>
void ClockApp::drawAnalog(Canvas& tft, time_t now) {
  CydUi::clear(tft);
  CydUi::header(tft, "Analog Clock", TFT_ORANGE, clock_.showOffset() ? clock_.zoneLabel() : nullptr);
  const ClockLogic::TimeParts parts = clock_.localParts(now);
  const int cx = 160;
  const int cy = 110;
  const int radius = 66;
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
  tft.drawLine(cx, cy, handX(cx, hourAngle, 36), handY(cy, hourAngle, 36), TFT_ORANGE);
  tft.drawLine(cx, cy, handX(cx, minuteAngle, 53), handY(cy, minuteAngle, 53), TFT_CYAN);
  tft.drawLine(cx, cy, handX(cx, secondAngle, 59), handY(cy, secondAngle, 59), TFT_RED);
  tft.fillCircle(cx, cy, 5, TFT_WHITE);
  drawFaceTabs(tft, clock_.face(), touchCapture_.activeId());
}

template <typename Canvas>
void ClockApp::drawUnix(Canvas& tft, time_t now) {
  CydUi::clear(tft);
  CydUi::header(tft, "Unix Time", TFT_MAGENTA, "UTC");
  char unixBuf[16];
  snprintf(unixBuf, sizeof(unixBuf), "%lu", static_cast<unsigned long>(now));
  CydUi::largeValue(tft, unixBuf, 79, TFT_MAGENTA);
  drawFaceTabs(tft, clock_.face(), touchCapture_.activeId());
}

template <typename Canvas>
void ClockApp::drawOptionsFace(Canvas& tft) {
  CydUi::clear(tft);
  CydUi::header(tft, "Clock", TFT_YELLOW);
  drawCentered(tft, "Options", 69, 4, TFT_YELLOW);
  drawCentered(tft, "Tap Settings", 132, 2, TFT_LIGHTGREY);
  drawFaceTabs(tft, clock_.face(), touchCapture_.activeId());
}

template <typename Canvas>
void ClockApp::drawOptions(Canvas& tft) {
  CydUi::clear(tft);
  String page = String(optionPage_ + 1) + "/" + String(OPTION_PAGE_COUNT);
  CydUi::header(tft, "Clock Options", TFT_YELLOW, page.c_str());
  const ClockLogic::Option currentOption = clock_.option();
  for (uint8_t row = 0; row < CLOCK_ROWS; ++row) {
    const uint8_t index = optionPage_ * CLOCK_ROWS + row;
    if (index >= ClockLogic::OPTION_COUNT) break;
    clock_.setOption(static_cast<ClockLogic::Option>(index));
    CydUi::touchListRow(tft, clockRow(row), clock_.optionLabel(),
                        currentOption == static_cast<ClockLogic::Option>(index),
                        TFT_YELLOW, clock_.optionValue(),
                        touchCapture_.activeId() == row);
  }
  clock_.setOption(currentOption);
  CydUi::touchAction(tft, PAGE_PREV, "Prev", TFT_BLUE, false,
                     touchCapture_.activeId() == 4);
  CydUi::touchAction(tft, PAGE_NEXT, "Next", TFT_BLUE, false,
                     touchCapture_.activeId() == 5);
}

template <typename Canvas>
void ClockApp::drawTimezone(Canvas& tft) {
  CydUi::clear(tft);
  String page = String(zonePage_ + 1) + "/" + String(ZONE_PAGE_COUNT);
  CydUi::header(tft, "Timezone", TFT_CYAN, page.c_str());
  for (uint8_t row = 0; row < CLOCK_ROWS; ++row) {
    const uint8_t index = zonePage_ * CLOCK_ROWS + row;
    if (index >= ClockLogic::ZONE_COUNT) break;
    CydUi::touchListRow(tft, clockRow(row), CLOCK_ZONES[index].label,
                        clock_.pickerZoneIndex() == index, TFT_CYAN,
                        clock_.pickerZoneIndex() == index ? "Selected" : nullptr,
                        touchCapture_.activeId() == row);
  }
  CydUi::touchAction(tft, PAGE_PREV, "<", TFT_BLUE, false,
                     touchCapture_.activeId() == 4);
  CydUi::touchAction(tft, PAGE_NEXT, ">", TFT_BLUE, false,
                     touchCapture_.activeId() == 5);
  CydUi::touchAction(tft, PAGE_DONE, "Done", TFT_GREEN, true,
                     touchCapture_.activeId() == 6);
}

void ClockApp::drawClockFooter(TFT_eSPI& tft, bool utc) {
  (void)utc;
  drawFaceTabs(tft, clock_.face(), touchCapture_.activeId());
}

void ClockApp::drawDigitalTick(TFT_eSPI& tft, time_t now) {
  tft.fillRect(0, CydUi::HEADER_H, CydUi::W,
               CydUi::H - CydUi::HEADER_H - CydUi::FOOTER_H, TFT_BLACK);

  char timeBuf[16];
  clock_.formatTime(now, timeBuf, sizeof(timeBuf), true);
  CydUi::largeValue(tft, timeBuf, clock_.showDate() ? 67 : 79, TFT_GREEN);

  if (clock_.showDate()) {
    char dateBuf[16];
    clock_.formatLongDate(now, dateBuf, sizeof(dateBuf));
    drawCentered(tft, dateBuf, 139, 2, TFT_LIGHTGREY);
  }
  drawClockFooter(tft);
}

void ClockApp::drawAnalogTick(TFT_eSPI& tft, time_t now) {
  tft.fillRect(0, CydUi::HEADER_H, CydUi::W,
               CydUi::H - CydUi::HEADER_H - CydUi::FOOTER_H, TFT_BLACK);

  const ClockLogic::TimeParts parts = clock_.localParts(now);
  const int cx = 160;
  const int cy = 110;
  const int radius = 66;
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
  tft.drawLine(cx, cy, handX(cx, hourAngle, 36), handY(cy, hourAngle, 36), TFT_ORANGE);
  tft.drawLine(cx, cy, handX(cx, minuteAngle, 53), handY(cy, minuteAngle, 53), TFT_CYAN);
  tft.drawLine(cx, cy, handX(cx, secondAngle, 59), handY(cy, secondAngle, 59), TFT_RED);
  tft.fillCircle(cx, cy, 5, TFT_WHITE);
  drawClockFooter(tft);
}

void ClockApp::drawUnixTick(TFT_eSPI& tft, time_t now) {
  tft.fillRect(0, CydUi::HEADER_H, CydUi::W,
               CydUi::H - CydUi::HEADER_H - CydUi::FOOTER_H, TFT_BLACK);

  char unixBuf[16];
  snprintf(unixBuf, sizeof(unixBuf), "%lu", static_cast<unsigned long>(now));
  CydUi::largeValue(tft, unixBuf, 79, TFT_MAGENTA);
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

  CydFramebuffer::draw(tft, CydUi::W, CydUi::H,
                            [&](auto& canvas) { drawClockFrame(canvas, now); });

  noteRendered(now);
}
