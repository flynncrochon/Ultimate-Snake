#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace snake {
namespace {
constexpr float Support = 10, Spacing = 4.4f, MaximumSpeed = 125;
constexpr int HashSide = 160, Neighbors = 48;
float square(Vec p){return p.x*p.x+p.y*p.y;}
Vec add(Vec a,Vec b){return {a.x+b.x,a.y+b.y};}
Vec sub(Vec a,Vec b){return {a.x-b.x,a.y-b.y};}
Vec mul(Vec p,float n){return {p.x*n,p.y*n};}
Vec unit(Vec p){const float n=square(p);return n>.00001f?mul(p,1/std::sqrt(n)):Vec{1,0};}
Vec limit(Vec p,float maximum){const float n=square(p);return n>maximum*maximum?mul(p,maximum/std::sqrt(n)):p;}
int hashCell(float x){return std::clamp(int(x/Support),0,HashSide-1);}
float sweep(Vec a,Vec b,Vec p,float radius){
    const Vec delta=sub(b,a),offset=sub(a,p);
    const float c=square(offset)-radius*radius;if(c<=0)return 0;
    const float aa=square(delta);if(aa<.000001f)return 2;
    const float bb=offset.x*delta.x+offset.y*delta.y,disc=bb*bb-aa*c;
    if(disc<0)return 2;
    const float t=(-bb-std::sqrt(disc))/aa;return t>=0&&t<=1?t:2;
}
bool headContact(Vec from,Vec to,Vec head){
    // The rendered head is a square. Sweep the water disc against its rounded
    // Minkowski boundary so even a corner touch is lethal, without a larger
    // square approximation killing the player across a visible dry corner.
    constexpr float half=2.8f,reach=half+FloodRadius;
    from=sub(from,head);to=sub(to,head);
    if(std::min(from.x,to.x)>reach || std::max(from.x,to.x)<-reach || std::min(from.y,to.y)>reach || std::max(from.y,to.y)<-reach)return false;
    const Vec delta=sub(to,from);
    auto box=[&](float hx,float hy){
        float enter=0,leave=1;
        auto axis=[&](float start,float d,float h){
            if(std::abs(d)<.000001f)return std::abs(start)<=h;
            const float a=(-h-start)/d,b=(h-start)/d;enter=std::max(enter,std::min(a,b));leave=std::min(leave,std::max(a,b));return enter<=leave;
        };
        return axis(from.x,delta.x,hx) && axis(from.y,delta.y,hy);
    };
    if(box(reach,half) || box(half,reach))return true;
    for(float y:{-half,half})for(float x:{-half,half})if(sweep(from,to,{x,y},FloodRadius)<=1)return true;
    return false;
}
}

