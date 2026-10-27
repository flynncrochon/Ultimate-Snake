"""Small dependency-free reader for the RGB/RGBA PNGs written by raylib.

Only non-interlaced, eight-bit PNGs are accepted. CRCs and every row filter are
checked so a corrupt capture cannot silently pass a visual game test.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct
import zlib


@dataclass(frozen=True)
class Image:
    width: int
    height: int
    rgba: bytes

    def pixel(self, x: int, y: int):
        if not 0 <= x < self.width or not 0 <= y < self.height:
            raise IndexError((x, y))
        offset = (y * self.width + x) * 4
        return tuple(self.rgba[offset:offset + 4])


def _paeth(left, above, upper_left):
    estimate = left + above - upper_left
    dl, da, du = (abs(estimate - candidate) for candidate in (left, above, upper_left))
    return left if dl <= da and dl <= du else above if da <= du else upper_left


def decode(data: bytes) -> Image:
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Not a PNG capture")
    offset = 8
    header = None
    compressed = bytearray()
    ended = False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        end = offset + 8 + length
        if end + 4 > len(data):
            raise ValueError("Truncated PNG chunk")
        payload = data[offset + 8:end]
        crc = struct.unpack_from(">I", data, end)[0]
        if zlib.crc32(kind + payload) & 0xFFFFFFFF != crc:
            raise ValueError("PNG chunk CRC mismatch")
        if kind == b"IHDR":
            if header is not None or length != 13:
                raise ValueError("Invalid PNG header")
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            ended = True
            break
        offset = end + 4
    if header is None or not ended:
        raise ValueError("Incomplete PNG capture")
    width, height, bits, color, compression, filtering, interlace = header
    if (bits, color, compression, filtering, interlace) not in ((8, 2, 0, 0, 0), (8, 6, 0, 0, 0)):
        raise ValueError(f"Unsupported PNG layout: {header}")
    if not 0 < width <= 16384 or not 0 < height <= 16384:
        raise ValueError("Invalid PNG dimensions")
    channels = 4 if color == 6 else 3
    stride = width * channels
    raw = zlib.decompress(compressed)
    if len(raw) != height * (stride + 1):
        raise ValueError("PNG scanline size mismatch")
    result = bytearray(width * height * 4)
    previous = bytearray(stride)
    for y in range(height):
        row_offset = y * (stride + 1)
        row_filter = raw[row_offset]
        row = bytearray(raw[row_offset + 1:row_offset + stride + 1])
        if row_filter == 1:
            for x in range(channels, stride):
                row[x] = (row[x] + row[x - channels]) & 255
        elif row_filter == 2:
            row = bytearray((value + above) & 255 for value, above in zip(row, previous))
        elif row_filter in (3, 4):
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                upper_left = previous[x - channels] if x >= channels else 0
                predictor = ((left + previous[x]) // 2 if row_filter == 3
                             else _paeth(left, previous[x], upper_left))
                row[x] = (row[x] + predictor) & 255
        elif row_filter != 0:
            raise ValueError(f"Unknown PNG row filter: {row_filter}")
        destination = y * width * 4
        if channels == 4:
            result[destination:destination + stride] = row
        else:
            for x in range(width):
                result[destination + x * 4:destination + x * 4 + 3] = row[x * 3:x * 3 + 3]
                result[destination + x * 4 + 3] = 255
        previous = row
    return Image(width, height, bytes(result))


def read(path) -> Image:
    return decode(Path(path).read_bytes())
