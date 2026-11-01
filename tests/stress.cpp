#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

struct Measurements {
    std::vector<double> updates;
    std::size_t enemies = 0, projectiles = 0, zones = 0, pickups = 0, eggs = 0, summons = 0;

    void observe(const snake::Game& game, double microseconds, int frame) {
        updates.push_back(microseconds);
        enemies = std::max(enemies, game.enemies.size());
        projectiles = std::max(projectiles, game.projectiles.size());
        zones = std::max(zones, game.zones.size());
        pickups = std::max(pickups, game.pickups.size());
        eggs=std::max(eggs,game.nestEggs.size());summons=std::max(summons,game.miniSnakes.size());
        require(game.enemies.size() <= 1800, "Enemy population exceeded its cap.");
        require(game.projectiles.size() <= 1600, "Projectile population exceeded its cap.");
        require(game.zones.size() <= 128, "Effect population exceeded its cap.");
        require(game.pickups.size() <= 1403, "Pickup population exceeded its cap.");
        require(game.particles.size() <= 1600, "Particle population exceeded its cap.");
        require(game.damageNumbers.size() <= 150, "Damage-number population exceeded its cap.");
        require(game.nestEggs.size()<=25 && game.miniSnakes.size()<=60,"Nest population exceeded its cap.");
        require(std::isfinite(game.player.position.x) && std::isfinite(game.player.position.y), "Player position became non-finite.");
        require(std::isfinite(game.elapsed), "Run clock became non-finite.");
        if (frame % 120 == 0) {
            for (const auto& enemy : game.enemies) {
                require(std::isfinite(enemy.position.x) && std::isfinite(enemy.position.y) && std::isfinite(enemy.hp), "Enemy state became non-finite.");
                require(snake::sameCell(snake::gridPosition(enemy.position),enemy.position),"Enemy left the grid during the stress run.");
                for(int i=0;i<enemy.trailSize;++i)require(snake::sameCell(snake::gridPosition(enemy.trail[std::size_t(i)]),enemy.trail[std::size_t(i)]),"Enemy body left the grid during the stress run.");
            }
            for(const auto& mini:game.miniSnakes)for(const auto segment:mini.segments)
                require(snake::sameCell(snake::gridPosition(segment),segment),"Summon left the grid during the stress run.");
            for(const auto& pickup:game.pickups)
                require(snake::inGridBounds(pickup.position)&&snake::sameCell(snake::gridPosition(pickup.position),pickup.position),"Ground loot left the grid during the stress run.");
            for (const auto& shot : game.projectiles)
                require(std::isfinite(shot.position.x) && std::isfinite(shot.position.y) && std::isfinite(shot.damage), "Projectile state became non-finite.");
        }
    }

    void print(const char* name) {
        require(!updates.empty(), "Scenario did not update.");
        const double average = std::accumulate(updates.begin(), updates.end(), 0.0) / double(updates.size());
        std::sort(updates.begin(), updates.end());
        const std::size_t percentile = std::min(updates.size() - 1, std::size_t(double(updates.size()) * .99));
        std::cout << name << ": " << updates.size() << " ticks; update us avg=" << std::fixed << std::setprecision(2) << average
                  << " p99=" << updates[percentile] << " worst=" << updates.back()
                  << "; peaks enemies=" << enemies << " projectiles=" << projectiles
                  << " zones=" << zones << " pickups=" << pickups << " eggs=" << eggs << " summons=" << summons << '\n';
    }
};

void settle(snake::Game& game, int frame) {
    // Choices obey the same public offer/slot/rank rules as a real run.
    for (int pending = 0; game.phase == snake::Phase::LevelUp && pending < 256; ++pending) {
        bool selected = false;
        for (int offset = 0; offset < 3; ++offset) {
            if (game.choose((frame + pending + offset) % 3)) { selected = true; break; }
        }
        require(selected, "Level-up could not progress, including exhausted-pool fallback.");
    }
    require(game.phase != snake::Phase::LevelUp, "Unbounded pending level-ups.");
}

