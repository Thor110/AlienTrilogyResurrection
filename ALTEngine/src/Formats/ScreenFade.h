#pragma once

#include <cstdint>

namespace ALTEngine::Formats
{
    // THE SCREEN FADES - FUN_0004eec0 (in), FUN_0004eefc (out), FUN_0004ef64
    // (one step a drawn frame) and FUN_0004f020, which every primitive passes
    // through as it is added to the draw list (FUN_00048f64, 240 callers: the
    // level, the models, sprites, particles, the weapon, the HUD and its text).
    //
    // It is not a black overlay. Each primitive's own colour - the light
    // colour its texels are multiplied by, 0x80 neutral - has an amount taken
    // off each channel, held at 0:
    //
    //   IN  (DAT_000ad846 = 1): the step counter runs 1..5, one a frame, and
    //       (5 - step) * 0x55 comes off: 340, 255, 170, 85, 0. A neutral 0x80
    //       face is black for three frames, then at a third, then whole.
    //   OUT (DAT_000ad846 = 2): the counter runs 25 down to 1, and
    //       (25 - step) * 11 comes off - or * 44 when any of the primitive's
    //       channels is over 0x7e. So a face lit at neutral or brighter is gone
    //       in three frames; a dim one takes up to eleven.
    //   After a fade out the mode is 0 but the last fade is remembered
    //   (DAT_004021e8 = 2), and everything stays black until a fade in.
    //
    // Pieces drawn without their primitive colour (the half-blended fills: the
    // translucent faces, flames and steam - span 0x5db67) are not darkened by
    // it, as they never read that colour.
    namespace ScreenFade
    {
        inline constexpr int IN_STEPS = 5;
        inline constexpr int IN_AMOUNT = 0x55;
        inline constexpr int OUT_STEPS = 0x19;
        inline constexpr int OUT_AMOUNT = 11;
        inline constexpr int OUT_AMOUNT_BRIGHT = 0x2c;
        inline constexpr int BRIGHT_ABOVE = 0x7e;
        // One step per drawn frame, at the original's design point of a frame
        // every two timer interrupts.
        inline constexpr double STEP_HZ = 1193182.0 / 17109.0 / 2.0;   // 34.87

        enum class Mode : uint8_t { None, In, Out };

        struct State
        {
            Mode mode = Mode::None;
            int step = 0;              // DAT_004021f0
            bool heldBlack = false;    // mode 0 after a fade out (DAT_004021e8 == 2)

            bool operator==(const State& o) const
            {
                return mode == o.mode && step == o.step && heldBlack == o.heldBlack;
            }
            bool operator!=(const State& o) const { return !(*this == o); }
        };

        // FUN_0004eec0.
        inline void StartIn(State& s) { s.mode = Mode::In; s.step = 1; s.heldBlack = false; }
        // FUN_0004eefc.
        inline void StartOut(State& s) { s.mode = Mode::Out; s.step = OUT_STEPS; }

        // FUN_0004ef64, once a drawn frame (after the frame's primitives).
        inline void Step(State& s)
        {
            if (s.mode == Mode::In)
            {
                if (++s.step == IN_STEPS + 1) { s.mode = Mode::None; }
            }
            else if (s.mode == Mode::Out)
            {
                if (--s.step == 0) { s.mode = Mode::None; s.heldBlack = true; }
            }
        }

        inline bool Busy(const State& s) { return s.mode != Mode::None; }

        // Nothing is taken off: the colours pass through as they are.
        inline bool Identity(const State& s)
        {
            if (s.heldBlack && s.mode == Mode::None) { return false; }
            if (s.mode == Mode::In) { return s.step >= IN_STEPS; }
            if (s.mode == Mode::Out) { return s.step >= OUT_STEPS; }
            return true;
        }

        // FUN_0004f020 on one primitive colour, 0-255 a channel.
        inline void Apply(const State& s, int& r, int& g, int& b)
        {
            int amount = 0;
            switch (s.mode)
            {
            case Mode::None:
                if (s.heldBlack) { r = g = b = 0; }
                return;
            case Mode::In:
                amount = (IN_STEPS - s.step) * IN_AMOUNT;
                break;
            case Mode::Out:
                amount = (OUT_STEPS - s.step)
                       * ((r > BRIGHT_ABOVE || g > BRIGHT_ABOVE || b > BRIGHT_ABOVE) ? OUT_AMOUNT_BRIGHT : OUT_AMOUNT);
                break;
            }
            r = r - amount < 0 ? 0 : r - amount;
            g = g - amount < 0 ? 0 : g - amount;
            b = b - amount < 0 ? 0 : b - amount;
        }

        // The same on a colour given as a multiplier, 1.0 = 0x80 (the port's
        // vertex and tint colours).
        inline void ApplyMultiplier(const State& s, float& r, float& g, float& b)
        {
            if (Identity(s)) { return; }
            auto toByte = [](float v) {
                const int c = static_cast<int>(v * 128.0f + 0.5f);
                return c < 0 ? 0 : (c > 255 ? 255 : c);
            };
            int ir = toByte(r), ig = toByte(g), ib = toByte(b);
            Apply(s, ir, ig, ib);
            r = static_cast<float>(ir) / 128.0f;
            g = static_cast<float>(ig) / 128.0f;
            b = static_cast<float>(ib) / 128.0f;
        }

        // What a neutral (0x80) primitive comes out as, 0..1 of itself: the
        // HUD's panels and text, which have no light of their own.
        inline float NeutralFactor(const State& s)
        {
            int r = 0x80, g = 0x80, b = 0x80;
            Apply(s, r, g, b);
            return static_cast<float>(r) / 128.0f;
        }
    }
}
