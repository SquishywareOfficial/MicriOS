#include "CommunicatorApp.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t ESPNOW_CHANNEL = 6;
constexpr uint16_t DOUBLE_TAP_MS = 360;
constexpr uint16_t FEEDBACK_MS = 900;
constexpr uint8_t ROW_COUNT = 5;
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

CommunicatorApp* activeCommunicator = nullptr;

using Node = CommunicatorApp::DictNode;

const Node QUICK_ITEMS[] = {
    {"OK", nullptr, 0},
    {"Yes", nullptr, 0},
    {"No", nullptr, 0},
    {"Thanks", nullptr, 0},
    {"Done", nullptr, 0},
    {"<3", nullptr, 0},
};

const Node SOCIAL_ITEMS[] = {
    {"hi", nullptr, 0},
    {"hey", nullptr, 0},
    {"how r u", nullptr, 0},
    {"im good", nullptr, 0},
    {"miss u", nullptr, 0},
    {"luv u", nullptr, 0},
    {"cya", nullptr, 0},
    {"gn/gm", nullptr, 0},
};

const Node STATUS_ITEMS[] = {
    {"busy", nullptr, 0},
    {"free", nullptr, 0},
    {"later", nullptr, 0},
    {"soon", nullptr, 0},
    {"now", nullptr, 0},
    {"waiting", nullptr, 0},
    {"running late", nullptr, 0},
};

const Node LOCATION_ITEMS[] = {
    {"where r u", nullptr, 0},
    {"im here", nullptr, 0},
    {"come here", nullptr, 0},
    {"outside", nullptr, 0},
    {"at home", nullptr, 0},
    {"at work", nullptr, 0},
};

const Node BRING_DRINK_ITEMS[] = {
    {"water", nullptr, 0},
    {"soda", nullptr, 0},
    {"juice", nullptr, 0},
    {"alcohol", nullptr, 0},
};

const Node BRING_FOOD_ITEMS[] = {
    {"snack", nullptr, 0},
    {"meal", nullptr, 0},
    {"fruit", nullptr, 0},
};

const Node BRING_DEVICE_ITEMS[] = {
    {"phone", nullptr, 0},
    {"laptop", nullptr, 0},
    {"charger", nullptr, 0},
};

const Node BRING_ITEM_ITEMS[] = {
    {"keys", nullptr, 0},
    {"bag", nullptr, 0},
    {"book", nullptr, 0},
    {"pet", nullptr, 0},
};

const Node ACTION_BRING_ITEMS[] = {
    {"drink", BRING_DRINK_ITEMS, COUNT_OF(BRING_DRINK_ITEMS)},
    {"food", BRING_FOOD_ITEMS, COUNT_OF(BRING_FOOD_ITEMS)},
    {"device", BRING_DEVICE_ITEMS, COUNT_OF(BRING_DEVICE_ITEMS)},
    {"item", BRING_ITEM_ITEMS, COUNT_OF(BRING_ITEM_ITEMS)},
};

const Node ACTION_GET_ITEMS[] = {
    {"food", nullptr, 0},
    {"drink", nullptr, 0},
    {"item", nullptr, 0},
};

const Node ACTION_CALL_ITEMS[] = {
    {"me", nullptr, 0},
};

const Node ACTION_FIND_ITEMS[] = {
    {"phone", nullptr, 0},
    {"keys", nullptr, 0},
};

const Node ACTION_ITEMS[] = {
    {"bring", ACTION_BRING_ITEMS, COUNT_OF(ACTION_BRING_ITEMS)},
    {"get", ACTION_GET_ITEMS, COUNT_OF(ACTION_GET_ITEMS)},
    {"call", ACTION_CALL_ITEMS, COUNT_OF(ACTION_CALL_ITEMS)},
    {"find", ACTION_FIND_ITEMS, COUNT_OF(ACTION_FIND_ITEMS)},
};

const Node STATEMENT_ITEMS[] = {
    {"here", nullptr, 0},
    {"ready", nullptr, 0},
    {"food ready", nullptr, 0},
    {"done", nullptr, 0},
    {"waiting", nullptr, 0},
    {"coming", nullptr, 0},
    {"arrived", nullptr, 0},
};

