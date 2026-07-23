#pragma once

#include <optional>
#include <string>

#include "Config.h"

namespace ALTEngine::Bootstrap
{
    class ResolutionSettings
    {
    public:
        explicit ResolutionSettings(Config& config) : config(config) {}

        // Returns the saved (width, height), or std::nullopt if none is
        // saved yet (caller should fall back to the desktop's current mode).
        std::optional<std::pair<int, int>> Get() const
        {
            auto w = config.Get("ResolutionWidth");
            auto h = config.Get("ResolutionHeight");
            if (!w.has_value() || !h.has_value()) { return std::nullopt; }
            try
            {
                return std::make_pair(std::stoi(*w), std::stoi(*h));
            }
            catch (const std::exception&)
            {
                return std::nullopt; // corrupt config value - treat as unset rather than throwing
            }
        }

        void Set(int width, int height)
        {
            config.Set("ResolutionWidth", std::to_string(width));
            config.Set("ResolutionHeight", std::to_string(height));
        }

    private:
        Config& config;
    };
}
