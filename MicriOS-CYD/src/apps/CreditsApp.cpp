#include "CreditsApp.h"

#include <TFT_eSPI.h>

#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
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
constexpr TouchUi::Rect LICENSE_CARD = {18, 58, 136, 92};
constexpr TouchUi::Rect CREDITS_CARD = {166, 58, 136, 92};
constexpr TouchUi::Rect LICENSE_TAB = {12, 39, 145, 27};
constexpr TouchUi::Rect CREDITS_TAB = {163, 39, 145, 27};
constexpr TouchUi::Rect PAGE_PREV = {12, 168, 92, 34};
constexpr TouchUi::Rect PAGE_NEXT = {216, 168, 92, 34};

template <typename Canvas>
void drawGithubHandle(Canvas& tft, int x, int y, const char* handle) {
  if (handle == nullptr) {
    return;
  }
  tft.setTextSize(1);
  if (handle[0] == 't') {
    tft.drawString("github:", x, y);
    tft.drawString(handle, x, y + 12);
  } else {
    tft.drawString("github:", x, y);
    tft.drawString(handle, x + 44, y);
  }
}

template <typename Canvas>
void drawShell(Canvas& tft, uint32_t width, uint32_t height, const char* title, uint8_t page = 0, uint8_t pageCount = 0) {
  CydUi::clear(tft);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(title, 12, 8);

  if (pageCount > 0) {
    char counter[10];
    snprintf(counter, sizeof(counter), "%u/%u", page + 1, pageCount);
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(counter, width - tft.textWidth(counter) - 12, 12);
  }

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
void drawMenuRow(Canvas& tft, uint32_t width, int y, const char* text, bool selected) {
  const uint16_t color = selected ? TFT_GREEN : TFT_DARKGREY;
  tft.fillRect(14, y, width - 28, 27, color);
  tft.setTextSize(2);
  tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE, color);
  tft.drawString(text, 24, y + 6);
}

template <typename Canvas>
void drawBodyLine(Canvas& tft, int x, int y, const char* text, uint16_t color = TFT_WHITE) {
  tft.setTextSize(1);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, x, y);
}

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}
}

CreditsApp::CreditsApp(uint32_t width, uint32_t height)
    : App("About", width, height) {}

bool CreditsApp::hasCustomOverlay() const {
  return true;
}

bool CreditsApp::startsRunningImmediately() const { return true; }

bool CreditsApp::handleTouch(const TouchUi::TouchSample& sample,
                             const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (mode_ == Mode::Select) {
    if (LICENSE_CARD.contains(point)) hit = 0;
    if (CREDITS_CARD.contains(point)) hit = 1;
  } else {
    if (LICENSE_TAB.contains(point)) hit = 0;
    if (CREDITS_TAB.contains(point)) hit = 1;
    if (PAGE_PREV.contains(point)) hit = 2;
    if (PAGE_NEXT.contains(point)) hit = 3;
  }
  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) markDirty();
  if (!result.activated) return result.consumed;

  if (mode_ == Mode::Select) {
    mode_ = result.id == 0 ? Mode::License : Mode::Credits;
    page_ = 0;
  } else if (result.id == 0 || result.id == 1) {
    mode_ = result.id == 0 ? Mode::License : Mode::Credits;
    page_ = 0;
  } else {
    const uint8_t count = mode_ == Mode::License ? LICENSE_PAGE_COUNT
                                                 : CREDIT_PAGE_COUNT;
    if (result.id == 2) page_ = page_ == 0 ? count - 1 : page_ - 1;
    if (result.id == 3) page_ = (page_ + 1) % count;
  }
  touchCapture_.reset();
  markDirty();
  return true;
}

void CreditsApp::render(TFT_eSPI& tft) {
  const AppPhase currentPhase = phase();
  if (!phaseCached_ || currentPhase != renderedPhase_) {
    phaseCached_ = true;
    renderedPhase_ = currentPhase;
    dirty_ = true;
    startDirty_ = true;
    endDirty_ = true;
  }
  App::render(tft);
}

void CreditsApp::markDirty() {
  dirty_ = true;
}

void CreditsApp::onAppReset() {
  mode_ = Mode::Select;
  selection_ = 0;
  page_ = 0;
  touchCapture_.reset();
  markDirty();
}

void CreditsApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;

  if (b2.click) {
    if (mode_ == Mode::Select) {
      requestExitToMenu();
    } else {
      mode_ = Mode::Select;
      page_ = 0;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Select) {
    if (b1.click) {
      selection_ = (selection_ + 1) % ABOUT_ITEM_COUNT;
      markDirty();
    } else if (b1.longPress) {
      mode_ = selection_ == 0 ? Mode::License : Mode::Credits;
      page_ = 0;
      markDirty();
    }
    return;
  }

  if (b1.longPress) {
    requestExitToMenu();
    return;
  }

  if (b1.click) {
    page_++;
    const uint8_t pageCount = mode_ == Mode::License ? LICENSE_PAGE_COUNT : CREDIT_PAGE_COUNT;
    if (page_ >= pageCount) {
      mode_ = Mode::Select;
      page_ = 0;
    }
    markDirty();
  }
}

void CreditsApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }
  switch (mode_) {
    case Mode::License:
      drawLicense(tft);
      break;
    case Mode::Credits:
      drawCredits(tft);
      break;
    case Mode::Select:
    default:
      drawSelect(tft);
      break;
  }
  dirty_ = false;
}

