#include "SplashImageLoader.h"
#include "BndParser.h"
#include "BxParser.h"
#include "OverrideImage.h"
#include "PaletteFile.h"
#include "RawImageRenderer.h"
#include "SplitFrameImage.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        constexpr int RAW_FRAME_SIZE = 256;
        constexpr int PADDING_INDEX = 0; // confirmed: never appears within real content

        std::string ZeroPad2(int n)
        {
            std::string s = std::to_string(n);
            return s.size() < 2 ? "0" + s : s;
        }

        // Override convention (OVERRIDE_GUIDE.txt): Override/{category}/
        // {baseName}_TP{frameA}_TP{frameB}.png - one pre-assembled final
        // image (e.g. 320x240 for LEGAL), not per-tile. `bndPath` is
        // something like .../CD/GFX/LEGAL.BND - category comes from its
        // parent folder name, and the Override root is derived by
        // walking up from CD/ to the game root, so no extra parameter is
        // needed at any call site.
        std::optional<OverrideImage> TryOverride(const std::filesystem::path& bndPath, int frameA, int frameB)
        {
            std::string category = bndPath.parent_path().filename().string(); // e.g. "GFX"
            std::string baseName = bndPath.stem().string();                    // e.g. "LEGAL"
            std::string key = category + "/" + baseName + "_TP" + ZeroPad2(frameA) + "_TP" + ZeroPad2(frameB);

            // .../CD/GFX/LEGAL.BND -> GFX -> CD, then CD/Override -
            // Override lives inside CD (confirmed against Edward's real
            // install tree: CD/Override/GFX/..., not GameRoot/Override/...).
            std::filesystem::path overrideRoot = bndPath.parent_path().parent_path() / "Override";

            return TryLoadOverrideImage(overrideRoot, key);
        }

        std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open())
            {
                throw std::runtime_error("SplashImageLoader: could not open " + path.string());
            }
            auto size = static_cast<size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            std::vector<uint8_t> data(size);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
            return data;
        }

        const BxRectangle* FindRect(const std::vector<BxRectangle>& rects, uint16_t texIndex)
        {
            auto it = std::find_if(rects.begin(), rects.end(),
                [&](const BxRectangle& r) { return r.textureIndex == texIndex; });
            return it != rects.end() ? &(*it) : nullptr;
        }
    }

    SplashImage SplashImageLoader::Load(
        const std::filesystem::path& bndPath,
        const std::filesystem::path& palPath,
        bool paletteTrimmed,
        int imageIndex)
    {
        int frameA = imageIndex * 2;
        int frameB = imageIndex * 2 + 1;

        if (auto override = TryOverride(bndPath, frameA, frameB))
        {
            SplashImage image;
            image.rgba = std::move(override->rgba);
            image.width = override->width;
            image.height = override->height;
            return image;
        }

        std::vector<uint8_t> bnd = ReadFile(bndPath);
        std::vector<BndSection> tpSections = BndParser::ParseFormSections(bnd, "TP");
        std::vector<BndSection> bxSections = BndParser::ParseFormSections(bnd, "BX");

        if (static_cast<int>(tpSections.size()) <= frameB)
        {
            throw std::runtime_error(
                "SplashImageLoader: imageIndex " + std::to_string(imageIndex) + " needs TP frames " +
                std::to_string(frameA) + "/" + std::to_string(frameB) + ", but " + bndPath.string() +
                " only has " + std::to_string(tpSections.size()));
        }
        if (static_cast<int>(bxSections.size()) <= frameB)
        {
            throw std::runtime_error(
                "SplashImageLoader: imageIndex " + std::to_string(imageIndex) + " needs BX sections " +
                std::to_string(frameA) + "/" + std::to_string(frameB) + ", but " + bndPath.string() +
                " only has " + std::to_string(bxSections.size()));
        }

        const BndSection& tpA = tpSections[frameA];
        const BndSection& tpB = tpSections[frameB];

        for (const BndSection* section : { &tpA, &tpB })
        {
            size_t expected = static_cast<size_t>(RAW_FRAME_SIZE) * RAW_FRAME_SIZE;
            if (section->data.size() != expected)
            {
                throw std::runtime_error(
                    "SplashImageLoader: " + bndPath.string() + " section " + section->name +
                    " is " + std::to_string(section->data.size()) + " bytes, expected " + std::to_string(expected));
            }
        }

        // Crop dimensions come from each file's own BX metadata, matched
        // by textureIndex (not just position) within the correspondingly-
        // positioned BX section - confirmed on LEGAL to be asymmetric
        // (240x240 / 80x240), so never hardcoded.
        std::vector<BxRectangle> rectsA = BxParser::ParseRectangles(bxSections[frameA].data);
        std::vector<BxRectangle> rectsB = BxParser::ParseRectangles(bxSections[frameB].data);
        const BxRectangle* rectA = FindRect(rectsA, static_cast<uint16_t>(frameA));
        const BxRectangle* rectB = FindRect(rectsB, static_cast<uint16_t>(frameB));
        if (!rectA || !rectB)
        {
            throw std::runtime_error(
                "SplashImageLoader: could not find texIndex " + std::to_string(frameA) + "/" +
                std::to_string(frameB) + " rects in " + bndPath.string());
        }
        if (rectA->height != rectB->height)
        {
            throw std::runtime_error("SplashImageLoader: frame A/B heights differ in " + bndPath.string());
        }

        std::vector<uint8_t> palette = PaletteFile::Load(palPath, paletteTrimmed);

        std::vector<uint8_t> rgbaA = RawImageRenderer::RenderRGBA(
            tpA.data, palette, RAW_FRAME_SIZE, RAW_FRAME_SIZE, { PADDING_INDEX });
        std::vector<uint8_t> rgbaB = RawImageRenderer::RenderRGBA(
            tpB.data, palette, RAW_FRAME_SIZE, RAW_FRAME_SIZE, { PADDING_INDEX });

        SplashImage image;
        image.rgba = SplitFrameImage::AssembleSideBySide(
            rgbaA, rgbaB, RAW_FRAME_SIZE, RAW_FRAME_SIZE, rectA->width, rectB->width, rectA->height);
        image.width = rectA->width + rectB->width;
        image.height = rectA->height;
        return image;
    }
}
