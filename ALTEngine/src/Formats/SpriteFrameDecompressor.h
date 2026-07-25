#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace ALTEngine::Formats
{
    // Enemy/weapon sprites (cd\NME\*.B16, weapon view-model .B16 files)
    // use a genuinely different structure from the OPTOBJ/PICKMOD/level
    // TP-section textures we already handle: sections are named F000,
    // F001, ... (not TP00, TP01...), there's no separate CL/BX palette/UV
    // sections, and - the key difference - each F0## section's data is
    // LZSS-style COMPRESSED, and contains MULTIPLE concatenated
    // compressed sub-frames (an animation sequence), not one fixed-size
    // raw 256x256 image. Confirmed against a real file (EGGS.B16):
    // BndParser already finds the F000-F005 sections fine (its chunk
    // scanning is format-agnostic), but each section is far smaller than
    // the 65536-byte raw-texture size that would be expected uncompressed
    // (Edward, 2026).
    //
    // Algorithm ported directly from Edward's own TileRenderer.cs
    // (DecompressSingleFrame/DecompressAllFramesInSection) - an LZSS
    // variant: an 8-bit control mask (read one byte at a time, LSB
    // first) selects, per bit, either a literal byte copy or a
    // back-reference match. Matches come in two encodings: a single
    // byte >= 96 means a fixed length-3 match with a small offset; a
    // two-byte encoding otherwise carries a 4-bit size nibble (5 means
    // "use a following byte for an extended 9+ length", others mean
    // length = nibble+4) and a 12-bit offset. An offset of 0 in the
    // two-byte encoding terminates the current frame.
    class SpriteFrameDecompressor
    {
    public:
        // Decompresses a single frame starting at `input[startOffset]`.
        // Returns the decompressed pixel bytes and how many INPUT bytes
        // were consumed (so the caller can find where the next frame,
        // if any, starts).
        static std::pair<std::vector<uint8_t>, size_t> DecompressSingleFrame(const std::vector<uint8_t>& input, size_t startOffset);

        // Decompresses every frame concatenated within one F0## section's
        // data (an animation sequence) - repeatedly calls
        // DecompressSingleFrame, skipping zero-byte padding between
        // frames, matching DecompressAllFramesInSection's own heuristics
        // (stop if a decompressed frame comes back suspiciously small,
        // matching the "< 8 bytes = likely invalid" cutoff).
        static std::vector<std::vector<uint8_t>> DecompressAllFramesInSection(const std::vector<uint8_t>& data);
    };
}
