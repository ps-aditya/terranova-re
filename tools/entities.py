#!/usr/bin/env python3
"""Dump the 50-byte entity records from a Terra Nova map resource.

    python tools/entities.py data/demo/TN1/DATA/MAP50A.RES
    python tools/entities.py data/demo/TN1/DATA/MAP50A.RES --plot out.png

The record layout is described in docs/ENTITIES.md. Fields marked `?` are
observed but not yet identified.
"""

from __future__ import annotations

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lgres import parse, subchunks  # noqa: E402

RECORD_SIZE = 50
FIRST_TABLE = 0x78
LAST_TABLE = 0x8F
COORD_TABLE = 0x56
TERRAIN_N = 513


def unpack_coord(value: int) -> tuple[int, int]:
    """18-bit coordinate: x in the low 9 bits, y in the next 9."""
    return value & 0x1FF, (value >> 9) & 0x1FF


def records(path: str):
    r = parse(path)
    by_id = {c.cid: c for c in r.chunks}
    for cid in range(FIRST_TABLE, LAST_TABLE + 1):
        chunk = by_id.get(cid)
        if chunk is None or not chunk.has_subchunks:
            continue
        for index, (off, length) in enumerate(subchunks(chunk)):
            if length != RECORD_SIZE:
                continue
            b = chunk.data()[off:off + length]
            a = b[8] | (b[9] << 8) | (b[10] << 16)
            c = b[12] | (b[13] << 8) | (b[14] << 16)
            yield {
                "table": cid,
                "index": index,
                "team": b[1],
                "unknown_a": struct.unpack_from("<H", b, 2)[0],
                "unknown_b": struct.unpack_from("<H", b, 4)[0],
                "from": unpack_coord(a),
                "to": unpack_coord(c),
                "unknown_c": struct.unpack_from("<H", b, 24)[0],
                "raw": b,
            }


def plot(path: str, out: str) -> None:
    from PIL import Image, ImageDraw

    r = parse(path)
    by_id = {c.cid: c for c in r.chunks}
    terrain = by_id[COORD_TABLE].data()
    n = TERRAIN_N
    img = Image.new("RGB", (n, n))
    px = img.load()
    for y in range(n):
        for x in range(n):
            z = terrain[(y * n + x) * 3 + 2]
            px[x, y] = (z // 3 + 40, z // 2 + 30, z // 4 + 20)
    draw = ImageDraw.Draw(img)
    colours = [(255, 60, 60), (80, 255, 80), (80, 180, 255),
               (255, 255, 80), (255, 80, 255)]
    for rec in records(path):
        colour = colours[rec["team"] % len(colours)]
        ax, ay = rec["from"]
        bx, by = rec["to"]
        draw.line([ax, ay, bx, by], fill=colour, width=1)
        draw.ellipse([ax - 2, ay - 2, ax + 2, ay + 2], fill=colour)
        draw.rectangle([bx - 2, by - 2, bx + 2, by + 2], outline=colour)
    img.resize((768, 768), Image.LANCZOS).save(out)
    print(f"wrote {out}")


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    if "--plot" in argv:
        plot(path, argv[argv.index("--plot") + 1])
        return 0
    print("tbl   idx team  from        to          ?@2     ?@4     ?@24")
    for rec in records(path):
        ax, ay = rec["from"]
        bx, by = rec["to"]
        print(f"0x{rec['table']:02X}  {rec['index']:3d}  {rec['team']:2d}  "
              f"({ax:3d},{ay:3d})  ({bx:3d},{by:3d})  "
              f"{rec['unknown_a']:5d}  {rec['unknown_b']:5d}  {rec['unknown_c']:5d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
