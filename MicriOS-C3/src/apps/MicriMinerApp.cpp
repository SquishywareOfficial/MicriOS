#include "MicriMinerApp.h"

#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace {
constexpr uint16_t DNS_PORT = 53;
constexpr uint8_t AP_CHANNEL = 6;
constexpr uint16_t PORTAL_LAUNCH_NOTICE_MS = 250;
constexpr uint32_t BEST_SAVE_INTERVAL_MS = 60000;
constexpr const char* HOSTNAME = "MicriDeck-C3";
constexpr const char* STATS_PREF_NS = "miner_stats";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);
}

MicriMinerApp::MicriMinerApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Micri Miner", width, height), server_(80), apIP_(AP_IP), left_(left) {}

bool MicriMinerApp::hasCustomOverlay() const {
  return true;
}

uint16_t MicriMinerApp::runningRenderIntervalMs() const {
  return mode_ == Mode::Portal ? 500 : 1000;
}

bool MicriMinerApp::startsRunningImmediately() const {
  return true;
}

void MicriMinerApp::onAppReset() {
  mode_ = Mode::Pages;
  page_ = 0;
  message_ = "";
  portalLaunchPending_ = false;
  modeStartedAtMs_ = millis();
  settings_.load();
  loadBestDifficulty();
  stopPortal();
  lastBestSaveMs_ = millis();
}

void MicriMinerApp::onAppExit() {
  persistBestDifficulty(true);
  stopPortal();
  miner_.stop();
}

void MicriMinerApp::launchPortal() {
  persistBestDifficulty(true);
  miner_.stop();
  mode_ = Mode::StartingPortal;
  portalLaunchPending_ = true;
  modeStartedAtMs_ = millis();
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
  server_.send(200, "text/html", buildPage("Saved."));
}

