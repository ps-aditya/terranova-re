# Bitmap frames

Status: **closed**. All 1,194 frames in the demo decode with zero errors.

## Frame header (28 bytes)

Verified against every frame. Confirms the System Shock bitmap header, with two
Terra Nova additions.

```
+0x00  int32   always 0
+0x04  int16   codec: 2 = raw, 4 = RLE, 5 = raw (alternate)
+0x06  int16   variant flag (0 or 1)
+0x08  int16   width
+0x0A  int16   height
+0x0C  int16   width again
+0x0E  int8    log2(width)
+0x0F  int8    log2(height)
+0x10  int16   centering, x
+0x12  int16   centering, y
+0x14  int16   centering, x
+0x16  int16   centering, y
+0x18  int32   stream length or 0
```

Every bitmap chunk (`type 0x02`) is a subchunk container: one subchunk per
animation frame. Even single-frame chunks use a one-entry container.

## Codecs

| Codec | Encoding | Evidence |
|---|---|---|
| 2 | raw, 1 byte per pixel | payload length == `width * height` |
| 4 | RLE | payload length < `width * height` |
| 5 | raw, 1 byte per pixel | payload length == `width * height` |

Codec 5 is not a different compression. On every frame checked its payload is
exactly `width * height` bytes, so it is probably an animation flag that happens
to be stored raw.

## RLE

```
00 nn xx               repeat xx nn times
01..7F <nn bytes>      copy nn literal bytes
80 00 00               end of stream
80 mm nn  (nn < 80)    skip nn*256+mm transparent pixels
80 mm nn vv (nn >= 80) fill ((nn & 3F) << 8 | mm) pixels with vv
81..FF                 skip (op & 7F) transparent pixels
```

The long opcode is a **fill**, not a literal copy. The System Shock
specification describes this case as "copy N bytes direct", which is wrong for
Terra Nova. Two independent confirmations:

- On the small frames the decoded count equals the image size exactly
  (`0x0CC38 = 3128 = 184*17`, `0x05FA = 1530 = 90*17`,
  `0x09F4 = 2548 = 182*14`), and the byte after the opcode is the fill colour.
- With the fill rule, all 604 RLE streams terminate at exactly `width * height`
  and consume the whole payload. With the literal rule, 79 streams overran.

Trailing pixels are transparent and are zero filled. A stream that ends on an
explicit `80 00 00` before `width * height` pixels is normal: it means the rest
of the frame is transparent.

## Results

```
files=18 bitmap_chunks=357 frames=1194 raw=590 rle=604 errors=0
decoded_pixels=7969767
```

The decoded screens are structurally coherent — panels, buttons and scrollbars
appear in the expected places — which is what confirmed the fill rule. Rendered
frames are not committed; regenerate them locally with `tools/render_bitmap.py`
or `tools/anim.py`.

Two subchunks are not frames: `DEMOGAM1 0x015F` frame 0 declares 12×12 but
carries 676 bytes (26×26), and `RESPLNT0 0x0042` frame 0 declares 2×2 but
carries 173 bytes. Both have an internal length at `+0x18` and look like
embedded metadata rather than images. The decoder consumes the declared
`width * height` pixels and ignores the rest.

## Palette: open

The frames are 8-bit indices, but **no RGB palette is stored in the resource
files**. What is known:

- There are no 768-byte chunks in any of the 18 demo files.
- `RESPLNT0.RES` chunk `0x2C` is 1,024 bytes and mostly grey; chunks `0x35`,
  `0x36`, `0x37`, `0x39`, `0x3A`–`0x41` are 4,096 / 2,048 / 1,280 / 256 bytes
  and behave like **shade tables** (index remaps), not colour.
- The sprite indices cluster in tight groups: the tree uses only 23 distinct
  indices, in four runs (183–186, 202–206, 216–219, 246). That is the signature
  of 16-entry material blocks, so the palette is likely built from ramps or
  lives in the executable.
- Chunk `0x43` contains the masks `0xFFFF, 0x000F, 0xF0F0, 0x7FFF`, i.e. RGB555
  handling exists in the engine.

A scan of `_TNDEMO.EXE` for 768-byte palettes (3- and 4-byte strides, smooth
neighbours, block consistency) found only grey/shade tables. The palette is
therefore either generated at load time or stored in a form not yet identified.

This does not affect the frame format: an index image is the complete
information the file contains. Rendering the real colours needs the palette,
and that is the next open problem.
