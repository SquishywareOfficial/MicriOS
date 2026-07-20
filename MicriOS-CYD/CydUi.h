#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "CydIcons.h"
#include "src/shared/logic/SystemControls.h"
#include "src/shared/logic/TouchUi.h"

namespace CydUi {

constexpr int16_t W = 320;
constexpr int16_t H = 208;
constexpr int16_t SCREEN_H = 240;
constexpr int16_t SYSTEM_BAR_H = SCREEN_H - H;
constexpr int16_t PAD = 12;
constexpr int16_t HEADER_H = 36;
constexpr int16_t FOOTER_H = 24;
constexpr uint16_t SURFACE = 0x18E3;
constexpr uint16_t SURFACE_RAISED = 0x2124;
constexpr uint16_t SURFACE_BORDER = 0x3186;

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
    return {4, 35, 2, 2, 1, 40, 14, static_cast<int16_t>(W - 14)};
  }
  return {6, 24, 2, 2, 1, 40, 12, static_cast<int16_t>(W - 12)};
}

inline TouchUi::Rect systemBarRect() {
  return {0, H, W, SYSTEM_BAR_H};
}

inline TouchUi::Rect backRect() {
  return {5, static_cast<int16_t>(H + 4), 56,
          static_cast<int16_t>(SYSTEM_BAR_H - 8)};
}

inline TouchUi::Rect menuPreviousPageRect() {
  return {218, 4, 34, 28};
}

inline TouchUi::Rect menuNextPageRect() {
  return {284, 4, 32, 28};
}

inline TouchUi::Rect systemControlRect(SystemControls::Control control) {
  switch (control) {
    case SystemControls::Control::Brightness:
      return {226, static_cast<int16_t>(H + 4), 28, 24};
    case SystemControls::Control::Volume:
      return {257, static_cast<int16_t>(H + 4), 28, 24};
    case SystemControls::Control::Battery:
      return {288, static_cast<int16_t>(H + 4), 28, 24};
    default:
      return {};
  }
}

inline TouchUi::Rect systemControlTrackRect() {
  return {40, static_cast<int16_t>(H + 9), 238, 14};
}

inline TouchUi::Rect systemControlCloseRect() {
  return {286, static_cast<int16_t>(H + 4), 30, 24};
}

inline TouchUi::GridMetrics rootGrid() {
  return {{14, 45, 292, 151}, 10, 10, 2, 2};
}

inline TouchUi::GridMetrics submenuGrid() {
  return {{10, 40, 300, 158}, 8, 7, 2, 3};
}

template <typename Canvas>
inline String fitText(Canvas& canvas, const String& text, int maxWidth) {
  if (canvas.textWidth(text) <= maxWidth) return text;
  String clipped = text;
  while (clipped.length() > 3 && canvas.textWidth(clipped + "...") > maxWidth) {
    clipped.remove(clipped.length() - 1);
  }
  return clipped + "...";
}

template <typename Canvas>
inline void clear(Canvas& canvas) {
  canvas.fillRect(0, 0, W, H, TFT_BLACK);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
}

inline uint8_t menuScrollOffset(uint8_t selected, uint8_t count,
                                const MenuStyle& style) {
  if (count <= style.visibleRows) return 0;
  const uint8_t maxOffset = count - style.visibleRows;
  const uint8_t guardedRows = style.visibleRows > 1 ? style.visibleRows - 1 : 1;
  if (selected >= guardedRows) {
    const uint8_t desired = selected - guardedRows + 1;
    return desired > maxOffset ? maxOffset : desired;
  }
  return 0;
}

template <typename Canvas>
inline void menuCounter(Canvas& canvas, uint8_t index, uint8_t count) {
  if (count == 0) return;
  char counter[12];
  snprintf(counter, sizeof(counter), "%u/%u", index + 1, count);
  canvas.fillRect(W - 72, 4, 60, 25, TFT_BLACK);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(counter, W - canvas.textWidth(counter) - 14, 13);
}

template <typename Canvas>
inline void menuShell(Canvas& canvas, const char* title, uint8_t index,
                      uint8_t count, TextSize size) {
  const MenuStyle style = menuStyle(size);
  clear(canvas);
  canvas.setTextSize(style.titleTextSize);
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.drawString(fitText(canvas, title, count ? 210 : 285), style.rowLeft, 9);
  menuCounter(canvas, index, count);
  canvas.drawFastHLine(style.rowLeft, 34, style.rowRight - style.rowLeft,
                       TFT_DARKGREY);
}

