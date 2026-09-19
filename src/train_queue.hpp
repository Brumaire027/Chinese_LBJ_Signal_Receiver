#pragma once
#include <stdint.h>
#if __cplusplus >= 201402L
#define TRAIN_CONSTEXPR constexpr
#else
#define TRAIN_CONSTEXPR
#endif

// Shared firmware policy and compile-time scenario test implementation.
struct TrainQueue {
    static constexpr int Capacity = 32;
    struct Entry { char key[9] = {}; uint32_t seen = 0; bool used = false, queued = false, dirty = false; };
    Entry entries[Capacity] = {};
    int order[Capacity] = {};
    int count = 0, current = -1;
    uint32_t shownAt = 0, overflow = 0;
    bool visibleBefore = false;
    TRAIN_CONSTEXPR static bool equal(const char *a, const char *b) {
        for (int i = 0; i < 9; ++i) { if (a[i] != b[i]) return false; if (!a[i]) return true; }
        return false;
    }
    TRAIN_CONSTEXPR int find(const char *key) const {
        for (int i = 0; i < Capacity; ++i) if (entries[i].used && equal(entries[i].key, key)) return i;
        return -1;
    }
    struct Arrival { int slot; bool fresh; };
    TRAIN_CONSTEXPR Arrival receive(const char *key, uint32_t now) {
        int slot = find(key);
        bool fresh = slot < 0 || uint32_t(now - entries[slot].seen) >= 600000U;
        if (slot < 0) {
            for (int i = 0; i < Capacity; ++i) if (!entries[i].used) { slot = i; break; }
            if (slot < 0) {
                for (int i = 0; i < Capacity; ++i)
                    if (i != current && !entries[i].queued &&
                        (slot < 0 || uint32_t(now - entries[i].seen) > uint32_t(now - entries[slot].seen))) slot = i;
            }
            if (slot < 0) { ++overflow; return {-1, false}; }
            entries[slot] = Entry{};
            for (int i = 0; i < 8 && key[i]; ++i) entries[slot].key[i] = key[i];
            entries[slot].used = true;
        }
        auto &entry = entries[slot];
        entry.seen = now; entry.dirty = true;
        if (fresh && slot != current && !entry.queued) {
            order[count++] = slot; entry.queued = true;
        }
        return {slot, fresh};
    }
    TRAIN_CONSTEXPR bool advance(uint32_t now, bool visible) {
        if (!visible) { visibleBefore = false; return false; }
        if (!visibleBefore) { shownAt = now; visibleBefore = true; }
        if (count && (current < 0 || uint32_t(now - shownAt) >= 2000U)) {
            current = order[0];
            for (int i = 1; i < count; ++i) order[i - 1] = order[i];
            --count; entries[current].queued = false; entries[current].dirty = true; shownAt = now;
        }
        if (current < 0 || !entries[current].dirty) return false;
        entries[current].dirty = false;
        return true;
    }
};
#undef TRAIN_CONSTEXPR
