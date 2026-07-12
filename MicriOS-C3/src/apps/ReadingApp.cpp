#include "ReadingApp.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint8_t LINES_PER_PAGE = 3;
constexpr uint8_t MAX_LINE_CHARS = 17;

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

const char* skipSpaces(const char* cursor) {
  while (*cursor == ' ') {
    cursor++;
  }
  return cursor;
}

bool nextWrappedLine(const char*& cursor, char* line, size_t lineSize) {
  cursor = skipSpaces(cursor);
  if (*cursor == '\0' || lineSize == 0) {
    return false;
  }

  uint8_t lineLen = 0;
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

    const int separator = lineLen > 0 ? 1 : 0;
    const int available = (int)MAX_LINE_CHARS - (int)lineLen - separator;
    if (available < (int)wordLen) {
      if (lineLen == 0) {
        const uint8_t copyLen = min<uint8_t>(wordLen, min<uint8_t>(MAX_LINE_CHARS, lineSize - 1));
        memcpy(line, wordStart, copyLen);
        lineLen = copyLen;
        scan = wordStart + copyLen;
      }
      break;
    }

    if (lineLen > 0 && lineLen < lineSize - 1) {
      line[lineLen++] = ' ';
    }
    const uint8_t copyLen = min<uint8_t>(wordLen, lineSize - lineLen - 1);
    memcpy(line + lineLen, wordStart, copyLen);
    lineLen += copyLen;
    scan = wordStart + wordLen;
  }

  line[lineLen] = '\0';
  cursor = scan;
  return lineLen > 0;
}
}

ReadingApp::ReadingApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Reading", width, height), left_(left), selection_(0), page_(0), mode_(Mode::Select) {}

bool ReadingApp::hasCustomOverlay() const {
  return true;
}

bool ReadingApp::startsRunningImmediately() const {
  return true;
}

void ReadingApp::onAppReset() {
  selection_ = 0;
  page_ = 0;
  mode_ = Mode::Select;
}

void ReadingApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;

  if (mode_ == Mode::Select) {
    if (input.click) {
      selection_ = (selection_ + 1) % (READING_COUNT + 1);
    } else if (input.longPress) {
      if (selection_ >= READING_COUNT) {
        requestExitToMenu();
      } else {
        page_ = 0;
        mode_ = Mode::Read;
      }
    }
    return;
  }

  if (mode_ == Mode::Read) {
    if (input.longPress) {
      mode_ = Mode::Select;
    } else if (input.click) {
      if (hasMoreAfterPage(READINGS[selection_].text, page_)) {
        page_++;
      } else {
        mode_ = Mode::Complete;
      }
    }
    return;
  }

  if (input.longPress) {
    mode_ = Mode::Select;
  } else if (input.click) {
    page_ = 0;
    mode_ = Mode::Read;
  }
}

void ReadingApp::drawRunning(U8G2& u8g2) {
  if (mode_ == Mode::Select) {
    drawSelection(u8g2);
  } else if (mode_ == Mode::Read) {
    drawReading(u8g2);
  } else {
    drawComplete(u8g2);
  }
}

void ReadingApp::drawStart(U8G2& u8g2) {
  drawSelection(u8g2);
}

bool ReadingApp::hasMoreAfterPage(const char* text, uint8_t page) const {
  const char* cursor = text;
  char line[MAX_LINE_CHARS + 1];
  const uint8_t linesToConsume = (page + 1) * LINES_PER_PAGE;

  for (uint8_t i = 0; i < linesToConsume; i++) {
    if (!nextWrappedLine(cursor, line, sizeof(line))) {
      return false;
    }
  }

  return *skipSpaces(cursor) != '\0';
}

void ReadingApp::drawCenteredText(U8G2& u8g2, int y, const char* text) const {
  int x = ((int)width + 2 - (int)u8g2.getStrWidth(text)) / 2;
  if (x < 2) {
    x = 2;
  }
  u8g2.drawStr(x, y, text);
}

void ReadingApp::drawSelection(U8G2& u8g2) const {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Reading");

  u8g2.setFont(u8g2_font_4x6_tr);
  char counter[8];
  snprintf(counter, sizeof(counter), "%u/%u", selection_ + 1, READING_COUNT + 1);
  u8g2.drawStr(width + 2 - u8g2.getStrWidth(counter) - 3, 9, counter);

  if (selection_ >= READING_COUNT) {
    u8g2.setFont(u8g2_font_5x8_tr);
    drawCenteredText(u8g2, 23, "Back");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 38, "Hold menu");
    return;
  }

  u8g2.setFont(u8g2_font_5x8_tr);
  if (u8g2.getStrWidth(READINGS[selection_].ref) > width - 4) {
    u8g2.setFont(u8g2_font_4x6_tr);
  }
  drawCenteredText(u8g2, 23, READINGS[selection_].ref);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 31, "Tap next");
  u8g2.drawStr(3, 38, "Hold open");
}

bool ReadingApp::drawWrappedPage(U8G2& u8g2, const char* text, uint8_t page, int firstY) const {
  const char* cursor = text;
  char line[MAX_LINE_CHARS + 1];
  const uint8_t linesToSkip = page * LINES_PER_PAGE;

  for (uint8_t i = 0; i < linesToSkip; i++) {
    if (!nextWrappedLine(cursor, line, sizeof(line))) {
      return false;
    }
  }

  for (uint8_t displayLine = 0; displayLine < LINES_PER_PAGE; displayLine++) {
    if (!nextWrappedLine(cursor, line, sizeof(line))) {
      break;
    }
    u8g2.drawStr(left_ + 2, firstY + displayLine * 7, line);
  }

  return *skipSpaces(cursor) != '\0';
}

void ReadingApp::drawReading(U8G2& u8g2) const {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 2, 7, READINGS[selection_].ref);
  const bool hasMore = drawWrappedPage(u8g2, READINGS[selection_].text, page_, 15);

  u8g2.drawStr(3, 38, hasMore ? "Tap more" : "Tap END");
}

void ReadingApp::drawComplete(U8G2& u8g2) const {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_7x13_tr);
  drawCenteredText(u8g2, 17, "END");
  u8g2.setFont(u8g2_font_4x6_tr);
  drawCenteredText(u8g2, 27, READINGS[selection_].ref);
  u8g2.drawStr(3, 34, "Tap restart");
  u8g2.drawStr(3, 39, "Hold list");
}
