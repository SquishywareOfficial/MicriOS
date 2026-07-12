#include "CreditsApp.h"

#include <U8g2lib.h>

#include "../shared/Version.h"

namespace {
constexpr uint8_t ABOUT_ITEM_COUNT = 2;
constexpr uint8_t LICENSE_PAGE_COUNT = 6;

struct CreditPage {
  const char* game;
  const char* author1;
  const char* author2;
};

const char* const ABOUT_ITEMS[ABOUT_ITEM_COUNT] = {
    "License",
    "Credits",
};

const CreditPage CREDIT_PAGES[] = {
    {"MicriOS", "thedarkfalcon", nullptr},
    {"Inspired by", "atomic14", nullptr},
    {"Breakout '76", "thedarkfalcon", nullptr},
    {"City Racer", "thedarkfalcon", nullptr},
    {"Alien Raiders", "thedarkfalcon", nullptr},
    {"Cave Chopper", "thedarkfalcon", nullptr},
    {"Tower Stacker", "thedarkfalcon", nullptr},
    {"Tiny Golf", "thedarkfalcon", nullptr},
    {"Mini Lander", "thedarkfalcon", nullptr},
    {"Need Speed", "thedarkfalcon", nullptr},
    {"Micri Field", "thedarkfalcon", nullptr},
    {"Noon Shooter", "thedarkfalcon", nullptr},
    {"Fishing Flick", "thedarkfalcon", nullptr},
    {"Maze Runner", "thedarkfalcon", nullptr},
    {"Maze Collector", "thedarkfalcon", nullptr},
    {"Pipe Mania", "thedarkfalcon", nullptr},
    {"Blackjack", "thedarkfalcon", nullptr},
    {"Knife Throw", "thedarkfalcon", nullptr},
    {"Reactor", "thedarkfalcon", nullptr},
    {"Simon", "thedarkfalcon", nullptr},
    {"Pet Simulator", "thedarkfalcon", nullptr},
    {"Micri Clock", "thedarkfalcon", nullptr},
    {"Micri Miner", "thedarkfalcon", nullptr},
    {"Distributed Miner", "thedarkfalcon", nullptr},
    {"Counter", "thedarkfalcon", nullptr},
    {"Mouse Emulator", "thedarkfalcon", nullptr},
    {"Reading", "thedarkfalcon", nullptr},
    {"Screen Saver", "thedarkfalcon", nullptr},
    {"Stopwatch", "thedarkfalcon", nullptr},
    {"Countdown", "thedarkfalcon", nullptr},
    {"Dice Roller", "thedarkfalcon", nullptr},
    {"Coin Flipper", "thedarkfalcon", nullptr},
    {"Random Number", "thedarkfalcon", nullptr},
    {"Metronome", "thedarkfalcon", nullptr},
    {"ESP Contacts", "thedarkfalcon", nullptr},
    {"Options", "thedarkfalcon", nullptr},
    {"About", "thedarkfalcon", nullptr},
};
constexpr uint8_t CREDIT_PAGE_COUNT = sizeof(CREDIT_PAGES) / sizeof(CREDIT_PAGES[0]);

void drawGithubHandle(U8G2& u8g2, int x, int y, const char* handle) {
  if (handle == nullptr) {
    return;
  }
  if (handle[0] == 't') {
    u8g2.drawStr(x, y, "github:");
    u8g2.drawStr(x, y + 7, handle);
  } else {
    u8g2.drawStr(x, y, "github:");
    u8g2.drawStr(x + 28, y, handle);
  }
}
}

CreditsApp::CreditsApp(uint32_t width, uint32_t height)
    : App("About", width, height) {}

bool CreditsApp::hasCustomOverlay() const {
  return true;
}

void CreditsApp::onAppReset() {
  mode_ = Mode::Select;
  selection_ = 0;
  page_ = 0;
}

void CreditsApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;

  if (mode_ == Mode::Select) {
    if (input.click) {
      selection_ = (selection_ + 1) % ABOUT_ITEM_COUNT;
    } else if (input.longPress) {
      mode_ = selection_ == 0 ? Mode::License : Mode::Credits;
      page_ = 0;
    }
    return;
  }

  if (input.longPress) {
    requestExitToMenu();
    return;
  }

  if (input.click) {
    page_++;
    const uint8_t pageCount = mode_ == Mode::License ? LICENSE_PAGE_COUNT : CREDIT_PAGE_COUNT;
    if (page_ >= pageCount) {
      mode_ = Mode::Select;
      page_ = 0;
    }
  }
}

void CreditsApp::drawRunning(U8G2& u8g2) {
  switch (mode_) {
    case Mode::License:
      drawLicense(u8g2);
      break;
    case Mode::Credits:
      drawCredits(u8g2);
      break;
    case Mode::Select:
    default:
      drawSelect(u8g2);
      break;
  }
}

void CreditsApp::drawSelect(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 8, "About");
  u8g2.setCursor(width - 14, 8);
  u8g2.print(selection_ + 1);
  u8g2.print("/");
  u8g2.print(ABOUT_ITEM_COUNT);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 22, ABOUT_ITEMS[selection_]);
  u8g2.drawStr(3, 32, "Tap next");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 39, "Hold open");
}

void CreditsApp::drawLicense(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);

  switch (page_) {
    case 0:
      u8g2.drawStr(3, 9, "License");
      u8g2.drawStr(3, 22, "WTFPL +");
      u8g2.drawStr(3, 34, "No Warranty");
      break;
    case 1:
      u8g2.drawStr(3, 9, "Copyright");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(3, 20, "2026");
      u8g2.drawStr(3, 29, "github:");
      u8g2.drawStr(3, 38, "thedarkfalcon");
      break;
    case 2:
      u8g2.drawStr(3, 9, "Repo");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(3, 20, "github.com");
      u8g2.drawStr(3, 29, "Squishyware");
      u8g2.drawStr(3, 38, "Official/MicriOS");
      break;
    case 3:
      u8g2.drawStr(3, 9, "Permission");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(3, 20, "copy modify");
      u8g2.drawStr(3, 29, "publish sell");
      u8g2.drawStr(3, 38, "as you want");
      break;
    case 4:
      u8g2.drawStr(3, 9, "MIT Parts");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(3, 20, "NerdMiner");
      u8g2.drawStr(3, 29, "mining code");
      u8g2.drawStr(3, 38, "uses MIT");
      break;
    default:
      u8g2.drawStr(3, 9, "No Warranty");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(3, 20, "provided as-is");
      u8g2.drawStr(3, 29, "use at your");
      u8g2.drawStr(3, 38, "own risk");
      break;
  }
}

void CreditsApp::drawCredits(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  const CreditPage& credit = CREDIT_PAGES[page_];

  u8g2.setFont(u8g2_font_5x8_tr);
  if (u8g2.getStrWidth(credit.game) > static_cast<int>(width - 6)) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 8, credit.game);
  } else {
    u8g2.drawStr(3, 9, credit.game);
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (credit.author2 != nullptr) {
    u8g2.drawStr(3, 18, "Created:atomic14");
    u8g2.drawStr(3, 29, "Modified:");
    u8g2.drawStr(3, 38, credit.author2);
  } else if (credit.author1[0] != 't') {
    u8g2.drawStr(3, 19, "Developer");
    drawGithubHandle(u8g2, 3, 28, credit.author1);
  } else {
    u8g2.drawStr(3, 19, "Developer");
    drawGithubHandle(u8g2, 3, 28, credit.author1);
  }
}

void CreditsApp::drawStart(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 10, "About");
  u8g2.drawStr(3, 24, "Tap to view");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 36, BuildInfo::BUILD_TEXT);
}

void CreditsApp::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 12, "About");
  u8g2.drawStr(3, 26, "Tap replay");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 32, BuildInfo::BUILD_TEXT);
  u8g2.drawStr(3, 38, "Hold menu");
}
