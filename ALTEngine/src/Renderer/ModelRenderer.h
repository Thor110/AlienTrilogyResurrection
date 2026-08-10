#pragma once

#include "../Formats/BndTextureLoader.h"
#include "../Formats/RenderMesh.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Renderer
{
    // A free-roaming FPS-style camera - genuinely different from the
    // model previews' auto-framed, bounding-sphere-based camera. Levels
    // need an actual explorable viewpoint (position + look direction),
    // not a fixed orbit around a single small object.
    struct FpsCamera
    {
        float x = 0, y = 0, z = 0;
        float yaw = 0;   // radians, rotation around Y (turning left/right)
        float pitch = 0; // radians, looking up/down
        float fovYRadians = 1.221f; // ~70 degrees, standard FPS default
    };

    // Renders OPTOBJ/PICKMOD/OBJ3D models via SDL_GPU. Architecture
    // note: the rest of the menu is drawn with the existing SDL_Renderer
    // 2D API, which doesn't directly share resources with SDL_GPU -
    // rather than rely on uncertain GPU-texture interop between the two,
    // this renders to an offscreen SDL_GPU color target, downloads the
    // result to CPU (SDL_DownloadFromGPUTexture), and hands back plain
    // RGBA bytes ready for SDL_UpdateTexture - the exact same mechanism
    // already used for the splash background images. Costs a
    // GPU->CPU->GPU round trip per frame, which is an acceptable trade
    // for reliability given this is one small menu preview, not
    // gameplay-critical performance.
    // Which model catalog a ModelCacheKey refers to - OPTOBJ (menu
    // items like the harddrives/computer/etc), PICKMOD (pause menu
    // weapons/equipment), and OBJ3D (level objects - crates, barrels,
    // switches, and other destructible/interactable world objects, per
    // Edward 2026 - NOT doors/lifts, which live entirely within the
    // .MAP format itself) are three separate BND files with their own,
    // independently-numbered index ranges, so the same modelIndex means
    // a different model depending on which catalog it's from.
    enum class ModelCatalog : int32_t
    {
        Optobj,
        Pickmod,
        Obj3d,
        // Door and lift meshes, which live in the level's own .MAP under
        // "D"/"L" sections rather than a model BND. modelIndex packs the
        // mesh and its texture page as mesh * 8 + page, since one door
        // mesh can span several of the level's texture pages and each
        // page needs its own draw call.
        LevelDoor,
        LevelLift,
    };

    // A cheap-to-copy/hash/compare model cache key - replaces what used
    // to be a string built as `cachePrefix + ":" + std::to_string(index)`
    // at every call site (Edward, 2026: "wouldn't it be more practical
    // to just use the model index as an integer rather than a string
    // comparison?" - yes, and this also centralizes the key-building
    // that string concatenation was duplicating everywhere).
    struct ModelCacheKey
    {
        ModelCatalog catalog = ModelCatalog::Optobj;
        int32_t modelIndex = -1;

        bool operator==(const ModelCacheKey&) const = default;
    };

    struct ModelCacheKeyHash
    {
        size_t operator()(const ModelCacheKey& key) const noexcept
        {
            return std::hash<int64_t>{}((static_cast<int64_t>(key.catalog) << 32) | static_cast<uint32_t>(key.modelIndex));
        }
    };

    // One entry per model to batch-load via PreloadBatch - same
    // parameters LoadModel takes individually.
    struct PreloadRequest
    {
        ModelCacheKey cacheKey;
        int meshNumber = -1;
        std::filesystem::path objBndPath;
        std::filesystem::path gfxBndPath;
        std::optional<std::array<uint8_t, 3>> transparentRgb;
        float baseRotationRadians = 0.0f;
    };

    // A single positioned, oriented instance of an already-loaded model
    // (crate, monster, pickup, door, lift...) drawn alongside a level's
    // static geometry in the same pass. `cacheKey` must already be
    // loaded via LoadModel/PreloadBatch - this doesn't load anything
    // itself, matching RenderLevelToRgba's own "level must already be
    // loaded" convention (Edward, 2026: "get the map rendering properly
    // and all of the level objects spawning").
    struct PlacedObject
    {
        ModelCacheKey cacheKey;
        float x = 0, y = 0, z = 0;
        float rotationRadians = 0.0f;
        // Non-uniform, and applied to the model matrix rather than the
        // vertices. Objects (crates, switches) are drawn at 0.875 uniform in
        // the original (Ghidra: FUN_0003765c); doors are drawn 1:1. The
        // non-uniform 0.75/0.85/0.75 triple belongs to the enemy draw path
        // (FUN_000377e4) - see the note at the object placement site.
        float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
        bool visible = true; // cleared when a pickup is collected, rather than rebuilding the list
    };

    class ModelRenderer
    {
    public:
        // Creates the SDL_GPU device, loads shaders, builds the pipeline.
        // Call once. Returns false (logs why) on any failure - the menu
        // should keep working with placeholder boxes if this fails, not
        // crash the whole program over a missing GPU feature.
        static bool Initialize();

        // Full teardown - device, both pipelines, sampler, the shared
        // render target, and every cached model AND level. Only meant
        // for true application exit (main.cpp), not screen transitions -
        // see UnloadLevels() for that instead. Edward, 2026: this used
        // to also get called when leaving the menu (transitioning into
        // gameplay) and when leaving gameplay, which meant the small,
        // cheap-to-keep OPTOBJ/PICKMOD/OBJ3D model cache got destroyed
        // as collateral damage every time - forcing a full reload from
        // scratch (including the ~80x-slower-than-warm first render
        // cost) on the next screen that needed any of them, even though
        // model data is small and worth keeping resident for the whole
        // app lifetime, unlike level geometry.
        static void Shutdown();

        // Frees only the currently-loaded levels' GPU resources (vertex/
        // index buffers, textures) - NOT the model cache, NOT the
        // device/pipelines. This is what GameplayScreen should call when
        // leaving a level: level geometry is the actual large, level-
        // specific thing worth freeing (tens of thousands of triangles,
        // up to 5 textures), unlike the small OPTOBJ/PICKMOD/OBJ3D model
        // cache, which is cheap to keep around and expensive to reload
        // (Edward, 2026).
        static void UnloadLevels();

        // Graphics > Quality. Smoothed = bilinear texel filtering; Original =
        // point sampling, i.e. the original's hard square pixels. Takes effect
        // on the next frame - no pipeline rebuild, just a sampler swap - so
        // this is safe to call from the menu at any time.
        static void SetTextureSmoothing(bool smoothed);
        static bool TextureSmoothing();

        // The original's draw-distance fade to black. Enabled = original
        // behaviour; disabled = unlimited draw distance. Driven by the
        // Modern > Render Distance feature (which is the inverse: the feature
        // ON means the fade OFF).
        static void SetDrawDistanceFade(bool enabled);

        // Loads and GPU-uploads a model, cached under `cacheKey` (repeated
        // calls with the same key are cheap) - callers should use a
        // catalog-prefixed key (e.g. "OPTOBJ:9", "PICKMOD:6") since the
        // same meshNumber means something different in each catalog.
        //
        // `meshNumber` is looked up by section NUMBER (via
        // ModelLoader::FindByNumber), not array position - required for
        // PICKMOD.BND, whose section naming has confirmed gaps (missing
        // M005/M024). OPTOBJ.BND happens to be gap-free, so this works
        // there too without needing a separate code path.
        //
        // The texture pairing scheme is auto-detected from the loaded
        // BndTextureSet: if it has exactly one BX section, every model
        // in the file shares that one texture+UV group (confirmed for
        // PICKGFX.BND - used by both PICKMOD and OBJ3D); otherwise each
        // model uses the BX section at its own meshNumber (confirmed for
        // OPTGFX.BND, one dedicated texture per OPTOBJ model).
        //
        // `transparentRgb`, if set, is passed through to BndTextureLoader
        // for this model's texture (see RawImageRenderer::RenderRGBA) -
        // needed for OPTGFX's Music/SFX speaker models and the Multitap
        // model, which use a colour key rather than the usual "black is
        // just opaque material colour" convention (Edward, 2026).
        //
        // `baseRotationRadians` is a fixed per-model orientation offset,
        // added to whatever spin angle RenderToRgba is called with each
        // frame - some models (e.g. NetworkedComputers) are authored at
        // a different natural resting angle than most, and need this to
        // display correctly rather than always starting from the same
        // default orientation (Edward, 2026).
        //
        // Returns false if the model/texture couldn't be loaded.
        static bool LoadModel(ModelCacheKey cacheKey, int meshNumber,
                               const std::filesystem::path& objBndPath, const std::filesystem::path& gfxBndPath,
                               std::optional<std::array<uint8_t, 3>> transparentRgb = std::nullopt,
                               float baseRotationRadians = 0.0f);

        // Same end result as calling LoadModel once per request, but all
        // GPU uploads share a single transfer buffer, command buffer,
        // and submit - Edward, 2026: ~40 individual LoadModel calls
        // during boot preload (14 OPTOBJ + 26 PICKMOD) were taking 2-3
        // seconds despite the combined data being under 1MB, because
        // each one independently created and tore down its own transfer
        // buffer and issued its own command buffer submission - roughly
        // 13 separate GPU API calls per model, ~520 total. Real GPU
        // drivers have meaningful fixed overhead per submission/resource
        // creation regardless of payload size, so the call count (not
        // the data volume) was the actual cost. This does the same CPU-
        // side work per model (parse, build render mesh, compute AABB)
        // but creates one shared transfer buffer sized for everything
        // combined, maps it once, and issues all the upload calls inside
        // a single copy pass before one final submit.
        static void PreloadBatch(const std::vector<PreloadRequest>& requests);

        // Uploads an already-built mesh and texture straight into the
        // model cache. Door and lift meshes come from the level file and
        // are textured from the level's own pages, so they cannot go
        // through LoadModel's BND-path-based loading - this is how they
        // get registered before being drawn as PlacedObjects.
        static bool RegisterMesh(ModelCacheKey cacheKey,
                                 const ALTEngine::Formats::RenderMesh& renderMesh,
                                 const ALTEngine::Formats::BndTexture& texture);

        // Renders the model cached under `cacheKey` (must already be
        // loaded via LoadModel), rotated by `angleRadians` around Y,
        // into a `width`x`height` offscreen target, and returns the
        // result as tightly-packed RGBA8 bytes (width*height*4), or an
        // empty vector on failure.
        static std::vector<uint8_t> RenderToRgba(ModelCacheKey cacheKey, float angleRadians, int width, int height);

        // Loads and GPU-uploads an entire level's static geometry,
        // cached under `cacheKey`. Unlike LoadModel (one texture, one
        // draw call), a level can use up to 5 different textures - see
        // Formats/RenderMesh.h's BuildLevelRenderMeshPerGroup - so this
        // internally creates up to 5 vertex/index/texture sets, one per
        // BX group actually used by the level.
        //
        // `mapPath` is the level's .MAP file; `gfxPath` is its paired
        // {name}GFX.B16 texture file (both confirmed real-data formats -
        // see LevelLoader.h/RenderMesh.h).
        //
        // UNTESTED beyond CPU-side data preparation (LevelLoader/
        // BuildLevelRenderMeshPerGroup are both confirmed against real
        // data) - the actual GPU upload/render path for a FULL LEVEL
        // (tens of thousands of triangles, up to 5 textures, real-time
        // FPS camera) hasn't been exercised the way the small model
        // previews have. Verify on real hardware before trusting this
        // the way ModelRenderer's model path is trusted.
        static bool LoadLevel(const std::string& cacheKey, const std::filesystem::path& mapPath, const std::filesystem::path& gfxPath);

        // Advances the level's light blink state machines by one tick and,
        // if any light's resolved colour actually changed, rebuilds and
        // re-uploads the level's vertex buffers.
        //
        // Call this once per gameplay tick, before RenderLevelToRgba.
        // Cheap on the great majority of ticks: it steps 128 small state
        // machines and compares 128 resolved entries, and only touches the
        // GPU when one of them moved. `randomBits` supplies the timing
        // jitter the original takes from its RNG - pass a real random
        // value, or a fixed one for reproducible blink timing.
        //
        // Returns true if the buffers were rebuilt, which is useful mainly
        // for logging and tests.
        static bool TickLevelLights(const std::string& cacheKey, int randomBits);

        // Applies script command 0 (ToggleLight) to one light record,
        // which is what arms a blink record - a mode 1 or 3 light does
        // nothing at all until its variant counter has been driven to its
        // maximum. Takes effect on the next TickLevelLights.
        static void ToggleLevelLight(const std::string& cacheKey, int lightIndex, int delta);

        // Raises the one-shot global brightness flash (weapon fire,
        // explosions). Cleared on the next tick.
        static void FlashLevelLights(const std::string& cacheKey);

        // Applies script command 9 (ChangeTexture) to one texture animator.
        // This is what arms an animated face that only changes when something
        // triggers it - L111's seven group-3 control panels each wait for
        // their own switch this way. Takes effect on the next TickLevelLights.
        static void ChangeLevelTexture(const std::string& cacheKey, int animatorIndex, int delta);


        // viewpoint. Unlike RenderToRgba's auto-framed single-model
        // camera, this uses a real explorable FPS camera and a fixed
        // FOV/near/far appropriate to the level's own coordinate scale
        // (near/far chosen generously since level geometry spans tens of
        // thousands of units, confirmed from real L111LEV.MAP vertex
        // ranges).
        //
        // `objects`, if non-empty, are drawn in the same pass right
        // after the level geometry - each one must already be loaded
        // via LoadModel/PreloadBatch under its own cacheKey (silently
        // skipped otherwise, same "must already be loaded" convention
        // as the level itself). This is how monsters/pickups/crates/
        // doors/lifts actually appear in a level, since level geometry
        // alone has no gameplay objects in it at all - those are a
        // separate list in LevelGeometry, not part of the MAP0 mesh
        // (Edward, 2026).
        static std::vector<uint8_t> RenderLevelToRgba(const std::string& cacheKey, const FpsCamera& camera, int width, int height,
                                                        const std::vector<PlacedObject>& objects = {});
    };
}
