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
            q.reserved = data[pos + 19]; // "light id" per ExportLevel's comment - not the same meaning as M0 models' reserved byte, kept as raw storage either way
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

        return result;
    }
}
