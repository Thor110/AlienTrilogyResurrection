#pragma once

#include "ModelRenderer.h"
#include "../Bootstrap/Localization.h"
#include "../Formats/Obj3DTexture.h"

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

        // OBJ3D.BND (level objects - crates, barrels, switches, and
        // other destructible/interactable world objects, per Edward
        // 2026) doesn't share one fixed texture file the way OPTOBJ/
        // PICKMOD do - which of PNL0GFX/PNL1GFX/PICKGFX applies depends
        // on the mesh number itself (see Obj3DTexture.h's own doc
        // comment for the exact ranges), and the PNL*GFX files are
        // further language-suffixed. gfxBndPath is left empty if
        // ResolveObj3DTextureFile can't find the right file - LoadModel
        // already fails gracefully on a missing/unreadable path, same
        // as every other model-loading failure mode in this codebase.
        static ModelPreviewSource ForObj3D(const std::filesystem::path& cdDirectory, int modelIndex,
                                            ALTEngine::Bootstrap::Language language)
        {
            ModelPreviewSource source;
            source.objBndPath = cdDirectory / "GFX" / "OBJ3D.BND";
            source.gfxBndPath = ALTEngine::Formats::ResolveObj3DTextureFile(cdDirectory, modelIndex, language).value_or(std::filesystem::path{});
            source.catalog = ModelCatalog::Obj3d;
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

    // Warms ModelRenderer's cache for the full OPTOBJ catalog (0-13, no
    // known gaps). This is the one used for the menu/options screen -
    // see MenuController::Run's own incremental, one-model-per-frame
    // preload queue for how it's actually invoked at boot, which this
    // function itself isn't (this blocks for the full duration, roughly
    // a few hundred ms even after PreloadBatch's GPU-upload batching -
    // resource creation itself, one CreateGPUBuffer/CreateGPUTexture
    // call per model, can't be batched the same way). Kept here as a
    // simple, synchronous option for anywhere else that might want to
    // force a full OPTOBJ preload without needing the incremental
    // version.
    void PreloadOptobjModels(const std::filesystem::path& cdDirectory);

    // Warms ModelRenderer's cache for PICKMOD (0-27, gaps at 5/24 -
    // Edward, 2026, corrected from an earlier wrong 0-25 range) and
    // OBJ3D (0-41, no known gaps) - the two catalogs gameplay itself
    // needs (pause-menu weapons and level objects respectively) but the
    // main menu doesn't. Edward, 2026: "we don't need PICKMOD.BND to
    // load until we are loading into a level" - meant to be called from
    // MissionBriefingScreen's loading phase, not at boot, since that's
    // the natural point where the player is looking at a loading
    // screen right before a level starts (whether starting fresh or,
    // once save/load exists, resuming) rather than the main menu, which
    // never needs either catalog.
    void PreloadGameplayModels(const std::filesystem::path& cdDirectory, ALTEngine::Bootstrap::Language language);
}
