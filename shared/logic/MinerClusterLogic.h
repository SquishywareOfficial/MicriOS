#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/queue.h>
#include <mbedtls/sha256.h>
#include <stdarg.h>

#include "WiFiLogic.h"
#include "nerdSHA256plus.h"

#if defined(CONFIG_IDF_TARGET_ESP32)
#include <hal/sha_ll.h>
#include <sha/sha_parallel_engine.h>
#include <soc/dport_access.h>
#include <soc/hwcrypto_reg.h>
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#include <sha/sha_parallel_engine.h>
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include <hal/sha_ll.h>
#include <sha/sha_core.h>
#include <soc/hwcrypto_reg.h>
#endif

namespace MinerCluster {

#ifndef MICRI_CLUSTER_SERIAL_DEBUG
#define MICRI_CLUSTER_SERIAL_DEBUG 0
#endif

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define MICRI_CLUSTER_SLAVE_TASK_ENABLED 1
#else
#define MICRI_CLUSTER_SLAVE_TASK_ENABLED 0
#endif

inline void debugPrintf(const char* format, ...) {
#if MICRI_CLUSTER_SERIAL_DEBUG
  char message[192];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  Serial.print(message);
#else
  (void)format;
#endif
}

constexpr const char* MINER_PREF_NS = "miner";
constexpr const char* CLUSTER_PREF_NS = "cluster";
constexpr const char* DEFAULT_POOL_HOST = "public-pool.io";
constexpr uint16_t DEFAULT_POOL_PORT = 21496;
constexpr const char* DEFAULT_POOL_PASS = "x";
constexpr const char* DEFAULT_WALLET = "bc1qfreyhgyjj03pk60jdpym2tmcx780jmsgcvj8gl";
constexpr const char* DEFAULT_HOSTNAME = "MicriDeck-Cluster";
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr uint8_t MAGIC_0 = 'F';
constexpr uint8_t MAGIC_1 = 'M';
constexpr uint8_t PROTOCOL_VERSION = 3;
constexpr uint8_t MAX_SLAVES = 8;
constexpr uint32_t DEFAULT_RANGE = 4096;
constexpr uint32_t DEFAULT_FAST_RANGE = 524288;
constexpr uint32_t MIN_RANGE = 4096;
constexpr uint32_t MAX_RANGE = 2000000;
constexpr uint32_t MAX_FAST_RANGE = 8000000;
constexpr uint32_t ASSIGNMENT_TIMEOUT_MS = 12000;
constexpr uint32_t TARGET_ASSIGNMENT_MS = 3000;
constexpr uint32_t FAST_TARGET_ASSIGNMENT_MS = 12000;
constexpr uint32_t PREFETCH_PERCENT = 75;
constexpr uint32_t DELIVERY_RETRY_MS = 250;
constexpr uint8_t DELIVERY_RETRY_LIMIT = 8;
constexpr uint32_t RESULT_ACK_TIMEOUT_MS = 5000;
constexpr uint32_t RETRY_BACKOFF_INITIAL_MS = 5000;
constexpr uint32_t RETRY_BACKOFF_MAX_MS = 300000;
#ifndef MICRI_CLUSTER_SLAVE_BATCH_NONCES
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr uint16_t SLAVE_BATCH_NONCES = 4096;
#else
constexpr uint16_t SLAVE_BATCH_NONCES = 768;
#endif
#else
constexpr uint16_t SLAVE_BATCH_NONCES = MICRI_CLUSTER_SLAVE_BATCH_NONCES;
#endif

#ifndef MICRI_CLUSTER_SLAVE_TASK_STACK
#define MICRI_CLUSTER_SLAVE_TASK_STACK 6144
#endif

#ifndef MICRI_CLUSTER_SLAVE_TASK_PRIORITY
#define MICRI_CLUSTER_SLAVE_TASK_PRIORITY 2
#endif

#ifndef MICRI_CLUSTER_S3_SOFTWARE_WORKER
#define MICRI_CLUSTER_S3_SOFTWARE_WORKER 1
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3) && MICRI_CLUSTER_SLAVE_TASK_ENABLED
#define MICRI_CLUSTER_DUAL_SLAVE_WORKER 1
#else
#define MICRI_CLUSTER_DUAL_SLAVE_WORKER 0
#endif

enum SlaveCapability : uint8_t {
  SlaveCapPendingAssignment = 1 << 0,
  SlaveCapFastAssignment = 1 << 1,
  SlaveCapDualWorker = 1 << 2,
  SlaveCapHardwareSha = 1 << 3,
};

inline uint8_t localSlaveCapabilities() {
  uint8_t caps = SlaveCapPendingAssignment;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  caps |= SlaveCapFastAssignment | SlaveCapHardwareSha;
#if MICRI_CLUSTER_S3_SOFTWARE_WORKER
  caps |= SlaveCapDualWorker;
#endif
#endif
  return caps;
}

inline uint32_t localPreferredAssignmentMs() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  return FAST_TARGET_ASSIGNMENT_MS;
#else
  return TARGET_ASSIGNMENT_MS;
#endif
}

inline uint32_t localMaxAssignmentRange() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  return MAX_FAST_RANGE;
#else
  return MAX_RANGE;
#endif
}

enum class PacketType : uint8_t {
  PairBeacon = 1,
  MasterBeacon = 2,
  SlaveHello = 3,
  WorkAssign = 4,
  WorkResult = 5,
  SlaveStatus = 6,
  CancelJob = 7,
  DeliveryAck = 8
};

enum class MasterState : uint8_t {
  Idle,
  NoWifi,
  ConnectingWifi,
  WifiFailed,
  RadioFailed,
  ConnectingPool,
  PoolFailed,
  Subscribing,
  Mining,
  Stopping,
  Error
};

enum class SlaveState : uint8_t {
  Searching,
  Pairing,
  Paired,
  Mining,
  Error
};

struct MinerConfig {
  String wallet = DEFAULT_WALLET;
  String poolHost = DEFAULT_POOL_HOST;
  String poolPass = DEFAULT_POOL_PASS;
  uint16_t poolPort = DEFAULT_POOL_PORT;
};

struct ClusterConfig {
  bool localMining = true;
  uint32_t clusterId = 0;
};

struct PacketHeader {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t version;
  uint8_t type;
  uint32_t clusterId;
  uint32_t sessionId;
  uint32_t sequence;
  uint8_t sender[6];
} __attribute__((packed));

struct PairBeaconPacket {
  PacketHeader header;
  uint32_t pairingEndsInMs;
  uint8_t channel;
} __attribute__((packed));

struct MasterBeaconPacket {
  PacketHeader header;
  uint8_t channel;
  uint8_t state;
  uint8_t slaveCount;
  uint8_t localMining;
  uint32_t totalHps;
  uint32_t jobSeq;
} __attribute__((packed));

struct SlaveHelloPacket {
  PacketHeader header;
  uint8_t role;
  uint8_t channel;
  uint8_t capabilities;
  uint8_t reserved;
  uint16_t build;
  uint32_t preferredAssignmentMs;
  uint32_t maxAssignmentRange;
  uint32_t uptimeSeconds;
} __attribute__((packed));

struct WorkAssignPacket {
  PacketHeader header;
  uint32_t jobSeq;
  uint32_t assignmentId;
  uint32_t nonceStart;
  uint32_t nonceCount;
  float poolDifficulty;
  uint8_t blockHeader[80];
} __attribute__((packed));

struct WorkResultPacket {
  PacketHeader header;
  uint32_t jobSeq;
  uint32_t assignmentId;
  uint32_t hashesDone;
  uint32_t elapsedMs;
  uint32_t wallElapsedMs;
  float hashrate;
  uint32_t effectiveHashrate;
  float bestDifficulty;
  uint8_t found;
  uint8_t capabilities;
  uint16_t reserved;
  uint32_t foundNonce;
  float foundDifficulty;
} __attribute__((packed));

struct SlaveStatusPacket {
  PacketHeader header;
  uint8_t state;
  uint8_t channel;
  uint8_t paired;
  uint8_t reserved;
  uint32_t hashrate;
  uint32_t effectiveHashrate;
  uint32_t uptimeSeconds;
  uint32_t lastSeenJobSeq;
  uint8_t capabilities;
  uint8_t reserved2[3];
  char lastError[32];
} __attribute__((packed));

struct CancelJobPacket {
  PacketHeader header;
  uint32_t jobSeq;
} __attribute__((packed));

enum class AckDisposition : uint8_t {
  Active = 1,
  Queued = 2,
  ResultReceived = 3,
};

struct DeliveryAckPacket {
  PacketHeader header;
  uint8_t ackedType;
  uint8_t disposition;
  uint16_t reserved;
  uint32_t jobSeq;
  uint32_t assignmentId;
} __attribute__((packed));

constexpr uint16_t MAX_CLUSTER_PACKET_BYTES = 192;
constexpr uint8_t RX_QUEUE_DEPTH = 16;

struct RxEvent {
  uint8_t from[6];
  uint16_t length;
  uint8_t data[MAX_CLUSTER_PACKET_BYTES];
};

static_assert(sizeof(WorkAssignPacket) <= MAX_CLUSTER_PACKET_BYTES, "Work assignment exceeds RX queue packet capacity");
static_assert(sizeof(WorkResultPacket) <= MAX_CLUSTER_PACKET_BYTES, "Work result exceeds RX queue packet capacity");

struct Job {
  String jobId;
  String prevHash;
  String coinb1;
  String coinb2;
  String version;
  String nbits;
  String ntime;
  String branches[16];
  uint8_t branchCount = 0;
  bool cleanJobs = false;
};

struct Subscribe {
  String extranonce1;
  String extranonce2;
  uint8_t extranonce2Size = 4;
};

struct WorkContext {
  Job job;
  Subscribe sub;
  uint8_t header[80] = {};
  uint8_t paddedHeader[128] = {};
  uint32_t midstate[8] = {};
  uint32_t bake[16] = {};
#if defined(CONFIG_IDF_TARGET_ESP32)
  uint32_t hwWords[32] = {};
#endif
  uint32_t jobSeq = 0;
  uint32_t nextNonce = 0xA5000000UL;
  bool ready = false;
};

struct SlaveRecord {
  bool used = false;
  uint8_t mac[6] = {};
  uint32_t lastSeenMs = 0;
  uint32_t hashrate = 0;
  uint32_t lastJobSeq = 0;
  uint32_t assignmentId = 0;
  uint32_t nonceStart = 0;
  uint32_t nonceCount = 0;
  uint32_t assignedAtMs = 0;
  bool assigned = false;
  uint32_t assignedJobSeq = 0;
  bool assignmentAcknowledged = false;
  uint8_t assignmentSendAttempts = 0;
  uint32_t assignmentLastSentAtMs = 0;
  bool queued = false;
  uint32_t queuedJobSeq = 0;
  uint32_t queuedAssignmentId = 0;
  uint32_t queuedNonceStart = 0;
  uint32_t queuedNonceCount = 0;
  uint32_t queuedAtMs = 0;
  bool queuedAcknowledged = false;
  uint8_t queuedSendAttempts = 0;
  uint32_t queuedLastSentAtMs = 0;
  uint32_t lastCompletedJobSeq = 0;
  uint32_t lastCompletedAssignmentId = 0;
  uint8_t capabilities = 0;
  uint32_t effectiveHashrate = 0;
  uint32_t preferredAssignmentMs = 0;
  uint32_t maxAssignmentRange = 0;
  float bestDifficulty = 0.0f;
  char status[32] = "";
};

struct MasterStats {
  MasterState state = MasterState::Idle;
  bool localMining = true;
  bool pairing = false;
  uint8_t channel = 0;
  uint8_t slaveCount = 0;
  uint32_t localHps = 0;
  uint32_t slaveHps = 0;
  uint32_t totalHps = 0;
  uint64_t localHashes = 0;
  uint64_t slaveHashes = 0;
  uint32_t jobs = 0;
  uint32_t submitted = 0;
  uint32_t accepted = 0;
  uint32_t rejected = 0;
  float poolDifficulty = 0.00015f;
  float bestDifficulty = 0.0f;
  uint32_t uptimeSeconds = 0;
  uint32_t retryCount = 0;
  uint32_t retryInSeconds = 0;
  char ssid[33] = "";
  char lastError[72] = "";
};

struct SlaveStats {
  SlaveState state = SlaveState::Searching;
  bool paired = false;
  uint8_t channel = 1;
  uint32_t hashrate = 0;
  uint32_t effectiveHashrate = 0;
  uint64_t totalHashes = 0;
  uint32_t jobs = 0;
  uint32_t completed = 0;
  uint32_t currentJobSeq = 0;
  uint32_t currentAssignmentId = 0;
  uint32_t assignmentSize = 0;
  uint32_t assignmentDone = 0;
  uint32_t lastResultHashes = 0;
  uint32_t lastResultMs = 0;
  float bestDifficulty = 0.0f;
  uint32_t uptimeSeconds = 0;
  uint8_t capabilities = 0;
  char lastError[48] = "";
  uint8_t masterMac[6] = {};
};

inline const char* masterStateLabel(MasterState state) {
  switch (state) {
    case MasterState::Idle: return "Idle";
    case MasterState::NoWifi: return "No WiFi";
    case MasterState::ConnectingWifi: return "WiFi...";
    case MasterState::WifiFailed: return "WiFi fail";
    case MasterState::RadioFailed: return "Radio fail";
    case MasterState::ConnectingPool: return "Pool...";
    case MasterState::PoolFailed: return "Pool fail";
    case MasterState::Subscribing: return "Sub...";
    case MasterState::Mining: return "Mining";
    case MasterState::Stopping: return "Stopping";
    case MasterState::Error: return "Error";
  }
  return "?";
}

inline const char* slaveStateLabel(SlaveState state) {
  switch (state) {
    case SlaveState::Searching: return "Searching";
    case SlaveState::Pairing: return "Pairing";
    case SlaveState::Paired: return "Paired";
    case SlaveState::Mining: return "Mining";
    case SlaveState::Error: return "Error";
  }
  return "?";
}

inline String sanitizedMac() {
  String mac = WiFi.macAddress();
  String clean;
  clean.reserve(12);
  for (uint8_t i = 0; i < mac.length(); i++) {
    const char c = mac[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
      clean += static_cast<char>(toupper(c));
    }
  }
  if (clean.length() == 0) clean = "000000000000";
  return clean;
}

inline String workerName() {
  return String("Micri") + sanitizedMac();
}

inline String stratumUser(const MinerConfig& config) {
  return config.wallet + "." + workerName();
}

inline void readMac(uint8_t out[6]) {
  WiFi.macAddress(out);
}

