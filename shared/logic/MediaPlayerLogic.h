#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace MediaPlayerLogic {

constexpr uint8_t MAX_ITEMS = 64;
constexpr size_t MAX_PATH_LENGTH = 112;
constexpr size_t MAX_TITLE_LENGTH = 56;

enum class State : uint8_t {
  Browsing,
  Opening,
  Playing,
  Paused,
  Finished,
  Error,
};

struct Item {
  char path[MAX_PATH_LENGTH] = {};
  char title[MAX_TITLE_LENGTH] = {};
  uint32_t sizeBytes = 0;
  bool directory = false;
};

struct Metadata {
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t frameDurationUs = 83333;
  uint32_t frameCount = 0;
  uint32_t durationMs = 0;
  uint32_t audioSampleRate = 0;
  uint16_t audioBits = 0;
  uint8_t audioChannels = 0;
  bool motionJpeg = false;
  bool pcmAudio = false;
};

inline char lowerAscii(char value) {
  return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

inline bool equalsIgnoreCase(const char* first, const char* second) {
  if (first == nullptr || second == nullptr) return false;
  while (*first != '\0' && *second != '\0') {
    if (lowerAscii(*first++) != lowerAscii(*second++)) return false;
  }
  return *first == '\0' && *second == '\0';
}

inline bool hasAviExtension(const char* path) {
  if (path == nullptr) return false;
  const char* dot = strrchr(path, '.');
  return dot != nullptr && equalsIgnoreCase(dot, ".avi");
}

inline bool hasWavExtension(const char* path) {
  if (path == nullptr) return false;
  const char* dot = strrchr(path, '.');
  return dot != nullptr && equalsIgnoreCase(dot, ".wav");
}

inline bool hasSupportedMediaExtension(const char* path) {
  return hasAviExtension(path) || hasWavExtension(path);
}

inline void copyText(char* destination, size_t capacity, const char* source) {
  if (destination == nullptr || capacity == 0) return;
  if (source == nullptr) source = "";
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

inline void titleFromPath(const char* path, char* destination,
                          size_t capacity) {
  if (destination == nullptr || capacity == 0) return;
  const char* name = path == nullptr ? "" : strrchr(path, '/');
  name = name == nullptr ? (path == nullptr ? "" : path) : name + 1;
  copyText(destination, capacity, name);
  char* extension = strrchr(destination, '.');
  if (extension != nullptr) *extension = '\0';
  for (char* cursor = destination; *cursor != '\0'; ++cursor) {
    if (*cursor == '_' || *cursor == '-') *cursor = ' ';
  }
}

class Catalog {
 public:
  void clear() {
    count_ = 0;
    selected_ = 0;
  }

  bool add(const char* path, uint32_t sizeBytes) {
    if (count_ >= MAX_ITEMS || path == nullptr ||
        strlen(path) >= MAX_PATH_LENGTH || !hasSupportedMediaExtension(path)) {
      return false;
    }
    Item& item = items_[count_++];
    copyText(item.path, sizeof(item.path), path);
    titleFromPath(path, item.title, sizeof(item.title));
    item.sizeBytes = sizeBytes;
    item.directory = false;
    return true;
  }

  bool addDirectory(const char* path) {
    if (count_ >= MAX_ITEMS || path == nullptr || path[0] == '\0' ||
        strlen(path) >= MAX_PATH_LENGTH) {
      return false;
    }
    Item& item = items_[count_++];
    copyText(item.path, sizeof(item.path), path);
    const char* name = strrchr(path, '/');
    name = name == nullptr ? path : name + 1;
    copyText(item.title, sizeof(item.title), name);
    for (char* cursor = item.title; *cursor != '\0'; ++cursor) {
      if (*cursor == '_' || *cursor == '-') *cursor = ' ';
    }
    item.sizeBytes = 0;
    item.directory = true;
    return true;
  }

  void sort() {
    for (uint8_t index = 1; index < count_; ++index) {
      Item moving = items_[index];
      uint8_t slot = index;
      while (slot > 0 && compare(items_[slot - 1], moving) > 0) {
        items_[slot] = items_[slot - 1];
        --slot;
      }
      items_[slot] = moving;
    }
  }

  uint8_t count() const { return count_; }
  bool empty() const { return count_ == 0; }

  const Item* item(uint8_t index) const {
    return index < count_ ? &items_[index] : nullptr;
  }

  uint8_t selected() const { return selected_; }

  void select(uint8_t index) {
    if (index < count_) selected_ = index;
  }

 private:
  static int compare(const Item& firstItem, const Item& secondItem) {
    if (firstItem.directory != secondItem.directory) {
      return firstItem.directory ? -1 : 1;
    }
    const char* first = firstItem.title;
    const char* second = secondItem.title;
    while (*first != '\0' && *second != '\0') {
      const char a = lowerAscii(*first++);
      const char b = lowerAscii(*second++);
      if (a != b) return static_cast<unsigned char>(a) -
                         static_cast<unsigned char>(b);
    }
    return static_cast<unsigned char>(*first) -
           static_cast<unsigned char>(*second);
  }

  Item items_[MAX_ITEMS];
  uint8_t count_ = 0;
  uint8_t selected_ = 0;
};

class Session {
 public:
  State state() const { return state_; }
  const Metadata& metadata() const { return metadata_; }
  uint32_t positionMs() const { return positionMs_; }
  const char* error() const { return error_; }
  bool muted() const { return muted_; }

  void browsing() {
    state_ = State::Browsing;
    positionMs_ = 0;
    error_[0] = '\0';
  }

  void opening() {
    state_ = State::Opening;
    positionMs_ = 0;
    error_[0] = '\0';
  }

  void playing(const Metadata& metadata) {
    metadata_ = metadata;
    state_ = State::Playing;
    positionMs_ = 0;
    error_[0] = '\0';
  }

  void setPosition(uint32_t positionMs) { positionMs_ = positionMs; }
  void seekPaused(uint32_t positionMs) {
    positionMs_ = positionMs;
    state_ = State::Paused;
    error_[0] = '\0';
  }
  void pause() {
    if (state_ == State::Playing) state_ = State::Paused;
  }
  void resume() {
    if (state_ == State::Paused) state_ = State::Playing;
  }
  void finish() { state_ = State::Finished; }
  void setMuted(bool muted) { muted_ = muted; }

  void fail(const char* message) {
    copyText(error_, sizeof(error_), message);
    state_ = State::Error;
  }

 private:
  State state_ = State::Browsing;
  Metadata metadata_;
  uint32_t positionMs_ = 0;
  bool muted_ = false;
  char error_[80] = {};
};

inline void formatDuration(uint32_t milliseconds, char* destination,
                           size_t capacity) {
  if (destination == nullptr || capacity == 0) return;
  const uint32_t totalSeconds = milliseconds / 1000;
  const uint32_t hours = totalSeconds / 3600;
  const uint32_t minutes = (totalSeconds / 60) % 60;
  const uint32_t seconds = totalSeconds % 60;
  if (hours > 0) {
    snprintf(destination, capacity, "%lu:%02lu:%02lu",
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
  } else {
    snprintf(destination, capacity, "%lu:%02lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
  }
}

}  // namespace MediaPlayerLogic
