// Offline balance experiment. Links the shipping snake_core without modifying it.
// Damage is an exact HP ledger, including children spawned after combat queries.
#include "core.hpp"
#include "grid.hpp"
#include "../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Json = nlohmann::json;
using namespace snake;
constexpr std::uint32_t ExternalId = 1000000000u;
constexpr int EnemyCap = 1800;

void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

struct Weapon { std::string id; int rank; bool evolved; };
struct Build {
    std::string id, label, category;
    std::vector<Weapon> weapons;
    std::map<std::string, int> passives;
};
struct Scenario {
    std::string id, name;
    float elapsed;
    int level, seconds;
    bool moving;
    int bossDistance = 0;
};
struct Spawn { int tick; Enemy enemy; Vec offset; };
struct Workload {
    Scenario scenario;
    std::uint32_t seed;
    std::vector<Spawn> spawns;
    Json summary;
};

Input control(int tick, bool moving) {
    static constexpr std::array<Vec, 4> directions{{{1,0},{0,1},{-1,0},{0,-1}}};
    return {moving ? directions[std::size_t((tick / 150) % 4)] : Vec{}};
}

void initialize(Game& game, const Scenario& scenario) {
    // Reset's starting enemies/fruit/starter are deliberately excluded. Each
    // scenario starts empty and receives the normal director's later waves.
    game.enemies.clear(); game.pickups.clear(); game.owned.clear();
    game.player.level = scenario.level;
    game.player.xp = 0; game.player.nextXp = 100000000;
    game.player.hp = game.player.maxHp = 100000000;
    game.player.invulnerable = 0;
    game.elapsed = scenario.elapsed;
    game.spawnClock = 0; game.bossClock = 1000000;
}

void addRecipe(const Catalog& catalog, Build& build, const Weapon& weapon) {
    if (!weapon.evolved) return;
    const auto recipe = std::find_if(catalog.evolutions.begin(), catalog.evolutions.end(),
        [&](const EvolutionDef& item) { return item.weapon == weapon.id; });
    require(recipe != catalog.evolutions.end(), "Missing evolution recipe: " + weapon.id);
    build.passives[recipe->passive] = 1;
}

std::string weaponName(const Catalog& catalog, const Weapon& weapon) {
    if (weapon.evolved) {
        for (const auto& recipe : catalog.evolutions)
            if (recipe.weapon == weapon.id) return recipe.name;
    }
    return catalog.upgrades[std::size_t(catalog.find(weapon.id))].name;
}

std::vector<Build> makeBuilds(const Catalog& catalog) {
    std::vector<std::string> families;
    for (const auto& item : catalog.upgrades)
        if (item.kind == UpgradeKind::Weapon) families.push_back(item.id);
    require(families.size() == 9, "Expected nine weapon families");
    std::vector<Build> builds;
    std::vector<Weapon> variants;
    for (const auto& id : families) {
        for (int rank : {1, 4, 8}) {
            Weapon weapon{id, rank, false};
            builds.push_back({id + "_r" + std::to_string(rank), weaponName(catalog, weapon) + " (rank " + std::to_string(rank) + ")", "base", {weapon}, {}});
        }
        Weapon weapon{id, 8, true};
        Build evolved{id + "_evolved", weaponName(catalog, weapon), "evolved", {weapon}, {}};
        addRecipe(catalog, evolved, weapon); builds.push_back(std::move(evolved));
        variants.push_back({id, 8, false}); variants.push_back(weapon);
    }
    int pairs = 0;
    for (std::size_t a = 0; a < variants.size(); ++a)
        for (std::size_t b = a + 1; b < variants.size(); ++b) {
            if (variants[a].id == variants[b].id) continue;
            Build pair{"pair_" + std::to_string(++pairs),
                weaponName(catalog, variants[a]) + " + " + weaponName(catalog, variants[b]),
                "pair", {variants[a], variants[b]}, {}};
            for (const auto& weapon : pair.weapons) addRecipe(catalog, pair, weapon);
            builds.push_back(std::move(pair));
        }
    require(pairs == 144, "Expected 144 legal two-weapon pairs");
    const std::array<std::array<const char*, 4>, 4> presets{{
        {{"venom_shot", "viper_fangs", "ricochet_fang", "tongue_lash"}},
        {{"plague_mortar", "snake_nest", "venom_nova", "cobra_pit"}},
        {{"venom_shot", "viper_fangs", "venom_nova", "tongue_lash"}},
        {{"plague_mortar", "snake_nest", "sidewinder_beam", "cobra_pit"}}
    }};
    for (std::size_t i = 0; i < presets.size(); ++i) {
        Build build{"four_" + std::to_string(i + 1), "", "build", {}, {}};
        for (const auto* id : presets[i]) {
            Weapon weapon{id, 8, i >= 2};
            if (!build.label.empty()) build.label += " + ";
            build.label += weaponName(catalog, weapon);
            build.weapons.push_back(weapon); addRecipe(catalog, build, weapon);
        }
        builds.push_back(std::move(build));
    }
    return builds;
}

