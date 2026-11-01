#include "core.hpp"
#include "render_motion.hpp"
#include "snake_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using namespace snake;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void isolate(Game& game,const Catalog& content){
    game.enemies.clear();game.pickups.clear();game.owned={{content.find("graviton"),8,false,0}};
    game.spawnClock=-1000;game.bossClock=100;game.player.moveClock=-1000;game.player.invulnerable=1000;game.greySnakes.enabled=false;
}
Enemy target(std::uint32_t id,Vec position){
    Enemy enemy;enemy.id=id;enemy.position=enemy.previous=position;enemy.trail.fill(position);
    enemy.speed=0;enemy.hp=enemy.maxHp=100000;enemy.trailSize=0;return enemy;
}
geometry::Point head(const render::SnakePose<9>& pose){
    const auto shape=geometry::buildSnake(pose.previous.data(),pose.current.data(),pose.count,pose.alpha,11,2.5f);
    require(shape.valid&&shape.count>0,"Pinned enemy pose became invalid.");return shape.centers[0];
}
}
int main(){
    int passed=0,failed=0;
    const auto content=Catalog::load(SNAKE_TEST_DATA_DIR);
    auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    test("stasis retains fractional movement through capture, hold, and release",[&]{
        Game game(content,1701);isolate(game,content);
        auto enemy=target(1000,{832,800});enemy.motionReady=true;enemy.direction={-1,0};enemy.previous={840,800};
        enemy.speed=Tile/.12f;enemy.moveInterval=.12f;enemy.moveClock=.045f;game.enemies.push_back(enemy);game.graviton.turns=4;
        render::MotionHistory history;history.beginStep(game);
        const auto before=head(history.sampleEnemy(game.enemies[0],1,true));
        game.update(FixedStep,{});
        for(float alpha:{0.0f,.25f,.5f,.75f,1.0f}){
            const auto frozen=head(history.sampleEnemy(game.enemies[0],alpha,true));
            require(frozen.x==before.x&&frozen.y==before.y,"Pinning snapped a partly completed movement step.");
        }
        for(int tick=0;tick<70;++tick){history.beginStep(game);game.update(FixedStep,{});}
        require(game.enemies[0].position.x==832&&std::abs(game.enemies[0].moveClock-.045f)<.00001f,"The lock advanced or discarded movement phase.");
        for(int tick=0;tick<25;++tick){
            history.beginStep(game);const auto from=head(history.sampleEnemy(game.enemies[0],1,true));game.update(FixedStep,{});
            auto previous=from;
            for(int frame=0;frame<=4;++frame){const auto point=head(history.sampleEnemy(game.enemies[0],float(frame)/4,true));require(std::abs(point.x-previous.x)<=2&&point.y==previous.y,"Release snapped or ran an overdue movement burst.");previous=point;}
        }
        require(game.enemies[0].gravityHold==0&&game.enemies[0].position.x<832,"The pinned enemy never resumed moving.");
    });
    test("one cast handles the entire bounded enemy pool without allocations",[&]{
        Game game(content,1702);isolate(game,content);game.graviton.turns=4;
        for(int i=0;i<1800;++i)game.enemies.push_back(target(std::uint32_t(i+1),{800+float((i%12-6)*Tile),800+float((i/12%12-6)*Tile)}));
        const auto enemyCapacity=game.enemies.capacity(),pickupCapacity=game.pickups.capacity(),projectileCapacity=game.projectiles.capacity();
        game.update(FixedStep,{});
        require(game.graviton.serial==1&&std::all_of(game.enemies.begin(),game.enemies.end(),[](const Enemy& enemy){return enemy.gravityHold>0;}),"A dense cast skipped live targets.");
        require(game.enemies.size()==1800&&game.pickups.empty(),"Gridlock spawned actors or pickups.");
        require(game.enemies.capacity()==enemyCapacity&&game.pickups.capacity()==pickupCapacity&&game.projectiles.capacity()==projectileCapacity,"The first gravity cast allocated actor storage.");
    });
    test("casts do not consume simulation RNG",[&]{
        Game cast(content,1703),idle(content,1703);isolate(cast,content);isolate(idle,content);
        cast.enemies.push_back(target(1000,{832,800}));idle.enemies=cast.enemies;cast.graviton.turns=4;
        cast.update(FixedStep,{});idle.update(FixedStep,{});cast.grantXp(8);idle.grantXp(8);
        require(cast.offers==idle.offers,"The gravity effect consumed gameplay RNG.");
    });
    test("grapples retain ownership and empty casts preserve the charge",[&]{
        Game game(content,1704);isolate(game,content);auto grabbed=target(1000,{832,800});grabbed.grabbed=true;
        game.enemies.push_back(grabbed);game.graviton.turns=4;game.update(FixedStep,{});
        require(game.graviton.serial==0&&game.graviton.turns==4,"An empty cast consumed the charge or grabbed a grapple's target.");
        game.enemies.push_back(target(1001,{824,800}));game.update(FixedStep,{});
        require(game.graviton.serial==1&&game.enemies[0].gravityHold==0&&game.enemies[1].gravityHold>0,"The field stole a target from an active grapple.");
    });
    test("banked turns respect recharge and selection freezes the field",[&]{
        Game game(content,1705);isolate(game,content);game.enemies.push_back(target(1000,{832,800}));game.graviton.turns=4;game.update(FixedStep,{});
        game.graviton.turns=4;
        const auto lock=game.graviton;game.phase=Phase::LevelUp;game.update(.1f,{});
        require(game.graviton.age==lock.age&&game.graviton.cooldown==lock.cooldown,"Selection advanced the gravity field.");
        game.phase=Phase::Playing;for(int tick=0;tick<240;++tick)game.update(FixedStep,{});
        require(game.graviton.serial==1&&game.graviton.turns==4,"Banked turns bypassed the recharge.");
        for(int tick=0;tick<62;++tick)game.update(FixedStep,{});
        require(game.graviton.serial==2&&game.graviton.turns==0,"A banked charge failed to activate after recharge.");
    });
    test("evolution inherits the pin even when the existing zone pool is full",[&]{
        Game game(content,1706);isolate(game,content);game.graviton.turns=4;
        game.owned.push_back({content.find("plague_mortar"),8,true,1000});game.enemies.push_back(target(1000,{832,800}));
        for(int i=0;i<128;++i){Zone zone;zone.damage=0;zone.life=zone.maxLife=1000;zone.pattern=Pattern::Shot;game.zones.push_back(zone);}
        const auto capacity=game.zones.capacity();game.update(FixedStep,{});
        require(game.graviton.serial==1&&game.enemies[0].gravityHold>0,"Zone saturation discarded the inherited turn trap.");
        require(!game.graviton.empowered&&game.zones.size()==128&&game.zones.capacity()==capacity,"The inherited well exceeded bounded zone storage.");
    });
    test("a cold empowered cast preserves all kills without growing combat pools",[&]{
        Game game(content,1707);isolate(game,content);game.graviton.turns=4;
        game.owned.push_back({content.find("plague_mortar"),8,true,1000});
        for(int i=0;i<1800;++i){auto enemy=target(std::uint32_t(i+1),{824,800});enemy.hp=enemy.maxHp=1;game.enemies.push_back(enemy);}
        const auto events=game.events.capacity(),pickups=game.pickups.capacity(),deaths=game.snakeDeaths.capacity(),zones=game.zones.capacity();
        game.update(FixedStep,{});
        require(game.kills==1800&&game.enemies.empty(),"The cold gravity cast did not defeat its full target pool.");
        require(std::count_if(game.events.begin(),game.events.end(),[](const Event& event){return event.type==Event::Kill;})==1800,"The cold cast discarded kill events.");
        require(game.events.capacity()==events&&game.pickups.capacity()==pickups&&game.snakeDeaths.capacity()==deaths&&game.zones.capacity()==zones,"The cold empowered cast grew bounded combat storage.");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
