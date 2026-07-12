#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

struct ClockZone {
  int16_t minutes;
  const char* label;
};

static constexpr ClockZone CLOCK_ZONES[] = {
    {-720, "UTC-12:00"}, {-660, "UTC-11:00"}, {-600, "UTC-10:00"}, {-570, "UTC-09:30"},
    {-540, "UTC-09:00"}, {-480, "UTC-08:00"}, {-420, "UTC-07:00"}, {-360, "UTC-06:00"},
    {-300, "UTC-05:00"}, {-240, "UTC-04:00"}, {-210, "UTC-03:30"}, {-180, "UTC-03:00"},
    {-120, "UTC-02:00"}, {-60, "UTC-01:00"},  {0, "UTC+00:00"},   {60, "UTC+01:00"},
    {120, "UTC+02:00"},  {180, "UTC+03:00"},   {210, "UTC+03:30"},  {240, "UTC+04:00"},
    {270, "UTC+04:30"},  {300, "UTC+05:00"},   {330, "UTC+05:30"},  {345, "UTC+05:45"},
    {360, "UTC+06:00"},  {390, "UTC+06:30"},   {420, "UTC+07:00"},  {480, "UTC+08:00"},
    {525, "UTC+08:45"},  {540, "UTC+09:00"},   {570, "UTC+09:30"},  {600, "UTC+10:00"},
    {630, "UTC+10:30"},  {660, "UTC+11:00"},   {720, "UTC+12:00"},  {765, "UTC+12:45"},
    {780, "UTC+13:00"},  {840, "UTC+14:00"},
};

class ClockLogic {
  public:
    enum class View : uint8_t { Face, Options, Timezone };
    enum class Face : uint8_t { Digital, Analog, UnixTime, Options };
    enum class Option : uint8_t { ShowDate, NextZone, Timezone, AddZone, DeleteZone, Use24Hour, ShowOffset, SyncNow, Back, Exit };
    enum class Action : uint8_t { None, RequestSync, RequestExit };

    struct TimeParts {
      uint16_t year = 1970;
      uint8_t month = 1;
      uint8_t day = 1;
      uint8_t hour = 0;
      uint8_t minute = 0;
      uint8_t second = 0;
      uint8_t weekday = 4;
    };

    static constexpr uint8_t FACE_COUNT = 4;
    static constexpr uint8_t OPTION_COUNT = 10;
    static constexpr uint8_t MAX_SAVED_ZONES = 4;
    static constexpr uint8_t ZONE_COUNT = sizeof(CLOCK_ZONES) / sizeof(CLOCK_ZONES[0]);

    void begin() {
      loadSettings();
      view_ = View::Face;
      face_ = Face::Digital;
      option_ = Option::ShowDate;
      pickerZoneIndex_ = zoneIndex();
    }

    void loadSettings() {
      prefs_.begin("clock", true);
      showDate_ = prefs_.getBool("date", false);
      use24Hour_ = prefs_.getBool("24h", true);
      showOffset_ = prefs_.getBool("offset", true);
      const int16_t legacyOffset = prefs_.getShort("tz", 0);
      const uint8_t savedCount = prefs_.getUChar("zcount", 0);
      const uint8_t savedActive = prefs_.getUChar("active", 0);

      if (savedCount == 0 || savedCount > MAX_SAVED_ZONES) {
        savedZoneCount_ = 1;
        savedZoneIndices_[0] = findZoneIndex(legacyOffset);
      } else {
        savedZoneCount_ = savedCount;
        for (uint8_t i = 0; i < savedZoneCount_; i++) {
          const int16_t offset = prefs_.getShort(zoneKey(i), i == 0 ? legacyOffset : 0);
          savedZoneIndices_[i] = findZoneIndex(offset);
        }
      }
      prefs_.end();

      activeZoneSlot_ = savedActive < savedZoneCount_ ? savedActive : 0;
      pickerZoneIndex_ = zoneIndex();
    }

    void saveSettings() {
      prefs_.begin("clock", false);
      prefs_.putBool("date", showDate_);
      prefs_.putBool("24h", use24Hour_);
      prefs_.putBool("offset", showOffset_);
      prefs_.putShort("tz", offsetMinutes());
      prefs_.putUChar("zcount", savedZoneCount_);
      prefs_.putUChar("active", activeZoneSlot_);
      for (uint8_t i = 0; i < MAX_SAVED_ZONES; i++) {
        prefs_.putShort(zoneKey(i), CLOCK_ZONES[savedZoneIndices_[i]].minutes);
      }
      prefs_.end();
    }

