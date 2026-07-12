#include "WiFiSetupApp.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint16_t DNS_PORT = 53;
constexpr uint16_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint8_t AP_CHANNEL = 6;
constexpr const char* AP_SSID = "MicriDeck-TDisplay";
constexpr const char* AP_PASS = "micrideck";
constexpr const char* STA_HOSTNAME = "MicriDeck-TDisplay";
constexpr uint16_t PORTAL_LAUNCH_NOTICE_MS = 250;
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
constexpr uint8_t ROW_COUNT = 4;

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}
}

WiFiSetupApp::WiFiSetupApp(uint32_t width, uint32_t height)
    : App("WiFi Setup", width, height), server_(80), apIP_(AP_IP) {}

bool WiFiSetupApp::hasCustomOverlay() const {
  return true;
}

bool WiFiSetupApp::startsRunningImmediately() const {
  return true;
}

void WiFiSetupApp::markDirty() {
  dirty_ = true;
}

void WiFiSetupApp::onAppReset() {
  mode_ = Mode::Main;
  mainIndex_ = 0;
  profileMenuIndex_ = 0;
  deleteProfileIndex_ = NO_PROFILE;
  deleteAll_ = false;
  portalLaunchPending_ = false;
  testState_ = TestState::Idle;
  portalState_ = PortalState::Off;
  statusTimerMs_ = 0;
  stopPortal();
  stopStationTest(true);
  logic_.init();
  markDirty();
}

void WiFiSetupApp::startPortal() {
  stopPortal();
  stopStationTest(false);

  portalState_ = PortalState::Starting;
  networkCount_ = 0;
  markDirty();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  logic_.loadProfiles();
  scanNetworks();

  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  delay(250);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET) || !WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, false, 4)) {
    portalState_ = PortalState::Error;
    markDirty();
    return;
  }
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  setupRoutes();
  dns_.start(DNS_PORT, "*", apIP_);
  server_.begin();
  portalRunning_ = true;
  portalState_ = PortalState::Running;
  markDirty();
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
    markDirty();
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
  markDirty();
}

bool WiFiSetupApp::beginProfileTest(uint8_t index, bool testAll) {
  if (index >= WiFiLogic::MAX_PROFILES || !logic_.isProfileUsed(index)) {
    testState_ = TestState::Failed;
    markDirty();
    return false;
  }

  const String pass = logic_.getPass(index);

  stopPortal();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  WiFi.setHostname(STA_HOSTNAME);
  WiFi.begin(logic_.getSsid(index).c_str(), pass.c_str());
  connectStartedAtMs_ = millis();
  testProfileIndex_ = index;
  testingAll_ = testAll;
  testState_ = TestState::Connecting;
  statusTimerMs_ = 0;
  markDirty();
  return true;
}

bool WiFiSetupApp::beginNextProfileTest(uint8_t startIndex) {
  for (uint8_t i = startIndex; i < WiFiLogic::MAX_PROFILES; i++) {
    if (logic_.isProfileUsed(i)) {
      return beginProfileTest(i, true);
    }
  }
  testingAll_ = false;
  testProfileIndex_ = NO_PROFILE;
  testState_ = TestState::Failed;
  markDirty();
  return false;
}

uint8_t WiFiSetupApp::profileMenuCount() const {
  return logic_.getProfileCount() + 2;
}

