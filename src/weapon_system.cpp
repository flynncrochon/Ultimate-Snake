#include "core.hpp"
#include "grid.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace snake {
namespace {
constexpr float Pi=3.14159265358979323846f;
float sq(Vec p){return p.x*p.x+p.y*p.y;}
Vec minus(Vec a,Vec b){return {a.x-b.x,a.y-b.y};}
Vec plus(Vec a,Vec b){return {a.x+b.x,a.y+b.y};}
Vec times(Vec a,float s){return {a.x*s,a.y*s};}
float dist(Vec a,Vec b){return sq(minus(a,b));}
Vec normal(Vec p){const float n=std::sqrt(sq(p));return n>.001f?times(p,1/n):Vec{1,0};}
float segmentDistance(Vec p,Vec a,Vec b){const Vec d=minus(b,a);const float n=sq(d);const float t=n>0?std::clamp((minus(p,a).x*d.x+minus(p,a).y*d.y)/n,0.0f,1.0f):0;return dist(p,plus(a,times(d,t)));}
Enemy* byId(std::vector<Enemy>& enemies,std::uint32_t id){for(auto& enemy:enemies)if(enemy.id==id)return &enemy;return nullptr;}
template<class T,class F>void compact(std::vector<T>& values,F dead){std::size_t i=0;while(i<values.size()){if(dead(values[i])){values[i]=std::move(values.back());values.pop_back();}else ++i;}}
bool contains(const std::array<std::uint32_t,32>& ids,std::uint8_t count,std::uint32_t id){return std::find(ids.begin(),ids.begin()+count,id)!=ids.begin()+count;}
}

