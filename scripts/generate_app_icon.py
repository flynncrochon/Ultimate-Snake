"""Rebuild Ultimate Snake's pixel-art Windows icon using only Python's stdlib.

The white, square-headed snake and gray tail match the game's player artwork.
Small sizes use native raster layouts; larger sizes repeat whole pixels without
antialiasing. No icon generation or image loading is needed during gameplay.
"""
from __future__ import annotations

from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 24, 32, 48, 64, 96, 128, 256)


def native_icon(size: int) -> bytes:
    pixels = bytearray(size * size * 4)

    def rect(x: int, y: int, width: int, height: int, gray: int) -> None:
        for row in range(y, y + height):
            for column in range(x, x + width):
                offset = (row * size + column) * 4
                pixels[offset:offset + 4] = bytes((gray, gray, gray, 255))

    # A dark tile with stepped corners keeps the white snake visible on both
    # light and dark taskbars, with very little padding around the silhouette.
    corner = 2 if size == 16 else 3
    rect(corner, 0, size - 2 * corner, size, 15)
    rect(0, corner, size, size - 2 * corner, 15)
    rect(1, 1, size - 2, size - 2, 15)
    if size == 16:
        rect(2, 11, 11, 3, 110)  # tail
        rect(10, 7, 3, 7, 190)
        rect(2, 7, 11, 3, 220)
        rect(2, 2, 3, 8, 255)
        rect(2, 2, 10, 3, 255)
        rect(9, 2, 5, 4, 255)  # square head, facing right
        rect(12, 2, 1, 1, 0)
        rect(12, 4, 1, 1, 0)
    elif size == 24:
        rect(3, 17, 16, 4, 110)
        rect(15, 10, 4, 11, 190)
        rect(3, 10, 16, 4, 220)
        rect(3, 3, 4, 11, 255)
        rect(3, 3, 14, 4, 255)
        rect(14, 3, 7, 6, 255)
        rect(19, 4, 1, 1, 0)
        rect(19, 7, 1, 1, 0)
    else:
        raise ValueError("Only 16 and 24 pixel layouts are authored")
    return bytes(pixels)


def icon_pixels(size: int) -> bytes:
    native_size = 24 if size == 24 else 16
    source = native_icon(native_size)
    scale = size // native_size
    if size % native_size:
        raise ValueError("Icon sizes must use integer pixel scaling")
    rows = []
    for y in range(native_size):
        row = b"".join(source[(y * native_size + x) * 4:(y * native_size + x + 1) * 4]
                       * scale for x in range(native_size))
        rows.extend([row] * scale)
    return b"".join(rows)


def png(size: int, pixels: bytes) -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))
    stride = size * 4
    raw = b"".join(b"\0" + pixels[y * stride:(y + 1) * stride] for y in range(size))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def dib(size: int, pixels: bytes) -> bytes:
    # ICO DIBs store bottom-up BGRA followed by a DWORD-aligned transparency mask.
    colors = bytearray()
    mask = bytearray()
    mask_stride = ((size + 31) // 32) * 4
    for y in reversed(range(size)):
        row_mask = bytearray(mask_stride)
        for x in range(size):
            offset = (y * size + x) * 4
            red, green, blue, alpha = pixels[offset:offset + 4]
            colors.extend((blue, green, red, alpha))
            if alpha == 0:
                row_mask[x // 8] |= 0x80 >> (x % 8)
        mask.extend(row_mask)
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0,
                         len(colors) + len(mask), 0, 0, 0, 0)
    return header + colors + mask


def main() -> None:
    images = {size: icon_pixels(size) for size in SIZES}
    frames = [png(size, images[size]) if size == 256 else dib(size, images[size])
              for size in SIZES]
    directory = bytearray(struct.pack("<HHH", 0, 1, len(frames)))
    offset = 6 + 16 * len(frames)
    for size, frame in zip(SIZES, frames):
        dimension = size if size < 256 else 0
        directory.extend(struct.pack("<BBBBHHII", dimension, dimension, 0, 0,
                                     1, 32, len(frame), offset))
        offset += len(frame)
    destination = ROOT / "data" / "icons"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "ultimate-snake.ico").write_bytes(directory + b"".join(frames))
    (destination / "ultimate-snake.png").write_bytes(png(256, images[256]))
    print(f"Wrote {destination / 'ultimate-snake.ico'} ({', '.join(map(str, SIZES))} px)")
    print(f"Wrote {destination / 'ultimate-snake.png'} (256 px)")


if __name__ == "__main__":
    main()
