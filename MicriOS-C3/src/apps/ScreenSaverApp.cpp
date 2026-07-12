#include "ScreenSaverApp.h"

#include <math.h>
#include <U8g2lib.h>

ScreenSaverApp::ScreenSaverApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Screen Saver", width, height) {
  (void)left;
}

uint16_t ScreenSaverApp::runningRenderIntervalMs() const {
  return mode_ == Mode::Play ? 45 : 150;
}

bool ScreenSaverApp::hasCustomOverlay() const {
  return true;
}

bool ScreenSaverApp::startsRunningImmediately() const {
  return true;
}

void ScreenSaverApp::onAppReset() {
  mode_ = Mode::Select;
  selected_ = Saver::Stars;
  resetAnimation();
}

const char* ScreenSaverApp::saverName(Saver saver) const {
  switch (saver) {
    case Saver::Stars: return "Stars";
    case Saver::Text: return "MicriOS";
    case Saver::Lines: return "Lines";
    case Saver::Matrix: return "Matrix";
    case Saver::Snow: return "Snow";
    case Saver::Lissajous: return "Lissajous";
    case Saver::Radar: return "Radar";
    case Saver::Fireworks: return "Fireworks";
    case Saver::Count: break;
  }
  return "Saver";
}

void ScreenSaverApp::resetStar(Star& star) {
  star.x = random(-35, 36);
  star.y = random(-18, 19);
  star.z = random(5, 32);
}

void ScreenSaverApp::resetAnimation() {
  frame_ = 0;
  for (Star& star : stars_) resetStar(star);
  for (uint8_t i = 0; i < 12; i++) {
    drops_[i].x = random(1, static_cast<int>(width) - 1);
    drops_[i].y = random(-20, static_cast<int>(height));
    drops_[i].speed = random(1, 4);
  }
  for (Spark& spark : sparks_) {
    spark.life = 0;
  }
  textX_ = 8;
  textY_ = 21;
  textVx_ = 1;
  textVy_ = 1;
}

void ScreenSaverApp::burstFirework() {
  const int8_t cx = random(16, static_cast<int>(width) - 16);
  const int8_t cy = random(8, 24);
  for (uint8_t i = 0; i < 18; i++) {
    sparks_[i].x = cx;
    sparks_[i].y = cy;
    sparks_[i].vx = static_cast<int8_t>((i % 7) - 3);
    sparks_[i].vy = static_cast<int8_t>((i / 3) - 3);
    if (sparks_[i].vx == 0 && sparks_[i].vy == 0) sparks_[i].vx = 1;
    sparks_[i].life = random(8, 18);
  }
}

void ScreenSaverApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (mode_ == Mode::Select) {
    if (input.click) {
      selected_ = static_cast<Saver>((static_cast<uint8_t>(selected_) + 1) % static_cast<uint8_t>(Saver::Count));
    }
    if (input.longPress) {
      mode_ = Mode::Play;
      resetAnimation();
    }
    return;
  }

  frame_++;
  if (input.click) {
    selected_ = static_cast<Saver>((static_cast<uint8_t>(selected_) + 1) % static_cast<uint8_t>(Saver::Count));
    resetAnimation();
  }
  if (input.longPress) {
    mode_ = Mode::Select;
  }
}

void ScreenSaverApp::drawSelect(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Saver");
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* name = saverName(selected_);
  int x = (width + 2 - u8g2.getStrWidth(name)) / 2;
  if (x < 2) x = 2;
  u8g2.drawStr(x, 24, name);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap next Hold play");
}

void ScreenSaverApp::drawStars(U8G2& u8g2) {
  const int cx = width / 2;
  const int cy = height / 2;
  for (Star& star : stars_) {
    if (star.z > 1) star.z--;
    int x = cx + (star.x * 12) / star.z;
    int y = cy + (star.y * 12) / star.z;
    if (x < 0 || x >= static_cast<int>(width) || y < 0 || y >= static_cast<int>(height)) {
      resetStar(star);
      star.z = 32;
      continue;
    }
    u8g2.drawPixel(x, y);
    if (star.z < 6) u8g2.drawPixel(x + 1, y);
  }
}

void ScreenSaverApp::drawText(U8G2& u8g2) {
  textX_ += textVx_;
  textY_ += textVy_;
  if (textX_ < 1 || textX_ > 25) textVx_ = -textVx_;
  if (textY_ < 10 || textY_ > 38) textVy_ = -textVy_;
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(textX_, textY_, "Micri");
  u8g2.drawStr(textX_ + 8, textY_ + 8, "OS");
}

