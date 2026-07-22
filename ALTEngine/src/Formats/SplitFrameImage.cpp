#include "SplitFrameImage.h"

#include <algorithm>
#include <stdexcept>

namespace ALTEngine::Formats
{
    std::vector<uint8_t> SplitFrameImage::AssembleSideBySide(
        const std::vector<uint8_t>& frameA, const std::vector<uint8_t>& frameB,
        int rawFrameWidth, int rawFrameHeight,
        int visibleWidthA, int visibleWidthB, int visibleHeight)
    {
        if (visibleWidthA > rawFrameWidth || visibleWidthB > rawFrameWidth || visibleHeight > rawFrameHeight)
        {
            throw std::runtime_error("SplitFrameImage::AssembleSideBySide: visible size exceeds raw frame size");
        }
        size_t expectedRawSize = static_cast<size_t>(rawFrameWidth) * rawFrameHeight * 4;
        if (frameA.size() != expectedRawSize || frameB.size() != expectedRawSize)
        {
            throw std::runtime_error("SplitFrameImage::AssembleSideBySide: frame buffer size doesn't match rawFrameWidth x rawFrameHeight x 4");
        }

        int outWidth = visibleWidthA + visibleWidthB;
        std::vector<uint8_t> out(static_cast<size_t>(outWidth) * visibleHeight * 4, 0);

        auto blit = [&](const std::vector<uint8_t>& frame, int destX, int visibleWidth) {
            for (int y = 0; y < visibleHeight; ++y)
            {
                const uint8_t* srcRow = frame.data() + (static_cast<size_t>(y) * rawFrameWidth) * 4;
                uint8_t* dstRow = out.data() + (static_cast<size_t>(y) * outWidth + destX) * 4;
                std::copy(srcRow, srcRow + static_cast<size_t>(visibleWidth) * 4, dstRow);
            }
        };

        blit(frameA, 0, visibleWidthA);
        blit(frameB, visibleWidthA, visibleWidthB);

        return out;
    }
}
