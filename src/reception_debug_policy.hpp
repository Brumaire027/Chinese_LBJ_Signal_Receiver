#pragma once
#include <stdint.h>
enum class RxDebugMode : uint8_t { Normal, NoLog, NoSd };
constexpr bool rxDebugLogPolicy(bool enabled, RxDebugMode mode) {
    return !enabled || mode == RxDebugMode::Normal;
}
constexpr bool rxDebugCsvPolicy(bool enabled, RxDebugMode mode) {
    return !enabled || mode != RxDebugMode::NoSd;
}
constexpr uint32_t rxDebugElapsed(uint32_t now, uint32_t start) { return now - start; }
