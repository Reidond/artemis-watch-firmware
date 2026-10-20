#!/usr/bin/env python3
"""Renders the companion app icon from the watch face's art.

    python3 tools/gen_app_icon.py

A 128x128 pixel scene (dithered Earth with a day/night line, the Moon, the
orbit and the red craft, as on the Orbit watch face) scaled 8x with square
pixels into ios/Artemis/Assets.xcassets/AppIcon.appiconset/icon-1024.png.
Needs tools/.art-src/ne_110m_land.geojson (run tools/gen_art.py first).
Pure standard library.
"""
import math
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_art  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "ios", "Artemis", "Assets.xcassets", "AppIcon.appiconset", "icon-1024.png")
N = 128
BLACK, GREY, WHITE, RED = (0, 0, 0), (0x80, 0x80, 0x80), (0xF2, 0xF2, 0xF2), (0xFC, 0x3D, 0x21)
BAYER = [
    [0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21],
]


def ramp(x, y, v):
    """Dither 0..1 through black, grey, white (firmware dither::grey)."""
    v = max(0.0, min(1.0, v)) * 2
    base = min(int(v), 2)
    if base == 2:
        return WHITE
    frac = int((v - base) * 64)
    colors = (BLACK, GREY, WHITE)
    return colors[base + 1] if frac > BAYER[y & 7][x & 7] else colors[base]


def smoothstep(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def main():
    land = gen_art.rasterize(gen_art.load_land())
    img = [[BLACK] * N for _ in range(N)]

    def put(x, y, c):
        if 0 <= x < N and 0 <= y < N:
            img[y][x] = c

    # Earth: seen over Europe and Africa, lit from the right like late afternoon.
    ex, ey, er = 50, 74, 38
    lat0, lon0 = math.radians(28), math.radians(22)
    sun = (math.radians(-10), math.radians(70))
    for py in range(-er, er + 1):
        for px in range(-er, er + 1):
            x, y = (px + 0.5) / er, -(py + 0.5) / er
            rho = math.hypot(x, y)
            if rho > 1:
                continue
            c = math.asin(min(rho, 1.0))
            lat = lat0 if rho < 1e-6 else math.asin(math.cos(c) * math.sin(lat0) + y * math.sin(c) * math.cos(lat0) / rho)
            lon = lon0 + math.atan2(x * math.sin(c), rho * math.cos(c) * math.cos(lat0) - y * math.sin(c) * math.sin(lat0))
            s = math.sin(lat) * math.sin(sun[0]) + math.cos(lat) * math.cos(sun[0]) * math.cos(lon - sun[1])
            day = smoothstep(-0.12, 0.12, s)
            row = min(127, int((90 - math.degrees(lat)) * 128 / 180))
            col = int(((math.degrees(lon) + 540) % 360) * 256 / 360) % 256
            is_land = land[row][col]
            lit, dark = (0.95, 0.24) if is_land else (0.40, 0.0)
            put(ex + px, ey + py, ramp(ex + px, ey + py, dark + (lit - dark) * day))
    for i in range(8 * er):
        a = i * 2 * math.pi / (8 * er)
        put(ex + round((er + 0.5) * math.cos(a)), ey + round((er + 0.5) * math.sin(a)), GREY)

    # Orbit: dashed ellipse from behind Earth out around the Moon.
    cx, cy, a, b, tilt = 66, 66, 60, 22, -0.55
    travelled, prev = 0.0, None
    for i in range(1441):
        t = i * 2 * math.pi / 1440
        x = cx + a * math.cos(t) * math.cos(tilt) - b * math.sin(t) * math.sin(tilt)
        y = cy + a * math.cos(t) * math.sin(tilt) + b * math.sin(t) * math.cos(tilt)
        if prev:
            travelled += math.hypot(x - prev[0], y - prev[1])
        prev = (x, y)
        if travelled % 6 >= 4:
            continue
        behind = math.sin(t) < 0 and (x - ex) ** 2 + (y - ey) ** 2 < er * er
        if not behind:
            put(int(x), int(y), WHITE)

    # Moon, full and plain at this size, upper right.
    mx, my, mr = 104, 28, 13
    for py in range(-mr, mr + 1):
        for px in range(-mr, mr + 1):
            if px * px + py * py <= mr * mr:
                shade = 0.88 - 0.25 * smoothstep(0.2, 1.0, math.hypot(px + 4, py + 2) / mr)
                put(mx + px, my + py, ramp(mx + px, my + py, shade))

    # Red craft on the orbit between them.
    t = math.radians(-60)
    x = cx + a * math.cos(t) * math.cos(tilt) - b * math.sin(t) * math.sin(tilt)
    y = cy + a * math.cos(t) * math.sin(tilt) + b * math.sin(t) * math.cos(tilt)
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            put(int(x) + dx, int(y) + dy, RED)

    scale = 8
    raw = bytearray()
    for row in img:
        line = bytearray([0])
        for c in row:
            line += bytes(c) * scale
        raw += bytes(line) * scale
    size = N * scale

    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(png)
    print(OUT)


if __name__ == "__main__":
    main()
