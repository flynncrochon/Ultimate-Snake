#include "presentation.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
void fixture(snake::Game& game,int deaths){
    game.reset(0,123);game.owned.clear();game.enemies.clear();game.pickups.clear();
    for(unsigned i=0;i<1800;++i){
        snake::Enemy enemy;enemy.id=10000+i;enemy.position=enemy.previous={672+float(i%36)*8,720+float(i/36%22)*8};
        enemy.trailSize=8;enemy.trail.fill(enemy.position);enemy.previousTrail=enemy.trail;
        game.enemies.push_back(enemy);
    }
    for(int i=0;i<1600;++i){snake::Projectile shot;shot.position={640+float(i%40)*8,704+float(i/40%24)*8};shot.velocity={90,50};game.projectiles.push_back(shot);}
    for(int i=0;i<64;++i){snake::Zone zone;zone.position={688+float(i%8)*28,728+float(i/8)*18};zone.radius=36;zone.pattern=i%3?snake::Pattern::Mortar:snake::Pattern::Nova;zone.life=zone.maxLife=3;game.zones.push_back(zone);}
    for(int i=0;i<60;++i){snake::MiniSnake mini;mini.id=unsigned(5000+i);mini.position=mini.previous={720+float(i%10)*16,752+float(i/10)*16};mini.segments.fill(mini.position);mini.previousSegments=mini.segments;game.miniSnakes.push_back(mini);}
    for(int i=0;i<deaths;++i){
        snake::SnakeDeath death;death.id=unsigned(30000+i);death.kind=snake::SnakeDeathKind(i%6);death.count=9;
        const float x=672+float(i%36)*8,y=720+float(i/36%22)*8;
        for(std::size_t j=0;j<death.count;++j)death.current[j]={x-float(std::min<std::size_t>(j,4))*8,y+float(j>4?j-4:0)*8};
        death.previous=death.current;game.snakeDeaths.push_back(death);
    }
}
void splitFixture(snake::Game& game,int parents){
    fixture(game,0);game.enemies.clear();game.player.moveClock=-1000;game.player.invulnerable=1000;game.spawnClock=-1000;game.bossClock=1000;
    for(int i=0;i<parents;++i){
        snake::Enemy parent;parent.id=unsigned(40000+i);parent.kind=snake::EnemyKind::Splitter;parent.alive=false;parent.trailSize=8;parent.direction={1,0};
        const float x=672+float(i%36)*8,y=720+float(i/36%22)*8;parent.position=parent.previous={x,y};
        for(std::size_t j=0;j<parent.trail.size();++j)parent.trail[j]={x-float(std::min<std::size_t>(j+1,4))*8,y+float(j+1>4?j-3:0)*8};
        parent.previousTrail=parent.trail;game.enemies.push_back(parent);
    }
    game.update(snake::FixedStep,{});
}
}
int main(int argc,char** argv){
    try{
        const int frames=argc>1?std::clamp(std::stoi(argv[1]),60,10000):1200;
        const bool splitting=argc>2 && std::string(argv[2])=="--split";
        const auto beside=std::filesystem::absolute(argv[0]).parent_path()/"data";
        const auto catalog=snake::Catalog::load(beside.string());
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(800,500,"Death wave timing");SetTargetFPS(0);
        if(!IsWindowReady())throw std::runtime_error("OpenGL context unavailable");
        const auto target=LoadRenderTexture(800,500);SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
        {
            const auto start=Clock::now();snake::Presentation app(catalog);app.screen=snake::Screen::Game;
            const double prepare=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
            // The first measured submission uses the death shader; no effect is
            // exercised by a preceding benchmark scenario.
            const std::array<int,4> populations=splitting?std::array<int,4>{1,0,128,900}:std::array<int,4>{1,0,512,int(snake::SnakeDeathCapacity)};
            for(int deaths:populations){
                if(splitting)splitFixture(app.game,deaths);else fixture(app.game,deaths);
                std::vector<double> times;times.reserve(std::size_t(frames));
                double first=0,firstMorph=0,warmupWorst=0;
                for(int frame=-60;frame<frames;++frame){
                    const float phase=float((frame+60)%120)/120;
                    for(auto& death:app.game.snakeDeaths)death.age=phase*snake::snakeDeathDuration(death.kind);
                    if(splitting)for(auto& enemy:app.game.enemies)enemy.splitBirth.age=phase*.42f;
                    const auto submitted=Clock::now();
                    app.setRenderFrame(1,false);BeginTextureMode(target);ClearBackground(BLACK);app.draw();EndTextureMode();
                    BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,800,-500},{0,0},WHITE);EndDrawing();
                    const double ms=std::chrono::duration<double,std::milli>(Clock::now()-submitted).count();
                    if(frame==-60)first=ms;
                    if(splitting && deaths>0 && firstMorph==0 && phase*.42f>=snake::SplitBirthFlash)firstMorph=ms;
                    if(frame<0)warmupWorst=std::max(warmupWorst,ms);else times.push_back(ms);
                }
                const double mean=std::accumulate(times.begin(),times.end(),0.0)/times.size();std::sort(times.begin(),times.end());
                std::cout<<std::fixed<<std::setprecision(4)<<"{\"deaths\":"<<deaths<<",\"split_wave\":"<<(splitting?"true":"false")<<",\"frames\":"<<frames<<",\"enemies\":"<<app.game.enemies.size()<<",\"projectiles\":"<<app.game.projectiles.size()<<",\"zones\":"<<app.game.zones.size()<<",\"summons\":"<<app.game.miniSnakes.size()<<",\"prepare_ms\":"<<prepare
                    <<",\"first_ms\":"<<first<<",\"first_morph_ms\":"<<firstMorph<<",\"warmup_worst_ms\":"<<warmupWorst<<",\"mean_ms\":"<<mean<<",\"p99_ms\":"<<times[std::min(times.size()-1,std::size_t(times.size()*.99))]<<",\"worst_ms\":"<<times.back()<<"}\n";
            }
        }
        UnloadRenderTexture(target);CloseWindow();return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
