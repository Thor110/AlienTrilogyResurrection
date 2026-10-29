#include "ModelRemoval.h"

#include <array>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace ALTEngine::Formats
{
    namespace
    {
        uint32_t ReadBe32(const std::vector<uint8_t>& b, size_t pos)
        {
            return (uint32_t(b[pos]) << 24) | (uint32_t(b[pos + 1]) << 16) | (uint32_t(b[pos + 2]) << 8) | uint32_t(b[pos + 3]);
        }

        void WriteBe32(std::vector<uint8_t>& b, size_t pos, uint32_t v)
        {
            b[pos] = uint8_t(v >> 24); b[pos + 1] = uint8_t(v >> 16); b[pos + 2] = uint8_t(v >> 8); b[pos + 3] = uint8_t(v);
        }

        struct Chunk { std::string tag; size_t pos = 0; uint32_t size = 0; size_t span = 0; };

        // The chunks after the FORM header, each a 4-byte tag, a big-endian
        // size and the payload, padded to an even length (as 0x27b23 skips).
        bool WalkChunks(const std::vector<uint8_t>& b, std::vector<Chunk>& out)
        {
            if (b.size() < 12 || b[0] != 'F' || b[1] != 'O' || b[2] != 'R' || b[3] != 'M') { return false; }
            size_t pos = 12;
            while (pos + 8 <= b.size())
            {
                Chunk c;
                c.tag.assign(reinterpret_cast<const char*>(&b[pos]), 4);
                c.pos = pos;
                c.size = ReadBe32(b, pos + 4);
                c.span = 8 + size_t(c.size) + (c.size & 1u);
                if (pos + 8 + c.size > b.size()) { return false; }
                out.push_back(c);
                pos += c.span;
            }
            return pos == b.size() || pos == b.size() + 1;
        }

        bool EditsInPlace(const std::vector<uint8_t>& b, const std::vector<BinaryEdit>& edits)
        {
            for (const auto& e : edits)
            {
                if (e.offset + e.bytes.size() > b.size()) { return false; }
                for (size_t i = 0; i < e.bytes.size(); ++i) { if (b[e.offset + i] != e.bytes[i]) { return false; } }
            }
            return true;
        }

        uint32_t ParseHex32(const std::string& s) { return static_cast<uint32_t>(std::stoull(s, nullptr, 0)); }
    }

    uint32_t Crc32(const uint8_t* data, size_t size)
    {
        static const std::array<uint32_t, 256> table = [] {
            std::array<uint32_t, 256> t{};
            for (uint32_t i = 0; i < 256; ++i)
            {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) { c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); }
                t[i] = c;
            }
            return t;
        }();
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < size; ++i) { crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8); }
        return crc ^ 0xFFFFFFFFu;
    }

    ModelRemovalOutcome ApplyModelRemoval(std::vector<uint8_t>& bytes, const ModelRemoval& removal, std::string& error)
    {
        std::vector<Chunk> chunks;
        if (!WalkChunks(bytes, chunks)) { error = "not a FORM file, or its chunks run past the end"; return ModelRemovalOutcome::Failed; }

        std::vector<const Chunk*> found;
        for (const auto& want : removal.chunks)
        {
            const Chunk* match = nullptr;
            for (const auto& c : chunks) { if (c.tag == want.tag) { match = &c; break; } }
            if (!match) { continue; }
            if (match->size != want.size || Crc32(&bytes[match->pos + 8], match->size) != want.crc32)
            {
                error = "chunk " + want.tag + " is not the expected one (size or CRC differs)";
                return ModelRemovalOutcome::Failed;
            }
            found.push_back(match);
        }

        if (found.empty())
        {
            if (EditsInPlace(bytes, removal.recordEdits)) { return ModelRemovalOutcome::AlreadyApplied; }
            error = "the chunks are gone but the door/lift records do not hold their new values";
            return ModelRemovalOutcome::Failed;
        }
        if (found.size() != removal.chunks.size())
        {
            error = "only some of the chunks to remove are present";
            return ModelRemovalOutcome::Failed;
        }

        // The records live in MAP0, the first chunk, before anything removed.
        for (const auto& e : removal.recordEdits)
        {
            for (const Chunk* c : found)
            {
                if (e.offset + e.bytes.size() > c->pos) { error = "a record edit lies at or after a removed chunk"; return ModelRemovalOutcome::Failed; }
            }
        }

        std::vector<uint8_t> out(bytes.begin(), bytes.begin() + 12);
        for (const auto& c : chunks)
        {
            bool drop = false;
            for (const Chunk* f : found) { if (f->pos == c.pos) { drop = true; break; } }
            if (drop) { continue; }
            const size_t end = std::min(bytes.size(), c.pos + c.span);
            out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(c.pos), bytes.begin() + static_cast<std::ptrdiff_t>(end));
        }
        for (const auto& e : removal.recordEdits)
        {
            std::copy(e.bytes.begin(), e.bytes.end(), out.begin() + static_cast<std::ptrdiff_t>(e.offset));
        }
        WriteBe32(out, 4, static_cast<uint32_t>(out.size() - 8));
        bytes = std::move(out);
        return ModelRemovalOutcome::Applied;
    }

    std::vector<ModelRemoval> ModelRemovalLoader::Load(const std::filesystem::path& jsonPath)
    {
        std::ifstream file(jsonPath);
        if (!file.is_open()) { throw std::runtime_error("ModelRemovalLoader::Load: could not open " + jsonPath.string()); }
        nlohmann::json root;
        file >> root;

        std::vector<ModelRemoval> result;
        if (!root.contains("modelRemovals")) { return result; }
        for (const auto& entry : root.at("modelRemovals"))
        {
            ModelRemoval r;
            r.targetFile = entry.at("target").get<std::string>();
            r.note = entry.value("note", "");
            for (const auto& c : entry.at("chunks"))
            {
                r.chunks.push_back({ c.at("tag").get<std::string>(), c.at("size").get<uint32_t>(),
                                     ParseHex32(c.at("crc32").get<std::string>()) });
            }
            if (entry.contains("edits"))
            {
                for (const auto& e : entry.at("edits"))
                {
                    BinaryEdit edit;
                    edit.offset = std::stoull(e.at("offset").get<std::string>(), nullptr, 0);
                    for (const auto& b : e.at("bytes")) { edit.bytes.push_back(static_cast<uint8_t>(std::stoul(b.get<std::string>(), nullptr, 0))); }
                    r.recordEdits.push_back(std::move(edit));
                }
            }
            if (r.chunks.empty()) { throw std::runtime_error("ModelRemovalLoader::Load: removal for '" + r.targetFile + "' lists no chunks"); }
            result.push_back(std::move(r));
        }
        return result;
    }

    std::vector<ModelRemovalResult> ModelRemovalRunner::ApplyAll(const std::filesystem::path& gameRoot,
                                                                 const std::vector<ModelRemoval>& removals)
    {
        std::vector<ModelRemovalResult> results;
        for (const auto& removal : removals)
        {
            ModelRemovalResult result;
            result.removal = &removal;
            const auto path = (gameRoot / removal.targetFile).lexically_normal().make_preferred();

            std::vector<uint8_t> bytes;
            {
                std::ifstream in(path, std::ios::binary);
                if (!in) { result.error = "target file not found: " + path.string(); results.push_back(result); continue; }
                bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }

            result.outcome = ApplyModelRemoval(bytes, removal, result.error);
            if (result.outcome == ModelRemovalOutcome::Applied)
            {
                // Written beside the file and moved over it, so a failure
                // part-way never leaves a half-written level.
                auto temp = path; temp += ".tmp";
                {
                    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                    if (!out) { result.outcome = ModelRemovalOutcome::Failed; result.error = "could not write " + temp.string(); }
                }
                std::error_code ec;
                if (result.outcome == ModelRemovalOutcome::Applied)
                {
                    std::filesystem::rename(temp, path, ec);
                    if (ec) { result.outcome = ModelRemovalOutcome::Failed; result.error = "could not replace " + path.string() + ": " + ec.message(); }
                }
                if (result.outcome == ModelRemovalOutcome::Failed) { std::filesystem::remove(temp, ec); }
            }
            results.push_back(std::move(result));
        }
        return results;
    }
}