void ScreenSaverApp::drawLines(U8G2& u8g2) {
  const int cx = width / 2;
  const int cy = height / 2;
  for (uint8_t i = 0; i < 8; i++) {
    int a = (frame_ + i * 9) % 64;
    int b = (frame_ * 2 + i * 11) % 64;
    int x1 = cx + ((a < 32 ? a : 64 - a) - 16) * 2;
    int y1 = cy + ((b < 32 ? b : 64 - b) - 16);
    int x2 = cx + ((b < 32 ? b : 64 - b) - 16) * 2;
    int y2 = cy + ((a < 32 ? a : 64 - a) - 16);
    u8g2.drawLine(x1, y1, x2, y2);
  }
}

void ScreenSaverApp::drawMatrix(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_4x6_tr);
  for (Drop& drop : drops_) {
    drop.y += drop.speed;
    if (drop.y > static_cast<int>(height) + 8) {
      drop.y = random(-24, -4);
      drop.x = random(1, static_cast<int>(width) - 4);
      drop.speed = random(1, 4);
    }
    for (uint8_t i = 0; i < 4; i++) {
      const int y = drop.y - i * 7;
      if (y < 0 || y > static_cast<int>(height)) continue;
      char ch[2] = {static_cast<char>('0' + ((drop.x + y + frame_ / 3) % 10)), 0};
      u8g2.drawStr(drop.x, y, ch);
    }
  }
}

void ScreenSaverApp::drawSnow(U8G2& u8g2) {
  for (Drop& drop : drops_) {
    drop.y += drop.speed;
    drop.x += ((frame_ + drop.y) & 3) == 0 ? 1 : 0;
    if (drop.y >= static_cast<int>(height)) {
      drop.y = random(-12, 0);
      drop.x = random(1, static_cast<int>(width) - 1);
      drop.speed = random(1, 3);
    }
    if (drop.x >= static_cast<int>(width)) drop.x = 0;
    u8g2.drawPixel(drop.x, drop.y);
  }
}

void ScreenSaverApp::drawLissajous(U8G2& u8g2) {
  int lastX = width / 2;
  int lastY = height / 2;
  for (uint8_t i = 0; i < 80; i++) {
    const float t = i * 0.11f;
    const int x = width / 2 + static_cast<int>(sinf(t * 3.0f + frame_ * 0.05f) * 31);
    const int y = height / 2 + static_cast<int>(sinf(t * 4.0f + frame_ * 0.033f) * 17);
    if (i > 0) u8g2.drawLine(lastX, lastY, x, y);
    lastX = x;
    lastY = y;
  }
}

void ScreenSaverApp::drawRadar(U8G2& u8g2) {
  const int cx = width / 2;
  const int cy = height / 2;
  const int r = 18;
  u8g2.drawCircle(cx, cy, r);
  u8g2.drawCircle(cx, cy, 9);
  const float a = frame_ * 0.09f;
  u8g2.drawLine(cx, cy, cx + static_cast<int>(cosf(a) * r), cy + static_cast<int>(sinf(a) * r));
  for (uint8_t i = 0; i < 5; i++) {
    const float ba = i * 1.31f;
    const int br = 5 + ((i * 7 + frame_ / 5) % 12);
    u8g2.drawPixel(cx + static_cast<int>(cosf(ba) * br), cy + static_cast<int>(sinf(ba) * br));
  }
}

void ScreenSaverApp::drawFireworks(U8G2& u8g2) {
  if ((frame_ % 70) == 1) burstFirework();
  bool anyAlive = false;
  for (Spark& spark : sparks_) {
    if (spark.life == 0) continue;
    anyAlive = true;
    spark.x += spark.vx;
    spark.y += spark.vy;
    if ((frame_ & 3) == 0) spark.vy++;
    spark.life--;
    if (spark.x >= 0 && spark.x < static_cast<int>(width) && spark.y >= 0 && spark.y < static_cast<int>(height)) {
      u8g2.drawPixel(spark.x, spark.y);
    }
  }
  if (!anyAlive) burstFirework();
}

void ScreenSaverApp::drawRunning(U8G2& u8g2) {
  if (mode_ == Mode::Select) {
    drawSelect(u8g2);
    return;
  }

  switch (selected_) {
    case Saver::Stars: drawStars(u8g2); break;
    case Saver::Text: drawText(u8g2); break;
    case Saver::Lines: drawLines(u8g2); break;
    case Saver::Matrix: drawMatrix(u8g2); break;
    case Saver::Snow: drawSnow(u8g2); break;
    case Saver::Lissajous: drawLissajous(u8g2); break;
    case Saver::Radar: drawRadar(u8g2); break;
    case Saver::Fireworks: drawFireworks(u8g2); break;
    case Saver::Count: break;
  }
}
