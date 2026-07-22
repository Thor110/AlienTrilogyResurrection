#pragma once

#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    struct BxRectangle
    {
        int x, y, width, height;
        uint16_t textureIndex; // which TP index this rect belongs to (00-04 etc)
    };

    // Port of ModelRenderer.cs's ParseBxRectangles. A BX section is:
    //   2 bytes  = Int16 LE rect count
    //   per rect: x(u8), y(u8), width(u8), height(u8), textureIndex(u16 LE)
    // width/height are stored off-by-one (+1 applied on read, matching the
    // original - noted there as unresolved/uncertain, carried over as-is).
    class BxParser
    {
    public:
        static std::vector<BxRectangle> ParseRectangles(const std::vector<uint8_t>& bxData);
    };
}
