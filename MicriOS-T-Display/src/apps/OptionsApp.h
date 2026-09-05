#pragma once

#include "../../App.h"
#include "../../TDisplayIdleSettings.h"
#include "../../TDisplayUi.h"
#include "../../TDisplayPower.h"
#include "../shared/logic/ClockLogic.h"

class OptionsApp : public App {
  public:
    enum class EntryPoint {
      Main,
      Initials,
      TextSize,
      Saves,
      Power,
      Brightness
    };

    OptionsApp(uint32_t width, uint32_t height);
    void setEntryPoint(EntryPoint entryPoint);
    bool hasCustomOverlay() const override;
    void render(TFT_eSPI& tft) override;

  protected:
    void onAppReset() override;
    void onAppExit() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;
    bool startsRunningImmediately() const override;

  private:
    enum class Mode {
      Main,
      Power,
      Initials,
      TextSize,
      Saves,
      ConfirmOne,
      ConfirmAll1,
      ConfirmAll2,
      BatteryInstalled,
      Battery,
      BatteryRuntime,
      FullVoltage,
      IdleDisplay,
      SaverTimeout,
      DimTimeout,
      ScreenOffTimeout,
      Brightness,
      Message
    };

    char nextInitial(char value) const;
    void clearNamespace(const char* ns);
    void clearSelectedSave();
    void clearAllSaves();
    void drawFit(TFT_eSPI& tft, int x, int y, const char* text);
    void markDirty();

    Mode mode_ = Mode::Main;
    char initials_[3] = {'J', 'F', '\0'};
    uint8_t selected_ = 0;
    uint8_t mainIndex_ = 0;
    uint8_t powerIndex_ = 0;
    uint8_t idleDisplayIndex_ = 0;
    uint8_t saveIndex_ = 0;
    TDisplayUi::TextSize textSize_ = TDisplayUi::TextSize::Compact;
    const char* message_ = "";
    bool messageToMain_ = false;
    bool messageToPower_ = false;
    EntryPoint requestedEntryPoint_ = EntryPoint::Main;
    bool directEntry_ = false;
    bool batteryInstalled_ = false;
    TDisplayPower::BatteryReading batteryReading_;
    TDisplayIdleSettings::Config idleDisplayConfig_;
    uint16_t editedTimeoutMinutes_ = 0;
    ClockLogic clockSettings_;
    uint8_t brightnessLevel_ = 8;
    uint32_t telemetryRefreshMs_ = 0;
    bool dirty_ = true;
    bool startDirty_ = true;
    bool endDirty_ = true;
    bool runningRendered_ = false;
    bool phaseCached_ = false;
    AppPhase renderedPhase_ = AppPhase::Start;
    Mode renderedMode_ = Mode::Main;
    TDisplayUi::TextSize renderedTextSize_ = TDisplayUi::TextSize::Compact;
    uint8_t renderedSaveIndex_ = 255;
    uint8_t renderedSaveScroll_ = 255;
};
