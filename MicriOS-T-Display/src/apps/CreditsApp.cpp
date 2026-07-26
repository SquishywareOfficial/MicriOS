#include "CreditsApp.h"

#include <TFT_eSPI.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include "../shared/Version.h"

namespace {
constexpr uint8_t ABOUT_ITEM_COUNT = 3;
constexpr uint8_t MICRIDECK_PAGE_COUNT = 5;
constexpr uint8_t LICENSE_PAGE_COUNT = 6;

struct CreditPage {
  const char* game;
  const char* author1;
  const char* author2;
};

const char* const ABOUT_ITEMS[ABOUT_ITEM_COUNT] = {
    "The MicriDeck",
    "License",
    "Credits",
};

const char* const MICRIDECK_PAGES[MICRIDECK_PAGE_COUNT][5] = {
    {
        "After buying cheap ESP32",
        "devices, I was disappointed",
        "by how little useful firmware",
        "was ready to install.",
        nullptr,
    },
    {
        "I wanted a complete little",
        "computer: utilities, games,",
        "tools, and one interface",
        "bringing them together.",
        nullptr,
    },
    {
        "Atomic14's simple C3 games",
        "showed me what was possible",
        "on such a tiny, inexpensive",
        "device.",
        nullptr,
    },
    {
        "So I created the experience",
        "I had been looking for:",
        "an original operating system",
        "named MicriOS.",
        nullptr,
    },
    {
        "The first Micri Deck used the",
        "tiny ESP32-C3. MicriOS later",
        "expanded to the T-Display",
        "and the CYD.",
        nullptr,
    },
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

void drawGithubHandle(TFT_eSPI& tft, int x, int y, const char* handle) {
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

void drawShell(TFT_eSPI& tft, uint32_t width, uint32_t height, const char* title, uint8_t page = 0, uint8_t pageCount = 0) {
  tft.fillScreen(TFT_BLACK);
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

void drawFooter(TFT_eSPI& tft, uint32_t width, uint32_t height, const char* text) {
  tft.fillRect(0, height - 17, width, 17, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(text, 12, height - 13);
}

void drawMenuRow(TFT_eSPI& tft, uint32_t width, int y, const char* text, bool selected) {
  const uint16_t color = selected ? TFT_GREEN : TFT_DARKGREY;
  tft.fillRect(14, y, width - 28, 27, color);
  tft.setTextSize(2);
  tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE, color);
  tft.drawString(text, 24, y + 6);
}

void drawBodyLine(TFT_eSPI& tft, int x, int y, const char* text, uint16_t color = TFT_WHITE) {
  tft.setTextSize(1);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, x, y);
}

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}
}

CreditsApp::CreditsApp(uint32_t width, uint32_t height)
    : App("About", width, height) {}

bool CreditsApp::hasCustomOverlay() const {
  return true;
}

bool CreditsApp::startsRunningImmediately() const {
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
      mode_ = selection_ == 0
                  ? Mode::MicriDeck
                  : (selection_ == 1 ? Mode::License : Mode::Credits);
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
    const uint8_t pageCount =
        mode_ == Mode::MicriDeck
            ? MICRIDECK_PAGE_COUNT
            : (mode_ == Mode::License ? LICENSE_PAGE_COUNT : CREDIT_PAGE_COUNT);
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
    case Mode::MicriDeck:
      drawMicriDeck(tft);
      break;
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
  const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
  const uint8_t first = TDisplayUi::menuScrollOffset(selection_, ABOUT_ITEM_COUNT, TDisplayUi::menuStyle(textSize));
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::menuFrame(canvas, "About", selection_, ABOUT_ITEM_COUNT, first,
                          [](uint8_t index) -> const char* { return ABOUT_ITEMS[index]; },
                          "B1 next/open  B2 back", textSize, TFT_GREEN);
    const String version = String("MicriOS ") + BuildInfo::BUILD_TEXT;
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(version, 12, 26);
  });
}

