#pragma once

#include <cstdint>

namespace rogue {

class XorShift32 {
public:
    explicit XorShift32(uint32_t seed) : state_(seed == 0 ? 0x6d2b79f5u : seed) {}

    uint32_t NextU32() {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }

    float NextFloat() {
        return static_cast<float>((NextU32() >> 8) & 0x00ffffffu) / 16777215.0f;
    }

    int NextRange(int lo, int hiInclusive) {
        const uint32_t span = static_cast<uint32_t>(hiInclusive - lo + 1);
        return lo + static_cast<int>(NextU32() % span);
    }

private:
    uint32_t state_;
};

}

