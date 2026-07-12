#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>
#include <string.h>

#include "CommunicatorLogic.h"

namespace EspContacts {

constexpr const char* PREF_NAMESPACE = "contacts";
constexpr uint8_t MAX_CONTACTS = 12;

struct Contact {
  char initials[3];
  char label[6];
  uint8_t mac[6];
  uint32_t lastSeen;
};

inline bool macEquals(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

inline void copyMac(uint8_t* dest, const uint8_t* src) {
  if (src == nullptr) {
    memset(dest, 0, 6);
  } else {
    memcpy(dest, src, 6);
  }
}

inline void normalizeInitials(char out[3], const char in[3]) {
  CommunicatorLogic::copyInitials(out, in);
}

class ContactBook {
public:
  void load() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    count_ = prefs.getUChar("count", 0);
    if (count_ > MAX_CONTACTS) count_ = MAX_CONTACTS;
    for (uint8_t i = 0; i < count_; i++) {
      char key[8];
      snprintf(key, sizeof(key), "i%u", i);
      String initials = prefs.getString(key, "AA");
      contacts_[i].initials[0] = initials.length() > 0 ? initials.charAt(0) : 'A';
      contacts_[i].initials[1] = initials.length() > 1 ? initials.charAt(1) : 'A';
      contacts_[i].initials[2] = '\0';
      normalizeInitials(contacts_[i].initials, contacts_[i].initials);

      snprintf(key, sizeof(key), "l%u", i);
      String label = prefs.getString(key, contacts_[i].initials);
      copyLabel(contacts_[i].label, label.c_str());

      snprintf(key, sizeof(key), "m%u", i);
      if (prefs.getBytesLength(key) == 6) {
        prefs.getBytes(key, contacts_[i].mac, 6);
      } else {
        memset(contacts_[i].mac, 0, 6);
      }

      snprintf(key, sizeof(key), "t%u", i);
      contacts_[i].lastSeen = prefs.getUInt(key, 0);
    }
    prefs.end();
  }

  void save() const {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.clear();
    prefs.putUChar("count", count_);
    for (uint8_t i = 0; i < count_; i++) {
      char key[8];
      snprintf(key, sizeof(key), "i%u", i);
      prefs.putString(key, contacts_[i].initials);
      snprintf(key, sizeof(key), "l%u", i);
      prefs.putString(key, contacts_[i].label);
      snprintf(key, sizeof(key), "m%u", i);
      prefs.putBytes(key, contacts_[i].mac, 6);
      snprintf(key, sizeof(key), "t%u", i);
      prefs.putUInt(key, contacts_[i].lastSeen);
    }
    prefs.end();
  }

  void clear() {
    count_ = 0;
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.clear();
    prefs.end();
  }

  uint8_t count() const { return count_; }

  const Contact& get(uint8_t index) const {
    return contacts_[index < count_ ? index : 0];
  }

  bool remove(uint8_t index) {
    if (index >= count_) return false;
    for (uint8_t i = index; i + 1 < count_; i++) {
      contacts_[i] = contacts_[i + 1];
    }
    count_--;
    rebuildLabels();
    save();
    return true;
  }

  int8_t findByMac(const uint8_t* mac) const {
    for (uint8_t i = 0; i < count_; i++) {
      if (macEquals(contacts_[i].mac, mac)) return static_cast<int8_t>(i);
    }
    return -1;
  }

  int8_t findByLabel(const char* label) const {
    for (uint8_t i = 0; i < count_; i++) {
      if (strncmp(contacts_[i].label, label, sizeof(contacts_[i].label)) == 0) return static_cast<int8_t>(i);
    }
    return -1;
  }

  int8_t upsert(const char initials[3], const uint8_t* mac, uint32_t nowMs) {
    char normalized[3];
    normalizeInitials(normalized, initials);

    const int8_t existing = findByMac(mac);
    if (existing >= 0) {
      normalizeInitials(contacts_[existing].initials, normalized);
      copyMac(contacts_[existing].mac, mac);
      contacts_[existing].lastSeen = nowMs;
      rebuildLabels();
      save();
      return existing;
    }

    if (count_ >= MAX_CONTACTS) {
      for (uint8_t i = 0; i + 1 < MAX_CONTACTS; i++) {
        contacts_[i] = contacts_[i + 1];
      }
      count_ = MAX_CONTACTS - 1;
    }

    Contact& contact = contacts_[count_];
    normalizeInitials(contact.initials, normalized);
    copyMac(contact.mac, mac);
    contact.lastSeen = nowMs;
    count_++;
    rebuildLabels();
    save();
    return static_cast<int8_t>(count_ - 1);
  }

  void displayMac(uint8_t index, char out[18]) const {
    if (index >= count_) {
      snprintf(out, 18, "--");
      return;
    }
    snprintf(out, 18, "%02X:%02X:%02X",
             contacts_[index].mac[3], contacts_[index].mac[4], contacts_[index].mac[5]);
  }

private:
  static void copyLabel(char out[6], const char* label) {
    strncpy(out, label == nullptr ? "AA" : label, 5);
    out[5] = '\0';
  }

  void rebuildLabels() {
    for (uint8_t i = 0; i < count_; i++) {
      uint8_t duplicateNumber = 1;
      for (uint8_t j = 0; j < i; j++) {
        if (CommunicatorLogic::initialsMatch(contacts_[i].initials, contacts_[j].initials)) {
          duplicateNumber++;
        }
      }
      if (duplicateNumber == 1) {
        snprintf(contacts_[i].label, sizeof(contacts_[i].label), "%c%c", contacts_[i].initials[0], contacts_[i].initials[1]);
      } else {
        snprintf(contacts_[i].label, sizeof(contacts_[i].label), "%c%c%u", contacts_[i].initials[0], contacts_[i].initials[1], duplicateNumber);
      }
    }
  }

  Contact contacts_[MAX_CONTACTS] = {};
  uint8_t count_ = 0;
};

} // namespace EspContacts
