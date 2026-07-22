#pragma once

#include "../../App.h"
#include "CydMediaBackend.h"

class MediaPlayerApp : public App {
 public:
  MediaPlayerApp(uint32_t width, uint32_t height);

  bool hasCustomOverlay() const override;
  bool handleTouch(const TouchUi::TouchSample& sample,
                   const TouchUi::TouchEvent& event) override;
  bool wantsImmediateRender() const override;
  uint16_t runningRenderIntervalMs() const override;

 protected:
  void onAppReset() override;
  void onAppExit() override;
  void updateRunning(uint32_t deltaMs, const ButtonInput& b1,
                     const ButtonInput& b2) override;
  void drawRunning(TFT_eSPI& tft) override;
  bool startsRunningImmediately() const override;

 private:
  enum class View : uint8_t { Browser, Player, Error };

  void refreshCatalog();
  void startSelected(TFT_eSPI& tft, uint8_t index);
  void returnToBrowser();
  void drawBrowser(TFT_eSPI& tft);
  void drawPlayer(TFT_eSPI& tft);
  void drawAudioOnly(TFT_eSPI& tft);
  void drawAudioStressFrame(TFT_eSPI& tft);
  void drawError(TFT_eSPI& tft);
  void drawTransport(TFT_eSPI& tft, bool force = false);
  void drawSeekOverlay(TFT_eSPI& tft);
  void drawProgress(TFT_eSPI& tft);
  void updateSeekTarget(int16_t x);
  uint8_t pageCount() const;

  CydMediaBackend backend_;
  MediaPlayerLogic::Catalog catalog_;
  View view_ = View::Browser;
  uint8_t page_ = 0;
  bool dirty_ = true;
  bool videoAreaReady_ = false;
  bool audioStress_ = false;
  bool seekMode_ = false;
  bool seekDragging_ = false;
  uint16_t audioStressFrame_ = 0;
  uint32_t pendingSeekMs_ = 0;
  uint32_t lastAudioStressMs_ = 0;
  uint32_t lastProgressSecond_ = UINT32_MAX;
  MediaPlayerLogic::State lastDrawnState_ = MediaPlayerLogic::State::Error;
  TFT_eSPI* tft_ = nullptr;
  TouchUi::ControlCapture touchCapture_;
};
