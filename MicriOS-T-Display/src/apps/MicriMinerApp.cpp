#include "MicriMinerApp.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_wifi.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include "../shared/logic/WiFiLogic.h"

namespace {
constexpr uint16_t DNS_PORT = 53;
constexpr uint8_t AP_CHANNEL = 6;
constexpr uint16_t PORTAL_LAUNCH_NOTICE_MS = 250;
constexpr uint32_t LIFETIME_SAVE_INTERVAL_MS = 60000;
constexpr uint32_t BTC_PRICE_STALE_MS = 15UL * 60UL * 1000UL;
constexpr const char* HOSTNAME = "MicriDeck-TDisplay";
constexpr const char* STATS_PREF_NS = "miner_stats";
constexpr const char* BTC_PRICE_URL = "https://mempool.space/api/v1/prices";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}

void drawFit(TFT_eSprite& canvas, const String& text, int x, int y, int maxWidth, uint8_t size, uint16_t color) {
  canvas.setTextSize(size);
  canvas.setTextColor(color, TFT_BLACK);
  String clipped = text;
  while (clipped.length() > 0 && canvas.textWidth(clipped) > maxWidth) clipped.remove(clipped.length() - 1);
  canvas.drawString(clipped, x, y);
}

template <typename Canvas>
void drawFitAny(Canvas& canvas, const String& text, int x, int y, int maxWidth, uint8_t size, uint16_t color) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(size);
  canvas.setTextColor(color, TFT_BLACK);
  String clipped = text;
  while (clipped.length() > 0 && canvas.textWidth(clipped) > maxWidth) clipped.remove(clipped.length() - 1);
  canvas.drawString(clipped, x, y);
}

template <typename Canvas>
void drawInfoLine(Canvas& canvas, int y, const char* label, const String& value, uint16_t color = TFT_WHITE) {
  canvas.fillRect(12, y - 2, 216, 13, TFT_BLACK);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(label, 14, y);
  canvas.setTextColor(color, TFT_BLACK);
  String clipped = value;
  const int maxWidth = 150;
  while (clipped.length() > 0 && canvas.textWidth(clipped) > maxWidth) clipped.remove(clipped.length() - 1);
  canvas.drawString(clipped, 76, y);
}

template <typename Canvas>
void drawCenteredValueBand(Canvas& canvas,
                           const String& text,
                           int y,
                           int h,
                           uint8_t maxSize,
                           uint16_t color) {
  canvas.fillRect(0, y, TDisplayUi::W, h, TFT_BLACK);
  uint8_t size = maxSize;
  canvas.setTextDatum(TL_DATUM);
  do {
    canvas.setTextSize(size);
    if (canvas.textWidth(text) <= TDisplayUi::W - TDisplayUi::PAD * 2 || size <= 2) {
      break;
    }
    size--;
  } while (true);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(text, (TDisplayUi::W - canvas.textWidth(text)) / 2, y + 2);
}
}

MicriMinerApp::MicriMinerApp(uint32_t width, uint32_t height)
    : App("Micri Miner", width, height), server_(80), apIP_(AP_IP) {}

bool MicriMinerApp::hasCustomOverlay() const {
  return true;
}

bool MicriMinerApp::startsRunningImmediately() const {
  return true;
}

uint16_t MicriMinerApp::runningRenderIntervalMs() const {
  return mode_ == Mode::Portal ? 500 : 750;
}

bool MicriMinerApp::wantsImmediateRender() const {
  return dirty_;
}

void MicriMinerApp::setDebugAutoStart(bool enabled) {
  debugAutoStart_ = enabled;
}

void MicriMinerApp::setEmitDebugLogs(bool enabled) {
  emitDebugLogs_ = enabled;
}

bool MicriMinerApp::emitDebugLogs() const {
  return emitDebugLogs_;
}

void MicriMinerApp::debugStartMining() {
  if (!miner_.running()) {
    resetSessionPersistMarkers();
    miner_.start(HOSTNAME);
    markDirty();
  }
  debugPrintStats("debug-start");
}

void MicriMinerApp::debugStopMining() {
  if (miner_.running()) {
    persistRuntimeTotals(true);
    miner_.stop();
    markDirty();
  }
  debugPrintStats("debug-stop");
}

