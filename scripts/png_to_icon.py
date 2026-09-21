#!/usr/bin/env python3
"""Convert any PNG into a multi-size .ico (stdlib only, no Pillow).

Usage:  python scripts/png_to_icon.py <input.png> [output.ico]

Decodes 8-bit non-interlaced RGB/RGBA PNGs, center-crops to a square,
bilinear-downscales to 16/24/32/48/256px, and packs a PNG-compressed .ico.
"""
import math
import os
import struct
import sys
import zlib

SIZES = (16, 24, 32, 48, 256)


def read_png(path):
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, w, h, depth, ctype, comp, filt, inter = 8, *[None] * 7
    raw = b""
    while pos < len(d):
        (ln,) = struct.unpack(">I", d[pos:pos + 4])
        tag = d[pos + 4:pos + 8]
        data = d[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h, depth, ctype, comp, filt, inter = struct.unpack(">IIBBBBB", data)
        elif tag == b"IDAT":
            raw += data
        elif tag == b"IEND":
            break
        pos += 12 + ln
    assert depth == 8 and inter == 0, "need 8-bit non-interlaced PNG"
    assert ctype in (2, 6), "need RGB or RGBA PNG, got type %d" % ctype
    raw = zlib.decompress(raw)
    ch = 3 if ctype == 2 else 4
    stride = w * ch
    out = bytearray(w * h * 4)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if f == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                b, c = prev[i], prev[i - ch] if i >= ch else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        elif f != 0:
            raise ValueError("bad filter %d" % f)
        for x in range(w):
            o = (y * w + x) * 4
            s = x * ch
            out[o:o + 3] = line[s:s + 3]
            out[o + 3] = line[s + 3] if ch == 4 else 255
        prev = line
    return w, h, out


def crop_square(w, h, px):
    s = min(w, h)
    ox, oy = (w - s) // 2, (h - s) // 2
    out = bytearray(s * s * 4)
    for y in range(s):
        src = ((oy + y) * w + ox) * 4
        out[y * s * 4:(y + 1) * s * 4] = px[src:src + s * 4]
    return s, out


def resize(s, px, n):
    out = bytearray(n * n * 4)
    for y in range(n):
        for x in range(n):
            fx = (x + 0.5) * s / n - 0.5
            fy = (y + 0.5) * s / n - 0.5
            x0 = max(0, min(s - 1, int(math.floor(fx))))
            y0 = max(0, min(s - 1, int(math.floor(fy))))
            x1 = max(0, min(s - 1, x0 + 1))
            y1 = max(0, min(s - 1, y0 + 1))
            tx, ty = min(1.0, max(0.0, fx - x0)), min(1.0, max(0.0, fy - y0))
            for k in range(4):
                a = px[(y0 * s + x0) * 4 + k]
                b = px[(y0 * s + x1) * 4 + k]
                c = px[(y1 * s + x0) * 4 + k]
                e = px[(y1 * s + x1) * 4 + k]
                out[(y * n + x) * 4 + k] = int(
                    a * (1 - tx) * (1 - ty) + b * tx * (1 - ty)
                    + c * (1 - tx) * ty + e * tx * ty + 0.5)
    return out


def encode_png(size, rgba):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(size):
        raw.append(0)
        for x in range(size):
            o = (y * size + x) * 4
            r, g, b, a = rgba[o], rgba[o + 1], rgba[o + 2], rgba[o + 3]
            raw.extend((r, g, b, a))
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(src)[0] + ".ico"
    w, h, px = read_png(src)
    s, sq = crop_square(w, h, px)
    print("source %dx%d -> square %d" % (w, h, s))
    blobs = [(n, encode_png(n, resize(s, sq, n))) for n in SIZES]
    header = struct.pack("<HHH", 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    entries, body = b"", b""
    for n, data in blobs:
        entries += struct.pack("<BBBBHHII", n if n < 256 else 0, n if n < 256 else 0,
                               0, 0, 1, 32, len(data), offset)
        offset += len(data)
        body += data
    with open(dst, "wb") as f:
        f.write(header + entries + body)
    print("wrote %s (%d bytes)" % (dst, os.path.getsize(dst)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
