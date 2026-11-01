#include "core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace snake {
namespace {
// Minute populations/cadences follow normal Mad Forest. HP, contact damage,
// XP and species mixes are authored for Snake's existing weapons and routing.
// In particular the 11:00/21:00 harvest waves give relief from elite waves.
constexpr std::array<StageWave, StageMinutes> Waves{{
    { 15,1.00f,{100, 0, 0},   50,19, 5,1},
    { 30,1.00f,{ 85, 0,15},  100,19, 6,1},
    { 50, .50f,{ 65, 0,35},   50,20, 5,1},
    { 40, .25f,{100, 0, 0},  180,20, 8,1},
    { 30,1.00f,{ 75, 0,25},  150,21, 8,1},
    { 10,1.00f,{ 50,50, 0},  350,20,10,3},
    { 20, .50f,{ 70,30, 0},  300,21,10,2},
    { 80, .50f,{ 55,15,30},  200,23, 8,1},
    {100,1.50f,{100, 0, 0},  250,21,10,1},
    { 30, .50f,{ 60, 0,40},  600,24,12,3},
    { 10, .50f,{ 40,60, 0},  900,22,14,3},
    {300, .10f,{100, 0, 0},  180,21, 8,1},
    { 20, .25f,{ 45,15,40}, 1000,25,14,3},
    {150, .50f,{ 35, 0,65},  350,26,10,1},
    { 20, .10f,{ 40,10,50}, 1400,26,16,3},
    {100, .10f,{ 50,25,25}, 1700,25,16,3},
    {100, .10f,{ 40,50,10}, 2000,25,18,3},
    { 20,1.00f,{100, 0, 0}, 3000,22,20,5},
    { 60, .50f,{ 70,30, 0}, 2600,24,18,3},
    {100, .50f,{ 60,40, 0}, 3000,25,20,4},
    {100, .10f,{ 50,25,25}, 3500,26,20,4},
    {300, .10f,{100, 0, 0}, 1000,22,12,1},
    {200, .10f,{ 65,35, 0}, 4000,26,22,2},
    {300, .10f,{ 65,35, 0}, 4500,27,22,3},
    {300, .10f,{ 75,25, 0}, 5000,27,24,3},
    {100, .10f,{ 50,50, 0}, 6000,25,24,5},
    {150, .10f,{ 65,35, 0}, 5000,26,22,3},
    {300, .10f,{ 50,35,15}, 5500,28,24,4},
    {300, .10f,{ 50, 0,50},  150,29,26,10,true},
    {300, .10f,{ 50, 0,50},  200,30,28,10,true}
}};
// Arcana-only 11:00/21:00 chests are omitted; Snake has no Arcana system.
constexpr std::array<BossWave, BossWaveCount> Bosses{{
    {1,100},{3,100},{5,200},{7,150},{8,400},{9,300},{10,750},
    {12,200},{14,300},{15,1000},{16,300},{18,400},{20,1200},
    {22,400},{23,500},{24,1000},{25,1500},{27,500},{29,750}
}};
}

int experienceForLevel(int level) {
    const long long n = std::max(1, level);
    const long long cost = n < 20 ? 10*n-5 : n == 20 ? 795 :
        n < 40 ? 13*n-6 : n == 40 ? 2914 : 16*n-8;
    return int(std::min(cost, static_cast<long long>(std::numeric_limits<int>::max())));
}
float breakpointGrowth(int level) { return level == 20 || level == 40 ? 1.0f : 0.0f; }
int stageMinute(float elapsed) {
    if (!std::isfinite(elapsed) || elapsed <= 0) return 0;
    return int(std::min(elapsed / WaveDuration, float(StageMinutes-1)));
}
const StageWave& stageWave(float elapsed) { return Waves[std::size_t(stageMinute(elapsed))]; }
const std::array<BossWave, BossWaveCount>& bossWaves() { return Bosses; }
EnemyProfile stageEnemy(EnemyKind kind, float elapsed, int level) {
    const auto& wave = stageWave(elapsed);
    const int playerLevel = std::max(1, level);
    if (kind == EnemyKind::Boss) {
        float health = Bosses.front().healthPerLevel;
        for (const auto& boss : Bosses) if (elapsed >= boss.minute*WaveDuration) health = boss.healthPerLevel;
        return {double(health)*playerLevel, 15.0f, std::min(30.0f,wave.damage+8), 30};
    }
    const float factor = kind == EnemyKind::Swift ? .6f : kind == EnemyKind::Splitter ? 1.5f : 1.0f;
    const double health = double(wave.health)*factor*(wave.healthPerLevel ? playerLevel : 1);
    return {health, wave.speed+(kind == EnemyKind::Swift ? 17.0f : kind == EnemyKind::Splitter ? -2.0f : 0.0f),
        wave.damage, wave.experience*(kind == EnemyKind::Splitter ? 2 : 1)};
}

