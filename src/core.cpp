#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace snake {
namespace {
constexpr float Pi = 3.14159265358979323846f;
constexpr std::size_t EnemyCap = 1800, ProjectileCap = 1600, PickupCap = 1400;
constexpr int GridCell = 32, GridWidth = ArenaTiles * Tile / GridCell;
float lengthSq(Vec a) { return a.x*a.x + a.y*a.y; }
Vec sub(Vec a, Vec b) { return {a.x-b.x,a.y-b.y}; }
Vec add(Vec a, Vec b) { return {a.x+b.x,a.y+b.y}; }
Vec mul(Vec a, float s) { return {a.x*s,a.y*s}; }
Vec unit(Vec a) { const float l = std::sqrt(lengthSq(a)); return l > .001f ? mul(a,1/l) : Vec{1,0}; }
float distanceSq(Vec a, Vec b) { return lengthSq(sub(a,b)); }
float segmentDistanceSq(Vec p, Vec a, Vec b) {
    const Vec ab = sub(b,a); const float l = lengthSq(ab);
    const float t = l > .001f ? std::clamp((sub(p,a).x*ab.x+sub(p,a).y*ab.y)/l,0.0f,1.0f) : 0;
    return distanceSq(p,add(a,mul(ab,t)));
}
template<typename T, typename Dead> void compact(std::vector<T>& v, Dead dead) {
    std::size_t i=0; while(i<v.size()) { if(dead(v[i])) { v[i]=std::move(v.back());v.pop_back(); } else ++i; }
}
std::string trim(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {};
    return value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
}
std::vector<std::string> csvLine(const std::string& line) {
    std::vector<std::string> fields; std::string field; bool quoted=false;
    for(std::size_t i=0;i<line.size();++i) {
        const char ch=line[i];
        if(ch=='"') { if(quoted && i+1<line.size() && line[i+1]=='"') {field+='"';++i;} else quoted=!quoted; }
        else if(ch==',' && !quoted) {fields.push_back(trim(field));field.clear();}
        else field+=ch;
    }
    if(quoted) throw std::runtime_error("Unclosed CSV quote");
    fields.push_back(trim(field)); return fields;
}
using Row = std::unordered_map<std::string,std::string>;
template<typename F> void readCsv(const std::string& path, F callback) {
    std::ifstream file(path);
    if(!file) throw std::runtime_error("Cannot open content file: "+path);
    std::string line; if(!std::getline(file,line)) throw std::runtime_error("Empty content file: "+path);
    if(line.size()>=3 && static_cast<unsigned char>(line[0])==0xef) line.erase(0,3);
    const auto header=csvLine(line); std::unordered_set<std::string> headers;
    for(const auto& key:header) if(key.empty() || !headers.insert(key).second) throw std::runtime_error("Invalid CSV header: "+path);
    int number=1;
    while(std::getline(file,line)) {
        ++number; if(trim(line).empty() || trim(line)[0]=='#') continue;
        try {
            const auto fields=csvLine(line);
            if(fields.size()!=header.size()) throw std::runtime_error("expected "+std::to_string(header.size())+" columns; got "+std::to_string(fields.size()));
            Row row; for(std::size_t i=0;i<header.size();++i) row.emplace(header[i],fields[i]);
            callback(row);
        } catch(const std::exception& e) { throw std::runtime_error(path+":"+std::to_string(number)+": "+e.what()); }
    }
}
const std::string& field(const Row& row, const std::string& key) {
    const auto i=row.find(key); if(i==row.end()) throw std::runtime_error("missing column '"+key+"'");return i->second;
}
float number(const Row& row, const std::string& key) {
    const auto& value=field(row,key); std::size_t used=0; float result=std::stof(value,&used);
    if(used!=value.size() || !std::isfinite(result)) throw std::runtime_error("invalid number for '"+key+"': "+value);
    return result;
}
int integer(const Row& row, const std::string& key) {
    const float result=number(row,key);
    if(result!=std::floor(result) || std::abs(result)>10000) throw std::runtime_error("invalid integer for '"+key+"'");
    return static_cast<int>(result);
}
Pattern parsePattern(const std::string& name) {
    static const std::unordered_map<std::string,Pattern> patterns{{"shot",Pattern::Shot},{"homing",Pattern::Homing},{"mortar",Pattern::Mortar},{"nest",Pattern::Nest},{"nova",Pattern::Nova},{"beam",Pattern::Beam},{"ricochet",Pattern::Ricochet},{"pit",Pattern::Pit},{"lash",Pattern::Lash}};
    const auto it=patterns.find(name); if(it==patterns.end()) throw std::runtime_error("unknown weapon pattern: "+name);return it->second;
}
Stat parseStat(const std::string& name) {
    static const std::unordered_map<std::string,Stat> values{{"damage",Stat::Damage},{"cooldown",Stat::Cooldown},{"area",Stat::Area},{"duration",Stat::Duration},{"amount",Stat::Amount},{"pickup",Stat::Pickup},{"crit",Stat::Crit},{"growth",Stat::Growth},{"range",Stat::Range},{"armor",Stat::Armor},{"speed",Stat::Speed}};
    const auto it=values.find(name);if(it==values.end()) throw std::runtime_error("unknown stat: "+name);return it->second;
}
struct SpatialGrid {
    std::array<int,GridWidth*GridWidth> heads;
    std::array<int,EnemyCap> next{};
    explicit SpatialGrid(const std::vector<Enemy>& enemies) {
        heads.fill(-1);
        for(int i=0;i<static_cast<int>(enemies.size());++i) {
            if(!enemies[i].alive) continue;
            const int x=std::clamp(int(enemies[i].position.x)/GridCell,0,GridWidth-1);
            const int y=std::clamp(int(enemies[i].position.y)/GridCell,0,GridWidth-1);
            const int cell=x+y*GridWidth;next[i]=heads[cell];heads[cell]=i;
        }
    }
    template<typename F> void query(Vec point,float radius,F callback) const {
        const int left=std::clamp(int((point.x-radius)/GridCell),0,GridWidth-1),right=std::clamp(int((point.x+radius)/GridCell),0,GridWidth-1);
        const int top=std::clamp(int((point.y-radius)/GridCell),0,GridWidth-1),bottom=std::clamp(int((point.y+radius)/GridCell),0,GridWidth-1);
        for(int y=top;y<=bottom;++y) for(int x=left;x<=right;++x) for(int i=heads[x+y*GridWidth];i!=-1;i=next[i]) callback(i);
    }
};
}

