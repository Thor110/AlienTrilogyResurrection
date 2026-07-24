#include "PauseMenuScreen.h"
#include "PauseMenuTree.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/MissionText.h"
#include "../Menu/MenuNavigation.h"
#include "../Renderer/ModelRenderer.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <optional>
#include <sstream>

namespace ALTEngine::Screens
{
    using ALTEngine::Audio::SfxId;
    using ALTEngine::Audio::SfxPlayer;
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::Language;
    using ALTEngine::Bootstrap::MissionTextFilenameCandidates;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;
    using ALTEngine::Formats::BriefingSegment;
    using ALTEngine::Formats::MissionBriefing;
    using ALTEngine::Formats::MissionTextLoader;
    using ALTEngine::Menu::EffectiveModelIndex;
    using ALTEngine::Menu::Enter;
    using ALTEngine::Menu::EnterResult;
    using ALTEngine::Menu::MenuNode;
    using ALTEngine::Menu::MenuNodeKind;
    using ALTEngine::Menu::MoveSelection;
    using ALTEngine::Menu::WalkPath;

    namespace
    {
        constexpr Color COLOR_DIM{ 24, 130, 52, 255 };       // ordinary items - matches the established dim-green
        constexpr Color COLOR_CURSOR{ 51, 255, 102, 255 };   // whichever item the cursor is on (not equipped)
        constexpr Color COLOR_EQUIPPED{ 235, 235, 235, 255 }; // the equipped weapon's label - bright/white, always, regardless of cursor
        constexpr Color COLOR_HIGHLIGHT_BG{ 0, 40, 15, 255 };
        constexpr Color COLOR_STATUS{ 51, 255, 102, 255 };   // "Selected" / "Not available" / etc

        struct WeaponInfo
        {
            const WeaponState* state;
            int weaponModel;
            int ammoModel;
        };

        // Maps a top-level item's label to its WeaponState, if it's a
        // weapon - nullptr for non-weapon items (Auto Mapper, Batteries,
        // Mission, Options, etc).
        std::optional<WeaponInfo> WeaponInfoFor(const std::string& label, const PlayerInventoryState& inv, const MenuNode& node)
        {
            const WeaponState* state = nullptr;
            if (label == "9mm Pistol") { state = &inv.pistol; }
            else if (label == "Shotgun") { state = &inv.shotgun; }
            else if (label == "Flamethrower") { state = &inv.flamethrower; }
            else if (label == "Pulse Rifle") { state = &inv.pulseRifle; }
            else if (label == "Smart Gun") { state = &inv.smartGun; }
            if (!state) { return std::nullopt; }
            return WeaponInfo{ state, node.modelIndex, node.secondaryModelIndex };
        }

        void DrawLeftColumn(SDL_Renderer* renderer, const std::vector<MenuNode>& items, int cursorIndex,
                             const PlayerInventoryState& inventory, int x, int y, int rowHeight, int scale, int& outWidth)
        {
            int width = 0;
            for (const auto& item : items) { width = std::max(width, TextWidth(item.label, scale)); }
            width += scale * 8;
            outWidth = width;

            for (size_t i = 0; i < items.size(); ++i)
            {
                int rowY = y + static_cast<int>(i) * rowHeight;
                bool isCursor = (static_cast<int>(i) == cursorIndex);
                bool isEquipped = false;
                if (auto info = WeaponInfoFor(items[i].label, inventory, items[i])) { isEquipped = info->state->equipped; }

                if (isCursor)
                {
                    SDL_SetRenderDrawColor(renderer, COLOR_HIGHLIGHT_BG.r, COLOR_HIGHLIGHT_BG.g, COLOR_HIGHLIGHT_BG.b, 255);
                    SDL_FRect bar{ static_cast<float>(x), static_cast<float>(rowY), static_cast<float>(width), static_cast<float>(rowHeight - 4) };
                    SDL_RenderFillRect(renderer, &bar);
                }

                Color color = isEquipped ? COLOR_EQUIPPED : (isCursor ? COLOR_CURSOR : COLOR_DIM);
                DrawBitmapText(renderer, items[i].label, x + scale * 4, rowY + (rowHeight - TextHeight(scale)) / 2, scale, color);
            }
        }

