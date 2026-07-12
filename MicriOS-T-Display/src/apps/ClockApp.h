#pragma once

#include "../../App.h"
#include "../shared/logic/ClockLogic.h"
#include "../shared/logic/WiFiLogic.h"

class ClockApp : public App {
  public:
    ClockApp(uint32_t width, uint32_t height);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
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
    void markDirty();
    bool shouldRedraw(time_t now);
    bool needsFullRedraw() const;
    void noteRendered(time_t now);
    void drawSecondTick(TFT_eSPI& tft, time_t now);
    void drawDigitalTick(TFT_eSPI& tft, time_t now);
    void drawAnalogTick(TFT_eSPI& tft, time_t now);
    void drawUnixTick(TFT_eSPI& tft, time_t now);
    void drawClockFooter(TFT_eSPI& tft, bool utc = false);
    template <typename Canvas>
    void drawClockFrame(Canvas& tft, time_t now);
    template <typename Canvas>
    void drawNetworkStatus(Canvas& tft);
    template <typename Canvas>
    void drawDigital(Canvas& tft, time_t now);
    template <typename Canvas>
    void drawAnalog(Canvas& tft, time_t now);
    template <typename Canvas>
    void drawUnix(Canvas& tft, time_t now);
    template <typename Canvas>
    void drawOptionsFace(Canvas& tft);
    template <typename Canvas>
    void drawOptions(Canvas& tft);
    template <typename Canvas>
    void drawTimezone(Canvas& tft);

    ClockLogic clock_;
    WiFiLogic wifi_;
    Mode mode_ = Mode::Splash;
    uint32_t modeStartedAtMs_ = 0;
    uint8_t currentSlot_ = 0;
    String currentSsid_;
    bool dirty_ = true;
    bool renderedOnce_ = false;
    Mode renderedMode_ = Mode::Splash;
    ClockLogic::View renderedView_ = ClockLogic::View::Face;
    ClockLogic::Face renderedFace_ = ClockLogic::Face::Digital;
    ClockLogic::Option renderedOption_ = ClockLogic::Option::ShowDate;
    uint8_t renderedZone_ = 255;
    uint8_t renderedZoneCount_ = 0;
    uint8_t renderedActiveZoneSlot_ = 255;
    time_t renderedSecond_ = 0;
    time_t lastSyncEpoch_ = 0;
    uint32_t lastSyncMillis_ = 0;
    bool hasSync_ = false;
};
