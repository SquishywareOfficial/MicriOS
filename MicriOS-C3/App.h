#pragma once

#include <stdint.h>

class U8G2;

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

        void begin(uint32_t nowMs, bool buttonDown);
        void tick(uint32_t nowMs, bool buttonDown);
        void render(U8G2& u8g2);
        virtual uint16_t runningRenderIntervalMs() const;

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
        virtual bool updateStart(uint32_t deltaMs, const ButtonInput& input);
        virtual void updateRunning(uint32_t deltaMs, const ButtonInput& input) = 0;
        virtual void drawRunning(U8G2& u8g2) = 0;
        virtual void drawStart(U8G2& u8g2);
        virtual void drawEnd(U8G2& u8g2);
        virtual bool startsRunningImmediately() const;
        virtual void onAppExit();
        void startRunning();

    private:
        AppPhase phase_ = AppPhase::Start;
        SingleButton button_;
        uint32_t lastUpdateMs_ = 0;
        uint32_t phaseStartedAtMs_ = 0;
        bool appEnded_ = false;
        bool exitToMenuRequested_ = false;
};
