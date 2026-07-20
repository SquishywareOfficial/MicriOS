#include "CydDeviceSettingsApp.h"

#include <ESP.h>
#include <TFT_eSPI.h>

#include "../../CydFramebuffer.h"
#include "../../CydHardware.h"
#include "../../CydUi.h"

namespace {
constexpr const char* ITEMS[] = {
    "Brightness", "Calibrate touch", "Touch test", "RGB test", "Exit"};
constexpr uint8_t ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);
constexpr TouchUi::Rect BRIGHTNESS_SLIDER = {30, 94, 260, 30};
constexpr TouchUi::Rect CANCEL_ACTION = {20, 151, 132, 43};
constexpr TouchUi::Rect SAVE_ACTION = {168, 151, 132, 43};
constexpr TouchUi::Rect BACK_ACTION = {90, 158, 140, 40};
constexpr TouchUi::Rect RGB_ACTIONS[] = {
    {12, 104, 68, 42}, {88, 104, 68, 42},
    {164, 104, 68, 42}, {240, 104, 68, 42}};

TouchUi::Rect settingsRow(uint8_t index) {
  return {12, static_cast<int16_t>(40 + index * 32), 296, 28};
}

uint8_t brightnessForX(int16_t x) {
  if (x < BRIGHTNESS_SLIDER.x) x = BRIGHTNESS_SLIDER.x;
  if (x >= BRIGHTNESS_SLIDER.x + BRIGHTNESS_SLIDER.w) {
    x = BRIGHTNESS_SLIDER.x + BRIGHTNESS_SLIDER.w - 1;
  }
  return 1 + static_cast<uint8_t>(
                 (x - BRIGHTNESS_SLIDER.x) * 15L /
                 (BRIGHTNESS_SLIDER.w - 1));
}
}

CydDeviceSettingsApp::CydDeviceSettingsApp(uint32_t width, uint32_t height)
    : App("Device", width, height) {}

bool CydDeviceSettingsApp::handleTouch(
    const TouchUi::TouchSample& sample, const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;

  if (view_ == View::Menu) {
    for (uint8_t i = 0; i < ITEM_COUNT; ++i) {
      if (settingsRow(i).contains(point)) {
        hit = i;
        break;
      }
    }
  } else if (view_ == View::Brightness) {
    if (BRIGHTNESS_SLIDER.contains(point)) hit = 0;
    if (CANCEL_ACTION.contains(point)) hit = 1;
    if (SAVE_ACTION.contains(point)) hit = 2;
  } else if (view_ == View::CalibrateConfirm) {
    if (CANCEL_ACTION.contains(point)) hit = 0;
    if (SAVE_ACTION.contains(point)) hit = 1;
  } else if (view_ == View::TouchTest) {
    if (BACK_ACTION.contains(point)) hit = 0;
  } else if (view_ == View::RgbTest) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (RGB_ACTIONS[i].contains(point)) hit = i;
    }
    if (BACK_ACTION.contains(point)) hit = 4;
  }

  const auto result = touchCapture_.update(sample, event, hit);
  if (sample.down && view_ == View::Brightness &&
      touchCapture_.activeId() == 0) {
    brightness_ = brightnessForX(sample.point.x);
    CydHardware::setBrightness(brightness_);
    dirty_ = true;
  }
  if (result.pressed || result.released) dirty_ = true;
  if (!result.activated) {
    return result.consumed ||
           (view_ == View::TouchTest && (sample.down || event.released));
  }

  if (view_ == View::Menu) {
    selection_ = result.id;
    if (selection_ == 0) {
      originalBrightness_ = brightness_;
      view_ = View::Brightness;
    } else if (selection_ == 1) {
      view_ = View::CalibrateConfirm;
    } else if (selection_ == 2) {
      view_ = View::TouchTest;
    } else if (selection_ == 3) {
      view_ = View::RgbTest;
    } else {
      requestExitToMenu();
    }
  } else if (view_ == View::Brightness) {
    if (result.id == 1) {
      brightness_ = originalBrightness_;
      CydHardware::setBrightness(brightness_);
      view_ = View::Menu;
    } else if (result.id == 2) {
      CydHardware::saveBrightness(brightness_);
      view_ = View::Menu;
    }
  } else if (view_ == View::CalibrateConfirm) {
    if (result.id == 0) {
      view_ = View::Menu;
    } else {
      CydHardware::clearCalibration();
      ESP.restart();
    }
  } else if (view_ == View::TouchTest) {
    view_ = View::Menu;
  } else if (view_ == View::RgbTest) {
    if (result.id == 4) {
      CydHardware::setRgb(0, 0, 0);
      view_ = View::Menu;
    } else {
      rgbMode_ = result.id == 3 ? 0 : result.id + 1;
      CydHardware::setRgb(rgbMode_ == 1 ? 255 : 0,
                          rgbMode_ == 2 ? 255 : 0,
                          rgbMode_ == 3 ? 255 : 0);
    }
  }
  touchCapture_.reset();
  dirty_ = true;
  return true;
}

bool CydDeviceSettingsApp::startsRunningImmediately() const { return true; }

bool CydDeviceSettingsApp::consumesButton2HoldInRunning() const { return false; }

void CydDeviceSettingsApp::onAppReset() {
  view_ = View::Menu;
  selection_ = 0;
  brightness_ = CydHardware::loadBrightness();
  originalBrightness_ = brightness_;
  rgbMode_ = 0;
  touchCapture_.reset();
  dirty_ = true;
}

void CydDeviceSettingsApp::onAppExit() {
  CydHardware::setRgb(0, 0, 0);
}