std::vector<Build> makeUpgradeBuilds(const Catalog& catalog, const std::string& profile) {
    std::vector<Build> builds;
    std::vector<std::string> profiles = profile == "all"
        ? std::vector<std::string>{"recipe", "max", "offense"}
        : std::vector<std::string>{profile == "min" ? "recipe" : profile};
    for (const auto& support : profiles) {
        require(support == "recipe" || support == "max" || support == "offense", "Unknown support profile: " + support);
        for (const auto& recipe : catalog.evolutions) {
            const int recipeRank = support == "recipe" ? 1 : catalog.upgrades[std::size_t(catalog.find(recipe.passive))].maxRank;
            std::map<std::string, int> passives{{recipe.passive, recipeRank}};
            if (support == "offense") {
                for (const auto* id : {"rapid_fire", "coiled_volley", "chronofield", "dead_eye", "ravenous_maw"}) {
                    if (passives.size() >= PassiveSlots) break;
                    passives[id] = catalog.upgrades[std::size_t(catalog.find(id))].maxRank;
                }
            }
            for (bool evolved : {false, true}) {
                Weapon weapon{recipe.weapon, 8, evolved};
                builds.push_back({support + "_" + recipe.weapon + (evolved ? "_evolved" : "_base"),
                    weaponName(catalog, weapon) + (evolved ? "" : " (rank 8)") + " [" + support + "]",
                    "upgrade_" + support, {weapon}, passives});
            }
        }
    }
    return builds;
}

Json buildJson(const Build& build) {
    Json result{{"id",build.id},{"label",build.label},{"category",build.category},
        {"weapons",Json::array()},{"passives",Json::array()}};
    for (const auto& weapon : build.weapons)
        result["weapons"].push_back({{"id",weapon.id},{"rank",weapon.rank},{"evolved",weapon.evolved}});
    for (const auto& passive : build.passives)
        result["passives"].push_back({{"id",passive.first},{"rank",passive.second}});
    return result;
}

Workload makeWorkload(const Catalog& catalog, const Scenario& scenario, std::uint32_t seed) {
    Game source(catalog, seed); initialize(source, scenario);
    Workload result{scenario, seed, {}, {}};
    if (scenario.bossDistance > 0) {
        Enemy boss;
        boss.id = ExternalId; boss.kind = EnemyKind::Boss;
        boss.position = boss.previous = {source.player.position.x + float(scenario.bossDistance), source.player.position.y};
        boss.trail.fill(boss.position); boss.previousTrail = boss.trail;
        boss.hp = boss.maxHp = 100000000; boss.speed = 0; boss.radius = 7; boss.trailSize = 8;
        result.spawns.push_back({0, boss, {float(scenario.bossDistance), 0}});
        result.summary = {{"scenario_id",scenario.id},{"seed",seed},{"external_spawned",1},
            {"external_hp",boss.maxHp},{"boss",1},{"probe",true},{"boss_distance",scenario.bossDistance},
            {"source_summary",{{"spawned",1},{"total_hp",boss.maxHp},{"kinds",{{"boss",1}}},
                {"min_hp",boss.maxHp},{"max_hp",boss.maxHp},{"min_speed",0},{"max_speed",0}}}};
        return result;
    }
    std::set<std::uint32_t> seen;
    std::array<int, 4> kinds{};
    double totalHp = 0;
    double minHp = 1e100, maxHp = 0, minSpeed = 1e100, maxSpeed = 0;
    std::uint64_t digest = 1469598103934665603ull;
    auto hash = [&](std::uint64_t value) { digest ^= value; digest *= 1099511628211ull; };
    for (int tick = 0; tick < scenario.seconds * 60; ++tick) {
        const Vec frameStart = source.player.position;
        if (!scenario.moving) source.player.moveClock = -1000000;
        source.update(FixedStep, control(tick, scenario.moving));
        require(source.phase == Phase::Playing, "Director source unexpectedly stopped");
        require(source.enemies.size() < EnemyCap, "Source director hit enemy cap; shorten the scenario");
        for (const auto& enemy : source.enemies) if (seen.insert(enemy.id).second) {
            require(enemy.kind != EnemyKind::Boss, "Unexpected source boss");
            Spawn spawn{tick, enemy, {enemy.position.x - frameStart.x,
                                      enemy.position.y - frameStart.y}};
            spawn.enemy.id = ExternalId + std::uint32_t(result.spawns.size());
            // A newly spawned enemy has not yet crossed its first grid cell.
            // Reset age to pre-update state because injection precedes update.
            spawn.enemy.moveClock = 0;
            spawn.enemy.previous = spawn.enemy.position;
            spawn.enemy.trail.fill(spawn.enemy.position);
            spawn.enemy.previousTrail = spawn.enemy.trail;
            totalHp += enemy.maxHp; ++kinds[std::size_t(enemy.kind)];
            minHp = std::min(minHp, enemy.maxHp); maxHp = std::max(maxHp, enemy.maxHp);
            minSpeed = std::min(minSpeed, double(enemy.speed)); maxSpeed = std::max(maxSpeed, double(enemy.speed));
            hash(std::uint64_t(tick)); hash(std::uint64_t(enemy.kind));
            hash(std::uint64_t(int(spawn.offset.x) + 10000)); hash(std::uint64_t(int(spawn.offset.y) + 10000));
            hash(std::uint64_t(std::llround(enemy.maxHp * 1000)));
            result.spawns.push_back(spawn);
        }
    }
    result.summary = {{"scenario_id",scenario.id},{"seed",seed},{"external_spawned",result.spawns.size()},
        {"external_hp",totalHp},{"red",kinds[0]},{"splitter",kinds[1]},{"swift",kinds[2]},
        {"source_summary",{{"spawned",result.spawns.size()},{"total_hp",totalHp},
            {"kinds",{{"red",kinds[0]},{"splitter",kinds[1]},{"swift",kinds[2]}}},
            {"min_hp",minHp},{"max_hp",maxHp},{"min_speed",minSpeed},{"max_speed",maxSpeed}}},
        {"schedule_digest",std::to_string(digest)}};
    return result;
}

