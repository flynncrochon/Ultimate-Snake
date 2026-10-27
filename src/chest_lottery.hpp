#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace snake::loot {
constexpr float IntroSeconds = .22f;
constexpr float SpinSeconds = 6.0f;
constexpr float RevealSeconds = IntroSeconds + SpinSeconds;
constexpr float ContinueDelay = .18f;
constexpr int WinningCell = 24;
constexpr int CellPitch = 56;

// Presentation-only motion. No random engine, storage, or simulation state.
inline float reelPosition(float seconds) {
    const float remaining = 1 - std::clamp((seconds - IntroSeconds) / SpinSeconds, 0.0f, 1.0f);
    return float(WinningCell) * (1 - remaining * remaining * remaining);
}
// Rendering and audio share this quantized offset. Even the final slowly moving
// square clicks on the same frame its native raster center reaches the marker.
inline int reelScrollPixels(float seconds) {
    return int(std::lround(reelPosition(seconds)*float(CellPitch)));
}
class ReelTicker {
public:
    void reset() { passed=0; }
    bool advance(float seconds,bool suppress=false) {
        const auto crossed=std::size_t(reelScrollPixels(seconds)/CellPitch);
        if(crossed<=passed)return false;
        passed=crossed;
        // A stalled/skipped frame consumes crossings without a catch-up burst.
        return !suppress;
    }
    std::size_t consumed() const { return passed; }
private:
    std::size_t passed=0;
};
inline int decoyIcon(std::uint64_t serial, int cell) {
    std::uint32_t hash = std::uint32_t(serial) ^ std::uint32_t(serial >> 32) ^ (std::uint32_t(cell) * 0x9E3779B9u);
    hash ^= hash >> 16; hash *= 0x7FEB352Du; hash ^= hash >> 15; hash *= 0x846CA68Bu; hash ^= hash >> 16;
    return int(hash % 18u);
}
}
