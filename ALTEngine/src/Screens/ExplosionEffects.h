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
                // PALETTE-INDEX TRANSPARENCY, and it is NOT one index for the
                // whole file.
                //
                // Checked per effect group by sampling the border row of every
                // frame in it:
                //     page 0, records  0..8   index 0
                //     page 0, records  9..17  index 0    (the barrel)
                //     page 0, records 18..23  index 0
                //     page 0, records 25..31  index 16   (the bullet impact)
                //     page 1, records 36..44  index 0    (the crate)
                //
                // So the impact frames sit on 16 while everything else sits on 0.
                // Passing only 0 left the impact puff as an opaque block while
                // the barrel looked right, which is exactly what was seen
                // (Edward, 2026).
                //
                // Both indices are given for page 0. That is safe: 0 and 16 are
                // each a background somewhere on that page, and neither is used
                // as a drawn colour by a group that relies on the other. Page 1
                // gets 0 alone, since 16 is a real colour there.
                const std::vector<std::vector<int>> transparentIndices{ { 0, 16 }, { 0 } };
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

                // A grey copy for the steam jets: the original fills steam
                // through its own blend at a grey level (FUN_0002caa8, 0x4b400)
                // rather than in the frames' fire colours, which drawn as they
                // are made the steam read as flame (Edward, 2026). Each texel
                // keeps its brightness and transparency.
                std::vector<uint8_t> grey = texture.rgba;
                for (size_t p = 0; p + 3 < grey.size(); p += 4)
                {
                    const int luma = (grey[p] * 77 + grey[p + 1] * 150 + grey[p + 2] * 29) >> 8;
                    grey[p] = grey[p + 1] = grey[p + 2] = static_cast<uint8_t>(luma);
                }
                ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                    GreySheetKey(static_cast<int>(i)), grey, texture.width, texture.height);
            }

            rects = set.uvRects;
            if (rects.size() < 45) { return false; }

            loaded = true;
            return true;
        }

        bool Ready() const { return loaded && rects.size() >= 45; }

        // Points a sprite at one of the file's 45 rectangles (the UV record
        // table at DAT_002405a0 that every effect indexes).
        bool UseRecord(int record, ALTEngine::Renderer::PlacedSprite& sprite, bool grey = false) const
        {
            if (!Ready() || record < 0 || static_cast<size_t>(record) >= rects.size()) { return false; }
            const auto& rect = rects[static_cast<size_t>(record)];
            const int page = (rect.page >= 0 && rect.page < 2) ? rect.page : 0;
            const float sheetW = static_cast<float>(sheetWidth[page]);
            const float sheetH = static_cast<float>(sheetHeight[page]);
            sprite.textureKey = grey ? GreySheetKey(page) : SheetKey(page);
            sprite.u0 = static_cast<float>(rect.x) / sheetW;
            sprite.v0 = static_cast<float>(rect.y) / sheetH;
            sprite.u1 = static_cast<float>(rect.x + rect.width) / sheetW;
            sprite.v1 = static_cast<float>(rect.y + rect.height) / sheetH;
            return true;
        }

        // Sets one off. `table` picks which of the file's effects to play - see
        // ExplosionGraphics for the full list.
        // `flipVertically` mirrors the frames upside down. A puff on the
        // CEILING is the same artwork hanging the other way up - the original's
        // effect frames are drawn with a fixed orientation, so without this a
        // ceiling hit shows smoke rising into the concrete.
        void Spawn(const ALTEngine::Formats::ExplosionGraphics::EffectTable& table,
                   float x, float y, float z, bool flipVertically = false)
        {
            if (!Ready()) { return; }
            if (active.size() >= MAX_ACTIVE) { return; }

            Live live;
            live.table = &table;
            live.x = x;
            live.y = y;
            live.z = z;
            live.flipVertically = flipVertically;

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

        // `cameraY` matters: the cull has to be a real 3D distance. Measuring
        // only on the ground plane means an effect at the player's feet or
        // directly overhead is nearly zero away and gets culled by the NEAR cut -
        // which is exactly why bullet impacts showed on walls but never on floors
        // or ceilings (Edward, 2026). The original culls on the PROJECTED
        // distance, which includes depth.
        // THE COLOUR (FUN_000429f0, given `light0`, light 0's first corner as
        // the light table holds it this tick): within 0x1000 the effect is
        // drawn in (0x80, light 0's green, light 0's blue) - the table's first
        // word with its first byte set to 0x80 - so a little cool at normal
        // light and more so in a flash. Past 0x1000 the original fades a colour
        // by distance through FUN_0004ecdc, at d * 4 - 0x1200: dimmer from
        // about 0x11c8 out to the 0x1e00 cut. The colour it fades is the
        // effect task's +0xa8, which nothing sets (task blocks are reused
        // without being cleared), so the port fades the near colour instead.
        // `fade` false (Render Distance on) leaves it unfaded, as the level is.
        void Collect(std::vector<ALTEngine::Renderer::PlacedSprite>& out,
                     float cameraX = 0.0f, float cameraY = 0.0f, float cameraZ = 0.0f,
                     bool cull = false, const uint8_t* light0 = nullptr, bool fade = false) const
        {
            if (!Ready()) { return; }
            namespace EG = ALTEngine::Formats::ExplosionGraphics;

            for (const Live& live : active)
            {
                if (cull)
                {
                    const float dx = live.x - cameraX;
                    const float dy = live.y - cameraY;
                    const float dz = live.z - cameraZ;
                    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
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

                // The frame's anchor sits on the effect's position, at the
                // original's size (ExplosionGraphics::QuadFor). The quad faces
                // the camera about the vertical, so "right" is across the line
                // from the camera to the effect.
                const EG::FrameQuad quad = EG::QuadFor(*live.table, frame, rect.width, rect.height);
                float toX = live.x - cameraX;
                float toZ = live.z - cameraZ;
                const float flat = std::sqrt(toX * toX + toZ * toZ);
                if (flat > 0.001f) { toX /= flat; toZ /= flat; } else { toX = 0.0f; toZ = -1.0f; }
                const float rightX = -toZ;
                const float rightZ = toX;
                const float up = live.flipVertically ? -quad.centreUp : quad.centreUp;
                sprite.x = live.x + rightX * quad.centreRight;
                sprite.y = live.y + up;
                sprite.z = live.z + rightZ * quad.centreRight;
                sprite.halfWidth = quad.halfWidth;
                sprite.halfHeight = quad.halfHeight;

                sprite.u0 = static_cast<float>(rect.x) / sheetW;
                sprite.v0 = static_cast<float>(rect.y) / sheetH;
                sprite.u1 = static_cast<float>(rect.x + rect.width) / sheetW;
                sprite.v1 = static_cast<float>(rect.y + rect.height) / sheetH;

                // Upside down for a ceiling hit - swap the V bounds.
                if (live.flipVertically) { std::swap(sprite.v0, sprite.v1); }

                if (light0)
                {
                    const float ex = live.x - cameraX, ey = live.y - cameraY, ez = live.z - cameraZ;
                    const float k = fade ? EG::EffectFade(std::sqrt(ex * ex + ey * ey + ez * ez)) : 1.0f;
                    sprite.r = 0x80 * k / 128.0f;
                    sprite.g = static_cast<float>(light0[1]) * k / 128.0f;
                    sprite.b = static_cast<float>(light0[2]) * k / 128.0f;
                    sprite.ownFade = true;
                }

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
            bool flipVertically = false;
            std::vector<uint16_t> sequence;
            ALTEngine::Formats::SpriteAnim::Animator animator;
        };

        static const char* SheetKey(int page) { return page == 1 ? "explgfx1" : "explgfx0"; }
        static const char* GreySheetKey(int page) { return page == 1 ? "explgfx1:grey" : "explgfx0:grey"; }

        static constexpr size_t MAX_ACTIVE = 32;

        ALTEngine::Screens::Particles::Rng rng;
        bool loaded = false;
        int sheetWidth[2] = { 256, 256 };
        int sheetHeight[2] = { 256, 256 };
        std::vector<ALTEngine::Formats::BxRectangle> rects;
        std::vector<Live> active;
    };
}
