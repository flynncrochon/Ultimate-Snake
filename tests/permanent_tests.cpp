#include "core.hpp"
#include "profile.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using namespace snake;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b,double epsilon=.0001){return std::abs(a-b)<=epsilon;}
Game fixture(const Catalog& catalog,const PermanentUpgrades& upgrades){
    Game game(catalog);game.reset(0,123,upgrades);game.enemies.clear();game.pickups.clear();game.owned.clear();
    game.greySnakes.enabled=false;game.flood.enabled=false;game.spawnClock=-1000000;game.bossClock=1000000;
    game.player.moveClock=-1000000;game.player.invulnerable=1000000;return game;
}
std::string contents(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
struct Files {
    std::filesystem::path directory,path,temporary;
    Files(){
        directory=std::filesystem::path(SNAKE_TEST_DATA_DIR).parent_path()/"artifacts/permanent/tests"/
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);path=directory/"profile.save";temporary=path;temporary+=".tmp";
    }
    ~Files(){std::error_code error;std::filesystem::remove(temporary,error);std::filesystem::remove(path,error);std::filesystem::remove(directory,error);}
    void put(const std::string& value){std::ofstream file(path,std::ios::binary);file<<value;require(bool(file),"Could not write fixture");}
};
}
int main(){
    int passed=0,failed=0;
    const auto test=[&](const char* name,const std::function<void()>& run){
        try{run();++passed;std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}
    };
    try{
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
        test("rank costs, global fees, caps and exact refunds conserve gold",[&]{
            Profile a,b;a.gold=b.gold=10000;
            require(permanentPrice(a.upgrades,PermanentUpgrade::Might)==200,"Initial researched price mismatch");
            require(purchasePermanent(a,PermanentUpgrade::Might),"First purchase failed");
            require(permanentPrice(a.upgrades,PermanentUpgrade::Might)==422,"Rank price or global fee mismatch");
            require(purchasePermanent(a,PermanentUpgrade::Armor) && purchasePermanent(a,PermanentUpgrade::Might),"Further purchases failed");
            require(purchasePermanent(b,PermanentUpgrade::Armor) && purchasePermanent(b,PermanentUpgrade::Might) && purchasePermanent(b,PermanentUpgrade::Might),"Reordered purchases failed");
            require(a.gold==8754 && a.investedGold==1246 && a.gold==b.gold,"Purchase order changed cost or money was lost");
            require(refundPermanent(a)==1246 && a.gold==10000 && a.upgrades.totalRanks()==0,"Refund did not return actual investment");
            require(refundPermanent(a)==0 && a.gold==10000,"Refund duplicated gold");
            a.gold=1000000;
            for(std::size_t i=0;i<PermanentUpgradeCount;++i){
                for(int rank=0;rank<permanentDefinitions()[i].maxRank;++rank)require(purchasePermanent(a,PermanentUpgrade(i)),"Maximum build purchase failed");
                const auto balance=a.gold;
                require(!purchasePermanent(a,PermanentUpgrade(i)) && a.gold==balance,"Maxed purchase charged gold");
            }
            require(a.investedGold==permanentInvestment(a.upgrades),"Recorded investment disagrees with order-independent prices");
            require(!purchasePermanent(a,PermanentUpgrade(-1)),"Invalid upgrade index was accepted");
            require(refundPermanent(a)>0 && a.gold==1000000,"Full build refund leaked currency");
            a.gold=MaxProfileGold-10;require(purchasePermanent(a,PermanentUpgrade::Might),"Large balance failed");
            require(bankGold(a,100)==10 && bankGold(a,-5)==0,"Banking overflowed the refund reserve");
            refundPermanent(a);require(a.gold==MaxProfileGold,"Saturated balance could not be fully refunded");
        });
        test("legacy profiles migrate records without inventing currency",[&]{
            std::istringstream old("SS1 321 17 1200 4 0\n");const auto loaded=readProfile(old);
            require(loaded.state==ProfileLoadState::Migrated,"Legacy profile was not recognized");
            auto profile=loaded.value;
            require(profile.bestKills==321 && profile.runs==17 && profile.bestSeconds==1200 && profile.victories==4 && !profile.sound,"Migration lost records/preferences");
            require(profile.gold==0 && profile.upgrades.totalRanks()==0,"Migration fabricated gold/upgrades");
            bankGold(profile,500);require(purchasePermanent(profile,PermanentUpgrade::Might),"Migrated profile could not purchase");
            std::ostringstream output;writeProfile(output,profile);std::istringstream input(output.str());const auto roundtrip=readProfile(input);
            require(roundtrip.state==ProfileLoadState::Loaded && roundtrip.value.gold==300 && roundtrip.value.investedGold==200 && roundtrip.value.upgrades.rank(PermanentUpgrade::Might)==1,"New save lost permanent progress");
            require(roundtrip.value.bestKills==321 && !roundtrip.value.sound,"New save lost legacy fields");
            for(const auto* malformed:{"", "SS1 -1 0 0 0 1", "SS2 0 0 0 0 1 -1 0 0 0 0 0 0 0 0 0", "SS2 0 0 0 0 1 100 200 9 0 0 0 0 0 0 0", "SS1 1 1 1 0 1 junk"}){
                std::istringstream bad(malformed);require(readProfile(bad).state==ProfileLoadState::Invalid,"Malformed profile partially loaded");
            }
        });
        test("atomic replacement preserves old saves on failure and protects unknown versions",[&]{
            Files files;const std::string legacy="SS1 90 2 300 0 1\n";files.put(legacy);
            std::filesystem::create_directory(files.temporary);
            Profile profile;
            {
                ProfileStorage storage(files.path.string(),true);profile=storage.loaded().value;bankGold(profile,500);
                require(storage.submit(profile),"Valid snapshot was not queued");storage.close(&profile);
                require(storage.state()==ProfileSaveState::Failed,"Write failure was hidden");
            }
            require(contents(files.path)==legacy,"Failed replacement destroyed the previous save");
            std::filesystem::remove(files.temporary);
            {
                ProfileStorage storage(files.path.string(),true);require(storage.submit(profile),"Retry snapshot was rejected");storage.close(&profile);
                require(storage.state()==ProfileSaveState::Saved,"Retry did not complete");
            }
            {ProfileStorage loaded(files.path.string());require(loaded.loaded().value.gold==500 && loaded.loaded().value.bestKills==90,"Atomic retry lost records");}
            const std::string future="SS99 retain this newer profile\n";files.put(future);
            {ProfileStorage storage(files.path.string(),true);require(storage.state()==ProfileSaveState::Blocked && !storage.submit(profile),"Unknown save version was writable");storage.close(&profile);}
            require(contents(files.path)==future,"Unknown future version was overwritten");
        });
        test("bounded asynchronous saves flush the latest snapshot and read-only previews never write",[&]{
            Files files;Profile profile;profile.gold=1000;
            {
                ProfileStorage storage(files.path.string(),true);
                for(int i=0;i<10000;++i){profile.gold=1000+i;profile.bestKills=i;(void)storage.submit(profile);}
                storage.close(&profile);
                require(storage.state()==ProfileSaveState::Saved,"Latest shutdown snapshot was not flushed");
            }
            const auto before=contents(files.path);
            {ProfileStorage readonly(files.path.string(),false);require(readonly.loaded().value.gold==10999 && readonly.loaded().value.bestKills==9999,"Coalescing lost the final snapshot");profile.gold=42;require(readonly.submit(profile),"Temporary preview refused in-memory state");readonly.close(&profile);}
            require(contents(files.path)==before,"Read-only preview modified the save");
        });
        test("permanent bonuses preserve geometry, movement and source weapon exclusions",[&]{
            PermanentUpgrades all;for(std::size_t i=0;i<PermanentUpgradeCount;++i)all.ranks[i]=permanentDefinitions()[i].maxRank;
            Game base(catalog,42),boosted(catalog,42);boosted.reset(0,42,all);
            const auto stats=boosted.stats();
            require(near(stats.damage,1.4375) && near(stats.cooldown,.95) && near(stats.growth,1.15) && near(stats.recovery,.5) && stats.armor==3,"Permanent stat budgets mismatch");
            require(near(boosted.player.maxHp,133.1) && boosted.player.hp==boosted.player.maxHp && boosted.rerolls==13 && boosted.banishes==13,"Starting health or run resources mismatch");
            require(stats.speed==1 && stats.area==1 && stats.duration==1 && stats.range==1 && stats.amount==0 && stats.pickup==Tile,"Permanent upgrades altered geometry or movement");
            for(std::size_t i=0;i<base.enemies.size();++i)require(base.enemies[i].position.x==boosted.enemies[i].position.x && base.enemies[i].position.y==boosted.enemies[i].position.y,"Applying bonuses consumed combat RNG");
            for(const auto& definition:catalog.upgrades)if(definition.kind==UpgradeKind::Weapon){
                const int index=catalog.find(definition.id);OwnedUpgrade own{index,1,false,0};
                const auto before=base.weaponSpec(own),after=boosted.weaponSpec(own);
                require(before.area==after.area && before.range==after.range && before.speed==after.speed && before.duration==after.duration && before.amount==after.amount,"Permanent upgrades changed attack geometry");
                if(!before.damageBonus)require(before.damage==after.damage,"Might bypassed a source damage exclusion");
            }
            boosted.reset(0,42);require(boosted.player.maxHp==100 && boosted.stats().damage==base.stats().damage && boosted.rerolls==3 && boosted.permanentUpgrades().totalRanks()==0,"Fixture reset leaked permanent upgrades");
        });
        test("three-percent Growth is retained across one hundred one-point fruit",[&]{
            PermanentUpgrades upgrades;upgrades.ranks[std::size_t(PermanentUpgrade::Growth)]=1;
            auto separate=fixture(catalog,upgrades),merged=fixture(catalog,upgrades);
            separate.player.nextXp=merged.player.nextXp=1000000;
            for(int i=0;i<100;++i){separate.pickups.push_back({separate.player.position,1,PickupKind::Experience});separate.update(FixedStep,{});}
            merged.pickups.push_back({merged.player.position,100,PickupKind::Experience});merged.update(FixedStep,{});
            require(separate.player.xp==103 && merged.player.xp==103 && near(separate.player.xpRemainder,0),"Fractional permanent XP was lost or rounded per pickup");
            require(separate.player.length==107 && merged.player.length==107,"Growth bonus created extra body length");
        });
        test("Recovery uses simulation time and Armor respects normal contact rules",[&]{
            PermanentUpgrades upgrades;upgrades.ranks[std::size_t(PermanentUpgrade::Recovery)]=5;
            auto game=fixture(catalog,upgrades);game.player.hp=50;
            for(int tick=0;tick<60;++tick)game.update(FixedStep,{});
            require(near(game.player.hp,50.5,.002),"Recovery was not 0.5 HP per simulation second");
            game.togglePause();const float hp=game.player.hp;
            for(int tick=0;tick<600;++tick)game.update(FixedStep,{});
            require(game.player.hp==hp,"Pause advanced Recovery");game.togglePause();game.grantXp(game.player.nextXp);
            for(int tick=0;tick<600;++tick)game.update(FixedStep,{});
            require(game.player.hp==hp,"Level selection advanced Recovery");
            game.phase=Phase::Dead;game.player.hp=0;game.update(FixedStep,{});require(game.player.hp==0,"Recovery revived a dead run");
            upgrades={};upgrades.ranks[std::size_t(PermanentUpgrade::Armor)]=3;auto armored=fixture(catalog,upgrades);
            armored.player.invulnerable=0;Enemy enemy;enemy.id=1000;enemy.position=armored.player.position;enemy.speed=0;enemy.trail.fill(enemy.position);armored.enemies.push_back(enemy);
            armored.update(FixedStep,{});require(armored.player.hp==83,"Permanent Armor did not reduce normal contact damage");
        });
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
