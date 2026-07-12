#include "OptionsApp.h"

#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences savePrefs;

struct SaveEntry {
  const char* label;
  const char* ns;
};

const SaveEntry SAVE_ENTRIES[] = {
    {"Breakout76", "breakout76"},
    {"City Racer", "cityrace"},
    {"AlienRaid", "aliens"},
    {"CaveChop", "cavechop"},
    {"Lander", "lander"},
    {"NeedSpeed", "needspeed"},
    {"MicriFld", "micrifld"},
    {"NoonShot", "shooter"},
    {"Fishing", "fish"},
    {"MazeRun", "maze"},
    {"MazeColl", "mazecol"},
    {"Pipe", "pipe"},
    {"Casino", "casino"},
    {"Golf", "golf"},
    {"Tower", "tower"},
    {"Knife", "knife"},
    {"Reactor", "reactor"},
    {"Simon", "simon"},
    {"Pet", "pet"},
    {"Clock", "clock"},
    {"Autolaunch", "autolaunch"},
    {"Miner", "miner"},
    {"Miner Stats", "miner_stats"},
    {"Cluster", "cluster"},
    {"Contacts", "contacts"},
};

constexpr uint8_t SAVE_COUNT = sizeof(SAVE_ENTRIES) / sizeof(SAVE_ENTRIES[0]);
constexpr uint8_t SAVE_ALL_INDEX = SAVE_COUNT;
constexpr uint8_t SAVE_BACK_INDEX = SAVE_COUNT + 1;
constexpr uint8_t SAVE_MENU_COUNT = SAVE_COUNT + 2;
const char* MAIN_ITEMS[] = {"Player Initials", "Text Size", "Save Manager", "Exit"};
constexpr uint8_t MAIN_COUNT = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);
constexpr uint8_t VISIBLE_SAVE_ROWS = 4;

template <typename Canvas>
void drawCounter(Canvas& tft, uint32_t width, uint8_t index, uint8_t count) {
  if (count > 0) {
    char counter[10];
    snprintf(counter, sizeof(counter), "%u/%u", index + 1, count);
    tft.fillRect(width - 62, 6, 50, 18, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(counter, width - tft.textWidth(counter) - 12, 12);
  }
}

template <typename Canvas>
void drawShell(Canvas& tft, uint32_t width, uint32_t height, const char* title, uint8_t index = 0, uint8_t count = 0) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(title, 12, 8);
  drawCounter(tft, width, index, count);
  tft.drawFastHLine(12, 34, width - 24, TFT_DARKGREY);
  tft.drawRect(0, 0, width, height, TFT_DARKGREY);
}

template <typename Canvas>
void drawFooter(Canvas& tft, uint32_t width, uint32_t height, const char* text) {
  tft.fillRect(0, height - 17, width, 17, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(text, 12, height - 13);
}

template <typename Canvas>
void drawClipped(Canvas& tft, const char* text, int x, int y, int maxWidth) {
  String label(text);
  while (label.length() > 0 && tft.textWidth(label) > maxWidth) {
    label.remove(label.length() - 1);
  }
  tft.drawString(label, x, y);
}

template <typename Canvas>
void drawRow(Canvas& tft, uint32_t width, int y, const char* text, bool selected) {
  const uint16_t bg = selected ? TFT_GREEN : TFT_DARKGREY;
  tft.fillRect(14, y, width - 28, 22, bg);
  tft.setTextSize(1);
  tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE, bg);
  drawClipped(tft, text, 24, y + 7, width - 48);
}

template <typename Canvas>
void drawSaveRow(Canvas& tft, uint32_t width, int y, const char* text, bool selected) {
  const uint16_t bg = selected ? TFT_GREEN : TFT_DARKGREY;
  tft.fillRect(14, y, width - 28, 17, bg);
  tft.setTextSize(1);
  tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE, bg);
  drawClipped(tft, text, 24, y + 5, width - 48);
}

uint8_t saveScrollStart(uint8_t selected) {
  if (SAVE_MENU_COUNT <= VISIBLE_SAVE_ROWS) {
    return 0;
  }
  if (selected < 2) {
    return 0;
  }
  if (selected >= SAVE_MENU_COUNT - 2) {
    return SAVE_MENU_COUNT - VISIBLE_SAVE_ROWS;
  }
  return selected - 2;
}

