#include "BndTextureLoader.h"
#include "BndParser.h"
#include "OverrideImage.h"
#include "RawImageRenderer.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        constexpr int TEXTURE_SIZE = 256;
        constexpr size_t CL_SUBHEADER_SIZE = 4;

        std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open())
            {
                throw std::runtime_error("BndTextureLoader: could not open " + path.string());
            }
            auto size = static_cast<size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            std::vector<uint8_t> data(size);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
            return data;
        }

        // "TP00" -> "00"
        std::string Suffix(const std::string& sectionName)
        {
            return sectionName.size() > 2 ? sectionName.substr(2) : "";
        }

        // Same convention as SplashImageLoader, just single-frame (no
        // second _TP suffix, since level/menu textures here aren't
        // split across two tiles the way LEGAL/LOGOSGFX/etc are):
        // Override/{category}/{baseName}_TP{suffix}.png. `bndPath` is
        // something like .../CD/SECT11/111GFX.B16 - category comes from
        // its parent folder name (e.g. "SECT11"), Override root is
        // CD/Override (confirmed against Edward's real install tree).
        std::optional<OverrideImage> TryOverride(const std::filesystem::path& bndPath, const std::string& suffix)
        {
            std::string category = bndPath.parent_path().filename().string();
            std::string baseName = bndPath.stem().string();
            std::string key = category + "/" + baseName + "_TP" + suffix;

            std::filesystem::path overrideRoot = bndPath.parent_path().parent_path() / "Override";
            return TryLoadOverrideImage(overrideRoot, key);
        }
    }

    BndTextureSet BndTextureLoader::Load(const std::filesystem::path& bndPath, std::optional<std::array<uint8_t, 3>> transparentRgb,
                                          const std::vector<std::vector<int>>& perTextureTransparentIndices)
    {
        std::vector<uint8_t> bnd = ReadFile(bndPath);

        std::vector<BndSection> tpSections = BndParser::ParseFormSections(bnd, "TP");
        std::vector<BndSection> clSections = BndParser::ParseFormSections(bnd, "CL");
        std::vector<BndSection> bxSections = BndParser::ParseFormSections(bnd, "BX");

        BndTextureSet result;
        result.textures.reserve(tpSections.size());

        for (size_t texPosition = 0; texPosition < tpSections.size(); ++texPosition)
        {
            const auto& tp = tpSections[texPosition];
            std::string suffix = Suffix(tp.name);

            if (auto override = TryOverride(bndPath, suffix))
            {
                BndTexture texture;
                texture.index = suffix;
                texture.rgba = std::move(override->rgba);
                texture.width = override->width;
                texture.height = override->height;
                result.textures.push_back(std::move(texture));
                continue;
            }

            auto clIt = std::find_if(clSections.begin(), clSections.end(),
                [&](const BndSection& cl) { return Suffix(cl.name) == suffix; });

            if (clIt == clSections.end())
            {
                throw std::runtime_error("BndTextureLoader: no matching CL section for " + tp.name + " in " + bndPath.string());
            }
            if (clIt->data.size() <= CL_SUBHEADER_SIZE)
            {
                throw std::runtime_error("BndTextureLoader: " + clIt->name + " too small to contain a palette in " + bndPath.string());
            }
            if (tp.data.size() != static_cast<size_t>(TEXTURE_SIZE) * TEXTURE_SIZE)
            {
                throw std::runtime_error(
                    "BndTextureLoader: " + tp.name + " is " + std::to_string(tp.data.size()) +
                    " bytes, expected " + std::to_string(TEXTURE_SIZE * TEXTURE_SIZE) + " in " + bndPath.string());
            }

            std::vector<uint8_t> raw16BitPalette(clIt->data.begin() + CL_SUBHEADER_SIZE, clIt->data.end());
            std::vector<uint8_t> palette = RawImageRenderer::Convert16BitPaletteToRGB(raw16BitPalette);

            BndTexture texture;
            texture.index = suffix;
            const std::vector<int> emptyIndices;
            const std::vector<int>& indicesForThisTexture =
                (texPosition < perTextureTransparentIndices.size()) ? perTextureTransparentIndices[texPosition] : emptyIndices;
            texture.rgba = RawImageRenderer::RenderRGBA(tp.data, palette, TEXTURE_SIZE, TEXTURE_SIZE, indicesForThisTexture, transparentRgb);
            texture.width = TEXTURE_SIZE;
            texture.height = TEXTURE_SIZE;
            result.textures.push_back(std::move(texture));
        }

        result.uvSections.reserve(bxSections.size());
        for (const auto& bx : bxSections)
        {
            result.uvSections.push_back(BxParser::ParseRectangles(bx.data));
        }

        return result;
    }
}
