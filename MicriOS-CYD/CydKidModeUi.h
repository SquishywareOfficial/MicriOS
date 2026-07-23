#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "CydUi.h"
#include "src/shared/logic/KidModeLogic.h"

namespace CydKidModeUi {

constexpr int16_t PIN_BACKSPACE = 10;
constexpr int16_t PIN_SUBMIT = 11;
constexpr int16_t NO_HIT = TouchUi::NO_CONTROL;
constexpr TouchUi::Rect EXPIRED_UNLOCK = {72, 158, 176, 48};
constexpr TouchUi::Rect CONFIRM_CANCEL = {18, 151, 136, 44};
constexpr TouchUi::Rect CONFIRM_DISABLE = {166, 151, 136, 44};
constexpr int16_t TEXT_SHIFT = 40;
constexpr int16_t TEXT_APOSTROPHE = 41;
constexpr int16_t TEXT_BACKSPACE = 42;
constexpr int16_t TEXT_SPACE = 43;
constexpr int16_t TEXT_CLEAR = 44;
constexpr int16_t TEXT_CANCEL = 45;
constexpr int16_t TEXT_SAVE = 46;
constexpr int16_t PALETTE_CANCEL = 6;
constexpr int16_t PALETTE_SAVE = 7;
constexpr TouchUi::Rect TEXT_SHIFT_RECT = {5, 117, 29, 27};
constexpr TouchUi::Rect TEXT_APOSTROPHE_RECT = {253, 117, 29, 27};
constexpr TouchUi::Rect TEXT_BACKSPACE_RECT = {284, 117, 29, 27};
constexpr TouchUi::Rect TEXT_CANCEL_RECT = {5, 153, 59, 48};
constexpr TouchUi::Rect TEXT_SPACE_RECT = {68, 153, 112, 48};
constexpr TouchUi::Rect TEXT_CLEAR_RECT = {184, 153, 58, 48};
constexpr TouchUi::Rect TEXT_SAVE_RECT = {246, 153, 69, 48};
constexpr TouchUi::Rect PALETTE_CANCEL_RECT = {34, 165, 118, 37};
constexpr TouchUi::Rect PALETTE_SAVE_RECT = {168, 165, 118, 37};

struct SplashColors {
  uint16_t background;
  uint16_t border;
  uint16_t text;
  uint16_t accent;
};

inline SplashColors splashColors(KidMode::SplashPalette palette) {
  switch (palette) {
    case KidMode::SplashPalette::Ocean:
      return {TFT_NAVY, TFT_CYAN, TFT_WHITE, TFT_BLUE};
    case KidMode::SplashPalette::Space:
      return {TFT_BLACK, 0x801F, TFT_CYAN, TFT_MAGENTA};
    case KidMode::SplashPalette::Forest:
      return {0x0200, TFT_GREEN, 0xD7E0, TFT_YELLOW};
    case KidMode::SplashPalette::Sunset:
      return {0x4008, TFT_ORANGE, TFT_YELLOW, TFT_MAGENTA};
    case KidMode::SplashPalette::Rainbow:
      return {0x0841, TFT_MAGENTA, TFT_WHITE, TFT_CYAN};
    case KidMode::SplashPalette::Candy:
    default:
      return {0x280A, TFT_MAGENTA, 0xFDF7, TFT_YELLOW};
  }
}

inline TouchUi::Rect pinKeyRect(uint8_t slot) {
  constexpr int16_t LEFT = 11;
  constexpr int16_t TOP = 68;
  constexpr int16_t WIDTH = 94;
  constexpr int16_t HEIGHT = 31;
  constexpr int16_t GAP_X = 8;
  constexpr int16_t GAP_Y = 3;
  return {static_cast<int16_t>(LEFT + (slot % 3) * (WIDTH + GAP_X)),
          static_cast<int16_t>(TOP + (slot / 3) * (HEIGHT + GAP_Y)), WIDTH,
          HEIGHT};
}

inline TouchUi::Rect durationRect(uint8_t index) {
  constexpr int16_t LEFT = 12;
  constexpr int16_t TOP = 45;
  constexpr int16_t WIDTH = 144;
  constexpr int16_t HEIGHT = 45;
  constexpr int16_t GAP_X = 8;
  constexpr int16_t GAP_Y = 7;
  return {static_cast<int16_t>(LEFT + (index % 2) * (WIDTH + GAP_X)),
          static_cast<int16_t>(TOP + (index / 2) * (HEIGHT + GAP_Y)), WIDTH,
          HEIGHT};
}

inline TouchUi::Rect manageRect(uint8_t index) {
  return {14, static_cast<int16_t>(37 + index * 28), 292, 25};
}

inline TouchUi::Rect splashMenuRect(uint8_t index) {
  return {18, static_cast<int16_t>(66 + index * 34), 284, 30};
}

inline TouchUi::Rect paletteRect(uint8_t index) {
  return {static_cast<int16_t>(7 + (index % 3) * 104),
          static_cast<int16_t>(47 + (index / 3) * 56), 98, 48};
}

inline TouchUi::Rect textLetterRect(uint8_t index) {
  if (index < 10) {
    return {static_cast<int16_t>(5 + index * 31), 57, 29, 27};
  }
  if (index < 19) {
    return {static_cast<int16_t>(13 + (index - 10) * 34), 87, 31, 27};
  }
  return {static_cast<int16_t>(36 + (index - 19) * 31), 117, 29, 27};
}

inline int16_t pinHit(TouchUi::Point point) {
  for (uint8_t slot = 0; slot < 12; ++slot) {
    if (!pinKeyRect(slot).contains(point)) continue;
    if (slot <= 8) return slot + 1;
    if (slot == 9) return PIN_BACKSPACE;
    if (slot == 10) return 0;
    return PIN_SUBMIT;
  }
  return NO_HIT;
}

inline int16_t durationHit(TouchUi::Point point) {
  for (uint8_t i = 0; i < 6; ++i) {
    if (durationRect(i).contains(point)) return i;
  }
  return NO_HIT;
}

inline int16_t manageHit(TouchUi::Point point) {
  for (uint8_t i = 0; i < 6; ++i) {
    if (manageRect(i).contains(point)) return i;
  }
  return NO_HIT;
}

inline int16_t splashMenuHit(TouchUi::Point point) {
  for (uint8_t i = 0; i < 4; ++i) {
    if (splashMenuRect(i).contains(point)) return i;
  }
  return NO_HIT;
}

inline int16_t paletteHit(TouchUi::Point point) {
  for (uint8_t i = 0; i < 6; ++i) {
    if (paletteRect(i).contains(point)) return i;
  }
  if (PALETTE_CANCEL_RECT.contains(point)) return PALETTE_CANCEL;
  if (PALETTE_SAVE_RECT.contains(point)) return PALETTE_SAVE;
  return NO_HIT;
}

inline int16_t textEditorHit(TouchUi::Point point) {
  for (uint8_t i = 0; i < 26; ++i) {
    if (textLetterRect(i).contains(point)) return i;
  }
  if (TEXT_SHIFT_RECT.contains(point)) return TEXT_SHIFT;
  if (TEXT_APOSTROPHE_RECT.contains(point)) return TEXT_APOSTROPHE;
  if (TEXT_BACKSPACE_RECT.contains(point)) return TEXT_BACKSPACE;
  if (TEXT_SPACE_RECT.contains(point)) return TEXT_SPACE;
  if (TEXT_CLEAR_RECT.contains(point)) return TEXT_CLEAR;
  if (TEXT_CANCEL_RECT.contains(point)) return TEXT_CANCEL;
  if (TEXT_SAVE_RECT.contains(point)) return TEXT_SAVE;
  return NO_HIT;
}

template <typename Canvas>
void drawPin(Canvas& canvas, const char* title, const char* prompt,
             const KidMode::PinEntry& entry, const char* submitLabel = "Unlock",
             int16_t pressedId = NO_HIT,
             bool fullScreen = false) {
  if (fullScreen) {
    canvas.fillScreen(TFT_BLACK);
  } else {
    CydUi::clear(canvas);
  }
  CydUi::header(canvas, title, TFT_ORANGE);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const String fitted = CydUi::fitText(canvas, prompt == nullptr ? "" : prompt,
                                       CydUi::W - 24);
  canvas.drawString(fitted, (CydUi::W - canvas.textWidth(fitted)) / 2, 35);

  const int16_t dotsWidth = KidMode::PIN_LENGTH * 24;
  const int16_t dotsX = (CydUi::W - dotsWidth) / 2;
  for (uint8_t i = 0; i < KidMode::PIN_LENGTH; ++i) {
    const int16_t x = dotsX + i * 24 + 8;
    canvas.drawCircle(x, 57, 5, TFT_DARKGREY);
    if (i < entry.length()) canvas.fillCircle(x, 57, 4, TFT_CYAN);
  }

  constexpr const char* LABELS[] = {"1", "2", "3", "4", "5", "6",
                                     "7", "8", "9", "<", "0"};
  for (uint8_t slot = 0; slot < 12; ++slot) {
    const int16_t id = slot <= 8 ? slot + 1
                       : slot == 9 ? PIN_BACKSPACE
                       : slot == 10 ? 0
                                    : PIN_SUBMIT;
    const bool submit = slot == 11;
    const uint16_t color = submit
                               ? (entry.complete() ? TFT_GREEN : TFT_DARKGREY)
                               : (slot == 9 ? TFT_ORANGE : TFT_CYAN);
    const char* label = submit ? submitLabel : LABELS[slot];
    CydUi::touchAction(canvas, pinKeyRect(slot), label, color,
                       submit && entry.complete(), pressedId == id);
  }
}

template <typename Canvas>
void drawLockout(Canvas& canvas, uint32_t seconds, bool fullScreen = false) {
  if (fullScreen) canvas.fillScreen(TFT_BLACK);
  else CydUi::clear(canvas);
  CydUi::header(canvas, "Child Mode", TFT_RED);
  CydUi::centered(canvas, "Incorrect", 49, 3, TFT_RED);
  CydUi::centered(canvas, "password", 80, 3, TFT_RED);
  CydUi::centered(canvas, "Locked out", 116, 2, TFT_WHITE);
  CydUi::centered(canvas, String(seconds) + " seconds", 145, 3, TFT_ORANGE);
  CydUi::centered(canvas, "Please wait", 184, 1, TFT_LIGHTGREY);
}

template <typename Canvas>
void drawDurations(Canvas& canvas, int16_t pressedId = NO_HIT,
                   bool fullScreen = false) {
  if (fullScreen) canvas.fillScreen(TFT_BLACK);
  else CydUi::clear(canvas);
  CydUi::header(canvas, "Unlock Duration", TFT_GREEN);
  constexpr const char* LABELS[] = {"10 minutes", "20 minutes", "30 minutes",
                                     "1 hour", "2 hours", "Unlimited"};
  for (uint8_t i = 0; i < 6; ++i) {
    CydUi::touchAction(canvas, durationRect(i), LABELS[i],
                       i == 5 ? TFT_ORANGE : TFT_GREEN, false,
                       pressedId == i);
  }
}

template <typename Canvas>
void drawManagement(Canvas& canvas, const String& status, bool splashEnabled,
                    int16_t pressedId = NO_HIT) {
  CydUi::clear(canvas);
  CydUi::header(canvas, "Child Mode", TFT_ORANGE, status.c_str());
  constexpr const char* LABELS[] = {
      "Lock Now",       "Custom Splash", "Customise Splash",
      "Change PIN",     "Disable Child Mode", "Back"};
  constexpr uint16_t COLORS[] = {TFT_ORANGE, TFT_GREEN, TFT_CYAN,
                                  TFT_CYAN, TFT_RED, TFT_BLUE};
  for (uint8_t i = 0; i < 6; ++i) {
    if (i == 1) {
      CydUi::touchToggle(canvas, manageRect(i), LABELS[i], splashEnabled,
                         pressedId == i);
    } else {
      CydUi::touchListRow(canvas, manageRect(i), LABELS[i], false, COLORS[i],
                          nullptr, pressedId == i);
    }
  }
}

template <typename Canvas>
void drawCustomSplash(Canvas& canvas, const KidMode::SplashSettings& settings,
                      int16_t height = CydUi::SCREEN_H,
                      bool showContinueHint = true) {
  const SplashColors colors = splashColors(settings.palette);
  canvas.fillRect(0, 0, CydUi::W, height, colors.background);
  canvas.drawRoundRect(13, 13, CydUi::W - 26, height - 26, 14, colors.border);
  canvas.drawRoundRect(18, 18, CydUi::W - 36, height - 36, 11, colors.accent);

  canvas.fillCircle(38, 42, 7, colors.accent);
  canvas.fillCircle(CydUi::W - 39, 43, 5, colors.border);
  canvas.drawCircle(47, height - 43, 8, colors.border);
  canvas.drawCircle(CydUi::W - 48, height - 45, 10, colors.accent);

  String full(settings.text);
  String first = full;
  String second;
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(3);
  if (canvas.textWidth(full) > CydUi::W - 54) {
    int16_t split = -1;
    int16_t bestDistance = 32767;
    const int16_t middle = full.length() / 2;
    for (uint16_t i = 1; i + 1 < full.length(); ++i) {
      if (full[i] != ' ') continue;
      const int16_t distance = abs(static_cast<int16_t>(i) - middle);
      if (distance < bestDistance) {
        bestDistance = distance;
        split = i;
      }
    }
    if (split >= 0) {
      first = full.substring(0, split);
      second = full.substring(split + 1);
    } else {
      canvas.setTextSize(2);
    }
  }

  canvas.setTextColor(colors.text, colors.background);
  const int16_t lineHeight = canvas.textWidth("M") > 14 ? 24 : 16;
  const int16_t centerY = showContinueHint ? height / 2 - 8 : height / 2;
  const int16_t firstY = second.length() == 0 ? centerY - lineHeight / 2
                                              : centerY - lineHeight - 2;
  canvas.drawString(first, (CydUi::W - canvas.textWidth(first)) / 2, firstY);
  if (second.length() != 0) {
    canvas.drawString(second, (CydUi::W - canvas.textWidth(second)) / 2,
                      firstY + lineHeight + 6);
  }

  if (showContinueHint) {
    canvas.setTextSize(1);
    canvas.setTextColor(colors.text, colors.background);
    const String hint = "Tap to continue";
    canvas.drawString(hint, (CydUi::W - canvas.textWidth(hint)) / 2,
                      height - 31);
  }
}

template <typename Canvas>
void drawSplashMenu(Canvas& canvas, const KidMode::SplashSettings& settings,
                    int16_t pressedId = NO_HIT) {
  CydUi::clear(canvas);
  CydUi::header(canvas, "Custom Splash", TFT_CYAN,
                settings.enabled ? "ON" : "OFF");
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String preview = CydUi::fitText(canvas, settings.text, CydUi::W - 40);
  canvas.drawString(preview, (CydUi::W - canvas.textWidth(preview)) / 2, 43);
  constexpr const char* LABELS[] = {"Custom Text", "Colour Palette",
                                     "Preview Splash", "Back"};
  constexpr uint16_t COLORS[] = {TFT_CYAN, TFT_MAGENTA, TFT_GREEN, TFT_BLUE};
  for (uint8_t i = 0; i < 4; ++i) {
    CydUi::touchListRow(canvas, splashMenuRect(i), LABELS[i], false, COLORS[i],
                        nullptr, pressedId == i);
  }
}

template <typename Canvas>
void drawTextEditor(Canvas& canvas, const KidMode::SplashTextEditor& editor,
                    bool shift, int16_t pressedId = NO_HIT) {
  CydUi::clear(canvas);
  CydUi::header(canvas, "Custom Text", TFT_CYAN);
  canvas.fillRoundRect(10, 34, 300, 20, 5, CydUi::SURFACE);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, CydUi::SURFACE);
  String value = editor.empty() ? String("Enter splash text")
                                : String(editor.value());
  value = CydUi::fitText(canvas, value, 286);
  canvas.drawString(value, 17, 41);

