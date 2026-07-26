#pragma once

#include "ModelRenderer.h"

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
        ModelCatalog catalog = ModelCatalog::Optobj;
        int modelIndex = -1;
        std::optional<std::array<uint8_t, 3>> transparentRgb; // colour-key cutout models (speakers, Multitap) - see MenuController's Volume() TODO
        float baseRotationRadians = 0.0f;                     // fixed per-model orientation offset, e.g. NetworkedComputers

        // The ModelRenderer cache key for this source - centralizes the
        // catalog+index -> ModelCacheKey construction that used to be a
        // string built as `cachePrefix + ":" + std::to_string(index)` at
        // every call site (Edward, 2026: "wouldn't it be more practical
        // to just use the model index as an integer rather than a
        // string comparison?").
        ModelCacheKey CacheKey() const { return { catalog, modelIndex }; }

        // Centralizes the OPTOBJ.BND/OPTGFX.B16 paths and OPTOBJ catalog
        // in one place. Edward, 2026: this construction (path + path +
        // catalog, just modelIndex differing per call) was duplicated
        // at 4 separate call sites - MenuController, PauseMenuScreen,
        // SaveSlotScreen, MultiplayerScreens - which is exactly why
        // fixing OPTGFX.BND -> OPTGFX.B16 (the 8-bit vs 16-bit palette
        // mixup) needed touching all 4 files instead of one. Callers
        // still set transparentRgb/baseRotationRadians themselves
        // afterward when needed (Multitap/speakers, NetworkedComputers)
        // since those genuinely vary per model, not per call site.
        static ModelPreviewSource ForOptobj(const std::filesystem::path& cdDirectory, int modelIndex)
        {
            ModelPreviewSource source;
            source.objBndPath = cdDirectory / "GFX" / "OPTOBJ.BND";
            source.gfxBndPath = cdDirectory / "GFX" / "OPTGFX.B16";
            source.catalog = ModelCatalog::Optobj;
            source.modelIndex = modelIndex;

            // Multitap (3) and the Music/SFX speaker models (11/12) use a
            // colour key (black) for transparency rather than most
            // OPTOBJ models' "black is just opaque material colour"
            // convention - see MenuTree.cpp's Volume() TODO and
            // RawImageRenderer's transparentRgb parameter (Edward, 2026).
            // Lives here rather than at each call site so every caller
            // (including preloading) gets this automatically.
            if (modelIndex == 3 || modelIndex == 11 || modelIndex == 12)
            {
                source.transparentRgb = std::array<uint8_t, 3>{ 0, 0, 0 };
            }
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
            source.catalog = ModelCatalog::Pickmod;
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

    // Warms ModelRenderer's cache for every OPTOBJ model (0-13, the full
    // catalog) and every PICKMOD model (0-25, skipping the confirmed
    // gaps at 5/24 - LoadModel already fails gracefully for those rather
    // than crashing, so this doesn't need to special-case them).
    //
    // Safe to call even if ModelRenderer::Initialize() hasn't run yet
    // (calls it itself) or if the GPU pipeline is unavailable (each
    // LoadModel call just fails and moves on, same as any other
    // placeholder-fallback path in this codebase).
    // NOTE: this blocks for the full duration (roughly 1-1.5s for the
    // full catalog, even after PreloadBatch's GPU-upload batching -
    // resource creation itself, one CreateGPUBuffer/CreateGPUTexture
    // call per model, can't be batched the same way) - not used for
    // boot preloading (see MenuController::Run's own incremental,
    // one-model-per-frame preload queue instead, which never blocks the
    // window for more than a single frame at a stretch). Kept here as a
    // simple, synchronous option for anywhere else that might want to
    // force a full preload without needing that.
    void PreloadAllModels(const std::filesystem::path& cdDirectory);
}
