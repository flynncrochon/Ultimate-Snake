#include "presentation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<unsigned char> capture(snake::Presentation& app,RenderTexture2D target,const std::filesystem::path& path={}){
    BeginTextureMode(target);ClearBackground(BLACK);app.drawWorldOnly();EndTextureMode();
    Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
    ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if(!path.empty())require(ExportImage(image,path.string().c_str()),"Could not save water capture");
    const auto* data=static_cast<const unsigned char*>(image.data);std::vector<unsigned char> pixels(data,data+image.width*image.height*4);
    UnloadImage(image);return pixels;
}
}
int main(){
    try{
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(800,500,"Flood pixel tests");SetTargetFPS(0);
        require(IsWindowReady(),"Native OpenGL window unavailable");
        const auto catalog=snake::Catalog::load(SNAKE_TEST_DATA_DIR);const std::filesystem::path output=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/"flood";
        std::filesystem::create_directories(output);
        RenderTexture2D target=LoadRenderTexture(800,500);SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
        RenderTexture2D wide=LoadRenderTexture(1280,800);SetTextureFilter(wide.texture,TEXTURE_FILTER_POINT);
        {
            snake::Presentation app(catalog);app.screen=snake::Screen::Game;app.captureScene("flood");app.setRenderFrame(1,false);
            capture(app,target,output/"showcase.png");
            app.game.owned.clear();app.game.projectiles.clear();app.game.zones.clear();app.game.beams.clear();
            const auto reference=capture(app,target,output/"full.png");
            int water=0,foam=0;for(std::size_t i=0;i<reference.size();i+=4){water+=reference[i+2]>reference[i]+40 && reference[i+2]>reference[i+1];foam+=reference[i]>130 && reference[i+1]>200 && reference[i+2]>230;}
            require(water>16000 && foam>150,"Flood lost its coherent blue surface or foam crest");
            std::vector<std::uint32_t> shades;
            auto isWater=[](const std::vector<unsigned char>& pixels,std::size_t i){return pixels[i+2]>pixels[i]+20 && pixels[i+2]>pixels[i+1];};
            for(std::size_t i=0;i<reference.size();i+=4)if(isWater(reference,i)){
                const auto color=std::uint32_t(reference[i])|(std::uint32_t(reference[i+1])<<8)|(std::uint32_t(reference[i+2])<<16);
                if(std::find(shades.begin(),shades.end(),color)==shades.end())shades.push_back(color);
            }
            require(shades.size()==7,"The selected original seven-tone water material changed");
            const float textureAge=app.game.flood.age;app.game.flood.age+=.125f;
            const auto shimmer=capture(app,target);int textureChanges=0;
            for(std::size_t i=0;i<reference.size();i+=4){
                require(isWater(reference,i)==isWater(shimmer,i),"Texture animation moved the water silhouette");
                textureChanges+=std::memcmp(reference.data()+i,shimmer.data()+i,4)!=0;
            }
            require(textureChanges>500,"Stationary water no longer animates its original wave lines");
            app.game.flood.age=textureAge;
            app.settings.render.weaponEffects=false;app.settings.render.particles=false;
            require(capture(app,target)==reference,"Effects settings hid or changed lethal water");
            app.settings.render.weaponEffects=true;app.settings.render.particles=true;
            const auto particles=app.game.flood.particles;const auto age=app.game.flood.age;
            for(int i=0;i<40;++i){app.setRenderFrame(float(i%5)/4,true);capture(app,target);}
            require(app.game.flood.age==age && app.game.flood.particles.size()==particles.size(),"Rendering advanced flood simulation");
            for(std::size_t i=0;i<particles.size();++i){const auto& a=particles[i];const auto& b=app.game.flood.particles[i];require(a.position.x==b.position.x && a.position.y==b.position.y && a.velocity.x==b.velocity.x && a.velocity.y==b.velocity.y,"Render cadence changed water motion");}
            app.setRenderFrame(.5f,true);const auto middle=capture(app,target,output/"interpolated.png");
            require(capture(app,target)==middle,"Identical render samples flickered");
            app.setRenderFrame(1,false);require(capture(app,target)==reference,"Interpolation left stale density pixels");
            app.game.flood.reset();for(auto& e:app.game.enemies)if(e.kind==snake::EnemyKind::Flood)e.alive=false;
            const auto dry=capture(app,target,output/"retired.png");int blue=0;
            for(std::size_t i=0;i<dry.size();i+=4)blue+=dry[i+2]>dry[i]+40 && dry[i+2]>dry[i+1];
            require(blue==0,"Retired water left stale pixels");
            for(const auto point:{snake::Vec{8,8},snake::Vec{1592,8},snake::Vec{8,1592},snake::Vec{1592,1592}}){
                app.game.enemies.clear();app.game.flood.reset();
                auto& player=app.game.player;player.position=player.previous=point;player.segments={point};player.previousSegments=player.segments;
                require(app.game.spawnFlood(point,{1,0},int(snake::FloodCapacity),0),"Arena-edge fixture failed");
                const auto edge=capture(app,target);int waterPixels=0;
                for(std::size_t i=0;i<edge.size();i+=4)waterPixels+=edge[i+2]>edge[i]+40 && edge[i+2]>edge[i+1];
                require(waterPixels>100,"Arena-edge clipping lost the flood");
            }
            const auto edgeReference=capture(app,target,output/"arena-edge.png");
            app.setViewport(1280,800);capture(app,wide,output/"wide.png");
            app.setViewport(800,500);require(capture(app,target)==edgeReference,"Viewport changes left stale density pixels or shifted the shared camera");
            app.game.enemies.clear();app.game.flood.reset();
            auto& player=app.game.player;player.position=player.previous={800,800};player.segments={{800,800}};player.previousSegments=player.segments;
            require(app.game.spawnFlood({880,824},{1,0},320,0),"First joining-water fixture failed");
            const auto beforeJoin=capture(app,target);
            const auto handle=app.game.flood.enemyId;
            require(app.game.spawnFlood({916,824},{-1,0},320,1.5f),"Second water wave did not join");
            const auto incoming=capture(app,target,output/"joining-warning.png");
            require(app.game.flood.enemyId==handle && app.game.enemies.size()==1 && app.game.flood.waveCount()==2,"Joined water duplicated its enemy handle");
            int shared=0,changes=0;
            for(std::size_t i=0;i<incoming.size();i+=4){shared+=isWater(beforeJoin,i) && isWater(incoming,i);changes+=std::memcmp(beforeJoin.data()+i,incoming.data()+i,4)!=0;}
            require(shared>5000 && changes>500,"Joining water erased old water or failed to appear");
            for(auto& p:app.game.flood.particles)p.age=2;
            const auto joined=capture(app,target,output/"joined.png");
            int firstX=800,lastX=0;
            for(int x=0;x<800;++x)if(isWater(joined,std::size_t((310*800+x)*4))){firstX=std::min(firstX,x);lastX=std::max(lastX,x);}
            require(lastX-firstX>160,"Joined water lost its broad connected surface");
            for(int x=firstX;x<=lastX;++x)require(isWater(joined,std::size_t((310*800+x)*4)),"A seam or gap split the touching water waves");
            for(auto& p:app.game.flood.particles)if(p.wave==1)p.age=snake::FloodLifetime+.5f;
            const auto draining=capture(app,target,output/"joining-drain.png");
            int stillOpaque=0;
            for(std::size_t i=0;i<joined.size();i+=4)stillOpaque+=isWater(joined,i) && std::memcmp(joined.data()+i,draining.data()+i,4)==0;
            require(stillOpaque>5000,"An older wave's fade drained the younger joined surface");
            app.startRun(1942);
            require(app.game.flood.enabled && !app.game.flood.active() && app.game.flood.clock==0 && app.game.flood.nextSpawn==snake::FloodFirstSeconds,
                    "Starting a normal run after the preview disabled natural floods");
        }
        UnloadRenderTexture(wide);UnloadRenderTexture(target);CloseWindow();std::cout<<"PASS native flood coverage, foam, settings, interpolation, retirement, arena edges, viewport changes and render purity\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}
}
