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
        std::vector<struct CollisionNode> collisionGrid;  // mapLength*mapWidth entries, row-major
        std::vector<struct PathNode> pathNodes;
        std::vector<struct Monster> monsters;
        std::vector<struct Pickup> pickups;
        std::vector<struct Crate> crates;
        std::vector<struct Door> doors;
        std::vector<struct Lift> lifts;
        std::vector<struct ActionGroup> actions;  // always 64 slots, but only ones with actionType != 0 are kept - matches AlienTrilogyMapLoader.cs's own filtering
        std::vector<struct LogicGroup> logics;    // always exactly 64 entries
    };

    // 16 bytes. One per grid cell, mapLength*mapWidth of them, row-major -
    // confirmed against AlienTrilogyMapLoader.cs's CollisionNode/its
    // read loop. unknown3/unknown4 are confirmed always 255 (wall) or 0
    // (traversable) across every level in the game - the only two
    // fields with a clearly-established gameplay meaning so far besides
    // floor/ceiling height.
    struct CollisionNode
    {
        uint8_t unknown1 = 0;
        uint8_t unknown2 = 0;
        uint8_t unknown3 = 0;    // 255 = wall, 0 = traversable (confirmed, only ever these two values)
        uint8_t unknown4 = 0;    // 255 = wall, 0 = traversable (confirmed, only ever these two values)
        uint8_t unknown5 = 0;    // confirmed always 0
        uint8_t unknown6 = 0;
        uint8_t unknown7 = 0;
        uint8_t unknown8 = 0;    // confirmed always 0
        uint8_t ceilingFog = 0;
        uint8_t floorFog = 0;
        uint8_t ceilingHeight = 0;
        uint8_t floorHeight = 0; // the "entities fall to the floor" answer - this is where that height comes from
        uint8_t unknown13 = 0;
        uint8_t unknown14 = 0;
        uint8_t lighting = 0;
        uint8_t scriptAction = 0; // indexes into `actions`
    };

    // 8 bytes. count = header.pathCount.
    struct PathNode
    {
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t unused = 0;      // confirmed always 0
        uint8_t nodeState = 0;   // 0 = active, 2 = inactive pending action, 3 = one-way only
        uint8_t nodeA = 0;
        uint8_t nodeB = 0;
        uint8_t nodeC = 0;
        uint8_t nodeD = 0;
    };

    // 20 bytes. count = header.monsterCount. Type/Rotation value tables
    // in the .cpp.
    struct Monster
    {
        uint8_t type = 0;
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t z = 0;        // confirmed NOT a height - "only used on L222LEV for the monsters that spawn after first flame vent closed" (Edward, 2026)
        uint8_t rotation = 0; // 8-direction compass: 0=N,1=NE,2=E,3=SE,4=S,5=SW,6=W,7=NW (45 degrees per step)
        uint8_t health = 0;
        uint8_t drop = 0;     // index of object dropped on death
        uint8_t unknown2 = 0;
        uint8_t difficulty = 0; // 0=Easy, 1=Medium, 2=Hard
        uint8_t unknown4 = 0;
        uint8_t unknown5 = 0;
        uint8_t unknown6 = 0;
        uint8_t unknown7 = 0;
        uint8_t unknown8 = 0;
        uint8_t speed = 0;
        uint8_t unknown9 = 0;  // confirmed always 0
        uint8_t unknown10 = 0;
        uint8_t unknown11 = 0;
        uint8_t unknown12 = 0;
        uint8_t unknown13 = 0;
    };

    // 8 bytes. count = header.pickupCount. Type value table in the .cpp.
    struct Pickup
    {
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t type = 0;
        uint8_t amount = 0;
        uint8_t multiplier = 0;
        uint8_t unknown1 = 0; // confirmed always 0
        uint8_t z = 0;        // confirmed only ever 0 or 1 - not a real height either
        uint8_t unknown2 = 0; // "always the same as amount for ammunition" (Edward, 2026)
    };

    // 16 bytes. count = header.objectCount ("boxCount" in the C#
    // reference). Type value table in the .cpp - no explicit Z field,
    // needs the collision grid's floorHeight like doors do.
    struct Crate
    {
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t type = 0;
        uint8_t drop = 0;     // 0 = pickup, 2 = enemy
        uint8_t unknown1 = 0;
        uint8_t unknown2 = 0; // confirmed only ever 0 or 10
        uint8_t drop1 = 0;    // index of first pickup dropped
        uint8_t drop2 = 0;    // index of second pickup dropped
        uint8_t unknown3 = 0;
        uint8_t unknown4 = 0;
        uint8_t unknown5 = 0;
        uint8_t unknown6 = 0;  // confirmed always 0
        uint8_t unknown7 = 0;
        uint8_t unknown8 = 0;  // confirmed always 0
        uint8_t rotation = 0;  // 4-direction: 0=N,2=E,4=S,6=W (only even values - 90 degrees per step, unlike Monster's 8-direction scheme)
        uint8_t unknown10 = 0; // confirmed always 0
    };

    // 8 bytes. count = header.doorCount. No explicit Z field - needs the
    // collision grid's floorHeight, same as Crate.
    struct Door
    {
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t unknown = 0;    // confirmed only ever 64 or 0
        uint8_t time = 0;       // open time - 0 means manually closed at the collider
        uint8_t lockState = 0;  // 1=unlocked, 2=locked, shootable = number of shots
        uint8_t unknown2 = 0;   // confirmed always 0
        uint8_t rotation = 0;   // 4-direction: 0=N,2=E,4=S,6=W, same scheme as Crate
        uint8_t modelIndex = 0; // index into the door model BND
    };

    // 16 bytes. count = header.liftCount. Has real X/Y/Z, unlike
    // Door/Crate - no floor-lookup needed.
    struct Lift
    {
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t z = 0;
        uint8_t unknown1 = 0;
        uint8_t unknown2 = 0;  // confirmed always 0
        uint8_t unknown3 = 0;
        uint8_t unknown4 = 0;  // confirmed always 1
        uint8_t unknown5 = 0;
        uint8_t unknown6 = 0;
        uint8_t unknown7 = 0;
        uint8_t unknown8 = 0;  // for shootables, number of shots - "must correspond with action byte 3" (Edward, 2026)
        uint8_t unknown9 = 0;  // confirmed always 0
        uint8_t unknown10 = 0;
        uint8_t unknown11 = 0; // confirmed always matches unknown12/unknown13
        uint8_t unknown12 = 0;
        uint8_t unknown13 = 0;
    };

    // 4 bytes, always exactly 64 slots read - only ones with
    // actionType != 0 are kept (matches AlienTrilogyMapLoader.cs's own
    // filtering). Referenced by CollisionNode::scriptAction.
    struct ActionGroup
    {
        uint8_t actionType = 0; // 0=no action, 1=standard, 2=shootable, 3=mission(?)
        uint8_t logicStep = 0;  // index into `logics`
        uint8_t byte3 = 0;      // possibly repeatable/activation count
        uint8_t byte4 = 0;      // confirmed always 0
    };

    // 4 bytes, always exactly 64 entries (all kept, unlike ActionGroup).
    struct LogicGroup
    {
        uint8_t action = 0;    // 0=lighting change,1=door unlock,2=pickup toggle,3=monster toggle,4=switch inactive,5=locked-lift operate,6=door open,7=lift operate,8=end level,9=path node activate
        uint8_t nextStep = 0;  // 1=next step in sequence, 255=stop
        uint8_t modifier = 0;
        uint8_t objectIndex = 0;
    };

    // Parses a level's MAP0 section: header, vertex/quad geometry, and
    // the full gameplay entity lists that follow (collision grid, path
    // nodes, monsters, pickups, crates, doors, lifts, action/logic
    // groups) - all confirmed against Edward's AlienTrilogyMapLoader.cs
    // (2026), a real, working Unity port's own binary reader, including
    // its per-list byte-size "formula" comments.
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
