#include "MediaPlayerApp.h"

#include <TFT_eSPI.h>

#include "../../CydFramebuffer.h"
#include "../../CydHardware.h"
#include "../../CydUi.h"

namespace {
constexpr uint8_t ROWS_PER_PAGE = 4;
constexpr TouchUi::Rect PREVIOUS_PAGE = {10, 174, 72, 28};
constexpr TouchUi::Rect UP_DIRECTORY = {91, 174, 138, 28};
constexpr TouchUi::Rect RETRY_SD = {91, 174, 138, 28};
constexpr TouchUi::Rect NEXT_PAGE = {238, 174, 72, 28};
constexpr TouchUi::Rect VIDEO_AREA = {0, 0, 320, 176};
constexpr TouchUi::Rect JUMP_TO = {4, 185, 98, 21};
constexpr TouchUi::Rect PLAY_PAUSE = {111, 185, 98, 21};
constexpr TouchUi::Rect MEDIA_LIST = {218, 185, 98, 21};
constexpr TouchUi::Rect SEEK_TOUCH = {8, 184, 274, 24};
constexpr TouchUi::Rect SEEK_TRACK = {12, 191, 266, 10};
constexpr TouchUi::Rect SEEK_CLOSE = {286, 184, 28, 22};

TouchUi::Rect rowRect(uint8_t row) {
  return {10, static_cast<int16_t>(38 + row * 33), 300, 29};
}

template <typename Canvas>
void drawMediaRow(Canvas& canvas, const TouchUi::Rect& rect,
                  const MediaPlayerLogic::Item& item) {
  const uint16_t background = item.directory ? TFT_NAVY : TFT_DARKGREY;
  const uint16_t border = item.directory ? TFT_BLUE : TFT_CYAN;
  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 4, background);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 4, border);
  int16_t textX = rect.x + 9;
  if (item.directory) {
    canvas.fillRect(rect.x + 9, rect.y + 9, 20, 12, TFT_YELLOW);
    canvas.fillRect(rect.x + 11, rect.y + 6, 9, 4, TFT_YELLOW);
    canvas.drawRect(rect.x + 9, rect.y + 9, 20, 12, TFT_ORANGE);
    textX = rect.x + 38;
  }
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE, background);
  String title(item.title);
  const int16_t titleWidth = item.directory ? rect.w - 104 : rect.w - 72;
  while (title.length() > 3 && canvas.textWidth(title) > titleWidth) {
    title.remove(title.length() - 1);
  }
  if (title != item.title) title += "...";
  canvas.drawString(title, textX, rect.y + 7);
  canvas.setTextSize(1);
  canvas.setTextColor(item.directory ? TFT_CYAN : TFT_LIGHTGREY, background);
  const String detail = item.directory
                            ? String("Folder")
                            : String(item.sizeBytes / 1048576.0f, 1) + " MB";
  canvas.drawString(detail, rect.x + rect.w - canvas.textWidth(detail) - 8,
                    rect.y + 10);
}
}  // namespace

MediaPlayerApp::MediaPlayerApp(uint32_t width, uint32_t height)
    : App("Media Player", width, height) {}

bool MediaPlayerApp::hasCustomOverlay() const { return true; }

bool MediaPlayerApp::startsRunningImmediately() const { return true; }

uint16_t MediaPlayerApp::runningRenderIntervalMs() const { return 200; }

bool MediaPlayerApp::wantsImmediateRender() const {
  return dirty_ ||
         (view_ == View::Player &&
          backend_.session().state() == MediaPlayerLogic::State::Playing);
}

void MediaPlayerApp::onAppReset() {
  view_ = View::Browser;
  page_ = 0;
  dirty_ = true;
  videoAreaReady_ = false;
  audioStress_ = false;
  seekMode_ = false;
  seekDragging_ = false;
  audioStressFrame_ = 0;
  pendingSeekMs_ = 0;
  lastAudioStressMs_ = 0;
  lastProgressSecond_ = UINT32_MAX;
  lastDrawnState_ = MediaPlayerLogic::State::Error;
  touchCapture_.reset();
  refreshCatalog();
}

void MediaPlayerApp::onAppExit() {
  backend_.end();
  CydFramebuffer::release();
  CydHardware::setRgb(0, 0, 0);
}

