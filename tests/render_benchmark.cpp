#include "presentation.hpp"
#include "chest_lottery.hpp"
#include "grid.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#define NOUSER
#define NOGDI
#include <windows.h>
#endif

namespace {
using Clock=std::chrono::steady_clock;
double threadCpuMs(){
#ifdef _WIN32
    FILETIME created,exited,kernel,user;
    if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)){
        const auto ticks=[](FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
        return double(ticks(kernel)+ticks(user))*.0001;
    }
#endif
    return 0;
}
void populate(snake::Game& game,bool dense) {
    game.reset(0,123);game.enemies.clear();game.projectiles.clear();game.zones.clear();game.pickups.clear();
    game.nestEggs.clear();game.miniSnakes.clear();game.damageNumbers.clear();
    const int count=dense?1800:300;
    for(int i=0;i<count;++i){
        snake::Enemy enemy;enemy.id=std::uint32_t(10000+i);enemy.kind=i%13==0?snake::EnemyKind::Splitter:snake::EnemyKind::Red;
        const int x=(i%36)-18,y=((i/36)%22)-11;
        enemy.position={game.player.position.x+float(x*snake::Tile),game.player.position.y+float(y*snake::Tile)};
        enemy.previous=enemy.position;enemy.direction={1,0};enemy.velocity={20,0};enemy.motionReady=true;
        for(int j=0;j<enemy.trailSize;++j)enemy.trail[std::size_t(j)]={enemy.position.x-float((j+1)*snake::Tile),enemy.position.y};
        enemy.previousTrail=enemy.trail;game.enemies.push_back(enemy);
    }
    for(int i=0;i<(dense?64:12);++i){
        snake::Zone zone;zone.position={game.player.position.x+float((i%8-4)*28),game.player.position.y+float((i/8-4)*18)};
        zone.radius=dense?36:22;zone.life=zone.maxLife=3;zone.pattern=i%3==0?snake::Pattern::Nova:snake::Pattern::Mortar;zone.evolved=i%7==0;
        game.zones.push_back(zone);
    }
    for(int i=0;i<(dense?1600:200);++i){
        snake::Projectile shot;shot.position={game.player.position.x+float((i%40-20)*8),game.player.position.y+float((i/40%24-12)*8)};
        shot.velocity={90,50};shot.life=1;game.projectiles.push_back(shot);
    }
    for(int i=0;i<(dense?60:12);++i){
        snake::MiniSnake mini;mini.id=std::uint32_t(5000+i);mini.position={game.player.position.x+float((i%10-5)*16),game.player.position.y+float((i/10-3)*16)};
        mini.previous=mini.position;
        for(int j=0;j<3;++j)mini.segments[std::size_t(j)]={mini.position.x-float(j*snake::Tile),mini.position.y};
        mini.previousSegments=mini.segments;game.miniSnakes.push_back(mini);
    }
}
double mean(const std::vector<double>& v){return std::accumulate(v.begin(),v.end(),0.0)/double(v.size());}
void populateReckoning(snake::Game& game,int scenario) {
    populate(game,scenario!=0);game.zones.clear();game.grapples.clear();
    const int tongues=scenario==0?1:16;
    const int bites=scenario==0?1:scenario==1?16:128;
    for(int i=0;i<tongues;++i){
        auto& enemy=game.enemies[std::size_t(i)];
        const float angle=float(i)*6.2831853f/float(tongues);
        enemy.position={game.player.position.x+std::cos(angle)*100,game.player.position.y+std::sin(angle)*72};
        enemy.previous=enemy.position;enemy.grabbed=true;
        for(int j=0;j<enemy.trailSize;++j)enemy.trail[std::size_t(j)]={enemy.position.x-float((j+1)*snake::Tile),enemy.position.y};
        enemy.previousTrail=enemy.trail;
        snake::Grapple tongue;tongue.target=enemy.id;tongue.phase=1;tongue.position=enemy.position;
        tongue.direction={std::cos(angle),std::sin(angle)};game.grapples.push_back(tongue);
    }
    for(int i=0;i<bites;++i){
        snake::Zone bite;bite.position={game.player.position.x+float((i%8-4)*12),game.player.position.y+float((i/8-4)*8)};
        if(scenario==0)bite.position=game.player.position;
        bite.pattern=snake::Pattern::Lash;bite.evolved=true;bite.radius=24;bite.life=bite.maxLife=.55f;
        bite.phase=float(i)*.78539816f;game.zones.push_back(bite);
    }
}
void populateSwarm(snake::Game& game,int count,bool combat) {
    populate(game,true);game.enemies.clear();game.projectiles.clear();game.zones.clear();game.miniSnakes.clear();game.owned.clear();
    game.player.moveClock=-10000;game.player.invulnerable=10000;game.spawnClock=-10000;game.bossClock=100;
    game.enemies.reserve(std::size_t(count));
    for(int i=0;i<count;++i){
        snake::Enemy enemy;enemy.id=std::uint32_t(10000+i);enemy.kind=i%13==0?snake::EnemyKind::Splitter:snake::EnemyKind::Red;
        enemy.position={800+float((i%36-18)*snake::Tile),800+float(((i/36)%22-11)*snake::Tile)};
        enemy.previous={enemy.position.x-snake::Tile,enemy.position.y};enemy.direction={1,0};enemy.motionReady=true;
        enemy.trailSize=8;enemy.speed=float(24+i%5*16);enemy.moveInterval=float(snake::Tile)/enemy.speed;
        enemy.moveClock=enemy.moveInterval*float(i%17)/17;enemy.hp=enemy.maxHp=1e12;
        for(int j=0;j<enemy.trailSize;++j){enemy.trail[std::size_t(j)]={enemy.previous.x-float(j*snake::Tile),enemy.position.y};enemy.previousTrail[std::size_t(j)]={enemy.trail[std::size_t(j)].x-snake::Tile,enemy.position.y};}
        game.enemies.push_back(enemy);
    }
    if(combat){
        for(const char* id:{"viper_fangs","snake_nest","venom_nova","plague_mortar"})game.owned.push_back({game.catalog.find(id),8,true,0});
        for(const char* id:{"rapid_fire","coiled_volley","toxic_expanse","chronofield"})game.owned.push_back({game.catalog.find(id),std::strcmp(id,"coiled_volley")==0?2:8,false,0});
    }
}
std::uint64_t enemyChecksum(const snake::Game& game){
    std::uint64_t result=14695981039346656037ull;
    const auto add=[&](float value){std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));result=(result^bits)*1099511628211ull;};
    for(const auto& enemy:game.enemies){result=(result^enemy.id)*1099511628211ull;add(enemy.position.x);add(enemy.position.y);add(enemy.moveClock);add(enemy.moveInterval);for(const auto p:enemy.trail){add(p.x);add(p.y);}}
    return result;
}
void populateGreyBarriers(snake::Game& game){
    auto& system=game.greySnakes;system.reset();system.enabled=false;system.clock=1175;
    // Maximum run population, with repeated own-wall sweeps. Every trail is
    // visible, keeps a cell gap from its neighbors, and uses full-quality art.
    for(std::size_t i=0;i<snake::GreyTrailCapacity;++i){
        system.trails.emplace_back();auto& trail=system.trails.back();trail.id=std::uint8_t(i+1);
        trail.alive=i+1==snake::GreyTrailCapacity;trail.age=trail.alive?5:10;
        const int count=trail.alive?84:167;
        for(int j=0;j<count;++j){
            const int sweep=i==6?14:30,phase=j%(sweep*2);
            // Leave the player's current cell/body clear: grey contact is
            // lethal even in the benchmark's otherwise immune moving fixture.
            system.append(trail,{(i==6?824:680)+float((phase<=sweep?phase:sweep*2-phase)*snake::Tile),704+float(i*16)});
        }
        trail.previous=trail.path[trail.count-2];trail.direction={trail.path[trail.count-1].x>trail.previous.x?1.0f:-1.0f,0};
        trail.moveClock=.03;trail.target={920,trail.path[trail.count-1].y};trail.wallDirection={0,-1};
    }
}
}

