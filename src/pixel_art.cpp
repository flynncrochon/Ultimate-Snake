#include "pixel_art.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace pixel {
namespace {
constexpr int SpriteCount = 20;
constexpr int IconCount = 27;
constexpr int IconStride = 32;
using Raster = std::array<const char*, 8>;
constexpr int FontSize = 8;
Font labelFont{};
int fontUsers = 0;

void acquireFont() {
    if (fontUsers > 0) { ++fontUsers; return; }
    const std::filesystem::path beside = std::filesystem::path(GetApplicationDirectory()) / "data/fonts/PixelOperator8.ttf";
    const std::filesystem::path local = "data/fonts/PixelOperator8.ttf";
    const auto path = std::filesystem::exists(beside) ? beside : local;
    if (!std::filesystem::exists(path))
        throw std::runtime_error("Missing Warforge font: data/fonts/PixelOperator8.ttf. Keep the data folder beside the executable.");
    // Match the native eight-row FONT_BITMAP bake used by Warforge. Include
    // the face's actual Latin/punctuation cmap rather than substituting glyphs.
    constexpr int ranges[][2] = {
        {0x20,0x7E},{0xA0,0xA3},{0xA5,0xA6},{0xA8,0xA9},{0xAB,0xAC},
        {0xAE,0xAE},{0xB0,0xB1},{0xB4,0xB8},{0xBB,0xBB},{0xBF,0xFF},
        {0x108,0x109},{0x10C,0x10F},{0x11A,0x11D},{0x124,0x125},{0x131,0x131},
        {0x134,0x135},{0x147,0x148},{0x152,0x153},{0x158,0x159},{0x15C,0x15D},
        {0x160,0x161},{0x164,0x165},{0x16C,0x16F},{0x178,0x178},{0x17D,0x17E},
        {0x2C6,0x2C7},{0x2D8,0x2D8},{0x2DA,0x2DA},{0x2DC,0x2DC},
        {0x2013,0x2014},{0x2018,0x201A},{0x201C,0x201E},{0x2020,0x2022},
        {0x2026,0x2026},{0x2030,0x2030},{0x2039,0x203A},{0x20AC,0x20AC},
        {0x20B1,0x20B1},{0x20B7,0x20B7},{0x2117,0x2117},{0x2122,0x2122}
    };
    std::vector<int> codepoints;
    for (const auto& range : ranges)
        for (int point = range[0]; point <= range[1]; ++point) codepoints.push_back(point);
    int byteCount = 0;
    unsigned char* bytes = LoadFileData(path.string().c_str(), &byteCount);
    if (!bytes || byteCount <= 0) throw std::runtime_error("Could not read PixelOperator8.ttf.");
    Font font{};
    font.baseSize = FontSize;
    font.glyphCount = int(codepoints.size());
    font.glyphPadding = 1;
    font.glyphs = LoadFontData(bytes, byteCount, FontSize, codepoints.data(), font.glyphCount, FONT_BITMAP);
    UnloadFileData(bytes);
    if (!font.glyphs) throw std::runtime_error("Could not rasterize PixelOperator8.ttf.");
    Image image = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, FontSize, font.glyphPadding, 1);
    if (!image.data) {
        UnloadFontData(font.glyphs, font.glyphCount);
        MemFree(font.recs);
        throw std::runtime_error("Could not create the Pixel Operator font atlas.");
    }
    font.texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!font.texture.id) {
        UnloadFontData(font.glyphs, font.glyphCount);
        MemFree(font.recs);
        throw std::runtime_error("Could not upload the Pixel Operator font atlas.");
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    labelFont = font;
    fontUsers = 1;
}

void releaseFont() {
    if (fontUsers > 0 && --fontUsers == 0) { UnloadFont(labelFont); labelFont = {}; }
}

int advance(const GlyphInfo& glyph, const Rectangle& rec) {
    return glyph.advanceX > 0 ? glyph.advanceX : int(rec.width);
}

constexpr Raster Head{{
    "33333333", "33333333", "33335533", "33335633",
    "33333333", "33335533", "33335633", "33333333"
}};
constexpr Raster Body{{
    "33333333", "33333333", "33333333", "33333333",
    "33333333", "33333333", "33333333", "33333333"
}};
constexpr Raster Gem{{
    "00000000", "00044000", "00455400", "04555540",
    "04533540", "00433400", "00044000", "00000000"
}};
constexpr Raster Heart{{
    "00000000", "04400440", "45544554", "45555554",
    "04555540", "00455400", "00044000", "00000000"
}};
constexpr Raster Chest{{
    "00000000", "01444410", "14333341", "14444441",
    "14355341", "12355321", "12222221", "01111110"
}};
constexpr Raster Shot{{
    "00000000", "00000000", "00004400", "00445540",
    "04555540", "00444000", "00000000", "00000000"
}};
constexpr Raster Fang{{
    "00000000", "00000550", "00005540", "00055430",
    "00554300", "05543000", "05430000", "00000000"
}};
constexpr Raster Mortar{{
    "00044000", "00455400", "04333540", "43333354",
    "43333334", "04333340", "00444400", "00000000"
}};
constexpr Raster Egg{{
    "00044000", "00455400", "04555540", "04535540",
    "45535554", "45553354", "04555540", "00444400"
}};
constexpr Raster Cobra{{
    "00044000", "00455400", "04333340", "43533534",
    "43366334", "04333340", "00433400", "00444400"
}};
constexpr Raster Skull{{
    "00444400", "04555540", "45555554", "45655654",
    "45655654", "04566540", "00455400", "00444400"
}};
constexpr Raster Star{{
    "00044000", "00055000", "44555544", "45555554",
    "04555540", "04555540", "04500540", "04000040"
}};