    View view() const { return view_; }
    Face face() const { return face_; }
    Option option() const { return option_; }
    bool showDate() const { return showDate_; }
    bool use24Hour() const { return use24Hour_; }
    bool showOffset() const { return showOffset_; }
    uint8_t zoneIndex() const { return savedZoneIndices_[activeZoneSlot_]; }
    uint8_t pickerZoneIndex() const { return pickerZoneIndex_; }
    uint8_t savedZoneCount() const { return savedZoneCount_; }
    uint8_t activeZoneSlot() const { return activeZoneSlot_; }
    bool canAddZone() const { return savedZoneCount_ < MAX_SAVED_ZONES; }
    bool canDeleteZone() const { return savedZoneCount_ > 1; }
    int16_t offsetMinutes() const { return CLOCK_ZONES[zoneIndex()].minutes; }
    const char* zoneLabel() const { return CLOCK_ZONES[zoneIndex()].label; }
    const char* pickerZoneLabel() const { return CLOCK_ZONES[pickerZoneIndex_].label; }

    const char* faceLabel() const {
      switch (face_) {
        case Face::Digital:
          return "Digital";
        case Face::Analog:
          return "Analog";
        case Face::UnixTime:
          return "Unix Time";
        case Face::Options:
        default:
          return "Options";
      }
    }

    const char* optionLabel() const {
      switch (option_) {
        case Option::ShowDate:
          return "Show Date";
        case Option::NextZone:
          return "Next Zone";
        case Option::Timezone:
          return "Edit Zone";
        case Option::AddZone:
          return "Add Zone";
        case Option::DeleteZone:
          return "Del Zone";
        case Option::Use24Hour:
          return "24 Hour";
        case Option::ShowOffset:
          return "Show Offset";
        case Option::SyncNow:
          return "Sync Now";
        case Option::Back:
          return "Back";
        case Option::Exit:
        default:
          return "Exit";
      }
    }

    const char* optionValue() const {
      switch (option_) {
        case Option::ShowDate:
          return showDate_ ? "YES" : "NO";
        case Option::NextZone:
          return zoneLabel();
        case Option::Timezone:
          return zoneLabel();
        case Option::AddZone:
          return canAddZone() ? "Add" : "Full";
        case Option::DeleteZone:
          return canDeleteZone() ? zoneLabel() : "Keep1";
        case Option::Use24Hour:
          return use24Hour_ ? "YES" : "NO";
        case Option::ShowOffset:
          return showOffset_ ? "YES" : "NO";
        case Option::SyncNow:
          return "NTP";
        case Option::Back:
          return "Clock";
        case Option::Exit:
        default:
          return "Menu";
      }
    }

    void nextFace() {
      face_ = static_cast<Face>((static_cast<uint8_t>(face_) + 1) % FACE_COUNT);
    }

    void previousFace() {
      const uint8_t raw = static_cast<uint8_t>(face_);
      face_ = static_cast<Face>(raw == 0 ? FACE_COUNT - 1 : raw - 1);
    }

    Action selectFace() {
      if (face_ == Face::Options) {
        view_ = View::Options;
        option_ = Option::ShowDate;
      }
      return Action::None;
    }

    void nextOption() {
      option_ = static_cast<Option>((static_cast<uint8_t>(option_) + 1) % OPTION_COUNT);
    }

    void previousOption() {
      const uint8_t raw = static_cast<uint8_t>(option_);
      option_ = static_cast<Option>(raw == 0 ? OPTION_COUNT - 1 : raw - 1);
    }

    Action activateOption() {
      switch (option_) {
        case Option::ShowDate:
          showDate_ = !showDate_;
          saveSettings();
          return Action::None;
        case Option::NextZone:
          nextSavedZone();
          saveSettings();
          return Action::None;
        case Option::Timezone:
          beginZoneEdit(false);
          return Action::None;
        case Option::AddZone:
          beginZoneEdit(true);
          return Action::None;
        case Option::DeleteZone:
          deleteActiveZone();
          saveSettings();
          return Action::None;
        case Option::Use24Hour:
          use24Hour_ = !use24Hour_;
          saveSettings();
          return Action::None;
        case Option::ShowOffset:
          showOffset_ = !showOffset_;
          saveSettings();
          return Action::None;
        case Option::SyncNow:
          return Action::RequestSync;
        case Option::Back:
          view_ = View::Face;
          return Action::None;
        case Option::Exit:
        default:
          return Action::RequestExit;
      }
    }

    void beginZoneEdit(bool addNew) {
      const uint8_t sourceZone = zoneIndex();
      if (addNew && canAddZone()) {
        activeZoneSlot_ = savedZoneCount_;
        savedZoneIndices_[activeZoneSlot_] = sourceZone;
        savedZoneCount_++;
        saveSettings();
      }
      pickerZoneIndex_ = zoneIndex();
      view_ = View::Timezone;
    }

    void nextSavedZone() {
      if (savedZoneCount_ <= 1) {
        return;
      }
      activeZoneSlot_ = (activeZoneSlot_ + 1) % savedZoneCount_;
      pickerZoneIndex_ = zoneIndex();
    }

    void previousSavedZone() {
      if (savedZoneCount_ <= 1) {
        return;
      }
      activeZoneSlot_ = activeZoneSlot_ == 0 ? savedZoneCount_ - 1 : activeZoneSlot_ - 1;
      pickerZoneIndex_ = zoneIndex();
    }