template <typename Canvas>
inline void menuFooter(Canvas& canvas, const char* text, TextSize size) {
  const MenuStyle style = menuStyle(size);
  canvas.fillRect(0, H - FOOTER_H, W, FOOTER_H, TFT_BLACK);
  canvas.drawFastHLine(0, H - FOOTER_H, W, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(style.footerTextSize);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const String label = fitText(canvas, text, W - PAD * 2);
  canvas.drawString(label, PAD, H - 16);
}

template <typename Canvas>
inline void menuRow(Canvas& canvas, uint8_t row, const char* label,
                    bool selected, TextSize size,
                    uint16_t accent = TFT_GREEN) {
  const MenuStyle style = menuStyle(size);
  const int16_t y = style.rowY + row * style.rowHeight;
  const int16_t x = style.rowLeft - 4;
  const int16_t rowW = style.rowRight - style.rowLeft + 8;
  const uint16_t bg = selected ? accent : TFT_BLACK;
  const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
  canvas.fillRoundRect(x, y, rowW, style.rowHeight - 3, 5, bg);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(style.rowTextSize);
  canvas.setTextColor(fg, bg);
  canvas.drawString(fitText(canvas, label, rowW - 20), style.rowLeft + 7,
                    y + (style.rowHeight - 16) / 2);
}

template <typename Canvas, typename LabelGetter>
inline void menuFrame(Canvas& canvas, const char* title, uint8_t selected,
                      uint8_t count, uint8_t scrollOffset,
                      LabelGetter labelFor, const char* footerText,
                      TextSize size, uint16_t accent = TFT_GREEN) {
  const MenuStyle style = menuStyle(size);
  menuShell(canvas, title, selected, count, size);
  for (uint8_t row = 0; row < style.visibleRows; ++row) {
    const uint8_t index = scrollOffset + row;
    if (index >= count) break;
    menuRow(canvas, row, labelFor(index), index == selected, size, accent);
  }
  menuFooter(canvas, footerText, size);
}

template <typename Canvas>
inline void header(Canvas& canvas, const char* title, uint16_t accent,
                   const char* right = nullptr) {
  canvas.fillRect(0, 0, W, HEADER_H, TFT_BLACK);
  canvas.drawFastHLine(0, HEADER_H - 1, W, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(accent, TFT_BLACK);
  canvas.drawString(fitText(canvas, title, right ? 218 : 292), PAD, 9);
  if (right != nullptr) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    const String label = fitText(canvas, right, 88);
    canvas.drawString(label, W - PAD - canvas.textWidth(label), 14);
  }
}

template <typename Canvas>
inline void footer(Canvas& canvas, const char* text) {
  canvas.fillRect(0, H - FOOTER_H, W, FOOTER_H, TFT_BLACK);
  canvas.drawFastHLine(0, H - FOOTER_H, W, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(fitText(canvas, text, W - PAD * 2), PAD, H - 16);
}

template <typename Canvas>
inline void centered(Canvas& canvas, const String& text, int y, uint8_t size,
                     uint16_t color) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(size);
  canvas.setTextColor(color, TFT_BLACK);
  const String label = fitText(canvas, text, W - PAD * 2);
  canvas.drawString(label, (W - canvas.textWidth(label)) / 2, y);
}

template <typename Canvas>
inline void largeValue(Canvas& canvas, const String& text, int y,
                       uint16_t color) {
  uint8_t size = 6;
  canvas.setTextDatum(TL_DATUM);
  do {
    canvas.setTextSize(size);
    if (canvas.textWidth(text) <= W - PAD * 2 || size <= 2) break;
    --size;
  } while (true);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(text, (W - canvas.textWidth(text)) / 2, y);
}

template <typename Canvas>
inline void row(Canvas& canvas, int y, const char* label, bool selected,
                uint16_t accent = TFT_CYAN) {
  const uint16_t bg = selected ? accent : TFT_BLACK;
  const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
  canvas.fillRoundRect(PAD, y, W - PAD * 2, 30, 5, bg);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(fg, bg);
  canvas.drawString(fitText(canvas, label, W - PAD * 2 - 16), PAD + 8, y + 7);
}

template <typename Canvas>
inline void labelValue(Canvas& canvas, int y, const char* label,
                       const String& value, uint16_t color) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(label, PAD, y);
  canvas.setTextColor(color, TFT_BLACK);
  const String clipped = fitText(canvas, value, W - 132);
  canvas.drawString(clipped, 126, y);
}

