#include "OptionsApp.h"

#include <Preferences.h>
#include <TFT_eSPI.h>
#include <esp_timer.h>
#include <string.h>

#include "../../CydHardware.h"
#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"

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
    {"Child Mode", KidMode::Storage::NAMESPACE},
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
const char* MAIN_ITEMS[] = {"User Initials", "Text Size", "Child Mode",
                            "Save Manager", "Exit"};
constexpr uint8_t MAIN_COUNT = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);
constexpr uint8_t VISIBLE_SAVE_ROWS = 4;
constexpr uint8_t SAVE_PAGE_COUNT =
    (SAVE_MENU_COUNT + VISIBLE_SAVE_ROWS - 1) / VISIBLE_SAVE_ROWS;
constexpr TouchUi::Rect INITIAL_CHARS[] = {
    {60, 49, 80, 76}, {180, 49, 80, 76}};
constexpr TouchUi::Rect INITIAL_MINUS = {14, 151, 58, 43};
constexpr TouchUi::Rect INITIAL_PLUS = {78, 151, 58, 43};
constexpr TouchUi::Rect ACTION_CANCEL = {144, 151, 76, 43};
constexpr TouchUi::Rect ACTION_SAVE = {226, 151, 80, 43};
constexpr TouchUi::Rect COMPACT_CHOICE = {24, 59, 128, 76};
constexpr TouchUi::Rect LARGE_CHOICE = {168, 59, 128, 76};
constexpr TouchUi::Rect PAGE_PREV = {12, 174, 76, 28};
constexpr TouchUi::Rect PAGE_NEXT = {232, 174, 76, 28};
constexpr TouchUi::Rect CONFIRM_CANCEL = {20, 151, 132, 43};
constexpr TouchUi::Rect CONFIRM_DELETE = {168, 151, 132, 43};
constexpr TouchUi::Rect CONTINUE_ACTION = {90, 150, 140, 44};

TouchUi::Rect mainRow(uint8_t index) {
  return {14, static_cast<int16_t>(39 + index * 33), 292, 30};
}

TouchUi::Rect saveRow(uint8_t row) {
  return {12, static_cast<int16_t>(42 + row * 32), 296, 29};
}

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
  CydUi::clear(tft);
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

uint8_t saveScrollStart(uint8_t selected, CydUi::TextSize textSize) {
  return CydUi::menuScrollOffset(selected, SAVE_MENU_COUNT, CydUi::menuStyle(textSize));
}

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}
}

OptionsApp::OptionsApp(uint32_t width, uint32_t height,
                       KidMode::Service& kidModeService,
                       const KidMode::Storage& kidModeStorage)
    : App("Options", width, height),
      kidModeService_(kidModeService),
      kidModeStorage_(kidModeStorage) {}

bool OptionsApp::hasCustomOverlay() const {
  return true;
}

bool OptionsApp::startsRunningImmediately() const { return true; }

