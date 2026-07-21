#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

// ESP32-S3-Zero is a dedicated dual-core, no-display cluster miner. Core 1
// drives the hardware SHA worker; core 0 can run the optional software helper.
#define MICRI_CLUSTER_SLAVE_BATCH_NONCES 8192
#define MICRI_CLUSTER_SLAVE_TASK_STACK 8192
#define MICRI_CLUSTER_SLAVE_TASK_PRIORITY 3

#ifndef MICRI_CLUSTER_S3_SOFTWARE_WORKER
#define MICRI_CLUSTER_S3_SOFTWARE_WORKER 1
#endif

#include "src/shared/Version.h"
#include "src/shared/logic/MinerClusterLogic.h"

namespace {
constexpr uint8_t LED_PIN = 21;
constexpr uint8_t BUTTON_PIN = 0;
constexpr uint32_t LONG_HOLD_MS = 5000;
constexpr uint32_t TAP_GAP_MS = 450;
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr uint32_t MIN_LED_WRITE_MS = 35;
constexpr uint32_t MINING_GRACE_MS = 3000;
constexpr const char* DEVICE_PREF_NS = "s3zero";
constexpr const char* LED_ENABLED_PREF_KEY = "led";

struct Rgb {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

constexpr Rgb LED_OFF = {0, 0, 0};
constexpr Rgb LED_WHITE = {30, 30, 30};
constexpr Rgb LED_RED = {36, 0, 0};
constexpr Rgb LED_AMBER = {34, 12, 0};
constexpr Rgb LED_BLUE = {0, 0, 36};
constexpr Rgb LED_GREEN = {0, 32, 0};
constexpr Rgb LED_PURPLE = {24, 0, 30};
constexpr Rgb LED_CYAN = {0, 28, 28};

bool sameColor(const Rgb& a, const Rgb& b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

Rgb scaledColor(const Rgb& color, uint8_t scale) {
  return {
      static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale) / 255),
      static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale) / 255),
      static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale) / 255),
  };
}

enum class ButtonEvent : uint8_t {
  None,
  OneTap,
  TwoTaps,
  ThreeTaps,
  Hold5s,
};

struct LedStep {
  Rgb color;
  uint16_t durationMs;
};

class LedProgram {
public:
  void start(const LedStep* steps, uint8_t count, uint32_t nowMs) {
    count_ = min<uint8_t>(count, MAX_STEPS);
    for (uint8_t i = 0; i < count_; ++i) steps_[i] = steps[i];
    index_ = 0;
    stepStartedAtMs_ = nowMs;
    active_ = count_ > 0;
  }

  void cancel() { active_ = false; }
  bool active() const { return active_; }

  Rgb update(uint32_t nowMs) {
    if (!active_ || count_ == 0) return LED_OFF;
    while (active_ && nowMs - stepStartedAtMs_ >= steps_[index_].durationMs) {
      stepStartedAtMs_ += steps_[index_].durationMs;
      index_++;
      if (index_ >= count_) active_ = false;
    }
    return active_ ? steps_[index_].color : LED_OFF;
  }

private:
  static constexpr uint8_t MAX_STEPS = 8;
  LedStep steps_[MAX_STEPS] = {};
  uint8_t count_ = 0;
  uint8_t index_ = 0;
  uint32_t stepStartedAtMs_ = 0;
  bool active_ = false;
};

class ButtonTracker {
public:
  void begin(bool down, uint32_t nowMs) {
    rawDown_ = down;
    stableDown_ = down;
    lastRawChangeMs_ = nowMs;
    pressedAtMs_ = down ? nowMs : 0;
  }

  ButtonEvent update(bool down, uint32_t nowMs) {
    if (down != rawDown_) {
      rawDown_ = down;
      lastRawChangeMs_ = nowMs;
    }

    if (rawDown_ != stableDown_ && nowMs - lastRawChangeMs_ >= DEBOUNCE_MS) {
      stableDown_ = rawDown_;
      if (stableDown_) {
        pressedAtMs_ = nowMs;
        holdEmitted_ = false;
      } else {
        if (!holdEmitted_) {
          tapCount_++;
          lastTapMs_ = nowMs;
        }
        pressedAtMs_ = 0;
        holdEmitted_ = false;
      }
    }

    if (stableDown_ && !holdEmitted_ && pressedAtMs_ != 0 && nowMs - pressedAtMs_ >= LONG_HOLD_MS) {
      holdEmitted_ = true;
      tapCount_ = 0;
      return ButtonEvent::Hold5s;
    }

    if (!stableDown_ && tapCount_ > 0 && nowMs - lastTapMs_ >= TAP_GAP_MS) {
      const uint8_t taps = tapCount_;
      tapCount_ = 0;
      if (taps == 1) return ButtonEvent::OneTap;
      if (taps == 2) return ButtonEvent::TwoTaps;
      if (taps == 3) return ButtonEvent::ThreeTaps;
    }
    return ButtonEvent::None;
  }

private:
  bool rawDown_ = false;
  bool stableDown_ = false;
  bool holdEmitted_ = false;
  uint8_t tapCount_ = 0;
  uint32_t lastRawChangeMs_ = 0;
  uint32_t pressedAtMs_ = 0;
  uint32_t lastTapMs_ = 0;
};

