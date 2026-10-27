#include "death_art.hpp"

#include <rlgl.h>
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

namespace snake::render {
namespace {
constexpr std::size_t Capacity=8192,StreamSlots=8;
constexpr int ShortAttribute=0x1402;
struct Instance {
    std::int16_t left,top,right,bottom;
    Color color;
    std::int16_t originX,originY;
    float age,duration,reach,seed;
};
static_assert(sizeof(Instance)==32,"Death instance packing changed");
#ifdef __EMSCRIPTEN__
constexpr const char* ShaderVersion="#version 300 es\nprecision highp float;\nprecision highp int;\n";
#else
constexpr const char* ShaderVersion="#version 330\n";
#endif
constexpr const char* VertexShader=R"GLSL(
in vec2 vertexPosition;
in vec4 vertexTexCoord;
in vec4 vertexNormal;
in vec4 vertexColor;
in vec2 vertexTangent;
uniform mat4 mvp;
out vec2 sourcePixel;
flat out vec2 origin;
flat out vec4 parameters;
flat out vec4 palette;
void main(){
    sourcePixel=mix(vertexTexCoord.xy,vertexTexCoord.zw,vertexPosition);
    origin=vertexTangent;parameters=vertexNormal;palette=vertexColor;
    gl_Position=mvp*vec4(sourcePixel,0.0,1.0);
}
)GLSL";
constexpr const char* FragmentShader=R"GLSL(
in vec2 sourcePixel;
flat in vec2 origin;
flat in vec4 parameters;
flat in vec4 palette;
out vec4 finalColor;
uint hashCell(ivec2 cell,uint seed){
    uint h=uint(cell.x)*374761393u+uint(cell.y)*668265263u+seed*1442695041u;
    h=(h^(h>>13u))*1274126177u;return h^(h>>16u);
}
void main(){
    // Negative age marks a solid, native-pixel fragment in the same batch.
    if(parameters.x<0.0){finalColor=palette;return;}
    float age=parameters.x,phase=age/parameters.y;
    if(phase>=0.84)discard;
    vec2 pixel=floor(sourcePixel)-origin;
    vec2 cell=floor(pixel/2.0);
    vec2 cellCenter=cell*2.0+0.5;
    float distanceAlong=(abs(cellCenter.x)+abs(cellCenter.y))/parameters.z;
    float erosion=clamp((phase-0.13-distanceAlong*0.18)/0.53,0.0,1.0);
    float grain=float(hashCell(ivec2(cell),uint(parameters.w))&1023u)/1024.0;
    if(erosion>grain)discard;
    vec3 light=mix(palette.rgb,vec3(1.0,0.94,0.82),0.78);
    // A short warm impact, then opaque two-pixel chips with a one-cell rim.
    // There is no blur, filtering, alpha fade, screen-space noise, or RNG.
    float flash=1.0-clamp(age/0.045,0.0,1.0);
    vec3 body=palette.rgb*mix(1.0,0.52,clamp(phase*1.25,0.0,1.0));
    vec3 shade=erosion>0.0 && grain<erosion+0.10?light:body;
    finalColor=vec4(floor(mix(shade,light,flash)*255.0+0.5)/255.0,1.0);
}
)GLSL";
}

struct DeathArt::Storage {
    Shader shader{};
    struct Batch {unsigned vao=0,instances=0;};
    std::array<Batch,StreamSlots> batches{};
    std::array<Instance,Capacity> stream{};
    unsigned vertices=0,indices=0;
    std::size_t count=0,nextBatch=0;
    bool valid=false;
};