  constexpr const char* LETTERS = "QWERTYUIOPASDFGHJKLZXCVBNM";
  for (uint8_t i = 0; i < 26; ++i) {
    char label[2] = {LETTERS[i], '\0'};
    if (!shift) label[0] = static_cast<char>(label[0] - 'A' + 'a');
    CydUi::touchAction(canvas, textLetterRect(i), label, TFT_CYAN, false,
                       pressedId == i);
  }
  CydUi::touchAction(canvas, TEXT_SHIFT_RECT, shift ? "SHIFT" : "shift",
                     shift ? TFT_ORANGE : TFT_DARKGREY, shift,
                     pressedId == TEXT_SHIFT);
  CydUi::touchAction(canvas, TEXT_APOSTROPHE_RECT, "'", TFT_CYAN, false,
                     pressedId == TEXT_APOSTROPHE);
  CydUi::touchAction(canvas, TEXT_BACKSPACE_RECT, "<", TFT_ORANGE, false,
                     pressedId == TEXT_BACKSPACE);
  CydUi::touchAction(canvas, TEXT_CANCEL_RECT, "Cancel", TFT_LIGHTGREY, false,
                     pressedId == TEXT_CANCEL);
  CydUi::touchAction(canvas, TEXT_SPACE_RECT, "Space", TFT_CYAN, false,
                     pressedId == TEXT_SPACE);
  CydUi::touchAction(canvas, TEXT_CLEAR_RECT, "Clear", TFT_ORANGE, false,
                     pressedId == TEXT_CLEAR);
  CydUi::touchAction(canvas, TEXT_SAVE_RECT, "Save",
                     editor.empty() ? TFT_DARKGREY : TFT_GREEN,
                     !editor.empty(), pressedId == TEXT_SAVE);
}

