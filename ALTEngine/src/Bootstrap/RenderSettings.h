#pragma once

#include "AppWindow.h"
#include "Config.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

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
        explicit RenderSettings(Config& cfg) : config(cfg) {}

        RenderFidelity Get() const
        {
            auto value = config.Get("RenderFidelity");
            return (value.has_value() && *value == "Smoothed") ? RenderFidelity::Smoothed : RenderFidelity::Original;
        }

        void Set(RenderFidelity fidelity)
        {
            config.Set("RenderFidelity", fidelity == RenderFidelity::Smoothed ? "Smoothed" : "Original");
        }

        // VSync - on by default, matching AppWindow::EnsureCreated's own
        // initial setting (Edward, 2026).
        bool VSync() const
        {
            auto value = config.Get("VSync");
            return !value.has_value() || *value != "Off";
        }

        void SetVSync(bool enabled)
        {
            config.Set("VSync", enabled ? "On" : "Off");
        }

        // Display mode - fullscreen by default, matching
        // AppWindow::EnsureCreated's own initial window flags.
        DisplayMode GetDisplayMode() const
        {
            auto value = config.Get("DisplayMode");
            if (value == "Windowed") { return DisplayMode::Windowed; }
            if (value == "Borderless") { return DisplayMode::Borderless; }
            return DisplayMode::Fullscreen;
        }

        void SetDisplayMode(DisplayMode mode)
        {
            const char* value = "Fullscreen";
            if (mode == DisplayMode::Windowed) { value = "Windowed"; }
            else if (mode == DisplayMode::Borderless) { value = "Borderless"; }
            config.Set("DisplayMode", value);
        }

        // The graphics API the 3D is drawn with (Options > Graphics >
        // Renderer): one of SDL's GPU drivers - "vulkan", "direct3d12",
        // "metal" - or empty for SDL's own choice ("Auto", the default). Read
        // when the GPU device is made (ModelRenderer::Initialize), so a change
        // takes effect the next time ALTEngine starts. A driver this machine
        // cannot run falls back to Auto there.
        std::string GpuDriver() const
        {
            auto value = config.Get("GpuDriver");
            if (!value.has_value() || *value == "Auto") { return {}; }
            return *value;
        }

        void SetGpuDriver(const std::string& driver)
        {
            config.Set("GpuDriver", driver.empty() ? "Auto" : driver);
        }

        // The GPU drivers SDL has and this machine can run, in SDL's order,
        // for the menu - with the shader formats ALTEngine ships (DXIL,
        // SPIR-V, MSL; data/shaders).
        static std::vector<std::string> AvailableGpuDrivers()
        {
            // Asked once: checking a driver can start its API.
            static const std::vector<std::string> available = [] {
            std::vector<std::string> out;
            const SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;
            const int count = SDL_GetNumGPUDrivers();
            for (int i = 0; i < count; ++i)
            {
                const char* name = SDL_GetGPUDriver(i);
                if (name && SDL_GPUSupportsShaderFormats(formats, name)) { out.emplace_back(name); }
            }
            return out;
            }();
            return available;
        }

        // The menu's names for the drivers, and back.
        static const char* DriverLabel(const std::string& driver)
        {
            if (driver == "vulkan") { return "Vulkan"; }
            if (driver == "direct3d12") { return "Direct3D 12"; }
            if (driver == "metal") { return "Metal"; }
            return "Auto";
        }
        static std::string DriverForLabel(const std::string& label)
        {
            if (label == "Vulkan") { return "vulkan"; }
            if (label == "Direct3D 12") { return "direct3d12"; }
            if (label == "Metal") { return "metal"; }
            return {};
        }

    private:
        Config& config;
    };
}
