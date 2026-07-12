#include "EspContactsApp.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t ESPNOW_CHANNEL = 6;
constexpr uint16_t FEEDBACK_MS = 900;
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const char* MAIN_ITEMS[] = {"Listen", "Send Mine", "Manage", "Exit"};
constexpr uint8_t MAIN_COUNT = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);
EspContactsApp* activeContactsApp = nullptr;
}

EspContactsApp::EspContactsApp(uint32_t width, uint32_t height, uint32_t left)
    : App("ESP Contacts", width, height), left_(left) {}

bool EspContactsApp::hasCustomOverlay() const { return true; }
bool EspContactsApp::startsRunningImmediately() const { return true; }

void EspContactsApp::onAppReset() {
  PlayerProfile::unpackInitials(PlayerProfile::loadInitials(), myInitials_);
  contactBook_.load();
  mode_ = Mode::Main;
  mainIndex_ = 0;
  contactIndex_ = 0;
  feedbackMs_ = 0;
  sendPending_ = false;
  sendSuccess_ = false;
  lastSeen_[0] = '\0';
}

bool EspContactsApp::beginRadio() {
  shutdownRadio();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  if (esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) return false;
  if (esp_now_init() != ESP_OK) return false;
  radioStarted_ = true;
  esp_now_register_recv_cb(EspContactsApp::onReceiveStatic);
  esp_now_register_send_cb(EspContactsApp::onSentStatic);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  const esp_err_t peerResult = esp_now_add_peer(&peer);
  if (peerResult != ESP_OK && peerResult != ESP_ERR_ESPNOW_EXIST) {
    shutdownRadio();
    return false;
  }

  activeContactsApp = this;
  radioReady_ = true;
  return true;
}

void EspContactsApp::shutdownRadio() {
  if (activeContactsApp == this) activeContactsApp = nullptr;
  if (radioStarted_) {
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_del_peer(BROADCAST_MAC);
    esp_now_deinit();
    radioStarted_ = false;
  }
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  radioReady_ = false;
}

bool EspContactsApp::sendMyContact() {
  if (!radioReady_ && !beginRadio()) {
    showFeedback("Radio fail", Mode::Main);
    return false;
  }
  CommunicatorLogic::MessagePacket packet = {};
  char to[3];
  CommunicatorLogic::setAllRecipient(to);
  logic_.fillBasePacket(packet, CommunicatorLogic::PacketType::Contact, myInitials_, to);
  const esp_err_t result = esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  if (result != ESP_OK) {
    showFeedback("Send fail", Mode::Main);
    return false;
  }
  showFeedback("Sending", Mode::Main);
  return true;
}

void EspContactsApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  if (sendPending_) {
    sendPending_ = false;
    showFeedback(sendSuccess_ ? "Sent" : "Failed", Mode::Main);
  }

  if (mode_ == Mode::Feedback) {
    feedbackMs_ += deltaMs;
    if (feedbackMs_ >= FEEDBACK_MS) {
      mode_ = feedbackReturnMode_;
      feedbackMs_ = 0;
    }
    return;
  }

  if (mode_ == Mode::Main) {
    if (input.click) mainIndex_ = (mainIndex_ + 1) % MAIN_COUNT;
    if (input.longPress) {
      if (mainIndex_ == 0) {
        radioReady_ = beginRadio();
        mode_ = Mode::Listen;
      } else if (mainIndex_ == 1) {
        sendMyContact();
      } else if (mainIndex_ == 2) {
        contactBook_.load();
        contactIndex_ = 0;
        mode_ = Mode::Manage;
      } else {
        shutdownRadio();
        requestExitToMenu();
      }
    }
    return;
  }

  if (mode_ == Mode::Listen) {
    if (input.longPress) {
      shutdownRadio();
      contactBook_.load();
      mode_ = Mode::Main;
    }
    return;
  }

  if (mode_ == Mode::Manage) {
    const uint8_t count = contactBook_.count() + 1;
    if (input.click) contactIndex_ = (contactIndex_ + 1) % count;
    if (input.longPress) {
      if (contactIndex_ >= contactBook_.count()) {
        mode_ = Mode::Main;
      } else {
        mode_ = Mode::ConfirmDelete;
      }
    }
    return;
  }

  if (mode_ == Mode::ConfirmDelete) {
    if (input.click) mode_ = Mode::Manage;
    if (input.longPress) {
      contactBook_.remove(contactIndex_);
      if (contactIndex_ >= contactBook_.count() && contactIndex_ > 0) contactIndex_--;
      showFeedback("Deleted", Mode::Manage);
    }
  }
}

void EspContactsApp::onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (activeContactsApp != nullptr && info != nullptr) {
    activeContactsApp->handleIncoming(info->src_addr, data, len);
  }
}