void Game::fire(OwnedUpgrade& own,const Stats& s){
    const auto& d=catalog.upgrades[std::size_t(own.definition)];const WeaponSpec w=weaponSpec(own);
    own.cooldown=std::max(.025f,w.cooldown);
    if(d.pattern==Pattern::Shot && !own.evolved){
        const auto active=std::count_if(projectiles.begin(),projectiles.end(),[](const Projectile& p){return p.alive && p.pattern==Pattern::Shot && !p.cobraSpit;});
        // The original gun waits for room for its whole volley. Retry next
        // tick without spending the cooldown or emitting a shot event.
        if(active+w.amount>40){own.cooldown=0;return;}
    }
    auto visual=[&](Pattern pattern,Vec end,float radius,float duration,bool evolved){
        if(zones.size()<128){Zone z;z.position=end;z.radius=radius;z.life=z.maxLife=duration;z.damage=0;z.pattern=pattern;z.evolved=evolved;zones.push_back(z);}
    };
    if(d.pattern==Pattern::Shot && own.evolved){
        bool found=false;for(auto& zone:zones)if(zone.pattern==Pattern::Shot && zone.evolved){zone.life=zone.maxLife=.45f;zone.radius=w.range;zone.damage=0;found=true;break;}
        if(!found)visual(Pattern::Shot,player.position,w.range,.45f,true);
        return; // The source evolution REPLACES the venom gun.
    }
    if(d.pattern==Pattern::Nova && own.evolved){
        for(auto& zone:zones)if(zone.pattern==Pattern::Nova && zone.evolved){zone.life=zone.maxLife=1;zone.radius=zone.baseRadius=w.area;zone.damage=w.unroundedDamage;zone.tickInterval=.5f;zone.damageFactor=1;zone.canCrit=true;zone.followPlayer=true;return;}
        if(zones.size()<128){Zone z;z.position=player.position;z.radius=z.baseRadius=w.area;z.life=z.maxLife=1;z.damage=w.unroundedDamage;z.tick=.5f;z.tickInterval=.5f;z.damageFactor=1;z.pattern=Pattern::Nova;z.evolved=true;z.canCrit=true;z.followPlayer=true;zones.push_back(z);}return;
    }
    if(d.pattern==Pattern::Beam && own.evolved){
        int active=0;for(const auto& beam:beams)if(beam.evolved && !beam.ancient)++active;
        while(active<w.amount && beams.size()<32){TrackingBeam beam;beam.evolved=true;beam.life=beam.duration=1;beam.damage=w.unroundedDamage;beam.range=w.range;beam.tickInterval=.1f;beams.push_back(beam);++active;}
        return;
    }
    std::array<Enemy*,1800> targets{};std::size_t count=0;
    for(auto& enemy:enemies)if(count<targets.size() && enemy.alive && !enemy.grabbed && dist(player.position,enemy.position)<=w.range*w.range){
        // Extending tongues reserve prey immediately, before the latch sets
        // grabbed. Fast casts must not send multiple grapples after one victim.
        if(d.pattern==Pattern::Lash && own.evolved && std::any_of(grapples.begin(),grapples.end(),[&](const Grapple& tongue){return tongue.target==enemy.id && tongue.phase!=2 && tongue.life>0;}))continue;
        targets[count++]=&enemy;
    }
    std::sort(targets.begin(),targets.begin()+static_cast<std::ptrdiff_t>(count),[&](const Enemy* a,const Enemy* b){const float da=dist(player.position,a->position),db=dist(player.position,b->position);return da!=db?da<db:a->id<b->id;});
    if(count==0 && !(d.pattern==Pattern::Nova) && !(d.pattern==Pattern::Pit && own.evolved)){own.cooldown=.05f;return;}
    if(events.size()<100)events.push_back({Event::Shot});
    if(d.pattern==Pattern::Nest){const auto before=nestEggs.size();launchNest(d,own,s);if(before==nestEggs.size())own.cooldown=.05f;return;}
    if(d.pattern==Pattern::Nova){
        if(flood.contact(player.position,player.position,w.area)<=1){flood.repel(player.position,w.area,{},65);const bool crit=random(0,1)<s.crit;damageDealt+=flood.damage(player.position,w.area,w.damage*float(w.amount)*(crit?2:1));}
        for(auto& enemy:enemies)if(enemy.alive && !enemy.grabbed && enemy.kind!=EnemyKind::Flood && dist(player.position,enemy.position)<=w.area*w.area){const bool crit=random(0,1)<s.crit;damageEnemy(enemy,w.damage*float(w.amount)*(crit?2:1),crit);}
        const int aftershocks=std::max(0,int(std::floor((s.duration-1)/.25f+.0001f)));
        for(int wave=0;wave<=aftershocks;++wave)for(int i=0;i<w.amount && zones.size()<75;++i){
            Zone zone;zone.position=player.position;zone.radius=w.area;zone.life=zone.maxLife=.45f;zone.pattern=Pattern::Nova;
            zone.delay=float(wave)*.25f+float(i)*.12f;zone.singleHit=wave>0;zone.damage=wave>0?std::round(w.unroundedDamage*.5):0;zone.damageFactor=1;zone.canCrit=true;zones.push_back(zone);
        }return;
    }
    if(d.pattern==Pattern::Beam){
        for(int i=0;i<w.amount && beams.size()<32;++i){TrackingBeam beam;beam.target=targets[std::size_t(i)%count]->id;beam.position=targets[std::size_t(i)%count]->position;beam.life=beam.duration=w.duration;beam.damage=w.damage;beam.range=w.range;beam.tickInterval=.2f;beams.push_back(beam);}return;
    }
    if(d.pattern==Pattern::Lash && !own.evolved){
        const float cone=std::min(Pi,Pi/3*s.area),limit=std::cos(cone);
        const float facing=std::atan2(player.direction.y,player.direction.x);
        for(int lash=0;lash<w.amount;++lash){
            const float angle=facing+(w.amount>1?(float(lash)/float(w.amount-1)-.5f)*.8f:0);const Vec axis{std::cos(angle),std::sin(angle)};
            for(std::size_t i=0;i<count;++i){auto& enemy=*targets[i];if(!enemy.alive)continue;const Vec offset=minus(enemy.position,player.position);const float length=std::sqrt(sq(offset));if(length<.01f || (offset.x*axis.x+offset.y*axis.y)/length<limit)continue;const bool crit=random(0,1)<s.crit;damageEnemy(enemy,std::round(w.baseDamage*(crit?2:1)*s.damage),crit);enemy.slow=1.5f*s.duration;enemy.slowFactor=.4f;if(enemy.alive)placeEnemy(enemy,plus(enemy.position,times(normal(offset),3.2f*Tile)));}
            visual(Pattern::Lash,plus(player.position,times(axis,w.range)),w.area,.35f,false);
        }return;
    }
    if(d.pattern==Pattern::Lash){
        for(int i=0;i<w.amount && std::size_t(i)<count && grapples.size()<16;++i){auto* target=targets[count-1-std::size_t(i)];
            if(target->kind==EnemyKind::Flood){const float radius=std::max(20.0f,w.area*s.area);flood.repel(target->position,radius,minus(target->position,player.position),100);const bool crit=random(0,1)<s.crit;damageDealt+=flood.damage(target->position,radius,std::round(w.unroundedDamage*(crit?2:1)));visual(Pattern::Lash,target->position,w.area,.35f,false);continue;}
            Grapple grapple;grapple.target=target->id;grapple.position=player.position;grapple.direction=normal(minus(target->position,player.position));grapple.maxDistance=std::sqrt(dist(target->position,player.position));grapple.damage=w.unroundedDamage;grapple.radius=w.area*s.area;grapples.push_back(grapple);}return;
    }
    if(d.pattern==Pattern::Mortar || d.pattern==Pattern::Pit){
        Enemy* primary=count?targets[0]:nullptr;int score=-1;
        const std::size_t sample=std::min(count,std::size_t(15));
        for(std::size_t i=0;i<sample;++i){int n=0;const float radius=d.pattern==Pattern::Mortar?w.area:4*Tile;for(std::size_t j=0;j<count;++j)if(dist(targets[i]->position,targets[j]->position)<=radius*radius)++n;if(n>score){score=n;primary=targets[i];}}
        Vec landing=primary?primary->position:plus(player.position,times(player.direction,6*Tile));
        if(d.pattern==Pattern::Pit && own.evolved && leviathan.active)landing=leviathan.position;
        const int amount=d.pattern==Pattern::Mortar && own.evolved?1:w.amount;
        for(int i=0;i<amount && projectiles.size()<1600;++i){
            Projectile p;p.pattern=d.pattern;p.evolved=own.evolved;p.start=p.position=player.position;p.target=landing;
            if(i>0){if(d.pattern==Pattern::Mortar && std::size_t(i)<count)p.target=targets[count-std::size_t(i)]->position;else{const float angle=float(i)*2.399963f;p.target=gridPosition(plus(landing,{std::cos(angle)*Tile*2.5f,std::sin(angle)*Tile*2.5f}));}}
            p.flightDuration=d.pattern==Pattern::Mortar?(own.evolved?.65f:.9f):(own.evolved?.85f:.75f);p.flightDuration+=float(i)*.1f;
            p.life=p.maxLife=p.flightDuration;p.velocity=times(minus(p.target,p.start),1/p.flightDuration);p.payloadDuration=w.duration;p.radius=w.area;p.damage=d.pattern==Pattern::Pit?w.unroundedDamage:w.damage;p.generation=own.rank;
            projectiles.push_back(p);
        }return;
    }
    const std::size_t family=std::count_if(projectiles.begin(),projectiles.end(),[&](const Projectile& p){return p.alive && p.pattern==d.pattern && !p.cobraSpit;});
    const std::size_t cap=d.pattern==Pattern::Shot?40:d.pattern==Pattern::Ricochet?60:1600;
    for(int i=0;i<w.amount && projectiles.size()<1600 && family+std::size_t(i)<cap;++i){
        Projectile p;p.position=player.position;p.damage=w.unroundedDamage;p.life=p.maxLife=w.duration;p.payloadDuration=w.duration;p.radius=w.area;p.pattern=d.pattern;p.evolved=own.evolved;
        p.sourceDamage=w.baseDamage;p.damageMultiplier=s.damage;
        p.turnSpeed=w.turnSpeed;p.targetId=targets[std::size_t(i)%count]->id;p.pierce=w.pierce;p.gatling=d.pattern==Pattern::Homing && own.evolved;
        p.bounces=std::max(0,w.bounces-1);p.bounceRadius=(own.evolved?6.0f:5.0f)*Tile*s.area;p.damageRamp=d.pattern==Pattern::Ricochet?.2:0;
        Vec direction=normal(minus(targets[std::size_t(i)%count]->position,player.position));
        if(d.pattern==Pattern::Shot){
            const float facing=std::atan2(player.direction.y,player.direction.x);float angle=facing;
            if(i==0)angle+=Pi/4;else if(i==1)angle-=Pi/4;
            direction={std::cos(angle),std::sin(angle)};
            if(i<2){const Vec to=normal(minus(targets[0]->position,player.position));const float side=i==0?1.0f:-1.0f;p.position=plus(player.position,{-to.y*.3f*Tile*side,to.x*.3f*Tile*side});}
            else if(i>=3){const float t=float(i-2)/float(w.amount-2);const Vec left{std::cos(facing+Pi/4),std::sin(facing+Pi/4)},right{std::cos(facing-Pi/4),std::sin(facing-Pi/4)};direction=normal(plus(times(left,1-t),times(right,t)));const Vec to=normal(minus(targets[0]->position,player.position));p.position=plus(player.position,{-to.y*.3f*Tile*(1-2*t),to.x*.3f*Tile*(1-2*t)});}
        }else{
            const float angle=std::atan2(direction.y,direction.x)+(w.amount>1?(float(i)/float(w.amount-1)-.5f)*(d.pattern==Pattern::Ricochet?.6f:own.evolved?.7f:.5f):0);
            direction={std::cos(angle),std::sin(angle)};
        }
        p.velocity=times(direction,w.speed);projectiles.push_back(p);
    }
}

