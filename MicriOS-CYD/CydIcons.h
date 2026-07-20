#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace CydIcons {

enum class Icon : uint8_t {
  Games,
  Apps,
  Settings,
  About,
  AlienRaiders,
  Breakout,
  CaveChopper,
  CityRacer,
  Casino,
  Field,
  Fishing,
  Knife,
  MazeCollector,
  MazeRunner,
  Lander,
  Speed,
  Shooter,
  Pet,
  Pipe,
  Reactor,
  Simon,
  Golf,
  Cards,
  Tower,
  Clock,
  Stopwatch,
  Countdown,
  Counter,
  Dice,
  Coin,
  Random,
  Screensaver,
  Metronome,
  Bitcoin,
  Cluster,
  Contacts,
  Communicator,
  Mouse,
  Reading,
  Device,
  Options,
  Autolaunch,
  Wifi,
};

template <typename Canvas>
inline void drawWifi(Canvas& canvas, int16_t cx, int16_t cy,
                     uint16_t color) {
  canvas.fillCircle(cx, cy + 9, 3, color);
  canvas.drawLine(cx - 7, cy + 4, cx - 3, cy, color);
  canvas.drawLine(cx - 3, cy, cx + 3, cy, color);
  canvas.drawLine(cx + 3, cy, cx + 7, cy + 4, color);
  canvas.drawLine(cx - 13, cy - 2, cx - 7, cy - 8, color);
  canvas.drawLine(cx - 7, cy - 8, cx + 7, cy - 8, color);
  canvas.drawLine(cx + 7, cy - 8, cx + 13, cy - 2, color);
}

template <typename Canvas>
inline void drawMaze(Canvas& canvas, int16_t cx, int16_t cy, uint16_t color,
                     bool collector) {
  const int16_t x = cx - 13;
  const int16_t y = cy - 12;
  canvas.drawRect(x, y, 26, 24, color);
  canvas.drawFastVLine(x + 7, y, 16, color);
  canvas.drawFastHLine(x + 7, y + 8, 12, color);
  canvas.drawFastVLine(x + 18, y + 8, 16, color);
  canvas.drawFastHLine(x + 12, y + 18, 7, color);
  if (collector) canvas.fillCircle(x + 12, y + 13, 2, TFT_YELLOW);
  canvas.fillTriangle(x + 22, y + 19, x + 22, y + 23, x + 26, y + 21,
                      TFT_GREEN);
}

template <typename Canvas>
inline void drawCardPair(Canvas& canvas, int16_t cx, int16_t cy,
                         uint16_t color) {
  canvas.fillRoundRect(cx - 12, cy - 12, 16, 23, 2, TFT_WHITE);
  canvas.drawRoundRect(cx - 12, cy - 12, 16, 23, 2, color);
  canvas.fillRoundRect(cx - 1, cy - 9, 16, 23, 2, TFT_LIGHTGREY);
  canvas.drawRoundRect(cx - 1, cy - 9, 16, 23, 2, color);
  canvas.fillCircle(cx + 7, cy, 3, TFT_RED);
}

