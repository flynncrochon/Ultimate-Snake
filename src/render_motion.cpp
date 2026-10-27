#include "render_motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace snake::render {
namespace {
template<std::size_t N> struct Snapshot {
    SnakePose<N> pose;
    float interval = .12f;
    bool ready = true;
};
float phase(float clock,float interval) {
    if(!std::isfinite(clock) || !std::isfinite(interval))return 1;
    return std::clamp(clock/std::max(.01f,interval),0.0f,1.0f);
}
bool same(Vec a,Vec b) {return a.x==b.x && a.y==b.y;}
template<std::size_t N> bool followsPath(const SnakePose<N>& current,const SnakePose<N>& old) {
    const auto count=std::min(old.count,current.count);
    bool moved=false;
    for(std::size_t i=0;i<count;++i)if(!same(old.current[i],current.current[i])){moved=true;break;}
    if(!moved)return true;
    for(std::size_t i=0;i<count;++i){
        if(!same(current.previous[i],old.current[i]))return false;
        const float dx=current.current[i].x-current.previous[i].x,dy=current.current[i].y-current.previous[i].y;
        if((dx!=0 && dy!=0) || std::abs(dx)+std::abs(dy)>Tile+.001f)return false;
    }
    return true;
}

Snapshot<128> snapshot(const Player& actor) {
    Snapshot<128> result;result.interval=actor.moveInterval;
    auto& pose=result.pose;pose.count=std::min(actor.segments.size(),pose.current.size());
    pose.alpha=phase(actor.moveClock,actor.moveInterval);pose.direction=actor.direction;
    for(std::size_t i=0;i<pose.count;++i){pose.current[i]=actor.segments[i];pose.previous[i]=i<actor.previousSegments.size()?actor.previousSegments[i]:actor.segments[i];}
    return result;
}
Snapshot<9> snapshot(const Enemy& actor) {
    Snapshot<9> result;result.interval=actor.moveInterval;result.ready=actor.motionReady;
    auto& pose=result.pose;pose.count=std::size_t(std::clamp(actor.trailSize,0,8))+1;
    pose.alpha=actor.motionReady?phase(actor.moveClock,actor.moveInterval):1;pose.direction=actor.direction;
    pose.current[0]=actor.position;pose.previous[0]=actor.motionReady?actor.previous:actor.position;
    for(std::size_t i=1;i<pose.count;++i){pose.current[i]=actor.trail[i-1];pose.previous[i]=actor.motionReady?actor.previousTrail[i-1]:actor.trail[i-1];}
    return result;
}
Snapshot<5> snapshot(const MiniSnake& actor) {
    Snapshot<5> result;result.interval=actor.moveInterval;
    auto& pose=result.pose;pose.count=std::size_t(std::clamp(actor.segmentCount,1,5));
    pose.alpha=phase(actor.moveClock,actor.moveInterval);pose.direction=actor.direction;
    for(std::size_t i=0;i<pose.count;++i){pose.current[i]=i<3?actor.segments[i]:actor.extraSegments[i-3];pose.previous[i]=i<3?actor.previousSegments[i]:actor.previousExtraSegments[i-3];}
    return result;
}
struct GreySnapshot {
    Snapshot<1> motion;
    std::size_t count=0;
    bool alive=false;
};
GreySnapshot greySnapshot(const GreySnake& actor) {
    GreySnapshot result;result.count=actor.count;result.alive=actor.alive;
    result.motion.interval=float(GreyMoveSeconds);
    auto& pose=result.motion.pose;pose.count=actor.count?1:0;
    if(actor.count){pose.previous[0]=actor.previous;pose.current[0]=actor.path[actor.count-1];}
    pose.direction=actor.direction;pose.alpha=actor.alive || actor.settling?phase(float(actor.moveClock),float(GreyMoveSeconds)):1;
    return result;
}

template<std::size_t N> SnakePose<N> sample(const Snapshot<N>& now,const Snapshot<N>* before,float frameAlpha,bool active) {
    const auto& current=now.pose;
    if(!active || !before || !before->ready || !now.ready || current.count==0 || before->pose.count==0 || !std::isfinite(frameAlpha))return current;
    // Normal gameplay has at most one grid move per fixed update. Deliberately
    // extreme test speeds safely use their actual current pose instead of
    // guessing an intermediate turn which was never retained by simulation.
    if(before->interval<FixedStep || now.interval<FixedStep)return current;
    const auto& old=before->pose;const auto count=std::min(old.count,current.count);
    bool moved=false;
    for(std::size_t i=0;i<count;++i)if(!same(old.current[i],current.current[i])){moved=true;break;}
    if(moved){
        // A regular step preserves the old waypoints as its previous path.
        // Knockback, recycling, new actors and arbitrary test relocations do
        // not satisfy this; never interpolate a teleport across the arena.
        for(std::size_t i=0;i<count;++i){
            if(!same(current.previous[i],old.current[i]))return current;
            const float dx=current.current[i].x-current.previous[i].x,dy=current.current[i].y-current.previous[i].y;
            if((dx!=0 && dy!=0) || std::abs(dx)+std::abs(dy)>Tile+.001f)return current;
        }
    }
    const bool crossed=moved || current.alpha+.000001f<old.alpha;
    const float end=current.alpha+(crossed?1.0f:0.0f);
    const float amount=std::clamp(frameAlpha,0.0f,1.0f);
    const float progress=old.alpha+(end-old.alpha)*amount;
    if(crossed && progress>=1){auto result=current;result.alpha=std::clamp(progress-1,0.0f,1.0f);return result;}
    auto result=old;result.alpha=std::clamp(progress,0.0f,1.0f);return result;
}

struct Correction {
    Vec offset;
    float speed=24*Tile;
};
struct EnemySnapshot {
    Snapshot<9> motion;
    std::uint32_t forceSerial=0,placementSerial=0;
    Correction correction;
};
EnemySnapshot enemySnapshot(const Enemy& actor) {
    EnemySnapshot result;result.motion=snapshot(actor);result.forceSerial=actor.forceSerial;result.placementSerial=actor.placementSerial;return result;
}
EnemySnapshot enemySnapshot(const SnakeDeath& death) {
    EnemySnapshot result;result.forceSerial=death.forceSerial;result.placementSerial=death.placementSerial;
    auto& pose=result.motion.pose;pose.previous=death.previous;pose.current=death.current;pose.count=death.count;pose.alpha=death.alpha;return result;
}
Vec headPosition(const SnakePose<9>& pose) {
    return {pose.previous[0].x+(pose.current[0].x-pose.previous[0].x)*pose.alpha,
            pose.previous[0].y+(pose.current[0].y-pose.previous[0].y)*pose.alpha};
}
Correction enemyCorrection(const EnemySnapshot& now,const EnemySnapshot& before,float frameAlpha) {
    if(now.placementSerial!=before.placementSerial || !now.motion.ready || !before.motion.ready)return {};
    auto correction=before.correction;
    auto& offset=correction.offset;
    if(now.forceSerial!=before.forceSerial){
        // Start at the last displayed head, including an unfinished grid step
        // and any earlier shove. Logical collision positions stay untouched.
        const Vec from=headPosition(before.motion.pose),to=headPosition(now.motion.pose);
        offset.x+=from.x-to.x;offset.y+=from.y-to.y;
        // Stacked volleys can shove much farther than one lash. Finish those
        // within the same short interval rather than trailing combat for seconds.
        correction.speed=std::max(float(24*Tile),(std::abs(offset.x)+std::abs(offset.y))*8);
    }else if(same(offset,{}) || !followsPath(now.motion.pose,before.motion.pose))return {};
    // A lash's three-cell shove takes about 0.125s. Fixed-tick decay and pure
    // fractional sampling keep catch-up, pause and display cadence identical.
    const float distance=std::abs(offset.x)+std::abs(offset.y);
    const float travel=correction.speed*FixedStep*std::clamp(frameAlpha,0.0f,1.0f);
    if(distance<=travel)return {};
    const float remaining=1-travel/distance;
    offset={offset.x*remaining,offset.y*remaining};return correction;
}

SnakePose<9> sampleEnemySnapshot(const EnemySnapshot& now,const EnemySnapshot* before,float frameAlpha,bool active) {
    auto result=sample(now.motion,before?&before->motion:nullptr,frameAlpha,active);
    if(active && before && std::isfinite(frameAlpha)){
        if(now.forceSerial!=before->forceSerial)result=now.motion.pose;
        result.offset=enemyCorrection(now,*before,frameAlpha).offset;
    }
    return result;
}

template<typename Value,std::size_t Capacity> struct Table {
    struct Bucket {std::uint32_t generation=0,id=0;Value value;};
    std::array<Bucket,Capacity> buckets{};
    void insert(std::uint32_t id,std::uint32_t generation,const Value& value) {
        if(id==0)return;
        std::size_t index=(std::size_t(id)*2654435761u)&(Capacity-1);
        for(std::size_t probe=0;probe<Capacity;++probe,index=(index+1)&(Capacity-1)){
            auto& slot=buckets[index];
            if(slot.generation!=generation || slot.id==id){slot.id=id;slot.generation=generation;slot.value=value;return;}
        }
    }
    const Value* find(std::uint32_t id,std::uint32_t generation) const {
        if(id==0 || generation==0)return nullptr;
        std::size_t index=(std::size_t(id)*2654435761u)&(Capacity-1);
        for(std::size_t probe=0;probe<Capacity;++probe,index=(index+1)&(Capacity-1)){
            const auto& slot=buckets[index];
            if(slot.generation!=generation)return nullptr;
            if(slot.id==id)return &slot.value;
        }
        return nullptr;
    }
};
}

