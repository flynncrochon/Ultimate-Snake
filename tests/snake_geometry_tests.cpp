#include "snake_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using snake::Vec;
using snake::Tile;
using snake::geometry::Point;
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
struct Mask {
    int left=0,top=0,width=0,height=0;
    std::vector<unsigned char> pixels;
    void square(Point p,int stroke){
        const int x=p.x-stroke/2-left,y=p.y-stroke/2-top;
        for(int row=0;row<stroke;++row)std::fill_n(pixels.begin()+std::ptrdiff_t((y+row)*width+x),stroke,static_cast<unsigned char>(1));
    }
    void rectangle(snake::geometry::Rect r){
        for(int row=0;row<r.height;++row)std::fill_n(pixels.begin()+std::ptrdiff_t((r.y-top+row)*width+r.x-left),r.width,static_cast<unsigned char>(1));
    }
};
struct Frame {std::vector<Vec> previous,current;};
int manhattan(Point a,Point b){return std::abs(a.x-b.x)+std::abs(a.y-b.y);}
int sign(int x){return x>0?1:x<0?-1:0;}
Point project(Vec p,int cameraX,int cameraY){return {int(std::lround(p.x*2.5f))-cameraX,int(std::lround(p.y*2.5f))-cameraY};}

// Independent oracle: walk the historical centerline one pixel at a time,
// trim it by the physical head/tail travel distance, then stamp a square at
// every occupied center. It does not use connector rectangles or elbows.
Mask reference(const Frame& frame,float alpha,int stroke,int cameraX,int cameraY){
    std::vector<Point> route;
    route.push_back(project(frame.current.front(),cameraX,cameraY));
    for(const auto p:frame.previous){const auto q=project(p,cameraX,cameraY);if(manhattan(q,route.back()))route.push_back(q);}
    int total=0,step=0;
    for(std::size_t i=1;i<route.size();++i)total+=manhattan(route[i-1],route[i]);
    for(std::size_t i=0;i<frame.previous.size();++i)step=std::max(step,manhattan(project(frame.previous[i],cameraX,cameraY),project(frame.current[i],cameraX,cameraY)));
    const int progress=int(std::lround(double(step)*std::clamp(alpha,0.0f,1.0f)));
    const int headMove=manhattan(project(frame.previous.front(),cameraX,cameraY),project(frame.current.front(),cameraX,cameraY));
    const int tailMove=manhattan(project(frame.previous.back(),cameraX,cameraY),project(frame.current.back(),cameraX,cameraY));
    const int start=headMove?headMove-progress:0,end=total-(tailMove?progress:0);
    int minX=route.front().x,maxX=minX,minY=route.front().y,maxY=minY;
    for(Point p:route){minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);}
    Mask mask;mask.left=minX-stroke-2;mask.top=minY-stroke-2;mask.width=maxX-minX+stroke*2+5;mask.height=maxY-minY+stroke*2+5;
    mask.pixels.resize(std::size_t(mask.width)*mask.height);
    if(route.size()==1){mask.square(route.front(),stroke);return mask;}
    int traveled=0;
    for(std::size_t i=1;i<route.size();++i){
        const Point a=route[i-1],b=route[i];
        require(a.x==b.x || a.y==b.y,"Oracle fixture has a diagonal path.");
        const int length=manhattan(a,b);
        for(int offset=0;offset<=length;++offset)if(traveled+offset>=start && traveled+offset<=end)
            mask.square({a.x+sign(b.x-a.x)*offset,a.y+sign(b.y-a.y)*offset},stroke);
        traveled+=length;
    }
    return mask;
}
void connected(const Mask& mask){
    const auto first=std::find(mask.pixels.begin(),mask.pixels.end(),1);
    require(first!=mask.pixels.end(),"Snake disappeared.");
    std::vector<unsigned char> visited(mask.pixels.size());std::vector<std::size_t> queue;
    queue.push_back(std::size_t(first-mask.pixels.begin()));visited[queue.front()]=1;
    for(std::size_t head=0;head<queue.size();++head){
        const auto p=queue[head];const int x=int(p%std::size_t(mask.width)),y=int(p/std::size_t(mask.width));
        constexpr int dx[]{1,-1,0,0},dy[]{0,0,1,-1};
        for(int d=0;d<4;++d){const int nx=x+dx[d],ny=y+dy[d];if(nx<0 || ny<0 || nx>=mask.width || ny>=mask.height)continue;
            const auto n=std::size_t(ny*mask.width+nx);if(mask.pixels[n] && !visited[n]){visited[n]=1;queue.push_back(n);}}
    }
    require(queue.size()==std::size_t(std::count(mask.pixels.begin(),mask.pixels.end(),1)),"Snake has disconnected stray pixels.");
}
void compare(const Frame& frame,float alpha,int stroke,int cameraX=0,int cameraY=0){
    const auto geometry=snake::geometry::buildSnake(frame.previous.data(),frame.current.data(),frame.current.size(),alpha,stroke,2.5f,cameraX,cameraY);
    require(geometry.valid,"Legal grid snake was rejected.");
    const auto expected=reference(frame,alpha,stroke,cameraX,cameraY);
    Mask actual=expected;std::fill(actual.pixels.begin(),actual.pixels.end(),static_cast<unsigned char>(0));
    for(std::size_t i=0;i<geometry.rectangleCount;++i){
        const auto r=geometry.rectangles[i];require(r.width==stroke || r.height==stroke,"Diagonal or variable-width rectangle leaked from a turn.");
        require(r.width>0 && r.height>0 && r.x>=actual.left && r.y>=actual.top && r.x+r.width<=actual.left+actual.width && r.y+r.height<=actual.top+actual.height,"Geometry exceeded its physical path bounds.");
        actual.rectangle(r);
    }
    require(actual.pixels==expected.pixels,"Rendered mask differs from the trimmed historical path at alpha="+std::to_string(alpha)+" width="+std::to_string(stroke));
    connected(actual);
}
Frame moving(std::vector<Vec> previous,Vec headStep){
    Frame frame;frame.previous=std::move(previous);frame.current=frame.previous;
    for(std::size_t i=frame.current.size()-1;i>0;--i)frame.current[i]=frame.previous[i-1];
    frame.current[0]={frame.previous[0].x+headStep.x*snake::Tile,frame.previous[0].y+headStep.y*snake::Tile};return frame;
}
Frame rotate(Frame frame,int turns){
    auto apply=[&](Vec& p){p.x-=800;p.y-=800;for(int i=0;i<turns;++i){const float old=p.x;p.x=-p.y;p.y=old;}p.x+=800;p.y+=800;};
    for(auto& p:frame.previous)apply(p);for(auto& p:frame.current)apply(p);return frame;
}
}

