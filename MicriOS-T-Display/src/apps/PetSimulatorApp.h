#pragma once

#include "../../App.h"

class PetSimulatorApp : public App {
  public:
    PetSimulatorApp(uint32_t width, uint32_t height, uint32_t left = 1);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    bool startsRunningImmediately() const override;
    bool hasCustomOverlay() const override;

  private:
    enum class Mode { ChoosePet, Menu, Stats, Message, PlayScene, Dead };

    void loadPet();
    void savePet();
    void resetCareStats();
    void applyAction();
    void tickCare(uint32_t deltaMs);
    void updateIdle(uint32_t deltaMs);
    void updatePlayScene(uint32_t deltaMs);
    void startPlayScene();
    template <typename Canvas> void drawPet(Canvas& tft, int x, int y);
    template <typename Canvas> void drawPoop(Canvas& tft, int x, int y);
    template <typename Canvas> void drawPoops(Canvas& tft);
    template <typename Canvas> void drawBar(Canvas& tft, int x, int y, const char* label, uint8_t value);
    template <typename Canvas> void drawLowHealthWarning(Canvas& tft);

    Mode mode_ = Mode::ChoosePet;
    uint8_t petType_ = 0;
    uint8_t menuIndex_ = 0;
    uint8_t hunger_ = 80;
    uint8_t fun_ = 80;
    uint8_t energy_ = 80;
    uint8_t clean_ = 80;
    uint8_t health_ = 100;
    uint8_t poop_ = 0;
    uint32_t careMs_ = 0;
    uint32_t messageMs_ = 0;
    uint32_t idleMs_ = 0;
    uint32_t wanderMs_ = 0;
    uint32_t playMs_ = 0;
    uint32_t playStepMs_ = 0;
    int16_t petX_ = 120;
    int16_t petY_ = 65;
    int8_t petDx_ = 1;
    int8_t petDy_ = 0;
    int16_t toyX_ = 160;
    int16_t toyY_ = 72;
    bool idleMode_ = false;
    const char* message_ = "";
    bool loaded_ = false;
};
