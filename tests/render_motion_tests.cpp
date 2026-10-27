#include "render_motion.hpp"
#include "snake_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef SNAKE_TEST_DATA_DIR
#define SNAKE_TEST_DATA_DIR "data"
#endif

namespace {
using namespace snake;
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
bool same(Vec a,Vec b){return a.x==b.x && a.y==b.y;}
void prepare(Game& game){game.enemies.clear();game.pickups.clear();game.owned.clear();game.spawnClock=-10000;game.bossClock=10;game.player.invulnerable=10000;}
template<std::size_t N> geometry::SnakeGeometry shape(const render::SnakePose<N>& pose){return geometry::buildSnake(pose.previous.data(),pose.current.data(),pose.count,pose.alpha,14,2.5f);}
template<std::size_t N> geometry::Point head(const render::SnakePose<N>& pose){const auto result=shape(pose);require(result.valid && result.count>0,"Interpolated cardinal pose became invalid");return {result.centers[0].x+int(std::lround(pose.offset.x*2.5f)),result.centers[0].y+int(std::lround(pose.offset.y*2.5f))};}
void move(Player& actor,Vec direction,float clock){
    actor.previous=actor.position;actor.previousSegments=actor.segments;actor.direction=direction;
    actor.position={actor.position.x+direction.x*Tile,actor.position.y+direction.y*Tile};
    for(std::size_t i=actor.segments.size()-1;i>0;--i)actor.segments[i]=actor.segments[i-1];actor.segments[0]=actor.position;actor.moveClock=clock;
}
Enemy makeEnemy(std::uint32_t id,float x,float phaseValue){
    Enemy enemy;enemy.id=id;enemy.position={x,800};enemy.previous={x-8,800};enemy.direction={1,0};enemy.motionReady=true;enemy.moveInterval=.12f;enemy.moveClock=phaseValue*.12f;enemy.trailSize=2;
    for(std::size_t i=0;i<enemy.trail.size();++i){enemy.trail[i]={x-float(i+1)*8,800};enemy.previousTrail[i]={x-float(i+2)*8,800};}return enemy;
}
void move(Enemy& actor,float clock){actor.previous=actor.position;actor.previousTrail=actor.trail;for(int i=actor.trailSize-1;i>0;--i)actor.trail[std::size_t(i)]=actor.trail[std::size_t(i-1)];actor.trail[0]=actor.position;actor.position.x+=8;actor.moveClock=clock;}
MiniSnake makeMini(std::uint32_t id,float x){MiniSnake mini;mini.id=id;mini.position={x,800};mini.previous={x-8,800};mini.moveInterval=.08f;mini.moveClock=.06f;for(std::size_t i=0;i<3;++i){mini.segments[i]={x-float(i)*8,800};mini.previousSegments[i]={x-float(i+1)*8,800};}return mini;}
}

