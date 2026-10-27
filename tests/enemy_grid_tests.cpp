#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < .001f; }
bool same(snake::Vec a, snake::Vec b) { return near(a.x,b.x) && near(a.y,b.y); }
bool onGrid(snake::Vec p) {
    return near(std::remainder(p.x,float(snake::Tile)),0) && near(std::remainder(p.y,float(snake::Tile)),0) && snake::inGridBounds(p);
}
snake::Game fixture(const snake::Catalog& catalog, std::uint32_t seed = 1729) {
    snake::Game game(catalog,seed);
    game.owned.clear(); game.enemies.clear(); game.pickups.clear();
    game.player.invulnerable = 10000; game.player.moveClock = -10000;
    game.spawnClock = -10000; game.bossClock = 100;
    game.player.position = game.player.previous = {880,880};
    return game;
}
snake::Enemy movingEnemy(snake::Vec position = {800,800}, float speed = 80) {
    snake::Enemy enemy;
    enemy.id = 10000; enemy.position = enemy.previous = position;
    enemy.trail.fill(position); enemy.previousTrail = enemy.trail;
    enemy.speed = speed; enemy.moveInterval = float(snake::Tile)/speed;
    enemy.hp = enemy.maxHp = 100000;
    enemy.motionReady = true;
    return enemy;
}
}

