#pragma once

#include <stdint.h>

struct ButtonInput {
    bool down = false;
    bool pressed = false;
    bool released = false;
    bool click = false;
    bool longPress = false;
    uint32_t holdMs = 0;
};

class SingleButton {
    public:
        SingleButton(uint16_t debounceMs = 30, uint16_t longPressMs = 700);

        void reset(bool rawDown, uint32_t nowMs);
        ButtonInput update(bool rawDown, uint32_t nowMs);

    private:
        uint16_t debounceMs_;
        uint16_t longPressMs_;
        bool rawDown_ = false;
        bool debouncedDown_ = false;
        bool longPressEmitted_ = false;
        uint32_t rawChangedAtMs_ = 0;
        uint32_t pressedAtMs_ = 0;
};

enum class AppPhase {
    Start,
    Running,
    End
};

class TFT_eSPI;

class App {
    protected:
        const char* title;
        uint32_t width;
        uint32_t height;

        void endApp();
        void requestExitToMenu();

    public:
        App(const char* title, uint32_t width, uint32_t height);
        virtual ~App() = default;

        // Unified button input for both buttons
        void begin(uint32_t nowMs, bool button1Down, bool button2Down);
        void tick(uint32_t nowMs, bool button1Down, bool button2Down);
        virtual void render(TFT_eSPI& tft);
        virtual uint16_t runningRenderIntervalMs() const;
        virtual uint16_t staticRenderIntervalMs() const;
        virtual bool wantsImmediateRender() const;

        bool shouldExitToMenu() const;
        void clearExitRequest();
        AppPhase phase() const;
        const char* appTitle() const;
        uint8_t startIntroPage() const;
        bool showStartScorePage() const;
        bool showStartPromptPage() const;
        virtual bool hasCustomOverlay() const;

    protected:
        virtual void onAppReset();
        virtual bool updateStart(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2);
        virtual void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) = 0;
        virtual void drawRunning(TFT_eSPI& tft) = 0;
        virtual void drawStart(TFT_eSPI& tft);
        virtual void drawEnd(TFT_eSPI& tft);
        virtual bool startsRunningImmediately() const;
        virtual bool consumesButton2HoldInRunning() const;
        virtual void onAppExit();
        void startRunning();

    private:
        AppPhase phase_ = AppPhase::Start;
        SingleButton button1_;
        SingleButton button2_;
        uint32_t lastUpdateMs_ = 0;
        uint32_t phaseStartedAtMs_ = 0;
        bool appEnded_ = false;
        bool exitToMenuRequested_ = false;
};