void MediaPlayerApp::refreshCatalog() {
  backend_.scan(catalog_);
  page_ = 0;
  view_ = backend_.mounted() ? View::Browser : View::Error;
  dirty_ = true;
}

uint8_t MediaPlayerApp::pageCount() const {
  return catalog_.empty()
             ? 1
             : static_cast<uint8_t>((catalog_.count() + ROWS_PER_PAGE - 1) /
                                    ROWS_PER_PAGE);
}

bool MediaPlayerApp::handleTouch(const TouchUi::TouchSample& sample,
                                 const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;

  if (view_ == View::Player && seekMode_) {
    if (!seekDragging_ && event.tap && SEEK_CLOSE.contains(event.point)) {
      seekMode_ = false;
      dirty_ = true;
      return true;
    }
    if (sample.down &&
        (seekDragging_ || SEEK_TOUCH.contains(sample.point))) {
      seekDragging_ = true;
      updateSeekTarget(sample.point.x);
      dirty_ = true;
      return true;
    }
    if (event.released && seekDragging_) {
      updateSeekTarget(event.point.x);
      seekDragging_ = false;
      if (tft_ != nullptr) {
        tft_->fillRect(0, CydMediaBackend::VIDEO_HEIGHT, width,
                       height - CydMediaBackend::VIDEO_HEIGHT, TFT_BLACK);
        CydUi::centered(*tft_, "Seeking...", 188, 1, TFT_CYAN);
        backend_.seekTo(pendingSeekMs_, *tft_);
      }
      seekMode_ = false;
      videoAreaReady_ = true;
      dirty_ = true;
      touchCapture_.reset();
      return true;
    }
    return sample.down || event.released || event.tap;
  }

  if (view_ == View::Browser) {
    const uint8_t offset = page_ * ROWS_PER_PAGE;
    for (uint8_t row = 0; row < ROWS_PER_PAGE; ++row) {
      if (offset + row < catalog_.count() && rowRect(row).contains(point)) {
        hit = row;
      }
    }
    if (PREVIOUS_PAGE.contains(point)) hit = 10;
    if (UP_DIRECTORY.contains(point)) hit = 11;
    if (NEXT_PAGE.contains(point)) hit = 12;
  } else if (view_ == View::Player) {
    if (JUMP_TO.contains(point)) hit = 20;
    if (PLAY_PAUSE.contains(point)) hit = 21;
    if (MEDIA_LIST.contains(point)) hit = 23;
    if (VIDEO_AREA.contains(point)) hit = 24;
  } else {
    if (RETRY_SD.contains(point)) hit = 11;
    if (MEDIA_LIST.contains(point)) hit = 23;
  }

  const auto result = touchCapture_.update(sample, event, hit);
  if (!result.activated) return result.consumed;

  if (view_ == View::Browser) {
    if (result.id >= 0 && result.id < ROWS_PER_PAGE) {
      const uint8_t index = page_ * ROWS_PER_PAGE + result.id;
      if (index < catalog_.count() && tft_ != nullptr) {
        startSelected(*tft_, index);
      }
    } else if (result.id == 10) {
      page_ = page_ == 0 ? pageCount() - 1 : page_ - 1;
      dirty_ = true;
    } else if (result.id == 11) {
      if (backend_.goUp(catalog_)) {
        page_ = 0;
        dirty_ = true;
      }
    } else if (result.id == 12) {
      page_ = (page_ + 1) % pageCount();
      dirty_ = true;
    }
  } else if (view_ == View::Player) {
    const auto state = backend_.session().state();
    if (result.id == 20) {
      if (backend_.session().metadata().durationMs > 0) {
        backend_.pause();
        pendingSeekMs_ = backend_.session().positionMs();
        seekMode_ = true;
        seekDragging_ = false;
        dirty_ = true;
      }
    } else if (result.id == 21 || result.id == 24) {
      if (result.id == 24 && backend_.audioOnly()) {
        audioStress_ = !audioStress_;
        audioStressFrame_ = 0;
        lastAudioStressMs_ = 0;
        videoAreaReady_ = false;
      } else if (state == MediaPlayerLogic::State::Playing) {
        backend_.pause();
      } else if (state == MediaPlayerLogic::State::Paused) {
        backend_.resume();
      } else if (state == MediaPlayerLogic::State::Finished) {
        backend_.restart();
      }
      dirty_ = true;
    } else if (result.id == 23) {
      returnToBrowser();
    }
  } else {
    if (result.id == 11) refreshCatalog();
    if (result.id == 23) returnToBrowser();
  }
  touchCapture_.reset();
  return true;
}

