#pragma once

#include <raylib.h>

namespace snake::render {

enum class EffectSprite {
    Venom, Gatling, HomingFang, RicochetFang, Mortar,
    Egg, HydraEgg, OrbitFang, CobraHead, OrbitFangSmall, OrbitFangMedium
};

// Hand-authored native raster sprites. All directions, glint phases and critical
// palettes are baked once, before play, into a single point-filtered texture.
class EffectArt {
public:
    EffectArt();
    ~EffectArt();
    EffectArt(const EffectArt&) = delete;
    EffectArt& operator=(const EffectArt&) = delete;

    // Angle is radians, with zero facing right. The destination is always a
    // native 20x20 source-pixel cell; transparent padding surrounds the art.
    void draw(EffectSprite sprite, int centerX, int centerY, float angle = 0,
              float age = 0, bool critical = false) const;

private:
    Texture2D atlas{};
};

} // namespace snake::render