void fullRun(const snake::Catalog& catalog,bool summoner=false,const snake::PermanentUpgrades& permanent={}) {
    snake::Game game(catalog,summoner?0xB200Du:0x20B00Du);
    if(permanent.totalRanks())game.reset(0,summoner?0xB200Du:0x20B00Du,permanent);
    // These scripted loops isolate combat/progression under contact immunity.
    // Grey barriers intentionally ignore immunity and need obstacle steering;
    // their complete 20-minute lifecycle is covered by snake_grey_snake_tests.
    game.greySnakes.enabled=false;
    // Lethal flood contact has its own lifecycle and obstacle fixtures.
    game.flood.enabled=false;
    game.player.invulnerable = 5000;
    if(summoner){
        game.owned.clear();
        for(const char* id:{"snake_nest","viper_fangs","plague_mortar","venom_nova","rapid_fire","coiled_volley","toxic_expanse","chronofield"}){
            const int definition=catalog.find(id);require(definition>=0,"Missing summoner fixture upgrade.");
            const auto& upgrade=catalog.upgrades[std::size_t(definition)];
            game.owned.push_back({definition,upgrade.maxRank,upgrade.kind==snake::UpgradeKind::Weapon,0});
        }
    }
    Measurements measures;
    measures.updates.reserve(std::size_t(snake::RunDuration*60)+200);
    constexpr snake::Vec directions[]{{1,0},{0,1},{-1,0},{0,-1}};
    // Three seconds of allowance accommodates single-precision clock accumulation.
    const int maximumFrames = int((snake::RunDuration + 3) / snake::FixedStep);
    for (int frame = 0; frame < maximumFrames && game.phase != snake::Phase::Victory; ++frame) {
        const auto start = Clock::now();
        // Keep the route wider than a fully grown player so body contact
        // cannot stall movement while endurance immunity is active.
        game.update(snake::FixedStep, {directions[(frame / 480) % 4]});
        settle(game, frame);
        const double elapsed = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        measures.observe(game, elapsed, frame);
        require(game.phase != snake::Phase::Dead, "Invulnerable full-run scenario died.");
    }
    require(game.phase == snake::Phase::Victory, "Full-length run never reached victory.");
    require(game.elapsed == snake::RunDuration, "Victory clock did not clamp to the run duration.");
    require(game.bossClock == snake::BossWaveCount, "Director did not schedule every boss wave.");
    require(game.kills > 0 && game.player.level > 1, "Full-run scenario did not exercise combat/progression.");
    measures.print(summoner?"30-minute Snake Nest/Hydra endurance":"30-minute scripted run");
    if(summoner)require(measures.eggs>0 && measures.summons>0,"Summoner endurance never exercised the egg/brood lifecycle.");
    std::cout << "  Result: victory, level=" << game.player.level << " kills=" << game.kills
              << " bosses defeated=" << game.bossesKilled << " scheduled=" << game.bossClock << '\n';
}