const Node ALERT_ITEMS[] = {
    {"urgent", nullptr, 0},
    {"help", nullptr, 0},
    {"problem", nullptr, 0},
    {"call now", nullptr, 0},
    {"emergency", nullptr, 0},
};

const Node HOME_ITEMS[] = {
    {"come eat", nullptr, 0},
    {"clean up", nullptr, 0},
    {"im home", nullptr, 0},
    {"food ready", nullptr, 0},
    {"bedtime", nullptr, 0},
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

CommunicatorApp::CommunicatorApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Communicator", width, height), left_(left) {}

bool CommunicatorApp::hasCustomOverlay() const {
  return true;
}

bool CommunicatorApp::startsRunningImmediately() const {
  return true;
}

void CommunicatorApp::onAppReset() {
  Serial.begin(115200);
  loadIdentityAndContacts();
  uiMode_ = UiMode::Send;
  feedback_ = SendFeedback::None;
  sendDepth_ = 0;
  sendIndex_ = 0;
  recipientIndex_ = 0;
  inboxIndex_ = 0;
  openInboxIndex_ = NO_MESSAGE;
  feedbackMs_ = 0;
  sendStatusPending_ = false;
  sendStatusSuccess_ = false;
  resetClickState();
  radioReady_ = beginRadio();
}

void CommunicatorApp::loadIdentityAndContacts() {
  PlayerProfile::unpackInitials(PlayerProfile::loadInitials(), myInitials_);
  contactBook_.load();
}

bool CommunicatorApp::beginRadio() {
  shutdownRadio();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  const esp_err_t initResult = esp_now_init();
  if (initResult != ESP_OK) {
    Serial.print("Communicator ESP-NOW init failed: ");
    Serial.println(static_cast<int>(initResult));
    return false;
  }

  esp_now_register_recv_cb(CommunicatorApp::onReceiveStatic);
  esp_now_register_send_cb(CommunicatorApp::onSentStatic);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;

  const esp_err_t peerResult = esp_now_add_peer(&peer);
  if (peerResult != ESP_OK && peerResult != ESP_ERR_ESPNOW_EXIST) {
    Serial.print("Communicator broadcast peer failed: ");
    Serial.println(static_cast<int>(peerResult));
    esp_now_deinit();
    return false;
  }

  activeCommunicator = this;
  Serial.print("Communicator ready, MAC=");
  Serial.print(WiFi.macAddress());
  Serial.print(", channel=");
  Serial.println(ESPNOW_CHANNEL);
  return true;
}

void CommunicatorApp::shutdownRadio() {
  if (activeCommunicator == this) {
    activeCommunicator = nullptr;
  }
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  esp_now_del_peer(BROADCAST_MAC);
  esp_now_deinit();
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

void CommunicatorApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  const uint32_t now = millis();
  bool singleTap = false;
  bool doubleTap = false;

  if (sendStatusPending_) {
    sendStatusPending_ = false;
    feedback_ = sendStatusSuccess_ ? SendFeedback::Sent : SendFeedback::Failed;
    feedbackMs_ = 0;
    Serial.println(sendStatusSuccess_ ? "Communicator send done" : "Communicator send failed");
  }

  if (input.click) {
    if (clickPending_ && (now - clickPendingAtMs_) <= DOUBLE_TAP_MS) {
      clickPending_ = false;
      doubleTap = true;
    } else {
      clickPending_ = true;
      clickPendingAtMs_ = now;
    }
  }

  if (clickPending_ && (now - clickPendingAtMs_) > DOUBLE_TAP_MS) {
    clickPending_ = false;
    singleTap = true;
  }

  if (input.longPress) {
    resetClickState();
    handleHold();
  } else if (doubleTap) {
    handleDoubleTap();
  } else if (singleTap) {
    handleTap();
  }

  if (feedback_ != SendFeedback::None) {
    feedbackMs_ += deltaMs;
    if (feedbackMs_ >= FEEDBACK_MS && feedback_ != SendFeedback::Sending) {
      feedback_ = SendFeedback::None;
      feedbackMs_ = 0;
    }
  }
}

void CommunicatorApp::resetClickState() {
  clickPending_ = false;
  clickPendingAtMs_ = 0;
}

void CommunicatorApp::handleTap() {
  if (feedback_ == SendFeedback::Sending) {
    return;
  }

  if (uiMode_ == UiMode::Send) {
    uint8_t count = 0;
    currentSendList(count);
    if (sendDepth_ == 0) {
      count = rootVisibleCount();
    }
    if (count > 0) {
      sendIndex_ = (sendIndex_ + 1) % count;
    }
    return;
  }

  if (uiMode_ == UiMode::Recipient) {
    const uint8_t count = contactBook_.count() + 1;
    recipientIndex_ = (recipientIndex_ + 1) % count;
    return;
  }

  if (openInboxIndex_ != NO_MESSAGE) {
    return;
  }

  const uint8_t count = inboxCount_ + 1;
  inboxIndex_ = (inboxIndex_ + 1) % count;
}

void CommunicatorApp::handleHold() {
  if (feedback_ == SendFeedback::Sending) {
    return;
  }

  if (uiMode_ == UiMode::Recipient) {
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
    uiMode_ = UiMode::Send;
    sendDepth_ = 0;
    sendIndex_ = 0;
    return;
  }

  if (uiMode_ == UiMode::Inbox) {
    if (openInboxIndex_ != NO_MESSAGE) {
      return;
    }
    if (inboxIndex_ >= inboxCount_) {
      shutdownRadio();
      requestExitToMenu();
      return;
    }
    openInboxIndex_ = inboxIndex_;
    inbox_[openInboxIndex_].unread = false;
    return;
  }

  if (selectedIsRepeat()) {
    pendingPacket_ = lastSent_;
    uiMode_ = UiMode::Recipient;
    recipientIndex_ = 0;
    return;
  }

  if (selectedIsSendName()) {
    sendMyContact();
    return;
  }

  const DictNode* node = selectedSendNode();
  if (node == nullptr) {
    return;
  }

  const uint8_t nodeIndex = selectedNodeIndex();
  if (sendDepth_ >= MAX_PATH) {
    return;
  }
  sendPath_[sendDepth_] = nodeIndex;

  if (node->childCount > 0 && sendDepth_ + 1 < MAX_PATH) {
    sendDepth_++;
    sendIndex_ = 0;
    return;
  }

  beginRecipientSelect(sendDepth_ + 1);
}

void CommunicatorApp::handleDoubleTap() {
  resetClickState();

  if (feedback_ == SendFeedback::Sending) {
    return;
  }

  if (uiMode_ == UiMode::Send) {
    if (sendDepth_ > 0) {
      sendDepth_--;
      sendIndex_ = sendDepth_ == 0 ? sendPath_[sendDepth_] + rootPrefixCount() : sendPath_[sendDepth_];
    } else {
      uiMode_ = UiMode::Inbox;
      inboxIndex_ = 0;
      openInboxIndex_ = NO_MESSAGE;
    }
    return;
  }

  if (uiMode_ == UiMode::Recipient) {
    uiMode_ = UiMode::Send;
    recipientIndex_ = 0;
    return;
  }

  if (openInboxIndex_ != NO_MESSAGE) {
    openInboxIndex_ = NO_MESSAGE;
  } else {
    uiMode_ = UiMode::Send;
  }
}

void CommunicatorApp::sendCurrentPath(uint8_t length) {
  MessagePacket packet = {};
  char to[3];
  CommunicatorLogic::setAllRecipient(to);
  logic_.fillBasePacket(packet, CommunicatorLogic::PacketType::Message, myInitials_, to);
  packet.length = length;
  memcpy(packet.path, sendPath_, MAX_PATH);
  pendingPacket_ = packet;
  uiMode_ = UiMode::Recipient;
  recipientIndex_ = 0;
}

void CommunicatorApp::beginRecipientSelect(uint8_t length) {
  sendCurrentPath(length);
}

void CommunicatorApp::sendMyContact() {
  MessagePacket packet = {};
  char to[3];
  CommunicatorLogic::setAllRecipient(to);
  logic_.fillBasePacket(packet, CommunicatorLogic::PacketType::Contact, myInitials_, to);
  sendPacket(packet, BROADCAST_MAC);
}

bool CommunicatorApp::sendPacket(const MessagePacket& packet, const uint8_t* mac) {
  if (!radioReady_ || !packetIsValid(packet)) {
    feedback_ = SendFeedback::Failed;
    feedbackMs_ = 0;
    return false;
  }

  if (packet.type == static_cast<uint8_t>(CommunicatorLogic::PacketType::Message)) {
    lastSent_ = packet;
    hasLastSent_ = true;
  }
  feedback_ = SendFeedback::Sending;
  feedbackMs_ = 0;
  sendStatusPending_ = false;

  if (!ensurePeer(mac)) {
    feedback_ = SendFeedback::Failed;
    feedbackMs_ = 0;
    Serial.println("Communicator peer add failed");
    return false;
  }

  const esp_err_t result = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  if (result != ESP_OK) {
    feedback_ = SendFeedback::Failed;
    feedbackMs_ = 0;
    Serial.print("Communicator send start failed: ");
    Serial.println(static_cast<int>(result));
    return false;
  }

  Serial.print("Communicator sent: ");
  Serial.println(packet.type == static_cast<uint8_t>(CommunicatorLogic::PacketType::Contact) ? "contact" : packetLeafLabel(packet));
  return true;
}

void CommunicatorApp::onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (activeCommunicator == nullptr || info == nullptr) {
    return;
  }
  activeCommunicator->handleIncoming(info->src_addr, data, len);
}

