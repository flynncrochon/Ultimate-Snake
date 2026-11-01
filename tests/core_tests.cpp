#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(float a, float b, float epsilon = .001f) { return std::abs(a - b) <= epsilon; }
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path);
    if (!(file << text)) throw std::runtime_error("Unable to write test fixture.");
}
struct CatalogFixture {
    std::filesystem::path path;
    CatalogFixture() {
        path = std::filesystem::current_path() / "catalog-test-fixture";
        std::filesystem::create_directories(path);
        for (const char* file : {"upgrades.csv", "evolutions.csv"})
            write(path / file, read(std::filesystem::path(SNAKE_TEST_DATA_DIR) / file));
    }
    ~CatalogFixture() {
        std::error_code ignored;
        std::filesystem::remove(path / "upgrades.csv", ignored);
        std::filesystem::remove(path / "evolutions.csv", ignored);
        std::filesystem::remove(path, ignored);
    }
};
void expectCatalogError(const std::string& contents, const std::string& file = "upgrades.csv") {
    CatalogFixture fixture;
    write(fixture.path / file, contents);
    bool rejected = false;
    try { (void)snake::Catalog::load(fixture.path.string()); }
    catch (const std::runtime_error& error) { rejected = std::string(error.what()).size() > 5; }
    require(rejected, "Invalid catalog must fail with an actionable runtime error.");
}
void settle(snake::Game& game) {
    for (int limit = 0; game.phase == snake::Phase::LevelUp && limit < 100; ++limit) {
        bool chosen = false;
        for (int i = 0; i < 3; ++i) if (game.choose(i)) { chosen = true; break; }
        require(chosen, "A level-up with eligible offers must accept a choice.");
    }
}
}