void MicriMinerApp::debugPrintStats(const char* reason) {
  printStatsLine(miner_.stats(), reason);
}

void MicriMinerApp::markDirty() {
  dirty_ = true;
}

void MicriMinerApp::forceClear() {
  dirty_ = true;
  forceScreenClear_ = true;
}

void MicriMinerApp::onAppReset() {
  mode_ = Mode::Dashboard;
  page_ = Page::Dashboard;
  message_ = "";
  portalLaunchPending_ = false;
  modeStartedAtMs_ = millis();
  settings_.load();
  loadLifetimeStats();
  resetSessionPersistMarkers();
  stopPortal();
  lastSerialStatsMs_ = 0;
  lastLifetimeSaveMs_ = millis();
  autoStartPending_ = debugAutoStart_;
  forceClear();
}

void MicriMinerApp::onAppExit() {
  persistRuntimeTotals(true);
  stopPortal();
  miner_.stop();
}

void MicriMinerApp::launchPortal() {
  persistRuntimeTotals(true);
  miner_.stop();
  mode_ = Mode::StartingPortal;
  portalLaunchPending_ = true;
  modeStartedAtMs_ = millis();
  forceClear();
}

void MicriMinerApp::startPortal() {
  portalLaunchPending_ = false;
  stopPortal();
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET) ||
      !WiFi.softAP(MinerLogic::SETUP_AP_SSID, MinerLogic::SETUP_AP_PASS, AP_CHANNEL, false, 4)) {
    showMessage("AP failed");
    return;
  }
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
  setupRoutes();
  dns_.start(DNS_PORT, "*", apIP_);
  server_.begin();
  portalRunning_ = true;
  mode_ = Mode::Portal;
  forceClear();
}

