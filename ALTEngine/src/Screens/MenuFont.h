#pragma once

#include "../Formats/BndTextureLoader.h"

#include <SDL3/SDL.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Screens::MenuFont
{
    // The large menu font, GFX/FONT1GFX (FUN_000188b0 / FUN_000187f4): the
    // Mission Assessment's and the multiplayer results' titles and figures.
    struct Font
    {
        SDL_Texture* texture = nullptr;
        std::vector<ALTEngine::Formats::BxRectangle> rects;
    };

    inline Font LoadFont(const std::filesystem::path& cdDirectory, SDL_Renderer* renderer)
    {
        Font font;
        for (const char* ext : { ".BND", ".B16", ".16" })
        {
            const auto path = cdDirectory / "GFX" / (std::string("FONT1GFX") + ext);
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) { continue; }
            try
            {
                // Palette index 0 is the key, as the original's fills test.
                auto set = ALTEngine::Formats::BndTextureLoader::Load(path, std::nullopt, { { 0 } });
                if (set.textures.empty()) { break; }
                const auto& page = set.textures[0];
                font.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, page.width, page.height);
                if (font.texture)
                {
                    SDL_UpdateTexture(font.texture, nullptr, page.rgba.data(), page.width * 4);
                    SDL_SetTextureScaleMode(font.texture, SDL_SCALEMODE_NEAREST);
                    SDL_SetTextureBlendMode(font.texture, SDL_BLENDMODE_BLEND);
                }
                font.rects = set.uvRects;
            }
            catch (const std::exception& e) { SDL_Log("MenuFont: FONT1GFX: %s", e.what()); }
            break;
        }
        if (!font.texture) { SDL_Log("MenuFont: FONT1GFX not found"); }
        return font;
    }

    // A descriptor's rect is u0..u1 inclusive, but the original's quads
    // span u1 - u0 (FUN_00018680), so the last row and column are not
    // drawn; drawing them shows bits of the neighbouring glyph. The bar
    // frame is drawn a pixel larger each way (FUN_00012858), so it keeps
    // the whole rect.
    inline void DrawDescriptor(SDL_Renderer* renderer, const Font& font, int index, float x, float y, SDL_Color tint, bool whole = false)
    {
        if (!font.texture || index < 0 || static_cast<size_t>(index) >= font.rects.size()) { return; }
        const auto& r = font.rects[static_cast<size_t>(index)];
        const float w = float(r.width - (whole ? 0 : 1));
        const float h = float(r.height - (whole ? 0 : 1));
        SDL_FRect src{ float(r.x), float(r.y), w, h };
        SDL_FRect dst{ x, y, w, h };
        SDL_SetTextureColorMod(font.texture, tint.r, tint.g, tint.b);
        SDL_RenderTexture(renderer, font.texture, &src, &dst);
    }

    // FUN_000188b0 / FUN_000187f4: a glyph is descriptor (char - 0x20),
    // the next one starts at its width less one, a space is 8 wide.
    inline int TextWidth(const Font& font, const std::string& text)
    {
        int w = 0;
        for (unsigned char ch : text)
        {
            if (ch == ' ') { w += 8; continue; }
            const int d = int(ch) - 0x20;
            if (d >= 0 && static_cast<size_t>(d) < font.rects.size()) { w += font.rects[static_cast<size_t>(d)].width - 1; }
        }
        return w;
    }

    inline void DrawString(SDL_Renderer* renderer, const Font& font, const std::string& text, int x, int y, SDL_Color tint)
    {
        for (unsigned char ch : text)
        {
            if (ch == ' ') { x += 8; continue; }
            const int d = int(ch) - 0x20;
            if (d < 0 || static_cast<size_t>(d) >= font.rects.size()) { continue; }
            DrawDescriptor(renderer, font, d, float(x), float(y), tint);
            x += font.rects[static_cast<size_t>(d)].width - 1;
        }
    }

    inline void DrawCentred(SDL_Renderer* renderer, const Font& font, const std::string& text, int y, SDL_Color tint,
                            int screenWidth = 320)
    {
        DrawString(renderer, font, text, (screenWidth - TextWidth(font, text)) / 2, y, tint);
    }

}
