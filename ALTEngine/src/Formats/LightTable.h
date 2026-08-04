#pragma once

#include "LevelLoader.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // Per-quad vertex colour, resolved from the level's light table.
    //
    // A quad's flags byte (& 0x7f) indexes this table, and the resulting
    // RGB is the quad's shading. This is separate from texture selection,
    // which comes from texIndex - so a quad with no colour applied
    // renders at full brightness rather than the intended level.
    //
    // The original runs this every tick from the per-level periodic
    // callback, applying a global multiplier and stepping the blink state
    // machines. Modes 2 and 5 are unimplemented: their handlers were
    // identified but not decompiled, and no level seen so far uses them.
    class LightTable
    {
    public:
        struct Rgb { uint8_t r = 255, g = 255, b = 255; };

        void Reset(const std::vector<LightRecord>& records)
        {
            lights = records;
            colours.assign(lights.size(), Rgb{});
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

        Rgb ColourFor(int flagByte) const
        {
            size_t index = static_cast<size_t>(flagByte & 0x7f);
            if (index >= colours.size()) { return Rgb{}; }
            return colours[index];
        }

        const std::vector<LightRecord>& Records() const { return lights; }

    private:
        static uint8_t Scale(uint8_t channel, int global)
        {
            int value = (static_cast<int>(channel) * global) / LIGHT_GLOBAL_DIVISOR;
            return static_cast<uint8_t>(value > 255 ? 255 : value);
        }

        void Restamp()
        {
            int global = flash ? LIGHT_GLOBAL_FLASH : LIGHT_GLOBAL_NORMAL;
            flash = false;

            for (size_t i = 0; i < lights.size(); ++i)
            {
                const LightRecord& light = lights[i];
                const uint8_t* source = light.on ? light.lit : light.unlit;

                // Mode 0 is flat and static; every L111 record uses it.
                // Modes 1 and 3 pick their triple by the runtime on/off
                // state stepped above. Mode 4 is gouraud - it writes a
                // different RGB per vertex, which this flat table cannot
                // express, so its first corner is used until the renderer
                // carries per-vertex colour.
                colours[i] = Rgb{ Scale(source[0], global), Scale(source[1], global), Scale(source[2], global) };
            }
        }

        std::vector<LightRecord> lights;
        std::vector<Rgb> colours;
        bool flash = false;
    };
}