template <typename Canvas>
inline void pill(Canvas& canvas, int x, int y, const char* label,
                 uint16_t color) {
  const String text(label);
  canvas.setTextSize(1);
  const int w = canvas.textWidth(text) + 14;
  canvas.fillRoundRect(x, y, w, 18, 5, color);
  canvas.setTextColor(TFT_BLACK, color);
  canvas.drawString(text, x + 7, y + 5);
}

template <typename Canvas>
inline void bar(Canvas& canvas, int x, int y, int w, int h, float ratio,
                uint16_t color) {
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  canvas.drawRect(x, y, w, h, TFT_DARKGREY);
  canvas.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_BLACK);
  canvas.fillRect(x + 1, y + 1, static_cast<int>((w - 2) * ratio), h - 2,
                  color);
}

template <typename Canvas>
inline void touchCard(Canvas& canvas, const TouchUi::Rect& rect,
                      const char* label, CydIcons::Icon icon,
                      uint16_t accent) {
  const uint16_t background = SURFACE;
  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 8, background);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 8, SURFACE_BORDER);
  canvas.setTextDatum(TL_DATUM);
  const String text(label);

  if (rect.h >= 64) {
    canvas.fillCircle(rect.x + rect.w / 2, rect.y + 24, 17,
                      SURFACE_RAISED);
    CydIcons::draw(canvas, icon, rect.x + rect.w / 2, rect.y + 24, accent);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE, background);
    const String fitted = fitText(canvas, text, rect.w - 16);
    canvas.drawString(fitted, rect.x + (rect.w - canvas.textWidth(fitted)) / 2,
                      rect.y + rect.h - 21);
    return;
  }

  const int16_t iconX = rect.x + 25;
  const int16_t textX = rect.x + 48;
  const int16_t textWidth = rect.w - 56;
  canvas.fillCircle(iconX, rect.y + rect.h / 2, 15, SURFACE_RAISED);
  CydIcons::draw(canvas, icon, iconX, rect.y + rect.h / 2, accent);
  canvas.setTextColor(TFT_WHITE, background);
  canvas.setTextSize(2);
  if (canvas.textWidth(text) <= textWidth) {
    canvas.drawString(text, textX, rect.y + (rect.h - 16) / 2);
    return;
  }

  canvas.setTextSize(1);
  if (canvas.textWidth(text) <= textWidth) {
    canvas.drawString(text, textX, rect.y + (rect.h - 8) / 2);
    return;
  }

  const int16_t split = text.indexOf(' ');
  if (split > 0) {
    const String first = fitText(canvas, text.substring(0, split), textWidth);
    const String second = fitText(canvas, text.substring(split + 1), textWidth);
    canvas.drawString(first, textX, rect.y + 11);
    canvas.drawString(second, textX, rect.y + 25);
  } else {
    canvas.drawString(fitText(canvas, text, textWidth), textX,
                      rect.y + (rect.h - 8) / 2);
  }
}

template <typename Canvas>
inline void touchAction(Canvas& canvas, const TouchUi::Rect& rect,
                        const char* label, uint16_t accent,
                        bool filled = true, bool pressed = false) {
  const uint16_t background = pressed ? TFT_DARKGREY
                                      : (filled ? accent : SURFACE_RAISED);
  const uint16_t foreground = filled ? TFT_BLACK : TFT_WHITE;
  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 8, background);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 8,
                       filled ? accent : SURFACE_BORDER);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(foreground, background);
  String text(label);
  if (canvas.textWidth(text) > rect.w - 14) {
    canvas.setTextSize(1);
  }
  text = fitText(canvas, text, rect.w - 14);
  canvas.drawString(text, rect.x + (rect.w - canvas.textWidth(text)) / 2,
                    rect.y + (rect.h - (canvas.textWidth("M") > 8 ? 16 : 8)) / 2);
}

template <typename Canvas>
inline void touchListRow(Canvas& canvas, const TouchUi::Rect& rect,
                         const char* label, bool selected,
                         uint16_t accent = TFT_CYAN,
                         const char* detail = nullptr,
                         bool pressed = false) {
  const uint16_t bg = pressed ? TFT_DARKGREY
                              : (selected ? SURFACE_RAISED : SURFACE);
  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 7, bg);
  if (selected) canvas.fillRoundRect(rect.x, rect.y, 4, rect.h, 2, accent);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE, bg);
  canvas.drawString(fitText(canvas, label, detail ? rect.w - 90 : rect.w - 22),
                    rect.x + 12, rect.y + (rect.h - 16) / 2);
  if (detail != nullptr) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, bg);
    String value = fitText(canvas, detail, 72);
    canvas.drawString(value, rect.x + rect.w - 10 - canvas.textWidth(value),
                      rect.y + (rect.h - 8) / 2);
  }
}

