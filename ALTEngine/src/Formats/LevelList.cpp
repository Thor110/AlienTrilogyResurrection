#include "LevelList.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace ALTEngine::Formats
{
    namespace
    {
        // "111" -> "1.1.1". The dotted form is what the briefing and gameplay
        // screens take; they strip it back to digits internally.
        std::string Dotted(const std::string& digits)
        {
            std::string out;
            for (size_t i = 0; i < digits.size(); ++i)
            {
                if (i > 0) { out += '.'; }
                out += digits[i];
            }
            return out;
        }
    }

    std::vector<LevelListEntry> LoadLevelList(const std::filesystem::path& manifestPath)
    {
        std::vector<LevelListEntry> levels;

        std::ifstream file(manifestPath);
        if (!file) { return levels; }

        nlohmann::json root;
        try { file >> root; }
        catch (const std::exception&) { return levels; }

        auto add = [&levels](const std::string& code, const std::string& sector, const std::string& mapFile,
                             int chapter, int part, bool multiplayer) {
            if (code.empty()) { return; }
            LevelListEntry entry;
            entry.digits = code;
            entry.dottedCode = Dotted(code);
            entry.sectorFolder = sector;
            entry.mapFile = mapFile;
            entry.chapter = chapter;
            entry.part = part;
            entry.multiplayer = multiplayer;

            std::string stem = mapFile;
            size_t dot = stem.find('.');
            if (dot != std::string::npos) { stem = stem.substr(0, dot); }
            entry.label = entry.dottedCode + "  " + stem + (multiplayer ? "  (MP)" : "");

            levels.push_back(entry);
        };

        for (const auto& chapter : root.value("chapters", nlohmann::json::array()))
        {
            int chapterNumber = chapter.value("chapter", 0);
            for (const auto& part : chapter.value("parts", nlohmann::json::array()))
            {
                int partNumber = part.value("part", 0);
                std::string sector = part.value("sectorFolder", "");
                for (const auto& level : part.value("levels", nlohmann::json::array()))
                {
                    add(level.value("code", ""), sector, level.value("mapFile", ""),
                        chapterNumber, partNumber, false);
                }
            }
        }

        if (root.contains("multiplayer"))
        {
            const auto& mp = root["multiplayer"];
            std::string sector = mp.value("sectorFolder", "");
            for (const auto& level : mp.value("levels", nlohmann::json::array()))
            {
                add(level.value("code", ""), sector, level.value("mapFile", ""), 9, 0, true);
            }
        }

        return levels;
    }
}
