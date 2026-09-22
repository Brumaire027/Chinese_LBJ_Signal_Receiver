#pragma once
#include <stddef.h>

namespace pager_decode {
// One complete 32-bit codeword contains at most five numeric symbols.
constexpr size_t symbolCapacity(size_t bytes) { return (bytes / 4) * 5; }
constexpr bool canReadCodeword(size_t bytes) { return bytes >= 4; }
constexpr bool canAppend(size_t used, size_t capacity) { return used < capacity; }
}
