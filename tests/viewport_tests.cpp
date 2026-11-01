#include "presentation.hpp"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace snake;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Target {
    RenderTexture2D value{};
    Target(int width,int height):value(LoadRenderTexture(width,height)){require(value.id!=0,"Viewport framebuffer unavailable");SetTextureFilter(value.texture,TEXTURE_FILTER_POINT);}
    ~Target(){UnloadRenderTexture(value);}
};
struct Capture {
    Image value{};
    explicit Capture(RenderTexture2D target):value(LoadImageFromTexture(target.texture)){require(value.data!=nullptr,"Viewport readback unavailable");ImageFlipVertical(&value);}
    ~Capture(){UnloadImage(value);}
    Color at(int x,int y)const{return GetImageColor(value,x,y);}
    void save(const char* name)const{
        const auto path=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/name;
        std::filesystem::create_directories(path.parent_path());require(ExportImage(value,path.string().c_str()),"Viewport capture export failed");
    }
};
bool same(Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;}
void fixture(Presentation& app){
    app.startRun(0x5151);app.screen=Screen::Game;
    auto& g=app.game;g.enemies.clear();g.projectiles.clear();g.zones.clear();g.pickups.clear();g.owned.clear();
    g.player.invulnerable=0;g.player.moveClock=-1000;g.spawnClock=-1000;g.bossClock=100;g.greySnakes.enabled=false;
    for(const Vec delta:std::vector<Vec>{{-200,0},{200,0},{0,-120},{0,120}}){
        Enemy e;e.id=std::uint32_t(g.enemies.size()+100);e.position={g.player.position.x+delta.x,g.player.position.y+delta.y};
        e.previous=e.position;e.trailSize=0;e.hp=e.maxHp=100;e.speed=0;g.enemies.push_back(e);
    }
    app.setRenderFrame(.5f,false);
}
void draw(Presentation& app,Target& target,bool world=true,int scale=1){
    BeginTextureMode(target.value);ClearBackground(BLACK);
    BeginMode2D({{0,0},{0,0},0,float(scale)});
    if(world)app.drawWorldOnly();else app.draw();
    EndMode2D();EndTextureMode();
}
}
int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& fn){try{fn();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(800,500,"Expanded viewport regression");SetTargetFPS(0);
    if(!IsWindowReady())return 1;
    {
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
        Target base(800,500),wide(1200,700),zoomed(2400,1400);
        test("extra viewport reveals actors on all sides without changing existing pixels",[&]{
            Presentation app(catalog);fixture(app);const auto elapsed=app.game.elapsed;const auto position=app.game.player.position;
            draw(app,base);Capture original(base.value);
            app.setViewport(1200,700);draw(app,wide);Capture expanded(wide.value);expanded.save("viewport-expanded.png");
            for(int y=0;y<500;++y)for(int x=0;x<800;++x)require(same(original.at(x,y),expanded.at(x+200,y+100)),"Existing arena pixels changed when revealing more world");
            for(const auto point:std::vector<Vec>{{100,350},{1100,350},{600,50},{600,650}}){
                int red=0;for(int y=int(point.y)-15;y<=int(point.y)+15;++y)for(int x=int(point.x)-15;x<=int(point.x)+15;++x){const auto c=expanded.at(x,y);red+=c.r>100&&c.g<30&&c.b<30;}
                require(red>20,"Actor outside the old viewport was not revealed");
            }
            require(app.game.elapsed==elapsed&&app.game.player.position.x==position.x&&app.game.player.position.y==position.y&&app.game.enemies.size()==4,"Viewport rendering advanced gameplay");
        });
        test("direct integer zoom preserves every source pixel and scissor edge",[&]{
            Presentation app(catalog);fixture(app);app.setViewport(1200,700);draw(app,wide);Capture original(wide.value);
            app.setViewport(1200,700,2);draw(app,zoomed,true,2);Capture enlarged(zoomed.value);enlarged.save("viewport-integer-zoom.png");
            for(int y=0;y<1400;++y)for(int x=0;x<2400;++x)require(same(original.at(x/2,y/2),enlarged.at(x,y)),"Integer zoom stretched, blurred, or clipped source pixels");
        });
        test("expanded menu settings and centered controls route clicks consistently",[&]{
            Presentation app(catalog);app.setViewport(1200,700);
            app.update(FixedStep,{1130,677},true);draw(app,wide,false);require(app.settingsOpen,"Expanded menu settings button missed its new edge");
            app.update(FixedStep,{675,190},true);require(app.settingsTabIndex()==2,"Centered settings tab click missed its displayed position");
            draw(app,wide,false);Capture image(wide.value);image.save("viewport-settings.png");
            app.update(FixedStep,{600,547},true);require(!app.settingsOpen,"Centered settings Cancel button missed its displayed position");
        });
        test("evolution guide stays centered sharp and clickable at integer zoom",[&]{
            Presentation app(catalog);fixture(app);app.codex=true;app.setViewport(1200,700);
            app.game.owned={{catalog.find("snake_nest"),3,false,0},{catalog.find("toxic_expanse"),1,false,0}};
            draw(app,wide,false);Capture original(wide.value);original.save("viewport-evolutions.png");
            app.setViewport(1200,700,2);draw(app,zoomed,false,2);Capture enlarged(zoomed.value);
            for(int y=0;y<1400;++y)for(int x=0;x<2400;++x)
                require(same(original.at(x/2,y/2),enlarged.at(x,y)),"Evolution guide blurred or changed a source pixel at integer zoom");
            app.update(FixedStep,{760,78},true);require(app.codex,"Expanded guide retained the old close-button hitbox");
            app.update(FixedStep,{960,178},true);require(!app.codex,"Expanded guide close button missed its displayed position");
        });
        test("expanded upgrade cards share hover click and keyboard geometry",[&]{
            Presentation app(catalog);fixture(app);app.setViewport(1200,700);app.game.grantXp(app.game.player.nextXp);
            require(app.game.phase==Phase::LevelUp,"Upgrade fixture did not open");
            app.update(FixedStep,{600,340},true);draw(app,wide,false);
            require(app.game.phase==Phase::Playing,"Centered upgrade card click did not choose an offer");
        });
        test("HUD and character-card clipping retain whole source pixels at integer zoom",[&]{
            Presentation app(catalog);fixture(app);app.setViewport(1200,700);draw(app,wide,false);Capture original(wide.value);
            app.setViewport(1200,700,2);draw(app,zoomed,false,2);Capture enlarged(zoomed.value);
            original.save("viewport-hud.png");enlarged.save("viewport-hud-zoom.png");
            for(int y=0;y<1400;++y)for(int x=0;x<2400;++x)if(!same(original.at(x/2,y/2),enlarged.at(x,y)))throw std::runtime_error("HUD source pixel changed at "+std::to_string(x)+","+std::to_string(y));
            app.screen=Screen::Menu;app.setViewport(1200,700);draw(app,wide,false);Capture menu(wide.value);
            app.setViewport(1200,700,2);draw(app,zoomed,false,2);Capture largeMenu(zoomed.value);
            menu.save("viewport-menu.png");largeMenu.save("viewport-menu-zoom.png");
            for(int y=0;y<1400;++y)for(int x=0;x<2400;++x)if(!same(menu.at(x/2,y/2),largeMenu.at(x,y)))throw std::runtime_error("Menu source pixel changed at "+std::to_string(x)+","+std::to_string(y));
        });
        test("expanded chest reel clipping and centered settings retain source pixels",[&]{
            Presentation app(catalog);fixture(app);
            app.game.chestRewards.push_back({1,ChestRewardKind::Upgrade,catalog.find("venom_shot"),2,0,0,"Venom Shot Lv 2","A stronger venom shot."});
            auto chest=[&](Target& target,int scale){
                app.setViewport(1200,700,scale);BeginTextureMode(target.value);ClearBackground(BLACK);
                BeginMode2D({{0,0},{0,0},0,float(scale)});app.draw(3.0f);EndMode2D();EndTextureMode();
            };
            chest(wide,1);Capture original(wide.value);chest(zoomed,2);Capture enlarged(zoomed.value);
            for(int y=0;y<1400;++y)for(int x=0;x<2400;++x)require(same(original.at(x/2,y/2),enlarged.at(x,y)),"Chest reel clipping changed source pixels");
            app.screen=Screen::Menu;app.openSettings();app.setViewport(1200,700);draw(app,wide,false);Capture settings(wide.value);
            app.setViewport(1200,700,2);draw(app,zoomed,false,2);Capture largeSettings(zoomed.value);
            for(int y=0;y<1400;++y)for(int x=0;x<2400;++x)require(same(settings.at(x/2,y/2),largeSettings.at(x,y)),"Centered settings changed source pixels");
        });
    }
    CloseWindow();std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
