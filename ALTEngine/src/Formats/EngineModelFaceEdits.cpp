#include "EngineModelFaceEdits.h"

#include <algorithm>
#include <cstring>

#include <nlohmann/json.hpp>

namespace ALTEngine::Formats
{
    namespace
    {
        uint32_t BigEndian32(const std::vector<uint8_t>& b, size_t at)
        {
            return (uint32_t(b[at]) << 24) | (uint32_t(b[at + 1]) << 16) | (uint32_t(b[at + 2]) << 8) | uint32_t(b[at + 3]);
        }

        int32_t LittleEndian32(const std::vector<uint8_t>& b, size_t at)
        {
            return int32_t(uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) | (uint32_t(b[at + 3]) << 24));
        }

        bool ReadRecord(const std::string& hex, std::array<uint8_t, 20>& out)
        {
            if (hex.size() != 40) { return false; }
            auto value = [](char c) -> int {
                if (c >= '0' && c <= '9') { return c - '0'; }
                if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
                if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
                return -1;
            };
            for (size_t i = 0; i < 20; ++i)
            {
                const int hi = value(hex[2 * i]), lo = value(hex[2 * i + 1]);
                if (hi < 0 || lo < 0) { return false; }
                out[i] = uint8_t(hi * 16 + lo);
            }
            return true;
        }

        bool Matches(const std::vector<uint8_t>& file, int64_t at, const std::array<uint8_t, 20>& record)
        {
            return std::memcmp(file.data() + at, record.data(), record.size()) == 0;
        }
    }

    namespace EngineModelFaceEdits
    {
        int64_t FaceOffset(const std::vector<uint8_t>& file, const std::string& section, int face)
        {
            if (face < 0 || section.size() != 4 || file.size() < 12 || std::memcmp(file.data(), "FORM", 4) != 0) { return -1; }
            // FORM, its size, the platform tag; then chunks of a 4-byte name,
            // a big-endian size and the payload padded to an even length.
            size_t pos = 12;
            while (pos + 8 <= file.size())
            {
                const uint32_t size = BigEndian32(file, pos + 4);
                const size_t data = pos + 8;
                if (size > file.size() - data) { return -1; }
                if (std::memcmp(file.data() + pos, section.data(), 4) == 0)
                {
                    // OBJ1, 4 bytes of padding, the identifier, the face count,
                    // the vertex count, then the 20-byte face records.
                    if (size < 20 || std::memcmp(file.data() + data, "OBJ1", 4) != 0) { return -1; }
                    const int32_t faces = LittleEndian32(file, data + 12);
                    if (face >= faces) { return -1; }
                    const size_t at = data + 20 + size_t(face) * 20;
                    if (at + 20 > data + size) { return -1; }
                    return int64_t(at);
                }
                pos = data + size + (size & 1u);
            }
            return -1;
        }

        EngineModelFaceOutcome Apply(std::vector<uint8_t>& file, const EngineModelFaceEdit& edit)
        {
            const int64_t at = FaceOffset(file, edit.section, edit.face);
            if (at < 0) { return EngineModelFaceOutcome::NotFound; }
            if (Matches(file, at, edit.now)) { return EngineModelFaceOutcome::AlreadyApplied; }
            if (!Matches(file, at, edit.was)) { return EngineModelFaceOutcome::Mismatch; }
            std::copy(edit.now.begin(), edit.now.end(), file.begin() + at);
            return EngineModelFaceOutcome::Applied;
        }

        bool Revert(std::vector<uint8_t>& file, const EngineModelFaceEdit& edit)
        {
            const int64_t at = FaceOffset(file, edit.section, edit.face);
            if (at < 0 || !Matches(file, at, edit.now)) { return false; }
            std::copy(edit.was.begin(), edit.was.end(), file.begin() + at);
            return true;
        }

        std::vector<EngineModelFaceEdit> Parse(const std::string& jsonText, std::vector<std::string>& errors)
        {
            std::vector<EngineModelFaceEdit> edits;
            nlohmann::json list;
            try { list = nlohmann::json::parse(jsonText); }
            catch (...) { errors.push_back("not valid JSON"); return edits; }
            if (!list.is_object() || !list.contains("modelFaces")) { return edits; }
            if (!list["modelFaces"].is_array()) { errors.push_back("\"modelFaces\" is not a list"); return edits; }
            for (const auto& entry : list["modelFaces"])
            {
                EngineModelFaceEdit edit;
                const bool shaped = entry.is_object() && entry.contains("target") && entry["target"].is_string()
                                    && entry.contains("section") && entry["section"].is_string()
                                    && entry.contains("face") && entry["face"].is_number_integer()
                                    && entry.contains("was") && entry["was"].is_string()
                                    && entry.contains("now") && entry["now"].is_string();
                if (shaped)
                {
                    edit.target = entry["target"].get<std::string>();
                    edit.section = entry["section"].get<std::string>();
                    edit.face = entry["face"].get<int>();
                    edit.note = entry.value("note", "");
                }
                if (!shaped || edit.section.size() != 4 || edit.face < 0
                    || !ReadRecord(entry["was"].get<std::string>(), edit.was) || !ReadRecord(entry["now"].get<std::string>(), edit.now))
                {
                    errors.push_back("an entry cannot be read: " + entry.dump());
                    continue;
                }
                edits.push_back(std::move(edit));
            }
            return edits;
        }
    }
}
