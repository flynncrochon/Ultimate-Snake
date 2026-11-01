#pragma once

#include "core.hpp"
#include "world_raster.hpp"

namespace snake::render {
// A bounded density surface: particles merge into a single shaded water sheet.
// Two-source-pixel samples, integer spans, no render targets or texture uploads.
class FloodArt {
public:
    FloodArt();
    ~FloodArt();
    FloodArt(const FloodArt&) = delete;
    FloodArt& operator=(const FloodArt&) = delete;
    void draw(const FloodSystem& flood,WorldRaster& raster,int cameraX,int cameraY,int width,int height,
              Vec pixelOffset,float alpha,bool interpolate);
private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};
}
