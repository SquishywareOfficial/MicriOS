#include "CommunicatorApp.h"

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

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

CommunicatorApp* activeCommunicator = nullptr;
using Node = CommunicatorLogic::DictNode;

const Node QUICK_ITEMS[] = {
    {"OK", nullptr, 0}, {"Yes", nullptr, 0}, {"No", nullptr, 0},
    {"Thanks", nullptr, 0}, {"Done", nullptr, 0}, {"<3", nullptr, 0},
};

const Node SOCIAL_ITEMS[] = {
    {"hi", nullptr, 0}, {"hey", nullptr, 0}, {"how r u", nullptr, 0},
    {"im good", nullptr, 0}, {"miss u", nullptr, 0}, {"luv u", nullptr, 0},
    {"cya", nullptr, 0}, {"gn/gm", nullptr, 0},
};

const Node STATUS_ITEMS[] = {
    {"busy", nullptr, 0}, {"free", nullptr, 0}, {"later", nullptr, 0},
    {"soon", nullptr, 0}, {"now", nullptr, 0}, {"waiting", nullptr, 0},
    {"running late", nullptr, 0},
};

const Node LOCATION_ITEMS[] = {
    {"where r u", nullptr, 0}, {"im here", nullptr, 0}, {"come here", nullptr, 0},
    {"outside", nullptr, 0}, {"at home", nullptr, 0}, {"at work", nullptr, 0},
};

const Node BRING_DRINK_ITEMS[] = {
    {"water", nullptr, 0}, {"soda", nullptr, 0}, {"juice", nullptr, 0}, {"alcohol", nullptr, 0},
};

const Node BRING_FOOD_ITEMS[] = {
    {"snack", nullptr, 0}, {"meal", nullptr, 0}, {"fruit", nullptr, 0},
};

const Node BRING_DEVICE_ITEMS[] = {
    {"phone", nullptr, 0}, {"laptop", nullptr, 0}, {"charger", nullptr, 0},
};

const Node BRING_ITEM_ITEMS[] = {
    {"keys", nullptr, 0}, {"bag", nullptr, 0}, {"book", nullptr, 0}, {"pet", nullptr, 0},
};

const Node ACTION_BRING_ITEMS[] = {
    {"drink", BRING_DRINK_ITEMS, COUNT_OF(BRING_DRINK_ITEMS)},
    {"food", BRING_FOOD_ITEMS, COUNT_OF(BRING_FOOD_ITEMS)},
    {"device", BRING_DEVICE_ITEMS, COUNT_OF(BRING_DEVICE_ITEMS)},
    {"item", BRING_ITEM_ITEMS, COUNT_OF(BRING_ITEM_ITEMS)},
};

const Node ACTION_GET_ITEMS[] = {
    {"food", nullptr, 0}, {"drink", nullptr, 0}, {"item", nullptr, 0},
};

const Node ACTION_CALL_ITEMS[] = {
    {"me", nullptr, 0},
};

const Node ACTION_FIND_ITEMS[] = {
    {"phone", nullptr, 0}, {"keys", nullptr, 0},
};

const Node ACTION_ITEMS[] = {
    {"bring", ACTION_BRING_ITEMS, COUNT_OF(ACTION_BRING_ITEMS)},
    {"get", ACTION_GET_ITEMS, COUNT_OF(ACTION_GET_ITEMS)},
    {"call", ACTION_CALL_ITEMS, COUNT_OF(ACTION_CALL_ITEMS)},
    {"find", ACTION_FIND_ITEMS, COUNT_OF(ACTION_FIND_ITEMS)},
};

const Node STATEMENT_ITEMS[] = {
    {"here", nullptr, 0}, {"ready", nullptr, 0}, {"food ready", nullptr, 0},
    {"done", nullptr, 0}, {"waiting", nullptr, 0}, {"coming", nullptr, 0},
    {"arrived", nullptr, 0},
};

const Node ALERT_ITEMS[] = {
    {"urgent", nullptr, 0}, {"help", nullptr, 0}, {"problem", nullptr, 0},
    {"call now", nullptr, 0}, {"emergency", nullptr, 0},
};