int main() {
    int passed = 0, failed = 0;
    auto test = [&](const char* name, const std::function<void()>& body) {
        try { body(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    snake::Catalog catalog;
    try { catalog = snake::Catalog::load(SNAKE_TEST_DATA_DIR); }
    catch (const std::exception& error) { std::cerr << "Catalog failed to load: " << error.what() << '\n'; return 1; }

    test("catalog contains the complete original weapon and evolution families", [&] {
        require(catalog.upgrades.size() >= 18, "Expected at least the original nine weapons and nine passives.");
        require(catalog.evolutions.size() >= 9, "Expected at least the original nine evolution recipes.");
        std::set<std::string> ids;
        for (const auto& definition : catalog.upgrades) {
            require(ids.insert(definition.id).second, "Duplicate upgrade ID.");
            require(definition.maxRank > 0 && definition.weight > 0, "Invalid upgrade progression.");
        }
        for (const auto& recipe : catalog.evolutions) {
            const int weapon = catalog.find(recipe.weapon), passive = catalog.find(recipe.passive);
            require(weapon >= 0 && passive >= 0, "Evolution has a missing prerequisite.");
            require(catalog.upgrades[weapon].kind == snake::UpgradeKind::Weapon, "Evolution weapon prerequisite has wrong category.");
            require(catalog.upgrades[passive].kind == snake::UpgradeKind::Passive, "Evolution passive prerequisite has wrong category.");
        }
    });

    test("invalid CSV is rejected before a run starts", [&] {
        expectCatalogError("not,a,valid,header\n");
        const auto upgrades = read(std::filesystem::path(SNAKE_TEST_DATA_DIR) / "upgrades.csv");
        const auto first = upgrades.find('\n'), second = upgrades.find('\n', first + 1);
        require(first != std::string::npos && second != std::string::npos, "Fixture has no first row.");
        expectCatalogError(upgrades + "\n" + upgrades.substr(first + 1, second - first - 1) + "\n");
        expectCatalogError("weapon,passive,name,description,damage,cooldown,area\nmissing,graviton,Bad,Bad recipe,2,1,1\n", "evolutions.csv");
    });

    test("pause freezes all simulation clocks and movement", [&] {
        snake::Game game(catalog, 7);
        const auto before = game.player.position;
        game.togglePause();
        for (int i = 0; i < 120; ++i) game.update(snake::FixedStep, {{0,1}});
        require(game.phase == snake::Phase::Paused, "Pause should remain active.");
        require(game.elapsed == 0 && game.player.position.x == before.x && game.player.position.y == before.y, "Paused simulation advanced.");
        game.togglePause();
        game.update(snake::FixedStep, {});
        require(game.elapsed > 0, "Resume should advance simulation.");
    });

    test("snake rejects reversal and remains on the eight pixel tile grid", [&] {
        snake::Game game(catalog, 19);
        const float start = game.player.position.x;
        for (int i = 0; i < 16; ++i) game.update(snake::FixedStep, {{-1,0}});
        require(game.player.position.x > start, "Snake reversed into its own neck.");
        require(near(std::fmod(game.player.position.x - start, float(snake::Tile)), 0), "Snake moved off its tile grid.");
        game.update(snake::FixedStep, {{0,1}});
        for (int i = 0; i < 10; ++i) game.update(snake::FixedStep, {{0,1}});
        require(game.player.direction.y == 1, "A perpendicular turn was ignored.");
    });

    test("level-up offers respect slot caps and maxed upgrades", [&] {
        snake::Game game(catalog, 23);
        game.owned.clear();
        int weapons = 0, passives = 0;
        for (int i = 0; i < int(catalog.upgrades.size()); ++i) {
            const auto& definition = catalog.upgrades[i];
            int& count = definition.kind == snake::UpgradeKind::Weapon ? weapons : passives;
            const int cap = definition.kind == snake::UpgradeKind::Weapon ? snake::WeaponSlots : snake::PassiveSlots;
            if (count < cap) { game.owned.push_back({i, 1, false, 0}); ++count; }
        }
        game.grantXp(game.player.nextXp);
        require(game.phase == snake::Phase::LevelUp, "XP did not open a level-up.");
        std::set<int> seen;
        for (int definition : game.offers) {
            if (definition < 0) continue;
            require(seen.insert(definition).second, "Duplicate offer.");
            require(std::any_of(game.owned.begin(), game.owned.end(), [&](const auto& item) { return item.definition == definition; }), "A new upgrade exceeded a filled slot category.");
        }
        require(!seen.empty(), "Upgradeable owned items produced no choices.");
        require(!game.choose(99), "Invalid card index was accepted.");
        for (auto& item : game.owned) item.rank = catalog.upgrades[item.definition].maxRank;
        game.phase = snake::Phase::Playing;
        game.offers.fill(-1);
        game.grantXp(1000);
        for (int definition : game.offers) {
            if (game.phase != snake::Phase::LevelUp || definition < 0) continue;
            const auto found = std::find_if(game.owned.begin(), game.owned.end(), [&](const auto& item) { return item.definition == definition; });
            require(found == game.owned.end() || found->rank < catalog.upgrades[definition].maxRank, "Offered a maxed upgrade.");
        }
    });

    test("derived bonuses compose and cooldown cannot become nonpositive", [&] {
        snake::Game game(catalog, 2);
        game.owned.clear();
        const auto baseline = game.stats();
        const int rapid = catalog.find("rapid_fire"), graviton = catalog.find("graviton"), volley = catalog.find("coiled_volley");
        require(rapid >= 0 && graviton >= 0 && volley >= 0, "Missing stat passives.");
        game.owned = {{rapid, catalog.upgrades[rapid].maxRank, false, 0}, {graviton, 2, false, 0}, {volley, 2, false, 0}};
        const auto enhanced = game.stats();
        require(enhanced.cooldown > 0 && enhanced.cooldown < baseline.cooldown, "Rapid Fire does not reduce cooldown safely.");
        require(enhanced.armor == baseline.armor, "Gridlock unexpectedly granted contact armor.");
        require(near(enhanced.pickup, float(snake::Tile)) && near(enhanced.pickup, baseline.pickup), "Gridlock bypassed the one-tile pickup limit.");
        require(enhanced.amount > baseline.amount, "Coiled Volley does not add projectiles.");
        require(near(enhanced.damage, baseline.damage), "Unrelated bonus changed damage.");
    });

    test("a chest evolves an eligible weapon in its existing slot", [&] {
        snake::Game game(catalog, 4);
        const auto& recipe = catalog.evolutions.front();
        const int weapon = catalog.find(recipe.weapon), passive = catalog.find(recipe.passive);
        game.owned = {{weapon, catalog.upgrades[weapon].maxRank, false, 0}, {passive, 1, false, 0}};
        game.pickups.clear();
        game.pickups.push_back({game.player.position, 1, snake::PickupKind::Chest});
        game.update(snake::FixedStep, {});
        require(game.owned.size() == 2, "Evolution created an extra equipment slot.");
        require(game.owned.front().evolved, "Chest did not evolve an eligible weapon.");
        require(game.upgradeName(game.owned.front()) == recipe.name, "Evolved display name does not match its recipe.");
    });

    test("spatial collision damages enemies without hitting a distant neighbor", [&] {
        snake::Game game(catalog, 101);
        game.owned.clear(); game.enemies.clear(); game.projectiles.clear();
        snake::Enemy target;
        target.id = 10001; target.position = {game.player.position.x + 40, game.player.position.y};
        target.hp = target.maxHp = 1000; target.speed = 0; target.radius = 3;
        target.trail.fill(target.position);
        snake::Enemy distant = target;
        distant.id = 10002; distant.position.x += 120; distant.trail.fill(distant.position);
        game.enemies = {target, distant};
        snake::Projectile shot;
        shot.position = target.position; shot.velocity = {0,0}; shot.damage = 25; shot.life = 1;
        game.projectiles.push_back(shot);
        game.update(snake::FixedStep, {});
        const auto hit = std::find_if(game.enemies.begin(), game.enemies.end(), [](const auto& e) { return e.id == 10001; });
        const auto far = std::find_if(game.enemies.begin(), game.enemies.end(), [](const auto& e) { return e.id == 10002; });
        require(hit != game.enemies.end() && hit->hp < 1000, "Overlapping projectile missed its target.");
        require(far != game.enemies.end() && far->hp == 1000, "Projectile hit an unrelated spatial cell.");
    });

    test("same seed and inputs reproduce simulation state", [&] {
        snake::Game a(catalog, 0xCAFE), b(catalog, 0xCAFE);
        a.greySnakes.enabled=b.greySnakes.enabled=false;
        a.player.invulnerable = b.player.invulnerable = 10000;
        constexpr snake::Vec directions[]{{1,0},{0,1},{-1,0},{0,-1}};
        for (int frame = 0; frame < 1800; ++frame) {
            const snake::Input input{directions[(frame / 240) % 4]};
            a.update(snake::FixedStep, input); b.update(snake::FixedStep, input);
            settle(a); settle(b);
            require(a.phase == b.phase && a.kills == b.kills && a.enemies.size() == b.enemies.size() && a.projectiles.size() == b.projectiles.size(), "Seeded runs diverged.");
            require(near(a.player.position.x, b.player.position.x) && near(a.player.position.y, b.player.position.y), "Seeded player movement diverged.");
            require(std::isfinite(a.player.position.x) && std::isfinite(a.player.position.y), "Simulation produced a non-finite coordinate.");
            for (std::size_t i = 0; i < a.enemies.size(); ++i)
                require(near(a.enemies[i].position.x, b.enemies[i].position.x) && near(a.enemies[i].position.y, b.enemies[i].position.y) && near(a.enemies[i].hp, b.enemies[i].hp), "Seeded enemy state diverged.");
        }
        require(a.phase != snake::Phase::Dead && a.elapsed > 29.9f, "Determinism scenario ended early: elapsed="+std::to_string(a.elapsed)+", phase="+std::to_string(int(a.phase))+", notice="+a.notice);
    });

    test("every base and evolved weapon delivers damage in a dense encounter", [&] {
        for (int definition = 0; definition < int(catalog.upgrades.size()); ++definition) {
            if (catalog.upgrades[definition].kind != snake::UpgradeKind::Weapon) continue;
            for (bool evolved : {false, true}) {
                snake::Game game(catalog, 303);
                game.owned = {{definition, catalog.upgrades[definition].maxRank, evolved, 0}};
                game.enemies.clear(); game.pickups.clear();
                game.player.moveClock = -100; game.player.invulnerable = 100;
                for (int i = 0; i < 12; ++i) {
                    snake::Enemy enemy;
                    enemy.id = std::uint32_t(10000 + i);
                    enemy.position = {game.player.position.x + 16 + float(i % 4) * 3, game.player.position.y + float(i / 4 - 1) * 3};
                    enemy.hp = enemy.maxHp = 10000; enemy.speed = 0; enemy.trail.fill(enemy.position);
                    game.enemies.push_back(enemy);
                }
                for (int frame = 0; frame < 180; ++frame) game.update(snake::FixedStep, {});
                float damage = 0;
                for (const auto& enemy : game.enemies) {
                    require(std::isfinite(enemy.position.x) && std::isfinite(enemy.position.y) && std::isfinite(enemy.hp), "Weapon encounter produced non-finite state.");
                    if (enemy.id >= 10000) damage += 10000 - enemy.hp;
                }
                require(damage > 0, catalog.upgrades[definition].id + (evolved ? " evolved" : " base") + " dealt no damage.");
                require(game.projectiles.size() <= 1600 && game.zones.size() <= 128, "Combat exceeded entity budgets.");
            }
        }
    });

    test("mortar impact preserves its configured pool duration", [&] {
        auto content = catalog;
        const int mortarDefinition = content.find("plague_mortar"), durationDefinition = content.find("chronofield");
        content.upgrades[mortarDefinition].duration = 4.25f;
        snake::Game game(content, 55);
        game.reset(3, 55);
        game.owned.clear(); game.enemies.clear(); game.pickups.clear();
        game.player.moveClock = -100; game.player.invulnerable = 100;
        game.owned = {{mortarDefinition,1,false,0},{durationDefinition,2,false,0}};
        const float expectedDuration = 4.25f * game.stats().duration;
        snake::Enemy target;
        target.id = 10000; target.position = {game.player.position.x + 30, game.player.position.y};
        target.hp = target.maxHp = 10000; target.speed = 0; target.trail.fill(target.position);
        game.enemies.push_back(target);
        game.update(snake::FixedStep, {});
        require(!game.projectiles.empty() && near(game.projectiles.front().payloadDuration, expectedDuration), "Mortar ignored the CSV duration or global duration bonuses.");
        game.owned.clear();
        for (int frame = 0; frame < 60; ++frame) game.update(snake::FixedStep, {});
        require(!game.zones.empty() && near(game.zones.front().maxLife, expectedDuration), "Mortar impact failed to retain the cast-time duration.");
    });

    test("pickup saturation never mixes XP into a healing pickup", [&] {
        snake::Game game(catalog, 56);
        game.owned.clear(); game.enemies.clear(); game.pickups.clear();
        for (int i = 0; i < 1400; ++i) game.pickups.push_back({{10,10},20,snake::PickupKind::Heart});
        snake::Enemy enemy;
        enemy.id = 10000; enemy.position = {game.player.position.x + 40, game.player.position.y};
        enemy.hp = enemy.maxHp = 1; enemy.speed = 0; enemy.trail.fill(enemy.position);
        game.enemies.push_back(enemy);
        snake::Projectile shot;
        shot.position = enemy.position; shot.damage = 10; shot.life = 1;
        game.projectiles.push_back(shot);
        game.update(snake::FixedStep, {});
        require(game.kills == 1, "Saturation fixture failed to defeat its enemy.");
        for (const auto& pickup : game.pickups)
            if (pickup.kind == snake::PickupKind::Heart) require(pickup.value == 20, "XP was incorrectly merged into a heart pickup.");
    });

    test("scheduled bosses still spawn when ordinary enemies fill capacity", [&] {
        snake::Game game(catalog, 89);
        game.owned.clear(); game.enemies.clear(); game.pickups.clear();
        game.elapsed = 59.99f;
        for (int i = 0; i < 1800; ++i) {
            snake::Enemy enemy;
            enemy.id = std::uint32_t(10000 + i); enemy.position = {game.player.position.x + 200, game.player.position.y};
            enemy.speed = 0; enemy.trail.fill(enemy.position);
            game.enemies.push_back(enemy);
        }
        game.update(snake::FixedStep, {});
        require(game.enemies.size() == 1800, "Boss scheduling exceeded the entity cap.");
        require(game.bossClock == 1 && std::any_of(game.enemies.begin(), game.enemies.end(), [](const auto& enemy) { return enemy.kind == snake::EnemyKind::Boss; }), "A full ordinary population silently suppressed a scheduled boss.");
    });

    test("only evolved mortar pools detonate once on expiration", [&] {
        for (bool evolved : {false, true}) {
            snake::Game game(catalog, 90);
            game.owned.clear(); game.enemies.clear(); game.pickups.clear();
            snake::Enemy enemy;
            enemy.id = 10000; enemy.position = {game.player.position.x + 40, game.player.position.y};
            enemy.hp = enemy.maxHp = 1000; enemy.speed = 0; enemy.trail.fill(enemy.position);
            game.enemies.push_back(enemy);
            game.zones.push_back({enemy.position,10,.001f,.1f,20,1,snake::Pattern::Mortar,evolved});
            game.update(snake::FixedStep, {});
            require(near(game.enemies.front().hp, evolved ? 950.0f : 1000.0f), "Mortar expiration dealt the wrong detonation damage.");
            require(game.zones.empty(), "Expired pool was retained.");
            game.update(snake::FixedStep, {});
            require(near(game.enemies.front().hp, evolved ? 950.0f : 1000.0f), "Mortar detonation repeated after expiration.");
        }
    });

    test("speed changes preserve progress through the current cell", [&] {
        auto prepare=[](snake::Game& game){game.owned.clear();game.enemies.clear();game.pickups.clear();game.spawnClock=-1000;game.bossClock=1000;};
        auto faster=catalog;snake::UpgradeDef speed;speed.id="test_stride";speed.kind=snake::UpgradeKind::Passive;speed.modifiers={{snake::Stat::Speed,1}};faster.upgrades.push_back(speed);
        snake::Game boosted(faster,19);prepare(boosted);boosted.owned.push_back({int(faster.upgrades.size()-1),1,false,0});boosted.player.moveClock=.09f;
        const auto start=boosted.player.position;boosted.update(snake::FixedStep,{});
        require(near(boosted.player.position.x,start.x+snake::Tile),"A speed increase skipped more than one cell.");
        require(near(boosted.player.moveClock/boosted.player.moveInterval,1.0f/36),"A speed increase lost its fractional movement progress.");
        boosted.owned.clear();boosted.player.moveClock=.045f;boosted.update(snake::FixedStep,{});
        require(near(boosted.player.moveClock/boosted.player.moveInterval,.75f+snake::FixedStep/.12f),"A speed decrease moved the animation backwards.");
    });
    std::cout << passed << " passed, " << failed << " failed.\n";
    return failed ? 1 : 0;
}