inline bool macEquals(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

inline bool macIsEmpty(const uint8_t* mac) {
  static const uint8_t empty[6] = {};
  return macEquals(mac, empty);
}

inline String macSuffix(const uint8_t* mac) {
  char out[7];
  snprintf(out, sizeof(out), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(out);
}

inline MinerConfig loadMinerConfig() {
  MinerConfig config;
  Preferences prefs;
  prefs.begin(MINER_PREF_NS, true);
  config.wallet = prefs.getString("wallet", DEFAULT_WALLET);
  config.poolHost = prefs.getString("host", DEFAULT_POOL_HOST);
  config.poolPass = prefs.getString("pass", DEFAULT_POOL_PASS);
  config.poolPort = prefs.getUShort("port", DEFAULT_POOL_PORT);
  prefs.end();
  config.wallet.trim();
  config.poolHost.trim();
  config.poolPass.trim();
  if (config.wallet.length() == 0) config.wallet = DEFAULT_WALLET;
  if (config.poolHost.length() == 0) config.poolHost = DEFAULT_POOL_HOST;
  if (config.poolPass.length() == 0) config.poolPass = DEFAULT_POOL_PASS;
  if (config.poolPort == 0) config.poolPort = DEFAULT_POOL_PORT;
  return config;
}

class ClusterSettings {
public:
  void load() {
    Preferences prefs;
    prefs.begin(CLUSTER_PREF_NS, false);
    config_.localMining = prefs.getBool("local", true);
    config_.clusterId = prefs.getUInt("id", 0);
    if (config_.clusterId == 0) {
      config_.clusterId = esp_random();
      if (config_.clusterId == 0) config_.clusterId = 0xC1057E21UL;
      prefs.putUInt("id", config_.clusterId);
    }
    prefs.end();
  }

  void save(const ClusterConfig& config) {
    config_ = config;
    Preferences prefs;
    prefs.begin(CLUSTER_PREF_NS, false);
    prefs.putBool("local", config_.localMining);
    prefs.putUInt("id", config_.clusterId);
    prefs.end();
  }

  void reset() {
    Preferences prefs;
    prefs.begin(CLUSTER_PREF_NS, false);
    prefs.clear();
    prefs.end();
    config_ = ClusterConfig();
  }

  const ClusterConfig& config() const { return config_; }

private:
  ClusterConfig config_;
};

inline uint8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

inline size_t hexToBytes(const char* hex, uint8_t* out, size_t maxOut) {
  const size_t len = strlen(hex);
  if (len % 2 != 0) return 0;
  const size_t bytes = len / 2;
  if (bytes > maxOut) return 0;
  for (size_t i = 0; i < bytes; i++) {
    out[i] = static_cast<uint8_t>((hexNibble(hex[i * 2]) << 4) | hexNibble(hex[i * 2 + 1]));
  }
  return bytes;
}

inline void bytesToHex(const uint8_t* data, size_t len, char* out, size_t outSize) {
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  if (outSize < len * 2 + 1) return;
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = HEX_DIGITS[data[i] >> 4];
    out[i * 2 + 1] = HEX_DIGITS[data[i] & 0x0F];
  }
  out[len * 2] = '\0';
}

inline void reverseRange(uint8_t* data, uint8_t offset, uint8_t len) {
  for (uint8_t i = 0; i < len / 2; i++) {
    const uint8_t tmp = data[offset + i];
    data[offset + i] = data[offset + len - 1 - i];
    data[offset + len - 1 - i] = tmp;
  }
}

inline void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
}

inline void doubleSha(const uint8_t* data, size_t len, uint8_t out[32]) {
  uint8_t first[32];
  sha256(data, len, first);
  sha256(first, sizeof(first), out);
}

inline uint64_t readLe64(const uint8_t* data) {
  uint64_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    value |= static_cast<uint64_t>(data[i]) << (i * 8);
  }
  return value;
}

inline double le256ToDouble(const uint8_t* data) {
  const uint64_t part0 = readLe64(data);
  const uint64_t part1 = readLe64(data + 8);
  const uint64_t part2 = readLe64(data + 16);
  const uint64_t part3 = readLe64(data + 24);
  double value = part3 * 6277101735386680763835789423207666416102355444464034512896.0;
  value += part2 * 340282366920938463463374607431768211456.0;
  value += part1 * 18446744073709551616.0;
  value += part0;
  return value;
}

inline float difficultyFromHash(const uint8_t* hash) {
  static constexpr double TRUE_DIFF_ONE = 26959535291011309493156476344723991336010898738574164086137773096960.0;
  double value = le256ToDouble(hash);
  if (value <= 0.0) value = 1.0;
  return static_cast<float>(TRUE_DIFF_ONE / value);
}

inline void preparePaddedWork(WorkContext& work) {
  memcpy(work.paddedHeader, work.header, sizeof(work.header));
  memset(work.paddedHeader + sizeof(work.header), 0, sizeof(work.paddedHeader) - sizeof(work.header));
  work.paddedHeader[80] = 0x80;
  work.paddedHeader[126] = 0x02;
  work.paddedHeader[127] = 0x80;
  nerd_mids(work.midstate, work.paddedHeader);
  nerd_sha256_bake(work.midstate, work.paddedHeader + 64, work.bake);
#if defined(CONFIG_IDF_TARGET_ESP32)
  for (uint8_t i = 0; i < 32; i++) {
    uint32_t word = 0;
    memcpy(&word, work.paddedHeader + i * 4, sizeof(word));
    work.hwWords[i] = __builtin_bswap32(word);
  }
#endif
}

inline bool hashNonceSoftware(const WorkContext& source, uint32_t nonce, uint8_t hash[32]) {
  WorkContext work = source;
  memcpy(work.paddedHeader + 76, &nonce, sizeof(nonce));
  return nerd_sha256d_baked(work.midstate, work.paddedHeader + 64, work.bake, hash);
}

inline bool hashNonceSoftwarePrepared(WorkContext& work, uint32_t nonce, uint8_t hash[32]) {
  memcpy(work.paddedHeader + 76, &nonce, sizeof(nonce));
  return nerd_sha256d_baked(work.midstate, work.paddedHeader + 64, work.bake, hash);
}

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// ESP32-S3 SHA register sequence adapted from NerdMiner_v2 revision
// a26865f7cdd9ac1a81c5b0a7c355e23cf4a1d568 (MIT). See the retained notice
// in licenses/NerdMiner_v2-MIT.txt.
inline void shaWaitIdleS3() {
  while (REG_READ(SHA_BUSY_REG)) {}
}

inline void shaWriteDigestS3(const uint32_t digest[8]) {
  for (uint8_t i = 0; i < 8; i++) {
    REG_WRITE(SHA_H_BASE + i * sizeof(uint32_t), digest[i]);
  }
}

inline void shaFillUpperS3(const uint8_t* upperHeader, uint32_t nonce) {
  const uint32_t* words = reinterpret_cast<const uint32_t*>(upperHeader);
  REG_WRITE(SHA_TEXT_BASE + 0 * 4, words[0]);
  REG_WRITE(SHA_TEXT_BASE + 1 * 4, words[1]);
  REG_WRITE(SHA_TEXT_BASE + 2 * 4, words[2]);
  REG_WRITE(SHA_TEXT_BASE + 3 * 4, nonce);
  REG_WRITE(SHA_TEXT_BASE + 4 * 4, 0x00000080);
  for (uint8_t i = 5; i < 15; i++) REG_WRITE(SHA_TEXT_BASE + i * 4, 0);
  REG_WRITE(SHA_TEXT_BASE + 15 * 4, 0x80020000);
}

inline void shaFillDoubleS3() {
  for (uint8_t i = 0; i < 8; i++) {
    REG_WRITE(SHA_TEXT_BASE + i * 4, REG_READ(SHA_H_BASE + i * 4));
  }
  REG_WRITE(SHA_TEXT_BASE + 8 * 4, 0x00000080);
  for (uint8_t i = 9; i < 15; i++) REG_WRITE(SHA_TEXT_BASE + i * 4, 0);
  REG_WRITE(SHA_TEXT_BASE + 15 * 4, 0x00010000);
}

inline bool shaReadDigestS3(uint8_t raw[32], bool candidateOnly) {
  const uint32_t last = REG_READ(SHA_H_BASE + 7 * 4);
  if (candidateOnly && static_cast<uint16_t>(last >> 16) != 0) return false;
  for (uint8_t i = 0; i < 7; i++) {
    const uint32_t word = REG_READ(SHA_H_BASE + i * 4);
    memcpy(raw + i * 4, &word, sizeof(word));
  }
  memcpy(raw + 7 * 4, &last, sizeof(last));
  return true;
}

inline void transformS3Digest(const uint8_t raw[32], uint8_t out[32], uint8_t mode) {
  if (mode == 0) {
    memcpy(out, raw, 32);
  } else if (mode == 1) {
    for (uint8_t i = 0; i < 32; i++) out[i] = raw[31 - i];
  } else if (mode == 2) {
    for (uint8_t i = 0; i < 32; i += 4) {
      out[i + 0] = raw[i + 3];
      out[i + 1] = raw[i + 2];
      out[i + 2] = raw[i + 1];
      out[i + 3] = raw[i + 0];
    }
  } else {
    for (uint8_t i = 0; i < 8; i++) {
      memcpy(out + i * 4, raw + (7 - i) * 4, 4);
    }
  }
}

inline bool hashNonceHardwareS3RawUnlocked(const WorkContext& source, uint32_t nonce,
                                            uint8_t raw[32], bool candidateOnly) {
  REG_WRITE(SHA_MODE_REG, SHA2_256);
  shaWriteDigestS3(source.midstate);
  shaFillUpperS3(source.paddedHeader + 64, nonce);
  REG_WRITE(SHA_CONTINUE_REG, 1);
  sha_ll_load(SHA2_256);
  shaWaitIdleS3();
  shaFillDoubleS3();
  REG_WRITE(SHA_START_REG, 1);
  sha_ll_load(SHA2_256);
  shaWaitIdleS3();
  return shaReadDigestS3(raw, candidateOnly);
}

inline int8_t& s3HardwareDigestMode() {
  static int8_t mode = -1;
  return mode;
}

inline bool& s3HardwareDisabled() {
  static bool disabled = false;
  return disabled;
}

inline bool validateHardwareS3Unlocked(const WorkContext& source, uint32_t nonce) {
  if (s3HardwareDisabled()) return false;
  if (s3HardwareDigestMode() >= 0) return true;

  uint8_t raw[32];
  uint8_t software[32];
  if (!hashNonceHardwareS3RawUnlocked(source, nonce, raw, false) ||
      !hashNonceSoftware(source, nonce, software)) {
    s3HardwareDisabled() = true;
    return false;
  }

  for (uint8_t mode = 0; mode < 4; mode++) {
    uint8_t candidate[32];
    transformS3Digest(raw, candidate, mode);
    if (memcmp(candidate, software, sizeof(candidate)) == 0) {
      s3HardwareDigestMode() = static_cast<int8_t>(mode);
      debugPrintf("[ClusterSlave] S3 hardware SHA validated mode=%u\n", mode);
      return true;
    }
  }

  s3HardwareDisabled() = true;
  debugPrintf("[ClusterSlave] S3 hardware SHA validation failed; using software\n");
  return false;
}

inline bool hashNonceHardwareS3Unlocked(const WorkContext& source, uint32_t nonce,
                                         uint8_t hash[32]) {
  if (s3HardwareDisabled() || s3HardwareDigestMode() < 0) return false;
  uint8_t raw[32];
  const bool directOrder = s3HardwareDigestMode() == 0;
  if (!hashNonceHardwareS3RawUnlocked(source, nonce, raw, directOrder)) return false;
  transformS3Digest(raw, hash, static_cast<uint8_t>(s3HardwareDigestMode()));
  if (!directOrder) {
    uint32_t last;
    memcpy(&last, hash + 28, sizeof(last));
    if (static_cast<uint16_t>(last >> 16) != 0) return false;
  }
  return true;
}
#endif

#if defined(CONFIG_IDF_TARGET_ESP32)
inline void shaWaitIdleEsp32() {
  while (DPORT_REG_READ(SHA_256_BUSY_REG)) {}
}

inline void shaFillBlockEsp32(const uint32_t* words) {
  uint32_t* reg = reinterpret_cast<uint32_t*>(SHA_TEXT_BASE);
  reg[0] = words[0];
  reg[1] = words[1];
  reg[2] = words[2];
  reg[3] = words[3];
  reg[4] = words[4];
  reg[5] = words[5];
  reg[6] = words[6];
  reg[7] = words[7];
  reg[8] = words[8];
  reg[9] = words[9];
  reg[10] = words[10];
  reg[11] = words[11];
  reg[12] = words[12];
  reg[13] = words[13];
  reg[14] = words[14];
  reg[15] = words[15];
}

inline void shaFillUpperEsp32(const uint32_t* words, uint32_t nonce) {
  uint32_t* reg = reinterpret_cast<uint32_t*>(SHA_TEXT_BASE);
  reg[0] = words[0];
  reg[1] = words[1];
  reg[2] = words[2];
  reg[3] = __builtin_bswap32(nonce);
  reg[4] = 0x80000000;
  reg[5] = 0x00000000;
  reg[6] = 0x00000000;
  reg[7] = 0x00000000;
  reg[8] = 0x00000000;
  reg[9] = 0x00000000;
  reg[10] = 0x00000000;
  reg[11] = 0x00000000;
  reg[12] = 0x00000000;
  reg[13] = 0x00000000;
  reg[14] = 0x00000000;
  reg[15] = 0x00000280;
}

inline void shaFillDoubleEsp32() {
  uint32_t* reg = reinterpret_cast<uint32_t*>(SHA_TEXT_BASE);
  reg[8] = 0x80000000;
  reg[9] = 0x00000000;
  reg[10] = 0x00000000;
  reg[11] = 0x00000000;
  reg[12] = 0x00000000;
  reg[13] = 0x00000000;
  reg[14] = 0x00000000;
  reg[15] = 0x00000100;
}

inline void storeWord(uint8_t* out, uint8_t index, uint32_t value) {
  memcpy(out + index * 4, &value, sizeof(value));
}

inline bool shaReadDigestCandidateEsp32(uint8_t* hash) {
  DPORT_INTERRUPT_DISABLE();
  const uint32_t fin = DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 7 * 4);
  if ((fin & 0xFFFF) != 0) {
    DPORT_INTERRUPT_RESTORE();
    return false;
  }
  storeWord(hash, 7, __builtin_bswap32(fin));
  storeWord(hash, 0, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 0 * 4)));
  storeWord(hash, 1, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 1 * 4)));
  storeWord(hash, 2, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 2 * 4)));
  storeWord(hash, 3, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 3 * 4)));
  storeWord(hash, 4, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 4 * 4)));
  storeWord(hash, 5, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 5 * 4)));
  storeWord(hash, 6, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 6 * 4)));
  DPORT_INTERRUPT_RESTORE();
  return true;
}

