#include "presentation.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef SNAKE_TEST_DATA_DIR
#define SNAKE_TEST_DATA_DIR "data"
#endif
#ifndef SNAKE_TEST_ARTIFACT_DIR
#define SNAKE_TEST_ARTIFACT_DIR "artifacts/gametest"
#endif

namespace {
using namespace snake;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void prepare(Presentation& app){
    app.startRun(0xB055u);app.screen=Screen::Game;auto& game=app.game;
    game.enemies.clear();game.projectiles.clear();game.zones.clear();game.pickups.clear();
    game.nestEggs.clear();game.miniSnakes.clear();game.beams.clear();game.cobraPits.clear();
    game.cobras.clear();game.grapples.clear();game.particles.clear();game.damageNumbers.clear();
    game.owned.clear();game.spawnClock=-1000;game.bossClock=10;
    game.player.invulnerable=1000;game.player.moveClock=-1000;
    app.setRenderFrame(1,false);
}
std::vector<Color> capture(Presentation& app,RenderTexture2D target,const char* name,bool worldOnly=false){
    BeginTextureMode(target);ClearBackground(BLACK);if(worldOnly)app.drawWorldOnly();else app.draw();EndTextureMode();
    // Exercise the same complete frame and presentation path as the game loop.
    BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,float(CanvasWidth),-float(CanvasHeight)},{0,0},WHITE);EndDrawing();
    Image image=LoadImageFromTexture(target.texture);require(image.data!=nullptr,"Native GUI framebuffer readback failed");
    ImageFlipVertical(&image);const auto path=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/name;
    std::filesystem::create_directories(path.parent_path());
    const bool written=ExportImage(image,path.string().c_str());
    Color* colors=LoadImageColors(image);if(!colors){UnloadImage(image);throw std::runtime_error("Native GUI pixel readback failed");}
    std::vector<Color> pixels(colors,colors+std::size_t(image.width)*std::size_t(image.height));
    UnloadImageColors(colors);UnloadImage(image);require(written,"Native GUI capture could not be written");
    return pixels;
}
void requireUpgradeFrame(const std::vector<Color>& pixels,int cards){
    int heading=0;
    for(int y=34;y<51;++y)for(int x=280;x<520;++x){const auto c=pixels[std::size_t(y*CanvasWidth+x)];
        heading+=c.r==255&&c.g==255&&c.b==255&&c.a==255;
    }
    require(heading>200,"Upgrade heading was not visible in the native framebuffer");
    const int first=(CanvasWidth-(176*cards+14*(cards-1)))/2;
    for(int slot=0;slot<cards;++slot){const auto c=pixels[std::size_t(130*CanvasWidth+first+8+slot*190)];
        require(c.r>=20&&c.g>=20&&c.b>=20&&c.a==255,"Upgrade card background disappeared in the native framebuffer");
    }
}
}

