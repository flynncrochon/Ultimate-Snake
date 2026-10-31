#include "presentation.hpp"
#include "snake_geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace snake {
namespace {
constexpr float WorldScale = 2.5f;
constexpr int PlayerSize = 14;
constexpr Color Venom{0, 204, 51, 255};
constexpr Color BossPurple{187, 68, 255, 255};

render::WorldRaster* activeRaster=nullptr;
int rasterWidth=CanvasWidth,rasterHeight=CanvasHeight;
struct RasterFrame {
    render::WorldRaster* previous;
    int previousWidth=rasterWidth,previousHeight=rasterHeight;
    RasterFrame(render::WorldRaster& raster,int width,int height):previous(activeRaster){
        if(previous)previous->flush();activeRaster=&raster;rasterWidth=width;rasterHeight=height;
    }
    ~RasterFrame(){activeRaster->flush();activeRaster=previous;rasterWidth=previousWidth;rasterHeight=previousHeight;}
};
void rasterFlush(){if(activeRaster)activeRaster->flush();}
void rasterRectangle(int x,int y,int width,int height,Color color){
    if(activeRaster)activeRaster->rectangle(x,y,width,height,color);else DrawRectangle(x,y,width,height,color);
}
void rasterPixel(int x,int y,Color color){rasterRectangle(x,y,1,1,color);}
void rasterLine(int x1,int y1,int x2,int y2,Color color){
    if(activeRaster)activeRaster->line(x1,y1,x2,y2,color);else pixel::line(x1,y1,x2,y2,color);
}

int rounded(float value) { return static_cast<int>(std::round(value)); }
int relativePixel(double value) {const double biased=value+.50001;const int whole=int(biased);return whole-(biased<whole);}
Vec interpolate(Vec previous, Vec current, float t) {
    return {previous.x + (current.x - previous.x) * t,
            previous.y + (current.y - previous.y) * t};
}
Vec cardinal(Vec direction) {
    if (std::abs(direction.x) > std::abs(direction.y)) return {direction.x < 0 ? -1.0f : 1.0f, 0};
    return {0, direction.y < 0 ? -1.0f : 1.0f};
}
void block(Vec center, int size, Color color) {
    rasterRectangle(rounded(center.x) - size / 2, rounded(center.y) - size / 2, size, size, color);
}
template<std::size_t N> void drawSnake(const geometry::BasicSnakeGeometry<N>& snake,Color color,int cameraX=0,int cameraY=0) {
    for(std::size_t i=0;i<snake.rectangleCount;++i){const auto& r=snake.rectangles[i];rasterRectangle(r.x-cameraX,r.y-cameraY,r.width,r.height,color);}
}
void drawShield(const geometry::SnakeGeometry& shield,int cameraX,int cameraY) {
    struct Layer {int x,y,trim;Color color;};
    constexpr Layer layers[]{
        {0,0,0,{37,85,126,255}},
        {0,0,2,{141,232,248,255}},
        {0,1,2,{67,172,209,255}},
        {1,1,2,{37,85,126,255}},
    };
    // Bevel the complete silhouette, rather than stamping a short highlight at
    // each waypoint. Finish each layer before the next so overlapping elbows
    // retain continuous edges without internal seams. Reuse the same bounded
    // geometry and shared camera for every integer-pixel layer.
    for(const auto& layer:layers)for(std::size_t i=0;i<shield.rectangleCount;++i){
        const auto& r=shield.rectangles[i];
        rasterRectangle(r.x-cameraX+layer.x,r.y-cameraY+layer.y,
                        r.width-layer.trim,r.height-layer.trim,layer.color);
    }
}
void worldLine(int x1,int y1,int x2,int y2,Color color) {
    // Most effect segments are short and completely visible. Convex viewport
    // bounds make clipping unnecessary in that case; avoid four double divides
    // for every strand/pixel run in dense full-quality effects.
    if(x1>=-2 && x1<=rasterWidth+1 && y1>=-2 && y1<=rasterHeight+1 &&
       x2>=-2 && x2<=rasterWidth+1 && y2>=-2 && y2<=rasterHeight+1){
        rasterLine(x1,y1,x2,y2,color);return;
    }
    if((x1<-2 && x2<-2) || (x1>rasterWidth+1 && x2>rasterWidth+1) ||
       (y1<-2 && y2<-2) || (y1>rasterHeight+1 && y2>rasterHeight+1))return;
    // Clip effects before Bresenham rasterization. Long targeting ranges must
    // not spend CPU plotting thousands of pixels outside the framebuffer.
    double first=0,last=1;const double dx=double(x2)-x1,dy=double(y2)-y1;
    auto clip=[&](double p,double q){if(p==0)return q>=0;const double t=q/p;if(p<0){if(t>last)return false;first=std::max(first,t);}else{if(t<first)return false;last=std::min(last,t);}return true;};
    if(!clip(-dx,double(x1)+2) || !clip(dx,rasterWidth+1.0-x1) || !clip(-dy,double(y1)+2) || !clip(dy,rasterHeight+1.0-y1))return;
    const int ax=int(std::lround(x1+dx*first)),ay=int(std::lround(y1+dy*first));
    rasterLine(ax,ay,int(std::lround(x1+dx*last)),int(std::lround(y1+dy*last)),color);
}
void energyPath(Vec start,Vec end,Color edge,Color core,float phase,bool helix,int width=2) {
    const float dx=end.x-start.x,dy=end.y-start.y,length=std::sqrt(dx*dx+dy*dy);
    if(length<1)return;
    const float px=-dy/length,py=dx/length;
    const int steps=std::clamp(int(length/6),8,72);
    for(int strand=0;strand<(helix?2:1);++strand){
        Vec previous=start;
        for(int i=1;i<=steps;++i){const float t=float(i)/float(steps);const float wave=std::sin(t*25.132741f+phase+float(strand)*3.14159265f)*std::sin(t*3.14159265f)*(helix?6.0f:4.0f);
            const Vec next{start.x+dx*t+px*wave,start.y+dy*t+py*wave};
            for(int offset=-width-1;offset<=width+1;++offset)worldLine(rounded(previous.x+px*float(offset)),rounded(previous.y+py*float(offset)),rounded(next.x+px*float(offset)),rounded(next.y+py*float(offset)),std::abs(offset)>width?Color{17,28,39,230}:offset==0?core:edge);
            if(i%4==0)rasterRectangle(rounded(next.x)-1,rounded(next.y)-1,2,2,core);
            previous=next;
        }
    }
}
Vec standingCobra(Vec base,float age,float remaining,float scale,bool ancient,bool glowing) {
    const float rise=std::clamp(age/(ancient?1.5f:.4f),0.0f,1.0f),sink=ancient?1.0f:std::clamp(remaining/.5f,0.0f,1.0f);
    const float height=(ancient?50.0f:32.0f)*scale*(1-(1-rise)*(1-rise))*sink;
    if(height<1)return base;
    const int half=std::max(2,rounded((ancient?3.6f:2.6f)*scale));
    Vec previous=base;
    for(int i=1;i<=10;++i){const float t=float(i)/10;const Vec next{base.x+std::sin(t*4.712f+age*.4f)*5*scale*(1-t*t)+std::sin(age*2.5f)*3*scale*std::sin(t*3.14159265f),base.y-height*t};
        // Standing turrets use an intentionally curved spine, rasterized into
        // thick one-pixel spans. Moving grid snakes always use buildSnake.
        for(int offset=-half-1;offset<=half+1;++offset){
            const Color shade=std::abs(offset)>half?Color{19,39,28,255}:ancient?(glowing?Color{255,220,80,255}:offset<0?Color{248,194,76,255}:Color{153,94,32,255}):offset<0?Color{104,199,94,255}:Color{30,105,55,255};
            worldLine(rounded(previous.x)+offset,rounded(previous.y),rounded(next.x)+offset,rounded(next.y),shade);
        }
        const int nx=rounded(next.x),ny=rounded(next.y);
        rasterRectangle(nx-std::max(1,half/2),ny,std::max(2,half),1,ancient?Color{255,223,137,255}:Color{190,230,132,255});
        previous=next;
    }
    const int hx=rounded(previous.x),hy=rounded(previous.y),hood=std::max(4,rounded((ancient?9.5f:4.0f)*scale)),head=std::max(4,rounded((ancient?7.0f:4.0f)*scale));
    constexpr float HoodProfile[]{.25f,.5f,.75f,.95f,1.0f,1.0f,.9f,.65f,.4f};
    for(int y=-head;y<=head/2;++y){const int row=std::clamp((y+head)*8/std::max(1,head+head/2),0,8);const int width=std::max(1,rounded(float(hood)*HoodProfile[row]));
        rasterRectangle(hx-width-1,hy+y,std::max(3,width*2+3),1,{16,36,25,255});
        rasterRectangle(hx-width,hy+y,std::max(1,width*2+1),1,ancient?Color{218,152,47,255}:Color{55,151,69,255});
        if(width>2){rasterRectangle(hx-width,hy+y,2,1,ancient?Color{255,214,105,255}:Color{136,226,116,255});rasterRectangle(hx+width-2,hy+y,2,1,ancient?Color{133,80,37,255}:Color{27,92,49,255});}
        if(width>5 && y%3==0){rasterRectangle(hx-width+3,hy+y,2,1,ancient?Color{146,84,36,255}:Color{22,75,46,255});rasterRectangle(hx+width-4,hy+y,2,1,ancient?Color{255,219,115,255}:Color{135,215,93,255});}
        if(ancient && width>6 && y<-1){rasterRectangle(hx-2,hy+y,4,1,{246,197,85,255});if(y%4==0)rasterRectangle(hx-1,hy+y,2,1,{255,237,167,255});}
    }
    const int eyeSpread=std::max(2,rounded(float(hood)*.45f));rasterRectangle(hx-eyeSpread,hy-1,2,2,ancient?Color{255,136,0,255}:Color{255,238,68,255});rasterRectangle(hx+eyeSpread-1,hy-1,2,2,ancient?Color{255,136,0,255}:Color{255,238,68,255});
    rasterPixel(hx-eyeSpread,hy,{17,17,17,255});rasterPixel(hx+eyeSpread,hy,{17,17,17,255});
    rasterRectangle(hx-1,hy+1,3,2,ancient?Color{255,227,141,255}:Color{161,219,112,255});
    rasterPixel(hx-2,hy+3,{245,255,214,255});rasterPixel(hx+2,hy+3,{245,255,214,255});
    return {float(hx),float(hy-head/3)};
}
constexpr int CachedCircleLimit=1024;
struct CircleRow {std::uint16_t first=0,last=0,half=0;};
struct CircleCache {
    // Store one quadrant in triangular storage: ~3 MiB for every radius,
    // rather than thousands of separately allocated full-circle span lists.
    std::array<CircleRow,(CachedCircleLimit+1)*(CachedCircleLimit+2)/2> rows{};
    static constexpr std::size_t offset(int radius){return std::size_t(radius)*(radius+1)/2;}
    CircleCache() {
        for(int radius=0;radius<=CachedCircleLimit;++radius){
            auto* profile=rows.data()+offset(radius);
            for(int row=0;row<=radius;++row){profile[row].first=65535;profile[row].half=std::uint16_t(std::sqrt(double(radius)*radius-double(row)*row));}
            const auto plot=[&](int x,int y){auto& row=profile[y];row.first=std::min(row.first,std::uint16_t(x));row.last=std::max(row.last,std::uint16_t(x));};
            int dx=radius,dy=0,error=1-radius;
            while(dx>=dy){
                plot(dx,dy);plot(dy,dx);
                ++dy;if(error<0)error+=2*dy+1;else{--dx;error+=2*(dy-dx)+1;}
            }
        }
    }
    const CircleRow* profile(int radius)const{return rows.data()+offset(radius);}
};
const CircleCache& circleCache(){static const CircleCache cache;return cache;}
void disc(int x,int y,int radius,Color color);
void worldRing(int x,int y,int radius,Color color) {
    if(radius<0 || x+radius<0 || y+radius<0 || x-radius>=rasterWidth || y-radius>=rasterHeight)return;
    if(radius>CachedCircleLimit){
        // Bound extreme upgrade radii to the visible framebuffer as well.
        const double rr=double(radius)*radius;
        for(int row=0;row<rasterHeight;++row){const double d=double(row)-y;if(d*d>rr)continue;const int half=int(std::sqrt(rr-d*d));rasterPixel(x-half,row,color);rasterPixel(x+half,row,color);}
        for(int col=0;col<rasterWidth;++col){const double d=double(col)-x;if(d*d>rr)continue;const int half=int(std::sqrt(rr-d*d));if(half>radius*7/10){rasterPixel(col,y-half,color);rasterPixel(col,y+half,color);}}
        return;
    }
    const auto* profile=circleCache().profile(radius);
    for(int row=std::max(-radius,-y);row<=std::min(radius,rasterHeight-y-1);++row){
        const auto span=profile[std::abs(row)];
        if(span.first==0)rasterRectangle(x-span.last,y+row,span.last*2+1,1,color);
        else {const int width=span.last-span.first+1;rasterRectangle(x-span.last,y+row,width,1,color);rasterRectangle(x+span.first,y+row,width,1,color);}
    }
}
void disc(int x, int y, int radius, Color color) {
    if(radius<0 || x+radius<0 || y+radius<0 || x-radius>=rasterWidth || y-radius>=rasterHeight)return;
    const double farX=std::max(std::abs(double(x)),std::abs(double(x-rasterWidth+1))),farY=std::max(std::abs(double(y)),std::abs(double(y-rasterHeight+1)));
    if(double(radius)*radius>=farX*farX+farY*farY){rasterRectangle(0,0,rasterWidth,rasterHeight,color);return;}
    if(radius>CachedCircleLimit){
        for(int row=std::max(-radius,-y);row<=std::min(radius,rasterHeight-y-1);++row){const int half=int(std::sqrt(double(radius)*radius-double(row)*row));rasterRectangle(x-half,y+row,half*2+1,1,color);}return;
    }
    const auto* profile=circleCache().profile(radius);
    const int last=std::min(radius,rasterHeight-y-1);
    for(int row=std::max(-radius,-y);row<=last;){
        const int half=profile[std::abs(row)].half;int height=1;
        while(row+height<=last && profile[std::abs(row+height)].half==half)++height;
        rasterRectangle(x-half,y+row,half*2+1,height,color);row+=height;
    }
}
void glint(int x,int y,int size,Color color) {
    rasterRectangle(x-size,y,size*2+1,1,color);
    rasterRectangle(x,y-size,1,size*2+1,color);
    rasterPixel(x,y,WHITE);
}
void arc(Vec center,float radius,float start,float sweep,Color color,int steps=16) {
    Vec previous{center.x+std::cos(start)*radius,center.y+std::sin(start)*radius};
    for(int i=1;i<=steps;++i){const float angle=start+sweep*float(i)/float(steps);
        const Vec next{center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius};
        worldLine(rounded(previous.x),rounded(previous.y),rounded(next.x),rounded(next.y),color);previous=next;
    }
}
// Reckoning's source animation has a pair of long white jaws, five teeth per
// bank and a fast open/clamp/release. Bake native pixel spans before play: no
// rotated textures, antialiasing, per-bite allocation or first-use GPU work.
// A grid devour has eight possible headings; every opening shares that grid.
struct ReckoningJaws {
    static constexpr int Directions=8,Openings=13,Extent=66;
    struct Span {std::int8_t x,y;std::uint8_t width,color;};
    struct Pose {std::array<Span,1024> spans{};std::uint16_t count=0;};
    std::array<Pose,Directions*Openings> poses{};
    ReckoningJaws() {
        constexpr int Width=73,Height=28,Left=-10,Top=-20;
        std::array<std::uint8_t,Width*Height> source{};
        constexpr int profile[]{0,2,4,6,8,10,12,14,16,17,17,16,14,11,8,5,2,0};
        const auto material=[&](int x,int y)->std::uint8_t {
            if(x<-8 || x>60)return 0;
            const int column=x+8,index=std::min(column/4,16),fraction=column-index*4;
            const int height=(profile[index]*(4-fraction)+profile[index+1]*fraction+2)/4;
            if(y<=0 && y>=-height){
                if(y>=-1)return 3;
                if(y==-height)return 2;
                if(y<=-height+2)return 5;
                return 4;
            }
            for(int tooth=0;tooth<5;++tooth){const int center=9+tooth*9;
                const int reach=5-tooth/2-std::abs(x-center)*2;
                if(y>0 && y<=reach)return y==reach?5:6;
            }
            return 0;
        };
        for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
            const int sx=x+Left,sy=y+Top;auto color=material(sx,sy);
            if(!color && (material(sx-1,sy)||material(sx+1,sy)||material(sx,sy-1)||material(sx,sy+1)))color=1;
            source[std::size_t(y*Width+x)]=color;
        }
        for(int direction=0;direction<Directions;++direction)for(int opening=0;opening<Openings;++opening){
            auto& pose=poses[std::size_t(direction*Openings+opening)];
            const float angle=float(direction)*.785398163f,spread=opening==0?-.05f:float(opening-1)*.1f;
            const float upperCos=std::cos(angle-spread),upperSin=std::sin(angle-spread);
            const float lowerCos=std::cos(angle+spread),lowerSin=std::sin(angle+spread);
            const auto sample=[&](int x,int y,float c,float s,bool lower)->std::uint8_t {
                const int sx=rounded(float(x)*c+float(y)*s)-Left;
                const int sy=rounded((lower?-1.0f:1.0f)*(-float(x)*s+float(y)*c))-Top;
                return sx>=0 && sx<Width && sy>=0 && sy<Height?source[std::size_t(sy*Width+sx)]:0;
            };
            for(int y=-Extent;y<=Extent;++y){
                int first=-Extent;std::uint8_t previous=0;
                for(int x=-Extent;x<=Extent+1;++x){
                    const auto upper=x<=Extent?sample(x,y,upperCos,upperSin,false):std::uint8_t(0);
                    const auto lower=x<=Extent?sample(x,y,lowerCos,lowerSin,true):std::uint8_t(0);
                    const auto color=lower>1 || !upper?lower:upper;
                    if(color==previous)continue;
                    if(previous){
                        if(pose.count==pose.spans.size())throw std::runtime_error("Reckoning jaw span capacity exceeded");
                        pose.spans[pose.count++]={std::int8_t(first),std::int8_t(y),std::uint8_t(x-first),previous};
                    }
                    first=x;previous=color;
                }
            }
        }
    }
    void draw(int x,int y,float angle,float opening,float opacity)const {
        constexpr Color colors[]{{0,0,0,0},{67,35,57,255},{156,132,160,255},
            {199,189,209,255},{242,239,237,255},{255,255,255,255},{255,242,239,255}};
        const int direction=(rounded(angle/.785398163f)%Directions+Directions)%Directions;
        const int frame=opening<-.025f?0:std::clamp(rounded(opening*10)+1,1,Openings-1);
        const auto& pose=poses[std::size_t(direction*Openings+frame)];
        const auto alpha=static_cast<unsigned char>(std::clamp(opacity*255,0.0f,255.0f));
        for(std::size_t i=0;i<pose.count;++i){const auto& span=pose.spans[i];auto color=colors[span.color];color.a=alpha;
            rasterRectangle(x+span.x,y+span.y,span.width,1,color);
        }
    }
};
const ReckoningJaws& reckoningJaws(){static const ReckoningJaws art;return art;}

