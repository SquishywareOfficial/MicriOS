#include "TDisplayFramebuffer.h"

#include <Arduino.h>

namespace TDisplayFramebuffer {
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
  }

  if (frameWidth == width && frameHeight == height && frameDepth == colorDepth) {
    return frame;
  }

  resetFrame();
  frame->setColorDepth(colorDepth);
  if (frame->createSprite(width, height) == nullptr) {
    resetFrame();
    Serial.print("[tdisplay-framebuffer] allocation failed ");
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