void MicriMinerApp::stopPortal() {
  if (portalRunning_) {
    server_.stop();
    dns_.stop();
  }
  portalRunning_ = false;
  if (mode_ == Mode::Portal || mode_ == Mode::StartingPortal) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

void MicriMinerApp::setupRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/reset", HTTP_POST, [this]() { handleReset(); });
  server_.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/fwlink", HTTP_GET, [this]() { handleRoot(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

void MicriMinerApp::handleRoot() {
  server_.send(200, "text/html", buildPage(nullptr));
}

void MicriMinerApp::handleSave() {
  MinerLogic::Config config;
  config.wallet = server_.arg("wallet");
  config.poolHost = server_.arg("host");
  config.poolPass = server_.arg("pass");
  config.poolPort = static_cast<uint16_t>(server_.arg("port").toInt());
  settings_.save(config);
  server_.send(200, "text/html", buildPage("Saved. Return to MicriOS."));
  markDirty();
}

void MicriMinerApp::handleReset() {
  settings_.reset();
  server_.send(200, "text/html", buildPage("Miner settings reset."));
  markDirty();
}

void MicriMinerApp::handleNotFound() {
  server_.sendHeader("Location", "/", true);
  server_.send(302, "text/plain", "");
}

String MicriMinerApp::escapeHtml(const String& value) const {
  String out;
  out.reserve(value.length());
  for (uint16_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else out += c;
  }
  return out;
}

void MicriMinerApp::loadLifetimeStats() {
  Preferences prefs;
  prefs.begin(STATS_PREF_NS, true);
  lifetime_.hashes = prefs.getULong64("hashes", 0);
  lifetime_.seconds = prefs.getULong64("seconds", 0);
  lifetime_.bestDifficulty = prefs.getDouble("bestdiff", 0.0);
  lifetime_.jobs = prefs.getUInt("jobs", 0);
  lifetime_.submitted = prefs.getUInt("submit", 0);
  lifetime_.accepted = prefs.getUInt("accept", 0);
  lifetime_.rejected = prefs.getUInt("reject", 0);
  prefs.end();
}

void MicriMinerApp::saveLifetimeStats() {
  Preferences prefs;
  prefs.begin(STATS_PREF_NS, false);
  prefs.putULong64("hashes", lifetime_.hashes);
  prefs.putULong64("seconds", lifetime_.seconds);
  prefs.putDouble("bestdiff", lifetime_.bestDifficulty);
  prefs.putUInt("jobs", lifetime_.jobs);
  prefs.putUInt("submit", lifetime_.submitted);
  prefs.putUInt("accept", lifetime_.accepted);
  prefs.putUInt("reject", lifetime_.rejected);
  prefs.end();
}

void MicriMinerApp::resetSessionPersistMarkers() {
  persistedSessionHashes_ = 0;
  persistedSessionSeconds_ = 0;
  persistedSessionBestDifficulty_ = 0.0;
  persistedSessionJobs_ = 0;
  persistedSessionSubmitted_ = 0;
  persistedSessionAccepted_ = 0;
  persistedSessionRejected_ = 0;
}

void MicriMinerApp::persistRuntimeTotals(bool force) {
  if (!miner_.running() && !force) return;
  const MinerLogic::Stats stats = miner_.stats();
  bool changed = false;

  if (stats.totalHashes > persistedSessionHashes_) {
    lifetime_.hashes += stats.totalHashes - persistedSessionHashes_;
    persistedSessionHashes_ = stats.totalHashes;
    changed = true;
  }
  if (stats.uptimeSeconds > persistedSessionSeconds_) {
    lifetime_.seconds += stats.uptimeSeconds - persistedSessionSeconds_;
    persistedSessionSeconds_ = stats.uptimeSeconds;
    changed = true;
  }
  if (stats.bestDifficulty > persistedSessionBestDifficulty_) {
    if (stats.bestDifficulty > lifetime_.bestDifficulty) {
      lifetime_.bestDifficulty = stats.bestDifficulty;
    }
    persistedSessionBestDifficulty_ = stats.bestDifficulty;
    changed = true;
  }
  if (stats.jobs > persistedSessionJobs_) {
    lifetime_.jobs += stats.jobs - persistedSessionJobs_;
    persistedSessionJobs_ = stats.jobs;
    changed = true;
  }
  if (stats.submitted > persistedSessionSubmitted_) {
    lifetime_.submitted += stats.submitted - persistedSessionSubmitted_;
    persistedSessionSubmitted_ = stats.submitted;
    changed = true;
  }
  if (stats.accepted > persistedSessionAccepted_) {
    lifetime_.accepted += stats.accepted - persistedSessionAccepted_;
    persistedSessionAccepted_ = stats.accepted;
    changed = true;
  }
  if (stats.rejected > persistedSessionRejected_) {
    lifetime_.rejected += stats.rejected - persistedSessionRejected_;
    persistedSessionRejected_ = stats.rejected;
    changed = true;
  }

  if (changed || force) {
    saveLifetimeStats();
    lastLifetimeSaveMs_ = millis();
  }
}

bool MicriMinerApp::connectWifiForPrice() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFiLogic wifi;
  wifi.loadProfiles();
  bool sawProfile = false;
  snprintf(priceStatus_, sizeof(priceStatus_), "Connecting WiFi...");
  forceClear();

  for (uint8_t slot = 0; slot < WiFiLogic::MAX_PROFILES; slot++) {
    if (!wifi.isProfileUsed(slot)) continue;
    sawProfile = true;
    const String ssid = wifi.getSsid(slot);
    const String pass = wifi.getPass(slot);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(HOSTNAME);
    WiFi.disconnect(false, false);
    delay(30);
    WiFi.begin(ssid.c_str(), pass.c_str());

    const uint32_t started = millis();
    while (millis() - started < 8000) {
      if (WiFi.status() == WL_CONNECTED) {
        return true;
      }
      delay(100);
    }
  }

  snprintf(priceStatus_, sizeof(priceStatus_), sawProfile ? "WiFi failed" : "Use WiFi Setup first");
  return false;
}

bool MicriMinerApp::fetchBtcPrice() {
  if (!connectWifiForPrice()) {
    return false;
  }

  snprintf(priceStatus_, sizeof(priceStatus_), "Fetching...");
  forceClear();

  WiFiClientSecure secure;
  secure.setInsecure();
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(secure, BTC_PRICE_URL)) {
    snprintf(priceStatus_, sizeof(priceStatus_), "Price begin failed");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(priceStatus_, sizeof(priceStatus_), "HTTP %d", code);
    http.end();
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err || !doc["USD"].is<uint32_t>()) {
    snprintf(priceStatus_, sizeof(priceStatus_), "Price parse failed");
    return false;
  }

  btcPriceUsd_ = doc["USD"].as<uint32_t>();
  btcPriceFetchedAtMs_ = millis();
  snprintf(priceStatus_, sizeof(priceStatus_), "mempool.space");
  return true;
}

