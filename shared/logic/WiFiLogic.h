#pragma once

#include <Arduino.h>
#include <Preferences.h>

class WiFiLogic {
public:
    static constexpr uint8_t MAX_PROFILES = 5;

    struct Profile {
        String ssid;
        bool used;
    };

    WiFiLogic() : profileCount_(0) {}

    void init() {
        loadProfiles();
    }

    void loadProfiles() {
        profileCount_ = 0;
        prefs_.begin("wifi", true);
        for (uint8_t i = 0; i < MAX_PROFILES; i++) {
            savedSsids_[i] = prefs_.getString(prefKey("ssid", i).c_str(), "");
            profileUsed_[i] = savedSsids_[i].length() > 0;
            if (profileUsed_[i]) {
                profileCount_++;
            }
        }
        prefs_.end();
    }

    int8_t saveProfile(const String& ssid, const String& pass) {
        int8_t slot = findProfile(ssid);
        if (slot < 0) slot = firstEmptyProfile();
        if (slot < 0) slot = 0;

        prefs_.begin("wifi", false);
        prefs_.putString(prefKey("ssid", slot).c_str(), ssid);
        prefs_.putString(prefKey("pass", slot).c_str(), pass);
        prefs_.end();

        loadProfiles();
        return slot;
    }

    void forgetProfile(uint8_t index) {
        if (index >= MAX_PROFILES) return;
        prefs_.begin("wifi", false);
        prefs_.remove(prefKey("ssid", index).c_str());
        prefs_.remove(prefKey("pass", index).c_str());
        prefs_.end();
        loadProfiles();
    }

    void forgetAllProfiles() {
        prefs_.begin("wifi", false);
        prefs_.clear();
        prefs_.end();
        loadProfiles();
    }

    int8_t findProfile(const String& ssid) const {
        for (uint8_t i = 0; i < MAX_PROFILES; i++) {
            if (profileUsed_[i] && savedSsids_[i] == ssid) return i;
        }
        return -1;
    }

    int8_t firstEmptyProfile() const {
        for (uint8_t i = 0; i < MAX_PROFILES; i++) {
            if (!profileUsed_[i]) return i;
        }
        return -1;
    }

    uint8_t getProfileCount() const { return profileCount_; }
    const String& getSsid(uint8_t index) const { return savedSsids_[index]; }
    bool isProfileUsed(uint8_t index) const { return profileUsed_[index]; }

    String getPass(uint8_t index) {
        prefs_.begin("wifi", true);
        String pass = prefs_.getString(prefKey("pass", index).c_str(), "");
        prefs_.end();
        return pass;
    }

private:
    String prefKey(const char* prefix, uint8_t index) const {
        return String(prefix) + index;
    }

    Preferences prefs_;
    String savedSsids_[MAX_PROFILES];
    bool profileUsed_[MAX_PROFILES];
    uint8_t profileCount_;
};
