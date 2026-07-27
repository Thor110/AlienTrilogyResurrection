#pragma once

#include "Config.h"

#include <algorithm>
#include <cstdlib>

namespace ALTEngine::Bootstrap
{
    // Music/SFX volume, 0-10 (matching MenuNode::sliderValue's own
    // scale) - persisted via Config, matching RenderSettings' own
    // pattern. Edward, 2026: "Functional volume sliders effecting the
    // music which is all we have to test against currently."
    class AudioSettings
    {
    public:
        explicit AudioSettings(Config& config) : config(config) {}

        int MusicVolume() const { return GetOr("MusicVolume", 8); }
        void SetMusicVolume(int volume) { config.Set("MusicVolume", std::to_string(std::clamp(volume, 0, 10))); }

        int SfxVolume() const { return GetOr("SfxVolume", 8); }
        void SetSfxVolume(int volume) { config.Set("SfxVolume", std::to_string(std::clamp(volume, 0, 10))); }

    private:
        int GetOr(const char* key, int fallback) const
        {
            auto value = config.Get(key);
            if (!value.has_value()) { return fallback; }
            return std::clamp(std::atoi(value->c_str()), 0, 10);
        }

        Config& config;
    };
}
