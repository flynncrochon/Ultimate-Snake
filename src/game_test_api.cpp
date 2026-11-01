#include "game_test_api.hpp"
#include "grid.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace snake {
namespace {
using Json=TestJson;
constexpr const char* Patterns[]{"shot","homing","mortar","nest","nova","beam","ricochet","pit","lash"};
constexpr const char* Phases[]{"playing","level_up","paused","dead","victory"};
constexpr const char* EnemyKinds[]{"red","splitter","swift","boss","flood"};
constexpr const char* EventNames[]{"shot","hit","kill","hurt","pickup","level","evolve","chest","win"};
constexpr std::uint32_t FixtureHandleBegin=1000000000u;
std::uint32_t nextFixtureHandle(std::uint32_t value){return value==std::numeric_limits<std::uint32_t>::max()?FixtureHandleBegin:value+1;}
Json vec(Vec p){return Json::array({p.x,p.y});}
template<class T>Json points(const T& source){Json result=Json::array();for(Vec p:source)result.push_back(vec(p));return result;}
float number(const Json& data,const char* key,float fallback,float low=-1e8f,float high=1e8f){
    if(!data.contains(key))return fallback;
    if(!data[key].is_number())throw std::invalid_argument(std::string(key)+" must be numeric");
    const float value=data[key].get<float>();
    if(!std::isfinite(value)||value<low||value>high)throw std::invalid_argument(std::string(key)+" is outside its supported range");
    return value;
}
int integer(const Json& data,const char* key,int fallback,int low,int high){
    if(!data.contains(key))return fallback;
    if(!data[key].is_number_integer())throw std::invalid_argument(std::string(key)+" must be an integer");
    const auto value=data[key].get<std::int64_t>();
    if(value<low||value>high)throw std::invalid_argument(std::string(key)+" is outside its supported range");
    return int(value);
}
bool flag(const Json& data,const char* key,bool fallback){
    if(!data.contains(key))return fallback;
    if(!data[key].is_boolean())throw std::invalid_argument(std::string(key)+" must be boolean");
    return data[key].get<bool>();
}
Vec vector(const Json& value){
    if(!value.is_array()||value.size()!=2||!value[0].is_number()||!value[1].is_number())throw std::invalid_argument("A position/direction must be [x,y]");
    Vec p{value[0].get<float>(),value[1].get<float>()};
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||std::abs(p.x)>1e6f||std::abs(p.y)>1e6f)throw std::invalid_argument("Non-finite or excessive vector coordinate");
    return p;
}
Vec direction(const Json& value,bool allowZero=true){
    const Vec p=vector(value);
    if((allowZero&&p.x==0&&p.y==0)||(std::abs(p.x)==1&&p.y==0)||(p.x==0&&std::abs(p.y)==1))return p;
    throw std::invalid_argument("Direction must be cardinal: [1,0],[-1,0],[0,1],[0,-1]");
}
Json specJson(const WeaponSpec& s){
    return {{"pattern",Patterns[int(s.pattern)]},{"evolved",s.evolved},{"persistent",s.persistent},{"damage_bonus",s.damageBonus},
        {"damage",s.damage},{"unrounded_damage",s.unroundedDamage},{"cooldown",s.cooldown},{"speed",s.speed},{"range",s.range},{"area",s.area},{"duration",s.duration},
        {"tick_interval",s.tickInterval},{"turn_speed",s.turnSpeed},{"amount",s.amount},{"pierce",s.pierce},{"bounces",s.bounces},{"brood_count",s.broodCount}};
}
Json statsJson(const Stats& s){return {{"damage",s.damage},{"cooldown",s.cooldown},{"area",s.area},{"duration",s.duration},{"pickup",s.pickup},
    {"crit",s.crit},{"growth",s.growth},{"recovery",s.recovery},{"range",s.range},{"speed",s.speed},{"amount",s.amount},{"armor",s.armor}};}
int definitionFor(const Catalog& catalog,std::string id){
    const std::pair<const char*,const char*> aliases[]{{"fangs","viper_fangs"},{"plague","plague_mortar"},{"sidewinder","sidewinder_beam"},
        {"ricochet","ricochet_fang"},{"gridlock","graviton"},{"magnet","graviton"},{"atk_speed","rapid_fire"},{"crit","dead_eye"},{"gorger","ravenous_maw"},
        {"multishot","coiled_volley"},{"blast_radius","toxic_expanse"},{"weapon_range","serpents_reach"},{"serpent_scales","serpents_scales"}};
    for(const auto& alias:aliases)if(id==alias.first)id=alias.second;
    const int found=catalog.find(id);if(found<0)throw std::invalid_argument("Unknown upgrade id: "+id);return found;
}
void clearActors(Game& game){
    game.graviton={};
    game.greySnakes.reset();
    game.flood.reset();
    game.enemies.clear();game.projectiles.clear();game.zones.clear();game.nestEggs.clear();game.miniSnakes.clear();
    game.beams.clear();game.cobraPits.clear();game.cobras.clear();game.grapples.clear();game.leviathan={};
    // Keep the source game's seeded fang parameters initialized by reset().
    // Clearing their visible positions must not collapse all 44 orbits together.
    game.ouroborosActive=false;game.orbitFangs={};
    game.pickups.clear();game.chestRewards.clear();game.particles.clear();game.snakeDeaths.clear();game.damageNumbers.clear();game.events.clear();
}
}

