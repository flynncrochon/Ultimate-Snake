#pragma once

#include "core.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace snake::audio {
constexpr int SampleRate = 44100;
constexpr std::size_t EffectCount = std::size_t(Event::Win) + 1;
constexpr std::size_t VariationCount = 3;
constexpr std::size_t VoiceCount = 3;
constexpr float ReelTickDuration = .025f;
constexpr float ReelTickVolume = .20f;

struct Definition {
    const char* name;
    float duration, volume, cooldown;
    int priority;
};
const Definition& definition(Event::Type effect);
std::vector<std::int16_t> synthesize(Event::Type effect, std::size_t variation = 0);
// Original, constant-pitch mechanical transient. Prepare once before play;
// the reel uses its own bounded voice rather than the combat cadence gate.
std::vector<std::int16_t> synthesizeReelTick();

struct Cue { Event::Type effect = Event::Shot; std::size_t variation = 0, voice = 0; };
struct Frame { std::array<Cue, VoiceCount> cues{}; std::size_t count = 0; };

// Render-frame aggregation is independent of simulation tick frequency.
// Three bounded voices and one shared combat gate prevent dense fights from
// becoming a wall of chirps. Important feedback can replace a quiet combat cue.
class Director {
public:
    void queue(Event::Type effect);
    Frame advance(float seconds, bool enabled = true);
    void reset();
private:
    std::array<bool, EffectCount> pending{};
    std::array<float, EffectCount> cooldown{};
    std::array<std::uint32_t, EffectCount> sequence{};
    std::array<float, VoiceCount> remaining{};
    std::array<int, VoiceCount> priority{};
    float combatGate = 0, rewardDuck = 0;
};
}
