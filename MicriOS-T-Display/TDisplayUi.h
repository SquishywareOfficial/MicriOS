#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace TDisplayUi {

constexpr uint16_t W = 240;
constexpr uint16_t H = 135;
constexpr uint16_t PAD = 10;
constexpr uint16_t HEADER_H = 28;
constexpr uint16_t FOOTER_H = 18;

enum class TextSize : uint8_t {
  Compact = 0,
  Large = 1
};

struct MenuStyle {
  uint8_t visibleRows;
  uint8_t rowHeight;
  uint8_t rowTextSize;
  uint8_t titleTextSize;
  uint8_t footerTextSize;
  int16_t rowY;
  int16_t rowLeft;
  int16_t rowRight;
};

TextSize loadTextSize();
void saveTextSize(TextSize size);
TextSize nextTextSize(TextSize size);
const char* textSizeLabel(TextSize size);

inline MenuStyle menuStyle(TextSize size) {
  if (size == TextSize::Large) {
    return {3, 22, 2, 2, 1, 39, 14, static_cast<int16_t>(W - 14)};
  }
  return {6, 13, 1, 2, 1, 42, 12, static_cast<int16_t>(W - 12)};
}

template <typename Canvas>
inline String fitText(Canvas& tft, const String& text, int maxWidth) {
  if (tft.textWidth(text) <= maxWidth) {
    return text;
  }

  String clipped = text;
  while (clipped.length() > 3 && tft.textWidth(clipped + "...") > maxWidth) {
    clipped.remove(clipped.length() - 1);
  }
  return clipped + "...";
}

template <typename Canvas>
inline void clear(Canvas& tft) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
}

inline uint8_t menuScrollOffset(uint8_t selected, uint8_t count, const MenuStyle& style) {
  if (count <= style.visibleRows) {
    return 0;
  }
  const uint8_t maxOffset = count - style.visibleRows;
  const uint8_t guardedRows = style.visibleRows > 1 ? style.visibleRows - 1 : style.visibleRows;
  if (selected >= guardedRows) {
    const uint8_t desired = selected - guardedRows + 1;
    return desired > maxOffset ? maxOffset : desired;
  }
  return 0;
}

