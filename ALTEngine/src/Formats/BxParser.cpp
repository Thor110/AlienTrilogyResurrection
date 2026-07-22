#include "BxParser.h"

#include <stdexcept>
#include <string>

namespace ALTEngine::Formats
{
    std::vector<BxRectangle> BxParser::ParseRectangles(const std::vector<uint8_t>& bxData)
    {
        if (bxData.size() < 2)
        {
            throw std::runtime_error("BxParser::ParseRectangles: data too short for rect count");
        }

        uint16_t rectCount = static_cast<uint16_t>(bxData[0] | (bxData[1] << 8)); // Int16 LE
        std::vector<BxRectangle> rectangles;
        rectangles.reserve(rectCount);

        size_t pos = 2;
        for (uint16_t i = 0; i < rectCount; ++i)
        {
            if (pos + 6 > bxData.size())
            {
                throw std::runtime_error("BxParser::ParseRectangles: data too short for rect " + std::to_string(i));
            }
            uint8_t x = bxData[pos + 0];
            uint8_t y = bxData[pos + 1];
            uint8_t width = bxData[pos + 2];
            uint8_t height = bxData[pos + 3];
            uint16_t textureIndex = static_cast<uint16_t>(bxData[pos + 4] | (bxData[pos + 5] << 8));
            pos += 6;

            rectangles.push_back({ x, y, width + 1, height + 1, textureIndex });
        }

        return rectangles;
    }
}
