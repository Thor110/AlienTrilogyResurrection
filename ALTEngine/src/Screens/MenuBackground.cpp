#include "MenuBackground.h"
#include "../Formats/SplashImageLoader.h"

#include <algorithm>
#include <optional>

namespace ALTEngine::Screens
{
    using ALTEngine::Formats::SplashImage;
    using ALTEngine::Formats::SplashImageLoader;

    namespace
    {
        std::optional<std::filesystem::path> ResolveGfxFile(const std::filesystem::path& gfxDir, const std::string& baseName)
        {
            for (const char* ext : { ".BND", ".B16", ".16" })
            {
                std::filesystem::path candidate = gfxDir / (baseName + ext);
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec)) { return candidate; }
            }
            return std::nullopt;
        }
    }

    SDL_Texture* LoadMenuBackground(const std::filesystem::path& cdDirectory, SDL_Renderer* renderer, int imageIndex, int& outW, int& outH)
    {
        auto bndPath = ResolveGfxFile(cdDirectory / "GFX", "LOGOSGFX");
        std::filesystem::path palPath = cdDirectory / "PALS" / "LOGOSGFX.PAL";
        if (!bndPath.has_value())
        {
            SDL_Log("MenuBackground: could not find LOGOSGFX graphics file");
            return nullptr;
        }
        std::error_code ec;
        if (!std::filesystem::exists(palPath, ec))
        {
            SDL_Log("MenuBackground: could not find %s", palPath.string().c_str());
            return nullptr;
        }

        try
        {
            SplashImage image = SplashImageLoader::Load(*bndPath, palPath, /*paletteTrimmed*/ false, imageIndex);
            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
            if (!texture) { return nullptr; }
            SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4);
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            outW = image.width;
            outH = image.height;
            return texture;
        }
        catch (const std::exception& e)
        {
            SDL_Log("MenuBackground: failed to load LOGOSGFX image %d: %s", imageIndex, e.what());
            return nullptr;
        }
    }

    void DrawMenuBackground(SDL_Renderer* renderer, SDL_Texture* texture, int texW, int texH)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (!texture) { return; }

        int windowW = 0, windowH = 0;
        SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

        // Height-based fill, not aspect-preserving fit - Edward, 2026:
        // "there are only 6 menu background images, I am thinking we
        // should scale them to remove black bars for non 16:9
        // resolutions... Scale the height and width until the height
        // matches the resolution height" - lets any excess width run
        // past the window edges (centred, clipped) rather than
        // letterboxing with black bars on the sides.
        float scale = static_cast<float>(windowH) / static_cast<float>(texH);
        float destW = texW * scale, destH = texH * scale;
        SDL_FRect dest{ (windowW - destW) / 2.0f, (windowH - destH) / 2.0f, destW, destH };
        SDL_RenderTexture(renderer, texture, nullptr, &dest);
    }
}