DeathArt::DeathArt():storage(std::make_unique<Storage>()) {
    auto& s=*storage;if(!IsWindowReady())return;
    const std::string vertex=std::string(ShaderVersion)+VertexShader,fragment=std::string(ShaderVersion)+FragmentShader;
    s.shader=LoadShaderFromMemory(vertex.c_str(),fragment.c_str());
    if(!s.shader.id || s.shader.id==rlGetShaderIdDefault()){
        if(s.shader.locs)MemFree(s.shader.locs);s.shader={};return;
    }
    constexpr std::int16_t corners[]{0,0,0,1,1,1,1,0};
    constexpr std::uint16_t indices[]{0,1,2,0,2,3};
    s.batches[0].vao=rlLoadVertexArray();rlEnableVertexArray(s.batches[0].vao);
    s.vertices=rlLoadVertexBuffer(corners,sizeof(corners),false);
    s.indices=rlLoadVertexBufferElement(indices,sizeof(indices),false);
    s.valid=s.vertices!=0 && s.indices!=0;
    for(auto& batch:s.batches){
        if(!batch.vao)batch.vao=rlLoadVertexArray();rlEnableVertexArray(batch.vao);
        batch.instances=rlLoadVertexBuffer(s.stream.data(),int(sizeof(s.stream)),true);
        rlEnableVertexBuffer(s.vertices);rlEnableVertexBufferElement(s.indices);
        rlSetVertexAttribute(0,2,ShortAttribute,false,2*sizeof(std::int16_t),0);rlEnableVertexAttribute(0);
        rlEnableVertexBuffer(batch.instances);
        rlSetVertexAttribute(1,4,ShortAttribute,false,sizeof(Instance),0);rlEnableVertexAttribute(1);rlSetVertexAttributeDivisor(1,1);
        rlSetVertexAttribute(2,4,RL_FLOAT,false,sizeof(Instance),int(offsetof(Instance,age)));rlEnableVertexAttribute(2);rlSetVertexAttributeDivisor(2,1);
        rlSetVertexAttribute(3,4,RL_UNSIGNED_BYTE,true,sizeof(Instance),int(offsetof(Instance,color)));rlEnableVertexAttribute(3);rlSetVertexAttributeDivisor(3,1);
        rlSetVertexAttribute(4,2,ShortAttribute,false,sizeof(Instance),int(offsetof(Instance,originX)));rlEnableVertexAttribute(4);rlSetVertexAttributeDivisor(4,1);
        s.valid=s.valid && batch.vao!=0 && batch.instances!=0;
    }
    rlDisableVertexArray();rlDisableVertexBuffer();rlDisableVertexBufferElement();
    // Submit both shader branches during loading, including a capacity rollover.
    // Degenerate quads exercise the pipeline without touching framebuffer pixels.
    if(s.valid){s.stream.fill({0,0,0,0,WHITE,0,0,.1f,.42f,80,1});s.count=Capacity;flush();s.stream[0].age=-1;s.count=1;flush();}
}
DeathArt::~DeathArt(){
    auto& s=*storage;
    if(IsWindowReady()){
        if(s.vertices)rlUnloadVertexBuffer(s.vertices);
        if(s.indices)rlUnloadVertexBuffer(s.indices);
        for(const auto& batch:s.batches){if(batch.instances)rlUnloadVertexBuffer(batch.instances);if(batch.vao)rlUnloadVertexArray(batch.vao);}
        if(s.shader.id)UnloadShader(s.shader);
    }
}
bool DeathArt::ready() const {return storage->valid;}
void DeathArt::rectangle(int x,int y,int width,int height,Color color,int originX,int originY,float age,float duration,float reach,float seed){
    auto& s=*storage;
    const int left=std::max(0,x),top=std::max(0,y),right=std::min(800,x+width),bottom=std::min(500,y+height);
    if(left>=right || top>=bottom)return;
    if(!s.valid){if(age<0 || age/duration<.35f)DrawRectangle(left,top,right-left,bottom-top,color);return;}
    if(s.count==Capacity)flush();
    s.stream[s.count++]={std::int16_t(left),std::int16_t(top),std::int16_t(right),std::int16_t(bottom),color,
        std::int16_t(std::clamp(originX,-16000,16000)),std::int16_t(std::clamp(originY,-16000,16000)),age,duration,reach,seed};
}
void DeathArt::draw(const geometry::SnakeGeometry& shape,Color color,float age,float duration,std::uint32_t seed,int cameraX,int cameraY,bool fragments){
    if(!shape.count || !shape.valid || !std::isfinite(age) || !std::isfinite(duration) || duration<=0 || age>=duration)return;
    age=std::max(0.0f,age);
    const auto head=shape.centers[0];const int originX=head.x-cameraX,originY=head.y-cameraY;
    int reach=16;
    for(std::size_t i=0;i<shape.count;++i)reach=std::max(reach,std::abs(shape.centers[i].x-head.x)+std::abs(shape.centers[i].y-head.y)+16);
    const float grainSeed=float(seed&65535u);
    for(std::size_t i=0;i<shape.rectangleCount;++i){const auto& r=shape.rectangles[i];rectangle(r.x-cameraX,r.y-cameraY,r.width,r.height,color,originX,originY,age,duration,float(reach),grainSeed);}
    const float phase=age/duration;
    if(!fragments || phase<.16f)return;
    constexpr int directions[][2]{{-1,-1},{1,-1},{-1,1},{1,1},{0,-1},{1,0}};
    const int count=duration>.6f?6:4;
    for(int i=0;i<count;++i){
        const unsigned motif=(seed+unsigned(i)*7u)%13u;
        const float delay=.16f+float(motif%4)*.045f,t=std::clamp((phase-delay)/(1-delay),0.0f,1.0f);
        if(t<=0 || t>=.84f+float(motif%3)*.06f)continue;
        const auto point=shape.centers[(std::size_t(i)*shape.count)/std::size_t(count)];
        const int travel=int(std::lround(t*float(8+motif%5))),rise=int(std::lround(8*t*(1-t)));
        const int x=point.x-cameraX+directions[i][0]*(4+travel),y=point.y-cameraY+directions[i][1]*(4+travel)-rise;
        const int size=t<.6f?2:1;
        const auto channel=[&](unsigned char c){return static_cast<unsigned char>(float(c)*.65f+float(255-c)*.20f);};
        const Color shade=t<.23f?Color{255,235,201,255}:Color{channel(color.r),channel(color.g),channel(color.b),255};
        rectangle(x,y,size,size,shade,0,0,-1,1,1,0);
    }
}
void DeathArt::flush(){
    auto& s=*storage;if(!s.count)return;
    rlDrawRenderBatchActive();rlEnableShader(s.shader.id);
    rlSetUniformMatrix(s.shader.locs[SHADER_LOC_MATRIX_MVP],MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));
    const auto& batch=s.batches[s.nextBatch];s.nextBatch=(s.nextBatch+1)%StreamSlots;
    rlEnableVertexArray(batch.vao);rlUpdateVertexBuffer(batch.instances,s.stream.data(),int(s.count*sizeof(Instance)),0);
    rlDrawVertexArrayElementsInstanced(0,6,nullptr,int(s.count));
    rlDisableVertexArray();rlDisableVertexBuffer();rlDisableVertexBufferElement();rlDisableShader();s.count=0;
}

}
