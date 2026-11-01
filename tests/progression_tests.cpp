#include "core.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace snake;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Game fixture(const Catalog& catalog){
    Game game(catalog,0xFACE);game.enemies.clear();game.pickups.clear();game.owned.clear();
    game.greySnakes.enabled=false;game.flood.enabled=false;
    game.spawnClock=-1000000;game.bossClock=1000000;
    game.player.moveClock=-1000000;game.player.invulnerable=1000000;return game;
}
void fruit(Game& game,int value){game.pickups.push_back({game.player.position,value,PickupKind::Experience});game.update(FixedStep,{});}
}
int main(){
    int failed=0,passed=0;
    const auto test=[&](const char* name,const std::function<void()>& run){
        try{run();++passed;std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}
    };
    try{
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
        test("documented XP costs include both transition walls",[&]{
            const int levels[]{1,2,3,19,20,21,39,40,41,80};
            const int costs[]{5,15,25,185,795,267,501,2914,648,1272};
            for(std::size_t i=0;i<std::size(levels);++i)require(experienceForLevel(levels[i])==costs[i],"Reference XP cost mismatch");
            require(experienceForLevel(std::numeric_limits<int>::max())>0,"Large level overflowed XP cost");
            auto game=fixture(catalog);game.grantXp(20);
            require(game.player.level==2 && game.player.xp==15 && game.phase==Phase::LevelUp,"First level did not retain overflow");
            const float time=game.elapsed;game.update(FixedStep,{});
            require(game.elapsed==time && game.choose(0),"Selection advanced combat or could not resolve");
            require(game.player.level==3 && game.player.xp==0 && game.player.nextXp==25,"Queued XP was lost or double counted");
        });
        test("small Growth bonuses conserve fractional XP and raw body growth",[&]{
            auto separate=fixture(catalog),merged=fixture(catalog);
            for(auto* game:{&separate,&merged}){
                game->owned={{catalog.find("ravenous_maw"),1,false,0}};game->player.nextXp=1000000;
            }
            for(int i=0;i<4;++i)fruit(separate,1);
            fruit(merged,4);
            require(separate.player.xp==5 && merged.player.xp==5,"One-point fruit discarded fractional Growth");
            require(separate.player.length==11 && merged.player.length==11,"XP multiplier changed body growth");
            require(std::abs(separate.player.xpRemainder)<1e-8 && separate.experienceCollected==4,"Fraction/raw collection ledger mismatch");
            separate.reset(0,123);require(separate.player.xpRemainder==0 && separate.experienceCollected==0,"Reset retained prior-run XP");
        });
        test("level 20 and 40 Growth help only their own transition",[&]{
            for(int level:{20,40}){
                auto game=fixture(catalog);game.player.level=level;game.player.nextXp=experienceForLevel(level);
                require(game.stats().growth==2,"Transition Growth was absent");
                fruit(game,level==20?398:1457);
                require(game.player.level==level+1 && game.stats().growth==1,"Transition Growth leaked into next level");
                require(game.player.xp==(level==20?1:0),"Transition overflow mismatch");
            }
            auto game=fixture(catalog);game.player.level=20;game.player.xp=794;game.player.nextXp=795;
            game.pickups={{game.player.position,1,PickupKind::Experience},{game.player.position,1,PickupKind::Experience}};
            game.update(FixedStep,{});
            require(game.player.level==21 && game.player.xp==2,"Same-tick pickups retained an expired breakpoint bonus");
        });
        test("all 30 wave rows are bounded and include recovery waves",[&]{
            require(stageMinute(-1)==0 && stageMinute(RunDuration)==29,"Stage lookup escaped table");
            for(int minute=0;minute<StageMinutes;++minute){
                const auto& wave=stageWave(float(minute)*60);
                require(wave.minimum>0 && wave.minimum<=OrdinaryEnemyLimit && wave.interval>=.1f,"Invalid wave population/cadence");
                require(wave.weights[0]+wave.weights[1]+wave.weights[2]==100,"Wave weights are not normalized");
            }
            require(stageWave(660).minimum==300 && stageWave(660).health<stageWave(600).health,"11-minute harvest wave missing");
            require(stageWave(1260).minimum==300 && stageWave(1260).health<stageWave(1200).health,"21-minute harvest wave missing");
        });
        test("population refills spread across fixed updates and stop at 300",[&]{
            auto game=fixture(catalog);game.elapsed=660;game.spawnClock=0;
            const auto* storage=game.enemies.data();
            game.update(FixedStep,{});
            require(game.enemies.size()==DirectorSpawnBudget,"Minute transition exceeded or missed spawn budget");
            game.togglePause();const auto time=game.elapsed;game.update(.1f,{});
            require(game.enemies.size()==DirectorSpawnBudget && game.elapsed==time,"Pause advanced pending refill");game.togglePause();
            for(int tick=0;tick<120;++tick){
                const auto before=game.enemies.size();game.update(FixedStep,{});
                require(game.enemies.size()-before<=DirectorSpawnBudget,"Refill burst exceeded budget");
                require(game.enemies.data()==storage,"Director reallocated live storage");
            }
            require(game.enemies.size()==OrdinaryEnemyLimit,"Periodic spawning exceeded or failed to reach quota");
            game.enemies.clear();game.spawnClock=0;game.update(.1f,{});game.spawnClock=-1000000;game.update(FixedStep,{});
            require(game.enemies.size()==DirectorSpawnBudget,"Suppression retained a queued refill");
        });
        test("HP scaling is selective and fixed for a living enemy",[&]{
            require(stageEnemy(EnemyKind::Red,300,1).health==stageEnemy(EnemyKind::Red,300,80).health,"Ordinary enemies scale with every level");
            require(stageEnemy(EnemyKind::Red,1740,20).health*2==stageEnemy(EnemyKind::Red,1740,40).health,"Late elites lost level scaling");
            auto game=fixture(catalog);game.elapsed=59.99f;game.bossClock=0;game.player.level=7;game.update(FixedStep,{});
            require(game.enemies.size()==1 && game.enemies.front().kind==EnemyKind::Boss && game.enemies.front().maxHp==700,"First boss did not use its spawn level");
            game.player.level=80;game.update(FixedStep,{});
            require(game.enemies.front().hp==700 && game.enemies.front().maxHp==700,"Living boss was rescaled after leveling");
        });
        test("damage is earned through equipment and DPS excludes overkill",[&]{
            auto game=fixture(catalog);game.owned={{catalog.find("venom_shot"),4,false,0}};
            const auto initial=game.weaponSpec(game.owned.front());game.player.level=80;
            require(game.weaponSpec(game.owned.front()).damage==initial.damage && game.weaponSpec(game.owned.front()).cooldown==initial.cooldown,"Levels silently changed weapon damage/cadence");
            game.owned.clear();Enemy enemy;enemy.id=10000;enemy.position={864,800};enemy.trail.fill(enemy.position);
            enemy.hp=enemy.maxHp=10;enemy.speed=0;enemy.experience=5;game.enemies.push_back(enemy);
            Projectile shot;shot.position=enemy.position;shot.damage=100;shot.life=1;game.projectiles.push_back(shot);
            game.update(FixedStep,{});
            require(game.kills==1 && game.damageDealt==10 && game.experienceDropped==5,"Effective DPS or XP ledger counted overkill/wrong reward");
        });
        test("minute boundaries and victory use exactly 108000 fixed updates",[&]{
            auto game=fixture(catalog);game.bossClock=0;
            for(int tick=1;tick<=StageMinutes*3600;++tick){
                game.update(FixedStep,{});
                if(tick==3599)require(game.bossClock==0 && stageMinute(game.elapsed)==0,"Wave/boss arrived early");
                if(tick==3600)require(game.bossClock==1 && stageMinute(game.elapsed)==1,"First minute missed its exact tick");
                if(tick<StageMinutes*3600)require(game.phase==Phase::Playing,"Stage ended before 30 simulation minutes");
            }
            require(game.phase==Phase::Victory && game.elapsed==RunDuration && game.bossClock==BossWaveCount,"Stage missed victory or boss schedule");
            require(game.enemies.size()==BossWaveCount,"Scheduled boss was lost or repeated");
        });
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
