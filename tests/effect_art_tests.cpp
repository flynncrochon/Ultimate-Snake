#include "effect_art.hpp"

#include <raylib.h>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using snake::render::EffectArt;
using snake::render::EffectSprite;
constexpr int Cell=20,Directions=16,Width=Cell*Directions;
constexpr float Tau=6.2831853071795864769f;
constexpr std::array<EffectSprite,11> Sprites{{
    EffectSprite::Venom,EffectSprite::Gatling,EffectSprite::HomingFang,
    EffectSprite::RicochetFang,EffectSprite::Mortar,EffectSprite::Egg,
    EffectSprite::HydraEgg,EffectSprite::OrbitFang,EffectSprite::CobraHead,
    EffectSprite::OrbitFangSmall,EffectSprite::OrbitFangMedium
}};
constexpr int Height=Cell*int(Sprites.size());
void require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
bool same(Color a,Color b){return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;}
std::vector<Color> capture(RenderTexture2D target,const EffectArt& art,float age=0,bool critical=false){
    BeginTextureMode(target);ClearBackground(BLANK);
    for(std::size_t sprite=0;sprite<Sprites.size();++sprite)for(int direction=0;direction<Directions;++direction)
        art.draw(Sprites[sprite],direction*Cell+Cell/2,int(sprite)*Cell+Cell/2,Tau*float(direction)/Directions,age,critical);
    EndTextureMode();
    Image image=LoadImageFromTexture(target.texture);
    require(image.data!=nullptr,"Effect framebuffer readback failed");
    ImageFlipVertical(&image);
    Color* data=LoadImageColors(image);
    if(!data){UnloadImage(image);throw std::runtime_error("Effect framebuffer colors unavailable");}
    std::vector<Color> result(data,data+Width*Height);
    UnloadImageColors(data);UnloadImage(image);return result;
}
Color pixel(const std::vector<Color>& image,int sprite,int direction,int x,int y){
    return image[std::size_t(sprite*Cell+y)*Width+direction*Cell+x];
}
int occupied(const std::vector<Color>& image,int sprite,int direction){
    int count=0;for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x)count+=pixel(image,sprite,direction,x,y).a!=0;return count;
}
}

int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& body){
        try{body();++passed;std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}
    };
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(Width,Height,"Native effect artwork contracts");
    if(!IsWindowReady()){std::cerr<<"OpenGL context unavailable\n";return 1;}
    auto target=LoadRenderTexture(Width,Height);
    {
        EffectArt art;
        const auto baseline=capture(target,art);
        test("every direction retains opaque pixels and a transparent tile gutter",[&]{
            for(int frame=0;frame<4;++frame)for(bool critical:{false,true}){
                const auto image=capture(target,art,float(frame)*.125f,critical);
                for(std::size_t sprite=0;sprite<Sprites.size();++sprite)for(int direction=0;direction<Directions;++direction){
                    const std::string context="sprite "+std::to_string(sprite)+" direction "+std::to_string(direction);
                    require(occupied(image,int(sprite),direction)>=8,"Missing native art: "+context);
                    for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x){
                        const Color color=pixel(image,int(sprite),direction,x,y);
                        require(color.a==0 || color.a==255,"Filtered alpha appeared: "+context);
                        if(x==0 || y==0 || x==Cell-1 || y==Cell-1)
                            require(color.a==0,"Art leaked into neighboring atlas cells: "+context);
                    }
                }
            }
        });
        test("glint phases and critical palettes preserve all occupied pixels",[&]{
            for(int frame=0;frame<4;++frame)for(bool critical:{false,true}){
                const auto image=capture(target,art,float(frame)*.125f,critical);
                require(image.size()==baseline.size(),"Effect framebuffer dimensions changed");
                for(std::size_t i=0;i<image.size();++i)
                    require((image[i].a!=0)==(baseline[i].a!=0),"Animation or critical palette changed the native silhouette");
            }
        });
        test("cardinal directions rotate native pixels around the same center",[&]{
            for(std::size_t sprite=0;sprite<Sprites.size();++sprite)for(int turns=1;turns<4;++turns)
                for(int y=1;y<Cell-1;++y)for(int x=1;x<Cell-1;++x){
                    int dx=x-Cell/2,dy=y-Cell/2;
                    for(int turn=0;turn<turns;++turn){const int old=dx;dx=-dy;dy=old;}
                    require(same(pixel(baseline,int(sprite),0,x,y),pixel(baseline,int(sprite),turns*4,dx+Cell/2,dy+Cell/2)),
                            "Cardinal art pivot or native pixel orientation changed");
                }
        });
        test("orbit sizes retain distinct native footprints in all directions",[&]{
            for(int direction=0;direction<Directions;++direction){
                const int large=occupied(baseline,7,direction),small=occupied(baseline,9,direction),medium=occupied(baseline,10,direction);
                require(small<medium && medium<large,"Orbit fang size variants lost their ordered footprints");
            }
        });
        test("repeated rendering and texture owner recreation retain identical pixels",[&]{
            const auto repeated=capture(target,art);
            for(std::size_t i=0;i<baseline.size();++i)require(same(baseline[i],repeated[i]),"Rendering the same art changed its pixels");
            for(int lifecycle=0;lifecycle<3;++lifecycle){
                EffectArt replacement;const auto image=capture(target,replacement);
                for(std::size_t i=0;i<baseline.size();++i)require(same(baseline[i],image[i]),"Recreated effect owner reused invalid GPU resources");
            }
        });
    }
    UnloadRenderTexture(target);CloseWindow();
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
