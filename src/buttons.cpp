#include "buttons.hpp"

#include "utilities.h"

namespace {

constexpr uint8_t BUTTON_COUNT = 4;
constexpr uint8_t EVENT_QUEUE_SIZE = 16;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t LONG_PRESS_MS = 800;

struct ButtonConfig {
    ButtonId id;
    uint8_t pin;
};

struct ButtonState {
    bool stablePressed = false;
    bool lastRawPressed = false;
    bool longPressEmitted = false;
    uint32_t lastRawChangeMs = 0;
    uint32_t pressedAtMs = 0;
};

constexpr ButtonConfig BUTTONS[BUTTON_COUNT] = {
    {ButtonId::Key1, KEY1_PIN},
    {ButtonId::Key2, KEY2_PIN},
    {ButtonId::Key3, KEY3_PIN},
    {ButtonId::Key4, KEY4_PIN},
};

ButtonState buttonStates[BUTTON_COUNT];
ButtonEvent eventQueue[EVENT_QUEUE_SIZE];
uint8_t eventHead = 0;
uint8_t eventTail = 0;
uint8_t eventCount = 0;

void pushEvent(ButtonId id, ButtonEventType type, uint32_t timestampMs, uint32_t durationMs) {
    if (eventCount == EVENT_QUEUE_SIZE) {
        eventTail = (eventTail + 1) % EVENT_QUEUE_SIZE;
        eventCount--;
    }

    eventQueue[eventHead] = {id, type, timestampMs, durationMs};
    eventHead = (eventHead + 1) % EVENT_QUEUE_SIZE;
    eventCount++;
}

bool readPressed(uint8_t pin) {
    return digitalRead(pin) == LOW;
}

}  // namespace

void initButtons() {
    const uint32_t now = millis();

    eventHead = 0;
    eventTail = 0;
    eventCount = 0;

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        pinMode(BUTTONS[i].pin, INPUT_PULLUP);
        const bool pressed = readPressed(BUTTONS[i].pin);
        buttonStates[i] = {};
        buttonStates[i].stablePressed = pressed;
        buttonStates[i].lastRawPressed = pressed;
        buttonStates[i].lastRawChangeMs = now;
        buttonStates[i].pressedAtMs = pressed ? now : 0;
    }
}

void updateButtons() {
    const uint32_t now = millis();

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        ButtonState &state = buttonStates[i];
        const bool rawPressed = readPressed(BUTTONS[i].pin);

        if (rawPressed != state.lastRawPressed) {
            state.lastRawPressed = rawPressed;
            state.lastRawChangeMs = now;
        }

        if (rawPressed != state.stablePressed && now - state.lastRawChangeMs >= DEBOUNCE_MS) {
            state.stablePressed = rawPressed;

            if (state.stablePressed) {
                state.pressedAtMs = now;
                state.longPressEmitted = false;
                pushEvent(BUTTONS[i].id, ButtonEventType::Pressed, now, 0);
            } else {
                const uint32_t duration = state.pressedAtMs == 0 ? 0 : now - state.pressedAtMs;
                if (!state.longPressEmitted) {
                    pushEvent(BUTTONS[i].id, ButtonEventType::ShortPress, now, duration);
                }
                pushEvent(BUTTONS[i].id, ButtonEventType::Released, now, duration);
                state.pressedAtMs = 0;
                state.longPressEmitted = false;
            }
        }

        if (state.stablePressed && !state.longPressEmitted &&
            state.pressedAtMs != 0 && now - state.pressedAtMs >= LONG_PRESS_MS) {
            state.longPressEmitted = true;
            pushEvent(BUTTONS[i].id, ButtonEventType::LongPress, now, now - state.pressedAtMs);
        }
    }
}

bool getNextButtonEvent(ButtonEvent &event) {
    if (eventCount == 0) {
        return false;
    }

    event = eventQueue[eventTail];
    eventTail = (eventTail + 1) % EVENT_QUEUE_SIZE;
    eventCount--;
    return true;
}
