#include "ReadingApp.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdio.h>
#include <string.h>

#include "../../CydFramebuffer.h"
#include "../../CydUi.h"

namespace {
constexpr uint8_t MAX_LINE_CHARS = 80;

struct ReadingLayout {
  uint8_t textSize;
  uint8_t lineHeight;
  uint8_t glyphHeight;
  uint8_t linesPerPage;
  int16_t firstY;
  int16_t textX;
  int16_t maxWidth;
};

struct Reading {
  const char* ref;
  const char* text;
};

// Long text is wrapped into pages automatically at runtime.
const Reading READINGS[] = {
    {"Psalm 23:4",
     "Even though I walk through the valley of the shadow of death, I will fear no evil, for You are with me; Your rod "
     "and Your staff, they comfort me."},
    {"John 16:33",
     "I have told you these things so that in Me you may have peace. In the world you will have tribulation. But take "
     "courage; I have overcome the world!"},
    {"Isa 41:10",
     "Do not fear, for I am with you; do not be afraid, for I am your God. I will strengthen you; I will surely help "
     "you; I will uphold you with My righteous right hand."},
    {"Phil 4:6-7",
     "Be anxious for nothing, but in everything, by prayer and petition, with thanksgiving, present your requests to God. "
     "And the peace of God, which surpasses all understanding, will guard your hearts and your minds in Christ Jesus."},
    {"Ps 34:4-5,8",
     "I sought the LORD, and He answered me; He delivered me from all my fears. Those who look to Him are radiant with "
     "joy; their faces shall never be ashamed. Taste and see that the LORD is good; blessed is the man who takes refuge "
     "in Him!"},
    {"Romans 8:28",
     "And we know that God works all things together for the good of those who love Him, who are called according to His "
     "purpose."},
    {"Joshua 1:9",
     "Have I not commanded you to be strong and courageous? Do not be afraid; do not be discouraged, for the LORD your "
     "God is with you wherever you go."},
    {"Matt 6:31-34",
     "Therefore do not worry, saying, 'What shall we eat?' or 'What shall we drink?' or 'What shall we wear?' For the "
     "Gentiles strive after all these things, and your heavenly Father knows that you need them. But seek first the "
     "kingdom of God and His righteousness, and all these things will be added unto you. Therefore do not worry about "
     "tomorrow, for tomorrow will worry about itself. Today has enough trouble of its own."},
    {"Prov 3:5-6",
     "Trust in the LORD with all your heart, and lean not on your own understanding; in all your ways acknowledge Him, "
     "and He will make your paths straight."},
    {"Romans 15:13",
     "Now may the God of hope fill you with all joy and peace as you believe in Him, so that you may overflow with hope "
     "by the power of the Holy Spirit."},
    {"2 Chron 7:14",
     "and if My people who are called by My name humble themselves and pray and seek My face and turn from their wicked "
     "ways, then I will hear from heaven, forgive their sin, and heal their land."},
    {"Phil 2:3-4",
     "Do nothing out of selfish ambition or empty pride, but in humility consider others more important than yourselves. "
     "Each of you should look not only to your own interests, but also to the interests of others."},
    {"Isa 41:13",
     "For I am the LORD your God, who takes hold of your right hand and tells you: Do not fear, I will help you."},
    {"1 Pet 5:6-7",
     "Humble yourselves, therefore, under God's mighty hand, so that in due time He may exalt you. Cast all your anxiety "
     "on Him, because He cares for you."},
    {"Ps 94:18-19",
     "If I say, 'My foot is slipping,' Your loving devotion, O LORD, supports me. When anxiety overwhelms me, Your "
     "consolation delights my soul."},
    {"Rev 21:4",
     "'He will wipe away every tear from their eyes,' and there will be no more death or mourning or crying or pain, for "
     "the former things have passed away."},
    {"About BSP",
     "All Bible passages quoted from the Berean Standard Bible (BSB). The Berean Bible and Majority Bible texts are "
     "officially dedicated to the public domain as of April 30, 2023. All uses are freely permitted. Attribution Notice: "
     "The Holy Bible, Berean Standard Bible, BSB is produced in cooperation with Bible Hub, Discovery Bible, "
     "OpenBible.com, and the Berean Bible Translation Committee. This text of God's Word has been dedicated to the "
     "public domain."},
};

constexpr uint8_t READING_COUNT = sizeof(READINGS) / sizeof(READINGS[0]);
constexpr uint8_t SELECT_ROWS = 4;
constexpr uint8_t SELECT_PAGE_COUNT =
    (READING_COUNT + SELECT_ROWS - 1) / SELECT_ROWS;
constexpr TouchUi::Rect SELECT_PREV = {12, 176, 82, 27};
constexpr TouchUi::Rect SELECT_NEXT = {226, 176, 82, 27};
constexpr TouchUi::Rect READ_PREV = {12, 163, 92, 38};
constexpr TouchUi::Rect READ_NEXT = {216, 163, 92, 38};
constexpr TouchUi::Rect COMPLETE_LIST = {30, 154, 120, 42};
constexpr TouchUi::Rect COMPLETE_RESTART = {170, 154, 120, 42};

TouchUi::Rect selectionRect(uint8_t row) {
  return {12, static_cast<int16_t>(42 + row * 33), 296, 29};
}

ReadingLayout makeReadingLayout(uint32_t width, uint32_t height, CydUi::TextSize size) {
  ReadingLayout layout;
  layout.textSize = size == CydUi::TextSize::Large ? 2 : 1;
  layout.lineHeight = size == CydUi::TextSize::Large ? 20 : 10;
  layout.glyphHeight = size == CydUi::TextSize::Large ? 16 : 8;
  layout.linesPerPage = 1;
  layout.firstY = 42;
  layout.textX = 12;
  layout.maxWidth = static_cast<int16_t>(width) - 24;

  const int16_t footerTop = 154;
  const int16_t usable = footerTop - layout.firstY - layout.glyphHeight;
  if (usable > 0) {
    layout.linesPerPage = static_cast<uint8_t>(usable / layout.lineHeight + 1);
  }
  return layout;
}

template <typename Canvas>
void drawShell(Canvas& tft, uint32_t width, uint32_t height, const char* title, uint8_t index = 0, uint8_t count = 0) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(title, 12, 8);
  if (count > 0) {
    char counter[10];
    snprintf(counter, sizeof(counter), "%u/%u", index + 1, count);
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

const char* skipSpaces(const char* cursor) {
  while (*cursor == ' ') {
    cursor++;
  }
  return cursor;
}

template <typename Canvas>
bool nextWrappedLine(Canvas& tft, const char*& cursor, char* line, size_t lineSize, int maxWidth) {
  cursor = skipSpaces(cursor);
  if (*cursor == '\0' || lineSize == 0) {
    return false;
  }

  String current;
  const char* scan = cursor;

  while (*scan != '\0') {
    scan = skipSpaces(scan);
    if (*scan == '\0') {
      break;
    }

    const char* wordStart = scan;
    uint8_t wordLen = 0;
    while (wordStart[wordLen] != '\0' && wordStart[wordLen] != ' ') {
      wordLen++;
    }

    String word;
    word.reserve(wordLen);
    for (uint8_t i = 0; i < wordLen; i++) {
      word += wordStart[i];
    }

    String candidate = current;
    if (candidate.length() > 0) {
      candidate += ' ';
    }
    candidate += word;

    if (tft.textWidth(candidate) > maxWidth) {
      if (current.length() == 0) {
        String chunk;
        for (uint8_t i = 0; i < wordLen && chunk.length() < lineSize - 1; i++) {
          String nextChunk = chunk;
          nextChunk += wordStart[i];
          if (chunk.length() > 0 && tft.textWidth(nextChunk) > maxWidth) {
            break;
          }
          chunk = nextChunk;
        }
        if (chunk.length() == 0 && wordLen > 0) {
          chunk += wordStart[0];
        }
        current = chunk;
        scan = wordStart + current.length();
      }
      break;
    }

    current = candidate;
    scan = wordStart + wordLen;
  }

  current.toCharArray(line, lineSize);
  cursor = scan;
  return current.length() > 0;
}

template <typename Canvas>
uint8_t wrappedPageCount(Canvas& tft, const char* text,
                         const ReadingLayout& layout) {
  const char* cursor = text;
  char line[MAX_LINE_CHARS + 1];
  uint16_t lines = 0;
  while (nextWrappedLine(tft, cursor, line, sizeof(line), layout.maxWidth)) {
    ++lines;
  }
  if (lines == 0) return 1;
  return static_cast<uint8_t>((lines + layout.linesPerPage - 1) /
                              layout.linesPerPage);
}
}

ReadingApp::ReadingApp(uint32_t width, uint32_t height)
    : App("Reading", width, height), selection_(0), page_(0), mode_(Mode::Select) {}

bool ReadingApp::hasCustomOverlay() const {
  return true;
}

bool ReadingApp::startsRunningImmediately() const {
  return true;
}

bool ReadingApp::handleTouch(const TouchUi::TouchSample& sample,
                             const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (mode_ == Mode::Select) {
    for (uint8_t row = 0; row < SELECT_ROWS; ++row) {
      const uint8_t index = selectionPage_ * SELECT_ROWS + row;
      if (index < READING_COUNT && selectionRect(row).contains(point)) {
        hit = row;
      }
    }
    if (SELECT_PREV.contains(point)) hit = 4;
    if (SELECT_NEXT.contains(point)) hit = 5;
  } else if (mode_ == Mode::Read) {
    if (READ_PREV.contains(point)) hit = 0;
    if (READ_NEXT.contains(point)) hit = 1;
  } else {
    if (COMPLETE_LIST.contains(point)) hit = 0;
    if (COMPLETE_RESTART.contains(point)) hit = 1;
  }

  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) markDirty();
  if (!result.activated) return result.consumed;

