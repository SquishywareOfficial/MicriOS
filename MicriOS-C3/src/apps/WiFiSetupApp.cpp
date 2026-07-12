#include "WiFiSetupApp.h"

#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace {
constexpr uint8_t WIFI_LED_PIN = 8;
constexpr bool WIFI_LED_ACTIVE_LOW = true;
constexpr uint16_t DNS_PORT = 53;
constexpr uint16_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint16_t FAIL_BLINK_MS = 300;
constexpr uint16_t PORTAL_BLINK_MS = 1000;
constexpr uint8_t AP_CHANNEL = 6;
constexpr const char* AP_SSID = "MicriDeck-C3";
constexpr const char* AP_PASS = "micrideck";
constexpr const char* STA_HOSTNAME = "ESP32C3-Test";
constexpr const char* PREF_NS = "wifi";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const char* const MAIN_ITEMS[] = {
    "Configure WiFi",
    "Test Saved",
    "Delete Saved",
    "Exit",
};
constexpr uint8_t MAIN_COUNT = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);
}

WiFiSetupApp::WiFiSetupApp(uint32_t width, uint32_t height, uint32_t left)
    : App("WiFi Settings", width, height), left_(left), server_(80), apIP_(AP_IP) {}

bool WiFiSetupApp::hasCustomOverlay() const {
  return true;
}

bool WiFiSetupApp::startsRunningImmediately() const {
  return true;
}

void WiFiSetupApp::onAppReset() {
  pinMode(WIFI_LED_PIN, OUTPUT);
  setSetupLed(false);
  mode_ = Mode::Main;
  mainIndex_ = 0;
  profileMenuIndex_ = 0;
  deleteProfileIndex_ = NO_PROFILE;
  deleteAll_ = false;
  testState_ = TestState::Idle;
  portalState_ = PortalState::Off;
  stopPortal();
  stopStationTest(true);
  loadProfiles();
}

void WiFiSetupApp::startPortal() {
  stopPortal();
  stopStationTest(false);

  portalState_ = PortalState::Starting;
  networkCount_ = 0;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  loadProfiles();
  scanNetworks();

  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET) || !WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, false, 4)) {
    portalState_ = PortalState::Error;
    return;
  }
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);

  setupRoutes();
  dns_.start(DNS_PORT, "*", apIP_);
  server_.begin();
  portalRunning_ = true;
  portalState_ = PortalState::Running;
}

void WiFiSetupApp::stopPortal() {
  if (portalRunning_) {
    server_.stop();
    dns_.stop();
  }
  portalRunning_ = false;
  if (portalState_ != PortalState::Off) {
    WiFi.softAPdisconnect(true);
  }
  portalState_ = PortalState::Off;
}

void WiFiSetupApp::stopStationTest(bool powerDown) {
  testingAll_ = false;
  testProfileIndex_ = NO_PROFILE;
  if (powerDown) {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
  } else {
    WiFi.disconnect(false, false);
  }
  testState_ = TestState::Idle;
  ledBlinkOn_ = false;
  ledTimerMs_ = 0;
  setSetupLed(false);
}

void WiFiSetupApp::setupRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/fwlink", HTTP_GET, [this]() { handleRoot(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

void WiFiSetupApp::scanNetworks() {
  networkCount_ = 0;
  const int found = WiFi.scanNetworks(false, true);
  if (found <= 0) {
    WiFi.scanDelete();
    return;
  }

  for (int i = 0; i < found && networkCount_ < MAX_NETWORKS; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) {
      continue;
    }

    bool duplicate = false;
    for (uint8_t existing = 0; existing < networkCount_; existing++) {
      if (networkNames_[existing] == ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }

    networkNames_[networkCount_] = ssid;
    networkRssi_[networkCount_] = WiFi.RSSI(i);
    networkCount_++;
  }
  WiFi.scanDelete();
}

void WiFiSetupApp::loadProfiles() {
  profileCount_ = 0;
  prefs_.begin(PREF_NS, true);
  for (uint8_t i = 0; i < MAX_PROFILES; i++) {
    savedSsids_[i] = prefs_.getString(prefKey("ssid", i).c_str(), "");
    profileUsed_[i] = savedSsids_[i].length() > 0;
    if (profileUsed_[i]) {
      profileCount_++;
    }
  }
  prefs_.end();
}

int8_t WiFiSetupApp::findProfile(const String& ssid) const {
  for (uint8_t i = 0; i < MAX_PROFILES; i++) {
    if (profileUsed_[i] && savedSsids_[i] == ssid) {
      return i;
    }
  }
  return -1;
}