bool MicriMinerApp::priceIsStale(uint32_t nowMs) const {
  return btcPriceUsd_ == 0 || btcPriceFetchedAtMs_ == 0 || nowMs - btcPriceFetchedAtMs_ >= BTC_PRICE_STALE_MS;
}

void MicriMinerApp::queueBtcPriceFetch() {
  priceFetchPending_ = true;
  snprintf(priceStatus_, sizeof(priceStatus_), "Fetching soon...");
  forceClear();
}

float MicriMinerApp::cpuTemperatureC() const {
#if defined(ESP32)
  return temperatureRead();
#else
  return 0.0f;
#endif
}

String MicriMinerApp::formatHashCount(uint64_t value) const {
  if (value < 1000ULL) return String(static_cast<unsigned long>(value));
  const double v = static_cast<double>(value);
  if (value < 1000000ULL) return String(v / 1000.0, 1) + "K";
  if (value < 1000000000ULL) return String(v / 1000000.0, 1) + "M";
  if (value < 1000000000000ULL) return String(v / 1000000000.0, 2) + "B";
  return String(v / 1000000000000.0, 2) + "T";
}

String MicriMinerApp::formatDuration(uint64_t seconds) const {
  if (seconds < 60ULL) return String(static_cast<unsigned long>(seconds)) + "s";
  const uint64_t minutes = seconds / 60ULL;
  if (minutes < 60ULL) return String(static_cast<unsigned long>(minutes)) + "m " + String(static_cast<unsigned long>(seconds % 60ULL)) + "s";
  const uint64_t hours = minutes / 60ULL;
  if (hours < 24ULL) return String(static_cast<unsigned long>(hours)) + "h " + String(static_cast<unsigned long>(minutes % 60ULL)) + "m";
  const uint64_t days = hours / 24ULL;
  return String(static_cast<unsigned long>(days)) + "d " + String(static_cast<unsigned long>(hours % 24ULL)) + "h";
}

String MicriMinerApp::buildPage(const char* notice) const {
  MinerLogic::MinerSettings s;
  s.load();
  const MinerLogic::Config& config = s.config();
  String html;
  html.reserve(2400);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Micri Miner Setup</title><style>body{font-family:sans-serif;margin:24px;max-width:640px}input{box-sizing:border-box;width:100%;padding:10px;margin:6px 0 14px}button{padding:10px 14px;margin-right:8px}.note{color:#075}.muted{color:#666}</style></head><body>");
  html += F("<h1>Micri Miner Setup</h1>");
  html += F("<p class='muted'>Configure pool and payout wallet. WiFi networks are still configured from MicriOS WiFi Setup.</p>");
  if (notice != nullptr) {
    html += F("<p class='note'>");
    html += notice;
    html += F("</p>");
  }
  html += F("<form method='post' action='/save'><label>Wallet</label><input name='wallet' value='");
  html += escapeHtml(config.wallet);
  html += F("'><label>Pool Host</label><input name='host' value='");
  html += escapeHtml(config.poolHost);
  html += F("'><label>Pool Port</label><input name='port' type='number' value='");
  html += String(config.poolPort);
  html += F("'><label>Pool Password</label><input name='pass' value='");
  html += escapeHtml(config.poolPass);
  html += F("'><button type='submit'>Save</button></form>");
  html += F("<form method='post' action='/reset'><button type='submit'>Reset Miner Settings</button></form>");
  html += F("<p class='muted'>Default worker name is ");
  html += MinerLogic::workerName();
  html += F(".</p></body></html>");
  return html;
}

void MicriMinerApp::showMessage(const char* message) {
  message_ = message;
  mode_ = Mode::Message;
  modeStartedAtMs_ = millis();
  forceClear();
}

void MicriMinerApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  const uint32_t now = millis();
  maybePrintSerialStats(now);
  if (miner_.running() && now - lastLifetimeSaveMs_ >= LIFETIME_SAVE_INTERVAL_MS) {
    persistRuntimeTotals();
  }

  if (mode_ == Mode::StartingPortal && portalLaunchPending_ && now - modeStartedAtMs_ >= PORTAL_LAUNCH_NOTICE_MS) {
    startPortal();
  }

  if (mode_ == Mode::Portal) {
    dns_.processNextRequest();
    server_.handleClient();
    if (b2.click) {
      stopPortal();
      mode_ = Mode::Dashboard;
      forceClear();
    }
    return;
  }

  if (mode_ == Mode::Message) {
    if (b1.click || b2.click || b1.longPress) {
      mode_ = Mode::Dashboard;
      forceClear();
    }
    return;
  }

  if (mode_ == Mode::ConfirmReset) {
    if (b1.longPress) {
      settings_.reset();
      showMessage("Miner reset");
    } else if (b1.click || b2.click) {
      mode_ = Mode::Dashboard;
      forceClear();
    }
    return;
  }

  if (priceFetchPending_ && page_ == Page::Price) {
    priceFetchPending_ = false;
    fetchBtcPrice();
    forceClear();
    return;
  }

  if (b1.longPress) {
    if (page_ == Page::Reset) {
      mode_ = Mode::ConfirmReset;
      forceClear();
    } else {
      launchPortal();
    }
    return;
  }
  if (b2.click) {
    page_ = static_cast<Page>((static_cast<uint8_t>(page_) + 1) % static_cast<uint8_t>(Page::Count));
    if (page_ == Page::Price && priceIsStale(now)) {
      queueBtcPriceFetch();
    } else {
      forceClear();
    }
    return;
  }
  if (b1.click) {
    if (page_ == Page::Price) {
      fetchBtcPrice();
      forceClear();
      return;
    }

    if (miner_.running()) {
      persistRuntimeTotals(true);
      miner_.stop();
    } else {
      if (priceIsStale(now)) {
        fetchBtcPrice();
      }
      resetSessionPersistMarkers();
      miner_.start(HOSTNAME);
    }
    forceClear();
  }
}

