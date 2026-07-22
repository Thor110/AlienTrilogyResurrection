#include "PatchLoader.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

namespace ALTEngine::Formats
{
    namespace
    {
        uint64_t ParseHexOffset(const std::string& s)
        {
            // "0x51342" -> 0x51342. std::stoull with base 0 auto-detects
            // the "0x" prefix.
            return std::stoull(s, nullptr, 0);
        }

        uint8_t ParseHexByte(const std::string& s)
        {
            unsigned long v = std::stoul(s, nullptr, 0);
            if (v > 0xFF)
            {
                throw std::runtime_error("PatchLoader: byte value out of range: " + s);
            }
            return static_cast<uint8_t>(v);
        }

        // The source .cs patches a handful of files (L111LEV.MAP,
        // L900LEV.MAP) across two separate calls rather than one. Merge
        // those into a single operation per target file, concatenating
        // edits in original encounter order - this is behaviourally
        // identical (each edit still lands in the same final order) but
        // means every file is opened, patched, and logged exactly once.
        std::vector<PatchOperation> MergeByTarget(std::vector<PatchOperation> ops)
        {
            std::vector<PatchOperation> merged;
            std::unordered_map<std::string, size_t> indexByTarget;

            for (auto& op : ops)
            {
                auto it = indexByTarget.find(op.targetFile);
                if (it == indexByTarget.end())
                {
                    indexByTarget[op.targetFile] = merged.size();
                    merged.push_back(std::move(op));
                    continue;
                }

                PatchOperation& existing = merged[it->second];
                existing.edits.insert(existing.edits.end(),
                                       std::make_move_iterator(op.edits.begin()),
                                       std::make_move_iterator(op.edits.end()));

                if (!op.note.empty() && existing.note.find(op.note) == std::string::npos)
                {
                    existing.note += existing.note.empty() ? op.note : ("; " + op.note);
                }
            }

            return merged;
        }
    }

    std::vector<PatchOperation> PatchLoader::Load(const std::filesystem::path& jsonPath)
    {
        std::ifstream file(jsonPath);
        if (!file.is_open())
        {
            throw std::runtime_error("PatchLoader::Load: could not open " + jsonPath.string());
        }

        nlohmann::json root;
        file >> root;

        std::vector<PatchOperation> ops;
        for (const auto& patchJson : root.at("patches"))
        {
            PatchOperation op;
            op.targetFile = patchJson.at("target").get<std::string>();
            op.note = patchJson.value("note", "");

            for (const auto& editJson : patchJson.at("edits"))
            {
                BinaryEdit edit;
                edit.offset = ParseHexOffset(editJson.at("offset").get<std::string>());

                edit.bytes.reserve(editJson.at("bytes").size());
                for (const auto& byteJson : editJson.at("bytes"))
                {
                    edit.bytes.push_back(ParseHexByte(byteJson.get<std::string>()));
                }
                op.edits.push_back(std::move(edit));
            }

            if (op.edits.empty())
            {
                throw std::runtime_error("PatchLoader::Load: patch for '" + op.targetFile + "' has no edits");
            }
            ops.push_back(std::move(op));
        }

        return MergeByTarget(std::move(ops));
    }
}
