#!/usr/bin/env python3
"""lgres.py - explorer for Looking Glass 'LG Res File v2' containers.

The container format is documented by the System Shock 1 community
(ss1edit/doc/SPECS). Terra Nova uses the same container with its own chunk
types; see docs/FORMAT.md.

Usage:
    python tools/lgres.py info  <file.res> [...]
    python tools/lgres.py list  <file.res> [-v]
    python tools/lgres.py stats <directory>
    python tools/lgres.py cat   <file.res> <chunk-id> [-o out.bin]
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
from dataclasses import dataclass, field
from typing import Iterator

MAGIC = b"LG Res File v2\r\n\x1a"
HEADER_SIZE = 128
TOC_PTR_OFF = 0x7C

FLAG_COMPRESSED = 0x01
FLAG_SUBCHUNKS = 0x02

# Content types. 0x00-0x04, 0x07, 0x0F, 0x11 and 0x30 come from the
# System Shock 1 specification. Types marked TN are seen in Terra Nova and are
# not documented there.
TYPE_NAMES = {
    0x00: "raw",
    0x01: "text",
    0x02: "bitmap",
    0x03: "font",
    0x04: "video?",
    0x06: "palette",
    0x07: "sound",
    0x08: "tn08",
    0x0C: "tn0C",
    0x0D: "tn0D",
    0x0F: "model3d",
    0x10: "tn10",
    0x11: "audio-log",
    0x12: "tn12",
    0x30: "map",
}


def u24(buf: bytes, off: int) -> int:
    return buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16)


def align4(n: int) -> int:
    return (n + 3) & ~3


@dataclass
class Chunk:
    cid: int
    unpacked_len: int
    flags: int
    packed_len: int
    type: int
    data_offset: int                 # absolute offset of packed bytes
    _file: bytes = field(repr=False, default=b"")
    _data: bytes | None = field(default=None, repr=False)

    @property
    def compressed(self) -> bool:
        return bool(self.flags & FLAG_COMPRESSED)

    @property
    def has_subchunks(self) -> bool:
        return bool(self.flags & FLAG_SUBCHUNKS)

    def packed(self) -> bytes:
        return self._file[self.data_offset:self.data_offset + self.packed_len]

    def data(self) -> bytes:
        """Return the chunk payload, decompressed if needed."""
        if self._data is None:
            raw = self.packed()
            if self.compressed:
                self._data = lz_decompress(raw, self.unpacked_len)
            else:
                self._data = raw
        return self._data


@dataclass
class ResFile:
    path: str
    size: int
    toc_offset: int
    first_chunk: int
    chunks: list[Chunk]
    blob: bytes = field(repr=False, default=b"")

    @property
    def name(self) -> str:
        return os.path.basename(self.path)


def lz_decompress(src: bytes, expected: int) -> bytes:
    """Decompress the Looking Glass 14-bit dictionary stream.

    Follows the algorithm documented in ss1edit/doc/SPECS section 3.4.
    """
    out = bytearray()
    pos = 0
    bitbuf = 0
    nbits = 0
    offsets = [0] * 16384
    lengths = [1] * 16384
    origins = [-1] * 16384
    ntokens = 0

    def next_word() -> int | None:
        nonlocal pos, bitbuf, nbits
        while nbits < 14:
            if pos >= len(src):
                return None
            bitbuf = (bitbuf << 8) | src[pos]
            pos += 1
            nbits += 8
        nbits -= 14
        return (bitbuf >> nbits) & 0x3FFF

    while len(out) < expected:
        val = next_word()
        if val is None or val == 0x3FFF:
            break
        if val == 0x3FFE:
            lengths[:] = [1] * 16384
            origins[:] = [-1] * 16384
            ntokens = 0
            continue

        if ntokens < 16384:
            offsets[ntokens] = len(out)
            if val >= 0x100:
                origins[ntokens] = val - 0x100
            ntokens += 1

        if val < 0x100:
            out.append(val)
            continue

        idx = val - 0x100
        if lengths[idx] == 1:
            origin = origins[idx]
            if origin != -1:
                lengths[idx] += lengths[origin]
            else:
                lengths[idx] += 1
        start = offsets[idx]
        for i in range(lengths[idx]):
            out.append(out[start + i])

    if len(out) < expected:
        out.extend(b"\x00" * (expected - len(out)))
    return bytes(out)


def parse(path: str) -> ResFile:
    blob = open(path, "rb").read()
    if len(blob) < HEADER_SIZE or not blob.startswith(MAGIC):
        raise ValueError(f"{path}: not an LG Res v2 file")

    toc = struct.unpack_from("<I", blob, TOC_PTR_OFF)[0]
    if toc + 6 > len(blob):
        raise ValueError(f"{path}: TOC offset out of range")

    count, first = struct.unpack_from("<HI", blob, toc)
    chunks: list[Chunk] = []
    cursor = first
    for i in range(count):
        off = toc + 6 + i * 10
        cid = struct.unpack_from("<H", blob, off)[0]
        unpacked = u24(blob, off + 2)
        flags = blob[off + 5]
        packed = u24(blob, off + 6)
        ctype = blob[off + 9]
        if cursor + packed > len(blob):
            raise ValueError(f"{path}: chunk {i} runs past EOF")
        chunks.append(Chunk(cid, unpacked, flags, packed, ctype, cursor, blob))
        cursor = align4(cursor + packed)

    return ResFile(path, len(blob), toc, first, chunks, blob)


def subchunks(chunk: Chunk) -> list[tuple[int, int]]:
    """Return [(offset, length)] for a subchunk container."""
    data = chunk.data()
    if len(data) < 6:
        return []
    count = struct.unpack_from("<H", data, 0)[0]
    if count == 0 or count > 4096:
        return []
    need = 2 + 4 * (count + 1)
    if need > len(data):
        return []
    bounds = list(struct.unpack_from(f"<{count + 1}I", data, 2))
    out = []
    for i in range(count):
        start, end = bounds[i], bounds[i + 1]
        if start > end or end > len(data):
            return []
        out.append((start, end - start))
    return out


def walk(chunks: list[Chunk], depth: int = 0, limit: int = 8) -> Iterator[tuple[int, Chunk]]:
    for c in chunks:
        yield depth, c
        if c.has_subchunks and depth < limit:
            yield from walk_sub(c, depth + 1, limit)


def walk_sub(chunk: Chunk, depth: int, limit: int) -> Iterator[tuple[int, Chunk]]:
    data = chunk.data()
    for i, (off, length) in enumerate(subchunks(chunk)):
        sub = Chunk(chunk.cid, length, 0, length, 0xFF, off, data)
        yield depth, sub
        if sub.has_subchunks and depth < limit:  # normally false
            yield from walk_sub(sub, depth + 1, limit)


def cmd_info(args) -> int:
    for path in args.files:
        r = parse(path)
        print(f"{r.name}: {r.size} bytes, {len(r.chunks)} chunks, toc={r.toc_offset}")
        types: dict[int, int] = {}
        for c in r.chunks:
            types[c.type] = types.get(c.type, 0) + 1
        for t, n in sorted(types.items()):
            print(f"  type 0x{t:02X} {TYPE_NAMES.get(t, '?'):10s} {n}")
    return 0


def cmd_list(args) -> int:
    for path in args.files:
        r = parse(path)
        print(f"== {r.name} ({len(r.chunks)} chunks) ==")
        for i, c in enumerate(r.chunks):
            tag = TYPE_NAMES.get(c.type, "?")
            flags = ("C" if c.compressed else "-") + ("S" if c.has_subchunks else "-")
            extra = ""
            if args.verbose and c.has_subchunks:
                extra = f" subchunks={len(subchunks(c))}"
            print(f"{i:4d} id=0x{c.cid:04X} {flags} type=0x{c.type:02X} "
                  f"{tag:10s} packed={c.packed_len:8d} unpacked={c.unpacked_len:8d}{extra}")
    return 0


def cmd_stats(args) -> int:
    files = 0
    chunks = 0
    subs = 0
    compressed = 0
    by_type: dict[int, int] = {}
    decompressed = 0
    failures = 0
    for entry in sorted(os.listdir(args.directory)):
        if not entry.lower().endswith(".res"):
            continue
        path = os.path.join(args.directory, entry)
        try:
            r = parse(path)
        except ValueError as exc:
            print(f"FAIL parse {entry}: {exc}")
            failures += 1
            continue
        files += 1
        chunks += len(r.chunks)
        for c in r.chunks:
            by_type[c.type] = by_type.get(c.type, 0) + 1
            if c.compressed:
                compressed += 1
                try:
                    d = c.data()
                except Exception as exc:  # noqa: BLE001
                    print(f"FAIL decompress {entry} id=0x{c.cid:04X}: {exc}")
                    failures += 1
                    continue
                decompressed += len(d)
            if c.has_subchunks:
                n = len(subchunks(c))
                if n == 0:
                    print(f"WARN empty subdir {entry} id=0x{c.cid:04X}")
                subs += n
    print(f"files={files} chunks={chunks} subchunks={subs} "
          f"compressed={compressed} decompressed={decompressed} failures={failures}")
    for t, n in sorted(by_type.items()):
        print(f"  type 0x{t:02X} {TYPE_NAMES.get(t, '?'):10s} {n}")
    return 1 if failures else 0


def cmd_cat(args) -> int:
    r = parse(args.file)
    chunk = next((c for c in r.chunks if c.cid == args.chunk_id), None)
    if chunk is None:
        print(f"no chunk with id 0x{args.chunk_id:04X}", file=sys.stderr)
        return 2
    data = chunk.data()
    if args.output:
        open(args.output, "wb").write(data)
        print(f"wrote {len(data)} bytes to {args.output}")
    else:
        sys.stdout.buffer.write(data)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("info")
    p.add_argument("files", nargs="+")
    p.set_defaults(func=cmd_info)

    p = sub.add_parser("list")
    p.add_argument("files", nargs="+")
    p.add_argument("-v", "--verbose", action="store_true")
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("stats")
    p.add_argument("directory")
    p.set_defaults(func=cmd_stats)

    p = sub.add_parser("cat")
    p.add_argument("file")
    p.add_argument("chunk_id", type=lambda s: int(s, 0))
    p.add_argument("-o", "--output")
    p.set_defaults(func=cmd_cat)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
