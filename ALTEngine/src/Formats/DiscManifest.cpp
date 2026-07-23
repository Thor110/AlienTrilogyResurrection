#include "DiscManifest.h"

#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace ALTEngine::Formats
{
    DiscManifest DiscManifestLoader::Load(const std::filesystem::path& jsonPath)
    {
        std::ifstream file(jsonPath);
        if (!file.is_open())
        {
            throw std::runtime_error("DiscManifestLoader::Load: could not open " + jsonPath.string());
        }

        nlohmann::json root;
        file >> root;

        DiscManifest manifest;
        for (const auto& f : root.at("rootFiles"))
        {
            manifest.rootFiles.push_back(f.get<std::string>());
        }

        for (const auto& [name, value] : root.at("folders").items())
        {
            DiscFolder folder;
            folder.copyAll = value.value("copyAll", false);
            for (const auto& f : value.at("files"))
            {
                folder.files.push_back(f.get<std::string>());
            }
            manifest.folders[name] = std::move(folder);
        }

        return manifest;
    }
}