const Node HOME_ITEMS[] = {
    {"come eat", nullptr, 0}, {"clean up", nullptr, 0}, {"im home", nullptr, 0},
    {"food ready", nullptr, 0}, {"bedtime", nullptr, 0},
};

const Node CATEGORY_ITEMS[] = {
    {"Quick", QUICK_ITEMS, COUNT_OF(QUICK_ITEMS)},
    {"Social", SOCIAL_ITEMS, COUNT_OF(SOCIAL_ITEMS)},
    {"Status", STATUS_ITEMS, COUNT_OF(STATUS_ITEMS)},
    {"Location", LOCATION_ITEMS, COUNT_OF(LOCATION_ITEMS)},
    {"Actions", ACTION_ITEMS, COUNT_OF(ACTION_ITEMS)},
    {"Statements", STATEMENT_ITEMS, COUNT_OF(STATEMENT_ITEMS)},
    {"Alerts", ALERT_ITEMS, COUNT_OF(ALERT_ITEMS)},
    {"Home", HOME_ITEMS, COUNT_OF(HOME_ITEMS)},
};
constexpr uint8_t CATEGORY_COUNT = COUNT_OF(CATEGORY_ITEMS);
}

CommunicatorApp::CommunicatorApp(uint32_t width, uint32_t height)
    : App("Communicator", width, height) {}

void CommunicatorApp::render(TFT_eSPI& tft) {
  const AppPhase currentPhase = phase();
  if (!phaseCached_ || currentPhase != renderedPhase_) {
    phaseCached_ = true;
    renderedPhase_ = currentPhase;
    dirty_ = true;
    startDirty_ = true;
  }
  App::render(tft);
}

void CommunicatorApp::markDirty() {
  dirty_ = true;
}

void CommunicatorApp::onAppReset() {
  loadIdentityAndContacts();
  mode_ = UiMode::Send;
  returnMode_ = UiMode::Send;
  depth_ = 0;
  index_ = 0;
  recipientIndex_ = 0;
  inboxIndex_ = 0;
  openInboxIndex_ = NO_MESSAGE;
  feedbackMs_ = 0;
  sendPending_ = false;
  sendSuccess_ = false;
  radioReady_ = beginRadio();
  markDirty();
}

void CommunicatorApp::loadIdentityAndContacts() {
  PlayerProfile::unpackInitials(PlayerProfile::loadInitials(), myInitials_);
  contactBook_.load();
}

