#pragma once

#include "core.hpp"
#include <algorithm>
#include <cmath>

namespace snake {
inline Vec gridPosition(Vec p) {
    auto component=[](float value) {
        if(!std::isfinite(value))return ArenaSize/2;
        return std::clamp(std::round(value/float(Tile))*float(Tile),float(Tile),ArenaSize-float(Tile));
    };
    return {component(p.x),component(p.y)};
}
inline bool sameCell(Vec a,Vec b) {return a.x==b.x && a.y==b.y;}
inline bool inGridBounds(Vec p) {return p.x>=Tile && p.y>=Tile && p.x<=ArenaSize-Tile && p.y<=ArenaSize-Tile;}
}
