// Offline progression experiment: actual combat, ground XP collection, offers,
// and boss chests. Steering/selection work is excluded from update timings.
#include "core.hpp"
#include "grid.hpp"
#include "../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace snake;
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
constexpr int Cells=ArenaTiles*ArenaTiles;
constexpr std::array<Vec,4> Directions{{{1,0},{0,1},{-1,0},{0,-1}}};
int cell(Vec point){return int(point.x)/Tile+int(point.y)/Tile*ArenaTiles;}
struct Collector {
    // Hamiltonian order plus safe shortcuts keeps the full 120-segment body
    // out of pockets. This is an offline collector, not gameplay auto-steering.
    static constexpr int Side=100, Origin=50, Count=Side*Side;
    std::array<int,Cells> order{};
    Collector(){
        order.fill(-1);int next=0;
        const auto append=[&](int x,int y){order[std::size_t(x+y*ArenaTiles)]=next++;};
        for(int x=Origin;x<Origin+Side;++x)append(x,Origin);
        for(int y=1;y<Side;++y){
            if(y%2)for(int x=Side-1;x>=1;--x)append(Origin+x,Origin+y);
            else for(int x=1;x<Side;++x)append(Origin+x,Origin+y);
        }
        for(int y=Side-1;y>=1;--y)append(Origin,Origin+y);
    }
    Vec steer(const Game& game){
        const int head=order[std::size_t(cell(game.player.position))];
        const int tail=order[std::size_t(cell(game.player.segments.back()))];
        const int gap=(tail-head+Count)%Count;
        int target=Count-1;
        for(const auto& pickup:game.pickups){
            const int index=order[std::size_t(cell(pickup.position))];
            if(index<0)continue;
            const int forward=(index-head+Count)%Count;
            if(forward>0)target=std::min(target,forward);
        }
        int best=-1,advance=-1;
        for(int i=0;i<4;++i){
            const auto d=Directions[std::size_t(i)];
            if(d.x*game.player.direction.x+d.y*game.player.direction.y<0)continue;
            const Vec next{game.player.position.x+d.x*Tile,game.player.position.y+d.y*Tile};
            if(!inGridBounds(next))continue;
            const int index=order[std::size_t(cell(next))];if(index<0)continue;
            const int forward=(index-head+Count)%Count;
            const int growthReserve=MaxPlayerLength-int(game.player.segments.size());
            if(forward>0 && forward+growthReserve<gap && forward<=target && forward>advance){best=i;advance=forward;}
        }
        return best<0?game.player.direction:Directions[std::size_t(best)];
    }
};

