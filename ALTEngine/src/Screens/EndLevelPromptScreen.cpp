#include "EndLevelPromptScreen.h"
#include "MenuBackground.h"
#include "SaveSlotScreen.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"

#include <SDL3/SDL.h>
#include <array>

namespace ALTEngine::Screens
{
    using ALTEngine::Audio::SfxId;
    using ALTEngine::Audio::SfxPlayer;
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;

    namespace
    {
        constexpr Color COLOR_CURSOR{ 51, 255, 102, 255 };
        constexpr Color COLOR_DIM{ 24, 130, 52, 255 };
        constexpr std::array<const char*, 3> ITEMS{ "Continue game", "Save Game", "Quit Game" };
    }

    EndLevelPromptResult EndLevelPromptScreen::Run(const std::filesystem::path& cdDirectory)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true, EndLevelPromptChoice::Continue };
        }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        int cursor = 0;
        EndLevelPromptResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    switch (event.key.key)
                    {
                    case SDLK_UP:
                        cursor = (cursor + 2) % 3;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_DOWN:
                        cursor = (cursor + 1) % 3;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        if (cursor == 0) { result.choice = EndLevelPromptChoice::Continue; running = false; }
                        else if (cursor == 1)
                        {
                            SaveSlotResult saveResult = SaveSlotScreen::Run(cdDirectory, SaveSlotMode::Save, StubSaveSlots());
                            if (saveResult.windowClosed) { result.windowClosed = true; running = false; }
                            // else: stays on this same prompt, matching a save checkpoint - doesn't force Continue/Quit
                        }
                        else { result.choice = EndLevelPromptChoice::Quit; running = false; }
                        break;
                    default:
                        break;
                    }
                }
            }
            if (!running) { break; }

            DrawMenuBackground(renderer, background, bgW, bgH);

            int scale = 3;
            int lineHeight = TextHeight(scale) + scale * 6;
            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
            int startY = windowH / 2 - lineHeight;

            for (int i = 0; i < 3; ++i)
            {
                Color color = (i == cursor) ? COLOR_CURSOR : COLOR_DIM;
                int textX = (windowW - TextWidth(ITEMS[static_cast<size_t>(i)], scale)) / 2;
                DrawBitmapText(renderer, ITEMS[static_cast<size_t>(i)], textX, startY + i * lineHeight, scale, color);
            }

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }
}