void validate(const Game& game, const Build& build) {
    require(game.phase == Phase::Playing, "Simulation left playing phase");
    require(game.enemies.size() < EnemyCap - 2, "Enemy cap could suppress splitter children");
    require(game.projectiles.size() <= 1600 && game.zones.size() <= 128 &&
        game.nestEggs.size() <= 25 && game.miniSnakes.size() <= 60 &&
        game.cobras.size() <= 30 && game.beams.size() <= 32 && game.grapples.size() <= 16,
        "Actor capacity invariant failed");
    require(game.owned.size() == build.weapons.size() + build.passives.size(), "Build changed during simulation");
    for (const auto& weapon : build.weapons) {
        const int definition = game.catalog.find(weapon.id);
        require(std::any_of(game.owned.begin(), game.owned.end(), [&](const OwnedUpgrade& item) {
            return item.definition == definition && item.rank == weapon.rank && item.evolved == weapon.evolved;
        }), "Weapon rank/evolution changed");
    }
    for (const auto& passive : build.passives) {
        const int definition = game.catalog.find(passive.first);
        require(std::any_of(game.owned.begin(), game.owned.end(), [&](const OwnedUpgrade& item) {
            return item.definition == definition && item.rank == passive.second;
        }), "Passive rank changed");
    }
    auto position = [&](Vec value) {
        require(std::isfinite(value.x) && std::isfinite(value.y) && sameCell(gridPosition(value), value),
            "Non-finite or off-grid position");
    };
    position(game.player.position);
    for (const auto& enemy : game.enemies) {
        position(enemy.position); require(std::isfinite(enemy.hp) && enemy.hp > 0, "Invalid enemy HP");
        for (int i = 0; i < enemy.trailSize; ++i) position(enemy.trail[std::size_t(i)]);
    }
    for (const auto& mini : game.miniSnakes) {
        position(mini.position);
        for (const auto& segment : mini.segments) position(segment);
        if (mini.segmentCount == 5) for (const auto& segment : mini.extraSegments) position(segment);
    }
    for (const auto& shot : game.projectiles)
        require(std::isfinite(shot.position.x) && std::isfinite(shot.position.y) && std::isfinite(shot.damage),
            "Non-finite projectile state");
}

