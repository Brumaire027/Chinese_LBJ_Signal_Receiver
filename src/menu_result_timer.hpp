#pragma once
#include <stdint.h>

// Start timing only after the result has actually been drawn. No blocking delay.
class MenuResultTimer {
    bool enabled = false, visible = false;
    uint32_t since = 0;
public:
    void arm(bool success) { enabled = success; visible = false; }
    bool active() const { return enabled; }
    void shown(uint32_t now) { if (enabled && !visible) { since = now; visible = true; } }
    bool ready(uint32_t now) const { return enabled && visible && uint32_t(now - since) >= 2000U; }
};
