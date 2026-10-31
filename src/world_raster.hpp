#pragma once

#include <raylib.h>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace snake::render {

// Integer, untextured rectangles are the common currency of the pixel renderer.
// The generic raylib shape path repeats transform/UV setup for every single
// pixel. This stream uploads one packed instance per rectangle in draw order,
// rotating through preallocated GPU buffers to preserve in-flight submissions.
class WorldRaster {
public:
    explicit WorldRaster(int width = 800, int height = 500);
    ~WorldRaster();
    WorldRaster(const WorldRaster&) = delete;
    WorldRaster& operator=(const WorldRaster&) = delete;
    bool ready() const;
    void setViewport(int width,int height);
    void rectangle(int x,int y,int width,int height,Color color);
    void pixel(int x,int y,Color color) { rectangle(x,y,1,1,color); }
    void line(int x1,int y1,int x2,int y2,Color color);
    void flush();
    std::size_t submittedQuads() const;
    std::size_t submissions() const;
    void resetCounters();
private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};

}