struct MotionHistory::Storage {
    std::uint32_t generation=0;
    Snapshot<128> player;
    // Separate generations retain corrections through reordering and hash
    // collisions without overwriting an actor still needed by this update.
    // Live enemies plus retained death poses stay below half capacity.
    std::array<Table<EnemySnapshot,8192>,2> enemies;
    std::size_t enemyTable=0;
    Table<Snapshot<5>,128> minis;
    std::array<GreySnapshot,GreyTrailCapacity> greys;
    std::size_t greyCount=0;
};

MotionHistory::MotionHistory():storage(std::make_unique<Storage>()) {}
MotionHistory::~MotionHistory()=default;
void MotionHistory::clear() {storage->generation=0;}
void MotionHistory::beginStep(const Game& game) {
    // Zeroing is only needed on the practically unreachable generation wrap,
    // or the first frame after clear; ordinary ticks rewrite occupied entries.
    const auto previousGeneration=storage->generation;
    if(++storage->generation==0)storage->generation=1;
    if(storage->generation==1){for(auto& table:storage->enemies)for(auto& slot:table.buckets)slot.generation=0;for(auto& slot:storage->minis.buckets)slot.generation=0;}
    storage->player=snapshot(game.player);
    const auto& old=storage->enemies[storage->enemyTable];
    auto& current=storage->enemies[storage->enemyTable^1];
    for(const auto& death:game.snakeDeaths)if(death.kind<=SnakeDeathKind::Boss && death.forceSerial!=0){
        auto value=enemySnapshot(death);
        if(const auto* before=old.find(death.id,previousGeneration))value.correction=enemyCorrection(value,*before,1);
        current.insert(death.id,storage->generation,value);
    }
    for(const auto& enemy:game.enemies)if(enemy.alive){
        auto value=enemySnapshot(enemy);
        if(enemy.forceSerial!=0)if(const auto* before=old.find(enemy.id,previousGeneration))value.correction=enemyCorrection(value,*before,1);
        current.insert(enemy.id,storage->generation,value);
    }
    storage->enemyTable^=1;
    for(const auto& mini:game.miniSnakes)if(mini.alive)storage->minis.insert(mini.id,storage->generation,snapshot(mini));
    storage->greyCount=game.greySnakes.trails.size();
    for(std::size_t i=0;i<storage->greyCount;++i)storage->greys[i]=greySnapshot(game.greySnakes.trails[i]);
}
SnakePose<128> MotionHistory::samplePlayer(const Player& player,float frameAlpha,bool active) const {return sample(snapshot(player),storage->generation?&storage->player:nullptr,frameAlpha,active);}
SnakePose<9> MotionHistory::sampleEnemy(const Enemy& enemy,float frameAlpha,bool active) const {
    if(!active)return snapshot(enemy).pose;
    const auto* before=storage->enemies[storage->enemyTable].find(enemy.id,storage->generation);
    if(!before || (enemy.forceSerial==before->forceSerial && same(before->correction.offset,{})))
        return sample(snapshot(enemy),before?&before->motion:nullptr,frameAlpha,true);
    const auto now=enemySnapshot(enemy);
    return sampleEnemySnapshot(now,before,frameAlpha,active);
}
SnakePose<9> MotionHistory::sampleDeath(const SnakeDeath& death,float frameAlpha,bool active) const {
    const auto now=enemySnapshot(death);
    if(!active || death.kind>SnakeDeathKind::Boss || death.forceSerial==0)return now.motion.pose;
    const auto* before=storage->enemies[storage->enemyTable].find(death.id,storage->generation);
    return sampleEnemySnapshot(now,before,frameAlpha,active);
}
SnakePose<5> MotionHistory::sampleMini(const MiniSnake& mini,float frameAlpha,bool active) const {return sample(snapshot(mini),storage->minis.find(mini.id,storage->generation),frameAlpha,active);}
GreyPose MotionHistory::sampleGrey(const GreySnake& snake,float frameAlpha,bool active) const {
    if(!snake.count)return {};
    const auto now=greySnapshot(snake);
    const GreySnapshot* before=storage->generation && snake.id>0 && snake.id<=storage->greyCount?&storage->greys[snake.id-1]:nullptr;
    const auto pose=sample(now.motion,before?&before->motion:nullptr,frameAlpha,active);
    const bool old=active && before && !same(pose.current[0],now.motion.pose.current[0]);
    const bool alive=old?before->alive:snake.alive || (active && before && before->alive && frameAlpha<1);
    return {pose.previous[0],pose.current[0],pose.direction,old?before->count:snake.count,pose.alpha,alive};
}

}
