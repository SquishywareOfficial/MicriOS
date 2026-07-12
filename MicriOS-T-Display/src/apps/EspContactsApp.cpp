#include "EspContactsApp.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint8_t ESPNOW_CHANNEL = 6;
constexpr uint16_t FEEDBACK_MS = 900;
constexpr uint8_t ROW_COUNT = 4;
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const char* MAIN_ITEMS[] = {"Listen for Contacts", "Send My Contact", "Manage Contacts", "Exit"};
constexpr uint8_t MAIN_COUNT = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);
EspContactsApp* activeContactsApp = nullptr;
}

EspContactsApp::EspContactsApp(uint32_t width, uint32_t height)
    : App("ESP Contacts", width, height) {}

void EspContactsApp::render(TFT_eSPI& tft) {
  const AppPhase currentPhase = phase();
  if (!phaseCached_ || currentPhase != renderedPhase_) {
    phaseCached_ = true;
    renderedPhase_ = currentPhase;
    dirty_ = true;
  }
  App::render(tft);
}

void EspContactsApp::markDirty() {
  dirty_ = true;
}

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
  markDirty();
}

bool EspContactsApp::beginRadio() {
  shutdownRadio();
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(40);
  WiFi.mode(WIFI_STA);
  delay(40);
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
    showFeedback("Radio failed", TFT_RED, Mode::Main);
    return false;
  }
  CommunicatorLogic::MessagePacket packet = {};
  char to[3];
  CommunicatorLogic::setAllRecipient(to);
  logic_.fillBasePacket(packet, CommunicatorLogic::PacketType::Contact, myInitials_, to);
  const esp_err_t result = esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  if (result != ESP_OK) {
    showFeedback("Send failed", TFT_RED, Mode::Main);
    return false;
  }
  showFeedback("Sending", TFT_YELLOW, Mode::Main);
  return true;
}

void EspContactsApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  if (sendPending_) {
    sendPending_ = false;
    showFeedback(sendSuccess_ ? "Sent" : "Failed", sendSuccess_ ? TFT_GREEN : TFT_RED, Mode::Main);
  }

  if (mode_ == Mode::Feedback) {
    feedbackMs_ += deltaMs;
    if (feedbackMs_ >= FEEDBACK_MS) {
      mode_ = feedbackReturnMode_;
      feedbackMs_ = 0;
      markDirty();
    }
    return;
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
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Listen) {
    if (b2.click || b1.longPress) {
      shutdownRadio();
      contactBook_.load();
      mode_ = Mode::Main;
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::Manage) {
    const uint8_t count = contactBook_.count() + 1;
    if (b2.click) {
      contactIndex_ = contactIndex_ == 0 ? count - 1 : contactIndex_ - 1;
      markDirty();
    } else if (b1.click) {
      contactIndex_ = (contactIndex_ + 1) % count;
      markDirty();
    } else if (b1.longPress) {
      if (contactIndex_ >= contactBook_.count()) {
        mode_ = Mode::Main;
      } else {
        mode_ = Mode::ConfirmDelete;
      }
      markDirty();
    }
    return;
  }

  if (mode_ == Mode::ConfirmDelete) {
    if (b2.click || b1.click) {
      mode_ = Mode::Manage;
      markDirty();
    } else if (b1.longPress) {
      contactBook_.remove(contactIndex_);
      if (contactIndex_ >= contactBook_.count() && contactIndex_ > 0) contactIndex_--;
      showFeedback("Deleted", TFT_GREEN, Mode::Manage);
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
    markDirty();
  }
}

void EspContactsApp::handleSendStatus(bool success) {
  sendSuccess_ = success;
  sendPending_ = true;
}

void EspContactsApp::showFeedback(const char* text, uint16_t color, Mode returnMode) {
  feedbackText_ = text;
  feedbackColor_ = color;
  feedbackReturnMode_ = returnMode;
  feedbackMs_ = 0;
  mode_ = Mode::Feedback;
  markDirty();
}

void EspContactsApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) return;
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    if (mode_ == Mode::Listen) drawListen(canvas);
    else if (mode_ == Mode::Manage) drawManage(canvas);
    else if (mode_ == Mode::ConfirmDelete) drawConfirmDelete(canvas);
    else if (mode_ == Mode::Feedback) drawFeedback(canvas);
    else drawMain(canvas);
  });
  dirty_ = false;
}

template <typename Canvas>
void EspContactsApp::drawMain(Canvas& tft) {
  TDisplayUi::header(tft, "ESP Contacts", TFT_CYAN, myInitials_);
  for (uint8_t i = 0; i < MAIN_COUNT; i++) {
    TDisplayUi::row(tft, 34 + i * 22, MAIN_ITEMS[i], i == mainIndex_, TFT_CYAN);
  }
  TDisplayUi::footer(tft, "B1 next/select  B2 prev");
}

template <typename Canvas>
void EspContactsApp::drawListen(Canvas& tft) {
  TDisplayUi::header(tft, radioReady_ ? "Listening" : "Radio Failed", radioReady_ ? TFT_GREEN : TFT_RED);
  TDisplayUi::centered(tft, "Saved Contacts", 38, 1, TFT_LIGHTGREY);
  TDisplayUi::largeValue(tft, String(contactBook_.count()), 50, TFT_GREEN);
  TDisplayUi::centered(tft, lastSeen_[0] ? String("Last ") + lastSeen_ : "Waiting...", 101, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 hold/B2 back");
}

template <typename Canvas>
void EspContactsApp::drawManage(Canvas& tft) {
  const String stat = String(contactBook_.count()) + "/" + String(EspContacts::MAX_CONTACTS);
  TDisplayUi::header(tft, "Manage Contacts", TFT_YELLOW, stat.c_str());
  const uint8_t count = contactBook_.count() + 1;
  const uint8_t first = contactIndex_ >= ROW_COUNT ? contactIndex_ - ROW_COUNT + 1 : 0;
  for (uint8_t row = 0; row < ROW_COUNT && first + row < count; row++) {
    const uint8_t item = first + row;
    String label = item >= contactBook_.count() ? "Back" : contactBook_.get(item).label;
    if (item < contactBook_.count()) {
      char mac[18];
      contactBook_.displayMac(item, mac);
      label += "  ";
      label += mac;
    }
    TDisplayUi::row(tft, 34 + row * 22, label.c_str(), item == contactIndex_, TFT_YELLOW);
  }
  TDisplayUi::footer(tft, "B1 next/select  B2 prev");
}

template <typename Canvas>
void EspContactsApp::drawConfirmDelete(Canvas& tft) {
  TDisplayUi::header(tft, "Delete Contact?", TFT_RED);
  if (contactIndex_ < contactBook_.count()) {
    TDisplayUi::largeValue(tft, contactBook_.get(contactIndex_).label, 48, TFT_RED);
  }
  TDisplayUi::footer(tft, "B1 hold yes  B1/B2 tap no");
}

template <typename Canvas>
void EspContactsApp::drawFeedback(Canvas& tft) {
  TDisplayUi::header(tft, "ESP Contacts", feedbackColor_);
  TDisplayUi::largeValue(tft, feedbackText_, 54, feedbackColor_);
  TDisplayUi::footer(tft, "ESP-NOW contact info");
}
