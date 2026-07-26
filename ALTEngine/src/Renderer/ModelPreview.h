#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>

struct SDL_Renderer;

namespace ALTEngine::Renderer
{
    // Which BND pair to load the model from, plus the bits that vary
    // per-model (transparency, base rotation). Bundled into one struct
    // since most of it is constant per call site (e.g. always
    // OPTOBJ.BND/OPTGFX.BND), not per-frame.
    struct ModelPreviewSource
    {
        std::filesystem::path objBndPath;
        std::filesystem::path gfxBndPath;
        std::string cachePrefix; // "OPTOBJ" or "PICKMOD" - becomes the ModelRenderer cache key prefix
        int modelIndex = -1;
        std::optional<std::array<uint8_t, 3>> transparentRgb; // colour-key cutout models (speakers, Multitap) - see MenuController's Volume() TODO
        float baseRotationRadians = 0.0f;                     // fixed per-model orientation offset, e.g. NetworkedComputers

        // Centralizes the OPTOBJ.BND/OPTGFX.B16 paths and "OPTOBJ" cache
        // prefix in one place. Edward, 2026: this construction (path +
        // path + prefix, just modelIndex differing per call) was
        // duplicated at 4 separate call sites - MenuController,
        // PauseMenuScreen, SaveSlotScreen, MultiplayerScreens - which is
        // exactly why fixing OPTGFX.BND -> OPTGFX.B16 (the 8-bit vs
        // 16-bit palette mixup) needed touching all 4 files instead of
        // one. Callers still set transparentRgb/baseRotationRadians
        // themselves afterward when needed (Multitap/speakers,
        // NetworkedComputers) since those genuinely vary per model, not
        // per call site.
        static ModelPreviewSource ForOptobj(const std::filesystem::path& cdDirectory, int modelIndex)
        {
            ModelPreviewSource source;
            source.objBndPath = cdDirectory / "GFX" / "OPTOBJ.BND";
            source.gfxBndPath = cdDirectory / "GFX" / "OPTGFX.B16";
            source.cachePrefix = "OPTOBJ";
            source.modelIndex = modelIndex;
            return source;
        }

        // Same idea for PICKMOD.BND/PICKGFX.BND (the pause menu's
        // weapon/equipment models) - currently only one call site
        // (PauseMenuScreen), but centralized to match ForOptobj and stay
        // consistent if that changes.
        static ModelPreviewSource ForPickmod(const std::filesystem::path& cdDirectory, int modelIndex)
        {
            ModelPreviewSource source;
            source.objBndPath = cdDirectory / "GFX" / "PICKMOD.BND";
            source.gfxBndPath = cdDirectory / "GFX" / "PICKGFX.BND";
            source.cachePrefix = "PICKMOD";
            source.modelIndex = modelIndex;
            return source;
        }
    };

    // Renders a spinning model preview into the (x,y,w,h) destination
    // rectangle on `renderer` - the full routine (idempotent Initialize,
    // LoadModel, RenderToRgba, SDL_Texture upload/draw/cleanup) that was
    // duplicated near-identically across MenuController's Options
    // screen, SaveSlotScreen, and MultiplayerScreens (Edward, 2026).
    // Callers passing a full-screen rect (0, 0, windowW, windowH) get
    // the same "fills the background" behaviour those screens already
    // had - this doesn't change that, just shares the code.
    //
    // Returns false if the model couldn't be loaded/rendered (GPU
    // unavailable, missing BND, etc) - callers decide what to do on
    // failure (e.g. draw their own placeholder, matching each screen's
    // existing behaviour, or just skip the way SaveSlotScreen/
    // MultiplayerScreens already did).
    bool DrawModelPreview(SDL_Renderer* renderer, const ModelPreviewSource& source,
                          int x, int y, int w, int h, float rotationAngle);
}
