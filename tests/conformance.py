#!/usr/bin/env python3
"""Conformance test: the C tool must agree with the Python reference parser.

    python tests/conformance.py [path-to-lgres]

Exit code 0 means every chunk in every demo resource file matched.
"""

from __future__ import annotations

import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from lgres import TYPE_NAMES, parse, subchunks  # noqa: E402

BINARY = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "lgres.exe")
DATA = os.path.join(ROOT, "data", "demo", "TN1", "DATA")


def expected(path: str) -> list[str]:
    r = parse(path)
    lines = [f"== {path} ({len(r.chunks)} chunks) =="]
    for i, c in enumerate(r.chunks):
        flags = ("C" if c.compressed else "-") + ("S" if c.has_subchunks else "-")
        subs = len(subchunks(c)) if c.has_subchunks else 0
        lines.append(
            f"{i:4d} id=0x{c.cid:04X} {flags} type=0x{c.type:02X} "
            f"{TYPE_NAMES.get(c.type, '?'):<8} packed={c.packed_len:6d} "
            f"unpacked={c.unpacked_len:6d} subs={subs}"
        )
    return lines


def main() -> int:
    files = sorted(glob.glob(os.path.join(DATA, "*.RES")))
    if not files:
        print(f"no .RES files under {DATA}; run scripts/fetch_demo.py", file=sys.stderr)
        return 2
    if not os.path.exists(BINARY):
        print(f"missing {BINARY}; build it first", file=sys.stderr)
        return 2

    failures = 0
    chunks = 0
    for path in files:
        want = expected(path)
        got = subprocess.run([BINARY, "list", path], capture_output=True,
                             text=True, check=False).stdout.splitlines()
        chunks += len(want) - 1
        if want != got:
            failures += 1
            print(f"FAIL {os.path.basename(path)}: {len(want)-1} vs {len(got)-1} lines")
            for a, b in zip(want, got):
                if a != b:
                    print(f"  want: {a}\n  got:  {b}")
                    break
        else:
            print(f"ok   {os.path.basename(path)} ({len(want)-1} chunks)")

    print(f"\n{len(files) - failures}/{len(files)} files, {chunks} chunks matched")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
