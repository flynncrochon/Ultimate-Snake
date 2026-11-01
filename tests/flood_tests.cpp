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
    test("warning is harmless, then head contact deals ten health damage",[&]{
        auto game=fixture(catalog);game.player.invulnerable=0;
        require(game.spawnFlood(game.player.position,{1,0},1,.1f),"Flood did not spawn");
        for(int i=0;i<5;++i)game.update(FixedStep,{});
        require(game.phase==Phase::Playing && game.player.hp==100,"Warning caused damage");
        for(int i=0;i<3;++i)game.update(FixedStep,{});
        require(game.phase==Phase::Playing && game.player.hp==90 && !game.playerDied,"Head contact did not deal a survivable ten-health hit");
    });
    test("water respects immunity and consumes one shield charge per protected contact",[&]{
        auto immune=fixture(catalog);immune.spawnFlood(immune.player.position,{1,0},1,0);immune.update(FixedStep,{});
        require(immune.player.hp==100 && immune.phase==Phase::Playing,"Water bypassed existing contact immunity");
        auto shielded=fixture(catalog);shielded.player.invulnerable=0;shielded.owned={{catalog.find("serpents_scales"),8,false,0}};
        shielded.spawnFlood(shielded.player.position,{1,0},32,0);shielded.update(FixedStep,{});
        require(shielded.shieldCharges==2 && shielded.player.hp==100 && shielded.player.invulnerable>0 && shielded.shieldTimer>0,"Water bypassed a shield or drained multiple charges in one tick");
        shielded.update(FixedStep,{});
        require(shielded.shieldCharges==2 && shielded.player.hp==100,"Water ignored shield contact immunity");
    });
    test("joined water shares a one-second damage cooldown with ordinary contact",[&]{
        auto game=fixture(catalog);game.player.invulnerable=0;
        game.spawnFlood(game.player.position,{1,0},1,0);game.spawnFlood(game.player.position,{-1,0},1,0);
        auto enemy=enemyAt(game.player.position);enemy.trailSize=0;game.enemies.push_back(enemy);
        auto touch=[&]{for(auto& p:game.flood.particles){p.position=game.player.position;p.velocity={};}game.update(FixedStep,{});};
        touch();
        require(game.player.hp==90 && std::count_if(game.events.begin(),game.events.end(),[](const Event& e){return e.type==Event::Hurt;})==1,"Joined samples or enemy contact multiplied a water hit");
        for(int tick=0;tick<59;++tick)touch();
        require(game.player.hp==90,"Sustained water contact damaged the player during the one-second cooldown");
        touch();touch();
        require(game.player.hp==80 && game.phase==Phase::Playing,"Water did not resume bounded damage after immunity expired");
    });
    test("armor reduces water damage and depleted health retains the flood death reason",[&]{
        auto armored=fixture(catalog);PermanentUpgrades upgrades;upgrades.ranks[std::size_t(PermanentUpgrade::Armor)]=3;
        armored.reset(0,1942,upgrades);armored.enemies.clear();armored.owned.clear();armored.player.moveClock=-10000;
        armored.greySnakes.enabled=armored.flood.enabled=false;armored.player.invulnerable=0;
        armored.spawnFlood(armored.player.position,{1,0},1,0);armored.update(FixedStep,{});
        require(armored.player.hp==93,"Armor did not reduce the ten-health water hit");
        auto dying=fixture(catalog);dying.player.invulnerable=0;dying.player.hp=7;
        dying.spawnFlood(dying.player.position,{1,0},1,0);dying.update(FixedStep,{});
        require(dying.player.hp==0 && dying.phase==Phase::Dead && dying.playerDied && dying.notice=="THE FLOOD CLAIMED YOUR HEAD","A fatal water hit underflowed health or lost its death reason");
    });
    test("snake side and tail contact rebound without head damage",[&]{
        auto game=fixture(catalog);game.spawnFlood({776,824},{0,-1},1,0);
        bool bounced=false;
        for(int i=0;i<90 && game.phase==Phase::Playing;++i){game.update(FixedStep,{});const auto& p=game.flood.particles[0];if(p.foam>0 && p.velocity.y>0){bounced=true;break;}}
        require(bounced,"Water crossed a body cell instead of reflecting");require(game.phase==Phase::Playing && game.player.hp==100,"Side contact hurt the head");
    });
    test("square head corners and swept crossings take damage while dry corners remain safe",[&]{
        auto corner=fixture(catalog);corner.player.invulnerable=0;corner.spawnFlood({804.9f,804.9f},{1,0},1,0);corner.update(FixedStep,{});
        require(corner.player.hp==90 && corner.phase==Phase::Playing,"A head corner touch missed its nonfatal damage");
        auto dry=fixture(catalog);dry.player.invulnerable=0;dry.spawnFlood({805.1f,805.1f},{1,0},1,0);dry.update(FixedStep,{});
        require(dry.player.hp==100,"Water damaged the head across a visible dry corner");
        auto crossing=fixture(catalog);crossing.player.invulnerable=0;crossing.spawnFlood({804,805.5f},{0,1},1,0);crossing.player.moveClock=crossing.player.moveInterval;
        crossing.update(FixedStep,{});require(crossing.player.hp==90 && crossing.phase==Phase::Playing,"The moving head skipped through a narrow water edge");
    });
    test("all enemy heads and bodies let water pass without changing its motion",[&]{
        for(int kind=0;kind<4;++kind)for(bool head:{false,true}){
            auto game=fixture(catalog),empty=fixture(catalog);
            auto enemy=enemyAt(head?Vec{840,800}:Vec{864,800});enemy.kind=EnemyKind(kind);enemy.trailSize=3;enemy.trail={Vec{856,800},Vec{848,800},Vec{840,800}};enemy.previousTrail=enemy.trail;game.enemies.push_back(enemy);
            game.spawnFlood({840,824},{0,-1},1,0);empty.spawnFlood({840,824},{0,-1},1,0);
            for(int i=0;i<90;++i){
                game.update(FixedStep,{});empty.update(FixedStep,{});
                const auto& p=game.flood.particles[0];const auto& q=empty.flood.particles[0];
                require(p.position.x==q.position.x && p.position.y==q.position.y && p.velocity.x==q.velocity.x && p.velocity.y==q.velocity.y && p.foam==q.foam,"An enemy head or body deflected the water");
            }
            require(game.enemies[0].hp==1e8 && game.kills==0 && game.phase==Phase::Playing,"Passing water damaged an enemy or the distant player");
        }
    });
    test("permanent grey barriers still deflect water",[&]{
        auto game=fixture(catalog);GreySnake trail;trail.id=1;trail.alive=false;game.greySnakes.trails.push_back(trail);game.greySnakes.append(game.greySnakes.trails.back(),{840,800});
        game.spawnFlood({840,824},{0,-1},1,0);bool bounced=false;
        for(int i=0;i<90;++i){game.update(FixedStep,{});const auto& p=game.flood.particles[0];if(p.foam>0 && p.velocity.y>0){bounced=true;break;}}
        require(bounced,"Water missed a grey barrier");
        require(game.phase==Phase::Playing && game.kills==0,"Water killed an enemy or damaged the player");
    });
    test("fast shots hit the actual surface before an enemy behind it",[&]{
        auto game=fixture(catalog);game.spawnFlood({824,800},{-1,0},1,0);
        auto enemy=enemyAt({856,800});enemy.trailSize=0;game.enemies.push_back(enemy);
        Projectile shot;shot.position={800,800};shot.velocity={6000,0};shot.radius=1;shot.damage=5;game.projectiles.push_back(shot);
        game.update(FixedStep,{});
        require(game.flood.particles[0].velocity.x>20 && game.flood.repulsions==1,"Fast shot tunneled through the flood");
        require(game.projectiles.empty() && game.enemies.back().hp==1e8,"A stopped shot hit an enemy behind water");
        require(game.flood.particles[0].hp==FloodHealth-5 && game.damageDealt==5,"A surface hit did not apply weapon damage");
        require(game.kills==0 && game.pickups.empty() && game.snakeDeaths.empty(),"A nonlethal water hit generated loot or a snake corpse");
    });
    test("shooting water dead drops stationary loot once and removes its target",[&]{
        auto game=fixture(catalog);game.spawnFlood({864,800},{-1,0},1,0);
        Projectile shot;shot.position={800,800};shot.velocity={6000,0};shot.radius=1;shot.damage=1000;game.projectiles.push_back(shot);
        game.update(FixedStep,{});
        require(!game.flood.active() && !game.flood.dangerous() && game.enemies.empty(),"Destroyed water retained a lethal surface or immortal target");
        require(game.kills==1 && game.damageDealt==FloodHealth && game.experienceDropped==1,"Water destruction missed its kill, damage or XP accounting");
        require(game.pickups.size()==1 && game.pickups[0].kind==PickupKind::Experience && sameCell(game.pickups[0].position,{864,800}),"Water did not drop XP at its death position");
        const auto landing=game.pickups[0].position;
        for(int i=0;i<30;++i)game.update(FixedStep,{});
        require(game.kills==1 && game.experienceDropped==1 && game.pickups.size()==1 && sameCell(game.pickups[0].position,landing) && game.snakeDeaths.empty(),"Water rewarded twice, moved loot or left a snake corpse");
    });
    test("damage is local, accumulates, and never rewards warnings, pushback or expiry",[&]{
        auto game=fixture(catalog);game.spawnFlood({920,800},{1,0},1,0);game.spawnFlood({1000,800},{1,0},1,0);game.spawnFlood({920,800},{1,0},1,2);
        require(game.flood.damage({920,800},1,7)==7 && game.flood.particles[0].hp==FloodHealth-7,"Water did not retain partial damage");
        require(game.flood.damage({920,800},1,1000)==FloodHealth-7 && game.flood.damage({920,800},1,1000)==0,"Water damage exceeded remaining HP or hit a warning/dead sample");
        game.update(FixedStep,{});
        require(game.kills==1 && game.experienceDropped==1 && game.flood.particles.size()==2 && game.flood.waveCount()==2 && game.enemies.size()==1,"Local damage erased unrelated waves or lost the shared target");
        require(game.flood.particles[0].hp==FloodHealth && game.flood.particles[1].hp==FloodHealth,"Local damage reached uncontacted water");
        game.flood.repel({1000,800},20,{1,0},100);game.update(FixedStep,{});
        require(game.kills==1 && game.experienceDropped==1,"Pushback awarded water loot");
    });
    test("zone damage reaches the real surface beyond its shared target and hits once",[&]{
        auto game=fixture(catalog);game.spawnFlood({864,800},{1,0},1,0);game.spawnFlood({1000,800},{1,0},1,0);
        Zone zone;zone.position={1000,800};zone.radius=8;zone.life=zone.maxLife=1;zone.damage=5;zone.damageFactor=1;zone.tickInterval=1;zone.pattern=Pattern::Mortar;game.zones.push_back(zone);
        game.update(FixedStep,{});
        require(game.flood.particles[0].hp==FloodHealth && game.flood.particles[1].hp==FloodHealth-5 && game.damageDealt==5,"Area damage used only the targeting handle or hit the same surface twice");
    });
    test("a maximum water clear preserves loot, event and particle storage",[&]{
        auto game=fixture(catalog);game.spawnFlood({1200,1200},{1,0},int(FloodCapacity),0);
        const auto* water=game.flood.particles.data();const auto* drops=game.pickups.data();const auto* events=game.events.data();
        Zone blast;blast.position={1200,1200};blast.radius=300;blast.life=blast.maxLife=1;blast.damage=1000;blast.damageFactor=1;blast.pattern=Pattern::Mortar;game.zones.push_back(blast);
        game.update(FixedStep,{});
        int xp=0,hearts=0;for(const auto& p:game.pickups){if(p.kind==PickupKind::Experience)xp+=p.value;else if(p.kind==PickupKind::Heart)++hearts;}
        require(!game.flood.active() && game.enemies.empty() && game.kills==int(FloodCapacity) && game.experienceDropped==FloodCapacity && xp==int(FloodCapacity),"A maximum clear lost samples or XP");
        require(hearts>0 && game.pickups.size()<=1403 && game.snakeDeaths.empty(),"Water did not use ordinary heart drops or overflowed the pickup pool");
        require(game.flood.particles.data()==water && game.pickups.data()==drops && game.events.data()==events,"Water destruction allocated combat storage");
    });
    test("simultaneous maximum enemy and water clears preserve saturated loot and event storage",[&]{
        auto game=fixture(catalog);for(int i=0;i<1400;++i)game.pickups.push_back({{1200,1200},20,PickupKind::Heart});
        for(int i=0;i<1799;++i){auto e=enemyAt({920,800});e.id=std::uint32_t(10000+i);e.hp=e.maxHp=1;game.enemies.push_back(e);}
        const auto* drops=game.pickups.data();const auto* events=game.events.data();const auto* enemies=game.enemies.data();const auto* water=game.flood.particles.data();
        game.spawnFlood({920,800},{1,0},int(FloodCapacity),0);
        Zone blast;blast.position={920,800};blast.radius=300;blast.life=blast.maxLife=1;blast.damage=1000;blast.damageFactor=1;blast.pattern=Pattern::Mortar;game.zones.push_back(blast);game.update(FixedStep,{});
        constexpr int defeated=1799+int(FloodCapacity);
        require(game.kills==defeated && game.enemies.empty() && !game.flood.active() && game.experienceDropped==defeated,"A simultaneous maximum clear lost kills or rewards");
        require(game.pickups.size()==1401 && game.pickups.data()==drops && game.pickups.back().kind==PickupKind::Experience && game.pickups.back().value==defeated,"Water loot overflowed storage or merged XP into hearts");
        require(game.events.data()==events && game.enemies.data()==enemies && game.flood.particles.data()==water,"A simultaneous clear grew event, enemy or water storage");
        require(std::count_if(game.pickups.begin(),game.pickups.end(),[](const Pickup& p){return p.kind==PickupKind::Heart && p.value==20;})==1400,"Water loot changed existing heart rewards");
    });
    test("an earlier enemy impact prevents an occluded water impulse",[&]{
        auto game=fixture(catalog);game.spawnFlood({840,800},{-1,0},1,0);auto enemy=enemyAt({816,800});enemy.trailSize=0;game.enemies.push_back(enemy);
        Projectile shot;shot.position={800,800};shot.velocity={6000,0};shot.radius=1;shot.damage=10;game.projectiles.push_back(shot);game.update(FixedStep,{});
        require(game.flood.repulsions==0 && game.enemies.back().hp==1e8-10,"Water was hit through an earlier blocking enemy");
    });
    for(const char* id:{"venom_shot","viper_fangs","plague_mortar","snake_nest","venom_nova","sidewinder_beam","ricochet_fang","cobra_pit","tongue_lash"})for(bool evolved:{false,true}){
        const auto name=std::string(id)+(evolved?" evolution":" base")+" destroys water without devouring its handle";
        test(name.c_str(),[&]{
            auto game=fixture(catalog);wall(game.player);game.owned={{catalog.find(id),8,evolved,0}};game.spawnFlood({856,800},{-1,0},48,0);
            for(int i=0;i<600 && game.kills==0 && game.phase==Phase::Playing;++i)game.update(FixedStep,{});
            require(game.flood.repulsions>0,"Equipped weapon never pushed any water");
            require(game.kills>0 && game.experienceDropped==std::uint64_t(game.kills),"Equipped weapon never destroyed water or dropped its XP");
            require(game.snakeDeaths.empty(),"Water left an ordinary snake corpse");
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
    test("minute arrivals grow and append without restarting existing water",[&]{
        auto game=fixture(catalog);game.flood.enabled=true;game.flood.clock=59.99;
        game.update(FixedStep,{});const auto handle=game.flood.enemyId;const auto* data=game.flood.particles.data();
        require(game.flood.serial==1 && game.flood.nextSpawn==120 && game.flood.particles.size()==128,"The first minute arrival was late, too large or shifted the next minute");
        for(auto& p:game.flood.particles)p.age=60;
        game.flood.clock=119.99;game.update(FixedStep,{});
        require(game.flood.serial==2 && game.flood.nextSpawn==180 && game.flood.waveCount()==2 && game.flood.particles.size()==288,"The second minute skipped an active wave or failed to grow");
        require(game.enemies.size()==1 && game.flood.enemyId==handle && game.flood.particles.data()==data,"Joining water allocated or duplicated its target");
        require(game.flood.particles.front().age>60 && game.flood.particles.back().age<FixedStep*2 && game.flood.dangerous(),"Arrival restarted old water or its warning protected the old wave");
    });
    test("water volume follows run time, keeps growing after clears and stays capped late",[&]{
        auto game=fixture(catalog);game.flood.enabled=true;
        const auto* data=game.flood.particles.data();
        // Fully retire each wave between arrivals. Destroying/clearing water
        // must not send the director back to its easy opening volume.
        for(int minute=1;minute<=30;++minute){
            for(auto& p:game.flood.particles)p.age=FloodLifetime+1;
            game.update(FixedStep,{});
            require(!game.flood.active(),"The previous wave did not retire");
            game.flood.clock=minute*60-.001;game.update(FixedStep,{});
            const std::size_t expected=minute<17?std::size_t(128+(minute-1)*32):640;
            require(game.flood.particles.size()==expected,"Natural water did not ramp from a small opening to bounded late waves");
            require(game.flood.particles.data()==data && game.flood.waveCount()==1 && !game.flood.dangerous(),"Growing arrivals allocated, duplicated a wave or lost their warning");
        }
        // A late-run schedule override must work without earlier arrivals.
        auto late=fixture(catalog);late.flood.enabled=true;late.flood.clock=1019.99;late.flood.nextSpawn=1020;
        late.update(FixedStep,{});
        require(late.flood.serial==1 && late.flood.particles.size()==640,"Volume depended on the number of surviving/previous waves");
    });
    test("joining warnings remain harmless while older water keeps moving and damaging",[&]{
        auto game=fixture(catalog);game.player.invulnerable=0;game.spawnFlood({920,800},{1,0},1,0);for(int i=0;i<15;++i)game.update(FixedStep,{});
        const auto old=game.flood.particles[0];const auto handle=game.flood.enemyId;
        require(game.spawnFlood({1000,800},{-1,0},1,.2f),"A second wave could not join");
        require(game.flood.particles[0].age==old.age && game.flood.particles[0].position.x==old.position.x && game.flood.enemyId==handle,"Joining changed existing water immediately");
        const auto warned=game.flood.particles.back().position;
        require(game.flood.contact(warned,warned,1)>1,"A new warning intercepted a shot");
        game.flood.repel(warned,12,{1,0},100);require(game.flood.repulsions==0,"A shot repelled telegraphing water");
        for(int i=0;i<5;++i)game.update(FixedStep,{});
        require(game.flood.particles[0].position.x!=old.position.x && game.flood.particles[1].position.x==warned.x && game.flood.particles[1].position.y==warned.y,"The warning froze old water or moved new water");
        game.flood.particles[0].position=game.player.position;game.flood.particles[0].velocity={};game.update(FixedStep,{});
        require(game.phase==Phase::Playing && game.player.hp==90,"An incoming wave made older water harmless");
        auto harmless=fixture(catalog);harmless.player.invulnerable=0;harmless.spawnFlood({1200,1200},{1,0},1,0);harmless.spawnFlood(harmless.player.position,{1,0},1,.1f);
        for(int i=0;i<5;++i)harmless.update(FixedStep,{});require(harmless.player.hp==100,"Joining water damaged during its own warning");
        for(int i=0;i<3;++i)harmless.update(FixedStep,{});require(harmless.phase==Phase::Playing && harmless.player.hp==90,"Joining water stayed harmless after its warning");
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
        require(game.enemies.size()==1800 && game.enemies.data()==data && game.flood.enemyId==handle && game.flood.particles.size()==288 && std::equal(targets.begin(),targets.end(),game.enemies.begin(),[](const Enemy& a,const Enemy& b){return a.id==b.id;}),"A joined wave evicted another enemy or allocated at the enemy cap");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