void CommunicatorApp::onSentStatic(const esp_now_send_info_t* info, esp_now_send_status_t status) {
  (void)info;
  if (activeCommunicator != nullptr) {
    activeCommunicator->handleSendStatus(status == ESP_NOW_SEND_SUCCESS);
  }
}

void CommunicatorApp::handleIncoming(const uint8_t* from, const uint8_t* data, int len) {
  if (len != static_cast<int>(sizeof(MessagePacket))) {
    return;
  }

  MessagePacket packet = {};
  memcpy(&packet, data, sizeof(packet));
  if (!packetIsValid(packet)) {
    return;
  }

  if (packet.type == static_cast<uint8_t>(CommunicatorLogic::PacketType::Contact)) {
    contactBook_.upsert(packet.from, from, millis());
    feedback_ = SendFeedback::ContactSaved;
    feedbackMs_ = 0;
    Serial.print("Communicator saved contact: ");
    Serial.println(packet.from);
    return;
  }

  if (!logic_.packetIsForMe(packet, myInitials_)) {
    return;
  }
  contactBook_.upsert(packet.from, from, millis());
  pushInbox(packet, from);
  Serial.print("Communicator received: ");
  Serial.println(packetLeafLabel(packet));
}

void CommunicatorApp::handleSendStatus(bool success) {
  sendStatusSuccess_ = success;
  sendStatusPending_ = true;
}

