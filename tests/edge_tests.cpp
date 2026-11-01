#include "core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < .001f; }

void isolate(snake::Game& game) {
    game.owned.clear();
    game.enemies.clear();
    game.projectiles.clear();
    game.zones.clear();
    game.pickups.clear();
    game.spawnClock = -100;
    game.bossClock = 10;
    game.player.moveClock = -100;
    game.player.invulnerable = 100;
}

snake::Enemy target(std::uint32_t id, snake::Vec position, float hp = 1000) {
    snake::Enemy enemy;
    enemy.id = id;
    enemy.position = position;
    enemy.hp = enemy.maxHp = hp;
    enemy.speed = 0;
    enemy.radius = 3.5f;
    enemy.trail.fill(position);
    return enemy;
}

float hp(const snake::Game& game, std::uint32_t id) {
    const auto found = std::find_if(game.enemies.begin(), game.enemies.end(),
        [&](const snake::Enemy& enemy) { return enemy.id == id; });
    require(found != game.enemies.end(), "Expected surviving target is missing.");
    return found->hp;
}
}

int main() {
    int passed = 0, failed = 0;
    auto test = [&](const char* name, const std::function<void()>& run) {
        try { run(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    snake::Catalog catalog;
    try { catalog = snake::Catalog::load(SNAKE_TEST_DATA_DIR); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }

    test("body contact holds the snake for one second before death, including with shields", [&] {
        for (bool shielded : {false, true}) {
            snake::Game game(catalog, 710);
            isolate(game);
            game.player.segments = {{800,800},{800,808},{808,808},{808,800},{816,800},{816,792},{808,792}};
            game.player.previousSegments = game.player.segments;
            game.player.direction = game.player.queued = {0,-1};
            game.player.moveClock = .119f;
            game.player.invulnerable = 0;
            if (shielded) game.owned = {{catalog.find("serpents_scales"),8,false,0}};
            const auto body = game.player.segments;
            const auto capacity = game.player.segments.capacity(), previousCapacity = game.player.previousSegments.capacity();
            game.update(snake::FixedStep, {{1,0}});
            require(game.phase == snake::Phase::Playing && game.player.hp == 100 && near(game.player.collisionClock, 0), "Self-contact did not begin grace.");
            for (int tick = 0; tick < 59; ++tick) game.update(snake::FixedStep, {});
            require(game.phase == snake::Phase::Playing && game.elapsed > .99f, "Body contact killed the snake before a full second of grace.");
            for (std::size_t i = 0; i < body.size(); ++i)
                require(near(game.player.segments[i].x, body[i].x) && near(game.player.segments[i].y, body[i].y), "Blocked movement changed the body.");
            require(near(game.player.position.x, 800) && near(game.player.position.y, 800), "The head entered the blocked cell.");
            for (int tick = 0; tick < 12 && game.phase == snake::Phase::Playing; ++tick) game.update(snake::FixedStep, {});
            require(game.phase == snake::Phase::Dead && game.player.hp == 0, "Remaining blocked did not end the run after grace.");
            require(!shielded || game.shieldCharges == 3, "Body contact consumed a shield.");
            require(game.player.segments.capacity() == capacity && game.player.previousSegments.capacity() == previousCapacity, "Blocked movement allocated body storage.");
            require(std::count_if(game.events.begin(), game.events.end(), [](const auto& event) { return event.type == snake::Event::Hurt; }) == 1,
                    "Collision death must emit one hurt event.");
            const auto position = game.player.position;
            const float elapsed = game.elapsed;
            game.update(snake::FixedStep, {{0,-1}});
            game.grantXp(100);
            require(game.phase == snake::Phase::Dead && game.elapsed == elapsed && near(game.player.position.x, position.x) && near(game.player.position.y, position.y),
                    "A dead snake moved or resumed progression.");
        }
    });

    test("a safe turn cancels late collision grace and a later collision gets fresh grace", [&] {
        snake::Game game(catalog, 718);
        isolate(game);
        game.player.invulnerable = 0;
        game.player.segments = {{800,800},{800,808},{808,808},{808,800},{816,800},{816,792},{808,792}};
        game.player.previousSegments = game.player.segments;
        game.player.direction = game.player.queued = {0,-1};
        game.player.moveClock = .119f;
        game.update(snake::FixedStep, {{1,0}});
        for (int tick = 0; tick < 55; ++tick) game.update(snake::FixedStep, {});
        require(game.player.collisionClock > .9f, "Fixture did not reach late grace.");
        game.update(snake::FixedStep, {{0,-1}});
        require(game.phase == snake::Phase::Playing && game.player.position.y < 800 && game.player.collisionClock < 0, "Turning away failed to cancel grace.");
        game.player.moveClock = .119f;
        game.update(snake::FixedStep, {{1,0}});
        for (int tick = 0; tick < 8; ++tick) game.update(snake::FixedStep, {{0,1}});
        require(game.phase == snake::Phase::Playing && game.player.collisionClock >= 0 && game.player.collisionClock < .2f, "A second body collision inherited expired grace.");
    });

    test("immune self-contact waits safely and starts grace after immunity expires", [&] {
        snake::Game game(catalog, 719);
        isolate(game);
        game.player.segments = {{800,800},{800,808},{808,808},{808,800},{816,800},{816,792},{808,792}};
        game.player.previousSegments = game.player.segments;
        game.player.direction = game.player.queued = {0,-1};
        game.player.moveClock = .119f;
        for (int tick = 0; tick < 120; ++tick) game.update(snake::FixedStep, {{1,0}});
        require(game.phase == snake::Phase::Playing && game.player.collisionClock < 0 && near(game.player.position.x, 800), "Immunity started grace or let the head enter the body.");
        game.player.invulnerable = 0;
        for (int tick = 0; tick < 8; ++tick) game.update(snake::FixedStep, {});
        require(game.player.collisionClock >= 0 && game.player.collisionClock < .14f, "Grace did not start after immunity expired.");
        for (int tick = 0; tick < 75 && game.phase == snake::Phase::Playing; ++tick) game.update(snake::FixedStep, {});
        require(game.phase == snake::Phase::Dead, "Expired immunity permanently disabled self-collision death.");
    });

    test("collision grace freezes in pause and selection while combat normally continues", [&] {
        snake::Game game(catalog, 720);
        isolate(game);
        game.player.invulnerable = 0;
        game.player.segments = {{800,800},{800,808},{808,808},{808,800},{816,800},{816,792},{808,792}};
        game.player.previousSegments = game.player.segments;
        game.player.direction = game.player.queued = {0,-1};
        game.player.moveClock = .119f;
        snake::Projectile shot;shot.position = {900,800};shot.velocity = {8,0};shot.life = 10;
        game.projectiles.push_back(shot);
        game.update(snake::FixedStep, {{1,0}});
        for (int tick = 0; tick < 30; ++tick) game.update(snake::FixedStep, {});
        require(game.projectiles.front().position.x > 903 && game.projectiles.front().life < 9.5f, "Collision grace froze combat.");
        const float clock = game.player.collisionClock, elapsed = game.elapsed;
        game.togglePause();
        for (int tick = 0; tick < 90; ++tick) game.update(snake::FixedStep, {{0,-1}});
        require(game.player.collisionClock == clock && game.elapsed == elapsed, "Pause consumed collision grace.");
        game.togglePause();game.phase = snake::Phase::LevelUp;
        for (int tick = 0; tick < 90; ++tick) game.update(snake::FixedStep, {});
        require(game.player.collisionClock == clock && game.elapsed == elapsed, "Selection consumed collision grace.");
        game.phase = snake::Phase::Playing;
        for (int tick = 0; tick < 8; ++tick) game.update(snake::FixedStep, {{0,-1}});
        require(game.phase == snake::Phase::Playing && game.player.collisionClock < 0, "Paused collision could not recover.");
        game.reset();
        require(game.player.collisionClock < 0, "A new run retained collision grace.");
    });

    test("a vacating tail cell is safe unless growth retains the tail", [&] {
        for (bool growing : {false, true}) {
            snake::Game game(catalog, 711);
            isolate(game);
            game.player.segments = {{800,800},{800,808},{808,808},{808,800}};
            game.player.previousSegments = game.player.segments;
            game.player.length = growing ? 5 : 4;
            game.player.invulnerable = 0;
            game.player.direction = game.player.queued = {0,-1};
            game.player.moveClock = .119f;
            game.update(snake::FixedStep, {{1,0}});
            require(game.phase == snake::Phase::Playing && (game.player.collisionClock >= 0) == growing, "Collision used the old tail position instead of the retained body.");
            require(near(game.player.position.x, growing ? 800.0f : 808.0f) && near(game.player.position.y, 800), "Tail-cell fixture moved incorrectly.");
        }
    });

    test("fast multi-cell movement stops before the first body collision and retains immunity", [&] {
        auto fast = catalog;
        snake::UpgradeDef haste;
        haste.id = "tail_test_haste";
        haste.modifiers = {{snake::Stat::Speed,1}};
        fast.upgrades.push_back(haste);
        snake::Game game(fast, 712);
        isolate(game);
        game.owned = {{int(fast.upgrades.size()-1),1,false,0}};
        game.player.segments = {{800,800},{800,808},{808,808},{816,808},{816,800},{824,800},{824,792}};
        game.player.previousSegments = game.player.segments;
        game.player.direction = game.player.queued = {0,-1};
        game.player.moveClock = .119f;
        game.player.invulnerable = .65f;
        game.update(.1f, {{1,0}});
        require(game.phase == snake::Phase::Playing && near(game.player.position.x, 808) && game.player.collisionClock < 0, "Fast movement skipped over the body or lost its protection.");
        for (int tick = 0; tick < 120 && game.phase == snake::Phase::Playing; ++tick) game.update(snake::FixedStep, {});
        require(game.phase == snake::Phase::Dead && near(game.player.position.x, 808), "Temporary protection did not expire into normal grace.");
    });

    test("wall grace keeps the body intact and permits a safe turn", [&] {
        snake::Game game(catalog, 713);
        isolate(game);
        game.player.position = game.player.previous = {snake::ArenaSize-snake::Tile,800};
        for (std::size_t i = 0; i < game.player.segments.size(); ++i)
            game.player.segments[i] = {game.player.position.x-float(i*snake::Tile),800};
        game.player.previousSegments = game.player.segments;
        const auto body = game.player.segments;
        game.player.moveClock = 0;
        for (int tick = 0; tick < 45; ++tick) game.update(snake::FixedStep, {{1,0}});
        require(game.phase == snake::Phase::Playing && game.player.collisionClock > .5f && game.player.segments.size() == body.size(), "Wall contact failed to hold the snake during grace.");
        for (std::size_t i = 0; i < body.size(); ++i)
            require(near(game.player.segments[i].x, body[i].x) && near(game.player.segments[i].y, body[i].y), "Wall contact collapsed the tail into the head.");
        for (int tick = 0; tick < 8; ++tick) game.update(snake::FixedStep, {{0,-1}});
        require(game.phase == snake::Phase::Playing && game.player.position.y < 800 && game.player.collisionClock < 0, "The snake could not turn away from an arena edge.");
    });

    test("every arena wall is lethal after grace even while immune", [&] {
        const snake::Vec directions[]{{1,0},{0,1},{-1,0},{0,-1}};
        const snake::Vec positions[]{{snake::ArenaSize-snake::Tile,800},{800,snake::ArenaSize-snake::Tile},{snake::Tile,800},{800,snake::Tile}};
        for (int edge = 0; edge < 4; ++edge) {
            snake::Game game(catalog, 721);
            isolate(game);
            game.player.position = game.player.previous = positions[edge];
            game.player.direction = game.player.queued = directions[edge];
            game.player.invulnerable = edge ? 100.0f : 0.0f;
            for (std::size_t i = 0; i < game.player.segments.size(); ++i)
                game.player.segments[i] = {positions[edge].x-directions[edge].x*float(i*snake::Tile),positions[edge].y-directions[edge].y*float(i*snake::Tile)};
            game.player.previousSegments = game.player.segments;game.player.moveClock = .119f;
            if (edge == 3) game.owned = {{catalog.find("serpents_scales"),8,false,0}};
            game.update(snake::FixedStep, {});
            for (int tick = 0; tick < 59; ++tick) game.update(snake::FixedStep, {});
            require(game.phase == snake::Phase::Playing, "Wall grace ended before one second.");
            for (int tick = 0; tick < 12 && game.phase == snake::Phase::Playing; ++tick) game.update(snake::FixedStep, {});
            require(game.phase == snake::Phase::Dead && game.notice == "YOU HIT THE WALL", "Wall collision never expired or used the wrong death reason.");
            require(near(game.player.position.x, positions[edge].x) && near(game.player.position.y, positions[edge].y), "A wall collision left the arena.");
        }
    });

    test("XP pickups grow the snake from raw value without Ravenous bonus length", [&] {
        for (int mawRank : {0, 1, 8}) {
            snake::Game game(catalog, 714);
            isolate(game);
            if (mawRank) game.owned = {{catalog.find("ravenous_maw"),mawRank,false,0}};
            game.pickups.push_back({game.player.position,3,snake::PickupKind::Experience});
            game.update(snake::FixedStep, {});
            require(game.player.length == 10 && game.gems == 3, "XP fruit failed to grow by its raw value.");
            require(game.player.xp + (game.player.level > 1 ? 5 : 0) == int(std::floor(3*(1+.25f*mawRank))), "Ravenous Maw lost its whole XP bonus.");
            require(near(float(game.player.xpRemainder),3*(1+.25f*mawRank)-std::floor(3*(1+.25f*mawRank))), "Fractional XP was not retained.");
            require((game.stats().damage > 1.15) == bool(mawRank), "Ravenous Maw lost its direct damage bonus.");
            if (game.phase == snake::Phase::LevelUp) require(game.choose(0), "Ravenous XP level-up could not be selected.");
            require(game.player.length == 10, "Choosing a Ravenous XP level-up added length.");
            game.player.nextXp = 1000000;
            game.pickups.push_back({game.player.position,1,snake::PickupKind::Experience});
            game.update(snake::FixedStep, {});
            require(game.player.length == 11, "A later XP pickup did not keep growing the snake.");
            game.pickups.push_back({game.player.position,20,snake::PickupKind::Heart});
            game.pickups.push_back({game.player.position,1,snake::PickupKind::Chest});
            game.update(snake::FixedStep, {});
            require(game.player.length == 13, "Heart and chest pickups failed to grow the snake once each.");
        }
    });

    test("level-ups and queued level choices do not add snake length", [&] {
        snake::Game game(catalog, 715);
        isolate(game);
        game.grantXp(150);
        for (int choice = 0; choice < 20 && game.phase == snake::Phase::LevelUp; ++choice)
            require(game.choose(0), "Queued level-up could not be selected.");
        require(game.player.level > 3 && game.phase == snake::Phase::Playing, "Fixture did not consume queued level-ups.");
        require(game.player.length == 7, "Level progression added extra snake length.");
    });

    test("large XP pickups retain bounded growth storage and unfold along the path", [&] {
        snake::Game game(catalog, 716);
        isolate(game);
        game.player.nextXp = 1000000;
        const auto capacity = game.player.segments.capacity(), previousCapacity = game.player.previousSegments.capacity();
        game.pickups.push_back({game.player.position,1000,snake::PickupKind::Experience});
        game.update(snake::FixedStep, {});
        require(game.player.length == snake::MaxPlayerLength && game.player.xp == 1000 && game.gems == 1000, "Large XP collection lost growth or experience.");
        game.pickups.push_back({game.player.position,3,snake::PickupKind::Experience});
        game.update(snake::FixedStep, {});
        require(game.player.length == snake::MaxPlayerLength && game.player.xp == 1003 && game.gems == 1003, "XP at maximum length was lost or exceeded capacity.");
        game.player.moveClock = 0;
        for (int tick = 0; tick < 1000; ++tick)
            game.update(snake::FixedStep, {{tick < 500 ? 1.0f : 0.0f, tick < 500 ? 0.0f : 1.0f}});
        require(game.phase == snake::Phase::Playing && game.player.segments.size() == snake::MaxPlayerLength, "Pending growth did not unfold along the movement path.");
        require(game.player.segments.capacity() == capacity && game.player.previousSegments.capacity() == previousCapacity, "Growth allocated body storage during play.");
        for (std::size_t i = 1; i < game.player.segments.size(); ++i) {
            const auto a = game.player.segments[i-1], b = game.player.segments[i];
            require(near(std::abs(a.x-b.x)+std::abs(a.y-b.y), float(snake::Tile)), "Growth left a gap or duplicate cell in the historical path.");
        }
    });

    test("boss contact cannot destroy the tail and head contact still deals damage", [&] {
        snake::Game game(catalog, 717);
        isolate(game);
        game.player.invulnerable = 0;
        auto boss = target(10000, game.player.segments.back());
        boss.kind = snake::EnemyKind::Boss;
        boss.radius = 7;
        game.enemies.push_back(boss);
        const auto body = game.player.segments;
        for (int tick = 0; tick < 120; ++tick) game.update(snake::FixedStep, {});
        require(game.player.length == 7 && game.player.segments.size() == body.size() && near(game.player.hp, 100), "Boss tail contact removed length or damaged the player.");
        for (std::size_t i = 0; i < body.size(); ++i)
            require(near(game.player.segments[i].x, body[i].x) && near(game.player.segments[i].y, body[i].y), "Boss contact changed a tail segment.");
        game.enemies.front().position = game.player.position;
        game.enemies.front().motionReady = false;
        game.update(snake::FixedStep, {});
        require(game.player.hp < 100 && game.player.length == 7 && game.player.segments.size() == body.size(), "Boss head contact failed to damage health while preserving length.");
    });

    test("all boss chest rewards and XP survive a saturated pickup pool", [&] {
        snake::Game game(catalog, 701);
        isolate(game);
        for (int i = 0; i < 1400; ++i)
            game.pickups.push_back({{8,8}, 1, snake::PickupKind::Experience});
        for (int i = 0; i < 10; ++i) {
            const snake::Vec position{game.player.position.x + 40 + float(i * 16), game.player.position.y};
            auto boss = target(std::uint32_t(10000 + i), position, 1);
            boss.kind = snake::EnemyKind::Boss;
            game.enemies.push_back(boss);
            snake::Projectile shot;
            shot.position = position;
            shot.damage = 10;
            shot.life = 1;
            game.projectiles.push_back(shot);
        }
        game.update(snake::FixedStep, {});
        require(game.bossesKilled == 10, "Fixture did not defeat all ten bosses.");
        int xp = 0, chests = 0;
        for (const auto& pickup : game.pickups) {
            if (pickup.kind == snake::PickupKind::Experience) xp += pickup.value;
            if (pickup.kind == snake::PickupKind::Chest) chests += pickup.value;
        }
        require(xp == 1700, "Merging boss rewards destroyed or duplicated XP.");
        require(chests == 10, "Pickup saturation silently discarded boss chests.");
        require(game.pickups.size() <= 1403, "Reward preservation exceeded the bounded pickup budget.");
        for (int attempt = 0; attempt < 10 && game.chests < 10; ++attempt) {
            const auto next = std::find_if(game.pickups.begin(), game.pickups.end(),
                [](const snake::Pickup& pickup) { return pickup.kind == snake::PickupKind::Chest; });
            require(next != game.pickups.end(), "A preserved chest disappeared before collection.");
            game.player.position = game.player.previous = next->position;
            game.player.segments.assign(std::size_t(game.player.length), game.player.position);
            game.player.previousSegments = game.player.segments;
            game.update(snake::FixedStep, {});
        }
        require(game.chests == 10, "Preserved chest quantities were not all awarded on collection.");
    });

    test("expired ordinary zones do not deal a final extra tick or fire", [&] {
        for (const auto pattern : {snake::Pattern::Nova, snake::Pattern::Mortar, snake::Pattern::Pit}) {
            snake::Game game(catalog, 702);
            isolate(game);
            const snake::Vec position{game.player.position.x + 40, game.player.position.y};
            game.enemies.push_back(target(10000, position));
            game.zones.push_back({position, 10, .001f, .1f, 20, 0, pattern, false});
            game.update(snake::FixedStep, {});
            require(near(hp(game, 10000), 1000), "Expired zone damaged a target.");
            require(game.projectiles.empty(), "Expired cobra pit emitted a projectile.");
            require(game.zones.empty(), "Expired zone was not removed.");
        }
    });

    test("fast projectiles hit targets crossed between fixed ticks", [&] {
        snake::Game game(catalog, 704);
        isolate(game);
        const snake::Vec origin = game.player.position;
        game.enemies.push_back(target(10000, {origin.x + 8, origin.y}));
        game.enemies.push_back(target(10001, {origin.x + 8, origin.y + 16}));
        snake::Projectile shot;
        shot.position = origin;
        shot.velocity = {1920, 0};
        shot.damage = 25;
        shot.life = 1;
        game.projectiles.push_back(shot);
        game.update(snake::FixedStep, {});
        require(near(hp(game, 10000), 975), "Projectile tunneled through its target.");
        require(near(hp(game, 10001), 1000), "Swept hit damaged a target outside its path.");
    });

    test("swept piercing resolves nearest targets before farther targets", [&] {
        snake::Game game(catalog, 705);
        isolate(game);
        const snake::Vec origin = game.player.position;
        // Store targets in an order unrelated to their distance along the shot.
        game.enemies.push_back(target(10002, {origin.x + 24, origin.y}));
        game.enemies.push_back(target(10000, {origin.x + 8, origin.y}));
        game.enemies.push_back(target(10001, {origin.x + 16, origin.y}));
        snake::Projectile shot;
        shot.position = origin;
        shot.velocity = {2400, 0};
        shot.damage = 25;
        shot.pierce = 1;
        shot.life = 1;
        game.projectiles.push_back(shot);
        game.update(snake::FixedStep, {});
        require(near(hp(game, 10000), 975) && near(hp(game, 10001), 975), "Piercing failed to hit the first two targets in path order.");
        require(near(hp(game, 10002), 1000), "Piercing hit a farther target before exhausting nearer targets.");
    });

    test("projectiles resolve the live part of their final lifetime tick", [&] {
        snake::Game game(catalog, 706);
        isolate(game);
        const snake::Vec origin = game.player.position;
        game.enemies.push_back(target(10000, {origin.x + 8, origin.y}));
        snake::Projectile shot;
        shot.position = origin;
        shot.velocity = {960, 0};
        shot.damage = 25;
        shot.life = .01f;
        game.projectiles.push_back(shot);
        game.update(snake::FixedStep, {});
        require(near(hp(game, 10000), 975), "A hit during the projectile's remaining lifetime was lost.");
        require(game.projectiles.empty(), "Expired projectile survived the final lifetime tick.");
    });

    test("Shatter Fang remains evolved after most ricochets are spent", [&] {
        for (const bool evolved : {false, true}) {
            snake::Game game(catalog, 707);
            isolate(game);
            const snake::Vec position{game.player.position.x + 40, game.player.position.y};
            game.enemies.push_back(target(10000, position));
            game.enemies.push_back(target(10001, {position.x + 32, position.y}));
            snake::Projectile fang;
            fang.position = position;
            fang.velocity = {20,0};
            fang.damage = 20;
            fang.life = 1;
            fang.pattern = snake::Pattern::Ricochet;
            fang.bounces = 2;
            fang.critical = true;
            fang.evolved = evolved;
            game.projectiles.push_back(fang);
            game.update(snake::FixedStep, {});
            require(hp(game, 10000) < 1000, "Critical ricochet fixture did not impact its target.");
            const auto shards = std::count_if(game.projectiles.begin(), game.projectiles.end(),
                [](const snake::Projectile& projectile) { return projectile.pattern == snake::Pattern::Ricochet && projectile.generation==1 && projectile.targetId!=0; });
            require(shards == (evolved ? 1 : 0), "Evolution identity was lost when ricochet count decreased.");
        }
    });

    test("Chronofield extends beam lifetime and lash slowing without stretching its animation", [&] {
        for (const char* id : {"sidewinder_beam", "tongue_lash"}) {
            snake::Game game(catalog, 708);
            isolate(game);
            const int weapon = catalog.find(id), chronofield = catalog.find("chronofield");
            require(weapon >= 0 && chronofield >= 0, "Duration fixture is missing a content definition.");
            game.owned = {{weapon,1,false,0},{chronofield,2,false,0}};
            const auto pattern = catalog.upgrades[std::size_t(weapon)].pattern;
            game.enemies.push_back(target(10000, {game.player.position.x + 24, game.player.position.y}));
            game.update(snake::FixedStep, {});
            if(pattern==snake::Pattern::Beam){
                require(!game.beams.empty(),"Sidewinder did not create a tracking beam.");
                require(near(game.beams.front().duration,2.5f*game.stats().duration),"Tracking beam ignored Chronofield.");
            }else{
                const auto effect=std::find_if(game.zones.begin(),game.zones.end(),[&](const auto& zone){return zone.pattern==pattern;});
                require(effect!=game.zones.end() && near(effect->maxLife,.35f),"Lash animation must retain its source 0.35s duration.");
                require(near(game.enemies.front().slow,1.5f*game.stats().duration),"Lash slow ignored Chronofield.");
            }
        }
    });

    test("Ouroboros keeps one following ring without nearby targets", [&] {
        snake::Game game(catalog, 709);
        isolate(game);
        const int weapon = catalog.find("venom_shot");
        require(weapon >= 0, "Ouroboros fixture is missing Venom Shot.");
        game.owned = {{weapon,catalog.upgrades[std::size_t(weapon)].maxRank,true,0}};
        game.player.moveClock = 0;
        const float startY = game.player.position.y;
        for (int frame = 0; frame < 240; ++frame) {
            game.update(snake::FixedStep, {{0,1}});
            const auto rings = std::count_if(game.zones.begin(), game.zones.end(),
                [](const snake::Zone& zone) { return zone.pattern == snake::Pattern::Shot && zone.evolved; });
            require(rings == 1, "Passive ring disappeared or stacked while no target was available.");
            const auto ring = std::find_if(game.zones.begin(), game.zones.end(),
                [](const snake::Zone& zone) { return zone.pattern == snake::Pattern::Shot && zone.evolved; });
            require(near(ring->position.x, game.player.position.x) && near(ring->position.y, game.player.position.y), "Passive ring stopped following the player.");
            require(game.projectiles.empty(), "Targetless ring maintenance fired an unaimed venom shot.");
        }
        require(game.player.position.y > startY, "Ring fixture never exercised movement.");
    });

    std::cout << passed << " passed, " << failed << " failed.\n";
    return failed ? 1 : 0;
}
