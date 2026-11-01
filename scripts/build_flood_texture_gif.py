"""Make a lossless-palette GIF from matched native flood captures; stdlib only."""
import argparse
import atexit
from collections import Counter
import json
import os
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests._png import Image, read as portable_read


def png_reader():
    """Use Windows' PNG codec when available, with the portable stdlib fallback.

    This keeps encoding a native capture sequence fast without adding Pillow
    or FFmpeg dependencies. It never changes source PNGs or their RGB values.
    """
    if os.name != 'nt':
        return portable_read
    import ctypes as ct
    from ctypes import wintypes as wt

    class Startup(ct.Structure):
        _fields_ = [('version', wt.UINT), ('callback', ct.c_void_p), ('background', wt.BOOL), ('codecs', wt.BOOL)]

    class Rect(ct.Structure):
        _fields_ = [('x', ct.c_int), ('y', ct.c_int), ('width', ct.c_int), ('height', ct.c_int)]

    class BitmapData(ct.Structure):
        _fields_ = [('width', wt.UINT), ('height', wt.UINT), ('stride', ct.c_int), ('format', ct.c_int),
                    ('scan', ct.c_void_p), ('reserved', ct.c_size_t)]

    api = ct.WinDLL('gdiplus')
    api.GdiplusStartup.argtypes = [ct.POINTER(ct.c_size_t), ct.POINTER(Startup), ct.c_void_p]
    api.GdiplusShutdown.argtypes = [ct.c_size_t]
    api.GdipCreateBitmapFromFile.argtypes = [wt.LPCWSTR, ct.POINTER(ct.c_void_p)]
    api.GdipGetImageWidth.argtypes = api.GdipGetImageHeight.argtypes = [ct.c_void_p, ct.POINTER(wt.UINT)]
    api.GdipBitmapLockBits.argtypes = [ct.c_void_p, ct.POINTER(Rect), wt.UINT, ct.c_int, ct.POINTER(BitmapData)]
    api.GdipBitmapUnlockBits.argtypes = [ct.c_void_p, ct.POINTER(BitmapData)]
    api.GdipDisposeImage.argtypes = [ct.c_void_p]
    token = ct.c_size_t()
    if api.GdiplusStartup(ct.byref(token), ct.byref(Startup(1, None, False, False)), None):
        return portable_read
    atexit.register(lambda: api.GdiplusShutdown(token))

    def read(path):
        bitmap, width, height, data = ct.c_void_p(), wt.UINT(), wt.UINT(), BitmapData()
        if api.GdipCreateBitmapFromFile(str(path.resolve()), ct.byref(bitmap)):
            raise ValueError(f'Cannot decode native PNG: {path}')
        try:
            if api.GdipGetImageWidth(bitmap, ct.byref(width)) or api.GdipGetImageHeight(bitmap, ct.byref(height)):
                raise ValueError(f'Invalid native PNG dimensions: {path}')
            rect = Rect(0, 0, width.value, height.value)
            if api.GdipBitmapLockBits(bitmap, ct.byref(rect), 1, 0x26200a, ct.byref(data)):
                raise ValueError(f'Cannot read native PNG pixels: {path}')
            try:
                stride = width.value*4
                bgra = (ct.string_at(data.scan, stride*height.value) if data.stride == stride else
                        b''.join(ct.string_at(data.scan+y*data.stride, stride) for y in range(height.value)))
                rgba = bytearray(bgra)
                rgba[0::4], rgba[2::4] = bgra[2::4], bgra[0::4]
            finally:
                api.GdipBitmapUnlockBits(bitmap, ct.byref(data))
            return Image(width.value, height.value, bytes(rgba))
        finally:
            api.GdipDisposeImage(bitmap)

    return read


read = png_reader()

CROP = (328, 26, 352, 448)
GAP, HEADER, FOOTER = 16, 28, 20
WIDTH, HEIGHT = CROP[2]*2+GAP, CROP[3]+HEADER+FOOTER
BACKGROUND, LABEL, ACCENT = 0xFF1C1610, 0xFFDFD6C9, 0xFFF5E192
FONT = {
    'A': '01110/10001/10001/11111/10001/10001/10001',
    'B': '11110/10001/10001/11110/10001/10001/11110',
    'E': '11111/10000/10000/11110/10000/10000/11111',
    'F': '11111/10000/10000/11110/10000/10000/10000',
    'O': '01110/10001/10001/10001/10001/10001/01110',
    'R': '11110/10001/10001/11110/10100/10010/10001',
    'T': '11111/00100/00100/00100/00100/00100/00100',
    'S': '01111/10000/10000/01110/00001/00001/11110',
    '0': '01110/10001/10011/10101/11001/10001/01110',
    '1': '00100/01100/00100/00100/00100/00100/01110',
    '2': '01110/10001/00001/00010/00100/01000/11111',
    '3': '11110/00001/00001/01110/00001/00001/11110',
    '4': '00010/00110/01010/10010/11111/00010/00010',
    '5': '11111/10000/10000/11110/00001/00001/11110',
    '6': '01110/10000/10000/11110/10001/10001/01110',
    '7': '11111/00001/00010/00100/01000/01000/01000',
    '8': '01110/10001/10001/01110/10001/10001/01110',
    '9': '01110/10001/10001/01111/00001/00001/01110',
    '.': '00000/00000/00000/00000/00000/00100/00100',
    ' ': '00000/00000/00000/00000/00000/00000/00000',
}