Catalog Catalog::load(const std::string& directory) {
    Catalog result;
    readCsv(directory+"/upgrades.csv",[&](const Row& row){
        UpgradeDef d; d.id=field(row,"id");d.name=field(row,"name");d.description=field(row,"description");
        if(d.id.empty() || d.name.empty() || result.find(d.id)>=0) throw std::runtime_error("empty or duplicate upgrade id: "+d.id);
        const auto type=field(row,"type");
        if(type!="weapon" && type!="passive") throw std::runtime_error("type must be weapon or passive");
        d.kind=type=="weapon"?UpgradeKind::Weapon:UpgradeKind::Passive;
        d.maxRank=integer(row,"max_rank");d.weight=number(row,"weight");d.pattern=parsePattern(field(row,"pattern"));
        d.damage=number(row,"damage");d.cooldown=number(row,"cooldown");d.speed=number(row,"speed");d.range=number(row,"range");d.area=number(row,"area");d.duration=number(row,"duration");d.amount=integer(row,"amount");d.pierce=integer(row,"pierce");
        if(d.maxRank<1 || d.maxRank>20 || d.weight<=0 || d.damage<0 || d.cooldown<=0 || d.speed<0 || d.range<=0 || d.area<=0 || d.duration<=0 || d.amount<1 || d.pierce<0) throw std::runtime_error("upgrade values outside supported range: "+d.id);
        std::istringstream modifiers(field(row,"modifiers"));std::string token;
        while(std::getline(modifiers,token,';')) {
            const auto colon=token.find(':'); if(colon==std::string::npos) throw std::runtime_error("modifier must be stat:value");
            const auto value=token.substr(colon+1);std::size_t used=0;const double amount=std::stod(value,&used);
            if(used!=value.size() || !std::isfinite(amount)) throw std::runtime_error("invalid modifier amount");
            d.modifiers.push_back({parseStat(token.substr(0,colon)),amount});
        }
        result.upgrades.push_back(std::move(d));
    });
    if(result.upgrades.empty()) throw std::runtime_error("Content catalog has no upgrades");
    readCsv(directory+"/evolutions.csv",[&](const Row& row){
        EvolutionDef d;d.weapon=field(row,"weapon");d.passive=field(row,"passive");d.name=field(row,"name");d.description=field(row,"description");d.damage=number(row,"damage");d.cooldown=number(row,"cooldown");d.area=number(row,"area");
        if(row.count("range"))d.range=number(row,"range");
        const int w=result.find(d.weapon),p=result.find(d.passive);
        if(w<0 || p<0 || result.upgrades[w].kind!=UpgradeKind::Weapon || result.upgrades[p].kind!=UpgradeKind::Passive || d.damage<=0 || d.cooldown<=0 || d.area<=0 || d.range<=0 || d.name.empty()) throw std::runtime_error("invalid evolution recipe: "+d.name);
        for(const auto& e:result.evolutions) if(e.weapon==d.weapon) throw std::runtime_error("duplicate evolution weapon: "+d.weapon);
        result.evolutions.push_back(std::move(d));
    });
    return result;
}
int Catalog::find(const std::string& id) const {
    for(int i=0;i<static_cast<int>(upgrades.size());++i) if(upgrades[i].id==id) return i;return -1;
}

