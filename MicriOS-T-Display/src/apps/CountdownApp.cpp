#include "CountdownApp.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include <TFT_eSPI.h>

CountdownApp::CountdownApp(uint32_t width, uint32_t height)
    : App("Countdown", width, height) {}

void CountdownApp::onAppReset() {
  logic_.reset();
  uiInitialized_ = false;
  lastRunning_ = false;
  alarmActive_ = false;
  lastAlarmFlash_ = false;
  lastDurationMs_ = 0;
  lastRenderedSecond_ = UINT32_MAX;
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
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "Countdown", stateColor, running ? "RUN" : "SET");
    if (running) TDisplayUi::footer(canvas, "B1 stop / B1 hold reset");
    else TDisplayUi::footer(canvas, "B1 +10s / B2 -10s / B1 hold start");

    uint32_t s = (remaining / 1000) % 60;
    uint32_t m = (remaining / 60000) % 60;
    uint32_t h = (remaining / 3600000);

    char buf[32];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    TDisplayUi::largeValue(canvas, buf, 53, stateColor);
    const float ratio = logic_.getDurationMs() == 0 ? 0.0f : static_cast<float>(remaining) / static_cast<float>(logic_.getDurationMs());
    TDisplayUi::bar(canvas, 22, 94, 196, 8, ratio, stateColor);
  });
}

void CountdownApp::drawFinishedAlert(TFT_eSPI& tft, bool flashOn) {
  const uint16_t bg = flashOn ? TFT_RED : TFT_BLACK;
  const uint16_t fg = flashOn ? TFT_WHITE : TFT_RED;
  const uint16_t sub = flashOn ? TFT_WHITE : TFT_LIGHTGREY;

  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    canvas.fillRect(0, 0, width, height, bg);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(sub, bg);
    canvas.setTextSize(2);
    canvas.drawString("Countdown", 10, 8);
    canvas.drawFastHLine(0, 29, width, flashOn ? TFT_WHITE : TFT_DARKGREY);

    canvas.setTextColor(fg, bg);
    canvas.setTextSize(4);
    String label = "TIME UP";
    canvas.drawString(label, (width - canvas.textWidth(label)) / 2, 45);

    canvas.setTextSize(2);
    String zero = "00:00:00";
    canvas.drawString(zero, (width - canvas.textWidth(zero)) / 2, 91);

    canvas.setTextSize(1);
    String hint = "B1 +10s / B1 hold start";
    canvas.setTextColor(sub, bg);
    canvas.drawString(hint, (width - canvas.textWidth(hint)) / 2, 121);
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
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Countdown", TFT_RED);
  TDisplayUi::centered(tft, "Set time", 50, 3, TFT_YELLOW);
  TDisplayUi::centered(tft, "Then hold to start", 88, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 start");
}

bool CountdownApp::hasCustomOverlay() const { return true; }
bool CountdownApp::startsRunningImmediately() const { return true; }
