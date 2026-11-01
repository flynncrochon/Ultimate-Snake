#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace snake {
enum class PermanentUpgrade { Might, Vitality, Armor, Recovery, Cooldown, Growth, Reroll, Banish, Count };
constexpr std::size_t PermanentUpgradeCount=std::size_t(PermanentUpgrade::Count);
struct PermanentUpgradeDef {
    const char* id;
    const char* name;
    const char* description;
    int maxRank, initialCost;
};
const std::array<PermanentUpgradeDef,PermanentUpgradeCount>& permanentDefinitions();
struct PermanentUpgrades {
    std::array<int,PermanentUpgradeCount> ranks{};
    int rank(PermanentUpgrade upgrade) const;
    int totalRanks() const;
    void sanitize();
};
int permanentPrice(const PermanentUpgrades& upgrades,PermanentUpgrade upgrade);
// Total investment is independent of purchase order. Used to validate saves.
std::int64_t permanentInvestment(const PermanentUpgrades& upgrades);
}
