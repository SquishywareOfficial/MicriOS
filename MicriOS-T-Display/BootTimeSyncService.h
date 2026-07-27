#pragma once

#include <Arduino.h>

#include "src/shared/logic/ClockLogic.h"
#include "src/shared/logic/WiFiLogic.h"

class BootTimeSyncService {
 public:
  void begin();
  void update(uint32_t nowMs, bool radioAvailable);
  void suspend(uint32_t nowMs);

  bool enabled() const;
  void setEnabled(bool enabled, uint32_t nowMs);

 private:
  enum class State : uint8_t {
    Disabled,
    Pending,
    Connecting,
    Syncing,
    Complete,
    Failed
  };

  static bool timeIsValid();
  bool beginNextProfile(uint8_t startSlot, uint32_t nowMs);
  void beginNtp(uint32_t nowMs);
  void finishSuccess();
  void stopWifi();

  ClockLogic clock_;
  WiFiLogic wifi_;
  State state_ = State::Disabled;
  uint8_t currentSlot_ = 0;
  uint32_t stateStartedAtMs_ = 0;
  uint32_t resumeAfterMs_ = 0;
};