bool OptionsApp::handleTouch(const TouchUi::TouchSample& sample,
                             const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
  int16_t hit = TouchUi::NO_CONTROL;
  if (mode_ == Mode::Main) {
    for (uint8_t i = 0; i < MAIN_COUNT; ++i) {
      if (mainRow(i).contains(point)) hit = i;
    }
  } else if (mode_ == Mode::Initials) {
    if (INITIAL_CHARS[0].contains(point)) hit = 0;
    if (INITIAL_CHARS[1].contains(point)) hit = 1;
    if (INITIAL_MINUS.contains(point)) hit = 2;
    if (INITIAL_PLUS.contains(point)) hit = 3;
    if (ACTION_CANCEL.contains(point)) hit = 4;
    if (ACTION_SAVE.contains(point)) hit = 5;
  } else if (mode_ == Mode::TextSize) {
    if (COMPACT_CHOICE.contains(point)) hit = 0;
    if (LARGE_CHOICE.contains(point)) hit = 1;
    if (ACTION_CANCEL.contains(point)) hit = 4;
    if (ACTION_SAVE.contains(point)) hit = 5;
  } else if (mode_ == Mode::Saves) {
    for (uint8_t row = 0; row < VISIBLE_SAVE_ROWS; ++row) {
      const uint8_t index = savePage_ * VISIBLE_SAVE_ROWS + row;
      if (index < SAVE_MENU_COUNT && saveRow(row).contains(point)) hit = row;
    }
    if (PAGE_PREV.contains(point)) hit = 4;
    if (PAGE_NEXT.contains(point)) hit = 5;
  } else if (mode_ == Mode::ConfirmOne || mode_ == Mode::ConfirmAll1 ||
             mode_ == Mode::ConfirmAll2) {
    if (CONFIRM_CANCEL.contains(point)) hit = 0;
    if (CONFIRM_DELETE.contains(point)) hit = 1;
  } else if (mode_ == Mode::Message) {
    if (CONTINUE_ACTION.contains(point)) hit = 0;
  } else if (childPinMode()) {
    if (!childAuthLockout_.active(nowUs)) hit = CydKidModeUi::pinHit(point);
  } else if (mode_ == Mode::ChildManage) {
    hit = CydKidModeUi::manageHit(point);
  } else if (mode_ == Mode::ChildSplashMenu) {
    hit = CydKidModeUi::splashMenuHit(point);
  } else if (mode_ == Mode::ChildSplashText) {
    hit = CydKidModeUi::textEditorHit(point);
  } else if (mode_ == Mode::ChildSplashPalette) {
    hit = CydKidModeUi::paletteHit(point);
  } else if (mode_ == Mode::ChildSplashPreview) {
    if (TouchUi::Rect{0, 0, static_cast<int16_t>(width),
                      static_cast<int16_t>(height)}.contains(point)) {
      hit = 0;
    }
  } else if (mode_ == Mode::ChildDuration) {
    hit = CydKidModeUi::durationHit(point);
  } else if (mode_ == Mode::ChildConfirmDisable) {
    if (CydKidModeUi::CONFIRM_CANCEL.contains(point)) hit = 0;
    if (CydKidModeUi::CONFIRM_DISABLE.contains(point)) hit = 1;
  }

  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) markDirty();
  if (!result.activated) return result.consumed;

  if (mode_ == Mode::Main) {
    mainIndex_ = result.id;
    if (result.id == 0) {
      mode_ = Mode::Initials;
      selected_ = 0;
    } else if (result.id == 1) {
      textSize_ = CydUi::loadTextSize();
      mode_ = Mode::TextSize;
    } else if (result.id == 2) {
      const auto config = kidModeStorage_.load();
      if (config.enabled) beginParentAuth(ParentAction::OpenManagement);
      else beginChildSetup();
    } else if (result.id == 3) {
      savePage_ = 0;
      mode_ = Mode::Saves;
    } else {
      requestExitToMenu();
    }
  } else if (mode_ == Mode::Initials) {
    if (result.id == 0 || result.id == 1) selected_ = result.id;
    if (result.id == 2) initials_[selected_] = previousInitial(initials_[selected_]);
    if (result.id == 3) initials_[selected_] = nextInitial(initials_[selected_]);
    if (result.id == 4) mode_ = Mode::Main;
    if (result.id == 5) {
      PlayerProfile::saveInitials(initials_[0], initials_[1]);
      message_ = "User saved";
      messageToMain_ = true;
      mode_ = Mode::Message;
    }
  } else if (mode_ == Mode::TextSize) {
    if (result.id == 0) textSize_ = CydUi::TextSize::Compact;
    if (result.id == 1) textSize_ = CydUi::TextSize::Large;
    if (result.id == 4) mode_ = Mode::Main;
    if (result.id == 5) {
      CydUi::saveTextSize(textSize_);
      message_ = "Text size saved";
      messageToMain_ = true;
      mode_ = Mode::Message;
    }
  } else if (mode_ == Mode::Saves) {
    if (result.id < VISIBLE_SAVE_ROWS) {
      saveIndex_ = savePage_ * VISIBLE_SAVE_ROWS + result.id;
      if (saveIndex_ == SAVE_BACK_INDEX) mode_ = Mode::Main;
      else if (saveIndex_ == SAVE_ALL_INDEX) {
        if (kidModeStorage_.load().enabled) {
          beginParentAuth(ParentAction::DeleteAll);
        } else {
          mode_ = Mode::ConfirmAll1;
        }
      } else if (strcmp(SAVE_ENTRIES[saveIndex_].ns,
                        KidMode::Storage::NAMESPACE) == 0 &&
                 kidModeStorage_.load().enabled) {
        beginParentAuth(ParentAction::DeleteChildMode);
      } else {
        mode_ = Mode::ConfirmOne;
      }
    } else if (result.id == 4) {
      savePage_ = savePage_ == 0 ? SAVE_PAGE_COUNT - 1 : savePage_ - 1;
    } else if (result.id == 5) {
      savePage_ = (savePage_ + 1) % SAVE_PAGE_COUNT;
    }
  } else if (mode_ == Mode::ConfirmOne) {
    if (result.id == 0) mode_ = Mode::Saves;
    else {
      clearSelectedSave();
      message_ = "Save erased";
      messageToMain_ = false;
      mode_ = Mode::Message;
    }
  } else if (mode_ == Mode::ConfirmAll1) {
    mode_ = result.id == 0 ? Mode::Saves : Mode::ConfirmAll2;
  } else if (mode_ == Mode::ConfirmAll2) {
    if (result.id == 0) mode_ = Mode::Saves;
    else {
      clearAllSaves();
      message_ = "All saves erased";
      messageToMain_ = false;
      mode_ = Mode::Message;
    }
  } else if (mode_ == Mode::Message) {
    mode_ = messageToMain_ ? Mode::Main : Mode::Saves;
  } else if (childPinMode()) {
    handleChildPinAction(result.id, nowUs);
  } else if (mode_ == Mode::ChildDuration) {
    selectChildDuration(static_cast<uint8_t>(result.id), nowUs);
  } else if (mode_ == Mode::ChildManage) {
    if (result.id == 0) {
      kidModeService_.lockNow();
    } else if (result.id == 1) {
      childSplashSettings_.enabled = !childSplashSettings_.enabled;
      saveChildSplash();
    } else if (result.id == 2) {
      childSplashSettings_ = kidModeStorage_.loadSplash();
      mode_ = Mode::ChildSplashMenu;
    } else if (result.id == 3) {
      childPinEntry_.clear();
      childPrompt_ = "Enter a new 6-digit PIN";
      mode_ = Mode::ChildChangePin;
    } else if (result.id == 4) {
      mode_ = Mode::ChildConfirmDisable;
    } else {
      mode_ = Mode::Main;
    }
  } else if (mode_ == Mode::ChildSplashMenu) {
    if (result.id == 0) {
      childSplashTextEditor_.begin(childSplashSettings_.text);
      childSplashShift_ = true;
      mode_ = Mode::ChildSplashText;
    } else if (result.id == 1) {
      pendingSplashPalette_ = childSplashSettings_.palette;
      mode_ = Mode::ChildSplashPalette;
    } else if (result.id == 2) {
      mode_ = Mode::ChildSplashPreview;
    } else {
      mode_ = Mode::ChildManage;
    }
  } else if (mode_ == Mode::ChildSplashText) {
    constexpr const char* LETTERS = "QWERTYUIOPASDFGHJKLZXCVBNM";
    if (result.id >= 0 && result.id < 26) {
      char letter = LETTERS[result.id];
      if (!childSplashShift_) letter = static_cast<char>(letter - 'A' + 'a');
      if (childSplashTextEditor_.append(letter)) childSplashShift_ = false;
    } else if (result.id == CydKidModeUi::TEXT_SHIFT) {
      childSplashShift_ = !childSplashShift_;
    } else if (result.id == CydKidModeUi::TEXT_APOSTROPHE) {
      childSplashTextEditor_.append('\'');
    } else if (result.id == CydKidModeUi::TEXT_BACKSPACE) {
      childSplashTextEditor_.backspace();
    } else if (result.id == CydKidModeUi::TEXT_SPACE) {
      if (childSplashTextEditor_.append(' ')) childSplashShift_ = true;
    } else if (result.id == CydKidModeUi::TEXT_CLEAR) {
      childSplashTextEditor_.clear();
      childSplashShift_ = true;
    } else if (result.id == CydKidModeUi::TEXT_CANCEL) {
      mode_ = Mode::ChildSplashMenu;
    } else if (result.id == CydKidModeUi::TEXT_SAVE &&
               !childSplashTextEditor_.empty()) {
      strncpy(childSplashSettings_.text, childSplashTextEditor_.value(),
              sizeof(childSplashSettings_.text));
      childSplashSettings_.text[sizeof(childSplashSettings_.text) - 1] = '\0';
      if (saveChildSplash()) mode_ = Mode::ChildSplashMenu;
    }
  } else if (mode_ == Mode::ChildSplashPalette) {
    if (result.id >= 0 && result.id < 6) {
      pendingSplashPalette_ =
          static_cast<KidMode::SplashPalette>(result.id);
    } else if (result.id == CydKidModeUi::PALETTE_CANCEL) {
      mode_ = Mode::ChildSplashMenu;
    } else if (result.id == CydKidModeUi::PALETTE_SAVE) {
      childSplashSettings_.palette = pendingSplashPalette_;
      if (saveChildSplash()) mode_ = Mode::ChildSplashMenu;
    }
  } else if (mode_ == Mode::ChildSplashPreview) {
    mode_ = Mode::ChildSplashMenu;
  } else if (mode_ == Mode::ChildConfirmDisable) {
    if (result.id == 0) {
      mode_ = Mode::ChildManage;
    } else {
      kidModeStorage_.clear();
      kidModeService_.setEnabled(false);
      pendingInitialEnable_ = false;
      message_ = "Child Mode disabled";
      messageToMain_ = true;
      mode_ = Mode::Message;
    }
  }
  touchCapture_.reset();
  runningRendered_ = false;
  markDirty();
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
  savePage_ = 0;
  textSize_ = CydUi::loadTextSize();
  message_ = "";
  messageToMain_ = false;
  runningRendered_ = false;
  renderedSaveIndex_ = 255;
  renderedSaveScroll_ = 255;
  childPinEntry_.clear();
  childAuthLockout_.clear();
  pendingPin_[0] = '\0';
  childPrompt_ = "";
  renderedLockoutSeconds_ = UINT32_MAX;
  pendingInitialEnable_ = false;
  childSplashSettings_ = kidModeStorage_.loadSplash();
  childSplashTextEditor_.begin(childSplashSettings_.text);
  pendingSplashPalette_ = childSplashSettings_.palette;
  childSplashShift_ = true;
  touchCapture_.reset();
  markDirty();
}

void OptionsApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;

  if (childPinMode()) {
    const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
    const uint32_t seconds = childAuthLockout_.remainingSeconds(nowUs);
    if (seconds != renderedLockoutSeconds_) {
      renderedLockoutSeconds_ = seconds;
      markDirty();
    }
  }

  if (b2.click) {
    if (mode_ == Mode::Main) {
      requestExitToMenu();
    } else if (mode_ == Mode::Saves || mode_ == Mode::Initials ||
               mode_ == Mode::TextSize) {
      mode_ = Mode::Main;
      markDirty();
    } else if (mode_ == Mode::ChildManage) {
      mode_ = Mode::Main;
      markDirty();
    } else if (mode_ == Mode::ChildSplashMenu) {
      mode_ = Mode::ChildManage;
      markDirty();
    } else if (mode_ == Mode::ChildSplashText ||
               mode_ == Mode::ChildSplashPalette ||
               mode_ == Mode::ChildSplashPreview) {
      mode_ = Mode::ChildSplashMenu;
      markDirty();
    } else if (mode_ == Mode::ChildChangePin ||
               mode_ == Mode::ChildChangeConfirm ||
               mode_ == Mode::ChildConfirmDisable) {
      mode_ = Mode::ChildManage;
      childPinEntry_.clear();
      markDirty();
    } else if (mode_ == Mode::ChildSetupConfirm) {
      mode_ = Mode::ChildSetupPin;
      childPinEntry_.clear();
      markDirty();
    } else if (mode_ == Mode::ChildSetupPin) {
      mode_ = Mode::Main;
      markDirty();
    } else if (mode_ == Mode::ChildAuth) {
      mode_ = parentAction_ == ParentAction::OpenManagement
                  ? Mode::Main
                  : Mode::Saves;
      childPinEntry_.clear();
      markDirty();
    } else if (mode_ == Mode::ChildDuration) {
      if (pendingInitialEnable_) {
        kidModeService_.setEnabled(true);
        pendingInitialEnable_ = false;
      }
      kidModeService_.lockNow();
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

  if (childPinMode() || mode_ == Mode::ChildManage ||
      mode_ == Mode::ChildSplashMenu || mode_ == Mode::ChildSplashText ||
      mode_ == Mode::ChildSplashPalette ||
      mode_ == Mode::ChildSplashPreview || mode_ == Mode::ChildDuration ||
      mode_ == Mode::ChildConfirmDisable) {
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
        textSize_ = CydUi::loadTextSize();
        mode_ = Mode::TextSize;
        markDirty();
      } else if (mainIndex_ == 2) {
        const auto config = kidModeStorage_.load();
        if (config.enabled) beginParentAuth(ParentAction::OpenManagement);
        else beginChildSetup();
        markDirty();
      } else if (mainIndex_ == 3) {
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
        if (kidModeStorage_.load().enabled) {
          beginParentAuth(ParentAction::DeleteAll);
        } else {
          mode_ = Mode::ConfirmAll1;
        }
        markDirty();
      } else if (strcmp(SAVE_ENTRIES[saveIndex_].ns,
                        KidMode::Storage::NAMESPACE) == 0 &&
                 kidModeStorage_.load().enabled) {
        beginParentAuth(ParentAction::DeleteChildMode);
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
      textSize_ = CydUi::nextTextSize(textSize_);
      markDirty();
    }
    if (b1.longPress) {
      CydUi::saveTextSize(textSize_);
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
      message_ = "User saved";
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
    canvas.drawString("User Initials", 20, 50);
    canvas.drawString(dotted, 112, 50);
    canvas.setTextSize(1);
    canvas.drawString("Initials and save manager", 20, 80);
    drawFooter(canvas, width, height, "Tap an option");
  });
  startDirty_ = false;
}

void OptionsApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }
  const bool fullRedraw = !runningRendered_ || renderedMode_ != mode_;
  (void)fullRedraw;

  if (childPinMode()) {
    const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
    drawBuffered(tft, width, height, [&](auto& canvas) {
      if (childAuthLockout_.active(nowUs)) {
        CydKidModeUi::drawLockout(
            canvas, childAuthLockout_.remainingSeconds(nowUs));
        return;
      }

      const char* title = "Parent PIN";
      const char* submit = "Unlock";
      if (mode_ == Mode::ChildSetupPin) {
        title = "Create PIN";
        submit = "Next";
      } else if (mode_ == Mode::ChildSetupConfirm) {
        title = "Confirm PIN";
        submit = "Enable";
      } else if (mode_ == Mode::ChildChangePin) {
        title = "New PIN";
        submit = "Next";
      } else if (mode_ == Mode::ChildChangeConfirm) {
        title = "Confirm PIN";
        submit = "Save";
      }
      CydKidModeUi::drawPin(
          canvas, title, childPrompt_.c_str(), childPinEntry_, submit,
          touchCapture_.activeId());
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    renderedLockoutSeconds_ = childAuthLockout_.remainingSeconds(nowUs);
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ChildDuration) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydKidModeUi::drawDurations(canvas, touchCapture_.activeId());
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ChildManage) {
    const String status = childStatus(static_cast<uint64_t>(esp_timer_get_time()));
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydKidModeUi::drawManagement(canvas, status,
                                   childSplashSettings_.enabled,
                                   touchCapture_.activeId());
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ChildSplashMenu) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydKidModeUi::drawSplashMenu(canvas, childSplashSettings_,
                                   touchCapture_.activeId());
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ChildSplashText) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydKidModeUi::drawTextEditor(canvas, childSplashTextEditor_,
                                   childSplashShift_,
                                   touchCapture_.activeId());
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ChildSplashPalette) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydKidModeUi::drawPalettePicker(canvas, pendingSplashPalette_,
                                      touchCapture_.activeId());
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ChildSplashPreview) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydKidModeUi::drawCustomSplash(canvas, childSplashSettings_, height,
                                     false);
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ChildConfirmDisable) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydKidModeUi::drawDisableConfirmation(canvas,
                                            touchCapture_.activeId());
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::Main) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydUi::header(canvas, "Options", TFT_ORANGE);
      for (uint8_t i = 0; i < MAIN_COUNT; ++i) {
        CydUi::touchListRow(canvas, mainRow(i), MAIN_ITEMS[i],
                            i == mainIndex_, TFT_GREEN, nullptr,
                            touchCapture_.activeId() == i);
      }
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::Saves) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      String page = String(savePage_ + 1) + "/" + String(SAVE_PAGE_COUNT);
      CydUi::header(canvas, "Save Manager", TFT_ORANGE, page.c_str());
      for (uint8_t row = 0; row < VISIBLE_SAVE_ROWS; ++row) {
        const uint8_t index = savePage_ * VISIBLE_SAVE_ROWS + row;
        if (index >= SAVE_MENU_COUNT) break;
        const bool destructive = index == SAVE_ALL_INDEX || index < SAVE_COUNT;
        CydUi::touchListRow(canvas, saveRow(row), saveLabel(index),
                            index == saveIndex_,
                            destructive ? TFT_RED : TFT_BLUE,
                            index == SAVE_BACK_INDEX ? "Open" : "Delete",
                            touchCapture_.activeId() == row);
      }
      CydUi::touchAction(canvas, PAGE_PREV, "Prev", TFT_BLUE, false,
                         touchCapture_.activeId() == 4);
      CydUi::touchAction(canvas, PAGE_NEXT, "Next", TFT_BLUE, false,
                         touchCapture_.activeId() == 5);
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    renderedSaveIndex_ = saveIndex_;
    renderedSaveScroll_ = savePage_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::TextSize) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      CydUi::header(canvas, "Text Size", TFT_ORANGE);
      CydUi::touchAction(canvas, COMPACT_CHOICE, "Compact", TFT_CYAN,
                         textSize_ == CydUi::TextSize::Compact,
                         touchCapture_.activeId() == 0);
      CydUi::touchAction(canvas, LARGE_CHOICE, "Large", TFT_CYAN,
                         textSize_ == CydUi::TextSize::Large,
                         touchCapture_.activeId() == 1);
      CydUi::touchAction(canvas, ACTION_CANCEL, "Cancel", TFT_LIGHTGREY,
                         false, touchCapture_.activeId() == 4);
      CydUi::touchAction(canvas, ACTION_SAVE, "Save", TFT_GREEN, true,
                         touchCapture_.activeId() == 5);
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    renderedTextSize_ = textSize_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::ConfirmOne || mode_ == Mode::ConfirmAll1 || mode_ == Mode::ConfirmAll2) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      String message = mode_ == Mode::ConfirmOne
          ? String("Delete ") + SAVE_ENTRIES[saveIndex_].label + "?"
          : (mode_ == Mode::ConfirmAll1
                 ? String("Delete every saved score and state?")
                 : String("Really delete every save?"));
      CydUi::confirmation(canvas,
                          mode_ == Mode::ConfirmAll2 ? "Final confirmation"
                                                     : "Delete save",
                          message, CONFIRM_CANCEL, CONFIRM_DELETE, "Delete");
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
      CydUi::touchAction(canvas, CONTINUE_ACTION, "Continue", TFT_GREEN, true,
                         touchCapture_.activeId() == 0);
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  drawBuffered(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "User Initials", TFT_ORANGE);
    for (uint8_t i = 0; i < 2; ++i) {
      char label[2] = {initials_[i], '\0'};
      CydUi::touchAction(canvas, INITIAL_CHARS[i], label, TFT_CYAN,
                         selected_ == i, touchCapture_.activeId() == i);
    }
    CydUi::touchAction(canvas, INITIAL_MINUS, "-", TFT_CYAN, false,
                       touchCapture_.activeId() == 2);
    CydUi::touchAction(canvas, INITIAL_PLUS, "+", TFT_CYAN, false,
                       touchCapture_.activeId() == 3);
    CydUi::touchAction(canvas, ACTION_CANCEL, "Cancel", TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == 4);
    CydUi::touchAction(canvas, ACTION_SAVE, "Save", TFT_GREEN, true,
                       touchCapture_.activeId() == 5);
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
    drawFooter(canvas, width, height, "Use Back to return");
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
    if (strcmp(SAVE_ENTRIES[saveIndex_].ns,
               KidMode::Storage::NAMESPACE) == 0) {
      clearChildEntry();
    } else {
      clearNamespace(SAVE_ENTRIES[saveIndex_].ns);
    }
  }
}

