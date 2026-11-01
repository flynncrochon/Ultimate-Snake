#include "presentation.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using namespace snake;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool same(Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;}
struct Target {
    RenderTexture2D value=LoadRenderTexture(800,500);
    Target(){require(value.id!=0,"Native gravity framebuffer unavailable.");SetTextureFilter(value.texture,TEXTURE_FILTER_POINT);}
    ~Target(){UnloadRenderTexture(value);}
};
struct Capture {
    Image value;
    Capture(Presentation& app,const Target& target){
        BeginTextureMode(target.value);ClearBackground(BLACK);app.drawWorldOnly();EndTextureMode();
        value=LoadImageFromTexture(target.value.texture);require(value.data!=nullptr,"Native gravity readback failed.");ImageFlipVertical(&value);
    }
    ~Capture(){UnloadImage(value);}
    Color at(int x,int y)const{return GetImageColor(value,x,y);}
};
void fixture(Presentation& app,int count){
    app.startRun(0x610C);app.screen=Screen::Game;auto& game=app.game;
    game.enemies.clear();game.pickups.clear();game.owned={{game.catalog.find("graviton"),8,false,0}};
    game.spawnClock=-1000;game.bossClock=100;game.player.moveClock=-1000;game.player.invulnerable=0;
    game.greySnakes.enabled=false;game.graviton.turns=4;
    for(int i=0;i<count;++i){
        Enemy enemy;enemy.id=std::uint32_t(i+1);enemy.position=count==1?Vec{832,800}:Vec{800+float((i%12-6)*Tile),800+float((i/12%12-6)*Tile)};
        enemy.previous=enemy.position;enemy.speed=0;enemy.hp=enemy.maxHp=100000;enemy.trailSize=0;enemy.trail.fill(enemy.position);
        game.enemies.push_back(enemy);
    }
    if(count>1)game.player.invulnerable=1000;
    for(int tick=0;tick<13;++tick)game.update(FixedStep,{});
    game.player.invulnerable=0;app.setRenderFrame(1,false);
}
}
int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(800,500,"Gridlock native raster checks");SetTargetFPS(0);
    if(!IsWindowReady())return 1;
    {
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);Target target;
        test("grid and target brackets preserve exact opaque native pixels",[&]{
            Presentation app(catalog);fixture(app,1);Capture capture(app,target);
            require(same(capture.at(471,241),{108,211,255,255}),"Pinned target lost its native bracket pixel.");
            require(same(capture.at(280,130),{179,235,255,255}),"The rooted field lost its corner marker.");
            require(same(capture.at(380,150),{10,34,53,255}),"The field lattice left its tile grid.");
            require(same(capture.at(480,250),{204,0,0,255}),"Gridlock obscured the original enemy body color.");
        });
        test("effect settings hide the art without touching the live pin",[&]{
            Presentation app(catalog);fixture(app,1);
            const auto lock=app.game.graviton;const auto enemy=app.game.enemies.front();const auto elapsed=app.game.elapsed;
            app.settings.render.weaponEffects=false;Capture capture(app,target);
            require(same(capture.at(471,241),BLACK)&&same(capture.at(280,130),BLACK)&&same(capture.at(380,150),BLACK),"Disabled effects left gravity artwork behind.");
            require(same(capture.at(480,250),{204,0,0,255}),"Disabling gravity effects hid an enemy.");
            require(app.game.elapsed==elapsed&&app.game.graviton.age==lock.age&&app.game.graviton.cooldown==lock.cooldown&&app.game.enemies[0].gravityHold==enemy.gravityHold,"Rendering settings changed gravity simulation.");
        });
        test("maximum pinned swarm retains its final actors and brackets",[&]{
            Presentation app(catalog);fixture(app,1800);Capture capture(app,target);
            const auto& last=app.game.enemies.back();const int x=400+int((last.position.x-800)*2.5f),y=250+int((last.position.y-800)*2.5f);
            require(last.gravityHold>0&&same(capture.at(x-9,y-9),{108,211,255,255}),"Dense gravity batching dropped the last target marker.");
            require(same(capture.at(x,y),{204,0,0,255}),"Dense gravity batching hid the last enemy.");
        });
        test("Singularity inheritance retains the rooted grid and pin markers",[&]{
            Presentation app(catalog);fixture(app,1);
            app.game.owned.push_back({catalog.find("plague_mortar"),8,true,1000});
            app.game.graviton.turns=4;app.game.graviton.cooldown=0;
            for(int tick=0;tick<13;++tick)app.game.update(FixedStep,{});
            Capture capture(app,target);const auto& enemy=app.game.enemies.front();
            const int x=400+int((enemy.position.x-800)*2.5f),y=250+int((enemy.position.y-800)*2.5f);
            require(app.game.graviton.empowered&&same(capture.at(280,130),{179,235,255,255}),"Evolution lost the rooted field's original corner marker.");
            require(enemy.gravityHold>0&&same(capture.at(x-9,y-9),{108,211,255,255}),"Evolution replaced the base pin instead of extending it.");
        });
    }
    CloseWindow();std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
