#pragma once

#include "core.hpp"
#include <array>
#include <cstddef>
#include <memory>

namespace snake::geometry {
constexpr std::size_t MaxSnakePoints = 128;
constexpr std::size_t MaxSnakeRectangles = MaxSnakePoints * 2;

struct Point { int x, y; };
struct Rect { int x, y, width, height; };
template<std::size_t Points> struct BasicSnakeGeometry {
    // Only the rectangleCount/count prefixes are initialized. Avoid clearing
    // maximum-sized buffers for every short enemy snake on every frame.
    std::array<Rect,Points*2> rectangles;
    std::array<Point,Points> centers;
    std::size_t rectangleCount = 0, count = 0;
    Point headDirection{};
    bool valid = true;
};
using SnakeGeometry = BasicSnakeGeometry<MaxSnakePoints>;
using EnemySnakeGeometry = BasicSnakeGeometry<9>;

// Preserve the shared camera's subpixel remainder until world motion has been
// combined with it. Final geometry still consists only of integer pixel spans.
struct PixelOrigin {Vec offset{};bool relative=false;};

// Previous/current contain the head followed by body waypoints. Each moving
// waypoint advances along one cardinal edge; ordinary grid motion shares one
// step length. One continuous phase is quantized against the shared camera origin.
// Rectangles are half-open, opaque, integer pixel spans with a fixed stroke.
SnakeGeometry buildSnake(const Vec* previous,const Vec* current,std::size_t count,
                         float alpha,int width,float scale,int cameraX=0,int cameraY=0,const PixelOrigin& origin={});

// A splitter's two blue body banks contract into the actual moving offspring.
// Interpolating ordered, overlapping rectangles keeps every intermediate body
// connected and axis aligned. Both endpoints are exact native silhouettes.
float splitBirthProgress(float age);
SnakeGeometry buildSplitBirth(const SplitBirth& birth,const EnemySnakeGeometry& target,
                             float age,const PixelOrigin& origin={},Point targetOffset={0,0});

bool contains(const Rect& rectangle,int x,int y);
bool contains(const SnakeGeometry& geometry,int x,int y);

// World-space enemy paths change at grid moves, while their rasterized progress
// changes only at source-pixel boundaries. Reuse both independently of the camera.
// Direct-mapped storage has constant lookup cost, including overload/collisions.
class EnemyGeometryCache {
public:
    EnemyGeometryCache();
    ~EnemyGeometryCache();
    EnemyGeometryCache(const EnemyGeometryCache&) = delete;
    EnemyGeometryCache& operator=(const EnemyGeometryCache&) = delete;
    const EnemySnakeGeometry& sample(std::uint32_t id,const Vec* previous,const Vec* current,
                                    std::size_t count,float alpha,int width,const PixelOrigin& origin={});
private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};
}