void CydDeviceSettingsApp::nextItem(int8_t direction) {
  if (direction > 0) {
    selection_ = (selection_ + 1) % ITEM_COUNT;
  } else {
    selection_ = selection_ == 0 ? ITEM_COUNT - 1 : selection_ - 1;
  }
  dirty_ = true;
}

void CydDeviceSettingsApp::updateRunning(uint32_t, const ButtonInput& b1,
                                         const ButtonInput& b2) {
  if (view_ == View::Menu) {
    if (b1.click) nextItem(1);
    if (b2.click) nextItem(-1);
    if (!b1.longPress) return;

    switch (selection_) {
      case 0:
        view_ = View::Brightness;
        break;
      case 1:
        view_ = View::CalibrateConfirm;
        break;
      case 2:
        view_ = View::TouchTest;
        break;
      case 3:
        view_ = View::RgbTest;
        break;
      default:
        requestExitToMenu();
        break;
    }
    dirty_ = true;
    return;
  }

  if (view_ == View::Brightness) {
    if (b1.click) brightness_ = brightness_ == 16 ? 1 : brightness_ + 1;
    if (b2.click) brightness_ = brightness_ == 1 ? 16 : brightness_ - 1;
    if (b1.click || b2.click) {
      CydHardware::setBrightness(brightness_);
      dirty_ = true;
    }
    if (b1.longPress) {
      CydHardware::saveBrightness(brightness_);
      view_ = View::Menu;
      dirty_ = true;
    }
    return;
  }

  if (view_ == View::RgbTest) {
    if (b1.click) {
      rgbMode_ = (rgbMode_ + 1) % 4;
      CydHardware::setRgb(rgbMode_ == 1 ? 255 : 0,
                          rgbMode_ == 2 ? 255 : 0,
                          rgbMode_ == 3 ? 255 : 0);
      dirty_ = true;
    }
  }

  if (b2.click || b1.longPress) {
    CydHardware::setRgb(0, 0, 0);
    view_ = View::Menu;
    dirty_ = true;
  }
}

void CydDeviceSettingsApp::drawRunning(TFT_eSPI& tft) {
  dirty_ = false;
  CydFramebuffer::draw(tft, static_cast<int16_t>(width),
                       static_cast<int16_t>(height), [&](auto& canvas) {
  CydUi::clear(canvas);

  if (view_ == View::Menu) {
    CydUi::header(canvas, "Device Settings", TFT_CYAN);
    for (uint8_t row = 0; row < ITEM_COUNT; ++row) {
      String detail;
      if (row == 0) detail = String(brightness_) + "/16";
      CydUi::touchListRow(canvas, settingsRow(row), ITEMS[row],
                          row == selection_, TFT_CYAN,
                          detail.length() ? detail.c_str() : nullptr,
                          touchCapture_.activeId() == row);
    }
    return;
  }

  if (view_ == View::Brightness) {
    CydUi::header(canvas, "Brightness", TFT_YELLOW);
    CydUi::centered(canvas, String(brightness_) + " / 16", 51, 4,
                    TFT_YELLOW);
    CydUi::bar(canvas, BRIGHTNESS_SLIDER.x, BRIGHTNESS_SLIDER.y + 8,
               BRIGHTNESS_SLIDER.w, 14, brightness_ / 16.0f, TFT_YELLOW);
    CydUi::touchAction(canvas, CANCEL_ACTION, "Cancel", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 1);
    CydUi::touchAction(canvas, SAVE_ACTION, "Save", TFT_GREEN, true,
                       touchCapture_.activeId() == 2);
    return;
  }

  if (view_ == View::CalibrateConfirm) {
    CydUi::confirmation(canvas, "Touch Calibration",
                        "Restart and calibrate the screen?", CANCEL_ACTION,
                        SAVE_ACTION, "Calibrate");
    return;
  }

  if (view_ == View::TouchTest) {
    const auto raw = CydHardware::readRawTouch();
    const auto touch = CydHardware::readTouch();
    CydUi::header(canvas, "Touch Diagnostics", TFT_GREEN);
    CydUi::labelValue(canvas, 55, "Raw X", String(raw.x), TFT_WHITE);
    CydUi::labelValue(canvas, 82, "Raw Y", String(raw.y), TFT_WHITE);
    CydUi::labelValue(canvas, 109, "Screen", String(touch.point.x) + "," +
                                             String(touch.point.y), TFT_GREEN);
    CydUi::labelValue(canvas, 136, "Pressure", String(raw.pressure), TFT_CYAN);
    CydUi::touchAction(canvas, BACK_ACTION, "Back", TFT_CYAN, false,
                       touchCapture_.activeId() == 0);
    return;
  }

  CydUi::header(canvas, "RGB Test", TFT_MAGENTA);
  const char* name = rgbMode_ == 1 ? "RED" : rgbMode_ == 2 ? "GREEN" :
                     rgbMode_ == 3 ? "BLUE" : "OFF";
  CydUi::largeValue(canvas, name, 54, TFT_WHITE);
  const char* labels[] = {"Red", "Green", "Blue", "Off"};
  const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_LIGHTGREY};
  for (uint8_t i = 0; i < 4; ++i) {
    CydUi::touchAction(canvas, RGB_ACTIONS[i], labels[i], colors[i],
                       rgbMode_ == i + 1 || (i == 3 && rgbMode_ == 0),
                       touchCapture_.activeId() == i);
  }
  CydUi::touchAction(canvas, BACK_ACTION, "Back", TFT_CYAN, false,
                     touchCapture_.activeId() == 4);
  });
}

uint16_t CydDeviceSettingsApp::runningRenderIntervalMs() const {
  return view_ == View::TouchTest ? 100 : 0;
}

bool CydDeviceSettingsApp::wantsImmediateRender() const { return dirty_; }