        // Placeholder for the eventual real spinning model, same pattern
        // established for the boot menu's Options screen (see
        // MenuController.cpp) - a labeled box, not yet wired to
        // ModelRenderer since PICKMOD.BND/PICKGFX.BND aren't available
        // yet (matches PICKMOD's ModelIndices.h caveat).
        void DrawModelPlaceholder(SDL_Renderer* renderer, int modelIndex, int x, int y, int w, int h, int scale)
        {
            if (modelIndex < 0) { return; }
            SDL_SetRenderDrawColor(renderer, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, 255);
            SDL_FRect box{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h) };
            SDL_RenderRect(renderer, &box);
            std::string label = "[PICKMOD #" + std::to_string(modelIndex) + "]";
            int textX = x + (w - TextWidth(label, scale)) / 2;
            int textY = y + h / 2 - TextHeight(scale) / 2;
            DrawBitmapText(renderer, label, textX, textY, scale, COLOR_DIM);
        }

        // Tracks whether ModelRenderer::Initialize() has been attempted -
        // same lazy-init-once pattern as MenuController's boot menu.
        bool modelRendererInitAttempted = false;
        bool modelRendererAvailable = false;

        // Renders a real PICKMOD model (via ModelRenderer, using the
        // shared PICKGFX texture scheme), falling back to
        // DrawModelPlaceholder if the GPU pipeline isn't available or
        // this specific model fails to load - the menu stays fully
        // usable either way, same resilience as the boot menu's models.
        void DrawPickModModel(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory, int modelIndex,
                               int x, int y, int w, int h, int scale, float rotationAngle)
        {
            if (modelIndex < 0) { return; }

            if (!modelRendererInitAttempted)
            {
                modelRendererInitAttempted = true;
                modelRendererAvailable = ALTEngine::Renderer::ModelRenderer::Initialize();
                if (!modelRendererAvailable)
                {
                    SDL_Log("PauseMenuScreen: ModelRenderer unavailable - using placeholder boxes instead of live 3D previews");
                }
            }

            if (!modelRendererAvailable)
            {
                DrawModelPlaceholder(renderer, modelIndex, x, y, w, h, scale);
                return;
            }

            std::filesystem::path objBnd = cdDirectory / "GFX" / "PICKMOD.BND";
            std::filesystem::path gfxBnd = cdDirectory / "GFX" / "PICKGFX.BND";
            std::string cacheKey = "PICKMOD:" + std::to_string(modelIndex);

            if (!ALTEngine::Renderer::ModelRenderer::LoadModel(cacheKey, modelIndex, objBnd, gfxBnd))
            {
                DrawModelPlaceholder(renderer, modelIndex, x, y, w, h, scale);
                return;
            }

            int renderSize = std::min(w, h);
            if (renderSize < 64) { renderSize = 64; }
            std::vector<uint8_t> pixels = ALTEngine::Renderer::ModelRenderer::RenderToRgba(cacheKey, rotationAngle, renderSize, renderSize);
            if (pixels.empty())
            {
                DrawModelPlaceholder(renderer, modelIndex, x, y, w, h, scale);
                return;
            }

            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, renderSize, renderSize);
            if (!texture)
            {
                DrawModelPlaceholder(renderer, modelIndex, x, y, w, h, scale);
                return;
            }
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_UpdateTexture(texture, nullptr, pixels.data(), renderSize * 4);

            SDL_FRect dest{ static_cast<float>(x + (w - renderSize) / 2), static_cast<float>(y + (h - renderSize) / 2),
                            static_cast<float>(renderSize), static_cast<float>(renderSize) };
            SDL_RenderTexture(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
        }

        void DrawSlider(SDL_Renderer* renderer, const std::string& label, int value, int x, int y, int scale)
        {
            DrawBitmapText(renderer, label, x, y, scale, COLOR_CURSOR);
            int barY = y + TextHeight(scale) + scale * 2;
            int cellSize = scale * 4;
            for (int i = 0; i < 10; ++i)
            {
                Color c = (i < value) ? COLOR_CURSOR : COLOR_DIM;
                SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                SDL_FRect cell{ static_cast<float>(x + i * (cellSize + scale)), static_cast<float>(barY), static_cast<float>(cellSize), static_cast<float>(cellSize) };
                SDL_RenderFillRect(renderer, &cell);
            }
        }

        // Word-wraps a briefing's flattened text to fit `maxWidth` pixels
        // - unlike the full-screen MissionBriefingScreen (which keeps the
        // original line breaks, since that layout is exactly as wide as
        // the source text was hand-wrapped for), this panel is narrower
        // and needs its own wrapping.
        struct WrappedSegment { Color color; std::string text; bool lineBreakAfter; };

