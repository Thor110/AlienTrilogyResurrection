#pragma once

#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // For LEGAL/LOGOSGFX/COLONY/PRISHOLD/BONESHIP: each of the two TP
    // frames is stored as a full 256x256 raw buffer, but only part of it
    // is real content - the rest is magenta padding. The two cropped
    // frames sit side by side to form the final image.
    //
    // Confirmed against the real LEGAL.BND + its BX metadata: frame A
    // contributes 240x240, frame B contributes 80x240 - final image is
    // 320x240 (native PS1 output resolution), NOT a symmetric 240+240.
    // Widths are per-frame parameters rather than hardcoded/shared for
    // exactly this reason - don't assume symmetry.
    class SplitFrameImage
    {
    public:
        // `frameA`/`frameB` are RGBA8888 buffers of size
        // rawFrameWidth x rawFrameHeight (e.g. 256x256, already rendered
        // via RawImageRenderer::RenderRGBA). Returns an RGBA8888 buffer
        // of size (visibleWidthA + visibleWidthB) x visibleHeight, with
        // frameA's top-left visibleWidthA x visibleHeight region on the
        // left and frameB's top-left visibleWidthB x visibleHeight region
        // immediately to its right.
        static std::vector<uint8_t> AssembleSideBySide(
            const std::vector<uint8_t>& frameA, const std::vector<uint8_t>& frameB,
            int rawFrameWidth, int rawFrameHeight,
            int visibleWidthA, int visibleWidthB, int visibleHeight);
    };
}