void reckoningTongue(Vec start,Vec end,float time,std::uint32_t target,bool pulling,bool detailed) {
    const float dx=end.x-start.x,dy=end.y-start.y,length=std::sqrt(dx*dx+dy*dy);
    if(length<1)return;
    const Vec forward{dx/length,dy/length},normal{-forward.y,forward.x};
    const float phase=time*8+float(target%23)*.37f;
    const int steps=std::clamp(int(length/6),8,64);
    std::array<Vec,65> points{};points[0]=start;
    for(int i=1;i<=steps;++i){const float t=float(i)/float(steps);
        // The ribbon tightens when latched; the traveling ripple runs back
        // toward the mouth, rather than looking like an electrical beam.
        const float wave=std::sin(t*12.566371f+phase)*std::sin(t*3.141593f)*(pulling?2.0f:4.0f);
        points[std::size_t(i)]={start.x+dx*t+normal.x*wave,start.y+dy*t+normal.y*wave};
    }
    struct Layer {int width;Color color;};
    constexpr Layer layers[]{{3,{66,19,45,210}},{2,{167,34,81,255}},{1,{231,64,112,255}},{0,{255,174,203,255}}};
    for(const auto& layer:layers){if(!detailed && layer.width>1)continue;
        for(int offset=-layer.width;offset<=layer.width;++offset)for(int i=1;i<=steps;++i){
            const auto a=points[std::size_t(i-1)],b=points[std::size_t(i)];
            worldLine(rounded(a.x+normal.x*float(offset)),rounded(a.y+normal.y*float(offset)),
                      rounded(b.x+normal.x*float(offset)),rounded(b.y+normal.y*float(offset)),layer.color);
        }
    }
    if(pulling && detailed){
        const int radius=10+rounded(std::sin(time*12+float(target%7)));
        disc(rounded(end.x),rounded(end.y),radius+2,{186,34,92,22});
        worldRing(rounded(end.x),rounded(end.y),radius,{242,74,128,155});
        for(int i=0;i<3;++i){const float t=(float(i)+1-std::fmod(time*1.8f,1.0f))/3;
            const auto p=points[std::size_t(std::clamp(rounded(t*float(steps)),1,steps))];
            worldLine(rounded(p.x+forward.x*2-normal.x*3),rounded(p.y+forward.y*2-normal.y*3),rounded(p.x-forward.x*2),rounded(p.y-forward.y*2),{255,214,225,225});
            worldLine(rounded(p.x-forward.x*2),rounded(p.y-forward.y*2),rounded(p.x+forward.x*2+normal.x*3),rounded(p.y+forward.y*2+normal.y*3),{255,214,225,225});
        }
    }
    // A bifid pink tip becomes two hooks around the actual interpolated prey.
    for(int side:{-1,1}){
        const Vec fork{end.x+forward.x*(pulling?7.0f:10.0f)+normal.x*float(side)*(pulling?7.0f:4.0f),
                       end.y+forward.y*(pulling?7.0f:10.0f)+normal.y*float(side)*(pulling?7.0f:4.0f)};
        for(int offset=-1;offset<=1;++offset)worldLine(rounded(end.x+normal.x*float(offset)),rounded(end.y+normal.y*float(offset)),
            rounded(fork.x+normal.x*float(offset)),rounded(fork.y+normal.y*float(offset)),{229,65,114,255});
        worldLine(rounded(end.x),rounded(end.y),rounded(fork.x),rounded(fork.y),{255,193,213,255});
        if(pulling)worldLine(rounded(fork.x),rounded(fork.y),rounded(fork.x-forward.x*4+normal.x*float(side)*2),
            rounded(fork.y-forward.y*4+normal.y*float(side)*2),{255,167,198,255});
    }
}