Game::Game(const Catalog& content,std::uint32_t runSeed):catalog(content) { reset(0,runSeed); }
float Game::random(float lo,float hi) { return lo+(hi-lo)*(float(rng()>>8)*(1.0f/16777216.0f)); }
void Game::reset(int selected,std::uint32_t runSeed) {
    character=std::clamp(selected,0,3);seed=runSeed;rng.seed(seed);nextEnemyId=1;nextMiniId=1;
    phase=Phase::Playing;player=Player{};elapsed=spawnClock=bossClock=shake=noticeTime=0;kills=gems=chests=bossesKilled=0;rerolls=banishes=3;notice.clear();chestRewards.clear();
    enemies.clear();projectiles.clear();zones.clear();nestEggs.clear();miniSnakes.clear();beams.clear();cobraPits.clear();cobras.clear();grapples.clear();leviathan={};cobraOverflow=1;orbitPhase=0;ouroborosActive=false;shieldTimer=0;shieldCharges=shieldMax=shieldRank=0;shieldCooldown=10;pickups.clear();particles.clear();damageNumbers.clear();owned.clear();banished.clear();events.clear();offers.fill(-1);
    enemies.reserve(EnemyCap);projectiles.reserve(ProjectileCap);zones.reserve(128);pickups.reserve(PickupCap+3);particles.reserve(1800);damageNumbers.reserve(180);owned.reserve(8);banished.reserve(catalog.upgrades.size());events.reserve(128);
    snakeDeaths.clear();snakeDeaths.reserve(SnakeDeathCapacity);nextSnakeDeath=0;playerDied=false;
    greySnakes.reset();greySnakes.enabled=true;
    player.segments.reserve(128);player.previousSegments.reserve(128);chestRewards.reserve(32);
    nestEggs.reserve(25);miniSnakes.reserve(60);beams.reserve(32);cobraPits.reserve(25);cobras.reserve(30);grapples.reserve(16);
    for(auto& fang:orbitFangStates){fang.angle=random(0,2*Pi);fang.orbit=random(4.5f,9.5f);fang.speed=random(4,8);fang.size=random(.55f,1);fang.radiusAmplitude=random(.3f,1);fang.radiusFrequency=random(1,3);fang.radiusPhase=random(0,2*Pi);fang.angleAmplitude=random(.03f,.12f);fang.angleFrequency=random(.8f,2.5f);fang.anglePhase=random(0,2*Pi);}
    for(int i=0;i<player.length;++i) player.segments.push_back({player.position.x-i*Tile,player.position.y});
    player.previousSegments=player.segments;
    const char* starts[]={"venom_shot","viper_fangs","tongue_lash","plague_mortar"};
    int starter=catalog.find(starts[character]);
    if(starter<0) for(int i=0;i<static_cast<int>(catalog.upgrades.size());++i) if(catalog.upgrades[i].kind==UpgradeKind::Weapon) {starter=i;break;}
    if(starter<0) throw std::runtime_error("Content catalog must contain at least one weapon");
    owned.push_back({starter,1,false,.1f});
    for(int i=0;i<24;++i) {const float a=random(0,Pi*2),r=random(16,130);pickups.push_back({add(player.position,{std::cos(a)*r,std::sin(a)*r}),1,PickupKind::Experience});}
    for(int i=0;i<10;++i) {const float a=random(0,Pi*2),r=random(105,170);spawnEnemy(EnemyKind::Red,add(player.position,{std::cos(a)*r,std::sin(a)*r}));}
}
Stats Game::stats() const {
    std::array<float,static_cast<int>(Stat::Count)> sums{};float cooldownProduct=1;double damageBonus=0;
    for(const auto& own:owned)if(own.definition>=0 && own.definition<int(catalog.upgrades.size()))for(const auto& m:catalog.upgrades[std::size_t(own.definition)].modifiers){
        if(m.stat==Stat::Cooldown)cooldownProduct*=float(std::pow(std::max(.01,1+m.perRank),own.rank));
        else if(m.stat==Stat::Damage && catalog.upgrades[std::size_t(own.definition)].id=="ravenous_maw")damageBonus+=(m.perRank*100+double(std::max(0,own.rank-1))*(40.0/7))/100;
        else if(m.stat==Stat::Damage)damageBonus+=m.perRank*own.rank;
        else sums[int(m.stat)]+=float(m.perRank*own.rank);
    }
    auto v=[&](Stat s){return sums[static_cast<int>(s)];};Stats s;
    s.damage=std::max(.1,1+damageBonus)*(character==0?1.15:1.0);
    s.cooldown=std::clamp(cooldownProduct,.025f,3.0f)*(character==1?.9f:1.0f);
    s.area=std::clamp(1+v(Stat::Area),.2f,5.0f)*(character==2?1.15f:1.0f);
    s.duration=std::clamp(1+v(Stat::Duration),.2f,6.0f)*(character==3?1.2f:1.0f);
    s.pickup=std::clamp(20+v(Stat::Pickup),4.0f,180.0f);s.crit=std::clamp(v(Stat::Crit),0.0f,1.0f);s.growth=std::clamp(1+v(Stat::Growth),.1f,5.0f);s.range=std::clamp(1+v(Stat::Range),.2f,4.0f);s.speed=std::clamp(1+v(Stat::Speed),.5f,2.0f);
    s.amount=std::clamp(int(std::round(v(Stat::Amount))),0,8);s.armor=std::clamp(int(std::round(v(Stat::Armor))),0,24);return s;
}
const EvolutionDef* Game::evolutionFor(int definition) const {
    if(definition<0 || definition>=static_cast<int>(catalog.upgrades.size())) return nullptr;
    for(const auto& e:catalog.evolutions) if(e.weapon==catalog.upgrades[definition].id) return &e;return nullptr;
}
std::string Game::upgradeName(const OwnedUpgrade& own) const {
    if(own.definition<0 || own.definition>=static_cast<int>(catalog.upgrades.size())) return {};
    if(own.evolved) if(const auto* e=evolutionFor(own.definition)) return e->name;
    return catalog.upgrades[own.definition].name;
}
void Game::rollOffers() {
    offers.fill(-1);int weapons=0,passives=0;
    for(const auto& own:owned) (catalog.upgrades[own.definition].kind==UpgradeKind::Weapon?weapons:passives)++;
    std::vector<int> candidates;std::vector<float> weights;candidates.reserve(catalog.upgrades.size());weights.reserve(catalog.upgrades.size());
    for(int i=0;i<static_cast<int>(catalog.upgrades.size());++i) {
        const auto& d=catalog.upgrades[i];if(std::find(banished.begin(),banished.end(),i)!=banished.end()) continue;
        const auto own=std::find_if(owned.begin(),owned.end(),[&](const OwnedUpgrade& o){return o.definition==i;});
        if(own!=owned.end()) {if(own->evolved || own->rank>=d.maxRank) continue;}
        else if((d.kind==UpgradeKind::Weapon && weapons>=WeaponSlots)||(d.kind==UpgradeKind::Passive && passives>=PassiveSlots)) continue;
        candidates.push_back(i);weights.push_back(d.weight*(own!=owned.end()?4.0f:1.0f));
    }
    for(int slot=0;slot<3 && !candidates.empty();++slot) {
        bool weaponFirst=slot==0 && std::any_of(candidates.begin(),candidates.end(),[&](int i){return catalog.upgrades[i].kind==UpgradeKind::Weapon;});
        float total=0;for(std::size_t j=0;j<candidates.size();++j) if(!weaponFirst || catalog.upgrades[candidates[j]].kind==UpgradeKind::Weapon) total+=weights[j];
        float ticket=random(0,total);std::size_t chosen=candidates.size()-1;
        for(std::size_t j=0;j<candidates.size();++j) {if(weaponFirst && catalog.upgrades[candidates[j]].kind!=UpgradeKind::Weapon) continue;ticket-=weights[j];if(ticket<=0) {chosen=j;break;}}
        offers[slot]=candidates[chosen];candidates.erase(candidates.begin()+static_cast<std::ptrdiff_t>(chosen));weights.erase(weights.begin()+static_cast<std::ptrdiff_t>(chosen));
    }
}
void Game::grantXp(int amount) {
    if(amount<=0 || phase==Phase::Dead || phase==Phase::Victory) return;
    player.xp+=amount;
    if(phase!=Phase::LevelUp && player.xp>=player.nextXp) {
        player.xp-=player.nextXp;++player.level;player.nextXp=8+player.level*4;
        phase=Phase::LevelUp;rollOffers();events.push_back({Event::Level});
    }
}
void Game::collectExperience(int amount,float xpMultiplier) {
    // Length follows raw fruit value. XP bonuses and queued level-ups must not
    // add extra growth; new segments unfold along the existing movement path.
    player.length=std::min(MaxPlayerLength,player.length+std::min(amount,MaxPlayerLength));
    gems+=amount;grantXp(std::max(1,int(std::round(amount*xpMultiplier))));
}
bool Game::choose(int choice) {
    if(phase!=Phase::LevelUp || choice<0 || choice>=3) return false;
    std::array<float,9> previousCooldown{};std::array<bool,9> previousWeapon{};
    for(const auto& own:owned)if(catalog.upgrades[std::size_t(own.definition)].kind==UpgradeKind::Weapon){const auto family=std::size_t(catalog.upgrades[std::size_t(own.definition)].pattern);previousCooldown[family]=weaponSpec(own).cooldown;previousWeapon[family]=true;}
    const int index=offers[choice];
    if(index<0) {if(choice!=0 || offers[0]>=0) return false;player.hp=std::min(player.maxHp,player.hp+25);gems+=10;}
    else {
        const auto& d=catalog.upgrades[index];auto own=std::find_if(owned.begin(),owned.end(),[&](const OwnedUpgrade& o){return o.definition==index;});
        if(own==owned.end()) {int count=0;for(const auto& o:owned) if(catalog.upgrades[o.definition].kind==d.kind) ++count;if(count>=(d.kind==UpgradeKind::Weapon?WeaponSlots:PassiveSlots)) return false;owned.push_back({index,1,false,.1f});}
        else {if(own->rank>=d.maxRank || own->evolved) return false;++own->rank;}
    }
    for(auto& own:owned)if(catalog.upgrades[std::size_t(own.definition)].kind==UpgradeKind::Weapon){const auto family=std::size_t(catalog.upgrades[std::size_t(own.definition)].pattern);if(previousWeapon[family])own.cooldown=std::max(0.0f,own.cooldown+weaponSpec(own).cooldown-previousCooldown[family]);}
    phase=Phase::Playing;offers.fill(-1);grantXp(0);
    if(player.xp>=player.nextXp) {const int pending=player.xp;player.xp=0;grantXp(pending);}return true;
}
bool Game::reroll() {if(phase!=Phase::LevelUp || rerolls<=0) return false;--rerolls;rollOffers();return true;}
bool Game::banish(int choice) {
    if(phase!=Phase::LevelUp || banishes<=0 || choice<0 || choice>=3 || offers[choice]<0) return false;
    banished.push_back(offers[choice]);--banishes;rollOffers();return true;
}
void Game::togglePause() {if(phase==Phase::Playing) phase=Phase::Paused;else if(phase==Phase::Paused) phase=Phase::Playing;}
void Game::spawnEnemy(EnemyKind kind,Vec position) {
    if(enemies.size()>=EnemyCap) return;Enemy e;e.id=nextEnemyId++;e.kind=kind;
    const float t=std::clamp(elapsed/RunDuration,0.0f,1.0f);const float normalHp=(1+std::floor(t*26))*100+player.level*10;
    e.hp=e.maxHp=kind==EnemyKind::Boss?5000+player.level*10:kind==EnemyKind::Splitter?600+player.level*10:kind==EnemyKind::Swift?normalHp*.6f:normalHp;
    e.speed=(kind==EnemyKind::Swift?40:kind==EnemyKind::Boss?15:kind==EnemyKind::Splitter?17:19)+std::min(14.0f,elapsed/65);
    e.radius=kind==EnemyKind::Boss?7.0f:3.5f;e.trailSize=kind==EnemyKind::Boss?8:kind==EnemyKind::Splitter?5:3;placeEnemy(e,position,true);enemies.push_back(e);
}
void Game::damageEnemy(Enemy& e,double damage,bool critical) {
    if(!e.alive || e.grabbed || damage<=0) return;e.hp-=damage;e.hitFlash=.09f;
    if(damageNumbers.size()<150) damageNumbers.push_back({e.position,std::max(1,int(std::round(damage))),.6f,critical});
    if(events.size()<100) events.push_back({Event::Hit});
    if(e.hp>0) return;e.alive=false;++kills;events.push_back({Event::Kill});
    // Keep the death notification before dead enemies are compacted. A
    // Consumption target can be finished by any equipped weapon.
    for(auto& beam:beams)if(beam.evolved && !beam.ancient && beam.target==e.id){beam.pendingAbsorb=true;beam.position=e.position;beam.target=0;}
    const int value=e.kind==EnemyKind::Boss?30:e.kind==EnemyKind::Splitter?3:1;
    if(!e.devoured && pickups.size()<PickupCap) pickups.push_back({e.position,value,PickupKind::Experience});
    else if(!e.devoured) {
        auto aggregate=std::find_if(pickups.begin(),pickups.end(),[](const Pickup& p){return p.kind==PickupKind::Experience;});
        if(aggregate!=pickups.end()) aggregate->value+=value;
        // Reserve one aggregate XP entry beyond the normal pool and two boss
        // chest slots. Heart healing and chest values must never absorb XP.
        else pickups.push_back({e.position,value,PickupKind::Experience});
    }
    if(e.kind==EnemyKind::Boss) {
        ++bossesKilled;
        auto chest=std::find_if(pickups.begin(),pickups.end(),[](const Pickup& p){return p.kind==PickupKind::Chest;});
        // Keep every boss reward. A full pickup pool combines chest quantities,
        // rather than silently deleting an evolution opportunity.
        if(pickups.size()>=PickupCap+2 && chest!=pickups.end())++chest->value;
        else {
            const Vec origin=gridPosition(e.position);
            auto freeCell=[&](Vec p){
                if(!inGridBounds(p))return false;
                if(greySnakes.blocks(p))return false;
                if(sameCell(p,player.position) || std::any_of(player.segments.begin(),player.segments.end(),[&](Vec s){return sameCell(p,s);}))return false;
                return std::none_of(pickups.begin(),pickups.end(),[&](const Pickup& p2){return p2.kind==PickupKind::Chest && sameCell(p2.position,p);});
            };
            Vec landing=origin;bool found=freeCell(landing);
            // Search in increasing Manhattan distance. Boss loot cannot land
            // outside the arena, beneath the player, or on another chest.
            for(int radius=1;!found && radius<ArenaTiles*2;++radius){
                for(int dx=-radius;dx<=radius && !found;++dx){
                    const int dy=radius-std::abs(dx);
                    for(int side=0;side<(dy==0?1:2);++side){
                        const Vec candidate{origin.x+float(dx*Tile),origin.y+float((side?-dy:dy)*Tile)};
                        if(freeCell(candidate)){landing=candidate;found=true;break;}
                    }
                }
            }
            pickups.push_back({landing,1,PickupKind::Chest});
        }
        notice="BOSS DEFEATED - CHEST DROPPED";noticeTime=4;shake=2;
    }
    else if(random(0,1)<.025f && pickups.size()<PickupCap) pickups.push_back({add(e.position,{4,0}),20,PickupKind::Heart});
    for(int i=0;i<5 && particles.size()<1600;++i) {const float a=random(0,2*Pi),speed=random(8,35);particles.push_back({e.position,{std::cos(a)*speed,std::sin(a)*speed},.35f,.35f,e.kind==EnemyKind::Boss?3:e.kind==EnemyKind::Splitter?2:1});particles.back().snakeDeath=true;}
}

