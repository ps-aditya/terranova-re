#!/usr/bin/env python3
"""Regression check for the bitmap frame decoder.

Asserts that every frame in the demo decodes without error and that the totals
stay stable. Run after `python scripts/fetch_demo.py` and a build.

    python tests/frames.py [path-to-lgres]
"""

from __future__ import annotations

import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "lgres.exe")
DATA = os.path.join(ROOT, "data", "demo", "TN1", "DATA")

EXPECTED_FRAMES = 1194
EXPECTED_RAW = 590
EXPECTED_RLE = 604
EXPECTED_PIXELS = 7969767


def main() -> int:
    files = sorted(glob.glob(os.path.join(DATA, "*.RES")))
    if not files:
        print(f"no .RES files under {DATA}", file=sys.stderr)
        return 2
    if not os.path.exists(BINARY):
        print(f"missing {BINARY}; build it first", file=sys.stderr)
        return 2

    out = subprocess.run([BINARY, "bmpscan", *files], capture_output=True,
                         text=True, check=False).stdout
    print(out.strip())
    m = re.search(r"frames=(\d+) raw=(\d+) rle=(\d+) errors=(\d+) "
                  r"decoded_pixels=(\d+)", out)
    if not m:
        print("could not parse bmpscan output", file=sys.stderr)
        return 1
    frames, raw, rle, errors, pixels = map(int, m.groups())
    checks = [
        ("frames", frames, EXPECTED_FRAMES),
        ("raw", raw, EXPECTED_RAW),
        ("rle", rle, EXPECTED_RLE),
        ("errors", errors, 0),
        ("pixels", pixels, EXPECTED_PIXELS),
    ]
    failed = False
    for name, got, want in checks:
        ok = got == want
        failed |= not ok
        print(f"{'ok  ' if ok else 'FAIL'} {name}: {got} (expected {want})")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