void CommunicatorApp::pushInbox(const MessagePacket& packet, const uint8_t* from) {
  if (inboxCount_ < INBOX_SIZE) {
    inboxCount_++;
  }
  for (int8_t i = inboxCount_ - 1; i > 0; i--) {
    inbox_[i] = inbox_[i - 1];
  }
  inbox_[0].packet = packet;
  copyMac(inbox_[0].from, from);
  inbox_[0].receivedMs = millis();
  inbox_[0].unread = true;
  inboxIndex_ = 0;
}

bool CommunicatorApp::packetIsValid(const MessagePacket& packet) const {
  return logic_.packetIsValid(packet, CATEGORY_ITEMS, CATEGORY_COUNT);
}

const CommunicatorApp::DictNode* CommunicatorApp::nodeAtPath(const uint8_t* path, uint8_t length) const {
  if (length == 0 || path[0] >= CATEGORY_COUNT) {
    return nullptr;
  }

  const DictNode* node = &CATEGORY_ITEMS[path[0]];
  for (uint8_t i = 1; i < length; i++) {
    if (node->children == nullptr || path[i] >= node->childCount) {
      return nullptr;
    }
    node = &node->children[path[i]];
  }
  return node;
}

const CommunicatorApp::DictNode* CommunicatorApp::currentSendList(uint8_t& count) const {
  if (sendDepth_ == 0) {
    count = CATEGORY_COUNT;
    return CATEGORY_ITEMS;
  }

  const DictNode* node = nodeAtPath(sendPath_, sendDepth_);
  if (node == nullptr || node->children == nullptr) {
    count = 0;
    return nullptr;
  }
  count = node->childCount;
  return node->children;
}