void reckoningBite(Vec center,int radius,float age,float duration,float angle) {
    const float t=std::clamp(age/std::max(.01f,duration),0.0f,1.0f);
    if(t>=1)return;
    const int x=rounded(center.x),y=rounded(center.y),ring=std::max(1,rounded(float(radius)*t));
    const auto fade=static_cast<unsigned char>((1-t)*170);
    disc(x,y,ring,{184,34,86,static_cast<unsigned char>((1-t)*10)});
    worldRing(x,y,ring,{255,222,234,fade});
    if(ring>3)worldRing(x,y,ring-2,{226,62,113,static_cast<unsigned char>(fade/3)});
    if(t<.25f){const float flash=1-t/.25f;const int r=3+rounded((1-flash)*30);
        disc(x,y,r,{255,222,238,static_cast<unsigned char>(flash*16)});
        disc(x,y,std::max(2,r/4),{255,245,249,static_cast<unsigned char>(flash*180)});
    }
    float opening;
    if(t<.15f){const float close=t/.15f;opening=(1-close*close)*1.1f;}
    else if(t<.45f)opening=-.05f*std::sin((t-.15f)/.3f*3.141593f);
    else {const float release=(t-.45f)/.55f;opening=release*release*.4f;}
    const float opacity=t<.45f?1.0f:(1-t)/.55f;
    reckoningJaws().draw(x,y,angle,opening,opacity);
    if(t>.1f && t<.45f){const float burst=(t-.1f)/.35f;
        const auto alpha=static_cast<unsigned char>((1-burst)*255);
        const Vec impact{center.x+std::cos(angle)*24,center.y+std::sin(angle)*24};
        for(int i=0;i<10;++i){const float a=angle+float(i)*.62831853f;
            const float c=std::cos(a),s=std::sin(a),distance=7+burst*27;
            const int sx=rounded(impact.x+c*distance),sy=rounded(impact.y+s*distance);
            worldLine(sx-rounded(c*4),sy-rounded(s*4),sx,sy,{245,110,156,alpha});
            rasterRectangle(sx,sy,i%3==0?2:1,i%3==0?2:1,{255,247,248,alpha});
        }
    }
}
// Fixed motif counts keep the additional detail independent of effect area.
// These positions are visual functions of actor age; rendering never uses RNG.
void venomPool(int x,int y,int radius,float age,bool miasma) {
    disc(x,y,radius,{8,48,34,75});
    disc(x,y,std::max(1,radius-3),{14,118,55,32});
    worldRing(x,y,radius,{28,106,64,190});
    worldRing(x,y,std::max(1,radius-2),{93,231,102,115});
    for(int i=0;i<(miasma?32:24);++i){
        const float angle=float(i)*2.399963f;
        const float distance=float(radius)*(.3f+.62f*float((i*7)%11)/10);
        const int bx=x+rounded(std::cos(angle)*distance),by=y+rounded(std::sin(angle)*distance);
        const float pulse=age*1.5f+float(i)*.37f;
        const int r=2+(int(pulse*3)%4);
        rasterRectangle(bx-r-2,by,r*2+6,2,{25,93,51,130});
        disc(bx,by,r,{26,96,60,175});worldRing(bx,by,r,{75,196,91,190});
        rasterRectangle(bx-1,by-r,3,1,{199,255,150,220});
        rasterRectangle(bx-r,by-1,1,2,{118,229,111,200});
        rasterRectangle(bx+r+3,by+2,3,1,{71,163,80,155});
        rasterPixel(bx+r+5,by-3,{148,227,123,155});
        if(miasma && i%3==0){rasterRectangle(bx+3,by-7,3,2,{48,151,93,150});rasterRectangle(bx+5,by-11,2,2,{128,221,118,130});}
    }
    // Broken foam shelves follow the edge instead of a uniformly smooth halo.
    for(int i=0;i<12;++i){const float a=float(i)*.523599f;const float c=std::cos(a),s=std::sin(a);
        const int bx=x+rounded(c*float(radius-3)),by=y+rounded(s*float(radius-3));
        rasterRectangle(bx-3,by-1,7,2,{59,167,82,170});rasterRectangle(bx-2,by-2,4,1,{145,231,123,180});
    }
}
void singularityArt(int x,int y,int radius,float age) {
    disc(x,y,radius,{45,17,78,80});
    worldRing(x,y,radius,{112,57,171,150});
    const Vec center{float(x),float(y)};
    for(int arm=0;arm<3;++arm){
        const float phase=age*1.8f+float(arm)*2.094395f;
        Vec previous=center;
        for(int i=1;i<=22;++i){const float t=float(i)/22,angle=phase+t*4.7f;
            const Vec next{float(x)+std::cos(angle)*float(radius)*t,float(y)+std::sin(angle)*float(radius)*t*.68f};
            const float tangent=angle+1.570796f;
            const float nx=std::cos(tangent),ny=std::sin(tangent);
            for(int width=-2;width<=2;++width)worldLine(rounded(previous.x+nx*float(width)),rounded(previous.y+ny*float(width)),rounded(next.x+nx*float(width)),rounded(next.y+ny*float(width)),std::abs(width)==2?Color{83,39,138,200}:Color{146,83,213,220});
            if(i%3!=0)worldLine(rounded(previous.x),rounded(previous.y),rounded(next.x),rounded(next.y),i%4==0?Color{239,203,255,245}:Color{180,120,237,235});
            if(i%3==0){rasterRectangle(rounded(next.x)-2,rounded(next.y)-2,4,4,{116,61,181,210});rasterRectangle(rounded(next.x)-1,rounded(next.y)-2,3,1,{221,170,255,245});}
            previous=next;
        }
        arc(center,float(radius)*.82f,phase,1.1f,{174,108,242,230});
        arc(center,float(radius)*.82f-2,phase,.7f,{106,62,172,210});
        glint(x+rounded(std::cos(phase)*float(radius)*.82f),y+rounded(std::sin(phase)*float(radius)*.82f),2,{205,145,255,255});
    }
    const int core=std::max(3,radius/6);
    disc(x,y,core+3,{91,47,156,230});disc(x,y,core+1,{202,143,255,245});disc(x,y,core,{7,4,19,255});
    arc(center,float(core+1),age*2,2.2f,{247,221,255,255},10);
}
void novaArt(int x,int y,int radius,float fade,float age,bool miasma) {
    if(miasma){venomPool(x,y,radius,age,true);return;}
    const auto alpha=static_cast<unsigned char>(std::clamp(fade*255,0.0f,255.0f));
    disc(x,y,radius,{22,123,64,12});
    worldRing(x,y,radius,{39,153,84,alpha});
    worldRing(x,y,std::max(1,radius-2),{175,255,169,alpha});
    worldRing(x,y,std::max(1,radius-5),{47,209,107,static_cast<unsigned char>(alpha/2)});
    for(int i=0;i<16;++i){const float a=float(i)*.392699f;const float c=std::cos(a),s=std::sin(a);
        const int reach=4+(i%3)*2;
        worldLine(x+rounded(c*float(std::max(1,radius-reach))),y+rounded(s*float(std::max(1,radius-reach))),x+rounded(c*float(radius+2)),y+rounded(s*float(radius+2)),{139,255,144,alpha});
        if(i%2==0)rasterRectangle(x+rounded(c*float(radius-1))-1,y+rounded(s*float(radius-1))-1,2,2,{230,255,204,alpha});
    }
}
void playerFace(Vec center, Vec direction, int character) {
    const int x = rounded(center.x), y = rounded(center.y);
    const int dx = rounded(direction.x), dy = rounded(direction.y), px = -dy, py = dx;
    const int ex1 = x + dx * 2 + px * 4, ey1 = y + dy * 2 + py * 4;
    const int ex2 = x + dx * 2 - px * 4, ey2 = y + dy * 2 - py * 4;
    if (character == 1) {
        const int eyeWidth = dx ? 2 : 4, eyeHeight = dx ? 4 : 2;
        rasterRectangle(ex1 - eyeWidth / 2, ey1 - eyeHeight / 2, eyeWidth, eyeHeight, {255, 170, 0, 255});
        rasterRectangle(ex2 - eyeWidth / 2, ey2 - eyeHeight / 2, eyeWidth, eyeHeight, {255, 170, 0, 255});
        rasterPixel(ex1, ey1, {17, 17, 17, 255});
        rasterPixel(ex2, ey2, {17, 17, 17, 255});
    } else if (character == 2) {
        rasterRectangle(ex1 - 1, ey1 - 1, 2, 2, {255, 51, 51, 255});
        rasterRectangle(ex2 - 1, ey2 - 1, 2, 2, {255, 51, 51, 255});
        rasterLine(x + dx * 4, y + dy * 4, x + dx * 11, y + dy * 11, {255, 34, 34, 255});
        rasterLine(x + dx * 11, y + dy * 11, x + dx * 14 + px * 2, y + dy * 14 + py * 2, {255, 34, 34, 255});
        rasterLine(x + dx * 11, y + dy * 11, x + dx * 14 - px * 2, y + dy * 14 - py * 2, {255, 34, 34, 255});
    } else if (character == 3) {
        for (const Vec eye : {Vec{float(ex1), float(ey1)}, Vec{float(ex2), float(ey2)}}) {
            rasterPixel(rounded(eye.x), rounded(eye.y) - 1, {68, 255, 68, 255});
            rasterRectangle(rounded(eye.x) - 1, rounded(eye.y), 3, 1, {68, 255, 68, 255});
            rasterPixel(rounded(eye.x), rounded(eye.y) + 1, {68, 255, 68, 255});
            rasterPixel(rounded(eye.x), rounded(eye.y), {0, 51, 0, 255});
        }
        // Keep the basilisk's venom mark on the head's last interior pixel.
        // The old +7 positive edge was outside a 14px block and looked like a
        // detached random pixel when the face rotated through a corner.
        rasterPixel(x + dx * 5, y + dy * 5, {0, 255, 0, 255});
    } else {
        rasterRectangle(ex1 - 1, ey1 - 1, 2, 2, {17, 17, 17, 255});
        rasterRectangle(ex2 - 1, ey2 - 1, 2, 2, {17, 17, 17, 255});
    }
}
void enemyFace(Vec center, Vec velocity, bool boss) {
    const Vec direction = cardinal(velocity);
    const int x = rounded(center.x + direction.x * 2), y = rounded(center.y + direction.y * 2);
    const int px = -rounded(direction.y), py = rounded(direction.x);
    const Color color = boss ? Color{255, 255, 0, 255} : WHITE;
    const int spread = boss ? 4 : 3, size = boss ? 3 : 2;
    rasterRectangle(x + px * spread - size / 2, y + py * spread - size / 2, size, size, color);
    rasterRectangle(x - px * spread - size / 2, y - py * spread - size / 2, size, size, color);
}
}