int main() {
    int passed=0,failed=0;
    auto test = [&](const char* name,const std::function<void()>& body) {
        try { body(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    };
    try {
        const auto catalog=snake::Catalog::load(SNAKE_TEST_DATA_DIR);
        test("enemies wait for their tick and advance one cardinal tile",[&]{
            auto game=fixture(catalog);game.enemies.push_back(movingEnemy());
            const auto start=game.enemies[0].position;
            game.update(.05f,{});
            require(same(start,game.enemies[0].position),"Enemy drifted between ticks.");
            game.update(.05f,{});
            const auto& enemy=game.enemies[0];
            const float dx=std::abs(enemy.position.x-start.x),dy=std::abs(enemy.position.y-start.y);
            require(near(dx+dy,snake::Tile) && (near(dx,0) || near(dy,0)),"Enemy did not take one cardinal grid step.");
            require(same(enemy.previous,start),"Previous head waypoint was not preserved.");
        });
        test("enemy bodies follow previous head cells through turns",[&]{
            auto game=fixture(catalog);game.enemies.push_back(movingEnemy());
            auto lastHead=game.enemies[0].position;
            for(int frame=0;frame<18;++frame){
                const auto lastTrail=game.enemies[0].trail;
                game.update(.1f,{});const auto& enemy=game.enemies[0];
                require(onGrid(enemy.position),"Enemy left the grid.");
                require(same(enemy.trail[0],lastHead),"Neck failed to follow the previous head cell.");
                for(int segment=1;segment<enemy.trailSize;++segment)
                    require(same(enemy.trail[std::size_t(segment)],lastTrail[std::size_t(segment-1)]),"Body waypoint was not shifted from the segment ahead.");
                lastHead=enemy.position;
            }
        });
        test("chasing never reverses an enemy head into its neck",[&]{
            auto game=fixture(catalog);game.player.position={760,800};
            auto enemy=movingEnemy();enemy.direction={1,0};enemy.trail[0]={792,800};enemy.previousTrail=enemy.trail;
            const auto neck=enemy.trail[0];game.enemies.push_back(enemy);game.update(.1f,{});
            require(!same(game.enemies[0].position,neck),"Enemy reversed into its own neck.");
            require(game.enemies[0].direction.y!=0,"Enemy failed to turn onto the other axis.");
        });
        test("wall and corner turns stay inside the grid",[&]{
            auto game=fixture(catalog);game.player.position={0,0};
            auto enemy=movingEnemy({8,8});enemy.direction={-1,0};enemy.trail.fill({16,8});enemy.previousTrail=enemy.trail;
            game.enemies.push_back(enemy);
            for(int frame=0;frame<100;++frame){game.update(.1f,{});require(onGrid(game.enemies[0].position),"Wall handling moved outside the grid.");}
        });
        test("slows reduce tick frequency without fractional drift",[&]{
            auto normal=fixture(catalog),slow=fixture(catalog);normal.enemies.push_back(movingEnemy());slow.enemies.push_back(movingEnemy());slow.enemies[0].slow=10;
            for(int frame=0;frame<5;++frame){normal.update(.1f,{});slow.update(.1f,{});require(onGrid(slow.enemies[0].position),"Slow effect introduced a fractional cell.");}
            const auto traveled=[](snake::Vec p){return std::abs(p.x-800)+std::abs(p.y-800);};
            require(traveled(normal.enemies[0].position)>traveled(slow.enemies[0].position),"Slow did not reduce grid movement speed.");
        });
        test("initialization and far recycling snap complete snakes to cells",[&]{
            auto game=fixture(catalog);auto enemy=movingEnemy({803,797},1);enemy.motionReady=false;
            game.enemies.push_back(enemy);game.update(.01f,{});
            require(onGrid(game.enemies[0].position),"Spawn normalization left a fractional head.");
            for(const auto segment:game.enemies[0].trail)require(onGrid(segment),"Spawn normalization left a fractional body.");
            game.enemies[0].position={8,8};game.enemies[0].speed=0;game.update(.01f,{});
            require(onGrid(game.enemies[0].position),"Recycling left a fractional head.");
            require(same(game.enemies[0].position,game.enemies[0].previous),"Recycling created a long interpolated teleport.");
            for(const auto segment:game.enemies[0].trail)require(same(segment,game.enemies[0].position),"Recycled body was stretched through the arena.");
        });
        test("contact knockback preserves grid cells and interpolation history",[&]{
            auto game=fixture(catalog);game.player.position={800,800};game.player.invulnerable=0;
            auto enemy=movingEnemy({800,800},1);enemy.speed=0;enemy.direction={1,0};enemy.trail[0]={792,800};enemy.trail[1]={784,800};enemy.trail[2]={776,800};enemy.previousTrail=enemy.trail;
            game.enemies.push_back(enemy);game.update(.01f,{});
            require(game.player.hp<game.player.maxHp,"Fixture did not exercise contact damage.");
            const auto& result=game.enemies[0];
            require(onGrid(result.position),"Knockback left the enemy off-grid.");
            require(same(result.position,result.previous),"Knockback retained an interpolated displacement.");
            for(int i=0;i<result.trailSize;++i){require(onGrid(result.trail[std::size_t(i)]),"Knockback left the body off-grid.");require(same(result.trail[std::size_t(i)],result.previousTrail[std::size_t(i)]),"Knockback kept stale body interpolation.");}
        });
        test("diagonal singularity pulls use one cardinal grid axis",[&]{
            auto game=fixture(catalog);game.player.position={800,800};
            auto enemy=movingEnemy({848,848},1);enemy.speed=0;enemy.direction={1,0};
            enemy.trail[0]={840,848};enemy.trail[1]={832,848};enemy.trail[2]={832,840};enemy.previousTrail=enemy.trail;
            const auto start=enemy.position;const auto body=enemy.trail;
            game.enemies.push_back(enemy);
            game.zones.push_back({{800,800},100,1,1,1,0,snake::Pattern::Mortar,true});
            // JS wells pull on their independent 0.16s cadence, not every frame.
            for(int tick=0;tick<10;++tick)game.update(snake::FixedStep,{});
            const auto& moved=game.enemies[0];
            const float dx=moved.position.x-start.x,dy=moved.position.y-start.y;
            require((near(dx,0) != near(dy,0)) && near(std::abs(dx)+std::abs(dy),float(snake::Tile)),"Diagonal gravity moved through both grid axes.");
            require(onGrid(moved.position) && same(moved.position,moved.previous),"Gravity pull left stale or off-grid head coordinates.");
            for(int i=0;i<moved.trailSize;++i){
                const auto segment=moved.trail[std::size_t(i)];
                require(onGrid(segment),"Gravity pull moved a body waypoint outside the grid.");
                require(same(segment,{body[std::size_t(i)].x+dx,body[std::size_t(i)].y+dy}),"Gravity pull distorted the body instead of translating it.");
                require(same(segment,moved.previousTrail[std::size_t(i)]),"Gravity pull retained stale body interpolation.");
            }
        });
        test("seeded enemy axis choices are reproducible",[&]{
            auto a=fixture(catalog,77),b=fixture(catalog,77);a.enemies.push_back(movingEnemy());b.enemies.push_back(movingEnemy());
            for(int frame=0;frame<80;++frame){a.update(.1f,{});b.update(.1f,{});require(same(a.enemies[0].position,b.enemies[0].position),"Seeded enemy paths diverged.");}
        });
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
