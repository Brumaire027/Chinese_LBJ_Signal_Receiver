#pragma once
#include <stddef.h>
#if __cplusplus >= 201402L
#define HISTORY_CONSTEXPR constexpr
#else
#define HISTORY_CONSTEXPR inline
#endif
namespace history_stream {
// Current CSV schema has 23 fields. Quoted raw reports may contain commas.
HISTORY_CONSTEXPR bool completeRecord(const char *s) {
    unsigned fields = 1;
    bool quoted = false;
    for (size_t i = 0; s[i]; ++i) {
        if (s[i] == '"') {
            if (quoted && s[i + 1] == '"') ++i;
            else quoted = !quoted;
        } else if (s[i] == ',' && !quoted) ++fields;
    }
    return !quoted && fields == 23;
}
template<size_t Capacity> struct Line {
    char data[Capacity] = {};
    size_t length = 0;
    bool discard = false;
    HISTORY_CONSTEXPR void reset(bool skipFirst = false) {
        length = 0; discard = skipFirst; data[0] = 0;
    }
    // A record is published only by newline, never by EOF. Overlong lines
    // are discarded in full, including their otherwise plausible prefix.
    HISTORY_CONSTEXPR bool push(char c) {
        if (c == '\r') return false;
        if (c == '\n') {
            data[length] = 0;
            const bool ready = !discard && length > 0;
            length = 0; discard = false;
            return ready;
        }
        if (!discard) {
            if (length + 1 < Capacity) data[length++] = c;
            else discard = true;
        }
        return false;
    }
};
}
#undef HISTORY_CONSTEXPR
