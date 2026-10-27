#include "snake_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace snake::geometry {
namespace {
bool equal(Point a,Point b) {return a.x==b.x && a.y==b.y;}
bool axisAligned(Point a,Point b) {return a.x==b.x || a.y==b.y;}
int sign(int value) {return value>0?1:value<0?-1:0;}
int distance(Point a,Point b) {return std::abs(a.x-b.x)+std::abs(a.y-b.y);}
bool project(Vec input,float scale,int cameraX,int cameraY,Point& result) {
    const double x=double(input.x)*scale,y=double(input.y)*scale;
    constexpr double limit=double(std::numeric_limits<int>::max()/8);
    if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x)>limit || std::abs(y)>limit)return false;
    // Coordinates are already finite and safely inside int range. Truncating
    // after the signed half-pixel bias is the same ties-away rounding without
    // two out-of-line CRT lround calls for every body waypoint on MSVC.
    result={int(x+(x<0?-.5:.5))-cameraX,int(y+(y<0?-.5:.5))-cameraY};return true;
}
template<std::size_t N> void append(BasicSnakeGeometry<N>& output,Point a,Point b,int width) {
    if(equal(a,b))return;
    Rect next{std::min(a.x,b.x)-width/2,std::min(a.y,b.y)-width/2,
              std::abs(a.x-b.x)+width,std::abs(a.y-b.y)+width};
    if(output.rectangleCount>0){
        auto& previous=output.rectangles[output.rectangleCount-1];
        // Co-linear body sections become one span. Besides fewer draw calls,
        // this ensures every straight run has precisely the same outer edge.
        if(previous.y==next.y && previous.height==width && next.height==width &&
           previous.x<=next.x+next.width && next.x<=previous.x+previous.width){
            const int end=std::max(previous.x+previous.width,next.x+next.width);
            previous.x=std::min(previous.x,next.x);previous.width=end-previous.x;return;
        }
        if(previous.x==next.x && previous.width==width && next.width==width &&
           previous.y<=next.y+next.height && next.y<=previous.y+previous.height){
            const int end=std::max(previous.y+previous.height,next.y+next.height);
            previous.y=std::min(previous.y,next.y);previous.height=end-previous.y;return;
        }
    }
    if(output.rectangleCount>=output.rectangles.size()){output.valid=false;return;}
    output.rectangles[output.rectangleCount++]=next;
}
template<std::size_t N> struct Path {
    std::array<Point,N> from,directions;
    std::size_t count=0;
    int step=0;
    bool valid=true;
};
template<std::size_t N> Path<N> prepare(const Vec* previous,const Vec* current,std::size_t count,float scale,int cameraX,int cameraY) {
    Path<N> path;
    if(count==0)return path;
    if(!previous || !current || count>N || !std::isfinite(scale) || scale<=0){path.valid=false;return path;}
    auto& from=path.from;
    for(std::size_t i=0;i<count;++i){
        Point to;
        if(!project(previous[i],scale,cameraX,cameraY,from[i]) ||
           !project(current[i],scale,cameraX,cameraY,to) || !axisAligned(from[i],to)){
            path.valid=false;return path;
        }
        const int moved=distance(from[i],to);
        if(moved>0){if(path.step!=0 && path.step!=moved){path.valid=false;return path;}path.step=moved;}
        path.directions[i]={sign(to.x-from[i].x),sign(to.y-from[i].y)};
    }
    path.count=count;return path;
}
// +X, -X, +Y, -Y advances and the stationary grid origin. One continuous
// movement phase is quantized against the same camera origin on every axis.
using PixelSteps=std::array<int,6>;
PixelSteps pixelSteps(double progress,const PixelOrigin& origin) {
    if(!origin.relative || !std::isfinite(origin.offset.x) || !std::isfinite(origin.offset.y)){
        const int step=int(progress+.5);return {{step,-step,step,-step,0,0}};
    }
    const double x=std::clamp(double(origin.offset.x),-.5,.5),y=std::clamp(double(origin.offset.y),-.5,.5);
    // Bounded finite offsets can round inline without CRT floor calls for
    // every actor/axis. Correct truncation explicitly on the negative side.
    // Float motion phases can straddle an exact half-pixel by a few millionths
    // while two actors travel together. Give that numerical tie one answer.
    const auto snap=[](double value){const double biased=value+.50001;const int whole=int(biased);return whole-(biased<whole);};
    return {{snap(progress+x),snap(-progress+x),snap(progress+y),snap(-progress+y),snap(x),snap(y)}};
}
template<std::size_t N> BasicSnakeGeometry<N> tessellate(const Path<N>& path,const PixelSteps& progress,int width) {
    BasicSnakeGeometry<N> output;
    output.valid=path.valid;
    const auto count=path.count;
    if(!path.valid || count==0)return output;
    const auto& from=path.from;
    output.count=count;
    for(std::size_t i=0;i<count;++i){
        const auto direction=path.directions[i];
        output.centers[i]={from[i].x+progress[direction.x>0?0:direction.x<0?1:4],
                           from[i].y+progress[direction.y>0?2:direction.y<0?3:5]};
    }
    output.headDirection=path.directions[0];
    for(std::size_t i=0;i+1<count;++i){
        const Point head=output.centers[i],tail=output.centers[i+1],elbow{from[i].x+progress[4],from[i].y+progress[5]};
        if(!axisAligned(head,elbow) || !axisAligned(elbow,tail)){
            output.valid=false;output.rectangleCount=0;output.count=0;return output;
        }
        append(output,head,elbow,width);append(output,elbow,tail,width);
    }
    if(output.rectangleCount==0){const auto head=output.centers[0];output.rectangles[output.rectangleCount++]={head.x-width/2,head.y-width/2,width,width};}
    return output;
}
}

