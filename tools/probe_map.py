#!/usr/bin/env python3
"""Probe a Terra Nova map resource and render its heightfield.

    python tools/probe_map.py data/demo/TN1/DATA/MAP50A.RES

Requires Pillow only for the PNG output; the report works without it.
"""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lgres import TYPE_NAMES, parse, subchunks  # noqa: E402


def main(path: str) -> int:
    r = parse(path)
    print(f"{r.name}: {len(r.chunks)} chunks, {r.size} bytes")

    by_id = {c.cid: c for c in r.chunks}
    for c in r.chunks:
        flags = ("C" if c.compressed else "-") + ("S" if c.has_subchunks else "-")
        subs = f" subs={len(subchunks(c))}" if c.has_subchunks else ""
        print(f"  0x{c.cid:04X} {flags} type=0x{c.type:02X} {TYPE_NAMES.get(c.type, '?'):8s} "
              f"packed={c.packed_len:8d}{subs}")

    # Both terrain LODs are 3 bytes per vertex on a square grid.
    for cid in (0x55, 0x56):
        chunk = by_id.get(cid)
        if chunk is None:
            continue
        data = chunk.data()
        n = round((len(data) / 3) ** 0.5)
        if n * n * 3 != len(data):
            continue
        print(f"\nterrain 0x{cid:02X}: {n}x{n} vertices")
        for ch in range(3):
            v = data[ch::3]
            dx = sum(abs(v[y * n + x] - v[y * n + x + 1])
                     for y in range(n) for x in range(n - 1)) / (n * (n - 1))
            print(f"  channel {ch}: uniq={len(set(v)):3d} mean|dx|={dx:6.2f}")

        try:
            from PIL import Image
        except ImportError:
            continue
        z = data[2::3]
        lo, hi = min(z), max(z)
        img = Image.new("L", (n, n))
        img.putdata([(v - lo) * 255 // max(1, hi - lo) for v in z])
        out = os.path.splitext(os.path.basename(path))[0] + f"_height_{n}.png"
        img.resize((768, 768)).save(out)
        print(f"  wrote {out}")

    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        raise SystemExit(2)
    raise SystemExit(main(sys.argv[1]))
