#pragma once

#include "../Formats/EnemyAnimData.h"
#include "../Formats/EnemySpriteSet.h"
#include "../Formats/SpriteAnimator.h"
#include "../Renderer/HudPanel.h"

#include <filesystem>
#include <vector>

namespace ALTEngine::Screens
{
    // The face hugger crawling onto the camera - FINGERS.B16.
    //
    // TWO HALVES OF ONE MIRRORED SPRITE, which is why FUN_000303ec sets up two
    // independent animators at the same screen position. Decoding both confirms
    // it: the frame records' X offsets are
    //
    //     overlay A   -57, -49, -52, -61      all negative - the LEFT half
    //     overlay B    +1,  +1,  +1,  +1      all +1       - the RIGHT half
    //
    // so they butt together at the anchor rather than overlapping, and their
    // widths run 60, 52, 56, 64 against 60, 64, 56, 52 - the same set reversed,
    // which is what a mirrored pair looks like. Four frames each, 88 to 92 pixels
    // tall.
    //
    // WHERE THEY COME FROM: animations 16 and 17 of the HUGGER's own table, read
    // through the pairs at +0x80/+0x84 and +0x88/+0x8c. Their frames are not in
    // HUGGER.B16 - they are the two sections of FINGERS.B16, one per half.
    //
    // THE ANCHOR is 0xa0, 0x154 from FUN_000303ec, and BOTH are real.
    //
    // 0xa0 is 160, the centre of a 320-wide screen - exactly where the two halves
    // meet, which confirms the reading.
    //
    // 0x154 is 340, past the bottom of a 240-row screen, and that is the point:
    // it is where the crawl STARTS. The creature comes up from below the view, as
    // if climbing your legs, and slides up onto the camera (Edward, 2026). I had
    // written that number off as "not a plain Y" because it was off-screen; being
    // off-screen was the whole meaning of it.
    class FaceHugOverlay
    {
    public:
        static constexpr int HALF_COUNT = 2;

        // `huggerTable` is the creature's animation table base; the overlay pairs
        // sit at +0x80 and +0x88 within it.
        // Loads straight from the baked animation data - the caller does not need
        // the executable's tables.
        bool Load(const std::filesystem::path& cdDirectory)
        {
            namespace AD = ALTEngine::Formats::EnemyAnimData;
            const AD::Creature* hugger = AD::ForType(2);
            if (!hugger) { return false; }

            auto toFrames = [](const AD::Anim& anim) {
                std::vector<ALTEngine::Formats::EnemySpriteSet::Frame> out;
                for (int i = 0; i < anim.count; ++i)
                {
                    ALTEngine::Formats::EnemySpriteSet::Frame f;
                    f.offsetX = anim.frames[i].ox;
                    f.offsetY = anim.frames[i].oy;
                    f.width = anim.frames[i].w;
                    f.height = anim.frames[i].h;
                    f.compressedOffset = anim.frames[i].off;
                    out.push_back(f);
                }
                return out;
            };

            return Load(cdDirectory,
                        toFrames(hugger->anims[AD::OVERLAY_HALF_A]),
                        toFrames(hugger->anims[AD::OVERLAY_HALF_B]));
        }

        bool Load(const std::filesystem::path& cdDirectory,
                  const std::vector<ALTEngine::Formats::EnemySpriteSet::Frame>& halfA,
                  const std::vector<ALTEngine::Formats::EnemySpriteSet::Frame>& halfB)
        {
            std::filesystem::path path = cdDirectory / "NME" / "FINGERS.B16";
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) { return false; }
            if (!sprites.Load(path)) { return false; }

            halves[0] = sprites.DecodeAnimation(halfA);
            halves[1] = sprites.DecodeAnimation(halfB);
            loaded = !halves[0].empty() && !halves[1].empty();
            return loaded;
        }

        bool Ready() const { return loaded; }
        bool Active() const { return active; }

        // True once the climb has finished - the point at which the creature
        // itself is spent. The overlay outlasts it.
        bool Arrived() const { return y <= REST_Y; }

        // True once it has slid back off - the point LAB_00030918 waits for
        // before landing the creature and killing it.
        bool Finished() const { return phase == Phase::Done; }

        // Starts the crawl. The pounce owns the timing, so this only needs telling
        // when it begins and ends.
        void Begin()
        {
            if (!loaded) { return; }
            active = true;
            frame = 0;
            timer = 0;
            hold = 0;
            y = START_Y;
            phase = Phase::Climbing;
        }

        void End() { active = false; }

        // One logic tick. Advances both halves together - they are one sprite cut
        // in two, so they must never drift apart.
        void Tick()
        {
            if (!active) { return; }

            // The three phases, as FUN_000307c0 runs them.
            if (phase == Phase::Climbing)
            {
                if (y > REST_Y) { y -= CLIMB_STEP; }
                else if (++hold >= HOLD_TICKS) { phase = Phase::Falling; }
            }
            else if (phase == Phase::Falling)
            {
                if (y < START_Y) { y += FALL_STEP; }
                else { phase = Phase::Done; active = false; }
            }

            if (++timer < TICKS_PER_FRAME) { return; }
            timer = 0;
            const int count = static_cast<int>(halves[0].size());
            if (count <= 0) { return; }
            if (frame + 1 < count) { frame++; }   // holds on the last frame
        }

