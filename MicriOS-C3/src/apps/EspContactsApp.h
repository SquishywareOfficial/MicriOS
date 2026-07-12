#pragma once

#include <esp_now.h>

#include "../../App.h"
#include "../shared/logic/CommunicatorLogic.h"
#include "../shared/logic/EspContactsLogic.h"

class EspContactsApp : public App {
  public:
    EspContactsApp(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;
    bool startsRunningImmediately() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;

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
    void showFeedback(const char* text, Mode returnMode);
    void drawHeader(U8G2& u8g2, const char* title) const;
    void drawFit(U8G2& u8g2, int x, int y, const char* text, uint8_t maxChars) const;
    void drawMain(U8G2& u8g2);
    void drawListen(U8G2& u8g2);
    void drawManage(U8G2& u8g2);
    void drawConfirmDelete(U8G2& u8g2);
    void drawFeedback(U8G2& u8g2);

    static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void onSentStatic(const esp_now_send_info_t* info, esp_now_send_status_t status);

    uint32_t left_;
    Mode mode_ = Mode::Main;
    Mode feedbackReturnMode_ = Mode::Main;
    CommunicatorLogic logic_;
    EspContacts::ContactBook contactBook_;
    uint8_t mainIndex_ = 0;
    uint8_t contactIndex_ = 0;
    bool radioReady_ = false;
    bool radioStarted_ = false;
    bool sendPending_ = false;
    bool sendSuccess_ = false;
    char myInitials_[3] = {'J', 'F', '\0'};
    char lastSeen_[6] = "";
    const char* feedbackText_ = "";
    uint16_t feedbackMs_ = 0;
};
