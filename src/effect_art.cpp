#include "effect_art.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace snake::render {
namespace {
constexpr int SourceSize = 15;
constexpr int Cell = 20;
constexpr int Pivot = Cell / 2;
constexpr int Directions = 16;
constexpr int Frames = 4;
constexpr int SpriteCount = 11;
constexpr float Tau = 6.2831853071795864769f;
using Mask = std::array<const char*, SourceSize>;
using Palette = std::array<Color, 9>;

// These are independently authored source pixels, never enlarged versions of
// an 8px icon. Dark keylines, joined shade clusters and ivory glints survive
// crowded combat at the actual 800x500 source resolution.
constexpr Mask Venom{{
    "...............", "...............", "...............",
    "...11..........", "..13211........", ".12334311......",
    "..1334554311...", "11334477554311.", "..13344333311..",
    ".1233211111....", "..121..........", "...1...........",
    "...............", "...............", "..............."
}};
constexpr Mask Gatling{{
    "...............", "...............", "...11..........",
    "..16211........", ".12634311......", "..1344554311...",
    "11234477555411.", ".12345585555431", "11234477555411.",
    "..1344554311...", ".12634311......", "..16211........",
    "...11..........", "...............", "..............."
}};
constexpr Mask Homing{{
    "...111.........", "..123211.......", ".123443211.....",
    ".12345543211...", "..123455543211.", "...12345755431.",
    "....1234554311.", ".....12344311..", "......123311...",
    ".....123211....", "....12311......", "...121.........",
    "..11...........", "...............", "..............."
}};
constexpr Mask Ricochet{{
    "...............", "...11..........", "..16311........",
    "...164311......", "..123645311....", ".123445775311..",
    "112344577554311", ".12345685555431", "112344577554311",
    ".123443333311..", "..123331111....", "...13311.......",
    "..1311.........", "...1...........", "..............."
}};
constexpr Mask Mortar{{
    "...............", "......11.......", ".....1641......",
    "....1143211....", "...123443321...", "..12344554321..",
    "..13477554431..", "..13452225431..", "..13452125431..",
    "..12342224321..", "...123333321...", "....1233321....",
    ".....11111.....", "...............", "..............."
}};
constexpr Mask Egg{{
    "...............", "......111......", ".....14541.....",
    "....1457541....", "....1455441....", "...145544431...",
    "...145436431...", "..14544366331..", "..14444436331..",
    "..14443443331..", "..13433333321..", "...133333321...",
    "....1222221....", ".....11111.....", "..............."
}};
constexpr Mask HydraEgg{{
    "...............", "......111......", ".....14541.....",
    "....1457541....", "....1455441....", "...145446431...",
    "...144463431...", "..14544634331..", "..14446364331..",
    "..14463633331..", "..13436333321..", "...133333321...",
    "....1222221....", ".....11111.....", "..............."
}};
constexpr Mask Orbit{{
    "...............", "...............", "...............",
    "....11.........", "...13411.......", "....1454311....",
    ".....145754311.", "...11245555431.", "..12334454311..",
    "...12344311....", "....12311......", ".....11........",
    "...............", "...............", "..............."
}};
constexpr Mask Cobra{{
    "...............", ".....11111.....", "....1233331....",
    "...123444431...", "..12346554431..", "..134665544431.",
    "..1344255445431", "..1344277555541", "..1344255445431",
    "..134665544431.", "..12346554431..", "...123444431...",
    "....1233331....", ".....11111.....", "..............."
}};
// Separate silhouettes preserve the orbit's varied native sizes without
// fractional destination scaling or resampling a large sprite at draw time.
constexpr Mask OrbitSmall{{
    "...............", "...............", "...............",
    "...............", "...............", ".....11........",
    "......1451.....", "....1237541....", ".....13431.....",
    "......111......", "...............", "...............",
    "...............", "...............", "..............."
}};
constexpr Mask OrbitMedium{{
    "...............", "...............", "...............",
    "...............", "....11.........", ".....14511.....",
    "......145431...", "...123457541...", "....1234431....",
    ".....12311.....", "......11.......", "...............",
    "...............", "...............", "..............."
}};
constexpr std::array<Mask, SpriteCount> Masks{{
    Venom, Gatling, Homing, Ricochet, Mortar, Egg, HydraEgg, Orbit, Cobra,
    OrbitSmall, OrbitMedium
}};

Palette colors(int sprite, int frame, bool critical) {
    Palette p{{BLANK, {13,35,31,255}, {23,66,43,255}, {38,132,65,255},
               {103,217,106,255}, {223,255,188,255}, {41,96,66,255},
               {246,255,225,255}, {246,255,225,255}}};
    switch (static_cast<EffectSprite>(sprite)) {
    case EffectSprite::Gatling:
        p = {{BLANK,{12,39,39,255},{24,83,69,255},{49,151,111,255},
              {117,229,170,255},{233,255,219,255},{217,167,56,255},
              {251,255,238,255},{255,222,100,255}}};
        break;
    case EffectSprite::HomingFang:
        p = {{BLANK,{16,39,53,255},{34,78,101,255},{66,142,158,255},
              {148,216,217,255},{231,255,238,255},{45,109,139,255},
              {255,255,241,255},{179,249,255,255}}};
        break;
    case EffectSprite::RicochetFang:
        p = {{BLANK,{35,21,61,255},{72,45,106,255},{120,73,171,255},
              {182,135,221,255},{240,224,255,255},{75,185,189,255},
              {255,253,242,255},{139,255,224,255}}};
        break;
    case EffectSprite::Mortar:
        p = {{BLANK,{14,34,25,255},{27,64,35,255},{43,117,43,255},
              {93,181,61,255},{183,239,100,255},{128,94,41,255},
              {235,255,155,255},{244,255,204,255}}};
        break;
    case EffectSprite::Egg:
        p = {{BLANK,{53,36,27,255},{104,64,34,255},{164,109,48,255},
              {225,177,86,255},{255,224,146,255},{121,77,39,255},
              {255,249,204,255},{255,238,173,255}}};
        break;
    case EffectSprite::HydraEgg:
        p = {{BLANK,{35,17,49,255},{66,30,90,255},{111,49,151,255},
              {178,102,222,255},{225,177,250,255},{79,39,109,255},
              {250,230,255,255},{232,197,255,255}}};
        break;
    case EffectSprite::OrbitFang:
    case EffectSprite::OrbitFangSmall:
    case EffectSprite::OrbitFangMedium:
        p = {{BLANK,{14,48,39,255},{25,93,65,255},{44,157,101,255},
              {109,232,154,255},{217,255,205,255},{34,126,97,255},
              {250,255,229,255},{255,241,147,255}}};
        break;
    case EffectSprite::CobraHead:
        p = {{BLANK,{16,36,27,255},{31,79,43,255},{52,137,60,255},
              {104,190,81,255},{185,229,111,255},{190,130,43,255},
              {255,232,117,255},{249,255,204,255}}};
        break;
    default: break;
    }
    // Four discrete light phases keep their silhouette and occupied pixels
    // stable. Only the tiny authored specular clusters change value.
    if (frame == 1) p[7] = p[5];
    if (frame == 2) { p[7] = p[4]; p[8] = p[5]; }
    if (frame == 3) p[8] = p[4];
    if (critical) { p[7] = {255,249,170,255}; p[8] = {255,213,73,255}; }
    return p;
}
} // namespace