SnakeGeometry buildSnake(const Vec* previous,const Vec* current,std::size_t count,
                         float alpha,int width,float scale,int cameraX,int cameraY,const PixelOrigin& origin) {
    if(count==0)return {};
    if(width<1 || width>1024 || !std::isfinite(alpha)){SnakeGeometry output;output.valid=false;return output;}
    const auto path=prepare<MaxSnakePoints>(previous,current,count,scale,cameraX,cameraY);
    const auto progress=pixelSteps(double(path.step)*std::clamp(alpha,0.0f,1.0f),origin);
    return tessellate(path,progress,width);
}

float splitBirthProgress(float age){
    const float t=std::clamp((age-SplitBirthFlash)/(SplitBirthDuration-SplitBirthFlash),0.0f,1.0f);
    return t*t*(3-2*t);
}
SnakeGeometry buildSplitBirth(const SplitBirth& birth,const EnemySnakeGeometry& target,float age,const PixelOrigin& origin,Point targetOffset){
    SnakeGeometry result;
    if(!birth.count || birth.count>birth.path.size() || !target.valid || !target.count || !std::isfinite(age)){result.valid=false;return result;}
    std::array<Vec,9> previous,current;
    for(std::size_t i=0;i<birth.count;++i){const auto p=birth.path[i];previous[i]={float(p.previousX*Tile),float(p.previousY*Tile)};current[i]={float(p.x*Tile),float(p.y*Tile)};}
    auto source=buildSnake(previous.data(),current.data(),birth.count,birth.alpha,5,2.5f,0,0,origin);
    if(!source.valid)source=buildSnake(current.data(),current.data(),birth.count,1,5,2.5f,0,0,origin);
    if(!source.count || !source.rectangleCount || !target.rectangleCount){result.valid=false;return result;}
    const Point sourceOffset{int(birth.normalX)*birth.side*3,int(birth.normalY)*birth.side*3};
    const float t=splitBirthProgress(age);
    // Give the two banks distinct screen-Y arcs: one peels upward, the other
    // downward even when the parent is vertical. Both settle onto the real
    // offspring exactly; all motion is quantized to native raster pixels.
    const int opening=int(std::lround(28*t*(1-t)));
    const int elevation=int(std::lround(104*t*(1-t)));
    const int verticalSide=birth.side*(birth.normalX?birth.normalX:birth.normalY);
    const Point bow{int(birth.normalX)*birth.side*opening,verticalSide*elevation};
    const auto blend=[&](int a,int b){return int(std::lround(double(a)+(double(b)-a)*t));};
    result.count=std::max(source.count,target.count);
    for(std::size_t i=0;i<result.count;++i){const auto a=source.centers[i*source.count/result.count],b=target.centers[i*target.count/result.count];result.centers[i]={blend(a.x+sourceOffset.x,b.x+targetOffset.x)+bow.x,blend(a.y+sourceOffset.y,b.y+targetOffset.y)+bow.y};}
    result.headDirection=target.headDirection;
    result.rectangleCount=std::max(source.rectangleCount,target.rectangleCount);
    for(std::size_t i=0;i<result.rectangleCount;++i){
        const auto a=source.rectangles[i*source.rectangleCount/result.rectangleCount],b=target.rectangles[i*target.rectangleCount/result.rectangleCount];
        const int x=blend(a.x+sourceOffset.x,b.x+targetOffset.x),y=blend(a.y+sourceOffset.y,b.y+targetOffset.y);
        const int right=blend(a.x+a.width+sourceOffset.x,b.x+b.width+targetOffset.x),bottom=blend(a.y+a.height+sourceOffset.y,b.y+b.height+targetOffset.y);
        result.rectangles[i]={x+bow.x,y+bow.y,right-x,bottom-y};
    }
    return result;
}