Json simulate(const Catalog& catalog, const Build& build, const Workload& workload) {
    const auto& scenario = workload.scenario;
    Game game(catalog, workload.seed); initialize(game, scenario);
    for (const auto& weapon : build.weapons)
        game.owned.push_back({catalog.find(weapon.id), weapon.rank, weapon.evolved, 0});
    for (const auto& passive : build.passives)
        game.owned.push_back({catalog.find(passive.first), passive.second, false, 0});
    require(build.weapons.size() <= WeaponSlots && build.passives.size() <= PassiveSlots,
        "Build exceeds equipment slots");
    std::vector<bool> childrenSeen(16384, false);
    double supplied = 0, remainingHp = 0, coldDamage = 0, previousDamage = 0;
    double externalSupplied = 0, externalRemainingHp = 0, externalDamage = 0, coldExternalDamage = 0;
    double bossKillSeconds = 0;
    int children = 0, hurt = 0, coldKills = 0, externalSplitters = 0, aliveExternalSplitters = 0;
    std::size_t aliveExternal = 0, coldExternalKills = 0;
    const int ticks = scenario.seconds * 60, coldTicks = ticks / 4;
    std::size_t cursor = 0;
    std::size_t peakEnemies = 0, peakProjectiles = 0, peakZones = 0, peakSummons = 0, peakCobras = 0;
    for (int tick = 0; tick < ticks; ++tick) {
        while (cursor < workload.spawns.size() && workload.spawns[cursor].tick == tick) {
            require(game.enemies.size() < EnemyCap, "External spawn rejected by enemy cap");
            const auto& spawn = workload.spawns[cursor++];
            Enemy enemy = spawn.enemy;
            const Vec position = gridPosition({game.player.position.x + spawn.offset.x,
                                               game.player.position.y + spawn.offset.y});
            enemy.position = enemy.previous = position;
            enemy.trail.fill(position); enemy.previousTrail = enemy.trail;
            game.enemies.push_back(enemy); supplied += enemy.maxHp; externalSupplied += enemy.maxHp;
            if (enemy.kind == EnemyKind::Splitter) ++externalSplitters;
        }
        game.spawnClock = -1000000; game.bossClock = 1000000;
        if (!scenario.moving) game.player.moveClock = -1000000;
        game.update(FixedStep, control(tick, scenario.moving));
        remainingHp = externalRemainingHp = 0; aliveExternal = 0; aliveExternalSplitters = 0;
        for (const auto& enemy : game.enemies) {
            // Splitter children are created after combat, so every child's full
            // original HP is observed before any weapon can damage it.
            if (enemy.id < ExternalId) {
                require(enemy.id < childrenSeen.size(), "Native child ID exceeded ledger bounds");
                if (!childrenSeen[enemy.id]) {
                    childrenSeen[enemy.id] = true; ++children; supplied += enemy.maxHp;
                }
            }
            remainingHp += enemy.hp;
            if (enemy.id >= ExternalId) {
                externalRemainingHp += enemy.hp; ++aliveExternal;
                if (enemy.kind == EnemyKind::Splitter) ++aliveExternalSplitters;
            }
        }
        externalDamage = externalSupplied - externalRemainingHp;
        if (scenario.bossDistance > 0 && game.kills > 0 && bossKillSeconds == 0) bossKillSeconds = (tick + 1) / 60.0;
        const double damage = supplied - remainingHp;
        require(std::isfinite(damage) && damage + .001 >= previousDamage && damage <= supplied + .001,
            "HP ledger is invalid/non-monotone");
        previousDamage = damage;
        for (const auto& event : game.events) if (event.type == Event::Hurt) ++hurt;
        peakEnemies = std::max(peakEnemies, game.enemies.size());
        peakProjectiles = std::max(peakProjectiles, game.projectiles.size());
        peakZones = std::max(peakZones, game.zones.size());
        peakSummons = std::max(peakSummons, game.miniSnakes.size());
        peakCobras = std::max(peakCobras, game.cobras.size());
        if (tick + 1 == coldTicks) {
            coldDamage = damage; coldKills = game.kills;
            coldExternalDamage = externalDamage; coldExternalKills = cursor - aliveExternal;
        }
        if (tick % 120 == 0 || tick + 1 == ticks) validate(game, build);
    }
    require(cursor == workload.spawns.size(), "Spawn schedule not fully consumed");
    require(std::size_t(game.kills) + game.enemies.size() == cursor + std::size_t(children),
        "Kills do not reconcile with supplied/alive enemies");
    require(game.player.level == scenario.level && game.chests == 0 &&
        game.bossesKilled == (scenario.bossDistance > 0 ? game.kills : 0),
        "Unexpected progression/boss/chest");
    const double damage = supplied - remainingHp;
    require(children % 2 == 0, "A native splitter child pair is incomplete");
    const int splittersKilled = externalSplitters - aliveExternalSplitters;
    const int suppressedSplitters = splittersKilled - children / 2;
    require(suppressedSplitters >= 0, "More native children than killed external splitters");
    require(std::isfinite(externalDamage) && externalDamage >= -.001 && externalDamage <= externalSupplied + .001 &&
        cursor >= aliveExternal && cursor - aliveExternal <= std::size_t(game.kills), "External HP/kill ledger is invalid");
    return {{"build_id",build.id},{"scenario_id",scenario.id},{"seed",workload.seed},
        {"seconds",scenario.seconds},{"damage",damage},{"dps",damage/scenario.seconds},{"kills",game.kills},
        {"kills_per_minute",game.kills*60.0/scenario.seconds},
        {"cold_seconds",coldTicks/60.0},{"cold_damage",coldDamage},{"cold_kills",coldKills},
        {"steady_seconds",(ticks-coldTicks)/60.0},{"steady_damage",damage-coldDamage},
        {"steady_kills",game.kills-coldKills},{"remaining",game.enemies.size()},
        {"remaining_hp",remainingHp},{"spawned",cursor+std::size_t(children)},
        {"external_spawned",cursor},{"splitter_children",children},{"total_spawn_hp",supplied},
        {"external_spawn_hp",externalSupplied},{"external_remaining_hp",externalRemainingHp},
        {"external_damage",externalDamage},{"external_dps",externalDamage/scenario.seconds},
        {"external_kills",cursor-aliveExternal},{"external_remaining",aliveExternal},
        {"external_splitters_killed",splittersKilled},{"suppressed_splitters",suppressedSplitters},
        {"cold_external_damage",coldExternalDamage},{"cold_external_kills",coldExternalKills},
        {"steady_external_damage",externalDamage-coldExternalDamage},
        {"steady_external_kills",cursor-aliveExternal-coldExternalKills},
        {"boss_kill_seconds",bossKillSeconds > 0 ? Json(bossKillSeconds) : Json(nullptr)},
        {"hurt",hurt},{"peak_enemies",peakEnemies},{"peak_projectiles",peakProjectiles},
        {"peak_zones",peakZones},{"peak_summons",peakSummons},{"peak_cobras",peakCobras},
        {"final_leviathan_growth",game.leviathan.growth},{"final_length",game.player.length},
        {"validation",{{"finite_grid_equipment_caps",true},{"hp_ledger",true},{"kill_ledger",true},
            {"external_hp_ledger",true},{"external_kill_ledger",true},{"splitter_suppression_ledger",true}}}};
}

