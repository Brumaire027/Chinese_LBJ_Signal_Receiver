#pragma once
#include <stdint.h>
#if __cplusplus >= 201402L
#define ID_CONSTEXPR constexpr
#else
#define ID_CONSTEXPR inline
#endif

namespace train_identity {
struct Text { char value[9] = {}; bool valid = true; };
ID_CONSTEXPR bool equal(const char *a, const char *b) {
    for (int i = 0; i < 9; ++i) { if (a[i] != b[i]) return false; if (!a[i]) return true; }
    return false;
}
ID_CONSTEXPR void copy(char *out, const char *in) {
    for (int i = 0; i < 9; ++i) { out[i] = in[i]; if (!in[i]) return; }
}
ID_CONSTEXPR Text trim(const char *s) {
    Text result; int begin = 0, end = 0;
    while (end < 32 && s[end]) ++end;
    if (end == 32) { result.valid = false; return result; }
    while (begin < end && s[begin] == ' ') ++begin;
    while (end > begin && s[end - 1] == ' ') --end;
    if (end - begin > 8) { result.valid = false; return result; }
    for (int i = begin; i < end; ++i) result.value[i - begin] = s[i];
    return result;
}
ID_CONSTEXPR bool digits(const char *s, int min, int max) {
    int n = 0;
    while (s[n] && n <= max) { if (s[n] < '0' || s[n] > '9') return false; ++n; }
    return n >= min && n <= max;
}
enum class Quality { Invalid, Missing, Complete };
struct Identity {
    char key[9] = {}, number[9] = {};
    Quality quality = Quality::Invalid;
    bool inferred = false;
};
ID_CONSTEXPR Identity parse(const char *number, const char *category, bool extended) {
    Identity out;
    const Text n = trim(number), c = trim(category);
    if (!n.valid || !c.valid || !digits(n.value, 1, 5)) return out;
    copy(out.number, n.value);
    if (equal(c.value, "NA") || (!c.value[0] && !extended)) {
        out.quality = Quality::Missing; return out;
    }
    // Validate the entire category. Never extract A from 8A, N from N;, or K from 0K.
    if (c.value[0] && (c.value[1] || c.value[0] < 'A' || c.value[0] > 'Z' || c.value[0] == 'X')) return out;
    int offset = c.value[0] ? 1 : 0;
    if (offset) out.key[0] = c.value[0];
    for (int i = 0; n.value[i]; ++i) out.key[offset + i] = n.value[i];
    out.quality = Quality::Complete;
    return out;
}

struct Resolver {
    static constexpr uint32_t Lifetime = 120000;
    static constexpr int Capacity = 32;
    struct Evidence {
        char key[9] = {}, number[9] = {}, loco[9] = {};
        int direction = 0;
        uint32_t seen = 0, locoSeen = 0;
        uint8_t hits = 0;
    };
    Evidence evidence[Capacity] = {};
    bool saturated = false;
    uint32_t evictionTime = 0;

    ID_CONSTEXPR Identity resolve(const char *number, const char *category, bool extended,
                                 int direction, const char *loco, uint32_t now) {
        Identity out = parse(number, category, extended);
        if (out.quality == Quality::Invalid) return out;
        Text engine = trim(loco);
        const bool engineValid = engine.valid && digits(engine.value, 8, 8);
        const bool directionValid = direction == 1 || direction == 3;
        if (out.quality == Quality::Complete) {
            if (!directionValid) return out; // Can display explicit identity, but cannot establish an association.
            int slot = -1;
            for (int i = 0; i < Capacity; ++i)
                if (evidence[i].hits && equal(evidence[i].key, out.key) && evidence[i].direction == direction) { slot = i; break; }
            if (slot < 0) {
                for (int i = 0; i < Capacity; ++i) if (!evidence[i].hits) { slot = i; break; }
                if (slot < 0) {
                    slot = 0;
                    for (int i = 1; i < Capacity; ++i)
                        if (uint32_t(now - evidence[i].seen) > uint32_t(now - evidence[slot].seen)) slot = i;
                    if (uint32_t(now - evidence[slot].seen) <= Lifetime) { saturated = true; evictionTime = now; }
                }
                evidence[slot] = Evidence{};
            }
            auto &e = evidence[slot];
            const bool continuous = e.hits && uint32_t(now - e.seen) <= Lifetime &&
                !(engineValid && e.loco[0] && !equal(engine.value, e.loco));
            e.hits = continuous ? (e.hits < 2 ? e.hits + 1 : 2) : 1;
            copy(e.key, out.key); copy(e.number, out.number); e.direction = direction; e.seen = now;
            if (engineValid) { copy(e.loco, engine.value); e.locoSeen = now; }
            else if (uint32_t(now - e.locoSeen) > Lifetime) e.loco[0] = 0;
            return out;
        }
        if (!directionValid || (saturated && uint32_t(now - evictionTime) <= Lifetime)) return out;
        int candidate = -1;
        for (int i = 0; i < Capacity; ++i) {
            const auto &e = evidence[i];
            if (!e.hits || uint32_t(now - e.seen) > Lifetime || e.direction != direction || !equal(e.number, out.number)) continue;
            if (engineValid && e.loco[0] && uint32_t(now - e.locoSeen) <= Lifetime && !equal(engine.value, e.loco)) continue;
            if (candidate >= 0) return out; // More than one compatible identity: stay unresolved.
            candidate = i;
        }
        if (candidate < 0) return out;
        const auto &e = evidence[candidate];
        if (e.hits < 2 && !(engineValid && e.loco[0] && uint32_t(now - e.locoSeen) <= Lifetime && equal(engine.value, e.loco))) return out;
        copy(out.key, e.key); out.inferred = true;
        // Inferred packets never refresh evidence, so guesses cannot sustain themselves indefinitely.
        return out;
    }
};
}
#undef ID_CONSTEXPR
