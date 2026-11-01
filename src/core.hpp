#pragma once

#include "progression.hpp"
#include "permanent_upgrades.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace snake {
constexpr int Tile = 8;
constexpr int ArenaTiles = 200;
constexpr float ArenaSize = float(ArenaTiles * Tile);
constexpr float FixedStep = 1.0f / 60.0f;
constexpr int WeaponSlots = 4;
constexpr int PassiveSlots = 4;
constexpr int MaxPlayerLength = 120;
constexpr float CollisionGraceSeconds = 1.0f;

struct Vec { float x = 0, y = 0; };
enum class Phase { Playing, LevelUp, Paused, Dead, Victory };
enum class UpgradeKind { Weapon, Passive };
enum class Pattern { Shot, Homing, Mortar, Nest, Nova, Beam, Ricochet, Pit, Lash };
enum class Stat { Damage, Cooldown, Area, Duration, Amount, Pickup, Crit, Growth, Range, Armor, Speed, Count };
struct Modifier { Stat stat = Stat::Damage; double perRank = 0; };
struct UpgradeDef {
    std::string id, name, description;
    UpgradeKind kind = UpgradeKind::Passive;
    int maxRank = 5;
    float weight = 1;
    Pattern pattern = Pattern::Shot;
    float damage = 0, cooldown = 1, speed = 100, range = 120, area = 8, duration = 1;
    int amount = 1, pierce = 0;
    std::vector<Modifier> modifiers;
};
struct EvolutionDef { std::string weapon, passive, name, description; float damage = 1.8f, cooldown = .75f, area = 1.3f, range = 1; };
struct Catalog {
    std::vector<UpgradeDef> upgrades;
    std::vector<EvolutionDef> evolutions;
    static Catalog load(const std::string& directory);
    int find(const std::string& id) const;
};
struct OwnedUpgrade { int definition = -1, rank = 0; bool evolved = false; float cooldown = 0; };
struct Stats {
    double damage = 1;
    double growth = 1, recovery = 0;
    float cooldown = 1, area = 1, duration = 1, pickup = float(Tile), crit = 0, range = 1, speed = 1;
    int amount = 0, armor = 0;
};
struct WeaponSpec {
    Pattern pattern = Pattern::Shot;
    bool evolved = false, persistent = false, damageBonus = true;
    float damage = 0, cooldown = 1, speed = 0, range = 0, area = 0, duration = 0, tickInterval = 0, turnSpeed = 0;
    double unroundedDamage = 0, baseDamage = 0;
    int amount = 1, pierce = 0, bounces = 0, broodCount = 0;
};
struct Player {
    Vec position{ArenaSize / 2, ArenaSize / 2}, previous{ArenaSize / 2, ArenaSize / 2};
    Vec direction{1, 0}, queued{1, 0};
    std::vector<Vec> segments;
    std::vector<Vec> previousSegments;
    float moveClock = 0, moveInterval = .12f, hp = 100, maxHp = 100, invulnerable = 0;
    // Negative when clear; otherwise simulation seconds since a blocked move.
    float collisionClock = -1;
    int level = 1, xp = 0, nextXp = 5, length = 7;
    double xpRemainder = 0;
};
constexpr double GreyMoveSeconds = .06, GreyLifetimeSeconds = 10, GreySpawnSeconds = 90;
constexpr double GreyFirstPressureSeconds = 10, GreyPressureSpawnSeconds = 30;
constexpr std::size_t GreyTrailCapacity = std::size_t(RunDuration / GreyPressureSpawnSeconds);
constexpr std::size_t GreyPathCapacity = 168; // Spawn cell plus at most 166 moves in ten seconds.
constexpr std::size_t GreyRouteCapacity = 512;
struct GreyRouteStep { Vec position, direction; };
struct GreySnake {
    // Append-only, oldest cell first. Only the head moves; every visited cell
    // remains a barrier for the rest of the run, including self-crossings.
    std::array<Vec,GreyPathCapacity> path{};
    std::size_t count = 0;
    Vec previous, direction{0,1}, target, wallDirection, minimum, maximum;
    double age = 0, moveClock = 0;
    int wallTicks = 0;
    bool alive = true, wall = false, settling = false, cutting = false;
    std::uint8_t id = 0;
};
struct GreySnakeSystem {
    std::vector<GreySnake> trails;
    // First owner of each occupied cell. A snake may cross its own trail,
    // but keeps a one-cell gap from every other snake's permanent barriers.
    std::array<std::uint8_t,ArenaTiles*ArenaTiles> owners{};
    // Successful grid moves only. A bounded history recognizes reused lanes
    // and predicts the upcoming turns of a familiar circuit without a map scan.
    std::array<GreyRouteStep,GreyRouteCapacity> route{};
    std::size_t routeCount = 0, routeNext = 0;
    double clock = 0, nextSpawn = GreySpawnSeconds;
    double lastMoveClock = -2;
    int straightSteps = 0, repeatedSteps = 0;
    std::size_t barrierCount = 0;
    bool enabled = true, attemptedSpawn = false;
    void reset();
    void recordMove(Vec position,Vec direction);
    float predictability() const;
    double spawnTime() const;
    Vec predict(Vec position,Vec direction,int steps,Vec& heading) const;
    bool blocks(Vec position) const;
    bool preservesEscape(Vec position) const;
    bool overlapsOther(Vec position,std::uint8_t owner = 0) const;
    void append(GreySnake& snake,Vec position);
};
enum class EnemyKind { Red, Splitter, Swift, Boss, Flood };
constexpr float SplitBirthFlash = .04f, SplitBirthDuration = .30f;
struct SplitBirth {
    // Grid coordinates fit in one byte. Each offspring owns a compact copy,
    // independent of death-pool compaction and saturation.
    struct Point {std::uint8_t previousX=0,previousY=0,x=0,y=0;};
    std::array<Point,9> path{};
    float alpha=1,age=SplitBirthDuration;
    std::uint8_t count=0;
    std::int8_t normalX=0,normalY=1,side=0;
};
static_assert(sizeof(SplitBirth)==48,"Split birth storage changed");
struct Enemy {
    std::uint32_t id = 0;
    Vec position, velocity;
    Vec previous, direction{0,1};
    std::array<Vec, 8> trail{};
    std::array<Vec, 8> previousTrail{};
    double hp = 10, maxHp = 10;
    float speed = 15, radius = 3, hitFlash = 0, poison = 0, slow = 0;
    float moveClock = 0, moveInterval = .4f;
    EnemyKind kind = EnemyKind::Red;
    int trailSize = 3;
    // Zero retains the legacy reward/contact behavior for hand-built fixtures.
    int experience = 0;
    float contactDamage = 0;
    float slowFactor = .4f, orbitCooldown = 0;
    float gravityHold = 0;
    bool alive = true, motionReady = false, grabbed = false, devoured = false;
    // Presentation distinguishes combat displacement from spawn/recycling.
    std::uint32_t forceSerial = 0, placementSerial = 0;
    SplitBirth splitBirth;
};

