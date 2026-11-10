# Campaign and simulation data

Two more resources complete the mission layer.

## `DEMOMIS1.RES` — operation list

29 chunks, 8,312 bytes.

| Chunk | Subchunks | Contents |
|---|---|---|
| `0x06AA` | 47 | operation names, null-terminated |
| `0x06A7` | 48 | 47 × 18-byte records (`00 01 01 01 …`) plus one 1-byte pad |
| `0x06A8` | 246 | 6-byte records, separated by 1-byte pads |
| `0x06A9` | 42 | 6-byte records, with pads |
| `0x06B7` | 32 | 20-byte records |
| `0x06B8` | 6 | 5-byte records |

The 47 operation names:

```
 0 OPERATION MADISON           24 OPERATION PIGPEN
 1 OPERATION BRICKDRAGGER      25 OPERATION HYPERBOLE
 2 OPERATION FLASHLIGHT        26 OPERATION CALAIS
 3 OPERATION CITADEL           27 this mission no longer exists
 4 OPERATION VALKYRIE          28 OPERATION GOBLIN
 5 OPERATION EAGLE'S NEST      29 OPERATION ICEPICK
 6 OPERATION MARINER           30 OPERATION ABSOLUTE ZERO
 7 OPERATION PERISCOPE         31 OPERATION METEOR
 8 OPERATION IVANHOE           32 OPERATION METEORITE
 9 OPERATION OILSLICK          33 OPERATION METEOR SHOWER
10 OPERATION MARKET SQUARE     34 OPERATION BUZZARD
11 OPERATION EGGSHELL          35 OPERATION CROPDUSTER
12 OPERATION CONDUCTOR         36 OPERATION CANNONBALL
13 OPERATION MERCURY           37 OPERATION PANAMA
14 OPERATION UNDERTOW          38 OPERATION COLD WAR
15 OPERATION SERAPHIM          39 OPERATION BRICKLAYER
16 OPERATION IVY               40 (empty)
17 this mission no longer exists
18 OPERATION BLOODLINE         41 RANDOM RAGE
19 OPERATION BLACK WIDOW       42 DEFEND
20 OPERATION AEGIS             43 ATTACK
21 OPERATION SPARKPLUG         44 (empty)
22 OPERATION CADMIUM           45 XR-5 QUALIFICATION
23 OPERATION FUSEBOX           46 COMBAT CERTIFICATION
```

`0x06A7` has one 18-byte row per operation. Most bytes are `1`, with occasional
`2`; that is consistent with a per-operation parameter preset (difficulty,
available units, or branch flags) rather than free-form data. Not confirmed.

`0x06A8` and `0x06A9` are lists of 6-byte records, `(a, b, c, d, e, f)`, with
1-byte separators between runs. Example records:

```
05 04 06 03 06 01
05 00 06 00 06 00
04 07 06 03 03 01
04 07 06 08 06 00
```

The first three bytes look like a class/type/limit triple, but the encoding is
not established.

## `RESSIM.RES` — simulation tables

40 chunks, 10,530 bytes. Two mirrored groups of parallel arrays.

| Group A | Count | Entry | Group B | Count | Entry |
|---|---|---|---|---|---|
| `0x0385` | 18 | 3 bytes | `0x0397` | 18 | 70 bytes |
| `0x0386` | 12 | 3 bytes | `0x0398` | 12 | 54 bytes |
| `0x0387` | 17 | 3 bytes | `0x0399` | 17 | — |
| `0x0388` | 74 | 3 bytes | `0x039A` | 74 | — |

So the engine has **121 object definitions in four categories** (18 / 12 / 17 /
74). The 3-byte arrays hold small values that look like costs:

```
0x0385: 3000, 3500, 4000, 750, 900, …
0x0386: 1000, 1000, 1500, …
0x0387:  100,  600,  300, …
```

The 70-byte records in `0x0397` start with values that increase across the
table (`1000, 1100, 1250, 1000, 1000, …`), followed by u16/u32 fields. They are
the per-unit stat blocks; individual field semantics are not established.

Two groups with the same category counts suggest the same schema is stored twice
— most likely one copy per side or per game mode. That is a hypothesis, not a
confirmed fact.

## Relation to the entity records

The 121 object definitions are the natural candidates for the unknown entity
fields, but none of the observed values (`30, 64, 40192, 1542, 1285, …`) fall
inside `0..120`, so `?A/?B/?C` are not plain type indices. They are more likely
packed parameter words.

## Tooling

```sh
python tools/waypoints.py data/demo/TN1/DATA/MISS1D.RES
```
