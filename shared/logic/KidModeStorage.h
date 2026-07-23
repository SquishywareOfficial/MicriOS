#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>

#include "KidModeLogic.h"

namespace KidMode {

class Storage {
 public:
  static constexpr const char* NAMESPACE = "kidmode";
  static constexpr const char* RECOVERY_PIN = "420420";

  struct Config {
    bool configured = false;
    bool enabled = false;
  };

  Config load() const {
    Config config;
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return config;
    const bool valid =
        prefs.getUChar(KEY_VERSION, 0) == SCHEMA_VERSION &&
        prefs.getBytesLength(KEY_SALT) == SALT_BYTES &&
        prefs.getBytesLength(KEY_HASH) == HASH_BYTES;
    config.configured = valid;
    config.enabled = valid && prefs.getBool(KEY_ENABLED, false);
    prefs.end();
    return config;
  }

  SplashSettings loadSplash() const {
    SplashSettings settings;
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return settings;
    settings.enabled = prefs.getBool(KEY_SPLASH_ENABLED, false);
    const uint8_t storedPalette = prefs.getUChar(
        KEY_SPLASH_PALETTE, static_cast<uint8_t>(SplashPalette::Candy));
    settings.palette = storedPalette < static_cast<uint8_t>(SplashPalette::Count)
                           ? static_cast<SplashPalette>(storedPalette)
                           : SplashPalette::Candy;
    const String storedText = prefs.getString(KEY_SPLASH_TEXT,
                                               DEFAULT_SPLASH_TEXT);
    prefs.end();

    SplashTextEditor editor;
    editor.begin(storedText.c_str());
    if (editor.empty()) editor.begin(DEFAULT_SPLASH_TEXT);
    strncpy(settings.text, editor.value(), sizeof(settings.text));
    settings.text[sizeof(settings.text) - 1] = '\0';
    return settings;
  }

  bool saveSplash(const SplashSettings& settings) const {
    SplashTextEditor editor;
    editor.begin(settings.text);
    if (editor.empty() || !validSplashPalette(settings.palette)) return false;

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return false;
    prefs.putBool(KEY_SPLASH_ENABLED, settings.enabled);
    prefs.putUChar(KEY_SPLASH_PALETTE,
                   static_cast<uint8_t>(settings.palette));
    const size_t textLength = prefs.putString(KEY_SPLASH_TEXT, editor.value());
    prefs.end();
    return textLength == editor.length();
  }

  bool configureAndEnable(const char* pin) const {
    if (!isSixDigitPin(pin)) return false;

    uint8_t salt[SALT_BYTES];
    uint8_t hash[HASH_BYTES];
    esp_fill_random(salt, sizeof(salt));
    calculateHash(salt, pin, hash);

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return false;
    const bool stored =
        prefs.putUChar(KEY_VERSION, SCHEMA_VERSION) == sizeof(uint8_t) &&
        prefs.putBytes(KEY_SALT, salt, sizeof(salt)) == sizeof(salt) &&
        prefs.putBytes(KEY_HASH, hash, sizeof(hash)) == sizeof(hash) &&
        prefs.putBool(KEY_ENABLED, true) == sizeof(bool);
    prefs.end();
    return stored;
  }

  bool verify(const char* pin) const {
    if (!isSixDigitPin(pin)) return false;
    if (constantTimeEqual(reinterpret_cast<const uint8_t*>(pin),
                          reinterpret_cast<const uint8_t*>(RECOVERY_PIN),
                          PIN_LENGTH)) {
      return true;
    }

    uint8_t salt[SALT_BYTES];
    uint8_t expected[HASH_BYTES];
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;
    const bool valid =
        prefs.getUChar(KEY_VERSION, 0) == SCHEMA_VERSION &&
        prefs.getBytesLength(KEY_SALT) == sizeof(salt) &&
        prefs.getBytesLength(KEY_HASH) == sizeof(expected) &&
        prefs.getBytes(KEY_SALT, salt, sizeof(salt)) == sizeof(salt) &&
        prefs.getBytes(KEY_HASH, expected, sizeof(expected)) == sizeof(expected);
    prefs.end();
    if (!valid) return false;

    uint8_t actual[HASH_BYTES];
    calculateHash(salt, pin, actual);
    return constantTimeEqual(actual, expected, sizeof(actual));
  }

  void clear() const {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return;
    prefs.clear();
    prefs.end();
  }

 private:
  static constexpr uint8_t SCHEMA_VERSION = 1;
  static constexpr size_t SALT_BYTES = 16;
  static constexpr size_t HASH_BYTES = 32;
  static constexpr const char* KEY_VERSION = "ver";
  static constexpr const char* KEY_ENABLED = "enabled";
  static constexpr const char* KEY_SALT = "salt";
  static constexpr const char* KEY_HASH = "hash";
  static constexpr const char* KEY_SPLASH_ENABLED = "spl_on";
  static constexpr const char* KEY_SPLASH_PALETTE = "spl_pal";
  static constexpr const char* KEY_SPLASH_TEXT = "spl_text";

  static void calculateHash(const uint8_t salt[SALT_BYTES], const char* pin,
                            uint8_t out[HASH_BYTES]) {
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    mbedtls_sha256_update(&context, salt, SALT_BYTES);
    mbedtls_sha256_update(
        &context, reinterpret_cast<const uint8_t*>(pin), PIN_LENGTH);
    mbedtls_sha256_finish(&context, out);
    mbedtls_sha256_free(&context);
  }

  static bool constantTimeEqual(const uint8_t* first, const uint8_t* second,
                                size_t length) {
    uint8_t difference = 0;
    for (size_t i = 0; i < length; ++i) difference |= first[i] ^ second[i];
    return difference == 0;
  }
};

}  // namespace KidMode
