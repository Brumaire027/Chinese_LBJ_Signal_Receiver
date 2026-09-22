#pragma once
#include <stdint.h>
#include <stddef.h>
#if __cplusplus >= 201402L
#define RX_BUFFER_CHECK constexpr
#else
#define RX_BUFFER_CHECK inline
#endif
// Caller serializes ISR/main access. A full buffer rejects the newest byte;
// unread bytes remain intact, and capacity is never confused with empty.
struct DirectReceiveByte {
    uint8_t value = 0, bits = 0;
    RX_BUFFER_CHECK void clear() { value = bits = 0; }
    RX_BUFFER_CHECK bool pushBit(uint8_t bit, uint8_t &out) {
        value = uint8_t((value << 1) | (bit & 1U));
        if(++bits != 8) return false;
        out = value; clear(); return true;
    }
};
template <size_t Capacity> struct DirectReceiveBuffer {
    static_assert(Capacity > 0 && Capacity <= 32767, "available() returns int16_t");
    uint8_t bytes[Capacity] = {};
    uint16_t head = 0, tail = 0, count = 0;
    RX_BUFFER_CHECK uint16_t available() const { return count; }
    RX_BUFFER_CHECK bool push(uint8_t value) {
        if(count == Capacity) return false;
        bytes[head] = value;
        head = (head + 1) % Capacity;
        ++count; return true;
    }
    RX_BUFFER_CHECK bool pop(uint8_t &value) {
        if(!count) return false;
        value = bytes[tail]; tail = (tail + 1) % Capacity;
        --count; return true;
    }
    RX_BUFFER_CHECK uint16_t clear() {
        const uint16_t discarded = count;
        head = tail = count = 0; return discarded;
    }
};
#undef RX_BUFFER_CHECK
