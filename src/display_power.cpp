#include "display_power.hpp"
#include "runtime_settings.hpp"
#include "status_display.hpp"

extern bool oled_off;
namespace {
uint32_t lastActivity = 0;
uint8_t heldKeys = 0;
uint8_t wakeOnlyKeys = 0;
}

void applyDisplayBrightness() {
#ifdef HAS_DISPLAY
    const uint8_t contrast[] = {32, 128, 255};
    if (u8g2) u8g2->setContrast(contrast[runtimeSettings().display.brightness]);
#endif
}

void initDisplayPower() {
    lastActivity = millis();
    heldKeys = wakeOnlyKeys = 0;
    oled_off = false;
    applyDisplayBrightness();
}

// Main loop only: receive workers merely publish a pending notification.
void displayActivity(bool allowWake) {
    if (oled_off && !allowWake) return;
    lastActivity = millis();
#ifdef HAS_DISPLAY
    if (oled_off && u8g2) {
        oled_off = false;
        requestMainDisplayRefresh();
        u8g2->setPowerSave(false);
    }
#endif
}

bool consumeDisplayButton(const ButtonEvent &event) {
    const uint8_t bit = 1U << static_cast<uint8_t>(event.id);
    if (event.type == ButtonEventType::Pressed) {
        heldKeys |= bit;
        if (oled_off || wakeOnlyKeys) wakeOnlyKeys |= bit;
        displayActivity(true);
        return true;
    }
    const bool consume = (wakeOnlyKeys & bit) != 0;
    displayActivity(true);
    if (event.type == ButtonEventType::Released) {
        heldKeys &= ~bit;
        wakeOnlyKeys &= ~bit;
        return true;
    }
    return consume;
}

void updateDisplayPower() {
#ifdef HAS_DISPLAY
    const bool protectedView = isMenuDisplayActive() || isHistoryDisplayActive();
    if (protectedView || heldKeys) lastActivity = millis();
    if (u8g2 && !oled_off && displaySleepDue(millis(), lastActivity,
            runtimeSettings().display.timeout, protectedView, heldKeys != 0)) {
        u8g2->setPowerSave(true);
        oled_off = true;
    }
#endif
}