using Palette = std::array<Color, 8>;
Palette snakePalette(int index) {
    const Color base = index < 2 ? WHITE : index < 4 ? Color{204,0,0,255} :
        index < 6 ? Color{34,102,204,255} : index < 8 ? Color{255,68,0,255} : Color{187,68,255,255};
    const Color eye = index >= 8 ? YELLOW : index < 2 ? BLACK : WHITE;
    const Color pupil = index < 2 ? BLACK : Color{17,17,17,255};
    return {{{0,0,0,0},base,base,base,base,eye,pupil,base}};
}

Palette itemPalette(int index) {
    Palette p{{{0,0,0,0},{35,54,53,255},{80,118,91,255},{69,190,99,255},{126,238,126,255},{231,255,204,255},{22,48,35,255},{171,255,151,255}}};
    if (index == int(Sprite::Gem)) {
        p[3] = {166,220,225,255}; p[4] = {228,246,237,255}; p[5] = {255,255,247,255};
    } else if (index == int(Sprite::Heart)) {
        p[3] = {183,42,74,255}; p[4] = {210,64,93,255}; p[5] = {255,108,122,255};
    } else if (index == int(Sprite::Chest) || index == int(Sprite::Star)) {
        p[1] = {84,57,35,255}; p[2] = {139,87,43,255}; p[3] = {181,118,49,255};
        p[4] = {236,180,83,255}; p[5] = {255,234,157,255};
    } else if (index == int(Sprite::Fang) || index == int(Sprite::Skull)) {
        p[3] = {132,160,165,255}; p[4] = {185,220,213,255}; p[5] = {246,255,233,255};
    }
    return p;
}

Raster rasterFor(int index) {
    if (index < 10) return (index % 2 == 0) ? Head : Body;
    switch (static_cast<Sprite>(index)) {
    case Sprite::Gem: return Gem;
    case Sprite::Heart: return Heart;
    case Sprite::Chest: return Chest;
    case Sprite::Shot: return Shot;
    case Sprite::Fang: return Fang;
    case Sprite::Mortar: return Mortar;
    case Sprite::Egg: return Egg;
    case Sprite::Cobra: return Cobra;
    case Sprite::Skull: return Skull;
    default: return Star;
    }
}

} // namespace

Art::Art() {
    acquireFont();
    try {
    const std::filesystem::path beside = std::filesystem::path(GetApplicationDirectory()) / "data/icons/original-icons.png";
    const std::filesystem::path local = "data/icons/original-icons.png";
    const auto iconPath = std::filesystem::exists(beside) ? beside : local;
    if (!std::filesystem::exists(iconPath))
        throw std::runtime_error("Missing original artwork: data/icons/original-icons.png. Keep the data folder beside the executable.");
    iconAtlas = LoadTexture(iconPath.string().c_str());
    if (!iconAtlas.id || iconAtlas.width != IconCount*IconStride || iconAtlas.height != 90)
        throw std::runtime_error("Invalid original icon atlas: expected an 864x90 PNG.");
    SetTextureFilter(iconAtlas, TEXTURE_FILTER_POINT);
    const std::filesystem::path chestBeside=std::filesystem::path(GetApplicationDirectory())/"data/sprites/chests.png";
    const std::filesystem::path chestLocal="data/sprites/chests.png";
    const auto chestPath=std::filesystem::exists(chestBeside)?chestBeside:chestLocal;
    if(!std::filesystem::exists(chestPath))
        throw std::runtime_error("Missing chest artwork: data/sprites/chests.png. Keep the data folder beside the executable.");
    chestAtlas=LoadTexture(chestPath.string().c_str());
    if(!chestAtlas.id || chestAtlas.width!=48 || chestAtlas.height!=32)
        throw std::runtime_error("Invalid chest atlas: expected a 48x32 PNG.");
    SetTextureFilter(chestAtlas,TEXTURE_FILTER_POINT);
    Image image = GenImageColor(SpriteCount*8, 32, BLANK);
    if (!image.data) throw std::runtime_error("Could not create the snake sprite atlas.");
    for (int sprite = 0; sprite < SpriteCount; ++sprite) {
        const auto raster = rasterFor(sprite);
        const auto palette = sprite < 10 ? snakePalette(sprite) : itemPalette(sprite);
        for (int rotation = 0; rotation < 4; ++rotation) {
            for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
                int px = x, py = y;
                for (int turn = 0; turn < rotation; ++turn) {
                    int oldX = px; px = 7 - py; py = oldX;
                }
                ImageDrawPixel(&image, sprite * 8 + px, rotation * 8 + py, palette[static_cast<unsigned>(raster[y][x] - '0')]);
            }
        }
    }
    atlas = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!atlas.id) throw std::runtime_error("Could not upload the snake sprite atlas.");
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    } catch (...) {
        if (atlas.id) UnloadTexture(atlas);
        if (iconAtlas.id) UnloadTexture(iconAtlas);
        if (chestAtlas.id) UnloadTexture(chestAtlas);
        releaseFont();
        throw;
    }
}