void MicriMinerApp::maybePrintSerialStats(uint32_t nowMs) {
  if (!emitDebugLogs_) return;
  if (!miner_.running()) return;
  if (lastSerialStatsMs_ != 0 && nowMs - lastSerialStatsMs_ < 1000) return;
  lastSerialStatsMs_ = nowMs;
  printStatsLine(miner_.stats(), "tick");
}

void MicriMinerApp::printStatsLine(const MinerLogic::Stats& stats, const char* reason) {
  Serial.print("[miner] reason=");
  Serial.print(reason);
  Serial.print(" state=");
  Serial.print(MinerLogic::stateLabel(stats.state));
  Serial.print(" hps=");
  Serial.print(stats.hashesPerSecond);
  Serial.print(" khps=");
  Serial.print(stats.hashesPerSecond / 1000.0f, 1);
  Serial.print(" total_kh=");
  Serial.print(static_cast<unsigned long>(stats.totalHashes / 1000ULL));
  Serial.print(" jobs=");
  Serial.print(stats.jobs);
  Serial.print(" submitted=");
  Serial.print(stats.submitted);
  Serial.print(" accepted=");
  Serial.print(stats.accepted);
  Serial.print(" rejected=");
  Serial.print(stats.rejected);
  Serial.print(" best=");
  Serial.print(stats.bestDifficulty, 8);
  Serial.print(" pool=");
  Serial.print(stats.poolDifficulty, 8);
  Serial.print(" uptime=");
  Serial.print(stats.uptimeSeconds);
  Serial.print(" temp_c=");
  Serial.print(cpuTemperatureC(), 1);
  if (stats.lastError[0]) {
    Serial.print(" error=");
    Serial.print(stats.lastError);
  }
  Serial.println();
}

const char* MicriMinerApp::pageTitle() const {
  switch (page_) {
    case Page::Shares: return "Shares";
    case Page::Session: return "Session";
    case Page::Lifetime: return "Lifetime";
    case Page::Pool: return "Pool";
    case Page::Price: return "BTC Price";
    case Page::Settings: return "Miner Setup";
    case Page::Reset: return "Reset Miner";
    case Page::Dashboard:
    default: return "Micri Miner";
  }
}

uint16_t MicriMinerApp::stateColor(MinerLogic::State state) const {
  switch (state) {
    case MinerLogic::State::Mining: return TFT_GREEN;
    case MinerLogic::State::Idle: return TFT_CYAN;
    case MinerLogic::State::NoWifi:
    case MinerLogic::State::WifiFailed:
    case MinerLogic::State::PoolFailed:
    case MinerLogic::State::Error: return TFT_RED;
    case MinerLogic::State::Stopping: return TFT_ORANGE;
    default: return TFT_YELLOW;
  }
}

