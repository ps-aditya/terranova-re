# `LG Res File v2`

Terra Nova uses the same container as System Shock 1. The container is
documented by the System Shock community (`ss1edit/doc/SPECS`); the payload
types below are what Terra Nova adds.

All numbers little-endian.

## Header (128 bytes)

```
0x00  char[124]  comment field, begins "LG Res File v2\r\n\x1A"
0x7C  u32        absolute offset to the table of contents
```

## Table of contents

```
+0x00  u16  chunk count
+0x02  u32  absolute offset of the first chunk
+0x06  entry[count]
```

Entry, 10 bytes:

```
+0x00  u16  chunk id
+0x02  u24  unpacked length
+0x05  u8   flags
+0x06  u24  packed length
+0x09  u8   content type
```

Chunks are packed in table order. **Each chunk starts 4-byte aligned**; the
original pads with zero bytes.

### Flags

```
0x01  compressed (not observed in the Terra Nova demo)
0x02  contains subchunks
```

### Subchunk directory

If `flags & 0x02`, the chunk body starts with:

```
+0x00  u16  subchunk count
+0x02  u32  offsets[count + 1]   relative to chunk start
```

The data begins immediately after the directory, so
`offsets[0] == 2 + 4 * (count + 1)` for every clean container checked so far.
A container with `count == 0` is legal and appears in the demo.

## Compression

Not present in the demo: every top-level chunk has `flags` 0 or 2. The 14-bit
dictionary scheme from the System Shock specification is implemented in
`tools/lgres.py` (`lz_decompress`) in case retail data uses it.

## Content types observed (demo, 1,252 chunks)

| Type | Name | Count | Status |
|------|------|-------|--------|
| 0x00 | raw / container | 387 | used as generic container |
| 0x01 | text | 42 | |
| 0x02 | bitmap / sprite | 357 | subchunk per frame |
| 0x03 | font | 20 | |
| 0x04 | video? | 3 | |
| 0x06 | **palette** | 15 | verified: 4,096 and 2,048-byte blocks = 16 / 8 palettes of 256 entries |
| 0x07 | sound | 378 | verified: Creative VOC |
| 0x08 | unknown (TN) | 7 | |
| 0x0C | unknown (TN) | 4 | |
| 0x0D | unknown (TN) | 1 | |
| 0x10 | unknown (TN) | 20 | |
| 0x12 | unknown (TN) | 17 | |
| 0x30 | map | 1 | container |

Types `0x08, 0x0C, 0x0D, 0x10, 0x12` are not in the System Shock
specification. They are the undocumented surface this project exists to map.

`0x00` is documented as "palette (raw data)" for System Shock, but Terra Nova
uses it as a generic container: 387 chunks, almost all with the subchunk flag.

## Open questions

- What do `0x08`, `0x0C`, `0x0D`, `0x10`, `0x12` contain?
- Is `0x02`'s 28-byte header identical to the System Shock bitmap header?
- Where are the compression flags in retail data, if anywhere?
- `0x04` (3 chunks) — is this video, or an animation container?
