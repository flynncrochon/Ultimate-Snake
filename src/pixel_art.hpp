#pragma once

#include <raylib.h>
#include <string>

namespace pixel {

// All sprite coordinates name the top-left source pixel. Every sprite is 8x8.
enum class Sprite {
    PlayerHead, PlayerBody, RedHead, RedBody, BlueHead, BlueBody,
    SwiftHead, SwiftBody, BossHead, BossBody, Gem, Heart, Chest,
    Shot, Fang, Mortar, Egg, Cobra, Skull, Star
};
enum class ChestSize { World, Reward };

class Art {
public:
    Art();
    ~Art();
    Art(const Art&) = delete;
    Art& operator=(const Art&) = delete;
    void draw(Sprite sprite, int x, int y, int quarterTurns = 0, Color tint = WHITE) const;
    // Original JavaScript icon artwork, baked into native-size raster rows.
    // Nine weapons, nine passives, then nine evolutions in recipe order.
    void icon(int index, int x, int y) const;
    void iconSmall(int index, int x, int y) const;
    void iconSized(int index, int x, int y, int size) const;
    // Each size is a separately authored raster, blitted 1:1 in source pixels.
    void chest(int centerX, int centerY, ChestSize size = ChestSize::World) const;
private:
    Texture2D atlas{};
    Texture2D iconAtlas{};
    Texture2D chestAtlas{};
};

// Warforge's Pixel Operator 8 face, rasterized once at its native eight rows.
// Art owns the shared font atlas; text/width use identical native glyph metrics.
void text(const std::string& value, int x, int y, Color color, int scale = 1);
int width(const std::string& value, int scale = 1);
int lineHeight(int scale = 1);
void line(int x1, int y1, int x2, int y2, Color color);
void ring(int x, int y, int radius, Color color);

} // namespace pixel
