#include "death_art.hpp"
#include "world_raster.hpp"
#include "pixel_art.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using snake::render::DeathArt;
constexpr int Width=800,Height=500;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<Color> pixels(RenderTexture2D target){
    Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);Color* colors=LoadImageColors(image);
    require(colors!=nullptr,"Framebuffer readback failed");std::vector<Color> result(colors,colors+Width*Height);
    UnloadImageColors(colors);UnloadImage(image);return result;
}
std::vector<Color> capture(RenderTexture2D target,DeathArt& art,const snake::geometry::SnakeGeometry& shape,float age,int cameraX=0,int cameraY=0,bool fragments=false){
    BeginTextureMode(target);ClearBackground(BLANK);art.draw(shape,{204,0,0,255},age,.42f,713,cameraX,cameraY,fragments);art.flush();EndTextureMode();return pixels(target);
}
bool same(const std::vector<Color>& a,const std::vector<Color>& b){return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](Color x,Color y){return x.r==y.r && x.g==y.g && x.b==y.b && x.a==y.a;});}
snake::geometry::SnakeGeometry body(int x=320,int y=220,int width=11){
    const std::array<snake::Vec,6> path{{{float(x),float(y)},{float(x-20),float(y)},{float(x-40),float(y)},{float(x-40),float(y+20)},{float(x-40),float(y+40)},{float(x-60),float(y+40)}}};
    return snake::geometry::buildSnake(path.data(),path.data(),path.size(),1,width,1);
}
}

int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(Width,Height,"Pixel death contracts");
    if(!IsWindowReady())return 1;
    auto target=LoadRenderTexture(Width,Height);SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
    {
        pixel::Art fontOwner;DeathArt art;require(art.ready(),"Death shader failed to prepare");const auto shape=body();
        test("impact exactly covers the native cardinal silhouette",[&]{
            const auto image=capture(target,art,shape,0);
            for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){const auto p=image[std::size_t(y)*Width+x];require((p.a!=0)==snake::geometry::contains(shape,x,y),"Death flash changed the body silhouette");require(p.a==0 || p.a==255,"Death edges use fractional alpha");}
        });
        test("dissolve removes pixels monotonically and finishes completely",[&]{
            auto previous=capture(target,art,shape,0);int initial=0,last=0;
            for(const auto& p:previous)initial+=p.a!=0;
            for(float age:{.05f,.10f,.16f,.22f,.29f,.36f,.42f}){
                const auto image=capture(target,art,shape,age);last=0;
                for(std::size_t i=0;i<image.size();++i){require(!image[i].a || previous[i].a,"Dissolve resurrected a pixel");require(image[i].a==0 || image[i].a==255,"Dissolve blurred native pixels");last+=image[i].a!=0;}previous=image;
                if(age==.22f)require(last>0 && last<initial,"Dissolve did not pass through a partial body");
            }
            require(last==0,"Finished death retained pixels");
        });
        test("camera translation preserves every dissolve cell and fragment",[&]{
            const auto original=capture(target,art,shape,.21f,0,0,true),translated=capture(target,art,shape,.21f,7,-5,true);
            for(int y=5;y<Height;++y)for(int x=0;x<Width-7;++x){const auto a=original[std::size_t(y-5)*Width+x+7],b=translated[std::size_t(y)*Width+x];require(a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a,"Camera made the dissolve crawl or changed fragment alignment");}
            require(same(original,capture(target,art,shape,.21f,0,0,true)),"Rendering consumed RNG or changed the effect");
        });
        test("each dissolve chip uses exactly one native two-pixel cell",[&]{
            const auto image=capture(target,art,shape,.21f);
            for(int y=180;y<290;y+=2)for(int x=220;x<350;x+=2){
                if(!snake::geometry::contains(shape,x,y) || !snake::geometry::contains(shape,x+1,y) ||
                   !snake::geometry::contains(shape,x,y+1) || !snake::geometry::contains(shape,x+1,y+1))continue;
                const auto first=image[std::size_t(y)*Width+x];
                for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto p=image[std::size_t(y+dy)*Width+x+dx];require(p.r==first.r && p.g==first.g && p.b==first.b && p.a==first.a,"Dissolve split or dithered a native chip");}
            }
        });
        test("batch ring rollover preserves opaque order and raster state",[&]{
            const snake::Vec point{100,100};const auto square=snake::geometry::buildSnake(&point,&point,1,1,2,1);
            snake::render::WorldRaster raster;
            BeginTextureMode(target);ClearBackground(BLANK);raster.rectangle(99,99,2,2,GREEN);raster.flush();
            for(unsigned i=0;i<70000;++i)art.draw(square,i==69999?BLUE:RED,0,.42f,i);
            art.flush();raster.rectangle(100,100,1,1,GOLD);raster.flush();EndTextureMode();
            const auto image=pixels(target);require(image[100*Width+100].r==GOLD.r,"Death batch changed later raster state");
            require(image[99*Width+99].b>image[99*Width+99].r,"Death upload ring lost last-instance order");
        });
        // Base death palettes. Splitters' live offspring handoff is captured
        // separately by showcase_splitter.py using the actual simulation.
        const std::array<Color,7> colors{{{204,0,0,255},{34,102,204,255},{204,0,0,255},{187,68,255,255},{0,255,0,255},{187,68,255,255},{226,236,225,255}}};
        constexpr const char* names[]{"RED","BLUE","SWIFT","BOSS","NEST","HYDRA","PLAYER"};
        for(int frame=0;frame<=48;++frame){
            BeginTextureMode(target);ClearBackground({9,12,14,255});
            for(int row=0;row<7;++row){
                const int y=38+row*65;pixel::text(names[row],24,y+8,{181,190,187,255});
                const int width=row==3?15:row==4?7:row==5?8:row==6?14:11;
                const auto specimen=body(455,y,width);const float duration=row==3?.68f:row==6?.72f:row>=4?.38f:.42f;
                art.draw(specimen,colors[std::size_t(row)],float(frame)/60,duration,unsigned(713+row));
            }
            art.flush();EndTextureMode();
            std::filesystem::create_directories(SNAKE_TEST_ARTIFACT_DIR "/death");
            Image shot=LoadImageFromTexture(target.texture);ImageFlipVertical(&shot);
            const auto path=std::string(SNAKE_TEST_ARTIFACT_DIR)+"/death/frame-"+std::to_string(frame)+".png";
            require(ExportImage(shot,path.c_str()),"Could not export death preview");UnloadImage(shot);
        }
    }
    test("death GPU owners can be recreated before context shutdown",[&]{
        for(int i=0;i<3;++i){DeathArt art;require(art.ready(),"Recreated shader failed");require(!capture(target,art,body(),.12f).empty(),"Recreated death capture failed");}
    });
    UnloadRenderTexture(target);CloseWindow();std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
