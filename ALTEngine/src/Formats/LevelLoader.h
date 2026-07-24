#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "ModelLoader.h"

namespace ALTEngine::Formats
{
    // MAP0's header, in exact field order - confirmed against MapViewer.cs's
    // inline reads (36 bytes total: the 4 ExportLevel itself reads directly,
    // vertCount/quadCount, plus the 32 it then skips over - both sources
    // agree on the total). Everything past ventTypes belongs to the
    // gameplay entity lists (collision blocks, paths, monsters, pickups,
    // objects, doors, lifts, action sequences, lights) - not parsed yet,
    // that's a separate, much larger next step. See MapViewer.cs for the
    // full documented layout of each when that work starts.
    struct LevelHeader
    {
        uint16_t vertCount = 0;
        uint16_t quadCount = 0;
        uint16_t mapLength = 0;
        uint16_t mapWidth = 0;
        uint16_t playerStartX = 0;
        uint16_t playerStartY = 0;
        uint8_t pathCount = 0;
        uint8_t lightCount = 0;          // "UNKNOWN (128 on all levels)" per MapViewer.cs - kept as a real field anyway, not hardcoded, in case that's not universally true
        uint16_t monsterCount = 0;
        uint16_t pickupCount = 0;
        uint16_t objectCount = 0;
        uint16_t doorCount = 0;
        uint16_t liftCount = 0;
        uint16_t playerStartAngle = 0;
        uint16_t unknownBlockA = 0;       // unknownListA size = this * 4
        uint16_t unknownBlockB = 0;       // unknownListB size = this * 4
        uint16_t enemyTypes = 0;          // bitmask - which enemy types are loaded into memory for this level
        uint16_t ventTypes = 0;           // bitmask - per MapViewer.cs, likely unused leftover editor metadata
    };

    struct LevelGeometry
    {
        LevelHeader header;
        std::vector<ModelVertex> vertices;
        std::vector<ModelQuad> quads;
    };

    // Parses a level's MAP0 section: header + vertex/quad geometry only
    // (see LevelHeader's comment - the gameplay entity lists that follow
    // in the file are a separate, not-yet-built next step).
    //
    // Two format quirks worth calling out explicitly, both confirmed
    // against ModelRenderer.cs's ExportLevel:
    //   - Vertices use the exact same 8-byte layout as M0 models
    //     (X,Y,Z Int16 + 2 padding bytes) - reuses ModelVertex directly.
    //   - Quads use the exact same 20-byte layout as M0 models too, BUT
    //     parsing starts at quad index 1, not 0 - the first quad is
    //     always a FF FF FF FF sentinel and must be skipped, per
    //     ExportLevel's own comment ("start at 1 to avoid the final face
    //     which is always FF FF FF FF" - the comment says "final" but the
    //     loop clearly skips the FIRST index, so that's followed as
    //     written in the code, not as described in the comment).
    //
    // CONFIRMED against the real L111LEV.MAP (Edward, 2026):
    //   - Three header fields matched independently-documented ground
    //     truth values exactly: lightCount=128 ("always 128" per
    //     MapViewer.cs), doorCount=6 ("6 -> 6 doors in L111LEV.MAP" per
    //     MapViewer.cs's own loop comment), and enemyTypes=0x0022
    //     (matches MapViewer.cs's documented per-level table exactly).
    //   - 96.8% of parsed quad edges are axis-aligned (X or Z
    //     near-constant), consistent with a grid-based PS1-era level
    //     layout - not something garbage/misparsed data would produce.
    //   - Vertex Y is entirely non-negative ([0, 3584]) while X/Z span
    //     both directions, consistent with Y being the vertical/height
    //     axis (floor at 0) and X/Z the horizontal ground plane.
    //
    // This surfaced one real fix: the file also has a TRAILING
    // degenerate sentinel quad (a=b=c=d=-1, texIndex=0xFFFF) as the very
    // last entry, mirroring the leading one - not handled by
    // ExportLevel's own loop (which only skips the first), and not
    // something worth replicating, since ExportLevel never actually
    // validates vertex indices before writing them to OBJ text, so this
    // case would silently produce a broken/invalid face there instead of
    // erroring the way a GPU buffer builder would. Now explicitly
    // skipped (see the loop below - checks a == -1, since unlike d,
    // 'a' is never legitimately -1 for real geometry).
    //
    // Not yet verified against a level with lifts (L111LEV has 0), or
    // against the multi-BX-group UV resolution specifically (needs the
    // paired 111GFX.B16, not yet available) - still only
    // self-consistency-tested for that part, see RenderMesh.h.
    class LevelLoader
    {
    public:
        static LevelGeometry Load(const std::filesystem::path& mapPath);
    };
}