int8_t WiFiSetupApp::profileSlotForPosition(uint8_t position) const {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < WiFiLogic::MAX_PROFILES; i++) {
    if (!logic_.isProfileUsed(i)) {
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
  markDirty();
}

void WiFiSetupApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  if (mode_ == Mode::StartingPortal) {
    if (b2.click) {
      portalLaunchPending_ = false;
      portalState_ = PortalState::Off;
      mode_ = Mode::Main;
      markDirty();
      return;
    }

    if (portalLaunchPending_ && millis() - portalLaunchAtMs_ >= PORTAL_LAUNCH_NOTICE_MS) {
      portalLaunchPending_ = false;
      startPortal();
      mode_ = Mode::ConfigurePortal;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::ConfigurePortal && portalRunning_) {
    dns_.processNextRequest();
    server_.handleClient();
  }

  if (mode_ == Mode::Main) {
    if (b2.click) {
      mainIndex_ = mainIndex_ == 0 ? MAIN_COUNT - 1 : mainIndex_ - 1;
      markDirty();
    } else if (b1.click) {
      mainIndex_ = (mainIndex_ + 1) % MAIN_COUNT;
      markDirty();
    } else if (b1.longPress) {
      if (mainIndex_ == 0) {
        portalState_ = PortalState::Starting;
        portalLaunchPending_ = true;
        portalLaunchAtMs_ = millis();
        mode_ = Mode::StartingPortal;
        markDirty();
      } else if (mainIndex_ == 1) {
        logic_.loadProfiles();
        if (logic_.getProfileCount() == 0) {
          showMessage("No saved WiFi", Mode::Main);
        } else {
          profileMenuIndex_ = 0;
          mode_ = Mode::TestSelect;
          markDirty();
        }
      } else if (mainIndex_ == 2) {
        logic_.loadProfiles();
        if (logic_.getProfileCount() == 0) {
          showMessage("No saved WiFi", Mode::Main);
        } else {
          profileMenuIndex_ = 0;
          mode_ = Mode::DeleteSelect;
          markDirty();
        }
      } else {
        stopPortal();
        stopStationTest(true);
        requestExitToMenu();
      }
    }
    return;
  }

  if (mode_ == Mode::ConfigurePortal) {
    if (b1.click) {
      scanNetworks();
    } else if (b2.click || b1.longPress) {
      stopPortal();
      logic_.loadProfiles();
      mode_ = Mode::Main;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::TestSelect) {
    const uint8_t count = profileMenuCount();
    if (b2.click) {
      profileMenuIndex_ = profileMenuIndex_ == 0 ? count - 1 : profileMenuIndex_ - 1;
      markDirty();
    } else if (b1.click) {
      profileMenuIndex_ = (profileMenuIndex_ + 1) % count;
      markDirty();
    } else if (b1.longPress) {
      const uint8_t profileCount = logic_.getProfileCount();
      if (profileMenuIndex_ == profileCount) {
        beginNextProfileTest(0);
        mode_ = Mode::Testing;
      } else if (profileMenuIndex_ == profileCount + 1) {
        mode_ = Mode::Main;
      } else {
        const int8_t slot = profileSlotForPosition(profileMenuIndex_);
        if (slot >= 0) {
          beginProfileTest(slot, false);
          mode_ = Mode::Testing;
        }
      }
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Testing) {
    updateConnectionTest(deltaMs);
    if (testState_ == TestState::Connecting) {
      if (b2.click || b1.longPress) {
        stopStationTest(true);
        mode_ = Mode::TestSelect;
        markDirty();
      }
    } else if (b2.click) {
      stopStationTest(true);
      mode_ = Mode::TestSelect;
      markDirty();
    } else if (b1.click) {
      mode_ = Mode::TestSelect;
      markDirty();
    } else if (b1.longPress) {
      stopStationTest(true);
      mode_ = Mode::Main;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::DeleteSelect) {
    const uint8_t count = profileMenuCount();
    if (b2.click) {
      profileMenuIndex_ = profileMenuIndex_ == 0 ? count - 1 : profileMenuIndex_ - 1;
      markDirty();
    } else if (b1.click) {
      profileMenuIndex_ = (profileMenuIndex_ + 1) % count;
      markDirty();
    } else if (b1.longPress) {
      const uint8_t profileCount = logic_.getProfileCount();
      if (profileMenuIndex_ == profileCount) {
        deleteAll_ = true;
        deleteProfileIndex_ = NO_PROFILE;
        mode_ = Mode::ConfirmDelete;
      } else if (profileMenuIndex_ == profileCount + 1) {
        mode_ = Mode::Main;
      } else {
        const int8_t slot = profileSlotForPosition(profileMenuIndex_);
        if (slot >= 0) {
          deleteAll_ = false;
          deleteProfileIndex_ = slot;
          mode_ = Mode::ConfirmDelete;
        }
      }
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::ConfirmDelete) {
    if (b2.click || b1.click) {
      mode_ = Mode::DeleteSelect;
      markDirty();
    } else if (b1.longPress) {
      if (deleteAll_) {
        logic_.forgetAllProfiles();
        showMessage("All WiFi erased", Mode::Main);
      } else {
        logic_.forgetProfile(deleteProfileIndex_);
        showMessage("WiFi erased", Mode::DeleteSelect);
      }
    }
    return;
  }

  if (mode_ == Mode::Message) {
    if (b2.click || b1.click || b1.longPress) {
      mode_ = messageReturnMode_;
      markDirty();
    }
  }
}

void WiFiSetupApp::updateConnectionTest(uint32_t deltaMs) {
  statusTimerMs_ += deltaMs;
  if (statusTimerMs_ < 500) {
    return;
  }

  statusTimerMs_ = 0;
  if (testState_ == TestState::Connecting && WiFi.status() == WL_CONNECTED) {
    testState_ = TestState::Connected;
    testingAll_ = false;
    markDirty();
  } else if (testState_ == TestState::Connecting && (millis() - connectStartedAtMs_) > CONNECT_TIMEOUT_MS) {
    if (testingAll_) {
      beginNextProfileTest(testProfileIndex_ + 1);
    } else {
      testState_ = TestState::Failed;
      markDirty();
    }
  }
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

  const int8_t slot = logic_.saveProfile(ssid, pass);
  String notice = "Saved ";
  notice += escapeHtml(ssid);
  notice += " in slot ";
  notice += slot + 1;
  notice += ". Use Test Saved on the device to verify it.";
  markDirty();
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
    if (c == '&') escaped += F("&amp;");
    else if (c == '<') escaped += F("&lt;");
    else if (c == '>') escaped += F("&gt;");
    else if (c == '"') escaped += F("&quot;");
    else if (c == '\'') escaped += F("&#39;");
    else escaped += c;
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
  html += F("<p class='muted'>Save up to five WiFi networks. Setup WiFi password is <strong>micrideck</strong>. Testing and deleting saved networks is done from MicriOS.</p>");

  if (notice != nullptr) {
    html += F("<p class='notice'>");
    html += notice;
    html += F("</p>");
  }

  html += F("<h2>Saved networks</h2>");
  if (logic_.getProfileCount() == 0) {
    html += F("<p class='muted'>No saved WiFi profiles yet.</p>");
  } else {
    for (uint8_t i = 0; i < WiFiLogic::MAX_PROFILES; i++) {
      if (!logic_.isProfileUsed(i)) {
        continue;
      }
      html += F("<div class='profile'><strong>");
      html += escapeHtml(logic_.getSsid(i));
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
  html += F("<p class='muted'>After saving, return to MicriOS. Press B2 to leave setup.</p>");
  html += F("</main></body></html>");
  return html;
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

void WiFiSetupApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }

  switch (mode_) {
    case Mode::StartingPortal:
      drawStartingPortal(tft);
      break;
    case Mode::ConfigurePortal:
      drawConfigure(tft);
      break;
    case Mode::TestSelect:
      drawProfileSelect(tft, "Test WiFi", TFT_CYAN, "B1 hold test  B2 prev");
      break;
    case Mode::Testing:
      drawTesting(tft);
      break;
    case Mode::DeleteSelect:
      drawProfileSelect(tft, "Delete WiFi", TFT_RED, "B1 hold del  B2 prev");
      break;
    case Mode::ConfirmDelete:
      drawConfirmDelete(tft);
      break;
    case Mode::Message:
      drawMessage(tft);
      break;
    case Mode::Main:
    default:
      drawMain(tft);
      break;
  }
  dirty_ = false;
}

void WiFiSetupApp::drawMain(TFT_eSPI& tft) {
  const String saved = String(logic_.getProfileCount()) + "/" + String(WiFiLogic::MAX_PROFILES);
  const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
  const uint8_t first = TDisplayUi::menuScrollOffset(mainIndex_, MAIN_COUNT, TDisplayUi::menuStyle(textSize));
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::menuFrame(canvas, "WiFi Setup", mainIndex_, MAIN_COUNT, first,
                          [](uint8_t index) -> const char* { return MAIN_ITEMS[index]; },
                          "B1 next/open  B2 prev", textSize, TFT_CYAN);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(saved, width - canvas.textWidth(saved) - 12, 10);
  });
}

void WiFiSetupApp::drawStartingPortal(TFT_eSPI& tft) {
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "WiFi Setup", TFT_CYAN, "Starting");
    TDisplayUi::centered(canvas, "Launching", 45, 2, TFT_YELLOW);
    TDisplayUi::centered(canvas, "WiFi AP", 69, 2, TFT_CYAN);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Scanning networks first...", 44, 102);
    TDisplayUi::footer(canvas, "B2 cancel");
  });
}

void WiFiSetupApp::drawConfigure(TFT_eSPI& tft) {
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "Configure", portalState_ == PortalState::Error ? TFT_RED : TFT_CYAN, portalLabel());
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Connect to setup WiFi:", 14, 38);
    TDisplayUi::centered(canvas, AP_SSID, 54, 2, TFT_CYAN);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Pass: micrideck", 14, 84);
    canvas.drawString("Open: 192.168.4.1", 14, 98);
    TDisplayUi::footer(canvas, "B1 rescan  B2 back");
  });
}