        // The two halves to draw this tick, with their own draw offsets applied to
        // the anchor. Empty when inactive.
        struct Piece
        {
            const ALTEngine::Formats::EnemySpriteSet::Frame* frame = nullptr;
            int x = 0;
            int y = 0;
            int sheetIndex = 0;   // into AllFrames()
            float width = 0;      // already scaled
            float height = 0;
        };

        std::vector<Piece> Visible() const
        {
            std::vector<Piece> out;
            if (!active || !loaded) { return out; }

            for (int half = 0; half < HALF_COUNT; ++half)
            {
                if (halves[half].empty()) { continue; }
                size_t index = static_cast<size_t>(frame);
                if (index >= halves[half].size()) { index = halves[half].size() - 1; }

                const auto& f = halves[half][index];
                Piece piece;
                piece.frame = &f;
                piece.sheetIndex = (half == 0) ? static_cast<int>(index)
                                               : static_cast<int>(halves[0].size() + index);
                // The offsets scale with the art, so the halves keep meeting at
                // the anchor instead of pulling apart.
                piece.x = ANCHOR_X + static_cast<int>(f.offsetX * DRAW_SCALE);
                piece.y = CurrentY() + static_cast<int>(f.offsetY * DRAW_SCALE);
                piece.width = f.width * DRAW_SCALE;
                piece.height = f.height * DRAW_SCALE;
                out.push_back(piece);
            }
            return out;
        }

        // Every decoded frame of both halves, in upload order: half A's frames
        // then half B's. Visible() reports indices into this.
        std::vector<const ALTEngine::Formats::EnemySpriteSet::Frame*> AllFrames() const
        {
            std::vector<const ALTEngine::Formats::EnemySpriteSet::Frame*> out;
            for (int half = 0; half < HALF_COUNT; ++half)
            {
                for (const auto& f : halves[half]) { out.push_back(&f); }
            }
            return out;
        }

        const std::vector<uint8_t>& Palette() const { return sprites.Palette(); }

        // A piece's pixels as RGBA, using the file's own C000 palette. The palette
        // is 16-bit entries, the same 5551 layout the rest of the game's art uses,
        // and index 0 is transparent as it is everywhere else in these files.
        std::vector<uint8_t> ToRgba(const ALTEngine::Formats::EnemySpriteSet::Frame& f) const
        {
            std::vector<uint8_t> rgba(static_cast<size_t>(f.width) * f.height * 4, 0);
            const auto& pal = sprites.Palette();
            for (size_t i = 0; i < f.pixels.size() && i * 4 + 3 < rgba.size(); ++i)
            {
                const uint8_t index = f.pixels[i];
                if (index == 0) { continue; }   // transparent
                const size_t p = static_cast<size_t>(index) * 2;
                if (p + 1 >= pal.size()) { continue; }
                const uint16_t entry = static_cast<uint16_t>(pal[p] | (pal[p + 1] << 8));
                rgba[i * 4 + 0] = static_cast<uint8_t>(((entry) & 0x1f) << 3);
                rgba[i * 4 + 1] = static_cast<uint8_t>(((entry >> 5) & 0x1f) << 3);
                rgba[i * 4 + 2] = static_cast<uint8_t>(((entry >> 10) & 0x1f) << 3);
                rgba[i * 4 + 3] = 255;
            }
            return rgba;
        }

        int CurrentY() const { return y; }

    private:
        // 0xa0 from FUN_000303ec - the centre of a 320-wide screen, and the point
        // the two halves meet at.
        static constexpr int ANCHOR_X = 0xa0;

        // ALL FOUR NUMBERS ARE TRACED NOW, from FUN_000307c0 - the crawl's own
        // per-tick handler. It replaces the climb and hold I had invented:
        //
        //     if (state == 0)                 climbing
        //         if (y > 0x78)  y -= 8       up eight rows a tick
        //     else if (y < 0x154) y += 0x10   falling off, sixteen a tick
        //     else                state = 2   done - and 2 is exactly the value
        //                                     LAB_00030918 waits for before it
        //                                     lands the creature and kills it
        //
        // So the creature's death is not on a timer at all: it happens when the
        // overlay has finished sliding back off the bottom. One clock, and it is
        // the overlay's.
        //
        // It falls off TWICE AS FAST as it climbs on, which is what "squiggles
        // about your face a bit and then falls off" looks like (Edward, 2026).
        // DRAWN AT DOUBLE SIZE - see the note above; the scale is not read from
        // anywhere, only the position is.
        static constexpr float DRAW_SCALE = 2.0f;

        static constexpr int START_Y = 0x154;   // 340, below the screen
        static constexpr int REST_Y = 0x78;     // 120 - my 190 was a guess
        static constexpr int CLIMB_STEP = 8;
        static constexpr int FALL_STEP = 0x10;

        // WHAT ENDS THE CLIMB is the one part still open. The handler tests a
        // state word it does not itself set, so something else decides the crawl
        // is over. Holding for the length of the animation is the stand-in.
        static constexpr int HOLD_TICKS = 90;

        // Paced so the four frames play across the climb.
        static constexpr int TICKS_PER_FRAME = 7;        ALTEngine::Formats::EnemySpriteSet sprites;
        std::vector<ALTEngine::Formats::EnemySpriteSet::Frame> halves[HALF_COUNT];
        bool loaded = false;
        bool active = false;
        int frame = 0;
        int timer = 0;
        enum class Phase { Climbing, Falling, Done };
        Phase phase = Phase::Climbing;
        int y = START_Y;
        int hold = 0;
    };
}
