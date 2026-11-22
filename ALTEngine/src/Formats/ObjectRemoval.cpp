#include "ObjectRemoval.h"
#include "../Bootstrap/FsUtil.h"

#include <cstring>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace ALTEngine::Formats
{
    namespace
    {
        uint32_t Be32(const std::vector<uint8_t>& b, size_t at)
        {
            return (uint32_t(b[at]) << 24) | (uint32_t(b[at + 1]) << 16) | (uint32_t(b[at + 2]) << 8) | uint32_t(b[at + 3]);
        }
        void PutBe32(std::vector<uint8_t>& b, size_t at, uint32_t v)
        {
            b[at] = uint8_t(v >> 24); b[at + 1] = uint8_t(v >> 16); b[at + 2] = uint8_t(v >> 8); b[at + 3] = uint8_t(v);
        }
        uint16_t Le16(const std::vector<uint8_t>& b, size_t at) { return uint16_t(b[at] | (b[at + 1] << 8)); }

        // The MAP0 chunk's header position (its payload) and size, or false.
        bool FindMap0(const std::vector<uint8_t>& b, size_t& chunk, size_t& data, uint32_t& size)
        {
            if (b.size() < 12 || std::memcmp(b.data(), "FORM", 4) != 0) { return false; }
            size_t pos = 12;
            while (pos + 8 <= b.size())
            {
                const uint32_t n = Be32(b, pos + 4);
                if (n > b.size() - pos - 8) { return false; }
                if (std::memcmp(b.data() + pos, "MAP0", 4) == 0) { chunk = pos; data = pos + 8; size = n; return true; }
                pos += 8 + n + (n & 1u);
            }
            return false;
        }

        bool ReadRecord(const std::string& hex, std::array<uint8_t, 16>& out)
        {
            if (hex.size() != 32) { return false; }
            for (size_t i = 0; i < 16; ++i)
            {
                try { out[i] = uint8_t(std::stoul(hex.substr(2 * i, 2), nullptr, 16)); }
                catch (...) { return false; }
            }
            return true;
        }
    }

    int64_t MapObjectOffset(const std::vector<uint8_t>& b, int index, int* objectCount)
    {
        size_t chunk = 0, d = 0; uint32_t size = 0;
        if (!FindMap0(b, chunk, d, size) || size < 36) { return -1; }
        // The header (LevelLoader): vertices, faces, map length and width,
        // then path count (u8, +0x0c), monsters (+0x0e), pickups (+0x10),
        // objects (+0x12). Then the sections in order: vertices (8 bytes),
        // faces (20), cells (16), path nodes (8), monsters (20), pickups (8),
        // objects (16).
        const size_t verts = Le16(b, d), faces = Le16(b, d + 2), length = Le16(b, d + 4), width = Le16(b, d + 6);
        const size_t paths = b[d + 12], monsters = Le16(b, d + 14), pickups = Le16(b, d + 16), objects = Le16(b, d + 18);
        if (objectCount) { *objectCount = int(objects); }
        const size_t at = d + 36 + verts * 8 + faces * 20 + length * width * 16 + paths * 8 + monsters * 20 + pickups * 8
                          + size_t(index) * 16;
        if (index < 0 || at + 16 > d + size) { return -1; }
        return int64_t(at);
    }

    ObjectRemovalOutcome ApplyObjectRemoval(std::vector<uint8_t>& bytes, const ObjectRemoval& removal, std::string& error)
    {
        size_t chunk = 0, d = 0; uint32_t size = 0;
        if (!FindMap0(bytes, chunk, d, size)) { error = "no MAP0 chunk (a trimmed level?)"; return ObjectRemovalOutcome::Failed; }
        int count = 0;
        const int64_t at = MapObjectOffset(bytes, removal.index, &count);
        if (at < 0) { error = "no object record " + std::to_string(removal.index); return ObjectRemovalOutcome::Failed; }
        if (count == removal.countBefore - 1)
        {
            const bool nextThere = removal.hasNext ? std::memcmp(bytes.data() + at, removal.next.data(), 16) == 0 : true;
            if (nextThere) { return ObjectRemovalOutcome::AlreadyApplied; }
            error = "the object count is one less, but the records are not the expected ones";
            return ObjectRemovalOutcome::Failed;
        }
        if (count != removal.countBefore) { error = "the level has " + std::to_string(count) + " objects, not " + std::to_string(removal.countBefore); return ObjectRemovalOutcome::Failed; }
        if (std::memcmp(bytes.data() + at, removal.record.data(), 16) != 0) { error = "object record " + std::to_string(removal.index) + " is not the expected one"; return ObjectRemovalOutcome::Failed; }

        bytes.erase(bytes.begin() + at, bytes.begin() + at + 16);
        const uint16_t newCount = uint16_t(count - 1);
        bytes[d + 18] = uint8_t(newCount); bytes[d + 19] = uint8_t(newCount >> 8);
        PutBe32(bytes, chunk + 4, size - 16);
        PutBe32(bytes, 4, uint32_t(bytes.size() - 8));
        return ObjectRemovalOutcome::Applied;
    }

    std::vector<ObjectRemoval> ObjectRemovalLoader::Load(const std::filesystem::path& jsonPath)
    {
        std::ifstream file(jsonPath);
        if (!file.is_open()) { throw std::runtime_error("ObjectRemovalLoader::Load: could not open " + ALTEngine::Bootstrap::PathText(jsonPath)); }
        nlohmann::json root;
        file >> root;
        std::vector<ObjectRemoval> result;
        if (!root.contains("objectRemovals")) { return result; }
        for (const auto& entry : root.at("objectRemovals"))
        {
            ObjectRemoval r;
            r.targetFile = entry.at("target").get<std::string>();
            r.note = entry.value("note", "");
            r.index = entry.at("index").get<int>();
            r.countBefore = entry.at("countBefore").get<int>();
            const std::string next = entry.value("next", "");
            r.hasNext = !next.empty();
            if (!ReadRecord(entry.at("record").get<std::string>(), r.record) || (r.hasNext && !ReadRecord(next, r.next)))
            {
                throw std::runtime_error("ObjectRemovalLoader::Load: removal for '" + r.targetFile + "' has a record that is not 16 bytes of hex");
            }
            result.push_back(std::move(r));
        }
        return result;
    }

    std::vector<ObjectRemovalResult> ObjectRemovalRunner::ApplyAll(const std::filesystem::path& gameRoot,
                                                                   const std::vector<ObjectRemoval>& removals)
    {
        std::vector<ObjectRemovalResult> results;
        for (const auto& removal : removals)
        {
            ObjectRemovalResult result;
            result.removal = &removal;
            const auto path = (gameRoot / removal.targetFile).lexically_normal().make_preferred();
            std::vector<uint8_t> bytes;
            {
                std::ifstream in(path, std::ios::binary);
                if (!in) { result.error = "target file not found: " + ALTEngine::Bootstrap::PathText(path); results.push_back(result); continue; }
                bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
            result.outcome = ApplyObjectRemoval(bytes, removal, result.error);
            if (result.outcome == ObjectRemovalOutcome::Applied)
            {
                auto temp = path; temp += ".tmp";
                {
                    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                    if (!out) { result.outcome = ObjectRemovalOutcome::Failed; result.error = "could not write " + ALTEngine::Bootstrap::PathText(temp); }
                }
                std::error_code ec;
                if (result.outcome == ObjectRemovalOutcome::Applied)
                {
                    std::filesystem::rename(temp, path, ec);
                    if (ec) { result.outcome = ObjectRemovalOutcome::Failed; result.error = "could not replace " + ALTEngine::Bootstrap::PathText(path) + ": " + ec.message(); }
                }
                else { std::filesystem::remove(temp, ec); }
            }
            results.push_back(std::move(result));
        }
        return results;
    }
}