template <typename Canvas>
inline void touchToggle(Canvas& canvas, const TouchUi::Rect& rect,
                        const char* label, bool enabled,
                        bool pressed = false) {
  touchListRow(canvas, rect, label, enabled, enabled ? TFT_GREEN : TFT_DARKGREY,
               enabled ? "ON" : "OFF", pressed);
  const int16_t trackX = rect.x + rect.w - 57;
  const int16_t trackY = rect.y + (rect.h - 20) / 2;
  canvas.fillRoundRect(trackX, trackY, 45, 20, 10,
                       enabled ? TFT_GREEN : TFT_DARKGREY);
  canvas.fillCircle(enabled ? trackX + 34 : trackX + 11, trackY + 10, 8,
                    TFT_WHITE);
}

template <typename Canvas>
inline void touchTab(Canvas& canvas, const TouchUi::Rect& rect,
                     const char* label, bool selected, uint16_t accent,
                     bool pressed = false) {
  const uint16_t bg = pressed ? TFT_DARKGREY
                              : (selected ? SURFACE_RAISED : TFT_BLACK);
  canvas.fillRect(rect.x, rect.y, rect.w, rect.h, bg);
  if (selected) canvas.fillRect(rect.x, rect.y + rect.h - 3, rect.w, 3, accent);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, bg);
  String text = fitText(canvas, label, rect.w - 8);
  canvas.drawString(text, rect.x + (rect.w - canvas.textWidth(text)) / 2,
                    rect.y + (rect.h - 8) / 2);
}

template <typename Canvas>
inline void confirmation(Canvas& canvas, const char* title,
                         const String& message,
                         const TouchUi::Rect& cancelRect,
                         const TouchUi::Rect& confirmRect,
                         const char* confirmLabel = "Delete") {
  clear(canvas);
  header(canvas, title, TFT_RED);
  centered(canvas, message, 72, 2, TFT_WHITE);
  touchAction(canvas, cancelRect, "Cancel", TFT_LIGHTGREY, false);
  touchAction(canvas, confirmRect, confirmLabel, TFT_RED, true);
}

template <typename Canvas>
inline void systemButton(Canvas& canvas, const TouchUi::Rect& rect,
                         const char* label, uint16_t color) {
  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 5, color);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, color);
  const String text = fitText(canvas, label, rect.w - 10);
  canvas.drawString(text, rect.x + (rect.w - canvas.textWidth(text)) / 2,
                    rect.y + 7);
}

template <typename Canvas>
inline void drawSystemControlIcon(Canvas& canvas,
                                  SystemControls::Control control,
                                  int16_t cx, int16_t cy, uint16_t color) {
  if (control == SystemControls::Control::Brightness) {
    canvas.drawCircle(cx, cy, 4, color);
    canvas.fillCircle(cx, cy, 2, color);
    for (uint8_t i = 0; i < 8; ++i) {
      const float angle = i * 0.785398f;
      const int16_t x0 = cx + static_cast<int16_t>(cosf(angle) * 7);
      const int16_t y0 = cy + static_cast<int16_t>(sinf(angle) * 7);
      const int16_t x1 = cx + static_cast<int16_t>(cosf(angle) * 10);
      const int16_t y1 = cy + static_cast<int16_t>(sinf(angle) * 10);
      canvas.drawLine(x0, y0, x1, y1, color);
    }
    return;
  }

  if (control == SystemControls::Control::Volume) {
    canvas.drawCircle(cx + 1, cy, 7, color);
    canvas.drawCircle(cx + 1, cy, 10, color);
    canvas.fillRect(cx - 10, cy - 11, 11, 22, TFT_BLACK);
    canvas.fillRect(cx - 9, cy - 3, 5, 7, color);
    canvas.fillTriangle(cx - 4, cy - 3, cx + 2, cy - 8,
                        cx + 2, cy + 8, color);
    return;
  }

  if (control == SystemControls::Control::Battery) {
    canvas.drawRoundRect(cx - 10, cy - 7, 19, 14, 2, color);
    canvas.fillRect(cx + 9, cy - 3, 3, 6, color);
    canvas.fillRect(cx - 7, cy - 4, 10, 8, color);
  }
}

template <typename Canvas>
inline void menuPager(Canvas& canvas, uint16_t page, uint16_t pages) {
  if (pages <= 1) return;
  systemButton(canvas, menuPreviousPageRect(), "<", TFT_BLUE);
  systemButton(canvas, menuNextPageRect(), ">", TFT_BLUE);
  char pageText[10];
  snprintf(pageText, sizeof(pageText), "%u/%u", page + 1, pages);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.drawString(pageText, 268 - canvas.textWidth(pageText) / 2, 14);
}

