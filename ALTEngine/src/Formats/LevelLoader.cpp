#include "LevelLoader.h"
#include "BinaryReadLE.h"
#include "BndParser.h"

#include <fstream>
#include <stdexcept>

namespace ALTEngine::Formats
{
    namespace
    {
        std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open())
            {
                throw std::runtime_error("LevelLoader: could not open " + path.string());
            }
            auto size = static_cast<size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            std::vector<uint8_t> data(size);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
            return data;
        }
    }

    LevelGeometry LevelLoader::Load(const std::filesystem::path& mapPath)
    {
        std::vector<uint8_t> file = ReadFile(mapPath);
        std::vector<BndSection> mapSections = BndParser::ParseFormSections(file, "MAP0");
        if (mapSections.empty())
        {
            throw std::runtime_error("LevelLoader: no MAP0 section found in " + mapPath.string());
        }
        const std::vector<uint8_t>& data = mapSections[0].data;

        constexpr size_t HEADER_SIZE = 36;
        if (data.size() < HEADER_SIZE)
        {
            throw std::runtime_error("LevelLoader: MAP0 section too small for its header in " + mapPath.string());
        }

        LevelGeometry result;
        LevelHeader& h = result.header;
        size_t pos = 0;

        h.vertCount = ReadUInt16LE(data, pos); pos += 2;
        h.quadCount = ReadUInt16LE(data, pos); pos += 2;
        h.mapLength = ReadUInt16LE(data, pos); pos += 2;
        h.mapWidth = ReadUInt16LE(data, pos); pos += 2;
        h.playerStartX = ReadUInt16LE(data, pos); pos += 2;
        h.playerStartY = ReadUInt16LE(data, pos); pos += 2;
        h.pathCount = data[pos]; pos += 1;
        h.lightCount = data[pos]; pos += 1;
        h.monsterCount = ReadUInt16LE(data, pos); pos += 2;
        h.pickupCount = ReadUInt16LE(data, pos); pos += 2;
        h.objectCount = ReadUInt16LE(data, pos); pos += 2;
        h.doorCount = ReadUInt16LE(data, pos); pos += 2;
        h.liftCount = ReadUInt16LE(data, pos); pos += 2;
        h.playerStartAngle = ReadUInt16LE(data, pos); pos += 2;
        h.unknownBlockA = ReadUInt16LE(data, pos); pos += 2;
        pos += 2; // action sequence block counts - always 64, not stored (matches MapViewer.cs)
        h.unknownBlockB = ReadUInt16LE(data, pos); pos += 2;
        h.enemyTypes = ReadUInt16LE(data, pos); pos += 2;
        h.ventTypes = ReadUInt16LE(data, pos); pos += 2;

        if (pos != HEADER_SIZE)
        {
            // Should be unreachable if the field list above is correct -
            // a mismatch here means the header layout assumption itself
            // is wrong, not a data problem, so fail loudly rather than
            // silently misreading everything after this point.
            throw std::runtime_error("LevelLoader: internal error - header field sum (" + std::to_string(pos) +
                                      ") doesn't match the expected 36 bytes");
        }

        size_t verticesSize = static_cast<size_t>(h.vertCount) * 8;
        size_t quadsSize = static_cast<size_t>(h.quadCount) * 20; // includes the skipped sentinel quad's own 20 bytes
        if (pos + verticesSize + quadsSize > data.size())
        {
            throw std::runtime_error("LevelLoader: MAP0 section declares more vertex/quad data than it contains in " + mapPath.string());
        }

        result.vertices.reserve(h.vertCount);
        for (uint16_t i = 0; i < h.vertCount; ++i)
        {
            ModelVertex v;
            v.x = ReadInt16LE(data, pos + 0);
            v.y = ReadInt16LE(data, pos + 2);
            v.z = ReadInt16LE(data, pos + 4);
            v.marker = ReadUInt16LE(data, pos + 6); // always 0 for level vertices, per MapViewer.cs - not enforced here, just read as-is
            result.vertices.push_back(v);
            pos += 8;
        }

        // Quads start at index 1, not 0 - the first quad is always a
        // FF FF FF FF sentinel and must be skipped (confirmed against
        // ExportLevel's loop bounds, "for (int i = 1; i < quadCount; i++)").
        // Its 20 bytes still need to be skipped over even though it's not
        // read as a real quad.
        pos += 20;
        result.quads.reserve(h.quadCount > 0 ? h.quadCount - 1 : 0);
        for (uint16_t i = 1; i < h.quadCount; ++i)
        {
            ModelQuad q;
            q.a = ReadInt32LE(data, pos + 0);
            q.b = ReadInt32LE(data, pos + 4);
            q.c = ReadInt32LE(data, pos + 8);
            q.d = ReadInt32LE(data, pos + 12);
            q.texIndex = ReadUInt16LE(data, pos + 16);
            q.flags = data[pos + 18];
            q.reserved = data[pos + 19]; // THE LIGHT ID ("light id x // Kaiser" in ExportLevel). Low 7 bits index the 128-entry light table, bit 0x80 is the end-of-face-run marker. NOT interchangeable with `flags` - see ModelQuad in ModelLoader.h.
            pos += 20;

            // A trailing degenerate sentinel (a=b=c=d=-1, texIndex=0xFFFF)
            // shows up as the very last quad in real data (confirmed
            // against L111LEV.MAP) - the same FF FF FF FF pattern as the
            // leading sentinel at index 0, just at the other end. 'a'
            // being -1 is never valid for real geometry (unlike 'd',
            // which legitimately means "this is a triangle") - skip
            // rather than emit a degenerate quad. ExportLevel itself
            // doesn't guard against this (it never looks vertex indices
            // up for validation, just writes them straight into OBJ text,
            // so this case silently produces a broken/invalid face there
            // instead of erroring) - not something to replicate.
            if (q.a == -1) { continue; }
            result.quads.push_back(q);
        }

        // Entity lists - all confirmed against AlienTrilogyMapLoader.cs's
        // BuildMapGeometry, including its own per-list byte-size
        // "formula" comments (see each struct's doc comment in the
        // header for the specific evidence).
        auto needBytes = [&](size_t n, const char* what) {
            if (pos + n > data.size())
            {
                throw std::runtime_error("LevelLoader: not enough data for " + std::string(what) + " in " + mapPath.string());
            }
        };

        size_t collisionCount = static_cast<size_t>(h.mapLength) * h.mapWidth;
        needBytes(collisionCount * 16, "collision grid");
        result.collisionGrid.reserve(collisionCount);
        for (size_t i = 0; i < collisionCount; ++i)
        {
            CollisionNode n;
            n.unknown1 = data[pos + 0]; n.unknown2 = data[pos + 1]; n.unknown3 = data[pos + 2]; n.unknown4 = data[pos + 3];
            n.unknown5 = data[pos + 4]; n.unknown6 = data[pos + 5]; n.unknown7 = data[pos + 6]; n.unknown8 = data[pos + 7];
            n.ceilingFog = data[pos + 8]; n.floorFog = data[pos + 9]; n.attribute = data[pos + 10]; n.floorHeight = data[pos + 11];
            // unknown13 deliberately NOT read from the file here - Ghidra
            // confirms it's runtime-only occupancy state (occupying
            // monster's type while alive, 0 = free), overwritten during
            // play. Whatever value the file itself carries is stale
            // editor navigation-bake data, not level data - loading it
            // would let leftover editor state leak into actual gameplay
            // (Edward, 2026 - Ghidra deep-dive).
            n.unknown13 = 0;
            n.ceilingHeight = data[pos + 13]; n.lighting = data[pos + 14]; n.scriptAction = data[pos + 15];
            pos += 16;
            result.collisionGrid.push_back(n);
        }

        needBytes(static_cast<size_t>(h.pathCount) * 8, "path nodes");
        result.pathNodes.reserve(h.pathCount);
        for (uint8_t i = 0; i < h.pathCount; ++i)
        {
            PathNode n;
            n.x = data[pos + 0]; n.y = data[pos + 1]; n.unused = data[pos + 2]; n.nodeState = data[pos + 3];
            n.nodeA = data[pos + 4]; n.nodeB = data[pos + 5]; n.nodeC = data[pos + 6]; n.nodeD = data[pos + 7];
            pos += 8;
            result.pathNodes.push_back(n);
        }

        needBytes(static_cast<size_t>(h.monsterCount) * 20, "monsters");
        result.monsters.reserve(h.monsterCount);
        for (uint16_t i = 0; i < h.monsterCount; ++i)
        {
            Monster m;
            m.type = data[pos + 0]; m.x = data[pos + 1]; m.y = data[pos + 2]; m.z = data[pos + 3]; m.rotation = data[pos + 4];
            m.health = data[pos + 5]; m.drop = data[pos + 6]; m.triggerCount = data[pos + 7]; m.triggerThreshold = data[pos + 8];
            m.unknown4 = data[pos + 9]; m.unknown5 = data[pos + 10]; m.unknown6 = data[pos + 11]; m.unknown7 = data[pos + 12];
            m.unknown8 = data[pos + 13]; m.speed = data[pos + 14]; m.unknown9 = data[pos + 15]; m.unknown10 = data[pos + 16];
            m.unknown11 = data[pos + 17]; m.unknown12 = data[pos + 18]; m.unknown13 = data[pos + 19];
            pos += 20;
            result.monsters.push_back(m);
        }

        needBytes(static_cast<size_t>(h.pickupCount) * 8, "pickups");
        result.pickups.reserve(h.pickupCount);
        for (uint16_t i = 0; i < h.pickupCount; ++i)
        {
            Pickup p;
            p.x = data[pos + 0]; p.y = data[pos + 1]; p.type = data[pos + 2]; p.amount = data[pos + 3];
            p.multiplier = data[pos + 4]; p.unknown1 = data[pos + 5]; p.z = data[pos + 6]; p.unknown2 = data[pos + 7];
            pos += 8;
            result.pickups.push_back(p);
        }

        needBytes(static_cast<size_t>(h.objectCount) * 16, "crates");
        result.crates.reserve(h.objectCount);
        for (uint16_t i = 0; i < h.objectCount; ++i)
        {
            Crate c;
            c.x = data[pos + 0]; c.y = data[pos + 1]; c.type = data[pos + 2]; c.drop = data[pos + 3];
            c.unknown1 = data[pos + 4]; c.unknown2 = data[pos + 5]; c.drop1 = data[pos + 6]; c.drop2 = data[pos + 7];
            c.unknown3 = data[pos + 8]; c.unknown4 = data[pos + 9]; c.unknown5 = data[pos + 10]; c.unknown6 = data[pos + 11];
            c.unknown7 = data[pos + 12]; c.unknown8 = data[pos + 13]; c.rotation = data[pos + 14]; c.unknown10 = data[pos + 15];
            pos += 16;
            result.crates.push_back(c);
        }

        needBytes(static_cast<size_t>(h.doorCount) * 8, "doors");
        result.doors.reserve(h.doorCount);
        for (uint16_t i = 0; i < h.doorCount; ++i)
        {
            Door d;
            d.x = data[pos + 0]; d.y = data[pos + 1]; d.unknown = data[pos + 2]; d.time = data[pos + 3];
            d.lockState = data[pos + 4]; d.unknown2 = data[pos + 5]; d.rotation = data[pos + 6]; d.modelIndex = data[pos + 7];
            pos += 8;
            result.doors.push_back(d);
        }

        needBytes(static_cast<size_t>(h.liftCount) * 16, "lifts");
        result.lifts.reserve(h.liftCount);
        for (uint16_t i = 0; i < h.liftCount; ++i)
        {
            Lift l;
            l.x = data[pos + 0]; l.y = data[pos + 1]; l.z = data[pos + 2]; l.unknown1 = data[pos + 3];
            l.unknown2 = data[pos + 4]; l.unknown3 = data[pos + 5]; l.unknown4 = data[pos + 6]; l.unknown5 = data[pos + 7];
            l.unknown6 = data[pos + 8]; l.unknown7 = data[pos + 9]; l.unknown8 = data[pos + 10]; l.unknown9 = data[pos + 11];
            l.unknown10 = data[pos + 12]; l.unknown11 = data[pos + 13]; l.unknown12 = data[pos + 14]; l.unknown13 = data[pos + 15];
            pos += 16;
            result.lifts.push_back(l);
        }

        // Always exactly 64 slots read, but only ones with actionType
        // != 0 are kept - matches AlienTrilogyMapLoader.cs's own filter.
        needBytes(64 * 4, "action groups");
        for (int i = 0; i < 64; ++i)
        {
            ActionGroup a;
            a.activationMask = data[pos + 0]; a.commandStart = data[pos + 1]; a.enable = data[pos + 2]; a.byte4 = data[pos + 3];
            pos += 4;
            // All 64 kept, unfiltered: a cell's action byte indexes this
            // table directly, so dropping the empty entries would shift
            // every index after the first gap.
            result.actions.push_back(a);
        }

        // Always exactly 64 entries, all kept (no filtering, unlike
        // ActionGroup above).
        needBytes(64 * 4, "logic groups");
        result.logics.reserve(64);
        for (int i = 0; i < 64; ++i)
        {
            LogicGroup l;
            l.action = data[pos + 0]; l.nextStep = data[pos + 1]; l.modifier = data[pos + 2]; l.objectIndex = data[pos + 3];
            pos += 4;
            result.logics.push_back(l);
        }

        // Lights: 128 records of 28 bytes. The stride matters - at 24 the
        // following lists land 4 bytes further out per row, which is what
        // made the tail look like it had a spare 512-byte block.
        needBytes(128 * 28, "lights");
        result.lights.reserve(128);
        for (int i = 0; i < 128; ++i)
        {
            LightRecord light;
            for (int c = 0; c < 3; ++c)
            {
                light.unlit[c]   = data[pos + 0 + c];
                light.lit[c]     = data[pos + 4 + c];
                light.corner2[c] = data[pos + 8 + c];
                light.corner3[c] = data[pos + 12 + c];
            }
            auto u16at = [&](size_t off) { return static_cast<uint16_t>(data[pos + off] | (data[pos + off + 1] << 8)); };
            light.blinkCountdown = u16at(16);
            light.blinkRepeats   = u16at(18);
            light.onDuration     = u16at(20);
            light.offDuration    = u16at(22);
            light.mode = data[pos + 24];
            light.on = data[pos + 25];
            light.variant = data[pos + 26];
            light.variantMax = data[pos + 27];
            pos += 28;
            result.lights.push_back(light);
        }

        auto readList = [&](uint16_t count, std::vector<uint32_t>& out, const char* what) {
            needBytes(static_cast<size_t>(count) * 4, what);
            out.reserve(count);
            for (uint16_t i = 0; i < count; ++i)
            {
                out.push_back(static_cast<uint32_t>(data[pos] | (data[pos + 1] << 8)
                                                    | (data[pos + 2] << 16) | (static_cast<uint32_t>(data[pos + 3]) << 24)));
                pos += 4;
            }
        };
        readList(result.header.unknownBlockA, result.unknownListA, "unknown list A");
        readList(result.header.unknownBlockB, result.unknownListB, "unknown list B");

        return result;
    }

    // Core height query, in GRID SPACE: gameX/gameZ are continuous
    // coordinates where cell = value >> 9 and the sub-cell position is
    // value & 0x1ff. This is the game's own GetFloorHeight
    // (FUN_00027e28) reproduced exactly, including the ramp and stair
    // cases which depend on where inside the cell you stand.
    float FindFloorHeightGridSpace(const LevelGeometry& level, int gameX, int gameZ)
    {
        if (gameX < 0 || gameZ < 0) { return 0.0f; }
        int cellX = gameX >> 9;
        int cellZ = gameZ >> 9;
        if (cellX >= level.header.mapLength || cellZ >= level.header.mapWidth) { return 0.0f; }

        size_t cellIndex = static_cast<size_t>(cellZ) * static_cast<size_t>(level.header.mapLength) + static_cast<size_t>(cellX);
        if (cellIndex >= level.collisionGrid.size()) { return 0.0f; }
        const CollisionNode& cell = level.collisionGrid[cellIndex];

        int floorHeight = static_cast<int>(cell.floorHeight) * 32;
        int fx = gameX & 0x1ff;
        int fz = gameZ & 0x1ff;

        // Stairs (0x2d-0x30) split the cell into two flat halves 128
        // apart; ramps (0x31-0x38) interpolate across the cell.
        switch (cell.attribute)
        {
        case 0x2d: if (fz > 0x100) { return static_cast<float>(floorHeight + 0x80); } break;
        case 0x2e: if (fx <= 0x100) { return static_cast<float>(floorHeight + 0x80); } break;
        case 0x2f: if (fx > 0x100) { return static_cast<float>(floorHeight + 0x80); } break;
        case 0x30: if (fz <= 0x100) { return static_cast<float>(floorHeight + 0x80); } break;
        case 0x31: case 0x35: return static_cast<float>(floorHeight + (fz >> 2));
        case 0x32: case 0x36: return static_cast<float>(floorHeight - 0x80 + ((0x200 - fx) >> 2));
        case 0x33: case 0x37: return static_cast<float>(floorHeight + (fx >> 2));
        case 0x34: case 0x38: return static_cast<float>(floorHeight - 0x80 + ((0x200 - fz) >> 2));
        default: break;
        }
        return static_cast<float>(floorHeight);
    }

    float FindFloorHeight(const LevelGeometry& level, int gridX, int gridZ)
    {
        if (gridX < 0 || gridZ < 0) { return 0.0f; }
        return FindFloorHeightGridSpace(level, gridX * 512, gridZ * 512);
    }

    bool IsCellBlocking(const LevelGeometry& level, int gameX, int gameZ)
    {
        if (gameX < 0 || gameZ < 0) { return true; }
        int cellX = gameX >> 9;
        int cellZ = gameZ >> 9;
        if (cellX >= level.header.mapLength || cellZ >= level.header.mapWidth) { return true; }

        size_t cellIndex = static_cast<size_t>(cellZ) * static_cast<size_t>(level.header.mapLength) + static_cast<size_t>(cellX);
        if (cellIndex >= level.collisionGrid.size()) { return true; }
        const CollisionNode& cell = level.collisionGrid[cellIndex];

        // Bytes +0x02/+0x03: 255 = wall, 0 = traversable.
        if (cell.unknown3 == 255 || cell.unknown4 == 255) { return true; }

        // A gap SMALLER THAN THE PLAYER blocks, not merely a zero gap.
        //
        // This tested for ceiling <= floor, which meant a door became passable
        // the instant it started to lift - one unit of gap was enough. It should
        // stay solid until it has actually opened enough to walk through
        // (Edward, 2026).
        //
        // The figure is the PLAYER's height in cell height units: 800 world
        // units tall over 32 world units per height unit is 25. See
        // MIN_PASSABLE_CLEARANCE - the 8 this used first is the blast's
        // clearance, not a person's, and let the player through a door that had
        // only lifted a third of the way.
        if (cell.ceilingHeight - cell.floorHeight < MIN_PASSABLE_CLEARANCE) { return true; }

        // Occupancy (byte 12) blocks: crates, switches and monsters stamp
        // their type here at spawn. 0xFF is the player's own marker and is
        // ignored, otherwise you would be unable to move at all.
        if (cell.unknown13 != 0 && cell.unknown13 != 0xFF) { return true; }

        // Attribute band 0x14-0x2c blocks. 0x2d and above are the stair
        // and ramp attributes, which are walkable - their height change
        // is handled by FindFloorHeightGridSpace and gated by the
        // step-up limit instead.
        if (cell.attribute > 0x13 && cell.attribute < 0x2d) { return true; }

        return false;
    }
}
