#include "PlayerProfile.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {
Preferences profilePrefs;

uint16_t packInitials(char first, char second) {
  return (static_cast<uint16_t>(PlayerProfile::normalizeInitial(first)) << 8) |
         static_cast<uint8_t>(PlayerProfile::normalizeInitial(second));
}
}

namespace PlayerProfile {
uint16_t defaultInitials() {
  return packInitials('J', 'F');
}

uint16_t loadInitials() {
  profilePrefs.begin("profile", true);
  const uint16_t packed = profilePrefs.getUShort("init", defaultInitials());
  profilePrefs.end();
  return packed;
}

void saveInitials(char first, char second) {
  profilePrefs.begin("profile", false);
  profilePrefs.putUShort("init", packInitials(first, second));
  profilePrefs.end();
}

void unpackInitials(uint16_t packed, char out[3]) {
  out[0] = normalizeInitial(static_cast<char>((packed >> 8) & 0xff));
  out[1] = normalizeInitial(static_cast<char>(packed & 0xff));
  out[2] = '\0';
}

void unpackDottedInitials(uint16_t packed, char out[4]) {
  out[0] = normalizeInitial(static_cast<char>((packed >> 8) & 0xff));
  out[1] = '.';
  out[2] = normalizeInitial(static_cast<char>(packed & 0xff));
  out[3] = '\0';
}

char normalizeInitial(char value) {
  if (value >= 'a' && value <= 'z') {
    value = static_cast<char>(value - 'a' + 'A');
  }
  if (value >= 'A' && value <= 'Z') {
    return value;
  }
  if (value >= '0' && value <= '9') {
    return value;
  }
  return 'A';
}

}
