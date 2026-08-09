# Ghidra reference notes

Addresses and values confirmed from the decompilation while working on other
things. Kept here so they don't have to be re-found. **Everything in this file
is a reading of the disassembly, not a verified behaviour** unless it says
otherwise — check against real data before building on any of it.

## Texture descriptors

| Symbol | What |
|---|---|
| `FUN_00018bcc` | Descriptor builder, **level path**. Expands EVERY `BX` chunk in file order with no tag filter. Confirms the port's flat-index scheme. |
| `FUN_00018fb4` | Descriptor builder, **menu/model path**. Same expansion but FILTERS to BX chunks whose tag digits equal the requested set number. Probably the source of the old "cumulative subtraction" confusion. |
| `DAT_00242608` | Level descriptor array. 16-byte stride, max 0x200 entries. |
| `DAT_002458e2` | Descriptor count. Found by scanning `+0x06` of each entry for zero. 339 for L111. |
| `DAT_002458d8` | Points at whichever descriptor array is currently bound for drawing. |
| `DAT_002408c0` | Descriptor set loaded from resource **0x7f / 0x83 / 0x84 / 0x85**, selected per episode → **enemy** graphics. |
| `DAT_00244608` | Descriptor set loaded from resource **0x7d**, same on every level → **object / pickup** graphics. |
| `DAT_002405a0` | Descriptor set loaded from resource **0x8f**. Unidentified. |

Runtime 16-byte descriptor layout, built from the 6-byte on-disk BX rect:

```
+0x00 u0  +0x01 v0    +0x02..03 CLUT id   (from BX rect byte 4)
+0x04 u1  +0x05 v1    +0x06..07 TPage id  (from the BX CHUNK's tag digits)
+0x08 u2  +0x09 v2
+0x0c u3  +0x0d v3    +0x0e..0f CLUT id again
```

Corners are `(x,y) (x+w,y) (x,y+h) (x+w,y+h)` with **no +1** on width/height —
inclusive coordinates. The port's `+1` exclusive convention samples the same
texels, so it is correct for a GPU sampler; don't "fix" it.

## Level face drawing

| Symbol | What |
|---|---|
| `FUN_00025648` | Level face walker. Stride `0x14`. `+0x10` texIndex (bounds-checked), `+0x13` → 35-entry draw-routine table, `+0x12 & 0x7f` → light LUT, loop ends on `+0x12 & 0x80`. **Runtime byte order is the reverse of on-disk.** |
| `FUN_000256ac` | Same walker, but takes its light LUT entry from **byte `+0x0e` of the entity record** and applies it to every face. This is how doors/crates/pickups/monsters are lit. |
| `0x000a7098` | 35-entry rasterizer dispatch table. Slot N at `0x000a7098 + N*4`. Draw routine **8** (animated faces) is at **`0x000a70b8`** — still not decompiled. |
| `FUN_000256ac` / `FUN_000269b8` / `FUN_000260cc` | Three face-walk variants selected by distance. Almost certainly LOD: near / mid / far. Thresholds differ per caller (`0x400`, `0x600`, `0x800`, `0x1000`). |

## Lighting

| Symbol | What |
|---|---|
| `DAT_00245d18` | Live light record table, 28-byte stride, 128 entries. |
| `DAT_002458dc` | Resolved colour LUT, **16-byte stride**, one entry per record, four RGB corners. What the draw routines actually read. |
| `FUN_00029be0` | Installs a level's lights: copies records and builds the resolved LUT. Mode 4 puts `lit` in corner 0 and `unlit` in corner 1 (note the swap). |
| `FUN_00029d44` | ToggleLight. `variant += delta`, clamped to `variantMax` at `+0x1b`. |
| `FUN_0004ed58` | Applies the global multiplier: `(channel * global) / 0xc00`, clamped to 255. |
| `FUN_0004ecdc` | **Distance fog / attenuation.** `factor = 0xff - clamp(dist/0x50 - 0xaa, 0, 0xff)`, then `colour * factor >> 8`. Not implemented in the port — this is the next visual step after lighting. |
| `LAB_00029d90` | Per-tick light updater. Still never lifted into a function. |
| `DAT_0040024c` | Set to `0xf00` (3840) — the ×1.25 global multiplier. |

## Object / entity scale

Three sibling draw functions that do **not** agree on scale. All gate on
`DAT_000a6164 == 1` and a visibility mask from the mesh header at `+0x06`.

| Symbol | Scale | Descriptor set | Notes |
|---|---|---|---|
| `FUN_0003765c` | `0xe00` uniform = **0.875** | `DAT_00244608`, or `DAT_002408c0` for type `0x14` on levels `0x16`–`0x22` | **Objects / crates / switches.** Type `0x17` is skipped entirely → scale 1.0. |
| `FUN_000377e4` | `0xc00 / 0xd98 / 0xc00` = 0.75 / 0.849609375 / 0.75 | `DAT_002408c0` | **Enemies** (episode-dependent texture set). |
| `FUN_00037930` | none | — | Allows `dist > -0x2c0`, i.e. partly behind the camera. Ceiling-mounted types? |
| `FUN_0003c6f4` | per-type table at **`DAT_000acbb8`**, stride 16, indexed by entity `+0x10` | `DAT_00244608` | **Pickups.** Ends with `param_1[9] += 0x10` — the confirmed spin rate. |

`FUN_00047800(matrix, scaleTriple)` is the scale-apply. `FUN_00047758`,
`FUN_00048104`, `FUN_000477d0`, `FUN_00047f3c`, `FUN_00047ba0` are the
surrounding matrix ops (translate / concat / push / pop) — not individually
identified yet.

## Still unlocated / wanted

- **HUD.** `FUN_0003e4a8` is the damage function; what it decrements is still
  untraced. `PNL0GFXU.16` / `PNL1GFXU.16` are the panel graphics (68172 bytes
  each, identical size — almost certainly a language or episode pair).
- **Text overlay.** `FUN_000572e4` is `PlayVoiceCue`, NOT `ShowMessage`. The
  real text system is unlocated. Strings likely in resources 389–395.
- **Draw routine 8's body** at `0x000a70b8` — the one remaining gap in the
  animated-face chain.
- **`DAT_000b0ca8`** is the current level id; it appears in level-range tests
  all over the draw code (`0x16`–`0x22` in particular). Useful for any
  per-episode behaviour.
- **`DAT_000b0b90` / `DAT_000b0b88`** are the transformed distance and a second
  extent used for both culling (`< 0x2000`) and LOD selection.
