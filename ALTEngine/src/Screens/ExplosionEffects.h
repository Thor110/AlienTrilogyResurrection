#pragma once

#include "../Formats/BndTextureLoader.h"
#include "../Formats/ExplosionGraphics.h"
#include "../Formats/SpriteAnimator.h"
#include "ParticleSystem.h"
#include "../Renderer/ModelRenderer.h"

#include <algorithm>
#include <cmath>
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

            // Both pages, as two sheets. The uv rect list is FLAT and global,
            // so each rectangle carries the page it belongs to and the sprite
            // key follows from that.
            for (size_t i = 0; i < set.textures.size() && i < 2; ++i)
            {
                const auto& texture = set.textures[i];
                if (!ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                        SheetKey(static_cast<int>(i)), texture.rgba, texture.width, texture.height))
                {
                    return false;
                }
                sheetWidth[i] = texture.width;
                sheetHeight[i] = texture.height;
            }

            rects = set.uvRects;
            if (rects.size() < 45) { return false; }

            loaded = true;
            return true;
        }

        bool Ready() const { return loaded && rects.size() >= 45; }

        // Sets one off. `table` picks which of the file's effects to play - see
        // ExplosionGraphics for the full list.
        void Spawn(const ALTEngine::Formats::ExplosionGraphics::EffectTable& table,
                   float x, float y, float z)
        {
            if (!Ready()) { return; }
            if (active.size() >= MAX_ACTIVE) { return; }

            Live live;
            live.table = &table;
            live.x = x;
            live.y = y;
            live.z = z;

            std::vector<uint16_t> frames;
            for (int i = 0; i < table.frameCount; ++i) { frames.push_back(static_cast<uint16_t>(i)); }
            live.sequence = ALTEngine::Formats::SpriteAnim::BuildSequence(
                ALTEngine::Formats::ExplosionGraphics::FRAME_DURATION, frames, false);
            ALTEngine::Formats::SpriteAnim::Start(live.animator, live.sequence);
            active.push_back(std::move(live));
        }

        // One of the original's logic ticks.
        void Tick()
        {
            for (Live& live : active) { ALTEngine::Formats::SpriteAnim::Tick(live.animator, live.sequence); }
            active.erase(std::remove_if(active.begin(), active.end(),
                                        [](const Live& live) { return live.animator.Ended(); }),
                         active.end());
        }

        void Collect(std::vector<ALTEngine::Renderer::PlacedSprite>& out,
                     float cameraX = 0.0f, float cameraZ = 0.0f, bool cull = false) const
        {
            if (!Ready()) { return; }
            namespace EG = ALTEngine::Formats::ExplosionGraphics;

            for (const Live& live : active)
            {
                if (cull)
                {
                    const float dx = live.x - cameraX;
                    const float dz = live.z - cameraZ;
                    const float distance = std::sqrt(dx * dx + dz * dz);
                    if (distance >= EG::EFFECT_FAR_CUTOFF || distance <= EG::EFFECT_NEAR_CUTOFF) { continue; }
                }

                int frame = static_cast<int>(live.animator.frameIndex);
                if (frame >= live.table->frameCount) { frame = live.table->frameCount - 1; }
                const size_t record = static_cast<size_t>(live.table->firstRecord + frame);
                if (record >= rects.size()) { continue; }
                const auto& rect = rects[record];

                const int page = (rect.page >= 0 && rect.page < 2) ? rect.page : 0;
                const float sheetW = static_cast<float>(sheetWidth[page]);
                const float sheetH = static_cast<float>(sheetHeight[page]);

                ALTEngine::Renderer::PlacedSprite sprite;
                sprite.textureKey = SheetKey(page);
                sprite.x = live.x;
                sprite.y = live.y;
                sprite.z = live.z;

                // The original multiplies the rect's size by the table's scale
                // and divides by distance. A world-space quad only needs the
                // first half - perspective does the rest.
                sprite.halfWidth = 0.5f * rect.width
                                 * (static_cast<float>(live.table->scaleX) / EG::SCALE_ONE)
                                 * EG::WORLD_UNITS_PER_SCALED_PIXEL;
                sprite.halfHeight = 0.5f * rect.height
                                  * (static_cast<float>(live.table->scaleY) / EG::SCALE_ONE)
                                  * EG::WORLD_UNITS_PER_SCALED_PIXEL;

                sprite.u0 = static_cast<float>(rect.x) / sheetW;
                sprite.v0 = static_cast<float>(rect.y) / sheetH;
                sprite.u1 = static_cast<float>(rect.x + rect.width) / sheetW;
                sprite.v1 = static_cast<float>(rect.y + rect.height) / sheetH;
                out.push_back(sprite);
            }
        }

        // A DESTROYED OBJECT SCATTERS SIX OF THESE, from FUN_00037dd0.
        //
        // It does not spawn one effect - it spawns SIX, at random offsets around
        // the object, and the scatter axis follows the object's facing:
        //
        //   facing 0 or 4  x + (random & 0x1ff) - 0x100,  y + random - 0x80
        //   facing 2 or 6  the same pattern on the other axis
        //   anything else  a single effect, no scatter
        //
        // Object type 0x1d gets ONE instead of six; everything else gets six.
        // The base position is lifted by 0x200 and by the object's own height
        // byte before any of that.
        //
        // Six flat sprites bursting outward is what reads as a crate breaking
        // into pieces. Whether the frames themselves are shard-shaped is a
        // question about the table at DAT_000ad008, which is one of page 0's
        // sequences and is not identified yet - this uses the barrel page until
        // it is, so the SCATTER is right and the artwork is not.
        static constexpr int SCATTER_COUNT = 6;
        static constexpr int SCATTER_SINGLE_TYPE = 0x1d;
        static constexpr int SCATTER_SPREAD = 0x100;   // half of the 0x1ff mask
        static constexpr int SCATTER_CROSS = 0x80;
        static constexpr int SCATTER_LIFT = 0x200;

        // `facing` is the object's rotation field: 0/4 scatter along X, 2/6
        // along Z, anything else gives a single effect.
        void SpawnScatter(float x, float y, float z, int facing, int objectType = 0)
        {
            if (!Ready()) { return; }

            const bool alongX = (facing == 0 || facing == 4);
            const bool alongZ = (facing == 2 || facing == 6);
            if (!alongX && !alongZ)
            {
                Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_CRATE, x, y + SCATTER_LIFT, z);
                return;
            }

            const int count = (objectType == SCATTER_SINGLE_TYPE) ? 1 : SCATTER_COUNT;
            for (int i = 0; i < count; ++i)
            {
                const float spread = static_cast<float>(rng.Bits(0x1ff) - SCATTER_SPREAD);
                const float cross = static_cast<float>(rng.Bits(0xff) - SCATTER_CROSS);
                namespace EG = ALTEngine::Formats::ExplosionGraphics;
                if (alongX) { Spawn(EG::EFFECT_CRATE, x + spread, y + SCATTER_LIFT + cross, z); }
                else        { Spawn(EG::EFFECT_CRATE, x, y + SCATTER_LIFT + cross, z + spread); }
            }
        }

        size_t LiveCount() const { return active.size(); }

    private:
        struct Live
        {
            float x = 0, y = 0, z = 0;
            const ALTEngine::Formats::ExplosionGraphics::EffectTable* table = nullptr;
            std::vector<uint16_t> sequence;
            ALTEngine::Formats::SpriteAnim::Animator animator;
        };

        static const char* SheetKey(int page) { return page == 1 ? "explgfx1" : "explgfx0"; }

        static constexpr size_t MAX_ACTIVE = 32;

        ALTEngine::Screens::Particles::Rng rng;
        bool loaded = false;
        int sheetWidth[2] = { 256, 256 };
        int sheetHeight[2] = { 256, 256 };
        std::vector<ALTEngine::Formats::BxRectangle> rects;
        std::vector<Live> active;
    };
}