int8_t WiFiSetupApp::firstEmptyProfile() const {
  for (uint8_t i = 0; i < MAX_PROFILES; i++) {
    if (!profileUsed_[i]) {
      return i;
    }
  }
  return -1;
}

int8_t WiFiSetupApp::saveProfile(const String& ssid, const String& pass) {
  int8_t slot = findProfile(ssid);
  if (slot < 0) {
    slot = firstEmptyProfile();
  }
  if (slot < 0) {
    slot = 0;
  }

  prefs_.begin(PREF_NS, false);
  prefs_.putString(prefKey("ssid", slot).c_str(), ssid);
  prefs_.putString(prefKey("pass", slot).c_str(), pass);
  prefs_.end();

  loadProfiles();
  return slot;
}

void WiFiSetupApp::forgetProfile(uint8_t index) {
  if (index >= MAX_PROFILES) {
    return;
  }
  prefs_.begin(PREF_NS, false);
  prefs_.remove(prefKey("ssid", index).c_str());
  prefs_.remove(prefKey("pass", index).c_str());
  prefs_.end();
  loadProfiles();
}

void WiFiSetupApp::forgetAllProfiles() {
  prefs_.begin(PREF_NS, false);
  prefs_.clear();
  prefs_.end();
  loadProfiles();
}

bool WiFiSetupApp::beginProfileTest(uint8_t index, bool testAll) {
  if (index >= MAX_PROFILES || !profileUsed_[index]) {
    testState_ = TestState::Failed;
    return false;
  }

  prefs_.begin(PREF_NS, true);
  const String pass = prefs_.getString(prefKey("pass", index).c_str(), "");
  prefs_.end();

  stopPortal();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  WiFi.setHostname(STA_HOSTNAME);
  WiFi.begin(savedSsids_[index].c_str(), pass.c_str());
  connectStartedAtMs_ = millis();
  testProfileIndex_ = index;
  testingAll_ = testAll;
  testState_ = TestState::Connecting;
  return true;
}

bool WiFiSetupApp::beginNextProfileTest(uint8_t startIndex) {
  for (uint8_t i = startIndex; i < MAX_PROFILES; i++) {
    if (profileUsed_[i]) {
      return beginProfileTest(i, true);
    }
  }
  testingAll_ = false;
  testProfileIndex_ = NO_PROFILE;
  testState_ = TestState::Failed;
  return false;
}

uint8_t WiFiSetupApp::profileMenuCount() const {
  return profileCount_ + 2;
}

int8_t WiFiSetupApp::profileSlotForPosition(uint8_t position) const {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < MAX_PROFILES; i++) {
    if (!profileUsed_[i]) {
      continue;
    }
    if (seen == position) {
      return i;
    }
    seen++;
  }
  return -1;
}

void WiFiSetupApp::showMessage(const char* message, Mode returnMode) {
  message_ = message;
  messageReturnMode_ = returnMode;
  mode_ = Mode::Message;
}

void WiFiSetupApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  statusTimerMs_ += deltaMs;

  if (mode_ == Mode::ConfigurePortal && portalRunning_) {
    dns_.processNextRequest();
    server_.handleClient();
  }

  if (mode_ == Mode::Main) {
    if (input.click) {
      mainIndex_ = (mainIndex_ + 1) % MAIN_COUNT;
    }
    if (input.longPress) {
      if (mainIndex_ == 0) {
        startPortal();
        mode_ = Mode::ConfigurePortal;
      } else if (mainIndex_ == 1) {
        loadProfiles();
        if (profileCount_ == 0) {
          showMessage("No saved WiFi", Mode::Main);
        } else {
          profileMenuIndex_ = 0;
          mode_ = Mode::TestSelect;
        }
      } else if (mainIndex_ == 2) {
        loadProfiles();
        if (profileCount_ == 0) {
          showMessage("No saved WiFi", Mode::Main);
        } else {
          profileMenuIndex_ = 0;
          mode_ = Mode::DeleteSelect;
        }
      } else {
        stopPortal();
        stopStationTest(true);
        requestExitToMenu();
      }
    }
  } else if (mode_ == Mode::ConfigurePortal) {
    if (input.click) {
      scanNetworks();
    }
    if (input.longPress) {
      stopPortal();
      loadProfiles();
      mode_ = Mode::Main;
    }
  } else if (mode_ == Mode::TestSelect) {
    if (input.click) {
      profileMenuIndex_ = (profileMenuIndex_ + 1) % profileMenuCount();
    }
    if (input.longPress) {
      if (profileMenuIndex_ == profileCount_) {
        beginNextProfileTest(0);
        mode_ = Mode::Testing;
      } else if (profileMenuIndex_ == profileCount_ + 1) {
        mode_ = Mode::Main;
      } else {
        const int8_t slot = profileSlotForPosition(profileMenuIndex_);
        if (slot >= 0) {
          beginProfileTest(slot, false);
          mode_ = Mode::Testing;
        }
      }
    }
  } else if (mode_ == Mode::Testing) {
    updateConnectionTest(deltaMs);
    if (testState_ == TestState::Connecting) {
      if (input.longPress) {
        stopStationTest(true);
        mode_ = Mode::TestSelect;
      }
    } else if (input.click) {
      mode_ = Mode::TestSelect;
    } else if (input.longPress) {
      stopStationTest(true);
      mode_ = Mode::Main;
    }
  } else if (mode_ == Mode::DeleteSelect) {
    if (input.click) {
      profileMenuIndex_ = (profileMenuIndex_ + 1) % profileMenuCount();
    }
    if (input.longPress) {
      if (profileMenuIndex_ == profileCount_) {
        deleteAll_ = true;
        deleteProfileIndex_ = NO_PROFILE;
        mode_ = Mode::ConfirmDelete;
      } else if (profileMenuIndex_ == profileCount_ + 1) {
        mode_ = Mode::Main;
      } else {
        const int8_t slot = profileSlotForPosition(profileMenuIndex_);
        if (slot >= 0) {
          deleteAll_ = false;
          deleteProfileIndex_ = slot;
          mode_ = Mode::ConfirmDelete;
        }
      }
    }
  } else if (mode_ == Mode::ConfirmDelete) {
    if (input.click) {
      mode_ = Mode::DeleteSelect;
    }
    if (input.longPress) {
      if (deleteAll_) {
        forgetAllProfiles();
        showMessage("All WiFi erased", Mode::Main);
      } else {
        forgetProfile(deleteProfileIndex_);
        showMessage("WiFi erased", Mode::DeleteSelect);
      }
    }
  } else if (mode_ == Mode::Message) {
    if (input.click || input.longPress) {
      mode_ = messageReturnMode_;
    }
  }

  if (mode_ != Mode::Testing) {
    updateLed(deltaMs);
  }
}

void WiFiSetupApp::updateConnectionTest(uint32_t deltaMs) {
  if (statusTimerMs_ >= 500) {
    statusTimerMs_ = 0;
    if (testState_ == TestState::Connecting && WiFi.status() == WL_CONNECTED) {
      testState_ = TestState::Connected;
      testingAll_ = false;
    } else if (testState_ == TestState::Connecting && (millis() - connectStartedAtMs_) > CONNECT_TIMEOUT_MS) {
      if (testingAll_) {
        beginNextProfileTest(testProfileIndex_ + 1);
      } else {
        testState_ = TestState::Failed;
      }
    }
  }
  updateLed(deltaMs);
}

void WiFiSetupApp::updateLed(uint32_t deltaMs) {
  if (testState_ == TestState::Connected) {
    setSetupLed(true);
    return;
  }

  if (testState_ == TestState::Failed || portalState_ == PortalState::Error) {
    ledTimerMs_ += deltaMs;
    if (ledTimerMs_ >= FAIL_BLINK_MS) {
      ledTimerMs_ = 0;
      ledBlinkOn_ = !ledBlinkOn_;
    }
    setSetupLed(ledBlinkOn_);
    return;
  }

  if (portalState_ == PortalState::Running) {
    ledTimerMs_ += deltaMs;
    if (ledTimerMs_ >= PORTAL_BLINK_MS) {
      ledTimerMs_ = 0;
      ledBlinkOn_ = !ledBlinkOn_;
    }
    setSetupLed(ledBlinkOn_);
    return;
  }

  ledBlinkOn_ = false;
  ledTimerMs_ = 0;
  setSetupLed(false);
}

void WiFiSetupApp::setSetupLed(bool on) {
  digitalWrite(WIFI_LED_PIN, WIFI_LED_ACTIVE_LOW ? !on : on);
}

void WiFiSetupApp::handleRoot() {
  server_.send(200, "text/html", buildPage(nullptr));
}

