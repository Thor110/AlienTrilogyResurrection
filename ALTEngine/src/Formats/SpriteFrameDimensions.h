#pragma once

#include <optional>
#include <string>
#include <utility>

namespace ALTEngine::Formats
{
    // Enemy/weapon sprite frame dimensions - NOT stored in the .B16 file
    // itself (unlike everything else we've parsed so far), so this has
    // to be a hardcoded lookup table, transcribed from Edward's
    // DetectDimensions.cs. Edward, 2026: "DetectDimensions is definitely
    // correct as I manually verified it with every single frame across
    // every weapon and enemy sprite" - and independently, this
    // transcription's EGGS entries were cross-checked against real
    // decompressed frame byte counts (SpriteFrameDecompressor) with an
    // exact match across all 33 frames in that file.
    //
    // Extracted programmatically from the real DetectDimensions.cs
    // source (not hand-transcribed) to avoid manual-copying errors
    // across ~1600 lines and 809 entries - every entry validated to have
    // a non-null, non-zero width and height.
    //
    // `section` matches the .B16 file's F0## section number (0-based).
    // `frame` is the 0-based index within that section's decompressed
    // frame sequence. Some sections use the same dimensions for every
    // frame (the source has no per-frame switch for those) - those are
    // stored once with frame=-1 and LookupSpriteFrameDimensions falls
    // back to that if no frame-specific entry exists.
    std::optional<std::pair<int, int>> LookupSpriteFrameDimensions(const std::string& spriteName, int section, int frame);
}