SnakeDeath* Game::recordSnakeDeath(const SnakeDeath& death) {
    if(snakeDeaths.size()<SnakeDeathCapacity){snakeDeaths.push_back(death);return &snakeDeaths.back();}
    auto* slot=&snakeDeaths[nextSnakeDeath];*slot=death;nextSnakeDeath=(nextSnakeDeath+1)%SnakeDeathCapacity;return slot;
}
SnakeDeath* Game::recordSnakeDeath(const Enemy& enemy) {
    // Devoured victims belong to the consuming weapon's animation.
    if(enemy.devoured)return nullptr;
    SnakeDeath death;death.id=enemy.id;death.kind=SnakeDeathKind(int(enemy.kind));
    death.forceSerial=enemy.forceSerial;death.placementSerial=enemy.placementSerial;
    death.count=std::uint8_t(std::clamp(enemy.trailSize,0,8)+1);
    death.alpha=enemy.motionReady?std::clamp(enemy.moveClock/std::max(.01f,enemy.moveInterval),0.0f,1.0f):1;
    death.previous[0]=enemy.motionReady?enemy.previous:enemy.position;death.current[0]=enemy.position;
    for(std::size_t i=1;i<death.count;++i){death.current[i]=enemy.trail[i-1];death.previous[i]=enemy.motionReady?enemy.previousTrail[i-1]:death.current[i];}
    return recordSnakeDeath(death);
}
void Game::recordSnakeDeath(const MiniSnake& mini) {
    SnakeDeath death;death.id=mini.id;death.kind=mini.evolved?SnakeDeathKind::Hydra:SnakeDeathKind::Hatchling;
    death.count=std::uint8_t(std::clamp(mini.segmentCount,1,5));
    death.alpha=std::clamp(mini.moveClock/std::max(.01f,mini.moveInterval),0.0f,1.0f);
    for(std::size_t i=0;i<death.count;++i){death.current[i]=i<3?mini.segments[i]:mini.extraSegments[i-3];death.previous[i]=i<3?mini.previousSegments[i]:mini.previousExtraSegments[i-3];}
    recordSnakeDeath(death);
}

