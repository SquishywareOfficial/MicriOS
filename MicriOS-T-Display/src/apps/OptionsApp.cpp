#include "OptionsApp.h"

#include <Preferences.h>
#include <TFT_eSPI.h>
#include <time.h>

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
    {"Cluster App", "distminer"},
    {"Contacts", "contacts"},
    {"Power", "power"},
};

constexpr uint8_t SAVE_COUNT = sizeof(SAVE_ENTRIES) / sizeof(SAVE_ENTRIES[0]);
constexpr uint8_t SAVE_ALL_INDEX = SAVE_COUNT;
constexpr uint8_t SAVE_BACK_INDEX = SAVE_COUNT + 1;
constexpr uint8_t SAVE_MENU_COUNT = SAVE_COUNT + 2;
const char* MAIN_ITEMS[] = {
    "User Initials", "Text Size", "Save Manager", "Power Settings",
    "Brightness", "Exit"};
constexpr uint8_t MAIN_COUNT = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);
const char* POWER_ITEMS[] = {
    "Battery Installed", "Battery Status", "Battery Runtime",
    "Set Full Voltage", "Back"};
constexpr uint8_t POWER_COUNT = sizeof(POWER_ITEMS) / sizeof(POWER_ITEMS[0]);
constexpr uint8_t VISIBLE_SAVE_ROWS = 4;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;

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

void formatMinutes(uint32_t minutes, char* output, size_t outputSize) {
  if (minutes >= 1440) {
    snprintf(output, outputSize, "%lud %luh",
             static_cast<unsigned long>(minutes / 1440),
             static_cast<unsigned long>((minutes % 1440) / 60));
  } else {
    snprintf(output, outputSize, "%luh %lum",
             static_cast<unsigned long>(minutes / 60),
             static_cast<unsigned long>(minutes % 60));
  }
}

bool formatLocalBootTime(const ClockLogic& clock, char* output,
                         size_t outputSize) {
  const time_t now = time(nullptr);
  if (now < VALID_TIME_THRESHOLD) {
    snprintf(output, outputSize, "unavailable");
    return false;
  }

  const uint64_t uptimeSeconds = TDisplayPower::bootUptimeSeconds();
  const time_t bootEpoch =
      now - static_cast<time_t>(uptimeSeconds);
  const ClockLogic::TimeParts boot = clock.localParts(bootEpoch);
  snprintf(output, outputSize, "%02u/%02u %02u:%02u %s",
           boot.day, boot.month, boot.hour, boot.minute, clock.zoneLabel());
  return true;
}

const char* powerSourceLabel(BatterySessionLogic::PowerSource source) {
  switch (source) {
    case BatterySessionLogic::PowerSource::Battery:
      return "Battery";
    case BatterySessionLogic::PowerSource::Usb:
      return "USB";
    case BatterySessionLogic::PowerSource::Unknown:
    default:
      return "Unknown";
  }
}
}

OptionsApp::OptionsApp(uint32_t width, uint32_t height)
    : App("Options", width, height) {}

void OptionsApp::setEntryPoint(EntryPoint entryPoint) {
  requestedEntryPoint_ = entryPoint;
}

bool OptionsApp::startsRunningImmediately() const {
  return true;
}

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
  directEntry_ = requestedEntryPoint_ != EntryPoint::Main;
  switch (requestedEntryPoint_) {
    case EntryPoint::Initials:
      mode_ = Mode::Initials;
      break;
    case EntryPoint::TextSize:
      mode_ = Mode::TextSize;
      break;
    case EntryPoint::Saves:
      mode_ = Mode::Saves;
      break;
    case EntryPoint::Power:
      mode_ = Mode::Power;
      break;
    case EntryPoint::Brightness:
      mode_ = Mode::Brightness;
      break;
    case EntryPoint::Main:
    default:
      mode_ = Mode::Main;
      break;
  }
  requestedEntryPoint_ = EntryPoint::Main;
  selected_ = 0;
  mainIndex_ = 0;
  powerIndex_ = 0;
  saveIndex_ = 0;
  textSize_ = TDisplayUi::loadTextSize();
  message_ = "";
  messageToMain_ = false;
  messageToPower_ = false;
  batteryInstalled_ = TDisplayPower::isBatteryInstalled();
  clockSettings_.begin();
  brightnessLevel_ = TDisplayPower::loadBrightnessLevel();
  telemetryRefreshMs_ = 0;
  if (mode_ == Mode::Brightness) {
    TDisplayPower::applyBrightnessLevel(brightnessLevel_);
  }
  runningRendered_ = false;
  renderedSaveIndex_ = 255;
  renderedSaveScroll_ = 255;
  markDirty();
}

void OptionsApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  if (mode_ == Mode::Battery || mode_ == Mode::BatteryRuntime) {
    telemetryRefreshMs_ += deltaMs;
    if (telemetryRefreshMs_ >= 60000) {
      telemetryRefreshMs_ = 0;
      if (mode_ == Mode::Battery) {
        const TDisplayPower::BatteryReading latest =
            TDisplayPower::latestBatteryReading();
        if (latest.valid) {
          batteryReading_ = latest;
        }
      }
      markDirty();
    }
  }

  if (b2.click) {
    if (mode_ == Mode::Brightness) {
      TDisplayPower::applyBrightnessLevel(TDisplayPower::loadBrightnessLevel());
      if (directEntry_) {
        requestExitToMenu();
      } else {
        mode_ = Mode::Main;
        markDirty();
      }
      return;
    }
    if (mode_ == Mode::Main) {
      requestExitToMenu();
    } else if (mode_ == Mode::Power || mode_ == Mode::Saves ||
               mode_ == Mode::Initials || mode_ == Mode::TextSize) {
      if (directEntry_) {
        requestExitToMenu();
      } else {
        mode_ = Mode::Main;
        markDirty();
      }
    } else if (mode_ == Mode::BatteryInstalled || mode_ == Mode::Battery ||
               mode_ == Mode::BatteryRuntime ||
               mode_ == Mode::FullVoltage) {
      mode_ = Mode::Power;
      markDirty();
    } else if (mode_ == Mode::Message) {
      if (directEntry_ && messageToMain_) {
        requestExitToMenu();
      } else {
        mode_ = messageToPower_ ? Mode::Power
                                : (messageToMain_ ? Mode::Main : Mode::Saves);
        markDirty();
      }
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
      } else if (mainIndex_ == 3) {
        powerIndex_ = 0;
        batteryInstalled_ = TDisplayPower::isBatteryInstalled();
        mode_ = Mode::Power;
        markDirty();
      } else if (mainIndex_ == 4) {
        brightnessLevel_ = TDisplayPower::loadBrightnessLevel();
        TDisplayPower::applyBrightnessLevel(brightnessLevel_);
        mode_ = Mode::Brightness;
        markDirty();
      } else {
        requestExitToMenu();
      }
    }
    return;
  }

  if (mode_ == Mode::Power) {
    if (b1.click) {
      powerIndex_ = (powerIndex_ + 1) % POWER_COUNT;
      markDirty();
    }
    if (b1.longPress) {
      if (powerIndex_ == 0) {
        batteryInstalled_ = TDisplayPower::isBatteryInstalled();
        mode_ = Mode::BatteryInstalled;
      } else if (powerIndex_ >= 1 && powerIndex_ <= 3) {
        if (batteryInstalled_) {
          telemetryRefreshMs_ = 0;
          if (powerIndex_ == 1) {
            batteryReading_ = TDisplayPower::readBattery();
            mode_ = Mode::Battery;
          } else if (powerIndex_ == 2) {
            TDisplayPower::readBattery();
            mode_ = Mode::BatteryRuntime;
          } else {
            batteryReading_ = TDisplayPower::readBattery();
            mode_ = Mode::FullVoltage;
          }
        } else {
          message_ = "Battery disabled";
          messageToMain_ = false;
          messageToPower_ = true;
          mode_ = Mode::Message;
        }
      } else {
        if (directEntry_) {
          requestExitToMenu();
          return;
        }
        mode_ = Mode::Main;
      }
      markDirty();
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
        if (directEntry_) {
          requestExitToMenu();
        } else {
          mode_ = Mode::Main;
          markDirty();
        }
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
      messageToPower_ = false;
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
      messageToPower_ = false;
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
      messageToPower_ = false;
      mode_ = Mode::Message;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Message) {
    if (b1.click || b1.longPress) {
      if (directEntry_ && messageToMain_) {
        requestExitToMenu();
      } else {
        mode_ = messageToPower_ ? Mode::Power
                                : (messageToMain_ ? Mode::Main : Mode::Saves);
        markDirty();
      }
    }
    return;
  }

  if (mode_ == Mode::BatteryInstalled) {
    if (b1.click) {
      batteryInstalled_ = !batteryInstalled_;
      markDirty();
    }
    if (b1.longPress) {
      TDisplayPower::setBatteryInstalled(batteryInstalled_);
      message_ = batteryInstalled_ ? "Battery enabled" : "Battery disabled";
      messageToMain_ = false;
      messageToPower_ = true;
      mode_ = Mode::Message;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Battery) {
    if (b1.longPress) {
      batteryReading_ = TDisplayPower::readBattery();
      if (batteryReading_.valid) {
        mode_ = Mode::FullVoltage;
      }
      markDirty();
    } else if (b1.click) {
      batteryReading_ = TDisplayPower::readBattery();
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::BatteryRuntime) {
    if (b1.click || b1.longPress) {
      TDisplayPower::readBattery();
      telemetryRefreshMs_ = 0;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::FullVoltage) {
    if (b1.click) {
      batteryReading_ = TDisplayPower::readBattery();
      markDirty();
    }
    if (b1.longPress) {
      if (batteryReading_.valid &&
          batteryReading_.millivolts >= TDisplayPower::MIN_FULL_VOLTAGE_MV &&
          batteryReading_.millivolts <= TDisplayPower::MAX_FULL_VOLTAGE_MV) {
        TDisplayPower::saveFullVoltageMv(batteryReading_.millivolts);
        message_ = "Full voltage saved";
      } else {
        message_ = "Voltage out of range";
      }
      messageToMain_ = false;
      messageToPower_ = true;
      mode_ = Mode::Message;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Brightness) {
    if (b1.click) {
      brightnessLevel_ =
          brightnessLevel_ >= TDisplayPower::MAX_BRIGHTNESS_LEVEL
              ? TDisplayPower::MIN_BRIGHTNESS_LEVEL
              : brightnessLevel_ + 1;
      TDisplayPower::applyBrightnessLevel(brightnessLevel_);
      markDirty();
    }
    if (b1.longPress) {
      TDisplayPower::saveBrightnessLevel(brightnessLevel_);
      message_ = "Brightness saved";
      messageToMain_ = true;
      messageToPower_ = false;
      mode_ = Mode::Message;
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
      messageToPower_ = false;
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

  if (mode_ == Mode::Power) {
    const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
    const uint8_t first =
        TDisplayUi::menuScrollOffset(powerIndex_, POWER_COUNT,
                                     TDisplayUi::menuStyle(textSize));
    drawBuffered(tft, width, height, [&](auto& canvas) {
      TDisplayUi::menuFrame(
          canvas, "Power Settings", powerIndex_, POWER_COUNT, first,
          [](uint8_t index) -> const char* { return POWER_ITEMS[index]; },
          "B1 next/open  B2 back", textSize, TFT_CYAN);
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    renderedTextSize_ = textSize;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::BatteryInstalled) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      drawShell(canvas, width, height, "Battery Installed");
      canvas.setTextSize(3);
      canvas.setTextColor(batteryInstalled_ ? TFT_GREEN : TFT_RED, TFT_BLACK);
      const char* state = batteryInstalled_ ? "YES" : "NO";
      canvas.drawString(state, (width - canvas.textWidth(state)) / 2, 48);
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      canvas.drawString("Enables battery status and sleep.", 25, 88);
      drawFooter(canvas, width, height, "B1 toggle  B1 hold save  B2 back");
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::Battery) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      drawShell(canvas, width, height, "Battery Status");
      if (batteryReading_.valid) {
        char voltage[12];
        char percent[8];
        snprintf(voltage, sizeof(voltage), "%u.%02u V",
                 batteryReading_.millivolts / 1000,
                 (batteryReading_.millivolts % 1000) / 10);
        snprintf(percent, sizeof(percent), "%u%%", batteryReading_.percent);
        const uint16_t fullVoltageMv = TDisplayPower::loadFullVoltageMv();

        canvas.setTextSize(3);
        canvas.setTextColor(TFT_CYAN, TFT_BLACK);
        canvas.drawString(voltage, 18, 45);
        canvas.setTextSize(2);
        canvas.setTextColor(
            batteryReading_.percent <= 15 ? TFT_RED :
            batteryReading_.percent <= 30 ? TFT_YELLOW : TFT_GREEN,
            TFT_BLACK);
        canvas.drawString(percent, 164, 52);
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        canvas.drawString("Estimated from battery/charge rail", 18, 83);
        char calibration[38];
        snprintf(calibration, sizeof(calibration),
                 "Full %u.%02u V; USB may affect reading",
                 fullVoltageMv / 1000, (fullVoltageMv % 1000) / 10);
        canvas.drawString(calibration, 18, 97);
      } else {
        canvas.setTextSize(2);
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.drawString("Unavailable", 56, 51);
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        canvas.drawString("Check battery hardware/revision.", 30, 84);
      }
      drawFooter(canvas, width, height, "B1 refresh  Hold set full  B2 back");
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::BatteryRuntime) {
    const BatterySessionLogic::Snapshot snapshot =
        TDisplayPower::batterySessionSnapshot();
    drawBuffered(tft, width, height, [&](auto& canvas) {
      drawShell(canvas, width, height, "Battery Runtime");
      canvas.setTextSize(1);

      char boot[18];
      char bootTime[34];
      char current[18];
      char last[18];
      formatMinutes(TDisplayPower::bootUptimeMinutes(), boot, sizeof(boot));
      formatLocalBootTime(clockSettings_, bootTime, sizeof(bootTime));
      formatMinutes(snapshot.currentMinutes, current, sizeof(current));
      formatMinutes(snapshot.lastMinutes, last, sizeof(last));

      char line[42];
      canvas.setTextColor(
          snapshot.source == BatterySessionLogic::PowerSource::Battery
              ? TFT_GREEN
              : snapshot.source == BatterySessionLogic::PowerSource::Usb
                    ? TFT_CYAN
                    : TFT_YELLOW,
          TFT_BLACK);
      snprintf(line, sizeof(line), "Power: %s",
               powerSourceLabel(snapshot.source));
      canvas.drawString(line, 14, 38);

      canvas.setTextColor(TFT_CYAN, TFT_BLACK);
      snprintf(line, sizeof(line), "Booted: %s", bootTime);
      canvas.drawString(line, 14, 49);

      canvas.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(line, sizeof(line), "Uptime: %s", boot);
      canvas.drawString(line, 14, 60);

      if (snapshot.currentActive) {
        snprintf(line, sizeof(line), "Current: %s%s", current,
                 snapshot.currentInterrupted ? "*" : "");
        canvas.drawString(line, 14, 71);
        snprintf(line, sizeof(line), "Current low: %u.%02u V%s",
                 snapshot.currentMinimumMillivolts / 1000,
                 (snapshot.currentMinimumMillivolts % 1000) / 10,
                 snapshot.currentInterrupted ? "*" : "");
        canvas.drawString(line, 14, 82);
      } else {
        canvas.drawString("Current: --", 14, 71);
        canvas.drawString("Current low: --", 14, 82);
      }

      if (snapshot.lastValid) {
        const bool marked =
            snapshot.lastInterrupted || !snapshot.lastComplete;
        snprintf(line, sizeof(line), "Last: %s%s", last, marked ? "*" : "");
        canvas.drawString(line, 14, 93);
        snprintf(line, sizeof(line), "Last low: %u.%02u V%s",
                 snapshot.lastMinimumMillivolts / 1000,
                 (snapshot.lastMinimumMillivolts % 1000) / 10,
                 marked ? "*" : "");
        canvas.drawString(line, 14, 104);
      } else {
        canvas.drawString("Last: --", 14, 93);
        canvas.drawString("Last low: --", 14, 104);
      }
      drawFooter(canvas, width, height,
                 snapshot.currentInterrupted ||
                         (snapshot.lastValid &&
                          (snapshot.lastInterrupted ||
                           !snapshot.lastComplete))
                     ? "* interrupted  B1 refresh  B2 back"
                     : "B1 refresh  B2 back");
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::FullVoltage) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      drawShell(canvas, width, height, "Set Full Voltage");
      if (batteryReading_.valid) {
        char prompt[30];
        snprintf(prompt, sizeof(prompt), "%u.%02u V = 100%%?",
                 batteryReading_.millivolts / 1000,
                 (batteryReading_.millivolts % 1000) / 10);
        canvas.setTextSize(2);
        canvas.setTextColor(TFT_CYAN, TFT_BLACK);
        canvas.drawString(prompt, (width - canvas.textWidth(prompt)) / 2, 42);

        const uint16_t savedMv = TDisplayPower::loadFullVoltageMv();
        char saved[24];
        snprintf(saved, sizeof(saved), "Saved: %u.%02u V",
                 savedMv / 1000, (savedMv % 1000) / 10);
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        canvas.drawString(saved, (width - canvas.textWidth(saved)) / 2, 73);
        canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
        canvas.drawString("Set only when fully charged.", 39, 91);
      } else {
        canvas.setTextSize(2);
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.drawString("Unavailable", 56, 52);
      }
      drawFooter(canvas, width, height,
                 "B1 refresh  Hold confirm  B2 cancel");
    });
    runningRendered_ = true;
    renderedMode_ = mode_;
    dirty_ = false;
    return;
  }

  if (mode_ == Mode::Brightness) {
    drawBuffered(tft, width, height, [&](auto& canvas) {
      drawShell(canvas, width, height, "Brightness");
      char levelText[16];
      snprintf(levelText, sizeof(levelText), "Level %u/%u",
               brightnessLevel_, TDisplayPower::MAX_BRIGHTNESS_LEVEL);
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_CYAN, TFT_BLACK);
      canvas.drawString(levelText, (width - canvas.textWidth(levelText)) / 2, 44);

      constexpr int barX = 20;
      constexpr int barY = 78;
      constexpr int gap = 3;
      constexpr int segmentWidth = 17;
      for (uint8_t i = 0; i < TDisplayPower::MAX_BRIGHTNESS_LEVEL; ++i) {
        const uint16_t color = i < brightnessLevel_ ? TFT_YELLOW : TFT_DARKGREY;
        canvas.fillRect(barX + i * (segmentWidth + gap), barY, segmentWidth, 13, color);
      }
      drawFooter(canvas, width, height, "B1 change  B1 hold save  B2 cancel");
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
      if (!messageToMain_ && !messageToPower_) {
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
    if (strcmp(SAVE_ENTRIES[saveIndex_].ns, "power") == 0) {
      TDisplayPower::resetBatteryTelemetry();
      batteryInstalled_ = false;
    }
  }
}

void OptionsApp::clearAllSaves() {
  for (uint8_t i = 0; i < SAVE_COUNT; i++) {
    clearNamespace(SAVE_ENTRIES[i].ns);
  }
  TDisplayPower::resetBatteryTelemetry();
  batteryInstalled_ = false;
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

void OptionsApp::onAppExit() {
  TDisplayPower::applyBrightnessLevel(TDisplayPower::loadBrightnessLevel());
}
