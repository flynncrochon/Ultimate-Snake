#include "audio_design.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace snake::audio {
namespace {
constexpr double Pi = 3.14159265358979323846;
constexpr std::array<Definition, EffectCount> Definitions{{
    {"shot",   .047f, .30f, .18f, 0},
    {"hit",    .058f, .32f, .14f, 1},
    {"kill",   .084f, .34f, .17f, 2},
    {"hurt",   .200f, .48f, .55f, 6},
    {"pickup", .112f, .38f, .13f, 3},
    {"level",  .480f, .56f, .60f, 7},
    {"evolve", .740f, .59f, .85f, 9},
    {"chest",  .410f, .56f, .48f, 8},
    {"win",   1.080f, .60f, 1.20f, 10}
}};

std::size_t index(Event::Type effect) {
    const auto value = static_cast<std::size_t>(effect);
    if (value >= EffectCount) throw std::invalid_argument("Unknown audio effect");
    return value;
}
double smooth(double value) {value = std::clamp(value, 0.0, 1.0); return value * value * (3 - 2 * value);}
double envelope(double t, double duration, double attack, double decay) {
    if (t < 0 || t >= duration) return 0;
    return smooth(t / attack) * smooth((duration - t) / std::min(.028, duration * .3)) * std::exp(-decay * t / duration);
}
void tone(std::vector<double>& samples, double start, double length, double from, double to,
          double gain, double attack = .006, double decay = 3.5, double overtone = .10) {
    double phase = 0;
    for (std::size_t i = std::size_t(start * SampleRate); i < samples.size(); ++i) {
        const double t = double(i) / SampleRate - start;
        if (t >= length) break;
        const double frequency = from + (to - from) * std::clamp(t / length, 0.0, 1.0);
        const double wave = std::sin(phase) + overtone * std::sin(phase * 2);
        samples[i] += gain * envelope(t, length, attack, decay) * wave;
        phase += 2 * Pi * frequency / SampleRate;
    }
}
void air(std::vector<double>& samples, double length, double gain, std::uint32_t seed, double pole = .045) {
    double filtered = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double t = double(i) / SampleRate;
        if (t >= length) break;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const double noise = double(seed) / 2147483647.5 - 1;
        filtered += pole * (noise - filtered);
        samples[i] += gain * filtered * envelope(t, length, .008, 3);
    }
}
}

const Definition& definition(Event::Type effect) {return Definitions[index(effect)];}

std::vector<std::int16_t> synthesizeReelTick() {
    const double duration = ReelTickDuration;
    std::vector<std::int16_t> result(std::size_t(std::ceil(duration * SampleRate)));
    std::uint32_t seed = 0x51C20A5u;
    double fast = 0, slow = 0;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const double t = double(i) / SampleRate;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const double noise = double(seed) / 2147483647.5 - 1;
        fast += .50 * (noise - fast);
        slow += .12 * (noise - slow);
        const double contact = smooth(t / .0004) * std::exp(-t / .0034);
        const double body = std::sin(2 * Pi * 1700 * t) * std::exp(-t / .0014);
        const double tapTime = std::max(0.0, t - .0032);
        const double tap = smooth(tapTime / .00025) * std::exp(-tapTime / .0015);
        const double fade = smooth((duration - t) / .002);
        // A damped noise impact and a very short fixed resonance make a dry
        // ratchet click. Neither pitch nor waveform changes as the reel slows.
        const double value = ((fast - slow) * (.30 * contact + .045 * tap)
            + .04 * body * smooth(t / .0004)) * fade;
        result[i] = std::int16_t(std::lround(std::clamp(value, -.22, .22) * 32767));
    }
    if (!result.empty()) result.front() = result.back() = 0;
    return result;
}