def rows(image):
    x, y, w, h = CROP
    for row in range(y, y+h):
        begin = (row*image.width+x)*4
        yield image.rgba[begin:begin+w*4]


def label(pixels, text, center, y, color, scale=1):
    left = center-(len(text)*6-1)*scale//2
    for i, letter in enumerate(text):
        for yy, row in enumerate(FONT[letter].split('/')):
            for xx, value in enumerate(row):
                if value == '1':
                    for dy in range(scale):
                        start = (y+yy*scale+dy)*WIDTH+left+(i*6+xx)*scale
                        pixels[start:start+scale] = bytes([color])*scale


def compose(paths, indices, frame):
    pixels = bytearray([indices[BACKGROUND]])*(WIDTH*HEIGHT)
    for version, path in enumerate(paths):
        offset = version*(CROP[2]+GAP)
        for y, row in enumerate(rows(read(path))):
            start = (y+HEADER)*WIDTH+offset
            pixels[start:start+CROP[2]] = bytes(indices[value[0]] for value in struct.iter_unpack('<I', row))
        center = offset+CROP[2]//2
        label(pixels, 'BEFORE' if version == 0 else 'AFTER', center, 7,
              indices[LABEL if version == 0 else ACCENT], 2)
        label(pixels, f'{frame/30:.2f} S', center, HEADER+CROP[3]+6, indices[LABEL])
    return pixels


def lzw(pixels):
    """GIF LZW, with dictionary clears and the GIF decoder's code-size lag."""
    result = bytearray()
    bits = buffer = 0
    width, next_code = 9, 258
    table = {}

    def emit(value):
        nonlocal bits, buffer
        buffer |= value << bits
        bits += width
        while bits >= 8:
            result.append(buffer & 255)
            buffer >>= 8
            bits -= 8

    emit(256)
    prefix = pixels[0]
    for value in pixels[1:]:
        key = (prefix << 8) | value
        code = table.get(key)
        if code is not None:
            prefix = code
            continue
        emit(prefix)
        if next_code == 1 << width and width < 12:
            width += 1
        if next_code < 4096:
            table[key] = next_code
            next_code += 1
        else:
            emit(256)
            table.clear()
            width, next_code = 9, 258
        prefix = value
    emit(prefix)
    if next_code == 1 << width and width < 12:
        width += 1
    emit(257)
    if bits:
        result.append(buffer & 255)
    return result


def png(path, pixels, palette):
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data) & 0xffffffff)
    raw = bytearray()
    for y in range(HEIGHT):
        raw.append(0)
        for value in pixels[y*WIDTH:(y+1)*WIDTH]:
            raw.extend(palette[value*3:value*3+3])
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
                     +chunk(b'IDAT', zlib.compress(bytes(raw), 9))+chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/flood-texture')
    parser.add_argument('--allow-flow-changes', action='store_true', help='Allow intentionally revised wave motion at matched native ticks')
    args = parser.parse_args()
    folder = args.output.resolve()
    reports = [json.loads((folder/version/'state-digests.json').read_text()) for version in ('before', 'after')]
    identical=reports[0] == reports[1]
    assert identical or args.allow_flow_changes, 'Native states differ'
    assert [(r['frame'],r['tick']) for r in reports[0]] == [(r['frame'],r['tick']) for r in reports[1]], 'Native capture timing differs'
    pairs = [(folder/'before'/f'frame-{i:03d}.png', folder/'after'/f'frame-{i:03d}.png') for i in range(len(reports[0]))]
    colors = Counter({BACKGROUND: 1, LABEL: 1, ACCENT: 1})
    for pair in pairs:
        for path in pair:
            for row in rows(read(path)):
                colors.update(value[0] for value in struct.iter_unpack('<I', row))
    assert len(colors) <= 256, f'GIF needs quantization: {len(colors)} native colors'
    ordered = [key for key, _ in colors.most_common()]
    indices = {color: i for i, color in enumerate(ordered)}
    palette = b''.join(struct.pack('<I', color)[:3] for color in ordered).ljust(768, b'\x00')
    output = folder/'water-texture-before-after.gif'
    with output.open('wb') as stream:
        stream.write(b'GIF89a'+struct.pack('<HHBBB', WIDTH, HEIGHT, 0xf7, indices[BACKGROUND], 0)+palette)
        stream.write(b'\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00')
        for frame, pair in enumerate(pairs):
            pixels = compose(pair, indices, frame)
            if frame == len(pairs)//2:
                png(folder/'comparison.png', pixels, palette)
            delay = round((frame+1)*100/30)-round(frame*100/30)
            if frame+1 == len(pairs):
                delay += 40
            stream.write(b'\x21\xf9\x04'+struct.pack('<BHB', 4, delay, 0)+b'\x00')
            stream.write(b'\x2c'+struct.pack('<HHHHB', 0, 0, WIDTH, HEIGHT, 0)+b'\x08')
            compressed = lzw(pixels)
            for start in range(0, len(compressed), 255):
                block = compressed[start:start+255]
                stream.write(bytes([len(block)])+block)
            stream.write(b'\x00')
        stream.write(b'\x3b')
    report = {'gif': str(output), 'width': WIDTH, 'height': HEIGHT, 'frames': len(pairs),
              'fps': 30, 'native_colors': len(colors), 'file_bytes': output.stat().st_size,
              'simulation_identical': identical, 'matched_ticks': True, 'crop': CROP, 'dither': False, 'scaled': False}
    (folder/'comparison-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