void MediaPlayerApp::startSelected(TFT_eSPI& tft, uint8_t index) {
  const MediaPlayerLogic::Item* item = catalog_.item(index);
  if (item == nullptr) return;
  if (item->directory) {
    char path[MediaPlayerLogic::MAX_PATH_LENGTH];
    MediaPlayerLogic::copyText(path, sizeof(path), item->path);
    if (backend_.enterDirectory(path, catalog_)) {
      page_ = 0;
      dirty_ = true;
    }
    return;
  }
  catalog_.select(index);
  CydFramebuffer::release();
  tft.fillRect(0, 0, width, height, TFT_BLACK);
  videoAreaReady_ = false;
  audioStress_ = false;
  seekMode_ = false;
  seekDragging_ = false;
  audioStressFrame_ = 0;
  pendingSeekMs_ = 0;
  lastAudioStressMs_ = 0;
  if (backend_.open(item->path)) {
    view_ = View::Player;
  } else {
    view_ = View::Error;
  }
  dirty_ = true;
}

void MediaPlayerApp::returnToBrowser() {
  backend_.closePlayback();
  view_ = View::Browser;
  seekMode_ = false;
  seekDragging_ = false;
  videoAreaReady_ = false;
  dirty_ = true;
}

void MediaPlayerApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1,
                                   const ButtonInput& b2) {
  (void)deltaMs;
  (void)b1;
  (void)b2;
}

void MediaPlayerApp::drawRunning(TFT_eSPI& tft) {
  tft_ = &tft;
  if (view_ == View::Browser) {
    drawBrowser(tft);
  } else if (view_ == View::Player) {
    drawPlayer(tft);
  } else {
    drawError(tft);
  }
}

void MediaPlayerApp::drawBrowser(TFT_eSPI& tft) {
  if (!dirty_) return;
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "Media Player", TFT_CYAN, backend_.mountedPath());
    if (catalog_.empty()) {
      CydUi::centered(canvas, "This folder is empty", 72, 2, TFT_WHITE);
      CydUi::centered(
          canvas,
          backend_.atRoot() ? "Put folders or media in /media"
                            : "Tap Up to return",
          106, 1, TFT_LIGHTGREY);
    } else {
      const uint8_t offset = page_ * ROWS_PER_PAGE;
      for (uint8_t row = 0; row < ROWS_PER_PAGE; ++row) {
        const MediaPlayerLogic::Item* item = catalog_.item(offset + row);
        if (item != nullptr) drawMediaRow(canvas, rowRect(row), *item);
      }
    }
    const bool severalPages = pageCount() > 1;
    CydUi::touchAction(canvas, PREVIOUS_PAGE, "Prev",
                       severalPages ? TFT_LIGHTGREY : TFT_DARKGREY, false);
    CydUi::touchAction(canvas, UP_DIRECTORY, "Up",
                       backend_.atRoot() ? TFT_DARKGREY : TFT_CYAN, false);
    CydUi::touchAction(canvas, NEXT_PAGE, "Next",
                       severalPages ? TFT_LIGHTGREY : TFT_DARKGREY, false);
  });
  dirty_ = false;
}

void MediaPlayerApp::drawPlayer(TFT_eSPI& tft) {
  if (!videoAreaReady_) {
    tft.fillRect(0, 0, width, CydMediaBackend::VIDEO_HEIGHT, TFT_BLACK);
    if (backend_.audioOnly()) {
      drawAudioOnly(tft);
    } else {
      CydUi::centered(tft, "Loading video...", 76, 2, TFT_LIGHTGREY);
    }
    videoAreaReady_ = true;
    dirty_ = true;
  }
  if (backend_.session().state() == MediaPlayerLogic::State::Playing) {
    backend_.service(tft);
    if (backend_.audioOnly() && audioStress_ &&
        millis() - lastAudioStressMs_ >= 83U) {
      lastAudioStressMs_ = millis();
      drawAudioStressFrame(tft);
    }
  }
  if (backend_.session().state() == MediaPlayerLogic::State::Error) {
    view_ = View::Error;
    dirty_ = true;
    drawError(tft);
    return;
  }
  const auto state = backend_.session().state();
  if (state != lastDrawnState_) dirty_ = true;
  if (dirty_) {
    drawTransport(tft, true);
    dirty_ = false;
  } else if (state == MediaPlayerLogic::State::Playing) {
    const uint32_t progressSecond = backend_.session().positionMs() / 1000U;
    if (progressSecond != lastProgressSecond_) drawProgress(tft);
  }
}