GameTestSession::GameTestSession(Game& world,TestCapture captureFunction):game(world),capture(std::move(captureFunction)){}
Json GameTestSession::reset(const Json& request){
    const auto seedValue=request.value("seed",std::uint64_t(0x5EEDu));
    if(seedValue>std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("seed must be a uint32");
    const int character=integer(request,"character",0,0,3);const bool isolated=flag(request,"isolated",true),full=flag(request,"full",false);
    PermanentUpgrades permanent;
    if(request.contains("permanent")){
        const auto& values=request["permanent"];if(!values.is_object())throw std::invalid_argument("permanent must be an object of upgrade ranks");
        for(auto item=values.begin();item!=values.end();++item){
            const auto& definitions=permanentDefinitions();
            const auto definition=std::find_if(definitions.begin(),definitions.end(),[&](const PermanentUpgradeDef& value){return item.key()==value.id;});
            if(definition==definitions.end())throw std::invalid_argument("Unknown permanent upgrade: "+item.key());
            permanent.ranks[std::size_t(definition-definitions.begin())]=integer(values,item.key().c_str(),0,0,definition->maxRank);
        }
    }
    game.reset(character,std::uint32_t(seedValue),permanent);tick=0;nextHandle=FixtureHandleBegin;eventCounts.fill(0);lastTiming=Json::object();
    director=!isolated;frozenPlayer=isolated;invulnerable=isolated;autoChoose=true;stop=false;
    game.greySnakes.enabled=!isolated;
    game.flood.enabled=!isolated;
    if(isolated){clearActors(game);game.owned.clear();game.player.invulnerable=1000000;}
    return snapshot(full);
}
Json GameTestSession::configure(const Json& request){
    const bool nextDirector=flag(request,"director",director),nextFrozen=flag(request,"freeze_player",frozenPlayer);
    const bool nextInvulnerable=flag(request,"invulnerable",invulnerable),nextAutoChoose=flag(request,"auto_choose",autoChoose);
    const float elapsed=number(request,"elapsed",game.elapsed,0,RunDuration),bossClock=number(request,"boss_clock",game.bossClock,0,1000000);
    const float spawnClock=number(request,"spawn_clock",game.spawnClock,-1000000,1000000);
    const bool greyEnabled=flag(request,"grey_snakes",request.contains("director")?nextDirector:game.greySnakes.enabled);
    const float greyClock=number(request,"grey_clock",request.contains("elapsed")?elapsed:float(game.greySnakes.clock),0,RunDuration);
    const float greyNextSpawn=number(request,"grey_next_spawn",float(game.greySnakes.nextSpawn),0,RunDuration+float(GreySpawnSeconds));
    const bool floodEnabled=flag(request,"floods",request.contains("director")?nextDirector:game.flood.enabled);
    const float floodClock=number(request,"flood_clock",request.contains("elapsed")?elapsed:float(game.flood.clock),0,RunDuration);
    const float floodNext=number(request,"flood_next_spawn",float(game.flood.nextSpawn),0,RunDuration+FloodIntervalSeconds);
    director=nextDirector;frozenPlayer=nextFrozen;invulnerable=nextInvulnerable;autoChoose=nextAutoChoose;
    if(request.contains("invulnerable")&&!invulnerable)game.player.invulnerable=0;
    game.elapsed=elapsed;game.bossClock=bossClock;game.spawnClock=spawnClock;
    game.greySnakes.enabled=greyEnabled;
    if(request.contains("elapsed") || request.contains("grey_clock"))game.greySnakes.clock=greyClock;
    if(request.contains("grey_next_spawn"))game.greySnakes.nextSpawn=greyNextSpawn;
    game.flood.enabled=floodEnabled;
    if(request.contains("elapsed") || request.contains("flood_clock"))game.flood.clock=floodClock;
    if(request.contains("flood_next_spawn"))game.flood.nextSpawn=floodNext;
    return {{"director",director},{"grey_snakes",greyEnabled},{"floods",floodEnabled},{"freeze_player",frozenPlayer},{"invulnerable",invulnerable},{"auto_choose",autoChoose}};
}
Json GameTestSession::setPlayer(const Json& request){
    Player p=game.player;
    if(request.contains("direction"))p.direction=p.queued=direction(request["direction"],false);
    if(request.contains("position"))p.position=gridPosition(vector(request["position"]));
    else if(request.contains("x")||request.contains("y"))p.position=gridPosition({number(request,"x",p.position.x),number(request,"y",p.position.y)});
    p.length=integer(request,"length",p.length,1,MaxPlayerLength);p.level=integer(request,"level",p.level,1,1000000);
    p.xp=integer(request,"xp",p.xp,0,100000000);p.nextXp=integer(request,"next_xp",p.nextXp,1,100000000);
    p.maxHp=number(request,"max_hp",p.maxHp,1,1e8f);p.hp=number(request,"hp",std::min(p.hp,p.maxHp),0,p.maxHp);
    p.invulnerable=number(request,"invulnerable",p.invulnerable,0,1e8f);
    const bool relocate=request.contains("position")||request.contains("x")||request.contains("y")||request.contains("length")||request.contains("direction");
    if(relocate){
        p.segments.clear();for(int i=0;i<p.length;++i)p.segments.push_back(gridPosition({p.position.x-p.direction.x*Tile*i,p.position.y-p.direction.y*Tile*i}));
        p.previous=p.position;p.previousSegments=p.segments;p.moveClock=0;p.collisionClock=-1;
    }
    if(request.contains("segments")){
        const auto& data=request["segments"];if(!data.is_array()||data.empty()||data.size()>MaxPlayerLength)throw std::invalid_argument("segments requires 1..120 grid points");
        p.segments.clear();for(const auto& point:data)p.segments.push_back(gridPosition(vector(point)));
        p.length=int(p.segments.size());p.position=p.segments.front();p.previous=p.position;p.previousSegments=p.segments;p.collisionClock=-1;
    }
    if(request.contains("previous_segments")){
        const auto& data=request["previous_segments"];if(!data.is_array()||data.size()!=p.segments.size())throw std::invalid_argument("previous_segments length must match segments");
        p.previousSegments.clear();for(const auto& point:data)p.previousSegments.push_back(gridPosition(vector(point)));p.previous=p.previousSegments.front();
    }
    if(request.contains("previous"))p.previous=gridPosition(vector(request["previous"]));
    p.moveClock=number(request,"move_clock",p.moveClock,0,10);p.moveInterval=number(request,"move_interval",p.moveInterval,.001f,10);
    game.player=std::move(p);return snapshot(false)["player"];
}
Json GameTestSession::equip(const Json& request){
    const int def=definitionFor(game.catalog,request.at("id").get<std::string>());const auto& definition=game.catalog.upgrades[std::size_t(def)];
    const int rank=integer(request,"rank",1,1,definition.maxRank);const bool evolved=flag(request,"evolved",false);
    if(evolved&&(definition.kind!=UpgradeKind::Weapon||!game.evolutionFor(def)))throw std::invalid_argument("This upgrade has no evolution");
    const float cooldown=number(request,"cooldown",0,0,10000);
    auto found=std::find_if(game.owned.begin(),game.owned.end(),[&](const OwnedUpgrade& owned){return owned.definition==def;});
    if(found==game.owned.end()){
        int count=0;for(const auto& owned:game.owned)if(game.catalog.upgrades[std::size_t(owned.definition)].kind==definition.kind)++count;
        if(count>=(definition.kind==UpgradeKind::Weapon?WeaponSlots:PassiveSlots))throw std::invalid_argument("Equipment slots are full");
        game.owned.push_back({def,rank,evolved,cooldown});found=std::prev(game.owned.end());
    }else *found={def,rank,evolved,cooldown};
    Json result={{"id",definition.id},{"rank",rank},{"evolved",evolved}};
    if(definition.kind==UpgradeKind::Weapon)result["spec"]=specJson(game.weaponSpec(*found));return result;
}
Json GameTestSession::spawnEnemy(const Json& request){
    if(game.enemies.size()>=1800)throw std::invalid_argument("Enemy capacity is 1800");
    Enemy enemy;enemy.position=gridPosition(request.contains("position")?vector(request["position"]):Vec{number(request,"x",game.player.position.x+64),number(request,"y",game.player.position.y)});
    const auto kind=request.value("kind",std::string("red"));bool matched=false;
    for(int i=0;i<4;++i)if(kind==EnemyKinds[i]){enemy.kind=EnemyKind(i);matched=true;}
    if(!matched)throw std::invalid_argument("kind must be red, splitter, swift, or boss");
    if(request.contains("id")){
        if(!request["id"].is_number_integer())throw std::invalid_argument("Fixture enemy id must be an integer");
        const auto value=request["id"].get<std::int64_t>();
        if(value<FixtureHandleBegin||std::uint64_t(value)>std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("Fixture enemy id must be in 1000000000..4294967295; lower ids belong to the director");
        enemy.id=std::uint32_t(value);
    }else{
        enemy.id=std::max(nextHandle,FixtureHandleBegin);
        while(std::any_of(game.enemies.begin(),game.enemies.end(),[&](const Enemy& e){return e.id==enemy.id;}))enemy.id=nextFixtureHandle(enemy.id);
    }
    if(enemy.id==0||std::any_of(game.enemies.begin(),game.enemies.end(),[&](const Enemy& e){return e.id==enemy.id;}))throw std::invalid_argument("Enemy id must be unique and positive");
    enemy.hp=enemy.maxHp=number(request,"hp",1000,.001f,1e12f);enemy.speed=number(request,"speed",0,0,10000);
    enemy.radius=number(request,"radius",enemy.kind==EnemyKind::Boss?7.0f:3.5f,.1f,16);
    enemy.trailSize=integer(request,"length",enemy.kind==EnemyKind::Boss?8:3,0,8);
    if(request.contains("direction"))enemy.direction=direction(request["direction"],false);
    enemy.previous=enemy.position;enemy.trail.fill(enemy.position);enemy.previousTrail=enemy.trail;
    game.enemies.push_back(enemy);if(!request.contains("id"))nextHandle=nextFixtureHandle(enemy.id);return enemy.id;
}
Json GameTestSession::advance(const Json& request){
    const int ticks=integer(request,"ticks",1,0,120000);Input input;
    const bool full=flag(request,"full",false),verify=flag(request,"validate",false);
    if(request.contains("direction"))input.direction=direction(request["direction"]);
    std::vector<double> timings;timings.reserve(std::size_t(ticks));
    for(int frame=0;frame<ticks;++frame){
        const float spawnClock=game.spawnClock,bossClock=game.bossClock,moveClock=game.player.moveClock;
        if(!director){game.spawnClock=-1000000;game.bossClock=1000000;}
        if(frozenPlayer){game.player.moveClock=-1000000;game.player.previous=game.player.position;game.player.previousSegments=game.player.segments;}
        if(invulnerable)game.player.invulnerable=1000000;
        if(beforeStep)beforeStep();
        const auto begin=std::chrono::steady_clock::now();game.update(FixedStep,input);
        if(autoChoose)for(int pending=0;game.phase==Phase::LevelUp&&pending<1024;++pending){bool chosen=false;for(int i=0;i<3;++i)if(game.choose(i)){chosen=true;break;}if(!chosen)throw std::runtime_error("No eligible level-up choice or fallback");}
        const auto end=std::chrono::steady_clock::now();timings.push_back(std::chrono::duration<double,std::micro>(end-begin).count());
        for(const auto& event:game.events)if(int(event.type)>=0&&int(event.type)<int(eventCounts.size()))++eventCounts[std::size_t(event.type)];
        if(!director){game.spawnClock=spawnClock;game.bossClock=bossClock;}
        if(frozenPlayer)game.player.moveClock=moveClock;
        ++tick;
    }
    if(!timings.empty()){
        const double mean=std::accumulate(timings.begin(),timings.end(),0.0)/double(timings.size());std::sort(timings.begin(),timings.end());
        auto quantile=[&](double p){return timings[std::min(timings.size()-1,std::size_t(double(timings.size()-1)*p))];};
        lastTiming={{"ticks",ticks},{"mean_us",mean},{"p50_us",quantile(.5)},{"p95_us",quantile(.95)},{"p99_us",quantile(.99)},{"max_us",timings.back()},{"total_us",mean*ticks}};
    }
    if(verify){const auto checks=validate();if(!checks["ok"].get<bool>())throw std::runtime_error("Invariant failure: "+checks["errors"].dump());}
    return snapshot(full);
}

Json GameTestSession::snapshot(bool full)const{
    const auto& p=game.player;
    Json counts={{"enemies",game.enemies.size()},{"projectiles",game.projectiles.size()},{"zones",game.zones.size()},
        {"nest_eggs",game.nestEggs.size()},{"mini_snakes",game.miniSnakes.size()},{"beams",game.beams.size()},
        {"cobra_pits",game.cobraPits.size()},{"cobras",game.cobras.size()},{"grapples",game.grapples.size()},
        {"pickups",game.pickups.size()},{"particles",game.particles.size()},{"damage_numbers",game.damageNumbers.size()},{"snake_deaths",game.snakeDeaths.size()},
        {"orbit_fangs",game.ouroborosActive?game.orbitFangs.size():0}};
    const auto greyActive=std::count_if(game.greySnakes.trails.begin(),game.greySnakes.trails.end(),[](const GreySnake& snake){return snake.alive;});
    counts["grey_snakes"]=greyActive;counts["grey_trails"]=game.greySnakes.trails.size()-std::size_t(greyActive);counts["grey_barriers"]=game.greySnakes.barrierCount;
    counts["floods"]=game.flood.active()?1:0;counts["water_particles"]=game.flood.particles.size();
    counts["water_impacts"]=0; // Retain the diagnostic key for older capture tools.
    Json result={{"api_version",1},{"tick",tick},{"seed",game.seed},{"phase",Phases[int(game.phase)]},{"elapsed",game.elapsed},
        {"kills",game.kills},{"gems",game.gems},{"chests",game.chests},{"bosses_killed",game.bossesKilled},{"counts",counts},
        {"stats",statsJson(game.stats())},{"timing",lastTiming},{"player",{{"position",vec(p.position)},{"previous",vec(p.previous)},
        {"direction",vec(p.direction)},{"queued",vec(p.queued)},{"hp",p.hp},{"max_hp",p.maxHp},{"level",p.level},{"xp",p.xp},{"next_xp",p.nextXp},
        {"length",p.length},{"xp_remainder",p.xpRemainder},{"move_clock",p.moveClock},{"move_interval",p.moveInterval},{"collision_clock",p.collisionClock},{"invulnerable",p.invulnerable}}},
        {"shield",{{"charges",game.shieldCharges},{"max",game.shieldMax},{"rank",game.shieldRank},{"timer",game.shieldTimer},{"cooldown",game.shieldCooldown}}},
        {"graviton",{{"turns",game.graviton.turns},{"rank",game.graviton.rank},{"serial",game.graviton.serial},{"center",vec(game.graviton.center)},
            {"age",game.graviton.age},{"duration",game.graviton.duration},{"radius",game.graviton.radius},{"cooldown",game.graviton.cooldown},{"empowered",game.graviton.empowered}}},
        {"orbit_phase",game.orbitPhase},{"ouroboros_active",game.ouroborosActive},{"cobra_overflow",game.cobraOverflow},
        {"leviathan",{{"active",game.leviathan.active},{"position",vec(game.leviathan.position)},{"growth",game.leviathan.growth},{"age",game.leviathan.age},{"absorb_flash",game.leviathan.absorbFlash}}}};
    const auto& wave=stageWave(game.elapsed);
    result["permanent_upgrades"]=Json::object();
    for(std::size_t i=0;i<PermanentUpgradeCount;++i)result["permanent_upgrades"][permanentDefinitions()[i].id]=game.permanentUpgrades().ranks[i];
    result["progression"]={{"run_seconds",RunDuration},{"wave",stageMinute(game.elapsed)+1},
        {"wave_seconds_remaining",game.elapsed>=RunDuration?0.0f:(stageMinute(game.elapsed)+1)*WaveDuration-game.elapsed},
        {"minimum_enemies",wave.minimum},{"spawn_interval",wave.interval},{"ordinary_limit",OrdinaryEnemyLimit},
        {"boss_waves_scheduled",game.bossClock},{"damage_dealt",game.damageDealt},
        {"experience_dropped",game.experienceDropped},{"experience_collected",game.experienceCollected}};
    result["events"]=Json::object();for(std::size_t i=0;i<std::size(EventNames);++i)result["events"][EventNames[i]]=eventCounts[i];
    result["notice"]=game.notice;result["notice_time"]=game.noticeTime;
    result["chest_rewards"]=Json::array();
    for(const auto& reward:game.chestRewards){
        const char* kind=reward.kind==ChestRewardKind::Evolution?"evolution":reward.kind==ChestRewardKind::Upgrade?"upgrade":"treasure";
        result["chest_rewards"].push_back({{"serial",reward.serial},{"kind",kind},{"definition",reward.definition},
            {"id",reward.definition>=0?Json(game.catalog.upgrades[std::size_t(reward.definition)].id):Json(nullptr)},
            {"rank",reward.rank},{"gold",reward.gold},{"healing",reward.healing},{"title",reward.title},{"description",reward.description}});
    }
    result["owned"]=Json::array();
    for(const auto& owned:game.owned){const auto& d=game.catalog.upgrades[std::size_t(owned.definition)];Json item={{"id",d.id},{"name",game.upgradeName(owned)},{"rank",owned.rank},{"evolved",owned.evolved},{"cooldown",owned.cooldown}};if(d.kind==UpgradeKind::Weapon)item["spec"]=specJson(game.weaponSpec(owned));result["owned"].push_back(item);}
    result["offers"]=Json::array();for(int offer:game.offers)result["offers"].push_back(offer<0?Json(nullptr):Json(game.catalog.upgrades[std::size_t(offer)].id));
    result["grey_system"]={{"enabled",game.greySnakes.enabled},{"clock",game.greySnakes.clock},{"next_spawn",game.greySnakes.nextSpawn},
        {"spawn_time",game.greySnakes.spawnTime()},{"predictability",game.greySnakes.predictability()},
        {"straight_steps",game.greySnakes.straightSteps},{"repeated_steps",game.greySnakes.repeatedSteps},{"route_count",game.greySnakes.routeCount},
        {"trail_capacity",GreyTrailCapacity}};
    result["flood"]={{"enabled",game.flood.enabled},{"clock",game.flood.clock},{"next_spawn",game.flood.nextSpawn},{"serial",game.flood.serial},
        {"age",game.flood.age},{"warning",game.flood.warning},{"lifetime",game.flood.lifetime},{"dangerous",game.flood.dangerous()},{"waves",game.flood.waveCount()},{"capacity",FloodCapacity},
        {"center",vec(game.flood.center)},{"direction",vec(game.flood.direction)},{"enemy_id",game.flood.enemyId},{"repulsions",game.flood.repulsions}};
    if(!full)return result;
    result["water_particles"]=Json::array();for(const auto& water:game.flood.particles)result["water_particles"].push_back({{"position",vec(water.position)},{"previous",vec(water.previous)},{"velocity",vec(water.velocity)},{"foam",water.foam},{"age",water.age},{"warning",water.warning},{"wave",water.wave},{"hp",water.hp}});
    result["water_impacts"]=Json::array();
    result["grey_trails"]=Json::array();
    for(const auto& snake:game.greySnakes.trails){
        Json path=Json::array();for(std::size_t i=0;i<snake.count;++i)path.push_back(vec(snake.path[i]));
        result["grey_trails"].push_back({{"id",snake.id},{"path",path},{"previous",vec(snake.previous)},
            {"direction",vec(snake.direction)},{"target",vec(snake.target)},{"wall_direction",vec(snake.wallDirection)},
            {"wall",snake.wall},{"wall_ticks",snake.wallTicks},{"alive",snake.alive},{"settling",snake.settling},{"cutting",snake.cutting},{"age",snake.age},{"move_clock",snake.moveClock}});
    }
    result["player"]["segments"]=points(p.segments);result["player"]["previous_segments"]=points(p.previousSegments);
    result["orbit_fangs"]=game.ouroborosActive?points(game.orbitFangs):Json::array();
    result["orbit_fang_states"]=Json::array();
    if(game.ouroborosActive)for(const auto& fang:game.orbitFangStates)result["orbit_fang_states"].push_back({{"angle",fang.angle},{"orbit",fang.orbit},{"speed",fang.speed},{"size",fang.size},
        {"radius_amplitude",fang.radiusAmplitude},{"radius_frequency",fang.radiusFrequency},{"radius_phase",fang.radiusPhase},{"angle_amplitude",fang.angleAmplitude},{"angle_frequency",fang.angleFrequency},{"angle_phase",fang.anglePhase}});
    for(const char* key:{"enemies","projectiles","zones","nest_eggs","mini_snakes","beams","cobra_pits","cobras","grapples","pickups"})result[key]=Json::array();
    for(const auto& e:game.enemies)result["enemies"].push_back({{"id",e.id},{"position",vec(e.position)},{"previous",vec(e.previous)},{"direction",vec(e.direction)},{"velocity",vec(e.velocity)},
        {"hp",e.hp},{"max_hp",e.maxHp},{"speed",e.speed},{"radius",e.radius},{"kind",EnemyKinds[int(e.kind)]},{"alive",e.alive},{"slow",e.slow},{"slow_factor",e.slowFactor},{"gravity_hold",e.gravityHold},
        {"experience",e.experience},{"contact_damage",e.contactDamage},
        {"move_clock",e.moveClock},{"move_interval",e.moveInterval},{"length",e.trailSize},{"trail",points(e.trail)},{"previous_trail",points(e.previousTrail)},{"grabbed",e.grabbed},{"devoured",e.devoured}});
    for(const auto& shot:game.projectiles)result["projectiles"].push_back({{"position",vec(shot.position)},{"velocity",vec(shot.velocity)},{"pattern",Patterns[int(shot.pattern)]},
        {"damage",shot.damage},{"life",shot.life},{"max_life",shot.maxLife},{"age",shot.age},{"radius",shot.radius},{"pierce",shot.pierce},{"bounces",shot.bounces},
        {"critical",shot.critical},{"evolved",shot.evolved},{"alive",shot.alive},{"target_id",shot.targetId},{"start",vec(shot.start)},{"target",vec(shot.target)},
        {"flight_duration",shot.flightDuration},{"turn_speed",shot.turnSpeed},{"bounce_radius",shot.bounceRadius},{"damage_ramp",shot.damageRamp},{"generation",shot.generation},{"impacts",shot.impacts},{"gatling",shot.gatling},{"cobra_spit",shot.cobraSpit},{"hit_count",shot.hitCount}});
    for(const auto& z:game.zones)result["zones"].push_back({{"position",vec(z.position)},{"pattern",Patterns[int(z.pattern)]},{"radius",z.radius},{"damage",z.damage},
        {"life",z.life},{"max_life",z.maxLife},{"tick",z.tick},{"tick_interval",z.tickInterval},{"damage_factor",z.damageFactor},{"delay",z.delay},{"detonation_damage",z.detonationDamage},
        {"evolved",z.evolved},{"single_hit",z.singleHit},{"can_crit",z.canCrit},{"follow_player",z.followPlayer},{"gridlock",z.gridlockSeed}});
    for(const auto& e:game.nestEggs)result["nest_eggs"].push_back({{"start",vec(e.start)},{"target",vec(e.target)},{"age",e.age},{"flight_duration",e.flightDuration},{"hatch_delay",e.hatchDelay},{"snake_life",e.snakeLife},{"damage",e.damage},{"count",e.count},{"evolved",e.evolved},{"brood_life",e.broodLife},{"brood_damage",e.broodDamage}});
    for(const auto& m:game.miniSnakes)result["mini_snakes"].push_back({{"position",vec(m.position)},{"previous",vec(m.previous)},{"direction",vec(m.direction)},{"segments",points(m.segments)},
        {"previous_segments",points(m.previousSegments)},{"extra_segments",points(m.extraSegments)},{"previous_extra_segments",points(m.previousExtraSegments)},{"segment_count",m.segmentCount},
        {"generation",m.generation},{"life",m.life},{"damage",m.damage},{"move_clock",m.moveClock},{"move_interval",m.moveInterval},{"evolved",m.evolved},{"alive",m.alive},
        {"brood_life",m.broodLife},{"brood_damage",m.broodDamage},{"wander_ticks",m.wanderTicks}});
    for(const auto& b:game.beams)result["beams"].push_back({{"target",b.target},{"origin",vec(b.origin)},{"position",vec(b.position)},{"life",b.life},{"duration",b.duration},
        {"tick",b.tick},{"tick_interval",b.tickInterval},{"damage",b.damage},{"range",b.range},{"evolved",b.evolved},{"ancient",b.ancient},{"pending_absorb",b.pendingAbsorb}});
    for(const auto& pit:game.cobraPits)result["cobra_pits"].push_back({{"position",vec(pit.position)},{"age",pit.age},{"life",pit.life},{"spawn_clock",pit.spawnClock},{"cobra_life",pit.cobraLife},{"spit_cooldown",pit.spitCooldown},{"damage",pit.damage},{"spawned",pit.spawned},{"max_cobras",pit.maxCobras}});
    for(const auto& cobra:game.cobras)result["cobras"].push_back({{"position",vec(cobra.position)},{"age",cobra.age},{"life",cobra.life},{"spit_clock",cobra.spitClock},{"spit_cooldown",cobra.spitCooldown},{"damage",cobra.damage}});
    for(const auto& g:game.grapples)result["grapples"].push_back({{"target",g.target},{"position",vec(g.position)},{"direction",vec(g.direction)},{"traveled",g.traveled},{"max_distance",g.maxDistance},{"phase",g.phase},{"pull_clock",g.pullClock},{"life",g.life},{"hit_count",g.hitCount}});
    for(const auto& pickup:game.pickups)result["pickups"].push_back({{"position",vec(pickup.position)},{"kind",pickup.kind==PickupKind::Chest?"chest":pickup.kind==PickupKind::Heart?"heart":"experience"},{"value",pickup.value}});
    return result;
}

Json GameTestSession::validate()const{
    Json errors=Json::array();auto problem=[&](const std::string& text){if(errors.size()<64)errors.push_back(text);};
    auto finite=[&](Vec p,const std::string& name){if(!std::isfinite(p.x)||!std::isfinite(p.y))problem(name+" position is non-finite");};
    auto grid=[&](Vec p,const std::string& name){finite(p,name);if(!sameCell(gridPosition(p),p))problem(name+" is not an in-bounds grid cell");};
    if(!std::isfinite(game.elapsed)||!std::isfinite(game.player.hp))problem("Non-finite player/run state");
    if(game.snakeDeaths.size()>SnakeDeathCapacity)problem("Snake death capacity exceeded");
    for(const auto& death:game.snakeDeaths)if(death.count>death.current.size() || !std::isfinite(death.age) || !std::isfinite(death.alpha))problem("Invalid snake death pose");
    grid(game.player.position,"player");for(Vec p:game.player.segments)grid(p,"player segment");
    if(game.greySnakes.trails.size()>GreyTrailCapacity)problem("Grey trail capacity exceeded");
    int greyActive=0;std::size_t greyCells=0;
    for(std::size_t i=0;i<game.greySnakes.trails.size();++i){
        const auto& snake=game.greySnakes.trails[i];greyActive+=snake.alive?1:0;
        if(snake.id!=i+1 || !snake.count || snake.count>GreyPathCapacity || !std::isfinite(snake.age) || !std::isfinite(snake.moveClock)){problem("Invalid grey snake state");continue;}
        for(std::size_t j=0;j<snake.count;++j){
            const Vec p=snake.path[j];grid(p,"grey barrier");
            if(p.x<Tile || p.y<Tile || p.x>(ArenaTiles-2)*Tile || p.y>(ArenaTiles-2)*Tile || !game.greySnakes.blocks(p))problem("Invalid grey barrier cell");
            if(j && std::abs(p.x-snake.path[j-1].x)+std::abs(p.y-snake.path[j-1].y)!=Tile)problem("Disconnected grey trail");
        }
    }
    for(auto owner:game.greySnakes.owners)if(owner){++greyCells;if(owner>game.greySnakes.trails.size())problem("Invalid grey barrier owner");}
    if(greyActive>1 || greyCells!=game.greySnakes.barrierCount)problem("Invalid grey barrier population");
    if(game.enemies.size()>1800||game.projectiles.size()>1600||game.zones.size()>128||game.nestEggs.size()>25||game.miniSnakes.size()>60)problem("Actor capacity exceeded");
    if(game.flood.particles.size()>FloodCapacity)problem("Flood capacity exceeded");
    int floodHandles=0;
    for(const auto& enemy:game.enemies)if(enemy.alive && enemy.kind==EnemyKind::Flood)++floodHandles;
    if(floodHandles!=(game.flood.active()?1:0))problem("Flood target lifetime mismatch");
    bool hazardous=false;
    for(const auto& water:game.flood.particles){finite(water.position,"water");finite(water.velocity,"water velocity");if(!std::isfinite(water.foam) || water.foam<0 || water.foam>1)problem("Invalid water foam");if(!std::isfinite(water.age) || !std::isfinite(water.warning) || water.age<0 || water.warning<0 || !water.wave)problem("Invalid water lifetime");if(!std::isfinite(water.hp) || water.hp<=0 || water.hp>FloodHealth)problem("Invalid water HP");hazardous=hazardous || game.flood.dangerous(water);}
    if(hazardous!=game.flood.dangerous())problem("Flood danger state mismatch");
    std::vector<std::uint32_t> ids;ids.reserve(game.enemies.size());
    for(const auto& enemy:game.enemies){grid(enemy.position,"enemy");if(!std::isfinite(enemy.hp))problem("Non-finite enemy HP");if(!std::isfinite(enemy.gravityHold)||enemy.gravityHold<0||enemy.gravityHold>1.5f)problem("Invalid gravity hold");if(enemy.trailSize<0||enemy.trailSize>8)problem("Invalid enemy trail size");else for(int i=0;i<enemy.trailSize;++i)grid(enemy.trail[std::size_t(i)],"enemy segment");ids.push_back(enemy.id);}
    std::sort(ids.begin(),ids.end());if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())problem("Duplicate enemy handles");
    for(const auto& p:game.projectiles){finite(p.position,"projectile");finite(p.velocity,"projectile velocity");if(!std::isfinite(p.life)||!std::isfinite(p.damage)||p.hitCount>p.hitIds.size())problem("Invalid projectile state");}
    for(const auto& mini:game.miniSnakes){grid(mini.position,"summon");for(Vec p:mini.segments)grid(p,"summon segment");if(mini.segmentCount>3)for(Vec p:mini.extraSegments)grid(p,"Hydra segment");}
    for(const auto& egg:game.nestEggs){finite(egg.start,"egg start");grid(egg.target,"egg target");if(!std::isfinite(egg.age)||egg.count<0)problem("Invalid egg state");}
    for(const auto& zone:game.zones){finite(zone.position,"zone");if(!std::isfinite(zone.radius)||!std::isfinite(zone.life))problem("Invalid zone state");}
    return {{"ok",errors.empty()},{"errors",errors},{"tick",tick}};
}

Json GameTestSession::execute(const Json& request){
    if(!request.is_object())throw std::invalid_argument("Request must be a JSON object");
    const auto command=request.at("cmd").get<std::string>();
    if(command=="hello")return {{"api_version",1},{"transport","jsonl"},{"fixed_dt",FixedStep},{"tile",Tile},{"arena_tiles",ArenaTiles},{"run_seconds",RunDuration},{"native_capture",bool(capture)}};
    if(command=="reset")return reset(request);
    if(command=="configure")return configure(request);
    if(command=="player")return setPlayer(request);
    if(command=="equip")return equip(request);
    if(command=="spawn_enemy")return spawnEnemy(request);
    if(command=="spawn_flood"){
        const Vec position=request.contains("position")?vector(request["position"]):Vec{number(request,"x",game.player.position.x+160,Tile,ArenaSize-Tile),number(request,"y",game.player.position.y,Tile,ArenaSize-Tile)};
        const Vec heading=request.contains("direction")?direction(request["direction"],false):Vec{-1,0};
        const int count=integer(request,"count",640,1,int(FloodCapacity));const float warning=number(request,"warning",1.5f,0,10);
        if(!game.spawnFlood(position,heading,count,warning))throw std::invalid_argument("Flood sample or enemy capacity reached");
        return {{"enemy_id",game.flood.enemyId},{"particles",game.flood.particles.size()}};
    }
    if(command=="spawn_grey_snake"){
        if(game.greySnakes.trails.size()>=GreyTrailCapacity)throw std::invalid_argument("Grey trail capacity exceeded");
        GreySnake snake;snake.id=std::uint8_t(game.greySnakes.trails.size()+1);
        snake.alive=flag(request,"alive",true);snake.wall=flag(request,"wall",false);
        snake.cutting=flag(request,"cutting",false);
        if(snake.alive && std::any_of(game.greySnakes.trails.begin(),game.greySnakes.trails.end(),[](const GreySnake& s){return s.alive;}))throw std::invalid_argument("Only one grey snake may be active");
        const auto& path=request.at("path");
        if(!path.is_array() || path.empty() || path.size()>GreyPathCapacity)throw std::invalid_argument("Grey path requires 1..168 oldest-first grid points");
        for(const auto& point:path){
            const Vec input=vector(point);const Vec p{std::round(input.x/Tile)*Tile,std::round(input.y/Tile)*Tile};
            if(p.x<Tile || p.y<Tile || p.x>(ArenaTiles-2)*Tile || p.y>(ArenaTiles-2)*Tile)throw std::invalid_argument("Grey path must stay inside the arena's outermost row");
            if(snake.count && std::abs(p.x-snake.path[snake.count-1].x)+std::abs(p.y-snake.path[snake.count-1].y)!=Tile)throw std::invalid_argument("Grey path must be cardinal and connected");
            if(game.greySnakes.overlapsOther(p))throw std::invalid_argument("Grey path overlaps another barrier or its one-cell gap");
            snake.path[snake.count++]=p;
        }
        snake.previous=request.contains("previous")?gridPosition(vector(request["previous"])):snake.path[snake.count>1?snake.count-2:0];
        const auto head=snake.path[snake.count-1];
        if((snake.previous.x!=head.x && snake.previous.y!=head.y) || std::abs(snake.previous.x-head.x)+std::abs(snake.previous.y-head.y)>Tile)throw std::invalid_argument("Grey previous head must be at most one cardinal cell away");
        snake.direction=request.contains("direction")?direction(request["direction"],false):Vec{0,1};
        snake.target=request.contains("target")?gridPosition(vector(request["target"])):snake.path[snake.count-1];
        snake.wallDirection=request.contains("wall_direction")?direction(request["wall_direction"],false):Vec{1,0};
        snake.age=number(request,"age",0,0,float(GreyLifetimeSeconds));
        snake.moveClock=number(request,"move_clock",0,0,float(GreyMoveSeconds));
        snake.wallTicks=integer(request,"wall_ticks",0,0,12);
        const std::size_t count=snake.count;snake.count=0;
        // Validation above is atomic. Only now publish the path and occupancy.
        game.greySnakes.trails.push_back(snake);auto& stored=game.greySnakes.trails.back();
        for(std::size_t i=0;i<count;++i)game.greySnakes.append(stored,snake.path[i]);
        return stored.id;
    }
    if(command=="spawn_pickup"){
        const std::string kind=request.value("kind",std::string("experience"));PickupKind type;
        if(kind=="experience")type=PickupKind::Experience;else if(kind=="heart")type=PickupKind::Heart;else if(kind=="chest")type=PickupKind::Chest;
        else throw std::invalid_argument("Pickup kind must be experience, heart, or chest");
        const int value=integer(request,"value",1,1,type==PickupKind::Chest?100:1000000);
        const Vec position=gridPosition(request.contains("position")?vector(request["position"]):Vec{number(request,"x",game.player.position.x),number(request,"y",game.player.position.y)});
        if(game.pickups.size()>=1403)throw std::invalid_argument("Pickup capacity is 1403");
        game.pickups.push_back({position,value,type});return {{"position",vec(position)},{"kind",kind},{"value",value}};
    }
    if(command=="spawn_enemies"){
        const auto& list=request.at("enemies");if(!list.is_array()||list.size()+game.enemies.size()>1800)throw std::invalid_argument("Enemy batch exceeds capacity");
        Json ids=Json::array();const auto before=game.enemies.size();const auto handle=nextHandle;
        try{for(const auto& enemy:list)ids.push_back(spawnEnemy(enemy));}catch(...){game.enemies.resize(before);nextHandle=handle;throw;}
        return ids;
    }
    if(command=="set_enemy"){
        const auto id=request.at("id").get<std::uint32_t>();auto found=std::find_if(game.enemies.begin(),game.enemies.end(),[&](const Enemy& enemy){return enemy.id==id;});
        if(found==game.enemies.end())throw std::invalid_argument("Unknown enemy handle");
        if(found->kind==EnemyKind::Flood)throw std::invalid_argument("Flood handles belong to the water system; use reset/clear to remove them");
        Enemy e=*found;e.hp=number(request,"hp",e.hp,0,1e12f);e.maxHp=std::max(e.maxHp,e.hp);e.speed=number(request,"speed",e.speed,0,10000);e.alive=flag(request,"alive",e.alive);
        if(request.contains("position")||request.contains("x")||request.contains("y")){
            e.position=gridPosition(request.contains("position")?vector(request["position"]):Vec{number(request,"x",e.position.x),number(request,"y",e.position.y)});
            e.previous=e.position;e.trail.fill(e.position);e.previousTrail=e.trail;e.motionReady=false;
            e.splitBirth.count=0;
        }
        *found=e;return id;
    }
    if(command=="step"||command=="benchmark")return advance(request);
    if(command=="snapshot")return snapshot(flag(request,"full",true));
    if(command=="invariants")return validate();
    if(command=="pause"){game.togglePause();return snapshot(false);}
    if(command=="choose")return game.choose(integer(request,"choice",0,0,2));
    if(command=="reroll")return game.reroll();
    if(command=="banish")return game.banish(integer(request,"choice",0,0,2));
    if(command=="grant_xp"){game.grantXp(integer(request,"amount",0,0,10000000));return snapshot(false);}
    if(command=="clear"){
        const bool clearOwned=flag(request,"owned",true);clearActors(game);if(clearOwned)game.owned.clear();
        game.shieldCharges=game.shieldMax=game.shieldRank=0;game.shieldTimer=game.orbitPhase=0;game.cobraOverflow=1;
        return snapshot(false);
    }
    if(command=="unequip"){
        const int def=definitionFor(game.catalog,request.at("id").get<std::string>());
        game.owned.erase(std::remove_if(game.owned.begin(),game.owned.end(),[&](const OwnedUpgrade& owned){return owned.definition==def;}),game.owned.end());return snapshot(false);
    }
    if(command=="catalog"){
        Json result={{"upgrades",Json::array()},{"evolutions",Json::array()}};
        for(std::size_t i=0;i<game.catalog.upgrades.size();++i){const auto& d=game.catalog.upgrades[i];Json def={{"id",d.id},{"name",d.name},{"description",d.description},{"kind",d.kind==UpgradeKind::Weapon?"weapon":"passive"},{"max_rank",d.maxRank},{"weight",d.weight}};
            if(d.kind==UpgradeKind::Weapon){def["ranks"]=Json::array();for(int rank=1;rank<=d.maxRank;++rank)def["ranks"].push_back(specJson(game.weaponSpec({int(i),rank,false,0})));if(game.evolutionFor(int(i)))def["evolved_spec"]=specJson(game.weaponSpec({int(i),d.maxRank,true,0}));}result["upgrades"].push_back(def);}
        for(const auto& e:game.catalog.evolutions)result["evolutions"].push_back({{"weapon",e.weapon},{"passive",e.passive},{"name",e.name},{"description",e.description},{"damage",e.damage},{"cooldown",e.cooldown},{"area",e.area},{"range",e.range}});return result;
    }
    if(command=="capture"){if(!capture)throw std::runtime_error("Capture requires the native --test-api host");return capture(request);}
    if(command=="quit"){stop=true;return {{"stopped",true}};}
    throw std::invalid_argument("Unknown command: "+command);
}
Json GameTestSession::dispatch(const Json& request)noexcept{
    Json response=Json::object();if(request.is_object()&&request.contains("request_id"))response["request_id"]=request["request_id"];
    try{response["result"]=execute(request);response["ok"]=true;}catch(const std::exception& e){response["ok"]=false;response["error"]={{"type","command_error"},{"message",e.what()}};}catch(...){response["ok"]=false;response["error"]={{"type","command_error"},{"message","Unknown native exception"}};}
    return response;
}
int GameTestSession::serve(std::istream& input,std::ostream& output){
    std::string line;
    while(!stop&&std::getline(input,line)){
        if(line.empty())continue;
        Json response;
        if(line.size()>4*1024*1024)response={{"ok",false},{"error",{{"type","request_error"},{"message","JSON request exceeds 4 MiB"}}}};
        else try{response=dispatch(Json::parse(line));}catch(const std::exception& e){response={{"ok",false},{"error",{{"type","parse_error"},{"message",e.what()}}}};}
        output<<response.dump()<<'\n';output.flush();
    }
    return 0;
}
}
