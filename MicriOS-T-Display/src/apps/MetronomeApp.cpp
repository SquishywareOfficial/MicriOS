#include "MetronomeApp.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include <TFT_eSPI.h>

MetronomeApp::MetronomeApp(uint32_t width, uint32_t height)
    : App("Metronome", width, height) {}

void MetronomeApp::onAppReset() {
  logic_.reset();
  pulseUntilMs_ = 0;
  uiInitialized_ = false;
  lastRunning_ = false;
  lastPulse_ = false;
  lastBpm_ = 0;
}

void MetronomeApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  if (b1.click) {
      if (logic_.isRunning()) logic_.toggle();
      else logic_.setBpm(logic_.getBpm() + 5);
      if (logic_.getBpm() > 300) logic_.setBpm(20);
      uiInitialized_ = false;
  }
  if (b2.click && !logic_.isRunning()) {
      logic_.setBpm(logic_.getBpm() <= 20 ? 300 : logic_.getBpm() - 5);
      uiInitialized_ = false;
  }
  if (b1.longPress) {
      logic_.toggle();
      uiInitialized_ = false;
  }

  if (logic_.update(millis())) {
      pulseUntilMs_ = millis() + 140;
  }
}

void MetronomeApp::drawMetronomeFrame(TFT_eSPI& tft, bool running, bool pulse) {
  const uint16_t panelBg = pulse ? TFT_DARKGREEN : TFT_BLACK;
  const uint16_t valueColor = pulse ? TFT_WHITE : (running ? TFT_GREEN : TFT_YELLOW);
  const uint16_t labelColor = pulse ? TFT_WHITE : TFT_LIGHTGREY;

  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "Metronome", running ? TFT_GREEN : TFT_YELLOW, running ? "RUN" : "SET");
    if (running) TDisplayUi::footer(canvas, "B1 stop / B1 hold stop");
    else TDisplayUi::footer(canvas, "B1 +5 / B2 -5 / B1 hold start");

    canvas.fillRoundRect(8, 35, width - 16, 72, 8, panelBg);
    canvas.drawRoundRect(8, 35, width - 16, 72, 8, pulse ? TFT_GREEN : (running ? TFT_DARKGREEN : TFT_DARKGREY));
    if (pulse) {
      canvas.fillRect(0, 31, 10, 84, TFT_GREEN);
      canvas.fillRect(width - 10, 31, 10, 84, TFT_GREEN);
      canvas.drawFastHLine(16, 41, width - 32, TFT_GREEN);
      canvas.drawFastHLine(16, 101, width - 32, TFT_GREEN);
    }

    String bpm = String(logic_.getBpm());
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(6);
    if (canvas.textWidth(bpm) > width - 70) {
      canvas.setTextSize(5);
    }
    canvas.setTextColor(valueColor, panelBg);
    canvas.drawString(bpm, (width - canvas.textWidth(bpm)) / 2, 45);

    canvas.setTextSize(2);
    canvas.setTextColor(labelColor, panelBg);
    canvas.drawString("BPM", (width - canvas.textWidth("BPM")) / 2, 95);
  });
}

void MetronomeApp::drawRunning(TFT_eSPI& tft) {
  const bool running = logic_.isRunning();
  const bool pulse = running && millis() < pulseUntilMs_;
  if (!uiInitialized_ || running != lastRunning_ || pulse != lastPulse_ || logic_.getBpm() != lastBpm_) {
    drawMetronomeFrame(tft, running, pulse);
    uiInitialized_ = true;
    lastRunning_ = running;
    lastPulse_ = pulse;
    lastBpm_ = logic_.getBpm();
  }
}

void MetronomeApp::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Metronome", TFT_BLUE);
  TDisplayUi::centered(tft, "120", 50, 5, TFT_YELLOW);
  TDisplayUi::centered(tft, "BPM", 99, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 start");
}

bool MetronomeApp::hasCustomOverlay() const { return true; }
bool MetronomeApp::startsRunningImmediately() const { return true; }
