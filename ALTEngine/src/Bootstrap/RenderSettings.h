#pragma once

#include "Config.h"

namespace ALTEngine::Bootstrap
{
    enum class RenderFidelity
    {
        Original, // vertex snapping, affine texture warp, no perspective-correct interpolation
        Smoothed, // standard modern 3D rendering
    };

    // Persisted via Config so the choice survives between runs. Doesn't
    // affect anything yet - there's no real 3D renderer to apply it to
    // until the SDL GPU pipeline exists - but the menu option and its
    // persistence are real starting now, so there's somewhere for that
    // pipeline to read from when it lands.
    class RenderSettings
    {
    public:
        explicit RenderSettings(Config& config) : config(config) {}

        RenderFidelity Get() const
        {
            auto value = config.Get("RenderFidelity");
            return (value.has_value() && *value == "Smoothed") ? RenderFidelity::Smoothed : RenderFidelity::Original;
        }

        void Set(RenderFidelity fidelity)
        {
            config.Set("RenderFidelity", fidelity == RenderFidelity::Smoothed ? "Smoothed" : "Original");
        }

    private:
        Config& config;
    };
}