template <typename Canvas>
inline void systemBar(Canvas& canvas, const char* title, bool showNavigation,
                      const char* navigationLabel,
                      const SystemControls::State& brightness,
                      const SystemControls::State& volume,
                      const SystemControls::State& battery) {
  canvas.fillRect(0, H, W, SYSTEM_BAR_H, TFT_BLACK);
  canvas.drawFastHLine(0, H, W, TFT_DARKGREY);
  if (showNavigation) {
    systemButton(canvas, backRect(), navigationLabel, TFT_DARKGREY);
  }

  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const int16_t titleX = showNavigation ? 69 : 10;
  const int16_t titleRight = 218;
  const String fitted = fitText(canvas, title, titleRight - titleX);
  canvas.drawString(fitted, titleX, H + 11);

  const SystemControls::Control controls[] = {
      SystemControls::Control::Brightness,
      SystemControls::Control::Volume,
      SystemControls::Control::Battery};
  const SystemControls::State states[] = {brightness, volume, battery};
  for (uint8_t i = 0; i < 3; ++i) {
    const auto rect = systemControlRect(controls[i]);
    const uint16_t color = states[i].implemented
                               ? (controls[i] == SystemControls::Control::Brightness
                                      ? TFT_YELLOW
                                      : TFT_CYAN)
                               : TFT_DARKGREY;
    drawSystemControlIcon(canvas, controls[i], rect.x + rect.w / 2,
                          rect.y + rect.h / 2, color);
  }
}

template <typename Canvas>
inline void immersiveExitBar(Canvas& canvas) {
  canvas.fillRect(0, H, W, SYSTEM_BAR_H, TFT_BLACK);
  canvas.drawFastHLine(0, H, W, TFT_DARKGREY);
  const TouchUi::Rect exitRect = {8, static_cast<int16_t>(H + 4), 76, 24};
  systemButton(canvas, exitRect, "Exit", TFT_RED);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Tap outside to hide", 98, H + 11);
}

inline TouchUi::Rect immersiveExitRect() {
  return {8, static_cast<int16_t>(H + 4), 76, 24};
}

template <typename Canvas>
inline void systemControlTrack(Canvas& canvas,
                               const SystemControls::State& state) {
  const auto track = systemControlTrackRect();
  canvas.fillRect(track.x - 6, track.y - 6, track.w + 12, track.h + 12,
                  TFT_BLACK);
  if (state.implemented && state.adjustable && state.percent >= 0) {
    const uint8_t percent = SystemControls::clampPercent(state.percent);
    const int16_t fillWidth =
        static_cast<int16_t>((track.w - 2) * percent / 100);
    canvas.drawRoundRect(track.x, track.y, track.w, track.h, 4, TFT_LIGHTGREY);
    canvas.fillRoundRect(track.x + 1, track.y + 1, track.w - 2,
                         track.h - 2, 3, TFT_BLACK);
    if (fillWidth > 0) {
      canvas.fillRoundRect(track.x + 1, track.y + 1, fillWidth,
                           track.h - 2, 3, TFT_YELLOW);
    }
    const int16_t knobX = track.x + 1 +
        static_cast<int16_t>((track.w - 3) * percent / 100);
    canvas.fillCircle(knobX, track.y + track.h / 2, 5, TFT_WHITE);
  }
}

template <typename Canvas>
inline void systemControlOverlay(Canvas& canvas,
                                 SystemControls::Control control,
                                 const SystemControls::State& state) {
  canvas.fillRect(0, H, W, SYSTEM_BAR_H, TFT_BLACK);
  canvas.drawFastHLine(0, H, W, TFT_DARKGREY);
  drawSystemControlIcon(canvas, control, 19, H + SYSTEM_BAR_H / 2,
                        state.implemented ? TFT_YELLOW : TFT_DARKGREY);

  if (state.implemented && state.adjustable && state.percent >= 0) {
    systemControlTrack(canvas, state);
  } else {
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    const String message = String(SystemControls::label(control)) +
                           ": not implemented";
    canvas.drawString(fitText(canvas, message, 230), 40, H + 11);
  }

  const auto close = systemControlCloseRect();
  canvas.drawRoundRect(close.x, close.y, close.w, close.h, 5, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("X", close.x + 11, close.y + 7);
}

}  // namespace CydUi