void EspContactsApp::onSentStatic(const esp_now_send_info_t* info, esp_now_send_status_t status) {
  (void)info;
  if (activeContactsApp != nullptr) {
    activeContactsApp->handleSendStatus(status == ESP_NOW_SEND_SUCCESS);
  }
}

void EspContactsApp::handleIncoming(const uint8_t* from, const uint8_t* data, int len) {
  if (len != static_cast<int>(sizeof(CommunicatorLogic::MessagePacket))) return;
  CommunicatorLogic::MessagePacket packet = {};
  memcpy(&packet, data, sizeof(packet));
  if (!logic_.packetHasHeader(packet)) return;
  if (packet.type != static_cast<uint8_t>(CommunicatorLogic::PacketType::Contact) &&
      packet.type != static_cast<uint8_t>(CommunicatorLogic::PacketType::Message)) return;
  const int8_t slot = contactBook_.upsert(packet.from, from, millis());
  if (slot >= 0) {
    strncpy(lastSeen_, contactBook_.get(slot).label, sizeof(lastSeen_) - 1);
    lastSeen_[sizeof(lastSeen_) - 1] = '\0';
  }
}

void EspContactsApp::handleSendStatus(bool success) {
  sendSuccess_ = success;
  sendPending_ = true;
}

void EspContactsApp::showFeedback(const char* text, Mode returnMode) {
  feedbackText_ = text;
  feedbackReturnMode_ = returnMode;
  feedbackMs_ = 0;
  mode_ = Mode::Feedback;
}

void EspContactsApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  if (mode_ == Mode::Listen) drawListen(u8g2);
  else if (mode_ == Mode::Manage) drawManage(u8g2);
  else if (mode_ == Mode::ConfirmDelete) drawConfirmDelete(u8g2);
  else if (mode_ == Mode::Feedback) drawFeedback(u8g2);
  else drawMain(u8g2);
}

void EspContactsApp::drawHeader(U8G2& u8g2, const char* title) const {
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 7, title);
  u8g2.drawHLine(1, 9, width);
}

void EspContactsApp::drawMain(U8G2& u8g2) {
  drawHeader(u8g2, "ESP Contacts");
  u8g2.setFont(u8g2_font_4x6_tr);
  for (uint8_t i = 0; i < MAIN_COUNT; i++) {
    const int y = 16 + i * 6;
    u8g2.drawStr(3, y, i == mainIndex_ ? ">" : " ");
    drawFit(u8g2, 10, y, MAIN_ITEMS[i], 15);
  }
}

void EspContactsApp::drawListen(U8G2& u8g2) {
  drawHeader(u8g2, radioReady_ ? "Listening" : "Radio Fail");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(4, 18);
  u8g2.print("Saved:");
  u8g2.print(contactBook_.count());
  u8g2.drawStr(4, 27, lastSeen_[0] ? lastSeen_ : "Waiting...");
  u8g2.drawStr(4, 38, "Hold back");
}

void EspContactsApp::drawManage(U8G2& u8g2) {
  drawHeader(u8g2, "Manage");
  u8g2.setFont(u8g2_font_4x6_tr);
  const uint8_t count = contactBook_.count() + 1;
  const uint8_t first = contactIndex_ >= 4 ? contactIndex_ - 3 : 0;
  for (uint8_t row = 0; row < 4 && first + row < count; row++) {
    const uint8_t item = first + row;
    const int y = 16 + row * 6;
    u8g2.drawStr(3, y, item == contactIndex_ ? ">" : " ");
    drawFit(u8g2, 10, y, item >= contactBook_.count() ? "Back" : contactBook_.get(item).label, 15);
  }
}

void EspContactsApp::drawConfirmDelete(U8G2& u8g2) {
  drawHeader(u8g2, "Delete?");
  u8g2.setFont(u8g2_font_5x8_tr);
  if (contactIndex_ < contactBook_.count()) {
    drawFit(u8g2, 4, 21, contactBook_.get(contactIndex_).label, 12);
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(4, 38, "Tap no Hold yes");
}

void EspContactsApp::drawFeedback(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_7x13B_tr);
  const int w = u8g2.getStrWidth(feedbackText_);
  u8g2.drawStr((width + 2 - w) / 2, 23, feedbackText_);
}

void EspContactsApp::drawFit(U8G2& u8g2, int x, int y, const char* text, uint8_t maxChars) const {
  char buffer[20] = {};
  strncpy(buffer, text == nullptr ? "" : text, sizeof(buffer) - 1);
  if (strlen(buffer) > maxChars) {
    buffer[maxChars - 1] = '~';
    buffer[maxChars] = '\0';
  }
  u8g2.drawStr(x, y, buffer);
}