int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    test("all cardinal turn fractions match a swept-square path with no stray pixels",[]{
        const std::vector<Frame> cases{
            moving({{800,800},{792,800},{784,800},{776,800}}, {1,0}),
            moving({{800,800},{792,800},{784,800},{776,800}}, {0,-1}),
            moving({{800,800},{792,800},{792,808},{784,808},{784,816}}, {0,-1}),
            moving({{800,800},{792,800},{792,808},{800,808},{808,808},{808,816}}, {0,-1}),
            moving({{800,800},{800,800},{800,800}}, {0,-1}),
            moving({{800,800},{792,800},{784,800},{776,800}}, {0,0}),
            moving({{800,800}}, {0,-1})};
        for(const auto& frame:cases)for(int rotation=0;rotation<4;++rotation)for(int stroke:{7,8,11,14,15})for(int tick=0;tick<=40;++tick)
            compare(rotate(frame,rotation),float(tick)/40.0f,stroke,901,-531);
    });
    test("straight runs merge without changing odd or even pixel footprints",[]{
        std::vector<Vec> points;for(int i=0;i<120;++i)points.push_back({800-float(i)*8,800});const auto frame=moving(points,{1,0});
        for(int stroke:{7,8,11,14,15})for(float alpha:{0.0f,.025f,.5f,.975f,1.0f}){
            compare(frame,alpha,stroke);const auto geometry=snake::geometry::buildSnake(frame.previous.data(),frame.current.data(),frame.current.size(),alpha,stroke,2.5f);
            require(geometry.rectangleCount==1,"A straight snake still issues per-segment draw calls.");}
    });
    test("long spiral and growth remain one exact connected silhouette",[]{
        std::vector<Vec> points{{800,800}};constexpr Vec directions[]{{-1,0},{0,1},{1,0},{0,-1}};
        for(int leg=0;points.size()<120;++leg)for(int i=0;i<1+leg/2 && points.size()<120;++i){const auto previous=points.back(),direction=directions[leg%4];points.push_back({previous.x+direction.x*8,previous.y+direction.y*8});}
        auto frame=moving(points,{0,-1});for(int stroke:{7,11,14,15})for(float alpha:{0.0f,.025f,.5f,.975f,1.0f})compare(frame,alpha,stroke);
        auto grown=moving({{800,800},{792,800},{784,800},{784,800}}, {0,-1});
        for(int tick=0;tick<=40;++tick)compare(grown,float(tick)/40,14);
    });
    test("translation and camera quantization preserve every covered pixel",[]{
        const auto frame=moving({{800,800},{792,800},{792,808},{784,808}}, {0,-1});
        for(int tick=0;tick<=40;++tick){const float alpha=float(tick)/40;const auto a=snake::geometry::buildSnake(frame.previous.data(),frame.current.data(),frame.current.size(),alpha,14,2.5f,0,0);
            const auto b=snake::geometry::buildSnake(frame.previous.data(),frame.current.data(),frame.current.size(),alpha,14,2.5f,1723,1198);
            require(a.rectangleCount==b.rectangleCount,"Camera changed geometry topology.");
            for(std::size_t i=0;i<a.rectangleCount;++i){const auto x=a.rectangles[i],y=b.rectangles[i];require(x.x==y.x+1723 && x.y==y.y+1198 && x.width==y.width && x.height==y.height,"Camera moved individual edges inconsistently.");}}
    });
    test("invalid diagonal snapshots cannot produce diagonal boxes",[]{
        const Vec before[]{{0,0},{-8,0}},after[]{{8,8},{0,0}};
        const auto bad=snake::geometry::buildSnake(before,after,2,.5f,14,2.5f);
        require(!bad.valid && bad.rectangleCount==0,"Invalid motion drew a diagonal blob.");
        const auto empty=snake::geometry::buildSnake(nullptr,nullptr,0,0,14,2.5f);require(empty.valid && empty.rectangleCount==0,"Empty geometry is not safe.");
    });
    test("enemy geometry cache retains exact paths through collisions reuse and width changes",[]{
        snake::geometry::EnemyGeometryCache cache;
        const auto check=[&](std::uint32_t id,const Frame& frame,float alpha,int width){
            auto expected=snake::geometry::buildSnake(frame.previous.data(),frame.current.data(),frame.current.size(),alpha,width,2.5f);
            if(!expected.valid)expected=snake::geometry::buildSnake(frame.current.data(),frame.current.data(),frame.current.size(),1,width,2.5f);
            const auto& actual=cache.sample(id,frame.previous.data(),frame.current.data(),frame.current.size(),alpha,width);
            require(actual.valid==expected.valid && actual.count==expected.count && actual.rectangleCount==expected.rectangleCount,"Cached path validity/topology differs");
            for(std::size_t i=0;i<actual.count;++i)require(actual.centers[i].x==expected.centers[i].x && actual.centers[i].y==expected.centers[i].y,"Cached center moved to a different pixel");
            for(std::size_t i=0;i<actual.rectangleCount;++i){const auto a=actual.rectangles[i],b=expected.rectangles[i];require(a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height,"Cache reused another path/phase/width");}
        };
        for(int rotation=0;rotation<4;++rotation){
            const auto frame=rotate(moving({{800,800},{792,800},{792,808},{784,808},{784,816}}, {0,-1}),rotation);
            for(int tick=0;tick<=240;++tick){const float alpha=float(tick)/240;check(7,frame,alpha,11);check(7,frame,alpha,15);check(4103,moving({{400,400},{408,400}}, {-1,0}),alpha,11);check(7,frame,alpha,11);}
        }
        check(7,{{{8,8},{0,8}},{{8,16},{8,8}}},.5f,11);
        check(7,{{{1400,1200}},{{1400,1200}}},0,15);
        check(7,{{{0,0},{-8,0}},{{8,8},{0,0}}},.5f,11);
        check(7,moving({{800,800},{792,800}}, {0,1}),std::numeric_limits<float>::quiet_NaN(),11);
        for(std::uint32_t id=1;id<=10000;++id)check(id,moving({{800,800},{792,800}}, {0,-1}),float(id%21)/20,11);
    });
    test("projection preserves ties-away rounding for negative and fractional source pixels",[]{
        for(float value:{-800.2f,-8.2f,-.6f,-.2f,0.0f,.2f,.6f,8.2f,800.2f})for(float offset:{-1.0f,0.0f,1.0f}){
            const float x=offset<0?std::nextafter(value,-std::numeric_limits<float>::infinity()):offset>0?std::nextafter(value,std::numeric_limits<float>::infinity()):value;
            const Vec point{x,-x};const auto shape=snake::geometry::buildSnake(&point,&point,1,.5f,11,2.5f);
            require(shape.valid && shape.centers[0].x==int(std::lround(double(x)*2.5)) && shape.centers[0].y==int(std::lround(-double(x)*2.5)),"Projection changed rounding near a half pixel");
        }
    });
    test("shared pixel origin prevents enemy backsteps against a moving camera on every axis",[]{
        snake::geometry::EnemyGeometryCache cache;
        for(double enemySpeed:{24.0,Tile/.12})for(double phaseOffset:{0.0,.2})for(const Vec direction:std::vector<Vec>{{1,0},{-1,0},{0,1},{0,-1}}){
            int last=0;
            for(int frame=0;frame<480;++frame){
                const double time=double(frame)/240,playerDistance=time*Tile/.12,enemyDistance=time*enemySpeed+phaseOffset;
                const double cx=800*2.5+direction.x*playerDistance*2.5,cy=800*2.5+direction.y*playerDistance*2.5;
                const int cameraX=int(std::floor(cx+.5))-400,cameraY=int(std::floor(cy+.5))-250;
                const snake::geometry::PixelOrigin origin{{float(cameraX-(cx-400)),float(cameraY-(cy-250))},true};
                const int cell=int(std::floor(enemyDistance/Tile));const float alpha=float(enemyDistance/Tile-cell);
                const Vec from{800-direction.x*64+direction.x*float(cell*Tile),800-direction.y*64+direction.y*float(cell*Tile)};
                const Vec to{from.x+direction.x*Tile,from.y+direction.y*Tile};
                const auto& shape=cache.sample(17,&from,&to,1,alpha,11,origin);
                require(shape.valid && shape.count==1,"Camera-relative enemy geometry became invalid");
                const int x=shape.centers[0].x-cameraX,y=shape.centers[0].y-cameraY;
                const int expectedX=int(std::floor((800-direction.x*64+direction.x*enemyDistance)*2.5-(cx-400)+.5));
                const int expectedY=int(std::floor((800-direction.y*64+direction.y*enemyDistance)*2.5-(cy-250)+.5));
                require(std::abs(x-expectedX)<=1 && std::abs(y-expectedY)<=1,"Enemy drifted from continuous relative motion");
                const int projected=int(direction.x)*x+int(direction.y)*y;
                if(frame){require(projected<=last && last-projected<=1,"Independent camera rounding introduced a one-pixel reverse step");if(enemySpeed>60)require(projected==last,"Equal-speed actors shimmered at a fractional pixel separation");}last=projected;
            }
        }
    });
    test("camera-relative elbows keep each center on its continuous historical path",[]{
        snake::geometry::EnemyGeometryCache cache;
        for(int rotation=0;rotation<4;++rotation){
            const auto frame=rotate(moving({{800,800},{792,800},{792,808},{784,808},{784,816}}, {0,-1}),rotation);
            for(float shift:{-.5f,-.49f,0.0f,.49f,.5f})for(int tick=0;tick<=80;++tick){
                const float alpha=float(tick)/80;const snake::geometry::PixelOrigin origin{{shift,-shift},true};
                const auto& shape=cache.sample(19,frame.previous.data(),frame.current.data(),frame.current.size(),alpha,11,origin);
                require(shape.valid && shape.count==frame.current.size(),"Camera-relative elbow became invalid");
                for(std::size_t i=0;i<shape.count;++i){
                    const double x=(double(frame.previous[i].x)+(frame.current[i].x-frame.previous[i].x)*(double(tick)/80))*2.5+shift;
                    const double y=(double(frame.previous[i].y)+(frame.current[i].y-frame.previous[i].y)*(double(tick)/80))*2.5-shift;
                    require(shape.centers[i].x==int(std::floor(x+.5)) && shape.centers[i].y==int(std::floor(y+.5)),"A body center used another axis's camera phase");
                }
                for(std::size_t i=0;i<shape.rectangleCount;++i)require(shape.rectangles[i].width==11 || shape.rectangles[i].height==11,"Camera correction changed body width");
            }
        }
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