std::vector<std::int16_t> synthesize(Event::Type effect, std::size_t variation) {
    const auto& d = definition(effect);
    const double pitch = std::pow(2.0, (double(variation % VariationCount) - 1) * .16 / 12.0);
    std::vector<double> work(std::size_t(std::ceil(double(d.duration) * SampleRate)));
    auto note = [&](double start, double duration, double frequency, double end, double gain,
                    double attack = .006, double decay = 3.5, double harmonic = .10) {
        tone(work, start, duration, frequency * pitch, end * pitch, gain, attack, decay, harmonic);
    };
    const std::uint32_t seed = 0xA0173u + std::uint32_t(index(effect) * 173 + variation * 7919);
    switch (effect) {
    case Event::Shot:
        note(0, .047, 330, 230, .052, .004, 4.2, .025); air(work, .035, .055, seed); break;
    case Event::Hit:
        note(0, .058, 185, 105, .073, .004, 4.0, .045); air(work, .036, .035, seed); break;
    case Event::Kill:
        note(0, .084, 280, 160, .078, .005, 4.5, .055);
        note(.008, .055, 560, 350, .013, .005, 5.0, 0); break;
    case Event::Hurt:
        note(0, .19, 146.83, 92.5, .113, .009, 3, .045); air(work, .13, .075, seed, .025); break;
    case Event::Pickup:
        note(0, .112, 659.25, 659.25, .074, .006, 5, .025);
        note(.012, .085, 987.77, 987.77, .016, .007, 5, 0); break;
    case Event::Level:
        note(0, .25, 392, 392, .086, .008, 3, .06);
        note(.075, .27, 493.88, 493.88, .076, .008, 3, .045);
        note(.15, .33, 587.33, 587.33, .072, .010, 3, .025); break;
    case Event::Evolve:
        note(0, .38, 349.23, 349.23, .068, .013, 2.7, .025);
        note(.07, .42, 440, 440, .065, .013, 2.7, .025);
        note(.14, .44, 523.25, 523.25, .063, .013, 2.7, .025);
        note(.23, .51, 698.46, 698.46, .060, .015, 3.0, .015); break;
    case Event::Chest:
        note(0, .10, 196, 130.81, .065, .004, 4, .04);
        note(.04, .24, 523.25, 523.25, .083, .008, 3.5, .025);
        note(.105, .305, 659.25, 659.25, .065, .010, 3.5, .015); break;
    case Event::Win:
        note(0, .40, 392, 392, .077, .014, 2.5, .025);
        note(.13, .40, 493.88, 493.88, .068, .014, 2.5, .025);
        note(.26, .44, 587.33, 587.33, .068, .014, 2.5, .025);
        note(.43, .65, 783.99, 783.99, .065, .018, 3, .015); break;
    }
    std::vector<std::int16_t> result(work.size());
    for (std::size_t i = 0; i < work.size(); ++i)
        result[i] = std::int16_t(std::lround(std::clamp(work[i], -.22, .22) * 32767));
    if (!result.empty()) result.front() = result.back() = 0;
    return result;
}

void Director::queue(Event::Type effect) {pending[index(effect)] = true;}
void Director::reset() {pending.fill(false); cooldown.fill(0); remaining.fill(0); priority.fill(-1); sequence.fill(0); combatGate = rewardDuck = 0;}

Frame Director::advance(float seconds, bool enabled) {
    Frame result;
    if (!enabled) {reset(); return result;}
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 1.0f) : 0;
    for (auto& time : cooldown) time = std::max(0.0f, time - dt);
    for (auto& time : remaining) time = std::max(0.0f, time - dt);
    combatGate = std::max(0.0f, combatGate - dt); rewardDuck = std::max(0.0f, rewardDuck - dt);
    // A chest that evolves a weapon uses the evolution chord rather than two
    // stacked reward sounds. Likewise victory takes precedence over level-up.
    if (pending[Event::Evolve]) pending[Event::Chest] = pending[Event::Level] = false;
    if (pending[Event::Win]) pending[Event::Chest] = pending[Event::Level] = pending[Event::Evolve] = false;
    constexpr std::array<Event::Type, EffectCount> order{{Event::Win, Event::Evolve, Event::Chest, Event::Level,
        Event::Hurt, Event::Pickup, Event::Kill, Event::Hit, Event::Shot}};
    for (const auto effect : order) {
        const auto i = index(effect); const auto& d = Definitions[i];
        if (!pending[i]) continue;
        const bool combat = d.priority <= 3;
        // Transient feedback is never held to be played late in a silent frame.
        pending[i] = false;
        if (cooldown[i] > 0 || (combat && (combatGate > 0 || rewardDuck > 0))) continue;
        std::size_t slot = VoiceCount;
        for (std::size_t voice = 0; voice < VoiceCount; ++voice)
            if (remaining[voice] <= 0) {slot = voice; break;}
        if (slot == VoiceCount && d.priority >= 6) {
            const auto low = std::min_element(priority.begin(), priority.end());
            if (*low < d.priority) slot = std::size_t(low - priority.begin());
        }
        if (slot == VoiceCount) continue;
        const auto variant = std::size_t((sequence[i]++ * 2 + i) % VariationCount);
        result.cues[result.count++] = {effect, variant, slot};
        remaining[slot] = d.duration; priority[slot] = d.priority; cooldown[i] = d.cooldown;
        if (combat) combatGate = .105f;
        if (d.priority >= 7) rewardDuck = std::max(rewardDuck, d.duration + .08f);
        if (result.count == VoiceCount) break;
    }
    // Overflow never carries a burst into later frames.
    pending.fill(false);
    return result;
}
}