void CreditsApp::drawMicriDeck(TFT_eSPI& tft) {
  drawShell(tft, width, height, "The MicriDeck", page_,
            MICRIDECK_PAGE_COUNT);
  for (uint8_t line = 0; line < 5; ++line) {
    const char* text = MICRIDECK_PAGES[page_][line];
    if (text == nullptr) {
      break;
    }
    drawBodyLine(tft, 18, 45 + line * 14, text,
                 line == 0 ? TFT_CYAN : TFT_WHITE);
  }
  drawFooter(tft, width, height, "B1 next page  B2 back");
}

void CreditsApp::drawLicense(TFT_eSPI& tft) {
  drawShell(tft, width, height, "License", page_, LICENSE_PAGE_COUNT);

  switch (page_) {
    case 0:
      drawBodyLine(tft, 18, 50, "WTFPL + No Warranty", TFT_GREEN);
      drawBodyLine(tft, 18, 70, "Do what you want.");
      drawBodyLine(tft, 18, 86, "Use at your own risk.");
      break;
    case 1:
      drawBodyLine(tft, 18, 50, "Copyright 2026");
      drawBodyLine(tft, 18, 68, "github:");
      drawBodyLine(tft, 72, 68, "thedarkfalcon", TFT_GREEN);
      break;
    case 2:
      drawBodyLine(tft, 18, 50, "Repository");
      drawBodyLine(tft, 18, 68, "github.com/");
      drawBodyLine(tft, 18, 84, "SquishywareOfficial", TFT_GREEN);
      drawBodyLine(tft, 18, 100, "/MicriOS", TFT_GREEN);
      break;
    case 3:
      drawBodyLine(tft, 18, 50, "Permission:");
      drawBodyLine(tft, 18, 68, "copy, modify, publish,");
      drawBodyLine(tft, 18, 84, "sell, and share freely.");
      break;
    case 4:
      drawBodyLine(tft, 18, 50, "NerdMiner-derived");
      drawBodyLine(tft, 18, 68, "miner code uses MIT.", TFT_GREEN);
      drawBodyLine(tft, 18, 86, "See licenses/");
      drawBodyLine(tft, 18, 102, "NerdMiner_v2-MIT.txt");
      break;
    default:
      drawBodyLine(tft, 18, 50, "No warranty.");
      drawBodyLine(tft, 18, 68, "Provided as-is.");
      drawBodyLine(tft, 18, 84, "No fitness guarantee.");
      break;
  }
  drawFooter(tft, width, height, "B1 next page  B2 back");
}

void CreditsApp::drawCredits(TFT_eSPI& tft) {
  drawShell(tft, width, height, "Credits", page_, CREDIT_PAGE_COUNT);
  const CreditPage& credit = CREDIT_PAGES[page_];

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(credit.game, 18, 48);

  if (credit.author2 != nullptr) {
    drawBodyLine(tft, 20, 78, "Created: atomic14");
    drawBodyLine(tft, 20, 94, "Modified:");
    drawBodyLine(tft, 84, 94, credit.author2, TFT_GREEN);
  } else if (credit.author1[0] != 't') {
    drawBodyLine(tft, 20, 78, "Inspiration");
    drawGithubHandle(tft, 20, 94, credit.author1);
  } else {
    drawBodyLine(tft, 20, 78, "Developer");
    drawGithubHandle(tft, 20, 94, credit.author1);
  }
  drawFooter(tft, width, height, "B1 next page  B2 back");
}

void CreditsApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  drawShell(tft, width, height, "About");
  drawBodyLine(tft, 20, 54, "License and credits");
  drawBodyLine(tft, 20, 76, BuildInfo::BUILD_TEXT, TFT_GREEN);
  drawFooter(tft, width, height, "B1 start  B2 back");
  startDirty_ = false;
}

void CreditsApp::drawEnd(TFT_eSPI& tft) {
  if (!endDirty_) {
    return;
  }
  drawShell(tft, width, height, "About");
  drawBodyLine(tft, 20, 54, "End");
  drawBodyLine(tft, 20, 76, BuildInfo::BUILD_TEXT, TFT_GREEN);
  drawFooter(tft, width, height, "B1 replay  B2 back");
  endDirty_ = false;
}