inline bool hashNonceHardwareEsp32Unlocked(const WorkContext& source, uint32_t nonce, uint8_t hash[32]) {
  shaFillBlockEsp32(source.hwWords);
  sha_ll_start_block(SHA2_256);
  shaWaitIdleEsp32();
  shaFillUpperEsp32(source.hwWords + 16, nonce);
  sha_ll_continue_block(SHA2_256);
  shaWaitIdleEsp32();
  sha_ll_load(SHA2_256);
  shaWaitIdleEsp32();
  shaFillDoubleEsp32();
  sha_ll_start_block(SHA2_256);
  shaWaitIdleEsp32();
  sha_ll_load(SHA2_256);
  return shaReadDigestCandidateEsp32(hash);
}

inline bool hashNonceHardwareEsp32(const WorkContext& source, uint32_t nonce, uint8_t hash[32]) {
  esp_sha_lock_engine(SHA2_256);
  const bool ok = hashNonceHardwareEsp32Unlocked(source, nonce, hash);
  esp_sha_unlock_engine(SHA2_256);
  return ok;
}
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3)
inline void transformC3Digest(const uint8_t raw[32], uint8_t out[32], uint8_t mode) {
  if (mode == 0) {
    memcpy(out, raw, 32);
  } else if (mode == 1) {
    for (uint8_t i = 0; i < 32; i++) out[i] = raw[31 - i];
  } else if (mode == 2) {
    for (uint8_t i = 0; i < 32; i += 4) {
      out[i + 0] = raw[i + 3];
      out[i + 1] = raw[i + 2];
      out[i + 2] = raw[i + 1];
      out[i + 3] = raw[i + 0];
    }
  } else {
    for (uint8_t i = 0; i < 8; i++) {
      const uint8_t* src = raw + (7 - i) * 4;
      out[i * 4 + 0] = src[0];
      out[i * 4 + 1] = src[1];
      out[i * 4 + 2] = src[2];
      out[i * 4 + 3] = src[3];
    }
  }
}

inline bool hashNonceHardwareC3Raw(const WorkContext& source, uint32_t nonce, uint8_t raw[32]) {
  uint8_t header[80];
  uint8_t first[32];
  memcpy(header, source.header, sizeof(header));
  memcpy(header + 76, &nonce, sizeof(nonce));
  esp_sha(SHA2_256, header, sizeof(header), first);
  esp_sha(SHA2_256, first, sizeof(first), raw);
  return true;
}

inline bool hashNonceHardwareC3(const WorkContext& source, uint32_t nonce, uint8_t hash[32]) {
  static int8_t digestMode = -1;
  static bool disabled = false;
  if (disabled) return false;

  uint8_t raw[32];
  if (!hashNonceHardwareC3Raw(source, nonce, raw)) return false;

  if (digestMode >= 0) {
    transformC3Digest(raw, hash, static_cast<uint8_t>(digestMode));
    return true;
  }

  uint8_t software[32];
  if (!hashNonceSoftware(source, nonce, software)) {
    disabled = true;
    return false;
  }

  for (uint8_t mode = 0; mode < 4; mode++) {
    uint8_t candidate[32];
    transformC3Digest(raw, candidate, mode);
    if (memcmp(candidate, software, sizeof(candidate)) == 0) {
      digestMode = static_cast<int8_t>(mode);
      memcpy(hash, candidate, sizeof(candidate));
      return true;
    }
  }

  disabled = true;
  return false;
}
#endif

inline bool hashNonce(const WorkContext& source, uint32_t nonce, uint8_t hash[32]) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  if (hashNonceHardwareEsp32(source, nonce, hash)) {
    return true;
  }
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  if (hashNonceHardwareC3(source, nonce, hash)) {
    return true;
  }
#endif
  return hashNonceSoftware(source, nonce, hash);
}

inline bool checkPacketHeader(const PacketHeader& header, PacketType expected, uint32_t clusterId, bool allowAnyCluster = false) {
  if (header.magic0 != MAGIC_0 || header.magic1 != MAGIC_1 || header.version != PROTOCOL_VERSION) return false;
  if (header.type != static_cast<uint8_t>(expected)) return false;
  return allowAnyCluster || header.clusterId == clusterId;
}

class MasterEngine;
class SlaveEngine;

inline MasterEngine*& activeMaster() {
  static MasterEngine* instance = nullptr;
  return instance;
}

inline SlaveEngine*& activeSlave() {
  static SlaveEngine* instance = nullptr;
  return instance;
}

class MasterEngine {
public:
  ~MasterEngine() { stop(); }

  bool start(const char* hostname = DEFAULT_HOSTNAME) {
    if (task_ != nullptr) return true;
    settings_.load();
    cluster_ = settings_.config();
    minerConfig_ = loadMinerConfig();
    hostname_ = hostname;
    sessionId_ = esp_random();
    if (sessionId_ == 0) sessionId_ = millis() ^ 0x4D435249UL;
    stopRequested_ = false;
    resetStats();
    setLocalMining(cluster_.localMining);
    BaseType_t ok = xTaskCreate(taskEntry, "MineCluster", 12288, this, 1, &task_);
    if (ok != pdPASS) {
      task_ = nullptr;
      setError("Task create failed");
      return false;
    }
    return true;
  }

  void stop() {
    if (task_ == nullptr) {
      shutdownRadio();
      destroyRxQueue();
      setState(MasterState::Idle);
      return;
    }
    stopRequested_ = true;
    setState(MasterState::Stopping);
    client_.stop();
    const uint32_t started = millis();
    while (task_ != nullptr && millis() - started < 2000) {
      delay(20);
    }
    if (task_ != nullptr) {
      vTaskDelete(task_);
      task_ = nullptr;
    }
    shutdownRadio();
    destroyRxQueue();
    client_.stop();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    setState(MasterState::Idle);
  }

  bool running() const { return task_ != nullptr; }

  void setLocalMining(bool enabled) {
    cluster_.localMining = enabled;
    settings_.save(cluster_);
    portENTER_CRITICAL(&mux_);
    stats_.localMining = enabled;
    portEXIT_CRITICAL(&mux_);
  }

  bool localMiningEnabled() const { return cluster_.localMining; }

  void startPairing(uint32_t durationMs = 60000) {
    pairingUntilMs_ = millis() + durationMs;
    sendPairBeacon();
    portENTER_CRITICAL(&mux_);
    stats_.pairing = true;
    portEXIT_CRITICAL(&mux_);
  }

  void stopPairing() {
    pairingUntilMs_ = 0;
    portENTER_CRITICAL(&mux_);
    stats_.pairing = false;
    portEXIT_CRITICAL(&mux_);
  }

  void forgetSlaves() {
    memset(slaves_, 0, sizeof(slaves_));
    portENTER_CRITICAL(&mux_);
    stats_.slaveCount = 0;
    stats_.slaveHps = 0;
    stats_.totalHps = stats_.localHps;
    stats_.slaveHashes = 0;
    portEXIT_CRITICAL(&mux_);
  }

  void resetClusterIdentity(bool preserveLocalMining = true) {
    const bool localMining = cluster_.localMining;
    const bool wasRunning = running();
    stop();
    settings_.reset();
    settings_.load();
    cluster_ = settings_.config();
    if (preserveLocalMining) {
      cluster_.localMining = localMining;
      settings_.save(cluster_);
    }
    forgetSlaves();
    currentWork_ = WorkContext();
    currentJobSeq_ = 0;
    nextAssignmentId_ = 1;
    nextSequence_ = 1;
    pairingUntilMs_ = 0;
    lastPairBeaconAt_ = 0;
    resetStats();
    setLocalMining(cluster_.localMining);
    if (wasRunning) start(hostname_);
  }

  MasterStats stats() {
    MasterStats out;
    portENTER_CRITICAL(&mux_);
    out = stats_;
    portEXIT_CRITICAL(&mux_);
    return out;
  }

  uint8_t slaveCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_SLAVES; i++) {
      if (slaves_[i].used && millis() - slaves_[i].lastSeenMs < 10000) count++;
    }
    return count;
  }

  bool slaveAt(uint8_t index, SlaveRecord& out) const {
    uint8_t seen = 0;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < MAX_SLAVES; i++) {
      if (!slaves_[i].used) continue;
      if (now - slaves_[i].lastSeenMs >= 10000) continue;
      if (seen == index) {
        out = slaves_[i];
        return true;
      }
      seen++;
    }
    return false;
  }

  void handleIncoming(const uint8_t* from, const uint8_t* data, int len) {
    if (from == nullptr || data == nullptr || len < static_cast<int>(sizeof(PacketHeader))) return;
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
    if (header->magic0 != MAGIC_0 || header->magic1 != MAGIC_1 || header->version != PROTOCOL_VERSION) return;
    if (header->clusterId != cluster_.clusterId || header->sessionId != sessionId_) return;

    switch (static_cast<PacketType>(header->type)) {
      case PacketType::SlaveHello:
        if (len == static_cast<int>(sizeof(SlaveHelloPacket))) {
          const auto* packet = reinterpret_cast<const SlaveHelloPacket*>(data);
          addOrUpdateSlave(from, "hello", 0, 0, 0.0f, 0, packet->capabilities,
                           packet->preferredAssignmentMs, packet->maxAssignmentRange);
        }
        break;
      case PacketType::SlaveStatus:
        if (len == static_cast<int>(sizeof(SlaveStatusPacket))) {
          const auto* packet = reinterpret_cast<const SlaveStatusPacket*>(data);
          addOrUpdateSlave(from, slaveStateLabel(static_cast<SlaveState>(packet->state)),
                           packet->hashrate, packet->effectiveHashrate, 0.0f, packet->lastSeenJobSeq,
                           packet->capabilities, 0, 0);
        }
        break;
      case PacketType::WorkResult:
        if (len == static_cast<int>(sizeof(WorkResultPacket))) {
          const auto* packet = reinterpret_cast<const WorkResultPacket*>(data);
          handleWorkResult(from, *packet);
        }
        break;
      case PacketType::DeliveryAck:
        if (len == static_cast<int>(sizeof(DeliveryAckPacket))) {
          const auto* packet = reinterpret_cast<const DeliveryAckPacket*>(data);
          handleDeliveryAck(from, *packet);
        }
        break;
      default:
        break;
    }
  }