template <typename Canvas>
void drawPalettePicker(Canvas& canvas, KidMode::SplashPalette selected,
                       int16_t pressedId = NO_HIT) {
  CydUi::clear(canvas);
  CydUi::header(canvas, "Colour Palette", TFT_MAGENTA);
  for (uint8_t i = 0; i < 6; ++i) {
    const auto palette = static_cast<KidMode::SplashPalette>(i);
    const SplashColors colors = splashColors(palette);
    const TouchUi::Rect rect = paletteRect(i);
    canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 7, colors.background);
    canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 7,
                         palette == selected ? TFT_WHITE : colors.border);
    if (pressedId == i) {
      canvas.drawRoundRect(rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4, 6,
                           colors.accent);
    }
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);
    canvas.setTextColor(colors.text, colors.background);
    const String label = KidMode::splashPaletteLabel(palette);
    canvas.drawString(label, rect.x + (rect.w - canvas.textWidth(label)) / 2,
                      rect.y + 20);
  }
  CydUi::touchAction(canvas, PALETTE_CANCEL_RECT, "Cancel", TFT_LIGHTGREY,
                     false, pressedId == PALETTE_CANCEL);
  CydUi::touchAction(canvas, PALETTE_SAVE_RECT, "Save", TFT_GREEN, true,
                     pressedId == PALETTE_SAVE);
}

