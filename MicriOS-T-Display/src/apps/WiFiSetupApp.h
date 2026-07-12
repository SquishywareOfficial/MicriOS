#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <FS.h>
#include <IPAddress.h>

using fs::FS;

#include <WebServer.h>

#include "../../App.h"
#include "../shared/logic/WiFiLogic.h"

class WiFiSetupApp : public App {
  public:
    WiFiSetupApp(uint32_t width, uint32_t height);

    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool startsRunningImmediately() const override;

  private:
    enum class Mode {
      Main,
      StartingPortal,
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

    static constexpr uint8_t MAX_NETWORKS = 8;
    static constexpr uint8_t NO_PROFILE = 255;

    void markDirty();
    void startPortal();
    void stopPortal();
    void stopStationTest(bool powerDown);
    void setupRoutes();
    void scanNetworks();
    bool beginProfileTest(uint8_t index, bool testAll);
    bool beginNextProfileTest(uint8_t startIndex);
    uint8_t profileMenuCount() const;
    int8_t profileSlotForPosition(uint8_t position) const;
    void showMessage(const char* message, Mode returnMode);
    void updateConnectionTest(uint32_t deltaMs);
    void handleRoot();
    void handleSave();
    void handleNotFound();
    String buildPage(const char* notice) const;
    String escapeHtml(const String& value) const;
    const char* portalLabel() const;
    const char* testLabel() const;
    void drawMain(TFT_eSPI& tft);
    void drawStartingPortal(TFT_eSPI& tft);
    void drawConfigure(TFT_eSPI& tft);
    void drawProfileSelect(TFT_eSPI& tft, const char* title, uint16_t color, const char* action);
    void drawTesting(TFT_eSPI& tft);
    void drawConfirmDelete(TFT_eSPI& tft);
    void drawMessage(TFT_eSPI& tft);

    WiFiLogic logic_;
    WebServer server_;
    DNSServer dns_;
    IPAddress apIP_;
    Mode mode_ = Mode::Main;
    Mode messageReturnMode_ = Mode::Main;
    PortalState portalState_ = PortalState::Off;
    TestState testState_ = TestState::Idle;
    bool portalRunning_ = false;
    bool testingAll_ = false;
    bool deleteAll_ = false;
    bool portalLaunchPending_ = false;
    bool dirty_ = true;
    String networkNames_[MAX_NETWORKS];
    int32_t networkRssi_[MAX_NETWORKS] = {};
    uint8_t networkCount_ = 0;
    uint8_t mainIndex_ = 0;
    uint8_t profileMenuIndex_ = 0;
    uint8_t testProfileIndex_ = NO_PROFILE;
    uint8_t deleteProfileIndex_ = NO_PROFILE;
    uint32_t connectStartedAtMs_ = 0;
    uint32_t statusTimerMs_ = 0;
    uint32_t portalLaunchAtMs_ = 0;
    const char* message_ = "";
};
