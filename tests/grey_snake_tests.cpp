#include "core.hpp"
#include "grid.hpp"
#include "render_motion.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool same(snake::Vec a,snake::Vec b){return a.x==b.x && a.y==b.y;}
snake::Game fixture(const snake::Catalog& catalog){
    snake::Game game(catalog,710);game.owned.clear();game.enemies.clear();game.pickups.clear();
    game.spawnClock=-10000;game.bossClock=100;game.player.invulnerable=10000;game.player.moveClock=-10000;
    return game;
}
}
int main(){
    int passed=0,failed=0;
    const auto test=[&](const char* name,const std::function<void()>& body){
        try{body();++passed;std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}
    };
    try{
        const auto catalog=snake::Catalog::load(SNAKE_TEST_DATA_DIR);
        test("full run keeps bounded permanent trails without reallocating",[&]{
            auto game=fixture(catalog);const auto* allocation=game.greySnakes.trails.data();
            std::size_t previousBarriers=0,peakTrails=0;
            for(int tick=0;tick<72180 && game.phase==snake::Phase::Playing;++tick){
                game.update(snake::FixedStep,{});
                require(game.greySnakes.trails.data()==allocation,"Grey trails reallocated during gameplay");
                require(game.greySnakes.barrierCount>=previousBarriers,"Permanent barriers disappeared");
                require(game.greySnakes.trails.size()<=snake::GreyTrailCapacity,"Trail capacity exceeded");
                previousBarriers=game.greySnakes.barrierCount;peakTrails=std::max(peakTrails,game.greySnakes.trails.size());
                int active=0;
                for(const auto& trail:game.greySnakes.trails){
                    active+=trail.alive?1:0;require(trail.count<=snake::GreyPathCapacity,"Path capacity exceeded");
                    if(tick%120==0)for(std::size_t i=0;i<trail.count;++i){
                        const auto p=trail.path[i];require(game.greySnakes.blocks(p),"A visited cell stopped blocking");
                        require(p.x>=snake::Tile && p.y>=snake::Tile && p.x<=(snake::ArenaTiles-2)*snake::Tile && p.y<=(snake::ArenaTiles-2)*snake::Tile,"Grey snake entered a boundary cell");
                        require(!game.greySnakes.overlapsOther(p,trail.id),"Different permanent walls became adjacent");
                        if(i)require(std::abs(p.x-trail.path[i-1].x)+std::abs(p.y-trail.path[i-1].y)==snake::Tile,"Grey path disconnected");
                    }
                }
                require(active<=1,"More than one grey head became active");
            }
            require(game.phase==snake::Phase::Victory,"Grey lifecycle run did not reach victory");
            require(peakTrails>1 && previousBarriers>100,"Full run did not exercise persistent barriers");
        });
        test("fixed updates are independent of render cadence and render sampling",[&]{
            auto a=fixture(catalog),b=fixture(catalog);a.greySnakes.clock=b.greySnakes.clock=89.95;
            snake::render::MotionHistory low,high;
            for(int tick=0;tick<650;++tick){
                low.beginStep(a);high.beginStep(b);a.update(snake::FixedStep,{});b.update(snake::FixedStep,{});
                for(const auto& trail:a.greySnakes.trails)(void)low.sampleGrey(trail,1,true);
                for(float alpha:{0.0f,.25f,.5f,.75f,1.0f})for(const auto& trail:b.greySnakes.trails)(void)high.sampleGrey(trail,alpha,true);
            }
            require(a.greySnakes.owners==b.greySnakes.owners,"Rendering changed grey paths or occupancy");
            require(a.greySnakes.clock==b.greySnakes.clock && a.greySnakes.nextSpawn==b.greySnakes.nextSpawn,"Rendering changed the schedule");
            require(a.greySnakes.trails[0].count==167 && b.greySnakes.trails[0].count==167,"Ten-second growth count changed");
        });
        test("head interpolation crosses a turn while the old trail stays fixed",[&]{
            auto game=fixture(catalog);auto& system=game.greySnakes;system.enabled=false;
            system.trails.emplace_back();auto& trail=system.trails.back();trail.id=1;
            system.append(trail,{960,800});system.append(trail,{968,800});
            trail.previous={960,800};trail.direction={1,0};trail.moveClock=.05;
            snake::render::MotionHistory history;history.beginStep(game);
            trail.previous={968,800};system.append(trail,{968,808});trail.direction={0,1};trail.moveClock=.01;
            const auto first=history.sampleGrey(trail,0,true),last=history.sampleGrey(trail,1,true);
            require(first.count==2 && same(first.previous,{960,800}) && same(first.current,{968,800}),"Turn lost the incoming edge");
            require(last.count==3 && same(last.previous,{968,800}) && same(last.current,{968,808}),"Turn lost the outgoing edge");
            float distance=-1;
            for(int frame=0;frame<=24;++frame){
                const auto p=history.sampleGrey(trail,float(frame)/24,true);
                const snake::Vec head{p.previous.x+(p.current.x-p.previous.x)*p.alpha,p.previous.y+(p.current.y-p.previous.y)*p.alpha};
                const float along=head.x-960+head.y-800;
                if(distance>=0)require(along>=distance && along-distance<1,"Head snapped or moved backwards at the turn");distance=along;
                require(head.x==968 || head.y==800,"Head cut diagonally across the turn");
            }
        });
        test("retirement finishes the last head edge at unchanged speed",[&]{
            auto game=fixture(catalog);game.greySnakes.enabled=false;
            game.greySnakes.trails.emplace_back();auto& trail=game.greySnakes.trails.back();trail.id=1;
            game.greySnakes.append(trail,{960,800});game.greySnakes.append(trail,{968,800});
            trail.previous={960,800};trail.direction={1,0};trail.age=10-double(snake::FixedStep);trail.moveClock=.023333333;
            snake::render::MotionHistory history;history.beginStep(game);game.update(snake::FixedStep,{});
            require(!trail.alive && trail.settling,"Retirement discarded the unfinished edge");
            const auto begin=history.sampleGrey(trail,0,true),end=history.sampleGrey(trail,1,true);
            require(std::abs((end.alpha-begin.alpha)*8-float(snake::FixedStep)*8/float(snake::GreyMoveSeconds))<.0001f,"Retiring head accelerated or snapped");
            game.togglePause();game.update(.1f,{});require(trail.settling && trail.moveClock<snake::GreyMoveSeconds,"Pause advanced the retiring head");game.togglePause();
            for(int tick=0;tick<3;++tick){history.beginStep(game);game.update(snake::FixedStep,{});}
            const auto settled=history.sampleGrey(trail,1,true);
            require(!trail.settling && settled.alpha==1 && !settled.alive,"Retired head did not become a stationary wall");
        });
        test("a fully obstructed spawn consumes one interval and stays bounded",[&]{
            auto game=fixture(catalog);game.greySnakes.owners.fill(1);game.greySnakes.barrierCount=game.greySnakes.owners.size();
            // Keep the player's current cell clear; surround every spawn search.
            game.greySnakes.owners[100+100*snake::ArenaTiles]=0;--game.greySnakes.barrierCount;
            game.greySnakes.clock=89.99;game.update(snake::FixedStep,{});
            require(game.greySnakes.trails.empty(),"Obstructed spawn ignored old barriers");
            require(game.greySnakes.nextSpawn>179,"Failed spawn retried every frame");
        });
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
