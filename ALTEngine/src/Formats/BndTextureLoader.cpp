#include "BndTextureLoader.h"
#include "BndParser.h"
#include "OverrideImage.h"
#include "RawImageRenderer.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        constexpr int TEXTURE_SIZE = 256;
        constexpr size_t CL_SUBHEADER_SIZE = 4;

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

        BndTextureSet ParseTextureSet(const std::filesystem::path& bndPath, std::optional<std::array<uint8_t, 3>> transparentRgb,
                                       const std::vector<std::vector<int>>& perTextureTransparentIndices)
        {
            // Cached, single-pass parse (Edward, 2026: this used to read
            // the file from disk and separately scan it three full
            // times, once each for "TP"/"CL"/"BX" - the main source of
            // the lag noticed navigating the options menu, since each
            // menu item is a different model and therefore triggers this
            // fresh). Filtering an already-parsed, cached list by prefix
            // is just a string comparison over an in-memory vector - no
            // I/O, no re-scan.
            const std::vector<BndSection>& allSections = BndParser::LoadCached(bndPath);

            auto filterByPrefix = [&allSections](const std::string& prefix) {
                std::vector<BndSection> result;
                for (auto& section : allSections)
                {
                    if (section.name.rfind(prefix, 0) == 0) { result.push_back(section); }
                }
                return result;
            };

            std::vector<BndSection> tpSections = filterByPrefix("TP");
            std::vector<BndSection> clSections = filterByPrefix("CL");
            std::vector<BndSection> bxSections = filterByPrefix("BX");

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

            for (const auto& bx : bxSections)
            {
                std::string suffix = Suffix(bx.name);
                int page = 0;
                try { page = std::stoi(suffix); }
                catch (...) { page = 0; } // malformed suffix - shouldn't happen given filterByPrefix already matched "BX", but don't throw over it
                auto rects = BxParser::ParseRectangles(bx.data, page);
                result.uvRects.insert(result.uvRects.end(), rects.begin(), rects.end());
            }

            return result;
        }
    }

    BndTextureSet BndTextureLoader::Load(const std::filesystem::path& bndPath, std::optional<std::array<uint8_t, 3>> transparentRgb,
                                          const std::vector<std::vector<int>>& perTextureTransparentIndices)
    {
        // Fully-parsed/decoded cache, not just the raw BND sections
        // (BndParser::LoadCached already handles that layer) - Edward,
        // 2026: without this, navigating the options menu (each cursor
        // move requests a different model, but all 14 OPTOBJ models
        // share one OPTGFX.BND) would still re-decode all 14 textures'
        // palette-indexed pixels to RGBA on every single navigation
        // step, even though only one texture is actually needed each
        // time. Only cached for the common menu-model case (no per-
        // texture transparency indices) - level texture loading, which
        // does use those, happens once per level rather than repeatedly
        // during navigation, so it doesn't need this and a vector-of-
        // vectors isn't worth serializing into a cache key.
        if (perTextureTransparentIndices.empty())
        {
            static std::unordered_map<std::string, BndTextureSet> cache;

            std::string key = std::filesystem::absolute(bndPath).lexically_normal().string();
            key += transparentRgb ? ("|" + std::to_string((*transparentRgb)[0]) + "," +
                                      std::to_string((*transparentRgb)[1]) + "," + std::to_string((*transparentRgb)[2]))
                                   : "|none";

            auto it = cache.find(key);
            if (it != cache.end())
            {
                return it->second;
            }

            BndTextureSet result = ParseTextureSet(bndPath, transparentRgb, perTextureTransparentIndices);
            auto [inserted, _] = cache.emplace(std::move(key), result);
            return inserted->second;
        }

        return ParseTextureSet(bndPath, transparentRgb, perTextureTransparentIndices);
    }
}
