#include "MetronomeApp.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include <TFT_eSPI.h>

MetronomeApp::MetronomeApp(uint32_t width, uint32_t height)
    : App("Metronome", width, height) {}

namespace {
constexpr TouchUi::Rect BPM_SLIDER = {76, 145, 168, 23};
constexpr TouchUi::Rect BPM_MINUS = {14, 148, 54, 45};
constexpr TouchUi::Rect BPM_PLUS = {252, 148, 54, 45};
constexpr TouchUi::Rect START_STOP = {90, 171, 140, 29};
enum MetronomeControl : int16_t { Slider = 0, Minus = 1, Plus = 2,
                                  StartStop = 3 };

uint16_t sliderBpm(int16_t x) {
  if (x < BPM_SLIDER.x) x = BPM_SLIDER.x;
  if (x >= BPM_SLIDER.x + BPM_SLIDER.w) x = BPM_SLIDER.x + BPM_SLIDER.w - 1;
  return 20 + static_cast<uint16_t>(
                  (x - BPM_SLIDER.x) * 280L / (BPM_SLIDER.w - 1));
}
}

bool MetronomeApp::handleTouch(const TouchUi::TouchSample& sample,
                               const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (!logic_.isRunning() && BPM_SLIDER.contains(point)) hit = Slider;
  if (!logic_.isRunning() && BPM_MINUS.contains(point)) hit = Minus;
  if (!logic_.isRunning() && BPM_PLUS.contains(point)) hit = Plus;
  if (START_STOP.contains(point)) hit = StartStop;
  const auto result = touchCapture_.update(sample, event, hit);

  if (sample.down && touchCapture_.activeId() == Slider) {
    logic_.setBpm(sliderBpm(sample.point.x));
    uiInitialized_ = false;
  }
  if (result.pressed || result.released) uiInitialized_ = false;
  if (!result.activated) return result.consumed;

  if (result.id == Minus) logic_.setBpm(logic_.getBpm() - 5);
  if (result.id == Plus) logic_.setBpm(logic_.getBpm() + 5);
  if (result.id == StartStop) logic_.toggle();
  uiInitialized_ = false;
  return true;
}

void MetronomeApp::onAppReset() {
  logic_.reset();
  pulseUntilMs_ = 0;
  uiInitialized_ = false;
  lastRunning_ = false;
  lastPulse_ = false;
  lastBpm_ = 0;
  touchCapture_.reset();
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

  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "Metronome", running ? TFT_GREEN : TFT_YELLOW, running ? "RUN" : "SET");
    canvas.fillRoundRect(8, 43, width - 16, 96, 8, panelBg);
    canvas.drawRoundRect(8, 43, width - 16, 96, 8, pulse ? TFT_GREEN : (running ? TFT_DARKGREEN : TFT_DARKGREY));
    if (pulse) {
      canvas.fillRect(0, 39, 10, 102, TFT_GREEN);
      canvas.fillRect(width - 10, 39, 10, 102, TFT_GREEN);
      canvas.drawFastHLine(16, 49, width - 32, TFT_GREEN);
      canvas.drawFastHLine(16, 132, width - 32, TFT_GREEN);
    }

    String bpm = String(logic_.getBpm());
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(6);
    if (canvas.textWidth(bpm) > width - 70) {
      canvas.setTextSize(5);
    }
    canvas.setTextColor(valueColor, panelBg);
    canvas.drawString(bpm, (width - canvas.textWidth(bpm)) / 2, 51);

    canvas.setTextSize(2);
    canvas.setTextColor(labelColor, panelBg);
    canvas.drawString("BPM", (width - canvas.textWidth("BPM")) / 2, 113);

    CydUi::touchAction(canvas, BPM_MINUS, "-", TFT_CYAN, !running,
                       touchCapture_.activeId() == Minus);
    CydUi::touchAction(canvas, BPM_PLUS, "+", TFT_CYAN, !running,
                       touchCapture_.activeId() == Plus);
    canvas.drawRoundRect(BPM_SLIDER.x, BPM_SLIDER.y, BPM_SLIDER.w,
                         BPM_SLIDER.h, 5, running ? TFT_DARKGREY : TFT_CYAN);
    const int16_t knobX = BPM_SLIDER.x +
        static_cast<int16_t>((logic_.getBpm() - 20) *
                             (BPM_SLIDER.w - 1) / 280L);
    canvas.fillCircle(knobX, BPM_SLIDER.y + BPM_SLIDER.h / 2, 7,
                      running ? TFT_DARKGREY : TFT_WHITE);
    CydUi::touchAction(canvas, START_STOP, running ? "Stop" : "Start",
                       running ? TFT_ORANGE : TFT_GREEN, true,
                       touchCapture_.activeId() == StartStop);
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
  CydUi::clear(tft);
  CydUi::header(tft, "Metronome", TFT_BLUE);
  CydUi::centered(tft, "120", 66, 6, TFT_YELLOW);
  CydUi::centered(tft, "BPM", 135, 2, TFT_LIGHTGREY);
  CydUi::footer(tft, "Set BPM and tap Start");
}

bool MetronomeApp::hasCustomOverlay() const { return true; }
bool MetronomeApp::startsRunningImmediately() const { return true; }
