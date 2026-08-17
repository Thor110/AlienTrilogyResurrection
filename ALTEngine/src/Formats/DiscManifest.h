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

        // Extensions to skip when copyAll is set, lower case with the dot
        // (".bin"). LANGUAGE needs this: its PNLGFX HUD files must be copied, so
        // the folder cannot switch to an explicit list, but its seven .BIN
        // language files are redundant now that their text lives in
        // LANGUAGE/<pack>/strings.txt.
        std::vector<std::string> excludeExtensions;

        bool IsExcluded(const std::filesystem::path& file) const
        {
            if (excludeExtensions.empty()) { return false; }
            std::string ext = file.extension().string();
            for (char& c : ext) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
            for (const auto& skip : excludeExtensions)
            {
                if (ext == skip) { return true; }
            }
            return false;
        }
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
