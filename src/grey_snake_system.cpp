#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <cmath>

namespace snake {
namespace {
int cellIndex(Vec p) {return int(p.x)/Tile+int(p.y)/Tile*ArenaTiles;}
bool interior(Vec p) {
    return p.x>=Tile && p.y>=Tile && p.x<=float((ArenaTiles-2)*Tile) && p.y<=float((ArenaTiles-2)*Tile);
}
float sign(float value) {return value>0?1.0f:value<0?-1.0f:0;}
Vec nextCell(Vec head,Vec direction) {return {head.x+direction.x*Tile,head.y+direction.y*Tile};}
bool adjacent(Vec a,Vec b) {return std::abs(a.x-b.x)<=Tile && std::abs(a.y-b.y)<=Tile;}
bool sameDirection(Vec a,Vec b) {return a.x==b.x && a.y==b.y;}
float distance(Vec a,Vec b) {return std::abs(a.x-b.x)+std::abs(a.y-b.y);}
Vec clampInterior(Vec p,int margin=1) {
    return {std::clamp(p.x,float(margin*Tile),float((ArenaTiles-1-margin)*Tile)),
            std::clamp(p.y,float(margin*Tile),float((ArenaTiles-1-margin)*Tile))};
}
bool sameLane(Vec a,Vec b,Vec direction) {
    const float along=std::abs((a.x-b.x)*direction.x+(a.y-b.y)*direction.y);
    const float across=std::abs((a.x-b.x)*direction.y-(a.y-b.y)*direction.x);
    return along<=Tile && across<=2*Tile;
}
int interceptSteps(const Player& player) {
    // Give the player about two seconds of travel before the cut. The grey
    // head approaches from the side rather than materializing on that lane.
    return std::clamp(int(std::ceil(1.8f/std::max(.01f,player.moveInterval))),12,32);
}
}

void GreySnakeSystem::reset() {
    trails.clear();trails.reserve(GreyTrailCapacity);owners.fill(0);
    clock=0;nextSpawn=GreySpawnSeconds;barrierCount=0;
    routeCount=routeNext=0;straightSteps=repeatedSteps=0;lastMoveClock=-2;attemptedSpawn=false;
}
void GreySnakeSystem::recordMove(Vec position,Vec direction) {
    if(routeCount){
        const auto& last=route[(routeNext+GreyRouteCapacity-1)%GreyRouteCapacity];
        // Test relocations and any future teleports are not travelled routes.
        if(distance(position,last.position)!=Tile){routeCount=routeNext=0;straightSteps=repeatedSteps=0;}
        else straightSteps=sameDirection(direction,last.direction)?std::min(64,straightSteps+1):1;
    }
    if(!routeCount)straightSteps=1;
    bool repeated=false;
    // Ignore the immediately preceding sixteen cells. Nearby parallel lanes
    // still count, so nudging a rectangle sideways cannot clear its history.
    for(std::size_t age=16;age<=routeCount;++age){
        const auto& old=route[(routeNext+GreyRouteCapacity-age)%GreyRouteCapacity];
        if(std::abs(old.direction.x)==std::abs(direction.x) && sameLane(position,old.position,direction)){repeated=true;break;}
    }
    repeatedSteps=std::clamp(repeatedSteps+(repeated?1:-2),0,64);
    route[routeNext]={position,direction};routeNext=(routeNext+1)%GreyRouteCapacity;
    routeCount=std::min(GreyRouteCapacity,routeCount+1);lastMoveClock=clock;
}
float GreySnakeSystem::predictability() const {
    if(clock-lastMoveClock>1)return 0;
    return std::max(std::clamp(float(straightSteps-20)/28,0.0f,1.0f),
                    std::clamp(float(repeatedSteps-8)/24,0.0f,1.0f));
}
double GreySnakeSystem::spawnTime() const {
    const double minimum=attemptedSpawn?GreyPressureSpawnSeconds:GreyFirstPressureSeconds;
    return nextSpawn-(GreySpawnSeconds-minimum)*double(predictability());
}
Vec GreySnakeSystem::predict(Vec position,Vec direction,int steps,Vec& heading) const {
    heading=direction;
    Vec predicted{position.x+direction.x*float(steps*Tile),position.y+direction.y*float(steps*Tile)};
    float best=4*Tile;
    // A matching older lane includes its recorded future. This anticipates a
    // rectangle's next corner, instead of perpetually aiming beyond it.
    for(std::size_t age=std::size_t(steps+1);age<=routeCount;++age){
        const auto& old=route[(routeNext+GreyRouteCapacity-age)%GreyRouteCapacity];
        if(!sameDirection(old.direction,direction) || !sameLane(position,old.position,direction))continue;
        const float score=distance(position,old.position);
        if(score>=best)continue;
        const auto& future=route[(routeNext+GreyRouteCapacity-age+std::size_t(steps))%GreyRouteCapacity];
        predicted={future.position.x+position.x-old.position.x,future.position.y+position.y-old.position.y};
        heading=future.direction;best=score;
        if(score==0)break;
    }
    return clampInterior(predicted,2);
}
bool GreySnakeSystem::blocks(Vec position) const {
    return inGridBounds(position) && owners[std::size_t(cellIndex(position))]!=0;
}
bool GreySnakeSystem::preservesEscape(Vec position) const {
    if(!interior(position))return false;
    if(blocks(position))return true; // Retracing a wall adds no obstruction.
    const auto open=[&](Vec p){return inGridBounds(p) && !blocks(p);};
    constexpr Vec cardinal[]{{0,-1},{1,0},{0,1},{-1,0}};
    // Removing a cell must not leave any adjacent free cell with a single
    // exit. This also protects narrow passages and the arena's corner cells.
    for(Vec direction:cardinal){
        const Vec neighbor=nextCell(position,direction);
        if(!open(neighbor))continue;
        int exits=0;
        for(Vec other:cardinal){const Vec p=nextCell(neighbor,other);
            if(!sameCell(p,position) && open(p))++exits;}
        if(exits<2)return false;
    }
    // The eight surrounding cells form a four-connected ring. Every open
    // cardinal neighbor must remain connected around the proposed wall cell.
    // Therefore any path through that cell has a local detour, so growth can
    // never split the arena's free space or close an enclosing barrier loop.
    // This deliberately conservative test costs at most eight ring expansions;
    // there is no flood fill, arena scan, scratch allocation, or cached route.
    constexpr Vec ring[]{{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
    unsigned mask=0;
    for(unsigned i=0;i<8;++i)
        if(open({position.x+ring[i].x*Tile,position.y+ring[i].y*Tile}))mask|=1u<<i;
    const unsigned exits=mask&0xaau;
    if(!exits)return false;
    unsigned connected=exits&(~exits+1u);
    for(int step=0;step<7;++step){
        const unsigned neighbors=((connected<<1)|(connected>>7)|(connected>>1)|(connected<<7))&0xffu;
        const unsigned expanded=connected|(neighbors&mask);
        if(expanded==connected)break;
        connected=expanded;
    }
    return (exits&~connected)==0;
}
bool GreySnakeSystem::overlapsOther(Vec position,std::uint8_t owner) const {
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y){
        const Vec p{position.x+float(x*Tile),position.y+float(y*Tile)};
        if(!inGridBounds(p))continue;
        const auto other=owners[std::size_t(cellIndex(p))];
        if(other!=0 && other!=owner)return true;
    }
    return false;
}
void GreySnakeSystem::append(GreySnake& snake,Vec position) {
    if(snake.count>=snake.path.size() || !interior(position))return;
    if(snake.count==0)snake.minimum=snake.maximum=position;
    else {
        snake.minimum={std::min(snake.minimum.x,position.x),std::min(snake.minimum.y,position.y)};
        snake.maximum={std::max(snake.maximum.x,position.x),std::max(snake.maximum.y,position.y)};
    }
    snake.path[snake.count++]=position;
    auto& owner=owners[std::size_t(cellIndex(position))];
    if(owner==0){owner=snake.id;++barrierCount;}
}

void Game::spawnGreySnake() {
    if(greySnakes.trails.size()>=GreyTrailCapacity)return;
    const bool cutting=greySnakes.predictability()>=.5f;
    Vec position,crossing,normal;
    if(cutting){
        Vec heading;
        crossing=greySnakes.predict(player.position,player.direction,interceptSteps(player),heading);
        normal={-heading.y,heading.x};
        if(random(0,1)<.5f)normal={-normal.x,-normal.y};
        position={crossing.x-normal.x*16*Tile,crossing.y-normal.y*16*Tile};
    }else{
        const float ahead=random(25,35);
        position={float(std::clamp(int(std::floor(player.position.x/Tile+player.direction.x*ahead+.5f)),1,ArenaTiles-2)*Tile),
                  float(std::clamp(int(std::floor(player.position.y/Tile+player.direction.y*ahead+.5f)),1,ArenaTiles-2)*Tile)};
    }
    const auto safe=[&](Vec p){
        if(!interior(p) || greySnakes.overlapsOther(p) || distance(p,player.position)<12*Tile || !greySnakes.preservesEscape(p))return false;
        if(cutting && std::abs((p.x-crossing.x)*normal.x+(p.y-crossing.y)*normal.y)<12*Tile)return false;
        return std::none_of(player.segments.begin(),player.segments.end(),[&](Vec body){return adjacent(p,body);});
    };
    if(!safe(position)){
        bool found=false;
        // Try the other side of the intended crossing before bounded offsets.
        if(cutting){const Vec other{crossing.x+normal.x*16*Tile,crossing.y+normal.y*16*Tile};
            if(safe(other)){position=other;found=true;}}
        constexpr Vec offsets[]{{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
        for(int radius=2;radius<=10 && !found;radius+=2)for(Vec offset:offsets){
            const Vec candidate{position.x+offset.x*float(radius*Tile),position.y+offset.y*float(radius*Tile)};
            if(safe(candidate)){position=candidate;found=true;break;}
        }
        if(!found)return;
    }
    greySnakes.trails.emplace_back();auto& snake=greySnakes.trails.back();
    snake.id=std::uint8_t(greySnakes.trails.size());snake.cutting=cutting;
    snake.previous=position;greySnakes.append(snake,position);
    planGreyIntercept(snake);
}
void Game::planGreyIntercept(GreySnake& snake) {
    if(snake.cutting){
        Vec heading;
        const Vec crossing=greySnakes.predict(player.position,player.direction,interceptSteps(player),heading);
        Vec normal{-heading.y,heading.x};
        const Vec head=snake.path[snake.count-1];
        if((crossing.x-head.x)*normal.x+(crossing.y-head.y)*normal.y<0)normal={-normal.x,-normal.y};
        snake.target=clampInterior({crossing.x-normal.x*6*Tile,crossing.y-normal.y*6*Tile});
        snake.wallDirection=normal;snake.wall=false;snake.wallTicks=0;
        return;
    }
    const float look=float(15+int(random(0,10)));
    snake.target={std::clamp(player.position.x+player.direction.x*look*Tile,float(2*Tile),float((ArenaTiles-3)*Tile)),
                  std::clamp(player.position.y+player.direction.y*look*Tile,float(2*Tile),float((ArenaTiles-3)*Tile))};
    snake.wall=false;snake.wallTicks=0;
    snake.wallDirection={-player.direction.y,player.direction.x};
    if(snake.wallDirection.x==0 && snake.wallDirection.y==0)snake.wallDirection={1,0};
    if(random(0,1)<.5f){snake.wallDirection.x=-snake.wallDirection.x;snake.wallDirection.y=-snake.wallDirection.y;}
}
void Game::tickGreySnake(GreySnake& snake) {
    const Vec head=snake.path[snake.count-1];
    snake.previous=head;
    const auto toward=[&](Vec target){
        const float dx=target.x-head.x,dy=target.y-head.y,total=std::abs(dx)+std::abs(dy);
        if(total==0)return Vec{};
        return random(0,1)<std::abs(dx)/total?Vec{sign(dx),0}:Vec{0,sign(dy)};
    };
    Vec direction;
    // A deliberate cut must be allowed to finish nearer its lane. The shared
    // head/tail and old-wall clearance checks still apply to every step.
    if(distance(head,player.position)<(snake.cutting?3:8)*Tile){
        const Vec away{head.x-player.position.x,head.y-player.position.y};
        direction=away.x==0 && away.y==0?Vec{1,0}:toward({head.x+away.x,head.y+away.y});
        if(snake.wall)planGreyIntercept(snake);
    }else if(!snake.wall){
        if(std::abs(head.x-snake.target.x)+std::abs(head.y-snake.target.y)<=Tile){snake.wall=true;snake.wallTicks=0;direction=snake.wallDirection;}
        else direction=toward(snake.target);
    }else {
        ++snake.wallTicks;direction=snake.wallDirection;
        if(snake.wallTicks>=12){planGreyIntercept(snake);direction=toward(snake.target);}
    }
    if(direction.x==0 && direction.y==0)return;
    Vec destination=nextCell(head,direction);
    if(!interior(destination)){
        if(snake.wall){snake.wallDirection={-snake.wallDirection.x,-snake.wallDirection.y};direction=snake.wallDirection;}
        else if(direction.x!=0){const float dy=sign(snake.target.y-head.y);direction={0,dy==0?1:dy};}
        else {const float dx=sign(snake.target.x-head.x);direction={dx==0?1:dx,0};}
        destination=nextCell(head,direction);if(!interior(destination))return;
    }
    const auto blocked=[&](Vec p){
        return greySnakes.overlapsOther(p,snake.id) || adjacent(p,player.position) ||
            (!player.segments.empty() && adjacent(p,player.segments.back())) || !greySnakes.preservesEscape(p);
    };
    if(blocked(destination)){
        bool found=false;
        const Vec alternatives[]{{-direction.y,direction.x},{direction.y,-direction.x}};
        for(Vec alternative:alternatives){const Vec candidate=nextCell(head,alternative);
            if(interior(candidate) && !blocked(candidate)){direction=alternative;destination=candidate;found=true;break;}}
        if(!found){if(snake.wall)planGreyIntercept(snake);return;}
    }
    snake.direction=direction;greySnakes.append(snake,destination);
}
void Game::updateGreySnakes(float dt) {
    greySnakes.clock+=double(dt);
    const bool active=std::any_of(greySnakes.trails.begin(),greySnakes.trails.end(),[](const GreySnake& snake){return snake.alive;});
    if(greySnakes.enabled && greySnakes.clock+1e-6>=greySnakes.spawnTime() && !active){
        greySnakes.nextSpawn=greySnakes.clock+GreySpawnSeconds;greySnakes.attemptedSpawn=true;spawnGreySnake();
        // JavaScript's newly spawned head has age zero in this update.
        return;
    }
    for(auto& snake:greySnakes.trails){
        if(!snake.alive){
            if(snake.settling){snake.moveClock=std::min(GreyMoveSeconds,snake.moveClock+double(dt));snake.settling=snake.moveClock<GreyMoveSeconds;}
            continue;
        }
        snake.age+=double(dt);
        snake.moveClock+=double(dt);
        if(snake.age+1e-6>=GreyLifetimeSeconds){snake.age=GreyLifetimeSeconds;snake.alive=false;snake.settling=snake.moveClock<GreyMoveSeconds;continue;}
        // dt is capped at 0.1s by Game::update, so this catches up at most twice.
        for(int moves=0;moves<2 && snake.moveClock+1e-6>=GreyMoveSeconds;++moves){
            snake.moveClock=std::max(0.0,snake.moveClock-GreyMoveSeconds);tickGreySnake(snake);
        }
    }
}
}