void Game::updateShield(float dt){
    int rank=0;for(const auto& own:owned)if(catalog.upgrades[std::size_t(own.definition)].id=="serpents_scales")rank=own.rank;
    rank=std::clamp(rank,0,8);constexpr int charges[]{0,1,1,1,2,2,3,3,3};
    if(rank!=shieldRank){const int maximum=charges[rank];shieldCharges=std::clamp(shieldCharges+std::max(0,maximum-shieldMax),0,maximum);shieldMax=maximum;shieldRank=rank;shieldCooldown=rank?10-6*float(rank-1)/7:10;if(!rank)shieldTimer=0;}
    if(shieldCharges<shieldMax){shieldTimer-=dt;if(shieldTimer<=0){++shieldCharges;shieldTimer=shieldCharges<shieldMax?shieldCooldown:0;}}
}

void Game::absorbKill(Vec position,int growth,float vacuumRadius){
    int collected=0;
    for(std::size_t i=0;i<pickups.size();){if(pickups[i].kind==PickupKind::Experience && vacuumRadius>0 && dist(position,pickups[i].position)<=vacuumRadius*vacuumRadius){collected+=pickups[i].value;pickups[i]=pickups.back();pickups.pop_back();}else ++i;}
    collectExperience(growth+collected,stats().growth);
}