void Game::collectChest() {
    ++chests;events.push_back({Event::Chest});
    auto record=[&](ChestReward reward){
        reward.serial=std::uint64_t(chests);
        if(chestRewards.size()==32)chestRewards.erase(chestRewards.begin());
        chestRewards.push_back(std::move(reward));
    };
    for(auto& own:owned) {
        const auto& d=catalog.upgrades[own.definition];if(own.evolved || own.rank<d.maxRank) continue;
        const auto* e=evolutionFor(own.definition);if(!e) continue;const int passive=catalog.find(e->passive);
        if(std::any_of(owned.begin(),owned.end(),[&](const OwnedUpgrade& p){return p.definition==passive && p.rank>0;})) {
            const Pattern family=d.pattern;
            compact(projectiles,[&](const Projectile& p){return (!p.evolved&&p.pattern==family&&!p.cobraSpit)||(family==Pattern::Pit&&p.cobraSpit);});
            compact(zones,[&](const Zone& zone){return zone.pattern==family&&!zone.evolved;});
            if(family==Pattern::Nest){nestEggs.clear();miniSnakes.clear();}
            if(family==Pattern::Beam)compact(beams,[](const TrackingBeam& beam){return !beam.evolved&&!beam.ancient;});
            if(family==Pattern::Pit){cobraPits.clear();cobras.clear();cobraOverflow=1;}
            own.evolved=true;own.cooldown=0;notice="EVOLVED: "+e->name;noticeTime=5;shake=2;events.push_back({Event::Evolve});
            record({0,ChestRewardKind::Evolution,own.definition,own.rank,0,0,e->name,e->description});return;
        }
    }
    // Chests still advance the build before an evolution is ready.
    std::vector<std::size_t> eligible;
    std::array<float,9> previousCooldown{};std::array<bool,9> previousWeapon{};
    for(const auto& own:owned)if(catalog.upgrades[std::size_t(own.definition)].kind==UpgradeKind::Weapon){const auto family=std::size_t(catalog.upgrades[std::size_t(own.definition)].pattern);previousCooldown[family]=weaponSpec(own).cooldown;previousWeapon[family]=true;}
    for(std::size_t i=0;i<owned.size();++i) if(!owned[i].evolved && owned[i].rank<catalog.upgrades[owned[i].definition].maxRank) eligible.push_back(i);
    if(!eligible.empty()) {auto& own=owned[eligible[std::size_t(random(0,float(eligible.size())))%eligible.size()]];++own.rank;notice="CHEST: "+catalog.upgrades[own.definition].name+" +1";
        record({0,ChestRewardKind::Upgrade,own.definition,own.rank,0,0,catalog.upgrades[own.definition].name,catalog.upgrades[own.definition].description});}
    else {const float oldHp=player.hp;gems+=25;player.hp=std::min(player.maxHp,player.hp+25);notice="CHEST: +25 GOLD / RESTORED";
        record({0,ChestRewardKind::Treasure,-1,0,25,player.hp-oldHp,"BOSS TREASURE","25 gold and up to 25 health restored."});}
    for(auto& own:owned)if(catalog.upgrades[std::size_t(own.definition)].kind==UpgradeKind::Weapon){const auto family=std::size_t(catalog.upgrades[std::size_t(own.definition)].pattern);if(previousWeapon[family])own.cooldown=std::max(0.0f,own.cooldown+weaponSpec(own).cooldown-previousCooldown[family]);}
    noticeTime=4;
}

