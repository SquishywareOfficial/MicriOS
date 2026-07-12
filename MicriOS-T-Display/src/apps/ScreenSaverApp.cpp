#include "ScreenSaverApp.h"

#include <math.h>
#include <TFT_eSPI.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}

uint16_t rainbow(uint8_t n) {
  switch (n % 6) {
    case 0: return TFT_CYAN;
    case 1: return TFT_GREEN;
    case 2: return TFT_YELLOW;
    case 3: return TFT_ORANGE;
    case 4: return TFT_MAGENTA;
    default: return TFT_SKYBLUE;
  }
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t wheel(uint8_t p) {
  if (p < 85) return rgb565(255 - p * 3, p * 3, 40);
  if (p < 170) {
    p -= 85;
    return rgb565(20, 255 - p * 3, p * 3);
  }
  p -= 170;
  return rgb565(p * 3, 40, 255 - p * 3);
}
}

ScreenSaverApp::ScreenSaverApp(uint32_t width, uint32_t height)
    : App("Screen Saver", width, height) {}

uint16_t ScreenSaverApp::runningRenderIntervalMs() const {
  if (mode_ != Mode::Play) return 120;
  if (selected_ == Saver::Fire || selected_ == Saver::Fractal) return 70;
  return selected_ == Saver::Fractal ? 70 : 33;
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
  dirty_ = true;
  resetAnimation();
}

void ScreenSaverApp::resetStar(Star& star, bool farAway) {
  star.x = random(-120, 121) / 10.0f;
  star.y = random(-70, 71) / 10.0f;
  star.z = farAway ? random(50, 120) / 10.0f : random(8, 120) / 10.0f;
}

void ScreenSaverApp::resetAnimation() {
  elapsedMs_ = 0;
  frame_ = 0;
  fireworkPrimed_ = false;
  for (Star& star : stars_) resetStar(star, false);
  resetPipes();
  resetParticles();
  textX_ = 18;
  textY_ = 56;
  textVx_ = 2;
  textVy_ = 1;
}

void ScreenSaverApp::resetPipes() {
  pipeSegmentCount_ = 0;
  pipeRunCount_ = random(4, 7);
  pipePauseFrame_ = 0;
  pipeLastGrowthFrame_ = frame_;
  pipePaused_ = false;

  auto validMove = [this](int16_t x, int16_t y, int8_t dir, int16_t len, int16_t& nx, int16_t& ny) -> bool {
    nx = x;
    ny = y;
    if (dir == 0) nx += len;
    if (dir == 1) ny += len;
    if (dir == 2) nx -= len;
    if (dir == 3) ny -= len;
    return nx >= 10 && nx <= static_cast<int>(width) - 10 &&
           ny >= 18 && ny <= static_cast<int>(height) - 18;
  };

  auto appendSegment = [this](int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    if (pipeSegmentCount_ < sizeof(pipeSegments_) / sizeof(pipeSegments_[0])) {
      pipeSegments_[pipeSegmentCount_++] = {x1, y1, x2, y2, color};
    }
  };

  for (uint8_t i = 0; i < pipeRunCount_; i++) {
    const uint8_t edge = (i + random(0, 4)) % 4;
    if (edge == 0) {
      pipeRuns_[i].x = 12;
      pipeRuns_[i].y = random(24, static_cast<int>(height) - 24);
      pipeRuns_[i].dir = 0;
    } else if (edge == 1) {
      pipeRuns_[i].x = random(24, static_cast<int>(width) - 24);
      pipeRuns_[i].y = 18;
      pipeRuns_[i].dir = 1;
    } else if (edge == 2) {
      pipeRuns_[i].x = width - 12;
      pipeRuns_[i].y = random(24, static_cast<int>(height) - 24);
      pipeRuns_[i].dir = 2;
    } else {
      pipeRuns_[i].x = random(24, static_cast<int>(width) - 24);
      pipeRuns_[i].y = height - 18;
      pipeRuns_[i].dir = 3;
    }
    pipeRuns_[i].color = rainbow(i + random(0, 6));
    pipeRuns_[i].steps = random(14, 26);
    pipeRuns_[i].active = true;

    int16_t x = pipeRuns_[i].x;
    int16_t y = pipeRuns_[i].y;
    int8_t dir = pipeRuns_[i].dir;
    for (uint8_t step = 0; step < pipeRuns_[i].steps; step++) {
      if (pipeSegmentCount_ >= sizeof(pipeSegments_) / sizeof(pipeSegments_[0])) break;

      const int16_t len = 15 + ((i * 5 + step * 7 + random(0, 5)) % 10);
      const int8_t left = static_cast<int8_t>((dir + 3) % 4);
      const int8_t right = static_cast<int8_t>((dir + 1) % 4);
      const int8_t back = static_cast<int8_t>((dir + 2) % 4);
      const int8_t wish = (step % 4 == 1) ? left : ((step % 4 == 3) ? right : dir);
      const int8_t candidates[8] = {
          wish,
          dir,
          left,
          right,
          static_cast<int8_t>(x < static_cast<int>(width) / 2 ? 0 : 2),
          static_cast<int8_t>(y < static_cast<int>(height) / 2 ? 1 : 3),
          back,
          static_cast<int8_t>(random(0, 4)),
      };

      int8_t chosen = -1;
      int16_t nx = x;
      int16_t ny = y;
      for (uint8_t c = 0; c < 8; c++) {
        if (validMove(x, y, candidates[c], len, nx, ny)) {
          chosen = candidates[c];
          break;
        }
      }
      if (chosen < 0) break;

      appendSegment(x, y, nx, ny, pipeRuns_[i].color);
      x = nx;
      y = ny;
      dir = chosen;
    }
    pipeRuns_[i].x = x;
    pipeRuns_[i].y = y;
    pipeRuns_[i].dir = dir;
  }

  if (pipeSegmentCount_ == 0) {
    pipeSegments_[pipeSegmentCount_++] = {12, 68, static_cast<int16_t>(width - 12), 68, TFT_CYAN};
  }
}

