#!/usr/bin/env python3
"""Generate assets/yarn.ico — a distinctive yarn-ball tray icon.

No dependencies (stdlib only): renders 16/24/32/48px RGBA yarn balls,
PNG-encodes them by hand, and packs a Vista-style PNG-compressed .ico.
Re-run:  python scripts/make_icon.py
"""
import math
import os
import struct
import zlib

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "assets", "yarn.ico")

BASE = (209, 73, 91)      # wool red
DARK = (110, 30, 42)      # outline
STRAND = (242, 164, 136)  # light peach yarn
GLOSS = (255, 255, 255)


def render(size):
    c = (size - 1) / 2.0
    r = size * 0.42
    px = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            dx, dy = x - c, y - c
            d = math.hypot(dx, dy)
            if d > r:
                continue
            # base wool with soft vertical shading
            shade = 0.88 + 0.24 * (1.0 - (y / max(size - 1, 1)))
            rr = min(255, int(BASE[0] * shade))
            gg = min(255, int(BASE[1] * shade))
            bb = min(255, int(BASE[2] * shade))
            aa = 255
            if d > r - max(1.2, size * 0.045):
                rr, gg, bb = DARK  # outline
            else:
                # three wavy wrap strands
                for k in (-1, 0, 1):
                    y0 = k * r * 0.42
                    curve = y0 + math.sin(dx / r * math.pi) * r * 0.14
                    if abs(dy - curve) < max(0.9, size * 0.028):
                        rr, gg, bb = STRAND
                        break
                # gloss highlight, upper left
                hx, hy = -r * 0.38, -r * 0.42
                if math.hypot(dx - hx, dy - hy) < r * 0.20:
                    rr, gg, bb = (min(255, (rr + GLOSS[0]) // 2),
                                  min(255, (gg + GLOSS[1]) // 2),
                                  min(255, (bb + GLOSS[2]) // 2))
            o = (y * size + x) * 4
            px[o:o + 4] = bytes((bb, gg, rr, aa))  # BGRA for PNG rows below
    # PNG rows need RGBA; swap B/R back
    rows = bytearray()
    for y in range(size):
        rows.append(0)
        for x in range(size):
            o = (y * size + x) * 4
            b, g, r, a = px[o], px[o + 1], px[o + 2], px[o + 3]
            rows.extend((r, g, b, a))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    pngs = [(s, render(s)) for s in (16, 24, 32, 48)]
    header = struct.pack("<HHH", 0, 1, len(pngs))
    offset = 6 + 16 * len(pngs)
    entries, blobs = b"", b""
    for size, data in pngs:
        w = size if size < 256 else 0
        entries += struct.pack("<BBBBHHII", w, w, 0, 0, 1, 32,
                               len(data), offset)
        offset += len(data)
        blobs += data
    with open(OUT, "wb") as f:
        f.write(header + entries + blobs)
    print("wrote %s (%d bytes)" % (OUT, os.path.getsize(OUT)))


if __name__ == "__main__":
    main()
