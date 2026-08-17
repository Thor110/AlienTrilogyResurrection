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

            // Optional. Entries are matched by EXTENSION, so "*.BIN" and ".bin"
            // both mean the same thing - the glob form is accepted because it
            // reads more naturally in the manifest.
            if (value.contains("exclude"))
            {
                for (const auto& pattern : value.at("exclude"))
                {
                    std::string ext = pattern.get<std::string>();
                    const size_t dot = ext.rfind('.');
                    if (dot != std::string::npos) { ext = ext.substr(dot); }
                    for (char& c : ext) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
                    if (!ext.empty()) { folder.excludeExtensions.push_back(ext); }
                }
            }
            manifest.folders[name] = std::move(folder);
        }

        return manifest;
    }
}
