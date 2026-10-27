#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace snake {
namespace {
constexpr float Pi = 3.14159265358979323846f;

Vec directionTo(Vec from, Vec to, bool horizontal) {
    const float delta = horizontal ? to.x - from.x : to.y - from.y;
    const float sign = delta > 0 ? 1.0f : delta < 0 ? -1.0f : 0.0f;
    return horizontal ? Vec{sign, 0} : Vec{0, sign};
}
Vec nextCell(Vec position, Vec direction) {
    return {position.x + direction.x * Tile, position.y + direction.y * Tile};
}
bool cardinal(Vec direction) {
    return (std::abs(direction.x) == 1 && direction.y == 0) ||
           (std::abs(direction.y) == 1 && direction.x == 0);
}
float squaredDistance(Vec a, Vec b) {
    const float x = a.x - b.x, y = a.y - b.y;
    return x * x + y * y;
}
}

void Game::placeEnemy(Enemy& enemy, Vec position, bool collapseBody) {
    Vec destination = gridPosition(position);
    if (enemy.motionReady && !collapseBody) {
        // A force can cross several cells, but it still follows one grid axis.
        // Snapping both components would turn diagonal gravity/knockback into
        // an instantaneous diagonal move that ordinary chasing never makes.
        const bool horizontal = destination.x != enemy.position.x &&
            (destination.y == enemy.position.y ||
             std::abs(position.x - enemy.position.x) >= std::abs(position.y - enemy.position.y));
        if (horizontal) destination.y = enemy.position.y;
        else destination.x = enemy.position.x;
    }
    const Vec delta{destination.x - enemy.position.x, destination.y - enemy.position.y};
    if (!enemy.motionReady || collapseBody) {
        enemy.splitBirth.count=0;
        ++enemy.placementSerial;
        enemy.trail.fill(destination);
    } else {
        ++enemy.forceSerial;
        // Translate every waypoint together. This keeps knockback and gravity
        // from leaving the head between cells or stretching the body.
        for (Vec& segment : enemy.trail)
            segment = gridPosition({segment.x + delta.x, segment.y + delta.y});
    }
    enemy.position = enemy.previous = destination;
    enemy.previousTrail = enemy.trail;
    enemy.trailSize = std::clamp(enemy.trailSize, 0, int(enemy.trail.size()));
    if (!cardinal(enemy.direction)) enemy.direction = {0, 1};
    enemy.moveClock = 0;
    enemy.motionReady = true;
}

void Game::moveEnemies(float dt) {
    for (Enemy& enemy : enemies) {
        if (!enemy.alive) continue;
        if(enemy.splitBirth.count){
            // Retain the endpoint for the renderer's one-tick interpolation
            // interval, so high-refresh frames finish the same color and pose.
            constexpr float retire=SplitBirthDuration+FixedStep;
            enemy.splitBirth.age=std::min(retire,enemy.splitBirth.age+dt);
            if(enemy.splitBirth.age>=retire)enemy.splitBirth.count=0;
        }
        if (!enemy.motionReady) placeEnemy(enemy, enemy.position, true);
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        enemy.slow = std::max(0.0f, enemy.slow - dt);
        enemy.orbitCooldown = std::max(0.0f, enemy.orbitCooldown - dt);
        if(enemy.grabbed){enemy.velocity={};continue;}

        // Recycling is a grid relocation, not a long interpolated trip through
        // the arena. It happens before this update's collision grid is built.
        if (squaredDistance(enemy.position, player.position) > 420.0f * 420.0f) {
            const float angle = random(0, 2 * Pi);
            placeEnemy(enemy, {player.position.x + std::cos(angle) * 205,
                               player.position.y + std::sin(angle) * 165}, true);
        }

        const float speed = std::isfinite(enemy.speed) ? std::max(0.0f, enemy.speed) : 0;
        const float effectiveSpeed = speed * (enemy.slow > 0 ? std::clamp(enemy.slowFactor,.05f,1.0f) : 1.0f);
        if (effectiveSpeed <= 0) {
            enemy.velocity = {};
            enemy.previous = enemy.position;
            enemy.previousTrail = enemy.trail;
            enemy.moveClock = 0;
            continue;
        }
        const float interval = std::max(.01f, float(Tile) / effectiveSpeed);
        // A slow changes how quickly the next cell is reached, without
        // teleporting or discarding progress through the current move.
        if (std::isfinite(enemy.moveInterval) && enemy.moveInterval > 0 && enemy.moveInterval != interval)
            enemy.moveClock = std::clamp(enemy.moveClock / enemy.moveInterval, 0.0f, 1.0f) * interval;
        enemy.moveInterval = interval;
        enemy.moveClock += dt;
        enemy.velocity = {enemy.direction.x * effectiveSpeed, enemy.direction.y * effectiveSpeed};

        while (enemy.moveClock >= interval) {
            enemy.moveClock -= interval;
            enemy.previous = enemy.position;
            enemy.previousTrail = enemy.trail;
            const float dx = player.position.x - enemy.position.x;
            const float dy = player.position.y - enemy.position.y;
            const float total = std::abs(dx) + std::abs(dy);
            if (total < Tile * .5f) continue;

            const bool horizontal = random(0, 1) < std::abs(dx) / total;
            Vec chosen = directionTo(enemy.position, player.position, horizontal);
            auto allowed = [&](Vec direction) {
                if (!cardinal(direction)) return false;
                const Vec destination = nextCell(enemy.position, direction);
                return inGridBounds(destination) &&
                    (enemy.trailSize == 0 || !sameCell(destination, enemy.trail[0]));
            };
            if (!allowed(chosen)) {
                chosen = directionTo(enemy.position, player.position, !horizontal);
                if (!cardinal(chosen)) {
                    const float sign = random(0, 1) < .5f ? -1.0f : 1.0f;
                    chosen = horizontal ? Vec{0, sign} : Vec{sign, 0};
                }
            }
            if (!allowed(chosen)) {
                // At a corner the other axis can also be blocked. Choose a
                // legal cardinal cell rather than clamping into the same cell.
                constexpr std::array<Vec, 4> candidates{{{1,0},{0,1},{-1,0},{0,-1}}};
                float best = std::numeric_limits<float>::max();
                chosen = {};
                const int offset = int(random(0, 4));
                for (int index = 0; index < 4; ++index) {
                    const Vec candidate = candidates[std::size_t((offset + index) % 4)];
                    if (!allowed(candidate)) continue;
                    const Vec destination = nextCell(enemy.position, candidate);
                    const float score = std::abs(destination.x - player.position.x) + std::abs(destination.y - player.position.y);
                    if (score < best) { best = score; chosen = candidate; }
                }
            }
            if (!allowed(chosen)) continue;

            for (int segment = enemy.trailSize - 1; segment > 0; --segment)
                enemy.trail[std::size_t(segment)] = enemy.trail[std::size_t(segment - 1)];
            if (enemy.trailSize > 0) enemy.trail[0] = enemy.position;
            enemy.position = nextCell(enemy.position, chosen);
            enemy.direction = chosen;
            enemy.velocity = {chosen.x * effectiveSpeed, chosen.y * effectiveSpeed};
        }
    }
}
}
