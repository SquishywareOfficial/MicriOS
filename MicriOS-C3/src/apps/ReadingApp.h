#pragma once

#include "../../App.h"

class ReadingApp : public App {
  public:
    ReadingApp(uint32_t width, uint32_t height, uint32_t left);

    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    bool startsRunningImmediately() const override;

  private:
    enum class Mode {
      Select,
      Read,
      Complete
    };

    bool hasMoreAfterPage(const char* text, uint8_t page) const;
    void drawSelection(U8G2& u8g2) const;
    void drawReading(U8G2& u8g2) const;
    void drawComplete(U8G2& u8g2) const;
    void drawCenteredText(U8G2& u8g2, int y, const char* text) const;
    bool drawWrappedPage(U8G2& u8g2, const char* text, uint8_t page, int firstY) const;

    uint32_t left_;
    uint8_t selection_;
    uint8_t page_;
    Mode mode_;
};