bool CommunicatorApp::beginRadio() {
  shutdownRadio();

  Serial.println("Communicator: starting ESP-NOW");
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(40);

  WiFi.mode(WIFI_STA);
  delay(40);
  WiFi.disconnect(false, false);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  esp_err_t result = esp_wifi_set_ps(WIFI_PS_NONE);
  if (result != ESP_OK) {
    Serial.print("Communicator: wifi power-save off failed ");
    Serial.println(static_cast<int>(result));
  }

  result = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (result != ESP_OK) {
    Serial.print("Communicator: channel set failed ");
    Serial.println(static_cast<int>(result));
    WiFi.mode(WIFI_OFF);
    return false;
  }

  result = esp_now_init();
  if (result != ESP_OK) {
    Serial.print("Communicator: ESP-NOW init failed ");
    Serial.println(static_cast<int>(result));
    WiFi.mode(WIFI_OFF);
    return false;
  }
  radioStarted_ = true;

  result = esp_now_register_recv_cb(CommunicatorApp::onReceiveStatic);
  if (result != ESP_OK) {
    Serial.print("Communicator: recv callback failed ");
    Serial.println(static_cast<int>(result));
    shutdownRadio();
    return false;
  }

  result = esp_now_register_send_cb(CommunicatorApp::onSentStatic);
  if (result != ESP_OK) {
    Serial.print("Communicator: send callback failed ");
    Serial.println(static_cast<int>(result));
    shutdownRadio();
    return false;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;

  const esp_err_t peerResult = esp_now_add_peer(&peer);
  if (peerResult != ESP_OK && peerResult != ESP_ERR_ESPNOW_EXIST) {
    Serial.print("Communicator: broadcast peer failed ");
    Serial.println(static_cast<int>(peerResult));
    shutdownRadio();
    return false;
  }
  activeCommunicator = this;
  Serial.print("Communicator: ready MAC=");
  Serial.print(WiFi.macAddress());
  Serial.print(" channel=");
  Serial.println(ESPNOW_CHANNEL);
  return true;
}

void CommunicatorApp::shutdownRadio() {
  if (activeCommunicator == this) {
    activeCommunicator = nullptr;
  }
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

bool CommunicatorApp::ensurePeer(const uint8_t* mac) {
  if (mac == nullptr) {
    return false;
  }
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  const esp_err_t result = esp_now_add_peer(&peer);
  return result == ESP_OK || result == ESP_ERR_ESPNOW_EXIST;
}

void CommunicatorApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  if (sendPending_) {
    sendPending_ = false;
    showFeedback(sendSuccess_ ? "Sent" : "Failed", sendSuccess_ ? TFT_GREEN : TFT_RED);
  }

  if (mode_ == UiMode::Feedback) {
    feedbackMs_ += deltaMs;
    if (feedbackMs_ >= FEEDBACK_MS) {
      mode_ = returnMode_;
      feedbackMs_ = 0;
      markDirty();
    }
    return;
  }

  if (b2.click) {
    handleBack();
  } else if (b1.longPress) {
    handleHold();
  } else if (b1.click) {
    handleTap();
  }
}

void CommunicatorApp::handleTap() {
  if (mode_ == UiMode::Recipient) {
    const uint8_t count = contactBook_.count() + 1;
    recipientIndex_ = (recipientIndex_ + 1) % count;
    markDirty();
    return;
  }

  if (mode_ == UiMode::Inbox) {
    const uint8_t count = logic_.getInboxCount() + 1;
    inboxIndex_ = (inboxIndex_ + 1) % count;
    markDirty();
    return;
  }
  if (mode_ == UiMode::OpenMessage) {
    return;
  }
  uint8_t count = 0;
  currentList(count);
  if (depth_ == 0) {
    count++;
  }
  if (count > 0) {
    index_ = (index_ + 1) % count;
    markDirty();
  }
}

void CommunicatorApp::handleHold() {
  if (mode_ == UiMode::Recipient) {
    char to[3];
    const uint8_t pendingLength = pendingPacket_.length;
    uint8_t pendingPath[MAX_PATH] = {};
    memcpy(pendingPath, pendingPacket_.path, MAX_PATH);
    const uint8_t contactCount = contactBook_.count();
    const uint8_t boundedIndex = recipientIndex_ > contactCount ? 0 : recipientIndex_;
    if (boundedIndex == 0) {
      CommunicatorLogic::setAllRecipient(to);
      logic_.fillBasePacket(pendingPacket_, CommunicatorLogic::PacketType::Message, myInitials_, to);
      pendingPacket_.length = pendingLength;
      memcpy(pendingPacket_.path, pendingPath, MAX_PATH);
      sendPacket(pendingPacket_, BROADCAST_MAC);
    } else {
      const EspContacts::Contact& contact = contactBook_.get(boundedIndex - 1);
      CommunicatorLogic::copyInitials(to, contact.initials);
      logic_.fillBasePacket(pendingPacket_, CommunicatorLogic::PacketType::Message, myInitials_, to);
      pendingPacket_.length = pendingLength;
      memcpy(pendingPacket_.path, pendingPath, MAX_PATH);
      sendPacket(pendingPacket_, contact.mac);
    }
    return;
  }

  if (mode_ == UiMode::Inbox) {
    if (inboxIndex_ >= logic_.getInboxCount()) {
      mode_ = UiMode::Send;
      markDirty();
      return;
    }
    openInboxIndex_ = inboxIndex_;
    logic_.getInboxItem(openInboxIndex_).unread = false;
    mode_ = UiMode::OpenMessage;
    markDirty();
    return;
  }

  if (mode_ == UiMode::OpenMessage) {
    mode_ = UiMode::Inbox;
    openInboxIndex_ = NO_MESSAGE;
    markDirty();
    return;
  }

  if (depth_ == 0 && index_ == 0) {
    sendMyContact();
    return;
  }

  const Node* node = selectedNode();
  if (node == nullptr || depth_ >= MAX_PATH) {
    return;
  }
  path_[depth_] = depth_ == 0 ? index_ - 1 : index_;
  if (node->children != nullptr && node->childCount > 0 && depth_ + 1 < MAX_PATH) {
    depth_++;
    index_ = 0;
    markDirty();
    return;
  }
  beginRecipientSelect(depth_ + 1);
}

void CommunicatorApp::handleBack() {
  if (mode_ == UiMode::Recipient) {
    mode_ = UiMode::Send;
    recipientIndex_ = 0;
    markDirty();
    return;
  }

  if (mode_ == UiMode::OpenMessage) {
    mode_ = UiMode::Inbox;
    openInboxIndex_ = NO_MESSAGE;
    markDirty();
    return;
  }
  if (mode_ == UiMode::Inbox) {
    mode_ = UiMode::Send;
    markDirty();
    return;
  }
  if (depth_ > 0) {
    depth_--;
    index_ = depth_ == 0 ? path_[depth_] + 1 : path_[depth_];
    markDirty();
    return;
  }
  mode_ = UiMode::Inbox;
  inboxIndex_ = 0;
  markDirty();
}

void CommunicatorApp::sendCurrentPath(uint8_t length) {
  CommunicatorLogic::MessagePacket packet = {};
  char to[3];
  CommunicatorLogic::setAllRecipient(to);
  logic_.fillBasePacket(packet, CommunicatorLogic::PacketType::Message, myInitials_, to);
  packet.length = length;
  memcpy(packet.path, path_, MAX_PATH);
  pendingPacket_ = packet;
  mode_ = UiMode::Recipient;
  recipientIndex_ = 0;
  markDirty();
}

void CommunicatorApp::beginRecipientSelect(uint8_t length) {
  sendCurrentPath(length);
}

void CommunicatorApp::sendMyContact() {
  CommunicatorLogic::MessagePacket packet = {};
  char to[3];
  CommunicatorLogic::setAllRecipient(to);
  logic_.fillBasePacket(packet, CommunicatorLogic::PacketType::Contact, myInitials_, to);
  sendPacket(packet, BROADCAST_MAC);
}

bool CommunicatorApp::sendPacket(const CommunicatorLogic::MessagePacket& packet, const uint8_t* mac) {
  returnMode_ = UiMode::Send;
  if (!radioReady_ || !logic_.packetIsValid(packet, CATEGORY_ITEMS, CATEGORY_COUNT)) {
    showFeedback("Radio failed", TFT_RED);
    return false;
  }
  showFeedback("Sending...", TFT_YELLOW);
  if (!ensurePeer(mac)) {
    showFeedback("Peer failed", TFT_RED);
    return false;
  }
  const esp_err_t result = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  if (result != ESP_OK) {
    showFeedback("Send failed", TFT_RED);
    return false;
  }
  depth_ = 0;
  index_ = 0;
  return true;
}

void CommunicatorApp::onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (activeCommunicator != nullptr && info != nullptr) {
    activeCommunicator->handleIncoming(info->src_addr, data, len);
  }
}

