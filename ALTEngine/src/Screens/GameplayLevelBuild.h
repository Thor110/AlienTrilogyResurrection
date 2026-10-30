#pragma once

#include "../Formats/BndTextureLoader.h"
#include "../Formats/ModelLoader.h"
#include "../Formats/RenderMesh.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ALTEngine::Screens::GameplayLevelBuild
{
    // Pieces of a level's setup that the gameplay screen uploads to the
    // renderer, built here without touching it so they can be tested.

    // One texture rect cut out of its page, as its own RGBA image.
    struct CutTexture
    {
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
    };

    // The rect `index` of a level's textures, cut from its page; nothing if
    // the level has no such rect. A window's flying glass wears one of these
    // (GameplayObjects::GlassTextureIndex).
    inline std::optional<CutTexture> CutTextureRect(const Formats::BndTextureSet& set, size_t index)
    {
        if (index >= set.uvRects.size()) { return std::nullopt; }
        const auto& r = set.uvRects[index];
        if (r.page < 0 || static_cast<size_t>(r.page) >= set.textures.size() || r.width <= 0 || r.height <= 0)
        {
            return std::nullopt;
        }
        const auto& page = set.textures[static_cast<size_t>(r.page)];
        CutTexture cut;
        cut.width = r.width;
        cut.height = r.height;
        cut.rgba.assign(static_cast<size_t>(r.width) * r.height * 4, 0);
        for (int y = 0; y < r.height; ++y)
        {
            for (int x = 0; x < r.width; ++x)
            {
                const int sx = std::min(r.x + x, page.width - 1);
                const int sy = std::min(r.y + y, page.height - 1);
                for (int c = 0; c < 4; ++c)
                {
                    cut.rgba[(static_cast<size_t>(y) * r.width + x) * 4 + c] =
                        page.rgba[(static_cast<size_t>(sy) * page.width + sx) * 4 + c];
                }
            }
        }
        return cut;
    }

    // The cryopod cover for one level: OBJ3D model 0x28, every face routine 14
    // (FUN_000367b4 / FUN_00036700), a flat colour half over what is behind.
    // The colour is entry 2 of CLUT slot 0 - the level's first palette (span
    // mode 6, 0x5eceb) - drawn over a white texel so it shows as it is.
    struct CryopodCover
    {
        Formats::RenderMesh mesh;
        Formats::BndTexture texture;     // one white texel
        std::array<int, 3> colour{};     // 0-255, for the log
        // Which way the cover tips open. The original turns it -0x100 about
        // its own Z, which lifts it; the model file is not to hand to check
        // the sign in the port's frame, so it turns whichever way lifts the
        // side most of the model lies on.
        float openSign = -1.0f;
    };

    inline std::optional<CryopodCover> BuildCryopodCover(const Formats::ModelMesh& coverMesh,
                                                         const std::vector<uint8_t>& levelFirstPalette)
    {
        if (levelFirstPalette.size() < 9) { return std::nullopt; }
        CryopodCover cover;
        // The palette is in the 0-63 range; the pages are drawn from it times 4
        // (RawImageRenderer).
        std::array<float, 3> colour{};
        for (int c = 0; c < 3; ++c)
        {
            cover.colour[static_cast<size_t>(c)] = levelFirstPalette[static_cast<size_t>(6 + c)] * 4;
            colour[static_cast<size_t>(c)] = static_cast<float>(cover.colour[static_cast<size_t>(c)]) / 255.0f;
        }
        const std::vector<Formats::BxRectangle> whiteRect = { { 0, 0, 1, 1, 0, 0 } };
        cover.mesh = Formats::BuildRenderMesh(coverMesh, whiteRect, true, colour);
        cover.texture.width = 1;
        cover.texture.height = 1;
        cover.texture.rgba = { 255, 255, 255, 255 };
        float sumX = 0.0f;
        for (const auto& v : coverMesh.vertices) { sumX += static_cast<float>(v.x); }
        cover.openSign = (sumX < 0.0f) ? -1.0f : 1.0f;
        return cover;
    }
}