MinerCluster::SlaveEngine slave;
ButtonTracker buttonTracker;
LedProgram ledProgram;
Rgb lastLedColor = LED_OFF;
uint32_t lastLedWriteAtMs = 0;
uint32_t lastStatsAtMs = 0;
uint32_t lastMiningSeenAtMs = 0;
bool ledEnabled = true;

Rgb stateColor(const MinerCluster::SlaveStats& stats, uint32_t nowMs);

bool buttonDown() {
  return digitalRead(BUTTON_PIN) == LOW;
}

void writeLed(const Rgb& color, uint32_t nowMs, bool force = false) {
  const Rgb& output = ledEnabled ? color : LED_OFF;
  if (!force && sameColor(output, lastLedColor)) return;
  if (!force && nowMs - lastLedWriteAtMs < MIN_LED_WRITE_MS) return;
  neopixelWrite(LED_PIN, output.r, output.g, output.b);
  lastLedColor = output;
  lastLedWriteAtMs = nowMs;
}

void loadLedPreference() {
  Preferences prefs;
  prefs.begin(DEVICE_PREF_NS, true);
  ledEnabled = prefs.getBool(LED_ENABLED_PREF_KEY, true);
  prefs.end();
}

void saveLedPreference() {
  Preferences prefs;
  prefs.begin(DEVICE_PREF_NS, false);
  prefs.putBool(LED_ENABLED_PREF_KEY, ledEnabled);
  prefs.end();
}

void toggleLed(uint32_t nowMs) {
  ledEnabled = !ledEnabled;
  saveLedPreference();
  ledProgram.cancel();
  Serial.print("[s3zero] BOOT tap: status LED ");
  Serial.println(ledEnabled ? "enabled" : "disabled");
  if (ledEnabled) {
    writeLed(stateColor(slave.stats(), nowMs), nowMs, true);
  } else {
    writeLed(LED_OFF, nowMs, true);
  }
}

Rgb pulseColor(const Rgb& color, uint32_t nowMs, uint32_t periodMs) {
  const uint32_t half = max<uint32_t>(1, periodMs / 2);
  const uint32_t phase = nowMs % periodMs;
  const uint32_t ramp = phase < half ? phase : periodMs - phase;
  const uint8_t scale = static_cast<uint8_t>(48 + (ramp * 207UL) / half);
  return scaledColor(color, scale);
}

void startBootLed(uint32_t nowMs) {
  const LedStep steps[] = {{LED_WHITE, 700}, {LED_OFF, 120}};
  ledProgram.start(steps, sizeof(steps) / sizeof(steps[0]), nowMs);
}

void startRestartAcknowledgement(uint32_t nowMs) {
  const LedStep steps[] = {{LED_PURPLE, 600}, {LED_OFF, 140}};
  ledProgram.start(steps, sizeof(steps) / sizeof(steps[0]), nowMs);
}

void startPairingClearedAcknowledgement(uint32_t nowMs) {
  const LedStep steps[] = {
      {LED_CYAN, 130}, {LED_OFF, 130},
      {LED_CYAN, 130}, {LED_OFF, 130},
      {LED_CYAN, 130}, {LED_OFF, 180},
  };
  ledProgram.start(steps, sizeof(steps) / sizeof(steps[0]), nowMs);
}

Rgb stateColor(const MinerCluster::SlaveStats& stats, uint32_t nowMs) {
  if (stats.state == MinerCluster::SlaveState::Error || MinerCluster::s3HardwareDisabled()) {
    return ((nowMs / 120) & 1U) == 0 ? LED_RED : LED_OFF;
  }
  if (stats.state == MinerCluster::SlaveState::Mining) lastMiningSeenAtMs = nowMs;
  if (lastMiningSeenAtMs != 0 && nowMs - lastMiningSeenAtMs < MINING_GRACE_MS) return LED_GREEN;
  if (!stats.paired) return pulseColor(LED_RED, nowMs, 1600);
  if (stats.state == MinerCluster::SlaveState::Searching || stats.state == MinerCluster::SlaveState::Pairing) {
    return pulseColor(LED_AMBER, nowMs, 1800);
  }
  return LED_BLUE;
}

void updateLed(uint32_t nowMs) {
  const MinerCluster::SlaveStats stats = slave.stats();
  if (stats.state == MinerCluster::SlaveState::Error || MinerCluster::s3HardwareDisabled()) {
    ledProgram.cancel();
    writeLed(stateColor(stats, nowMs), nowMs);
    return;
  }
  if (ledProgram.active()) {
    writeLed(ledProgram.update(nowMs), nowMs);
    return;
  }
  writeLed(stateColor(stats, nowMs), nowMs);
}

