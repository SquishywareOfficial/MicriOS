#pragma once

#include <stdint.h>

namespace BatterySessionLogic {

constexpr uint32_t RECORD_MAGIC = 0x4D425452UL;  // "MBTR"
constexpr uint16_t RECORD_VERSION = 1;
constexpr uint8_t FLAG_VALID = 0x01;
constexpr uint8_t FLAG_COMPLETE = 0x02;
constexpr uint8_t FLAG_INTERRUPTED = 0x04;
constexpr uint16_t USB_THRESHOLD_MV = 4500;
constexpr uint16_t BATTERY_THRESHOLD_MV = 4400;
constexpr uint64_t MINUTE_US = 60ULL * 1000ULL * 1000ULL;
constexpr uint64_t CHECKPOINT_INTERVAL_US = 30ULL * MINUTE_US;

enum class PowerSource : uint8_t {
  Unknown,
  Battery,
  Usb,
};

enum class EndReason : uint8_t {
  None,
  UsbConnected,
  ManualSleep,
  LowBattery,
  Disabled,
  Interrupted,
};

enum class PersistenceAction : uint8_t {
  None,
  SaveCurrent,
  FinalizeToLast,
};

struct SessionRecord {
  uint32_t magic = RECORD_MAGIC;
  uint16_t version = RECORD_VERSION;
  uint8_t flags = 0;
  uint8_t endReason = static_cast<uint8_t>(EndReason::None);
  uint32_t elapsedMinutes = 0;
  uint16_t minimumMillivolts = 0;
  uint16_t endingMillivolts = 0;
  uint32_t checksum = 0;
};

struct Snapshot {
  PowerSource source = PowerSource::Unknown;
  bool currentActive = false;
  bool currentInterrupted = false;
  uint32_t currentMinutes = 0;
  uint16_t currentMinimumMillivolts = 0;
  uint16_t currentMillivolts = 0;
  bool lastValid = false;
  bool lastComplete = false;
  bool lastInterrupted = false;
  EndReason lastEndReason = EndReason::None;
  uint32_t lastMinutes = 0;
  uint16_t lastMinimumMillivolts = 0;
  uint16_t lastEndingMillivolts = 0;
};

struct PersistenceUpdate {
  PersistenceAction action = PersistenceAction::None;
  SessionRecord record;
};

inline uint32_t checksumFor(const SessionRecord& record) {
  uint32_t hash = 2166136261UL;
  const auto mix = [&hash](uint32_t value, uint8_t bytes) {
    for (uint8_t i = 0; i < bytes; ++i) {
      hash ^= static_cast<uint8_t>(value >> (i * 8));
      hash *= 16777619UL;
    }
  };
  mix(record.magic, 4);
  mix(record.version, 2);
  mix(record.flags, 1);
  mix(record.endReason, 1);
  mix(record.elapsedMinutes, 4);
  mix(record.minimumMillivolts, 2);
  mix(record.endingMillivolts, 2);
  return hash;
}

inline void seal(SessionRecord& record) {
  record.checksum = checksumFor(record);
}

inline bool isValid(const SessionRecord& record) {
  if (record.magic != RECORD_MAGIC || record.version != RECORD_VERSION ||
      (record.flags & FLAG_VALID) == 0) {
    return false;
  }
  if (record.minimumMillivolts < 2500 ||
      record.minimumMillivolts > 5000 ||
      record.endingMillivolts < 2500 ||
      record.endingMillivolts > 5000) {
    return false;
  }
  return record.checksum == checksumFor(record);
}

inline PowerSource classifyPowerSource(uint16_t millivolts,
                                       PowerSource previous) {
  if (millivolts > USB_THRESHOLD_MV) {
    return PowerSource::Usb;
  }
  if (millivolts < BATTERY_THRESHOLD_MV) {
    return PowerSource::Battery;
  }
  return previous;
}

class Tracker {
 public:
  void restore(const SessionRecord* current, const SessionRecord* last) {
    reset();
    if (last != nullptr && isValid(*last)) {
      last_ = *last;
      lastValid_ = true;
    }
    if (current != nullptr && isValid(*current) &&
        (current->flags & FLAG_COMPLETE) == 0) {
      restoredCurrent_ = *current;
      restoredCurrentValid_ = true;
    }
  }

  void reset() {
    source_ = PowerSource::Unknown;
    current_ = SessionRecord{};
    last_ = SessionRecord{};
    restoredCurrent_ = SessionRecord{};
    currentActive_ = false;
    lastValid_ = false;
    restoredCurrentValid_ = false;
    currentStartedUs_ = 0;
    baseMinutes_ = 0;
    nextCheckpointUs_ = 0;
    pending_ = PersistenceUpdate{};
  }

  void observeVoltage(uint16_t millivolts, uint64_t nowUs) {
    const PowerSource classified =
        classifyPowerSource(millivolts, source_);
    if (classified == PowerSource::Unknown) {
      return;
    }

    if (source_ == PowerSource::Unknown) {
      source_ = classified;
      if (source_ == PowerSource::Battery) {
        resumeOrStart(millivolts, nowUs);
      } else {
        promoteInterruptedRecord();
      }
      return;
    }

    if (classified != source_) {
      if (source_ == PowerSource::Battery && classified == PowerSource::Usb) {
        finish(EndReason::UsbConnected, nowUs);
      } else if (source_ == PowerSource::Usb &&
                 classified == PowerSource::Battery) {
        startNew(millivolts, nowUs);
      }
      source_ = classified;
      return;
    }

    if (source_ == PowerSource::Battery && currentActive_) {
      updateVoltage(millivolts);
      if (nowUs >= nextCheckpointUs_) {
        queueCurrentCheckpoint(nowUs);
        nextCheckpointUs_ = nowUs + CHECKPOINT_INTERVAL_US;
      }
    }
  }