const CommunicatorApp::DictNode* CommunicatorApp::selectedSendNode() const {
  if (selectedIsRepeat()) {
    return nullptr;
  }

  uint8_t count = 0;
  const DictNode* list = currentSendList(count);
  const uint8_t index = selectedNodeIndex();
  if (list == nullptr || index >= count) {
    return nullptr;
  }
  return &list[index];
}

uint8_t CommunicatorApp::selectedNodeIndex() const {
  if (sendDepth_ == 0) {
    const uint8_t prefix = rootPrefixCount();
    return sendIndex_ < prefix ? 0 : sendIndex_ - prefix;
  }
  return sendIndex_;
}

const char* CommunicatorApp::packetLeafLabel(const MessagePacket& packet) const {
  const DictNode* node = nodeAtPath(packet.path, packet.length);
  return node == nullptr ? "?" : node->label;
}

const char* CommunicatorApp::nodeLabelAt(const MessagePacket& packet, uint8_t index) const {
  const DictNode* node = nodeAtPath(packet.path, index + 1);
  return node == nullptr ? "" : node->label;
}

void CommunicatorApp::copyMac(uint8_t* dest, const uint8_t* src) const {
  if (src == nullptr) {
    memset(dest, 0, 6);
  } else {
    memcpy(dest, src, 6);
  }
}

bool CommunicatorApp::selectedIsRepeat() const {
  return uiMode_ == UiMode::Send && sendDepth_ == 0 && hasLastSent_ && sendIndex_ == 1;
}

bool CommunicatorApp::selectedIsSendName() const {
  return uiMode_ == UiMode::Send && sendDepth_ == 0 && sendIndex_ == 0;
}

uint8_t CommunicatorApp::rootPrefixCount() const {
  return 1 + (hasLastSent_ ? 1 : 0);
}

uint8_t CommunicatorApp::rootVisibleCount() const {
  return CATEGORY_COUNT + rootPrefixCount();
}

void CommunicatorApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  if (feedback_ != SendFeedback::None) {
    drawFeedback(u8g2);
    return;
  }

  if (uiMode_ == UiMode::Inbox && openInboxIndex_ != NO_MESSAGE) {
    drawOpenMessage(u8g2);
  } else if (uiMode_ == UiMode::Inbox) {
    drawInbox(u8g2);
  } else if (uiMode_ == UiMode::Recipient) {
    drawRecipient(u8g2);
  } else {
    drawSend(u8g2);
  }
}

void CommunicatorApp::drawHeader(U8G2& u8g2, const char* title) const {
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 7, title);
  if (!radioReady_) {
    u8g2.drawStr(width - 8, 7, "!");
  }
  u8g2.drawHLine(1, 9, width);
}

