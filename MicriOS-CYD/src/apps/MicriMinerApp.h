#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <FS.h>
#include <IPAddress.h>

using fs::FS;

#include <WebServer.h>

#include "../../App.h"
#include "../shared/logic/MinerLogic.h"

class MicriMinerApp : public App {
  public:
    MicriMinerApp(uint32_t width, uint32_t height);

    bool hasCustomOverlay() const override;
    uint16_t runningRenderIntervalMs() const override;
    bool wantsImmediateRender() const override;
    void setDebugAutoStart(bool enabled);
    void setEmitDebugLogs(bool enabled);
    bool emitDebugLogs() const;
    void debugStartMining();
    void debugStopMining();
    void debugPrintStats(const char* reason = "manual");
    bool handleTouch(const TouchUi::TouchSample& sample,
                     const TouchUi::TouchEvent& event) override;

  protected:
    void onAppReset() override;
    void onAppExit() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool startsRunningImmediately() const override;

  private:
    enum class Mode { Dashboard, StartingPortal, Portal, Message, ConfirmReset };
    enum class Page : uint8_t {
      Dashboard = 0,
      Shares,
      Session,
      Lifetime,
      Pool,
      Price,
      Settings,
      Reset,
      Count
    };

    struct LifetimeStats {
      uint64_t hashes = 0;
      uint64_t seconds = 0;
      double bestDifficulty = 0.0;
      uint32_t jobs = 0;
      uint32_t submitted = 0;
      uint32_t accepted = 0;
      uint32_t rejected = 0;
    };

    void markDirty();
    void forceClear();
    void launchPortal();
    void startPortal();
    void stopPortal();
    void setupRoutes();
    void handleRoot();
    void handleSave();
    void handleReset();
    void handleNotFound();
    String buildPage(const char* notice) const;
    String escapeHtml(const String& value) const;
    void showMessage(const char* message);
    const char* pageTitle() const;
    uint16_t stateColor(MinerLogic::State state) const;
    void loadLifetimeStats();
    void saveLifetimeStats();
    void persistRuntimeTotals(bool force = false);
    void resetSessionPersistMarkers();
    bool connectWifiForPrice();
    bool fetchBtcPrice();
    bool priceIsStale(uint32_t nowMs) const;
    void queueBtcPriceFetch();
    float cpuTemperatureC() const;
    String formatHashCount(uint64_t value) const;
    String formatDuration(uint64_t seconds) const;
    void maybePrintSerialStats(uint32_t nowMs);
    void printStatsLine(const MinerLogic::Stats& stats, const char* reason);

    template <typename Canvas>
    void drawFrame(Canvas& canvas);
    template <typename Canvas>
    void drawDashboard(Canvas& canvas, const MinerLogic::Stats& stats);
    template <typename Canvas>
    void drawShares(Canvas& canvas, const MinerLogic::Stats& stats);
    template <typename Canvas>
    void drawSession(Canvas& canvas, const MinerLogic::Stats& stats);
    template <typename Canvas>
    void drawLifetime(Canvas& canvas, const MinerLogic::Stats& stats);
    template <typename Canvas>
    void drawPool(Canvas& canvas, const MinerLogic::Stats& stats);
    template <typename Canvas>
    void drawPrice(Canvas& canvas);
    template <typename Canvas>
    void drawSettings(Canvas& canvas);
    template <typename Canvas>
    void drawReset(Canvas& canvas);
    template <typename Canvas>
    void drawPortal(Canvas& canvas);
    template <typename Canvas>
    void drawMessage(Canvas& canvas);
    template <typename Canvas>
    void drawConfirmReset(Canvas& canvas);
    template <typename Canvas>
    void drawPageControls(Canvas& canvas, const char* action,
                          uint16_t actionColor);

    MinerLogic::MinerEngine miner_;
    MinerLogic::MinerSettings settings_;
    WebServer server_;
    DNSServer dns_;
    IPAddress apIP_;
    Mode mode_ = Mode::Dashboard;
    Page page_ = Page::Dashboard;
    LifetimeStats lifetime_;
    uint64_t persistedSessionHashes_ = 0;
    uint64_t persistedSessionSeconds_ = 0;
    double persistedSessionBestDifficulty_ = 0.0;
    uint32_t persistedSessionJobs_ = 0;
    uint32_t persistedSessionSubmitted_ = 0;
    uint32_t persistedSessionAccepted_ = 0;
    uint32_t persistedSessionRejected_ = 0;
    bool portalRunning_ = false;
    bool portalLaunchPending_ = false;
    bool dirty_ = true;
    bool forceScreenClear_ = true;
    bool debugAutoStart_ = false;
    bool autoStartPending_ = false;
    bool emitDebugLogs_ = false;
    bool priceFetchPending_ = false;
    uint32_t modeStartedAtMs_ = 0;
    uint32_t lastSerialStatsMs_ = 0;
    uint32_t lastLifetimeSaveMs_ = 0;
    uint32_t btcPriceUsd_ = 0;
    uint32_t btcPriceFetchedAtMs_ = 0;
    char priceStatus_[40] = "Auto fetch on page";
    const char* message_ = "";
    TouchUi::ControlCapture touchCapture_;
};
