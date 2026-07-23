#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ALTEngine::Formats
{
    struct DiscFolder
    {
        bool copyAll = false;          // AVI/LANGUAGE/SFX - copy every file present, list not exhaustive
        std::vector<std::string> files; // explicit file list otherwise
    };

    struct DiscManifest
    {
        std::vector<std::string> rootFiles;                    // e.g. README.TXT
        std::unordered_map<std::string, DiscFolder> folders;   // "GFX" -> ..., under CD/
    };

    class DiscManifestLoader
    {
    public:
        static DiscManifest Load(const std::filesystem::path& jsonPath);
    };
}