struct EnemyGeometryCache::Storage {
    struct Entry {
        std::array<Vec,9> previous{},current{};
        Path<9> path;
        EnemySnakeGeometry shape;
        std::uint32_t id=0;
        std::size_t count=10;
        PixelSteps progress{};
        int width=0;
        bool dirty=true;
    };
    std::array<Entry,4096> entries;
    EnemySnakeGeometry invalid;
};
EnemyGeometryCache::EnemyGeometryCache():storage(std::make_unique<Storage>()) {storage->invalid.valid=false;}
EnemyGeometryCache::~EnemyGeometryCache()=default;
const EnemySnakeGeometry& EnemyGeometryCache::sample(std::uint32_t id,const Vec* previous,const Vec* current,std::size_t count,float alpha,int width,const PixelOrigin& origin) {
    if(count>9 || (count && (!previous || !current)) || width<1 || width>1024)return storage->invalid;
    auto& entry=storage->entries[(std::size_t(id)*2654435761u)&(storage->entries.size()-1)];
    const auto bytes=count*sizeof(Vec);
    if(entry.id!=id || entry.count!=count || (count && (std::memcmp(entry.previous.data(),previous,bytes)!=0 || std::memcmp(entry.current.data(),current,bytes)!=0))){
        entry.id=id;entry.count=count;entry.dirty=true;
        if(count){std::memcpy(entry.previous.data(),previous,bytes);std::memcpy(entry.current.data(),current,bytes);}
        entry.path=prepare<9>(previous,current,count,2.5f,0,0);
    }
    const bool finite=std::isfinite(alpha);
    const auto progress=pixelSteps(finite?double(entry.path.step)*std::clamp(alpha,0.0f,1.0f):0,origin);
    if(entry.dirty || entry.progress!=progress || entry.width!=width || !finite){
        entry.shape=tessellate(entry.path,progress,width);
        if(!entry.shape.valid || !finite)entry.shape=tessellate(prepare<9>(current,current,count,2.5f,0,0),pixelSteps(0,origin),width);
        entry.progress=progress;entry.width=width;entry.dirty=!finite;
    }
    return entry.shape;
}

bool contains(const Rect& rectangle,int x,int y) {
    return x>=rectangle.x && y>=rectangle.y && x<rectangle.x+rectangle.width && y<rectangle.y+rectangle.height;
}
bool contains(const SnakeGeometry& geometry,int x,int y) {
    for(std::size_t i=0;i<geometry.rectangleCount;++i)if(contains(geometry.rectangles[i],x,y))return true;
    return false;
}
}