void prepareWorldEffects(){(void)circleCache();(void)reckoningJaws();}

void Presentation::world(bool backdrop,float deathPreviewTime) {
    RasterFrame rasterFrame(worldRaster,viewWidth,viewHeight);
    const auto playerPose=motionHistory.samplePlayer(game.player,renderFraction,interpolateMotion && !backdrop);
    const auto& previousPlayer=playerPose.previous;const auto& currentPlayer=playerPose.current;
    const std::size_t playerCount=playerPose.count;
    const float t=backdrop?1.0f:playerPose.alpha;
    const float renderOffset=interpolateMotion?(renderFraction-1)*FixedStep:0;
    const float effectTime=std::max(0.0f,game.elapsed+renderOffset);
    const bool playerDeath=!backdrop && game.phase==Phase::Dead && game.playerDied;
    const float deathAge=playerDeath?(deathPreviewTime>=0?deathPreviewTime:deathSeconds):0;
    auto playerGeometry=geometry::buildSnake(previousPlayer.data(),currentPlayer.data(),playerCount,t,PlayerSize,WorldScale);
    const bool playerPathValid=playerGeometry.valid;
    if(!playerGeometry.valid)playerGeometry=geometry::buildSnake(currentPlayer.data(),currentPlayer.data(),playerCount,1,PlayerSize,WorldScale);
    const geometry::Point headWorld=playerGeometry.count>0?playerGeometry.centers[0]:geometry::Point{rounded(game.player.position.x*WorldScale),rounded(game.player.position.y*WorldScale)};
    const int worldExtent=rounded(ArenaSize*WorldScale);
    const auto camera=[&](double head,int size){return size>=worldExtent?double((worldExtent-size)/2):std::clamp(head-size/2,0.0,double(worldExtent-size));};
    const int cameraX=int(camera(headWorld.x,rasterWidth)),cameraY=int(camera(headWorld.y,rasterHeight));
    geometry::PixelOrigin pixelOrigin;
    pixelOrigin.relative=interpolateMotion && !backdrop;
    if(pixelOrigin.relative && playerCount){
        const auto before=playerPathValid?previousPlayer[0]:currentPlayer[0],after=currentPlayer[0];
        const double amount=playerPathValid?double(t):1;
        const double x=(double(before.x)+(double(after.x)-before.x)*amount)*WorldScale;
        const double y=(double(before.y)+(double(after.y)-before.y)*amount)*WorldScale;
        pixelOrigin.offset={float(cameraX-camera(x,rasterWidth)),float(cameraY-camera(y,rasterHeight))};
        playerGeometry=geometry::buildSnake(previousPlayer.data(),currentPlayer.data(),playerCount,t,PlayerSize,WorldScale,0,0,pixelOrigin);
        if(!playerGeometry.valid)playerGeometry=geometry::buildSnake(currentPlayer.data(),currentPlayer.data(),playerCount,1,PlayerSize,WorldScale,0,0,pixelOrigin);
    }
    const auto screenPosition = [&](Vec position) -> Vec {
        if(pixelOrigin.relative)return {float(relativePixel(double(position.x)*WorldScale+pixelOrigin.offset.x)-cameraX),
                                        float(relativePixel(double(position.y)*WorldScale+pixelOrigin.offset.y)-cameraY)};
        return {float(rounded(position.x * WorldScale) - cameraX), float(rounded(position.y * WorldScale) - cameraY)};
    };
    const auto visible = [](Vec position, int padding = 10) {
        return position.x >= -padding && position.y >= -padding && position.x <= rasterWidth + padding && position.y <= rasterHeight + padding;
    };

    BeginScissorMode(0,0,rasterWidth*pixelScale,rasterHeight*pixelScale);
    rasterRectangle(0, 0, rasterWidth, rasterHeight, BLACK);
    if(cameraX==0 || cameraY==0 || cameraX==worldExtent-rasterWidth || cameraY==worldExtent-rasterHeight){
        rasterFlush();outline(-cameraX,-cameraY,worldExtent,worldExtent,WHITE);
    }

    if(!backdrop && !game.snakeDeaths.empty()){
        rasterFlush();
        for(const auto& death:game.snakeDeaths){
            if(!death.count || !visible(screenPosition(death.current[0]),220))continue;
            const float age=std::max(0.0f,death.age+renderOffset)+deathAge;
            // Successful splits hand their material to the two live offspring.
            // A second dissolving corpse would hide that continuous transition.
            if(death.splitChildren && age>=SplitBirthFlash)continue;
            const bool boss=death.kind==SnakeDeathKind::Boss;
            const int width=boss?15:death.kind==SnakeDeathKind::Hatchling?7:death.kind==SnakeDeathKind::Hydra?8:11;
            const auto pose=motionHistory.sampleDeath(death,renderFraction,interpolateMotion);
            auto shape=geometry::buildSnake(pose.previous.data(),pose.current.data(),pose.count,pose.alpha,width,WorldScale,0,0,pixelOrigin);
            if(!shape.valid)shape=geometry::buildSnake(pose.current.data(),pose.current.data(),pose.count,1,width,WorldScale,0,0,pixelOrigin);
            const Color color=boss?BossPurple:death.kind==SnakeDeathKind::Splitter?Color{34,102,204,255}:death.kind==SnakeDeathKind::Hatchling?Color{0,255,0,255}:death.kind==SnakeDeathKind::Hydra?BossPurple:Color{204,0,0,255};
            deathArt.draw(shape,color,age,snakeDeathDuration(death.kind),death.id,
                          cameraX-rounded(pose.offset.x*WorldScale),cameraY-rounded(pose.offset.y*WorldScale),settings.render.particles);
        }
        deathArt.flush();
    }

    for (const Pickup& pickup : game.pickups) {
        const Vec position = screenPosition(interpolateMotion && pickup.motionReady?interpolate(pickup.previous,pickup.position,renderFraction):pickup.position);
        if (!visible(position)) continue;
        if (pickup.kind == PickupKind::Experience) block(position, 8, WHITE);
        else if (pickup.kind == PickupKind::Heart){rasterFlush();art.draw(pixel::Sprite::Heart,rounded(position.x)-4,rounded(position.y)-4);}
        else {
            rasterFlush();
            art.chest(rounded(position.x),rounded(position.y));
        }
    }

    for (const Zone& zone : game.zones) {
        if (!settings.render.weaponEffects || zone.delay>0 || zone.pattern == Pattern::Beam || zone.pattern == Pattern::Lash) continue;
        const Vec position = screenPosition(zone.position);
        const float expansion=zone.pattern==Pattern::Nova && !zone.evolved?std::clamp((zone.maxLife-zone.life)/.25f,0.0f,1.0f):1.0f;
        const int x = rounded(position.x), y = rounded(position.y), radius = std::max(1, rounded(zone.radius * WorldScale*expansion));
        if (!visible(position, radius + 8)) continue;
        const bool singularity = zone.pattern == Pattern::Mortar && zone.evolved;
        if (zone.pattern == Pattern::Mortar || zone.pattern == Pattern::Pit) {
            if(singularity)singularityArt(x,y,radius,zone.maxLife-zone.life+renderOffset);
            else venomPool(x,y,radius,zone.maxLife-zone.life+renderOffset,false);
        } else if (zone.pattern == Pattern::Shot && zone.evolved) {
            // The actual 44-fang Ouroboros orbit is rendered below, rather than
            // drawing an invented circular aura in place of the weapon.
            continue;
        } else {
            const float fade = std::clamp(zone.life / std::max(.01f, zone.maxLife), 0.0f, 1.0f);
            novaArt(x,y,radius,fade,effectTime,zone.evolved);
        }
        if (zone.pattern == Pattern::Pit) {
            rasterFlush();
            art.draw(pixel::Sprite::Cobra, x - 4, y - 4);
            if (zone.evolved) {
                art.draw(pixel::Sprite::Cobra, x - 11, y);
                art.draw(pixel::Sprite::Cobra, x + 3, y);
            }
        }
    }

    if(settings.render.weaponEffects)for(const CobraPit& pit:game.cobraPits){const Vec p=screenPosition(pit.position);if(!visible(p,30))continue;
        const float fade=std::clamp((pit.life-pit.age)/std::max(.01f,pit.life),0.0f,1.0f);
        disc(rounded(p.x),rounded(p.y),24,{80,60,20,static_cast<unsigned char>(38*fade)});
        disc(rounded(p.x),rounded(p.y),17,{50,40,15,static_cast<unsigned char>(90*fade)});
        disc(rounded(p.x),rounded(p.y),10,{20,15,5,static_cast<unsigned char>(178*fade)});
        for(int stone=0;stone<10;++stone){const float a=float(stone)*.628319f;
            const int sx=rounded(p.x+std::cos(a)*19),sy=rounded(p.y+std::sin(a)*12);
            rasterRectangle(sx-2,sy-1,5,3,{61,71,45,static_cast<unsigned char>(210*fade)});
            rasterRectangle(sx-2,sy-2,4,1,{143,167,83,static_cast<unsigned char>(210*fade)});
        }
    }
    Vec leviathanMouth{};
    if(settings.render.weaponEffects && game.leviathan.active){const Vec p=screenPosition(game.leviathan.position);const float scale=1.5f+.2f*float(game.leviathan.growth);const int r=rounded(24*scale);
        if(visible(p,r+150)){disc(rounded(p.x),rounded(p.y),r,{80,55,20,65});disc(rounded(p.x),rounded(p.y),r/2,{40,28,10,150});
            for(int i=0;i<12;++i){const float a=float(i)*.523599f;const int gx=rounded(p.x+std::cos(a)*float(r)),gy=rounded(p.y+std::sin(a)*float(r)*.55f);rasterRectangle(gx-2,gy-1,5,3,{158,111,41,220});rasterPixel(gx-1,gy-2,{255,216,112,255});}
            if(game.leviathan.absorbFlash>0)worldRing(rounded(p.x),rounded(p.y),r+rounded((.6f-game.leviathan.absorbFlash)*20),{255,200,60,180});}
    }

    for(const auto& snake:game.greySnakes.trails){
        if(!snake.count)continue;
        const Vec minimum=screenPosition(snake.minimum),maximum=screenPosition(snake.maximum);
        if(maximum.x<-PlayerSize || maximum.y<-PlayerSize || minimum.x>rasterWidth+PlayerSize || minimum.y>rasterHeight+PlayerSize)continue;
        const auto pose=motionHistory.sampleGrey(snake,renderFraction,interpolateMotion && !backdrop);
        if(!pose.count)continue;
        const Color color=pose.alive?Color{102,102,102,255}:Color{85,85,85,255};
        // The historical path stays fixed. Interpolate just the new head edge,
        // then quantize it with the same camera origin as every other actor.
        const Vec head=screenPosition(interpolate(pose.previous,pose.current,pose.alpha));
        Vec previous=pose.count==1?head:screenPosition(snake.path[0]);
        block(previous,PlayerSize,color);
        for(std::size_t i=1;i<pose.count;++i){
            const Vec p=i+1==pose.count?head:screenPosition(snake.path[i]);
            const int x=rounded(std::min(previous.x,p.x))-PlayerSize/2,y=rounded(std::min(previous.y,p.y))-PlayerSize/2;
            rasterRectangle(x,y,rounded(std::abs(p.x-previous.x))+PlayerSize,rounded(std::abs(p.y-previous.y))+PlayerSize,color);
            previous=p;
        }
        if(pose.alive){
            const Vec d=pose.direction;
            for(int side:{-1,1}){
                const int x=rounded(head.x+d.x*2.1f-d.y*3.5f*float(side));
                const int y=rounded(head.y+d.y*2.1f+d.x*3.5f*float(side));
                rasterRectangle(x-1,y-1,3,3,{238,238,238,255});
                rasterRectangle(x-1,y-1,2,2,{17,17,17,255});
            }
        }
    }

    for (const Enemy& enemy : game.enemies) {
        if (!enemy.alive) continue;
        if(!visible(screenPosition(enemy.position),200))continue;
        const auto pose=motionHistory.sampleEnemy(enemy,renderFraction,interpolateMotion);
        const float enemyT=pose.alpha;
        const bool boss=enemy.kind==EnemyKind::Boss;
        const int size=boss?15:11;
        const Color color=boss?BossPurple:enemy.kind==EnemyKind::Splitter?Color{34,102,204,255}:Color{204,0,0,255};
        const Color bodyColor=boss?Color{119,34,170,255}:color;
        const auto& current=pose.current;const auto& previous=pose.previous;
        const auto& shape=enemyGeometry.sample(enemy.id,previous.data(),current.data(),pose.count,enemyT,size,pixelOrigin);
        if(shape.count==0)continue;
        const int originX=cameraX-rounded(pose.offset.x*WorldScale),originY=cameraY-rounded(pose.offset.y*WorldScale);
        Vec position{float(shape.centers[0].x-originX),float(shape.centers[0].y-originY)};
        if(!visible(position,180))continue;
        bool face=true;int headSize=size;Vec faceDirection=pose.direction;
        const float birthAge=std::max(0.0f,enemy.splitBirth.age+renderOffset)+deathAge;
        if(enemy.splitBirth.count && birthAge<SplitBirthFlash)continue;
        if(enemy.splitBirth.count && birthAge<SplitBirthDuration){
            const auto birthShape=geometry::buildSplitBirth(enemy.splitBirth,shape,birthAge,pixelOrigin,
                {cameraX-originX,cameraY-originY});
            if(birthShape.valid && birthShape.count){
                const float amount=geometry::splitBirthProgress(birthAge);
                const auto channel=[&](int before,int after){return static_cast<unsigned char>(rounded(float(before)+float(after-before)*amount));};
                const Color material{channel(34,bodyColor.r),channel(102,bodyColor.g),channel(204,bodyColor.b),255};
                drawSnake(birthShape,material,cameraX,cameraY);
                position={float(birthShape.centers[0].x-cameraX),float(birthShape.centers[0].y-cameraY)};
                headSize=rounded(5+6*amount);face=amount>=.35f;
                if(amount<.65f)faceDirection={float(enemy.splitBirth.normalY),float(-enemy.splitBirth.normalX)};
            }else drawSnake(shape,bodyColor,originX,originY);
        }else drawSnake(shape,bodyColor,originX,originY);
        // The opaque body already includes the head square. Ordinary snakes
        // only need a second head layer while flashing; bosses have a distinct
        // head color and retain that layer.
        if(boss || enemy.hitFlash>0)block(position,headSize,enemy.hitFlash>0?Color{255,85,85,255}:color);
        if(face)enemyFace(position, faceDirection, boss);
        if (boss) {
            const int barX = rounded(position.x) - 15, barY = rounded(position.y) - 14;
            rasterRectangle(barX - 1, barY - 1, 32, 5, {0, 0, 0, 160});
            rasterRectangle(barX, barY, 30, 3, {68, 0, 102, 255});
            rasterRectangle(barX, barY, rounded(float(30 * std::clamp(enemy.hp / enemy.maxHp, 0.0, 1.0))), 3, BossPurple);
        }
    }

    Color playerColor=WHITE;
    if(game.player.invulnerable>0 && int(visualTime*12)%2==0)playerColor={255,68,68,255};
    if(!playerDeath && game.shieldCharges>0){auto shield=geometry::buildSnake(previousPlayer.data(),currentPlayer.data(),playerCount,t,PlayerSize+4,WorldScale,0,0,pixelOrigin);if(!shield.valid)shield=geometry::buildSnake(currentPlayer.data(),currentPlayer.data(),playerCount,1,PlayerSize+4,WorldScale,0,0,pixelOrigin);drawShield(shield,cameraX,cameraY);
    }
    if(playerDeath){
        constexpr Color palettes[]{{226,236,225,255},{247,215,166,255},{231,176,187,255},{169,226,173,255}};
        rasterFlush();deathArt.draw(playerGeometry,palettes[std::clamp(game.character,0,3)],deathAge,.72f,game.seed,cameraX,cameraY,settings.render.particles);deathArt.flush();
    }else drawSnake(playerGeometry,playerColor,cameraX,cameraY);
    const Vec headScreen{float(headWorld.x-cameraX),float(headWorld.y-cameraY)};
    if(!playerDeath && playerGeometry.count>0)playerFace(headScreen,playerPose.direction,game.character);

    for(const Cobra& cobra:game.cobras){const Vec p=screenPosition(cobra.position);const float age=std::max(0.0f,cobra.age+renderOffset);if(visible(p,50))standingCobra(p,age,cobra.life-age,1,false,false);}
    if(game.leviathan.active){const Vec p=screenPosition(game.leviathan.position);const float scale=1.5f+.2f*float(game.leviathan.growth);leviathanMouth=standingCobra(p,std::max(0.0f,game.leviathan.age+renderOffset),1,scale,true,game.leviathan.absorbFlash>0);}

    for(const NestEgg& egg:game.nestEggs) {
        const float eggAge=std::max(0.0f,egg.age+renderOffset);
        const float flight=std::clamp(eggAge/std::max(.01f,egg.flightDuration),0.0f,1.0f);
        Vec ground=interpolate(egg.start,egg.target,flight);
        Vec position=screenPosition(ground);
        if(flight<1) {
            if(settings.render.weaponEffects && visible(position,12))rasterRectangle(rounded(position.x)-3,rounded(position.y)+3,7,2,egg.evolved?Color{46,25,67,255}:Color{65,45,0,255});
            position.y-=6*Tile*WorldScale*4*flight*(1-flight);
        }else{
            const float hatch=std::clamp((eggAge-egg.flightDuration)/std::max(.01f,egg.hatchDelay),0.0f,1.0f);
            if(settings.render.weaponEffects && visible(position,18))worldRing(rounded(position.x),rounded(position.y),4+rounded(hatch*12),egg.evolved?Color{170,100,238,180}:Color{180,130,25,180});
        }
        if(visible(position,12)){
            if(settings.render.weaponEffects && eggAge>=egg.flightDuration){
                const float hatch=std::clamp((eggAge-egg.flightDuration)/std::max(.01f,egg.hatchDelay),0.0f,1.0f);
                for(int i=0;i<5;++i){const float a=float(i)*1.256637f;
                    rasterRectangle(rounded(position.x+std::cos(a)*(6+hatch*5)),rounded(position.y+std::sin(a)*(4+hatch*4)),2,2,egg.evolved?Color{211,161,255,220}:Color{244,201,103,220});}
            }
            rasterFlush();effectArt.draw(egg.evolved?render::EffectSprite::HydraEgg:render::EffectSprite::Egg,rounded(position.x),rounded(position.y),0,eggAge);
        }
    }
    for(const MiniSnake& mini:game.miniSnakes) {
        if(!mini.alive)continue;
        const auto pose=motionHistory.sampleMini(mini,renderFraction,interpolateMotion);
        const float miniT=pose.alpha;const auto& current=pose.current;const auto& previous=pose.previous;const auto count=pose.count;
        const Color bodyColor=mini.evolved?Color{187,68,255,255}:Color{0,255,0,255};
        const int size=mini.evolved?8:7;
        auto shape=geometry::buildSnake(previous.data(),current.data(),std::size_t(count),miniT,size,WorldScale,cameraX,cameraY,pixelOrigin);
        if(!shape.valid)shape=geometry::buildSnake(current.data(),current.data(),std::size_t(count),1,size,WorldScale,cameraX,cameraY,pixelOrigin);
        if(shape.count==0)continue;
        const Vec tip{float(shape.centers[0].x),float(shape.centers[0].y)};
        if(!visible(tip,100))continue;
        drawSnake(shape,bodyColor);
        // Hydra brood uses a plain purple snake; base hatchlings retain scales.
        if(!mini.evolved)for(std::size_t i=0;i<shape.count;++i){const auto p=shape.centers[i];
            rasterRectangle(p.x-2,p.y-2,3,1,{171,244,112,255});
            rasterRectangle(p.x-1,p.y+2,3,1,{28,145,61,255});
            rasterPixel(p.x+1,p.y,{66,189,70,255});
        }
        const int dx=rounded(pose.direction.x),dy=rounded(pose.direction.y);
        const Color eyeColor=mini.evolved?Color{40,13,58,255}:Color{0,70,0,255};
        rasterPixel(rounded(tip.x)+dx*2-dy*2,rounded(tip.y)+dy*2+dx*2,eyeColor);
        rasterPixel(rounded(tip.x)+dx*2+dy*2,rounded(tip.y)+dy*2-dx*2,eyeColor);
    }

    const auto projectilePosition=[&](const Projectile& projectile){
        const float age=std::max(0.0f,projectile.age+renderOffset);
        Vec ground=interpolateMotion && projectile.motionReady?interpolate(projectile.previousPosition,projectile.position,renderFraction):projectile.position;
        if(projectile.flightDuration>0)ground=interpolate(projectile.start,projectile.target,std::clamp(age/projectile.flightDuration,0.0f,1.0f));
        Vec position=screenPosition(ground);
        if(projectile.flightDuration>0){const float flight=std::clamp(age/projectile.flightDuration,0.0f,1.0f);position.y-=(projectile.pattern==Pattern::Pit?7.0f:5.0f)*Tile*WorldScale*4*flight*(1-flight);}
        return position;
    };
    // Draw all untextured wakes before one contiguous atlas pass. Dense volleys
    // never alternate the raster batch and texture batch per projectile.
    if(settings.render.weaponEffects)for (const Projectile& projectile : game.projectiles) {
        if (!projectile.alive) continue;
        const Vec position=projectilePosition(projectile);
        if (!visible(position,24)) continue;
        const int x = rounded(position.x), y = rounded(position.y);
        const float speed = std::max(1.0f, std::sqrt(projectile.velocity.x * projectile.velocity.x + projectile.velocity.y * projectile.velocity.y));
        const Vec direction{projectile.velocity.x / speed, projectile.velocity.y / speed};
        if(projectile.flightDuration>0){
            const Vec ground=screenPosition(interpolate(projectile.start,projectile.target,std::clamp((projectile.age+renderOffset)/projectile.flightDuration,0.0f,1.0f)));
            rasterRectangle(rounded(ground.x)-4,rounded(ground.y)-1,9,3,{31,55,26,180});
        }else{
            const Color wake=projectile.pattern==Pattern::Ricochet?Color{91,48,146,190}:projectile.pattern==Pattern::Homing?Color{40,97,124,200}:Color{24,94,52,210};
            worldLine(x-rounded(direction.x*18),y-rounded(direction.y*18),x,y,wake);
            for(int i=1;i<=2;++i){const int bx=x-rounded(direction.x*float(7+i*5)),by=y-rounded(direction.y*float(7+i*5));rasterRectangle(bx,by,i==1?2:1,i==1?2:1,wake);}
        }
    }
    rasterFlush();
    for(const Projectile& projectile:game.projectiles){
        if(!projectile.alive)continue;const Vec p=projectilePosition(projectile);if(!visible(p,12))continue;
        using Sprite=render::EffectSprite;
        const Sprite sprite=projectile.pattern==Pattern::Pit?(projectile.evolved?Sprite::HydraEgg:Sprite::Egg):projectile.pattern==Pattern::Mortar?Sprite::Mortar:projectile.gatling?Sprite::Gatling:projectile.pattern==Pattern::Homing?Sprite::HomingFang:projectile.pattern==Pattern::Ricochet?Sprite::RicochetFang:Sprite::Venom;
        effectArt.draw(sprite,rounded(p.x),rounded(p.y),projectile.flightDuration>0?0:std::atan2(projectile.velocity.y,projectile.velocity.x),projectile.age+renderOffset,projectile.critical);
    }

    if(game.ouroborosActive){
        const float time=game.elapsed+renderOffset,area=game.stats().area;
        for(std::size_t i=0;i<game.orbitFangs.size();++i){
            const auto& fang=game.orbitFangStates[i];const float angle=fang.angle+fang.speed*renderOffset;
            Vec p=screenPosition(game.orbitFangs[i]);
            if(interpolateMotion){
                const float radius=(fang.orbit+fang.radiusAmplitude*std::sin(fang.radiusPhase+time*fang.radiusFrequency))*Tile*area*WorldScale;
                const float orbitAngle=angle+fang.angleAmplitude*std::cos(fang.anglePhase+time*fang.angleFrequency);
                p={headScreen.x+std::cos(orbitAngle)*radius,headScreen.y+std::sin(orbitAngle)*radius};
            }
            if(!visible(p,12))continue;
            const auto sprite=fang.size<.70f?render::EffectSprite::OrbitFangSmall:fang.size<.86f?render::EffectSprite::OrbitFangMedium:render::EffectSprite::OrbitFang;
            effectArt.draw(sprite,rounded(p.x),rounded(p.y),angle,time+float(i)*.13f);
        }
    }

    auto beamTarget=[&](std::uint32_t id,Vec fallback){
        for(const Enemy& enemy:game.enemies)if(enemy.id==id){const auto pose=motionHistory.sampleEnemy(enemy,renderFraction,interpolateMotion);const auto& shape=enemyGeometry.sample(enemy.id,pose.previous.data(),pose.current.data(),pose.count,pose.alpha,enemy.kind==EnemyKind::Boss?15:11,pixelOrigin);if(shape.count)return Vec{float(shape.centers[0].x-cameraX+rounded(pose.offset.x*WorldScale)),float(shape.centers[0].y-cameraY+rounded(pose.offset.y*WorldScale))};}return screenPosition(fallback);
    };
    for(const TrackingBeam& beam:game.beams){
        if(beam.life<=0 || beam.target==0)continue;
        const Vec start=beam.ancient?leviathanMouth:headScreen;Vec end=beamTarget(beam.target,beam.position);
        if(beam.ancient){const float dx=end.x-start.x,dy=end.y-start.y,length=std::max(1.0f,std::sqrt(dx*dx+dy*dy));end={start.x+dx/length*beam.range*WorldScale,start.y+dy/length*beam.range*WorldScale};const int half=std::clamp(rounded((.5f+.08f*float(game.leviathan.growth))*Tile*WorldScale),1,26);const float px=-dy/length,py=dx/length;
            for(int offset=-half;offset<=half;++offset)worldLine(rounded(start.x+px*float(offset)),rounded(start.y+py*float(offset)),rounded(end.x+px*float(offset)),rounded(end.y+py*float(offset)),std::abs(offset)<std::max(1,half/3)?Color{255,249,208,245}:std::abs(offset)>half-2?Color{210,113,30,170}:Color{255,185,57,110});
            if(settings.render.weaponEffects){for(int i=1;i<=12;++i){const float fraction=float(i)/13;const Vec p{start.x+(end.x-start.x)*fraction,start.y+(end.y-start.y)*fraction};worldLine(rounded(p.x-px*float(half)),rounded(p.y-py*float(half)),rounded(p.x+px*float(half)),rounded(p.y+py*float(half)),{255,221,121,100});}glint(rounded(start.x),rounded(start.y),5,{255,210,92,255});}
        }else if(!settings.render.weaponEffects)worldLine(rounded(start.x),rounded(start.y),rounded(end.x),rounded(end.y),beam.evolved?Color{255,136,68,255}:Color{204,255,255,255});
        else if(beam.evolved)energyPath(start,end,{181,65,47,255},{255,193,113,255},effectTime*6,true);
        else energyPath(start,end,{30,128,172,255},{208,255,249,255},effectTime*8,false);
        if(settings.render.weaponEffects && !beam.ancient){glint(rounded(end.x),rounded(end.y),3,beam.evolved?Color{255,158,91,255}:Color{111,237,255,255});glint(rounded(start.x),rounded(start.y),2,beam.evolved?Color{255,193,113,255}:Color{208,255,249,255});}
    }
    for(const Grapple& tongue:game.grapples){if(tongue.phase>=2)continue;const Vec end=tongue.phase==1?beamTarget(tongue.target,tongue.position):screenPosition({tongue.position.x+tongue.direction.x*40*Tile*renderOffset,tongue.position.y+tongue.direction.y*40*Tile*renderOffset});
        reckoningTongue(headScreen,end,effectTime,tongue.target,tongue.phase==1,settings.render.weaponEffects);
    }

    for (const Zone& zone : game.zones) {
        if (!settings.render.weaponEffects || zone.delay>0 || (zone.pattern != Pattern::Beam && zone.pattern != Pattern::Lash)) continue;
        if(zone.pattern==Pattern::Lash && zone.evolved){const Vec p=screenPosition(zone.position);const int radius=std::max(1,rounded(zone.radius*WorldScale));
            if(visible(p,std::max(radius,ReckoningJaws::Extent)))reckoningBite(p,radius,std::max(0.0f,zone.maxLife-zone.life+renderOffset),zone.maxLife,zone.phase);
            continue;
        }
        const Vec end = screenPosition(zone.position);
        const int hx = rounded(headScreen.x), hy = rounded(headScreen.y), ex = rounded(end.x), ey = rounded(end.y);
        if (zone.pattern == Pattern::Beam) {
            worldLine(hx, hy - 1, ex, ey - 1, {34, 136, 204, 255});
            worldLine(hx, hy, ex, ey, {204, 255, 255, 255});
            worldLine(hx, hy + 1, ex, ey + 1, {68, 221, 255, 255});
        } else {
            const float distance = std::max(1.0f, std::sqrt((end.x - headScreen.x) * (end.x - headScreen.x) + (end.y - headScreen.y) * (end.y - headScreen.y)));
            const float dx = (end.x - headScreen.x) / distance, dy = (end.y - headScreen.y) / distance;
            worldLine(hx,hy-1,ex,ey-1,{100,22,52,255});
            worldLine(hx, hy, ex, ey, {247,100,139,255});
            worldLine(hx,hy+1,ex,ey+1,{167,39,80,255});
            worldLine(ex, ey, rounded(end.x + dx * 7 - dy * 4), rounded(end.y + dy * 7 + dx * 4), {255, 100, 130, 255});
            worldLine(ex, ey, rounded(end.x + dx * 7 + dy * 4), rounded(end.y + dy * 7 - dx * 4), {255, 100, 130, 255});
        }
    }

    constexpr Color ParticleColors[]{Venom, {255, 68, 68, 255}, {0, 255, 255, 255}, {255, 215, 0, 255}, BossPurple, WHITE};
    const std::size_t particleBudget=settings.render.quality==0?200:settings.render.quality==1?600:game.particles.size();
    std::size_t particleCount=0;
    if(settings.render.particles)for (const Particle& particle : game.particles) {
        if(particle.snakeDeath)continue;
        if(particleCount++>=particleBudget)break;
        const Vec position = screenPosition(interpolateMotion && particle.motionReady?interpolate(particle.previous,particle.position,renderFraction):particle.position);
        if (visible(position)){
            const int x=rounded(position.x),y=rounded(position.y),color=std::abs(particle.color)%6;
            const float fade=std::clamp(particle.life/std::max(.01f,particle.maxLife),0.0f,1.0f);
            const Color base=ParticleColors[color];
            if(fade>.65f){rasterRectangle(x,y,2,2,base);rasterPixel(x,y,{228,255,208,255});}
            else if(fade>.3f)rasterPixel(x,y,base);
            else rasterPixel(x,y,{base.r,base.g,base.b,static_cast<unsigned char>(fade*650)});
        }
    }
    const std::size_t numberBudget=settings.render.quality==0?40:settings.render.quality==1?90:game.damageNumbers.size();
    std::size_t numberCount=0;
    if(settings.render.damageNumbers)for (const DamageNumber& damage : game.damageNumbers) {
        if(numberCount++>=numberBudget)break;
        const Vec position = screenPosition({damage.position.x,damage.position.y-12*renderOffset});
        if (!visible(position)) continue;
        const std::string text = std::to_string(damage.value);
        const int x = rounded(position.x) - pixel::width(text) / 2, y = rounded(position.y) - 4;
        rasterFlush();
        pixel::text(text, x + 1, y + 1, BLACK);
        pixel::text(text, x, y, damage.critical ? Color{255, 255, 0, 255} : WHITE);
    }
    rasterFlush();EndScissorMode();
}
}
