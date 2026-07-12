#pragma once

#include "../../App.h"

class CreditsApp : public App {
  public:
    CreditsApp(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    enum class Mode {
      Select,
      License,
      Credits
    };

    void drawSelect(U8G2& u8g2);
    void drawLicense(U8G2& u8g2);
    void drawCredits(U8G2& u8g2);

    Mode mode_ = Mode::Select;
    uint8_t selection_ = 0;
    uint8_t page_ = 0;
};
