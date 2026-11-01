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
Game fixture(const Catalog& catalog){
    Game game(catalog,1942);game.enemies.clear();game.pickups.clear();game.owned.clear();
    game.spawnClock=-10000;game.bossClock=100;game.player.moveClock=-10000;game.player.invulnerable=10000;
    game.greySnakes.enabled=false;game.flood.enabled=false;return game;
}
void wall(Player& player){
    player.segments.clear();
    for(int y=800;y<=880;y+=Tile)player.segments.push_back({800,float(y)});
    player.segments.push_back({808,880});
    for(int y=880;y>=720;y-=Tile)player.segments.push_back({816,float(y)});
    player.previousSegments=player.segments;player.length=int(player.segments.size());
}
Enemy enemyAt(Vec p){Enemy e;e.id=1000;e.position=e.previous=p;e.trail.fill(p);e.speed=0;e.hp=e.maxHp=1e8;e.motionReady=true;return e;}
}
int main(){
    int passed=0,failed=0;const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    test("warning is harmless, then head contact bypasses HP, immunity and shields",[&]{
        auto game=fixture(catalog);game.owned={{catalog.find("serpents_scales"),8,false,0}};
        require(game.spawnFlood(game.player.position,{1,0},1,.1f),"Flood did not spawn");
        for(int i=0;i<5;++i)game.update(FixedStep,{});
        require(game.phase==Phase::Playing && game.player.hp==100,"Warning caused damage");
        for(int i=0;i<3;++i)game.update(FixedStep,{});
        require(game.phase==Phase::Dead && game.player.hp==0 && game.playerDied,"Head contact was not immediately lethal");
        require(game.shieldCharges==3 && game.notice=="THE FLOOD CLAIMED YOUR HEAD","A shield absorbed the flood or death reason was overwritten");
    });
    test("snake side and tail contact rebound without head damage",[&]{
        auto game=fixture(catalog);game.spawnFlood({776,824},{0,-1},1,0);
        bool bounced=false;
        for(int i=0;i<90 && game.phase==Phase::Playing;++i){game.update(FixedStep,{});const auto& p=game.flood.particles[0];if(p.foam>0 && p.velocity.y>0){bounced=true;break;}}
        require(bounced,"Water crossed a body cell instead of reflecting");require(game.phase==Phase::Playing && game.player.hp==100,"Side contact hurt the head");
    });
    test("a square head corner touch and a swept head crossing are lethal",[&]{
        auto corner=fixture(catalog);corner.spawnFlood({804.9f,804.9f},{1,0},1,0);corner.update(FixedStep,{});
        require(corner.phase==Phase::Dead,"Water touched a head corner without ending the run");
        auto crossing=fixture(catalog);crossing.spawnFlood({804,805.5f},{0,1},1,0);crossing.player.moveClock=crossing.player.moveInterval;
        crossing.update(FixedStep,{});require(crossing.phase==Phase::Dead,"The moving head skipped through a narrow water edge");
    });
    test("all enemy body segments and permanent grey barriers deflect water",[&]{
        for(int kind=0;kind<5;++kind){
            auto game=fixture(catalog);
            if(kind<4){auto enemy=enemyAt({864,800});enemy.kind=EnemyKind(kind);enemy.trailSize=3;enemy.trail={Vec{856,800},Vec{848,800},Vec{840,800}};enemy.previousTrail=enemy.trail;game.enemies.push_back(enemy);}
            else{GreySnake trail;trail.id=1;game.greySnakes.trails.push_back(trail);game.greySnakes.append(game.greySnakes.trails.back(),{840,800});}
            game.spawnFlood({840,824},{0,-1},1,0);bool bounced=false;
            for(int i=0;i<90;++i){game.update(FixedStep,{});const auto& p=game.flood.particles[0];if(p.foam>0 && p.velocity.y>0){bounced=true;break;}}
            require(bounced,"Water missed an enemy tail or grey barrier");
            require(game.phase==Phase::Playing && game.kills==0,"Water killed an enemy or damaged the player");
        }
    });
    test("fast shots hit the actual surface before an enemy behind it",[&]{
        auto game=fixture(catalog);game.spawnFlood({824,800},{-1,0},1,0);
        auto enemy=enemyAt({856,800});enemy.trailSize=0;game.enemies.push_back(enemy);
        Projectile shot;shot.position={800,800};shot.velocity={6000,0};shot.radius=1;shot.damage=1000;game.projectiles.push_back(shot);
        game.update(FixedStep,{});
        require(game.flood.particles[0].velocity.x>20 && game.flood.repulsions==1,"Fast shot tunneled through the flood");
        require(game.projectiles.empty() && game.enemies.back().hp==1e8,"A stopped shot hit an enemy behind water");
        require(game.kills==0 && game.pickups.empty() && game.snakeDeaths.empty(),"Water generated kills, loot or a snake corpse");
    });
    test("an earlier enemy impact prevents an occluded water impulse",[&]{
        auto game=fixture(catalog);game.spawnFlood({840,800},{-1,0},1,0);auto enemy=enemyAt({816,800});enemy.trailSize=0;game.enemies.push_back(enemy);
        Projectile shot;shot.position={800,800};shot.velocity={6000,0};shot.radius=1;shot.damage=10;game.projectiles.push_back(shot);game.update(FixedStep,{});
        require(game.flood.repulsions==0 && game.enemies.back().hp==1e8-10,"Water was hit through an earlier blocking enemy");
    });
    for(const char* id:{"venom_shot","viper_fangs","plague_mortar","snake_nest","venom_nova","sidewinder_beam","ricochet_fang","cobra_pit","tongue_lash"})for(bool evolved:{false,true}){
        const auto name=std::string(id)+(evolved?" evolution":" base")+" repels water without devouring it";
        test(name.c_str(),[&]{
            auto game=fixture(catalog);wall(game.player);game.owned={{catalog.find(id),8,evolved,0}};game.spawnFlood({856,800},{-1,0},48,0);
            for(int i=0;i<240 && game.flood.repulsions==0 && game.phase==Phase::Playing;++i)game.update(FixedStep,{});
            require(game.flood.repulsions>0,"Equipped weapon never pushed any water");
            require(game.kills==0 && game.pickups.empty() && game.flood.active(),"Water was devoured, defeated or converted to loot");
            for(const auto& e:game.enemies)if(e.kind==EnemyKind::Flood)require(!e.grabbed && !e.devoured,"A grapple captured water");
        });
    }
    test("a corner-compressed maximum flood stays finite and reuses its storage",[&]{
        auto game=fixture(catalog);game.spawnFlood({Tile,Tile},{-1,0},int(FloodCapacity),0);const auto* data=game.flood.particles.data();
        for(int i=0;i<240;++i){
            game.update(FixedStep,{});require(game.flood.particles.data()==data && game.flood.particles.size()==FloodCapacity,"Flood storage grew or lost volume");
            for(const auto& p:game.flood.particles)require(std::isfinite(p.position.x) && std::isfinite(p.velocity.y) && p.position.x>=Tile+FloodRadius && p.position.y>=Tile+FloodRadius && p.position.x<=ArenaSize-Tile-FloodRadius && p.position.y<=ArenaSize-Tile-FloodRadius,"Compressed flood escaped its numerical/arena bounds");
        }
    });
    test("retirement removes its target and reset preserves reserved storage",[&]{
        auto game=fixture(catalog);game.spawnFlood({1500,1500},{1,0},1,0);const auto* data=game.flood.particles.data();
        game.flood.particles[0].age=FloodLifetime-.1f;
        for(int i=0;i<90;++i)game.update(FixedStep,{});
        require(game.phase==Phase::Playing && !game.flood.active() && game.enemies.empty(),"Expired flood left an immortal target or killed a distant player");
        require(game.kills==0 && game.snakeDeaths.empty() && game.pickups.empty(),"Retirement emitted loot/death art");
        game.reset();require(game.flood.particles.data()==data && game.flood.serial==0 && game.flood.enabled,"Reset allocated storage or retained old water");
    });
    test("schedule and pause use simulation time and water does not consume combat RNG",[&]{
        require(FloodFirstSeconds==60 && FloodIntervalSeconds==60,"Floods must arrive at each full minute");
        auto wet=fixture(catalog),dry=fixture(catalog);wet.flood.enabled=true;wet.flood.clock=FloodFirstSeconds-.001;
        wet.update(FixedStep,{});dry.update(FixedStep,{});require(wet.flood.active() && wet.flood.serial==1,"Director did not spawn a flood");
        const auto clock=wet.flood.clock;const float age=wet.flood.age;wet.togglePause();for(int i=0;i<100;++i)wet.update(FixedStep,{});
        require(wet.flood.clock==clock && wet.flood.age==age,"Pause advanced water");wet.togglePause();
        wet.grantXp(8);dry.grantXp(8);require(wet.offers==dry.offers,"Water consumed combat RNG");
    });
    test("minute arrivals append full waves without restarting existing water",[&]{
        auto game=fixture(catalog);game.flood.enabled=true;game.flood.clock=59.99;
        game.update(FixedStep,{});const auto handle=game.flood.enemyId;const auto* data=game.flood.particles.data();
        require(game.flood.serial==1 && game.flood.nextSpawn==120,"The first minute arrival was late or shifted the next minute");
        for(auto& p:game.flood.particles)p.age=60;
        game.flood.clock=119.99;game.update(FixedStep,{});
        require(game.flood.serial==2 && game.flood.nextSpawn==180 && game.flood.waveCount()==2 && game.flood.particles.size()==1280,"The second minute skipped an active wave");
        require(game.enemies.size()==1 && game.flood.enemyId==handle && game.flood.particles.data()==data,"Joining water allocated or duplicated its target");
        require(game.flood.particles.front().age>60 && game.flood.particles.back().age<FixedStep*2 && game.flood.dangerous(),"Arrival restarted old water or its warning protected the old wave");
    });
    test("joining warnings remain harmless while older water keeps moving and stays lethal",[&]{
        auto game=fixture(catalog);game.spawnFlood({920,800},{1,0},1,0);for(int i=0;i<15;++i)game.update(FixedStep,{});
        const auto old=game.flood.particles[0];const auto handle=game.flood.enemyId;
        require(game.spawnFlood({1000,800},{-1,0},1,.2f),"A second wave could not join");
        require(game.flood.particles[0].age==old.age && game.flood.particles[0].position.x==old.position.x && game.flood.enemyId==handle,"Joining changed existing water immediately");
        const auto warned=game.flood.particles.back().position;
        require(game.flood.contact(warned,warned,1)>1,"A new warning intercepted a shot");
        game.flood.repel(warned,12,{1,0},100);require(game.flood.repulsions==0,"A shot repelled telegraphing water");
        for(int i=0;i<5;++i)game.update(FixedStep,{});
        require(game.flood.particles[0].position.x!=old.position.x && game.flood.particles[1].position.x==warned.x && game.flood.particles[1].position.y==warned.y,"The warning froze old water or moved new water");
        game.flood.particles[0].position=game.player.position;game.flood.particles[0].velocity={};game.update(FixedStep,{});
        require(game.phase==Phase::Dead,"An incoming wave made older water harmless");
        auto harmless=fixture(catalog);harmless.spawnFlood({1200,1200},{1,0},1,0);harmless.spawnFlood(harmless.player.position,{1,0},1,.1f);
        for(int i=0;i<5;++i)harmless.update(FixedStep,{});require(harmless.phase==Phase::Playing,"Joining water killed during its own warning");
        for(int i=0;i<3;++i)harmless.update(FixedStep,{});require(harmless.phase==Phase::Dead,"Joining water stayed harmless after its warning");
    });
    test("joined waves share pressure and weapon impulses",[&]{
        auto game=fixture(catalog);game.spawnFlood({920,800},{1,0},1,0);game.spawnFlood({920,800},{-1,0},1,0);
        for(auto& p:game.flood.particles)p.velocity={};game.update(FixedStep,{});
        const auto a=game.flood.particles[0].position,b=game.flood.particles[1].position;
        require((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)>.001f,"Separate waves passed through one another without shared liquid pressure");
        game.flood.repel({920,800},16,{-1,0},100);
        require(game.flood.repulsions==1 && game.flood.particles[0].velocity.x<0 && game.flood.particles[1].velocity.x<0 && game.enemies.size()==1,"A merged water surface did not share an impulse or its targeting handle");
    });
    test("older waves drain independently without retiring a younger joined wave",[&]{
        auto game=fixture(catalog);game.spawnFlood({1500,1500},{1,0},16,0);
        for(auto& p:game.flood.particles)p.age=FloodLifetime-.1f;
        const auto handle=game.flood.enemyId;game.spawnFlood({1500,1400},{1,0},16,0);
        for(int i=0;i<90;++i)game.update(FixedStep,{});
        require(game.flood.particles.size()==16 && game.flood.waveCount()==1 && game.flood.dangerous() && game.flood.enemyId==handle && game.enemies.size()==1,"An old wave cleared, drained or untargeted new water");
        require(game.flood.particles.front().wave==2 && game.flood.particles.front().age<2,"Retirement retained old samples or changed the younger lifetime");
        const auto copy=game;require(copy.flood.dangerous() && copy.flood.waveCount()==1,"Copying water lost independent danger/lifetime state");
    });
    test("joined water is capped and rejects excess volume without allocating or resetting",[&]{
        auto game=fixture(catalog);const auto* data=game.flood.particles.data();
        require(game.spawnFlood({1200,1200},{1,0},int(FloodCapacity/2),0) && game.spawnFlood({1400,1400},{-1,0},int(FloodCapacity/2),0),"The reserved pool could not hold two full waves");
        const auto first=game.flood.particles.front();const auto serial=game.flood.serial;
        require(!game.spawnFlood({1300,1300},{1,0},1,0),"Joined water exceeded its cap");
        require(game.flood.particles.data()==data && game.flood.particles.size()==FloodCapacity && game.flood.serial==serial && game.flood.particles.front().position.x==first.position.x && game.enemies.size()==1,"Rejected volume changed existing water or target storage");
    });
    test("a full enemy pool admits one flood without allocating or evicting a boss",[&]{
        auto game=fixture(catalog);
        for(int i=0;i<1800;++i){auto e=enemyAt({864,864});e.id=std::uint32_t(10000+i);e.trailSize=0;e.kind=i==0?EnemyKind::Boss:EnemyKind::Red;game.enemies.push_back(e);}
        const auto* data=game.enemies.data();game.flood.enabled=true;game.flood.clock=FloodFirstSeconds-.001;
        game.update(FixedStep,{});
        require(game.enemies.size()==1800 && game.enemies.data()==data && game.flood.active(),"Full-capacity arrival grew or overfilled enemy storage");
        require(std::any_of(game.enemies.begin(),game.enemies.end(),[](const Enemy& e){return e.id==10000 && e.kind==EnemyKind::Boss;}),"Flood arrival evicted the boss");
        const auto targets=game.enemies;const auto handle=game.flood.enemyId;
        game.flood.clock=119.99;game.update(FixedStep,{});
        require(game.enemies.size()==1800 && game.enemies.data()==data && game.flood.enemyId==handle && game.flood.particles.size()==1280 && std::equal(targets.begin(),targets.end(),game.enemies.begin(),[](const Enemy& a,const Enemy& b){return a.id==b.id;}),"A joined wave evicted another enemy or allocated at the enemy cap");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
