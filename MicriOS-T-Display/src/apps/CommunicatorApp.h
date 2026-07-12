#pragma once

#include <esp_now.h>

#include "../../App.h"
#include "../shared/logic/CommunicatorLogic.h"
#include "../shared/logic/EspContactsLogic.h"

class CommunicatorApp : public App {
  public:
    CommunicatorApp(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override { return true; }
    void render(TFT_eSPI& tft) override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool startsRunningImmediately() const override { return false; }

  private:
    static constexpr uint8_t MAX_PATH = CommunicatorLogic::MAX_PATH;
    static constexpr uint8_t NO_MESSAGE = 255;

    enum class UiMode {
      Send,
      Recipient,
      Inbox,
      OpenMessage,
      Feedback
    };

    bool beginRadio();
    void shutdownRadio();
    bool ensurePeer(const uint8_t* mac);
    void handleTap();
    void handleHold();
    void handleBack();
    void sendCurrentPath(uint8_t length);
    void beginRecipientSelect(uint8_t length);
    void sendMyContact();
    bool sendPacket(const CommunicatorLogic::MessagePacket& packet, const uint8_t* mac);
    void handleIncoming(const uint8_t* from, const uint8_t* data, int len);
    void handleSendStatus(bool success);
    const CommunicatorLogic::DictNode* currentList(uint8_t& count) const;
    const CommunicatorLogic::DictNode* selectedNode() const;
    const CommunicatorLogic::DictNode* nodeAtPath(const uint8_t* path, uint8_t length) const;
    const char* packetLeafLabel(const CommunicatorLogic::MessagePacket& packet) const;
    const char* currentTitle() const;
    const char* recipientLabel(uint8_t index) const;
    void showFeedback(const char* text, uint16_t color);
    template <typename Canvas> void drawSend(Canvas& tft);
    template <typename Canvas> void drawRecipient(Canvas& tft);
    template <typename Canvas> void drawInbox(Canvas& tft);
    template <typename Canvas> void drawOpenMessage(Canvas& tft);
    template <typename Canvas> void drawFeedback(Canvas& tft);
    void markDirty();
    void loadIdentityAndContacts();

    static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void onSentStatic(const esp_now_send_info_t* info, esp_now_send_status_t status);

    CommunicatorLogic logic_;
    EspContacts::ContactBook contactBook_;
    UiMode mode_ = UiMode::Send;
    UiMode returnMode_ = UiMode::Send;
    uint8_t path_[MAX_PATH] = {};
    CommunicatorLogic::MessagePacket pendingPacket_ = {};
    uint8_t depth_ = 0;
    uint8_t index_ = 0;
    uint8_t recipientIndex_ = 0;
    uint8_t inboxIndex_ = 0;
    uint8_t openInboxIndex_ = NO_MESSAGE;
    bool radioReady_ = false;
    bool radioStarted_ = false;
    bool sendPending_ = false;
    bool sendSuccess_ = false;
    bool dirty_ = true;
    bool startDirty_ = true;
    bool phaseCached_ = false;
    AppPhase renderedPhase_ = AppPhase::Start;
    const char* feedbackText_ = "";
    uint16_t feedbackColor_ = 0;
    uint16_t feedbackMs_ = 0;
    char myInitials_[3] = {'J', 'F', '\0'};
};