void MediaPlayerApp::drawAudioOnly(TFT_eSPI& tft) {
  const auto& metadata = backend_.session().metadata();
  tft.fillRect(0, 0, width, CydMediaBackend::VIDEO_HEIGHT, TFT_BLACK);
  CydUi::centered(tft, backend_.currentTitle(), 12, 2, TFT_WHITE);
  tft.fillCircle(160, 78, 48, TFT_DARKGREY);
  tft.drawCircle(160, 78, 48, TFT_CYAN);
  tft.fillCircle(160, 78, 17, TFT_BLACK);
  tft.drawCircle(160, 78, 17, TFT_LIGHTGREY);
  tft.fillCircle(160, 78, 4, TFT_CYAN);
  char format[40];
  snprintf(format, sizeof(format), "%lu Hz  %u-bit  %s",
           static_cast<unsigned long>(metadata.audioSampleRate),
           metadata.audioBits,
           metadata.audioChannels == 1 ? "mono" : "stereo");
  CydUi::centered(tft, format, 145, 1, TFT_LIGHTGREY);
  CydUi::centered(tft,
                  audioStress_ ? "Tap artwork: stress ON"
                               : "Tap artwork: stress OFF",
                  160, 1, audioStress_ ? TFT_ORANGE : TFT_GREEN);
}