Art::~Art() {
    if (atlas.id) UnloadTexture(atlas);
    if (iconAtlas.id) UnloadTexture(iconAtlas);
    if (chestAtlas.id) UnloadTexture(chestAtlas);
    releaseFont();
}

void Art::draw(Sprite sprite, int x, int y, int quarterTurns, Color tint) const {
    const int index = static_cast<int>(sprite);
    if (index < 0 || index >= SpriteCount) return;
    const int rotation = ((quarterTurns % 4) + 4) % 4;
    DrawTextureRec(atlas, {float(index*8),float(rotation*8),8,8}, {float(x),float(y)}, tint);
}

void Art::icon(int index, int x, int y) const {
    iconSized(index,x,y,16);
}
void Art::chest(int centerX,int centerY,ChestSize size) const {
    const bool reward=size==ChestSize::Reward;
    const int pixels=reward?32:16;
    DrawTextureRec(chestAtlas,{reward?16.0f:0.0f,0,float(pixels),float(pixels)},
                   {float(centerX-pixels/2),float(centerY-pixels/2)},WHITE);
}

void Art::iconSmall(int index, int x, int y) const {
    iconSized(index,x,y,8);
}

void Art::iconSized(int index, int x, int y, int size) const {
    if (index < 0 || index >= IconCount) return;
    const int row = size == 8 ? 0 : size == 10 ? 8 : size == 16 ? 18 : size == 24 ? 34 : size == 32 ? 58 : -1;
    if (row < 0) return;
    DrawTextureRec(iconAtlas, {float(index*IconStride),float(row),float(size),float(size)}, {float(x),float(y)}, WHITE);
}

void text(const std::string& value, int x, int y, Color color, int scale) {
    if (scale < 1 || !labelFont.glyphs) return;
    const int startX = x;
    for (std::size_t i = 0; i < value.size();) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(value.c_str()+i, &bytes);
        i += static_cast<std::size_t>(std::max(1, bytes));
        if (codepoint == '\r') continue;
        if (codepoint == '\n') { x = startX; y += lineHeight(scale); continue; }
        const int index = GetGlyphIndex(labelFont, codepoint);
        const auto& glyph = labelFont.glyphs[index];
        const Rectangle rec = labelFont.recs[index];
        if (codepoint != ' ' && rec.width > 0 && rec.height > 0)
            DrawTexturePro(labelFont.texture, rec,
                {float(x+glyph.offsetX*scale),float(y+glyph.offsetY*scale),rec.width*float(scale),rec.height*float(scale)},
                {0,0}, 0, color);
        x += advance(glyph,rec)*scale;
    }
}

int width(const std::string& value, int scale) {
    if (!labelFont.glyphs || scale < 1) return 0;
    int current = 0, widest = 0;
    for (std::size_t i = 0; i < value.size();) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(value.c_str()+i, &bytes);
        i += static_cast<std::size_t>(std::max(1,bytes));
        if (codepoint == '\r') continue;
        if (codepoint == '\n') { widest = std::max(widest,current); current = 0; }
        else {
            const int index = GetGlyphIndex(labelFont,codepoint);
            current += advance(labelFont.glyphs[index],labelFont.recs[index]);
        }
    }
    widest = std::max(widest,current);
    return widest*scale;
}

int lineHeight(int scale) { return FontSize*std::max(1,scale); }

void line(int x1, int y1, int x2, int y2, Color color) {
    const int dx = std::abs(x2-x1), sx = x1 < x2 ? 1 : -1;
    const int dy = -std::abs(y2-y1), sy = y1 < y2 ? 1 : -1;
    int error = dx+dy;
    for (;;) {
        DrawPixel(x1,y1,color);
        if (x1 == x2 && y1 == y2) break;
        const int twice = 2*error;
        if (twice >= dy) { error += dy; x1 += sx; }
        if (twice <= dx) { error += dx; y1 += sy; }
    }
}

void ring(int x, int y, int radius, Color color) {
    if (radius < 0) return;
    int dx = radius, dy = 0, error = 1-radius;
    while (dx >= dy) {
        DrawPixel(x+dx,y+dy,color); DrawPixel(x+dy,y+dx,color);
        DrawPixel(x-dy,y+dx,color); DrawPixel(x-dx,y+dy,color);
        DrawPixel(x-dx,y-dy,color); DrawPixel(x-dy,y-dx,color);
        DrawPixel(x+dy,y-dx,color); DrawPixel(x+dx,y-dy,color);
        ++dy;
        if (error < 0) error += 2*dy+1;
        else { --dx; error += 2*(dy-dx)+1; }
    }
}
} // namespace pixel
