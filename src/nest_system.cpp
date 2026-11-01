#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace snake {
namespace {
constexpr std::size_t NestLimit = 25;
constexpr std::size_t BaseBroodLimit = 30;
constexpr std::size_t HydraBroodLimit = 60;
constexpr float HuntRange = 30.0f * Tile;

float distanceSquared(Vec a, Vec b) {
    const float dx=a.x-b.x,dy=a.y-b.y;
    return dx*dx+dy*dy;
}
float segmentDistanceSquared(Vec point,Vec start,Vec end) {
    const Vec delta{end.x-start.x,end.y-start.y};
    const float length=delta.x*delta.x+delta.y*delta.y;
    const float t=length>0?std::clamp(((point.x-start.x)*delta.x+(point.y-start.y)*delta.y)/length,0.0f,1.0f):0;
    return distanceSquared(point,{start.x+delta.x*t,start.y+delta.y*t});
}
Vec step(Vec point,Vec direction) {return {point.x+direction.x*Tile,point.y+direction.y*Tile};}

Vec chaseDirection(const MiniSnake& mini,const Enemy* target) {
    const float dx=target?target->position.x-mini.position.x:mini.direction.x;
    const float dy=target?target->position.y-mini.position.y:mini.direction.y;
    const Vec horizontal{dx<0?-1.0f:1.0f,0};
    const Vec vertical{0,dy<0?-1.0f:1.0f};
    std::array<Vec,4> options{};
    if(std::abs(dx)>=std::abs(dy))options={{horizontal,vertical,{0,-vertical.y},{-horizontal.x,0}}};
    else options={{vertical,horizontal,{-horizontal.x,0},{0,-vertical.y}}};
    if(!target)options={{mini.direction,{-mini.direction.y,mini.direction.x},{mini.direction.y,-mini.direction.x},{-mini.direction.x,-mini.direction.y}}};
    for(const Vec direction:options) {
        const Vec next=step(mini.position,direction);
        if(!inGridBounds(next) || sameCell(next,mini.segments[1]))continue;
        return direction;
    }
    // A stacked hatchling can turn toward a target immediately because its
    // neck has not occupied the cell behind its initial facing direction.
    // Once the body unfolds, the real neck cell prevents reversal.
    return {};
}
}