struct FloodSystem::Storage {
    std::array<std::int16_t,HashSide*HashSide> heads{};
    std::array<std::int16_t,FloodCapacity> next{};
    std::array<Vec,FloodCapacity> accelerations{};
    std::array<std::uint8_t,ArenaTiles*ArenaTiles> obstacles{};
    Vec minimum,maximum;
    template<class F>void query(Vec a,Vec b,float radius,F&& visit)const{
        const float left=std::min(a.x,b.x)-radius,right=std::max(a.x,b.x)+radius;
        const float top=std::min(a.y,b.y)-radius,bottom=std::max(a.y,b.y)+radius;
        if(right<minimum.x || left>maximum.x || bottom<minimum.y || top>maximum.y)return;
        for(int y=hashCell(top);y<=hashCell(bottom);++y)for(int x=hashCell(left);x<=hashCell(right);++x)
            for(int i=heads[std::size_t(y*HashSide+x)];i>=0;i=next[std::size_t(i)])visit(std::size_t(i));
    }
};
FloodSystem::FloodSystem():storage(std::make_unique<Storage>()){particles.reserve(FloodCapacity);}
FloodSystem::~FloodSystem() = default;
FloodSystem::FloodSystem(FloodSystem&&) noexcept = default;
FloodSystem& FloodSystem::operator=(FloodSystem&&) noexcept = default;
FloodSystem::FloodSystem(const FloodSystem& other):FloodSystem(){*this=other;}
FloodSystem& FloodSystem::operator=(const FloodSystem& other){
    if(this==&other)return *this;
    particles=other.particles;center=other.center;direction=other.direction;age=other.age;warning=other.warning;lifetime=other.lifetime;
    clock=other.clock;nextSpawn=other.nextSpawn;enemyId=other.enemyId;serial=other.serial;repulsions=other.repulsions;enabled=other.enabled;
    hazardous=other.hazardous;
    *storage=*other.storage;return *this;
}
void FloodSystem::reset(){particles.clear();center={};direction={1,0};age=0;warning=1.5f;lifetime=FloodLifetime;clock=0;nextSpawn=FloodFirstSeconds;enemyId=serial=repulsions=0;hazardous=false;}
std::size_t FloodSystem::waveCount() const {
    std::size_t count=0;std::uint32_t previous=0;
    for(const auto& p:particles)if(p.wave!=previous){++count;previous=p.wave;}
    return count;
}
bool FloodSystem::start(Vec position,Vec heading,int count,float warningSeconds){
    if(count<1 || count>int(FloodCapacity-particles.size()) || !std::isfinite(position.x) || !std::isfinite(position.y) ||
       !std::isfinite(heading.x) || !std::isfinite(heading.y) || !std::isfinite(warningSeconds) || warningSeconds<0 ||
       position.x<Tile || position.y<Tile || position.x>ArenaSize-Tile || position.y>ArenaSize-Tile)return false;
    const Vec incoming=unit(heading);
    if(!active()){direction=incoming;center=position;age=0;warning=warningSeconds;repulsions=0;hazardous=false;}
    ++serial;hazardous=hazardous || (warningSeconds==0 && lifetime>0);
    const Vec tangent{-incoming.y,incoming.x};
    const int width=std::min({count,48,std::max(1,int(std::ceil(std::sqrt(float(count)*3))))});
    const int depth=(count+width-1)/width;
    for(int i=0;i<count;++i){
        const int row=i/width,column=i%width;
        // A broad sheet with a scalloped leading edge, rather than a ball of
        // droplets. Phase comes from indices, not the game's combat RNG.
        const float across=(float(column)-float(width-1)*.5f)*Spacing;
        const float along=(float(row)-float(depth-1)*.5f)*Spacing+std::sin(float(column)*.43f)*2.5f;
        Vec p=add(position,add(mul(tangent,across),mul(incoming,along)));
        p.x=std::clamp(p.x,Tile+FloodRadius,ArenaSize-Tile-FloodRadius);
        p.y=std::clamp(p.y,Tile+FloodRadius,ArenaSize-Tile-FloodRadius);
        particles.push_back({p,p,mul(incoming,26),row==depth-1?.6f:0,0,warningSeconds,serial});
    }
    rebuildIndex();return true;
}
void FloodSystem::rebuildIndex(){
    auto& s=*storage;s.heads.fill(-1);s.minimum={ArenaSize,ArenaSize};s.maximum={};
    for(std::size_t i=0;i<particles.size();++i){
        const Vec p=particles[i].position;const int cell=hashCell(p.y)*HashSide+hashCell(p.x);
        s.next[i]=s.heads[std::size_t(cell)];s.heads[std::size_t(cell)]=std::int16_t(i);
        s.minimum.x=std::min(s.minimum.x,p.x);s.minimum.y=std::min(s.minimum.y,p.y);
        s.maximum.x=std::max(s.maximum.x,p.x);s.maximum.y=std::max(s.maximum.y,p.y);
    }
}
Vec FloodSystem::nearest(Vec from) const {
    Vec result=center;float best=std::numeric_limits<float>::max();
    for(const auto& p:particles){
        if(dangerous() && !dangerous(p))continue;
        const float d=square(sub(p.position,from));if(d<best){best=d;result=p.position;}
    }
    return result;
}
float FloodSystem::contact(Vec from,Vec to,float radius) const {
    if(!dangerous())return 2;
    float first=2;storage->query(from,to,radius+FloodRadius,[&](std::size_t i){if(dangerous(particles[i]))first=std::min(first,sweep(from,to,particles[i].position,radius+FloodRadius));});return first;
}
void FloodSystem::repel(Vec position,float radius,Vec heading,float strength){
    if(!dangerous() || radius<=0 || strength<=0)return;
    const bool radial=square(heading)<.00001f;
    const Vec axis=radial?Vec{}:unit(heading);
    bool hit=false;
    storage->query(position,position,radius+FloodRadius,[&](std::size_t i){auto& p=particles[i];
        if(!dangerous(p))return;
        const Vec delta=sub(p.position,position);const float d=square(delta);if(d>(radius+FloodRadius)*(radius+FloodRadius))return;
        const float falloff=1-.65f*std::min(1.0f,std::sqrt(d)/(radius+FloodRadius));
        p.velocity=limit(add(p.velocity,mul(radial?unit(delta):axis,strength*falloff)),MaximumSpeed);
        p.foam=std::min(1.0f,p.foam+.7f*falloff);
        hit=true;
    });
    if(hit)++repulsions;
}