  if (mode_ == Mode::Select) {
    if (result.id < SELECT_ROWS) {
      selection_ = selectionPage_ * SELECT_ROWS + result.id;
      page_ = 0;
      pageHasMore_ = true;
      mode_ = Mode::Read;
    } else if (result.id == 4) {
      selectionPage_ = selectionPage_ == 0 ? SELECT_PAGE_COUNT - 1
                                            : selectionPage_ - 1;
    } else if (result.id == 5) {
      selectionPage_ = (selectionPage_ + 1) % SELECT_PAGE_COUNT;
    }
  } else if (mode_ == Mode::Read) {
    if (result.id == 0 && page_ > 0) {
      --page_;
    } else if (result.id == 1) {
      if (page_ + 1 < pageCount_) ++page_;
      else mode_ = Mode::Complete;
    }
  } else {
    if (result.id == 0) mode_ = Mode::Select;
    if (result.id == 1) {
      page_ = 0;
      mode_ = Mode::Read;
    }
  }
  touchCapture_.reset();
  markDirty();
  return true;
}

void ReadingApp::render(TFT_eSPI& tft) {
  const AppPhase currentPhase = phase();
  if (!phaseCached_ || currentPhase != renderedPhase_) {
    phaseCached_ = true;
    renderedPhase_ = currentPhase;
    dirty_ = true;
    startDirty_ = true;
  }
  App::render(tft);
}