void WiFiSetupApp::handleSave() {
  String ssid = server_.arg("ssid");
  String pass = server_.arg("pass");
  ssid.trim();

  if (ssid.length() == 0) {
    server_.send(200, "text/html", buildPage("Enter a WiFi name first."));
    return;
  }

  const int8_t slot = saveProfile(ssid, pass);
  String notice = "Saved ";
  notice += escapeHtml(ssid);
  notice += " in slot ";
  notice += slot + 1;
  notice += ". Use the device Test Saved menu to verify it.";
  server_.send(200, "text/html", buildPage(notice.c_str()));
}

void WiFiSetupApp::handleNotFound() {
  server_.sendHeader("Location", String("http://") + apIP_.toString(), true);
  server_.send(302, "text/plain", "");
}

String WiFiSetupApp::escapeHtml(const String& value) const {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (uint16_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '&') {
      escaped += F("&amp;");
    } else if (c == '<') {
      escaped += F("&lt;");
    } else if (c == '>') {
      escaped += F("&gt;");
    } else if (c == '"') {
      escaped += F("&quot;");
    } else if (c == '\'') {
      escaped += F("&#39;");
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String WiFiSetupApp::buildPage(const char* notice) const {
  String html;
  html.reserve(5600);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>MicriOS WiFi</title><style>");
  html += F("body{font-family:system-ui,sans-serif;background:#10141c;color:#eef3ff;margin:0;padding:24px}");
  html += F("main{max-width:560px;margin:auto;background:#171d28;border:1px solid #334155;border-radius:8px;padding:20px}");
  html += F("label{display:block;margin:14px 0 6px}input,button{box-sizing:border-box;width:100%;font:inherit;padding:10px;border-radius:6px}");
  html += F("input{border:1px solid #64748b;background:#0b1020;color:#eef3ff}button{border:0;background:#3b82f6;color:white;margin-top:12px}");
  html += F(".muted{color:#b8c2d6}.notice{background:#263349;border-radius:6px;padding:10px}.profile{border-top:1px solid #334155;padding:10px 0}");
  html += F("</style></head><body><main>");
  html += F("<h1>MicriOS WiFi Setup</h1>");
  html += F("<p class='muted'>Save up to five WiFi networks. Setup WiFi password is <strong>micrideck</strong>. Testing and deleting saved networks is done from the MicriOS WiFi Settings app.</p>");

  if (notice != nullptr) {
    html += F("<p class='notice'>");
    html += notice;
    html += F("</p>");
  }

  html += F("<h2>Saved networks</h2>");
  if (profileCount_ == 0) {
    html += F("<p class='muted'>No saved WiFi profiles yet.</p>");
  } else {
    for (uint8_t i = 0; i < MAX_PROFILES; i++) {
      if (!profileUsed_[i]) {
        continue;
      }
      html += F("<div class='profile'><strong>");
      html += escapeHtml(savedSsids_[i]);
      html += F("</strong><br><span class='muted'>Slot ");
      html += i + 1;
      html += F("</span></div>");
    }
  }

  html += F("<h2>Add or update</h2><form method='post' action='/save'>");
  html += F("<label for='ssid'>WiFi name</label>");
  html += F("<input id='ssid' name='ssid' list='networks' required>");
  html += F("<datalist id='networks'>");
  for (uint8_t i = 0; i < networkCount_; i++) {
    html += F("<option value='");
    html += escapeHtml(networkNames_[i]);
    html += F("'>");
    html += escapeHtml(networkNames_[i]);
    html += F(" ");
    html += networkRssi_[i];
    html += F(" dBm</option>");
  }
  html += F("</datalist>");
  html += F("<label for='pass'>Password</label>");
  html += F("<input id='pass' name='pass' type='password' autocomplete='current-password'>");
  html += F("<button type='submit'>Save WiFi</button></form>");
  html += F("<p class='muted'>After saving, return to MicriOS. Hold the device button to leave setup.</p>");
  html += F("</main></body></html>");
  return html;
}

String WiFiSetupApp::prefKey(const char* prefix, uint8_t index) const {
  String key(prefix);
  key += index;
  return key;
}

const char* WiFiSetupApp::portalLabel() const {
  switch (portalState_) {
    case PortalState::Starting:
      return "Starting";
    case PortalState::Running:
      return "Portal on";
    case PortalState::Error:
      return "AP failed";
    case PortalState::Off:
    default:
      return "Portal off";
  }
}

const char* WiFiSetupApp::testLabel() const {
  switch (testState_) {
    case TestState::Connecting:
      return testingAll_ ? "Testing all" : "Testing";
    case TestState::Connected:
      return "Connected";
    case TestState::Failed:
      return "Failed";
    case TestState::Idle:
    default:
      return "Ready";
  }
}

void WiFiSetupApp::drawFit(U8G2& u8g2, int x, int y, const char* text) const {
  u8g2.setFont(u8g2_font_5x8_tr);
  if (u8g2.getStrWidth(text) > static_cast<int>(width + 2 - x - 3)) {
    u8g2.setFont(u8g2_font_4x6_tr);
  }
  u8g2.drawStr(x, y, text);
}

void WiFiSetupApp::drawRunning(U8G2& u8g2) {
  switch (mode_) {
    case Mode::ConfigurePortal:
      drawConfigure(u8g2);
      break;
    case Mode::TestSelect:
      drawProfileSelect(u8g2, "Test WiFi", "Hold test");
      break;
    case Mode::Testing:
      drawTesting(u8g2);
      break;
    case Mode::DeleteSelect:
      drawProfileSelect(u8g2, "Delete WiFi", "Hold del");
      break;
    case Mode::ConfirmDelete:
      drawConfirmDelete(u8g2);
      break;
    case Mode::Message:
      drawMessage(u8g2);
      break;
    case Mode::Main:
    default:
      drawMain(u8g2);
      break;
  }
}

void WiFiSetupApp::drawMain(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 3, 9, "WiFi");
  drawFit(u8g2, left_ + 3, 23, MAIN_ITEMS[mainIndex_]);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 38);
  u8g2.print(mainIndex_ + 1);
  u8g2.print("/");
  u8g2.print(MAIN_COUNT);
  u8g2.print(" Tap next");
}

