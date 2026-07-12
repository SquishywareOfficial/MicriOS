#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct MouseIdentityProfile {
  const char* manufacturer;
  const char* deviceName;
  const char* shortName;
  uint16_t vendorId;
  uint16_t productId;
};

class MouseIdentityLogic {
  public:
    static constexpr uint8_t PROFILE_COUNT = 5;

    static const MouseIdentityProfile& profile(uint8_t index) {
      if (index >= PROFILE_COUNT) {
        index = 0;
      }
      return profiles()[index];
    }

    static uint8_t loadIndex() {
      Preferences prefs;
      prefs.begin("mouse", true);
      const uint8_t index = prefs.getUChar("profile", 0);
      prefs.end();
      return index < PROFILE_COUNT ? index : 0;
    }

    static void saveIndex(uint8_t index) {
      Preferences prefs;
      prefs.begin("mouse", false);
      prefs.putUChar("profile", index < PROFILE_COUNT ? index : 0);
      prefs.end();
    }

  private:
    static const MouseIdentityProfile* profiles() {
      static const MouseIdentityProfile items[PROFILE_COUNT] = {
          {"Logitech", "Logitech Signature M650", "Logitech M650", 0x046d, 0xb02a},
          {"Lenovo", "ThinkPad Wireless Mouse", "ThinkPad Mouse", 0x17ef, 0x608d},
          {"Dell", "MS5320W Multi-Device", "Dell MS5320W", 0x413c, 0xb06a},
          {"Generic", "Wireless Mouse", "Wireless Mouse", 0xe502, 0xa111},
          {"Apple", "Apple Magic Mouse", "Magic Mouse", 0x05ac, 0x030d},
      };
      return items;
    }
};
