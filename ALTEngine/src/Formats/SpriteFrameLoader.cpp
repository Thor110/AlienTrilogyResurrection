#include "SpriteFrameLoader.h"
#include "BndParser.h"
#include "OverrideImage.h"
#include "RawImageRenderer.h"
#include "SpriteFrameDecompressor.h"
#include "SpriteFrameDimensions.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace ALTEngine::Formats
{
    namespace
    {
        std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open())
            {
                throw std::runtime_error("SpriteFrameLoader: could not open " + path.string());
            }
            auto size = static_cast<size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            std::vector<uint8_t> data(size);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
            return data;
        }

        // "EGGS_F000_FRAME00" - Edward's own export naming convention,
        // used for the override system too (Edward, 2026).
        std::string OverrideKey(const std::string& category, const std::string& spriteName, int section, int frame)
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s_F%03d_FRAME%02d", spriteName.c_str(), section, frame);
            return category + "/" + std::string(buf);
        }
    }

    std::optional<SpriteFrameInfo> SpriteFrameLoader::LoadFrame(
        const std::filesystem::path& b16Path,
        const std::string& spriteName,
        int section, int frame,
        const std::vector<uint8_t>& palette)
    {
        std::string category = b16Path.parent_path().filename().string();
        std::filesystem::path overrideRoot = b16Path.parent_path().parent_path() / "Override";
        std::string key = OverrideKey(category, spriteName, section, frame);

        if (auto override = TryLoadOverrideImage(overrideRoot, key))
        {
            SpriteFrameInfo info;
            info.rgba = std::move(override->rgba);
            info.width = override->width;
            info.height = override->height;
            return info;
        }

        auto dims = LookupSpriteFrameDimensions(spriteName, section, frame);
        if (!dims)
        {
            // Not necessarily an error - callers may probe for frames
            // that don't exist to find out how many there are.
            return std::nullopt;
        }

        std::vector<uint8_t> fileData = ReadFile(b16Path);
        char sectionName[8];
        std::snprintf(sectionName, sizeof(sectionName), "F%03d", section);
        std::vector<BndSection> sections = BndParser::ParseFormSections(fileData, sectionName);
        if (sections.empty())
        {
            throw std::runtime_error("SpriteFrameLoader: no section " + std::string(sectionName) + " in " + b16Path.string());
        }

        std::vector<std::vector<uint8_t>> frames = SpriteFrameDecompressor::DecompressAllFramesInSection(sections[0].data);
        if (frame < 0 || static_cast<size_t>(frame) >= frames.size())
        {
            return std::nullopt;
        }

        const std::vector<uint8_t>& pixelData = frames[static_cast<size_t>(frame)];
        auto [width, height] = *dims;

        // Palette index 0 is transparent - confirmed universal for
        // enemy/weapon sprites specifically (Edward's own
        // DetectDimensions.cs TransparencyValues lists every enemy and
        // weapon filename with a plain {0} rule), unlike level textures'
        // more complex per-level-ID/per-group table.
        SpriteFrameInfo info;
        info.rgba = RawImageRenderer::RenderRGBA(pixelData, palette, width, height, { 0 });
        info.width = width;
        info.height = height;
        return info;
    }
}