void Game::updateDirector(float dt) {
    const auto& wave=stageWave(elapsed);
    const int minute=stageMinute(elapsed);
    spawnClock+=dt;
    if(spawnClock<0)pendingSpawns=0; // Explicit fixture suppression, including queued refills.
    else {
        if(directorMinute!=minute){
            directorMinute=minute;pendingSpawns=0;
            spawnClock=std::max(spawnClock,wave.interval);
        }
        if(spawnClock>=wave.interval || pendingSpawns>0){
            const int population=int(std::count_if(enemies.begin(),enemies.end(),[](const Enemy& enemy){
                return enemy.alive && enemy.kind!=EnemyKind::Boss && enemy.kind!=EnemyKind::Flood;
            }));
            if(spawnClock>=wave.interval){
                spawnClock=std::fmod(spawnClock,wave.interval);
                const int types=int(std::count_if(wave.weights.begin(),wave.weights.end(),[](int weight){return weight>0;}));
                pendingSpawns=std::max(wave.minimum-population,types);
            }
            pendingSpawns=std::min(pendingSpawns,std::max(0,OrdinaryEnemyLimit-population));
            const int count=std::min({pendingSpawns,DirectorSpawnBudget,1800-int(enemies.size())});
            const int weight=wave.weights[0]+wave.weights[1]+wave.weights[2];
            constexpr float pi=3.14159265358979323846f;
            for(int i=0;i<count;++i){
                const float angle=random(0,2*pi),ticket=random(0,float(weight));
                const auto kind=ticket<wave.weights[0]?EnemyKind::Red:
                    ticket<wave.weights[0]+wave.weights[1]?EnemyKind::Splitter:EnemyKind::Swift;
                spawnEnemy(kind,{player.position.x+std::cos(angle)*random(155,225),
                                 player.position.y+std::sin(angle)*random(125,185)});
            }
            pendingSpawns-=count;
        }
    }
    // One scheduled boss at most per update. Full ordinary pools yield a slot;
    // a full special-only pool keeps the due boss pending instead of losing it.
    if(bossClock<0 || bossClock>=float(Bosses.size()))return;
    const auto& boss=Bosses[std::size_t(bossClock)];
    if(elapsed<boss.minute*WaveDuration)return;
    if(enemies.size()>=1800){
        const auto ordinary=std::find_if(enemies.begin(),enemies.end(),[](const Enemy& enemy){
            return enemy.kind!=EnemyKind::Boss && enemy.kind!=EnemyKind::Flood;
        });
        if(ordinary!=enemies.end()){*ordinary=std::move(enemies.back());enemies.pop_back();}
    }
    if(enemies.size()<1800){
        constexpr float pi=3.14159265358979323846f;
        const float angle=random(0,2*pi);
        spawnEnemy(EnemyKind::Boss,{player.position.x+std::cos(angle)*155,player.position.y+std::sin(angle)*125});
        enemies.back().hp=enemies.back().maxHp=double(boss.healthPerLevel)*std::max(1,player.level);
        ++bossClock;notice="AN ANCIENT SERPENT APPROACHES";noticeTime=4;
    }
}
}
