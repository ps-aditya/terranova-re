#!/usr/bin/env python3
"""Render decoded bitmap frames.

    python tools/render_bitmap.py <file.res> <chunk-id> <sub-index> <out.png>
    python tools/render_bitmap.py <file.res> <chunk-id> <sub-index> <out.png> --palette pal.rgb

Without a palette the frame is written as an index image. With `--palette` the
file must be 768 bytes of RGB triples.

`--blocks` colours each 16-entry palette block with its own hue and keeps the
index as brightness. That is a diagnostic of the material layout, not the
game's real colours.
"""

from __future__ import annotations

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lgres import parse, subchunks  # noqa: E402


def decode_rle(src: bytes, width: int, height: int) -> bytes:
    out = bytearray()
    i = 0
    n = width * height
    while len(out) < n and i < len(src):
        op = src[i]
        i += 1
        if op == 0:
            if i + 2 > len(src):
                break
            count, value = src[i], src[i + 1]
            i += 2
            out += bytes([value]) * min(count, n - len(out))
        elif op < 0x80:
            if i + op > len(src):
                break
            take = min(op, n - len(out))
            out += src[i:i + take]
            i += op
        elif op == 0x80:
            if i + 2 > len(src):
                break
            lo, hi = src[i], src[i + 1]
            i += 2
            if lo == 0 and hi == 0:
                break
            if hi < 0x80:
                out += b"\x00" * min(hi * 256 + lo, n - len(out))
            else:
                count = ((hi & 0x3F) << 8) | lo
                if i >= len(src):
                    break
                value = src[i]
                i += 1
                out += bytes([value]) * min(count, n - len(out))
        else:
            out += b"\x00" * min(op & 0x7F, n - len(out))
    out += b"\x00" * (n - len(out))
    return bytes(out[:n])


def decode_frame(data: bytes) -> tuple[int, int, int, bytes]:
    if len(data) < 28:
        raise ValueError("short frame header")
    codec = struct.unpack_from("<h", data, 4)[0]
    width = struct.unpack_from("<h", data, 8)[0]
    height = struct.unpack_from("<h", data, 10)[0]
    payload = data[28:]
    if codec in (2, 5):
        pixels = payload[:width * height].ljust(width * height, b"\x00")
    elif codec == 4:
        pixels = decode_rle(payload, width, height)
    else:
        raise ValueError(f"unknown codec {codec}")
    return width, height, codec, pixels


def block_palette() -> list[tuple[int, int, int]]:
    import colorsys
    pal = []
    for i in range(256):
        block = i >> 4
        shade = (i & 0x0F) / 15.0
        hue = (block * 0.61803398875) % 1.0
        r, g, b = colorsys.hsv_to_rgb(hue, 0.75, 0.25 + 0.75 * shade)
        pal.append((int(r * 255), int(g * 255), int(b * 255)))
    return pal


def main(argv: list[str]) -> int:
    if len(argv) < 5:
        print(__doc__)
        return 2
    path, chunk_id, sub, out = argv[1], int(argv[2], 0), int(argv[3]), argv[4]
    palette = None
    blocks = "--blocks" in argv
    if "--palette" in argv:
        raw = open(argv[argv.index("--palette") + 1], "rb").read()
        if len(raw) != 768:
            raise SystemExit("palette must be exactly 768 bytes")
        palette = [tuple(raw[i * 3:i * 3 + 3]) for i in range(256)]
    if blocks:
        palette = block_palette()

    r = parse(path)
    chunk = next((c for c in r.chunks if c.cid == chunk_id), None)
    if chunk is None:
        raise SystemExit(f"no chunk 0x{chunk_id:04X}")
    if chunk.has_subchunks:
        bounds = subchunks(chunk)
        off, length = bounds[sub]
    else:
        off, length = 0, len(chunk.data())
    width, height, codec, pixels = decode_frame(chunk.data()[off:off + length])
    print(f"0x{chunk_id:04X}[{sub}] {width}x{height} codec={codec}")

    from PIL import Image
    if palette:
        img = Image.new("RGB", (width, height))
        img.putdata([palette[v] for v in pixels])
    else:
        img = Image.frombytes("L", (width, height), pixels)
    img.resize((width * 4, height * 4), Image.NEAREST).save(out)
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