int main(int argc,char** argv){
    try{
        int frames=240;if(argc>1)frames=std::clamp(std::stoi(argv[1]),30,10000);
        bool diagnose=false,shield=false,grey=false,reckoning=false;
        for(int i=2;i<argc;++i){
            if(std::string(argv[i])=="--diagnose")diagnose=true;
            else if(std::string(argv[i])=="--shield")shield=true;
            else if(std::string(argv[i])=="--grey")grey=true;
            else if(std::string(argv[i])=="--reckoning")reckoning=true;
            else throw std::runtime_error("Unknown render benchmark option.");
        }
        const std::filesystem::path beside=std::filesystem::absolute(argv[0]).parent_path()/"data";
        const auto catalog=snake::Catalog::load(std::filesystem::exists(beside/"upgrades.csv")?beside.string():"data");
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(800,500,"Snake render benchmark");SetTargetFPS(0);
        if(!IsWindowReady())throw std::runtime_error("OpenGL window is unavailable.");
        RenderTexture2D target=LoadRenderTexture(800,500);SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
        {
            const auto preparing=Clock::now();
            snake::Presentation app(catalog);app.screen=snake::Screen::Game;
            const double preparationMs=std::chrono::duration<double,std::milli>(Clock::now()-preparing).count();
            for(int scenario=0;scenario<(reckoning?3:6);++scenario){
                const bool dense=scenario!=0,lottery=!reckoning && scenario==2;
                const bool moving=!reckoning && (scenario==3 || scenario==4),renderOnly=scenario==5;
                if(reckoning)populateReckoning(app.game,scenario);
                else if(scenario>=3)populateSwarm(app.game,renderOnly?10000:1800,scenario==4);else populate(app.game,dense);
                if(grey)populateGreyBarriers(app.game);
                if(shield){
                    // Exercise a maximum-length shield with many visible turns.
                    auto& player=app.game.player;player.segments={player.position};player.length=120;
                    constexpr snake::Vec directions[]{{-1,0},{0,1},{1,0},{0,-1}};
                    for(int leg=0;player.segments.size()<120;++leg)for(int step=0;step<1+leg/2 && player.segments.size()<120;++step){
                        const auto p=player.segments.back(),d=directions[leg%4];
                        player.segments.push_back({p.x+d.x*snake::Tile,p.y+d.y*snake::Tile});
                    }
                    player.previousSegments=player.segments;
                    app.game.owned.push_back({catalog.find("serpents_scales"),8,false,0});app.game.shieldCharges=3;
                }
                std::vector<double> cpu,total,updates;cpu.reserve(std::size_t(frames));total.reserve(std::size_t(frames));updates.reserve(std::size_t(frames/4+1));
                if(lottery)app.game.chestRewards.push_back({1,snake::ChestRewardKind::Upgrade,catalog.find("venom_shot"),2,0,0,"Venom Shot Lv 2","A stronger venom shot."});
                double firstCpu=0,firstTotal=0,warmupCpuWorst=0,warmupTotalWorst=0;
                std::size_t peakProjectiles=0,peakZones=0,peakSummons=0;
                struct SlowFrame {int frame;double renderMs,totalMs,threadMs;};
                std::vector<SlowFrame> slowFrames;slowFrames.reserve(64);int slowFrameCount=0;
                for(int frame=-60;frame<frames;++frame){
                    const double threadStart=diagnose?threadCpuMs():0;
                    const auto start=Clock::now();
                    if(reckoning){
                        app.game.elapsed=float(frame+60)/240;
                        for(std::size_t i=0;i<app.game.zones.size();++i){auto& bite=app.game.zones[i];bite.life=bite.maxLife-std::fmod(app.game.elapsed+float(i)*.037f,bite.maxLife);}
                    }
                    if(!renderOnly && (frame+60)%4==0){app.beginSimulationStep();if(moving)app.game.update(snake::FixedStep,{});}
                    const auto updated=Clock::now();
                    if(moving && frame>=0 && (frame+60)%4==0)updates.push_back(std::chrono::duration<double,std::milli>(updated-start).count());
                    app.setRenderFrame(float((frame+60)%4)/4,!renderOnly);
                    const int loopFrames=int(std::ceil((snake::loot::RevealSeconds+.5f)*60));
                    const float revealTime=lottery?float((frame+60)%loopFrames)/60.0f:-1;
                    BeginTextureMode(target);ClearBackground(BLACK);app.draw(revealTime);EndTextureMode();
                    const auto submitted=Clock::now();
                    BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,800,-500},{0,0},WHITE);EndDrawing();
                    const auto end=Clock::now();
                    peakProjectiles=std::max(peakProjectiles,app.game.projectiles.size());peakZones=std::max(peakZones,app.game.zones.size());peakSummons=std::max(peakSummons,app.game.miniSnakes.size());
                    if(moving && (app.game.phase!=snake::Phase::Playing || app.game.enemies.size()!=1800))throw std::runtime_error("Moving benchmark stopped or lost its enemy population.");
                    const double cpuMs=std::chrono::duration<double,std::milli>(submitted-(moving?updated:start)).count();
                    const double totalMs=std::chrono::duration<double,std::milli>(end-start).count();
                    const double threadMs=diagnose?threadCpuMs()-threadStart:0;
                    if(totalMs>1000.0/60){++slowFrameCount;if(slowFrames.size()<64)slowFrames.push_back({frame,cpuMs,totalMs,threadMs});}
                    if(frame==-60){firstCpu=cpuMs;firstTotal=totalMs;}
                    if(frame<0){warmupCpuWorst=std::max(warmupCpuWorst,cpuMs);warmupTotalWorst=std::max(warmupTotalWorst,totalMs);}
                    else {cpu.push_back(cpuMs);total.push_back(totalMs);}
                }
                const double cpuMean=mean(cpu),totalMean=mean(total);std::sort(cpu.begin(),cpu.end());std::sort(total.begin(),total.end());
                const auto p99=std::min(total.size()-1,std::size_t(double(total.size())*.99));
                const double updateMean=updates.empty()?0:mean(updates);std::sort(updates.begin(),updates.end());
                const char* name=reckoning?(scenario==0?"reckoning_sparse":scenario==1?"reckoning_dense":"reckoning_capacity"):scenario==5?"render_only_10000":scenario==4?"moving_combat_1800":scenario==3?"moving_swarm_1800":lottery?"dense_lottery":dense?"dense":"representative";
                std::cout<<std::fixed<<std::setprecision(4)<<"{\"scenario\":\""<<name<<"\",\"frames\":"<<frames
                    <<",\"shield\":"<<(shield?"true":"false")
                    <<",\"grey_barriers\":"<<app.game.greySnakes.barrierCount<<",\"grey_trails\":"<<app.game.greySnakes.trails.size()
                    <<",\"enemies\":"<<app.game.enemies.size()<<",\"projectiles\":"<<app.game.projectiles.size()<<",\"zones\":"<<app.game.zones.size()
                    <<",\"grapples\":"<<app.game.grapples.size()
                    <<",\"peak_projectiles\":"<<peakProjectiles<<",\"peak_zones\":"<<peakZones<<",\"peak_summons\":"<<peakSummons
                    <<",\"presentation_prepare_ms\":"<<preparationMs
                    <<",\"render_cpu_first_ms\":"<<firstCpu<<",\"frame_first_ms\":"<<firstTotal
                    <<",\"render_cpu_warmup_worst_ms\":"<<warmupCpuWorst<<",\"frame_warmup_worst_ms\":"<<warmupTotalWorst
                    <<",\"render_cpu_mean_ms\":"<<cpuMean<<",\"render_cpu_p99_ms\":"<<cpu[p99]<<",\"render_cpu_worst_ms\":"<<cpu.back()
                    <<",\"frame_mean_ms\":"<<totalMean<<",\"frame_p99_ms\":"<<total[p99]<<",\"frame_worst_ms\":"<<total.back()
                    <<",\"simulation_hz\":"<<(moving?60:0)<<",\"virtual_render_hz\":240,\"simulation_ticks\":"<<updates.size()<<",\"simulation_mean_ms\":"<<updateMean
                    <<",\"simulation_p99_ms\":"<<(updates.empty()?0:updates[std::min(updates.size()-1,std::size_t(double(updates.size())*.99))])
                    <<",\"simulation_worst_ms\":"<<(updates.empty()?0:updates.back())<<",\"enemy_checksum\":"<<enemyChecksum(app.game)
                    <<",\"thread_cpu_diagnostics\":"<<(diagnose?"true":"false")<<",\"frames_over_60hz_budget\":"<<slowFrameCount<<",\"slow_frames\":[";
                for(std::size_t i=0;i<slowFrames.size();++i){const auto& sample=slowFrames[i];if(i)std::cout<<',';std::cout<<"{\"frame\":"<<sample.frame<<",\"render_ms\":"<<sample.renderMs<<",\"total_ms\":"<<sample.totalMs<<",\"thread_cpu_ms\":"<<sample.threadMs<<'}';}
                std::cout<<"]}\n";
            }
        }
        UnloadRenderTexture(target);CloseWindow();return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
