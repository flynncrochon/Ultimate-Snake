#include "core.hpp"

#include <algorithm>
#include <cmath>

namespace snake {
void Game::updateGraviton(float dt) {
    int rank=0;
    for(const auto& own:owned)if(catalog.upgrades[std::size_t(own.definition)].id=="graviton")rank=std::clamp(own.rank,0,8);
    if(!rank){
        if(graviton.rank)for(auto& enemy:enemies)enemy.gravityHold=0;
        graviton={};graviton.lastDirection=player.direction;return;
    }
    if(!graviton.rank)graviton.lastDirection=player.direction;
    graviton.rank=rank;
    graviton.cooldown=std::max(0.0f,graviton.cooldown-dt);
    graviton.age=std::min(graviton.duration+.25f,graviton.age+dt);
}

void Game::triggerGraviton() {
    if(!graviton.rank || graviton.turns<4 || graviton.cooldown>0)return;
    const float radius=float((3+(graviton.rank-1)/2)*Tile);
    const auto inField=[&](const Enemy& enemy){
        return enemy.alive && !enemy.grabbed && std::abs(enemy.position.x-player.position.x)<=radius && std::abs(enemy.position.y-player.position.y)<=radius;
    };
    // Bank the charge if there is no target. A cast visits the bounded enemy
    // pool once, using no RNG, allocations, new actors or spatial-grid rebuild.
    if(std::none_of(enemies.begin(),enemies.end(),inField))return;
    graviton.center=player.position;graviton.radius=radius;
    graviton.duration=.65f+.1f*float(graviton.rank);graviton.age=0;
    graviton.cooldown=5;graviton.turns=0;++graviton.serial;
    graviton.empowered=false;
    for(auto& enemy:enemies)if(inField(enemy))enemy.gravityHold=std::max(enemy.gravityHold,graviton.duration);
    // Evolution builds on the same turn charge, rooted grid and held targets.
    // Its extra well shares Mortar's bounded actors and live damage contracts;
    // ordinary bombs continue to cast independently.
    if(zones.size()<128)for(const auto& own:owned){
        if(!own.evolved || catalog.upgrades[std::size_t(own.definition)].pattern!=Pattern::Mortar)continue;
        const auto spec=weaponSpec(own);
        Zone well;well.position=graviton.center;well.radius=graviton.radius;
        well.life=well.maxLife=graviton.duration;well.damage=spec.damage;
        well.pattern=Pattern::Mortar;well.evolved=true;well.canCrit=true;well.gridlockSeed=true;
        well.tickInterval=.4f;well.damageFactor=.5f;
        well.detonationDamage=float(std::round(6.25*spec.unroundedDamage*.5));
        zones.push_back(well);graviton.empowered=true;break;
    }
}
}