template <typename Canvas>
void MicriMinerApp::drawDashboard(Canvas& canvas, const MinerLogic::Stats& stats) {
  const uint16_t color = stateColor(stats.state);
  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), color, page.c_str());
  const float kh = stats.hashesPerSecond / 1000.0f;
  char rate[18];
  if (stats.hashesPerSecond >= 1000) snprintf(rate, sizeof(rate), "%.1f KH/s", kh);
  else snprintf(rate, sizeof(rate), "%lu H/s", static_cast<unsigned long>(stats.hashesPerSecond));
  drawCenteredValueBand(canvas, rate, 40, 39, 5, color);
  canvas.fillRect(12, 82, 216, 34, TFT_BLACK);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String("State ") + MinerLogic::stateLabel(stats.state), 18, 84);
  canvas.drawString(String("Pool jobs ") + String(stats.jobs), 18, 99);
  canvas.drawString(String("Shares OK ") + String(stats.accepted) + " Rej " + String(stats.rejected), 114, 99);
  TDisplayUi::footer(canvas, miner_.running() ? "B1 stop  B2 page  B1 hold setup" : "B1 start  B2 page  B1 hold setup");
}

template <typename Canvas>
void MicriMinerApp::drawShares(Canvas& canvas, const MinerLogic::Stats& stats) {
  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), TFT_GREEN, page.c_str());
  drawInfoLine(canvas, 38, "Pool jobs", String(stats.jobs), TFT_CYAN);
  drawInfoLine(canvas, 55, "Shares out", String(stats.submitted), TFT_WHITE);
  drawInfoLine(canvas, 72, "Shares OK", String(stats.accepted), TFT_GREEN);
  drawInfoLine(canvas, 89, "Shares rej", String(stats.rejected), stats.rejected == 0 ? TFT_LIGHTGREY : TFT_RED);
  drawInfoLine(canvas, 106, "Best diff", String(stats.bestDifficulty, 6), TFT_YELLOW);
  TDisplayUi::footer(canvas, "B1 start/stop  B2 page");
}

template <typename Canvas>
void MicriMinerApp::drawSession(Canvas& canvas, const MinerLogic::Stats& stats) {
  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), TFT_CYAN, page.c_str());
  drawInfoLine(canvas, 36, "Hashes", formatHashCount(stats.totalHashes), TFT_CYAN);
  drawInfoLine(canvas, 53, "Raw", String(static_cast<unsigned long>(stats.totalHashes / 1000ULL)) + "K", TFT_LIGHTGREY);
  drawInfoLine(canvas, 70, "Runtime", formatDuration(stats.uptimeSeconds), TFT_GREEN);
  drawInfoLine(canvas, 87, "Rate", String(stats.hashesPerSecond / 1000.0f, 1) + " KH/s", stateColor(stats.state));
  drawInfoLine(canvas, 104, "CPU temp", String(cpuTemperatureC(), 1) + " C", TFT_ORANGE);
  TDisplayUi::footer(canvas, "B1 start/stop  B2 page");
}

template <typename Canvas>
void MicriMinerApp::drawLifetime(Canvas& canvas, const MinerLogic::Stats& stats) {
  LifetimeStats current = lifetime_;
  if (stats.totalHashes > persistedSessionHashes_) current.hashes += stats.totalHashes - persistedSessionHashes_;
  if (stats.uptimeSeconds > persistedSessionSeconds_) current.seconds += stats.uptimeSeconds - persistedSessionSeconds_;
  if (stats.jobs > persistedSessionJobs_) current.jobs += stats.jobs - persistedSessionJobs_;
  if (stats.submitted > persistedSessionSubmitted_) current.submitted += stats.submitted - persistedSessionSubmitted_;
  if (stats.accepted > persistedSessionAccepted_) current.accepted += stats.accepted - persistedSessionAccepted_;
  if (stats.rejected > persistedSessionRejected_) current.rejected += stats.rejected - persistedSessionRejected_;
  if (stats.bestDifficulty > current.bestDifficulty) current.bestDifficulty = stats.bestDifficulty;

  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), TFT_ORANGE, page.c_str());
  drawInfoLine(canvas, 38, "Hashes", formatHashCount(current.hashes), TFT_ORANGE);
  drawInfoLine(canvas, 55, "Duration", formatDuration(current.seconds), TFT_GREEN);
  drawInfoLine(canvas, 72, "Best diff", String(current.bestDifficulty, 6), TFT_YELLOW);
  drawInfoLine(canvas, 89, "Jobs", String(current.jobs), TFT_CYAN);
  drawInfoLine(canvas, 106, "OK/Rej", String(current.accepted) + "/" + String(current.rejected), current.rejected == 0 ? TFT_GREEN : TFT_RED);
  TDisplayUi::footer(canvas, "Saved every 60s/stop  B2 page");
}

