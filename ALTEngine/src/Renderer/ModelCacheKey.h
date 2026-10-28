#pragma once

// Split out of ModelRenderer.h so game logic (DamageSystem, the shatter
// effect) can name a model without pulling in SDL.

#include <cstddef>
#include <cstdint>
#include <functional>

namespace ALTEngine::Renderer
{
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
}