private:
  static void taskEntry(void* ctx) {
    static_cast<MasterEngine*>(ctx)->run();
  }

  static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (activeMaster() != nullptr && info != nullptr) {
      activeMaster()->enqueueIncoming(info->src_addr, data, len);
    }
  }

  void enqueueIncoming(const uint8_t* from, const uint8_t* data, int len) {
    if (rxQueue_ == nullptr || from == nullptr || data == nullptr || len <= 0 ||
        len > static_cast<int>(MAX_CLUSTER_PACKET_BYTES)) return;
    RxEvent event = {};
    memcpy(event.from, from, sizeof(event.from));
    event.length = static_cast<uint16_t>(len);
    memcpy(event.data, data, event.length);
    if (xQueueSend(rxQueue_, &event, 0) != pdTRUE) {
      debugPrintf("[ClusterMaster] RX queue full\n");
    }
  }

  void processIncomingQueue() {
    if (rxQueue_ == nullptr) return;
    RxEvent event;
    uint8_t processed = 0;
    while (processed < RX_QUEUE_DEPTH && xQueueReceive(rxQueue_, &event, 0) == pdTRUE) {
      handleIncoming(event.from, event.data, event.length);
      processed++;
    }
  }

  void run() {
    uint32_t retryCount = 0;
    while (!stopRequested_) {
      const bool cleanStop = runWorker();
      shutdownRadio();
      client_.stop();
      WiFi.disconnect(false, false);

      if (stopRequested_ || cleanStop) break;

      retryCount++;
      const uint32_t backoffMs = retryBackoffMs(retryCount);
      waitRetry(backoffMs, retryCount);
    }

    shutdownRadio();
    client_.stop();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    task_ = nullptr;
    vTaskDelete(nullptr);
  }

  bool runWorker() {
    if (!connectWifi()) return stopRequested_;
    if (!beginRadio()) return stopRequested_;
    if (!connectPool()) return stopRequested_;

    Subscribe sub;
    WorkContext work;
    float poolDifficulty = 0.00015f;
    uint32_t requestId = 1;
    uint32_t lastRateAt = millis();
    uint64_t lastLocalHashes = 0;
    uint32_t lastBeaconAt = 0;

    setState(MasterState::Subscribing);
    sendSubscribe(requestId++);
    const uint32_t subStart = millis();
    bool subscribed = false;
    while (!stopRequested_ && client_.connected() && millis() - subStart < 15000) {
      String line;
      if (readLine(line, 250)) {
        parseLine(line, sub, work, poolDifficulty, subscribed);
        if (subscribed) break;
      }
      serviceRadio(work, poolDifficulty, lastBeaconAt);
      updateRate(lastRateAt, lastLocalHashes);
    }
    if (!subscribed) {
      setError("Subscribe timeout");
      return false;
    }

    sendAuth(requestId++);
    sendSuggestDifficulty(requestId++, poolDifficulty);
    setState(MasterState::Mining);

    while (!stopRequested_) {
      if (!client_.connected()) {
        setError("Pool disconnected");
        return false;
      }

      while (client_.available()) {
        String line;
        if (!readLine(line, 5)) break;
        parseLine(line, sub, work, poolDifficulty, subscribed);
      }

      serviceRadio(work, poolDifficulty, lastBeaconAt);
      if (work.ready && cluster_.localMining) {
        mineLocalBatch(work, poolDifficulty, requestId);
      } else {
        vTaskDelay(2);
      }
      vTaskDelay(1);
      updateRate(lastRateAt, lastLocalHashes);
    }
    return true;
  }

  static uint32_t retryBackoffMs(uint32_t retryCount) {
    uint32_t delayMs = RETRY_BACKOFF_INITIAL_MS;
    const uint8_t shifts = retryCount > 1 ? static_cast<uint8_t>(retryCount - 1) : 0;
    for (uint8_t i = 0; i < shifts && delayMs < RETRY_BACKOFF_MAX_MS; i++) {
      delayMs *= 2;
      if (delayMs > RETRY_BACKOFF_MAX_MS) {
        delayMs = RETRY_BACKOFF_MAX_MS;
        break;
      }
    }
    return delayMs;
  }

  void waitRetry(uint32_t delayMs, uint32_t retryCount) {
    const uint32_t started = millis();
    while (!stopRequested_) {
      const uint32_t elapsed = millis() - started;
      if (elapsed >= delayMs) break;
      const uint32_t remaining = delayMs - elapsed;
      setRetry(retryCount, (remaining + 999) / 1000);
      delay(250);
    }
  }

  bool connectWifi() {
    WiFiLogic wifi;
    wifi.loadProfiles();
    bool sawProfile = false;
    for (uint8_t slot = 0; slot < WiFiLogic::MAX_PROFILES && !stopRequested_; slot++) {
      if (!wifi.isProfileUsed(slot)) continue;
      sawProfile = true;
      const String ssid = wifi.getSsid(slot);
      const String pass = wifi.getPass(slot);
      setState(MasterState::ConnectingWifi);
      setSsid(ssid.c_str());
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      WiFi.setHostname(hostname_);
      WiFi.disconnect(false, false);
      WiFi.begin(ssid.c_str(), pass.c_str());
      const uint32_t started = millis();
      while (!stopRequested_ && millis() - started < 15000) {
        if (WiFi.status() == WL_CONNECTED) return true;
        delay(150);
      }
    }
    setState(sawProfile ? MasterState::WifiFailed : MasterState::NoWifi);
    return false;
  }

  bool beginRadio() {
    if (rxQueue_ == nullptr) {
      rxQueue_ = xQueueCreate(RX_QUEUE_DEPTH, sizeof(RxEvent));
      if (rxQueue_ == nullptr) {
        setState(MasterState::RadioFailed);
        setError("Radio queue failed");
        return false;
      }
    } else {
      xQueueReset(rxQueue_);
    }
    activeMaster() = this;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&channel_, &second);
    if (channel_ == 0) channel_ = 1;

    esp_err_t result = esp_now_init();
    if (result != ESP_OK && result != ESP_ERR_ESPNOW_INTERNAL) {
      setState(MasterState::RadioFailed);
      setError("ESP-NOW init failed");
      return false;
    }
    radioStarted_ = true;
    esp_now_register_recv_cb(MasterEngine::onReceiveStatic);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = channel_;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    result = esp_now_add_peer(&peer);
    if (result != ESP_OK && result != ESP_ERR_ESPNOW_EXIST) {
      setState(MasterState::RadioFailed);
      setError("Broadcast peer failed");
      return false;
    }
    portENTER_CRITICAL(&mux_);
    stats_.channel = channel_;
    portEXIT_CRITICAL(&mux_);
    return true;
  }

  void shutdownRadio() {
    if (activeMaster() == this) activeMaster() = nullptr;
    if (radioStarted_) {
      esp_now_unregister_recv_cb();
      esp_now_deinit();
      radioStarted_ = false;
    }
    if (rxQueue_ != nullptr) xQueueReset(rxQueue_);
  }

  void destroyRxQueue() {
    if (rxQueue_ != nullptr) {
      vQueueDelete(rxQueue_);
      rxQueue_ = nullptr;
    }
  }

  bool connectPool() {
    setState(MasterState::ConnectingPool);
    if (!client_.connect(minerConfig_.poolHost.c_str(), minerConfig_.poolPort)) {
      setState(MasterState::PoolFailed);
      setError("Pool connect failed");
      return false;
    }
    return true;
  }

  void fillHeader(PacketHeader& header, PacketType type) {
    memset(&header, 0, sizeof(header));
    header.magic0 = MAGIC_0;
    header.magic1 = MAGIC_1;
    header.version = PROTOCOL_VERSION;
    header.type = static_cast<uint8_t>(type);
    header.clusterId = cluster_.clusterId;
    header.sessionId = sessionId_;
    header.sequence = nextSequence_++;
    readMac(header.sender);
  }

  bool ensurePeer(const uint8_t* mac) {
    if (mac == nullptr) return false;
    if (esp_now_is_peer_exist(mac)) return true;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = channel_;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t result = esp_now_add_peer(&peer);
    return result == ESP_OK || result == ESP_ERR_ESPNOW_EXIST;
  }

  void sendPairBeacon() {
    if (!radioStarted_) return;
    PairBeaconPacket packet = {};
    fillHeader(packet.header, PacketType::PairBeacon);
    packet.channel = channel_;
    const uint32_t now = millis();
    packet.pairingEndsInMs = pairingUntilMs_ > now ? pairingUntilMs_ - now : 0;
    esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  }

  void sendMasterBeacon() {
    if (!radioStarted_) return;
    MasterBeaconPacket packet = {};
    fillHeader(packet.header, PacketType::MasterBeacon);
    MasterStats snapshot = stats();
    packet.channel = channel_;
    packet.state = static_cast<uint8_t>(snapshot.state);
    packet.slaveCount = slaveCount();
    packet.localMining = cluster_.localMining ? 1 : 0;
    packet.totalHps = snapshot.totalHps;
    packet.jobSeq = currentJobSeq_;
    esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  }

  void sendCancelJob(uint32_t jobSeq) {
    if (!radioStarted_) return;
    CancelJobPacket packet = {};
    fillHeader(packet.header, PacketType::CancelJob);
    packet.jobSeq = jobSeq;
    esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
    for (uint8_t i = 0; i < MAX_SLAVES; i++) {
      slaves_[i].assigned = false;
      slaves_[i].queued = false;
      slaves_[i].assignmentAcknowledged = false;
      slaves_[i].queuedAcknowledged = false;
    }
  }

  void serviceRadio(WorkContext& work, float poolDifficulty, uint32_t& lastBeaconAt) {
    processIncomingQueue();
    const uint32_t now = millis();
    const bool pairingActive = pairingUntilMs_ != 0 && static_cast<int32_t>(pairingUntilMs_ - now) > 0;
    const uint8_t activeSlaves = slaveCount();
    portENTER_CRITICAL(&mux_);
    stats_.pairing = pairingActive;
    stats_.slaveCount = activeSlaves;
    portEXIT_CRITICAL(&mux_);

    if (pairingActive && now - lastPairBeaconAt_ >= 1000) {
      lastPairBeaconAt_ = now;
      sendPairBeacon();
    }
    if (now - lastBeaconAt >= 1000) {
      lastBeaconAt = now;
      sendMasterBeacon();
    }
    if (work.ready) assignSlaveWork(work, poolDifficulty);
    updateSlaveTotals();
  }

  int8_t findSlave(const uint8_t* mac) const {
    for (uint8_t i = 0; i < MAX_SLAVES; i++) {
      if (slaves_[i].used && macEquals(slaves_[i].mac, mac)) return i;
    }
    return -1;
  }

  int8_t allocSlave(const uint8_t* mac) {
    int8_t index = findSlave(mac);
    if (index >= 0) return index;
    for (uint8_t i = 0; i < MAX_SLAVES; i++) {
      if (!slaves_[i].used) {
        slaves_[i].used = true;
        memcpy(slaves_[i].mac, mac, 6);
        return i;
      }
    }
    return -1;
  }

  void addOrUpdateSlave(const uint8_t* mac, const char* status, uint32_t hps, uint32_t effectiveHps,
                        float best, uint32_t jobSeq, uint8_t capabilities,
                        uint32_t preferredAssignmentMs, uint32_t maxAssignmentRange) {
    int8_t index = allocSlave(mac);
    if (index < 0) return;
    SlaveRecord& slave = slaves_[index];
    slave.lastSeenMs = millis();
    if (hps > 0) slave.hashrate = hps;
    if (effectiveHps > 0) slave.effectiveHashrate = effectiveHps;
    else if (hps > 0 && slave.effectiveHashrate == 0) slave.effectiveHashrate = hps;
    if (capabilities != 0) slave.capabilities = capabilities;
    if (preferredAssignmentMs != 0) slave.preferredAssignmentMs = preferredAssignmentMs;
    if (maxAssignmentRange != 0) slave.maxAssignmentRange = maxAssignmentRange;
    if (best > slave.bestDifficulty) slave.bestDifficulty = best;
    if (jobSeq > 0) slave.lastJobSeq = jobSeq;
    if (!(slave.assigned && strcmp(status, "hello") == 0)) {
      strncpy(slave.status, status, sizeof(slave.status) - 1);
      slave.status[sizeof(slave.status) - 1] = '\0';
    }
    ensurePeer(mac);
  }

  uint32_t targetMsForSlave(const SlaveRecord& slave) const {
    if (slave.preferredAssignmentMs != 0) return slave.preferredAssignmentMs;
    if (slave.capabilities & SlaveCapFastAssignment) return FAST_TARGET_ASSIGNMENT_MS;
    return TARGET_ASSIGNMENT_MS;
  }

  uint32_t maxRangeForSlave(const SlaveRecord& slave) const {
    if (slave.maxAssignmentRange != 0) return slave.maxAssignmentRange;
    if (slave.capabilities & SlaveCapFastAssignment) return MAX_FAST_RANGE;
    return MAX_RANGE;
  }

  uint32_t rangeForSlave(const SlaveRecord& slave) const {
    if (slave.hashrate == 0) {
      return (slave.capabilities & SlaveCapFastAssignment) ? DEFAULT_FAST_RANGE : DEFAULT_RANGE;
    }
    uint32_t range = static_cast<uint32_t>((static_cast<uint64_t>(slave.hashrate) * targetMsForSlave(slave)) / 1000ULL);
    if (range < MIN_RANGE) range = MIN_RANGE;
    const uint32_t maxRange = maxRangeForSlave(slave);
    if (range > maxRange) range = maxRange;
    return range;
  }

  uint32_t allocateNonceRange(WorkContext& work, uint32_t count) {
    const uint32_t start = work.nextNonce;
    work.nextNonce += count;
    return start;
  }

  uint32_t estimatedAssignmentMs(const SlaveRecord& slave) const {
    if (slave.hashrate == 0 || slave.nonceCount == 0) return targetMsForSlave(slave);
    uint32_t estimate = static_cast<uint32_t>((static_cast<uint64_t>(slave.nonceCount) * 1000ULL) / slave.hashrate);
    if (estimate < 1000) estimate = 1000;
    return estimate;
  }

  bool shouldPrefetch(const SlaveRecord& slave, uint32_t now) const {
    if (!(slave.capabilities & SlaveCapPendingAssignment)) return false;
    if (!slave.assigned || !slave.assignmentAcknowledged || slave.queued || slave.hashrate == 0) return false;
    const uint32_t estimate = estimatedAssignmentMs(slave);
    const uint32_t prefetchAt = max<uint32_t>(1000, (estimate * PREFETCH_PERCENT) / 100);
    return now - slave.assignedAtMs >= prefetchAt;
  }

  uint32_t timeoutMsForSlave(const SlaveRecord& slave) const {
    const uint32_t estimate = estimatedAssignmentMs(slave);
    return max<uint32_t>(ASSIGNMENT_TIMEOUT_MS, estimate * 2 + 3000);
  }

  bool sendStoredAssignment(SlaveRecord& slave, float poolDifficulty, bool queued) {
    WorkAssignPacket packet = {};
    fillHeader(packet.header, PacketType::WorkAssign);
    packet.jobSeq = queued ? slave.queuedJobSeq : slave.assignedJobSeq;
    packet.assignmentId = queued ? slave.queuedAssignmentId : slave.assignmentId;
    packet.nonceStart = queued ? slave.queuedNonceStart : slave.nonceStart;
    packet.nonceCount = queued ? slave.queuedNonceCount : slave.nonceCount;
    packet.poolDifficulty = poolDifficulty;
    memcpy(packet.blockHeader, currentWork_.header, sizeof(packet.blockHeader));
    if (!ensurePeer(slave.mac)) return false;
    const uint32_t now = millis();
    if (queued) {
      slave.queuedLastSentAtMs = now;
      if (slave.queuedSendAttempts < UINT8_MAX) slave.queuedSendAttempts++;
    } else {
      slave.assignmentLastSentAtMs = now;
      if (slave.assignmentSendAttempts < UINT8_MAX) slave.assignmentSendAttempts++;
    }
    return esp_now_send(slave.mac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet)) == ESP_OK;
  }

  bool sendAssignmentToSlave(SlaveRecord& slave, WorkContext& work, float poolDifficulty, bool queued) {
    const uint32_t range = rangeForSlave(slave);
    const uint32_t assignmentId = nextAssignmentId_++;
    const uint32_t nonceStart = allocateNonceRange(work, range);
    const uint32_t now = millis();

    if (queued) {
      slave.queued = true;
      slave.queuedJobSeq = work.jobSeq;
      slave.queuedAssignmentId = assignmentId;
      slave.queuedNonceStart = nonceStart;
      slave.queuedNonceCount = range;
      slave.queuedAtMs = now;
      slave.queuedAcknowledged = false;
      slave.queuedSendAttempts = 0;
      slave.queuedLastSentAtMs = 0;
      strncpy(slave.status, "queued", sizeof(slave.status) - 1);
    } else {
      slave.assignmentId = assignmentId;
      slave.assignedJobSeq = work.jobSeq;
      slave.nonceStart = nonceStart;
      slave.nonceCount = range;
      slave.assignedAtMs = now;
      slave.assigned = true;
      slave.assignmentAcknowledged = false;
      slave.assignmentSendAttempts = 0;
      slave.assignmentLastSentAtMs = 0;
      slave.lastJobSeq = work.jobSeq;
      strncpy(slave.status, "assigned", sizeof(slave.status) - 1);
    }
    slave.status[sizeof(slave.status) - 1] = '\0';
    return sendStoredAssignment(slave, poolDifficulty, queued);
  }

  void serviceAssignmentDelivery(SlaveRecord& slave, float poolDifficulty, uint32_t now) {
    if (slave.assigned && !slave.assignmentAcknowledged &&
        slave.assignmentSendAttempts < DELIVERY_RETRY_LIMIT &&
        now - slave.assignmentLastSentAtMs >= DELIVERY_RETRY_MS) {
      sendStoredAssignment(slave, poolDifficulty, false);
    }
    if (slave.queued && !slave.queuedAcknowledged &&
        slave.queuedSendAttempts < DELIVERY_RETRY_LIMIT &&
        now - slave.queuedLastSentAtMs >= DELIVERY_RETRY_MS) {
      sendStoredAssignment(slave, poolDifficulty, true);
    }
  }

  void assignSlaveWork(WorkContext& work, float poolDifficulty) {
    if (!radioStarted_) return;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < MAX_SLAVES; i++) {
      SlaveRecord& slave = slaves_[i];
      if (!slave.used) continue;
      if (now - slave.lastSeenMs > 10000) continue;
      serviceAssignmentDelivery(slave, poolDifficulty, now);
      if (slave.assigned) {
        if (shouldPrefetch(slave, now)) {
          sendAssignmentToSlave(slave, work, poolDifficulty, true);
        }
        if (now - slave.assignedAtMs < timeoutMsForSlave(slave)) {
          continue;
        }
      }
      slave.assigned = false;
      slave.queued = false;
      sendAssignmentToSlave(slave, work, poolDifficulty, false);
    }
  }

  void sendDeliveryAck(const uint8_t* to, PacketType ackedType, AckDisposition disposition,
                       uint32_t jobSeq, uint32_t assignmentId) {
    if (!radioStarted_ || !ensurePeer(to)) return;
    DeliveryAckPacket packet = {};
    fillHeader(packet.header, PacketType::DeliveryAck);
    packet.ackedType = static_cast<uint8_t>(ackedType);
    packet.disposition = static_cast<uint8_t>(disposition);
    packet.jobSeq = jobSeq;
    packet.assignmentId = assignmentId;
    esp_now_send(to, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  }

  void handleDeliveryAck(const uint8_t* from, const DeliveryAckPacket& packet) {
    if (packet.ackedType != static_cast<uint8_t>(PacketType::WorkAssign)) return;
    const int8_t index = findSlave(from);
    if (index < 0) return;
    SlaveRecord& slave = slaves_[index];
    slave.lastSeenMs = millis();
    if (slave.assigned && packet.jobSeq == slave.assignedJobSeq &&
        packet.assignmentId == slave.assignmentId) {
      slave.assignmentAcknowledged = true;
      strncpy(slave.status,
              packet.disposition == static_cast<uint8_t>(AckDisposition::Queued) ? "queued" : "working",
              sizeof(slave.status) - 1);
    } else if (slave.queued && packet.jobSeq == slave.queuedJobSeq &&
               packet.assignmentId == slave.queuedAssignmentId) {
      slave.queuedAcknowledged = true;
      strncpy(slave.status, "queued", sizeof(slave.status) - 1);
    }
    slave.status[sizeof(slave.status) - 1] = '\0';
  }

  void handleWorkResult(const uint8_t* from, const WorkResultPacket& packet) {
    int8_t index = findSlave(from);
    sendDeliveryAck(from, PacketType::WorkResult, AckDisposition::ResultReceived,
                    packet.jobSeq, packet.assignmentId);
    if (index < 0) return;
    if (index >= 0) {
      SlaveRecord& slave = slaves_[index];
      slave.lastSeenMs = millis();
      if (slave.lastCompletedJobSeq == packet.jobSeq &&
          slave.lastCompletedAssignmentId == packet.assignmentId) {
        return;
      }
      slave.lastCompletedJobSeq = packet.jobSeq;
      slave.lastCompletedAssignmentId = packet.assignmentId;
      if (packet.assignmentId == slave.assignmentId) {
        if (slave.queued) {
          slave.assignedJobSeq = slave.queuedJobSeq;
          slave.assignmentId = slave.queuedAssignmentId;
          slave.nonceStart = slave.queuedNonceStart;
          slave.nonceCount = slave.queuedNonceCount;
          slave.assignedAtMs = millis();
          slave.assignmentAcknowledged = slave.queuedAcknowledged;
          slave.assignmentSendAttempts = slave.queuedSendAttempts;
          slave.assignmentLastSentAtMs = slave.queuedLastSentAtMs;
          slave.queued = false;
          slave.queuedAcknowledged = false;
          strncpy(slave.status, "prefetched", sizeof(slave.status) - 1);
        } else {
          slave.assigned = false;
        }
      } else if (packet.assignmentId == slave.queuedAssignmentId) {
        slave.queued = false;
        slave.queuedAcknowledged = false;
      }
      if (packet.hashrate > 0.0f) {
        slave.hashrate = static_cast<uint32_t>(packet.hashrate);
      }
      if (packet.effectiveHashrate > 0) {
        slave.effectiveHashrate = packet.effectiveHashrate;
      } else if (packet.hashrate > 0.0f && slave.effectiveHashrate == 0) {
        slave.effectiveHashrate = static_cast<uint32_t>(packet.hashrate);
      }
      if (packet.capabilities != 0) slave.capabilities = packet.capabilities;
      slave.lastJobSeq = packet.jobSeq;
      if (packet.bestDifficulty > slave.bestDifficulty) slave.bestDifficulty = packet.bestDifficulty;
      if (packet.found) strncpy(slave.status, "share", sizeof(slave.status) - 1);
      slave.status[sizeof(slave.status) - 1] = '\0';
    }
    portENTER_CRITICAL(&mux_);
    stats_.slaveHashes += packet.hashesDone;
    if (packet.bestDifficulty > stats_.bestDifficulty) stats_.bestDifficulty = packet.bestDifficulty;
    portEXIT_CRITICAL(&mux_);

    if (!packet.found || packet.jobSeq != currentJobSeq_ || !currentWork_.ready) return;
    uint8_t hash[32];
    if (!hashNonce(currentWork_, packet.foundNonce, hash)) return;
    const float diff = difficultyFromHash(hash);
    if (diff <= currentPoolDifficulty_) return;
    noteBest(diff);
    sendSubmit(nextRequestId_++, currentWork_, packet.foundNonce);
  }

  void updateSlaveTotals() {
    uint32_t slaveHps = 0;
    uint8_t active = 0;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < MAX_SLAVES; i++) {
      if (slaves_[i].used && now - slaves_[i].lastSeenMs < 10000) {
        active++;
        slaveHps += slaves_[i].effectiveHashrate > 0 ? slaves_[i].effectiveHashrate : slaves_[i].hashrate;
      }
    }
    portENTER_CRITICAL(&mux_);
    stats_.slaveCount = active;
    stats_.slaveHps = slaveHps;
    stats_.totalHps = stats_.localHps + stats_.slaveHps;
    portEXIT_CRITICAL(&mux_);
  }

  void sendSubscribe(uint32_t id) {
    client_.printf("{\"id\":%lu,\"method\":\"mining.subscribe\",\"params\":[\"MicriCluster/2.0\"]}\n",
                   static_cast<unsigned long>(id));
  }

  void sendAuth(uint32_t id) {
    const String user = stratumUser(minerConfig_);
    client_.printf("{\"params\":[\"%s\",\"%s\"],\"id\":%lu,\"method\":\"mining.authorize\"}\n",
                   user.c_str(), minerConfig_.poolPass.c_str(), static_cast<unsigned long>(id));
  }

  void sendSuggestDifficulty(uint32_t id, float difficulty) {
    client_.printf("{\"id\":%lu,\"method\":\"mining.suggest_difficulty\",\"params\":[%.10g]}\n",
                   static_cast<unsigned long>(id), static_cast<double>(difficulty));
  }

  void sendSubmit(uint32_t id, const WorkContext& work, uint32_t nonce) {
    char nonceHex[9];
    snprintf(nonceHex, sizeof(nonceHex), "%08lx", static_cast<unsigned long>(nonce));
    const String user = stratumUser(minerConfig_);
    client_.printf("{\"id\":%lu,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}\n",
                   static_cast<unsigned long>(id), user.c_str(), work.job.jobId.c_str(),
                   work.sub.extranonce2.c_str(), work.job.ntime.c_str(), nonceHex);
    incrementSubmitted();
  }

  bool readLine(String& out, uint32_t timeoutMs) {
    const uint32_t started = millis();
    while (!stopRequested_ && millis() - started < timeoutMs) {
      if (client_.available()) {
        out = client_.readStringUntil('\n');
        out.trim();
        return out.length() > 0;
      }
      delay(1);
    }
    return false;
  }

  void parseLine(const String& line, Subscribe& sub, WorkContext& work, float& poolDifficulty, bool& subscribed) {
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) return;

    if (doc["id"].is<uint32_t>() && doc["result"].is<bool>()) {
      const bool ok = doc["result"].as<bool>();
      if (doc["id"].as<uint32_t>() >= 4) {
        if (ok) incrementAccepted();
        else incrementRejected();
      }
    }

    const char* method = doc["method"] | "";
    if (strcmp(method, "mining.set_difficulty") == 0) {
      poolDifficulty = doc["params"][0] | poolDifficulty;
      currentPoolDifficulty_ = poolDifficulty;
      setPoolDifficulty(poolDifficulty);
      return;
    }

    if (strcmp(method, "mining.notify") == 0) {
      JsonArray params = doc["params"].as<JsonArray>();
      if (params.size() < 9) return;
      Job job;
      job.jobId = params[0].as<const char*>();
      job.prevHash = params[1].as<const char*>();
      job.coinb1 = params[2].as<const char*>();
      job.coinb2 = params[3].as<const char*>();
      JsonArray branches = params[4].as<JsonArray>();
      job.branchCount = 0;
      for (JsonVariant branch : branches) {
        if (job.branchCount >= 16) break;
        job.branches[job.branchCount++] = branch.as<const char*>();
      }
      job.version = params[5].as<const char*>();
      job.nbits = params[6].as<const char*>();
      job.ntime = params[7].as<const char*>();
      job.cleanJobs = params[8] | false;
      work.job = job;
      work.sub = sub;
      work.jobSeq = ++currentJobSeq_;
      work.nextNonce = 0xA5000000UL + currentJobSeq_ * 0x100000UL;
      work.ready = buildWork(work);
      if (work.ready) {
        currentWork_ = work;
        sendCancelJob(work.jobSeq);
      }
      incrementJobs();
      return;
    }

    if (doc["result"].is<JsonArray>()) {
      JsonArray result = doc["result"].as<JsonArray>();
      if (result.size() >= 3) {
        sub.extranonce1 = result[1].as<const char*>();
        sub.extranonce2Size = result[2] | 4;
        sub.extranonce2 = extranonce2ForSize(sub.extranonce2Size);
        subscribed = sub.extranonce1.length() > 0;
      }
    }
  }

  bool buildWork(WorkContext& work) {
    if (work.sub.extranonce1.length() == 0) return false;
    const String coinbase = work.job.coinb1 + work.sub.extranonce1 + work.sub.extranonce2 + work.job.coinb2;
    uint8_t coinbaseBytes[384];
    const size_t coinbaseLen = hexToBytes(coinbase.c_str(), coinbaseBytes, sizeof(coinbaseBytes));
    if (coinbaseLen == 0) return false;

    uint8_t merkle[32];
    doubleSha(coinbaseBytes, coinbaseLen, merkle);
    for (uint8_t i = 0; i < work.job.branchCount; i++) {
      uint8_t branch[32];
      if (hexToBytes(work.job.branches[i].c_str(), branch, sizeof(branch)) != 32) return false;
      uint8_t concat[64];
      memcpy(concat, merkle, 32);
      memcpy(concat + 32, branch, 32);
      doubleSha(concat, sizeof(concat), merkle);
    }

    char merkleHex[65];
    bytesToHex(merkle, 32, merkleHex, sizeof(merkleHex));
    const String headerHex = work.job.version + work.job.prevHash + String(merkleHex) + work.job.ntime + work.job.nbits + "00000000";
    if (hexToBytes(headerHex.c_str(), work.header, sizeof(work.header)) != 80) return false;

    reverseRange(work.header, 0, 4);
    for (uint8_t offset = 4; offset < 36; offset += 4) {
      reverseRange(work.header, offset, 4);
    }
    reverseRange(work.header, 68, 4);
    reverseRange(work.header, 72, 4);
    preparePaddedWork(work);
    return true;
  }

  static String extranonce2ForSize(uint8_t size) {
    if (size == 2) return "0001";
    if (size == 8) return "0000000000000001";
    return "00000001";
  }

