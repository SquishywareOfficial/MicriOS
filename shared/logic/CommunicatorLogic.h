#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <esp_now.h>
#include "../Version.h"

class CommunicatorLogic {
public:
    static constexpr uint8_t MAX_PATH = 4;
    static constexpr uint8_t INBOX_SIZE = 8;
    static constexpr uint8_t COMM_MAGIC_0 = 'F';
    static constexpr uint8_t COMM_MAGIC_1 = 'C';
    static constexpr uint8_t COMM_VERSION = 2;
    static constexpr char ALL_TO_0 = '*';
    static constexpr char ALL_TO_1 = '*';

    enum class PacketType : uint8_t {
      Message = 1,
      Contact = 2
    };

    struct MessagePacket {
      uint8_t magic0;
      uint8_t magic1;
      uint8_t version;
      uint8_t type;
      uint8_t firmwareMajor;
      uint8_t firmwareMinor;
      uint16_t firmwareBuild;
      char from[3];
      char to[3];
      uint8_t length;
      uint8_t path[MAX_PATH];
      uint32_t nonce;
    } __attribute__((packed));

    struct InboxItem {
      MessagePacket packet;
      uint8_t from[6];
      uint32_t receivedMs;
      bool unread;
    };

    struct DictNode {
      const char* label;
      const DictNode* children;
      uint8_t childCount;
    };

    CommunicatorLogic() : inboxCount_(0), nonce_(1) {}

    static void setAllRecipient(char out[3]) {
      out[0] = ALL_TO_0;
      out[1] = ALL_TO_1;
      out[2] = '\0';
    }

    static bool isAllRecipient(const char to[3]) {
      return to[0] == ALL_TO_0 && to[1] == ALL_TO_1;
    }

    static bool initialsMatch(const char a[3], const char b[3]) {
      return normalizeInitial(a[0]) == normalizeInitial(b[0]) &&
             normalizeInitial(a[1]) == normalizeInitial(b[1]);
    }

    static char normalizeInitial(char value) {
      if (value >= 'a' && value <= 'z') {
        value = static_cast<char>(value - 'a' + 'A');
      }
      if ((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')) {
        return value;
      }
      return 'A';
    }

    static void copyInitials(char out[3], const char in[3]) {
      out[0] = normalizeInitial(in[0]);
      out[1] = normalizeInitial(in[1]);
      out[2] = '\0';
    }

    void fillBasePacket(MessagePacket& packet, PacketType type, const char from[3], const char to[3]) {
      memset(&packet, 0, sizeof(packet));
      packet.magic0 = COMM_MAGIC_0;
      packet.magic1 = COMM_MAGIC_1;
      packet.version = COMM_VERSION;
      packet.type = static_cast<uint8_t>(type);
      packet.firmwareMajor = BuildInfo::VERSION_MAJOR;
      packet.firmwareMinor = BuildInfo::VERSION_MINOR;
      packet.firmwareBuild = BuildInfo::BUILD_NUMBER;
      copyInitials(packet.from, from);
      if (to == nullptr) {
        setAllRecipient(packet.to);
      } else if (isAllRecipient(to)) {
        setAllRecipient(packet.to);
      } else {
        copyInitials(packet.to, to);
      }
      packet.nonce = nextNonce();
    }

    bool packetHasHeader(const MessagePacket& packet) const {
      return packet.magic0 == COMM_MAGIC_0 && packet.magic1 == COMM_MAGIC_1 && packet.version == COMM_VERSION;
    }

    bool packetIsValid(const MessagePacket& packet, const DictNode* categories, uint8_t categoryCount) const {
      if (!packetHasHeader(packet)) return false;
      if (packet.type == static_cast<uint8_t>(PacketType::Contact)) {
        return packet.from[0] != '\0' && packet.from[1] != '\0';
      }
      return packet.type == static_cast<uint8_t>(PacketType::Message) &&
             packet.length > 0 && packet.length <= MAX_PATH &&
             nodeAtPath(packet.path, packet.length, categories, categoryCount) != nullptr;
    }

    bool packetIsForMe(const MessagePacket& packet, const char myInitials[3]) const {
      return isAllRecipient(packet.to) || initialsMatch(packet.to, myInitials);
    }

    bool firmwareMismatch(const MessagePacket& packet) const {
      return packet.firmwareMajor != BuildInfo::VERSION_MAJOR ||
             packet.firmwareMinor != BuildInfo::VERSION_MINOR ||
             packet.firmwareBuild != BuildInfo::BUILD_NUMBER;
    }

    const DictNode* nodeAtPath(const uint8_t* path, uint8_t length, const DictNode* categories, uint8_t categoryCount) const {
      if (length == 0 || path[0] >= categoryCount) return nullptr;
      const DictNode* node = &categories[path[0]];
      for (uint8_t i = 1; i < length; i++) {
        if (node->children == nullptr || path[i] >= node->childCount) return nullptr;
        node = &node->children[path[i]];
      }
      return node;
    }

    void pushInbox(const MessagePacket& packet, const uint8_t* from) {
      if (inboxCount_ < INBOX_SIZE) inboxCount_++;
      for (int8_t i = inboxCount_ - 1; i > 0; i--) inbox_[i] = inbox_[i - 1];
      inbox_[0].packet = packet;
      memcpy(inbox_[0].from, from, 6);
      inbox_[0].receivedMs = millis();
      inbox_[0].unread = true;
    }

    uint8_t getInboxCount() const { return inboxCount_; }
    InboxItem& getInboxItem(uint8_t index) { return inbox_[index]; }
    uint32_t nextNonce() { return nonce_++; }

private:
    InboxItem inbox_[INBOX_SIZE];
    uint8_t inboxCount_;
    uint32_t nonce_;
};
