#include "world_raster.hpp"

#include <rlgl.h>
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace snake::render {
namespace {
constexpr std::size_t QuadCapacity=8192;
constexpr std::size_t StreamSlots=16;
constexpr int ShortAttribute=0x1402; // GL_SHORT (rlgl exposes only its common float/byte aliases).
struct Rectangle {std::int16_t left,top,right,bottom;std::uint8_t r,g,b,a;};
static_assert(sizeof(Rectangle)==12,"Packed raster rectangle changed");
#ifdef __EMSCRIPTEN__
constexpr const char* ShaderVersion="#version 300 es\nprecision highp float;\n";
#else
constexpr const char* ShaderVersion="#version 330\n";
#endif
constexpr const char* VertexShader=R"GLSL(
in vec2 vertexPosition;
in vec4 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec4 fragmentColor;
void main(){fragmentColor=vertexColor;gl_Position=mvp*vec4(mix(vertexTexCoord.xy,vertexTexCoord.zw,vertexPosition),0.0,1.0);}
)GLSL";
constexpr const char* FragmentShader=R"GLSL(
in vec4 fragmentColor;
out vec4 finalColor;
void main(){finalColor=fragmentColor;}
)GLSL";
}

struct WorldRaster::Storage {
    int width,height;
    Shader shader{};
    struct Batch {unsigned vao=0,instances=0;};
    std::array<Batch,StreamSlots> batches{};
    unsigned vertices=0,indices=0;
    std::size_t nextBatch=0;
    std::array<Rectangle,QuadCapacity> stream{};
    std::size_t count=0,quads=0,draws=0;
    bool valid=false;
    Storage(int w,int h):width(w),height(h) {}
};

