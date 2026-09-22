#pragma once
#include <stddef.h>

#if __cplusplus >= 201402L
#define SERIAL_LINE_CONSTEXPR constexpr
#else
#define SERIAL_LINE_CONSTEXPR
#endif

// No wait for a terminator and no heap growth while a user types a command.
class SerialLineBuffer {
public:
    enum class Result { Pending, Ready, TooLong };
    static constexpr size_t Capacity = 64;
    SERIAL_LINE_CONSTEXPR Result push(char c) {
        if (c == '\r' || c == '\n') {
            if (discarding) {
                discarding = false;
                used = 0;
                return Result::TooLong;
            }
            if (!used) return Result::Pending; // Ignore blank lines and CRLF's LF.
            buffer[used] = '\0';
            used = 0;
            return Result::Ready;
        }
        if (discarding) return Result::Pending;
        if (used == Capacity - 1) {
            used = 0;
            discarding = true; // Never execute a truncated command or its suffix.
        } else {
            buffer[used++] = c;
        }
        return Result::Pending;
    }
    constexpr const char *line() const { return buffer; }
private:
    char buffer[Capacity] = {};
    size_t used = 0;
    bool discarding = false;
};
#undef SERIAL_LINE_CONSTEXPR
