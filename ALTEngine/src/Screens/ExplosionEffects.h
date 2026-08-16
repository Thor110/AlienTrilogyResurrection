#pragma once

#include "../Formats/BndTextureLoader.h"
#include "../Formats/ExplosionGraphics.h"
#include "../Formats/SpriteAnimator.h"
#include "../Renderer/ModelRenderer.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Screens
{
    // Explosions.
    //
    // EXPLGFX.B16 is the same texture-page format as the level graphics, so
    // BndTextureLoader reads it with no new format work: two 256x256 pages, each
    // with its own CL palette, and BX rectangles saying where each frame sits.
    //
    // Page 1 is the barrel blast - nine 84x84 frames in a 3x3 grid filling the
    // page exactly. Uniform size, uniform spacing, nothing else on the page.
    // Page 0 carries several smaller effects packed together.
    //
    // Frames advance on the animation VM, the same one weapons and enemies use,
    // so an explosion is a sequence like any other. The sequence itself is NOT
    // traced - the original's is in code data that has not been read - so the
    // duration here is a guess and is marked as one in ExplosionGraphics.h.
    class ExplosionEffects
    {
    public:
        static constexpr const char* SHEET_KEY = "explgfx";

        // Reads the file and uploads the barrel page as one sprite sheet. Safe
        // to call more than once; only the first does the work.
        bool Load(const std::filesystem::path& cdDirectory)
        {
            if (loaded) { return true; }

            std::filesystem::path path = cdDirectory / "EXPLGFX.B16";
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
            {
                path = cdDirectory / "GFX" / "EXPLGFX.B16";
                if (!std::filesystem::exists(path, ec)) { return false; }
            }

            ALTEngine::Formats::BndTextureSet set;
            try
            {
                // PALETTE-INDEX TRANSPARENCY, one entry per page. Without it the
                // blast draws as an opaque 84x84 square: the fragment shader cuts
                // out on alpha below 0.5, and every texel decodes fully opaque
                // unless the loader is told which palette index means "nothing".
                //
                // Index 0, and that is checked rather than assumed: on the
                // barrel page the top-left texel of all nine frames is 0, the
                // unused strip past the grid is entirely 0, and 0 accounts for
                // 49,389 of the page's 65,536 texels - three quarters of it. It
                // is the background.
                const std::vector<std::vector<int>> transparentIndices{ { 0 }, { 0 } };
                set = ALTEngine::Formats::BndTextureLoader::Load(path, std::nullopt, transparentIndices);
            }
            catch (const std::exception&)
            {
                return false;
            }

            const int page = ALTEngine::Formats::ExplosionGraphics::BARREL_PAGE;
            if (static_cast<size_t>(page) >= set.textures.size()) { return false; }
            const auto& texture = set.textures[static_cast<size_t>(page)];

            if (!ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                    SHEET_KEY, texture.rgba, texture.width, texture.height))
            {
                return false;
            }
            sheetWidth = texture.width;
            sheetHeight = texture.height;

            // Keep only the rectangles belonging to the barrel page, in order.
            for (const auto& rect : set.uvRects)
            {
                if (rect.page == page) { frames.push_back(rect); }
            }

            // The page is a clean grid, so if the BX chunk is missing or odd the
            // rectangles can be reconstructed from the geometry rather than
            // giving up on the effect entirely.
            if (frames.size() < static_cast<size_t>(ALTEngine::Formats::ExplosionGraphics::BARREL_FRAME_COUNT))
            {
                frames.clear();
                const int size = ALTEngine::Formats::ExplosionGraphics::BARREL_FRAME_SIZE;
                for (int i = 0; i < ALTEngine::Formats::ExplosionGraphics::BARREL_FRAME_COUNT; ++i)
                {
                    ALTEngine::Formats::BxRectangle rect{};
                    rect.x = (i % 3) * size;
                    rect.y = (i / 3) * size;
                    rect.width = size;
                    rect.height = size;
                    rect.page = page;
                    frames.push_back(rect);
                }
            }

            sequence = ALTEngine::Formats::SpriteAnim::BuildSequence(
                ALTEngine::Formats::ExplosionGraphics::FRAME_DURATION_GUESS,
                [this] {
                    std::vector<uint16_t> list;
                    for (size_t i = 0; i < frames.size(); ++i) { list.push_back(static_cast<uint16_t>(i)); }
                    return list;
                }(),
                false);

            loaded = true;
            return true;
        }

        bool Ready() const { return loaded && !frames.empty(); }

        // Sets one off at a world position. `radius` is the half-size the sprite
        // is drawn at - a barrel's blast reaches its neighbouring cells, so it is
        // drawn about a cell across by default.
        void Spawn(float x, float y, float z, float halfSize = 320.0f)
        {
            if (!Ready()) { return; }
            Live live;
            live.x = x;
            live.y = y;
            live.z = z;
            live.halfSize = halfSize;
            ALTEngine::Formats::SpriteAnim::Start(live.animator, sequence);
            active.push_back(live);
        }

        // One of the original's logic ticks.
        void Tick()
        {
            for (Live& live : active)
            {
                ALTEngine::Formats::SpriteAnim::Tick(live.animator, sequence);
            }
            active.erase(std::remove_if(active.begin(), active.end(),
                                        [](const Live& live) { return live.animator.Ended(); }),
                         active.end());
        }

        // Appends this frame's sprites to the render list.
        void Collect(std::vector<ALTEngine::Renderer::PlacedSprite>& out) const
        {
            if (!Ready()) { return; }
            for (const Live& live : active)
            {
                size_t index = live.animator.frameIndex;
                if (index >= frames.size()) { index = frames.size() - 1; }
                const auto& rect = frames[index];

                ALTEngine::Renderer::PlacedSprite sprite;
                sprite.textureKey = SHEET_KEY;
                sprite.x = live.x;
                sprite.y = live.y;
                sprite.z = live.z;
                sprite.halfWidth = live.halfSize;
                sprite.halfHeight = live.halfSize;
                sprite.u0 = static_cast<float>(rect.x) / static_cast<float>(sheetWidth);
                sprite.v0 = static_cast<float>(rect.y) / static_cast<float>(sheetHeight);
                sprite.u1 = static_cast<float>(rect.x + rect.width) / static_cast<float>(sheetWidth);
                sprite.v1 = static_cast<float>(rect.y + rect.height) / static_cast<float>(sheetHeight);
                out.push_back(sprite);
            }
        }

        size_t LiveCount() const { return active.size(); }

    private:
        struct Live
        {
            float x = 0, y = 0, z = 0;
            float halfSize = 320.0f;
            ALTEngine::Formats::SpriteAnim::Animator animator;
        };

        bool loaded = false;
        int sheetWidth = 256;
        int sheetHeight = 256;
        std::vector<ALTEngine::Formats::BxRectangle> frames;
        std::vector<uint16_t> sequence;
        std::vector<Live> active;
    };
}
