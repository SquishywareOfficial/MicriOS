#include "OptionsApp.h"

#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

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
    {"Miner", "miner"},
    {"Miner Stats", "miner_stats"},
    {"Cluster", "cluster"},
    {"WiFi", "wifi"},
    {"Contacts", "contacts"},
};

constexpr uint8_t SAVE_COUNT = sizeof(SAVE_ENTRIES) / sizeof(SAVE_ENTRIES[0]);
constexpr uint8_t SAVE_ALL_INDEX = SAVE_COUNT;
constexpr uint8_t SAVE_BACK_INDEX = SAVE_COUNT + 1;
constexpr uint8_t SAVE_MENU_COUNT = SAVE_COUNT + 2;
const char* MAIN_ITEMS[] = {"Player", "Saves", "Exit"};
constexpr uint8_t MAIN_COUNT = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);
}

OptionsApp::OptionsApp(uint32_t width, uint32_t height)
    : App("Options", width, height) {}

bool OptionsApp::hasCustomOverlay() const {
  return true;
}

void OptionsApp::onAppReset() {
  PlayerProfile::unpackInitials(PlayerProfile::loadInitials(), initials_);
  mode_ = Mode::Main;
  selected_ = 0;
  mainIndex_ = 0;
  saveIndex_ = 0;
  message_ = "";
  messageToMain_ = false;
}

void OptionsApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (mode_ == Mode::Main) {
    if (input.click) {
      mainIndex_ = (mainIndex_ + 1) % MAIN_COUNT;
    }
    if (input.longPress) {
      if (mainIndex_ == 0) {
        mode_ = Mode::Initials;
        selected_ = 0;
      } else if (mainIndex_ == 1) {
        mode_ = Mode::Saves;
        saveIndex_ = 0;
      } else {
        requestExitToMenu();
      }
    }
    return;
  }

  if (mode_ == Mode::Saves) {
    if (input.click) {
      saveIndex_ = (saveIndex_ + 1) % SAVE_MENU_COUNT;
    }
    if (input.longPress) {
      if (saveIndex_ == SAVE_BACK_INDEX) {
        mode_ = Mode::Main;
      } else if (saveIndex_ == SAVE_ALL_INDEX) {
        mode_ = Mode::ConfirmAll1;
      } else {
        mode_ = Mode::ConfirmOne;
      }
    }
    return;
  }

  if (mode_ == Mode::ConfirmOne) {
    if (input.click) {
      mode_ = Mode::Saves;
    }
    if (input.longPress) {
      clearSelectedSave();
      message_ = "Save erased";
      messageToMain_ = false;
      mode_ = Mode::Message;
    }
    return;
  }

  if (mode_ == Mode::ConfirmAll1) {
    if (input.click) {
      mode_ = Mode::Saves;
    }
    if (input.longPress) {
      mode_ = Mode::ConfirmAll2;
    }
    return;
  }

  if (mode_ == Mode::ConfirmAll2) {
    if (input.click) {
      mode_ = Mode::Saves;
    }
    if (input.longPress) {
      clearAllSaves();
      message_ = "All erased";
      messageToMain_ = false;
      mode_ = Mode::Message;
    }
    return;
  }

  if (mode_ == Mode::Message) {
    if (input.click || input.longPress) {
      mode_ = messageToMain_ ? Mode::Main : Mode::Saves;
    }
    return;
  }

  if (input.click) {
    initials_[selected_] = nextInitial(initials_[selected_]);
  }
  if (input.longPress) {
    if (selected_ == 0) {
      selected_ = 1;
    } else {
      PlayerProfile::saveInitials(initials_[0], initials_[1]);
      message_ = "Player saved";
      messageToMain_ = true;
      mode_ = Mode::Message;
    }
  }
}

void OptionsApp::drawStart(U8G2& u8g2) {
  char dotted[4];
  PlayerProfile::unpackDottedInitials(PlayerProfile::loadInitials(), dotted);

  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Options");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 21, "Player");
  u8g2.drawStr(36, 21, dotted);
  u8g2.drawStr(3, 31, "Save manager");
  u8g2.drawStr(3, 38, "Press to open");
}

void OptionsApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  if (mode_ == Mode::Main) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, "Options");
    drawFit(u8g2, 14, 25, MAIN_ITEMS[mainIndex_]);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 38, "Tap next Hold ok");
    return;
  }

  if (mode_ == Mode::Saves) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, "Save Mgr");
    if (saveIndex_ == SAVE_ALL_INDEX) {
      drawFit(u8g2, 5, 24, "Delete ALL");
    } else if (saveIndex_ == SAVE_BACK_INDEX) {
      drawFit(u8g2, 22, 24, "Back");
    } else {
      drawFit(u8g2, 3, 24, SAVE_ENTRIES[saveIndex_].label);
    }
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 38);
    u8g2.print(saveIndex_ + 1);
    u8g2.print("/");
    u8g2.print(SAVE_MENU_COUNT);
    u8g2.print(" Tap next");
    return;
  }

  if (mode_ == Mode::ConfirmOne || mode_ == Mode::ConfirmAll1 || mode_ == Mode::ConfirmAll2) {
    u8g2.setFont(u8g2_font_5x8_tr);
    if (mode_ == Mode::ConfirmOne) {
      u8g2.drawStr(3, 9, "DELETE?");
    } else if (mode_ == Mode::ConfirmAll1) {
      u8g2.drawStr(3, 9, "DEL ALL?");
    } else {
      u8g2.drawStr(3, 9, "REALLY?");
    }
    u8g2.setFont(u8g2_font_4x6_tr);
    if (mode_ == Mode::ConfirmOne) {
      u8g2.drawStr(3, 19, "Save:");
      drawFit(u8g2, 25, 19, SAVE_ENTRIES[saveIndex_].label);
    } else {
      u8g2.drawStr(3, 21, "Delete all saves");
    }
    u8g2.drawStr(3, 30, "Hold = erase");
    u8g2.drawStr(3, 38, "Tap = cancel");
    return;
  }

  if (mode_ == Mode::Message) {
    u8g2.setFont(u8g2_font_5x8_tr);
    drawFit(u8g2, 3, 15, message_);
    u8g2.setFont(u8g2_font_4x6_tr);
    if (!messageToMain_) {
      u8g2.drawStr(3, 27, "Restart refreshes");
    }
    u8g2.drawStr(3, 38, "Tap continue");
    return;
  }

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Initials");
  u8g2.setCursor(26, 24);
  u8g2.print(initials_[0]);
  u8g2.print(".");
  u8g2.print(initials_[1]);
  const int cursorX = selected_ == 0 ? 25 : 37;
  u8g2.drawLine(cursorX, 27, cursorX + 5, 27);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, selected_ == 0 ? "Tap char Hold next" : "Hold save/menu");
}

void OptionsApp::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Saved");
  u8g2.setCursor(25, 24);
  u8g2.print(initials_[0]);
  u8g2.print(".");
  u8g2.print(initials_[1]);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Hold menu");
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

void OptionsApp::drawFit(U8G2& u8g2, int x, int y, const char* text) {
  u8g2.setFont(u8g2_font_5x8_tr);
  if (u8g2.getStrWidth(text) > static_cast<int>(width + 2 - x - 3)) {
    u8g2.setFont(u8g2_font_4x6_tr);
  }
  u8g2.drawStr(x, y, text);
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
