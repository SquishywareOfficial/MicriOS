#pragma once

#include "../../App.h"

class OptionsApp : public App {
  public:
    OptionsApp(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawStart(U8G2& u8g2) override;
    void drawRunning(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    enum class Mode {
      Main,
      Initials,
      Saves,
      ConfirmOne,
      ConfirmAll1,
      ConfirmAll2,
      Message
    };

    char nextInitial(char value) const;
    void clearNamespace(const char* ns);
    void clearSelectedSave();
    void clearAllSaves();
    void drawFit(U8G2& u8g2, int x, int y, const char* text);

    Mode mode_ = Mode::Main;
    char initials_[3] = {'J', 'F', '\0'};
    uint8_t selected_ = 0;
    uint8_t mainIndex_ = 0;
    uint8_t saveIndex_ = 0;
    const char* message_ = "";
    bool messageToMain_ = false;
};
