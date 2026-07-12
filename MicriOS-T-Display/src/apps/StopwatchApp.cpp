#include "StopwatchApp.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include <TFT_eSPI.h>

StopwatchApp::StopwatchApp(uint32_t width, uint32_t height)
    : App("Stopwatch", width, height) {}

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
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "Stopwatch", running ? TFT_GREEN : TFT_YELLOW, running ? "RUN" : "STOP");
    TDisplayUi::footer(canvas, "B1 start/stop / B1 hold or B2 reset");
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

  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "Stopwatch", running ? TFT_GREEN : TFT_YELLOW, running ? "RUN" : "STOP");
    TDisplayUi::footer(canvas, "B1 start/stop / B1 hold or B2 reset");
    TDisplayUi::largeValue(canvas, buf, 53, running ? TFT_GREEN : TFT_YELLOW);
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
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Stopwatch", TFT_GREEN);
  TDisplayUi::centered(tft, "00:00:00", 50, 3, TFT_GREEN);
  TDisplayUi::centered(tft, "Hundredths shown live", 88, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 start");
}

bool StopwatchApp::hasCustomOverlay() const { return true; }
bool StopwatchApp::startsRunningImmediately() const { return true; }
