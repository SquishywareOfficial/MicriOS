#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <IPAddress.h>
#include <Preferences.h>
#include <WebServer.h>

#include "../../App.h"

class WiFiSetupApp : public App {
  public:
    WiFiSetupApp(uint32_t width, uint32_t height, uint32_t left);

    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    bool startsRunningImmediately() const override;

  private:
    enum class Mode {
      Main,
      ConfigurePortal,
      TestSelect,
      Testing,
      DeleteSelect,
      ConfirmDelete,
      Message
    };

    enum class PortalState {
      Off,
      Starting,
      Running,
      Error
    };

    enum class TestState {
      Idle,
      Connecting,
      Connected,
      Failed
    };

    void startPortal();
    void stopPortal();
    void stopStationTest(bool powerDown);
    void setupRoutes();
    void scanNetworks();
    void loadProfiles();
    int8_t findProfile(const String& ssid) const;
    int8_t firstEmptyProfile() const;
    int8_t saveProfile(const String& ssid, const String& pass);
    void forgetProfile(uint8_t index);
    void forgetAllProfiles();
    bool beginProfileTest(uint8_t index, bool testAll);
    bool beginNextProfileTest(uint8_t startIndex);
    uint8_t profileMenuCount() const;
    int8_t profileSlotForPosition(uint8_t position) const;
    void showMessage(const char* message, Mode returnMode);
    void updateConnectionTest(uint32_t deltaMs);
    void updateLed(uint32_t deltaMs);
    void setSetupLed(bool on);
    void handleRoot();
    void handleSave();
    void handleNotFound();
    String buildPage(const char* notice) const;
    String escapeHtml(const String& value) const;
    String prefKey(const char* prefix, uint8_t index) const;
    const char* portalLabel() const;
    const char* testLabel() const;
    void drawFit(U8G2& u8g2, int x, int y, const char* text) const;
    void drawMain(U8G2& u8g2);
    void drawConfigure(U8G2& u8g2);
    void drawProfileSelect(U8G2& u8g2, const char* title, const char* action);
    void drawTesting(U8G2& u8g2);
    void drawConfirmDelete(U8G2& u8g2);
    void drawMessage(U8G2& u8g2);

    static constexpr uint8_t MAX_NETWORKS = 8;
    static constexpr uint8_t MAX_PROFILES = 5;
    static constexpr uint8_t NO_PROFILE = 255;

    uint32_t left_;
    WebServer server_;
    DNSServer dns_;
    Preferences prefs_;
    IPAddress apIP_;
    Mode mode_ = Mode::Main;
    Mode messageReturnMode_ = Mode::Main;
    PortalState portalState_ = PortalState::Off;
    TestState testState_ = TestState::Idle;
    bool portalRunning_ = false;
    bool testingAll_ = false;
    bool ledBlinkOn_ = false;
    bool deleteAll_ = false;
    String savedSsids_[MAX_PROFILES];
    bool profileUsed_[MAX_PROFILES] = {};
    String networkNames_[MAX_NETWORKS];
    int32_t networkRssi_[MAX_NETWORKS] = {};
    uint8_t networkCount_ = 0;
    uint8_t profileCount_ = 0;
    uint8_t mainIndex_ = 0;
    uint8_t profileMenuIndex_ = 0;
    uint8_t testProfileIndex_ = NO_PROFILE;
    uint8_t deleteProfileIndex_ = NO_PROFILE;
    uint32_t connectStartedAtMs_ = 0;
    uint32_t statusTimerMs_ = 0;
    uint32_t ledTimerMs_ = 0;
    const char* message_ = "";
};