void Game::update(float dt,const Input& input) {
    events.clear();if(phase!=Phase::Playing || !std::isfinite(dt) || dt<=0) return;
    dt=std::min(dt,.1f);elapsed+=dt;shake=std::max(0.0f,shake-dt*8);noticeTime=std::max(0.0f,noticeTime-dt);
    for(auto& death:snakeDeaths)death.age+=dt;
    compact(snakeDeaths,[](const SnakeDeath& death){return death.age>=snakeDeathDuration(death.kind);});
    const Vec playerFrameStart=player.position;
    if(elapsed>=RunDuration) {elapsed=RunDuration;phase=Phase::Victory;events.push_back({Event::Win});return;}
    const Stats s=stats();updateShield(dt);player.invulnerable=std::max(0.0f,player.invulnerable-dt);
    if(player.collisionClock>=0)player.collisionClock=std::min(CollisionGraceSeconds,player.collisionClock+dt);
    updateGreySnakes(dt);
    if(greySnakes.blocks(player.position)){
        player.hp=0;phase=Phase::Dead;playerDied=true;notice="YOU HIT A GREY SNAKE BARRIER";
        noticeTime=5;shake=3;events.push_back({Event::Hurt});return;
    }
    // One immutable spec per equipped family is reused by all of its live
    // actors. JavaScript reads current weapon damage on impact/tick, while
    // trajectories, already-launched spit and existing bite damage stay fixed.
    std::array<WeaponSpec,9> current{};std::array<bool,9> equipped{};
    for(const auto& own:owned){const auto& definition=catalog.upgrades[std::size_t(own.definition)];if(definition.kind==UpgradeKind::Weapon){const auto family=std::size_t(definition.pattern);current[family]=weaponSpec(own);equipped[family]=true;}}
    Vec requested=input.direction;
    if(std::abs(requested.x)>0 || std::abs(requested.y)>0) {
        requested=std::abs(requested.x)>=std::abs(requested.y)?Vec{requested.x>0?1.0f:-1.0f,0}:Vec{0,requested.y>0?1.0f:-1.0f};
        if(requested.x*player.direction.x+requested.y*player.direction.y>=0) player.queued=requested;
    }
    const float moveInterval=.12f/s.speed;
    if(player.moveClock>=0 && player.moveInterval>0 && player.moveInterval!=moveInterval)
        player.moveClock=std::clamp(player.moveClock/player.moveInterval,0.0f,1.0f)*moveInterval;
    player.moveInterval=moveInterval;player.moveClock+=dt;
    while(player.moveClock>=player.moveInterval) {
        player.moveClock-=player.moveInterval;player.previous=player.position;player.previousSegments=player.segments;player.direction=player.queued;
        const Vec next=add(player.position,mul(player.direction,float(Tile)));
        const bool hitsWall=!inGridBounds(next),hitsGrey=greySnakes.blocks(next),growing=static_cast<int>(player.segments.size())<player.length;
        const std::size_t bodyEnd=!player.segments.empty() && !growing?player.segments.size()-1:player.segments.size();
        bool hitsBody=false;
        for(std::size_t i=1;i<bodyEnd;++i)if(sameCell(player.segments[i],next)){hitsBody=true;break;}
        // Match JavaScript's pre-move check: hold the complete snake at the
        // obstacle, allow steering, and keep combat running during grace.
        // Immunity blocks self-bites without starting grace. Arena walls and
        // grey barriers remain lethal, including with charged Serpent's Scales.
        if(hitsWall || hitsBody || hitsGrey) {
            if(hitsWall || hitsGrey || player.invulnerable<=0) {
                if(player.collisionClock<0)player.collisionClock=0;
                else if(player.collisionClock>=CollisionGraceSeconds-.000001f) {
                    player.collisionClock=-1;player.hp=0;phase=Phase::Dead;playerDied=true;
                    notice=hitsWall?"YOU HIT THE WALL":hitsGrey?"YOU HIT A GREY SNAKE BARRIER":"YOU BIT YOUR OWN TAIL";
                    noticeTime=5;shake=3;events.push_back({Event::Hurt});return;
                }
            }
            player.moveClock=0;break;
        }
        player.collisionClock=-1;
        player.position=next;
        if(player.segments.empty()) player.segments.push_back(player.position);
        if(static_cast<int>(player.segments.size())<player.length) player.segments.push_back(player.segments.back());
        for(std::size_t i=player.segments.size()-1;i>0;--i) player.segments[i]=player.segments[i-1];player.segments[0]=player.position;
        if(player.previousSegments.size()<player.segments.size()) player.previousSegments.push_back(player.previousSegments.empty()?player.previous:player.previousSegments.back());
    }
    const float spawnInterval=std::max(.045f,.75f/(1+elapsed/75));spawnClock+=dt;
    while(spawnClock>=spawnInterval) {
        spawnClock-=spawnInterval;const float a=random(0,Pi*2);const float roll=random(0,1);
        const EnemyKind kind=elapsed>35 && roll<.16f?EnemyKind::Splitter:elapsed>70 && roll<.34f?EnemyKind::Swift:EnemyKind::Red;
        spawnEnemy(kind,add(player.position,{std::cos(a)*random(155,225),std::sin(a)*random(125,185)}));
    }
    const float nextBoss=60+bossClock*120;
    if(elapsed>=nextBoss) {
        // The director runs before any spatial queries, so recycling an ordinary
        // enemy here preserves stable indices throughout the combat phase.
        if(enemies.size()>=EnemyCap) {
            const auto ordinary=std::find_if(enemies.begin(),enemies.end(),[](const Enemy& e){return e.kind!=EnemyKind::Boss;});
            if(ordinary!=enemies.end()) {*ordinary=std::move(enemies.back());enemies.pop_back();}
        }
        if(enemies.size()<EnemyCap) {
            const float a=random(0,Pi*2);spawnEnemy(EnemyKind::Boss,add(player.position,{std::cos(a)*155,std::sin(a)*125}));
            bossClock+=1;notice="AN ANCIENT SERPENT APPROACHES";noticeTime=4;
        }
    }
    moveEnemies(dt);
    for(auto& own:owned) if(catalog.upgrades[own.definition].kind==UpgradeKind::Weapon) {own.cooldown-=dt;if(own.cooldown<=0) fire(own,s);}
    updateNests(dt,s,equipped[std::size_t(Pattern::Nest)]?&current[std::size_t(Pattern::Nest)]:nullptr);
    updateWeaponActors(dt,s,current,equipped);
    const SpatialGrid grid(enemies);
    auto wasHit=[](const Projectile& shot,std::uint32_t id) {
        return id==shot.lastHit || std::find(shot.hitIds.begin(),shot.hitIds.begin()+shot.hitCount,id)!=shot.hitIds.begin()+shot.hitCount;
    };
    auto nearest=[&](Vec position,float range,std::uint32_t exclude,const Projectile* history=nullptr)->Enemy* {
        Enemy* best=nullptr;float dist=range*range;
        grid.query(position,range,[&](int i){
            auto& e=enemies[std::size_t(i)];const float d=distanceSq(position,e.position);
            if(e.alive && !e.grabbed && e.id!=exclude && (!history || !wasHit(*history,e.id)) && d<dist){best=&e;dist=d;}
        });
        return best;
    };
    struct SweptHit {int enemy;float time;};
    std::array<SweptHit,EnemyCap> hits;
    std::array<Projectile,128> splinters{};std::size_t splinterCount=0;
    std::array<Vec,EnemyCap> forcedPositions{};
    std::array<bool,EnemyCap> forced{};
    for(auto& p:projectiles) if(p.alive) {
        p.previousPosition=p.position;p.motionReady=true;
        const auto family=std::size_t(p.pattern);
        if(equipped[family] && current[family].evolved==p.evolved && !p.cobraSpit){
            const auto& spec=current[family];
            if(p.sourceDamage>0){p.sourceDamage=spec.baseDamage*(p.pattern==Pattern::Ricochet?std::ldexp(1.0,-p.generation):1);p.damageMultiplier=s.damage;p.damage=p.sourceDamage*p.damageMultiplier;}
            if(p.pattern==Pattern::Ricochet){p.maxLife=spec.duration;p.bounceRadius=(p.evolved?6.0f:5.0f)*Tile*s.area;}
        }
        const float travelDt=std::min(dt,std::max(0.0f,p.life));
        if(travelDt<=0){p.alive=false;continue;}
        p.life-=dt;
        if(p.turnSpeed>0){
            Enemy* target=nullptr;for(auto& candidate:enemies)if(candidate.id==p.targetId && candidate.alive && !candidate.grabbed && !wasHit(p,candidate.id)){target=&candidate;break;}
            if(!target && (p.pattern==Pattern::Ricochet || p.gatling))target=nearest(p.position,p.gatling?30*Tile:p.bounceRadius,p.lastHit,&p);
            if(target){p.targetId=target->id;const float speed=std::sqrt(lengthSq(p.velocity));const float angle=std::atan2(p.velocity.y,p.velocity.x),wanted=std::atan2(target->position.y-p.position.y,target->position.x-p.position.x);const float diff=std::remainder(wanted-angle,2*Pi),next=angle+std::clamp(diff,-p.turnSpeed*travelDt,p.turnSpeed*travelDt);p.velocity={std::cos(next)*speed,std::sin(next)*speed};}
        }
        const Vec start=p.position;
        p.position=add(start,mul(p.velocity,travelDt));p.age+=travelDt;
        if(p.pattern==Pattern::Mortar || p.pattern==Pattern::Pit){
            if(p.flightDuration>0)p.position=add(p.start,mul(sub(p.target,p.start),std::clamp(p.age/p.flightDuration,0.0f,1.0f)));
            if(p.life<=0){p.alive=false;
                if(p.pattern==Pattern::Mortar && zones.size()<128){Zone zone;zone.position=p.position;zone.radius=p.radius;zone.life=zone.maxLife=p.payloadDuration;zone.damage=p.damage;double rawDamage=p.damage;if(equipped[family]&&current[family].evolved==p.evolved){zone.radius=current[family].area;zone.life=zone.maxLife=current[family].duration;zone.damage=current[family].damage;rawDamage=current[family].unroundedDamage;}zone.tickInterval=p.evolved?.4f:.35f;zone.tick=p.evolved?0:zone.tickInterval;zone.damageFactor=1;zone.pattern=Pattern::Mortar;zone.evolved=p.evolved;zone.canCrit=p.evolved;zone.detonationDamage=p.evolved?float(std::round(6.25*rawDamage)):0;zones.push_back(zone);}
                else if(p.pattern==Pattern::Pit){if(p.evolved){if(leviathan.active){if(leviathan.growth==10)cobraOverflow+=.15*(6+std::max(0,p.generation-1)/3)*(1+s.amount);leviathan.growth=std::min(10,leviathan.growth+1);leviathan.absorbFlash=.6f;}else{leviathan={};leviathan.active=true;leviathan.position=p.position;leviathan.damage=p.damage;leviathan.halfWidth=p.radius*s.area;}}
                    else if(cobraPits.size()<25){CobraPit pit;pit.position=p.position;pit.life=p.payloadDuration;pit.damage=p.damage;pit.maxCobras=6+std::max(0,p.generation-1)/3;pit.cobraLife=5*s.duration;pit.spitCooldown=std::max(.5f,1.2f-float(std::max(0,p.generation-1))*.08f);cobraPits.push_back(pit);}}
            }continue;
        }
        const Vec delta=sub(p.position,start),middle=mul(add(start,p.position),.5f);
        const float a=lengthSq(delta);
        const float queryRadius=std::sqrt(a)*.5f+p.radius+8;
        std::size_t hitCount=0;
        grid.query(middle,queryRadius,[&](int i){
            const auto& enemy=enemies[std::size_t(i)];
            if(!enemy.alive || enemy.grabbed || wasHit(p,enemy.id))return;
            const Vec offset=sub(start,enemy.position);
            const float radius=enemy.radius+p.radius,c=lengthSq(offset)-radius*radius;
            float time=0;
            if(c>0){
                if(a<=.000001f)return;
                const float b=offset.x*delta.x+offset.y*delta.y,discriminant=b*b-a*c;
                if(discriminant<0)return;
                time=(-b-std::sqrt(discriminant))/a;
                if(time<0 || time>1)return;
            }
            if(hitCount<hits.size())hits[hitCount++]={i,time};
        });
        std::sort(hits.begin(),hits.begin()+static_cast<std::ptrdiff_t>(hitCount),[&](const SweptHit& lhs,const SweptHit& rhs){
            return lhs.time!=rhs.time?lhs.time<rhs.time:enemies[std::size_t(lhs.enemy)].id<enemies[std::size_t(rhs.enemy)].id;
        });
        for(std::size_t index=0;index<hitCount && p.alive;++index){
            auto& enemy=enemies[std::size_t(hits[index].enemy)];
            if(!enemy.alive || wasHit(p,enemy.id))continue;
            const Vec impact=add(start,mul(delta,hits[index].time));
            const bool impactCritical=p.critResolved?p.critical:(p.critical || random(0,1)<s.crit);
            const double pierceFactor=p.gatling?(p.impacts==0?1.0:p.impacts==1?.7:.5):1.0;
            const double baseDamage=p.sourceDamage>0?p.sourceDamage:p.damage,damageMultiplier=p.sourceDamage>0?p.damageMultiplier:1;
            const double hitDamage=p.cobraSpit?p.damage*(impactCritical?2:1):std::round(baseDamage*(1+p.damageRamp*p.impacts)*pierceFactor*(impactCritical?2:1)*damageMultiplier);
            damageEnemy(enemy,hitDamage,impactCritical);++p.impacts;p.lastHit=enemy.id;
            if(p.hitCount<p.hitIds.size())p.hitIds[p.hitCount++]=enemy.id;
            if(p.pattern==Pattern::Ricochet && impactCritical && p.evolved && p.generation<3){
                std::array<Enemy*,3> picks{};std::array<float,3> ranges{{1e30f,1e30f,1e30f}};
                for(auto& candidate:enemies)if(candidate.alive && !candidate.grabbed && !wasHit(p,candidate.id)){
                    const float range=distanceSq(impact,candidate.position);if(range>p.bounceRadius*p.bounceRadius)continue;
                    for(std::size_t j=0;j<picks.size();++j)if(range<ranges[j]){for(std::size_t k=picks.size()-1;k>j;--k){picks[k]=picks[k-1];ranges[k]=ranges[k-1];}picks[j]=&candidate;ranges[j]=range;break;}
                }
                const auto ricochetCount=std::count_if(projectiles.begin(),projectiles.end(),[](const Projectile& bullet){return bullet.alive && bullet.pattern==Pattern::Ricochet;});
                for(auto* target:picks)if(target && splinterCount<splinters.size() && std::size_t(ricochetCount)+splinterCount<60){
                    Projectile shard;shard.position=impact;shard.velocity=mul(unit(sub(target->position,impact)),26*Tile);shard.damage=p.damage*.5;shard.sourceDamage=p.sourceDamage*.5;shard.damageMultiplier=p.damageMultiplier;
                    shard.life=shard.maxLife=.6f*s.duration;shard.radius=.14f*Tile;shard.pattern=Pattern::Ricochet;shard.evolved=true;
                    shard.generation=p.generation+1;shard.targetId=target->id;shard.turnSpeed=14;shard.bounces=std::max(0,6-p.generation*2-1);shard.bounceRadius=p.bounceRadius;
                    shard.damageRamp=.2;shard.hitIds=p.hitIds;shard.hitCount=p.hitCount;shard.lastHit=enemy.id;splinters[splinterCount++]=shard;
                }
            }
            if(p.hitCount>=p.hitIds.size()){p.alive=false;break;}
            if(p.bounces>0){
                --p.bounces;p.position=impact;
                if(auto* target=nearest(impact,p.bounceRadius,enemy.id,&p)){p.targetId=target->id;p.velocity=mul(unit(sub(target->position,impact)),std::sqrt(lengthSq(p.velocity)));p.life=std::min(p.maxLife,p.life+(p.evolved?.25f:.3f)*s.duration);}
                else p.alive=false;
                break; // Remaining old-path hits are invalid after a ricochet.
            }
            if(p.pierce>0){--p.pierce;if(p.gatling){p.targetId=0;p.life=std::min(p.maxLife,p.life+.15f);}}else p.alive=false;
        }
        if(p.life<=0 || p.position.x<0 || p.position.x>ArenaSize || p.position.y<0 || p.position.y>ArenaSize)p.alive=false;
    }
    for(std::size_t index=0;index<splinterCount && projectiles.size()<ProjectileCap;++index)projectiles.push_back(splinters[index]);
    for(auto& z:zones) {
        const auto family=std::size_t(z.pattern);
        if(equipped[family]&&current[family].evolved==z.evolved){const auto& spec=current[family];
            if(z.pattern==Pattern::Mortar){z.damage=spec.damage;if(z.evolved)z.detonationDamage=float(std::round(6.25*spec.unroundedDamage));}
            else if(z.pattern==Pattern::Nova){if(z.evolved){z.damage=spec.unroundedDamage;z.baseRadius=spec.area;}else if(z.singleHit&&z.damage>0)z.damage=std::round(spec.unroundedDamage*.5);}
        }
        if(z.followPlayer || z.pattern==Pattern::Shot || (z.pattern==Pattern::Nova && z.evolved))z.position=player.position;
        if(z.pattern==Pattern::Nova && z.evolved)z.radius=(z.baseRadius>0?z.baseRadius:8*Tile*s.area)*(1+.1f*std::sin(elapsed*1.5f));
        if(z.delay>0){z.delay-=dt;if(z.delay>0)continue;}
        z.life-=dt;z.tick-=dt;
        if(z.pattern==Pattern::Mortar && z.evolved && z.life>0){
            z.pullClock+=dt;
            if(z.pullClock>=.16f){z.pullClock-=.16f;
                grid.query(z.position,z.radius+10,[&](int index){auto& enemy=enemies[index];
                    if(!enemy.alive || enemy.grabbed || distanceSq(enemy.position,z.position)>(z.radius+enemy.radius)*(z.radius+enemy.radius))return;
                    const auto i=std::size_t(index);const Vec current=forced[i]?forcedPositions[i]:enemy.position;
                    if(distanceSq(z.position,current)<=.000001f){enemy.slow=.5f;enemy.slowFactor=.2f;return;}
                    forcedPositions[i]=add(current,mul(unit(sub(z.position,current)),float(Tile)));forced[i]=true;enemy.slow=.5f;enemy.slowFactor=.2f;
                });
            }
        }
        if(z.life<=0 && z.pattern==Pattern::Mortar && z.evolved) {
            const float blastRadius=z.detonationDamage>0?z.radius:z.radius*1.35f;
            grid.query(z.position,blastRadius+10,[&](int index){
                auto& enemy=enemies[index];
                if(enemy.alive && distanceSq(enemy.position,z.position)<=(blastRadius+enemy.radius)*(blastRadius+enemy.radius)) {const bool critical=z.canCrit && random(0,1)<s.crit;damageEnemy(enemy,(z.detonationDamage>0?z.detonationDamage:z.damage*2.5f)*(critical?2:1),critical);}
            });
            for(int count=0;count<20 && particles.size()<1600;++count) {
                const float angle=random(0,2*Pi),speed=random(20,75);
                particles.push_back({z.position,{std::cos(angle)*speed,std::sin(angle)*speed},.55f,.55f,count%3==0?4:0});
            }
            shake=std::max(shake,2.0f);
            continue; // Expired zones are compacted below; the pulse fires once.
        }
        if(z.life<=0 || z.tick>0 || z.damage<=0) continue;
        z.tick=z.tickInterval;
        grid.query(z.position,z.radius+10,[&](int i){auto& e=enemies[i];if(!e.alive || distanceSq(e.position,z.position)>(z.radius+e.radius)*(z.radius+e.radius)) return;
            if(e.grabbed || z.pattern==Pattern::Shot)return;
            const bool critical=z.canCrit && random(0,1)<s.crit;const double scaled=z.damage*z.damageFactor;damageEnemy(e,z.pattern==Pattern::Nova&&z.evolved?std::round(scaled*(critical?2:1)):std::round(scaled)*(critical?2:1),critical);
        });
        if(z.singleHit)z.damage=0;
    }
    for(std::size_t index=0;index<enemies.size();++index)if(forced[index] && enemies[index].alive)placeEnemy(enemies[index],forcedPositions[index]);
    const SpatialGrid contactGrid(enemies);
    if(player.invulnerable<=0) {
        contactGrid.query(player.position,18,[&](int i){auto& e=enemies[i];if(player.invulnerable>0 || !e.alive || distanceSq(e.position,player.position)>(e.radius+3.0f)*(e.radius+3.0f)) return;
            if(e.grabbed)return;
            if(shieldCharges>0){--shieldCharges;if(shieldTimer<=0)shieldTimer=shieldCooldown;player.invulnerable=.25f;}
            else{player.hp-=std::max(3,20-s.armor);player.invulnerable=1.0f;shake=3;events.push_back({Event::Hurt});}
            placeEnemy(e,add(e.position,mul(unit(sub(e.position,player.position)),float(Tile)*2)));
        });
    }
    for(std::size_t i=0;i<pickups.size();) {
        auto& p=pickups[i];p.previous=p.position;p.motionReady=true;const float dist=distanceSq(p.position,player.position);const float radius=p.kind==PickupKind::Chest?12:s.pickup;
        if(dist<radius*radius && p.kind!=PickupKind::Chest) p.position=add(p.position,mul(unit(sub(player.position,p.position)),std::min(180*dt,std::sqrt(dist))));
        const bool collected=p.kind==PickupKind::Chest
            ?segmentDistanceSq(p.position,playerFrameStart,player.position)<=6*6
            :distanceSq(p.position,player.position)<9*9;
        if(collected) {
            const auto kind=p.kind;const int value=p.value;pickups[i]=pickups.back();pickups.pop_back();
            if(kind==PickupKind::Experience) collectExperience(value,s.growth);
            else if(kind==PickupKind::Heart) player.hp=std::min(player.maxHp,player.hp+value);
            else for(int reward=0;reward<value;++reward)collectChest();
            if(events.size()<100) events.push_back({Event::Pickup});
        } else ++i;
    }
    // Split after all queries; grid indices and vector references remain stable during combat.
    std::size_t i=0;
    while(i<enemies.size()) {
        if(enemies[i].alive) {++i;continue;}
        const Enemy dead=enemies[i];auto* death=recordSnakeDeath(dead);enemies[i]=enemies.back();enemies.pop_back();
        if(dead.kind==EnemyKind::Splitter && !dead.devoured){
            SplitBirth birth;birth.age=0;birth.count=death->count;birth.alpha=death->alpha;
            birth.normalX=std::int8_t(-dead.direction.y);birth.normalY=std::int8_t(dead.direction.x);
            if(birth.normalX==0 && birth.normalY==0)birth.normalY=1;
            const auto cell=[](float value){return std::uint8_t(std::clamp(int(std::lround(value/Tile)),0,ArenaTiles-1));};
            for(std::size_t part=0;part<birth.count;++part){const auto a=death->previous[part],b=death->current[part];birth.path[part]={cell(a.x),cell(a.y),cell(b.x),cell(b.y)};}
            for(int j=0;j<2;++j){
                const auto before=enemies.size();spawnEnemy(EnemyKind::Swift,add(dead.position,{j?6.0f:-6.0f,3}));
                if(enemies.size()>before){enemies.back().splitBirth=birth;enemies.back().splitBirth.side=std::int8_t((j?1:-1)*(birth.normalX?birth.normalX:birth.normalY));++death->splitChildren;}
            }
        }
    }
    compact(projectiles,[](const Projectile& p){return !p.alive;});compact(zones,[](const Zone& z){return z.life<=0;});
    for(auto& p:particles) {p.previous=p.position;p.motionReady=true;p.life-=dt;p.position=add(p.position,mul(p.velocity,dt));}
    compact(particles,[](const Particle& p){return p.life<=0;});
    for(auto& d:damageNumbers) {d.life-=dt;d.position.y-=dt*12;}
    compact(damageNumbers,[](const DamageNumber& d){return d.life<=0;});
    if(player.hp<=0) {player.hp=0;phase=Phase::Dead;playerDied=true;notice="THE SWARM CLAIMS ANOTHER";noticeTime=5;}
}

