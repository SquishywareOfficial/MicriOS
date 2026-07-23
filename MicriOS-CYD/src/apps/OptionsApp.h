#pragma once

#include "../../App.h"
#include "../../CydKidModeUi.h"
#include "../../CydUi.h"
#include "../shared/logic/KidModeStorage.h"

class OptionsApp : public App {
  public:
    OptionsApp(uint32_t width, uint32_t height,
               KidMode::Service& kidModeService,
               const KidMode::Storage& kidModeStorage);
    bool hasCustomOverlay() const override;
    void render(TFT_eSPI& tft) override;
    bool handleTouch(const TouchUi::TouchSample& sample,
                     const TouchUi::TouchEvent& event) override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;
    bool startsRunningImmediately() const override;
    void onAppExit() override;

  private:
    enum class Mode {
      Main,
      Initials,
      TextSize,
      Saves,
      ConfirmOne,
      ConfirmAll1,
      ConfirmAll2,
      Message,
      ChildSetupPin,
      ChildSetupConfirm,
      ChildAuth,
      ChildManage,
      ChildSplashMenu,
      ChildSplashText,
      ChildSplashPalette,
      ChildSplashPreview,
      ChildDuration,
      ChildChangePin,
      ChildChangeConfirm,
      ChildConfirmDisable
    };

    enum class ParentAction {
      OpenManagement,
      DeleteChildMode,
      DeleteAll
    };

    char nextInitial(char value) const;
    char previousInitial(char value) const;
    void clearNamespace(const char* ns);
    void clearSelectedSave();
    void clearAllSaves();
    void drawFit(TFT_eSPI& tft, int x, int y, const char* text);
    void markDirty();
    void beginChildSetup();
    void beginParentAuth(ParentAction action);
    void handleChildPinAction(int16_t id, uint64_t nowUs);
    void selectChildDuration(uint8_t index, uint64_t nowUs);
    void clearChildEntry();
    String childStatus(uint64_t nowUs) const;
    bool childPinMode() const;
    bool saveChildSplash();

    Mode mode_ = Mode::Main;
    char initials_[3] = {'A', 'A', '\0'};
    uint8_t selected_ = 0;
    uint8_t mainIndex_ = 0;
    uint8_t saveIndex_ = 0;
    uint8_t savePage_ = 0;
    CydUi::TextSize textSize_ = CydUi::TextSize::Compact;
    const char* message_ = "";
    bool messageToMain_ = false;
    bool dirty_ = true;
    bool startDirty_ = true;
    bool endDirty_ = true;
    bool runningRendered_ = false;
    bool phaseCached_ = false;
    AppPhase renderedPhase_ = AppPhase::Start;
    Mode renderedMode_ = Mode::Main;
    CydUi::TextSize renderedTextSize_ = CydUi::TextSize::Compact;
    uint8_t renderedSaveIndex_ = 255;
    uint8_t renderedSaveScroll_ = 255;
    TouchUi::ControlCapture touchCapture_;
    KidMode::Service& kidModeService_;
    const KidMode::Storage& kidModeStorage_;
    KidMode::PinEntry childPinEntry_;
    KidMode::AuthLockout childAuthLockout_;
    KidMode::SplashSettings childSplashSettings_;
    KidMode::SplashTextEditor childSplashTextEditor_;
    KidMode::SplashPalette pendingSplashPalette_ = KidMode::SplashPalette::Candy;
    bool childSplashShift_ = true;
    ParentAction parentAction_ = ParentAction::OpenManagement;
    char pendingPin_[KidMode::PIN_LENGTH + 1] = {};
    String childPrompt_;
    uint32_t renderedLockoutSeconds_ = UINT32_MAX;
    bool pendingInitialEnable_ = false;
};