void CommunicatorApp::onSentStatic(const esp_now_send_info_t* info, esp_now_send_status_t status) {
  (void)info;
  if (activeCommunicator != nullptr) {
    activeCommunicator->handleSendStatus(status == ESP_NOW_SEND_SUCCESS);
  }
}

void CommunicatorApp::handleIncoming(const uint8_t* from, const uint8_t* data, int len) {
  if (len != static_cast<int>(sizeof(CommunicatorLogic::MessagePacket))) {
    return;
  }
  CommunicatorLogic::MessagePacket packet = {};
  memcpy(&packet, data, sizeof(packet));
  if (!logic_.packetIsValid(packet, CATEGORY_ITEMS, CATEGORY_COUNT)) {
    return;
  }

  if (packet.type == static_cast<uint8_t>(CommunicatorLogic::PacketType::Contact)) {
    contactBook_.upsert(packet.from, from, millis());
    showFeedback("Contact saved", TFT_GREEN);
    return;
  }

  if (!logic_.packetIsForMe(packet, myInitials_)) {
    return;
  }
  contactBook_.upsert(packet.from, from, millis());
  logic_.pushInbox(packet, from);
  markDirty();
}

void CommunicatorApp::handleSendStatus(bool success) {
  sendSuccess_ = success;
  sendPending_ = true;
}

