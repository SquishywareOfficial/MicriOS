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
    MicriMinerApp(uint32_t width, uint32_t height, uint32_t left = 1);

    bool hasCustomOverlay() const override;
    uint16_t runningRenderIntervalMs() const override;

  protected:
    void onAppReset() override;
    void onAppExit() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    bool startsRunningImmediately() const override;

  private:
    enum class Mode { Pages, StartingPortal, Portal, Message };

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
    void drawFit(U8G2& u8g2, int x, int y, const char* text) const;
    void showMessage(const char* message);
    void loadBestDifficulty();
    void saveBestDifficulty();
    void persistBestDifficulty(bool force = false);

    MinerLogic::MinerEngine miner_;
    MinerLogic::MinerSettings settings_;
    WebServer server_;
    DNSServer dns_;
    IPAddress apIP_;
    uint32_t left_;
    Mode mode_ = Mode::Pages;
    uint8_t page_ = 0;
    bool portalRunning_ = false;
    bool portalLaunchPending_ = false;
    double lifetimeBestDifficulty_ = 0.0;
    double savedBestDifficulty_ = 0.0;
    uint32_t modeStartedAtMs_ = 0;
    uint32_t lastBestSaveMs_ = 0;
    const char* message_ = "";
};