WorldRaster::WorldRaster(int width,int height):storage(std::make_unique<Storage>(width,height)) {
    auto& s=*storage;
    if(!IsWindowReady())return;
    // Shader strings are assembled once, before the first gameplay frame.
    const std::string vertexSource=std::string(ShaderVersion)+VertexShader;
    const std::string fragmentSource=std::string(ShaderVersion)+FragmentShader;
    s.shader=LoadShaderFromMemory(vertexSource.c_str(),fragmentSource.c_str());
    // LoadShaderFromMemory can return the shared default on compile failure,
    // with a separately allocated location array. Keep the shared program but
    // release that array before falling back to ordinary raylib rectangles.
    if(!s.shader.id || s.shader.id==rlGetShaderIdDefault()){
        if(s.shader.locs)MemFree(s.shader.locs);s.shader={};return;
    }
    constexpr std::int16_t corners[]{0,0,0,1,1,1,1,0};
    constexpr std::uint16_t indices[]{0,1,2,0,2,3};
    s.batches[0].vao=rlLoadVertexArray();rlEnableVertexArray(s.batches[0].vao);
    s.vertices=rlLoadVertexBuffer(corners,sizeof(corners),false);
    s.indices=rlLoadVertexBufferElement(indices,sizeof(indices),false);
    s.valid=s.vertices!=0 && s.indices!=0;
    // Keep earlier submissions' storage intact while the GPU consumes it.
    // All slots/VAOs are prepared here; flush never creates GPU objects or
    // explicitly waits for an earlier draw to finish.
    for(auto& batch:s.batches){
        if(!batch.vao)batch.vao=rlLoadVertexArray();rlEnableVertexArray(batch.vao);
        batch.instances=rlLoadVertexBuffer(s.stream.data(),int(s.stream.size()*sizeof(Rectangle)),true);
        rlEnableVertexBuffer(s.vertices);rlEnableVertexBufferElement(s.indices);
        rlSetVertexAttribute(0,2,ShortAttribute,false,2*sizeof(std::int16_t),0);rlEnableVertexAttribute(0);
        rlEnableVertexBuffer(batch.instances);
        rlSetVertexAttribute(1,4,ShortAttribute,false,sizeof(Rectangle),0);rlEnableVertexAttribute(1);rlSetVertexAttributeDivisor(1,1);
        rlSetVertexAttribute(3,4,RL_UNSIGNED_BYTE,true,sizeof(Rectangle),int(offsetof(Rectangle,r)));rlEnableVertexAttribute(3);rlSetVertexAttributeDivisor(3,1);
        s.valid=s.valid && batch.vao!=0 && batch.instances!=0;
    }
    rlDisableVertexArray();rlDisableVertexBuffer();rlDisableVertexBufferElement();
}
WorldRaster::~WorldRaster(){
    auto& s=*storage;
    // Presentation owns this object and dies before its raylib context.
    if(IsWindowReady()){
        if(s.vertices)rlUnloadVertexBuffer(s.vertices);
        for(const auto& batch:s.batches){if(batch.instances)rlUnloadVertexBuffer(batch.instances);if(batch.vao)rlUnloadVertexArray(batch.vao);}
        if(s.indices)rlUnloadVertexBuffer(s.indices);
        if(s.shader.id)UnloadShader(s.shader);
    }
}
bool WorldRaster::ready() const {return storage->valid;}
void WorldRaster::rectangle(int x,int y,int width,int height,Color color){
    auto& s=*storage;
    if(width<=0 || height<=0 || color.a==0)return;
    // Coordinates are bounded before packing; clipping does not change pixel
    // coverage and avoids submitting geometry outside the logical framebuffer.
    const std::int64_t right=std::int64_t(x)+width,bottom=std::int64_t(y)+height;
    const int left=std::max(0,x),top=std::max(0,y);
    const int endX=int(std::min<std::int64_t>(s.width,right)),endY=int(std::min<std::int64_t>(s.height,bottom));
    if(left>=endX || top>=endY)return;
    if(!s.valid){DrawRectangle(left,top,endX-left,endY-top,color);return;}
    if(s.count==QuadCapacity)flush();
    s.stream[s.count++]={std::int16_t(left),std::int16_t(top),std::int16_t(endX),std::int16_t(endY),color.r,color.g,color.b,color.a};
}
void WorldRaster::line(int x1,int y1,int x2,int y2,Color color){
    const int dx=std::abs(x2-x1),sx=x1<x2?1:-1;
    const int dy=-std::abs(y2-y1),sy=y1<y2?1:-1;
    int error=dx+dy,firstX=x1,firstY=y1,lastX=x1,lastY=y1;
    // Consecutive samples on the same row/column are one rectangle. Each
    // Bresenham sample still appears exactly once; separate calls stay ordered.
    auto emit=[&]{rectangle(std::min(firstX,lastX),std::min(firstY,lastY),std::abs(lastX-firstX)+1,std::abs(lastY-firstY)+1,color);};
    for(;;){
        if(x1==x2 && y1==y2){emit();break;}
        const int twice=2*error;int nextX=x1,nextY=y1;
        if(twice>=dy){error+=dy;nextX+=sx;}
        if(twice<=dx){error+=dx;nextY+=sy;}
        if((firstY==lastY && nextY==firstY) || (firstX==lastX && nextX==firstX)){lastX=nextX;lastY=nextY;}
        else {emit();firstX=lastX=nextX;firstY=lastY=nextY;}
        x1=nextX;y1=nextY;
    }
}
void WorldRaster::flush(){
    auto& s=*storage;if(!s.count)return;
    // Generic texture/font commands preceding this stream must reach the GPU
    // first. The next generic batch will restore its own shader/VAO state.
    rlDrawRenderBatchActive();
    rlEnableShader(s.shader.id);
    const Matrix mvp=MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection());
    rlSetUniformMatrix(s.shader.locs[SHADER_LOC_MATRIX_MVP],mvp);
    const auto& batch=s.batches[s.nextBatch];s.nextBatch=(s.nextBatch+1)%StreamSlots;
    rlEnableVertexArray(batch.vao);
    rlUpdateVertexBuffer(batch.instances,s.stream.data(),int(s.count*sizeof(Rectangle)),0);
    rlDrawVertexArrayElementsInstanced(0,6,nullptr,int(s.count));
    rlDisableVertexArray();rlDisableVertexBuffer();rlDisableVertexBufferElement();rlDisableShader();
    s.quads+=s.count;++s.draws;s.count=0;
}
std::size_t WorldRaster::submittedQuads() const{return storage->quads;}
std::size_t WorldRaster::submissions() const{return storage->draws;}
void WorldRaster::resetCounters(){storage->quads=storage->draws=0;}

}