void OptionsApp::clearAllSaves() {
  for (uint8_t i = 0; i < SAVE_COUNT; i++) {
    if (strcmp(SAVE_ENTRIES[i].ns, KidMode::Storage::NAMESPACE) == 0) {
      clearChildEntry();
    } else {
      clearNamespace(SAVE_ENTRIES[i].ns);
    }
  }
}

void OptionsApp::beginChildSetup() {
  childPinEntry_.clear();
  childAuthLockout_.clear();
  pendingPin_[0] = '\0';
  childPrompt_ = "Choose a 6-digit parent PIN";
  pendingInitialEnable_ = false;
  mode_ = Mode::ChildSetupPin;
}

void OptionsApp::beginParentAuth(ParentAction action) {
  parentAction_ = action;
  childPinEntry_.clear();
  childAuthLockout_.clear();
  childPrompt_ = "Enter the parent PIN";
  renderedLockoutSeconds_ = UINT32_MAX;
  mode_ = Mode::ChildAuth;
}

bool OptionsApp::childPinMode() const {
  return mode_ == Mode::ChildSetupPin ||
         mode_ == Mode::ChildSetupConfirm || mode_ == Mode::ChildAuth ||
         mode_ == Mode::ChildChangePin ||
         mode_ == Mode::ChildChangeConfirm;
}

