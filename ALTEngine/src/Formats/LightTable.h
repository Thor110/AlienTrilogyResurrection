#pragma once

#include "LevelLoader.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // The value at which a light colour means "leave the texel alone".
    //
    // *** THIS IS THE ONE REMAINING GUESS IN THE LIGHTING PATH. ***
    //
    // 255 says the light table only ever darkens; 128 says a record can
    // brighten a texel up to 2x. Measured over all 11264 quads of L111
    // (every one of which resolves to a real light record - there are no
    // out-of-range fallbacks):
    //
    //   /255 -> mean brightness 0.34, range 0.20-0.78. Nothing is ever
    //           brighter than its texture.
    //   /128 -> mean brightness 0.67, and only 256 quads (2%) come out
    //           brighter than their texture.
    //
    // So the honest position is that BOTH are plausible and this is not
    // settled. What made /255 the default:
    //   - FUN_0004ed58, which applies the global multiplier, clamps each
    //     channel at 0xff. Clamping at 255 is natural if 255 is the top
    //     of the scale.
    //   - the engine's distance-fog modulate (FUN_0004ecdc) is
    //     `colour * factor >> 8`, i.e. /256, with 0xff as its maximum
    //     factor. Same scale, and it is the one modulate in the dump that
    //     has actually been read.
    //
    // What argues for /128: a 0.34 mean is dark even for this game, and
    // 128 being the neutral point is the convention the PlayStation and
    // Saturn versions' hardware would have used.
    //
    // NOTE: an earlier version of this comment claimed /128 would put
    // roughly half the level's surfaces above 1.0. That was wrong - the
    // real figure is 2%, measured above - and the argument built on it
    // should not be reused.
    //
    // Trivially distinguishable on screen: if the level looks too dark
    // overall, change this to 128.0f and rebuild. Nothing else moves.
    constexpr float LIGHT_COLOUR_NEUTRAL = 128.0f;

    // Per-quad vertex colour, resolved from the level's light table.
    //
    // A quad's LIGHT ID byte (& 0x7f) indexes this table, and the
    // resulting RGB is the quad's shading. This is separate from texture
    // selection, which comes from texIndex - so a quad with no colour
    // applied renders at full brightness rather than the intended level.
    //
    // WHICH BYTE. The light id is the ON-DISK byte at quad +0x13 (the
    // one `ModelQuad::reserved` holds, annotated "light id x // Kaiser"
    // in Edward's own ModelRenderer.cs). An earlier version of this file
    // used +0x12 (`flags`) instead. That was wrong, and provably so
    // against L111LEV.MAP:
    //
    //   +0x12 takes only 6 distinct values in the whole level (0,1,2,3,
    //   4,8), so it can only ever reach 6 light records - all of them
    //   mode 0, i.e. static. L111 contains 18 mode-1 and 30 mode-3
    //   BLINK records. Under +0x12 no quad in the level can reach a
    //   single one of them, so nothing could ever blink. That alone
    //   rules it out, since blinking lights are observably a thing the
    //   original does.
    //
    //   +0x13 & 0x7f reaches 24 records: 8 static, 8 mode-1 and 8
    //   mode-3. The static ones form an obvious authored brightness
    //   ramp (160,144,128,96,80,64,50 grey).
    //
    // The runtime layout is the on-disk pair SWAPPED, which is what made
    // this confusing for six rounds. The original's face walker
    // (Ghidra: FUN_00025648, stride 0x14 = the level quad size) reads
    // its light index from RUNTIME +0x12 & 0x7f and its draw-routine
    // index from RUNTIME +0x13, terminating the loop when RUNTIME +0x12
    // has bit 0x80 set. On disk it is the other way round, confirmed
    // three ways in L111:
    //   - on-disk +0x12 never has bit 0x80 set on any of the 11264
    //     quads, so it cannot be the byte carrying the walker's
    //     terminator; on-disk +0x13 has it on 3885, and those partition
    //     the array into 3885 runs with ZERO quads left over past the
    //     last terminator.
    //   - the walker rejects a draw index >= 0x23. On-disk +0x12 maxes
    //     out at 8; on-disk +0x13 reaches 255. Only +0x12 can be the
    //     draw index.
    //   - the light-record evidence above.
    //
    // This also settles the open question in the corrections log without
    // disturbing anything: the 35-entry table at 0x000a7098 really is
    // the rasterizer dispatch (Rounds 1 and 2 were right), level quads
    // really do go through it (the recent suggestion was right), and
    // both can be true because the walker sees the bytes in the
    // opposite order from disk. Nothing about the door/lift flag
    // convention follows from it, so nothing there changes.
    //
    // The original runs this every tick from the per-level periodic
    // callback, applying a global multiplier and stepping the blink state
    // machines. Modes 2 and 5 are unimplemented: their handlers were
    // identified but not decompiled, and no level seen so far uses them.
    class LightTable
    {
    public:
        struct Rgb
        {
            uint8_t r = 255, g = 255, b = 255;
            bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
            bool operator!=(const Rgb& o) const { return !(*this == o); }
        };

        void Reset(const std::vector<LightRecord>& records)
        {
            lights = records;
            entries.assign(lights.size(), Entry{});
            Restamp();
        }

        // One tick. `randomBits` supplies the jitter the original takes
        // from its RNG - passing a fixed value gives deterministic blink
        // timing, which is useful when comparing against a recording.
        void Tick(int randomBits)
        {
            int jitter = (randomBits & 3) + 1;

            for (size_t i = 0; i < lights.size(); ++i)
            {
                LightRecord& light = lights[i];

                // Blink only runs once ToggleLight has driven the variant
                // counter to its maximum - that is how a switch or a door
                // brings a set of wall lights to life.
                bool armed = (light.variant >= light.variantMax);

                if ((light.mode == 1 || light.mode == 3) && armed)
                {
                    if (light.on == 0)
                    {
                        if (light.blinkCountdown == 0)
                        {
                            light.on = 1;
                            light.blinkCountdown = static_cast<uint16_t>(jitter + light.onDuration);
                        }
                        else { light.blinkCountdown--; }
                    }
                    else
                    {
                        if (light.blinkCountdown == 0)
                        {
                            if (light.blinkRepeats != 0)
                            {
                                light.on = 0;
                                light.blinkRepeats--;
                                light.blinkCountdown = static_cast<uint16_t>(jitter + light.offDuration);
                            }
                        }
                        else { light.blinkCountdown--; }
                    }
                }
            }

            Restamp();
        }

        // Advances a light's ToggleLight variant counter, clamped to its
        // maximum. Script command 0 does exactly this.
        void ToggleLight(size_t index, int delta)
        {
            if (index >= lights.size()) { return; }
            int value = static_cast<int>(lights[index].variant) + delta;
            if (value < 0) { value = 0; }
            if (value > lights[index].variantMax) { value = lights[index].variantMax; }
            lights[index].variant = static_cast<uint8_t>(value);
        }

        // Raises the one-shot brightness flash the original uses for
        // weapon fire and explosions. Cleared automatically on the next
        // tick, matching the updater.
        void RequestFlash() { flash = true; }

        // One resolved light-table entry: four corner colours, mirroring
        // the 16-byte entries the original builds at DAT_002458dc
        // (Ghidra: FUN_00029be0). Only mode 4 (gouraud) uses all four;
        // every other mode fills all four with the same triple, which is
        // how a flat-shaded face falls out of the same structure.
        struct Entry
        {
            Rgb corner[4];
            bool operator==(const Entry& o) const
            {
                return corner[0] == o.corner[0] && corner[1] == o.corner[1]
                    && corner[2] == o.corner[2] && corner[3] == o.corner[3];
            }
            bool operator!=(const Entry& o) const { return !(*this == o); }
        };

        // `lightIdByte` is the quad's ON-DISK +0x13 byte, i.e.
        // ModelQuad::reserved - NOT `flags`. See the note at the top of
        // this file; getting this wrong is what stopped every blink
        // record in the level from ever being reached.
        //
        // Bit 0x80 of that byte is the original's end-of-run marker for
        // its face lists, not part of the index, so it is masked off.
        const Entry& ColourFor(int lightIdByte) const
        {
            static const Entry fallback{};
            size_t index = static_cast<size_t>(lightIdByte & 0x7f);
            if (index >= entries.size()) { return fallback; }
            return entries[index];
        }

        const std::vector<Entry>& Entries() const { return entries; }

        const std::vector<LightRecord>& Records() const { return lights; }

    private:
        static uint8_t Scale(uint8_t channel, int global)
        {
            int value = (static_cast<int>(channel) * global) / LIGHT_GLOBAL_DIVISOR;
            return static_cast<uint8_t>(value > 255 ? 255 : value);
        }

        Rgb ScaleTriple(const uint8_t* rgb, int global) const
        {
            return Rgb{ Scale(rgb[0], global), Scale(rgb[1], global), Scale(rgb[2], global) };
        }

        // Rebuilds the resolved entry table. This is FUN_00029be0's
        // second half, which the original runs when a level's lights are
        // installed and the per-tick updater re-runs as blink states
        // change. The corner ordering below is that function's, including
        // its swap: for mode 4 the FIRST corner takes the record's `lit`
        // triple (+0x04) and the SECOND takes `unlit` (+0x00), not the
        // other way round.
        //
        // The global multiplier is folded in here rather than at draw
        // time. The original applies it per-face in FUN_0004ed58 as
        // (channel * global) / 0xc00 clamped to 255; since it is the same
        // multiplier for every face on a given tick, doing it once per
        // record per tick is equivalent and much cheaper.
        void Restamp()
        {
            int global = flash ? LIGHT_GLOBAL_FLASH : LIGHT_GLOBAL_NORMAL;
            flash = false;

            for (size_t i = 0; i < lights.size(); ++i)
            {
                const LightRecord& light = lights[i];
                Entry& out = entries[i];

                if (light.mode == 4)
                {
                    out.corner[0] = ScaleTriple(light.lit, global);
                    out.corner[1] = ScaleTriple(light.unlit, global);
                    out.corner[2] = ScaleTriple(light.corner2, global);
                    out.corner[3] = ScaleTriple(light.corner3, global);
                }
                else
                {
                    // Modes 1 and 3 pick their triple by the runtime
                    // on/off state stepped in Tick; mode 0 is static and
                    // its `on` byte is 1 on disk for every L111 record,
                    // so it resolves to `lit` - which is where the
                    // authored brightness actually lives (every mode-0
                    // record in L111 has unlit = 0,0,0).
                    const uint8_t* source = light.on ? light.lit : light.unlit;
                    Rgb flat = ScaleTriple(source, global);
                    out.corner[0] = flat;
                    out.corner[1] = flat;
                    out.corner[2] = flat;
                    out.corner[3] = flat;
                }
            }
        }

        std::vector<LightRecord> lights;
        std::vector<Entry> entries;
        bool flash = false;
    };
}