void ScreenSaverApp::resetParticles() {
  for (uint8_t i = 0; i < 24; i++) {
    matrix_[i].y = random(-80, static_cast<int>(height));
    matrix_[i].speed = random(1, 4);
    matrix_[i].length = random(5, 14);
  }
  for (uint8_t i = 0; i < 7; i++) {
    balls_[i].x = random(20, static_cast<int>(width) - 20);
    balls_[i].y = random(24, static_cast<int>(height) - 20);
    balls_[i].vx = random(8, 20) / 10.0f * (random(0, 2) ? 1 : -1);
    balls_[i].vy = random(6, 17) / 10.0f * (random(0, 2) ? 1 : -1);
    balls_[i].r = random(4, 9);
    balls_[i].color = wheel(i * 31);
  }
  for (uint8_t i = 0; i < 60; i++) {
    particles_[i].x = random(0, static_cast<int>(width));
    particles_[i].y = random(0, static_cast<int>(height));
    particles_[i].vx = random(-12, 13) / 10.0f;
    particles_[i].vy = random(-8, 14) / 10.0f;
    particles_[i].life = random(20, 120);
    particles_[i].color = wheel(i * 7);
  }
}

void ScreenSaverApp::burstFirework(int16_t x, int16_t y) {
  const uint16_t color = wheel(frame_ * 3);
  for (uint8_t i = 0; i < 60; i++) {
    const float a = i * 0.628f;
    const float speed = 0.8f + (i % 7) * 0.22f;
    particles_[i].x = x;
    particles_[i].y = y;
    particles_[i].vx = cosf(a) * speed;
    particles_[i].vy = sinf(a) * speed;
    particles_[i].life = random(35, 90);
    particles_[i].color = i % 3 == 0 ? TFT_WHITE : color;
  }
}

const char* ScreenSaverApp::saverName(Saver saver) const {
  switch (saver) {
    case Saver::Stars: return "Starfield";
    case Saver::Pipes: return "2D Pipes";
    case Saver::Stargate: return "Stargate";
    case Saver::Lines: return "Mystify Lines";
    case Saver::Fractal: return "Fractal Zoom";
    case Saver::Text: return "Bouncing Text";
    case Saver::Matrix: return "Matrix Rain";
    case Saver::Plasma: return "Plasma";
    case Saver::Fire: return "Fire";
    case Saver::Balls: return "Bouncing Balls";
    case Saver::Ribbons: return "Bezier Ribbons";
    case Saver::Lissajous: return "Lissajous";
    case Saver::Hypercube: return "Hypercube";
    case Saver::Snow: return "Snowfall";
    case Saver::Fireworks: return "Fireworks";
    case Saver::Grid: return "Hyperspace Grid";
    case Saver::Radar: return "Radar Sweep";
    case Saver::Kaleidoscope: return "Kaleidoscope";
    case Saver::Spirograph: return "Spirograph";
    case Saver::Sandstorm: return "Sandstorm";
    case Saver::NightDrive: return "Night Drive";
    case Saver::Count: break;
  }
  return "Saver";
}

void ScreenSaverApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  if (mode_ == Mode::Select) {
    if (b1.click || b2.click) {
      int8_t dir = b2.click ? -1 : 1;
      int8_t next = static_cast<int8_t>(selected_) + dir;
      if (next < 0) next = static_cast<int8_t>(Saver::Count) - 1;
      if (next >= static_cast<int8_t>(Saver::Count)) next = 0;
      selected_ = static_cast<Saver>(next);
      dirty_ = true;
    }
    if (b1.longPress) {
      mode_ = Mode::Play;
      resetAnimation();
    }
    return;
  }

  elapsedMs_ += deltaMs;
  frame_++;
  if (b1.click) {
    selected_ = static_cast<Saver>((static_cast<uint8_t>(selected_) + 1) % static_cast<uint8_t>(Saver::Count));
    resetAnimation();
  }
  if (b2.click || b1.longPress) {
    mode_ = Mode::Select;
    dirty_ = true;
  }
}

