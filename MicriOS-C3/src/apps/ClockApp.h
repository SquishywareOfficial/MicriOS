#pragma once

#include "../../App.h"
#include "../shared/logic/ClockLogic.h"
#include "../shared/logic/WiFiLogic.h"

class ClockApp : public App {
  public:
    ClockApp(uint32_t width, uint32_t height, uint32_t left = 1);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    bool startsRunningImmediately() const override;
    bool hasCustomOverlay() const override;

  private:
    enum class Mode { Splash, NoProfiles, Connecting, Connected, Syncing, ConnectFailed, SyncFailed, Clock };

    void beginRetry();
    bool beginNextProfile(uint8_t startSlot);
    void beginSync();
    void enterClock();
    void stopWifi();
    bool timeIsValid() const;
    bool hasUsableTime() const;
    void noteSync();
    time_t currentTime() const;
    uint32_t syncAgeSeconds() const;
    void syncLabel(char* out, size_t outSize) const;
    void handleClockAction(ClockLogic::Action action);
    void drawFit(U8G2& u8g2, int x, int y, const char* text) const;
    void drawNetworkStatus(U8G2& u8g2);
    void drawDigital(U8G2& u8g2, time_t now);
    void drawAnalog(U8G2& u8g2, time_t now);
    void drawUnix(U8G2& u8g2, time_t now);
    void drawOptionsFace(U8G2& u8g2);
    void drawOptions(U8G2& u8g2);
    void drawTimezone(U8G2& u8g2);

    ClockLogic clock_;
    WiFiLogic wifi_;
    Mode mode_ = Mode::Splash;
    uint32_t modeStartedAtMs_ = 0;
    uint32_t timezoneIdleMs_ = 0;
    uint8_t currentSlot_ = 0;
    String currentSsid_;
    time_t lastSyncEpoch_ = 0;
    uint32_t lastSyncMillis_ = 0;
    bool hasSync_ = false;
};
