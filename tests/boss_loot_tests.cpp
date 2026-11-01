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
    game.spawnClock=-1000;game.bossClock=10;game.player.moveClock=-1000;game.player.invulnerable=1000;game.player.nextXp=1000000;
}
void relocate(Game& game,Vec position){
    game.player.position=game.player.previous=position;
    game.player.segments.assign(std::size_t(game.player.length),position);game.player.previousSegments=game.player.segments;
}
void killBoss(Game& game,Vec position,std::uint32_t id){
    Enemy boss;boss.kind=EnemyKind::Boss;boss.id=id;boss.position=boss.previous=position;boss.hp=boss.maxHp=1;boss.speed=0;boss.radius=7;boss.trailSize=0;boss.trail.fill(position);
    game.enemies.push_back(boss);
    Projectile shot;shot.pattern=Pattern::Lash;shot.position=position;shot.damage=10000;shot.life=1;game.projectiles.push_back(shot);
}
const Pickup& chest(const Game& game){
    const auto found=std::find_if(game.pickups.begin(),game.pickups.end(),[](const Pickup& p){return p.kind==PickupKind::Chest;});
    require(found!=game.pickups.end(),"Expected a dropped boss chest.");return *found;
}
}

int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    const auto content=Catalog::load(SNAKE_TEST_DATA_DIR);
    test("arena-edge boss deaths leave distinct reachable visible chests",[&]{
        Game game(content,901);isolate(game);relocate(game,{ArenaSize-4*Tile,800});
        const Vec death{ArenaSize-Tile,800};killBoss(game,death,1000);killBoss(game,death,1001);game.update(FixedStep,{});
        require(game.bossesKilled==2,"Both bosses must die through real combat.");
        std::vector<Vec> boxes;for(const auto& p:game.pickups)if(p.kind==PickupKind::Chest){
            require(inGridBounds(p.position)&&sameCell(gridPosition(p.position),p.position),"A chest is outside the reachable grid.");
            require(std::none_of(game.player.segments.begin(),game.player.segments.end(),[&](Vec s){return sameCell(p.position,s);}),"A chest spawned under the player body.");
            for(Vec previous:boxes)require(!sameCell(previous,p.position),"Boss chests overlap and hide each other.");boxes.push_back(p.position);
        }
        require(boxes.size()==2 && game.chests==0 && game.chestRewards.empty(),"Drops beyond one tile were silently auto-opened instead of remaining visible.");
    });
    test("walking over a chest grants one visible durable upgrade reward",[&]{
        Game game(content,902);isolate(game);const int definition=content.find("venom_shot");game.owned={{definition,1,false,100}};
        const Vec landing{game.player.position.x+2*Tile,game.player.position.y};game.pickups.push_back({landing,1,PickupKind::Chest});
        game.update(FixedStep,{});require(game.chests==0,"Distant chest opened before contact.");relocate(game,landing);game.update(FixedStep,{});
        require(game.chests==1 && game.owned.front().rank==2 && game.chestRewards.size()==1,"Chest failed to award its upgrade.");
        const auto reward=game.chestRewards.front();require(reward.serial==1 && reward.kind==ChestRewardKind::Upgrade && reward.definition==definition && reward.rank==2 && !reward.title.empty() && !reward.description.empty(),"Upgrade cannot be rendered from its reward record.");
        game.update(FixedStep,{});require(game.chests==1 && game.owned.front().rank==2 && game.chestRewards.front().serial==reward.serial,"The chest reward repeated or vanished after its event tick.");
    });
    test("real boss kill chest collection evolves the eligible weapon",[&]{
        Game game(content,903);isolate(game);const int weapon=content.find("venom_shot");game.owned={{weapon,8,false,100},{content.find("serpents_scales"),1,false,0}};
        killBoss(game,{game.player.position.x+4*Tile,game.player.position.y},2000);game.update(FixedStep,{});
        const auto landing=chest(game).position;relocate(game,landing);game.update(FixedStep,{});
        require(game.bossesKilled==1 && game.chests==1 && game.owned.front().evolved,"Boss reward failed the actual evolution path.");
        require(game.chestRewards.size()==1 && game.chestRewards.front().kind==ChestRewardKind::Evolution && game.chestRewards.front().title=="Ouroboros","Evolution reward data is missing.");
    });
    test("fast multi-cell movement collects chests crossed between endpoints",[&]{
        Catalog fast=content;UpgradeDef haste;haste.id="loot_test_haste";haste.kind=UpgradeKind::Passive;haste.maxRank=1;haste.modifiers={{Stat::Speed,1}};fast.upgrades.push_back(haste);
        Game game(fast,904);isolate(game);game.owned={{int(fast.upgrades.size()-1),1,false,0}};game.player.moveClock=.119f;game.player.hp=30;
        const Vec start=game.player.position,landing{start.x+Tile,start.y};game.pickups.push_back({landing,1,PickupKind::Chest});game.update(.1f,{{1,0}});
        require(start.x<landing.x-6 && game.player.position.x>landing.x+6,"Fixture did not cross a chest between distant endpoints.");
        require(game.chests==1 && game.pickups.empty() && game.gems==25 && game.player.hp==55,"Fast movement skipped the chest or lost its fallback reward.");
    });
    test("merged chest quantities grant every reward exactly once",[&]{
        Game game(content,905);isolate(game);game.player.hp=10;game.pickups.push_back({game.player.position,3,PickupKind::Chest});game.update(FixedStep,{});
        require(game.chests==3 && game.gems==75 && game.player.hp==85 && game.chestRewards.size()==3,"Merged chest rewards were dropped.");
        for(std::size_t i=0;i<3;++i){const auto& reward=game.chestRewards[i];require(reward.serial==i+1 && reward.kind==ChestRewardKind::Treasure && reward.gold==25 && reward.healing==25,"Merged reward order or actual reward amounts are incorrect.");}
        game.update(FixedStep,{});require(game.chests==3 && game.gems==75,"Merged chest rewarded twice.");
    });
    test("simultaneous chests reveal evolution then subsequent build upgrade",[&]{
        Game game(content,906);isolate(game);const int passive=content.find("serpents_scales");game.owned={{content.find("venom_shot"),8,false,100},{passive,1,false,0}};
        game.pickups.push_back({game.player.position,2,PickupKind::Chest});game.update(FixedStep,{});
        require(game.chests==2 && game.chestRewards.size()==2,"Simultaneous chest reward records were overwritten.");
        require(game.chestRewards[0].kind==ChestRewardKind::Evolution && game.chestRewards[1].kind==ChestRewardKind::Upgrade && game.chestRewards[1].definition==passive && game.owned[1].rank==2,"Evolution or following rank reward was applied incorrectly.");
    });
    test("pause freezes collection and a new run clears reward presentation state",[&]{
        Game game(content,907);isolate(game);game.pickups.push_back({game.player.position,1,PickupKind::Chest});game.togglePause();game.update(.1f,{});
        require(game.chests==0 && game.chestRewards.empty() && game.pickups.size()==1,"Paused chest state advanced.");game.togglePause();game.update(FixedStep,{});require(game.chestRewards.size()==1,"Unpaused chest did not open.");
        game.reset(0,908);require(game.chestRewards.empty() && game.chests==0,"New run retained the previous loot reveal.");
    });
    test("bounded reward history preserves all granted gold and serial order",[&]{
        Game game(content,909);isolate(game);game.pickups.push_back({game.player.position,100,PickupKind::Chest});game.update(FixedStep,{});
        require(game.chests==100 && game.gems==2500 && game.chestRewards.size()==32,"Bounded history lost actual chest rewards.");
        require(game.chestRewards.front().serial==69 && game.chestRewards.back().serial==100,"Reward history did not retain the most recent ordered records.");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
