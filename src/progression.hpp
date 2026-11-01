#pragma once

#include <array>
#include <cstddef>

namespace snake {
enum class EnemyKind;

constexpr int StageMinutes = 30;
constexpr float WaveDuration = 60.0f;
constexpr float RunDuration = StageMinutes * WaveDuration;
constexpr int OrdinaryEnemyLimit = 300;
constexpr int DirectorSpawnBudget = 8;

// Costs are for the CURRENT level -> next level. See docs/PROGRESSION.md for
// the reference, breakpoint interpretation, and deliberate Snake adaptations.
int experienceForLevel(int level);
float breakpointGrowth(int level);

struct StageWave {
    int minimum;
    float interval;
    std::array<int, 3> weights; // Red, Splitter, Swift.
    float health, speed, damage;
    int experience;
    bool healthPerLevel = false;
};
struct EnemyProfile {
    double health;
    float speed, damage;
    int experience;
};
struct BossWave { int minute; float healthPerLevel; };
constexpr std::size_t BossWaveCount = 19;

int stageMinute(float elapsed);
const StageWave& stageWave(float elapsed);
const std::array<BossWave, BossWaveCount>& bossWaves();
EnemyProfile stageEnemy(EnemyKind kind, float elapsed, int level);
}