bool FloodSystem::update(float dt,const Player& player,const std::vector<Enemy>& enemies,const GreySnakeSystem& grey,Vec headStart){
    if(!active())return false;
    age+=dt;
    for(auto& p:particles){p.previous=p.position;p.age+=dt;}
    const auto oldCount=particles.size();
    particles.erase(std::remove_if(particles.begin(),particles.end(),[&](const WaterParticle& p){return p.age>=lifetime+1;}),particles.end());
    hazardous=false;float movingDt=0;
    for(const auto& p:particles){hazardous=hazardous || dangerous(p);movingDt=std::max(movingDt,std::min(dt,std::max(0.0f,p.age-p.warning)));}
    if(!active())return false;
    if(particles.size()!=oldCount)rebuildIndex();
    // New warnings never freeze, protect or reset water already in play.
    if(movingDt<=0)return false;
    auto& s=*storage;s.obstacles.fill(0);
    auto mark=[&](Vec p,std::uint8_t type){
        const int x=int(std::lround(p.x/Tile)),y=int(std::lround(p.y/Tile));
        if(x>=0&&y>=0&&x<ArenaTiles&&y<ArenaTiles)s.obstacles[std::size_t(y*ArenaTiles+x)]=std::max(type,s.obstacles[std::size_t(y*ArenaTiles+x)]);
    };
    // The head is deliberately excluded. Side and tail contact redirect water;
    // the head instead uses an unconditional swept lethal test below.
    for(std::size_t i=1;i<player.segments.size();++i)mark(player.segments[i],2);
    for(const auto& e:enemies)if(e.alive && e.kind!=EnemyKind::Flood){
        const std::uint8_t type=e.kind==EnemyKind::Boss?3:1;mark(e.position,type);
        for(int i=0;i<e.trailSize;++i)mark(e.trail[std::size_t(i)],type);
    }
    bool killed=false;
    // A moving head must not tunnel through a stationary leading edge. During
    // the warning only the newly active endpoint can be lethal.
    s.query(headStart,player.position,FloodRadius+2.8f,[&](std::size_t i){
        const auto& p=particles[i];if(!dangerous(p))return;
        const Vec from=p.age-dt<p.warning?player.position:headStart;
        if(headContact(sub(p.position,from),sub(p.position,player.position),{}))killed=true;
    });
    center={};int flowing=0;
    for(const auto& p:particles)if(p.age>=p.warning){center=add(center,p.position);++flowing;}
    center=mul(center,1/float(flowing));
    const Vec wanted=unit(sub(player.position,center));
    direction=unit(add(direction,mul(sub(wanted,direction),movingDt*.65f)));
    constexpr int Substeps=2;
    for(int substep=0;substep<Substeps;++substep){
        rebuildIndex();
        for(std::size_t i=0;i<particles.size();++i){
            auto& p=particles[i];const Vec desired=mul(direction,27);
            if(p.age<p.warning)continue;
            Vec force=mul(sub(desired,p.velocity),.7f);
            // A low-frequency curl rolls the sheet into eddies. It is evaluated
            // at 60 Hz; render frequency and effects settings cannot alter it.
            const float curl=std::sin(p.position.x*.055f+age*1.7f)*std::cos(p.position.y*.047f-age*1.3f);
            force=add(force,{-direction.y*curl*13,direction.x*curl*13});
            int visited=0;const int cx=hashCell(p.position.x),cy=hashCell(p.position.y);
            for(int y=std::max(0,cy-1);y<=std::min(HashSide-1,cy+1);++y)
                for(int x=std::max(0,cx-1);x<=std::min(HashSide-1,cx+1);++x)
                    for(int j=s.heads[std::size_t(y*HashSide+x)];j>=0 && visited<Neighbors;j=s.next[std::size_t(j)]){
                        if(std::size_t(j)==i)continue;
                        const auto& other=particles[std::size_t(j)];if(other.age<other.warning)continue;
                        Vec delta=sub(p.position,other.position);float d2=square(delta);
                        if(d2>=Support*Support)continue;++visited;
                        // Coincident samples can occur when a whole wave is
                        // clamped at an arena corner. Deterministic separation
                        // and the neighbor budget bound this worst case too.
                        if(d2<.0001f){delta={i<std::size_t(j)?-.01f:.01f,((i+std::size_t(j))&1)?.01f:-.01f};d2=square(delta);}
                        const float d=std::sqrt(d2),q=1-d/Support;
                        const float pressure=d<Spacing?(Spacing-d)*230:-(d-Spacing)*7*q;
                        force=add(force,mul(delta,pressure/d));
                        force=add(force,mul(sub(other.velocity,p.velocity),q*1.7f));
                    }
            s.accelerations[i]=limit(force,1400);
        }
        for(std::size_t i=0;i<particles.size();++i){
            auto& p=particles[i];const Vec start=p.position;
            if(p.age<p.warning)continue;
            const float step=std::min(dt,std::max(0.0f,p.age-p.warning))/Substeps;
            p.velocity=limit(add(p.velocity,mul(s.accelerations[i],step)),MaximumSpeed);
            p.position=add(p.position,mul(p.velocity,step));p.foam=std::max(0.0f,p.foam-step*.65f);
            // All obstacles use a cell lookup. Dense combat never adds an
            // enemies-times-particles collision loop or a growing contact list.
            const int cx=int(std::lround(p.position.x/Tile)),cy=int(std::lround(p.position.y/Tile));
            for(int y=std::max(1,cy-1);y<=std::min(ArenaTiles-2,cy+1);++y)
                for(int x=std::max(1,cx-1);x<=std::min(ArenaTiles-2,cx+1);++x){
                    const auto cell=std::size_t(y*ArenaTiles+x);const auto type=s.obstacles[cell];
                    if(!type && !grey.owners[cell])continue;
                    const float half=grey.owners[cell]?4.0f:type==3?3.0f:type==2?2.8f:2.2f;
                    const Vec obstacle{float(x*Tile),float(y*Tile)};
                    const Vec delta=sub(p.position,obstacle);const float reach=half+FloodRadius;
                    const float px=reach-std::abs(delta.x),py=reach-std::abs(delta.y);if(px<=0 || py<=0)continue;
                    Vec n;if(px<py)n={delta.x<0?-1.0f:1.0f,0};else n={0,delta.y<0?-1.0f:1.0f};
                    p.position=add(p.position,mul(n,std::min(px,py)+.001f));
                    const float into=p.velocity.x*n.x+p.velocity.y*n.y;
                    if(into<0)p.velocity=sub(p.velocity,mul(n,into*1.65f));
                    p.foam=std::min(1.0f,p.foam+.13f+std::abs(into)*.004f);
                }
            constexpr float minimum=Tile+FloodRadius,maximum=ArenaSize-Tile-FloodRadius;
            if(p.position.x<minimum || p.position.x>maximum){p.position.x=std::clamp(p.position.x,minimum,maximum);p.velocity.x=-p.velocity.x*.65f;p.foam=1;}
            if(p.position.y<minimum || p.position.y>maximum){p.position.y=std::clamp(p.position.y,minimum,maximum);p.velocity.y=-p.velocity.y*.65f;p.foam=1;}
            if(!killed && dangerous(p) && headContact(start,p.position,player.position))killed=true;
        }
    }
    rebuildIndex();
    return killed;
}