int main(){
    int passed=0,failed=0;RenderTexture2D target{};
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"[PASS] "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"[FAIL] "<<name<<": "<<error.what()<<'\n';}};
    try{
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(CanvasWidth,CanvasHeight,"Snake GUI text tests");
        require(IsWindowReady()&&IsWindowHidden(),"Hidden native OpenGL window is unavailable");
        target=LoadRenderTexture(CanvasWidth,CanvasHeight);require(target.id!=0,"Native GUI framebuffer creation failed");
        SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
        test("all equipment ranks remain separated and readable over a bright world",[&]{
            Presentation app(catalog);prepare(app);auto& game=app.game;
            for(const char* id:{"venom_shot","viper_fangs","plague_mortar","snake_nest","dead_eye","ravenous_maw","chronofield","serpents_scales"})
                game.owned.push_back({catalog.find(id),8,false,1000});
            // A real white snake sweeps beneath both HUD rows. No overlay is
            // involved in the reference capture, so this exercises contrast.
            game.player.invulnerable=0;game.player.segments={{800,800},{632,800},{632,704}};
            for(int row=0;row<5;++row){
                const float y=704+float(row*8),end=row%2==0?960.0f:632.0f;
                if(row>0)game.player.segments.push_back({row%2==0?632.0f:960.0f,y});
                game.player.segments.push_back({end,y});
            }
            game.player.previousSegments=game.player.segments;
            const auto world=capture(app,target,"ui-hud-bright-world.png",true);
            const auto pixels=capture(app,target,"ui-hud-ranks.png");
            for(int row=0;row<2;++row)for(int column=0;column<4;++column){
                int behind=0,white=0,dark=0;
                for(int y=50+row*25;y<58+row*25;++y)for(int x=6+column*21;x<18+column*21;++x){
                    const auto background=world[std::size_t(y*CanvasWidth+x)],c=pixels[std::size_t(y*CanvasWidth+x)];
                    behind+=background.r>240&&background.g>240&&background.b>240;
                    white+=c.r==255&&c.g==255&&c.b==255;
                    dark+=c.r<24&&c.g<24&&c.b<24;
                }
                require(behind>20,"HUD contrast fixture did not place bright gameplay beneath a rank");
                require(white>=8&&dark>=40,"An equipment rank has insufficient bright glyphs or dark surrounding pixels");
            }
            for(int y=58;y<61;++y)for(int x=0;x<96;++x){const auto c=pixels[std::size_t(y*CanvasWidth+x)];
                require(c.r<24&&c.g<24&&c.b<24,"Equipment icons intrude into the gap beneath weapon ranks");
            }
            game.kills=1234567890;game.player.length=1234567890;
            const auto longLabels=capture(app,target,"ui-hud-long-counters.png");
            const int killsRight=pixel::width("Kills: 1234567890")+8;
            const int lengthLeft=CanvasWidth-pixel::width("Length: 1234567890")-10;
            require(killsRight>95&&lengthLeft<CanvasWidth-80,"Long-counter fixture does not exceed the previous fixed HUD panels");
            for(const int x:{killsRight,lengthLeft}){const auto c=longLabels[std::size_t(10*CanvasWidth+x)];
                require(c.r<24&&c.g<24&&c.b<24,"A long HUD counter extends beyond its dark background");
            }
        });
        test("menu upgrade result and codex text fixtures render without changing the run",[&]{
            Presentation app(catalog);prepare(app);auto& game=app.game;
            const auto elapsed=game.elapsed;const auto seed=game.seed;
            app.screen=Screen::Menu;
            for(int character=0;character<4;++character){app.selectedCharacter=character;
                const std::string name="ui-menu-"+std::to_string(character)+".png";capture(app,target,name.c_str());
            }
            app.screen=Screen::Game;game.phase=Phase::LevelUp;
            for(std::size_t first=0;first<catalog.upgrades.size();first+=3){
                for(int slot=0;slot<3;++slot)game.offers[std::size_t(slot)]=first+std::size_t(slot)<catalog.upgrades.size()?int(first)+slot:-1;
                const std::string name="ui-upgrades-"+std::to_string(first/3)+".png";const auto pixels=capture(app,target,name.c_str());
                requireUpgradeFrame(pixels,3);
            }
            game.offers.fill(-1);requireUpgradeFrame(capture(app,target,"ui-upgrades-fallback.png"),1);
            game.phase=Phase::Dead;capture(app,target,"ui-result-dead.png");
            game.phase=Phase::Victory;capture(app,target,"ui-result-victory.png");
            game.phase=Phase::Playing;app.codex=true;capture(app,target,"ui-codex.png");
            require(game.elapsed==elapsed&&game.seed==seed&&game.owned.empty()&&game.chestRewards.empty(),"Drawing GUI text fixtures changed gameplay state");
        });
        test("minimal evolution recipes retain accurate ranks and neutral uncluttered tiles",[&]{
            Presentation app(catalog);prepare(app);app.codex=true;
            // Maxed without a passive, ready with a rank-one passive, already
            // evolved, partly ranked, a passive-only recipe, then unowned recipes.
            app.game.owned={{catalog.find("venom_shot"),8,false,1000},
                {catalog.find("viper_fangs"),8,false,1000},{catalog.find("rapid_fire"),1,false,1000},
                {catalog.find("plague_mortar"),8,true,1000},{catalog.find("graviton"),8,false,1000},
                {catalog.find("snake_nest"),3,false,1000},{catalog.find("toxic_expanse"),1,false,1000}};
            const auto pixels=capture(app,target,"ui-codex-states.png");
            // Compare the actual count glyphs, including a rank-eight passive
            // capped to the recipe's one-rank requirement, with native font masks.
            const char* counts[][2]{{"8/8","0/1"},{"8/8","1/1"},{"8/8","1/1"},{"3/8","0/1"},{"0/8","1/1"}};
            BeginTextureMode(target);ClearBackground(BLACK);
            for(int i=0;i<5;++i)for(int row=0;row<2;++row){
                const int x=49+(i%3)*238+214-pixel::width(counts[i][row]),y=112+(i/3)*96+45+row*19;
                pixel::text(counts[i][row],x,y,WHITE);
            }
            EndTextureMode();Image mask=LoadImageFromTexture(target.texture);
            require(mask.data!=nullptr,"Native requirement font mask could not be read");ImageFlipVertical(&mask);
            bool countsMatch=true;
            for(int i=0;i<5;++i)for(int row=0;row<2;++row){
                const int width=pixel::width(counts[i][row]),x=49+(i%3)*238+214-width,y=112+(i/3)*96+45+row*19;
                for(int py=y;py<y+8;++py)for(int px=x;px<x+width;++px)
                    countsMatch=countsMatch&&((GetImageColor(mask,px,py).r>0)==(pixels[std::size_t(py*CanvasWidth+px)].r>=148));
            }
            UnloadImage(mask);require(countsMatch,"A trimmed recipe lost its real weapon or passive count");
            for(std::size_t i=0;i<catalog.evolutions.size()&&i<9;++i){
                const auto& recipe=catalog.evolutions[i];
                const auto& weapon=catalog.upgrades[std::size_t(catalog.find(recipe.weapon))];
                const auto& passive=catalog.upgrades[std::size_t(catalog.find(recipe.passive))];
                require(pixel::width(recipe.name)<=164,"An evolution title overlaps its completion tick");
                require(pixel::width(weapon.name)+pixel::width("8/8")+6<=186,"A weapon label collides with its rank");
                require(pixel::width(passive.name)+pixel::width("1/1")+6<=186,"A passive label collides with its rank");
                const int x=49+int(i%3)*238,y=112+int(i/3)*96;
                const bool hasWeapon=i<4;const int backing=hasWeapon?30:23;
                const auto marker=pixels[std::size_t((y+18)*CanvasWidth+x+5)];
                require(marker.r==(hasWeapon?148:23),"Ownership marker did not follow the weapon, including passive-only recipes");
                int brightRank=0;
                for(int py=y+45;py<y+53;++py)for(int px=x+190;px<x+214;++px)
                    brightRank+=pixels[std::size_t(py*CanvasWidth+px)].r==226;
                require(hasWeapon?brightRank>10:brightRank==0,"A partly ranked weapon did not stand out from an unowned weapon");
                for(int py=0;py<84;++py)for(int px=0;px<226;++px){
                    const bool icon=(px>=12&&px<36&&py>=10&&py<34)||
                        (px>=12&&px<22&&((py>=44&&py<54)||(py>=63&&py<73)));
                    const auto c=pixels[std::size_t((y+py)*CanvasWidth+x+px)];
                    if(!icon)require(c.r==c.g&&c.g==c.b,"An evolution tile regained a colored accent");
                    if(px>=44&&px<214&&py>=28&&py<43)
                        require(c.r==backing&&c.g==backing&&c.b==backing,"A status label or divider returned beneath an evolution title");
                }
            }
            for(int x=28;x<772;++x)for(const int y:{96,403}){const auto c=pixels[std::size_t(y*CanvasWidth+x)];
                require(c.r==12&&c.g==12&&c.b==12,"The evolution guide regained a bright outer border");
            }
            require(app.game.owned[0].rank==8&&!app.game.owned[0].evolved&&app.game.chests==0,
                "Drawing chest readiness evolved a weapon or granted a reward");
        });
        require(!IsAudioDeviceReady(),"Native GUI tests initialized audio");
    }catch(const std::exception& error){++failed;std::cerr<<"Native GUI setup failed: "<<error.what()<<'\n';}
    if(target.id)UnloadRenderTexture(target);if(IsWindowReady())CloseWindow();
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