void WiFiSetupApp::drawProfileSelect(TFT_eSPI& tft, const char* title, uint16_t color, const char* action) {
  const uint8_t count = profileMenuCount();
  const TDisplayUi::TextSize textSize = TDisplayUi::loadTextSize();
  const TDisplayUi::MenuStyle style = TDisplayUi::menuStyle(textSize);
  const uint8_t first = TDisplayUi::menuScrollOffset(profileMenuIndex_, count, style);
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::menuShell(canvas, title, profileMenuIndex_, count, textSize);
    for (uint8_t row = 0; row < style.visibleRows && first + row < count; row++) {
      const uint8_t item = first + row;
      String label;
      const uint8_t profileCount = logic_.getProfileCount();
      if (item == profileCount) {
        label = mode_ == Mode::TestSelect ? "Test ALL" : "Delete ALL";
      } else if (item == profileCount + 1) {
        label = "Back";
      } else {
        const int8_t slot = profileSlotForPosition(item);
        label = slot >= 0 ? logic_.getSsid(slot) : "?";
      }
      TDisplayUi::menuRow(canvas, row, label.c_str(), item == profileMenuIndex_, textSize, color);
    }
    TDisplayUi::menuFooter(canvas, action, textSize);
  });
}

void WiFiSetupApp::drawTesting(TFT_eSPI& tft) {
  const bool ok = testState_ == TestState::Connected;
  const bool fail = testState_ == TestState::Failed;
  const uint16_t color = ok ? TFT_GREEN : fail ? TFT_RED : TFT_YELLOW;
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "WiFi Test", color, testLabel());
    if (testProfileIndex_ != NO_PROFILE && testProfileIndex_ < WiFiLogic::MAX_PROFILES) {
      TDisplayUi::centered(canvas, logic_.getSsid(testProfileIndex_), 42, 2, TFT_WHITE);
    }
    if (ok) {
      TDisplayUi::centered(canvas, WiFi.localIP().toString(), 79, 2, TFT_GREEN);
    } else if (fail) {
      TDisplayUi::centered(canvas, "No connection", 79, 2, TFT_RED);
    } else {
      TDisplayUi::centered(canvas, "Connecting...", 79, 2, TFT_YELLOW);
    }
    TDisplayUi::footer(canvas, testState_ == TestState::Connecting ? "B2 cancel" : "B1 list  B2 back");
  });
}

void WiFiSetupApp::drawConfirmDelete(TFT_eSPI& tft) {
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, deleteAll_ ? "DELETE ALL?" : "DELETE WiFi?", TFT_RED);
    if (deleteAll_) {
      TDisplayUi::centered(canvas, "All saved WiFi", 50, 2, TFT_RED);
    } else if (deleteProfileIndex_ < WiFiLogic::MAX_PROFILES) {
      TDisplayUi::centered(canvas, logic_.getSsid(deleteProfileIndex_), 50, 2, TFT_RED);
    }
    TDisplayUi::footer(canvas, "B1 hold erase  B1/B2 tap no");
  });
}

void WiFiSetupApp::drawMessage(TFT_eSPI& tft) {
  drawBuffered(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "WiFi Setup", TFT_CYAN);
    TDisplayUi::largeValue(canvas, message_, 52, TFT_YELLOW);
    TDisplayUi::footer(canvas, "B1/B2 continue");
  });
}

void WiFiSetupApp::drawStart(TFT_eSPI& tft) {
  drawRunning(tft);
}