void WiFiSetupApp::drawConfigure(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 3, 7, portalLabel());
  u8g2.drawStr(left_ + 3, 15, "AP:MicriDeck-C3");
  u8g2.drawStr(left_ + 3, 23, "PW:micrideck");
  u8g2.drawStr(left_ + 3, 31, "IP:192.168.4.1");
  u8g2.drawStr(left_ + 3, 39, "Hold back");
}

void WiFiSetupApp::drawProfileSelect(U8G2& u8g2, const char* title, const char* action) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 3, 9, title);

  if (profileMenuIndex_ == profileCount_) {
    drawFit(u8g2, left_ + 3, 23, mode_ == Mode::TestSelect ? "Test ALL" : "Delete ALL");
  } else if (profileMenuIndex_ == profileCount_ + 1) {
    drawFit(u8g2, left_ + 22, 23, "Back");
  } else {
    const int8_t slot = profileSlotForPosition(profileMenuIndex_);
    drawFit(u8g2, left_ + 3, 23, slot >= 0 ? savedSsids_[slot].c_str() : "?");
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 38);
  u8g2.print(profileMenuIndex_ + 1);
  u8g2.print("/");
  u8g2.print(profileMenuCount());
  u8g2.print(" ");
  u8g2.print(action);
}

void WiFiSetupApp::drawTesting(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 3, 9, "WiFi Test");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 3, 18, testLabel());
  if (testProfileIndex_ != NO_PROFILE && testProfileIndex_ < MAX_PROFILES) {
    drawFit(u8g2, left_ + 3, 28, savedSsids_[testProfileIndex_].c_str());
  }
  if (testState_ == TestState::Connecting) {
    u8g2.drawStr(left_ + 3, 39, "Hold cancel");
  } else {
    u8g2.drawStr(left_ + 3, 39, "Tap list Hold menu");
  }
}

void WiFiSetupApp::drawConfirmDelete(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 3, 9, deleteAll_ ? "DEL ALL?" : "DELETE?");
  u8g2.setFont(u8g2_font_4x6_tr);
  if (deleteAll_) {
    u8g2.drawStr(left_ + 3, 21, "All WiFi saves");
  } else if (deleteProfileIndex_ < MAX_PROFILES) {
    drawFit(u8g2, left_ + 3, 21, savedSsids_[deleteProfileIndex_].c_str());
  }
  u8g2.drawStr(left_ + 3, 30, "Hold erase");
  u8g2.drawStr(left_ + 3, 38, "Tap cancel");
}

void WiFiSetupApp::drawMessage(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawFit(u8g2, left_ + 3, 18, message_);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 3, 38, "Tap continue");
}

void WiFiSetupApp::drawStart(U8G2& u8g2) {
  drawMain(u8g2);
}