Json upgradeComparisons(const std::vector<Build>& builds, const std::vector<Scenario>& scenarios, const Json& runs) {
    Json checks = Json::array();
    for (std::size_t index = 0; index < builds.size(); index += 2) {
        require(index + 1 < builds.size(), "Upgrade build pair is incomplete");
        const auto& base = builds[index]; const auto& evolved = builds[index + 1];
        require(base.passives == evolved.passives && !base.weapons.front().evolved && evolved.weapons.front().evolved,
            "Upgrade pair does not have identical passive investment");
        for (const auto& scenario : scenarios) {
            double baseDamage = 0, evolvedDamage = 0, baseKills = 0, evolvedKills = 0;
            double baseSupply = 0, baseSpawned = 0, externalSupply = 0, externalCount = 0;
            double baseExternalDamage = 0, evolvedExternalDamage = 0, baseExternalKills = 0, evolvedExternalKills = 0;
            double baseSuppressed = 0, evolvedSuppressed = 0, baseBossSeconds = 0, evolvedBossSeconds = 0;
            int count = 0, baseBossKills = 0, evolvedBossKills = 0;
            Json seedPairs = Json::array();
            for (const auto& run : runs) if (run["scenario_id"] == scenario.id && run["build_id"] == base.id) {
                const auto match = std::find_if(runs.begin(), runs.end(), [&](const Json& other) {
                    return other["scenario_id"] == scenario.id && other["build_id"] == evolved.id && other["seed"] == run["seed"];
                });
                require(match != runs.end(), "Missing matched evolution seed");
                baseDamage += run["damage"].get<double>(); evolvedDamage += (*match)["damage"].get<double>();
                baseKills += run["kills"].get<double>(); evolvedKills += (*match)["kills"].get<double>();
                baseSupply += run["total_spawn_hp"].get<double>(); baseSpawned += run["spawned"].get<double>();
                require(run["external_spawn_hp"] == (*match)["external_spawn_hp"] &&
                    run["external_spawned"] == (*match)["external_spawned"], "Matched external enemy supply differs");
                externalSupply += run["external_spawn_hp"].get<double>();
                externalCount += run["external_spawned"].get<double>();
                baseExternalDamage += run["external_damage"].get<double>(); evolvedExternalDamage += (*match)["external_damage"].get<double>();
                baseExternalKills += run["external_kills"].get<double>(); evolvedExternalKills += (*match)["external_kills"].get<double>();
                baseSuppressed += run["suppressed_splitters"].get<double>(); evolvedSuppressed += (*match)["suppressed_splitters"].get<double>();
                if (!run["boss_kill_seconds"].is_null()) { baseBossSeconds += run["boss_kill_seconds"].get<double>(); ++baseBossKills; }
                if (!(*match)["boss_kill_seconds"].is_null()) { evolvedBossSeconds += (*match)["boss_kill_seconds"].get<double>(); ++evolvedBossKills; }
                ++count;
                seedPairs.push_back({{"seed",run["seed"]},{"base_damage",run["damage"]},{"evolved_damage",(*match)["damage"]},
                    {"base_kills",run["kills"]},{"evolved_kills",(*match)["kills"]},
                    {"base_external_damage",run["external_damage"]},{"evolved_external_damage",(*match)["external_damage"]},
                    {"base_external_kills",run["external_kills"]},{"evolved_external_kills",(*match)["external_kills"]},
                    {"base_suppressed_splitters",run["suppressed_splitters"]},{"evolved_suppressed_splitters",(*match)["suppressed_splitters"]},
                    {"base_boss_kill_seconds",run["boss_kill_seconds"]},{"evolved_boss_kill_seconds",(*match)["boss_kill_seconds"]}});
            }
            require(count > 0, "Missing upgrade comparison runs");
            baseDamage /= count; evolvedDamage /= count; baseKills /= count; evolvedKills /= count;
            baseSupply /= count; baseSpawned /= count;
            externalSupply /= count; externalCount /= count; baseExternalDamage /= count; evolvedExternalDamage /= count;
            baseExternalKills /= count; evolvedExternalKills /= count; baseSuppressed /= count; evolvedSuppressed /= count;
            // Externally supplied enemies are identical across each pair. Devouring a
            // splitter prevents two real children, so raw child HP/kills cannot be
            // treated as lost output. Never fabricate damage for avoided children.
            const bool suppressionBenefit = evolvedSuppressed > baseSuppressed;
            const double externalKillTolerance = scenario.bossDistance > 0 ? 0 : std::max(1.0, baseExternalKills * .02);
            const double rawKillTolerance = scenario.bossDistance > 0 ? 0 : std::max(1.0, baseKills * .02);
            const bool damageFloor = evolvedExternalDamage + .001 >= baseExternalDamage * .98;
            const bool killFloor = evolvedExternalKills + externalKillTolerance + 1e-6 >= baseExternalKills;
            const bool rawDamageFloor = evolvedDamage + .001 >= baseDamage * .98;
            const bool rawKillFloor = evolvedKills + rawKillTolerance + 1e-6 >= baseKills;
            const bool supplySaturated = baseExternalDamage >= externalSupply * .85 || baseExternalKills >= externalCount * .85;
            const bool bothKillBoss = scenario.bossDistance > 0 && baseBossKills == count && evolvedBossKills == count;
            const bool quickerBoss = bothKillBoss && evolvedBossSeconds <= baseBossSeconds * .95;
            const bool meaningful = evolvedExternalDamage > baseExternalDamage * 1.05 + .001 ||
                evolvedExternalKills > baseExternalKills * 1.05 + .001 || quickerBoss;
            const bool saturationWaivesGain = supplySaturated && !(scenario.bossDistance > 0 && baseBossKills == count);
            const bool passed = damageFloor && killFloor && (suppressionBenefit || (rawDamageFloor && rawKillFloor)) &&
                (saturationWaivesGain || meaningful);
            checks.push_back({{"weapon_id",base.weapons.front().id},{"profile",base.category.substr(8)},
                {"scenario_id",scenario.id},{"base_id",base.id},{"evolved_id",evolved.id},{"seeds",count},
                {"base_damage",baseDamage},{"evolved_damage",evolvedDamage},
                {"base_dps",baseDamage/scenario.seconds},{"evolved_dps",evolvedDamage/scenario.seconds},
                {"damage_ratio",baseDamage > 0 ? Json(evolvedDamage/baseDamage) : Json(nullptr)},
                {"base_kills",baseKills},{"evolved_kills",evolvedKills},
                {"base_external_damage",baseExternalDamage},{"evolved_external_damage",evolvedExternalDamage},
                {"base_external_dps",baseExternalDamage/scenario.seconds},{"evolved_external_dps",evolvedExternalDamage/scenario.seconds},
                {"external_damage_ratio",baseExternalDamage > 0 ? Json(evolvedExternalDamage/baseExternalDamage) : Json(nullptr)},
                {"base_external_kills",baseExternalKills},{"evolved_external_kills",evolvedExternalKills},
                {"base_external_supply_removed",externalSupply > 0 ? baseExternalDamage/externalSupply : 0},
                {"base_external_kill_fraction",externalCount > 0 ? baseExternalKills/externalCount : 0},
                {"base_suppressed_splitters",baseSuppressed},{"evolved_suppressed_splitters",evolvedSuppressed},
                {"suppression_benefit",suppressionBenefit},{"raw_floor_required",!suppressionBenefit},
                {"external_kill_tolerance",externalKillTolerance},{"raw_kill_tolerance",rawKillTolerance},
                {"base_boss_kill_seconds",baseBossKills == count ? Json(baseBossSeconds/count) : Json(nullptr)},
                {"evolved_boss_kill_seconds",evolvedBossKills == count ? Json(evolvedBossSeconds/count) : Json(nullptr)},
                {"quicker_boss_kill",quickerBoss},
                {"base_supply_removed",baseSupply > 0 ? baseDamage/baseSupply : 0},
                {"base_kill_fraction",baseSpawned > 0 ? baseKills/baseSpawned : 0},
                {"damage_floor_passed",damageFloor},{"kill_floor_passed",killFloor},
                {"raw_damage_floor_passed",rawDamageFloor},{"raw_kill_floor_passed",rawKillFloor},
                {"base_supply_saturated",supplySaturated},{"saturation_waives_gain",saturationWaivesGain},{"meaningful_gain",meaningful},
                {"passed",passed},{"matched_seeds",std::move(seedPairs)}});
        }
    }
    return checks;
}
}