void ScreenSaverApp::drawSelect(TFT_eSPI& tft) {
  if (!dirty_) return;
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Screen Saver", TFT_CYAN);
  TDisplayUi::centered(tft, saverName(selected_), 48, 2, TFT_WHITE);
  char pos[8];
  snprintf(pos, sizeof(pos), "%u/%u", static_cast<unsigned>(selected_) + 1, static_cast<unsigned>(Saver::Count));
  TDisplayUi::centered(tft, pos, 76, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 next  B2 prev  B1 hold play");
  dirty_ = false;
}

template <typename Canvas>
void ScreenSaverApp::drawStars(Canvas& canvas) {
  const int cx = width / 2;
  const int cy = height / 2;
  for (Star& star : stars_) {
    star.z -= 0.18f;
    if (star.z <= 0.8f) resetStar(star, true);
    int x = cx + static_cast<int>(star.x * 42.0f / star.z);
    int y = cy + static_cast<int>(star.y * 42.0f / star.z);
    if (x < 0 || x >= static_cast<int>(width) || y < 0 || y >= static_cast<int>(height)) {
      resetStar(star, true);
      continue;
    }
    uint16_t color = star.z < 2.6f ? TFT_WHITE : (star.z < 5.0f ? TFT_LIGHTGREY : TFT_DARKGREY);
    canvas.drawPixel(x, y, color);
    if (star.z < 2.4f) canvas.drawPixel(x + 1, y, color);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawPipes(Canvas& canvas) {
  if (pipePaused_) {
    if (static_cast<uint16_t>(frame_ - pipePauseFrame_) > 55) resetPipes();
  } else if (pipeSegmentCount_ == 0) {
    resetPipes();
  }

  const uint16_t age = static_cast<uint16_t>(frame_ - pipeLastGrowthFrame_);
  uint8_t visibleSegments = 1 + age / 3;
  if (visibleSegments > pipeSegmentCount_) visibleSegments = pipeSegmentCount_;
  if (!pipePaused_ && visibleSegments >= pipeSegmentCount_ && age > pipeSegmentCount_ * 3 + 55) {
    pipePaused_ = true;
    pipePauseFrame_ = frame_;
  }

  for (uint8_t i = 0; i < visibleSegments; i++) {
    drawPipeSegment(canvas, pipeSegments_[i]);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawPipeSegment(Canvas& canvas, const PipeSegment& seg) {
  const int16_t x = min(seg.x1, seg.x2);
  const int16_t y = min(seg.y1, seg.y2);
  const int16_t w = abs(seg.x2 - seg.x1);
  const int16_t h = abs(seg.y2 - seg.y1);
  if (w >= h) {
    canvas.fillRect(x, seg.y1 - 5, w + 1, 11, TFT_DARKGREY);
    canvas.fillRect(x, seg.y1 - 4, w + 1, 8, seg.color);
    canvas.drawFastHLine(x, seg.y1 - 3, w + 1, TFT_WHITE);
    canvas.drawFastHLine(x, seg.y1 + 4, w + 1, TFT_BLACK);
  } else {
    canvas.fillRect(seg.x1 - 5, y, 11, h + 1, TFT_DARKGREY);
    canvas.fillRect(seg.x1 - 4, y, 8, h + 1, seg.color);
    canvas.drawFastVLine(seg.x1 - 3, y, h + 1, TFT_WHITE);
    canvas.drawFastVLine(seg.x1 + 4, y, h + 1, TFT_BLACK);
  }
  canvas.fillCircle(seg.x1, seg.y1, 6, seg.color);
  canvas.fillCircle(seg.x2, seg.y2, 6, seg.color);
  canvas.drawCircle(seg.x1, seg.y1, 6, TFT_DARKGREY);
  canvas.drawCircle(seg.x2, seg.y2, 6, TFT_DARKGREY);
}

template <typename Canvas>
void ScreenSaverApp::drawStargate(Canvas& canvas) {
  const int cx = width / 2;
  const int cy = height / 2;
  for (uint8_t i = 0; i < 42; i++) {
    const float a = i * 0.1496f + frame_ * 0.018f;
    const float wobble = sinf(frame_ * 0.035f + i * 0.7f) * 8.0f;
    const int inner = 7 + ((frame_ * 2 + i * 5) % 34);
    const int outer = 132 + wobble;
    const int x1 = cx + cosf(a) * inner;
    const int y1 = cy + sinf(a) * inner * 0.58f;
    const int x2 = cx + cosf(a + 0.08f) * outer;
    const int y2 = cy + sinf(a + 0.08f) * outer * 0.58f;
    canvas.drawLine(x1, y1, x2, y2, rainbow(i + frame_ / 5));
  }
  for (uint8_t r = 0; r < 8; r++) {
    const int rad = 12 + ((frame_ * 3 + r * 17) % 116);
    canvas.drawEllipse(cx, cy, rad, rad / 2, rainbow(r + frame_ / 8));
  }
}

template <typename Canvas>
void ScreenSaverApp::drawLines(Canvas& canvas) {
  const int cx = width / 2;
  const int cy = height / 2;
  for (uint8_t i = 0; i < 18; i++) {
    const float a = (frame_ * 0.035f) + i * 0.42f;
    const float b = (frame_ * -0.026f) + i * 0.53f;
    int x1 = cx + cosf(a) * (20 + i * 5);
    int y1 = cy + sinf(b) * (12 + i * 3);
    int x2 = cx + sinf(a * 1.4f) * (32 + i * 4);
    int y2 = cy + cosf(b * 1.2f) * (18 + i * 2);
    canvas.drawLine(x1, y1, x2, y2, rainbow(i + frame_ / 8));
  }
}

template <typename Canvas>
void ScreenSaverApp::drawFractal(Canvas& canvas) {
  struct Target {
    float x;
    float y;
    float startScale;
    float zoom;
  };
  static constexpr Target TARGETS[] = {
      {-0.743643887f, 0.131825904f, 2.8f, 7.2f},
      {-0.800000000f, 0.156000000f, 2.7f, 6.4f},
      {-1.250000000f, 0.020000000f, 2.6f, 6.7f},
      {-0.235125000f, 0.827215000f, 2.5f, 6.3f},
      {-0.160000000f, 1.040000000f, 2.5f, 6.0f},
      {-0.390540000f, -0.586790000f, 2.6f, 6.5f},
  };
  static constexpr uint16_t TOUR_FRAMES = 1100;
  const uint8_t targetIndex = (frame_ / TOUR_FRAMES) % (sizeof(TARGETS) / sizeof(TARGETS[0]));
  const Target& target = TARGETS[targetIndex];
  const float phase = (frame_ % TOUR_FRAMES) / static_cast<float>(TOUR_FRAMES);
  const float eased = phase * phase * (3.0f - 2.0f * phase);
  const float scale = target.startScale * expf(-target.zoom * eased);
  const float centerX = target.x + sinf(frame_ * 0.011f) * scale * 0.045f;
  const float centerY = target.y + cosf(frame_ * 0.009f) * scale * 0.035f;
  for (int py = 0; py < static_cast<int>(height); py += 3) {
    for (int px = 0; px < static_cast<int>(width); px += 3) {
      float x0 = centerX + (px - width / 2.0f) * scale / width;
      float y0 = centerY + (py - height / 2.0f) * scale / width;
      float x = 0.0f;
      float y = 0.0f;
      uint8_t iter = 0;
      while (x * x + y * y <= 4.0f && iter < 26) {
        const float xt = x * x - y * y + x0;
        y = 2.0f * x * y + y0;
        x = xt;
        iter++;
      }
      const uint16_t color = iter == 26 ? TFT_BLACK : rainbow(iter + frame_ / 9 + targetIndex);
      canvas.fillRect(px, py, 3, 3, color);
    }
  }
}

template <typename Canvas>
void ScreenSaverApp::drawText(Canvas& canvas) {
  textX_ += textVx_;
  textY_ += textVy_;
  if (textX_ < 4 || textX_ > static_cast<int>(width) - 126) textVx_ = -textVx_;
  if (textY_ < 28 || textY_ > static_cast<int>(height) - 24) textVy_ = -textVy_;
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_NAVY, TFT_BLACK);
  canvas.drawString("MicriOS", textX_ + 5, textY_ + 5);
  canvas.setTextColor(TFT_DARKGREY, TFT_BLACK);
  canvas.drawString("MicriOS", textX_ + 3, textY_ + 3);
  canvas.setTextColor(rainbow(frame_ / 10), TFT_BLACK);
  canvas.drawString("MicriOS", textX_, textY_);
}

template <typename Canvas>
void ScreenSaverApp::drawMatrix(Canvas& canvas) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  for (uint8_t i = 0; i < 24; i++) {
    MatrixColumn& col = matrix_[i];
    col.y += col.speed;
    const int x = i * 10;
    if (col.y - col.length * 9 > static_cast<int>(height)) {
      col.y = random(-80, -8);
      col.speed = random(1, 4);
      col.length = random(5, 14);
    }
    for (uint8_t j = 0; j < col.length; j++) {
      const int y = col.y - j * 9;
      if (y < -8 || y > static_cast<int>(height)) continue;
      char glyph[2] = {static_cast<char>('0' + ((i * 7 + j * 3 + frame_ / 3) % 10)), 0};
      const uint16_t color = j == 0 ? TFT_WHITE : (j < 3 ? TFT_GREEN : rgb565(0, 80, 18));
      canvas.setTextColor(color, TFT_BLACK);
      canvas.drawString(glyph, x, y);
    }
  }
}

template <typename Canvas>
void ScreenSaverApp::drawPlasma(Canvas& canvas) {
  for (int y = 0; y < static_cast<int>(height); y += 6) {
    for (int x = 0; x < static_cast<int>(width); x += 6) {
      const float v = sinf(x * 0.075f + frame_ * 0.055f) +
                      sinf(y * 0.095f + frame_ * 0.043f) +
                      sinf((x + y) * 0.055f + frame_ * 0.035f);
      const uint8_t hue = static_cast<uint8_t>((v + 3.0f) * 42.0f + frame_);
      canvas.fillRect(x, y, 6, 6, wheel(hue));
    }
  }
}

template <typename Canvas>
void ScreenSaverApp::drawFire(Canvas& canvas) {
  for (int y = 0; y < static_cast<int>(height); y += 4) {
    const int fromBottom = height - y;
    for (int x = 0; x < static_cast<int>(width); x += 4) {
      int heat = y * 2 - static_cast<int>(fromBottom * 0.9f) +
                 static_cast<int>(24.0f * sinf(x * 0.13f + frame_ * 0.12f));
      heat += static_cast<int>(14.0f * sinf((x + y) * 0.09f - frame_ * 0.17f));
      if (heat < 0) heat = 0;
      if (heat > 255) heat = 255;
      uint16_t color = TFT_BLACK;
      if (heat > 170) color = rgb565(255, min(255, heat), 40);
      else if (heat > 85) color = rgb565(255, heat / 2, 0);
      else if (heat > 20) color = rgb565(heat * 2, 0, 0);
      canvas.fillRect(x, y, 4, 4, color);
    }
  }
}

template <typename Canvas>
void ScreenSaverApp::drawBalls(Canvas& canvas) {
  for (Ball& ball : balls_) {
    ball.x += ball.vx;
    ball.y += ball.vy;
    if (ball.x < ball.r) {
      ball.x = ball.r;
      ball.vx = fabsf(ball.vx);
    } else if (ball.x > width - ball.r) {
      ball.x = width - ball.r;
      ball.vx = -fabsf(ball.vx);
    }
    if (ball.y < ball.r) {
      ball.y = ball.r;
      ball.vy = fabsf(ball.vy);
    } else if (ball.y > height - ball.r) {
      ball.y = height - ball.r;
      ball.vy = -fabsf(ball.vy);
    }
  }

  for (uint8_t i = 0; i < 7; i++) {
    for (uint8_t j = i + 1; j < 7; j++) {
      Ball& a = balls_[i];
      Ball& b = balls_[j];
      const float dx = b.x - a.x;
      const float dy = b.y - a.y;
      const float minDist = a.r + b.r;
      const float dist2 = dx * dx + dy * dy;
      if (dist2 <= 0.01f || dist2 >= minDist * minDist) continue;

      const float dist = sqrtf(dist2);
      const float nx = dx / dist;
      const float ny = dy / dist;
      const float overlap = (minDist - dist) * 0.5f;
      a.x -= nx * overlap;
      a.y -= ny * overlap;
      b.x += nx * overlap;
      b.y += ny * overlap;

      const float va = a.vx * nx + a.vy * ny;
      const float vb = b.vx * nx + b.vy * ny;
      const float exchange = vb - va;
      a.vx += exchange * nx;
      a.vy += exchange * ny;
      b.vx -= exchange * nx;
      b.vy -= exchange * ny;
    }
  }

  for (Ball& ball : balls_) {
    canvas.fillCircle(static_cast<int>(ball.x), static_cast<int>(ball.y), ball.r, ball.color);
    canvas.drawCircle(static_cast<int>(ball.x), static_cast<int>(ball.y), ball.r, TFT_WHITE);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawRibbons(Canvas& canvas) {
  for (uint8_t r = 0; r < 5; r++) {
    int lastX = 0;
    int lastY = 0;
    for (uint8_t i = 0; i <= 24; i++) {
      const float t = i / 24.0f;
      const float ax = 18 + sinf(frame_ * 0.021f + r) * 16;
      const float ay = 18 + cosf(frame_ * 0.018f + r * 0.7f) * 12;
      const float bx = width / 2 + sinf(frame_ * 0.028f + r * 1.6f) * 78;
      const float by = height / 2 + cosf(frame_ * 0.032f + r) * 48;
      const float cx = width - 18 + cosf(frame_ * 0.017f + r) * 18;
      const float cy = height - 18 + sinf(frame_ * 0.024f + r * 0.9f) * 12;
      const float omt = 1.0f - t;
      const int x = static_cast<int>(omt * omt * ax + 2 * omt * t * bx + t * t * cx);
      const int y = static_cast<int>(omt * omt * ay + 2 * omt * t * by + t * t * cy);
      if (i > 0) canvas.drawLine(lastX, lastY, x, y, wheel(r * 36 + frame_ / 3));
      lastX = x;
      lastY = y;
    }
  }
}

template <typename Canvas>
void ScreenSaverApp::drawLissajous(Canvas& canvas) {
  int lastX = width / 2;
  int lastY = height / 2;
  for (uint16_t i = 0; i <= 180; i++) {
    const float t = i * 0.07f;
    const int x = width / 2 + static_cast<int>(sinf(t * 3.0f + frame_ * 0.035f) * 96);
    const int y = height / 2 + static_cast<int>(sinf(t * 4.0f + frame_ * 0.021f) * 52);
    if (i > 0) canvas.drawLine(lastX, lastY, x, y, wheel(i + frame_));
    lastX = x;
    lastY = y;
  }
}

template <typename Canvas>
void ScreenSaverApp::drawHypercube(Canvas& canvas) {
  struct Point {
    int16_t x;
    int16_t y;
    float z;
  };

  Point points[16];
  const float ax = frame_ * 0.024f;
  const float ay = frame_ * 0.018f;
  const float az = frame_ * 0.013f;
  const float cx = cosf(ax);
  const float sx = sinf(ax);
  const float cy = cosf(ay);
  const float sy = sinf(ay);
  const float cz = cosf(az);
  const float sz = sinf(az);

  for (uint8_t i = 0; i < 16; i++) {
    float x = (i & 1) ? 1.0f : -1.0f;
    float y = (i & 2) ? 1.0f : -1.0f;
    float z = (i & 4) ? 1.0f : -1.0f;
    float w = (i & 8) ? 1.0f : -1.0f;

    float tx = x * cx - w * sx;
    float tw = x * sx + w * cx;
    x = tx;
    w = tw;

    float ty = y * cy - z * sy;
    float tz = y * sy + z * cy;
    y = ty;
    z = tz;

    tx = x * cz - y * sz;
    ty = x * sz + y * cz;
    x = tx;
    y = ty;

    const float fourD = 2.5f / (2.9f - w);
    x *= fourD;
    y *= fourD;
    z *= fourD;
    const float threeD = 62.0f / (3.3f - z);
    points[i].x = width / 2 + static_cast<int16_t>(x * threeD);
    points[i].y = height / 2 + static_cast<int16_t>(y * threeD * 0.72f);
    points[i].z = z;
  }

  for (uint8_t i = 0; i < 16; i++) {
    for (uint8_t bit = 0; bit < 4; bit++) {
      const uint8_t j = i ^ (1 << bit);
      if (j < i) continue;
      const uint16_t color = bit == 3 ? TFT_MAGENTA : wheel(bit * 55 + frame_ / 3);
      canvas.drawLine(points[i].x, points[i].y, points[j].x, points[j].y, color);
    }
  }

  for (uint8_t i = 0; i < 16; i++) {
    const uint16_t color = points[i].z > 0 ? TFT_WHITE : TFT_LIGHTGREY;
    canvas.fillCircle(points[i].x, points[i].y, 2, color);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawSnow(Canvas& canvas) {
  for (Particle& p : particles_) {
    p.y += 0.35f + (p.life % 5) * 0.11f;
    p.x += sinf(frame_ * 0.04f + p.life) * 0.35f;
    if (p.y > height) {
      p.y = random(-20, 0);
      p.x = random(0, static_cast<int>(width));
      p.life = random(20, 120);
    }
    canvas.drawPixel(static_cast<int>(p.x), static_cast<int>(p.y), TFT_WHITE);
    if ((p.life & 3) == 0) canvas.drawPixel(static_cast<int>(p.x) + 1, static_cast<int>(p.y), TFT_LIGHTGREY);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawFireworks(Canvas& canvas) {
  if (!fireworkPrimed_) {
    fireworkPrimed_ = true;
    burstFirework(random(35, static_cast<int>(width) - 35), random(24, static_cast<int>(height) - 48));
  }

  bool anyAlive = false;
  for (Particle& p : particles_) {
    if (p.life == 0) continue;
    anyAlive = true;
    p.x += p.vx;
    p.y += p.vy;
    p.vy += 0.035f;
    p.life--;
    canvas.drawPixel(static_cast<int>(p.x), static_cast<int>(p.y), p.color);
    if (p.life > 20) canvas.drawPixel(static_cast<int>(p.x) + 1, static_cast<int>(p.y), p.color);
  }
  if (!anyAlive) {
    burstFirework(random(35, static_cast<int>(width) - 35), random(24, static_cast<int>(height) - 48));
  }
}

template <typename Canvas>
void ScreenSaverApp::drawGrid(Canvas& canvas) {
  const int cx = width / 2;
  const int horizon = height / 2;
  for (int i = -8; i <= 8; i++) {
    canvas.drawLine(cx, horizon, cx + i * 34, height, TFT_CYAN);
  }
  for (uint8_t i = 0; i < 12; i++) {
    const int y = horizon + ((frame_ * 3 + i * i * 6) % (height - horizon));
    canvas.drawFastHLine(0, y, width, wheel(i * 9 + frame_));
  }
  for (uint8_t i = 0; i < 24; i++) {
    const int x = (i * 37 + frame_ * 5) % width;
    const int y = (i * 19 + frame_ * 2) % horizon;
    canvas.drawPixel(x, y, TFT_WHITE);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawRadar(Canvas& canvas) {
  const int cx = width / 2;
  const int cy = height / 2;
  const int r = 56;
  canvas.drawCircle(cx, cy, r, TFT_GREEN);
  canvas.drawCircle(cx, cy, r / 2, rgb565(0, 90, 0));
  canvas.drawFastHLine(cx - r, cy, r * 2, rgb565(0, 90, 0));
  canvas.drawFastVLine(cx, cy - r, r * 2, rgb565(0, 90, 0));
  const float a = frame_ * 0.055f;
  canvas.drawLine(cx, cy, cx + cosf(a) * r, cy + sinf(a) * r, TFT_GREEN);
  for (uint8_t i = 0; i < 10; i++) {
    const float ba = i * 1.71f;
    const int br = 12 + ((i * 23 + frame_ / 4) % 42);
    canvas.fillCircle(cx + cosf(ba) * br, cy + sinf(ba) * br, 2, i % 3 == 0 ? TFT_WHITE : TFT_GREEN);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawKaleidoscope(Canvas& canvas) {
  const int cx = width / 2;
  const int cy = height / 2;
  for (uint8_t i = 0; i < 18; i++) {
    const float a = frame_ * 0.03f + i * 0.41f;
    const int r1 = 15 + (i * 9 + frame_) % 55;
    const int r2 = 35 + (i * 7 + frame_ * 2) % 70;
    const int x1 = cosf(a) * r1;
    const int y1 = sinf(a) * r1;
    const int x2 = cosf(a * 1.8f) * r2;
    const int y2 = sinf(a * 1.8f) * r2;
    const uint16_t color = wheel(i * 13 + frame_);
    for (uint8_t m = 0; m < 4; m++) {
      const int sx = (m & 1) ? -1 : 1;
      const int sy = (m & 2) ? -1 : 1;
      canvas.drawLine(cx + x1 * sx, cy + y1 * sy, cx + x2 * sx, cy + y2 * sy, color);
      canvas.drawLine(cx + y1 * sx, cy + x1 * sy, cx + y2 * sx, cy + x2 * sy, color);
    }
  }
}

template <typename Canvas>
void ScreenSaverApp::drawSpirograph(Canvas& canvas) {
  int lastX = width / 2;
  int lastY = height / 2;
  const float R = 51.0f;
  const float r = 19.0f + 4.0f * sinf(frame_ * 0.013f);
  const float d = 42.0f;
  for (uint16_t i = 0; i <= 220; i++) {
    const float t = i * 0.075f + frame_ * 0.018f;
    const float k = (R - r) / r;
    const int x = width / 2 + static_cast<int>((R - r) * cosf(t) + d * cosf(k * t));
    const int y = height / 2 + static_cast<int>(((R - r) * sinf(t) - d * sinf(k * t)) * 0.72f);
    if (i > 0) canvas.drawLine(lastX, lastY, x, y, wheel(i + frame_ / 2));
    lastX = x;
    lastY = y;
  }
}

template <typename Canvas>
void ScreenSaverApp::drawSandstorm(Canvas& canvas) {
  for (Particle& p : particles_) {
    p.x += 1.2f + sinf(frame_ * 0.025f + p.life) * 0.9f;
    p.y += sinf(frame_ * 0.04f + p.x * 0.02f) * 0.45f;
    if (p.x >= width) {
      p.x = random(-30, 0);
      p.y = random(8, static_cast<int>(height) - 8);
      p.life = random(10, 120);
    }
    const uint16_t color = (p.life & 1) ? TFT_YELLOW : TFT_ORANGE;
    canvas.drawPixel(static_cast<int>(p.x), static_cast<int>(p.y), color);
  }
}

template <typename Canvas>
void ScreenSaverApp::drawNightDrive(Canvas& canvas) {
  const int cx = width / 2;
  const int horizon = 55;
  canvas.fillRect(0, 0, width, height, rgb565(0, 0, 9));

  for (uint8_t i = 0; i < 26; i++) {
    const int x = (i * 37 + frame_ / 2) % width;
    const int y = (i * 17) % 42;
    canvas.drawPixel(x, y, (i & 1) ? TFT_DARKGREY : TFT_WHITE);
  }

  for (uint8_t side = 0; side < 2; side++) {
    const bool right = side == 1;
    const int baseX = right ? width - 12 : 0;
    for (uint8_t i = 0; i < 9; i++) {
      const int w = 10 + ((i * 7) % 16);
      const int h = 32 + ((i * 19) % 58);
      const int x = right ? baseX - i * 15 - w : i * 15;
      const int y = horizon - h + 8 + ((i * 3) % 10);
      canvas.fillRect(x, y, w, h, rgb565(4, 8, 18));
      canvas.drawFastVLine(x, y, h, right ? TFT_MAGENTA : TFT_CYAN);
      if ((i & 2) == 0) canvas.drawFastVLine(x + w - 1, y, h, right ? TFT_CYAN : TFT_MAGENTA);

      for (int wy = y + 5; wy < y + h - 3; wy += 7) {
        for (int wx = x + 3; wx < x + w - 2; wx += 5) {
          if (((wx + wy + frame_ / 9 + i) % 4) == 0) {
            canvas.drawPixel(wx, wy, (i & 1) ? TFT_YELLOW : TFT_CYAN);
          }
        }
      }

      if (i == 2 || i == 5) {
        const int signW = min(w + 8, 26);
        const uint16_t sign = i == 2 ? TFT_MAGENTA : TFT_CYAN;
        canvas.fillRect(right ? x - 2 : x + w - signW + 2, y + 10, signW, 14, sign);
        canvas.fillRect(right ? x : x + w - signW + 4, y + 12, signW - 4, 10,
                        i == 2 ? rgb565(40, 0, 25) : rgb565(0, 20, 32));
      }
    }
  }

  for (uint8_t side = 0; side < 2; side++) {
    const bool right = side == 1;
    for (uint8_t i = 0; i < 7; i++) {
      const int depth = (frame_ / 2 + i * 31) % 130;
      const int y = horizon + depth / 2 - 16;
      const int towerH = 20 + depth / 2;
      const int towerW = 5 + depth / 9;
      const int roadEdge = (depth * 82) / 130;
      const int x = right ? cx + 34 + roadEdge : cx - 34 - roadEdge - towerW;
      const uint16_t edge = right ? TFT_MAGENTA : TFT_CYAN;
      const uint16_t glow = right ? rgb565(45, 0, 42) : rgb565(0, 32, 45);

      if (x + towerW < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) continue;
      canvas.fillRect(x, y - towerH, towerW, towerH, rgb565(3, 6, 14));
      canvas.drawFastVLine(x, y - towerH, towerH, edge);
      canvas.drawFastVLine(x + towerW - 1, y - towerH, towerH, glow);

      if ((i & 1) == 0) {
        const int signH = min(22, max(8, towerH / 3));
        const int signY = y - towerH + 6 + ((frame_ / 6 + i * 5) % max(7, towerH - signH));
        canvas.fillRect(x + 1, signY, max(3, towerW - 2), signH, edge);
        canvas.fillRect(x + 2, signY + 2, max(1, towerW - 4), max(1, signH - 4), TFT_BLACK);
      }

      for (int wy = y - towerH + 5; wy < y - 4; wy += 9) {
        if (((wy + frame_ / 11 + i) & 3) == 0) {
          canvas.drawPixel(x + towerW / 2, wy, right ? TFT_CYAN : TFT_MAGENTA);
        }
      }
    }
  }

  canvas.fillTriangle(0, height, width, height, cx, horizon, rgb565(4, 4, 9));
  canvas.drawLine(cx, horizon, 4, height, rgb565(0, 38, 55));
  canvas.drawLine(cx, horizon, width - 4, height, rgb565(55, 0, 50));

  for (uint8_t i = 0; i < 10; i++) {
    const int y = horizon + ((frame_ * 3 + i * i * 7) % (height - horizon + 20));
    if (y >= height) continue;
    const int spread = (y - horizon) * 2;
    const int lane = max(3, (y - horizon) / 4);
    canvas.drawFastHLine(cx - lane / 2, y, lane, TFT_YELLOW);
    canvas.drawPixel(cx - spread / 2, y, TFT_CYAN);
    canvas.drawPixel(cx + spread / 2, y, TFT_MAGENTA);
  }

  for (uint8_t i = 0; i < 8; i++) {
    const int y = horizon + 7 + ((i * 29 + frame_ / 2) % 55);
    const int spread = max(8, (y - horizon) * 2);
    const int x = cx + ((i & 1) ? spread / 3 : -spread / 3);
    const uint16_t body = (i % 3 == 0) ? TFT_ORANGE : rgb565(10, 14, 22);
    canvas.fillRect(x - 5, y - 3, 10, 6, body);
    canvas.drawPixel(x - 3, y + 3, TFT_RED);
    canvas.drawPixel(x + 3, y + 3, TFT_RED);
    canvas.drawPixel(x - 3, y - 4, TFT_WHITE);
    canvas.drawPixel(x + 3, y - 4, TFT_WHITE);
  }

  canvas.fillRect(0, 108, width, 27, TFT_BLACK);
  canvas.fillRoundRect(44, 114, 63, 18, 6, rgb565(0, 18, 28));
  canvas.fillRoundRect(116, 113, 66, 20, 6, rgb565(13, 8, 21));
  canvas.drawFastHLine(0, 108, width, rgb565(0, 55, 80));
  canvas.drawFastHLine(0, 109, width, rgb565(80, 0, 70));
  canvas.drawCircle(28, 126, 20, rgb565(0, 80, 120));
  canvas.drawFastHLine(122, 122, 48, TFT_CYAN);
  canvas.drawPixel(154 + (frame_ % 12), 118, TFT_RED);
}

template <typename Canvas>
void ScreenSaverApp::drawFrame(Canvas& canvas) {
  switch (selected_) {
    case Saver::Stars: drawStars(canvas); break;
    case Saver::Pipes: drawPipes(canvas); break;
    case Saver::Stargate: drawStargate(canvas); break;
    case Saver::Lines: drawLines(canvas); break;
    case Saver::Fractal: drawFractal(canvas); break;
    case Saver::Text: drawText(canvas); break;
    case Saver::Matrix: drawMatrix(canvas); break;
    case Saver::Plasma: drawPlasma(canvas); break;
    case Saver::Fire: drawFire(canvas); break;
    case Saver::Balls: drawBalls(canvas); break;
    case Saver::Ribbons: drawRibbons(canvas); break;
    case Saver::Lissajous: drawLissajous(canvas); break;
    case Saver::Hypercube: drawHypercube(canvas); break;
    case Saver::Snow: drawSnow(canvas); break;
    case Saver::Fireworks: drawFireworks(canvas); break;
    case Saver::Grid: drawGrid(canvas); break;
    case Saver::Radar: drawRadar(canvas); break;
    case Saver::Kaleidoscope: drawKaleidoscope(canvas); break;
    case Saver::Spirograph: drawSpirograph(canvas); break;
    case Saver::Sandstorm: drawSandstorm(canvas); break;
    case Saver::NightDrive: drawNightDrive(canvas); break;
    case Saver::Count: break;
  }
}

void ScreenSaverApp::drawRunning(TFT_eSPI& tft) {
  tft.setRotation(1);
  if (mode_ == Mode::Select) {
    drawSelect(tft);
    return;
  }
  drawBuffered(tft, width, height, [this](auto& canvas) { drawFrame(canvas); });
}
