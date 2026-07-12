#include "App.h"
#include <TFT_eSPI.h>

namespace {
constexpr uint16_t END_INPUT_LOCKOUT_MS = 500;
constexpr uint16_t INTRO_PAGE_MS = 2000;
constexpr uint8_t INTRO_PAGE_COUNT = 3;
}

SingleButton::SingleButton(uint16_t debounceMs, uint16_t longPressMs)
    : debounceMs_(debounceMs), longPressMs_(longPressMs) {}

void SingleButton::reset(bool rawDown, uint32_t nowMs) {
    rawDown_ = rawDown;
    debouncedDown_ = rawDown;
    longPressEmitted_ = false;
    rawChangedAtMs_ = nowMs;
    pressedAtMs_ = nowMs;
}

ButtonInput SingleButton::update(bool rawDown, uint32_t nowMs) {
    ButtonInput input;

    if (rawDown != rawDown_) {
        rawDown_ = rawDown;
        rawChangedAtMs_ = nowMs;
    }

    if ((nowMs - rawChangedAtMs_) >= debounceMs_ && debouncedDown_ != rawDown_) {
        debouncedDown_ = rawDown_;
        if (debouncedDown_) {
            input.pressed = true;
            pressedAtMs_ = nowMs;
            longPressEmitted_ = false;
        } else {
            input.released = true;
            if (!longPressEmitted_) {
                input.click = true;
            }
        }
    }

    if (debouncedDown_) {
        input.holdMs = nowMs - pressedAtMs_;
        if (!longPressEmitted_ && input.holdMs >= longPressMs_) {
            longPressEmitted_ = true;
            input.longPress = true;
        }
    }

    input.down = debouncedDown_;
    return input;
}

App::App(const char* title, uint32_t width, uint32_t height)
    : title(title), width(width), height(height) {}

void App::begin(uint32_t nowMs, bool button1Down, bool button2Down) {
    button1_.reset(button1Down, nowMs);
    button2_.reset(button2Down, nowMs);
    phase_ = AppPhase::Start;
    lastUpdateMs_ = nowMs;
    phaseStartedAtMs_ = nowMs;
    appEnded_ = false;
    exitToMenuRequested_ = false;
    if (startsRunningImmediately()) {
        startRunning();
    }
}

void App::tick(uint32_t nowMs, bool button1Down, bool button2Down) {
    const ButtonInput b1 = button1_.update(button1Down, nowMs);
    const ButtonInput b2 = button2_.update(button2Down, nowMs);
    const uint32_t deltaMs = nowMs - lastUpdateMs_;
    lastUpdateMs_ = nowMs;

    // Global back button (button 2 long press or click in menu)
    if (b2.longPress && !(phase_ == AppPhase::Running && consumesButton2HoldInRunning())) {
        requestExitToMenu();
        return;
    }

    switch (phase_) {
        case AppPhase::Start:
            if (updateStart(deltaMs, b1, b2)) {
                break;
            }
            if (b1.click || b1.longPress || b1.longPress) {
                startRunning();
            }
            break;
        case AppPhase::Running:
            updateRunning(deltaMs, b1, b2);
            if (appEnded_) {
                phase_ = AppPhase::End;
                phaseStartedAtMs_ = nowMs;
            }
            break;
        case AppPhase::End:
            if ((nowMs - phaseStartedAtMs_) < END_INPUT_LOCKOUT_MS) {
                break;
            }
            if (b1.longPress) {
                requestExitToMenu();
            } else if (b1.click || b1.longPress) {
                startRunning();
            }
            break;
    }
}

void App::render(TFT_eSPI& tft) {
    switch (phase_) {
        case AppPhase::Start:
            drawStart(tft);
            break;
        case AppPhase::Running:
            drawRunning(tft);
            break;
        case AppPhase::End:
            drawEnd(tft);
            break;
    }
}

uint16_t App::runningRenderIntervalMs() const {
    return 33;
}

uint16_t App::staticRenderIntervalMs() const {
    return 2000;
}

bool App::wantsImmediateRender() const {
    return false;
}

bool App::shouldExitToMenu() const {
    return exitToMenuRequested_;
}

void App::clearExitRequest() {
    exitToMenuRequested_ = false;
}

AppPhase App::phase() const {
    return phase_;
}

const char* App::appTitle() const {
    return title;
}

uint8_t App::startIntroPage() const {
    return static_cast<uint8_t>(((lastUpdateMs_ - phaseStartedAtMs_) / INTRO_PAGE_MS) % INTRO_PAGE_COUNT);
}

bool App::showStartScorePage() const {
    return startIntroPage() == 1;
}

bool App::showStartPromptPage() const {
    return startIntroPage() == 2;
}

bool App::hasCustomOverlay() const {
    return false;
}

void App::endApp() {
    appEnded_ = true;
}

void App::requestExitToMenu() {
    if (!exitToMenuRequested_) {
        onAppExit();
    }
    exitToMenuRequested_ = true;
}

void App::onAppReset() {}

bool App::updateStart(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
    (void)deltaMs;
    (void)b1;
    (void)b2;
    return false;
}

void App::drawStart(TFT_eSPI& tft) {
    (void)tft;
}

void App::drawEnd(TFT_eSPI& tft) {
    (void)tft;
}

bool App::startsRunningImmediately() const {
    return false;
}

bool App::consumesButton2HoldInRunning() const {
    return false;
}

void App::onAppExit() {}

void App::startRunning() {
    appEnded_ = false;
    onAppReset();
    phase_ = AppPhase::Running;
    phaseStartedAtMs_ = lastUpdateMs_;
}
