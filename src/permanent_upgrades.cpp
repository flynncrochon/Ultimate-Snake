#include "permanent_upgrades.hpp"

#include <algorithm>
#include <cmath>

namespace snake {
namespace {
constexpr std::array<PermanentUpgradeDef,PermanentUpgradeCount> Definitions{{
    {"might","Might","Direct weapon damage +5% per rank.",5,200},
    {"vitality","Vitality","Maximum health +10% per rank, compounded.",3,200},
    {"armor","Armor","Contact damage reduced by 1 per rank.",3,600},
    {"recovery","Recovery","Recover 0.1 HP per second per rank.",5,200},
    {"cooldown","Cooldown","Weapon cooldowns -2.5% per rank.",2,900},
    {"growth","Growth","Experience gained +3% per rank.",5,900},
    {"reroll","Reroll","Two extra level-up rerolls per rank.",5,1000},
    {"banish","Banish","Two extra level-up banishes per rank.",5,100}
}};
int fee(int bought){return bought<=0?0:int(std::floor(20.0*std::pow(1.1,bought)+1e-9));}
}
const std::array<PermanentUpgradeDef,PermanentUpgradeCount>& permanentDefinitions(){return Definitions;}
int PermanentUpgrades::rank(PermanentUpgrade upgrade) const {
    const auto index=std::size_t(upgrade);
    return index<ranks.size()?std::clamp(ranks[index],0,Definitions[index].maxRank):0;
}
int PermanentUpgrades::totalRanks() const {
    int count=0;for(std::size_t i=0;i<ranks.size();++i)count+=rank(PermanentUpgrade(i));return count;
}
void PermanentUpgrades::sanitize(){for(std::size_t i=0;i<ranks.size();++i)ranks[i]=rank(PermanentUpgrade(i));}
int permanentPrice(const PermanentUpgrades& upgrades,PermanentUpgrade upgrade){
    const auto index=std::size_t(upgrade);if(index>=Definitions.size())return 0;
    const int rank=upgrades.rank(upgrade);if(rank>=Definitions[index].maxRank)return 0;
    return Definitions[index].initialCost*(rank+1)+fee(upgrades.totalRanks());
}
std::int64_t permanentInvestment(const PermanentUpgrades& upgrades){
    std::int64_t total=0;
    for(std::size_t i=0;i<Definitions.size();++i){const int rank=upgrades.rank(PermanentUpgrade(i));total+=std::int64_t(Definitions[i].initialCost)*rank*(rank+1)/2;}
    for(int bought=1;bought<upgrades.totalRanks();++bought)total+=fee(bought);
    return total;
}
}