template <typename Canvas>
void MicriMinerApp::drawPool(Canvas& canvas, const MinerLogic::Stats& stats) {
  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), stateColor(stats.state), page.c_str());
  drawInfoLine(canvas, 38, "WiFi", stats.ssid[0] ? String(stats.ssid) : String("-"), TFT_CYAN);
  drawInfoLine(canvas, 55, "State", MinerLogic::stateLabel(stats.state), stateColor(stats.state));
  drawInfoLine(canvas, 72, "Pool diff", String(stats.poolDifficulty, 6), TFT_YELLOW);
  drawInfoLine(canvas, 89, "Best diff", String(stats.bestDifficulty, 6), TFT_GREEN);
  if (stats.retryInSeconds > 0) {
    drawFitAny(canvas, String("Retry ") + stats.retryCount + " in " + stats.retryInSeconds + "s", 14, 108, 210, 1, TFT_ORANGE);
  } else if (stats.lastError[0]) {
    drawFitAny(canvas, stats.lastError, 14, 108, 210, 1, TFT_RED);
  } else {
    drawInfoLine(canvas, 106, "Error", "-", TFT_LIGHTGREY);
  }
  TDisplayUi::footer(canvas, "B1 start/stop  B2 page");
}

template <typename Canvas>
void MicriMinerApp::drawPrice(Canvas& canvas) {
  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), TFT_YELLOW, page.c_str());
  if (btcPriceUsd_ > 0) {
    drawCenteredValueBand(canvas, String("$") + btcPriceUsd_, 40, 40, 5, TFT_YELLOW);
    const uint32_t age = btcPriceFetchedAtMs_ == 0 ? 0 : (millis() - btcPriceFetchedAtMs_) / 1000;
    drawInfoLine(canvas, 94, "Age", formatDuration(age), TFT_LIGHTGREY);
  } else {
    TDisplayUi::centered(canvas, "No price yet", 54, 2, TFT_LIGHTGREY);
  }
  drawFitAny(canvas, priceStatus_, 14, 108, 212, 1, btcPriceUsd_ > 0 ? TFT_CYAN : TFT_ORANGE);
  TDisplayUi::footer(canvas, "Auto fetch  B1 retry  B2 page");
}

template <typename Canvas>
void MicriMinerApp::drawSettings(Canvas& canvas) {
  settings_.load();
  const MinerLogic::Config& config = settings_.config();
  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), TFT_ORANGE, page.c_str());
  drawFitAny(canvas, String("Wallet: ") + config.wallet, 14, 36, 212, 1, TFT_WHITE);
  drawFitAny(canvas, String("Pool: ") + config.poolHost + ":" + config.poolPort, 14, 54, 212, 1, TFT_CYAN);
  drawFitAny(canvas, String("Worker: ") + MinerLogic::workerName(), 14, 72, 212, 1, TFT_GREEN);
  drawFitAny(canvas, String("User: ") + MinerLogic::stratumUser(config), 14, 90, 212, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 hold setup AP  B2 page");
}

