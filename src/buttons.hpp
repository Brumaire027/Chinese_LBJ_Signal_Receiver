#ifndef LBJ_BUTTONS_HPP
#define LBJ_BUTTONS_HPP

#include <Arduino.h>

enum class ButtonId : uint8_t {
    Key1 = 0,
    Key2,
    Key3,
    Key4,
};

enum class ButtonEventType : uint8_t {
    ShortPress = 0,
    LongPress,
    Released,
    Pressed,
};

struct ButtonEvent {
    ButtonId id;
    ButtonEventType type;
    uint32_t timestampMs;
    uint32_t durationMs;
};

void initButtons();
void updateButtons();
bool getNextButtonEvent(ButtonEvent &event);

#endif
