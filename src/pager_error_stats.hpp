#pragma once
#include <stddef.h>

#if __cplusplus >= 201402L
#define PAGER_STATS_CONSTEXPR constexpr
#else
#define PAGER_STATS_CONSTEXPR
#endif

// Keep the existing numeric-POCSAG estimate: five symbols per 32-bit codeword.
// The aggregate must accommodate all 16 packet slots, not just one byte.
struct PagerErrorStats {
    size_t totalErrors = 0;
    size_t uncorrectedErrors = 0;
    size_t bitCount = 0;
    PAGER_STATS_CONSTEXPR void add(size_t symbols, size_t errors, size_t uncorrected) {
        totalErrors += errors;
        uncorrectedErrors += uncorrected;
        bitCount += (symbols / 5) * 32;
    }
    constexpr double percentage() const {
        return bitCount ? 100.0 * totalErrors / bitCount : 0.0;
    }
};
#undef PAGER_STATS_CONSTEXPR
