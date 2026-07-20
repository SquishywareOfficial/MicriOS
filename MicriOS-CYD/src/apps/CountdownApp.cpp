#include "CountdownApp.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include <TFT_eSPI.h>

CountdownApp::CountdownApp(uint32_t width, uint32_t height)
    : App("Countdown", width, height) {}

namespace {
constexpr TouchUi::Rect COUNTDOWN_CONTROLS[] = {
    {14, 151, 62, 43}, {82, 151, 62, 43},
    {150, 151, 88, 43}, {244, 151, 62, 43}};
enum CountdownControl : int16_t { MinusTen = 0, PlusTen = 1, StartPause = 2,
                                  Reset = 3 };
}

bool CountdownApp::handleTouch(const TouchUi::TouchSample& sample,
                               const TouchUi::TouchEvent& event) {
  const int16_t hit = TouchUi::hitRectList(
      COUNTDOWN_CONTROLS, 4, TouchUi::currentPoint(sample, event));
  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) uiInitialized_ = false;
  if (!result.activated) return result.consumed;

  const uint32_t now = millis();
  const bool finished = logic_.getRemainingMs(now) == 0;
  if (result.id == MinusTen && !logic_.isRunning()) {
    logic_.adjustDuration(-10000);
  } else if (result.id == PlusTen && !logic_.isRunning()) {
    logic_.adjustDuration(10000);
  } else if (result.id == StartPause) {
    if (finished) logic_.reset();
    logic_.toggle(now);
  } else if (result.id == Reset) {
    logic_.reset();
  }
  uiInitialized_ = false;
  return true;
}

void CountdownApp::onAppReset() {
  logic_.reset();
  uiInitialized_ = false;
  lastRunning_ = false;
  alarmActive_ = false;
  lastAlarmFlash_ = false;
  lastDurationMs_ = 0;
  lastRenderedSecond_ = UINT32_MAX;
  touchCapture_.reset();
}

void CountdownApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  if (b1.click) {
      if (logic_.isRunning()) logic_.toggle(millis());
      else logic_.adjustDuration(10000); // Add 10s if not running
      uiInitialized_ = false;
  }
  if (b2.click && !logic_.isRunning()) {
      logic_.adjustDuration(-10000);
      uiInitialized_ = false;
  }
  if (b1.longPress) {
      if (logic_.isRunning()) logic_.reset();
      else logic_.toggle(millis());
      uiInitialized_ = false;
  }
}

void CountdownApp::drawCountdownFrame(TFT_eSPI& tft, uint32_t remaining, bool running, uint16_t stateColor) {
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "Countdown", stateColor, running ? "RUN" : "SET");

    uint32_t s = (remaining / 1000) % 60;
    uint32_t m = (remaining / 60000) % 60;
    uint32_t h = (remaining / 3600000);

    char buf[32];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    CydUi::largeValue(canvas, buf, 61, stateColor);
    const float ratio = logic_.getDurationMs() == 0 ? 0.0f : static_cast<float>(remaining) / static_cast<float>(logic_.getDurationMs());
    CydUi::bar(canvas, 22, 128, 276, 12, ratio, stateColor);
    CydUi::touchAction(canvas, COUNTDOWN_CONTROLS[MinusTen], "-10s",
                       running ? TFT_DARKGREY : TFT_CYAN, !running,
                       touchCapture_.activeId() == MinusTen);
    CydUi::touchAction(canvas, COUNTDOWN_CONTROLS[PlusTen], "+10s",
                       running ? TFT_DARKGREY : TFT_CYAN, !running,
                       touchCapture_.activeId() == PlusTen);
    CydUi::touchAction(canvas, COUNTDOWN_CONTROLS[StartPause],
                       running ? "Pause" : "Start",
                       running ? TFT_ORANGE : TFT_GREEN, true,
                       touchCapture_.activeId() == StartPause);
    CydUi::touchAction(canvas, COUNTDOWN_CONTROLS[Reset], "Reset",
                       TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == Reset);
  });
}

void CountdownApp::drawFinishedAlert(TFT_eSPI& tft, bool flashOn) {
  const uint16_t bg = flashOn ? TFT_RED : TFT_BLACK;
  const uint16_t fg = flashOn ? TFT_WHITE : TFT_RED;
  const uint16_t sub = flashOn ? TFT_WHITE : TFT_LIGHTGREY;

  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    canvas.fillRect(0, 0, width, height, bg);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(sub, bg);
    canvas.setTextSize(2);
    canvas.drawString("Countdown", 10, 8);
    canvas.drawFastHLine(0, 29, width, flashOn ? TFT_WHITE : TFT_DARKGREY);

    canvas.setTextColor(fg, bg);
    canvas.setTextSize(4);
    String label = "TIME UP";
    canvas.drawString(label, (width - canvas.textWidth(label)) / 2, 57);

    canvas.setTextSize(2);
    String zero = "00:00:00";
    canvas.drawString(zero, (width - canvas.textWidth(zero)) / 2, 111);

    CydUi::touchAction(canvas, COUNTDOWN_CONTROLS[StartPause], "Restart",
                       TFT_GREEN, true,
                       touchCapture_.activeId() == StartPause);
    CydUi::touchAction(canvas, COUNTDOWN_CONTROLS[Reset], "Reset",
                       TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == Reset);
  });
}

void CountdownApp::drawRunning(TFT_eSPI& tft) {
  const uint32_t remaining = logic_.getRemainingMs(millis());
  const bool running = logic_.isRunning();
  const uint16_t stateColor = remaining == 0 ? TFT_RED : (running ? TFT_GREEN : TFT_YELLOW);

  if (remaining == 0) {
    const bool flashOn = ((millis() / 250) % 2) == 0;
    if (!alarmActive_ || flashOn != lastAlarmFlash_) {
      drawFinishedAlert(tft, flashOn);
      alarmActive_ = true;
      lastAlarmFlash_ = flashOn;
    }
    return;
  }

  if (alarmActive_) {
    uiInitialized_ = false;
    alarmActive_ = false;
    lastRenderedSecond_ = UINT32_MAX;
  }

  const uint32_t secondTick = (remaining + 999) / 1000;
  if (!uiInitialized_ || running != lastRunning_ || logic_.getDurationMs() != lastDurationMs_ ||
      secondTick != lastRenderedSecond_) {
    drawCountdownFrame(tft, remaining, running, stateColor);
    uiInitialized_ = true;
    lastRunning_ = running;
    alarmActive_ = false;
    lastDurationMs_ = logic_.getDurationMs();
    lastRenderedSecond_ = secondTick;
  }
}

void CountdownApp::drawStart(TFT_eSPI& tft) {
  CydUi::clear(tft);
  CydUi::header(tft, "Countdown", TFT_RED);
  CydUi::centered(tft, "Set time", 66, 4, TFT_YELLOW);
  CydUi::centered(tft, "Then hold to start", 126, 2, TFT_LIGHTGREY);
  CydUi::footer(tft, "Set a duration to begin");
}

bool CountdownApp::hasCustomOverlay() const { return true; }
bool CountdownApp::startsRunningImmediately() const { return true; }
