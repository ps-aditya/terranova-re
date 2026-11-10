#!/usr/bin/env python3
"""Render an animation bank to a sprite sheet and an animated GIF.

    python tools/anim.py <file.res> <chunk-id> sheet.png anim.gif

Frames are 8-bit index images. Without the palette they are drawn as grey;
`--blocks` colours each 16-entry palette block instead.
"""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lgres import parse, subchunks  # noqa: E402
from render_bitmap import block_palette, decode_frame  # noqa: E402


def frames(path: str, chunk_id: int):
    r = parse(path)
    chunk = next((c for c in r.chunks if c.cid == chunk_id), None)
    if chunk is None:
        raise SystemExit(f"no chunk 0x{chunk_id:04X}")
    data = chunk.data()
    bounds = subchunks(chunk) if chunk.has_subchunks else [(0, len(data))]
    for off, length in bounds:
        try:
            yield decode_frame(data[off:off + length])
        except ValueError:
            continue


def main(argv: list[str]) -> int:
    if len(argv) < 5:
        print(__doc__)
        return 2
    path, chunk_id, sheet_path, gif_path = argv[1], int(argv[2], 0), argv[3], argv[4]
    blocks = "--blocks" in argv

    from PIL import Image

    palette = block_palette() if blocks else None
    decoded = list(frames(path, chunk_id))
    if not decoded:
        raise SystemExit("no frames")

    cell_w = max(w for w, _, _, _ in decoded)
    cell_h = max(h for _, h, _, _ in decoded)
    scale = max(1, min(4, 512 // max(cell_w, cell_h)))
    sheet = Image.new("RGB", (cell_w * scale * len(decoded), cell_h * scale), (24, 24, 24))
    images = []
    for i, (w, h, codec, pixels) in enumerate(decoded):
        img = Image.new("RGB", (w, h), (24, 24, 24))
        if palette:
            img.putdata([palette[v] for v in pixels])
        else:
            img.putdata([(40 + v * 3 // 4,) * 3 for v in pixels])
        sheet.paste(img.resize((w * scale, h * scale), Image.NEAREST),
                    (i * cell_w * scale, 0))
        images.append(img.resize((w * 2, h * 2), Image.NEAREST))

    sheet.save(sheet_path)
    print(f"wrote {sheet_path} ({len(decoded)} frames, {cell_w}x{cell_h})")

    canvas_w = max(im.width for im in images)
    canvas_h = max(im.height for im in images)
    gif = [Image.new("RGB", (canvas_w, canvas_h), (24, 24, 24)) for _ in images]
    for bg, im in zip(gif, images):
        bg.paste(im, ((canvas_w - im.width) // 2, (canvas_h - im.height) // 2))
    gif[0].save(gif_path, save_all=True, append_images=gif[1:], duration=120, loop=0)
    print(f"wrote {gif_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