void OptionsApp::handleChildPinAction(int16_t id, uint64_t nowUs) {
  if (childAuthLockout_.active(nowUs)) return;
  if (id >= 0 && id <= 9) {
    childPinEntry_.append(static_cast<uint8_t>(id));
    return;
  }
  if (id == CydKidModeUi::PIN_BACKSPACE) {
    childPinEntry_.backspace();
    return;
  }
  if (id != CydKidModeUi::PIN_SUBMIT || !childPinEntry_.complete()) return;

  if (mode_ == Mode::ChildSetupPin || mode_ == Mode::ChildChangePin) {
    memcpy(pendingPin_, childPinEntry_.value(), KidMode::PIN_LENGTH + 1);
    const bool setup = mode_ == Mode::ChildSetupPin;
    childPinEntry_.clear();
    childPrompt_ = "Enter the same PIN again";
    mode_ = setup ? Mode::ChildSetupConfirm : Mode::ChildChangeConfirm;
    return;
  }

  if (mode_ == Mode::ChildSetupConfirm ||
      mode_ == Mode::ChildChangeConfirm) {
    const bool setup = mode_ == Mode::ChildSetupConfirm;
    if (strcmp(pendingPin_, childPinEntry_.value()) != 0) {
      pendingPin_[0] = '\0';
      childPinEntry_.clear();
      childPrompt_ = "PINs did not match - try again";
      mode_ = setup ? Mode::ChildSetupPin : Mode::ChildChangePin;
      return;
    }
    if (!kidModeStorage_.configureAndEnable(pendingPin_)) {
      pendingPin_[0] = '\0';
      childPinEntry_.clear();
      childPrompt_ = "Could not save PIN - try again";
      mode_ = setup ? Mode::ChildSetupPin : Mode::ChildChangePin;
      return;
    }

    pendingPin_[0] = '\0';
    childPinEntry_.clear();
    if (setup) {
      pendingInitialEnable_ = true;
      mode_ = Mode::ChildDuration;
    } else {
      childPrompt_ = "";
      mode_ = Mode::ChildManage;
    }
    return;
  }

  if (mode_ != Mode::ChildAuth) return;
  if (!kidModeStorage_.verify(childPinEntry_.value())) {
    childPinEntry_.clear();
    childAuthLockout_.fail(nowUs);
    renderedLockoutSeconds_ = UINT32_MAX;
    CydHardware::playNotificationBeep();
    return;
  }

  childPinEntry_.clear();
  childAuthLockout_.clear();
  if (parentAction_ == ParentAction::OpenManagement) {
    childSplashSettings_ = kidModeStorage_.loadSplash();
    mode_ = Mode::ChildManage;
  } else if (parentAction_ == ParentAction::DeleteChildMode) {
    mode_ = Mode::ConfirmOne;
  } else {
    mode_ = Mode::ConfirmAll1;
  }
}

