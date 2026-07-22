# Split-frame textures

The following GFX entries are stored as two frames within their file
rather than one contiguous image:

- LEGAL
- LOGOSGFX
- COLONY
- PRISHOLD
- BONESHIP

Reference dumps: LEGAL_TP00.png / LEGAL_TP01.png.

All external-palette-driven (`palfile = true` in GraphicsViewer.cs); only
BONESHIP/COLONY/PRISHOLD are additionally `trimmed` (see PaletteFile) -
LEGAL and LOGOSGFX are not.

## Confirmed against the real LEGAL.BND + LEGAL.PAL (2026-07-21)

- Each `TP00`/`TP01` chunk is a raw 256x256 8bpp buffer, no sub-header
  (chunk size exactly 65536 = 256*256).
- Final image is **320x240** (native PS1 output resolution) - **not** a
  symmetric 240+240/480x240 as first assumed. Per the file's own `BX`
  metadata: frame A (TP00) contributes 240x240, frame B (TP01)
  contributes only **80x240**. `SplashImageLoader` reads these crop
  dimensions from each file's BX0/BX1 rects rather than hardcoding them,
  specifically because they're asymmetric and there's no guarantee the
  other four images match LEGAL's exact numbers.
- Padding (outside each frame's own BX-defined visible rect) is
  uniformly palette index 0, which never appears within real content.
- Round-tripped against the real LEGAL.BND/.PAL end to end: correct
  320x240 disclaimer-text image, real colours, no seam artifacts.

BX rects also carry a `textureIndex` field (which TP frame each rect
belongs to) - used here to match rect->frame, not assumed to be
positional.

## Important note re: dimensions

Width/height come from BX section metadata, not the embedded CL palette
data. CL sections contain only colour information (256 x 16-bit RGB555) -
don't go looking for size fields in there.

## Not yet confirmed

Whether LOGOSGFX/COLONY/PRISHOLD/BONESHIP use the same 240/80 split as
LEGAL, or their own numbers - SplashImageLoader reads each file's own BX
metadata per-image specifically so this isn't assumed, but the actual
values haven't been checked against those four files yet.
