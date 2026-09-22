#pragma once
#include <stddef.h>
#include <string.h>
#if __cplusplus >= 201402L
#define FIELD_CHECK constexpr
#else
#define FIELD_CHECK inline
#endif
namespace display_fields {
// Check field syntax, not the plausibility of an entire radio packet.
FIELD_CHECK bool number(const char *s, bool decimal = false) {
    bool digit = false, point = false, trailing = false;
    for (size_t i = 0; s[i]; ++i) {
        const char c = s[i];
        if (c == ' ') { if (digit) trailing = true; continue; }
        if (trailing) return false;
        if (c >= '0' && c <= '9') { digit = true; continue; }
        if (decimal && c == '.' && digit && !point && s[i+1] >= '0' && s[i+1] <= '9') {
            point = true; continue;
        }
        return false;
    }
    return digit;
}
FIELD_CHECK bool digits(const char *s, size_t count) {
    for (size_t i = 0; i < count; ++i) if (s[i] < '0' || s[i] > '9') return false;
    return s[count] == 0;
}
template <size_t N> void missing(char (&s)[N]) {
    memset(s, 0, N); s[0] = '-'; s[1] = '-';
}
// Operates on a display copy only; raw decode and CSV keep their original evidence.
template <typename Data> void normalize(Data &d) {
    if (!number(d.train)) missing(d.train);
    if (!number(d.speed)) missing(d.speed);
    if (!number(d.position, true)) missing(d.position);
    char category = 0; bool invalid = false;
    for (size_t i = 0; i < sizeof(d.lbj_class) && d.lbj_class[i]; ++i) {
        const char c = d.lbj_class[i];
        if (c == ' ') continue;
        if (category || c < 'A' || c > 'Z' || c == 'X') invalid = true;
        category = c;
    }
    if (invalid) { d.lbj_class[0] = '*'; d.lbj_class[1] = 0; }
    else { d.lbj_class[0] = category; d.lbj_class[1] = 0; }
    if (!digits(d.loco, 8)) { missing(d.loco); d.loco_type = ""; }
    if (!digits(d.pos_lon, 9)) {
        missing(d.pos_lon); d.pos_lon_deg[0] = d.pos_lon_min[0] = 0;
    }
    if (!digits(d.pos_lat, 8)) {
        missing(d.pos_lat); d.pos_lat_deg[0] = d.pos_lat_min[0] = 0;
    }
    if (!d.route_utf8[0]) { d.route_utf8[0] = '*'; d.route_utf8[1] = '*'; d.route_utf8[2] = 0; }
}
}
#undef FIELD_CHECK