int main(){
    int passed=0,failed=0;auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"[PASS] "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"[FAIL] "<<name<<": "<<e.what()<<'\n';}};
    const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
    test("120 144 and 240 Hz traces follow constant velocity without 60 Hz holds",[&]{
        for(int refresh:{60,120,144,240}){
            Game game(catalog,17);prepare(game);render::MotionHistory history;double accumulator=0,time=0;int previous=2000;
            for(int frame=0;frame<refresh*3;++frame){
                const double dt=1.0/double(refresh);time+=dt;accumulator+=dt;
                while(accumulator>=double(FixedStep)){history.beginStep(game);game.update(FixedStep,{{1,0}});accumulator-=FixedStep;}
                const auto point=head(history.samplePlayer(game.player,float(accumulator/FixedStep),true));
                if(time>.35){
                    const double expected=2000+(time-double(FixedStep)-double(.12f))*(20.0/double(.12f));
                    require(std::abs(double(point.x)-expected)<1.01,"Render clock drifted from constant velocity at "+std::to_string(refresh)+" Hz");
                    require(point.x>=previous,"Straight movement ran backward");
                    const int maxAdvance=int(std::ceil((20.0/double(.12f))/double(refresh)));
                    require(point.x-previous<=maxAdvance,"A render frame jumped by a full 60 Hz movement chunk");
                    if(refresh==120 || refresh==144)require(point.x>previous,"High-refresh movement held an old 60 Hz pose");
                }
                previous=point.x;
            }
        }
    });
    test("uneven render cadence and catch-up updates preserve the same motion clock",[&]{
        Game game(catalog,19);prepare(game);render::MotionHistory history;double accumulator=0,time=0;
        const double durations[]{1.0/240,1.0/90,1.0/144,.04,1.0/120};
        for(int frame=0;frame<400;++frame){time+=durations[frame%5];accumulator+=durations[frame%5];while(accumulator>=double(FixedStep)){history.beginStep(game);game.update(FixedStep,{{1,0}});accumulator-=FixedStep;}
            if(time>.4){const auto point=head(history.samplePlayer(game.player,float(accumulator/FixedStep),true));const double expected=2000+(time-double(FixedStep)-double(.12f))*(20.0/double(.12f));require(std::abs(double(point.x)-expected)<1.01,"Catch-up frames lost or duplicated interpolation time");}}
    });
    test("cell crossing follows the old elbow then the new edge without a clamp plateau",[&]{
        Game game(catalog,23);prepare(game);game.player.moveClock=.108f;game.player.previousSegments=game.player.segments;
        for(auto& point:game.player.previousSegments)point.x-=8;game.player.previous.x-=8;
        render::MotionHistory history;history.beginStep(game);move(game.player,{0,1},.108f+FixedStep-.12f);
        const auto start=head(history.samplePlayer(game.player,0,true));const auto end=head(history.samplePlayer(game.player,1,true));
        require(start.x==1998 && start.y==2000 && end.x==2000 && end.y==2001,"Boundary endpoints lost the cardinal centerline");
        for(int i=0;i<=40;++i){const auto pose=history.samplePlayer(game.player,float(i)/40,true);const auto p=head(pose);require(p.y==2000 || p.x==2000,"Turn interpolation cut diagonally across the elbow");require(p.x>=1998 && p.x<=2000 && p.y>=2000 && p.y<=2001,"Turn overshot its historical path");}
        const auto nearEnd=history.samplePlayer(game.player,.95f,true);require(nearEnd.alpha>0 && nearEnd.direction.y==1,"Render phase clamped at the cell boundary instead of entering the next edge");
    });
    test("speed changes preserve normalized movement phase",[&]{
        Game game(catalog,29);prepare(game);game.player.moveClock=.09f;game.player.previousSegments=game.player.segments;for(auto& point:game.player.previousSegments)point.x-=8;
        render::MotionHistory history;history.beginStep(game);game.player.moveInterval=.06f;move(game.player,{1,0},.75f*.06f+FixedStep-.06f);
        int previous=head(history.samplePlayer(game.player,0,true)).x;
        for(int i=1;i<=20;++i){const int point=head(history.samplePlayer(game.player,float(i)/20,true)).x;require(point>=previous && point-previous<=1,"A speed increase jumped or reversed within a rendered fixed update");previous=point;}
        require(previous==2001,"A speed increase did not retain its fractional progress");
        game.player.moveInterval=.06f;game.player.moveClock=.045f;history.beginStep(game);game.player.moveInterval=.12f;game.player.moveClock=.75f*.12f+FixedStep;
        const auto before=head(history.samplePlayer(game.player,0,true)),after=head(history.samplePlayer(game.player,1,true));require(after.x>=before.x && after.x-before.x<=3,"A speed decrease ran the snake backward or teleported it");
    });
    test("stable enemy IDs survive reordering and colliding hash buckets",[&]{
        Game game(catalog,31);prepare(game);game.enemies={makeEnemy(1,800,.9f),makeEnemy(8193,1000,.9f)};render::MotionHistory history;history.beginStep(game);
        for(auto& enemy:game.enemies)move(enemy,.108f+FixedStep-.12f);std::swap(game.enemies[0],game.enemies[1]);
        for(const auto& enemy:game.enemies){const auto p=head(history.sampleEnemy(enemy,.5f,true));require(p.x==int(std::lround((enemy.position.x-8)*2.5f))-1,"Enemy inherited another vector slot's cached pose");}
        game.enemies.pop_back();history.beginStep(game);game.enemies.push_back(makeEnemy(1,1200,.25f));const auto newPose=history.sampleEnemy(game.enemies.back(),0,true);require(head(newPose).x==2985,"An absent generation reused a stale enemy history entry");
    });
    test("1800 live enemies follow constant velocity at 60 120 144 and 240 Hz",[&]{
        std::vector<Enemy> reference;
        for(int refresh:{60,120,144,240}){
            Game game(catalog,61);prepare(game);game.player.moveClock=-10000;game.player.position={1000,800};
            for(int i=0;i<1800;++i){auto enemy=makeEnemy(std::uint32_t(i+1),600+float(i%20)*8,float(i%17)/17);enemy.speed=40+float(i%4)*16;enemy.moveInterval=float(Tile)/enemy.speed;enemy.moveClock=float(i%17)/17*enemy.moveInterval;game.enemies.push_back(enemy);}
            render::MotionHistory history;double accumulator=0,time=0;int ticks=0;
            for(int frame=0;frame<refresh*2 && ticks<90;++frame){
                time+=1.0/refresh;accumulator+=1.0/refresh;
                while(accumulator>=double(FixedStep)){history.beginStep(game);game.update(FixedStep,{});accumulator-=FixedStep;++ticks;}
                if(ticks==0)continue;
                for(int tracked=0;tracked<32;++tracked){
                    const int i=(tracked*53)%1800;const auto& enemy=game.enemies[std::size_t(i)];
                    const auto point=head(history.sampleEnemy(enemy,float(accumulator/FixedStep),true));
                    const double origin=600+double(i%20)*8-8+double(float(i%17)/17)*8;
                    const double expected=(origin+double(enemy.speed)*(time-double(FixedStep)))*2.5;
                    require(std::abs(double(point.x)-expected)<1.01 && point.y==2000,"Dense enemy interpolation lost constant speed at "+std::to_string(refresh)+" Hz");
                }
            }
            require(ticks==90 && game.enemies.size()==1800,"Dense motion fixture stopped or lost enemies");
            if(reference.empty())reference=game.enemies;
            else for(std::size_t i=0;i<reference.size();++i){require(same(reference[i].position,game.enemies[i].position) && reference[i].moveClock==game.enemies[i].moveClock,"Render cadence changed enemy simulation/RNG");for(std::size_t j=0;j<8;++j)require(same(reference[i].trail[j],game.enemies[i].trail[j]),"Render cadence changed enemy body history");}
        }
    });
    test("live enemy turns and slow transitions remain continuous between updates",[&]{
        for(int refresh:{120,144,240}){
            Game game(catalog,67);prepare(game);game.player.moveClock=-10000;
            auto enemy=makeEnemy(11,760,.8f);enemy.speed=80;enemy.moveInterval=.1f;enemy.moveClock=.08f;game.enemies.push_back(enemy);
            render::MotionHistory history;double accumulator=0;auto previous=head(history.sampleEnemy(game.enemies[0],0,false));int ticks=0;
            for(int frame=0;frame<refresh*2;++frame){
                accumulator+=1.0/refresh;
                while(accumulator>=double(FixedStep)){
                    history.beginStep(game);
                    if(ticks==30){game.enemies[0].slow=.5f;game.enemies[0].slowFactor=.4f;}
                    if(ticks==75)game.enemies[0].speed=96;
                    game.update(FixedStep,{});accumulator-=FixedStep;++ticks;
                }
                const auto point=head(history.sampleEnemy(game.enemies[0],float(accumulator/FixedStep),true));
                const int travel=std::abs(point.x-previous.x)+std::abs(point.y-previous.y);
                require(travel<=int(std::ceil(96*2.5/refresh))+1,"An enemy turn or slow transition snapped at display cadence");previous=point;
            }
        }
    });
    test("mini IDs retain history through pool compaction and new spawns",[&]{
        Game game(catalog,37);prepare(game);game.miniSnakes={makeMini(1,800),makeMini(129,1000)};render::MotionHistory history;history.beginStep(game);
        auto retained=game.miniSnakes[1];retained.moveClock=.07f;game.miniSnakes={retained,makeMini(2,1200)};
        require(head(history.sampleMini(game.miniSnakes[0],0,true)).x==2495,"Mini pool compaction lost its stable pose");require(head(history.sampleMini(game.miniSnakes[1],0,true)).x==2995,"New mini inherited the actor previously occupying its pool slot");
    });
    test("lash knockback travels smoothly at every display cadence without changing combat",[&]{
        for(int refresh:{60,120,144,240}){
            Game game(catalog,83);prepare(game);game.player.moveClock=-10000;
            auto enemy=makeEnemy(19,824,.5f);enemy.speed=80;enemy.moveInterval=.1f;enemy.moveClock=.05f;
            enemy.hp=enemy.maxHp=100000;game.enemies.push_back(enemy);
            game.owned.push_back({catalog.find("tongue_lash"),1,false,0});
            render::MotionHistory history;double accumulator=0;auto previous=head(history.sampleEnemy(game.enemies[0],0,false));int ticks=0;
            for(int frame=0;frame<refresh/4;++frame){
                accumulator+=1.0/refresh;
                while(accumulator>=double(FixedStep)){
                    history.beginStep(game);game.update(FixedStep,{});accumulator-=FixedStep;++ticks;
                    if(ticks==1){require(game.enemies[0].position.x==848 && game.enemies[0].hp==99827,"Smoothing changed lash damage or grid knockback");}
                }
                const auto point=head(history.sampleEnemy(game.enemies[0],float(accumulator/FixedStep),true));
                const int travel=std::abs(point.x-previous.x)+std::abs(point.y-previous.y);
                require(travel<=int(std::ceil(320*2.5/refresh))+1,"A surviving lash victim teleported on hit");previous=point;
            }
            require(previous.x==2120 && previous.y==2000,"Knockback presentation did not reach its grid destination");
        }
    });
    test("contact and wall-clamped shoves preserve the visible starting pose",[&]{
        for(float x:{800.0f,ArenaSize-float(Tile)}){
            Game game(catalog,89);prepare(game);game.player.position={x,800};game.player.moveClock=-10000;game.player.invulnerable=0;
            auto enemy=makeEnemy(23,x,.5f);enemy.speed=80;enemy.moveInterval=.1f;enemy.moveClock=.05f;game.enemies.push_back(enemy);
            render::MotionHistory history;const auto before=head(history.sampleEnemy(game.enemies[0],1,false));history.beginStep(game);game.update(FixedStep,{});
            require(game.player.hp==80 && game.enemies[0].forceSerial==1,"Contact fixture did not exercise a shove");
            const auto start=head(history.sampleEnemy(game.enemies[0],0,true));
            require(start.x==before.x && start.y==before.y,"Contact or a blocked shove snapped an unfinished move");
            const auto frozen=history.sampleEnemy(game.enemies[0],.5f,true);
            for(int i=0;i<10;++i)require(head(history.sampleEnemy(game.enemies[0],.5f,true)).x==head(frozen).x,"Paused knockback drifted between samples");
            require(same(history.sampleEnemy(game.enemies[0],.5f,false).offset,{}),"Raw captures inherited cosmetic displacement");
            require(same(history.sampleEnemy(game.enemies[0],std::numeric_limits<float>::quiet_NaN(),true).offset,{}),"Invalid sample time retained a shove");
        }
    });
    test("repeated forces retain corrections across pool reorder and hash collisions",[&]{
        Game game(catalog,97);prepare(game);game.player.moveClock=-10000;
        for(auto id:{7u,8199u}){auto enemy=makeEnemy(id,824,.5f);enemy.speed=0;enemy.hp=enemy.maxHp=100000;game.enemies.push_back(enemy);}
        game.owned.push_back({catalog.find("tongue_lash"),1,false,0});render::MotionHistory history;
        history.beginStep(game);game.update(FixedStep,{});
        const auto endpoint=head(history.sampleEnemy(game.enemies[0],1,true));
        std::swap(game.enemies[0],game.enemies[1]);game.player.position={824,800};game.owned[0].cooldown=0;
        history.beginStep(game);game.update(FixedStep,{});
        for(const auto& enemy:game.enemies){
            require(enemy.position.x==872 && enemy.forceSerial==2,"Repeated-hit fixture missed its target");
            const auto point=head(history.sampleEnemy(enemy,0,true));
            require(point.x==endpoint.x && point.y==endpoint.y,"A repeated hit or pool reorder lost the visual correction");
        }
        for(int tick=0;tick<8;++tick){history.beginStep(game);game.update(FixedStep,{});}
        require(same(history.sampleEnemy(game.enemies[0],1,true).offset,{}),"A finished shove retained residual drift");
        game.player.position={848,800};game.owned[0].cooldown=0;history.beginStep(game);game.update(FixedStep,{});
        require(!same(history.sampleEnemy(game.enemies[0],1,true).offset,{}),"Recycling fixture had no active correction");
        game.player.position={80,80};history.beginStep(game);game.update(FixedStep,{});
        require(same(history.sampleEnemy(game.enemies[0],0,true).offset,{}),"Offscreen recycling carried a shove across the arena");
        history.clear();require(same(history.sampleEnemy(game.enemies[0],0,true).offset,{}),"Reset retained a prior run's shove");
    });
    test("an enemy dying during knockback retains its visible death position",[&]{
        Game game(catalog,101);prepare(game);game.player.moveClock=-10000;
        auto enemy=makeEnemy(29,824,.5f);enemy.speed=0;enemy.hp=enemy.maxHp=100000;game.enemies.push_back(enemy);
        game.owned.push_back({catalog.find("tongue_lash"),1,false,0});render::MotionHistory history;history.beginStep(game);game.update(FixedStep,{});
        auto previous=head(history.sampleEnemy(game.enemies[0],1,true));
        game.enemies[0].hp=1;game.owned.push_back({catalog.find("venom_nova"),8,false,0});game.owned.push_back({catalog.find("toxic_expanse"),8,false,0});history.beginStep(game);game.update(FixedStep,{});
        require(game.enemies.empty() && game.snakeDeaths.size()==1,"Death fixture did not compact the enemy");
        const auto start=head(history.sampleDeath(game.snakeDeaths[0],0,true));
        require(start.x==previous.x && start.y==previous.y,"Death animation jumped to the logical knockback destination");
        for(int tick=0;tick<10;++tick){
            const auto point=head(history.sampleDeath(game.snakeDeaths[0],1,true));
            require(point.x>=previous.x && point.x-previous.x<=10,"Death history snapped or inherited another actor's correction");previous=point;
            history.beginStep(game);game.update(FixedStep,{});
        }
        require(same(history.sampleDeath(game.snakeDeaths[0],1,true).offset,{}),"Death correction did not settle");
    });
    test("teleports missing history and clear sample the actual actor",[&]{
        Game game(catalog,41);prepare(game);game.enemies={makeEnemy(7,800,.5f)};render::MotionHistory history;history.beginStep(game);auto& enemy=game.enemies[0];enemy.position=enemy.previous={1400,1200};enemy.trail.fill(enemy.position);enemy.previousTrail=enemy.trail;enemy.moveClock=0;
        require(head(history.sampleEnemy(enemy,0,true)).x==3500,"Forced relocation interpolated through the arena");
        history.clear();game.player.moveClock=.06f;const auto expected=history.samplePlayer(game.player,.5f,false);const auto actual=history.samplePlayer(game.player,.1f,true);require(head(expected).x==head(actual).x,"Cleared history used stale player data");
    });
    test("collision holds and safe turns retain continuous player and camera motion",[&]{
        Game game(catalog,47);prepare(game);game.player.invulnerable=0;
        game.player.segments={{800,800},{800,808},{808,808},{808,800},{816,800},{816,792},{808,792}};
        game.player.previousSegments={{800,808},{808,808},{808,800},{816,800},{816,792},{808,792},{800,792}};
        game.player.direction=game.player.queued={0,-1};game.player.moveClock=.119f;
        render::MotionHistory history;history.beginStep(game);game.update(FixedStep,{{1,0}});
        int previousY=head(history.samplePlayer(game.player,0,true)).y;
        for(int frame=1;frame<=20;++frame){const auto point=head(history.samplePlayer(game.player,float(frame)/20,true));require(point.x==2000 && point.y<=previousY && previousY-point.y<=1,"Collision entry snapped the head or camera");previousY=point.y;}
        require(previousY==2000,"Collision did not finish at the last safe waypoint");
        for(int tick=0;tick<30;++tick){history.beginStep(game);game.update(FixedStep,{});for(float alpha:{0.0f,.5f,1.0f}){const auto point=head(history.samplePlayer(game.player,alpha,true));require(point.x==2000 && point.y==2000,"Blocked snake drifted between display frames");}}
        for(int tick=0;tick<15;++tick){history.beginStep(game);game.update(FixedStep,{{0,-1}});for(int frame=0;frame<=4;++frame){const auto point=head(history.samplePlayer(game.player,float(frame)/4,true));require(point.x==2000 && point.y<=previousY && previousY-point.y<=1,"Safe turn snapped or reversed the interpolated path");previousY=point.y;}}
        require(previousY<2000 && game.player.collisionClock<0,"Safe turn did not resume movement");
    });
    test("paused repeated samples and manual captures are deterministic and read only",[&]{
        Game game(catalog,43);prepare(game);game.player.moveClock=.06f;game.player.previousSegments=game.player.segments;for(auto& p:game.player.previousSegments)p.x-=8;render::MotionHistory history;history.beginStep(game);game.update(FixedStep,{});
        const auto frozen=history.samplePlayer(game.player,.7f,true);const auto original=game.player.segments;
        for(int i=0;i<20;++i)require(head(history.samplePlayer(game.player,.7f,true)).x==head(frozen).x,"A paused sample drifted without a simulation tick");
        const auto manual=history.samplePlayer(game.player,0,false);require(std::abs(manual.alpha-game.player.moveClock/game.player.moveInterval)<.00001f,"Manual capture changed its configured movement fraction");
        const auto invalid=history.samplePlayer(game.player,std::numeric_limits<float>::quiet_NaN(),true);require(invalid.alpha==manual.alpha,"Non-finite frame alpha did not use safe current pose");
        require(game.player.segments.size()==original.size(),"Presentation mutated snake length");for(std::size_t i=0;i<original.size();++i)require(same(original[i],game.player.segments[i]),"Presentation moved a simulation waypoint");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