bool OptionsApp::saveChildSplash() {
  if (kidModeStorage_.saveSplash(childSplashSettings_)) return true;
  message_ = "Could not save splash";
  messageToMain_ = true;
  mode_ = Mode::Message;
  return false;
}

void OptionsApp::selectChildDuration(uint8_t index, uint64_t nowUs) {
  if (index >= 6) return;
  if (pendingInitialEnable_) {
    kidModeService_.setEnabled(true);
    pendingInitialEnable_ = false;
  }
  constexpr uint16_t MINUTES[] = {10, 20, 30, 60, 120};
  const bool unlocked = index == 5
                            ? kidModeService_.unlockUnlimited()
                            : kidModeService_.unlockForMinutes(MINUTES[index],
                                                               nowUs);
  if (!unlocked) {
    kidModeService_.setEnabled(true);
    kidModeService_.lockNow();
    message_ = "Unable to unlock";
  } else {
    message_ = index == 5 ? "Unlocked until reboot" : "Child Mode active";
  }
  messageToMain_ = true;
  mode_ = Mode::Message;
}

void OptionsApp::clearChildEntry() {
  kidModeStorage_.clear();
  kidModeService_.setEnabled(false);
  pendingInitialEnable_ = false;
}

String OptionsApp::childStatus(uint64_t nowUs) const {
  const auto config = kidModeStorage_.load();
  if (!config.enabled) return "Off";
  if (kidModeService_.isUnlimited()) return "Unlimited";
  if (kidModeService_.state() == KidMode::State::Timed) {
    const uint32_t seconds = kidModeService_.remainingSeconds(nowUs);
    if (seconds < 60) return "<1 min";
    return String((seconds + 59U) / 60U) + " min";
  }
  return "Locked";
}

void OptionsApp::onAppExit() {
  if (pendingInitialEnable_) {
    kidModeService_.setEnabled(true);
    kidModeService_.lockNow();
    pendingInitialEnable_ = false;
  }
  childPinEntry_.clear();
  childAuthLockout_.clear();
  pendingPin_[0] = '\0';
  touchCapture_.reset();
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

char OptionsApp::previousInitial(char value) const {
  value = PlayerProfile::normalizeInitial(value);
  if (value > 'A' && value <= 'Z') return static_cast<char>(value - 1);
  if (value == 'A') return '9';
  if (value > '0' && value <= '9') return static_cast<char>(value - 1);
  return 'Z';
}