const Node* CommunicatorApp::currentList(uint8_t& count) const {
  if (depth_ == 0) {
    count = CATEGORY_COUNT;
    return CATEGORY_ITEMS;
  }
  const Node* node = nodeAtPath(path_, depth_);
  if (node == nullptr || node->children == nullptr) {
    count = 0;
    return nullptr;
  }
  count = node->childCount;
  return node->children;
}

const Node* CommunicatorApp::selectedNode() const {
  uint8_t count = 0;
  const Node* list = currentList(count);
  const uint8_t nodeIndex = depth_ == 0 ? index_ - 1 : index_;
  if (list == nullptr || (depth_ == 0 && index_ == 0) || nodeIndex >= count) {
    return nullptr;
  }
  return &list[nodeIndex];
}

const Node* CommunicatorApp::nodeAtPath(const uint8_t* path, uint8_t length) const {
  return logic_.nodeAtPath(path, length, CATEGORY_ITEMS, CATEGORY_COUNT);
}

const char* CommunicatorApp::packetLeafLabel(const CommunicatorLogic::MessagePacket& packet) const {
  const Node* node = nodeAtPath(packet.path, packet.length);
  return node == nullptr ? "?" : node->label;
}

const char* CommunicatorApp::currentTitle() const {
  if (depth_ == 0) {
    return "Send";
  }
  const Node* node = nodeAtPath(path_, depth_);
  return node == nullptr ? "Send" : node->label;
}

void CommunicatorApp::showFeedback(const char* text, uint16_t color) {
  feedbackText_ = text;
  feedbackColor_ = color;
  feedbackMs_ = 0;
  mode_ = UiMode::Feedback;
  markDirty();
}

void CommunicatorApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    if (mode_ == UiMode::Feedback) {
      drawFeedback(canvas);
    } else if (mode_ == UiMode::Recipient) {
      drawRecipient(canvas);
    } else if (mode_ == UiMode::Inbox) {
      drawInbox(canvas);
    } else if (mode_ == UiMode::OpenMessage) {
      drawOpenMessage(canvas);
    } else {
      drawSend(canvas);
    }
  });
  dirty_ = false;
}

void CommunicatorApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Communicator", TFT_CYAN);
  TDisplayUi::centered(tft, "ESP-NOW", 47, 3, TFT_WHITE);
  TDisplayUi::centered(tft, "peer messages", 80, 2, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 start");
  startDirty_ = false;
}

template <typename Canvas>
void CommunicatorApp::drawSend(Canvas& tft) {
  TDisplayUi::header(tft, currentTitle(), radioReady_ ? TFT_CYAN : TFT_RED, radioReady_ ? "radio" : "!");
  uint8_t count = 0;
  const Node* list = currentList(count);
  if (depth_ == 0) {
    count++;
  }
  const uint8_t first = index_ >= ROW_COUNT ? index_ - ROW_COUNT + 1 : 0;
  for (uint8_t row = 0; row < ROW_COUNT && first + row < count; row++) {
    const uint8_t item = first + row;
    const char* label = "Send Name";
    if (depth_ > 0 || item > 0) {
      const uint8_t nodeIndex = depth_ == 0 ? item - 1 : item;
      label = (list != nullptr && nodeIndex < (depth_ == 0 ? count - 1 : count)) ? list[nodeIndex].label : "?";
    }
    TDisplayUi::row(tft, 34 + row * 22, label, item == index_, TFT_CYAN);
  }
  TDisplayUi::footer(tft, "B1 next/hold select  B2 back/inbox");
}

