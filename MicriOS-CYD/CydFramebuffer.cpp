#include "CydFramebuffer.h"

#include <Arduino.h>

namespace CydFramebuffer {
namespace {
TFT_eSprite* frame = nullptr;
int16_t frameWidth = 0;
int16_t frameHeight = 0;
uint8_t frameDepth = 0;

void resetFrame() {
  if (frame != nullptr) {
    frame->deleteSprite();
  }
  frameWidth = 0;
  frameHeight = 0;
  frameDepth = 0;
}
}

TFT_eSprite* acquire(TFT_eSPI& tft, int16_t width, int16_t height, uint8_t colorDepth) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  if (frame == nullptr) {
    frame = new TFT_eSprite(&tft);
    if (frame == nullptr) {
      return nullptr;
    }
    // TFT_eSprite's constructor in TFT_eSPI 2.5.43 does not initialize the
    // inherited GFX-font pointer for heap-created sprites. Fresh ESP32 heap is
    // filled with 0xA5, so the first textWidth() can dereference 0xA5A5A5A5.
    // Selecting the built-in font explicitly clears that pointer.
    frame->setTextFont(1);
  }

  if (frameWidth == width && frameHeight == height && frameDepth == colorDepth) {
    return frame;
  }

  resetFrame();
  frame->setColorDepth(colorDepth);
  if (frame->createSprite(width, height) == nullptr) {
    resetFrame();
    Serial.print("[cyd-framebuffer] allocation failed ");
    Serial.print(width);
    Serial.print("x");
    Serial.print(height);
    Serial.print("x");
    Serial.println(colorDepth);
    return nullptr;
  }

  frameWidth = width;
  frameHeight = height;
  frameDepth = colorDepth;
  return frame;
}

void release() {
  resetFrame();
}

}