template <typename Canvas>
inline void draw(Canvas& canvas, Icon icon, int16_t cx, int16_t cy,
                 uint16_t color) {
  canvas.setTextDatum(TL_DATUM);
  switch (icon) {
    case Icon::Games:
      canvas.fillRoundRect(cx - 14, cy - 8, 28, 18, 6, color);
      canvas.fillRect(cx - 8, cy - 3, 10, 3, TFT_BLACK);
      canvas.fillRect(cx - 5, cy - 6, 3, 10, TFT_BLACK);
      canvas.fillCircle(cx + 7, cy - 2, 2, TFT_BLACK);
      canvas.fillCircle(cx + 11, cy + 3, 2, TFT_BLACK);
      break;
    case Icon::Apps:
      for (uint8_t row = 0; row < 2; ++row) {
        for (uint8_t col = 0; col < 2; ++col) {
          canvas.fillRoundRect(cx - 12 + col * 14, cy - 12 + row * 14,
                               10, 10, 2, color);
        }
      }
      break;
    case Icon::Settings:
      canvas.drawCircle(cx, cy, 9, color);
      canvas.fillCircle(cx, cy, 3, color);
      canvas.drawFastHLine(cx - 14, cy, 28, color);
      canvas.drawFastVLine(cx, cy - 14, 28, color);
      canvas.drawLine(cx - 10, cy - 10, cx + 10, cy + 10, color);
      canvas.drawLine(cx + 10, cy - 10, cx - 10, cy + 10, color);
      break;
    case Icon::About:
      canvas.drawCircle(cx, cy, 13, color);
      canvas.fillCircle(cx, cy - 7, 2, color);
      canvas.fillRoundRect(cx - 2, cy - 2, 4, 11, 2, color);
      break;
    case Icon::AlienRaiders:
      canvas.fillTriangle(cx, cy - 14, cx - 11, cy + 10, cx + 11, cy + 10,
                          color);
      canvas.fillRect(cx - 3, cy + 10, 6, 5, TFT_ORANGE);
      canvas.fillCircle(cx, cy + 1, 3, TFT_BLACK);
      break;
    case Icon::Breakout:
      for (uint8_t row = 0; row < 2; ++row) {
        for (uint8_t col = 0; col < 3; ++col) {
          canvas.fillRect(cx - 14 + col * 10, cy - 13 + row * 6, 8, 4,
                          color);
        }
      }
      canvas.fillCircle(cx + 7, cy + 3, 3, TFT_WHITE);
      canvas.fillRoundRect(cx - 11, cy + 11, 22, 4, 2, color);
      break;
    case Icon::CaveChopper:
      canvas.fillRoundRect(cx - 9, cy - 5, 18, 11, 4, color);
      canvas.drawLine(cx + 8, cy, cx + 15, cy + 6, color);
      canvas.drawFastVLine(cx + 15, cy + 2, 8, color);
      canvas.drawFastHLine(cx - 13, cy - 10, 27, color);
      canvas.drawFastVLine(cx, cy - 10, 5, color);
      break;
    case Icon::CityRacer:
      canvas.fillRoundRect(cx - 10, cy - 13, 20, 26, 4, color);
      canvas.fillRect(cx - 6, cy - 8, 12, 6, TFT_SKYBLUE);
      canvas.fillRect(cx - 14, cy - 8, 4, 7, TFT_DARKGREY);
      canvas.fillRect(cx + 10, cy - 8, 4, 7, TFT_DARKGREY);
      canvas.fillRect(cx - 14, cy + 5, 4, 7, TFT_DARKGREY);
      canvas.fillRect(cx + 10, cy + 5, 4, 7, TFT_DARKGREY);
      break;
    case Icon::Casino:
      canvas.fillCircle(cx, cy, 14, TFT_RED);
      canvas.drawCircle(cx, cy, 13, TFT_WHITE);
      canvas.fillCircle(cx, cy, 9, TFT_BLACK);
      canvas.drawCircle(cx, cy, 7, TFT_GOLD);
      canvas.fillCircle(cx, cy, 3, TFT_GOLD);
      canvas.fillRect(cx - 2, cy - 13, 4, 5, TFT_WHITE);
      canvas.fillRect(cx - 2, cy + 9, 4, 5, TFT_WHITE);
      canvas.fillRect(cx - 13, cy - 2, 5, 4, TFT_WHITE);
      canvas.fillRect(cx + 9, cy - 2, 5, 4, TFT_WHITE);
      break;
    case Icon::Field:
      canvas.fillCircle(cx, cy - 10, 4, color);
      canvas.drawLine(cx, cy - 6, cx - 2, cy + 3, color);
      canvas.drawLine(cx - 2, cy - 1, cx - 10, cy + 4, color);
      canvas.drawLine(cx - 1, cy - 1, cx + 8, cy - 3, color);
      canvas.drawLine(cx - 2, cy + 3, cx - 10, cy + 13, color);
      canvas.drawLine(cx - 2, cy + 3, cx + 9, cy + 10, color);
      break;
    case Icon::Fishing:
      canvas.fillEllipse(cx - 2, cy, 12, 7, color);
      canvas.fillTriangle(cx + 8, cy, cx + 15, cy - 8, cx + 15, cy + 8,
                          color);
      canvas.fillCircle(cx - 8, cy - 2, 1, TFT_BLACK);
      break;
    case Icon::Knife:
      canvas.fillTriangle(cx - 11, cy + 12, cx + 10, cy - 13,
                          cx + 4, cy + 7, color);
      canvas.drawLine(cx - 12, cy + 10, cx - 6, cy + 15, TFT_ORANGE);
      canvas.drawLine(cx - 10, cy + 8, cx - 4, cy + 13, TFT_ORANGE);
      break;
    case Icon::MazeCollector:
      drawMaze(canvas, cx, cy, color, true);
      break;
    case Icon::MazeRunner:
      drawMaze(canvas, cx, cy, color, false);
      break;
    case Icon::Lander:
      canvas.fillTriangle(cx, cy - 14, cx - 9, cy + 6, cx + 9, cy + 6,
                          color);
      canvas.drawLine(cx - 8, cy + 5, cx - 13, cy + 12, color);
      canvas.drawLine(cx + 8, cy + 5, cx + 13, cy + 12, color);
      canvas.fillTriangle(cx - 4, cy + 7, cx + 4, cy + 7, cx, cy + 15,
                          TFT_ORANGE);
      break;
    case Icon::Speed:
      canvas.drawCircle(cx, cy + 2, 13, color);
      canvas.drawFastHLine(cx - 13, cy + 9, 26, TFT_NAVY);
      canvas.drawLine(cx, cy + 2, cx + 8, cy - 7, TFT_YELLOW);
      canvas.fillCircle(cx, cy + 2, 2, color);
      break;
    case Icon::Shooter:
      canvas.drawCircle(cx, cy, 12, color);
      canvas.drawCircle(cx, cy, 4, color);
      canvas.drawFastHLine(cx - 15, cy, 30, color);
      canvas.drawFastVLine(cx, cy - 15, 30, color);
      break;
    case Icon::Pet:
      canvas.fillCircle(cx, cy + 2, 11, color);
      canvas.fillTriangle(cx - 10, cy - 6, cx - 11, cy - 15, cx - 3,
                          cy - 9, color);
      canvas.fillTriangle(cx + 10, cy - 6, cx + 11, cy - 15, cx + 3,
                          cy - 9, color);
      canvas.fillCircle(cx - 4, cy, 2, TFT_BLACK);
      canvas.fillCircle(cx + 4, cy, 2, TFT_BLACK);
      canvas.drawFastHLine(cx - 3, cy + 7, 6, TFT_BLACK);
      break;
    case Icon::Pipe:
      canvas.drawFastVLine(cx - 8, cy - 14, 20, color);
      canvas.drawFastHLine(cx - 8, cy + 5, 17, color);
      canvas.drawFastVLine(cx + 8, cy + 5, 10, color);
      canvas.drawFastVLine(cx - 4, cy - 14, 15, color);
      canvas.drawFastHLine(cx - 4, cy + 1, 17, color);
      canvas.drawFastVLine(cx + 12, cy + 1, 14, color);
      break;
    case Icon::Reactor:
      canvas.drawCircle(cx, cy, 4, color);
      canvas.fillTriangle(cx, cy - 6, cx - 6, cy - 14, cx + 6, cy - 14,
                          color);
      canvas.fillTriangle(cx + 6, cy + 3, cx + 14, cy + 9, cx + 8, cy + 14,
                          color);
      canvas.fillTriangle(cx - 6, cy + 3, cx - 14, cy + 9, cx - 8,
                          cy + 14, color);
      break;
    case Icon::Simon:
      canvas.fillCircle(cx - 7, cy - 7, 6, TFT_RED);
      canvas.fillCircle(cx + 7, cy - 7, 6, TFT_GREEN);
      canvas.fillCircle(cx - 7, cy + 7, 6, TFT_BLUE);
      canvas.fillCircle(cx + 7, cy + 7, 6, TFT_YELLOW);
      break;
    case Icon::Golf:
      canvas.drawFastVLine(cx + 4, cy - 14, 24, color);
      canvas.fillTriangle(cx + 5, cy - 13, cx + 15, cy - 8, cx + 5,
                          cy - 3, TFT_RED);
      canvas.fillCircle(cx - 8, cy + 10, 4, TFT_WHITE);
      canvas.drawEllipse(cx + 3, cy + 11, 13, 4, color);
      break;
    case Icon::Cards:
      drawCardPair(canvas, cx, cy, color);
      break;
    case Icon::Tower:
      canvas.fillRect(cx - 12, cy + 7, 24, 6, color);
      canvas.fillRect(cx - 9, cy, 19, 6, color);
      canvas.fillRect(cx - 6, cy - 7, 15, 6, color);
      canvas.fillRect(cx - 3, cy - 14, 10, 6, color);
      break;
    case Icon::Clock:
      canvas.drawCircle(cx, cy, 13, color);
      canvas.drawLine(cx, cy, cx, cy - 9, color);
      canvas.drawLine(cx, cy, cx + 7, cy + 5, color);
      canvas.fillCircle(cx, cy, 2, color);
      break;
    case Icon::Stopwatch:
      canvas.drawCircle(cx, cy + 2, 12, color);
      canvas.drawFastHLine(cx - 4, cy - 14, 8, color);
      canvas.drawFastVLine(cx, cy - 14, 4, color);
      canvas.drawLine(cx, cy + 2, cx + 7, cy - 5, color);
      break;
    case Icon::Countdown:
      canvas.drawFastHLine(cx - 10, cy - 14, 20, color);
      canvas.drawFastHLine(cx - 10, cy + 14, 20, color);
      canvas.drawLine(cx - 9, cy - 13, cx + 8, cy + 13, color);
      canvas.drawLine(cx + 9, cy - 13, cx - 8, cy + 13, color);
      canvas.fillTriangle(cx - 7, cy - 10, cx + 7, cy - 10, cx, cy, color);
      canvas.fillTriangle(cx - 7, cy + 11, cx + 7, cy + 11, cx, cy + 1,
                          color);
      break;
    case Icon::Counter:
      canvas.drawFastHLine(cx - 13, cy - 7, 10, color);
      canvas.drawFastVLine(cx - 8, cy - 12, 10, color);
      canvas.drawFastHLine(cx + 3, cy + 7, 10, color);
      break;
    case Icon::Dice:
      canvas.drawRoundRect(cx - 13, cy - 13, 26, 26, 4, color);
      canvas.fillCircle(cx - 7, cy - 7, 2, color);
      canvas.fillCircle(cx + 7, cy - 7, 2, color);
      canvas.fillCircle(cx, cy, 2, color);
      canvas.fillCircle(cx - 7, cy + 7, 2, color);
      canvas.fillCircle(cx + 7, cy + 7, 2, color);
      break;
    case Icon::Coin:
      canvas.fillCircle(cx, cy, 13, TFT_GOLD);
      canvas.drawCircle(cx, cy, 10, color);
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_DARKGREY, TFT_GOLD);
      canvas.drawString("1", cx - 5, cy - 7);
      break;
    case Icon::Random:
      canvas.drawLine(cx - 13, cy - 8, cx - 5, cy - 8, color);
      canvas.drawLine(cx - 5, cy - 8, cx + 6, cy + 8, color);
      canvas.drawLine(cx + 6, cy + 8, cx + 13, cy + 8, color);
      canvas.fillTriangle(cx + 13, cy + 8, cx + 7, cy + 3, cx + 7,
                          cy + 13, color);
      canvas.drawLine(cx - 13, cy + 8, cx - 5, cy + 8, color);
      canvas.drawLine(cx - 5, cy + 8, cx + 6, cy - 8, color);
      canvas.drawLine(cx + 6, cy - 8, cx + 13, cy - 8, color);
      break;
    case Icon::Screensaver:
      for (uint8_t i = 0; i < 8; ++i) {
        const float a = i * 0.785398f;
        canvas.drawLine(cx, cy, cx + static_cast<int16_t>(cosf(a) * 14),
                        cy + static_cast<int16_t>(sinf(a) * 14), color);
      }
      canvas.fillCircle(cx, cy, 4, TFT_WHITE);
      break;
    case Icon::Metronome:
      canvas.drawTriangle(cx - 11, cy + 13, cx + 11, cy + 13, cx, cy - 14,
                          color);
      canvas.drawLine(cx, cy - 9, cx + 8, cy + 8, TFT_YELLOW);
      canvas.fillCircle(cx + 8, cy + 8, 2, TFT_YELLOW);
      break;
    case Icon::Bitcoin:
      canvas.drawCircle(cx, cy, 13, TFT_ORANGE);
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_ORANGE, TFT_NAVY);
      canvas.drawString("B", cx - 6, cy - 7);
      canvas.drawFastVLine(cx - 3, cy - 11, 22, TFT_ORANGE);
      break;
    case Icon::Cluster:
      canvas.fillCircle(cx, cy - 10, 5, color);
      canvas.fillCircle(cx - 11, cy + 9, 5, color);
      canvas.fillCircle(cx + 11, cy + 9, 5, color);
      canvas.drawLine(cx, cy - 5, cx - 8, cy + 5, color);
      canvas.drawLine(cx, cy - 5, cx + 8, cy + 5, color);
      canvas.drawFastHLine(cx - 6, cy + 9, 12, color);
      break;
    case Icon::Contacts:
      canvas.fillCircle(cx - 7, cy - 6, 6, color);
      canvas.fillCircle(cx + 8, cy - 3, 5, TFT_SKYBLUE);
      canvas.fillRoundRect(cx - 14, cy + 3, 15, 11, 5, color);
      canvas.fillRoundRect(cx + 2, cy + 4, 13, 10, 5, TFT_SKYBLUE);
      break;
    case Icon::Communicator:
      canvas.fillRoundRect(cx - 14, cy - 11, 28, 20, 5, color);
      canvas.fillTriangle(cx - 7, cy + 7, cx - 12, cy + 15, cx - 2,
                          cy + 8, color);
      canvas.fillCircle(cx - 7, cy - 1, 2, TFT_BLACK);
      canvas.fillCircle(cx, cy - 1, 2, TFT_BLACK);
      canvas.fillCircle(cx + 7, cy - 1, 2, TFT_BLACK);
      break;
    case Icon::Mouse:
      canvas.drawRoundRect(cx - 10, cy - 14, 20, 28, 9, color);
      canvas.drawFastVLine(cx, cy - 13, 10, color);
      canvas.drawFastHLine(cx - 9, cy - 4, 18, color);
      canvas.fillRoundRect(cx - 2, cy - 11, 4, 6, 2, color);
      break;
    case Icon::Reading:
      canvas.drawRect(cx - 14, cy - 11, 13, 23, color);
      canvas.drawRect(cx + 1, cy - 11, 13, 23, color);
      canvas.drawFastVLine(cx, cy - 9, 22, color);
      canvas.drawLine(cx - 12, cy - 8, cx - 4, cy - 6, color);
      canvas.drawLine(cx + 12, cy - 8, cx + 4, cy - 6, color);
      break;
    case Icon::Device:
      canvas.drawRoundRect(cx - 11, cy - 13, 22, 26, 3, color);
      canvas.fillRect(cx - 7, cy - 9, 14, 15, TFT_DARKGREY);
      canvas.fillCircle(cx, cy + 9, 2, color);
      break;
    case Icon::Options:
      canvas.drawFastHLine(cx - 14, cy - 9, 28, color);
      canvas.drawFastHLine(cx - 14, cy, 28, color);
      canvas.drawFastHLine(cx - 14, cy + 9, 28, color);
      canvas.fillCircle(cx - 5, cy - 9, 3, color);
      canvas.fillCircle(cx + 7, cy, 3, color);
      canvas.fillCircle(cx - 1, cy + 9, 3, color);
      break;
    case Icon::Autolaunch:
      canvas.fillTriangle(cx + 12, cy - 12, cx - 7, cy - 5, cx + 5,
                          cy + 7, color);
      canvas.drawLine(cx - 7, cy - 5, cx - 14, cy + 13, TFT_ORANGE);
      canvas.drawLine(cx + 5, cy + 7, cx - 14, cy + 13, TFT_ORANGE);
      break;
    case Icon::Wifi:
      drawWifi(canvas, cx, cy, color);
      break;
  }
}

}  // namespace CydIcons