template <typename Canvas>
void MicriMinerApp::drawReset(Canvas& canvas) {
  const String page = String(static_cast<uint8_t>(page_) + 1) + "/" + static_cast<uint8_t>(Page::Count);
  TDisplayUi::header(canvas, pageTitle(), TFT_RED, page.c_str());
  TDisplayUi::centered(canvas, "Reset wallet", 42, 2, TFT_WHITE);
  TDisplayUi::centered(canvas, "and pool settings?", 68, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 hold confirm  B2 page");
}

template <typename Canvas>
void MicriMinerApp::drawPortal(Canvas& canvas) {
  TDisplayUi::header(canvas, mode_ == Mode::StartingPortal ? "Miner Setup" : "Setup AP", TFT_ORANGE);
  if (mode_ == Mode::StartingPortal) {
    TDisplayUi::centered(canvas, "Launching", 46, 2, TFT_YELLOW);
    TDisplayUi::centered(canvas, "Miner Setup AP", 73, 2, TFT_ORANGE);
  } else {
    TDisplayUi::centered(canvas, MinerLogic::SETUP_AP_SSID, 42, 2, TFT_ORANGE);
    TDisplayUi::centered(canvas, "Pass: micriminer", 70, 1, TFT_LIGHTGREY);
    TDisplayUi::centered(canvas, "Open 192.168.4.1", 88, 1, TFT_CYAN);
  }
  TDisplayUi::footer(canvas, "B2 back");
}

template <typename Canvas>
void MicriMinerApp::drawMessage(Canvas& canvas) {
  TDisplayUi::header(canvas, "Micri Miner", TFT_YELLOW);
  TDisplayUi::largeValue(canvas, message_, 54, TFT_YELLOW);
  TDisplayUi::footer(canvas, "B1/B2 continue");
}

template <typename Canvas>
void MicriMinerApp::drawConfirmReset(Canvas& canvas) {
  TDisplayUi::header(canvas, "Are You Sure?", TFT_RED);
  TDisplayUi::centered(canvas, "Hold B1", 47, 3, TFT_RED);
  TDisplayUi::centered(canvas, "to reset", 81, 2, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 hold yes  B2 no");
}

template <typename Canvas>
void MicriMinerApp::drawFrame(Canvas& canvas) {
  TDisplayUi::clear(canvas);
  if (mode_ == Mode::StartingPortal || mode_ == Mode::Portal) {
    drawPortal(canvas);
    return;
  }
  if (mode_ == Mode::Message) {
    drawMessage(canvas);
    return;
  }
  if (mode_ == Mode::ConfirmReset) {
    drawConfirmReset(canvas);
    return;
  }
  const MinerLogic::Stats stats = miner_.stats();
  switch (page_) {
    case Page::Shares:
      drawShares(canvas, stats);
      break;
    case Page::Session:
      drawSession(canvas, stats);
      break;
    case Page::Lifetime:
      drawLifetime(canvas, stats);
      break;
    case Page::Pool:
      drawPool(canvas, stats);
      break;
    case Page::Price:
      drawPrice(canvas);
      break;
    case Page::Settings:
      drawSettings(canvas);
      break;
    case Page::Reset:
      drawReset(canvas);
      break;
    case Page::Dashboard:
    default:
      drawDashboard(canvas, stats);
      break;
  }
}

void MicriMinerApp::drawRunning(TFT_eSPI& tft) {
  tft.setRotation(1);
  if (forceScreenClear_) {
    forceScreenClear_ = false;
  }
  drawBuffered(tft, width, height, [this](auto& canvas) { drawFrame(canvas); });
  dirty_ = false;
  if (autoStartPending_) {
    autoStartPending_ = false;
    if (priceIsStale(millis())) {
      fetchBtcPrice();
    }
    resetSessionPersistMarkers();
    miner_.start(HOSTNAME);
    forceClear();
  }
}

void MicriMinerApp::drawStart(TFT_eSPI& tft) {
  tft.setRotation(1);
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Micri Miner", TFT_ORANGE);
    TDisplayUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    TDisplayUi::centered(tft, "to Start", 80, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 start");
  } else if (showStartScorePage()) {
    TDisplayUi::header(tft, "Solo Miner", TFT_YELLOW);
    TDisplayUi::centered(tft, "public-pool.io", 49, 2, TFT_CYAN);
    TDisplayUi::centered(tft, "Wallet configurable", 78, 1, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Settings via portal");
  } else {
    TDisplayUi::header(tft, "Micri Miner", TFT_ORANGE);
    tft.drawCircle(120, 66, 28, TFT_ORANGE);
    tft.drawCircle(120, 66, 18, TFT_YELLOW);
    tft.drawString("BTC", 104, 59, 2);
    TDisplayUi::footer(tft, "B1 start");
  }
}