constexpr std::size_t FloodCapacity = 1536;
constexpr float FloodRadius = 3.0f, FloodFirstSeconds = 60, FloodIntervalSeconds = 60, FloodLifetime = 90;
constexpr float FloodHealth = 20, FloodContactDamage = 10;
struct WaterParticle {
    Vec position, previous, velocity;
    float foam = 0;
    float age = 0, warning = 1.5f;
    std::uint32_t wave = 0;
    float hp = FloodHealth;
};
// Arriving waves join one shared pressure, cohesion and viscosity simulation.
// Each sample keeps its own warning and lifetime. The single
// enemy handle lets existing weapons target the nearest part of the surface.
// All scratch storage is prepared at construction, never at first contact.
struct FloodSystem {
    FloodSystem();
    ~FloodSystem();
    FloodSystem(const FloodSystem&);
    FloodSystem& operator=(const FloodSystem&);
    FloodSystem(FloodSystem&&) noexcept;
    FloodSystem& operator=(FloodSystem&&) noexcept;
    std::vector<WaterParticle> particles;
    Vec center, direction{1,0};
    float age = 0, warning = 1.5f, lifetime = FloodLifetime;
    double clock = 0, nextSpawn = FloodFirstSeconds;
    std::uint32_t enemyId = 0, serial = 0, repulsions = 0;
    bool enabled = true;
    bool active() const { return !particles.empty(); }
    bool dangerous() const { return hazards > 0; }
    bool dangerous(const WaterParticle& p) const { return p.hp > 0 && p.age >= p.warning && p.age < lifetime; }
    std::size_t waveCount() const;
    void reset();
    bool start(Vec position,Vec heading,int count = 640,float warningSeconds = 1.5f);
    Vec nearest(Vec from) const;
    // Swept circle contact returns the first impact, including fast shots.
    float contact(Vec from,Vec to,float radius) const;
    void repel(Vec position,float radius,Vec direction,float strength);
    // Damage only the contacted live surface. Removal waits until the end of
    // combat so the index and other weapon queries stay stable for this tick.
    double damage(Vec position,float radius,double amount);
    bool update(float dt,const Player& player,const GreySnakeSystem& grey,Vec headStart);
private:
    struct Storage;
    std::unique_ptr<Storage> storage;
    std::size_t hazards = 0;
    void rebuildIndex();
};
struct Projectile {
    Vec position, velocity;
    double damage = 1;
    float life = 1, radius = 2, payloadDuration = 1;
    int pierce = 0, bounces = 0;
    Pattern pattern = Pattern::Shot;
    std::uint32_t lastHit = 0;
    std::array<std::uint32_t,32> hitIds{};
    std::uint8_t hitCount = 0;
    bool alive = true, critical = false, evolved = false;
    Vec start, target;
    std::uint32_t targetId = 0;
    float age = 0, flightDuration = 0, turnSpeed = 0, bounceRadius = 40, maxLife = 1;
    double damageRamp = 0;
    int generation = 0, impacts = 0;
    bool cobraSpit = false, gatling = false, critResolved = false;
    double sourceDamage = 0, damageMultiplier = 1;
    Vec previousPosition;
    bool motionReady = false;
};
struct Zone {
    Vec position; float radius = 16, life = 1, maxLife = 1; double damage = 1; float tick = 0; Pattern pattern = Pattern::Nova; bool evolved = false;
    float tickInterval = .3f, damageFactor = .38f, delay = 0, detonationDamage = 0, pullClock = 0, phase = 0, baseRadius = 0;
    bool singleHit = false, canCrit = false, followPlayer = false, gridlockSeed = false;
};
struct NestEgg {
    Vec start, target;
    float age = 0, flightDuration = .75f, hatchDelay = .5f, snakeLife = 5.6f, damage = 8;
    int count = 3;
    bool evolved = false;
    float broodLife = 3.5f, broodDamage = 150;
};
struct MiniSnake {
    Vec position, previous, direction{1,0};
    std::array<Vec,3> segments{}, previousSegments{};
    float moveClock = 0, moveInterval = .08f, life = 5.6f, damage = 8;
    bool evolved = false, alive = true;
    std::array<Vec,2> extraSegments{}, previousExtraSegments{};
    int segmentCount = 3, generation = 0;
    float broodLife = 3.5f, broodDamage = 150;
    int wanderTicks = 0;
    float maxLife = 0;
    std::uint32_t id = 0;
};
enum class SnakeDeathKind { Red, Splitter, Swift, Boss, Hatchling, Hydra };
constexpr std::size_t SnakeDeathCapacity = 2048;
constexpr float snakeDeathDuration(SnakeDeathKind kind) {
    return kind==SnakeDeathKind::Boss?.68f:kind==SnakeDeathKind::Hatchling || kind==SnakeDeathKind::Hydra?.38f:.42f;
}
// Cosmetic history survives actor compaction. Storage is reserved at reset;
// recording and aging these poses never draws from the simulation RNG.
struct SnakeDeath {
    std::array<Vec,9> previous{}, current{};
    std::uint32_t id = 0;
    float age = 0, alpha = 1;
    std::uint8_t count = 0;
    std::uint8_t splitChildren = 0;
    SnakeDeathKind kind = SnakeDeathKind::Red;
    std::uint32_t forceSerial = 0, placementSerial = 0;
};
struct TrackingBeam {
    std::uint32_t target = 0;
    Vec origin, position;
    float life = 2.5f, duration = 2.5f, tick = 0, tickInterval = .2f;double damage = 15;float range = 64;
    bool evolved = false, ancient = false, pendingAbsorb = false;
};
struct CobraPit { Vec position;float age=0,life=6,spawnClock=.2f,cobraLife=5,spitCooldown=1.2f;double damage=100;int spawned=0,maxCobras=6; };
struct Cobra { Vec position;float age=0,life=5,spitClock=0,spitCooldown=1.2f;double damage=100; };
struct Leviathan { Vec position;float age=0,absorbFlash=0;int growth=0;bool active=false;double damage=180;float halfWidth=Tile*.5f; };
struct OrbitFang {float angle=0,orbit=4.5f,speed=4,size=.75f,radiusAmplitude=.3f,radiusFrequency=1,radiusPhase=0,angleAmplitude=.03f,angleFrequency=.8f,anglePhase=0;};
struct Grapple {
    std::uint32_t target = 0;Vec position,direction;float traveled=0,maxDistance=0,pullClock=0,life=8;
    std::array<std::uint32_t,32> hitIds{};std::uint8_t hitCount=0;int phase=0;
    double damage=400;float radius=1.5f*Tile;
};
enum class PickupKind { Experience, Heart, Chest };
struct Pickup { Vec position; int value = 1; PickupKind kind = PickupKind::Experience; Vec previous; bool motionReady = false; };
enum class ChestRewardKind { Upgrade, Evolution, Treasure };
struct ChestReward {
    std::uint64_t serial = 0;
    ChestRewardKind kind = ChestRewardKind::Treasure;
    int definition = -1, rank = 0, gold = 0;
    float healing = 0;
    std::string title, description;
};
struct Particle { Vec position, velocity; float life = 0, maxLife = 1; int color = 0; Vec previous; bool motionReady = false, snakeDeath = false; };
struct DamageNumber { Vec position; int value = 0; float life = .6f; bool critical = false; };
struct Input { Vec direction; };
struct Event { enum Type { Shot, Hit, Kill, Hurt, Pickup, Level, Evolve, Chest, Win } type = Shot; };
struct GravityLock {
    Vec center, lastDirection{1,0};
    float age = 0, duration = 0, radius = 0, cooldown = 0;
    int rank = 0, turns = 0;
    std::uint32_t serial = 0;
    bool empowered = false;
};

