#pragma once
#include "display_fields.hpp"
#include <stdint.h>
#if __cplusplus >= 201402L
#define HISTORY_CHECK constexpr
#else
#define HISTORY_CHECK inline
#endif
namespace history_policy {
HISTORY_CHECK bool equal(const char *a, const char *b) {
    for (unsigned i = 0; ; ++i) { if (a[i] != b[i]) return false; if (!a[i]) return true; }
}
HISTORY_CHECK bool route(const char *s) {
    if (equal(s, "NUL") || equal(s, "<NUL>") || equal(s, "null") || equal(s, "NA")) return false;
    for (unsigned i = 0; s[i]; ++i) {
        const unsigned char c = s[i];
        if (c >= 0x80 || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z' && c != 'X') ||
            (c >= 'a' && c <= 'z')) return true;
    }
    return false;
}
HISTORY_CHECK bool coordinate(const char *s, unsigned expectedDigits) {
    unsigned count = 0;
    for (unsigned i = 0; s[i]; ++i) {
        const unsigned char c = s[i];
        if (c >= '0' && c <= '9') ++count;
        else if (c < 0x80 && c != ' ' && c != '.' && c != '\'') return false;
    }
    return count == expectedDigits;
}
HISTORY_CHECK bool useful(const char *train, const char *speed, const char *position,
                         const char *loco, const char *line, const char *lat, const char *lon) {
    return display_fields::number(train) || display_fields::number(speed) ||
        display_fields::number(position, true) || display_fields::digits(loco, 8) ||
        route(line) || coordinate(lat, 8) || coordinate(lon, 9);
}
// Records are stored oldest first; the UI numbers the newest record as 1.
constexpr uint8_t entry(uint8_t count, bool up) { return !count || up ? 0 : count - 1; }
constexpr uint8_t number(uint8_t count, uint8_t index) { return count ? count - index : 0; }
constexpr uint8_t move(uint8_t count, uint8_t index, bool up) {
    return !count ? 0 : up ? (index + 1) % count : index ? index - 1 : count - 1;
}
}
#undef HISTORY_CHECK