void Game::debugPopulate() {
    reset(0,0x5EEDu);elapsed=147;kills=428;gems=532;player.level=14;player.xp=31;player.nextXp=68;player.length=16;
    owned.clear();
    for(const auto& id:{"venom_shot","venom_nova","viper_fangs","plague_mortar","rapid_fire","toxic_expanse","graviton","serpents_scales"}) {const int index=catalog.find(id);if(index>=0) owned.push_back({index,3,false,random(0,1)});}
    player.segments.clear();
    for(int i=0;i<16;++i) {const int x=i<6?-i*8:-40;const int y=i<6?0:(i-5)*8;player.segments.push_back(add(player.position,{float(x),float(y)}));}
    player.previousSegments=player.segments;enemies.clear();pickups.clear();
    for(int i=0;i<52;++i) {const float a=random(0,Pi*2),r=random(45,168);spawnEnemy(i%8==0?EnemyKind::Splitter:i%11==0?EnemyKind::Swift:EnemyKind::Red,add(player.position,{std::cos(a)*r,std::sin(a)*r*.65f}));auto& e=enemies.back();constexpr Vec dirs[]{{1,0},{0,1},{-1,0},{0,-1}};e.direction=dirs[i%4];e.velocity=mul(e.direction,e.speed);for(int k=0;k<e.trailSize;++k)e.trail[std::size_t(k)]=gridPosition(sub(e.position,mul(e.direction,float((k+1)*Tile))));e.previousTrail=e.trail;}
    spawnEnemy(EnemyKind::Boss,add(player.position,{118,-36}));
    for(int i=0;i<65;++i) {const float a=random(0,Pi*2),r=random(12,175);pickups.push_back({add(player.position,{std::cos(a)*r,std::sin(a)*r*.65f}),i%9==0?3:1,PickupKind::Experience});}
    pickups.push_back({add(player.position,{-85,-47}),1,PickupKind::Chest});pickups.push_back({add(player.position,{63,38}),20,PickupKind::Heart});
    zones.push_back({add(player.position,{45,-25}),23,2,3,8,0,Pattern::Mortar,false});zones.push_back({player.position,33,.4f,.6f,9,0,Pattern::Nova,false});
    for(int i=0;i<11;++i) {const float a=random(0,Pi*2);Projectile p;p.position=add(player.position,{std::cos(a)*random(15,65),std::sin(a)*random(15,65)});p.velocity={std::cos(a)*100,std::sin(a)*100};p.damage=15;p.life=1.5f;projectiles.push_back(p);}
    notice="THE BROOD IS GROWING";noticeTime=3;phase=Phase::Playing;
}
}
