#include "MissionBriefingScreen.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/MissionText.h"
#include "../Formats/SplashImageLoader.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <optional>

namespace ALTEngine::Screens
{
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::Language;
    using ALTEngine::Bootstrap::MissionTextFilename;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;
    using ALTEngine::Formats::BriefingParagraph;
    using ALTEngine::Formats::MissionBriefing;
    using ALTEngine::Formats::MissionTextLoader;
    using ALTEngine::Formats::SplashImage;
    using ALTEngine::Formats::SplashImageLoader;

    namespace
    {
        constexpr Color COLOR_BRIGHT{ 51, 255, 102, 255 };  // &1
        constexpr Color COLOR_DARK{ 24, 130, 52, 255 };     // &0
        constexpr Color COLOR_LOADING{ 220, 40, 40, 255 };  // "Loading data" - red, per the reference screenshot
        constexpr Color COLOR_READY{ 235, 235, 235, 255 };  // "Hit any key to Continue..." - light/white, per the reference screenshot

        constexpr int LOADING_MS = 2000; // placeholder timer - see MissionBriefingScreen.h
        constexpr int MS_PER_CHAR = 25;  // typewriter speed - ~40 chars/sec

        // "1.1.1" -> chapter 1 -> COLONY. Matches the chapter/background
        // pairing already established in data/LevelManifest.json
        // (Chapter 1: COLONY, Chapter 2: PRISHOLD, Chapter 3: BONESHIP).
        std::string ChapterBackgroundName(const std::string& levelCode)
        {
            if (!levelCode.empty())
            {
                switch (levelCode[0])
                {
                case '2': return "PRISHOLD";
                case '3': return "BONESHIP";
                case '1':
                default:  return "COLONY";
                }
            }
            return "COLONY";
        }

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

        SDL_Texture* LoadBackgroundTexture(const std::filesystem::path& cdDirectory, SDL_Renderer* renderer,
                                            const std::string& baseName, int& outW, int& outH)
        {
            auto bndPath = ResolveGfxFile(cdDirectory / "GFX", baseName);
            std::filesystem::path palPath = cdDirectory / "PALS" / (baseName + ".PAL");
            std::error_code ec;
            if (!bndPath.has_value() || !std::filesystem::exists(palPath, ec))
            {
                SDL_Log("MissionBriefingScreen: could not find %s background", baseName.c_str());
                return nullptr;
            }

            try
            {
                // COLONY/PRISHOLD/BONESHIP are "trimmed" palettes (disk
                // file missing the first 96 bytes) - established earlier
                // alongside BONESHIP/COLONY/PRISHOLD in PaletteFile.h.
                SplashImage image = SplashImageLoader::Load(*bndPath, palPath, /*paletteTrimmed*/ true, 0);
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
                SDL_Log("MissionBriefingScreen: failed to load %s: %s", baseName.c_str(), e.what());
                return nullptr;
            }
        }

        void DrawBackground(SDL_Renderer* renderer, SDL_Texture* texture, int texW, int texH)
        {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            if (!texture) { return; }

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
            float scale = std::min(static_cast<float>(windowW) / texW, static_cast<float>(windowH) / texH);
            float destW = texW * scale, destH = texH * scale;
            SDL_FRect dest{ (windowW - destW) / 2.0f, (windowH - destH) / 2.0f, destW, destH };
            SDL_RenderTexture(renderer, texture, nullptr, &dest);
        }

        int TotalCharCount(const MissionBriefing& briefing)
        {
            int total = 0;
            for (const auto& paragraph : briefing.paragraphs)
            {
                for (const auto& line : paragraph.lines)
                {
                    for (const auto& segment : line.segments) { total += static_cast<int>(segment.text.size()); }
                }
            }
            return total;
        }