        std::vector<WrappedSegment> WrapBriefing(const MissionBriefing& briefing, int maxWidth, int scale)
        {
            std::vector<WrappedSegment> result;
            for (size_t p = 0; p < briefing.paragraphs.size(); ++p)
            {
                if (p > 0) { result.push_back({ COLOR_DIM, "", true }); } // blank line between paragraphs

                int lineWidth = 0;
                for (const auto& line : briefing.paragraphs[p].lines)
                {
                    for (const auto& segment : line.segments)
                    {
                        std::istringstream words(segment.text);
                        std::string word;
                        while (words >> word)
                        {
                            Color color = segment.bright ? COLOR_CURSOR : COLOR_DIM;
                            int wordWidth = TextWidth(word + " ", scale);
                            if (lineWidth + wordWidth > maxWidth && lineWidth > 0)
                            {
                                result.push_back({ color, "", true });
                                lineWidth = 0;
                            }
                            result.push_back({ color, word + " ", false });
                            lineWidth += wordWidth;
                        }
                    }
                    // original source line break -> also a wrapped line
                    // break, so words never merge across the source's own
                    // paragraph structure
                    result.push_back({ COLOR_DIM, "", true });
                    lineWidth = 0;
                }
            }
            return result;
        }

        void DrawWrappedBriefing(SDL_Renderer* renderer, const std::vector<WrappedSegment>& wrapped, int x, int y, int scale, int lineHeight)
        {
            int cursorX = x, cursorY = y;
            for (const auto& seg : wrapped)
            {
                if (!seg.text.empty())
                {
                    DrawBitmapText(renderer, seg.text, cursorX, cursorY, scale, seg.color);
                    cursorX += TextWidth(seg.text, scale);
                }
                if (seg.lineBreakAfter) { cursorX = x; cursorY += lineHeight; }
            }
        }
    }

    PauseMenuResult PauseMenuScreen::Run(
        const std::filesystem::path& cdDirectory,
        Language language,
        const std::string& missionLevelCode,
        const PlayerInventoryState& inventory)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { PauseMenuOutcome::WindowClosed };
        }
        SDL_Renderer* renderer = app.Renderer();

        // Mission text loaded once up front (not re-loaded every frame) -
        // reuses the same resolver logic as MissionBriefingScreen
        // (MISSIONU-then-MISSIONE fallback for English).
        std::vector<MissionBriefing> allBriefings;
        const MissionBriefing* missionBriefing = nullptr;
        for (const auto& candidate : MissionTextFilenameCandidates(language))
        {
            std::filesystem::path path = cdDirectory / "LANGUAGE" / (candidate + ".TXT");
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) { continue; }
            try
            {
                allBriefings = MissionTextLoader::Load(path);
                for (const auto& b : allBriefings)
                {
                    if (b.levelCode == missionLevelCode) { missionBriefing = &b; break; }
                }
            }
            catch (const std::exception& e)
            {
                SDL_Log("PauseMenuScreen: failed to load mission text: %s", e.what());
            }
            break;
        }

        MenuNode root = BuildPauseMenuTree();
        std::vector<int> path = { 0 };

        PauseMenuResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.outcome = PauseMenuOutcome::WindowClosed; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    switch (event.key.key)
                    {
                    case SDLK_UP:
                        MoveSelection(root, path, -1);
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_DOWN:
                        MoveSelection(root, path, 1);
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    {
                        const MenuNode& deepest = WalkPath(root, path);
                        std::vector<int> parentPath(path.begin(), path.end() - 1);
                        std::string parentLabel = WalkPath(root, parentPath).label;

                        EnterResult r = Enter(root, path);
                        if (r == EnterResult::Toggled && parentLabel == "Exit Game" && deepest.label == "Yes")
                        {
                            result.outcome = PauseMenuOutcome::ExitGame;
                            running = false;
                        }
                        if (r != EnterResult::NoOp) { SfxPlayer::Play(SfxId::MenuSelect, cdDirectory); }
                        break;
                    }
                    case SDLK_ESCAPE:
                        if (!ALTEngine::Menu::Back(path)) { running = false; } // backed out of the top level - resume
                        SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (!running) { break; }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            float rotationAngle = static_cast<float>(SDL_GetTicks()) / 1000.0f; // 1 radian/sec, same slow spin as the boot menu

            int scale = 3;
            int rowHeight = TextHeight(scale) + scale * 6;
            int margin = scale * 8;

            int leftWidth = 0;
            DrawLeftColumn(renderer, root.children, path[0], inventory, margin, margin, rowHeight, scale, leftWidth);

            int panelX = margin + leftWidth + scale * 8;
            int panelY = margin;
            const std::string& topLabel = root.children[static_cast<size_t>(path[0])].label;

            if (topLabel == "Options")
            {
                const MenuNode& optionsNode = root.children[static_cast<size_t>(path[0])];
                DrawSlider(renderer, "SFX VOLUME", optionsNode.children[0].sliderValue, panelX, panelY, scale);
                DrawSlider(renderer, "MUSIC VOLUME", optionsNode.children[1].sliderValue, panelX, panelY + rowHeight * 2, scale);

                bool exitCursor = (path.size() >= 2 && path[1] == 2);
                Color exitColor = exitCursor ? COLOR_CURSOR : COLOR_DIM;
                int exitY = panelY + rowHeight * 4;
                DrawBitmapText(renderer, "EXIT GAME", panelX, exitY, scale, exitColor);

                if (path.size() >= 2 && path[1] == 2) // Exit Game is the active/previewed column
                {
                    int confirmIndex = (path.size() >= 3) ? path[2] : 0;
                    std::string confirmLabel = confirmIndex == 1 ? "Yes" : "No";
                    Color confirmColor = confirmIndex == 1 ? COLOR_CURSOR : COLOR_DIM;
                    std::string prefix = "ARE YOU SURE ? ";
                    DrawBitmapText(renderer, prefix, panelX + TextWidth("EXIT GAME", scale) + scale * 8, exitY, scale, COLOR_DIM);
                    DrawBitmapText(renderer, confirmLabel, panelX + TextWidth("EXIT GAME", scale) + scale * 8 + TextWidth(prefix, scale), exitY, scale, confirmColor);
                }
            }
            else if (topLabel == "Mission")
            {
                if (missionBriefing)
                {
                    int panelWidth = 500; // TODO: derive from actual window width once layout gets its polish pass
                    auto wrapped = WrapBriefing(*missionBriefing, panelWidth, scale);
                    DrawWrappedBriefing(renderer, wrapped, panelX, panelY, scale, TextHeight(scale) + scale * 4);
                }
                else
                {
                    DrawBitmapText(renderer, "Not available", panelX, panelY, scale, COLOR_STATUS);
                }
            }
            else if (auto weaponInfo = WeaponInfoFor(topLabel, inventory, root.children[static_cast<size_t>(path[0])]))
            {
                std::string ammoText = weaponInfo->state->available
                    ? (std::to_string(weaponInfo->state->ammo) + " rounds available")
                    : "No ammo available";
                DrawBitmapText(renderer, ammoText, panelX, panelY, scale, COLOR_STATUS);

                DrawPickModModel(renderer, cdDirectory, weaponInfo->ammoModel, panelX, panelY + rowHeight * 2, 200, 150, scale, rotationAngle);
                DrawPickModModel(renderer, cdDirectory, weaponInfo->weaponModel, panelX, panelY + rowHeight * 2 + 170, 260, 150, scale, rotationAngle);

                std::string statusText = weaponInfo->state->equipped ? "Selected" : "Not available";
                DrawBitmapText(renderer, statusText, panelX, panelY + rowHeight * 2 + 340, scale, COLOR_STATUS);
            }
            else // Auto Mapper / Shoulder Lamp / Batteries - single model, no ammo
            {
                const MenuNode& node = root.children[static_cast<size_t>(path[0])];
                bool owned = (topLabel == "Auto Mapper" && inventory.hasAutoMapper) ||
                             (topLabel == "Shoulder Lamp" && inventory.hasShoulderLamp) ||
                             (topLabel == "Batteries" && inventory.hasBatteries);
                DrawPickModModel(renderer, cdDirectory, node.modelIndex, panelX, panelY, 260, 200, scale, rotationAngle);
                if (!owned) { DrawBitmapText(renderer, "Not available", panelX, panelY + 220, scale, COLOR_STATUS); }
            }

            SDL_RenderPresent(renderer);
        }

        return result;
    }
}
