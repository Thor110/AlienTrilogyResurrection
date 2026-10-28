#include "Progression.h"

namespace ALTEngine::Game::Progression
{
    std::optional<Formats::LevelListEntry> NextLevel(const std::vector<Formats::LevelListEntry>& levels,
                                                     const std::string& levelDigits)
    {
        for (size_t i = 0; i < levels.size(); ++i)
        {
            if (levels[i].digits != levelDigits || levels[i].multiplayer) { continue; }
            for (size_t j = i + 1; j < levels.size(); ++j)
            {
                if (!levels[j].multiplayer) { return levels[j]; }
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<Formats::LevelListEntry> Find(const std::vector<Formats::LevelListEntry>& levels,
                                                const std::string& levelDigits)
    {
        for (const auto& entry : levels)
        {
            if (entry.digits == levelDigits) { return entry; }
        }
        return std::nullopt;
    }

    Assessment Assess(int aliensKilled, int aliensTotal)
    {
        Assessment a;
        a.aliensPercent = aliensTotal > 0 ? 100.0f * static_cast<float>(aliensKilled) / static_cast<float>(aliensTotal) : 100.0f;
        a.secretsPercent = 0.0f;
        a.missionPercent = 100.0f;
        return a;
    }
}