        // Draws the briefing text up to `revealedChars` characters -
        // complete lines before the cutoff draw in full, the line at the
        // cutoff draws partially, nothing after it draws at all. Original
        // line breaks are preserved exactly as parsed, not re-wrapped.
        void DrawTypedText(SDL_Renderer* renderer, const MissionBriefing& briefing, int revealedChars, int x, int y, int scale, int lineHeight)
        {
            int consumed = 0;
            int cursorY = y;

            for (size_t p = 0; p < briefing.paragraphs.size(); ++p)
            {
                if (p > 0) { cursorY += lineHeight; } // blank line between paragraphs

                for (const auto& line : briefing.paragraphs[p].lines)
                {
                    int cursorX = x;
                    for (const auto& segment : line.segments)
                    {
                        Color color = segment.bright ? COLOR_BRIGHT : COLOR_DARK;
                        for (char c : segment.text)
                        {
                            if (consumed >= revealedChars) { return; }
                            std::string glyph(1, c);
                            DrawBitmapText(renderer, glyph, cursorX, cursorY, scale, color);
                            cursorX += TextWidth(glyph, scale);
                            ++consumed;
                        }
                    }
                    cursorY += lineHeight;
                }
            }
        }
    }

    MissionBriefingResult MissionBriefingScreen::Run(
        const std::filesystem::path& cdDirectory,
        Language language,
        const std::string& levelCode)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true };
        }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadBackgroundTexture(cdDirectory, renderer, ChapterBackgroundName(levelCode), bgW, bgH);

        std::vector<MissionBriefing> allBriefings;
        const MissionBriefing* briefing = nullptr;
        try
        {
            std::filesystem::path textPath = cdDirectory / "LANGUAGE" / (MissionTextFilename(language) + ".TXT");
            allBriefings = MissionTextLoader::Load(textPath);
            for (const auto& b : allBriefings)
            {
                if (b.levelCode == levelCode) { briefing = &b; break; }
            }
        }
        catch (const std::exception& e)
        {
            SDL_Log("MissionBriefingScreen: failed to load mission text: %s", e.what());
        }
        if (!briefing)
        {
            SDL_Log("MissionBriefingScreen: no briefing found for level %s - showing an empty briefing", levelCode.c_str());
        }
        static const MissionBriefing emptyBriefing{};
        const MissionBriefing& shown = briefing ? *briefing : emptyBriefing;

        int totalChars = TotalCharCount(shown);
        Uint64 startTicks = SDL_GetTicks();

        MissionBriefingResult result;
        bool running = true;
        bool textFullyRevealed = false;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                {
                    Uint64 elapsed = SDL_GetTicks() - startTicks;
                    bool loadingDone = elapsed >= static_cast<Uint64>(LOADING_MS);

                    if (!textFullyRevealed)
                    {
                        // Skip the typewriter straight to full text, same
                        // as most games with this kind of text-crawl.
                        textFullyRevealed = true;
                    }
                    else if (loadingDone)
                    {
                        running = false; // "Hit any key to Continue..." - proceed
                    }
                }
            }
            if (!running) { break; }

            Uint64 elapsed = SDL_GetTicks() - startTicks;
            bool loadingDone = elapsed >= static_cast<Uint64>(LOADING_MS);

            int revealedChars = textFullyRevealed ? totalChars : static_cast<int>(elapsed / MS_PER_CHAR);
            if (revealedChars >= totalChars) { revealedChars = totalChars; textFullyRevealed = true; }

            DrawBackground(renderer, background, bgW, bgH);

            int scale = 3;
            int lineHeight = TextHeight(scale) + scale * 4;
            int margin = scale * 8;

            DrawBitmapText(renderer, "Incoming data transfer", margin, margin, scale, COLOR_BRIGHT);
            DrawBitmapText(renderer, "Mission brief:", margin, margin + lineHeight * 2, scale, COLOR_BRIGHT);

            DrawTypedText(renderer, shown, revealedChars, margin, margin + lineHeight * 4, scale, lineHeight);

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
            if (!loadingDone)
            {
                DrawBitmapText(renderer, "Loading data", margin, windowH - lineHeight * 2, scale, COLOR_LOADING);
            }
            else
            {
                DrawBitmapText(renderer, "Hit any key to Continue...", margin, windowH - lineHeight * 2, scale, COLOR_READY);
            }

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }
}
