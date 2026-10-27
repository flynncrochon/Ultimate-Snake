#include "core.hpp"
#include "grid.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace snake;
void require(bool value,const std::string& reason){if(!value)throw std::runtime_error(reason);}
bool near(float a,float b,float epsilon=.002f){return std::abs(a-b)<=epsilon;}
void step(Game& game,int frames){for(int i=0;i<frames;++i)game.update(FixedStep,{});}
void prepare(Game& game){
    game.owned.clear();game.enemies.clear();game.pickups.clear();game.character=-1;
    game.player.moveClock=-1000;game.player.invulnerable=1000;game.player.nextXp=1000000;
    game.spawnClock=-1000;game.bossClock=100;
}
void equip(Game& game,const char* id,int rank=1,bool evolved=false){game.owned.push_back({game.catalog.find(id),rank,evolved,0});}
Enemy& target(Game& game,float x,float y,float hp=100000){
    Enemy enemy;enemy.id=10000+std::uint32_t(game.enemies.size());enemy.position={game.player.position.x+x,game.player.position.y+y};
    enemy.previous=enemy.position;enemy.trail.fill(enemy.position);enemy.previousTrail=enemy.trail;enemy.hp=enemy.maxHp=hp;enemy.speed=0;
    game.enemies.push_back(enemy);return game.enemies.back();
}
Enemy* enemy(Game& game,std::uint32_t id){for(auto& actor:game.enemies)if(actor.id==id)return &actor;return nullptr;}
const EvolutionDef& recipe(const Catalog& catalog,const char* id){
    const auto found=std::find_if(catalog.evolutions.begin(),catalog.evolutions.end(),[&](const EvolutionDef& evolution){return evolution.weapon==id;});
    if(found==catalog.evolutions.end())throw std::runtime_error(std::string("Missing evolution recipe: ")+id);
    return *found;
}
float tunedDamage(const Catalog& catalog,const char* id,float sourceDamage){return std::round(sourceDamage*recipe(catalog,id).damage);}
}
int main(){
    using namespace snake;
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    Catalog content;try{content=Catalog::load(SNAKE_TEST_DATA_DIR);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}

    test("source constants and explicit evolution tuning preserve weapon contracts",[&]{
        struct Row{const char* id;bool evolved;float damage,cooldown,range;int amount;};
        const Row rows[]{
            {"venom_shot",false,150,.4f,112,3},{"viper_fangs",false,150,2.5f,72,1},
            {"plague_mortar",false,125,4,128,1},{"snake_nest",false,50,7,96,1},
            {"venom_nova",false,100,3.5f,20,1},{"sidewinder_beam",false,15,5,64,1},
            {"ricochet_fang",false,200,3.5f,80,1},{"cobra_pit",false,60,8,96,1},{"tongue_lash",false,150,2,24,1},
            {"venom_shot",true,180,.1f,76,44},{"viper_fangs",true,150,.18f,88,2},
            {"plague_mortar",true,400,5.5f,144,1},{"snake_nest",true,200,5.5f,96,1},
            {"venom_nova",true,320,.1f,20,1},{"sidewinder_beam",true,100,.1f,80,1},
            {"ricochet_fang",true,600,3,96,1},{"cobra_pit",true,180,6,112,1},{"tongue_lash",true,400,2.2f,112,1}};
        Catalog sourceContent=content;
        for(auto& evolution:sourceContent.evolutions)evolution.damage=evolution.cooldown=evolution.area=evolution.range=1;
        Game source(sourceContent);prepare(source);
        Game live(content);prepare(live);
        for(const auto& row:rows){const OwnedUpgrade owned{content.find(row.id),1,row.evolved,0};const auto original=source.weaponSpec(owned),actual=live.weaponSpec(owned);
            require(near(original.damage,row.damage)&&near(original.cooldown,row.cooldown)&&near(original.range,row.range)&&original.amount==row.amount,std::string(row.id)+(row.evolved?" evolution":" base")+" source constants differ from JavaScript");
            const auto* tuning=row.evolved?&recipe(content,row.id):nullptr;
            require(near(actual.damage,std::round(row.damage*(tuning?tuning->damage:1)))&&near(actual.cooldown,row.cooldown*(tuning?tuning->cooldown:1))&&near(actual.range,row.range*(tuning?tuning->range:1))&&actual.amount==row.amount,std::string(row.id)+" does not apply the explicit evolution tuning to source constants");
            require(near(actual.area,original.area*(tuning?tuning->area:1)),std::string(row.id)+" does not apply the explicit evolution area tuning");}
    });
    test("rank-eight damage cooldown and brood progression match source formulas",[&]{
        struct Row{const char* id;float damage,cooldown;int amount;};
        const Row rows[]{{"venom_shot",360,.19f,5},{"viper_fangs",450,1,8},{"plague_mortar",375,1.5f,1},{"snake_nest",150,2.8f,1},
            {"venom_nova",300,1.2f,1},{"sidewinder_beam",120,2.2f,1},{"ricochet_fang",600,1.5f,1},{"cobra_pit",300,4.15f,1},{"tongue_lash",750,.74f,1}};
        Game game(content);prepare(game);for(const auto& row:rows){const auto spec=game.weaponSpec({content.find(row.id),8,false,0});require(near(spec.damage,row.damage)&&near(spec.cooldown,row.cooldown)&&spec.amount==row.amount,std::string(row.id)+" rank-eight progression differs");}
        require(game.weaponSpec({content.find("snake_nest"),8,false,0}).broodCount==6,"Nest rank-eight brood must contain six parents");
        require(game.weaponSpec({content.find("ricochet_fang"),8,false,0}).bounces==7,"Ricochet rank-eight needs seven total impacts");
    });
    test("bonus formulas and indirect-damage exclusions match JavaScript",[&]{
        Game game(content);prepare(game);equip(game,"rapid_fire",8);equip(game,"dead_eye",8);equip(game,"ravenous_maw",8);equip(game,"chronofield",2);equip(game,"coiled_volley",2);
        const auto stats=game.stats();require(near(stats.cooldown,std::pow(.85f,8))&&near(stats.crit,1)&&near(stats.damage,1.5f)&&near(stats.duration,1.5f)&&stats.amount==2,"Passive modifiers differ from source");
        require(near(game.weaponSpec({content.find("plague_mortar"),1,false,0}).damage,125),"Plague poison incorrectly receives direct-weapon Maw damage");
        require(near(game.weaponSpec({content.find("snake_nest"),8,true,0}).damage,tunedDamage(content,"snake_nest",200*3)),"Hydra must keep Volley investment while excluding direct-weapon Maw damage");
        require(game.weaponSpec({content.find("snake_nest"),8,true,0}).amount==1,"Hydra must concentrate Volley into one egg rather than extra actors");
        require(game.weaponSpec({content.find("plague_mortar"),8,true,0}).amount==1,"Singularity must concentrate Volley into one well rather than extra actors");
    });
    test("Venom Shot fires facing fangs and center with the source lifetime",[&]{
        Game game(content);prepare(game);equip(game,"venom_shot");target(game,64,0);step(game,1);
        require(game.projectiles.size()==3,"Base gun requires exactly three bullets");
        int up=0,down=0,straight=0;for(const auto& bullet:game.projectiles){require(near(std::sqrt(bullet.velocity.x*bullet.velocity.x+bullet.velocity.y*bullet.velocity.y),320),"Gun speed changed");if(bullet.velocity.y>1)++down;else if(bullet.velocity.y<-1)++up;else ++straight;require(near(bullet.life,.55f-FixedStep),"Gun lifetime changed");}
        require(up==1&&down==1&&straight==1,"Gun no longer emits two 45-degree fangs and a facing center shot");
    });
    test("Cobra spit does not consume the gun's independent bullet budget",[&]{
        Game game(content);prepare(game);equip(game,"venom_shot");target(game,64,0);
        for(int i=0;i<45;++i){Projectile spit;spit.position={game.player.position.x-96,game.player.position.y};spit.cobraSpit=true;spit.life=1;game.projectiles.push_back(spit);}step(game,1);
        int fangs=0;for(const auto& shot:game.projectiles)if(shot.pattern==Pattern::Shot&&!shot.cobraSpit)++fangs;
        require(fangs==3,"Cobra swarm incorrectly occupied Venom Shot's 40-projectile family cap");
    });
    test("Fangs steer toward a target and Gatling pierces with falloff",[&]{
        Game game(content);prepare(game);equip(game,"viper_fangs");target(game,64,0);step(game,1);game.owned.clear();
        require(game.projectiles.size()==1 && near(game.projectiles.front().turnSpeed,10),"Base fang must be an independently homing projectile");
        const auto previous=game.projectiles.front().velocity;game.enemies.front().position.y+=32;step(game,1);require(game.projectiles.front().velocity.y>previous.y,"Fang did not track a moving enemy");
        Game gatling(content);prepare(gatling);equip(gatling,"viper_fangs",8,true);target(gatling,64,0);step(gatling,1);
        require(gatling.projectiles.size()==2,"Gatling needs two base homing fangs");for(const auto& bullet:gatling.projectiles)require(bullet.gatling&&bullet.pierce==2&&near(bullet.turnSpeed,12),"Gatling lacks its three-impact homing contract");
        gatling.owned.clear();gatling.projectiles.clear();gatling.enemies.clear();target(gatling,24,0);target(gatling,40,0);target(gatling,56,0);
        Projectile shot;shot.position=gatling.player.position;shot.velocity={1000,0};shot.damage=150;shot.life=1;shot.pierce=2;shot.gatling=true;gatling.projectiles.push_back(shot);step(gatling,4);
        require(near(gatling.enemies[0].hp,99850)&&near(gatling.enemies[1].hp,99895)&&near(gatling.enemies[2].hp,99925),"Gatling impact damage must be 150/105/75");
    });
    test("direct critical damage rounds after the multiplier and Gatling retargets",[&]{
        Game game(content);prepare(game);game.character=0;equip(game,"venom_shot");equip(game,"dead_eye",8);target(game,64,0);step(game,15);
        require(near(game.enemies.front().hp,99655),"150 base x1.15 Cobra x2 crit must round to345, not346");
        require(game.weaponSpec({content.find("sidewinder_beam"),6,false,0}).damage==103,"JS double90*1.15 is below103.5 and must round to103");
        require(game.weaponSpec({content.find("viper_fangs"),8,false,0}).damage==518,"450*1.15 must round517.5 upward to518 without a blanket half-point epsilon");
        Game nova(content);prepare(nova);nova.character=0;equip(nova,"venom_nova");equip(nova,"chronofield",1);target(nova,16,0);step(nova,17);
        require(near(nova.enemies.front().hp,99828),"Cobra Nova must deal115 main plus57 aftershock; aftershock halves raw damage before rounding");
        Game gatling(content);prepare(gatling);target(gatling,48,24);Projectile shot;shot.position=gatling.player.position;shot.velocity={192,0};shot.life=shot.maxLife=.5f;shot.pattern=Pattern::Homing;shot.gatling=true;shot.turnSpeed=12;shot.targetId=999999;shot.pierce=2;gatling.projectiles.push_back(shot);step(gatling,1);
        require(gatling.projectiles.front().targetId==gatling.enemies.front().id&&gatling.projectiles.front().velocity.y>0,"Gatling must replace a dead/missing target with an unhit nearby enemy");
    });
    test("Plague mortar travels before creating raw poison ticks",[&]{
        Game game(content);prepare(game);equip(game,"plague_mortar");target(game,64,0);step(game,1);game.owned.clear();
        require(game.projectiles.size()==1&&near(game.projectiles.front().flightDuration,.9f),"Mortar requires a .9-second fixed landing arc");
        const auto landing=game.projectiles.front().target;step(game,45);require(game.zones.empty()&&near(game.enemies.front().hp,100000),"Mortar damaged before landing");
        step(game,15);require(game.projectiles.empty()&&game.zones.size()==1&&near(game.zones.front().damageFactor,1),"Landed mortar did not create a full-damage poison pool");
        require(sameCell(landing,game.zones.front().position),"Poison pool moved away from its snapshotted landing");step(game,20);require(game.enemies.front().hp<100000,"Poison pool failed its .35-second damage tick");
    });
    test("Singularity delays its damage and detonates for 2500 with crits",[&]{
        Game game(content);prepare(game);equip(game,"dead_eye",8);target(game,16,0);Zone well;well.position=game.enemies.front().position;well.radius=32;well.life=well.maxLife=.1f;well.damage=400;well.tick=.4f;well.tickInterval=.4f;well.damageFactor=1;well.pattern=Pattern::Mortar;well.evolved=true;well.canCrit=true;well.detonationDamage=2500;game.zones.push_back(well);
        step(game,1);require(near(game.enemies.front().hp,100000),"Well dealt damage before its tick or detonation");step(game,6);require(near(game.enemies.front().hp,95000),"Singularity detonation did not apply a 2500 x2 critical hit");
    });
    test("Singularity lands in .65 seconds hits immediately and detonates only once",[&]{
        Game game(content);prepare(game);equip(game,"plague_mortar",8,true);target(game,64,0);step(game,1);
        require(game.projectiles.size()==1&&near(game.projectiles.front().flightDuration,.65f),"Singularity must use the authored .65-second flight");
        game.owned.front().cooldown=100;step(game,37);
        require(game.zones.empty()&&near(float(game.enemies.front().hp),100000),"Airborne Singularity dealt damage before landing");
        step(game,2);const float landedHp=100000-tunedDamage(content,"plague_mortar",400);
        require(game.projectiles.empty()&&game.zones.size()==1&&near(float(game.enemies.front().hp),landedHp),"Singularity must deal its first well hit on landing, before the base mortar's first poison tick");
        require(near(game.zones.front().tickInterval,.4f),"Later well hits must keep the .4-second interval");
        game.zones.front().life=.001f;step(game,1);
        const float detonatedHp=landedHp-tunedDamage(content,"plague_mortar",2500);
        require(game.zones.empty()&&near(float(game.enemies.front().hp),detonatedHp),"Expired Singularity must detonate once without an extra periodic hit");
        step(game,30);require(game.zones.empty()&&near(float(game.enemies.front().hp),detonatedHp),"Singularity repeated its detonation after expiry");
    });
    test("Singularity leaves a centered enemy settled while retaining slow",[&]{
        Game game(content);prepare(game);target(game,48,0);const Vec center=game.enemies.front().position;
        Zone well;well.position=center;well.radius=32;well.life=well.maxLife=2;well.damage=400;well.tick=.4f;well.tickInterval=.4f;well.damageFactor=1;well.pattern=Pattern::Mortar;well.evolved=true;well.detonationDamage=2500;game.zones.push_back(well);
        step(game,11);require(sameCell(game.enemies.front().position,center)&&game.enemies.front().slow>0,"Zero-distance pull pushed a centered enemy away or failed to slow it");
        step(game,20);require(sameCell(game.enemies.front().position,center),"Centered enemy oscillated after later gravity pulses");
    });
    test("Nest and Hydra preserve eggs parent bodies and expiry brood",[&]{
        Game game(content);prepare(game);equip(game,"snake_nest",8,true);equip(game,"coiled_volley",2);target(game,64,0);step(game,1);game.owned.clear();
        require(game.nestEggs.size()==1&&game.nestEggs.front().count==6&&near(game.nestEggs.front().damage,tunedDamage(content,"snake_nest",200*3)),"Hydra must concentrate three Volley payloads into one egg with six configured parents");
        step(game,75);require(game.miniSnakes.size()==6,"Hydra egg did not hatch six parents");for(auto& mini:game.miniSnakes){require(mini.segmentCount==5&&mini.generation==0,"Hydra parents require five body segments");mini.life=.001f;}
        step(game,1);require(game.miniSnakes.size()==12,"Each expired Hydra parent must split into two children");for(const auto& mini:game.miniSnakes)require(mini.segmentCount==3&&mini.generation==1&&near(mini.damage,tunedDamage(content,"snake_nest",150*3))&&near(mini.moveInterval,.07f),"Hydra split children lost their configured damage or independent movement stats");
    });
    test("Nova is immediate while duration adds separate half-damage aftershocks",[&]{
        Game game(content);prepare(game);equip(game,"venom_nova");equip(game,"chronofield",1);target(game,16,0);step(game,1);game.owned.clear();
        require(near(game.enemies.front().hp,99900),"Nova did not apply its main wave immediately");require(game.zones.size()==2,"One Chronofield rank must add one delayed aftershock");step(game,16);require(near(game.enemies.front().hp,99850),"Nova aftershock must deal half the main wave damage once");step(game,20);require(near(game.enemies.front().hp,99850),"Nova aftershock incorrectly repeats as a damage pool");
    });
    test("Miasma follows the player and pulses every half second",[&]{
        Game game(content);prepare(game);equip(game,"venom_nova",8,true);target(game,32,0);step(game,1);require(game.zones.size()==1&&game.zones.front().followPlayer,"Miasma must be one persistent following aura");step(game,27);require(near(game.enemies.front().hp,100000),"Miasma pulsed early");step(game,4);require(near(game.enemies.front().hp,100000-tunedDamage(content,"venom_nova",320)),"Miasma must apply exactly one configured damage pulse at .5 seconds");
    });
    test("configured evolution damage reaches orbit well laser and tongue impacts",[&]{
        Game orbit(content);prepare(orbit);equip(orbit,"venom_shot",8,true);
        for(auto& fangState:orbit.orbitFangStates)fangState.speed=fangState.radiusAmplitude=fangState.angleAmplitude=0;
        step(orbit,1);
        const Vec fang=orbit.orbitFangs[8];
        const Vec contact=gridPosition(fang);const auto orbitId=target(orbit,contact.x-orbit.player.position.x,contact.y-orbit.player.position.y).id;
        step(orbit,1);const auto* bitten=enemy(orbit,orbitId);
        require(bitten&&near(float(bitten->hp),100000-tunedDamage(content,"venom_shot",180)),"Orbit contact ignored the configured evolution damage");

        Game mortar(content);prepare(mortar);equip(mortar,"plague_mortar",8,true);mortar.owned.front().cooldown=100;target(mortar,16,0);
        Zone well;well.position=mortar.enemies.front().position;well.radius=32;well.life=well.maxLife=.001f;well.damage=400;well.tick=.4f;well.tickInterval=.4f;well.pattern=Pattern::Mortar;well.evolved=true;well.canCrit=true;well.detonationDamage=2500;mortar.zones.push_back(well);
        step(mortar,1);require(near(float(mortar.enemies.front().hp),100000-tunedDamage(content,"plague_mortar",2500)),"Well detonation ignored the configured evolution damage");

        Game pit(content);prepare(pit);equip(pit,"cobra_pit",8,true);pit.owned.front().cooldown=100;target(pit,32,0);
        pit.leviathan.active=true;pit.leviathan.position=pit.player.position;pit.leviathan.age=2;pit.leviathan.growth=4;
        TrackingBeam laser;laser.ancient=laser.evolved=true;laser.life=laser.duration=1;laser.target=pit.enemies.front().id;laser.tick=1;pit.beams.push_back(laser);
        step(pit,1);require(near(float(pit.enemies.front().hp),100000-tunedDamage(content,"cobra_pit",360)),"Grown leviathan laser ignored the configured evolution damage");

        Game lash(content);prepare(lash);equip(lash,"tongue_lash",8,true);lash.owned.front().cooldown=100;
        const auto captured=target(lash,8,0).id;const auto bystander=target(lash,16,0).id;
        enemy(lash,captured)->grabbed=true;Grapple tongue;tongue.target=captured;tongue.position=enemy(lash,captured)->position;tongue.phase=1;tongue.pullClock=.1f;lash.grapples.push_back(tongue);
        step(lash,1);const auto* blasted=enemy(lash,bystander);
        require(!enemy(lash,captured)&&blasted&&near(float(blasted->hp),100000-tunedDamage(content,"tongue_lash",500)),"Devour blast ignored the configured evolution damage");

        Game drag(content);prepare(drag);equip(drag,"tongue_lash",8,true);drag.owned.front().cooldown=100;
        const auto prey=target(drag,64,0).id;const auto crossed=target(drag,56,0).id;
        enemy(drag,prey)->grabbed=true;Grapple pulling;pulling.target=prey;pulling.position=enemy(drag,prey)->position;pulling.phase=1;pulling.pullClock=.1f;drag.grapples.push_back(pulling);
        step(drag,1);const auto* struck=enemy(drag,crossed);
        require(struck&&near(float(struck->hp),100000-tunedDamage(content,"tongue_lash",250)),"Grapple drag ignored the configured evolution damage");
    });
    test("Sidewinder tracks one enemy without piercing a collinear neighbor",[&]{
        Game game(content);prepare(game);equip(game,"sidewinder_beam");target(game,24,0);target(game,48,0);step(game,1);game.owned.clear();require(game.beams.size()==1&&near(game.beams.front().duration,2.5f),"Sidewinder requires a persistent 2.5-second tracking beam");step(game,10);require(near(game.enemies[0].hp,100000),"Beam tick occurred before .2 seconds");step(game,2);require(near(game.enemies[0].hp,99985)&&near(game.enemies[1].hp,100000),"Tracking beam must damage only its selected target");
    });
    test("Consumption absorbs a target finished by another weapon without healing",[&]{
        Game game(content);prepare(game);equip(game,"sidewinder_beam",8,true);target(game,32,0,20);game.player.hp=37;step(game,1);require(game.beams.size()==1,"Consumption beam was not created");
        const int oldLength=game.player.length;const int oldXp=game.player.xp;game.pickups.push_back({{game.player.position.x+48,game.player.position.y},2,PickupKind::Experience});
        Projectile killer;killer.position=game.enemies.front().position;killer.velocity={0,0};killer.damage=50;killer.life=1;game.projectiles.push_back(killer);step(game,1);step(game,1);
        require(game.enemies.empty()&&game.player.length>=oldLength+5&&game.player.xp>=oldXp+5,"Consumption lost a death notification or failed its +3 growth and fruit vacuum");require(near(game.player.hp,37),"Consumption incorrectly heals rather than absorbing growth");
    });
    test("Ricochet uses a capped hit history and Shatter creates homing children",[&]{
        Game game(content);prepare(game);equip(game,"ricochet_fang");target(game,64,0);step(game,1);require(game.projectiles.size()==1&&game.projectiles.front().bounces==2&&near(game.projectiles.front().damageRamp,.2f),"Ricochet requires three total hits and +20% damage per bounce");
        game.owned.clear();game.projectiles.clear();game.enemies.clear();target(game,24,0);target(game,40,16);target(game,40,-16);target(game,56,0);equip(game,"dead_eye",8);
        Projectile shot;shot.position=game.player.position;shot.velocity={1500,0};shot.damage=600;shot.life=1;shot.pattern=Pattern::Ricochet;shot.evolved=true;shot.bounceRadius=48;shot.bounces=5;shot.damageRamp=.2f;game.projectiles.push_back(shot);step(game,1);
        int children=0;for(const auto& bullet:game.projectiles)if(bullet.generation==1){++children;require(bullet.targetId!=0&&near(bullet.turnSpeed,14)&&near(bullet.damage,300),"Shatter splinter must home and inherit half base damage");}
        require(children==3,"Critical Shatter hit must create up to three uniquely targeted homing splinters");
    });
    test("Cobra Pit raises independent stationary spit turrets",[&]{
        Game game(content);prepare(game);target(game,40,0);CobraPit pit;pit.position={game.player.position.x+24,game.player.position.y};pit.life=1;pit.cobraLife=5;pit.damage=60;game.cobraPits.push_back(pit);
        step(game,11);require(game.cobras.empty(),"Cobra spawned before initial .2-second hatch");step(game,2);require(game.cobras.size()==1,"Pit failed to create an individual cobra actor");const Vec position=game.cobras.front().position;step(game,75);require(game.cobraPits.empty()&&!game.cobras.empty(),"Expired pit incorrectly deleted its living cobras");require(near(game.cobras.front().position.x,position.x)&&near(game.cobras.front().position.y,position.y),"Cobra turret walked rather than remaining rooted");require(game.enemies.front().hp<100000||!game.projectiles.empty(),"Raised cobra failed to spit venom");
    });
    test("Ancient Pit raises one leviathan absorbs later seeds and pierces",[&]{
        Game game(content);prepare(game);equip(game,"cobra_pit",8,true);target(game,48,0);step(game,1);game.owned.front().cooldown=100;step(game,52);require(game.leviathan.active&&!game.cobraPits.size()&&game.cobras.empty(),"Ancient seed must summon a single leviathan rather than base cobra turrets");step(game,91);require(!game.beams.empty()&&game.beams.front().ancient,"Leviathan did not raise its piercing laser after 1.5 seconds");game.owned.front().cooldown=0;step(game,1);game.owned.front().cooldown=100;step(game,52);require(game.leviathan.growth==1,"Subsequent Ancient seed must grow the existing leviathan");
    });
    test("Tongue Lash uses a facing cone and timed slow",[&]{
        Game game(content);prepare(game);equip(game,"tongue_lash");target(game,16,0);target(game,-16,0);step(game,1);
        require(near(game.enemies[0].hp,99850)&&near(game.enemies[1].hp,100000),"Lash hit outside its facing cone or missed its forward target");require(game.enemies[0].slow>1.4f&&near(game.enemies[0].slowFactor,.4f),"Lash lost its 1.5-second slow");require(near(std::remainder(game.enemies[0].position.x,float(Tile)),0),"Lash knockback left the enemy off-grid");
    });
    test("Reckoning captures pulls on the grid and devours without XP drops",[&]{
        Game game(content);prepare(game);equip(game,"tongue_lash",8,true);const auto id=target(game,64,0).id;step(game,1);game.owned.clear();require(game.grapples.size()==1,"Reckoning requires an extending grapple actor");step(game,15);auto* held=enemy(game,id);require(held&&held->grabbed&&game.grapples.front().phase==1,"Grapple did not latch onto its distant target");const int length=game.player.length;step(game,65);require(!enemy(game,id)&&game.kills==1&&game.pickups.empty()&&game.player.length==length+1,"Reckoning must devour its captured enemy and directly grant growth without dropping fruit");
    });
    test("rapid Reckoning volleys reserve unique prey before latching",[&]{
        Game game(content);prepare(game);equip(game,"tongue_lash",8,true);equip(game,"rapid_fire",8);equip(game,"coiled_volley",2);equip(game,"serpents_reach",8);
        for(int index=0;index<9;++index)target(game,160+float(index%3)*8,float(index/3-1)*8,10000000);
        bool sawExtending=false,sawLatched=false;int casts=0;
        for(int tick=0;tick<240;++tick){
            step(game,1);
            for(const auto& event:game.events)if(event.type==Event::Shot)++casts;
            for(std::size_t first=0;first<game.grapples.size();++first){
                const auto& tongue=game.grapples[first];if(tongue.phase==2||tongue.life<=0)continue;
                sawExtending=sawExtending||tongue.phase==0;sawLatched=sawLatched||tongue.phase==1;
                for(std::size_t second=first+1;second<game.grapples.size();++second){const auto& other=game.grapples[second];
                    require(other.phase==2||other.life<=0||tongue.target!=other.target,"Rapid casts reserved the same prey before its first tongue latched");}
            }
            for(const auto& actor:game.enemies)if(actor.grabbed){int owners=0;for(const auto& tongue:game.grapples)if(tongue.phase==1&&tongue.life>0&&tongue.target==actor.id)++owners;
                require(owners==1,"Grabbed prey must have exactly one live latched owner");}
        }
        require(casts>=3&&sawExtending&&sawLatched,"Reservation fixture failed to exercise overlapping rapid volleys and latches");
    });
    test("Ouroboros replaces gun fire with forty-four orbiting fangs",[&]{
        Game game(content);prepare(game);equip(game,"venom_shot",8,true);step(game,1);require(game.ouroborosActive&&game.orbitFangs.size()==44&&game.projectiles.empty(),"Ouroboros must replace the gun with 44 passive fangs");const Vec fang=game.orbitFangs[0];target(game,fang.x-game.player.position.x,fang.y-game.player.position.y);step(game,1);require(game.enemies.front().hp<100000&&game.enemies.front().orbitCooldown>.25f,"Orbiting fang failed its 180-damage contact and .3-second victim cooldown");const float hp=game.enemies.front().hp;step(game,1);require(near(game.enemies.front().hp,hp),"Ouroboros repeatedly hit one enemy inside its contact cooldown");
    });
    test("in-flight fangs and active beams read upgraded rank and damage bonus",[&]{
        Game game(content);prepare(game);equip(game,"viper_fangs");target(game,48,0);step(game,1);
        require(game.projectiles.size()==1,"Missing original fang");game.owned.front().rank=3;game.owned.front().cooldown=100;equip(game,"ravenous_maw",8);step(game,25);
        require(near(game.enemies.front().hp,99550),"Original fang did not use current rank-three300 x1.5 Maw damage on impact");
        Game beam(content);prepare(beam);equip(beam,"sidewinder_beam");target(beam,48,0);step(beam,1);beam.owned.front().rank=3;beam.owned.front().cooldown=100;equip(beam,"ravenous_maw",8);step(beam,12);
        require(beam.beams.size()==1&&near(beam.enemies.front().hp,99932),"Existing beam did not refresh to round(45 x1.5)=68 on its next tick");
    });
    test("active poison and delayed Nova use current tick damage",[&]{
        Game poison(content);prepare(poison);equip(poison,"plague_mortar");poison.owned.front().cooldown=100;target(poison,24,0);
        Zone pool;pool.position=poison.enemies.front().position;pool.radius=18;pool.life=pool.maxLife=4;pool.damage=125;pool.tick=.05f;pool.tickInterval=.35f;pool.damageFactor=1;pool.pattern=Pattern::Mortar;poison.zones.push_back(pool);step(poison,1);
        poison.owned.front().rank=3;equip(poison,"ravenous_maw",8);step(poison,3);require(near(poison.enemies.front().hp,99750),"Existing poison must refresh to250 at rank3 while excluding Maw");
        Game nova(content);prepare(nova);equip(nova,"venom_nova");equip(nova,"chronofield",1);target(nova,16,0);step(nova,1);nova.owned.front().rank=3;nova.owned.front().cooldown=100;equip(nova,"ravenous_maw",8);step(nova,16);
        require(near(nova.enemies.front().hp,99750),"Delayed Nova must use current round(200 x1.5 x.5)=150 after the original100-damage main wave");
    });
    test("nest upgrades apply at hatch while summoned bites stay fixed and duration refreshes",[&]{
        Game game(content);prepare(game);equip(game,"snake_nest");target(game,64,0);step(game,1);game.owned.front().rank=3;game.owned.front().cooldown=100;step(game,75);
        require(game.miniSnakes.size()==4,"Unhatched egg did not use rank-three brood count");for(const auto& mini:game.miniSnakes)require(near(mini.damage,100),"Unhatched egg did not use rank-three bite damage");
        const float remaining=game.miniSnakes.front().life;game.owned.front().rank=8;equip(game,"chronofield",1);step(game,1);
        require(game.miniSnakes.size()==4&&near(game.miniSnakes.front().damage,100),"Upgrading changed already-hatched brood count or bite damage");
        require(near(game.miniSnakes.front().maxLife,7.04f)&&near(game.miniSnakes.front().life,remaining+1.44f-FixedStep),"Live summon did not extend to source round(70 x1.25)=88 movement ticks without resetting its age");
    });
    test("living cobra upgrades affect future spit and preserve launched damage",[&]{
        Game game(content);prepare(game);equip(game,"cobra_pit");game.owned.front().cooldown=100;target(game,48,0);
        Cobra cobra;cobra.position={game.player.position.x+24,game.player.position.y};cobra.age=.5f;cobra.life=5;cobra.spitClock=0;cobra.damage=60;game.cobras.push_back(cobra);step(game,1);
        require(game.projectiles.size()==1&&game.projectiles.front().damage==60,"Initial cobra spit missing");game.owned.front().rank=8;equip(game,"ravenous_maw",8);step(game,1);
        require(game.projectiles.front().damage==60,"Changing bonuses retroactively modified a launched cobra spit");game.cobras.front().spitClock=0;step(game,1);
        require(std::any_of(game.projectiles.begin(),game.projectiles.end(),[](const Projectile& p){return p.cobraSpit&&p.damage==450;}),"Living cobra failed to use current rank-eight300 x1.5 Maw on its next spit");
    });
    test("rank changes preserve elapsed cooldown and evolution clears the base family",[&]{
        Game game(content);prepare(game);equip(game,"viper_fangs");game.owned.front().cooldown=2;game.phase=Phase::LevelUp;game.offers={{content.find("viper_fangs"),-1,-1}};require(game.choose(0),"Rank upgrade rejected");require(near(game.owned.front().cooldown,1.75f),"Rank upgrade reset elapsed attack time instead of applying the new period");
        Game evolution(content);prepare(evolution);equip(evolution,"venom_shot",8);equip(evolution,"serpents_scales",1);target(evolution,64,0);step(evolution,1);require(!evolution.projectiles.empty(),"Missing base bullets before evolution");
        Projectile spit;spit.position={evolution.player.position.x-80,evolution.player.position.y};spit.life=1;spit.cobraSpit=true;evolution.projectiles.push_back(spit);evolution.pickups.push_back({evolution.player.position,1,PickupKind::Chest});step(evolution,1);
        require(evolution.owned.front().evolved,"Chest failed to evolve max-rank gun with Scales");require(std::all_of(evolution.projectiles.begin(),evolution.projectiles.end(),[](const Projectile& p){return p.cobraSpit||p.evolved;}),"Evolution retained the replaced gun's bullets");require(std::any_of(evolution.projectiles.begin(),evolution.projectiles.end(),[](const Projectile& p){return p.cobraSpit;}),"Gun evolution erased another weapon's cobra spit");
    });
    test("Serpent Scales grants source shield charges and timed replenishment",[&]{
        Game game(content);prepare(game);equip(game,"serpents_scales",8);step(game,1);require(game.shieldMax==3&&game.shieldCharges==3&&near(game.shieldCooldown,4),"Rank-eight Scales needs three charges and a four-second refill");game.shieldCharges=1;game.shieldTimer=.1f;step(game,7);require(game.shieldCharges==2&&game.shieldTimer>3.9f,"Scales must restore one charge per cooldown");
    });
    test("Venom Shot waits for the complete volley to fit its family cap",[&]{
        Game game(content);prepare(game);equip(game,"venom_shot");target(game,64,0);
        for(int i=0;i<38;++i){Projectile p;p.position={8,8};p.life=2;game.projectiles.push_back(p);}
        step(game,1);require(game.projectiles.size()==38,"Gun emitted a partial volley at its cap");
        require(game.owned.front().cooldown<=0,"Blocked volley spent its attack cooldown");
        require(std::none_of(game.events.begin(),game.events.end(),[](const Event& e){return e.type==Event::Shot;}),"Blocked volley emitted a shot event");
        game.projectiles.pop_back();step(game,1);
        require(game.projectiles.size()==40&&game.owned.front().cooldown>0,"Gun did not retry its full volley as soon as room became available");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