template <typename Canvas>
void drawDisableConfirmation(Canvas& canvas, int16_t pressedId = NO_HIT) {
  CydUi::confirmation(canvas, "Disable Child Mode",
                      "Disable the lock and erase its PIN?", CONFIRM_CANCEL,
                      CONFIRM_DISABLE, "Disable");
  if (pressedId == 0) {
    CydUi::touchAction(canvas, CONFIRM_CANCEL, "Cancel", TFT_LIGHTGREY, false,
                       true);
  } else if (pressedId == 1) {
    CydUi::touchAction(canvas, CONFIRM_DISABLE, "Disable", TFT_RED, true,
                       true);
  }
}

template <typename Canvas>
void drawExpired(Canvas& canvas, uint32_t seconds, bool pressed = false) {
  canvas.fillScreen(TFT_BLACK);
  canvas.drawRoundRect(18, 18, 284, 204, 12, TFT_RED);
  CydUi::centered(canvas, "Time is up", 55, 4, TFT_RED);
  CydUi::centered(canvas, "MicriOS is locked", 109, 2, TFT_WHITE);
  CydUi::centered(canvas, String("Sleeping in ") + String(seconds) + "s", 137,
                  1, TFT_LIGHTGREY);
  CydUi::touchAction(canvas, EXPIRED_UNLOCK, "Parent Unlock", TFT_ORANGE,
                     false, pressed);
}

}  // namespace CydKidModeUi
