#include "MultiplayerResultsScreen.h"
#include "MenuBackground.h"
#include "MenuFont.h"
#include "ScreenFadeDraw.h"
#include "../Audio/MusicPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Renderer/HudPanel.h"
#include "../Renderer/HudRenderer.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>

namespace ALTEngine::Screens
{
    namespace
    {
        // 0xa6588 (0x80 full brightness), and ours for players 5-8.
        SDL_Color PlayerColour(int number)
        {
            static const SDL_Color table[9] = {
                { 0xe0, 0xe0, 0xe0, 255 },
                { 0xe0, 0xe0, 0xe0, 255 }, { 0xe0, 0x00, 0x00, 255 }, { 0x00, 0xe0, 0x00, 255 }, { 0x00, 0x00, 0xe0, 255 },
                { 0xe0, 0xe0, 0x00, 255 }, { 0x00, 0xe0, 0xe0, 255 }, { 0xe0, 0x00, 0xe0, 255 }, { 0xe0, 0x70, 0x00, 255 },
            };
            return table[(number >= 0 && number <= 8) ? number : 0];
        }

        // FUN_000151c4: a space is 5 wide; the letters between, a word at a
        // time, at font C's own advance.
        void DrawFontC(SDL_Renderer* renderer, const ALTEngine::Renderer::HudRenderer& hud, const std::string& text,
                       int x, int y, SDL_Color colour)
        {
            size_t i = 0;
            while (i < text.size())
            {
                if (text[i] == ' ') { x += 5; ++i; continue; }
                const size_t end = text.find(' ', i);
                const std::string word = text.substr(i, end == std::string::npos ? std::string::npos : end - i);
                x += hud.DrawHudText(renderer, word, x, y, ALTEngine::Renderer::HUD_FONT_C_FIRST_DESCRIPTOR,
                                     1.0f, 1.0f, ALTEngine::Renderer::HUD_FONT_C_LETTER_SPACING, colour);
                i += word.size();
            }
        }
    }

    bool MultiplayerResultsScreen::Run(const std::filesystem::path& cdDirectory, Bootstrap::Language language,
                                       const std::string& levelDigits, const std::vector<MultiplayerResultsPlayer>& players,
                                       const std::function<void()>& idle)
    {
        auto& app = ALTEngine::Bootstrap::AppWindow::Instance();
        if (!app.EnsureCreated()) { return true; }
        SDL_Renderer* renderer = app.Renderer();
        constexpr int W = 320, H = 240;

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);
        MenuFont::Font font = MenuFont::LoadFont(cdDirectory, renderer);
        ALTEngine::Renderer::HudRenderer hud;
        hud.Load(renderer, cdDirectory,
                 ALTEngine::Renderer::HudPanelFileForLevelId(ALTEngine::Audio::LevelIdForCode(levelDigits)),
                 ALTEngine::Bootstrap::LanguageFolderName(language));
        SDL_Texture* canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, W, H);
        if (canvas) { SDL_SetTextureScaleMode(canvas, SDL_SCALEMODE_NEAREST); }

        const auto cells = MultiplayerResults::Layout(players);
        const std::string title = ALTEngine::Bootstrap::TrOr(ALTEngine::Bootstrap::StringId::GsScores, language, "SCORES");
        const std::string prompt = ALTEngine::Bootstrap::TrOr(ALTEngine::Bootstrap::StringId::GsPressAnyKey, language, "Press any key");

        auto drawFrame = [&]() {
            if (canvas) { SDL_SetRenderTarget(renderer, canvas); }
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            if (background) { SDL_RenderTexture(renderer, background, nullptr, nullptr); }
            const SDL_Color white{ 255, 255, 255, 255 };
            MenuFont::DrawCentred(renderer, font, title, MultiplayerResults::TITLE_Y, white, W);
            for (const auto& cell : cells) { DrawFontC(renderer, hud, cell.text, cell.x, cell.y, PlayerColour(cell.colour)); }
            MenuFont::DrawCentred(renderer, font, prompt, MultiplayerResults::PROMPT_Y, white, W);
            if (canvas)
            {
                SDL_SetRenderTarget(renderer, nullptr);
                DrawMenuBackground(renderer, canvas, W, H);
            }
        };

        // In as the original's screens come in (FUN_0004eec0), out on a key
        // or after 700 frames (FUN_0004eefc).
        namespace SF = ALTEngine::Formats::ScreenFade;
        SF::State fade;
        SF::StartIn(fade);
        ScreenFadeDraw::Clock fadeClock;
        fadeClock.Start();
        const Uint64 start = SDL_GetTicks();
        bool windowClosed = false;
        bool running = true;
        while (running)
        {
            if (idle) { idle(); }
            ALTEngine::Audio::MusicPlayer::Update();
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { windowClosed = true; running = false; }
                else if ((event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                {
                    running = false;
                }
            }
            if (static_cast<double>(SDL_GetTicks() - start) / 1000.0 >= MultiplayerResults::FRAMES_TO_LEAVE / SF::STEP_HZ)
            {
                running = false;
            }
            if (!running) { break; }
            drawFrame();
            ScreenFadeDraw::Overlay(renderer, fade);
            SDL_RenderPresent(renderer);
            SDL_Delay(1);
            fadeClock.Advance(fade);
        }
        if (!windowClosed && !ScreenFadeDraw::RunFadeOut(renderer, fade, drawFrame)) { windowClosed = true; }

        hud.Unload();
        if (canvas) { SDL_DestroyTexture(canvas); }
        if (font.texture) { SDL_DestroyTexture(font.texture); }
        if (background) { SDL_DestroyTexture(background); }
        return !windowClosed;
    }
}
