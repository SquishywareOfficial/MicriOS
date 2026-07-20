#include "StopwatchApp.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include <TFT_eSPI.h>

StopwatchApp::StopwatchApp(uint32_t width, uint32_t height)
    : App("Stopwatch", width, height) {}

namespace {
constexpr TouchUi::Rect START_STOP = {18, 153, 184, 43};
constexpr TouchUi::Rect RESET = {212, 153, 90, 43};
}

bool StopwatchApp::handleTouch(const TouchUi::TouchSample& sample,
                               const TouchUi::TouchEvent& event) {
  const bool overStart = START_STOP.contains(sample.down ? sample.point
                                                          : event.point);
  const bool overReset = RESET.contains(sample.down ? sample.point
                                                     : event.point);
  if (event.tap && overStart) {
    logic_.toggle(millis());
    uiInitialized_ = false;
  } else if (event.tap && overReset) {
    logic_.reset();
    uiInitialized_ = false;
  }
  return overStart || overReset;
}

void StopwatchApp::onAppReset() {
  logic_.reset();
  uiInitialized_ = false;
  lastRunning_ = false;
  lastRenderedTick_ = UINT32_MAX;
}

void StopwatchApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  if (b1.click) {
    logic_.toggle(millis());
    uiInitialized_ = false;
  }
  if (b1.longPress || b2.click) {
    logic_.reset();
    uiInitialized_ = false;
  }
}

void StopwatchApp::drawStatic(TFT_eSPI& tft, bool running) {
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "Stopwatch", running ? TFT_GREEN : TFT_YELLOW, running ? "RUN" : "STOP");
    CydUi::touchAction(canvas, START_STOP, running ? "Pause" : "Start",
                       running ? TFT_ORANGE : TFT_GREEN);
    CydUi::touchAction(canvas, RESET, "Reset", TFT_LIGHTGREY, false);
  });
  uiInitialized_ = true;
  lastRunning_ = running;
  lastRenderedTick_ = UINT32_MAX;
}

void StopwatchApp::drawElapsed(TFT_eSPI& tft, uint32_t elapsed, bool running) {
  uint32_t ms = elapsed % 1000;
  uint32_t s = (elapsed / 1000) % 60;
  uint32_t m = (elapsed / 60000) % 60;
  uint32_t h = (elapsed / 3600000);

  char buf[32];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%03u", (unsigned)h, (unsigned)m, (unsigned)s, (unsigned)ms);

  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "Stopwatch", running ? TFT_GREEN : TFT_YELLOW, running ? "RUN" : "STOP");
    CydUi::largeValue(canvas, buf, 72, running ? TFT_GREEN : TFT_YELLOW);
    CydUi::touchAction(canvas, START_STOP, running ? "Pause" : "Start",
                       running ? TFT_ORANGE : TFT_GREEN);
    CydUi::touchAction(canvas, RESET, "Reset", TFT_LIGHTGREY, false);
  });
}

void StopwatchApp::drawRunning(TFT_eSPI& tft) {
  const bool running = logic_.isRunning();
  if (!uiInitialized_ || running != lastRunning_) {
    drawStatic(tft, running);
  }

  const uint32_t elapsed = logic_.getElapsedMs(millis());
  const uint32_t renderTick = running ? elapsed / 50 : elapsed;
  if (renderTick != lastRenderedTick_) {
    drawElapsed(tft, elapsed, running);
    lastRenderedTick_ = renderTick;
  }
}

void StopwatchApp::drawStart(TFT_eSPI& tft) {
  CydUi::clear(tft);
  CydUi::header(tft, "Stopwatch", TFT_GREEN);
  CydUi::centered(tft, "00:00:00", 69, 4, TFT_GREEN);
  CydUi::centered(tft, "Hundredths shown live", 128, 2, TFT_LIGHTGREY);
  CydUi::footer(tft, "Tap Start");
}

bool StopwatchApp::hasCustomOverlay() const { return true; }
bool StopwatchApp::startsRunningImmediately() const { return true; }
