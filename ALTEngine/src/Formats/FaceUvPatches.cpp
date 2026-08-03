#include "FaceUvPatches.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace ALTEngine::Formats
{
    std::vector<FaceUvRotation> FaceUvPatchLoader::Load(const std::filesystem::path& manifestPath)
    {
        std::vector<FaceUvRotation> result;

        std::ifstream file(manifestPath);
        if (!file.is_open()) { return result; }

        nlohmann::json root;
        try { file >> root; }
        catch (const std::exception&) { return result; }

        if (!root.contains("faceUvRotations") || !root["faceUvRotations"].is_array()) { return result; }

        for (const auto& entry : root["faceUvRotations"])
        {
            if (!entry.contains("target") || !entry.contains("vertices")) { continue; }

            FaceUvRotation rotation;
            rotation.targetFile = entry["target"].get<std::string>();
            rotation.steps = entry.value("steps", 1);

            const auto& verts = entry["vertices"];
            if (!verts.is_array() || verts.size() < 3) { continue; }

            rotation.vertices[0] = verts[0].get<int32_t>();
            rotation.vertices[1] = verts[1].get<int32_t>();
            rotation.vertices[2] = verts[2].get<int32_t>();
            rotation.vertices[3] = verts.size() > 3 ? verts[3].get<int32_t>() : -1;

            result.push_back(rotation);
        }

        return result;
    }

    std::vector<FaceUvRotation> FaceUvPatchLoader::ForLevelFile(const std::vector<FaceUvRotation>& all,
                                                                const std::string& levelFileName)
    {
        std::vector<FaceUvRotation> result;
        for (const auto& rotation : all)
        {
            if (rotation.targetFile.size() < levelFileName.size()) { continue; }
            if (rotation.targetFile.compare(rotation.targetFile.size() - levelFileName.size(),
                                             levelFileName.size(), levelFileName) == 0)
            {
                result.push_back(rotation);
            }
        }
        return result;
    }
}
