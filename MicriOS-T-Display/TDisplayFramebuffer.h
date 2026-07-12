#pragma once

#include <stdint.h>

#include <TFT_eSPI.h>

namespace TDisplayFramebuffer {

TFT_eSprite* acquire(TFT_eSPI& tft, int16_t width, int16_t height, uint8_t colorDepth = 8);
void release();

template <typename Drawer>
bool draw(TFT_eSPI& tft, int16_t width, int16_t height, Drawer drawer, uint8_t colorDepth = 8) {
  TFT_eSprite* frame = acquire(tft, width, height, colorDepth);
  if (frame != nullptr) {
    frame->fillSprite(TFT_BLACK);
    drawer(*frame);
    frame->pushSprite(0, 0);
    return true;
  }

  tft.fillScreen(TFT_BLACK);
  drawer(tft);
  return false;
}

}
