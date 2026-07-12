#pragma once

#include <esp_now.h>

#include "../../App.h"
#include "../shared/logic/CommunicatorLogic.h"
#include "../shared/logic/EspContactsLogic.h"

class EspContactsApp : public App {
  public:
    EspContactsApp(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override { return true; }
    bool startsRunningImmediately() const override { return true; }
    void render(TFT_eSPI& tft) override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;

  private:
    enum class Mode {
      Main,
      Listen,
      Manage,
      ConfirmDelete,
      Feedback
    };

    bool beginRadio();
    void shutdownRadio();
    bool sendMyContact();
    void handleIncoming(const uint8_t* from, const uint8_t* data, int len);
    void handleSendStatus(bool success);
    void showFeedback(const char* text, uint16_t color, Mode returnMode);
    void markDirty();
    template <typename Canvas> void drawMain(Canvas& tft);
    template <typename Canvas> void drawListen(Canvas& tft);
    template <typename Canvas> void drawManage(Canvas& tft);
    template <typename Canvas> void drawConfirmDelete(Canvas& tft);
    template <typename Canvas> void drawFeedback(Canvas& tft);

    static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void onSentStatic(const esp_now_send_info_t* info, esp_now_send_status_t status);

    CommunicatorLogic logic_;
    EspContacts::ContactBook contactBook_;
    Mode mode_ = Mode::Main;
    Mode feedbackReturnMode_ = Mode::Main;
    uint8_t mainIndex_ = 0;
    uint8_t contactIndex_ = 0;
    bool radioReady_ = false;
    bool radioStarted_ = false;
    bool sendPending_ = false;
    bool sendSuccess_ = false;
    bool dirty_ = true;
    bool phaseCached_ = false;
    AppPhase renderedPhase_ = AppPhase::Start;
    char myInitials_[3] = {'J', 'F', '\0'};
    char lastSeen_[6] = "";
    const char* feedbackText_ = "";
    uint16_t feedbackColor_ = 0;
    uint16_t feedbackMs_ = 0;
};