void Game::launchNest(const UpgradeDef&,const OwnedUpgrade& upgrade,const Stats& derived) {
    if(nestEggs.size()>=NestLimit)return;
    const WeaponSpec spec=weaponSpec(upgrade);
    const float range=spec.range;
    const float rangeSquared=range*range;
    // Score a bounded, deterministic sample of the nearest candidates. Each egg
    // targets a ground cell at cast time; neither the egg nor its nest homes.
    std::array<const Enemy*,12> candidates{};
    std::array<float,12> distances{};
    distances.fill(std::numeric_limits<float>::max());
    for(const Enemy& enemy:enemies) {
        if(!enemy.alive)continue;
        const float distance=distanceSquared(player.position,enemy.position);
        if(distance>rangeSquared)continue;
        for(std::size_t i=0;i<candidates.size();++i)if(distance<distances[i]) {
            for(std::size_t j=candidates.size()-1;j>i;--j) {candidates[j]=candidates[j-1];distances[j]=distances[j-1];}
            candidates[i]=&enemy;distances[i]=distance;break;
        }
    }
    const Enemy* target=nullptr;
    int bestScore=-1;
    constexpr float clusterRadius=4.0f*Tile;
    for(const Enemy* candidate:candidates) {
        if(!candidate)break;
        int score=0;
        for(const Enemy& enemy:enemies)if(enemy.alive && distanceSquared(candidate->position,enemy.position)<=clusterRadius*clusterRadius)++score;
        if(score>bestScore) {target=candidate;bestScore=score;}
    }
    if(!target)return;

    const int eggs=std::clamp(spec.amount,1,9);
    const Vec landing=gridPosition(target->position);
    constexpr std::array<Vec,8> spread{{{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{-1,-1},{1,-1}}};
    for(int index=0;index<eggs && nestEggs.size()<NestLimit;++index) {
        NestEgg egg;
        egg.start=player.position;
        egg.target=index==0?landing:gridPosition({landing.x+spread[std::size_t(index-1)].x*Tile*2,
                                                 landing.y+spread[std::size_t(index-1)].y*Tile*2});
        egg.flightDuration=.75f+float(index)*.1f;
        egg.hatchDelay=.5f;
        egg.snakeLife=spec.duration;
        egg.damage=spec.damage;
        egg.count=spec.broodCount;
        egg.evolved=upgrade.evolved;
        egg.broodLife=float(std::round(50.0*derived.duration)*.07);egg.broodDamage=upgrade.evolved?spec.damage*.75f:150;
        nestEggs.push_back(egg);
    }
}

void Game::updateNests(float dt,const Stats& derived,const WeaponSpec* current) {
    std::array<MiniSnake,60> children{};std::size_t childCount=0;
    // Existing summons advance first. A newly hatched brood begins with its full
    // snapshotted lifetime and cannot instantly bite during the hatching frame.
    for(MiniSnake& mini:miniSnakes) {
        if(!mini.alive)continue;
        if(flood.dangerous())flood.repel(mini.position,4,mini.direction,5);
        if(current && current->evolved==mini.evolved && mini.maxLife>0){
            const float total=mini.generation==1?float(std::round(50.0*derived.duration)*.07):current->duration;
            mini.life+=total-mini.maxLife;mini.maxLife=total;
            mini.broodLife=float(std::round(50.0*derived.duration)*.07);
        }
        mini.life-=dt;
        if(mini.life<=0) {
            if(mini.evolved && mini.generation==0){
                const int liveBrood=int(std::count_if(miniSnakes.begin(),miniSnakes.end(),[](const MiniSnake& actor){return actor.alive && actor.generation==1;}));
                for(int side:{-1,1})if(liveBrood+int(childCount)<30 && childCount<children.size()){
                    MiniSnake brood;brood.id=nextMiniId++;brood.position=brood.previous=mini.position;brood.direction={-mini.direction.y*side,mini.direction.x*side};
                    brood.segments.fill(brood.position);brood.previousSegments=brood.segments;brood.extraSegments.fill(brood.position);brood.previousExtraSegments=brood.extraSegments;
                    brood.evolved=true;brood.generation=1;brood.life=brood.maxLife=mini.broodLife;brood.damage=mini.broodDamage;brood.moveInterval=.07f;brood.wanderTicks=2+int(random(0,4));children[childCount++]=brood;
                }
            }
            recordSnakeDeath(mini);mini.alive=false;continue;
        }
        mini.moveClock+=dt;
        while(mini.moveClock>=mini.moveInterval && mini.alive) {
            mini.moveClock-=mini.moveInterval;
            const Enemy* nearest=nullptr;
            float nearestDistance=HuntRange*HuntRange;
            for(const Enemy& enemy:enemies)if(enemy.alive) {
                const float distance=distanceSquared(mini.position,enemy.position);
                if(distance<nearestDistance) {nearest=&enemy;nearestDistance=distance;}
            }
            Vec direction=chaseDirection(mini,nearest);
            if(mini.wanderTicks>0){--mini.wanderTicks;const Vec next=step(mini.position,mini.direction);if(inGridBounds(next) && !sameCell(next,mini.segments[1]))direction=mini.direction;}
            mini.previous=mini.position;
            mini.previousSegments=mini.segments;
            mini.previousExtraSegments=mini.extraSegments;
            if(direction.x==0 && direction.y==0)continue;
            mini.direction=direction;
            mini.position=step(mini.position,direction);
            if(mini.segmentCount==5){mini.extraSegments[1]=mini.extraSegments[0];mini.extraSegments[0]=mini.segments[2];}
            for(std::size_t segment=mini.segments.size()-1;segment>0;--segment)mini.segments[segment]=mini.segments[segment-1];
            mini.segments[0]=mini.position;
            // One swept bite per enemy per grid tick. Summons plow through a
            // swarm without turning into disposable homing projectiles.
            for(Enemy& enemy:enemies)if(enemy.alive) {
                const float radius=enemy.radius+Tile*.45f;
                if(segmentDistanceSquared(enemy.position,mini.previous,mini.position)<radius*radius)damageEnemy(enemy,mini.damage);
            }
        }
    }
    miniSnakes.erase(std::remove_if(miniSnakes.begin(),miniSnakes.end(),[](const MiniSnake& mini){return !mini.alive;}),miniSnakes.end());
    for(std::size_t i=0;i<childCount && miniSnakes.size()<HydraBroodLimit;++i)miniSnakes.push_back(children[i]);

    for(std::size_t index=0;index<nestEggs.size();) {
        NestEgg& egg=nestEggs[index];
        if(current && current->evolved==egg.evolved){egg.count=current->broodCount;egg.damage=current->damage;egg.snakeLife=current->duration;egg.broodLife=float(std::round(50.0*derived.duration)*.07);if(egg.evolved)egg.broodDamage=current->damage*.75f;}
        egg.age+=dt;
        if(egg.age<egg.flightDuration+egg.hatchDelay) {++index;continue;}
        const std::size_t limit=egg.evolved?HydraBroodLimit:BaseBroodLimit;
        int hydraCount=int(std::count_if(miniSnakes.begin(),miniSnakes.end(),[](const MiniSnake& mini){return mini.evolved && mini.generation==0;}));
        constexpr std::array<Vec,4> directions{{{1,0},{-1,0},{0,1},{0,-1}}};
        for(int count=0;count<egg.count && miniSnakes.size()<limit;++count) {
            if(egg.evolved && hydraCount>=30)break;
            MiniSnake mini;
            mini.id=nextMiniId++;
            mini.direction=directions[std::size_t(count)%directions.size()];
            const float offset=float(1+count/4)*Tile;
            mini.position=gridPosition({egg.target.x+mini.direction.x*offset,egg.target.y+mini.direction.y*offset});
            mini.previous=mini.position;
            mini.segments.fill(mini.position);
            mini.previousSegments=mini.segments;
            mini.extraSegments.fill(mini.position);mini.previousExtraSegments=mini.extraSegments;
            mini.life=mini.maxLife=egg.snakeLife;
            mini.damage=egg.damage;
            mini.evolved=egg.evolved;
            mini.segmentCount=egg.evolved?5:3;mini.broodLife=egg.broodLife;mini.broodDamage=egg.broodDamage;
            if(egg.evolved){++hydraCount;mini.wanderTicks=3+int(random(0,5));}
            miniSnakes.push_back(mini);
        }
        if(events.size()<100)events.push_back({Event::Shot});
        // Consume the nest once, even when the brood pool is full. Existing
        // summons are never evicted by repeated casts or an evolution.
        nestEggs[index]=nestEggs.back();nestEggs.pop_back();
    }
}
}