class Game {
public:
    explicit Game(const Catalog& catalog, std::uint32_t seed = 0x5EEDu);
    void reset(int character = 0, std::uint32_t seed = 0x5EEDu,const PermanentUpgrades& permanent = {});
    const PermanentUpgrades& permanentUpgrades() const { return permanent; }
    void update(float dt, const Input& input);
    bool choose(int choice);
    bool reroll();
    bool banish(int choice);
    void togglePause();
    void grantXp(int xp);
    void debugPopulate();
    bool spawnFlood(Vec position,Vec direction,int count = 640,float warning = 1.5f);
    Stats stats() const;
    WeaponSpec weaponSpec(const OwnedUpgrade& upgrade) const;
    const EvolutionDef* evolutionFor(int definition) const;
    std::string upgradeName(const OwnedUpgrade& owned) const;
    float progress() const { return elapsed / RunDuration; }

    const Catalog& catalog;
    Phase phase = Phase::Playing;
    Player player;
    GreySnakeSystem greySnakes;
    FloodSystem flood;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<Zone> zones;
    std::vector<NestEgg> nestEggs;
    std::vector<MiniSnake> miniSnakes;
    std::vector<TrackingBeam> beams;
    std::vector<CobraPit> cobraPits;
    std::vector<Cobra> cobras;
    std::vector<Grapple> grapples;
    Leviathan leviathan;
    std::array<Vec,44> orbitFangs{};
    std::array<OrbitFang,44> orbitFangStates{};
    bool ouroborosActive = false;
    double cobraOverflow = 1;
    float orbitPhase = 0, shieldTimer = 0, shieldCooldown = 10;
    int shieldCharges = 0, shieldMax = 0, shieldRank = 0;
    GravityLock graviton;
    std::vector<Pickup> pickups;
    // Small durable reward ledger: presentation can acknowledge rewards without
    // depending on a one-tick event or losing simultaneous boss chest openings.
    std::vector<ChestReward> chestRewards;
    std::vector<Particle> particles;
    std::vector<SnakeDeath> snakeDeaths;
    bool playerDied = false;
    std::vector<DamageNumber> damageNumbers;
    std::vector<OwnedUpgrade> owned;
    std::array<int, 3> offers{{-1,-1,-1}};
    std::vector<int> banished;
    std::vector<Event> events;
    float elapsed = 0, spawnClock = 0, bossClock = 0, shake = 0, noticeTime = 0;
    int kills = 0, gems = 0, chests = 0, rerolls = 3, banishes = 3, character = 0, bossesKilled = 0;
    double damageDealt = 0;
    std::uint64_t experienceDropped = 0, experienceCollected = 0;
    std::string notice;
    std::uint32_t seed = 0;
private:
    std::mt19937 rng;
    std::uint32_t nextEnemyId = 1;
    std::uint32_t nextMiniId = 1;
    std::size_t nextSnakeDeath = 0;
    int directorMinute = 0, pendingSpawns = 0;
    double elapsedTime = 0;
    PermanentUpgrades permanent;
    SnakeDeath* recordSnakeDeath(const Enemy& enemy);
    void recordSnakeDeath(const MiniSnake& mini);
    SnakeDeath* recordSnakeDeath(const SnakeDeath& death);
    float random(float lo, float hi);
    void rollOffers();
    void spawnEnemy(EnemyKind kind, Vec position);
    void updateDirector(float dt);
    void damageEnemy(Enemy& enemy, double damage, bool critical = false);
    void dropExperience(Vec position,int value);
    void fire(OwnedUpgrade& upgrade, const Stats& stats);
    void collectChest();
    void moveEnemies(float dt);
    void directFlood(float dt);
    void updateFlood(float dt,Vec headStart,int armor);
    void placeEnemy(Enemy& enemy, Vec position, bool collapseBody = false);
    void updateGreySnakes(float dt);
    void spawnGreySnake();
    void planGreyIntercept(GreySnake& snake);
    void tickGreySnake(GreySnake& snake);
    void launchNest(const UpgradeDef& definition, const OwnedUpgrade& upgrade, const Stats& stats);
    void updateNests(float dt,const Stats& stats,const WeaponSpec* current);
    void updateWeaponActors(float dt,const Stats& stats,const std::array<WeaponSpec,9>& current,const std::array<bool,9>& equipped);
    void updateShield(float dt);
    void updateGraviton(float dt);
    void triggerGraviton();
    void collectExperience(int amount,double xpMultiplier);
    void absorbKill(Vec position,int growth,float vacuumRadius);
};
}