bool Game::spawnFlood(Vec position,Vec heading,int count,float warningSeconds){
    const bool joining=flood.active();
    if(!joining && enemies.size()>=1800)return false;
    if(!flood.start(position,heading,count,warningSeconds))return false;
    if(joining){notice="FLOOD INCOMING - SHOOT IT BACK";noticeTime=4;return true;}
    Enemy target;target.id=nextEnemyId++;target.kind=EnemyKind::Flood;target.trailSize=0;target.speed=0;
    // Water is displaced, never devoured, split or converted into loot.
    target.hp=target.maxHp=1e30;target.position=flood.nearest(player.position);target.radius=FloodRadius;
    placeEnemy(target,target.position,true);flood.enemyId=target.id;enemies.push_back(target);
    notice="FLOOD INCOMING - SHOOT IT BACK";noticeTime=4;return true;
}
void Game::directFlood(float dt){
    flood.clock+=dt;
    if(flood.enabled && flood.clock>=flood.nextSpawn){
        flood.nextSpawn+=FloodIntervalSeconds;
        if(!flood.active() && enemies.size()>=1800){
            const auto ordinary=std::find_if(enemies.begin(),enemies.end(),[](const Enemy& e){return e.alive && e.kind!=EnemyKind::Boss && e.kind!=EnemyKind::Flood;});
            if(ordinary!=enemies.end()){*ordinary=enemies.back();enemies.pop_back();}
        }
        // Independent of combat RNG, with enough lead distance to react.
        constexpr std::array<Vec,4> headings{{{-1,0},{0,1},{1,0},{0,-1}}};
        Vec heading=headings[std::size_t(flood.serial)%headings.size()];
        Vec at{sub(player.position,mul(heading,175))};
        if(at.x<60 || at.y<60 || at.x>ArenaSize-60 || at.y>ArenaSize-60){heading=mul(heading,-1);at=sub(player.position,mul(heading,175));}
        spawnFlood(at,heading);
    }
}
void Game::updateFlood(float dt,Vec headStart){
    const bool killed=flood.update(dt,player,enemies,greySnakes,headStart);
    for(auto& e:enemies)if(e.kind==EnemyKind::Flood){
        e.alive=flood.active() && e.id==flood.enemyId;
        if(e.alive){e.position=e.previous=gridPosition(flood.nearest(player.position));e.grabbed=false;e.devoured=false;}
    }
    if(killed){player.hp=0;phase=Phase::Dead;playerDied=true;notice="THE FLOOD CLAIMED YOUR HEAD";noticeTime=5;shake=3;events.push_back({Event::Hurt});}
}
}