void printMac() {
  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  char text[18];
  snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("[s3zero] mac=");
  Serial.println(text);
}

void restartSlave(uint32_t nowMs) {
  Serial.println("[s3zero] BOOT hold: restarting slave engine");
  slave.stop();
  slave.begin();
  lastMiningSeenAtMs = 0;
  startRestartAcknowledgement(nowMs);
}

void handleButton(ButtonEvent event, uint32_t nowMs) {
  if (event == ButtonEvent::OneTap) {
    toggleLed(nowMs);
  } else if (event == ButtonEvent::ThreeTaps) {
    Serial.println("[s3zero] BOOT triple-tap: clearing master pairing");
    slave.clearPairing();
    lastMiningSeenAtMs = 0;
    startPairingClearedAcknowledgement(nowMs);
  } else if (event == ButtonEvent::Hold5s) {
    restartSlave(nowMs);
  }
}

const char* shaMode() {
  if (MinerCluster::s3HardwareDisabled()) return "software-fallback";
  if (MinerCluster::s3HardwareDigestMode() >= 0) return "hardware";
  return "pending-validation";
}

void printStats(uint32_t nowMs) {
  if (nowMs - lastStatsAtMs < 2000) return;
  lastStatsAtMs = nowMs;
  const MinerCluster::SlaveStats stats = slave.stats();

  Serial.print("[s3zero] build=");
  Serial.print(BuildInfo::BUILD_TEXT);
  Serial.print(" state=");
  Serial.print(MinerCluster::slaveStateLabel(stats.state));
  Serial.print(" paired=");
  Serial.print(stats.paired ? "yes" : "no");
  Serial.print(" ch=");
  Serial.print(stats.channel);
  Serial.print(" raw_khps=");
  Serial.print(stats.hashrate / 1000.0f, 1);
  Serial.print(" effective_khps=");
  Serial.print(stats.effectiveHashrate / 1000.0f, 1);
  Serial.print(" sha=");
  Serial.print(shaMode());
  Serial.print(" caps=0x");
  Serial.print(stats.capabilities, HEX);
  Serial.print(" jobs=");
  Serial.print(stats.jobs);
  Serial.print(" done=");
  Serial.print(stats.completed);
  Serial.print(" assign=");
  Serial.print(stats.currentAssignmentId);
  Serial.print(" progress=");
  Serial.print(stats.assignmentDone);
  Serial.print('/');
  Serial.print(stats.assignmentSize);
  Serial.print(" rx_drop=");
  Serial.print(stats.rxQueueDrops);
  Serial.print(" result_retry=");
  Serial.print(stats.resultRetries);
  Serial.print(" idle_ms=");
  Serial.print(stats.idleMs);
  Serial.print(" heap=");
  Serial.print(stats.freeHeap);
  Serial.print(" stack=");
  Serial.print(stats.taskStackHighWater);
  Serial.print(" stack2=");
  Serial.print(stats.secondaryStackHighWater);
  Serial.print(" total_kh=");
  Serial.print(static_cast<unsigned long>(stats.totalHashes / 1000ULL));
  Serial.print(" temp_c=");
  Serial.print(temperatureRead(), 1);
  Serial.print(" led=");
  Serial.print(ledEnabled ? "on" : "off");
  if (stats.lastError[0]) {
    Serial.print(" error=");
    Serial.print(stats.lastError);
  }
  Serial.println();
}
}  // namespace

void setup() {
  setCpuFrequencyMhz(240);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  delay(300);
  buttonTracker.begin(buttonDown(), millis());
  loadLedPreference();
  writeLed(LED_OFF, millis(), true);
  if (ledEnabled) startBootLed(millis());

  Serial.println();
  Serial.print("[s3zero] MicriOS dedicated Distributed Miner slave ");
  Serial.println(BuildInfo::BUILD_TEXT);
  Serial.print("[s3zero] chip=");
  Serial.print(ESP.getChipModel());
  Serial.print(" cores=");
  Serial.print(ESP.getChipCores());
  Serial.print(" cpu_mhz=");
  Serial.print(getCpuFrequencyMhz());
  Serial.print(" flash_bytes=");
  Serial.print(ESP.getFlashChipSize());
  Serial.print(" psram_bytes=");
  Serial.println(ESP.getPsramSize());
#if MICRI_CLUSTER_S3_SOFTWARE_WORKER
  Serial.println("[s3zero] workers=core1 hardware SHA + core0 software helper");
#else
  Serial.println("[s3zero] workers=core1 hardware SHA only");
#endif
  Serial.println("[s3zero] BOOT: tap toggle LED; triple-tap clear pairing; hold 5s restart engine");
  Serial.print("[s3zero] status LED=");
  Serial.println(ledEnabled ? "enabled" : "disabled");

  slave.begin();
  printMac();
}

void loop() {
  const uint32_t nowMs = millis();
  handleButton(buttonTracker.update(buttonDown(), nowMs), nowMs);
  slave.update();
  updateLed(nowMs);
  printStats(nowMs);
  delay(1);
}