void ReadingApp::markDirty() {
  dirty_ = true;
}

void ReadingApp::onAppReset() {
  selection_ = 0;
  page_ = 0;
  pageCount_ = 1;
  selectionPage_ = 0;
  pageHasMore_ = false;
  mode_ = Mode::Select;
  touchCapture_.reset();
  markDirty();
}

void ReadingApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;

  if (b2.click) {
    if (mode_ == Mode::Select) {
      requestExitToMenu();
    } else {
      mode_ = Mode::Select;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Select) {
    if (b1.click) {
      selection_ = (selection_ + 1) % READING_COUNT;
      selectionPage_ = selection_ / SELECT_ROWS;
      markDirty();
    } else if (b1.longPress) {
      if (selection_ >= READING_COUNT) {
        requestExitToMenu();
      } else {
        page_ = 0;
        pageHasMore_ = true;
        mode_ = Mode::Read;
        markDirty();
      }
    }
    return;
  }

  if (mode_ == Mode::Read) {
    if (b1.longPress) {
      mode_ = Mode::Select;
      markDirty();
    } else if (b1.click) {
      if (pageHasMore_) {
        page_++;
      } else {
        mode_ = Mode::Complete;
      }
      markDirty();
    }
    return;
  }

  if (b1.longPress) {
    mode_ = Mode::Select;
    markDirty();
  } else if (b1.click) {
    page_ = 0;
    pageHasMore_ = true;
    mode_ = Mode::Read;
    markDirty();
  }
}

void ReadingApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }
  if (mode_ == Mode::Select) {
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) { drawSelection(canvas); });
  } else if (mode_ == Mode::Read) {
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) { drawReading(canvas); });
  } else {
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) { drawComplete(canvas); });
  }
  dirty_ = false;
}

void ReadingApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  drawSelection(tft);
  startDirty_ = false;
}

template <typename Canvas>
void ReadingApp::drawCenteredText(Canvas& tft, int y, const char* text) const {
  int x = ((int)width - (int)tft.textWidth(text)) / 2;
  if (x < 2) {
    x = 2;
  }
  tft.drawString(text, x, y);
}

template <typename Canvas>
void ReadingApp::drawSelection(Canvas& tft) {
  CydUi::header(tft, "Reading", TFT_ORANGE,
                (String(selectionPage_ + 1) + "/" + SELECT_PAGE_COUNT).c_str());
  for (uint8_t row = 0; row < SELECT_ROWS; ++row) {
    const uint8_t index = selectionPage_ * SELECT_ROWS + row;
    if (index >= READING_COUNT) break;
    CydUi::touchListRow(tft, selectionRect(row), READINGS[index].ref,
                        index == selection_, TFT_ORANGE, "Open",
                        touchCapture_.activeId() == row);
  }
  CydUi::touchAction(tft, SELECT_PREV, "Prev", TFT_BLUE, false,
                     touchCapture_.activeId() == 4);
  CydUi::touchAction(tft, SELECT_NEXT, "Next", TFT_BLUE, false,
                     touchCapture_.activeId() == 5);
}

template <typename Canvas>
bool drawWrappedPage(Canvas& tft, const char* text, uint8_t page, const ReadingLayout& layout) {
  const char* cursor = text;
  char line[MAX_LINE_CHARS + 1];
  const uint16_t linesToSkip = static_cast<uint16_t>(page) * layout.linesPerPage;

  for (uint16_t i = 0; i < linesToSkip; i++) {
    if (!nextWrappedLine(tft, cursor, line, sizeof(line), layout.maxWidth)) {
      return false;
    }
  }

  for (uint8_t displayLine = 0; displayLine < layout.linesPerPage; displayLine++) {
    if (!nextWrappedLine(tft, cursor, line, sizeof(line), layout.maxWidth)) {
      break;
    }
    tft.drawString(line, layout.textX, layout.firstY + displayLine * layout.lineHeight);
  }

  return *skipSpaces(cursor) != '\0';
}

template <typename Canvas>
void ReadingApp::drawReading(Canvas& tft) {
  drawShell(tft, width, height, READINGS[selection_].ref);
  const ReadingLayout layout = makeReadingLayout(width, height, CydUi::loadTextSize());
  pageCount_ = wrappedPageCount(tft, READINGS[selection_].text, layout);
  if (page_ >= pageCount_) page_ = pageCount_ - 1;
  tft.setTextSize(layout.textSize);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  pageHasMore_ = drawWrappedPage(tft, READINGS[selection_].text, page_, layout);
  CydUi::touchAction(tft, READ_PREV, "Previous", TFT_BLUE, page_ > 0,
                     touchCapture_.activeId() == 0);
  CydUi::touchAction(tft, READ_NEXT,
                     page_ + 1 < pageCount_ ? "Next" : "End", TFT_GREEN,
                     true, touchCapture_.activeId() == 1);
  CydUi::centered(tft, String(page_ + 1) + " / " + String(pageCount_), 178,
                  1, TFT_LIGHTGREY);
}

template <typename Canvas>
void ReadingApp::drawComplete(Canvas& tft) const {
  drawShell(tft, width, height, "Reading");
  tft.setTextSize(4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  drawCenteredText(tft, 70, "END");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  drawCenteredText(tft, 135, READINGS[selection_].ref);
  CydUi::touchAction(tft, COMPLETE_LIST, "Readings", TFT_BLUE, false,
                     touchCapture_.activeId() == 0);
  CydUi::touchAction(tft, COMPLETE_RESTART, "Read again", TFT_GREEN, true,
                     touchCapture_.activeId() == 1);
}