const char* CommunicatorApp::recipientLabel(uint8_t index) const {
  if (index == 0) {
    return "ALL";
  }
  if (index - 1 < contactBook_.count()) {
    return contactBook_.get(index - 1).label;
  }
  return "?";
}

template <typename Canvas>
void CommunicatorApp::drawRecipient(Canvas& tft) {
  TDisplayUi::header(tft, "Send To", TFT_CYAN, packetLeafLabel(pendingPacket_));
  const uint8_t count = contactBook_.count() + 1;
  const uint8_t first = recipientIndex_ >= ROW_COUNT ? recipientIndex_ - ROW_COUNT + 1 : 0;
  for (uint8_t row = 0; row < ROW_COUNT && first + row < count; row++) {
    const uint8_t item = first + row;
    TDisplayUi::row(tft, 34 + row * 22, recipientLabel(item), item == recipientIndex_, TFT_CYAN);
  }
  TDisplayUi::footer(tft, "B1 next/hold send  B2 back");
}

template <typename Canvas>
void CommunicatorApp::drawInbox(Canvas& tft) {
  const String stat = String(logic_.getInboxCount()) + " msg";
  TDisplayUi::header(tft, "Inbox", TFT_YELLOW, stat.c_str());
  const uint8_t count = logic_.getInboxCount() + 1;
  const uint8_t first = inboxIndex_ >= ROW_COUNT ? inboxIndex_ - ROW_COUNT + 1 : 0;
  for (uint8_t row = 0; row < ROW_COUNT && first + row < count; row++) {
    const uint8_t item = first + row;
    String label;
    if (item >= logic_.getInboxCount()) {
      label = "Back to Send";
    } else {
      auto& inbox = logic_.getInboxItem(item);
      label = String(inbox.unread ? "* " : "  ") + packetLeafLabel(inbox.packet);
    }
    TDisplayUi::row(tft, 34 + row * 22, label.c_str(), item == inboxIndex_, TFT_YELLOW);
  }
  TDisplayUi::footer(tft, "B1 next/hold open  B2 send");
}

template <typename Canvas>
void CommunicatorApp::drawOpenMessage(Canvas& tft) {
  TDisplayUi::header(tft, "Message", TFT_YELLOW);
  if (openInboxIndex_ < logic_.getInboxCount()) {
    auto& inbox = logic_.getInboxItem(openInboxIndex_);
    String route = String(inbox.packet.from[0]) + String(inbox.packet.from[1]) + " > " +
                   (CommunicatorLogic::isAllRecipient(inbox.packet.to) ? "ALL" : String(inbox.packet.to[0]) + String(inbox.packet.to[1]));
    TDisplayUi::centered(tft, route, 36, 2, TFT_LIGHTGREY);
    TDisplayUi::largeValue(tft, packetLeafLabel(inbox.packet), 61, TFT_WHITE);
    String mac;
    for (uint8_t i = 3; i < 6; i++) {
      if (i > 3) mac += ":";
      if (inbox.from[i] < 16) mac += "0";
      mac += String(inbox.from[i], HEX);
    }
    TDisplayUi::centered(tft, (logic_.firmwareMismatch(inbox.packet) ? "v! " : "") + String("from ...") + mac, 101, 1, TFT_LIGHTGREY);
  }
  TDisplayUi::footer(tft, "B1/B2 back");
}

template <typename Canvas>
void CommunicatorApp::drawFeedback(Canvas& tft) {
  TDisplayUi::header(tft, "Communicator", feedbackColor_);
  TDisplayUi::largeValue(tft, feedbackText_, 55, feedbackColor_);
  TDisplayUi::footer(tft, "ESP-NOW broadcast");
}
