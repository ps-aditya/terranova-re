#!/usr/bin/env python3
"""Dump the waypoint table from a Terra Nova mission resource.

    python tools/waypoints.py data/demo/TN1/DATA/MISS1D.RES
    python tools/waypoints.py data/demo/TN1/DATA/MISS1D.RES --plot out.png

Positions are 16.16 fixed point (see docs/MISSION.md).
"""

from __future__ import annotations

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lgres import parse  # noqa: E402

POSITIONS = 0x00B0
NAMES = 0x00B1
COORD_TABLE = 0x56
TERRAIN_N = 513


def waypoints(path: str):
    r = parse(path)
    by_id = {c.cid: c for c in r.chunks}
    names = by_id[NAMES].data()
    positions = by_id[POSITIONS].data()
    for i in range(128):
        name = names[i * 16:(i + 1) * 16].split(b"\x00")[0].decode("latin1")
        x, y = struct.unpack_from("<II", positions, i * 8)
        fx, fy = x / 65536.0, y / 65536.0
        if name or fx or fy:
            yield i, name, fx, fy


def plot(mission: str, map_path: str, out: str) -> None:
    from PIL import Image, ImageDraw

    r = parse(map_path)
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
    for index, name, x, y in waypoints(mission):
        colour = (255, 220, 60) if index < 16 else (255, 120, 255)
        draw.ellipse([x - 3, y - 3, x + 3, y + 3], outline=colour, width=2)
        draw.text((x + 5, y - 4), f"{index}: {name}"[:14], fill=(255, 255, 255))
    img.resize((900, 900), Image.LANCZOS).save(out)
    print(f"wrote {out}")


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    mission = argv[1]
    if "--plot" in argv:
        map_path = argv[argv.index("--plot") + 1]
        out = argv[argv.index("--plot") + 2] if len(argv) > argv.index("--plot") + 2 \
            else "waypoints.png"
        plot(mission, map_path, out)
        return 0
    print("  #  name              x         y")
    for index, name, x, y in waypoints(mission):
        print(f"{index:3d}  {name:<16} {x:8.3f} {y:8.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
