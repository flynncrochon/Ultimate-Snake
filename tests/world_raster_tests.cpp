#include "world_raster.hpp"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int Width=800,Height=500;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void referenceLine(int x1,int y1,int x2,int y2,Color color){
    const int dx=std::abs(x2-x1),sx=x1<x2?1:-1,dy=-std::abs(y2-y1),sy=y1<y2?1:-1;int error=dx+dy;
    for(;;){DrawPixel(x1,y1,color);if(x1==x2 && y1==y2)break;const int twice=2*error;if(twice>=dy){error+=dy;x1+=sx;}if(twice<=dx){error+=dx;y1+=sy;}}
}
std::vector<Color> pixels(RenderTexture2D target){
    Image image=LoadImageFromTexture(target.texture);require(image.data!=nullptr,"Framebuffer readback failed");
    Color* data=LoadImageColors(image);require(data!=nullptr,"Framebuffer colors unavailable");
    std::vector<Color> result(data,data+std::size_t(Width)*Height);UnloadImageColors(data);UnloadImage(image);return result;
}
void equal(const std::vector<Color>& a,const std::vector<Color>& b,const char* message){
    require(a.size()==b.size(),"Framebuffer sizes differ");
    for(std::size_t i=0;i<a.size();++i)if(a[i].r!=b[i].r || a[i].g!=b[i].g || a[i].b!=b[i].b || a[i].a!=b[i].a){
        throw std::runtime_error(std::string(message)+" at sourcepixel "+std::to_string(i%Width)+","+std::to_string(i/Width)+
            " old="+std::to_string(a[i].r)+","+std::to_string(a[i].g)+","+std::to_string(a[i].b)+","+std::to_string(a[i].a)+
            " new="+std::to_string(b[i].r)+","+std::to_string(b[i].g)+","+std::to_string(b[i].b)+","+std::to_string(b[i].a));
    }
}
}
int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(Width,Height,"Native raster equivalence");
    if(!IsWindowReady()){std::cerr<<"OpenGL context unavailable\n";return 1;}
    auto reference=LoadRenderTexture(Width,Height),actual=LoadRenderTexture(Width,Height);
    {
        snake::render::WorldRaster raster;require(raster.ready(),"Native packed raster initialization failed");
        test("packed integer rectangles preserve clipped RGBA blends",[&]{
            auto draw=[&](bool fast){
                for(int i=0;i<300;++i){const Color color{std::uint8_t(i*53),std::uint8_t(i*97),std::uint8_t(i*29),std::uint8_t((i*31)%256)};
                    const int x=(i*67)%1000-100,y=(i*83)%650-75,w=1+(i*23)%210,h=1+(i*19)%150;
                    if(fast)raster.rectangle(x,y,w,h,color);else DrawRectangle(x,y,w,h,color);
                }
                if(fast)raster.flush();
            };
            BeginTextureMode(reference);ClearBackground({17,23,31,127});draw(false);EndTextureMode();
            BeginTextureMode(actual);ClearBackground({17,23,31,127});draw(true);EndTextureMode();
            equal(pixels(reference),pixels(actual),"Packed rectangle coverage/blending changed");
        });
        test("Bresenham runs preserve every octant and repeated translucent endpoints",[&]{
            auto draw=[&](bool fast){
                for(int x=-120;x<=120;x+=9)for(int y=-120;y<=120;y+=11){
                    const Color color{std::uint8_t(90+x),std::uint8_t(120+y),210,std::uint8_t(8+(x*x+y*y)%180)};
                    if(fast)raster.line(400,250,400+x,250+y,color);else referenceLine(400,250,400+x,250+y,color);
                }
                if(fast)raster.flush();
            };
            BeginTextureMode(reference);ClearBackground(BLACK);draw(false);EndTextureMode();
            BeginTextureMode(actual);ClearBackground(BLACK);draw(true);EndTextureMode();
            equal(pixels(reference),pixels(actual),"Line rasterization or alpha overlap changed");
        });
        test("scissor and generic texture commands retain ordered composition",[&]{
            Image image=GenImageColor(4,4,{240,110,50,180});const Texture2D texture=LoadTextureFromImage(image);UnloadImage(image);
            auto draw=[&](bool fast){
                BeginScissorMode(100,100,500,300);
                if(fast){raster.rectangle(0,0,800,500,{10,70,170,32});raster.flush();}else DrawRectangle(0,0,800,500,{10,70,170,32});
                DrawTextureEx(texture,{150,180},0,40,{255,255,255,220});
                if(fast){raster.rectangle(200,150,130,210,{40,220,60,100});raster.flush();}else DrawRectangle(200,150,130,210,{40,220,60,100});
                DrawRectangle(0,350,800,100,{210,20,100,130});EndScissorMode();
                if(fast){raster.rectangle(50,420,700,30,{230,240,30,58});raster.flush();}else DrawRectangle(50,420,700,30,{230,240,30,58});
            };
            BeginTextureMode(reference);ClearBackground(BLACK);draw(false);EndTextureMode();
            BeginTextureMode(actual);ClearBackground(BLACK);draw(true);EndTextureMode();
            equal(pixels(reference),pixels(actual),"Texture/scissor ordering changed");UnloadTexture(texture);
        });
        test("bounded stream rollover preserves dense overlapping geometry",[&]{
            auto draw=[&](bool fast){
                // Exceed both a batch and the complete preallocated stream
                // ring, retaining every translucent overlap in source order.
                for(int i=0;i<150000;++i){const int x=(i*17)%800,y=(i*29)%500;const Color color{std::uint8_t(i*37),std::uint8_t(i*13),std::uint8_t(i*7),std::uint8_t(18+i%100)};
                    if(fast)raster.rectangle(x,y,7,5,color);else DrawRectangle(x,y,7,5,color);
                }
                if(fast)raster.flush();
            };
            BeginTextureMode(reference);ClearBackground(BLACK);draw(false);EndTextureMode();
            BeginTextureMode(actual);ClearBackground(BLACK);draw(true);EndTextureMode();
            equal(pixels(reference),pixels(actual),"Stream rollover changed dense composition");
            require(raster.submissions()>16,"Dense fixture did not wrap the upload ring");
        });
        test("repeated GPU owner lifetimes retain texture and raster state",[&]{
            for(int cycle=0;cycle<4;++cycle){
                snake::render::WorldRaster temporary;require(temporary.ready(),"Recreated raster resources are unavailable");
                BeginTextureMode(reference);ClearBackground(BLACK);DrawRectangle(40,50,120,90,{80,170,210,180});EndTextureMode();
                BeginTextureMode(actual);ClearBackground(BLACK);temporary.rectangle(40,50,120,90,{80,170,210,180});temporary.flush();EndTextureMode();
                equal(pixels(reference),pixels(actual),"Resource recreation leaked graphics state");
            }
        });
    }
    UnloadRenderTexture(reference);UnloadRenderTexture(actual);CloseWindow();
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
