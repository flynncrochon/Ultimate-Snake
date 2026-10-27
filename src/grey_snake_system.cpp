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
}

void GreySnakeSystem::reset() {
    trails.clear();trails.reserve(GreyTrailCapacity);owners.fill(0);
    clock=0;nextSpawn=GreySpawnSeconds;barrierCount=0;
}
bool GreySnakeSystem::blocks(Vec position) const {
    return inGridBounds(position) && owners[std::size_t(cellIndex(position))]!=0;
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
    const float ahead=random(25,35);
    Vec position{float(std::clamp(int(std::floor(player.position.x/Tile+player.direction.x*ahead+.5f)),1,ArenaTiles-2)*Tile),
                 float(std::clamp(int(std::floor(player.position.y/Tile+player.direction.y*ahead+.5f)),1,ArenaTiles-2)*Tile)};
    if(greySnakes.overlapsOther(position)){
        bool found=false;
        constexpr Vec offsets[]{{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
        for(int radius=2;radius<=10 && !found;radius+=2)for(Vec offset:offsets){
            const Vec candidate{position.x+offset.x*float(radius*Tile),position.y+offset.y*float(radius*Tile)};
            if(interior(candidate) && !greySnakes.overlapsOther(candidate)){position=candidate;found=true;break;}
        }
        if(!found)return;
    }
    greySnakes.trails.emplace_back();auto& snake=greySnakes.trails.back();
    snake.id=std::uint8_t(greySnakes.trails.size());snake.previous=position;greySnakes.append(snake,position);
    planGreyIntercept(snake);
}
void Game::planGreyIntercept(GreySnake& snake) {
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
    if(std::abs(head.x-player.position.x)+std::abs(head.y-player.position.y)<8*Tile){
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
            (!player.segments.empty() && adjacent(p,player.segments.back()));
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
    if(greySnakes.enabled && greySnakes.clock+1e-6>=greySnakes.nextSpawn && !active){
        greySnakes.nextSpawn=greySnakes.clock+GreySpawnSeconds;spawnGreySnake();
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
