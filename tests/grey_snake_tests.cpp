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
    game.flood.enabled=false;
    game.spawnClock=-10000;game.bossClock=100;game.player.invulnerable=10000;game.player.moveClock=-10000;
    return game;
}
void movingPlayer(snake::Game& game,snake::Vec position){
    game.player.position=game.player.previous=position;game.player.direction=game.player.queued={1,0};
    game.player.length=1;game.player.segments={position};game.player.previousSegments=game.player.segments;game.player.moveClock=0;
}
void drive(snake::Game& game,snake::Vec direction,int cells){
    snake::Input input;input.direction=direction;
    for(int cell=0;cell<cells;++cell){
        const auto before=game.player.position;int ticks=0;
        while(same(before,game.player.position) && ticks++<10){
            game.update(snake::FixedStep,input);require(game.phase==snake::Phase::Playing,"Scripted route stopped playing");
        }
        require(!same(before,game.player.position),"Scripted route was obstructed");
    }
}
void record(snake::GreySnakeSystem& system,snake::Vec& position,snake::Vec direction,int cells){
    for(int i=0;i<cells;++i){position.x+=direction.x*snake::Tile;position.y+=direction.y*snake::Tile;
        system.clock+=.12;system.recordMove(position,direction);}
}
void wallLeg(snake::GreySnakeSystem& system,snake::GreySnake& trail,snake::Vec direction,int cells){
    for(int i=0;i<cells;++i){const auto p=trail.path[trail.count-1];
        system.append(trail,{p.x+direction.x*snake::Tile,p.y+direction.y*snake::Tile});}
}
void requireOpenArena(const snake::GreySnakeSystem& system){
    // Independent whole-map oracle, used only in tests. Runtime growth must
    // preserve one connected free region with at least two exits per cell.
    std::array<bool,snake::ArenaTiles*snake::ArenaTiles> seen{};
    std::array<std::uint16_t,snake::ArenaTiles*snake::ArenaTiles> queue{};
    int count=0,head=0;
    const auto free=[&](int x,int y){return x>=1 && y>=1 && x<snake::ArenaTiles && y<snake::ArenaTiles && !system.owners[std::size_t(x+y*snake::ArenaTiles)];};
    for(int y=1;y<snake::ArenaTiles && !count;++y)for(int x=1;x<snake::ArenaTiles;++x)if(free(x,y)){
        const int cell=x+y*snake::ArenaTiles;seen[std::size_t(cell)]=true;queue[std::size_t(count++)]=std::uint16_t(cell);break;}
    constexpr int dx[]{1,-1,0,0},dy[]{0,0,1,-1};
    while(head<count){
        const int cell=queue[std::size_t(head++)],x=cell%snake::ArenaTiles,y=cell/snake::ArenaTiles;int exits=0;
        for(int d=0;d<4;++d){const int nx=x+dx[d],ny=y+dy[d];if(!free(nx,ny))continue;++exits;
            const int next=nx+ny*snake::ArenaTiles;if(seen[std::size_t(next)])continue;
            seen[std::size_t(next)]=true;queue[std::size_t(count++)]=std::uint16_t(next);
        }
        require(exits>=2,"Grey walls left a dead-end free cell");
    }
    require(std::size_t(count)==std::size_t((snake::ArenaTiles-1)*(snake::ArenaTiles-1))-system.barrierCount,"Grey walls disconnected an enclosed pocket");
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
        test("grey growth cannot close its own loop and leaves a usable doorway",[&]{
            auto game=fixture(catalog);auto& system=game.greySnakes;system.enabled=false;
            system.trails.emplace_back();auto& trail=system.trails.back();trail.id=1;
            system.append(trail,{800,720});wallLeg(system,trail,{1,0},10);wallLeg(system,trail,{0,1},20);
            wallLeg(system,trail,{-1,0},20);wallLeg(system,trail,{0,-1},20);wallLeg(system,trail,{1,0},8);
            requireOpenArena(system);
            require(!system.preservesEscape({792,720}),"Closing an enclosing loop was allowed");
            require(system.preservesEscape({784,720}),"Retracing an existing wall was forbidden");
            trail.wall=true;trail.wallDirection={1,0};trail.target={808,720};trail.age=9.93;
            for(int tick=0;tick<4;++tick)game.update(snake::FixedStep,{});
            require(!system.blocks({792,720}),"Live grey head sealed the last doorway");
            const auto head=trail.path[trail.count-1];
            require(head.x==784 && std::abs(head.y-720)==8,"Grey head failed to take a safe alternative");
            game.update(snake::FixedStep,{});require(!trail.alive,"Fixture did not retire before the escape");
            movingPlayer(game,{792,800});drive(game,{0,-1},20);
            require(game.player.position.y==640,"Player could not escape through the preserved doorway");
            requireOpenArena(system);
        });
        test("growth cannot cap a pocket against the arena boundary or create corner dead ends",[&]{
            snake::GreySnakeSystem system;system.reset();system.trails.emplace_back();auto& trail=system.trails.back();trail.id=1;
            system.append(trail,{8,720});wallLeg(system,trail,{1,0},9);wallLeg(system,trail,{0,1},20);wallLeg(system,trail,{-1,0},8);
            requireOpenArena(system);
            require(!system.preservesEscape({8,880}),"Wall was allowed to seal a pocket against the arena edge");
            require(!system.preservesEscape({16,8}) && !system.preservesEscape({8,16}),"Single-cell corner dead end was allowed");
            require(system.preservesEscape({160,160}),"Ordinary open-space wall growth was rejected");
        });
        test("bounded local placement preserves global connectivity under seeded adversarial walls",[&]{
            snake::GreySnakeSystem system;system.reset();std::mt19937 random(5143);int accepted=0,rejected=0;
            for(int attempt=0;attempt<5000;++attempt){
                const int x=1+int(random()%std::uint32_t(snake::ArenaTiles-2)),y=1+int(random()%std::uint32_t(snake::ArenaTiles-2));
                const snake::Vec p{float(x*snake::Tile),float(y*snake::Tile)};
                if(system.blocks(p))continue;
                if(!system.preservesEscape(p)){++rejected;continue;}
                system.owners[std::size_t(x+y*snake::ArenaTiles)]=std::uint8_t(1+accepted%int(snake::GreyTrailCapacity));++system.barrierCount;++accepted;
                if(accepted%128==0)requireOpenArena(system);
            }
            require(accepted>3000 && rejected>0,"Adversarial fixture did not exercise both growth and rejection");
            requireOpenArena(system);
        });
        test("straight movement summons a side approach that cuts the lane and permits escape",[&]{
            auto game=fixture(catalog);movingPlayer(game,{320,800});drive(game,{1,0},84);
            require(game.greySnakes.trails.size()==1,"Straight movement retained the ninety-second spawn delay");
            const auto& trail=game.greySnakes.trails.front();
            require(trail.cutting,"Predictable movement spawned a random grey snake");
            require(game.greySnakes.clock>=snake::GreyFirstPressureSeconds && game.greySnakes.clock<11,"Initial pressure grace changed");
            require(trail.wallDirection.x==0 && std::abs(trail.wallDirection.y)==1,"Cut did not cross the straight lane");
            require(std::abs(trail.path[0].y-800)>=12*snake::Tile,"Grey head spawned on the player's lane");
            require(trail.target.x>game.player.position.x,"Intercept was aimed behind the player");
            require(game.greySnakes.spawnTime()>=39.99,"Pressure scheduled overlapping grey lifetimes");
            for(int tick=0;tick<180 && game.player.collisionClock<0;++tick)game.update(snake::FixedStep,{});
            require(game.phase==snake::Phase::Playing && game.player.collisionClock>=0,"Grey snake failed to cut the predicted movement");
            require(game.greySnakes.blocks({game.player.position.x+snake::Tile,game.player.position.y}),"Collision grace was not caused by the cut");
            drive(game,{0,-1},1);require(game.player.collisionClock<0,"A deliberate turn could not escape the cut");
        });
        test("fresh varied movement keeps the ordinary spawn interval",[&]{
            auto game=fixture(catalog);movingPlayer(game,{160,160});
            for(int i=0;i<20;++i){drive(game,{1,0},5);drive(game,{0,1},5);}
            require(game.greySnakes.clock>23 && game.greySnakes.trails.empty(),"New routes triggered an early grey spawn");
            require(game.greySnakes.predictability()==0 && game.greySnakes.spawnTime()==snake::GreySpawnSeconds,"Varied movement built prediction pressure");
        });
        test("repeated rectangles predict the next corner and tolerate small lane shifts",[&]{
            snake::GreySnakeSystem system;system.reset();snake::Vec p{800,800};
            for(int lap=0;lap<2;++lap){record(system,p,{1,0},12);record(system,p,{0,1},10);record(system,p,{-1,0},12);record(system,p,{0,-1},10);}
            record(system,p,{1,0},8);
            require(system.straightSteps==8 && system.predictability()==1,"Turning at every corner evaded repeated-route pressure");
            snake::Vec heading;
            require(same(system.predict(p,{1,0},18,heading),{864,880}) && same(heading,{-1,0}),"Prediction missed the rectangle's next two corners");
            require(same(system.predict({p.x,p.y+8},{1,0},18,heading),{864,888}),"One-cell lane shift erased the learned route");
            for(int i=0;i<6;++i){record(system,p,{0,-1},5);record(system,p,{1,0},5);}
            require(system.predictability()==0,"Fresh movement did not shed repeated-route pressure");
            system.reset();require(!system.routeCount && system.predictability()==0 && !system.attemptedSpawn,"Reset retained a previous run's movement pattern");
        });
        test("thin up and down circuits raise pressure and history stays bounded",[&]{
            snake::GreySnakeSystem system;system.reset();snake::Vec p{800,800};
            for(int lap=0;lap<12;++lap){record(system,p,{0,1},30);record(system,p,{1,0},4);record(system,p,{0,-1},30);record(system,p,{-1,0},4);}
            require(system.routeCount==snake::GreyRouteCapacity && system.predictability()==1,"Back-and-forth circuit evaded pressure or overflowed history");
            system.attemptedSpawn=true;system.nextSpawn=system.clock+snake::GreySpawnSeconds;
            require(std::abs(system.spawnTime()-system.clock-snake::GreyPressureSpawnSeconds)<1e-6,"Repeated movement did not shorten the next spawn interval");
            system.clock+=2;require(system.predictability()==0,"A frozen player kept stale movement pressure");
        });
        test("full run keeps bounded permanent trails without reallocating",[&]{
          for(bool pressure:{false,true}){
            auto game=fixture(catalog);const auto* allocation=game.greySnakes.trails.data();
            std::size_t previousBarriers=0,peakTrails=0;int attempts=0;double lastAttempt=-100;
            for(int tick=0;tick<int(snake::RunDuration*60)+180 && game.phase==snake::Phase::Playing;++tick){
                // Isolate scheduling/capacity from steering around permanent
                // walls. Actual-movement pressure is covered by route tests.
                if(pressure){game.greySnakes.straightSteps=48;game.greySnakes.lastMoveClock=game.greySnakes.clock;}
                const double nextSpawn=game.greySnakes.nextSpawn;
                game.update(snake::FixedStep,{});
                if(game.greySnakes.nextSpawn!=nextSpawn){
                    require(game.greySnakes.clock-lastAttempt+1e-6>=(pressure?snake::GreyPressureSpawnSeconds:snake::GreySpawnSeconds),"Spawn attempts ignored the cooldown");
                    lastAttempt=game.greySnakes.clock;++attempts;
                }
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
                if(tick%1200==0)requireOpenArena(game.greySnakes);
            }
            require(game.phase==snake::Phase::Victory,"Grey lifecycle run did not reach victory");
            require(peakTrails>1 && previousBarriers>100,"Full run did not exercise persistent barriers");
            require(attempts<=int(snake::GreyTrailCapacity),"Adaptive schedule exceeded reserved trail storage");
            if(pressure)require(attempts>int(snake::RunDuration/snake::GreySpawnSeconds),"Sustained pressure did not increase full-run spawn attempts");
          }
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