int priority(const Game& game,int definition){
    if(definition<0)return -1;
    const auto& upgrade=game.catalog.upgrades[std::size_t(definition)];
    const auto owned=std::find_if(game.owned.begin(),game.owned.end(),[&](const OwnedUpgrade& item){return item.definition==definition;});
    if(upgrade.kind==UpgradeKind::Weapon){
        const int weapons=int(std::count_if(game.owned.begin(),game.owned.end(),[&](const OwnedUpgrade& item){return game.catalog.upgrades[std::size_t(item.definition)].kind==UpgradeKind::Weapon;}));
        return owned!=game.owned.end()?100+owned->rank:weapons<2?110:30;
    }
    for(const auto& recipe:game.catalog.evolutions)if(recipe.passive==upgrade.id){
        const int weapon=game.catalog.find(recipe.weapon);
        if(std::any_of(game.owned.begin(),game.owned.end(),[&](const OwnedUpgrade& item){return item.definition==weapon && !item.evolved;}))return 90;
    }
    if(upgrade.id=="ravenous_maw")return 80;
    if(upgrade.id=="rapid_fire")return 70;
    return owned!=game.owned.end()?50:40;
}
void choose(Game& game){
    for(int pending=0;game.phase==Phase::LevelUp && pending<1024;++pending){
        std::array<int,3> choices{{0,1,2}};
        std::stable_sort(choices.begin(),choices.end(),[&](int a,int b){return priority(game,game.offers[std::size_t(a)])>priority(game,game.offers[std::size_t(b)]);});
        bool selected=false;for(int choice:choices)if(game.choose(choice)){selected=true;break;}
        if(!selected)throw std::runtime_error("Progression choices exhausted after a simultaneous chest");
    }
    if(game.phase==Phase::LevelUp)throw std::runtime_error("Unbounded XP queue");
}
Json run(const Catalog& catalog,int character,std::uint32_t seed,bool mortal){
    Game game(catalog,seed);game.reset(character,seed);
    // These runs isolate the researched combat economy. The native lifecycle
    // tests separately cover lethal flood/grey hazards, which this collector
    // cannot dodge. Contact is either explicitly immune or fully enabled.
    game.greySnakes.enabled=false;game.flood.enabled=false;
    if(!mortal)game.player.invulnerable=1000000;
    Collector collector;Vec direction=game.player.direction;
    std::vector<double> timings;timings.reserve(std::size_t(RunDuration*60));
    Json samples=Json::array();double priorDamage=0,firstEvolution=0;
    std::uint64_t priorXp=0;int priorKills=0,peakEnemies=0;
    for(int tick=1;tick<=int(RunDuration*60) && game.phase!=Phase::Dead && game.phase!=Phase::Victory;++tick){
        // A pickup between moves can retain a formerly vacating tail cell.
        // Re-plan before each movement attempt, including a blocked attempt.
        if(tick==1 || game.player.moveClock+FixedStep>=game.player.moveInterval)direction=collector.steer(game);
        const auto begin=Clock::now();game.update(FixedStep,{direction});
        timings.push_back(std::chrono::duration<double,std::micro>(Clock::now()-begin).count());
        choose(game);
        peakEnemies=std::max(peakEnemies,int(game.enemies.size()));
        if(firstEvolution==0 && std::any_of(game.owned.begin(),game.owned.end(),[](const OwnedUpgrade& item){return item.evolved;}))firstEvolution=game.elapsed;
        if(tick%3600==0 || game.phase==Phase::Dead || game.phase==Phase::Victory){
            const double seconds=timings.size()/60.0;
            const double interval=seconds-(samples.empty()?0:samples.back()["seconds"].get<double>());
            Json equipment=Json::array();for(const auto& own:game.owned)equipment.push_back({{"id",catalog.upgrades[std::size_t(own.definition)].id},{"rank",own.rank},{"evolved",own.evolved}});
            samples.push_back({{"seconds",seconds},{"run_clock",game.elapsed},{"level",game.player.level},{"hp",game.player.hp},
                {"effective_dps",(game.damageDealt-priorDamage)/interval},{"kills_per_second",(game.kills-priorKills)/interval},
                {"raw_xp_per_second",double(game.experienceCollected-priorXp)/interval},{"collected_raw_xp",game.experienceCollected},
                {"dropped_raw_xp",game.experienceDropped},{"enemies",game.enemies.size()},{"equipment",equipment}});
            priorDamage=game.damageDealt;priorXp=game.experienceCollected;priorKills=game.kills;
        }
    }
    const double total=std::accumulate(timings.begin(),timings.end(),0.0);
    const double cold=timings.empty()?0:timings.front();std::sort(timings.begin(),timings.end());
    const auto percentile=[&](double value){return timings.empty()?0:timings[std::size_t((timings.size()-1)*value)];};
    const char* names[]{"Cobra","Taipan","Mamba","Basilisk"};
    return {{"character",names[character]},{"seed",seed},{"contact_damage",mortal},{"victory",game.phase==Phase::Victory},
        {"seconds",game.elapsed},{"level",game.player.level},{"kills",game.kills},{"bosses_killed",game.bossesKilled},
        {"first_evolution_seconds",firstEvolution},{"peak_enemies",peakEnemies},{"samples",samples},
        {"update_us",{{"mean",total/std::max<std::size_t>(1,timings.size())},{"p99",percentile(.99)},{"worst",percentile(1)},{"first",cold}}}};
}
}
int main(int argc,char** argv){
    try{
        std::string data="data",output="artifacts/pacing/progression.json";int seeds=3;bool mortal=false;
        for(int i=1;i<argc;++i){const std::string option=argv[i];
            if(option=="--mortal"){mortal=true;continue;}
            if(i+1>=argc)throw std::runtime_error("Missing argument");const std::string value=argv[++i];
            if(option=="--data")data=value;else if(option=="--output")output=value;else if(option=="--seeds")seeds=std::stoi(value);else throw std::runtime_error("Unknown option: "+option);
        }
        if(seeds<1 || seeds>20)throw std::runtime_error("seeds must be 1..20");
        const auto catalog=Catalog::load(data);Json report{{"fixed_hz",60},{"run_seconds",RunDuration},{"runs",Json::array()},
            {"assumptions",{{"steering","Hamiltonian shortcuts toward actual ground loot in the central 100x100 cells; avoids retained body"},{"selection","two weapons, then owned ranks, recipe passives, Growth, cooldown"},
                {"contact_damage",mortal},{"grey_snakes",false},{"floods",false},{"free_xp",false},{"damage_metric","actual HP removed, including executes; excludes overkill"},
                {"timings","headless Game::update only; excludes steering, choices, rendering and I/O"}}}};
        for(int character=0;character<4;++character)for(int index=0;index<seeds;++index){
            auto result=run(catalog,character,0xACE000u+std::uint32_t(index)*977u,mortal);
            std::cerr<<result["character"]<<" seed "<<result["seed"]<<": level "<<result["level"]<<", survived "<<result["seconds"]<<"s, evolution "<<result["first_evolution_seconds"]<<"s\n";
            report["runs"].push_back(std::move(result));
        }
        const auto path=std::filesystem::path(output);if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);if(!(file<<report.dump(2)<<'\n'))throw std::runtime_error("Could not write report");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
