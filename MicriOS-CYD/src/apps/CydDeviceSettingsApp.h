#pragma once

#include "../../App.h"

class CydDeviceSettingsApp : public App {
 public:
  CydDeviceSettingsApp(uint32_t width, uint32_t height);
  bool handleTouch(const TouchUi::TouchSample& sample,
                   const TouchUi::TouchEvent& event) override;

 protected:
  bool startsRunningImmediately() const override;
  bool consumesButton2HoldInRunning() const override;
  void onAppReset() override;
  void onAppExit() override;
  void updateRunning(uint32_t deltaMs, const ButtonInput& b1,
                     const ButtonInput& b2) override;
  void drawRunning(TFT_eSPI& tft) override;
  uint16_t runningRenderIntervalMs() const override;
  bool wantsImmediateRender() const override;

 private:
  enum class View : uint8_t {
    Menu,
    Brightness,
    CalibrateConfirm,
    TouchTest,
    RgbTest
  };

  void nextItem(int8_t direction);

  View view_ = View::Menu;
  uint8_t selection_ = 0;
  uint8_t brightness_ = 13;
  uint8_t originalBrightness_ = 13;
  uint8_t rgbMode_ = 0;
  bool dirty_ = true;
  TouchUi::ControlCapture touchCapture_;
};
