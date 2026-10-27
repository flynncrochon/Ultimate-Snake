#pragma once

#include "snake_geometry.hpp"
#include <raylib.h>
#include <memory>

namespace snake::render {

// One ordered instance stream for every corpse. The shader and all GPU/CPU
// storage are prepared before play; no textures, readback, or per-snake flushes.
class DeathArt {
public:
    DeathArt();
    ~DeathArt();
    DeathArt(const DeathArt&) = delete;
    DeathArt& operator=(const DeathArt&) = delete;
    bool ready() const;
    void draw(const geometry::SnakeGeometry& shape,Color color,float age,float duration,
              std::uint32_t seed,int cameraX=0,int cameraY=0,bool fragments=true);
    void flush();
private:
    struct Storage;
    std::unique_ptr<Storage> storage;
    void rectangle(int x,int y,int width,int height,Color color,int originX,int originY,
                   float age,float duration,float reach,float seed);
};

}