EffectArt::EffectArt() {
    constexpr int width = Directions * Cell;
    constexpr int height = SpriteCount * Frames * 2 * Cell;
    static_assert(width <= 2048 && height <= 2048, "Effect atlas exceeds the portable texture budget");
    Image image = GenImageColor(width, height, BLANK);
    if (!image.data) throw std::runtime_error("Could not create native effect artwork.");
    // Inverse nearest-neighbour rasterization bakes actual rotated pixels.
    // Runtime rendering never rotates/scales texture quads or interpolates art.
    for (int sprite = 0; sprite < SpriteCount; ++sprite) {
        for (int direction = 0; direction < Directions; ++direction) {
            const float angle = Tau * float(direction) / float(Directions);
            const float cosine = std::cos(angle), sine = std::sin(angle);
            for (int critical = 0; critical < 2; ++critical) {
                for (int frame = 0; frame < Frames; ++frame) {
                    const auto palette = colors(sprite, frame, critical != 0);
                    const int cellX = direction * Cell;
                    const int cellY = (sprite * Frames * 2 + critical * Frames + frame) * Cell;
                    for (int y = 0; y < Cell; ++y) for (int x = 0; x < Cell; ++x) {
                        const float dx = float(x - Pivot), dy = float(y - Pivot);
                        const int sx = int(std::lround(dx * cosine + dy * sine)) + SourceSize / 2;
                        const int sy = int(std::lround(-dx * sine + dy * cosine)) + SourceSize / 2;
                        if (sx < 0 || sx >= SourceSize || sy < 0 || sy >= SourceSize) continue;
                        const char shade = Masks[std::size_t(sprite)][std::size_t(sy)][sx];
                        if (shade != '.') ImageDrawPixel(&image, cellX + x, cellY + y, palette[std::size_t(shade - '0')]);
                    }
                }
            }
        }
    }
    atlas = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!atlas.id) throw std::runtime_error("Could not upload native effect artwork.");
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
}

EffectArt::~EffectArt() {
    if (atlas.id) UnloadTexture(atlas);
}

void EffectArt::draw(EffectSprite sprite, int centerX, int centerY, float angle,
                     float age, bool critical) const {
    const int index = static_cast<int>(sprite);
    if (!atlas.id || index < 0 || index >= SpriteCount) return;
    const float turn = std::isfinite(angle) ? std::fmod(angle, Tau) : 0;
    const int direction = (int(std::lround(turn * float(Directions) / Tau)) + Directions) % Directions;
    const int frame = std::isfinite(age) ? int(std::abs(std::fmod(age, .5f)) * 8) : 0;
    DrawTextureRec(atlas,
                   {float(direction * Cell),
                    float((index * Frames * 2 + (critical ? Frames : 0) + frame) * Cell), float(Cell), float(Cell)},
                   {float(centerX - Pivot), float(centerY - Pivot)}, WHITE);
}

} // namespace snake::render
