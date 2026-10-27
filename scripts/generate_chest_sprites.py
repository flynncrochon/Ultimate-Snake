"""Rebuild the two hand-authored native-resolution chest sprites using stdlib.

Each map character is one source pixel. The reward sprite is independently
drawn at 32x32, not an enlargement of the 16x16 world sprite. Runtime code loads
the resulting PNG once and blits its regions at their exact native sizes.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
PALETTE = {
    ".": (0, 0, 0, 0),
    "#": (36, 23, 16, 255),
    "s": (75, 41, 24, 255),
    "w": (107, 59, 26, 255),
    "b": (123, 75, 42, 255),
    "l": (139, 91, 42, 255),
    "h": (155, 107, 58, 255),
    "g": (221, 170, 0, 255),
    "G": (255, 215, 0, 255),
    "H": (255, 233, 168, 255),
    "r": (154, 31, 32, 255),
    "R": (255, 51, 51, 255),
    "j": (255, 155, 114, 255),
}

WORLD = (
    "................",
    "....########....",
    "..##ggllllgg##..",
    ".#lHGlwwwwlGHl#.",
    ".#lGglbhhblgGl#.",
    ".#lGglwwwwlgGl#.",
    ".#wggwwwwwwggw#.",
    ".#GGGGGGGGGGGG#.",
    ".#gGbbG##GbbGg#.",
    ".#gGbhGRRGhbGg#.",
    ".#gGbbGHrGbbGg#.",
    ".#gGbbggggbbGg#.",
    ".#gGbbwwwwbbGg#.",
    ".#ggwwwwwwwwgg#.",
    "..############..",
    "................",
)

# Each group below contains exactly eight native pixels for easy review.
REWARD = (
    "........" "........" "........" "........",
    "........" "........" "........" "........",
    ".......#" "########" "########" "#.......",
    ".....##G" "llllllll" "llllllll" "G##.....",
    "....#lHg" "hlllllll" "lllllllh" "gHl#....",
    "...#lhGg" "llhlllll" "lllllhll" "gGhl#...",
    "..#lhhGg" "llwlllll" "llllllhl" "gGwhhl#.",
    "..#lhbGg" "bllwwlll" "llllwwbb" "gGlwhl#.",
    "..#lhlGg" "lhllllhh" "llllhlll" "gGlwhl#.",
    "..#lhwGg" "wlllllll" "lllllwww" "gGlwhl#.",
    "..#lhbGg" "bllhhlll" "llllllbb" "gGlwhl#.",
    "..#lhlGg" "lllllllh" "lllllllh" "gGlwhl#.",
    "..#lwwgg" "wwwwwwww" "wwwwwwww" "ggwwsl#.",
    "..######" "########" "########" "######..",
    "..#GGGGG" "GGGGGGGG" "GGGGGGGG" "GGGGG#..",
    "..#gGGGG" "GGGGGG##" "##GGGGGG" "GGGGg#..",
    "..#gbbGh" "bbbbbgGH" "HGgbbbbb" "hGbbg#..",
    "..#GbhGg" "bhbbbgGR" "jGgbbbhb" "gGhbG#..",
    "..#GbbGg" "bbbhbgGR" "RGgbhbbb" "gGbbG#..",
    "..#GbbGg" "bbbbbggH" "rggbbbbb" "gGbbG#..",
    "..#GbhGg" "bbbbbgGG" "GGgbbbbh" "gGhbG#..",
    "..#GbbGg" "hbbbbgGG" "GGgbbbbh" "gGbbG#..",
    "..#GbbGg" "bbhbbggg" "gggbbhbb" "gGbbG#..",
    "..#GbbGg" "bbbbbbbw" "wbbbbbbb" "gGbbG#..",
    "..#GbhGg" "bbbbbhbw" "wbhbbbbb" "gGhbG#..",
    "..#GbbGg" "bhbbbbbw" "wbbbbbhb" "gGbbG#..",
    "..#GbbGg" "bbbbbbbw" "wbbbbbbb" "gGbbG#..",
    "..#gbbgg" "wwwwwwww" "wwwwwwww" "ggbbg#..",
    "..#sgggg" "ssssssss" "ssssssss" "ggggs#..",
    "...#####" "########" "########" "#####...",
    "........" "........" "........" "........",
    "........" "........" "........" "........",
)


def validate_map(name: str, rows: tuple[str, ...], size: int) -> None:
    if len(rows) != size or any(len(row) != size for row in rows):
        raise ValueError(f"{name}: expected {size} rows of {size} pixels; lengths={[len(r) for r in rows]}")
    if set("".join(rows)) - PALETTE.keys():
        raise ValueError(f"{name}: unknown palette character")
    if any(char != "." for char in rows[0] + rows[-1]):
        raise ValueError(f"{name}: horizontal outer edge must be transparent")
    if any(row[0] != "." or row[-1] != "." for row in rows):
        raise ValueError(f"{name}: vertical outer edge must be transparent")
    if not any(rows[y][x] != rows[y][x ^ 1] for y in range(size) for x in range(0, size, 2)):
        raise ValueError(f"{name}: contains no single-pixel detail")


def rgba(rows: tuple[str, ...]) -> bytes:
    return bytes(channel for row in rows for char in row for channel in PALETTE[char])


def png(path: Path, width: int, height: int, pixels: bytes) -> None:
    if len(pixels) != width * height * 4:
        raise ValueError("RGBA buffer dimensions do not match")
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    stride = width * 4
    raw = b"".join(b"\0" + pixels[y * stride:(y + 1) * stride] for y in range(height))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def atlas() -> bytes:
    pixels = bytearray(48 * 32 * 4)
    for rows, left, size in ((WORLD, 0, 16), (REWARD, 16, 32)):
        image = rgba(rows)
        for y in range(size):
            start = (y * 48 + left) * 4
            pixels[start:start + size * 4] = image[y * size * 4:(y + 1) * size * 4]
    return bytes(pixels)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", action="store_true", help="Also write an integer-enlarged review image under artifacts")
    args = parser.parse_args()
    validate_map("world", WORLD, 16)
    validate_map("reward", REWARD, 32)
    if all(REWARD[y][x] == WORLD[y // 2][x // 2] for y in range(32) for x in range(32)):
        raise ValueError("The reward artwork must be separately authored")
    destination = ROOT / "data" / "sprites" / "chests.png"
    pixels = atlas()
    png(destination, 48, 32, pixels)
    if args.preview:
        scale = 10
        enlarged = bytes(channel for y in range(32 * scale) for x in range(48 * scale)
                         for channel in pixels[((y // scale) * 48 + x // scale) * 4:((y // scale) * 48 + x // scale) * 4 + 4])
        png(ROOT / "artifacts" / "chest-sprites" / "preview.png", 48 * scale, 32 * scale, enlarged)
    print(f"Wrote {destination}")
    print("Atlas: 48x32 RGBA, world=(0,0,16,16), reward=(16,0,32,32)")
    print(f"SHA256 {hashlib.sha256(destination.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
