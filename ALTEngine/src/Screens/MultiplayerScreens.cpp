#include "MultiplayerScreens.h"
#include "MenuBackground.h"
#include "TextFieldEditor.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Menu/MenuTree.h" // for ModelIndex::NetworkedComputers
#include "../Renderer/ModelRenderer.h"
#include "../Renderer/ModelPreview.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>

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
        constexpr Color COLOR_EDITING{ 235, 235, 235, 255 };
        constexpr Color COLOR_WHITE{ 255, 255, 255, 255 }; // matches the main menu's own selected-item white exactly

        Color LerpColor(Color a, Color b, float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            return Color{
                static_cast<Uint8>(a.r + (b.r - a.r) * t),
                static_cast<Uint8>(a.g + (b.g - a.g) * t),
                static_cast<Uint8>(a.b + (b.b - a.b) * t),
                255
            };
        }

        // Same ~1.7s "breathing" pulse as the other menu screens (Edward,
        // 2026: "same frequency pulse as the boxed items").
        float PulsePhase()
        {
            return static_cast<float>((std::sin(static_cast<double>(SDL_GetTicks()) / 400.0) + 1.0) / 2.0);
        }

        // The selected-but-not-editing text color for these menus -
        // pulses between green and white rather than a static light
        // green (Edward, 2026: "currently the multiplayer menus have a
        // light green, it should be white like the main menu and the
        // pulsing should go between the green and the white").
        Color SelectedTextColor()
        {
            return LerpColor(COLOR_CURSOR, COLOR_WHITE, PulsePhase());
        }

        // NetworkedComputers (OPTOBJ index 10) - Edward noted this model
        // (and others like it) rests at a different natural angle than
        // most. The exact value below is a visual guess from the
        // reference screenshots' diagonal framing, NOT measured/confirmed
        // - adjust once it can actually be seen rendered.
        constexpr float NETWORKED_COMPUTERS_BASE_ROTATION = -0.6f;

        // Fills the screen (roughly) - fixed, not tied to cursor
        // position or any other dynamic value. Every multiplayer screen
        // wants the same full-background treatment, so this takes no
        // position/size parameters anymore - it just fills the current
        // render target (Edward, 2026: models should fill "the entire
        // background or thereabouts", and shouldn't jump around while
        // navigating). Uses the shared DrawModelPreview helper
        // (src/Renderer/ModelPreview.h), also used by MenuController's
        // Options screen and SaveSlotScreen (Edward, 2026).
        void DrawNetworkedComputersModel(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory)
        {
            ALTEngine::Renderer::ModelPreviewSource source = ALTEngine::Renderer::ModelPreviewSource::ForOptobj(cdDirectory, ALTEngine::Menu::ModelIndex::NetworkedComputers);
            source.baseRotationRadians = NETWORKED_COMPUTERS_BASE_ROTATION;

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
            float rotationAngle = static_cast<float>(SDL_GetTicks()) / 1000.0f;
            ALTEngine::Renderer::DrawModelPreview(renderer, source, 0, 0, windowW, windowH, rotationAngle);
        }
    }

    MultiplayerMainResult MultiplayerMainScreen::Run(const std::filesystem::path& cdDirectory)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { return { true, MultiplayerMainChoice::Back }; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        constexpr std::array<const char*, 3> ITEMS{ "Start Multiplayer Game", "Join Multiplayer Game", "Multiplayer Options" };
        int cursor = 0;
        MultiplayerMainResult result;
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
                        result.choice = cursor == 0 ? MultiplayerMainChoice::StartGame
                                       : cursor == 1 ? MultiplayerMainChoice::JoinGame
                                                      : MultiplayerMainChoice::Options;
                        running = false;
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        break;
                    case SDLK_ESCAPE:
                        result.choice = MultiplayerMainChoice::Back;
                        running = false;
                        SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
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

            DrawNetworkedComputersModel(renderer, cdDirectory);

            int startY = windowH / 2 - lineHeight / 2;
            for (int i = 0; i < 3; ++i)
            {
                Color color = (i == cursor) ? SelectedTextColor() : COLOR_DIM;
                int textX = (windowW - TextWidth(ITEMS[static_cast<size_t>(i)], scale)) / 2;
                DrawBitmapText(renderer, ITEMS[static_cast<size_t>(i)], textX, startY + i * lineHeight, scale, color);
            }

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }

    MultiplayerSubResult MultiplayerOptionsScreen::Run(const std::filesystem::path& cdDirectory, MultiplayerSettings& settings)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { return { true }; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        int cursor = 0; // 0 = name, 1-8 = F2-F9 messages
        TextFieldEditor editor;
        MultiplayerSubResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; break; }

                if (editor.IsEditing())
                {
                    bool wasEditing = editor.IsEditing();
                    editor.HandleEvent(event);
                    if (wasEditing && !editor.IsEditing())
                    {
                        // Just finished editing - commit back to settings
                        if (cursor == 0) { settings.playerName = editor.Value(); }
                        else { settings.messages[static_cast<size_t>(cursor - 1)] = editor.Value(); }
                    }
                    continue;
                }

                if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    switch (event.key.key)
                    {
                    case SDLK_UP:
                        cursor = (cursor + 8) % 9;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_DOWN:
                        cursor = (cursor + 1) % 9;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    {
                        std::string current = (cursor == 0) ? settings.playerName : settings.messages[static_cast<size_t>(cursor - 1)];
                        editor.BeginEdit(current, cursor == 0 ? 16 : 40);
                        break;
                    }
                    case SDLK_ESCAPE:
                        running = false;
                        SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (!running) { break; }

            DrawMenuBackground(renderer, background, bgW, bgH);
            DrawNetworkedComputersModel(renderer, cdDirectory);
            int scale = 3;
            int lineHeight = TextHeight(scale) + scale * 4;
            int margin = scale * 8;
            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            std::string title = "EDIT YOUR DATA";
            DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, margin, scale, COLOR_CURSOR);

            int rowY = margin + lineHeight * 3;
            std::string nameLine = "YOUR NAME: " + (cursor == 0 && editor.IsEditing() ? editor.Value() : settings.playerName);
            DrawBitmapText(renderer, nameLine, margin, rowY, scale, (cursor == 0) ? (editor.IsEditing() ? COLOR_EDITING : SelectedTextColor()) : COLOR_DIM);

            for (int i = 0; i < 8; ++i)
            {
                int row = i + 1;
                std::string label = "F" + std::to_string(i + 2) + " Message: ";
                std::string value = (cursor == row && editor.IsEditing()) ? editor.Value() : settings.messages[static_cast<size_t>(i)];
                Color color = (cursor == row) ? (editor.IsEditing() ? COLOR_EDITING : SelectedTextColor()) : COLOR_DIM;
                DrawBitmapText(renderer, label + value, margin, rowY + lineHeight * (i + 2), scale, color);
            }

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }

    MultiplayerStartResult MultiplayerStartScreen::Run(const std::filesystem::path& cdDirectory, MultiplayerSettings& settings)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { return { true, false }; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        int cursor = 0; // 0=Start Game, 1=Name Of Game, 2=Start at level, 3=Minimum game length
        TextFieldEditor editor;
        MultiplayerStartResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; break; }

                if (editor.IsEditing())
                {
                    bool wasEditing = editor.IsEditing();
                    editor.HandleEvent(event);
                    if (wasEditing && !editor.IsEditing()) { settings.gameName = editor.Value(); }
                    continue;
                }

                if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    switch (event.key.key)
                    {
                    case SDLK_UP:
                        cursor = (cursor + 3) % 4;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_DOWN:
                        cursor = (cursor + 1) % 4;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_LEFT:
                        if (cursor == 2) { settings.startLevel = std::max(1, settings.startLevel - 1); SfxPlayer::Play(SfxId::MenuMove, cdDirectory); }
                        else if (cursor == 3) { settings.minGameLength = std::max(1, settings.minGameLength - 1); SfxPlayer::Play(SfxId::MenuMove, cdDirectory); }
                        break;
                    case SDLK_RIGHT:
                        if (cursor == 2) { settings.startLevel = std::min(10, settings.startLevel + 1); SfxPlayer::Play(SfxId::MenuMove, cdDirectory); }
                        else if (cursor == 3) { settings.minGameLength = std::min(99, settings.minGameLength + 1); SfxPlayer::Play(SfxId::MenuMove, cdDirectory); }
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        if (cursor == 0) { result.startedGame = true; running = false; }
                        else if (cursor == 1) { editor.BeginEdit(settings.gameName, 24); }
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (!running) { break; }

            DrawMenuBackground(renderer, background, bgW, bgH);
            DrawNetworkedComputersModel(renderer, cdDirectory);
            int scale = 3;
            int lineHeight = TextHeight(scale) + scale * 6;
            int margin = scale * 8;
            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            std::string title = "GAME SETUP";
            DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, margin, scale, COLOR_CURSOR);

            int rowY = margin + lineHeight * 4;
            DrawBitmapText(renderer, "Start Game", margin, rowY, scale, (cursor == 0) ? SelectedTextColor() : COLOR_DIM);

            std::string gameNameValue = (cursor == 1 && editor.IsEditing()) ? editor.Value() : settings.gameName;
            DrawBitmapText(renderer, "Name Of Game: " + gameNameValue, margin, rowY + lineHeight, scale,
                            (cursor == 1) ? (editor.IsEditing() ? COLOR_EDITING : SelectedTextColor()) : COLOR_DIM);

            DrawBitmapText(renderer, "Start at level (1-10): " + std::to_string(settings.startLevel), margin, rowY + lineHeight * 2, scale,
                            (cursor == 2) ? SelectedTextColor() : COLOR_DIM);

            DrawBitmapText(renderer, "Minimum game length: " + std::to_string(settings.minGameLength), margin, rowY + lineHeight * 3, scale,
                            (cursor == 3) ? SelectedTextColor() : COLOR_DIM);

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }

    MultiplayerSubResult MultiplayerJoinScreen::Run(const std::filesystem::path& cdDirectory)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { return { true }; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        MultiplayerSubResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                    SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                }
            }
            if (!running) { break; }

            DrawMenuBackground(renderer, background, bgW, bgH);
            DrawNetworkedComputersModel(renderer, cdDirectory);
            int scale = 3;
            int lineHeight = TextHeight(scale) + scale * 6;
            int margin = scale * 8;
            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            std::string title = "SEARCHING FOR NET GAMES....";
            DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, margin, scale, COLOR_CURSOR);

            // No real network discovery exists - always empty. Matches
            // the reference exactly (5 numbered, unfilled slots).
            for (int i = 0; i < 5; ++i)
            {
                std::string line = std::to_string(i + 1) + ":";
                DrawBitmapText(renderer, line, margin, margin + lineHeight * 3 + i * lineHeight, scale, COLOR_DIM);
            }

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }
}