const char* saveLabel(uint8_t index) {
  if (index == SAVE_ALL_INDEX) return "Delete ALL";
  if (index == SAVE_BACK_INDEX) return "Back";
  return SAVE_ENTRIES[index].label;
}

uint8_t saveScrollStart(uint8_t selected, TDisplayUi::TextSize textSize) {
  return TDisplayUi::menuScrollOffset(selected, SAVE_MENU_COUNT, TDisplayUi::menuStyle(textSize));
}

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}
}

OptionsApp::OptionsApp(uint32_t width, uint32_t height)
    : App("Options", width, height) {}

bool OptionsApp::hasCustomOverlay() const {
  return true;
}

void OptionsApp::render(TFT_eSPI& tft) {
  const AppPhase currentPhase = phase();
  if (!phaseCached_ || currentPhase != renderedPhase_) {
    phaseCached_ = true;
    renderedPhase_ = currentPhase;
    dirty_ = true;
    startDirty_ = true;
    endDirty_ = true;
    runningRendered_ = false;
  }
  App::render(tft);
}

void OptionsApp::markDirty() {
  dirty_ = true;
}

void OptionsApp::onAppReset() {
  PlayerProfile::unpackInitials(PlayerProfile::loadInitials(), initials_);
  mode_ = Mode::Main;
  selected_ = 0;
  mainIndex_ = 0;
  saveIndex_ = 0;
  textSize_ = TDisplayUi::loadTextSize();
  message_ = "";
  messageToMain_ = false;
  runningRendered_ = false;
  renderedSaveIndex_ = 255;
  renderedSaveScroll_ = 255;
  markDirty();
}

void OptionsApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;

  if (b2.click) {
    if (mode_ == Mode::Main) {
      requestExitToMenu();
    } else if (mode_ == Mode::Saves || mode_ == Mode::Initials || mode_ == Mode::TextSize) {
      mode_ = Mode::Main;
      markDirty();
    } else if (mode_ == Mode::Message) {
      mode_ = messageToMain_ ? Mode::Main : Mode::Saves;
      markDirty();
    } else {
      mode_ = Mode::Saves;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Main) {
    if (b1.click) {
      mainIndex_ = (mainIndex_ + 1) % MAIN_COUNT;
      markDirty();
    }
    if (b1.longPress) {
      if (mainIndex_ == 0) {
        mode_ = Mode::Initials;
        selected_ = 0;
        markDirty();
      } else if (mainIndex_ == 1) {
        textSize_ = TDisplayUi::loadTextSize();
        mode_ = Mode::TextSize;
        markDirty();
      } else if (mainIndex_ == 2) {
        mode_ = Mode::Saves;
        saveIndex_ = 0;
        renderedSaveIndex_ = 255;
        renderedSaveScroll_ = 255;
        markDirty();
      } else {
        requestExitToMenu();
      }
    }
    return;
  }

  if (mode_ == Mode::Saves) {
    if (b1.click) {
      saveIndex_ = (saveIndex_ + 1) % SAVE_MENU_COUNT;
      markDirty();
    }
    if (b1.longPress) {
      if (saveIndex_ == SAVE_BACK_INDEX) {
        mode_ = Mode::Main;
        markDirty();
      } else if (saveIndex_ == SAVE_ALL_INDEX) {
        mode_ = Mode::ConfirmAll1;
        markDirty();
      } else {
        mode_ = Mode::ConfirmOne;
        markDirty();
      }
    }
    return;
  }

  if (mode_ == Mode::TextSize) {
    if (b1.click) {
      textSize_ = TDisplayUi::nextTextSize(textSize_);
      markDirty();
    }
    if (b1.longPress) {
      TDisplayUi::saveTextSize(textSize_);
      message_ = "Text size saved";
      messageToMain_ = true;
      mode_ = Mode::Message;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::ConfirmOne) {
    if (b1.click) {
      mode_ = Mode::Saves;
      markDirty();
    }
    if (b1.longPress) {
      clearSelectedSave();
      message_ = "Save erased";
      messageToMain_ = false;
      mode_ = Mode::Message;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::ConfirmAll1) {
    if (b1.click) {
      mode_ = Mode::Saves;
      markDirty();
    }
    if (b1.longPress) {
      mode_ = Mode::ConfirmAll2;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::ConfirmAll2) {
    if (b1.click) {
      mode_ = Mode::Saves;
      markDirty();
    }
    if (b1.longPress) {
      clearAllSaves();
      message_ = "All erased";
      messageToMain_ = false;
      mode_ = Mode::Message;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Message) {
    if (b1.click || b1.longPress) {
      mode_ = messageToMain_ ? Mode::Main : Mode::Saves;
      markDirty();
    }
    return;
  }

  if (b1.click) {
    initials_[selected_] = nextInitial(initials_[selected_]);
    markDirty();
  }
  if (b1.longPress) {
    if (selected_ == 0) {
      selected_ = 1;
      markDirty();
    } else {
      PlayerProfile::saveInitials(initials_[0], initials_[1]);
      message_ = "Player saved";
      messageToMain_ = true;
      mode_ = Mode::Message;
      markDirty();
    }
  }
}

void OptionsApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  char dotted[4];
  PlayerProfile::unpackDottedInitials(PlayerProfile::loadInitials(), dotted);

  drawBuffered(tft, width, height, [&](auto& canvas) {
    drawShell(canvas, width, height, "Options");
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString("Player", 20, 50);
    canvas.drawString(dotted, 112, 50);
    canvas.setTextSize(1);
    canvas.drawString("Initials and save manager", 20, 80);
    drawFooter(canvas, width, height, "B1 start  B2 back");
  });
  startDirty_ = false;
}

void OptionsApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }
  const bool fullRedraw = !runningRendered_ || renderedMode_ != mode_;

  if (mode_ == Mode::Main) {
    const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
    const uint8_t first = TDisplayUi::menuScrollOffset(mainIndex_, MAIN_COUNT, TDisplayUi::menuStyle(textSize));
    drawBuffered(tft, width, height, [&](auto& canvas) {
      TDisplayUi::menuFrame(canvas, "Options", mainIndex_, MAIN_COUNT, first,
                            [](uint8_t index) -> const char* { return MAIN_ITEMS[index]; },
                            "B1 next/open  B2 back", textSize, TFT_GREEN);
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    renderedTextSize_ = textSize;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::Saves) {
    const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
    const TDisplayUi::MenuStyle style = TDisplayUi::menuStyle(textSize);
    const uint8_t first = saveScrollStart(saveIndex_, textSize);
    if (!fullRedraw && renderedTextSize_ == textSize && renderedSaveScroll_ == first && renderedSaveIndex_ < SAVE_MENU_COUNT) {
      TDisplayUi::menuCounter(tft, saveIndex_, SAVE_MENU_COUNT);
      const uint8_t oldRow = renderedSaveIndex_ - first;
      if (oldRow < style.visibleRows) {
        TDisplayUi::menuRow(tft, oldRow, saveLabel(renderedSaveIndex_), false, textSize, TFT_GREEN);
      }
      const uint8_t newRow = saveIndex_ - first;
      if (newRow < style.visibleRows) {
        TDisplayUi::menuRow(tft, newRow, saveLabel(saveIndex_), true, textSize, TFT_GREEN);
      }
    } else {
      drawBuffered(tft, width, height, [&](auto& canvas) {
        TDisplayUi::menuFrame(canvas, "Save Manager", saveIndex_, SAVE_MENU_COUNT, first,
                              [](uint8_t index) -> const char* { return saveLabel(index); },
                              "B1 next/delete  B2 back", textSize, TFT_GREEN);
      });
    }
    runningRendered_ = true;
    renderedMode_ = mode_;
    renderedTextSize_ = textSize;
    renderedSaveIndex_ = saveIndex_;
    renderedSaveScroll_ = first;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::TextSize) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      TDisplayUi::menuShell(canvas, "Text Size", static_cast<uint8_t>(textSize_), 2, textSize_);
      TDisplayUi::centered(canvas, TDisplayUi::textSizeLabel(textSize_), 54, textSize_ == TDisplayUi::TextSize::Large ? 3 : 2, TFT_GREEN);
      TDisplayUi::centered(canvas, "Default: Compact", 91, 1, TFT_LIGHTGREY);
      TDisplayUi::menuFooter(canvas, "B1 toggle  B1 hold save  B2 back", textSize_);
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    renderedTextSize_ = textSize_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ConfirmOne || mode_ == Mode::ConfirmAll1 || mode_ == Mode::ConfirmAll2) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      drawShell(canvas, width, height, "Confirm");
      if (mode_ == Mode::ConfirmOne) {
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setTextSize(2);
        canvas.drawString("DELETE?", 20, 46);
      } else if (mode_ == Mode::ConfirmAll1) {
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setTextSize(2);
        canvas.drawString("DELETE ALL?", 20, 46);
      } else {
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setTextSize(2);
        canvas.drawString("REALLY?", 20, 46);
      }

      canvas.setTextSize(1);
      canvas.setTextColor(TFT_WHITE, TFT_BLACK);
      if (mode_ == Mode::ConfirmOne) {
        canvas.drawString("Save:", 22, 78);
        drawClipped(canvas, SAVE_ENTRIES[saveIndex_].label, 62, 78, width - 82);
      } else {
        canvas.drawString("This erases every score/state.", 22, 78);
      }
      drawFooter(canvas, width, height, "B1 hold erase  B1 tap/B2 cancel");
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::Message) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      drawShell(canvas, width, height, "Done");
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_GREEN, TFT_BLACK);
      drawClipped(canvas, message_, 22, 54, width - 44);
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_WHITE, TFT_BLACK);
      if (!messageToMain_) {
        canvas.drawString("Restart refreshes affected apps.", 22, 84);
      }
      drawFooter(canvas, width, height, "B1 continue  B2 back");
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  drawBuffered(tft, width, height, [&](auto& canvas) {
    drawShell(canvas, width, height, "Initials");
    canvas.setTextSize(4);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setCursor(70, 52);
    canvas.print(initials_[0]);
    canvas.print(".");
    canvas.print(initials_[1]);
    const int cursorX = selected_ == 0 ? 68 : 139;
    canvas.drawFastHLine(cursorX, 96, 30, TFT_GREEN);
    drawFooter(canvas, width, height, selected_ == 0 ? "B1 char/open next  B2 back" : "B1 char/open save  B2 back");
  });
  runningRendered_ = true;
  renderedMode_ = mode_;
  dirty_ = false;
}

