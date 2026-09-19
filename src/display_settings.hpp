#pragma once
#include <stdint.h>

struct DisplaySettings {
    uint8_t brightness = 1; // Low, medium, high.
    uint8_t timeout = 2;    // Always on, 30s, 60s, 180s, 300s.
    bool wakeOnArrival = true;
};

constexpr uint32_t displayTimeoutMs(uint8_t index) {
    return index == 0 ? 0 : index == 1 ? 30000 : index == 3 ? 180000 : index == 4 ? 300000 : 60000;
}
constexpr bool displaySleepDue(uint32_t now, uint32_t activity, uint8_t timeout, bool protectedView, bool keyHeld) {
    return !protectedView && !keyHeld && displayTimeoutMs(timeout) != 0 &&
           uint32_t(now - activity) >= displayTimeoutMs(timeout);
}
constexpr uint32_t encodeDisplaySettings(uint8_t brightness, uint8_t timeout, bool wake) {
    return brightness | (uint32_t(timeout) << 8) | (uint32_t(wake) << 16);
}
constexpr bool validDisplaySettings(uint32_t packed) {
    return (packed & 0xff) < 3 && ((packed >> 8) & 0xff) < 5 && (packed >> 16) < 2;
}