void MicriMinerApp::handleReset() {
  settings_.reset();
  server_.send(200, "text/html", buildPage("Reset."));
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

String MicriMinerApp::buildPage(const char* notice) const {
  MinerLogic::MinerSettings s;
  s.load();
  const MinerLogic::Config& config = s.config();
  String html;
  html.reserve(2200);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Micri Miner Setup</title><style>body{font-family:sans-serif;margin:24px;max-width:640px}input{box-sizing:border-box;width:100%;padding:10px;margin:6px 0 14px}button{padding:10px 14px}.note{color:#075}.muted{color:#666}</style></head><body>");
  html += F("<h1>Micri Miner Setup</h1>");
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
  html += F("<p class='muted'>Worker: ");
  html += MinerLogic::workerName();
  html += F("</p></body></html>");
  return html;
}

void MicriMinerApp::showMessage(const char* message) {
  message_ = message;
  mode_ = Mode::Message;
  modeStartedAtMs_ = millis();
}

void MicriMinerApp::loadBestDifficulty() {
  Preferences prefs;
  prefs.begin(STATS_PREF_NS, true);
  lifetimeBestDifficulty_ = prefs.getDouble("bestdiff", 0.0);
  prefs.end();
  savedBestDifficulty_ = lifetimeBestDifficulty_;
}

void MicriMinerApp::saveBestDifficulty() {
  Preferences prefs;
  prefs.begin(STATS_PREF_NS, false);
  prefs.putDouble("bestdiff", lifetimeBestDifficulty_);
  prefs.end();
  savedBestDifficulty_ = lifetimeBestDifficulty_;
  lastBestSaveMs_ = millis();
}

void MicriMinerApp::persistBestDifficulty(bool force) {
  const MinerLogic::Stats stats = miner_.stats();
  if (stats.bestDifficulty > lifetimeBestDifficulty_) {
    lifetimeBestDifficulty_ = stats.bestDifficulty;
  }
  if (force || (lifetimeBestDifficulty_ > savedBestDifficulty_ && millis() - lastBestSaveMs_ >= BEST_SAVE_INTERVAL_MS)) {
    saveBestDifficulty();
  }
}

void MicriMinerApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  const uint32_t now = millis();

  if (mode_ == Mode::StartingPortal && portalLaunchPending_ && now - modeStartedAtMs_ >= PORTAL_LAUNCH_NOTICE_MS) {
    startPortal();
  }

  if (mode_ == Mode::Portal) {
    dns_.processNextRequest();
    server_.handleClient();
    if (input.longPress) {
      stopPortal();
      mode_ = Mode::Pages;
    }
    return;
  }

  if (mode_ == Mode::Message) {
    if (input.click || input.longPress) {
      mode_ = Mode::Pages;
    }
    return;
  }

  if (miner_.running()) {
    persistBestDifficulty(false);
  }

  if (input.click) {
    page_ = (page_ + 1) % 5;
    return;
  }

  if (input.longPress) {
    if (page_ == 0) {
      if (miner_.running()) {
        persistBestDifficulty(true);
        miner_.stop();
      }
      else miner_.start(HOSTNAME);
    } else if (page_ == 2) {
      launchPortal();
    } else if (page_ == 3) {
      settings_.reset();
      showMessage("Miner reset");
    } else if (page_ == 4) {
      requestExitToMenu();
    }
  }
}

void MicriMinerApp::drawFit(U8G2& u8g2, int x, int y, const char* text) const {
  u8g2.setFont(u8g2_font_4x6_tr);
  char buf[24];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  while (strlen(buf) > 0 && u8g2.getStrWidth(buf) > static_cast<int>(width - x - 2)) {
    buf[strlen(buf) - 1] = '\0';
  }
  u8g2.drawStr(x, y, buf);
}

void MicriMinerApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 2, 8, "Micri Miner");
  u8g2.setFont(u8g2_font_4x6_tr);

  if (mode_ == Mode::StartingPortal) {
    u8g2.drawStr(left_ + 4, 20, "Starting AP");
    u8g2.drawStr(left_ + 4, 31, "Please wait");
    return;
  }
  if (mode_ == Mode::Portal) {
    u8g2.drawStr(left_ + 4, 18, "AP:");
    drawFit(u8g2, left_ + 20, 18, MinerLogic::SETUP_AP_SSID);
    u8g2.drawStr(left_ + 4, 28, "192.168.4.1");
    u8g2.drawStr(left_ + 4, 38, "Hold back");
    return;
  }
  if (mode_ == Mode::Message) {
    drawFit(u8g2, left_ + 4, 24, message_);
    u8g2.drawStr(left_ + 4, 38, "Tap continue");
    return;
  }

  const MinerLogic::Stats stats = miner_.stats();
  const double bestDifficulty = stats.bestDifficulty > lifetimeBestDifficulty_ ? stats.bestDifficulty : lifetimeBestDifficulty_;
  if (page_ == 0) {
    drawFit(u8g2, left_ + 4, 18, MinerLogic::stateLabel(stats.state));
    char rate[18];
    if (stats.hashesPerSecond >= 1000) snprintf(rate, sizeof(rate), "%lu KH/s", static_cast<unsigned long>(stats.hashesPerSecond / 1000));
    else snprintf(rate, sizeof(rate), "%lu H/s", static_cast<unsigned long>(stats.hashesPerSecond));
    drawFit(u8g2, left_ + 4, 28, rate);
    u8g2.drawStr(left_ + 4, 38, miner_.running() ? "Hold stop" : "Hold start");
  } else if (page_ == 1) {
    char buf[24];
    snprintf(buf, sizeof(buf), "Jobs %lu", static_cast<unsigned long>(stats.jobs));
    drawFit(u8g2, left_ + 4, 18, buf);
    snprintf(buf, sizeof(buf), "OK %lu R %lu", static_cast<unsigned long>(stats.accepted), static_cast<unsigned long>(stats.rejected));
    drawFit(u8g2, left_ + 4, 28, buf);
    drawFit(u8g2, left_ + 4, 38, (String("Best ") + String(bestDifficulty, 3)).c_str());
  } else if (page_ == 2) {
    settings_.load();
    const MinerLogic::Config& config = settings_.config();
    drawFit(u8g2, left_ + 4, 18, config.poolHost.c_str());
    drawFit(u8g2, left_ + 4, 28, MinerLogic::workerName().c_str());
    u8g2.drawStr(left_ + 4, 38, "Hold setup");
  } else if (page_ == 3) {
    u8g2.drawStr(left_ + 4, 18, "Reset");
    u8g2.drawStr(left_ + 4, 28, "settings?");
    u8g2.drawStr(left_ + 4, 38, "Hold yes");
  } else {
    u8g2.drawStr(left_ + 4, 22, "Exit miner?");
    u8g2.drawStr(left_ + 4, 38, "Hold exit");
  }
}

void MicriMinerApp::drawStart(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 4, 10, "Micri");
  u8g2.drawStr(left_ + 4, 21, "Miner");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 4, 38, "Tap start");
}
