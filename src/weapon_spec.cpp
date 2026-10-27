#include "core.hpp"
#include <algorithm>
#include <cmath>

namespace snake {
WeaponSpec Game::weaponSpec(const OwnedUpgrade& own) const {
    WeaponSpec w;
    if(own.definition<0 || own.definition>=int(catalog.upgrades.size()))return w;
    const auto& d=catalog.upgrades[std::size_t(own.definition)];const auto s=stats();
    const int level=std::clamp(own.rank,1,d.maxRank);const float r=float(level-1);
    w.pattern=d.pattern;w.evolved=own.evolved;
    w.damage=d.damage;w.cooldown=d.cooldown;w.speed=d.speed;w.range=d.range;w.area=d.area;w.duration=d.duration;w.amount=1+s.amount;
    if(!own.evolved)switch(d.pattern){
    case Pattern::Shot:w.damage=d.damage+30*r;w.cooldown=std::max(.18f,d.cooldown-.03f*r);w.amount=3+(level-1)/3+s.amount;w.area=d.area;break;
    case Pattern::Homing:w.damage=d.damage*float(1+level/3);w.cooldown=std::max(1.0f,d.cooldown-.25f*r);w.amount=level+s.amount;w.turnSpeed=10;break;
    case Pattern::Mortar:w.damage=d.damage*float(1+level/3);w.cooldown=std::max(1.5f,d.cooldown-.4f*r);w.area=d.area+float(Tile)*.35f*r;w.tickInterval=.35f;w.damageBonus=false;break;
    case Pattern::Nest:w.damage=d.damage*float(1+level/3);w.cooldown=std::max(2.5f,d.cooldown-.6f*r);w.broodCount=3+(level-1)/2;w.damageBonus=false;break;
    case Pattern::Nova:w.damage=d.damage*float(1+level/3);w.cooldown=std::max(1.2f,d.cooldown-.35f*r);w.area=d.area+float(Tile)*.4f*r;break;
    case Pattern::Beam:w.damage=d.damage*float(level);w.cooldown=std::max(2.0f,d.cooldown-.4f*r);w.range=d.range+float(Tile)*.5f*r;w.tickInterval=.2f;break;
    case Pattern::Ricochet:w.damage=d.damage*float(1+level/3);w.cooldown=std::max(1.5f,d.cooldown-.3f*r);w.bounces=3+level/2;w.turnSpeed=14;break;
    case Pattern::Pit:w.damage=d.damage*float(1+level/2);w.cooldown=std::max(3.5f,d.cooldown-.55f*r);w.broodCount=6+(level-1)/3;w.tickInterval=std::max(.5f,1.2f-.08f*r);break;
    case Pattern::Lash:w.damage=d.damage*float(1+level/2);w.cooldown=std::max(.6f,d.cooldown-.18f*r);w.range=d.range+float(Tile)*.35f*r;break;
    }
    if(own.evolved)switch(d.pattern){
    case Pattern::Shot:w.damage=180;w.cooldown=.1f;w.range=9.5f*Tile;w.area=1.1f*Tile;w.tickInterval=.3f;w.duration=.45f;w.amount=44;w.persistent=true;w.speed=0;break;
    case Pattern::Homing:w.damage=150;w.cooldown=.18f;w.speed=24*Tile;w.range=11*Tile;w.duration=.5f;w.area=.14f*Tile;w.amount=2+s.amount;w.pierce=2;w.turnSpeed=12;break;
    case Pattern::Mortar:w.damage=400;w.cooldown=5.5f;w.range=18*Tile;w.area=4*Tile;w.duration=2;w.tickInterval=.4f;w.amount=1;break;
    case Pattern::Nest:w.damage=200;w.cooldown=5.5f;w.range=12*Tile;w.duration=d.duration;w.broodCount=6;w.amount=1;w.damageBonus=false;break;
    case Pattern::Nova:w.damage=320;w.cooldown=.1f;w.area=8*Tile;w.duration=1;w.tickInterval=.5f;w.persistent=true;w.amount=1;break;
    case Pattern::Beam:w.damage=100;w.cooldown=.1f;w.range=10*Tile;w.duration=1;w.tickInterval=.1f;w.persistent=true;break;
    case Pattern::Ricochet:w.damage=600;w.cooldown=3;w.speed=22*Tile;w.range=12*Tile;w.duration=.9f;w.area=.2f*Tile;w.bounces=6;w.turnSpeed=14;break;
    case Pattern::Pit:w.damage=180;w.cooldown=6;w.range=14*Tile;w.area=.5f*Tile;w.duration=1;w.tickInterval=.2f;break;
    case Pattern::Lash:w.damage=400;w.cooldown=2.2f;w.speed=40*Tile;w.range=14*Tile;w.area=1.5f*Tile;w.duration=8;w.tickInterval=.1f;break;
    }
    if(own.evolved)if(const auto* evolution=evolutionFor(own.definition)){
        w.damage*=evolution->damage;w.cooldown*=evolution->cooldown;w.area*=evolution->area;w.range*=evolution->range;
    }
    // Replacing an attack must retain the offensive investment it could use.
    // Concentrate extra casts into damage for persistent/single-payload forms:
    // faster tick rates or duplicate wells/broods would increase hot-path work.
    if(own.evolved)switch(d.pattern){
    case Pattern::Shot:w.damage*=float(1+s.amount)/s.cooldown;break;
    case Pattern::Mortar:case Pattern::Nest:w.damage*=float(1+s.amount);break;
    case Pattern::Nova:w.damage*=float(1+s.amount)*(1+.5f*std::floor(std::max(0.0f,s.duration-1)/.25f+.0001f))/s.cooldown;break;
    case Pattern::Beam:w.damage*=s.duration/s.cooldown;break;
    case Pattern::Pit:w.damage/=s.cooldown;break;
    default:break;
    }
    w.baseDamage=w.damage;w.unroundedDamage=w.baseDamage*(w.damageBonus?s.damage:1);w.damage=float(std::round(w.unroundedDamage));
    w.cooldown*=s.cooldown;w.range*=s.range;
    // Projectile hit radii do not expand with Toxic Expanse in the JS source.
    if(d.pattern==Pattern::Mortar || d.pattern==Pattern::Nova || (d.pattern==Pattern::Shot && own.evolved))w.area*=s.area;
    if(d.pattern==Pattern::Nest)w.duration=float(std::round(std::round(double(d.duration)/.08)*s.duration)*.08);
    else if(d.pattern==Pattern::Mortar || (d.pattern==Pattern::Beam && !own.evolved) || d.pattern==Pattern::Ricochet || (d.pattern==Pattern::Pit && !own.evolved))w.duration*=s.duration;
    return w;
}
}