void OptionsApp::drawEnd(TFT_eSPI& tft) {
  if (!endDirty_) {
    return;
  }
  drawBuffered(tft, width, height, [&](auto& canvas) {
    drawShell(canvas, width, height, "Saved");
    canvas.setTextSize(4);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setCursor(70, 52);
    canvas.print(initials_[0]);
    canvas.print(".");
    canvas.print(initials_[1]);
    drawFooter(canvas, width, height, "B2 back");
  });
  endDirty_ = false;
}

void OptionsApp::clearNamespace(const char* ns) {
  savePrefs.begin(ns, false);
  savePrefs.clear();
  savePrefs.end();
}

void OptionsApp::clearSelectedSave() {
  if (saveIndex_ < SAVE_COUNT) {
    clearNamespace(SAVE_ENTRIES[saveIndex_].ns);
  }
}

void OptionsApp::clearAllSaves() {
  for (uint8_t i = 0; i < SAVE_COUNT; i++) {
    clearNamespace(SAVE_ENTRIES[i].ns);
  }
}

void OptionsApp::drawFit(TFT_eSPI& tft, int x, int y, const char* text) {
  drawClipped(tft, text, x, y, static_cast<int>(width) - x - 3);
}

char OptionsApp::nextInitial(char value) const {
  value = PlayerProfile::normalizeInitial(value);
  if (value >= 'A' && value < 'Z') {
    return static_cast<char>(value + 1);
  }
  if (value == 'Z') {
    return '0';
  }
  if (value >= '0' && value < '9') {
    return static_cast<char>(value + 1);
  }
  return 'A';
}
