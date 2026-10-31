#include "MultiplayerScreens.h"
#include "MenuBackground.h"
#include "TextFieldEditor.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Menu/MenuTree.h" // for ModelIndex::NetworkedComputers
#include "../Renderer/ModelRenderer.h"
#include "../Renderer/ModelPreview.h"
#include "../Net/Session.h"
#include "../Formats/EnemyAnimData.h"
#include "../Formats/EnemySpriteSet.h"

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
    using ALTEngine::Bootstrap::ComputeMenuScale;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::LerpColor;
    using ALTEngine::Bootstrap::PulsePhase;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;

    namespace
    {
        constexpr Color COLOR_CURSOR{ 51, 255, 102, 255 };
        constexpr Color COLOR_DIM{ 24, 130, 52, 255 };
        constexpr Color COLOR_EDITING{ 235, 235, 235, 255 };
        constexpr Color COLOR_WHITE{ 255, 255, 255, 255 }; // matches the main menu's own selected-item white exactly

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

    MultiplayerMainResult MultiplayerMainScreen::Run(const std::filesystem::path& cdDirectory, int& cursor)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { return { true, MultiplayerMainChoice::Back }; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        constexpr std::array<const char*, 3> ITEMS{ "Start Multiplayer Game", "Join Multiplayer Game", "Multiplayer Options" };
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
            int scale = ComputeMenuScale(renderer);
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

    namespace
    {
        // The chosen look, turning: the creature's eight walking views
        // (animations 0-7, frame 0), one after another. A type that mirrors
        // (all but 1, 10 and 13 - FUN_000314b8) draws views 5-7 as 3-1 flipped.
        class SkinPreview
        {
        public:
            ~SkinPreview() { Clear(); }
            void Load(const std::filesystem::path& cdDirectory, SDL_Renderer* renderer, int creatureType)
            {
                if (creatureType == loadedType) { return; }
                Clear();
                loadedType = creatureType;
                namespace AD = ALTEngine::Formats::EnemyAnimData;
                const AD::Creature* creature = AD::ForType(creatureType);
                if (!creature) { return; }
                ALTEngine::Formats::EnemySpriteSet set;
                if (!set.Load(cdDirectory / "NME" / (std::string(creature->file) + ".B16"))) { return; }
                mirrors = creatureType != 1 && creatureType != 10 && creatureType != 13;
                for (int view = 0; view < 8 && view < creature->count; ++view)
                {
                    const AD::Anim& anim = creature->anims[view];
                    if (anim.count <= 0) { continue; }
                    ALTEngine::Formats::EnemySpriteSet::Frame f;
                    f.offsetX = anim.frames[0].ox; f.offsetY = anim.frames[0].oy;
                    f.width = anim.frames[0].w; f.height = anim.frames[0].h;
                    f.compressedOffset = anim.frames[0].off;
                    auto decoded = set.DecodeAnimation({ f });
                    if (decoded.empty() || decoded[0].width <= 0 || decoded[0].height <= 0) { continue; }
                    const auto& d = decoded[0];
                    std::vector<uint8_t> rgba(static_cast<size_t>(d.width) * d.height * 4, 0);
                    const auto& pal = set.Palette();
                    for (size_t i = 0; i < d.pixels.size() && i * 4 + 3 < rgba.size(); ++i)
                    {
                        const size_t p = static_cast<size_t>(d.pixels[i]) * 2;
                        if (d.pixels[i] == 0 || p + 1 >= pal.size()) { continue; }
                        const uint16_t e = static_cast<uint16_t>(pal[p] | (pal[p + 1] << 8));
                        rgba[i * 4 + 0] = static_cast<uint8_t>((e & 0x1f) << 3);
                        rgba[i * 4 + 1] = static_cast<uint8_t>(((e >> 5) & 0x1f) << 3);
                        rgba[i * 4 + 2] = static_cast<uint8_t>(((e >> 10) & 0x1f) << 3);
                        rgba[i * 4 + 3] = 255;
                    }
                    SDL_Texture* t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, d.width, d.height);
                    if (!t) { continue; }
                    SDL_UpdateTexture(t, nullptr, rgba.data(), d.width * 4);
                    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
                    views[static_cast<size_t>(view)] = { t, d.width, d.height };
                }
            }
            // Turning once every two seconds or so, centred on (cx, bottom).
            void Draw(SDL_Renderer* renderer, float cx, float bottom, float scale) const
            {
                const int step = static_cast<int>(SDL_GetTicks() / 250) & 7;
                int view = step;
                bool flip = false;
                if (mirrors && view >= 5) { view = 8 - view; flip = true; }
                const View& v = views[static_cast<size_t>(view)];
                if (!v.texture) { return; }
                SDL_FRect dst{ cx - v.w * scale * 0.5f, bottom - v.h * scale, v.w * scale, v.h * scale };
                SDL_RenderTextureRotated(renderer, v.texture, nullptr, &dst, 0.0, nullptr,
                                         flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            }

        private:
            struct View { SDL_Texture* texture = nullptr; int w = 0, h = 0; };
            void Clear()
            {
                for (auto& v : views) { if (v.texture) { SDL_DestroyTexture(v.texture); } v = {}; }
                loadedType = -1;
            }
            std::array<View, 8> views{};
            int loadedType = -1;
            bool mirrors = false;
        };
    }

    MultiplayerSubResult MultiplayerOptionsScreen::Run(const std::filesystem::path& cdDirectory, MultiplayerSettings& settings)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { return { true }; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        int cursor = 0; // 0 = name, 1-8 = F2-F9 messages, 9 = how the others see you
        constexpr int ROWS = 10;
        constexpr int SKIN_ROW = 9;
        TextFieldEditor editor;
        SkinPreview preview;
        const int skinCount = static_cast<int>(ALTEngine::Net::Skin::Count);
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
                        cursor = (cursor + ROWS - 1) % ROWS;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_DOWN:
                        cursor = (cursor + 1) % ROWS;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_LEFT:
                    case SDLK_RIGHT:
                        if (cursor == SKIN_ROW)
                        {
                            const int delta = event.key.key == SDLK_LEFT ? skinCount - 1 : 1;
                            settings.skin = (settings.skin + delta) % skinCount;
                            SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        }
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    {
                        if (cursor == SKIN_ROW)
                        {
                            settings.skin = (settings.skin + 1) % skinCount;
                            SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                            break;
                        }
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
            int scale = ComputeMenuScale(renderer);
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

            // How the others see you, turning on the right.
            const auto skin = static_cast<ALTEngine::Net::Skin>(std::clamp(settings.skin, 0, skinCount - 1));
            const std::string skinLine = std::string("PLAYER: < ") + ALTEngine::Net::SkinName(skin) + " >";
            DrawBitmapText(renderer, skinLine, margin, rowY + lineHeight * (SKIN_ROW + 2), scale,
                           cursor == SKIN_ROW ? SelectedTextColor() : COLOR_DIM);
            preview.Load(cdDirectory, renderer, ALTEngine::Net::SkinCreatureType(skin));
            preview.Draw(renderer, windowW * 0.8f, windowH * 0.8f, static_cast<float>(scale) * 1.5f);

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
            int scale = ComputeMenuScale(renderer);
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

    MultiplayerJoinResult MultiplayerJoinScreen::Run(const std::filesystem::path& cdDirectory, std::string& typedAddress)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { MultiplayerJoinResult closed; closed.windowClosed = true; return closed; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        ALTEngine::Net::GameBrowser browser;
        browser.Start();
        constexpr int SLOTS = 5;
        constexpr int ADDRESS_ROW = SLOTS;   // the line after the five games
        int cursor = 0;
        TextFieldEditor editor;

        MultiplayerJoinResult result;
        bool running = true;
        while (running)
        {
            const double now = static_cast<double>(SDL_GetTicks()) / 1000.0;
            browser.Update(now);
            const auto& games = browser.Games();

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; break; }
                if (editor.IsEditing())
                {
                    editor.HandleEvent(event);
                    if (!editor.IsEditing()) { typedAddress = editor.Value(); }
                    continue;
                }
                if (event.type != SDL_EVENT_KEY_DOWN) { continue; }
                switch (event.key.key)
                {
                case SDLK_ESCAPE:
                    running = false;
                    SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                    break;
                case SDLK_UP:
                    cursor = (cursor + ADDRESS_ROW) % (ADDRESS_ROW + 1);
                    SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                    break;
                case SDLK_DOWN:
                    cursor = (cursor + 1) % (ADDRESS_ROW + 1);
                    SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (cursor < static_cast<int>(games.size()))
                    {
                        result.chosen = true;
                        result.address = games[static_cast<size_t>(cursor)].address.ToString();
                        running = false;
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                    }
                    else if (cursor == ADDRESS_ROW)
                    {
                        if (!typedAddress.empty())
                        {
                            result.chosen = true;
                            result.address = typedAddress;
                            running = false;
                            SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        }
                        else
                        {
                            editor.BeginEdit(typedAddress, 40);
                        }
                    }
                    break;
                case SDLK_BACKSPACE:
                case SDLK_E:
                    // Edit the address again.
                    if (cursor == ADDRESS_ROW) { editor.BeginEdit(typedAddress, 40); }
                    break;
                default:
                    break;
                }
            }
            if (!running) { break; }

            DrawMenuBackground(renderer, background, bgW, bgH);
            DrawNetworkedComputersModel(renderer, cdDirectory);
            int scale = ComputeMenuScale(renderer);
            int lineHeight = TextHeight(scale) + scale * 6;
            int margin = scale * 8;
            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            std::string title = "SEARCHING FOR NET GAMES....";
            DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, margin, scale, COLOR_CURSOR);

            for (int i = 0; i < SLOTS; ++i)
            {
                std::string line = std::to_string(i + 1) + ":";
                if (i < static_cast<int>(games.size()))
                {
                    const auto& g = games[static_cast<size_t>(i)];
                    line += " " + g.info.gameName + "  LEVEL " + g.info.level + "  "
                          + std::to_string(g.info.players) + "/" + std::to_string(g.info.maxPlayers);
                }
                const Color colour = (i == cursor) ? COLOR_WHITE : (i < static_cast<int>(games.size()) ? COLOR_CURSOR : COLOR_DIM);
                DrawBitmapText(renderer, line, margin, margin + lineHeight * 3 + i * lineHeight, scale, colour);
            }
            const std::string addressLine = "ADDRESS: " + (editor.IsEditing() ? editor.Value() + "_" : typedAddress);
            DrawBitmapText(renderer, addressLine, margin, margin + lineHeight * 3 + (SLOTS + 1) * lineHeight, scale,
                           editor.IsEditing() ? COLOR_EDITING : (cursor == ADDRESS_ROW ? COLOR_WHITE : COLOR_DIM));

            SDL_RenderPresent(renderer);
            SDL_Delay(10);
        }

        browser.Stop();
        if (background) { SDL_DestroyTexture(background); }
        return result;
    }
}