void Game::updateWeaponActors(float dt,const Stats& s,const std::array<WeaponSpec,9>& current,const std::array<bool,9>& equipped){
    auto nearest=[&](Vec position,float range,const std::array<std::uint32_t,32>* exclude=nullptr,std::size_t count=0)->Enemy*{
        Enemy* target=nullptr;float best=range*range;for(auto& enemy:enemies){if(!enemy.alive || enemy.grabbed || (exclude && std::find(exclude->begin(),exclude->begin()+static_cast<std::ptrdiff_t>(count),enemy.id)!=exclude->begin()+static_cast<std::ptrdiff_t>(count)))continue;const float d=dist(position,enemy.position);if(d<best){best=d;target=&enemy;}}return target;
    };
    ouroborosActive=false;
    for(const auto& own:owned)if(own.evolved && catalog.upgrades[std::size_t(own.definition)].pattern==Pattern::Shot)ouroborosActive=true;
    if(ouroborosActive){
        const auto& orbitSpec=current[std::size_t(Pattern::Shot)];
        const float contactRadius=orbitSpec.area/s.area;
        orbitPhase+=dt;for(std::size_t i=0;i<orbitFangs.size();++i){auto& fang=orbitFangStates[i];fang.angle+=fang.speed*dt;
            // Eight inner fangs protect close targets instead of leaving the
            // evolved gun unable to hit enemies beside the head.
            const float orbit=i<8?1.0f+(fang.orbit-4.5f)*.4f:fang.orbit;
            const float wobble=i<8?fang.radiusAmplitude*.2f:fang.radiusAmplitude;
            const float radius=(orbit+wobble*std::sin(fang.radiusPhase+elapsed*fang.radiusFrequency))*Tile*s.area;
            const float angle=fang.angle+fang.angleAmplitude*std::cos(fang.anglePhase+elapsed*fang.angleFrequency);orbitFangs[i]=plus(player.position,{std::cos(angle)*radius,std::sin(angle)*radius});}
        if(flood.dangerous())for(const Vec fang:orbitFangs)flood.repel(fang,contactRadius,minus(fang,player.position),8);
        for(auto& enemy:enemies)if(enemy.alive && !enemy.grabbed && enemy.orbitCooldown<=0){const float radial=dist(enemy.position,player.position);if(radial>float(Tile*Tile)*145*s.area*s.area)continue;
            for(const Vec fang:orbitFangs)if(dist(enemy.position,fang)<=(contactRadius+enemy.radius)*(contactRadius+enemy.radius)){const bool crit=random(0,1)<s.crit;damageEnemy(enemy,std::round(orbitSpec.unroundedDamage*(crit?2:1)),crit);enemy.orbitCooldown=orbitSpec.tickInterval;break;}}
    }
    const bool currentPit=equipped[std::size_t(Pattern::Pit)]&&!current[std::size_t(Pattern::Pit)].evolved;
    const auto& pitSpec=current[std::size_t(Pattern::Pit)];
    // A pit first raises separate cobras. Its end does not remove living turrets.
    for(auto& pit:cobraPits){if(currentPit){pit.life=pitSpec.duration;pit.maxCobras=pitSpec.broodCount;pit.cobraLife=5*s.duration;pit.damage=pitSpec.unroundedDamage;pit.spitCooldown=pitSpec.tickInterval;}pit.age+=dt;pit.spawnClock-=dt;if(pit.age>=pit.life)continue;
        if(pit.spawned<pit.maxCobras && pit.spawnClock<=0){pit.spawnClock+=.55f;
            if(cobras.size()<30){const float angle=float(pit.spawned)*2.399963f;Cobra cobra;cobra.position=plus(pit.position,{std::cos(angle)*Tile,std::sin(angle)*Tile});cobra.life=pit.cobraLife;cobra.damage=pit.damage;cobra.spitCooldown=pit.spitCooldown;cobra.spitClock=pit.spitCooldown;cobras.push_back(cobra);}else cobraOverflow+=.15;
            ++pit.spawned;}}
    compact(cobraPits,[](const CobraPit& pit){return pit.age>=pit.life;});
    for(auto& cobra:cobras){if(currentPit){cobra.damage=pitSpec.unroundedDamage;cobra.spitClock=std::max(0.0f,cobra.spitClock+pitSpec.tickInterval-cobra.spitCooldown);cobra.spitCooldown=pitSpec.tickInterval;}cobra.age+=dt;cobra.spitClock-=dt;if(cobra.age>=cobra.life || cobra.age<.4f || cobra.spitClock>0)continue;
        if(auto* target=nearest(cobra.position,6*Tile)){if(projectiles.size()<1600){Projectile p;p.position=cobra.position;p.velocity=times(normal(minus(target->position,cobra.position)),12*Tile);p.life=p.maxLife=1*s.duration;p.damage=cobra.damage*cobraOverflow;p.radius=.4f*Tile;p.cobraSpit=true;p.critResolved=true;p.critical=random(0,1)<s.crit;projectiles.push_back(p);}cobra.spitClock=cobra.spitCooldown;}}
    compact(cobras,[](const Cobra& cobra){return cobra.age>=cobra.life;});

    const bool currentAncient=equipped[std::size_t(Pattern::Pit)]&&pitSpec.evolved;
    if(leviathan.active){
        if(currentAncient){leviathan.damage=pitSpec.unroundedDamage;leviathan.halfWidth=pitSpec.area*s.area;}
        leviathan.age+=dt;leviathan.absorbFlash=std::max(0.0f,leviathan.absorbFlash-dt);
        if(dist(leviathan.position,player.position)>float(22*Tile)*float(22*Tile)){leviathan.active=false;for(auto& beam:beams)if(beam.ancient)beam.life=0;}
        else if(leviathan.age>=1.5f){int active=0;for(const auto& beam:beams)if(beam.ancient)++active;
            while(active<1+s.amount && beams.size()<32){TrackingBeam beam;beam.ancient=true;beam.evolved=true;beam.life=beam.duration=1;beam.origin=leviathan.position;beams.push_back(beam);++active;}}
    }
    const bool currentBeam=equipped[std::size_t(Pattern::Beam)];const auto& beamSpec=current[std::size_t(Pattern::Beam)];
    if(currentBeam&&beamSpec.evolved){int active=0;for(const auto& beam:beams)if(beam.evolved&&!beam.ancient)++active;while(active<beamSpec.amount&&beams.size()<32){TrackingBeam beam;beam.evolved=true;beam.life=beam.duration=1;beam.tickInterval=.1f;beam.damage=beamSpec.unroundedDamage;beam.range=beamSpec.range;beams.push_back(beam);++active;}}
    std::array<std::uint32_t,32> targeted{};std::size_t targetedCount=0;
    for(auto& beam:beams){
        if(beam.pendingAbsorb){absorbKill(beam.position,3,5*Tile*s.duration);beam.pendingAbsorb=false;}
        if(beam.ancient && !leviathan.active){beam.life=0;continue;}
        beam.life-=dt;if(beam.evolved)beam.life=1;
        beam.origin=beam.ancient?leviathan.position:player.position;
        if(!beam.ancient && currentBeam && beam.evolved==beamSpec.evolved){beam.damage=beam.evolved?beamSpec.unroundedDamage:beamSpec.damage;beam.range=beamSpec.range;}
        if(beam.ancient){beam.range=(12+float(leviathan.growth))*Tile*s.range;beam.damage=std::round(leviathan.damage*(1+.25*leviathan.growth)*cobraOverflow);beam.tickInterval=.2f/std::max(.5f,s.duration);}
        Enemy* target=byId(enemies,beam.target);
        if(target && !target->alive && beam.evolved && !beam.ancient){absorbKill(target->position,3,5*Tile*s.duration);beam.target=0;target=nullptr;}
        if(!target || !target->alive || target->grabbed || dist(beam.origin,target->position)>beam.range*beam.range*1.5f){target=nearest(beam.origin,beam.range,&targeted,targetedCount);beam.target=target?target->id:0;beam.tick=0;}
        if(!target)continue;
        if(targetedCount<targeted.size())targeted[targetedCount++]=target->id;
        beam.position=target->position;beam.tick+=dt;
        if(beam.life<=0 || beam.tick+1e-6f<beam.tickInterval)continue;
        beam.tick-=beam.tickInterval;const bool crit=random(0,1)<s.crit;const double damage=std::round(beam.damage*(crit?2:1));
        if(beam.ancient){const Vec end=plus(beam.origin,times(normal(minus(target->position,beam.origin)),beam.range));const float width=leviathan.halfWidth*(1+.16f*float(leviathan.growth));
            for(auto& enemy:enemies)if(enemy.alive && !enemy.grabbed && segmentDistance(enemy.position,beam.origin,end)<(width+enemy.radius)*(width+enemy.radius))damageEnemy(enemy,damage,crit);
        }else{
            if(beam.evolved && target->hp<=damage)target->devoured=true;
            damageEnemy(*target,damage,crit);
            if(beam.pendingAbsorb){absorbKill(beam.position,3,5*Tile*s.duration);beam.pendingAbsorb=false;}
        }
    }
    compact(beams,[](const TrackingBeam& beam){return beam.life<=0;});

    for(auto& tongue:grapples){
        const auto& lashSpec=current[std::size_t(Pattern::Lash)];
        if(equipped[std::size_t(Pattern::Lash)]&&lashSpec.evolved){tongue.damage=lashSpec.unroundedDamage;tongue.radius=lashSpec.area*s.area;}
        tongue.life-=dt;Enemy* target=byId(enemies,tongue.target);
        if(tongue.life<=0 || !target || !target->alive){if(target)target->grabbed=false;tongue.phase=2;continue;}
        if(tongue.phase==0 && target->grabbed){tongue.phase=2;continue;}
        if(tongue.phase==0){const Vec previous=tongue.position;tongue.position=plus(tongue.position,times(tongue.direction,40*Tile*dt));tongue.traveled+=40*Tile*dt;
            if(segmentDistance(target->position,previous,tongue.position)<float(Tile*Tile)*2.25f || tongue.traveled>=tongue.maxDistance+2*Tile){tongue.phase=1;tongue.position=target->position;target->grabbed=true;tongue.pullClock=0;}
        }else if(tongue.phase==1){tongue.pullClock+=dt;if(tongue.pullClock<.1f)continue;tongue.pullClock-=.1f;
            const Vec delta=minus(player.position,target->position);
            if(std::abs(delta.x)<1.5f*Tile && std::abs(delta.y)<1.5f*Tile){
                const Vec at=target->position;target->grabbed=false;target->devoured=true;damageEnemy(*target,target->hp+1);absorbKill(at,1,0);
                const float biteRadius=2*tongue.radius;
                for(auto& enemy:enemies)if(enemy.alive && !enemy.grabbed && dist(enemy.position,player.position)<=biteRadius*biteRadius){const bool crit=random(0,1)<s.crit;damageEnemy(enemy,std::round(1.25*tongue.damage*(crit?2:1)),crit);}
                if(zones.size()<128){Zone bite;bite.position=player.position;bite.radius=biteRadius;bite.life=bite.maxLife=.55f;bite.damage=0;bite.pattern=Pattern::Lash;bite.evolved=true;
                    // Cosmetic heading is retained at the actual devour. Turning
                    // afterward must not rotate the already snapping jaws.
                    const Vec facing=minus(at,player.position);bite.phase=std::atan2(facing.y,facing.x);zones.push_back(bite);}tongue.phase=2;
            }else{
                const Vec old=target->position;placeEnemy(*target,plus(old,times(normal(delta),float(Tile))));tongue.position=target->position;
                for(auto& enemy:enemies)if(enemy.alive && !enemy.grabbed && !contains(tongue.hitIds,tongue.hitCount,enemy.id) && segmentDistance(enemy.position,old,target->position)<=tongue.radius*tongue.radius){const bool crit=random(0,1)<s.crit;damageEnemy(enemy,std::round(.625*tongue.damage*(crit?2:1)),crit);enemy.slow=1*s.duration;enemy.slowFactor=.3f;if(tongue.hitCount<tongue.hitIds.size())tongue.hitIds[tongue.hitCount++]=enemy.id;}
            }
        }
    }
    compact(grapples,[](const Grapple& tongue){return tongue.phase==2;});
}
}
