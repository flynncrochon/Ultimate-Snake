#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using namespace snake;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void isolate(Game& game){
    game.enemies.clear();game.projectiles.clear();game.zones.clear();game.pickups.clear();game.owned.clear();
    game.greySnakes.enabled=false;game.spawnClock=-1000;game.bossClock=100;
    game.player.moveClock=-1000;game.player.invulnerable=1000;game.player.nextXp=1000000;
}
void checkGrid(const Game& game){
    for(const auto& pickup:game.pickups)
        require(inGridBounds(pickup.position)&&sameCell(gridPosition(pickup.position),pickup.position),"Ground loot left the in-bounds tile grid.");
}
void defeat(Game& game,Vec position,std::uint32_t id){
    Enemy enemy;enemy.id=id;enemy.position=enemy.previous=position;enemy.hp=enemy.maxHp=1;
    enemy.speed=0;enemy.trailSize=0;enemy.trail.fill(position);game.enemies.push_back(enemy);
    Projectile shot;shot.position=gridPosition(position);shot.damage=10000;shot.life=1;
    game.projectiles.push_back(shot);game.update(FixedStep,{});
}
}

int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& run){
        try{run();++passed;std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}
    };
    const auto content=Catalog::load(SNAKE_TEST_DATA_DIR);
    test("starting and showcase loot occupies native grid cells",[&]{
        Game game(content,1200);require(game.pickups.size()==24,"Starting fruit fixture changed.");checkGrid(game);
        game.debugPopulate();require(game.pickups.size()==67,"Showcase loot fixture changed.");checkGrid(game);
    });
    test("real enemy deaths leave grid-aligned fruit and hearts at arena edges",[&]{
        Game game(content,1201);isolate(game);
        const Vec positions[]{{9,10},{ArenaSize-9,10},{9,ArenaSize-9},{ArenaSize-9,ArenaSize-10}};
        for(int i=0;i<256;++i){
            const Vec position=positions[i%4];
            // Stay close enough that normal enemy recycling does not move the
            // fixture away from the edge before the projectile hits it.
            game.player.position=game.player.previous={position.x<ArenaSize/2?80:ArenaSize-80,position.y<ArenaSize/2?80:ArenaSize-80};
            game.player.segments.assign(std::size_t(game.player.length),game.player.position);game.player.previousSegments=game.player.segments;
            defeat(game,position,std::uint32_t(1000+i));checkGrid(game);
        }
        require(game.kills==256,"Fixture did not defeat the actual enemies.");
        int fruit=0,hearts=0;for(const auto& pickup:game.pickups){
            fruit+=pickup.kind==PickupKind::Experience;hearts+=pickup.kind==PickupKind::Heart;
        }
        require(fruit==256&&hearts>0,"Seeded deaths did not exercise both XP and heart drops.");
    });
    test("all ground loot collects within one tile and never attracts from farther away",[&]{
        for(int gravitonRank:{0,8})for(auto kind:{PickupKind::Experience,PickupKind::Heart,PickupKind::Chest})
        for(Vec offset:{Vec{0,0},Vec{8,0},Vec{-8,0},Vec{0,8},Vec{0,-8},Vec{8.01f,0},Vec{16,0},Vec{8,8},Vec{48,0}}){
            Game game(content,1202);isolate(game);game.player.hp=40;
            if(gravitonRank)game.owned={{content.find("graviton"),gravitonRank,false,0}};
            const Vec landing{game.player.position.x+offset.x,game.player.position.y+offset.y};
            const int value=kind==PickupKind::Experience?3:kind==PickupKind::Heart?20:1;
            game.pickups.push_back({landing,value,kind});game.update(FixedStep,{});
            const bool inRange=offset.x*offset.x+offset.y*offset.y<=Tile*Tile;
            require(game.pickups.empty()==inRange,"Pickup ignored the inclusive one-tile head radius.");
            require(game.player.length==7+(inRange?(kind==PickupKind::Experience?3:1):0),"Pickup growth was missing or applied outside pickup range.");
            require(game.stats().pickup==float(Tile),"A passive bypassed the hard pickup limit.");
            if(!inRange){
                for(int tick=0;tick<60;++tick)game.update(FixedStep,{});
                require(game.pickups.size()==1&&sameCell(game.pickups.front().position,landing),"Distant loot drifted or disappeared while the head stood still.");
                require(game.player.length==7&&game.chests==0&&game.player.hp==40,"Distant loot granted a reward or tail growth.");
            }else{
                game.update(FixedStep,{});
                require(game.player.length==7+(kind==PickupKind::Experience?3:1),"An already-collected pickup grew the tail twice.");
                require(kind!=PickupKind::Heart||game.player.hp==60,"Heart growth replaced its healing reward.");
                require(kind!=PickupKind::Chest||game.chests==1,"Chest growth replaced its chest reward.");
            }
        }
    });
    test("body and tail contact cannot collect distant ground loot",[&]{
        Game game(content,1203);isolate(game);
        const Vec landing=game.player.segments.back();game.pickups.push_back({landing,1,PickupKind::Experience});
        for(int tick=0;tick<60;++tick)game.update(FixedStep,{});
        require(game.pickups.size()==1&&sameCell(game.pickups.front().position,landing)&&game.player.length==7,"The tail collected a drop outside the head's pickup range.");
    });
    test("fast movement sweeps the one-tile radius for every pickup kind",[&]{
        Catalog fast=content;UpgradeDef haste;haste.id="pickup_test_haste";haste.kind=UpgradeKind::Passive;haste.modifiers={{Stat::Speed,1}};
        fast.upgrades.push_back(haste);
        for(auto kind:{PickupKind::Experience,PickupKind::Heart,PickupKind::Chest}){
            Game game(fast,1204);isolate(game);game.owned={{int(fast.upgrades.size()-1),1,false,0}};game.player.moveClock=.119f;
            const Vec start=game.player.position,nearby{start.x+Tile,start.y+Tile},farther{start.x+Tile,start.y+2*Tile};
            game.pickups.push_back({nearby,1,kind});game.pickups.push_back({farther,1,kind});game.update(.1f,{{1,0}});
            require(game.player.position.x>=start.x+2*Tile,"Fixture did not move past both pickup endpoints.");
            require(game.pickups.size()==1&&sameCell(game.pickups.front().position,farther),"Fast movement skipped nearby loot or collected beyond one tile.");
            require(game.player.length==8,"Swept collection failed to grow the tail exactly once.");
        }
    });
    test("hearts at full health and every merged chest still grow the tail",[&]{
        Game game(content,1205);isolate(game);
        game.pickups.push_back({game.player.position,20,PickupKind::Heart});game.update(FixedStep,{});
        require(game.player.length==8&&game.player.hp==game.player.maxHp,"Full health suppressed heart pickup growth.");
        game.pickups.push_back({game.player.position,3,PickupKind::Chest});game.update(FixedStep,{});
        require(game.player.length==11&&game.chests==3,"Merged chests lost tail growth or a reward.");
        game.update(FixedStep,{});require(game.player.length==11&&game.chests==3,"Merged chests granted growth or rewards twice.");
    });
    test("pause freezes pickup growth and all loot respects maximum length",[&]{
        for(auto kind:{PickupKind::Experience,PickupKind::Heart,PickupKind::Chest}){
            Game game(content,1206);isolate(game);game.player.length=MaxPlayerLength-1;
            game.pickups.push_back({game.player.position,kind==PickupKind::Heart?20:3,kind});
            game.togglePause();game.update(.1f,{});
            require(game.player.length==MaxPlayerLength-1&&game.pickups.size()==1,"Paused collection grew the tail.");
            game.togglePause();game.update(FixedStep,{});
            require(game.player.length==MaxPlayerLength&&game.pickups.empty(),"Pickup exceeded bounded tail storage or failed to grow.");
            game.pickups.push_back({game.player.position,1,kind});game.update(FixedStep,{});
            require(game.player.length==MaxPlayerLength&&game.pickups.empty(),"Maximum length prevented collection or overflowed.");
        }
    });
    test("non-XP growth unfolds along the existing path without allocations",[&]{
        Game game(content,1207);isolate(game);
        const auto capacity=game.player.segments.capacity(),previousCapacity=game.player.previousSegments.capacity();
        const auto body=game.player.segments;
        game.pickups.push_back({game.player.position,20,PickupKind::Heart});
        game.pickups.push_back({game.player.position,3,PickupKind::Chest});game.update(FixedStep,{});
        require(game.player.length==11&&game.player.segments.size()==body.size(),"Pickup growth snapped the visible tail immediately.");
        for(std::size_t i=0;i<body.size();++i)require(sameCell(body[i],game.player.segments[i]),"A pickup moved an existing body segment.");
        game.player.moveClock=0;
        for(int tick=0;tick<36;++tick)game.update(FixedStep,{{1,0}});
        require(game.player.segments.size()==11,"New tail segments did not unfold during movement.");
        require(game.player.segments.capacity()==capacity&&game.player.previousSegments.capacity()==previousCapacity,"Pickup growth allocated body storage during play.");
        for(std::size_t i=1;i<game.player.segments.size();++i){
            const Vec a=game.player.segments[i-1],b=game.player.segments[i];
            require(std::abs(a.x-b.x)+std::abs(a.y-b.y)==Tile,"Tail growth left a gap or duplicate cell in the path.");
        }
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
