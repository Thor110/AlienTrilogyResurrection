#include "BndParser.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        std::string ReadTag(const std::vector<uint8_t>& data, size_t offset, size_t length)
        {
            if (offset + length > data.size())
            {
                throw std::runtime_error("BndParser: unexpected end of data reading tag");
            }
            return std::string(reinterpret_cast<const char*>(data.data() + offset), length);
        }

        // Big-endian Int32, matching BitConverter.ToInt32(bytes.Reverse()...)
        int32_t ReadBigEndianInt32(const std::vector<uint8_t>& data, size_t offset)
        {
            if (offset + 4 > data.size())
            {
                throw std::runtime_error("BndParser: unexpected end of data reading Int32");
            }
            return static_cast<int32_t>(
                (static_cast<uint32_t>(data[offset]) << 24) |
                (static_cast<uint32_t>(data[offset + 1]) << 16) |
                (static_cast<uint32_t>(data[offset + 2]) << 8) |
                static_cast<uint32_t>(data[offset + 3]));
        }
    }

    std::vector<BndSection> BndParser::ParseAllFormSections(const std::vector<uint8_t>& bnd)
    {
        std::vector<BndSection> sections;

        size_t pos = 0;
        std::string formTag = ReadTag(bnd, pos, 4);
        pos += 4;
        if (formTag != "FORM")
        {
            throw std::runtime_error("Invalid BND file: missing FORM header.");
        }

        pos += 4; // form size (unused, matches C# reading-and-discarding it beyond validation)
        pos += 4; // platform tag, e.g. "PSXT" (unused)

        while (pos + 8 <= bnd.size())
        {
            std::string chunkName = ReadTag(bnd, pos, 4);
            pos += 4;
            int32_t chunkSize = ReadBigEndianInt32(bnd, pos);
            pos += 4;

            if (chunkSize < 0 || pos + static_cast<size_t>(chunkSize) > bnd.size())
            {
                break;
            }

            BndSection section;
            section.name = chunkName;
            section.data.assign(bnd.begin() + static_cast<ptrdiff_t>(pos),
                                 bnd.begin() + static_cast<ptrdiff_t>(pos) + chunkSize);
            sections.push_back(std::move(section));

            pos += static_cast<size_t>(chunkSize);
            if (chunkSize % 2 != 0) { pos += 1; } // IFF padding to 2-byte alignment
        }

        return sections;
    }

    std::vector<BndSection> BndParser::ParseFormSections(const std::vector<uint8_t>& bnd, const std::string& sectionPrefix)
    {
        // Single sequential pass (ParseAllFormSections), then filter in
        // memory - avoids re-scanning the file for every prefix a
        // caller wants (Edward, 2026: prefer callers use
        // ParseAllFormSections/LoadCached directly and filter themselves
        // when they need more than one prefix from the same file, e.g.
        // BndTextureLoader needing "TP"/"CL"/"BX" all at once).
        std::vector<BndSection> all = ParseAllFormSections(bnd);
        std::vector<BndSection> filtered;
        for (auto& section : all)
        {
            if (section.name.rfind(sectionPrefix, 0) == 0) // starts with
            {
                filtered.push_back(std::move(section));
            }
        }
        return filtered;
    }

    namespace
    {
        std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open())
            {
                throw std::runtime_error("BndParser: could not open " + path.string());
            }
            auto size = static_cast<size_t>(in.tellg());
            in.seekg(0);
            std::vector<uint8_t> data(size);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
            return data;
        }
    }

    const std::vector<BndSection>& BndParser::LoadCached(const std::filesystem::path& path)
    {
        // Keyed by the canonical absolute path string so the same file
        // reached via different relative paths still shares one cache
        // entry. This cache only ever grows (a handful of BND/B16 files
        // total per game, all small enough to keep resident) - no
        // eviction needed.
        static std::unordered_map<std::string, std::vector<BndSection>> cache;

        std::string key = std::filesystem::absolute(path).lexically_normal().string();
        auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        std::vector<uint8_t> bytes = ReadFileBytes(path);
        std::vector<BndSection> sections = ParseAllFormSections(bytes);
        auto [inserted, _] = cache.emplace(std::move(key), std::move(sections));
        return inserted->second;
    }

    int64_t BndParser::FindFormSectionOffset(const std::vector<uint8_t>& bnd, int index)
    {
        size_t pos = 0;
        std::string formTag = ReadTag(bnd, pos, 4);
        pos += 4;
        if (formTag != "FORM")
        {
            throw std::runtime_error("Invalid BND file: missing FORM header.");
        }
        pos += 4; // form size
        pos += 4; // platform tag

        char indexBuf[3];
        std::snprintf(indexBuf, sizeof(indexBuf), "%02d", index);
        std::string wantedName = "F0" + std::string(indexBuf);

        while (pos + 8 <= bnd.size())
        {
            size_t chunkStart = pos;
            std::string chunkName = ReadTag(bnd, pos, 4);
            pos += 4;
            int32_t chunkSize = ReadBigEndianInt32(bnd, pos);
            pos += 4;

            if (chunkName == wantedName)
            {
                return static_cast<int64_t>(chunkStart);
            }

            if (chunkSize < 0) { break; }
            pos += static_cast<size_t>(chunkSize);
            if (chunkSize % 2 != 0) { pos += 1; }
        }

        throw std::runtime_error("Section " + wantedName + " not found.");
    }
}