#if defined(CONFIG_IDF_TARGET_ESP32)
  static inline void shaWaitIdleEsp32() {
    while (DPORT_REG_READ(SHA_256_BUSY_REG)) {}
  }

  static inline void shaFillBlockEsp32(const uint32_t* words) {
    uint32_t* reg = reinterpret_cast<uint32_t*>(SHA_TEXT_BASE);
    for (uint8_t i = 0; i < 16; i++) reg[i] = words[i];
  }

  static inline void shaFillUpperEsp32(const uint32_t* words, uint32_t nonce) {
    uint32_t* reg = reinterpret_cast<uint32_t*>(SHA_TEXT_BASE);
    reg[0] = words[0];
    reg[1] = words[1];
    reg[2] = words[2];
    reg[3] = __builtin_bswap32(nonce);
    reg[4] = 0x80000000;
    for (uint8_t i = 5; i < 15; i++) reg[i] = 0;
    reg[15] = 0x00000280;
  }

  static inline void shaFillDoubleEsp32() {
    uint32_t* reg = reinterpret_cast<uint32_t*>(SHA_TEXT_BASE);
    reg[8] = 0x80000000;
    for (uint8_t i = 9; i < 15; i++) reg[i] = 0;
    reg[15] = 0x00000100;
  }

  static inline void storeWord(uint8_t* out, uint8_t index, uint32_t value) {
    memcpy(out + index * 4, &value, sizeof(value));
  }

  static inline bool shaReadDigestCandidateEsp32(uint8_t* hash) {
    DPORT_INTERRUPT_DISABLE();
    const uint32_t fin = DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 7 * 4);
    if ((fin & 0xFFFF) != 0) {
      DPORT_INTERRUPT_RESTORE();
      return false;
    }
    storeWord(hash, 7, __builtin_bswap32(fin));
    for (uint8_t i = 0; i < 7; i++) {
      storeWord(hash, i, __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + i * 4)));
    }
    DPORT_INTERRUPT_RESTORE();
    return true;
  }

  void mineLocalBatch(WorkContext& work, float poolDifficulty, uint32_t& requestId) {
    uint8_t hash[32];
    uint32_t foundNonce = 0;
    float foundDiff = 0.0f;
    bool found = false;
    uint32_t processed = 0;
    float localBest = 0.0f;
    const uint32_t startNonce = allocateNonceRange(work, 4096);
    const uint32_t endNonce = startNonce + 4096UL;

    esp_sha_lock_engine(SHA2_256);
    for (uint32_t nonce = startNonce; nonce != endNonce && !stopRequested_; nonce++) {
      shaFillBlockEsp32(work.hwWords);
      sha_ll_start_block(SHA2_256);
      shaWaitIdleEsp32();
      shaFillUpperEsp32(work.hwWords + 16, nonce);
      sha_ll_continue_block(SHA2_256);
      shaWaitIdleEsp32();
      sha_ll_load(SHA2_256);
      shaWaitIdleEsp32();
      shaFillDoubleEsp32();
      sha_ll_start_block(SHA2_256);
      shaWaitIdleEsp32();
      sha_ll_load(SHA2_256);
      processed++;
      if (shaReadDigestCandidateEsp32(hash)) {
        const float diff = difficultyFromHash(hash);
        if (diff > localBest) localBest = diff;
        if (diff > poolDifficulty) {
          found = true;
          foundNonce = nonce;
          foundDiff = diff;
          break;
        }
      }
    }
    esp_sha_unlock_engine(SHA2_256);
    noteLocalHashes(processed, localBest);
    if (found && client_.connected()) {
      noteBest(foundDiff);
      sendSubmit(requestId++, work, foundNonce);
    }
  }