template <typename Canvas>
inline void menuCounter(Canvas& tft, uint8_t index, uint8_t count) {
  if (count == 0) {
    return;
  }
  char counter[10];
  snprintf(counter, sizeof(counter), "%u/%u", index + 1, count);
  tft.fillRect(W - 64, 5, 52, 20, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(counter, W - tft.textWidth(counter) - 12, 10);
}

template <typename Canvas>
inline void menuShell(Canvas& tft, const char* title, uint8_t index, uint8_t count, TextSize size) {
  const MenuStyle style = menuStyle(size);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextSize(style.titleTextSize);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(fitText(tft, title, count > 0 ? 158 : 210), style.rowLeft, 8);
  menuCounter(tft, index, count);
  tft.drawFastHLine(style.rowLeft, 33, style.rowRight - style.rowLeft, TFT_DARKGREY);
  tft.drawRect(0, 0, W, H, TFT_DARKGREY);
}

template <typename Canvas>
inline void menuFooter(Canvas& tft, const char* text, TextSize size) {
  const MenuStyle style = menuStyle(size);
  tft.fillRect(0, H - 16, W, 16, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(style.footerTextSize);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(fitText(tft, text, W - style.rowLeft * 2), style.rowLeft, H - 12);
}

template <typename Canvas>
inline void menuRow(Canvas& tft, uint8_t row, const char* label, bool selected, TextSize size, uint16_t accent = TFT_GREEN) {
  const MenuStyle style = menuStyle(size);
  const int16_t y = style.rowY + row * style.rowHeight;
  const int16_t rowW = style.rowRight - style.rowLeft + 8;
  const int16_t x = style.rowLeft - 4;
  const uint16_t bg = selected ? accent : TFT_BLACK;
  const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;

  tft.fillRect(x, y - 1, rowW, style.rowHeight, bg);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(style.rowTextSize);
  tft.setTextColor(fg, bg);

  const int16_t markerW = size == TextSize::Large ? 18 : 14;
  tft.drawString(selected ? ">" : " ", style.rowLeft, y + (size == TextSize::Large ? 4 : 0));
  tft.drawString(fitText(tft, label, style.rowRight - style.rowLeft - markerW),
                 style.rowLeft + markerW, y + (size == TextSize::Large ? 4 : 0));
}

template <typename Canvas, typename LabelGetter>
inline void menuFrame(Canvas& tft,
                      const char* title,
                      uint8_t selected,
                      uint8_t count,
                      uint8_t scrollOffset,
                      LabelGetter labelFor,
                      const char* footerText,
                      TextSize size,
                      uint16_t accent = TFT_GREEN) {
  const MenuStyle style = menuStyle(size);
  menuShell(tft, title, selected, count, size);
  for (uint8_t row = 0; row < style.visibleRows; row++) {
    const uint8_t index = scrollOffset + row;
    if (index >= count) {
      break;
    }
    menuRow(tft, row, labelFor(index), index == selected, size, accent);
  }
  menuFooter(tft, footerText, size);
}

template <typename Canvas>
inline void header(Canvas& tft, const char* title, uint16_t accent, const char* right = nullptr) {
  tft.fillRect(0, 0, W, HEADER_H, TFT_BLACK);
  tft.drawFastHLine(0, HEADER_H - 1, W, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(accent, TFT_BLACK);
  tft.drawString(fitText(tft, title, right ? 158 : 210), PAD, 7);

  if (right != nullptr) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    String label = fitText(tft, right, 65);
    tft.drawString(label, W - PAD - tft.textWidth(label), 11);
  }
}

template <typename Canvas>
inline void footer(Canvas& tft, const char* text) {
  tft.fillRect(0, H - FOOTER_H, W, FOOTER_H, TFT_BLACK);
  tft.drawFastHLine(0, H - FOOTER_H, W, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(fitText(tft, text, W - PAD * 2), PAD, H - 13);
}

template <typename Canvas>
inline void centered(Canvas& tft, const String& text, int y, uint8_t size, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(size);
  tft.setTextColor(color, TFT_BLACK);
  String label = fitText(tft, text, W - PAD * 2);
  tft.drawString(label, (W - tft.textWidth(label)) / 2, y);
}

template <typename Canvas>
inline void largeValue(Canvas& tft, const String& text, int y, uint16_t color) {
  uint8_t size = 5;
  tft.setTextDatum(TL_DATUM);
  do {
    tft.setTextSize(size);
    if (tft.textWidth(text) <= W - PAD * 2 || size <= 2) {
      break;
    }
    size--;
  } while (true);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, (W - tft.textWidth(text)) / 2, y);
}

template <typename Canvas>
inline void row(Canvas& tft, int y, const char* label, bool selected, uint16_t accent = TFT_CYAN) {
  const uint16_t bg = selected ? accent : TFT_BLACK;
  const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
  tft.fillRect(PAD, y, W - PAD * 2, 20, bg);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(fg, bg);
  tft.drawString(fitText(tft, label, W - PAD * 2 - 10), PAD + 5, y + 3);
}

template <typename Canvas>
inline void labelValue(Canvas& tft, int y, const char* label, const String& value, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(label, PAD, y);
  tft.setTextSize(2);
  tft.setTextColor(color, TFT_BLACK);
  String clipped = fitText(tft, value, W - 95);
  tft.drawString(clipped, 92, y - 4);
}

template <typename Canvas>
inline void pill(Canvas& tft, int x, int y, const char* label, uint16_t color) {
  String text(label);
  tft.setTextSize(1);
  int w = tft.textWidth(text) + 12;
  tft.fillRoundRect(x, y, w, 15, 4, color);
  tft.setTextColor(TFT_BLACK, color);
  tft.drawString(text, x + 6, y + 4);
}

template <typename Canvas>
inline void bar(Canvas& tft, int x, int y, int w, int h, float ratio, uint16_t color) {
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  tft.drawRect(x, y, w, h, TFT_DARKGREY);
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_BLACK);
  tft.fillRect(x + 1, y + 1, static_cast<int>((w - 2) * ratio), h - 2, color);
}

}
