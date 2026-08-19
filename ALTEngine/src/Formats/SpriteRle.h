#pragma once

#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // THE SPRITE RLE, from FUN_00028350. This is the last link in the enemy
    // frame chain, and it settles what a frame record's third dword is.
    //
    // The decoder takes a raw byte pointer and writes pixels until it hits a
    // terminator. So frameRecord[+8] is not a key and not an index - it is a
    // POINTER TO COMPRESSED PIXEL DATA. The values climb per frame (0, 1884,
    // 3752, 5728, 7732 on the first creature's first animation) because they are
    // offsets into one packed stream, which is what made them look like a data
    // offset in the first place. That reading was right; the "opaque key" one was
    // wrong, and the truth is that the same number serves as both the source
    // address and the sprite cache's tag.
    //
    // THE FORMAT, one control byte at a time:
    //
    //     value = b >> 2          the pixel, so SIX BITS - a 64-entry palette
    //     b & 3 == 0              the next byte is a RUN LENGTH of `value`
    //                             pixels; a run length of ZERO ends the sprite
    //     b & 2                   emit two pixels of `value`
    //     b & 1                   emit one more
    //
    // So the low two bits carry a literal count of one to three, and a count of
    // zero escapes to a byte-length run. Short runs cost one byte and long ones
    // cost two, with no separate literal mode at all - every pixel in a sprite
    // comes from a repeat. That works because these are low-colour sprites with
    // large flat areas.
    //
    // The decoder returns the number of pixels written, which is how the caller
    // knows the frame's size without the record having to agree.
    // HOW THIS SITS WITH THE PORT'S EXISTING DECODER - and this is the part I
    // cannot settle without a real file.
    //
    // The port already has SpriteFrameDecompressor, a genuine LZSS variant that
    // was validated against EGGS.B16 and MM9.B16 and produces frames whose byte
    // counts match the dimension table. That is not in doubt.
    //
    // But FUN_000285b4 hands FUN_00028350 the frame record's own pointer and
    // FUN_00028350 is unambiguously this RLE. So there are two plausible
    // arrangements:
    //
    //   TWO LAYERS - the F0## section is LZSS-compressed, and once expanded it
    //     holds a packed stream of RLE sprites which the frame records index into
    //     by byte offset. The record offsets climbing 0, 1884, 3752, 5728, 7732
    //     fit a packed stream well.
    //
    //   TWO PATHS - the F0## sections are LZSS and used for something else
    //     (weapons, the pickup models), while enemy animation frames live
    //     elsewhere in the file as raw RLE.
    //
    // The first is more likely, because the frame record's offsets are far too
    // small to be raw pixels and the LZSS output is the only expanded buffer
    // those offsets could be relative to.
    //
    // DECIDING IT NEEDS ONE NME FILE. With HUGGER.B16 in hand the test is
    // immediate: expand its F0 sections with the existing decompressor, then try
    // this RLE at the offsets its animation table's frame records name. If the
    // pixel counts come out as width*height for those records, the two-layer
    // reading is right and the loader can be pointed at it. Nothing is being
    // ripped out until then - the working path stays working.
    namespace SpriteRle
    {
        // Pixels are six bits, so the palette this indexes has 64 entries rather
        // than the 256 a full byte would give.
        inline constexpr int PIXEL_BITS = 6;
        inline constexpr int PALETTE_ENTRIES = 1 << PIXEL_BITS;

        // Decodes one sprite starting at `data[offset]`. Appends to `out` and
        // returns the number of pixels written, or 0 if the data runs out.
        //
        // `limit` caps the output so a corrupt stream cannot run away - the
        // original trusts its data and has no such guard.
        inline size_t Decode(const std::vector<uint8_t>& data, size_t offset,
                             std::vector<uint8_t>& out, size_t limit = 1u << 20)
        {
            const size_t start = out.size();

            while (offset < data.size() && out.size() - start < limit)
            {
                const uint8_t control = data[offset++];
                const uint8_t value = static_cast<uint8_t>(control >> 2);

                if ((control & 3) == 0)
                {
                    // Escape: the next byte is the run length, and zero ends the
                    // sprite.
                    if (offset >= data.size()) { break; }
                    const uint8_t run = data[offset++];
                    if (run == 0) { break; }
                    for (uint8_t i = 0; i < run; ++i) { out.push_back(value); }
                    continue;
                }

                // A literal count of one to three, two bits at a time.
                if ((control & 2) != 0) { out.push_back(value); out.push_back(value); }
                if ((control & 1) != 0) { out.push_back(value); }
            }

            return out.size() - start;
        }

        // Decodes the sprite a frame record points at, given the expanded section
        // bytes. `expectedPixels` is width * height from the record; a mismatch
        // means the offset is not addressing what we think it is, which is exactly
        // the check that settles the layering question above.
        struct DecodedSprite
        {
            std::vector<uint8_t> pixels;
            size_t decoded = 0;
            bool matchesExpected = false;
        };

        inline DecodedSprite DecodeFrame(const std::vector<uint8_t>& sectionBytes,
                                         size_t recordOffset, int width, int height)
        {
            DecodedSprite result;
            if (recordOffset >= sectionBytes.size()) { return result; }

            const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
            result.decoded = Decode(sectionBytes, recordOffset, result.pixels,
                                    expected > 0 ? expected * 2 : (1u << 20));
            result.matchesExpected = (expected > 0 && result.decoded == expected);
            return result;
        }

        // Walks a stream and reports where each sprite starts, by decoding them
        // in sequence. Useful for checking a frame record's offsets against the
        // file rather than trusting them.
        inline std::vector<size_t> SpriteOffsets(const std::vector<uint8_t>& data,
                                                 size_t startOffset, int maxSprites)
        {
            std::vector<size_t> offsets;
            size_t cursor = startOffset;

            for (int i = 0; i < maxSprites && cursor < data.size(); ++i)
            {
                offsets.push_back(cursor);

                // Re-walk the control stream to find where this sprite ends,
                // without keeping the pixels.
                while (cursor < data.size())
                {
                    const uint8_t control = data[cursor++];
                    if ((control & 3) == 0)
                    {
                        if (cursor >= data.size()) { break; }
                        const uint8_t run = data[cursor++];
                        if (run == 0) { break; }   // sprite ends here
                    }
                }
            }
            return offsets;
        }
    }
}
