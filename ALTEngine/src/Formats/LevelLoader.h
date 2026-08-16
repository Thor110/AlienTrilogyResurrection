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

    // 28 bytes, indexed directly by a quad's flags byte (& 0x7f) - a
    // flag-8 quad uses light record 8. Supplies the quad's VERTEX COLOUR,
    // not its UVs: texture selection comes from texIndex as normal, and
    // this is the shading on top. Every quad in a level takes a colour
    // this way, so without it the whole level renders over-bright.
    struct LightRecord
    {
        uint8_t unlit[3]{};     // +0x00 RGB used during the off phase
        uint8_t lit[3]{};       // +0x04 RGB used during the on phase
        uint8_t corner2[3]{};   // +0x08 RGB for vertex 2, mode 4 only
        uint8_t corner3[3]{};   // +0x0c RGB for vertex 3, mode 4 only
        uint16_t blinkCountdown = 0; // +0x10 runtime
        uint16_t blinkRepeats = 0;   // +0x12 runtime, decrements per off phase
        uint16_t onDuration = 0;     // +0x14 base ticks, jittered at reload
        uint16_t offDuration = 0;    // +0x16 base ticks, jittered at reload
        uint8_t mode = 0;            // +0x18 0-5: 0 flat, 1 blink, 3 second variant, 4 gouraud
        uint8_t on = 0;              // +0x19 runtime on/off state
        uint8_t variant = 0;         // +0x1a advanced by ToggleLight
        uint8_t variantMax = 0;      // +0x1b blink only runs once variant reaches this
    };

    // Global light multiplier, 12.12-ish fixed point over 0xc00. The
    // brighter entry is a one-shot flash (weapon fire, explosion) that the
    // updater clears each pass.
    constexpr int LIGHT_GLOBAL_NORMAL = 3840; // x1.25
    constexpr int LIGHT_GLOBAL_FLASH  = 8192; // x2.667
    constexpr int LIGHT_GLOBAL_DIVISOR = 0xc00;

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
        std::vector<struct ActionGroup> actions;  // always exactly 64, unfiltered - a cell's action byte indexes this directly
        std::vector<struct LogicGroup> logics;    // always exactly 64 entries
        std::vector<LightRecord> lights;          // 128 entries of 28 bytes
        std::vector<uint32_t> unknownListA;       // header unknownBlockA entries of 4 bytes
        std::vector<uint32_t> unknownListB;       // header unknownBlockB entries of 4 bytes - the texture-animation frame lists
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
        uint8_t unknown5 = 0;    // confirmed always 0 on disk - runtime-only, entity+0x70 copied here (Ghidra: FUN_00032aec)
        uint8_t unknown6 = 0;
        uint8_t unknown7 = 0;
        uint8_t unknown8 = 0;    // confirmed always 0 on disk - runtime-only, unidentified
        uint8_t ceilingFog = 0;  // low byte of a u16 "visibility" field spanning +0x08/+0x09 per Ghidra - kept as two bytes here since nothing currently interprets it as one value
        uint8_t floorFog = 0;
        uint8_t attribute = 0;   // WAS misnamed ceilingHeight - Ghidra: GetFloorHeight's switch on this exact byte drives ramp/stair sub-height interpolation (0x2d-0x38) and the 0x13<a<0x2d blocking/sloped predicate
        uint8_t floorHeight = 0; // world Y = floorHeight * 32 (confirmed scale, Ghidra FUN_00027e28), modified by `attribute` for ramps/stairs - see LevelLoader::FindFloorHeight
        uint8_t unknown13 = 0;
        uint8_t ceilingHeight = 0; // WAS misnamed unknown14 - Ghidra FUN_00027fb0: world ceiling Y = this * 32, same scale as floorHeight
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
        // Indexed directly by a cell's action byte (+0x0f). Entry 0 is
        // unused. Confirmed via Ghidra: the game compacts these into a
        // working table keeping only the first three bytes.
        uint8_t activationMask = 0; // which trigger classes fire this: 0x04 = player walks onto the cell
        uint8_t commandStart = 0;   // first index into `logics`
        uint8_t enable = 0;         // must be non-zero or nothing fires
        uint8_t byte4 = 0;          // confirmed always 0
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

    // Floor height at grid cell (gridX, gridZ), confirmed via Ghidra
    // decompilation of the actual game's GetFloorHeight/FUN_00027e28
    // (Edward, 2026 - full formula supplied from a Ghidra deep-dive):
    //
    //   worldY = floorHeight * 32, then adjusted by `attribute` for
    //   ramps/stairs based on sub-cell position (fx, fz).
    //
    // Entities are spawned at exact grid-cell CORNERS - confirmed from
    // the same decompilation's spawn code: `ent.x = rec.x * 512 - bias`,
    // i.e. fx = fz = 0 relative to the cell - not the cell center. Since
    // Crate::x/y (and every other entity record) are already grid
    // indices rather than world coordinates, this takes grid indices
    // directly rather than a world position, matching how the real
    // spawn code actually calls it (no >>9 conversion needed - we
    // already have the grid index).
    //
    // Does NOT include the "+32, entities rest one floorHeight unit
    // above the floor" standing offset - callers add that themselves,
    // since it's specific to how an object should visually sit on the
    // surface, not part of the surface height itself.
    //
    // Supersedes the old quad-geometry search entirely: that heuristic
    // (picking the min, then the mode, of nearby quad heights) was
    // fundamentally guessing at something the real game reads directly
    // from a single collision cell byte plus a confirmed x32 scale -
    // this replaces guessing with the actual formula.
    float FindFloorHeight(const LevelGeometry& level, int gridX, int gridZ);

    // Height query in GRID SPACE, where cell = coord >> 9 and the
    // sub-cell position is coord & 0x1ff. Unlike the grid-index version
    // above (which always samples a cell corner), this evaluates the
    // stair and ramp attributes at the exact point given, so it is the
    // one to use for anything that moves.
    float FindFloorHeightGridSpace(const LevelGeometry& level, int gameX, int gameZ);

    // True if a mover cannot occupy this grid-space point: outside the
    // grid, a wall (bytes +0x02/+0x03 == 255), or an attribute in the
    // 0x14-0x2c blocking band. Stairs and ramps (0x2d+) are walkable and
    // return false - use the step-up limit against
    // FindFloorHeightGridSpace to reject rises that are too steep.
    // One height unit is 32 world units - FindFloorHeightGridSpace multiplies
    // the floor byte by 32, and a door moves 32 world units per step.
    inline constexpr int WORLD_UNITS_PER_HEIGHT_UNIT = 32;

    // A normal cell's ceiling is floor + 0x30. FUN_00027950 writes exactly that
    // when it builds the grid, so a standard room is 48 units - 1536 world
    // units - of headroom.
    inline constexpr int STANDARD_ROOM_CLEARANCE = 0x30;

    // How much vertical gap the player needs to fit through.
    //
    // The player is STAND_OFFSET + EYE_HEIGHT tall, 800 world units, which is 25
    // height units. That is the figure - not the 8 this used first.
    //
    // 8 came from FUN_000368c8, but that is the test for whether an EXPLOSION
    // can pass through a gap, not a person. Reusing it let the player through a
    // door that had lifted a third of the way (Edward, 2026). A blast squeezing
    // through a gap a player cannot is entirely reasonable behaviour for the
    // original to have; borrowing the number across was the mistake.
    //
    // 25 against a standard room's 48 leaves plenty of margin, so no ordinary
    // cell becomes impassable.
    inline constexpr int MIN_PASSABLE_CLEARANCE = 25;

    // The blast's own figure, kept separate now that they are known to differ.
    inline constexpr int MIN_BLAST_CLEARANCE = 8;

    bool IsCellBlocking(const LevelGeometry& level, int gameX, int gameZ);
}