#else
  void mineLocalBatch(WorkContext& work, float poolDifficulty, uint32_t& requestId) {
    uint8_t hash[32];
    uint32_t foundNonce = 0;
    float foundDiff = 0.0f;
    bool found = false;
    uint32_t processed = 0;
    float localBest = 0.0f;
    const uint32_t startNonce = allocateNonceRange(work, 1024);
    for (uint32_t i = 0; i < 1024 && !stopRequested_; i++) {
      const uint32_t nonce = startNonce + i;
      processed++;
      if (hashNonce(work, nonce, hash)) {
        const float diff = difficultyFromHash(hash);
        if (diff > localBest) localBest = diff;
        if (diff > poolDifficulty) {
          found = true;
          foundNonce = nonce;
          foundDiff = diff;
          break;
        }
      }
    }
    noteLocalHashes(processed, localBest);
    if (found && client_.connected()) {
      noteBest(foundDiff);
      sendSubmit(requestId++, work, foundNonce);
    }
  }
#endif

  void resetStats() {
    portENTER_CRITICAL(&mux_);
    stats_ = MasterStats();
    stats_.localMining = cluster_.localMining;
    stats_.poolDifficulty = 0.00015f;
    startedAtMs_ = millis();
    portEXIT_CRITICAL(&mux_);
  }

  void setState(MasterState state) {
    portENTER_CRITICAL(&mux_);
    stats_.state = state;
    if (state != MasterState::Error) {
      stats_.lastError[0] = '\0';
      stats_.retryInSeconds = 0;
    }
    stats_.uptimeSeconds = startedAtMs_ == 0 ? 0 : (millis() - startedAtMs_) / 1000;
    portEXIT_CRITICAL(&mux_);
  }

  void setError(const char* message) {
    portENTER_CRITICAL(&mux_);
    stats_.state = MasterState::Error;
    strncpy(stats_.lastError, message, sizeof(stats_.lastError) - 1);
    stats_.lastError[sizeof(stats_.lastError) - 1] = '\0';
    portEXIT_CRITICAL(&mux_);
  }

  void setRetry(uint32_t retryCount, uint32_t retryInSeconds) {
    portENTER_CRITICAL(&mux_);
    stats_.retryCount = retryCount;
    stats_.retryInSeconds = retryInSeconds;
    portEXIT_CRITICAL(&mux_);
  }

  void setSsid(const char* ssid) {
    portENTER_CRITICAL(&mux_);
    strncpy(stats_.ssid, ssid, sizeof(stats_.ssid) - 1);
    stats_.ssid[sizeof(stats_.ssid) - 1] = '\0';
    portEXIT_CRITICAL(&mux_);
  }

  void setPoolDifficulty(float diff) {
    portENTER_CRITICAL(&mux_);
    stats_.poolDifficulty = diff;
    portEXIT_CRITICAL(&mux_);
  }

  void incrementJobs() {
    portENTER_CRITICAL(&mux_);
    stats_.jobs++;
    portEXIT_CRITICAL(&mux_);
  }

  void incrementSubmitted() {
    portENTER_CRITICAL(&mux_);
    stats_.submitted++;
    portEXIT_CRITICAL(&mux_);
  }

  void incrementAccepted() {
    portENTER_CRITICAL(&mux_);
    stats_.accepted++;
    portEXIT_CRITICAL(&mux_);
  }

  void incrementRejected() {
    portENTER_CRITICAL(&mux_);
    stats_.rejected++;
    portEXIT_CRITICAL(&mux_);
  }

  void noteBest(float diff) {
    portENTER_CRITICAL(&mux_);
    if (diff > stats_.bestDifficulty) stats_.bestDifficulty = diff;
    portEXIT_CRITICAL(&mux_);
  }

  void noteLocalHashes(uint32_t count, float bestDiff) {
    if (count == 0) return;
    portENTER_CRITICAL(&mux_);
    stats_.localHashes += count;
    if (bestDiff > stats_.bestDifficulty) stats_.bestDifficulty = bestDiff;
    stats_.uptimeSeconds = (millis() - startedAtMs_) / 1000;
    portEXIT_CRITICAL(&mux_);
  }

  void updateRate(uint32_t& lastRateAt, uint64_t& lastLocalHashes) {
    const uint32_t now = millis();
    if (now - lastRateAt < 1000) return;
    portENTER_CRITICAL(&mux_);
    const uint64_t local = stats_.localHashes;
    const uint32_t elapsed = now - lastRateAt;
    stats_.localHps = elapsed == 0 ? 0 : static_cast<uint32_t>((local - lastLocalHashes) * 1000ULL / elapsed);
    stats_.totalHps = stats_.localHps + stats_.slaveHps;
    stats_.uptimeSeconds = (now - startedAtMs_) / 1000;
    portEXIT_CRITICAL(&mux_);
    lastRateAt = now;
    lastLocalHashes = local;
  }

  ClusterSettings settings_;
  ClusterConfig cluster_;
  MinerConfig minerConfig_;
  const char* hostname_ = DEFAULT_HOSTNAME;
  WiFiClient client_;
  TaskHandle_t task_ = nullptr;
  volatile bool stopRequested_ = false;
  bool radioStarted_ = false;
  uint8_t channel_ = 1;
  uint32_t nextSequence_ = 1;
  uint32_t nextAssignmentId_ = 1;
  uint32_t currentJobSeq_ = 0;
  uint32_t sessionId_ = 0;
  uint32_t nextRequestId_ = 4;
  uint32_t pairingUntilMs_ = 0;
  uint32_t lastPairBeaconAt_ = 0;
  float currentPoolDifficulty_ = 0.00015f;
  WorkContext currentWork_;
  SlaveRecord slaves_[MAX_SLAVES];
  uint32_t startedAtMs_ = 0;
  MasterStats stats_;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  QueueHandle_t rxQueue_ = nullptr;
};

class SlaveEngine {
public:
  ~SlaveEngine() { stop(); }

  void begin() {
    startedAtMs_ = millis();
#if MICRI_CLUSTER_SLAVE_TASK_ENABLED
    stopRequested_ = false;
#endif
    loadPairing();
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    readMac(localMac_);
    if (rxQueue_ == nullptr) rxQueue_ = xQueueCreate(RX_QUEUE_DEPTH, sizeof(RxEvent));
    if (rxQueue_ == nullptr) {
      setError("Radio queue failed");
      return;
    }
    xQueueReset(rxQueue_);
    activeSlave() = this;
    if (esp_now_init() == ESP_OK) {
      radioStarted_ = true;
      esp_now_register_recv_cb(SlaveEngine::onReceiveStatic);
    } else {
      setError("ESP-NOW init failed");
    }
    setChannel(channel_);
    sendHello();
#if MICRI_CLUSTER_SLAVE_TASK_ENABLED
    startTask();
#endif
  }

  void stop() {
#if MICRI_CLUSTER_SLAVE_TASK_ENABLED
    stopTask();
#endif
    if (activeSlave() == this) activeSlave() = nullptr;
    if (radioStarted_) {
      esp_now_unregister_recv_cb();
      esp_now_deinit();
      radioStarted_ = false;
    }
    if (rxQueue_ != nullptr) {
      vQueueDelete(rxQueue_);
      rxQueue_ = nullptr;
    }
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
  }

  void clearPairing() {
    Preferences prefs;
    prefs.begin(CLUSTER_PREF_NS, false);
    prefs.remove("master");
    prefs.remove("id");
    prefs.end();
    paired_ = false;
    memset(masterMac_, 0, sizeof(masterMac_));
    clusterId_ = 0;
    masterSessionId_ = 0;
    highestAssignmentIdSeen_ = 0;
    pendingResultActive_ = false;
    masterPeerChannel_ = 0;
    cancelCurrentAssignment();
    setState(SlaveState::Searching);
  }

  void update() {
#if MICRI_CLUSTER_SLAVE_TASK_ENABLED
    if (task_ == nullptr) service();
    return;
#else
    service();
#endif
  }

  SlaveStats stats() {
    SlaveStats out;
    portENTER_CRITICAL(&mux_);
    out = stats_;
    portEXIT_CRITICAL(&mux_);
    return out;
  }

  void handleIncoming(const uint8_t* from, const uint8_t* data, int len) {
    if (from == nullptr || data == nullptr || len < static_cast<int>(sizeof(PacketHeader))) return;
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
    if (header->magic0 != MAGIC_0 || header->magic1 != MAGIC_1 || header->version != PROTOCOL_VERSION) return;
    const PacketType type = static_cast<PacketType>(header->type);

    if (type == PacketType::PairBeacon && len == static_cast<int>(sizeof(PairBeaconPacket))) {
      const auto* packet = reinterpret_cast<const PairBeaconPacket*>(data);
      if (!paired_) {
        savePairing(from, packet->header.clusterId, packet->header.sessionId);
      }
      if (paired_ && macEquals(masterMac_, from)) {
        if (clusterId_ != packet->header.clusterId) {
          savePairing(from, packet->header.clusterId, packet->header.sessionId);
        } else {
          adoptMasterSession(packet->header.sessionId);
        }
        lastMasterSeenMs_ = millis();
        setChannel(packet->channel);
        sendHello();
      }
      return;
    }

    if (!paired_ || !macEquals(masterMac_, from) || header->clusterId != clusterId_) return;
    lastMasterSeenMs_ = millis();

    if (type == PacketType::MasterBeacon && len == static_cast<int>(sizeof(MasterBeaconPacket))) {
      const auto* packet = reinterpret_cast<const MasterBeaconPacket*>(data);
      adoptMasterSession(packet->header.sessionId);
      setChannel(packet->channel);
      sendHelloThrottled();
      if (!assignmentActive_) setState(SlaveState::Paired);
      return;
    }

    if (masterSessionId_ == 0 || header->sessionId != masterSessionId_) return;

    if (type == PacketType::WorkAssign && len == static_cast<int>(sizeof(WorkAssignPacket))) {
      const auto* packet = reinterpret_cast<const WorkAssignPacket*>(data);
      acceptWork(*packet);
      return;
    }

    if (type == PacketType::CancelJob && len == static_cast<int>(sizeof(CancelJobPacket))) {
      const auto* packet = reinterpret_cast<const CancelJobPacket*>(data);
      cancelAssignmentsExcept(packet->jobSeq);
      return;
    }

    if (type == PacketType::DeliveryAck && len == static_cast<int>(sizeof(DeliveryAckPacket))) {
      const auto* packet = reinterpret_cast<const DeliveryAckPacket*>(data);
      if (packet->ackedType == static_cast<uint8_t>(PacketType::WorkResult) &&
          pendingResultActive_ && packet->jobSeq == pendingResultPacket_.jobSeq &&
          packet->assignmentId == pendingResultPacket_.assignmentId) {
        pendingResultActive_ = false;
      }
    }
  }

private:
#if MICRI_CLUSTER_SLAVE_TASK_ENABLED
  static void taskEntry(void* ctx) {
    static_cast<SlaveEngine*>(ctx)->runTask();
  }

#if MICRI_CLUSTER_DUAL_SLAVE_WORKER && MICRI_CLUSTER_S3_SOFTWARE_WORKER
  static void secondaryTaskEntry(void* ctx) {
    static_cast<SlaveEngine*>(ctx)->runSecondaryTask();
  }
#endif

  bool startTask() {
    if (task_ != nullptr) return true;
    stopRequested_ = false;
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
    BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "MicriSlv0", MICRI_CLUSTER_SLAVE_TASK_STACK,
                                            this, MICRI_CLUSTER_SLAVE_TASK_PRIORITY, &task_, 1);
    if (ok != pdPASS) {
      ok = xTaskCreate(taskEntry, "MicriSlv0", MICRI_CLUSTER_SLAVE_TASK_STACK,
                       this, MICRI_CLUSTER_SLAVE_TASK_PRIORITY, &task_);
    }
#else
    BaseType_t ok = xTaskCreate(taskEntry, "MicriSlv", MICRI_CLUSTER_SLAVE_TASK_STACK,
                                this, MICRI_CLUSTER_SLAVE_TASK_PRIORITY, &task_);
#endif
    if (ok != pdPASS) {
      task_ = nullptr;
      setError("Slave task failed");
      return false;
    }
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER && MICRI_CLUSTER_S3_SOFTWARE_WORKER
    secondaryStopRequested_ = false;
    BaseType_t secondaryOk = xTaskCreatePinnedToCore(secondaryTaskEntry, "MicriSlv1",
                                                     MICRI_CLUSTER_SLAVE_TASK_STACK, this,
                                                     MICRI_CLUSTER_SLAVE_TASK_PRIORITY,
                                                     &secondaryTask_, 0);
    if (secondaryOk != pdPASS) {
      secondaryTask_ = nullptr;
      debugPrintf("[ClusterSlave] secondary worker unavailable\n");
    }
#endif
    return true;
  }

  void stopTask() {
    stopRequested_ = true;
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER && MICRI_CLUSTER_S3_SOFTWARE_WORKER
    secondaryStopRequested_ = true;
#endif
    const uint32_t started = millis();
    while ((task_ != nullptr
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER && MICRI_CLUSTER_S3_SOFTWARE_WORKER
            || secondaryTask_ != nullptr
#endif
            ) && millis() - started < 1000) {
      delay(10);
    }
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER && MICRI_CLUSTER_S3_SOFTWARE_WORKER
    if (secondaryTask_ != nullptr) {
      vTaskDelete(secondaryTask_);
      secondaryTask_ = nullptr;
    }
#endif
    if (task_ != nullptr) {
      vTaskDelete(task_);
      task_ = nullptr;
    }
  }

  void runTask() {
    uint32_t lastCooperativeDelayAt = millis();
    while (!stopRequested_) {
      service();
      if (!assignmentActive_) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lastCooperativeDelayAt = millis();
      } else if (millis() - lastCooperativeDelayAt >= 50) {
        vTaskDelay(1);
        lastCooperativeDelayAt = millis();
      } else {
        taskYIELD();
      }
    }
    task_ = nullptr;
    vTaskDelete(nullptr);
  }