int main(int argc, char** argv) {
    try {
        std::string data = "data", output = "artifacts/balance/results.json", profile = "recipe", weaponFilter;
        int seeds = 5, seconds = 120, limit = 0, offset = 0;
        bool repeat = false, evolutionCheck = false, assertUpgrades = false, explicitOutput = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--verify-repeat") repeat = true;
            else if (arg == "--evolution-check") evolutionCheck = true;
            else if (arg == "--assert-upgrades") assertUpgrades = true;
            else if (arg == "--help") {
                std::cout << "balance_sim [--data DIR] [--output FILE] [--seeds N] [--seconds N] "
                    "[--limit N] [--offset N] [--verify-repeat] [--evolution-check] "
                    "[--profile recipe|min|max|offense|all] [--weapon ID] [--assert-upgrades]\n"; return 0;
            } else {
                require(i + 1 < argc, "Missing argument for " + arg);
                const std::string value = argv[++i];
                if (arg == "--data") data = value;
                else if (arg == "--output") { output = value; explicitOutput = true; }
                else if (arg == "--seeds") seeds = std::stoi(value);
                else if (arg == "--seconds") seconds = std::stoi(value);
                else if (arg == "--limit") limit = std::stoi(value);
                else if (arg == "--offset") offset = std::stoi(value);
                else if (arg == "--profile") profile = value;
                else if (arg == "--weapon") weaponFilter = value;
                else throw std::runtime_error("Unknown option: " + arg);
            }
        }
        require(seeds >= 1 && seeds <= 100 && seconds >= 4 && seconds <= 180 && limit >= 0 && offset >= 0,
            "Invalid seeds/seconds/limit/offset bounds");
        const auto catalog = Catalog::load(data);
        require(!assertUpgrades || evolutionCheck, "--assert-upgrades requires --evolution-check");
        if (evolutionCheck && !explicitOutput) output = "artifacts/balance/evolution-checks.json";
        auto builds = evolutionCheck ? makeUpgradeBuilds(catalog, profile) : makeBuilds(catalog);
        if (!weaponFilter.empty()) {
            require(evolutionCheck, "--weapon requires --evolution-check");
            builds.erase(std::remove_if(builds.begin(), builds.end(), [&](const Build& build) {
                return build.weapons.front().id != weaponFilter;
            }), builds.end());
            require(!builds.empty(), "No evolution family matches --weapon " + weaponFilter);
        }
        require(!evolutionCheck || (offset % 2 == 0 && limit % 2 == 0), "Evolution offsets/limits must keep complete pairs");
        require(offset < int(builds.size()), "Build offset is out of range");
        builds.erase(builds.begin(), builds.begin() + offset);
        if (limit && int(builds.size()) > limit) builds.resize(std::size_t(limit));
        std::vector<Scenario> scenarios{
            {"early_moving", "Early director, moving square route", 0, 5, seconds, true},
            {"late_moving", "Late director, moving square route", 840, 40, std::min(seconds,60), true},
            {"late_hold", "Late director, stationary east-facing defense", 840, 40, std::min(seconds,60), false}
        };
        if (evolutionCheck) {
            scenarios.push_back({"boss_close", "Stationary durable boss, three cells ahead", 840, 40, std::min(seconds,90), false, 24});
            scenarios.push_back({"boss_mid", "Stationary durable boss, eight cells ahead", 840, 40, std::min(seconds,90), false, 64});
        }
        Json result{{"method_version",1},{"seconds",seconds},{"fixed_hz",60},{"character",0},
            {"seed_values",Json::array()},{"scenarios",Json::array()},{"builds",Json::array()},
            {"workloads",Json::array()},{"runs",Json::array()},{"control_runs",Json::array()},
            {"assumptions",{{"bosses",false},{"initial_actors",false},{"player_hp",100000000},
                {"player_invulnerable",false},{"progression_locked",true},{"recipe_passive_rank",1},
                {"turn_every_ticks",150},{"damage_metric","effective_hp_removed_no_overkill"}}}};
        if (evolutionCheck) {
            result["mode"] = "evolution_check"; result["profile"] = profile;
            if (!weaponFilter.empty()) result["weapon_filter"] = weaponFilter;
            result["assumptions"]["matched_recipe_passives"] = true;
            result["assumptions"]["boss_probes"] = true;
            result["upgrade_criteria"] = {{"minimum_damage_ratio",.98},{"maximum_mean_kill_loss","max(1,2% of base kills); bosses: zero"},
                {"minimum_gain_without_saturation",.05},{"saturation_fraction",.85},
                {"scope","mean observed external-target damage and kills across matched seeds per profile/scenario"},
                {"raw_output_floors","also required unless the evolution actually suppresses splitter children"},
                {"boss_ttk_gain_when_both_kill",.05},
                {"boss_probe_hp",100000000},{"boss_probe_speed",0}};
        }
        for (const auto& build : builds) result["builds"].push_back(buildJson(build));
        std::vector<Workload> workloads;
        for (const auto& scenario : scenarios) {
            result["scenarios"].push_back({{"id",scenario.id},{"name",scenario.name},
                {"elapsed",scenario.elapsed},{"level",scenario.level},{"moving",scenario.moving},
                {"seconds",scenario.seconds},{"cold_seconds",scenario.seconds/4.0}});
            if (scenario.bossDistance > 0) result["scenarios"].back()["boss_distance"] = scenario.bossDistance;
            for (int seedIndex = 0; seedIndex < seeds; ++seedIndex) {
                const std::uint32_t seed = 0xBA1A000u + std::uint32_t(seedIndex)*977u;
                if (scenario.id == "early_moving") result["seed_values"].push_back(seed);
                workloads.push_back(makeWorkload(catalog, scenario, seed));
                result["workloads"].push_back(workloads.back().summary);
            }
        }
        const Build controlBuild{"control", "No weapons", "control", {}, {}};
        for (const auto& workload : workloads) {
            auto run = simulate(catalog, controlBuild, workload);
            require(run["damage"].get<double>() == 0 && run["kills"].get<int>() == 0,
                "No-weapon control produced damage or kills");
            result["control_runs"].push_back(std::move(run));
        }
        std::size_t done = 0;
        for (const auto& build : builds) {
            for (const auto& workload : workloads) {
                auto run = simulate(catalog, build, workload);
                if (repeat) require(run == simulate(catalog, build, workload), "Seeded repeat differs");
                result["runs"].push_back(std::move(run)); ++done;
            }
            std::cerr << done << "/" << builds.size()*workloads.size() << " runs: " << build.id << '\n';
        }
        result["validation"] = {{"all_runs_passed",true},{"repeat_verified",repeat},
            {"run_count",result["runs"].size()},{"build_count",builds.size()},
            {"zero_damage_controls_passed",true},{"control_count",result["control_runs"].size()}};
        std::size_t upgradeFailures = 0;
        if (evolutionCheck) {
            result["upgrade_checks"] = upgradeComparisons(builds, scenarios, result["runs"]);
            for (const auto& check : result["upgrade_checks"]) if (!check["passed"].get<bool>()) {
                ++upgradeFailures;
                std::cerr << "UPGRADE FAIL " << check["profile"].get<std::string>() << ' '
                    << check["weapon_id"].get<std::string>() << ' ' << check["scenario_id"].get<std::string>()
                    << ": dps=" << check["base_dps"] << " -> " << check["evolved_dps"]
                    << ", kills=" << check["base_kills"] << " -> " << check["evolved_kills"]
                    << "; external dps=" << check["base_external_dps"] << " -> " << check["evolved_external_dps"]
                    << ", external kills=" << check["base_external_kills"] << " -> " << check["evolved_external_kills"] << '\n';
            }
            result["validation"]["upgrade_check_count"] = result["upgrade_checks"].size();
            result["validation"]["upgrade_failure_count"] = upgradeFailures;
            result["validation"]["all_upgrades_passed"] = upgradeFailures == 0;
            std::cerr << result["upgrade_checks"].size() - upgradeFailures << '/' << result["upgrade_checks"].size()
                << " upgrade comparisons passed\n";
        }
        const auto path = std::filesystem::path(output);
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path); require(bool(file), "Cannot open output " + output);
        file << result.dump(2) << '\n'; require(bool(file), "Could not write output");
        std::cerr << "Wrote " << output << '\n';
        return assertUpgrades && upgradeFailures ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Balance simulation failed: " << error.what() << '\n'; return 1;
    }
}