void concentratedLateRun(const snake::Catalog& catalog) {
    snake::Game game(catalog, 0xD3E5Eu);
    game.elapsed = snake::RunDuration - 6;
    game.bossClock = float(snake::BossWaveCount); // Earlier waves already occurred.
    game.player.invulnerable = 5000;
    game.enemies.clear(); game.pickups.clear(); game.owned.clear();
    for (const char* id : {"viper_fangs", "snake_nest", "plague_mortar", "venom_nova", "rapid_fire", "graviton", "coiled_volley", "toxic_expanse"}) {
        const int definition = catalog.find(id);
        require(definition >= 0, std::string("Missing fixture definition: ") + id);
        const auto& upgrade = catalog.upgrades[definition];
        game.owned.push_back({definition, upgrade.maxRank, upgrade.kind == snake::UpgradeKind::Weapon, 0});
    }
    for (int index = 0; index < 1800; ++index) {
        snake::Enemy enemy;
        const float angle = float(index) * 2.39996323f;
        const float distance = 18 + float(index % 165);
        enemy.id = 100000u + static_cast<unsigned>(index);
        enemy.position = {game.player.position.x + std::cos(angle) * distance, game.player.position.y + std::sin(angle) * distance};
        enemy.hp = enemy.maxHp = 100000000;
        enemy.speed = 0;
        enemy.kind = index % 200 == 0 ? snake::EnemyKind::Boss : snake::EnemyKind::Red;
        enemy.radius = enemy.kind == snake::EnemyKind::Boss ? 7.0f : 3.5f;
        enemy.trailSize = enemy.kind == snake::EnemyKind::Boss ? 8 : 3;
        enemy.trail.fill(enemy.position);
        game.enemies.push_back(enemy);
    }
    Measurements measures;
    constexpr snake::Vec directions[]{{1,0},{0,1},{-1,0},{0,-1}};
    for (int frame = 0; frame < 300; ++frame) {
        const auto start = Clock::now();
        game.update(snake::FixedStep, {directions[(frame / 15) % 4]});
        settle(game, frame);
        const double elapsed = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        measures.observe(game, elapsed, frame);
        require(game.phase == snake::Phase::Playing, "Late-run fixture unexpectedly stopped simulation.");
    }
    measures.print("Concentrated late-run max build");
    // A dense swarm consumes projectiles rapidly through collisions. Population
    // pressure comes from all 1800 live enemies; don't require missed shots to
    // accumulate before considering this a useful combat workload.
    require(measures.enemies == 1800 && measures.projectiles > 0 && measures.zones > 0,
            "Dense scenario did not exercise the intended combat population.");
    require(std::any_of(game.enemies.begin(), game.enemies.end(), [](const snake::Enemy& enemy) { return enemy.hp < enemy.maxHp; }),
            "Dense combat workload never damaged the swarm.");
}
void floodLifecycle(const snake::Catalog& catalog){
    snake::Game game(catalog,1942);game.enemies.clear();game.pickups.clear();game.owned.clear();
    game.greySnakes.enabled=false;game.spawnClock=-10000;game.bossClock=100;game.player.moveClock=-10000;game.player.invulnerable=10000;
    // A closed, connected body bend protects the head. Unlike immunity this
    // exercises real water/body collision through every scheduled flood.
    auto& body=game.player.segments;body.clear();
    for(int y=800;y<=824;y+=8)body.push_back({800,float(y)});
    for(int x=792;x>=776;x-=8)body.push_back({float(x),824});
    for(int y=816;y>=776;y-=8)body.push_back({776,float(y)});
    for(int x=784;x<=824;x+=8)body.push_back({float(x),776});
    for(int y=784;y<=824;y+=8)body.push_back({824,float(y)});
    body.push_back({816,824});body.push_back({808,824});
    game.player.previousSegments=body;game.player.length=int(body.size());
    for(int i=0;i<1800;++i){
        snake::Enemy enemy;enemy.id=std::uint32_t(10000+i);enemy.position=enemy.previous={960+float((i%24)*8),704+float((i/24%24)*8)};
        enemy.trail.fill(enemy.position);enemy.previousTrail=enemy.trail;enemy.motionReady=true;enemy.speed=0;enemy.hp=enemy.maxHp=1e12;game.enemies.push_back(enemy);
    }
    const auto* waterStorage=game.flood.particles.data();Measurements measures;std::size_t peakWater=0;bool joined=false;
    const int runTicks=int(snake::RunDuration*60)+1;measures.updates.reserve(std::size_t(runTicks));
    for(int frame=0;frame<runTicks && game.phase==snake::Phase::Playing;++frame){
        const auto start=Clock::now();game.update(snake::FixedStep,{});
        measures.observe(game,std::chrono::duration<double,std::micro>(Clock::now()-start).count(),frame);
        require(game.flood.particles.size()<=snake::FloodCapacity && game.flood.particles.data()==waterStorage,"Flood lifecycle grew sample storage.");
        peakWater=std::max(peakWater,game.flood.particles.size());joined=joined || game.flood.waveCount()==2;
        for(const auto& water:game.flood.particles)require(std::isfinite(water.position.x) && std::isfinite(water.position.y) && std::isfinite(water.velocity.x),"Flood lifecycle became non-finite.");
        require(game.phase!=snake::Phase::Dead,"Water crossed a closed body barrier to kill the head.");
    }
    const int floods=1+int((game.flood.clock-snake::FloodFirstSeconds)/snake::FloodIntervalSeconds);
    require(game.phase==snake::Phase::Victory && game.flood.serial==floods && joined && peakWater==1280 && game.flood.waveCount()<=2,"Full-length minute arrivals did not overlap within their bounded lifetimes.");
    require(game.kills==0 && game.pickups.empty() && game.snakeDeaths.empty(),"Flood expiry emitted loot or death effects.");
    measures.print("Thirty-minute flood lifecycle with dense obstacles");
}
} // namespace

int main(int argc,char** argv) {
    try {
        const bool inherited=argc==2 && std::string(argv[1])=="--permanent";
        require(argc==1 || inherited,"Expected no arguments or --permanent.");
        const auto catalog = snake::Catalog::load(SNAKE_TEST_DATA_DIR);
        snake::PermanentUpgrades permanent;
        if(inherited){
            for(std::size_t i=0;i<snake::PermanentUpgradeCount;++i)permanent.ranks[i]=snake::permanentDefinitions()[i].maxRank;
            std::cout<<"Maximum permanent upgrades enabled for both full runs.\n";
        }
        fullRun(catalog,false,permanent);
        fullRun(catalog,true,permanent);
        if(!inherited){concentratedLateRun(catalog);floodLifecycle(catalog);}
        std::cout << "Stress checks passed. Timings measure simulation only, with no graphics.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Stress check failed: " << error.what() << '\n';
        return 1;
    }
}