void CommunicatorApp::drawSend(U8G2& u8g2) {
  drawHeader(u8g2, sendDepth_ == 0 ? "SEND" : nodeAtPath(sendPath_, sendDepth_)->label);
  u8g2.setFont(u8g2_font_4x6_tr);

  uint8_t count = 0;
  const DictNode* list = currentSendList(count);
  if (sendDepth_ == 0) {
    count = rootVisibleCount();
  }

  const uint8_t first = sendIndex_ >= ROW_COUNT ? sendIndex_ - ROW_COUNT + 1 : 0;
  for (uint8_t row = 0; row < ROW_COUNT && first + row < count; row++) {
    const uint8_t item = first + row;
    const int y = 16 + row * 6;
    u8g2.drawStr(3, y, item == sendIndex_ ? ">" : " ");

    char label[18] = {};
    if (sendDepth_ == 0 && item == 0) {
      snprintf(label, sizeof(label), "Send Name");
    } else if (sendDepth_ == 0 && hasLastSent_ && item == 1) {
      snprintf(label, sizeof(label), "Last:%s", packetLeafLabel(lastSent_));
    } else {
      const uint8_t nodeIndex = sendDepth_ == 0 ? item - rootPrefixCount() : item;
      if (list != nullptr && nodeIndex < (sendDepth_ == 0 ? CATEGORY_COUNT : count)) {
        snprintf(label, sizeof(label), "%s", list[nodeIndex].label);
      }
    }
    drawFit(u8g2, 10, y, label, 15);
  }
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

void CommunicatorApp::drawRecipient(U8G2& u8g2) {
  drawHeader(u8g2, "SEND TO");
  u8g2.setFont(u8g2_font_4x6_tr);

  const uint8_t count = contactBook_.count() + 1;
  const uint8_t first = recipientIndex_ >= ROW_COUNT ? recipientIndex_ - ROW_COUNT + 1 : 0;
  for (uint8_t row = 0; row < ROW_COUNT && first + row < count; row++) {
    const uint8_t item = first + row;
    const int y = 16 + row * 6;
    u8g2.drawStr(3, y, item == recipientIndex_ ? ">" : " ");
    drawFit(u8g2, 10, y, recipientLabel(item), 15);
  }
}

void CommunicatorApp::drawInbox(U8G2& u8g2) {
  drawHeader(u8g2, "INBOX");
  u8g2.setFont(u8g2_font_4x6_tr);

  const uint8_t count = inboxCount_ + 1;
  const uint8_t first = inboxIndex_ >= ROW_COUNT ? inboxIndex_ - ROW_COUNT + 1 : 0;
  for (uint8_t row = 0; row < ROW_COUNT && first + row < count; row++) {
    const uint8_t item = first + row;
    const int y = 16 + row * 6;
    u8g2.drawStr(3, y, item == inboxIndex_ ? ">" : " ");
    if (item >= inboxCount_) {
      u8g2.drawStr(10, y, "Exit");
    } else {
      char label[18] = {};
      snprintf(label, sizeof(label), "%c %s", inbox_[item].unread ? '*' : '<', packetLeafLabel(inbox_[item].packet));
      drawFit(u8g2, 10, y, label, 15);
    }
  }
}

void CommunicatorApp::drawOpenMessage(U8G2& u8g2) {
  if (openInboxIndex_ >= inboxCount_) {
    openInboxIndex_ = NO_MESSAGE;
    return;
  }

  const MessagePacket& packet = inbox_[openInboxIndex_].packet;
  drawHeader(u8g2, "MESSAGE");
  u8g2.setFont(u8g2_font_4x6_tr);
  char line[18] = {};
  snprintf(line, sizeof(line), "%c%c>%s", packet.from[0], packet.from[1],
           CommunicatorLogic::isAllRecipient(packet.to) ? "ALL" : packet.to);
  drawFit(u8g2, 8, 15, line, 14);
  for (uint8_t i = 0; i < packet.length && i < 3; i++) {
    drawFit(u8g2, 8, 22 + i * 6, nodeLabelAt(packet, i), 14);
  }
  if (logic_.firmwareMismatch(packet)) {
    u8g2.drawStr(46, 39, "v!");
  }
  u8g2.drawStr(3, 39, "2tap");
}

void CommunicatorApp::drawFeedback(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_7x13B_tr);
  const char* text = "Sending";
  if (feedback_ == SendFeedback::Sent) {
    text = "Sent";
  } else if (feedback_ == SendFeedback::Failed) {
    text = "Failed";
  } else if (feedback_ == SendFeedback::ContactSaved) {
    text = "Saved";
  }
  const int textW = u8g2.getStrWidth(text);
  u8g2.drawStr((width + 2 - textW) / 2, 18, text);

  u8g2.setFont(u8g2_font_4x6_tr);
  const char* label = hasLastSent_ ? packetLeafLabel(lastSent_) : "";
  drawFit(u8g2, 5, 32, label, 15);
}

void CommunicatorApp::drawFit(U8G2& u8g2, int x, int y, const char* text, uint8_t maxChars) const {
  char buffer[20] = {};
  strncpy(buffer, text, sizeof(buffer) - 1);
  if (strlen(buffer) > maxChars) {
    buffer[maxChars - 1] = '~';
    buffer[maxChars] = '\0';
  }
  u8g2.drawStr(x, y, buffer);
}