void CreditsApp::drawSelect(TFT_eSPI& tft) {
  drawBuffered(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "About", TFT_ORANGE, BuildInfo::BUILD_TEXT);
    CydUi::touchCard(canvas, LICENSE_CARD, "License", CydIcons::Icon::About,
                     TFT_CYAN);
    CydUi::touchCard(canvas, CREDITS_CARD, "Credits", CydIcons::Icon::About,
                     TFT_GREEN);
  });
}

void CreditsApp::drawLicense(TFT_eSPI& tft) {
  drawBuffered(tft, width, height, [&](auto& canvas) {
  drawShell(canvas, width, height, "About", page_, LICENSE_PAGE_COUNT);
  CydUi::touchTab(canvas, LICENSE_TAB, "License", true, TFT_CYAN,
                  touchCapture_.activeId() == 0);
  CydUi::touchTab(canvas, CREDITS_TAB, "Credits", false, TFT_GREEN,
                  touchCapture_.activeId() == 1);
  switch (page_) {
    case 0:
      drawBodyLine(canvas, 18, 78, "WTFPL + No Warranty", TFT_GREEN);
      drawBodyLine(canvas, 18, 98, "Do what you want.");
      drawBodyLine(canvas, 18, 114, "Use at your own risk.");
      break;
    case 1:
      drawBodyLine(canvas, 18, 78, "Copyright 2026");
      drawBodyLine(canvas, 18, 98, "github:");
      drawBodyLine(canvas, 72, 98, "thedarkfalcon", TFT_GREEN);
      break;
    case 2:
      drawBodyLine(canvas, 18, 76, "Repository");
      drawBodyLine(canvas, 18, 94, "github.com/");
      drawBodyLine(canvas, 18, 110, "SquishywareOfficial", TFT_GREEN);
      drawBodyLine(canvas, 18, 126, "/MicriOS", TFT_GREEN);
      break;
    case 3:
      drawBodyLine(canvas, 18, 78, "Permission:");
      drawBodyLine(canvas, 18, 98, "copy, modify, publish,");
      drawBodyLine(canvas, 18, 114, "sell, and share freely.");
      break;
    case 4:
      drawBodyLine(canvas, 18, 76, "NerdMiner-derived");
      drawBodyLine(canvas, 18, 94, "miner code uses MIT.", TFT_GREEN);
      drawBodyLine(canvas, 18, 112, "See licenses/");
      drawBodyLine(canvas, 18, 128, "NerdMiner_v2-MIT.txt");
      break;
    default:
      drawBodyLine(canvas, 18, 78, "No warranty.");
      drawBodyLine(canvas, 18, 98, "Provided as-is.");
      drawBodyLine(canvas, 18, 114, "No fitness guarantee.");
      break;
  }
  CydUi::touchAction(canvas, PAGE_PREV, "Previous", TFT_BLUE, false,
                     touchCapture_.activeId() == 2);
  CydUi::touchAction(canvas, PAGE_NEXT, "Next", TFT_BLUE, false,
                     touchCapture_.activeId() == 3);
  });
}

void CreditsApp::drawCredits(TFT_eSPI& tft) {
  drawBuffered(tft, width, height, [&](auto& canvas) {
  drawShell(canvas, width, height, "About", page_, CREDIT_PAGE_COUNT);
  CydUi::touchTab(canvas, LICENSE_TAB, "License", false, TFT_CYAN,
                  touchCapture_.activeId() == 0);
  CydUi::touchTab(canvas, CREDITS_TAB, "Credits", true, TFT_GREEN,
                  touchCapture_.activeId() == 1);
  const CreditPage& credit = CREDIT_PAGES[page_];

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString(credit.game, 18, 78);

  if (credit.author2 != nullptr) {
    drawBodyLine(canvas, 20, 108, "Created: atomic14");
    drawBodyLine(canvas, 20, 124, "Modified:");
    drawBodyLine(canvas, 84, 124, credit.author2, TFT_GREEN);
  } else if (credit.author1[0] != 't') {
    drawBodyLine(canvas, 20, 108, "Inspiration");
    drawGithubHandle(canvas, 20, 124, credit.author1);
  } else {
    drawBodyLine(canvas, 20, 108, "Developer");
    drawGithubHandle(canvas, 20, 124, credit.author1);
  }
  CydUi::touchAction(canvas, PAGE_PREV, "Previous", TFT_BLUE, false,
                     touchCapture_.activeId() == 2);
  CydUi::touchAction(canvas, PAGE_NEXT, "Next", TFT_BLUE, false,
                     touchCapture_.activeId() == 3);
  });
}

void CreditsApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  drawShell(tft, width, height, "About");
  drawBodyLine(tft, 20, 54, "License and credits");
  drawBodyLine(tft, 20, 76, BuildInfo::BUILD_TEXT, TFT_GREEN);
  drawFooter(tft, width, height, "Choose License or Credits");
  startDirty_ = false;
}

void CreditsApp::drawEnd(TFT_eSPI& tft) {
  if (!endDirty_) {
    return;
  }
  drawShell(tft, width, height, "About");
  drawBodyLine(tft, 20, 54, "End");
  drawBodyLine(tft, 20, 76, BuildInfo::BUILD_TEXT, TFT_GREEN);
  drawFooter(tft, width, height, "Choose License or Credits");
  endDirty_ = false;
}