#if MICRI_CLUSTER_DUAL_SLAVE_WORKER && MICRI_CLUSTER_S3_SOFTWARE_WORKER
  void runSecondaryTask() {
    uint32_t lastCooperativeDelayAt = millis();
    while (!stopRequested_ && !secondaryStopRequested_) {
      if (assignmentActive_) {
        if (!processDualWorkerChunk(true)) {
          vTaskDelay(pdMS_TO_TICKS(2));
          lastCooperativeDelayAt = millis();
        } else if (millis() - lastCooperativeDelayAt >= 50) {
          vTaskDelay(1);
          lastCooperativeDelayAt = millis();
        } else {
          taskYIELD();
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }
    secondaryTask_ = nullptr;
    vTaskDelete(nullptr);
  }
#endif
#endif

  void service() {
    processIncomingQueue();
    const uint32_t now = millis();
    servicePendingResult(now);
    startPendingIfReady();
    const bool masterStale = paired_ && now - lastMasterSeenMs_ > 10000;
    if (!paired_ || masterStale) {
      if (now - lastHopMs_ >= 350) {
        lastHopMs_ = now;
        channel_ = channel_ >= 13 ? 1 : channel_ + 1;
        setChannel(channel_);
      }
      if (!assignmentActive_) {
        clearLiveWorkStats();
        setState(SlaveState::Searching);
      }
    }

    if (assignmentActive_) {
      mineChunk();
    } else if (paired_ && now - lastStatusAtMs_ >= 2000) {
      lastStatusAtMs_ = now;
      sendStatus();
    }

    portENTER_CRITICAL(&mux_);
    stats_.paired = paired_;
    stats_.channel = channel_;
    stats_.uptimeSeconds = (now - startedAtMs_) / 1000;
    stats_.capabilities = localSlaveCapabilities();
    memcpy(stats_.masterMac, masterMac_, 6);
    portEXIT_CRITICAL(&mux_);
  }
  static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (activeSlave() != nullptr && info != nullptr) {
      activeSlave()->enqueueIncoming(info->src_addr, data, len);
    }
  }

  void enqueueIncoming(const uint8_t* from, const uint8_t* data, int len) {
    if (rxQueue_ == nullptr || from == nullptr || data == nullptr || len <= 0 ||
        len > static_cast<int>(MAX_CLUSTER_PACKET_BYTES)) return;
    RxEvent event = {};
    memcpy(event.from, from, sizeof(event.from));
    event.length = static_cast<uint16_t>(len);
    memcpy(event.data, data, event.length);
    if (xQueueSend(rxQueue_, &event, 0) != pdTRUE) {
      debugPrintf("[ClusterSlave] RX queue full\n");
    }
  }

  void processIncomingQueue() {
    if (rxQueue_ == nullptr) return;
    RxEvent event;
    uint8_t processed = 0;
    while (processed < RX_QUEUE_DEPTH && xQueueReceive(rxQueue_, &event, 0) == pdTRUE) {
      handleIncoming(event.from, event.data, event.length);
      processed++;
    }
  }

  void setChannel(uint8_t channel) {
    if (channel < 1 || channel > 13) return;
    channel_ = channel;
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
  }

  void loadPairing() {
    Preferences prefs;
    prefs.begin(CLUSTER_PREF_NS, true);
    String master = prefs.getString("master", "");
    clusterId_ = prefs.getUInt("id", 0);
    prefs.end();
    paired_ = parseMac(master, masterMac_) && clusterId_ != 0;
    masterSessionId_ = 0;
    highestAssignmentIdSeen_ = 0;
    setState(paired_ ? SlaveState::Paired : SlaveState::Searching);
  }

  void savePairing(const uint8_t* master, uint32_t clusterId, uint32_t sessionId) {
    memcpy(masterMac_, master, 6);
    clusterId_ = clusterId;
    masterSessionId_ = sessionId;
    highestAssignmentIdSeen_ = 0;
    paired_ = true;
    Preferences prefs;
    prefs.begin(CLUSTER_PREF_NS, false);
    prefs.putString("master", macToString(masterMac_));
    prefs.putUInt("id", clusterId_);
    prefs.end();
    ensureMasterPeer();
    setState(SlaveState::Paired);
  }

  void adoptMasterSession(uint32_t sessionId) {
    if (sessionId == 0 || sessionId == masterSessionId_) return;
    masterSessionId_ = sessionId;
    highestAssignmentIdSeen_ = 0;
    pendingResultActive_ = false;
    cancelCurrentAssignment();
  }

  static String macToString(const uint8_t* mac) {
    char out[18];
    snprintf(out, sizeof(out), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(out);
  }

  static bool parseMac(const String& text, uint8_t out[6]) {
    if (text.length() < 17) return false;
    for (uint8_t i = 0; i < 6; i++) {
      out[i] = static_cast<uint8_t>((hexNibble(text[i * 3]) << 4) | hexNibble(text[i * 3 + 1]));
    }
    return true;
  }

  void fillHeader(PacketHeader& header, PacketType type) {
    memset(&header, 0, sizeof(header));
    header.magic0 = MAGIC_0;
    header.magic1 = MAGIC_1;
    header.version = PROTOCOL_VERSION;
    header.type = static_cast<uint8_t>(type);
    header.clusterId = clusterId_;
    header.sessionId = masterSessionId_;
    header.sequence = nextSequence_++;
    memcpy(header.sender, localMac_, 6);
  }

  bool ensureMasterPeer() {
    if (!paired_ || macIsEmpty(masterMac_)) return false;
    if (esp_now_is_peer_exist(masterMac_)) {
      if (masterPeerChannel_ == channel_) return true;
      esp_now_del_peer(masterMac_);
      masterPeerChannel_ = 0;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, masterMac_, 6);
    peer.channel = channel_;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t result = esp_now_add_peer(&peer);
    const bool ok = result == ESP_OK || result == ESP_ERR_ESPNOW_EXIST;
    if (ok) masterPeerChannel_ = channel_;
    return ok;
  }

  void sendHelloThrottled() {
    const uint32_t now = millis();
    if (now - lastHelloAtMs_ < 3000) return;
    lastHelloAtMs_ = now;
    sendHello();
  }

  void sendHello() {
    if (!paired_ || !radioStarted_ || !ensureMasterPeer()) return;
    SlaveHelloPacket packet = {};
    fillHeader(packet.header, PacketType::SlaveHello);
    packet.role = 1;
    packet.channel = channel_;
    packet.capabilities = localSlaveCapabilities();
    packet.build = 0;
    packet.preferredAssignmentMs = localPreferredAssignmentMs();
    packet.maxAssignmentRange = localMaxAssignmentRange();
    packet.uptimeSeconds = (millis() - startedAtMs_) / 1000;
    esp_now_send(masterMac_, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  }

  void sendStatus() {
    if (!paired_ || !radioStarted_ || !ensureMasterPeer()) return;
    SlaveStatusPacket packet = {};
    fillHeader(packet.header, PacketType::SlaveStatus);
    SlaveStats snapshot = stats();
    packet.state = static_cast<uint8_t>(snapshot.state);
    packet.channel = channel_;
    packet.paired = paired_ ? 1 : 0;
    packet.hashrate = snapshot.hashrate;
    packet.effectiveHashrate = snapshot.effectiveHashrate;
    packet.uptimeSeconds = snapshot.uptimeSeconds;
    packet.lastSeenJobSeq = currentJobSeq_;
    packet.capabilities = localSlaveCapabilities();
    strncpy(packet.lastError, snapshot.lastError, sizeof(packet.lastError) - 1);
    esp_now_send(masterMac_, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  }

  void sendDeliveryAck(PacketType ackedType, AckDisposition disposition,
                       uint32_t jobSeq, uint32_t assignmentId) {
    if (!paired_ || !radioStarted_ || !ensureMasterPeer()) return;
    DeliveryAckPacket packet = {};
    fillHeader(packet.header, PacketType::DeliveryAck);
    packet.ackedType = static_cast<uint8_t>(ackedType);
    packet.disposition = static_cast<uint8_t>(disposition);
    packet.jobSeq = jobSeq;
    packet.assignmentId = assignmentId;
    esp_now_send(masterMac_, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  }

  void sendPendingResult(uint32_t now) {
    if (!pendingResultActive_ || !paired_ || !radioStarted_ || !ensureMasterPeer()) return;
    if (pendingResultSendAttempts_ >= DELIVERY_RETRY_LIMIT) return;
    if (pendingResultSendAttempts_ > 0 && now - pendingResultLastSentAtMs_ < DELIVERY_RETRY_MS) return;
    pendingResultLastSentAtMs_ = now;
    pendingResultSendAttempts_++;
    esp_now_send(masterMac_, reinterpret_cast<const uint8_t*>(&pendingResultPacket_),
                 sizeof(pendingResultPacket_));
  }

  void servicePendingResult(uint32_t now) {
    if (!pendingResultActive_) return;
    if (now - pendingResultCreatedAtMs_ >= RESULT_ACK_TIMEOUT_MS) {
      pendingResultActive_ = false;
      return;
    }
    sendPendingResult(now);
  }

  void sendResult(bool found, uint32_t foundNonce, float foundDifficulty) {
    if (!paired_ || !radioStarted_) return;
    const uint32_t now = millis();
    const uint32_t activeElapsedMs = now - assignmentStartedAtMs_;
    const uint32_t wallElapsedMs = lastResultSentAtMs_ == 0 ? activeElapsedMs : now - lastResultSentAtMs_;
    const uint32_t effectiveHps = wallElapsedMs == 0 ? 0 : static_cast<uint32_t>((assignmentHashes_ * 1000ULL) / wallElapsedMs);
    WorkResultPacket packet = {};
    fillHeader(packet.header, PacketType::WorkResult);
    packet.jobSeq = currentJobSeq_;
    packet.assignmentId = currentAssignmentId_;
    packet.hashesDone = assignmentHashes_;
    packet.elapsedMs = activeElapsedMs;
    packet.wallElapsedMs = wallElapsedMs;
    packet.hashrate = packet.elapsedMs == 0 ? 0.0f : (assignmentHashes_ * 1000.0f) / packet.elapsedMs;
    packet.effectiveHashrate = effectiveHps;
    packet.bestDifficulty = assignmentBest_;
    packet.found = found ? 1 : 0;
    packet.capabilities = localSlaveCapabilities();
    packet.foundNonce = foundNonce;
    packet.foundDifficulty = foundDifficulty;
    pendingResultPacket_ = packet;
    pendingResultActive_ = true;
    pendingResultCreatedAtMs_ = now;
    pendingResultLastSentAtMs_ = 0;
    pendingResultSendAttempts_ = 0;
    sendPendingResult(now);
    lastResultSentAtMs_ = now;
    debugPrintf("[ClusterSlave] result job=%lu assign=%lu hashes=%lu ms=%lu hps=%.1f found=%u\n",
                static_cast<unsigned long>(packet.jobSeq),
                static_cast<unsigned long>(packet.assignmentId),
                static_cast<unsigned long>(packet.hashesDone),
                static_cast<unsigned long>(packet.elapsedMs),
                packet.hashrate,
                packet.found);
    portENTER_CRITICAL(&mux_);
    stats_.hashrate = static_cast<uint32_t>(packet.hashrate);
    stats_.effectiveHashrate = packet.effectiveHashrate;
    stats_.totalHashes += assignmentHashes_;
    stats_.assignmentDone = assignmentHashes_;
    stats_.lastResultHashes = assignmentHashes_;
    stats_.lastResultMs = packet.elapsedMs;
    if (assignmentBest_ > stats_.bestDifficulty) stats_.bestDifficulty = assignmentBest_;
    stats_.completed++;
    portEXIT_CRITICAL(&mux_);
  }

  void acceptWork(const WorkAssignPacket& packet) {
    if (assignmentActive_ && packet.assignmentId == currentAssignmentId_) {
      sendDeliveryAck(PacketType::WorkAssign, AckDisposition::Active,
                      packet.jobSeq, packet.assignmentId);
      return;
    }
    if (pendingAssignmentActive_ && packet.assignmentId == pendingPacket_.assignmentId) {
      sendDeliveryAck(PacketType::WorkAssign, AckDisposition::Queued,
                      packet.jobSeq, packet.assignmentId);
      return;
    }
    if (highestAssignmentIdSeen_ != 0) {
      const int32_t delta = static_cast<int32_t>(packet.assignmentId - highestAssignmentIdSeen_);
      if (delta <= 0) {
        sendDeliveryAck(PacketType::WorkAssign, AckDisposition::Active,
                        packet.jobSeq, packet.assignmentId);
        return;
      }
    }
    highestAssignmentIdSeen_ = packet.assignmentId;

    if (assignmentActive_) {
      pendingPacket_ = packet;
      pendingAssignmentActive_ = true;
      if (packet.jobSeq != currentJobSeq_) {
        deactivateCurrentAssignment();
      }
      sendDeliveryAck(PacketType::WorkAssign, AckDisposition::Queued,
                      packet.jobSeq, packet.assignmentId);
      return;
    }
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
    if (activeWorkerCount() != 0) {
      pendingPacket_ = packet;
      pendingAssignmentActive_ = true;
      sendDeliveryAck(PacketType::WorkAssign, AckDisposition::Queued,
                      packet.jobSeq, packet.assignmentId);
      return;
    }
#endif
    beginAssignment(packet);
    sendDeliveryAck(PacketType::WorkAssign, AckDisposition::Active,
                    packet.jobSeq, packet.assignmentId);
  }

  void beginAssignment(const WorkAssignPacket& packet) {
    currentWork_ = WorkContext();
    memcpy(currentWork_.header, packet.blockHeader, sizeof(currentWork_.header));
    preparePaddedWork(currentWork_);
    currentWork_.ready = true;
    currentWork_.jobSeq = packet.jobSeq;
    currentJobSeq_ = packet.jobSeq;
    currentAssignmentId_ = packet.assignmentId;
    assignmentStartNonce_ = packet.nonceStart;
    assignmentNonceCount_ = packet.nonceCount;
    assignmentNextNonce_ = packet.nonceStart;
    assignmentPoolDifficulty_ = packet.poolDifficulty;
    assignmentStartedAtMs_ = millis();
    assignmentHashes_ = 0;
    assignmentBest_ = 0.0f;
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
    const uint32_t previousHashrate = stats_.hashrate > 0 ? stats_.hashrate : stats_.effectiveHashrate;
    const uint32_t cancelStride = cancellationCheckStrideForHashrate(previousHashrate);
    portENTER_CRITICAL(&workMux_);
    dualNextNonce_ = packet.nonceStart;
    dualFound_ = false;
    dualFoundNonce_ = 0;
    dualFoundDifficulty_ = 0.0f;
    dualCancelCheckMask_ = cancelStride - 1;
    assignmentActive_ = true;
#else
    assignmentActive_ = true;
#endif
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
    portEXIT_CRITICAL(&workMux_);
#endif
    portENTER_CRITICAL(&mux_);
    stats_.jobs++;
    stats_.currentJobSeq = currentJobSeq_;
    stats_.currentAssignmentId = currentAssignmentId_;
    stats_.assignmentSize = assignmentNonceCount_;
    stats_.assignmentDone = 0;
    stats_.capabilities = localSlaveCapabilities();
    portEXIT_CRITICAL(&mux_);
    debugPrintf("[ClusterSlave] work job=%lu assign=%lu start=%lu count=%lu diff=%.6f\n",
                static_cast<unsigned long>(currentJobSeq_),
                static_cast<unsigned long>(currentAssignmentId_),
                static_cast<unsigned long>(assignmentStartNonce_),
                static_cast<unsigned long>(assignmentNonceCount_),
                assignmentPoolDifficulty_);
    setState(SlaveState::Mining);
  }

  void startPendingIfReady() {
    if (assignmentActive_ || !pendingAssignmentActive_) return;
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
    if (activeWorkerCount() != 0) return;
#endif
    WorkAssignPacket packet = pendingPacket_;
    pendingAssignmentActive_ = false;
    beginAssignment(packet);
  }

  void deactivateCurrentAssignment() {
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
    portENTER_CRITICAL(&workMux_);
    assignmentActive_ = false;
    portEXIT_CRITICAL(&workMux_);
#else
    assignmentActive_ = false;
#endif
  }

  void cancelCurrentAssignment() {
    deactivateCurrentAssignment();
    pendingAssignmentActive_ = false;
    clearLiveWorkStats();
    setState(SlaveState::Paired);
  }

  void cancelAssignmentsExcept(uint32_t keepJobSeq) {
    if (assignmentActive_ && currentJobSeq_ != keepJobSeq) {
      deactivateCurrentAssignment();
    }
    if (pendingAssignmentActive_ && pendingPacket_.jobSeq != keepJobSeq) {
      pendingAssignmentActive_ = false;
    }
    if (!assignmentActive_ && !pendingAssignmentActive_) {
      clearLiveWorkStats();
      setState(SlaveState::Paired);
    }
  }

#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
  uint8_t activeWorkerCount() {
    portENTER_CRITICAL(&workMux_);
    const uint8_t count = dualWorkersActive_;
    portEXIT_CRITICAL(&workMux_);
    return count;
  }

  static uint32_t roundDownPowerOfTwo(uint32_t value) {
    uint32_t power = 1;
    while ((power << 1) != 0 && (power << 1) <= value) {
      power <<= 1;
    }
    return power;
  }

  static uint32_t cancellationCheckStrideForHashrate(uint32_t hashrate) {
    static constexpr uint32_t MIN_STRIDE = 1024;
    static constexpr uint32_t MAX_STRIDE = 65536;
    static constexpr uint32_t TARGET_CHECK_MS = 50;
    uint32_t target = hashrate == 0 ? 4096 : static_cast<uint32_t>((static_cast<uint64_t>(hashrate) * TARGET_CHECK_MS) / 1000ULL);
    if (target < MIN_STRIDE) target = MIN_STRIDE;
    if (target > MAX_STRIDE) target = MAX_STRIDE;
    return roundDownPowerOfTwo(target);
  }

  static void copyPreparedWork(const WorkContext& source, WorkContext& target) {
    memcpy(target.header, source.header, sizeof(target.header));
    memcpy(target.paddedHeader, source.paddedHeader, sizeof(target.paddedHeader));
    memcpy(target.midstate, source.midstate, sizeof(target.midstate));
    memcpy(target.bake, source.bake, sizeof(target.bake));
#if defined(CONFIG_IDF_TARGET_ESP32)
    memcpy(target.hwWords, source.hwWords, sizeof(target.hwWords));
#endif
    target.jobSeq = source.jobSeq;
    target.ready = source.ready;
  }

  bool claimDualChunk(WorkContext& work, uint32_t& startNonce, uint32_t& nonceCount,
                      uint32_t& assignmentId, float& poolDifficulty) {
    bool claimed = false;
    portENTER_CRITICAL(&workMux_);
    if (assignmentActive_ && !dualFound_) {
      const uint32_t consumed = dualNextNonce_ - assignmentStartNonce_;
      if (consumed < assignmentNonceCount_) {
        const uint32_t remaining = assignmentNonceCount_ - consumed;
        nonceCount = min<uint32_t>(SLAVE_BATCH_NONCES, remaining);
        startNonce = dualNextNonce_;
        dualNextNonce_ += nonceCount;
        assignmentId = currentAssignmentId_;
        poolDifficulty = assignmentPoolDifficulty_;
        copyPreparedWork(currentWork_, work);
        dualWorkersActive_++;
        claimed = true;
      }
    }
    portEXIT_CRITICAL(&workMux_);
    return claimed;
  }

  bool assignmentStillCurrent(uint32_t assignmentId) {
    bool current = false;
    portENTER_CRITICAL(&workMux_);
    current = assignmentActive_ && currentAssignmentId_ == assignmentId && !dualFound_;
    portEXIT_CRITICAL(&workMux_);
    return current;
  }

  void noteDualProgress(uint32_t assignmentId, uint32_t processed, float bestDifficulty,
                        bool found, uint32_t foundNonce, float foundDifficulty) {
    uint32_t hashes = 0;
    float best = 0.0f;
    uint32_t elapsed = 0;
    bool accepted = false;
    portENTER_CRITICAL(&workMux_);
    if (dualWorkersActive_ > 0) dualWorkersActive_--;
    if (currentAssignmentId_ == assignmentId && (assignmentActive_ || dualFound_)) {
      assignmentHashes_ += processed;
      if (bestDifficulty > assignmentBest_) assignmentBest_ = bestDifficulty;
      if (found && !dualFound_) {
        dualFound_ = true;
        dualFoundNonce_ = foundNonce;
        dualFoundDifficulty_ = foundDifficulty;
      }
      hashes = assignmentHashes_;
      best = assignmentBest_;
      elapsed = millis() - assignmentStartedAtMs_;
      accepted = true;
    }
    portEXIT_CRITICAL(&workMux_);

    if (!accepted || elapsed == 0) return;
    portENTER_CRITICAL(&mux_);
    stats_.hashrate = static_cast<uint32_t>(hashes * 1000ULL / elapsed);
    stats_.assignmentDone = hashes;
    if (best > stats_.bestDifficulty) stats_.bestDifficulty = best;
    portEXIT_CRITICAL(&mux_);
  }

  bool processDualWorkerChunk(bool secondary) {
    WorkContext work;
    uint32_t startNonce = 0;
    uint32_t nonceCount = 0;
    uint32_t assignmentId = 0;
    float poolDifficulty = 0.0f;
    uint32_t cancelCheckMask = 0x3FF;
    if (!claimDualChunk(work, startNonce, nonceCount, assignmentId, poolDifficulty)) {
      return false;
    }
    portENTER_CRITICAL(&workMux_);
    cancelCheckMask = dualCancelCheckMask_;
    portEXIT_CRITICAL(&workMux_);

    uint8_t hash[32];
    uint32_t processed = 0;
    float localBest = 0.0f;
    bool found = false;
    uint32_t foundNonce = 0;
    float foundDifficulty = 0.0f;

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    const bool hardwareWorker = !secondary;
    bool hardwareReady = false;
    if (hardwareWorker) {
      esp_sha_acquire_hardware();
      hardwareReady = validateHardwareS3Unlocked(work, startNonce);
    }
#else
    (void)secondary;
#endif

    for (uint32_t i = 0; i < nonceCount && !stopRequested_; i++) {
      if ((i & cancelCheckMask) == 0 && !assignmentStillCurrent(assignmentId)) break;
      const uint32_t nonce = startNonce + i;
      processed++;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
      const bool candidate = hardwareReady
          ? hashNonceHardwareS3Unlocked(work, nonce, hash)
          : hashNonceSoftwarePrepared(work, nonce, hash);
#else
      const bool candidate = hashNonceSoftwarePrepared(work, nonce, hash);
#endif
      if (candidate) {
        const float diff = difficultyFromHash(hash);
        if (diff > localBest) localBest = diff;
        if (diff > poolDifficulty) {
          found = true;
          foundNonce = nonce;
          foundDifficulty = diff;
          break;
        }
      }
    }

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (hardwareWorker) esp_sha_release_hardware();
#endif

    noteDualProgress(assignmentId, processed, localBest, found, foundNonce, foundDifficulty);
    return processed > 0;
  }

  bool finishDualAssignmentIfReady() {
    bool complete = false;
    bool found = false;
    uint32_t foundNonce = 0;
    float foundDifficulty = 0.0f;

    portENTER_CRITICAL(&workMux_);
    if (assignmentActive_) {
      const uint32_t consumed = dualNextNonce_ - assignmentStartNonce_;
      const bool exhausted = consumed >= assignmentNonceCount_;
      complete = (dualFound_ || exhausted) && dualWorkersActive_ == 0;
      if (complete) {
        found = dualFound_;
        foundNonce = dualFoundNonce_;
        foundDifficulty = dualFoundDifficulty_;
        assignmentActive_ = false;
      }
    }
    portEXIT_CRITICAL(&workMux_);

    if (!complete) return false;
    sendResult(found, foundNonce, foundDifficulty);
    setState(SlaveState::Paired);
    return true;
  }
#endif

  void mineChunk() {
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
    processDualWorkerChunk(false);
    finishDualAssignmentIfReady();
    return;
#else
    uint8_t hash[32];
    uint32_t processed = 0;
#if defined(CONFIG_IDF_TARGET_ESP32)
    esp_sha_lock_engine(SHA2_256);
#endif
    for (uint16_t i = 0; i < SLAVE_BATCH_NONCES && assignmentActive_; i++) {
      const uint32_t offset = assignmentNextNonce_ - assignmentStartNonce_;
      if (offset >= assignmentNonceCount_) {
        assignmentHashes_ += processed;
        assignmentActive_ = false;
#if defined(CONFIG_IDF_TARGET_ESP32)
        esp_sha_unlock_engine(SHA2_256);
#endif
        sendResult(false, 0, 0.0f);
        setState(SlaveState::Paired);
        return;
      }
      const uint32_t nonce = assignmentNextNonce_++;
      processed++;
#if defined(CONFIG_IDF_TARGET_ESP32)
      const bool hashed = hashNonceHardwareEsp32Unlocked(currentWork_, nonce, hash);
#else
      const bool hashed = hashNonce(currentWork_, nonce, hash);
#endif
      if (hashed) {
        const float diff = difficultyFromHash(hash);
        if (diff > assignmentBest_) assignmentBest_ = diff;
        if (diff > assignmentPoolDifficulty_) {
          assignmentHashes_ += processed;
          assignmentActive_ = false;
#if defined(CONFIG_IDF_TARGET_ESP32)
          esp_sha_unlock_engine(SHA2_256);
#endif
          sendResult(true, nonce, diff);
          setState(SlaveState::Paired);
          return;
        }
      }
    }
#if defined(CONFIG_IDF_TARGET_ESP32)
    esp_sha_unlock_engine(SHA2_256);
#endif
    assignmentHashes_ += processed;
    const uint32_t elapsed = millis() - assignmentStartedAtMs_;
    if (elapsed > 0) {
      portENTER_CRITICAL(&mux_);
      stats_.hashrate = static_cast<uint32_t>(assignmentHashes_ * 1000ULL / elapsed);
      stats_.assignmentDone = assignmentHashes_;
      if (assignmentBest_ > stats_.bestDifficulty) stats_.bestDifficulty = assignmentBest_;
      portEXIT_CRITICAL(&mux_);
    }
#if !MICRI_CLUSTER_SLAVE_TASK_ENABLED
    delay(1);
#endif
#endif
  }

  void clearLiveWorkStats() {
    portENTER_CRITICAL(&mux_);
    stats_.hashrate = 0;
    stats_.assignmentSize = 0;
    stats_.assignmentDone = 0;
    stats_.currentAssignmentId = 0;
    stats_.lastResultHashes = 0;
    stats_.lastResultMs = 0;
    portEXIT_CRITICAL(&mux_);
  }

  void setState(SlaveState state) {
    portENTER_CRITICAL(&mux_);
    stats_.state = state;
    if (state != SlaveState::Error) stats_.lastError[0] = '\0';
    portEXIT_CRITICAL(&mux_);
  }

  void setError(const char* message) {
    portENTER_CRITICAL(&mux_);
    stats_.state = SlaveState::Error;
    strncpy(stats_.lastError, message, sizeof(stats_.lastError) - 1);
    stats_.lastError[sizeof(stats_.lastError) - 1] = '\0';
    portEXIT_CRITICAL(&mux_);
  }

  bool radioStarted_ = false;
  bool paired_ = false;
  uint8_t channel_ = 1;
  uint8_t masterPeerChannel_ = 0;
  uint8_t localMac_[6] = {};
  uint8_t masterMac_[6] = {};
  uint32_t clusterId_ = 0;
  uint32_t nextSequence_ = 1;
  uint32_t startedAtMs_ = 0;
  uint32_t lastHopMs_ = 0;
  uint32_t lastMasterSeenMs_ = 0;
  uint32_t lastHelloAtMs_ = 0;
  uint32_t lastStatusAtMs_ = 0;
  volatile bool assignmentActive_ = false;
  bool pendingAssignmentActive_ = false;
  WorkAssignPacket pendingPacket_ = {};
  WorkContext currentWork_;
  uint32_t currentJobSeq_ = 0;
  uint32_t masterSessionId_ = 0;
  uint32_t highestAssignmentIdSeen_ = 0;
  uint32_t currentAssignmentId_ = 0;
  uint32_t assignmentStartNonce_ = 0;
  uint32_t assignmentNonceCount_ = 0;
  uint32_t assignmentNextNonce_ = 0;
  uint32_t assignmentStartedAtMs_ = 0;
  uint32_t assignmentHashes_ = 0;
  uint32_t lastResultSentAtMs_ = 0;
  float assignmentPoolDifficulty_ = 0.0f;
  float assignmentBest_ = 0.0f;
  bool pendingResultActive_ = false;
  WorkResultPacket pendingResultPacket_ = {};
  uint32_t pendingResultCreatedAtMs_ = 0;
  uint32_t pendingResultLastSentAtMs_ = 0;
  uint8_t pendingResultSendAttempts_ = 0;
  SlaveStats stats_;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  QueueHandle_t rxQueue_ = nullptr;
#if MICRI_CLUSTER_SLAVE_TASK_ENABLED
  TaskHandle_t task_ = nullptr;
  volatile bool stopRequested_ = false;
#endif
#if MICRI_CLUSTER_DUAL_SLAVE_WORKER
#if MICRI_CLUSTER_S3_SOFTWARE_WORKER
  TaskHandle_t secondaryTask_ = nullptr;
  volatile bool secondaryStopRequested_ = false;
#endif
  uint8_t dualWorkersActive_ = 0;
  uint32_t dualNextNonce_ = 0;
  bool dualFound_ = false;
  uint32_t dualFoundNonce_ = 0;
  float dualFoundDifficulty_ = 0.0f;
  uint32_t dualCancelCheckMask_ = 0x0FFF;
  portMUX_TYPE workMux_ = portMUX_INITIALIZER_UNLOCKED;
#endif
};

}  // namespace MinerCluster
