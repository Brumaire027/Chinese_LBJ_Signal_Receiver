#pragma once
#include <stddef.h>
#include <stdint.h>
#if __cplusplus >= 201402L
#define ROUTE_CONSTEXPR constexpr
#else
#define ROUTE_CONSTEXPR inline
#endif
namespace route_encoding {
// Sizes include any terminator. Never split a UTF-8 character or read past input.
template<class Decode>
ROUTE_CONSTEXPR size_t convert(const char *input, size_t inputSize, char *out,
                              size_t outSize, Decode decode) {
    if (!outSize) return 0;
    size_t written = 0;
    out[0] = 0;
    for (size_t i = 0; i < inputSize && input[i];) {
        const uint8_t first = static_cast<uint8_t>(input[i++]);
        uint16_t cp = first;
        if (first >= 0x80) {
            cp = '*';
            if (first >= 0x81 && first <= 0xfe && i < inputSize && input[i]) {
                const uint8_t second = static_cast<uint8_t>(input[i]);
                if (second >= 0x40 && second <= 0xfe && second != 0x7f) {
                    ++i;
                    cp = decode(static_cast<uint16_t>((first << 8) | second));
                    if (!cp || (cp >= 0xd800 && cp <= 0xdfff)) cp = '*';
                }
            }
        }
        const size_t bytes = cp < 0x80 ? 1 : cp < 0x800 ? 2 : 3;
        if (bytes >= outSize - written) break;
        if (bytes == 1) out[written++] = static_cast<char>(cp);
        else {
            if (bytes == 3) {
                out[written++] = static_cast<char>(0xe0 | (cp >> 12));
                out[written++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            } else out[written++] = static_cast<char>(0xc0 | (cp >> 6));
            out[written++] = static_cast<char>(0x80 | (cp & 0x3f));
        }
        out[written] = 0;
    }
    return written;
}
}
#undef ROUTE_CONSTEXPR