  void finish(EndReason reason, uint64_t nowUs) {
    if (!currentActive_) {
      return;
    }
    updateElapsed(nowUs);
    current_.flags |= FLAG_COMPLETE;
    current_.endReason = static_cast<uint8_t>(reason);
    seal(current_);
    last_ = current_;
    lastValid_ = true;
    currentActive_ = false;
    queuePersistence(PersistenceAction::FinalizeToLast, current_);
  }

  bool takePersistenceUpdate(PersistenceUpdate& update) {
    if (pending_.action == PersistenceAction::None) {
      return false;
    }
    update = pending_;
    pending_ = PersistenceUpdate{};
    return true;
  }

  Snapshot snapshot(uint64_t nowUs) const {
    Snapshot result;
    result.source = source_;
    result.currentActive = currentActive_;
    if (currentActive_) {
      result.currentInterrupted =
          (current_.flags & FLAG_INTERRUPTED) != 0;
      result.currentMinutes =
          baseMinutes_ +
          static_cast<uint32_t>((nowUs - currentStartedUs_) / MINUTE_US);
      result.currentMinimumMillivolts = current_.minimumMillivolts;
      result.currentMillivolts = current_.endingMillivolts;
    }

    result.lastValid = lastValid_;
    if (lastValid_) {
      result.lastComplete = (last_.flags & FLAG_COMPLETE) != 0;
      result.lastInterrupted =
          (last_.flags & FLAG_INTERRUPTED) != 0 ||
          (last_.flags & FLAG_COMPLETE) == 0;
      result.lastEndReason = static_cast<EndReason>(last_.endReason);
      result.lastMinutes = last_.elapsedMinutes;
      result.lastMinimumMillivolts = last_.minimumMillivolts;
      result.lastEndingMillivolts = last_.endingMillivolts;
    }
    return result;
  }

 private:
  void resumeOrStart(uint16_t millivolts, uint64_t nowUs) {
    if (restoredCurrentValid_) {
      current_ = restoredCurrent_;
      current_.flags |= FLAG_VALID | FLAG_INTERRUPTED;
      current_.flags &= static_cast<uint8_t>(~FLAG_COMPLETE);
      current_.endReason = static_cast<uint8_t>(EndReason::None);
      baseMinutes_ = current_.elapsedMinutes;
      updateVoltage(millivolts);
      currentStartedUs_ = nowUs;
      nextCheckpointUs_ = nowUs + CHECKPOINT_INTERVAL_US;
      currentActive_ = true;
      restoredCurrentValid_ = false;
      return;
    }
    startNew(millivolts, nowUs);
  }

  void startNew(uint16_t millivolts, uint64_t nowUs) {
    current_ = SessionRecord{};
    current_.flags = FLAG_VALID;
    current_.minimumMillivolts = millivolts;
    current_.endingMillivolts = millivolts;
    baseMinutes_ = 0;
    currentStartedUs_ = nowUs;
    nextCheckpointUs_ = nowUs + CHECKPOINT_INTERVAL_US;
    currentActive_ = true;
    restoredCurrentValid_ = false;
    queueCurrentCheckpoint(nowUs);
  }

  void updateVoltage(uint16_t millivolts) {
    current_.endingMillivolts = millivolts;
    if (current_.minimumMillivolts == 0 ||
        millivolts < current_.minimumMillivolts) {
      current_.minimumMillivolts = millivolts;
    }
  }

  void updateElapsed(uint64_t nowUs) {
    current_.elapsedMinutes =
        baseMinutes_ +
        static_cast<uint32_t>((nowUs - currentStartedUs_) / MINUTE_US);
  }

  void queueCurrentCheckpoint(uint64_t nowUs) {
    updateElapsed(nowUs);
    current_.flags &= static_cast<uint8_t>(~FLAG_COMPLETE);
    current_.endReason = static_cast<uint8_t>(EndReason::None);
    seal(current_);
    queuePersistence(PersistenceAction::SaveCurrent, current_);
  }

  void promoteInterruptedRecord() {
    if (!restoredCurrentValid_) {
      return;
    }
    SessionRecord interrupted = restoredCurrent_;
    interrupted.flags |= FLAG_VALID | FLAG_INTERRUPTED;
    interrupted.flags &= static_cast<uint8_t>(~FLAG_COMPLETE);
    interrupted.endReason = static_cast<uint8_t>(EndReason::Interrupted);
    seal(interrupted);
    last_ = interrupted;
    lastValid_ = true;
    restoredCurrentValid_ = false;
    queuePersistence(PersistenceAction::FinalizeToLast, interrupted);
  }

  void queuePersistence(PersistenceAction action,
                        const SessionRecord& record) {
    pending_.action = action;
    pending_.record = record;
  }

  PowerSource source_ = PowerSource::Unknown;
  SessionRecord current_;
  SessionRecord last_;
  SessionRecord restoredCurrent_;
  bool currentActive_ = false;
  bool lastValid_ = false;
  bool restoredCurrentValid_ = false;
  uint64_t currentStartedUs_ = 0;
  uint32_t baseMinutes_ = 0;
  uint64_t nextCheckpointUs_ = 0;
  PersistenceUpdate pending_;
};

}  // namespace BatterySessionLogic
