# Map resources (`MAP50A.RES`)

`MAP50A.RES` is the demo mission. It holds 39 top-level chunks. All of them are
type `0x00`; 30 are subchunk containers.

## Inventory

| Chunk | Size | Interpretation | Confidence |
|-------|------|----------------|------------|
| `0x50` | 54 | world header: sizes 64/192/128, filename `resplnt0.res` | high |
| `0x51` | 2,189 | unknown | — |
| `0x52` | 14 | unknown; values 2048, 4096, 46000, 20000 | low |
| `0x53` | 16,384 | 128×128 grid, six values; coherent black regions | medium (detail/scatter map) |
| `0x54` | 480 | 15 × 32-byte names: `Tile set 1`, `Tile set 3`, … | high |
| `0x55` | 198,147 | **3 × 257²** terrain vertices | high |
| `0x56` | 789,507 | **3 × 513²** terrain vertices | high |
| `0x57` | 128 | all zero | — |
| `0x5F` | 8 | four u16: 0x0185, 0x019D, 0x018A, 0x01A0 | low |
| `0x78`–`0x7C` | 276–438 | 50-byte records, 8/5/5/5/5 entries | medium (entity tables) |
| `0x7D`–`0x8F` | 6 | empty containers (`count = 0`) | verified |

## Terrain

`0x55` and `0x56` are the same terrain at two levels of detail:

```
0x55:  198,147 = 3 × 257²
0x56:  789,507 = 3 × 513²
```

Each vertex is three bytes. Only the **third byte** is terrain height:

| chunk | channel | unique | mean Δx | mean Δy |
|-------|---------|--------|---------|---------|
| 0x56 | 0 | 83 | 48.38 | 42.97 |
| 0x56 | 1 | 192 | 77.16 | 76.80 |
| 0x56 | 2 | 69 | **0.49** | **0.55** |
| 0x55 | 2 | 122 | 3.15 | 3.87 |

A mean adjacent delta of 0.49 on an 8-bit channel is a height field; 48–77 is
noise. Visual confirmation: `docs/img/terrain_map50a.png` — the island, the
river/canyon network, and the pits read correctly.

Channels 0 and 1 are still unknown. Candidates: per-vertex material index and
shading/light level. They are *not* coordinates: a regular grid does not need
to store x/y, and neither channel is spatially coherent.

## Next

1. Determine grid orientation (row-major, origin corner) by matching a
   distinctive terrain feature against an in-game screenshot.
2. Decode `0x50` field-by-field; `resplnt0.res` is present in the demo, so the
   header can be validated against a real file.
3. Decode the 50-byte records in `0x78`–`0x7C` as entity/unit tables.
4. Find where the terrain *textures* live (`resplnt0.res`, type `0x02`).
