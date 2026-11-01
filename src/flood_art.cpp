#include "flood_art.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace snake::render {
namespace {
constexpr int Sample=2,Side=2004,Radius=7,KernelSide=Radius*2+1;
constexpr float Scale=2.5f;
constexpr std::uint16_t Surface=72;
// Joined blue water, wandering pale current lines and fine foam at solid bodies.
constexpr std::array<Color,8> Palette{{
    {0,0,0,0},{9,40,72,255},{20,73,128,255},{29,103,167,255},
    {46,135,194,255},{75,170,217,255},{139,218,242,255},{222,252,255,255}
}};
struct Bounds {int left=Side,top=Side,right=0,bottom=0;bool valid()const{return left<=right && top<=bottom;}};
}
struct FloodArt::Storage {
    // Allocate and prepare the complete bounded surface before play.
    std::vector<std::uint16_t> density;
    std::vector<std::uint16_t> foam;
    std::vector<std::uint8_t> opacity;
    std::array<std::uint16_t,KernelSide*KernelSide> kernel{};
    std::array<int,256> sine{};
    Bounds previous;
    Storage():density(std::size_t(Side*Side),0),foam(std::size_t(Side*Side),0),opacity(std::size_t(Side*Side),0){
        for(int y=-Radius;y<=Radius;++y)for(int x=-Radius;x<=Radius;++x){
            const float q=std::max(0.0f,1-float(x*x+y*y)/float(Radius*Radius));
            kernel[std::size_t((y+Radius)*KernelSide+x+Radius)]=std::uint16_t(q*q*160);
        }
        for(std::size_t i=0;i<sine.size();++i)sine[i]=int(std::lround(std::sin(float(i)*6.2831853f/256)*16));
    }
};
FloodArt::FloodArt():storage(std::make_unique<Storage>()){}
FloodArt::~FloodArt() = default;
void FloodArt::draw(const FloodSystem& flood,WorldRaster& raster,int cameraX,int cameraY,int width,int height,Vec pixelOffset,float alpha,bool interpolating){
    auto& s=*storage;
    if(s.previous.valid())for(int y=s.previous.top;y<=s.previous.bottom;++y){
        const auto begin=std::size_t(y*Side+s.previous.left),end=std::size_t(y*Side+s.previous.right+1);
        std::fill(s.density.begin()+begin,s.density.begin()+end,std::uint16_t(0));
        std::fill(s.foam.begin()+begin,s.foam.begin()+end,std::uint16_t(0));
        std::fill(s.opacity.begin()+begin,s.opacity.begin()+end,std::uint8_t(0));
    }
    s.previous={};if(!flood.active())return;
    const int minX=std::clamp(cameraX/Sample-1,1,Side-2),minY=std::clamp(cameraY/Sample-1,1,Side-2);
    const int maxX=std::clamp((cameraX+width)/Sample+1,1,Side-2),maxY=std::clamp((cameraY+height)/Sample+1,1,Side-2);
    Bounds bounds;
    for(const auto& particle:flood.particles){
        const Vec p=interpolating?Vec{particle.previous.x+(particle.position.x-particle.previous.x)*alpha,
                                    particle.previous.y+(particle.position.y-particle.previous.y)*alpha}:particle.position;
        const int cx=int(std::floor((p.x*Scale+pixelOffset.x+1)/Sample)),cy=int(std::floor((p.y*Scale+pixelOffset.y+1)/Sample));
        const int left=std::max(minX,cx-Radius),right=std::min(maxX,cx+Radius);
        const int top=std::max(minY,cy-Radius),bottom=std::min(maxY,cy+Radius);
        if(left>right || top>bottom)continue;
        bounds.left=std::min(bounds.left,left);bounds.right=std::max(bounds.right,right);
        bounds.top=std::min(bounds.top,top);bounds.bottom=std::max(bounds.bottom,bottom);
        const unsigned foam=unsigned(particle.foam*128);
        const float time=std::max(0.0f,particle.age+(interpolating?(alpha-1)*FixedStep:0));
        const float fade=std::clamp(flood.lifetime+1-time,0.0f,1.0f);
        const auto opacity=static_cast<std::uint8_t>(255*fade*(time<particle.warning?.35f+.12f*std::sin(time*10):1));
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){
            const auto index=std::size_t(y*Side+x);const unsigned weight=s.kernel[std::size_t((y-cy+Radius)*KernelSide+x-cx+Radius)];
            s.density[index]=std::uint16_t(std::min(65535u,unsigned(s.density[index])+weight));
            s.foam[index]=std::uint16_t(std::min(65535u,unsigned(s.foam[index])+((weight*foam)>>7)));
            if(weight)s.opacity[index]=std::max(s.opacity[index],opacity);
        }
    }
    s.previous=bounds;if(!bounds.valid())return;
    const float time=std::max(0.0f,flood.age+(interpolating?(alpha-1)*FixedStep:0));
    const int phase=int(time*36),flowX=int(flood.direction.x*phase),flowY=int(flood.direction.y*phase);
    auto shade=[&](int x,int y)->int{
        const auto index=std::size_t(y*Side+x);const unsigned value=s.density[index];if(value<Surface)return 0;
        const unsigned up=s.density[index-Side],left=s.density[index-1],right=s.density[index+1],down=s.density[index+Side];
        const unsigned edge=std::min({up,left,right,down});
        const int bend=s.sine[std::size_t((y*5+x-phase)&255)]+s.sine[std::size_t((x*2+y*3+phase/2)&255)]/2;
        const int ripple=(x*3+y+bend-flowX*2-flowY)&63;
        const unsigned froth=s.foam[index];
        if(edge<Surface){
            if(up<Surface || left<Surface)return ((x+y+phase/5)&7)<5?7:6;
            return froth>value/3?6:1;
        }
        if(froth>value/2 && ((x*13+y*7+phase/3)&15)<5)return 7;
        if(edge<Surface*2)return 5;
        if(ripple<2 && ((x/13+y/9+phase/12)&3))return 5;
        if(ripple<7)return 4;
        return value>430?2:3;
    };
    for(int y=bounds.top;y<=bounds.bottom;++y){
        int first=bounds.left,lastShade=shade(first,y);
        auto lastOpacity=s.opacity[std::size_t(y*Side+first)];
        for(int x=first+1;x<=bounds.right+1;++x){
            const int material=x>bounds.right?0:shade(x,y);
            const auto opacity=x>bounds.right?std::uint8_t(0):s.opacity[std::size_t(y*Side+x)];
            if(material==lastShade && opacity==lastOpacity && x<=bounds.right)continue;
            if(lastShade){Color color=Palette[std::size_t(lastShade)];color.a=lastOpacity;raster.rectangle(first*Sample-cameraX,y*Sample-cameraY,(x-first)*Sample,Sample,color);}
            first=x;lastShade=material;lastOpacity=opacity;
        }
    }
}
}
