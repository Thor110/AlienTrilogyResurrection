#pragma once

#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    struct BxRectangle
    {
        int x, y, width, height;
        uint16_t textureIndex; // which TP index this rect belongs to (00-04 etc)
        int page = 0;          // TPage - from the BX chunk's own tag digits ("BX00" -> 0, "BX01" -> 1...), NOT computed from position in a flattened list. Confirmed via Ghidra (Edward, 2026): the descriptor's own TPage field is what actually selects the texture at render time, resolved once at load, not by "which group" a texIndex falls into.
    };

    // Port of ModelRenderer.cs's ParseBxRectangles. A BX section is:
    //   2 bytes  = Int16 LE rect count
    //   per rect: x(u8), y(u8), width(u8), height(u8), textureIndex(u16 LE)
    // width/height are stored off-by-one (+1 applied on read, matching the
    // original - noted there as unresolved/uncertain, carried over as-is).
    //
    // `page`, if given, stamps every parsed rectangle's own `page` field -
    // pass the BX chunk's own tag digits ("BX00" -> 0, "BX01" -> 1...),
    // confirmed via Ghidra (Edward, 2026) as the actual source of a
    // descriptor's TPage, not its position within a group.
    class BxParser
    {
    public:
        static std::vector<BxRectangle> ParseRectangles(const std::vector<uint8_t>& bxData, int page = 0);
    };
}