    void deleteActiveZone() {
      if (savedZoneCount_ <= 1) {
        return;
      }
      for (uint8_t i = activeZoneSlot_; i + 1 < savedZoneCount_; i++) {
        savedZoneIndices_[i] = savedZoneIndices_[i + 1];
      }
      savedZoneCount_--;
      if (activeZoneSlot_ >= savedZoneCount_) {
        activeZoneSlot_ = savedZoneCount_ - 1;
      }
      pickerZoneIndex_ = zoneIndex();
    }

    void nextZone() {
      pickerZoneIndex_ = (pickerZoneIndex_ + 1) % ZONE_COUNT;
      savedZoneIndices_[activeZoneSlot_] = pickerZoneIndex_;
      saveSettings();
    }

    void previousZone() {
      pickerZoneIndex_ = pickerZoneIndex_ == 0 ? ZONE_COUNT - 1 : pickerZoneIndex_ - 1;
      savedZoneIndices_[activeZoneSlot_] = pickerZoneIndex_;
      saveSettings();
    }

    void closeTimezone() {
      view_ = View::Options;
      saveSettings();
    }

    void formatZonePosition(char* out, size_t outSize) const {
      snprintf(out, outSize, "%u/%u", activeZoneSlot_ + 1, savedZoneCount_);
    }

    static void formatSyncAge(uint32_t ageSeconds, char* out, size_t outSize) {
      if (ageSeconds < 60) {
        snprintf(out, outSize, "sync %lus", static_cast<unsigned long>(ageSeconds));
      } else if (ageSeconds < 3600) {
        snprintf(out, outSize, "sync %lum", static_cast<unsigned long>(ageSeconds / 60));
      } else if (ageSeconds < 86400) {
        snprintf(out, outSize, "sync %luh", static_cast<unsigned long>(ageSeconds / 3600));
      } else {
        snprintf(out, outSize, "sync %lud", static_cast<unsigned long>(ageSeconds / 86400));
      }
    }

    TimeParts localParts(time_t utc) const {
      time_t adjusted = utc + static_cast<time_t>(offsetMinutes()) * 60;
      tm t;
      gmtime_r(&adjusted, &t);

      TimeParts parts;
      parts.year = static_cast<uint16_t>(t.tm_year + 1900);
      parts.month = static_cast<uint8_t>(t.tm_mon + 1);
      parts.day = static_cast<uint8_t>(t.tm_mday);
      parts.hour = static_cast<uint8_t>(t.tm_hour);
      parts.minute = static_cast<uint8_t>(t.tm_min);
      parts.second = static_cast<uint8_t>(t.tm_sec);
      parts.weekday = static_cast<uint8_t>(t.tm_wday);
      return parts;
    }

    void formatTime(time_t utc, char* out, size_t outSize, bool withSeconds = true) const {
      const TimeParts parts = localParts(utc);
      if (use24Hour_) {
        if (withSeconds) {
          snprintf(out, outSize, "%02u:%02u:%02u", parts.hour, parts.minute, parts.second);
        } else {
          snprintf(out, outSize, "%02u:%02u", parts.hour, parts.minute);
        }
        return;
      }

      uint8_t hour = parts.hour % 12;
      if (hour == 0) hour = 12;
      const char suffix = parts.hour >= 12 ? 'p' : 'a';
      if (withSeconds) {
        snprintf(out, outSize, "%u:%02u:%02u%c", hour, parts.minute, parts.second, suffix);
      } else {
        snprintf(out, outSize, "%u:%02u%c", hour, parts.minute, suffix);
      }
    }

    void formatDate(time_t utc, char* out, size_t outSize) const {
      const TimeParts parts = localParts(utc);
      snprintf(out, outSize, "%02u/%02u/%02u", parts.day, parts.month, parts.year % 100);
    }

    void formatLongDate(time_t utc, char* out, size_t outSize) const {
      const TimeParts parts = localParts(utc);
      snprintf(out, outSize, "%04u-%02u-%02u", parts.year, parts.month, parts.day);
    }

  private:
    uint8_t findZoneIndex(int16_t minutes) const {
      for (uint8_t i = 0; i < ZONE_COUNT; i++) {
        if (CLOCK_ZONES[i].minutes == minutes) {
          return i;
        }
      }
      return 14;
    }

    const char* zoneKey(uint8_t index) const {
      switch (index) {
        case 0:
          return "zone0";
        case 1:
          return "zone1";
        case 2:
          return "zone2";
        case 3:
        default:
          return "zone3";
      }
    }

    Preferences prefs_;
    View view_ = View::Face;
    Face face_ = Face::Digital;
    Option option_ = Option::ShowDate;
    uint8_t savedZoneIndices_[MAX_SAVED_ZONES] = {14, 14, 14, 14};
    uint8_t savedZoneCount_ = 1;
    uint8_t activeZoneSlot_ = 0;
    uint8_t pickerZoneIndex_ = 14;
    bool showDate_ = false;
    bool use24Hour_ = true;
    bool showOffset_ = true;
};
