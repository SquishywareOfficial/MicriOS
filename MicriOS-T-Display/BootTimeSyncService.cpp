#include "BootTimeSyncService.h"

#include <WiFi.h>
#include <time.h>

namespace {
constexpr uint32_t CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t SYNC_TIMEOUT_MS = 12000;
constexpr uint32_t RESUME_DELAY_MS = 1000;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;
constexpr const char* HOSTNAME = "MicriDeck-TDisplay";
}

void BootTimeSyncService::begin() {
  clock_.begin();
  wifi_.init();
  currentSlot_ = 0;
  stateStartedAtMs_ = millis();
  resumeAfterMs_ = stateStartedAtMs_;

  if (!clock_.bootSyncEnabled()) {
    state_ = State::Disabled;
  } else if (timeIsValid()) {
    state_ = State::Complete;
  } else {
    state_ = State::Pending;
  }

  Serial.printf("[time] boot sync %s, zone %s\n",
                clock_.bootSyncEnabled() ? "enabled" : "disabled",
                clock_.zoneLabel());
}

bool BootTimeSyncService::enabled() const {
  return clock_.bootSyncEnabled();
}

void BootTimeSyncService::setEnabled(bool enabled, uint32_t nowMs) {
  // Reload first so toggling this setting cannot overwrite Clock changes made
  // after the boot service initialized.
  clock_.loadSettings();

  if (!enabled) {
    stopWifi();
    clock_.setBootSyncEnabled(false);
    state_ = State::Disabled;
    Serial.println("[time] boot sync disabled");
    return;
  }

  clock_.setBootSyncEnabled(true);
  currentSlot_ = 0;
  stateStartedAtMs_ = nowMs;
  resumeAfterMs_ = nowMs;
  state_ = timeIsValid() ? State::Complete : State::Pending;
  Serial.printf("[time] boot sync enabled, zone %s\n", clock_.zoneLabel());
}

bool BootTimeSyncService::timeIsValid() {
  return time(nullptr) >= VALID_TIME_THRESHOLD;
}

void BootTimeSyncService::stopWifi() {
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
}

bool BootTimeSyncService::beginNextProfile(uint8_t startSlot, uint32_t nowMs) {
  for (uint8_t slot = startSlot; slot < WiFiLogic::MAX_PROFILES; slot++) {
    if (!wifi_.isProfileUsed(slot)) {
      continue;
    }

    currentSlot_ = slot;
    const String ssid = wifi_.getSsid(slot);
    const String password = wifi_.getPass(slot);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(HOSTNAME);
    WiFi.disconnect(false, false);
    WiFi.begin(ssid.c_str(), password.c_str());

    state_ = State::Connecting;
    stateStartedAtMs_ = nowMs;
    Serial.printf("[time] connecting to saved WiFi %u: %s\n",
                  static_cast<unsigned>(slot + 1), ssid.c_str());
    return true;
  }

  stopWifi();
  state_ = State::Failed;
  Serial.println(wifi_.getProfileCount() > 0
                     ? "[time] saved WiFi profiles exhausted"
                     : "[time] no saved WiFi profiles; boot sync skipped");
  return false;
}

void BootTimeSyncService::beginNtp(uint32_t nowMs) {
  const long offsetSeconds = static_cast<long>(clock_.offsetMinutes()) * 60L;
  configTime(offsetSeconds, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  state_ = State::Syncing;
  stateStartedAtMs_ = nowMs;
  Serial.printf("[time] requesting NTP for %s\n", clock_.zoneLabel());
}

void BootTimeSyncService::finishSuccess() {
  const time_t now = time(nullptr);
  const ClockLogic::TimeParts local = clock_.localParts(now);
  Serial.printf("[time] boot sync complete: %04u-%02u-%02u %02u:%02u:%02u %s\n",
                local.year, local.month, local.day,
                local.hour, local.minute, local.second, clock_.zoneLabel());
  stopWifi();
  state_ = State::Complete;
}

void BootTimeSyncService::suspend(uint32_t nowMs) {
  if (state_ != State::Connecting && state_ != State::Syncing) {
    return;
  }

  stopWifi();
  state_ = State::Pending;
  stateStartedAtMs_ = nowMs;
  resumeAfterMs_ = nowMs + RESUME_DELAY_MS;
  currentSlot_ = 0;
  Serial.println("[time] boot sync suspended for foreground app");
}

void BootTimeSyncService::update(uint32_t nowMs, bool radioAvailable) {
  if (!clock_.bootSyncEnabled() || state_ == State::Disabled ||
      state_ == State::Complete || state_ == State::Failed) {
    return;
  }

  if (timeIsValid()) {
    if (state_ == State::Connecting || state_ == State::Syncing) {
      finishSuccess();
    } else {
      state_ = State::Complete;
    }
    return;
  }

  if (!radioAvailable) {
    suspend(nowMs);
    return;
  }

  if (state_ == State::Pending) {
    if (static_cast<int32_t>(nowMs - resumeAfterMs_) >= 0) {
      wifi_.loadProfiles();
      beginNextProfile(0, nowMs);
    }
    return;
  }

  if (state_ == State::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      beginNtp(nowMs);
    } else if ((nowMs - stateStartedAtMs_) >= CONNECT_TIMEOUT_MS) {
      beginNextProfile(currentSlot_ + 1, nowMs);
    }
    return;
  }

  if (state_ == State::Syncing &&
      (nowMs - stateStartedAtMs_) >= SYNC_TIMEOUT_MS) {
    stopWifi();
    state_ = State::Failed;
    Serial.println("[time] NTP request timed out");
  }
}