void MediaPlayerApp::drawAudioStressFrame(TFT_eSPI& tft) {
  static constexpr uint16_t colors[] = {
      TFT_NAVY, TFT_DARKGREEN, TFT_MAROON, TFT_DARKCYAN,
      TFT_PURPLE, TFT_OLIVE, TFT_BLUE, TFT_RED,
  };
  const uint8_t phase = audioStressFrame_++;
  tft.fillRect(0, 0, width, CydMediaBackend::VIDEO_HEIGHT,
               colors[phase % 8]);
  for (uint8_t row = 0; row < 8; ++row) {
    const int16_t y = (row * 23 + phase * 3) %
                      CydMediaBackend::VIDEO_HEIGHT;
    const uint16_t color = colors[(phase + row + 3) % 8];
    tft.fillRect(0, y, width, 9, color);
    tft.drawFastHLine(0, (y + 13) % CydMediaBackend::VIDEO_HEIGHT,
                      width, TFT_LIGHTGREY);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("DISPLAY STRESS 12 FPS", 160, 82);
  tft.setTextDatum(TL_DATUM);
}

void MediaPlayerApp::drawTransport(TFT_eSPI& tft, bool force) {
  (void)force;
  if (seekMode_) {
    drawSeekOverlay(tft);
    return;
  }
  tft.fillRect(0, CydMediaBackend::VIDEO_HEIGHT, width,
               height - CydMediaBackend::VIDEO_HEIGHT, TFT_BLACK);
  tft.drawFastHLine(0, CydMediaBackend::VIDEO_HEIGHT, width, TFT_DARKGREY);
  const auto state = backend_.session().state();
  const char* playLabel = state == MediaPlayerLogic::State::Playing
                              ? "Pause"
                              : (state == MediaPlayerLogic::State::Finished
                                     ? "Replay"
                                     : "Play");
  CydUi::touchAction(tft, JUMP_TO, "Jump To", TFT_CYAN, false);
  CydUi::touchAction(tft, PLAY_PAUSE, playLabel, TFT_GREEN, false);
  CydUi::touchAction(tft, MEDIA_LIST, "List", TFT_CYAN, false);

  lastDrawnState_ = state;
  drawProgress(tft);
}

void MediaPlayerApp::drawSeekOverlay(TFT_eSPI& tft) {
  tft.fillRect(0, CydMediaBackend::VIDEO_HEIGHT, width,
               height - CydMediaBackend::VIDEO_HEIGHT, TFT_BLACK);
  tft.drawFastHLine(0, CydMediaBackend::VIDEO_HEIGHT, width, TFT_DARKGREY);
  char position[16];
  char duration[16];
  MediaPlayerLogic::formatDuration(pendingSeekMs_, position, sizeof(position));
  MediaPlayerLogic::formatDuration(backend_.session().metadata().durationMs,
                                   duration, sizeof(duration));
  const String progress = String(position) + " / " + duration;
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(progress, 12, 178);

  const uint32_t durationMs = backend_.session().metadata().durationMs;
  const uint8_t percent = durationMs == 0
                              ? 0
                              : static_cast<uint8_t>(
                                    (static_cast<uint64_t>(pendingSeekMs_) *
                                     100ULL) /
                                    durationMs);
  const int16_t fillWidth =
      static_cast<int16_t>((SEEK_TRACK.w - 2) * percent / 100);
  tft.drawRoundRect(SEEK_TRACK.x, SEEK_TRACK.y, SEEK_TRACK.w, SEEK_TRACK.h, 3,
                    TFT_LIGHTGREY);
  tft.fillRoundRect(SEEK_TRACK.x + 1, SEEK_TRACK.y + 1, SEEK_TRACK.w - 2,
                    SEEK_TRACK.h - 2, 2, TFT_BLACK);
  if (fillWidth > 0) {
    tft.fillRoundRect(SEEK_TRACK.x + 1, SEEK_TRACK.y + 1, fillWidth,
                      SEEK_TRACK.h - 2, 2, TFT_CYAN);
  }
  const int16_t knobX = SEEK_TRACK.x + 1 +
      static_cast<int16_t>((SEEK_TRACK.w - 3) * percent / 100);
  tft.fillCircle(knobX, SEEK_TRACK.y + SEEK_TRACK.h / 2, 5, TFT_WHITE);
  tft.drawRoundRect(SEEK_CLOSE.x, SEEK_CLOSE.y, SEEK_CLOSE.w, SEEK_CLOSE.h, 4,
                    TFT_DARKGREY);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("X", SEEK_CLOSE.x + 10, SEEK_CLOSE.y + 7);
}

void MediaPlayerApp::updateSeekTarget(int16_t x) {
  const int16_t start = SEEK_TRACK.x + 1;
  const int16_t end = SEEK_TRACK.x + SEEK_TRACK.w - 2;
  x = constrain(x, start, end);
  const uint32_t durationMs = backend_.session().metadata().durationMs;
  pendingSeekMs_ = static_cast<uint32_t>(
      (static_cast<uint64_t>(x - start) * durationMs) / (end - start));
}

void MediaPlayerApp::drawProgress(TFT_eSPI& tft) {
  tft.fillRect(0, CydMediaBackend::VIDEO_HEIGHT, width, 9, TFT_BLACK);
  tft.drawFastHLine(0, CydMediaBackend::VIDEO_HEIGHT, width, TFT_DARKGREY);
  char position[16];
  char duration[16];
  MediaPlayerLogic::formatDuration(backend_.session().positionMs(), position,
                                   sizeof(position));
  MediaPlayerLogic::formatDuration(backend_.session().metadata().durationMs,
                                   duration, sizeof(duration));
  const String progress = String(position) + " / " + duration;
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(progress, width / 2, 176);
  tft.setTextDatum(TL_DATUM);
  lastProgressSecond_ = backend_.session().positionMs() / 1000U;
}

void MediaPlayerApp::drawError(TFT_eSPI& tft) {
  if (!dirty_) return;
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "Media Player", TFT_RED);
    CydUi::centered(canvas, backend_.session().error(), 72, 2, TFT_WHITE);
    CydUi::centered(canvas, "Use MJPEG + PCM AVI", 111, 1,
                    TFT_LIGHTGREY);
    CydUi::touchAction(canvas, RETRY_SD, "Retry SD", TFT_CYAN, false);
    CydUi::touchAction(canvas, MEDIA_LIST, "List", TFT_LIGHTGREY, false);
  });
  dirty_ = false;
}
