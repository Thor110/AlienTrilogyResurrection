#include "PauseMenuScreen.h"

#include "../Audio/MusicPlayer.h"
#include "../Renderer/Minimap.h"
#include "../Bootstrap/Config.h"
#include "../Bootstrap/ModernSettings.h"
#include "PauseMenuTree.h"
#include "SaveSlotScreen.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/MissionText.h"
#include "../Menu/MenuController.h"
#include "../Menu/MenuNavigation.h"
#include "../Renderer/ModelRenderer.h"
#include "../Renderer/ModelPreview.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>

namespace ALTEngine::Screens
{
    using ALTEngine::Audio::SfxId;
    using ALTEngine::Audio::SfxPlayer;
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::AudioSettings;
    using ALTEngine::Bootstrap::CameraSwaySettings;
    using ALTEngine::Bootstrap::ComputeMenuScale;
    using ALTEngine::Bootstrap::DifficultySettings;
    using ALTEngine::Bootstrap::KeyBindings;
    using ALTEngine::Bootstrap::LanguageSettings;
    using ALTEngine::Bootstrap::LerpColor;
    using ALTEngine::Bootstrap::PulsePhase;
    using ALTEngine::Bootstrap::RenderSettings;
    using ALTEngine::Bootstrap::ResolutionSettings;
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
    using ALTEngine::Menu::MenuController;
    using ALTEngine::Menu::MenuNode;
    using ALTEngine::Menu::MenuNodeKind;
    using ALTEngine::Menu::MoveSelection;
    using ALTEngine::Menu::WalkPath;

    namespace
    {
        constexpr Color COLOR_DIM{ 24, 130, 52, 255 };       // ordinary items - matches the established dim-green
        constexpr Color COLOR_CURSOR{ 51, 255, 102, 255 };   // whichever item the cursor is on (not equipped)
        constexpr Color COLOR_EQUIPPED{ 235, 235, 235, 255 }; // the equipped weapon's label - bright/white, always, regardless of cursor
        constexpr Color COLOR_HIGHLIGHT_BG{ 0, 40, 15, 255 };         // very dark green - every row's default box now (Edward, 2026), not just the cursor's
        constexpr Color COLOR_HIGHLIGHT_BG_LIGHT{ 20, 130, 60, 255 }; // light green - pulse target for the cursor's row
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
                             const PlayerInventoryState& inventory, int x, int y, int rowHeight, int scale, int& outWidth, Language language)
        {
            int width = 0;
            for (const auto& item : items) { width = std::max(width, TextWidth(DisplayLabel(item, language), scale)); }
            width += scale * 8;
            outWidth = width;

            float pulse = PulsePhase();

            for (size_t i = 0; i < items.size(); ++i)
            {
                int rowY = y + static_cast<int>(i) * rowHeight;
                bool isCursor = (static_cast<int>(i) == cursorIndex);
                bool isEquipped = false;
                if (auto info = WeaponInfoFor(items[i].label, inventory, items[i])) { isEquipped = info->state->equipped; }

                // Every row gets a box now (Edward, 2026: "a disabled
                // version of the current highlight around all options
                // and pause menu items") - very dark green by default,
                // brighter and pulsing for the cursor's row regardless
                // of whether that item is enabled, matching the boot
                // menu's own Controls list (Edward, 2026: "a matching
                // pulsing highlight, just leaving the darker text in
                // place") - currently a no-op here since no pause-menu
                // item is ever actually disabled, but keeps this
                // consistent with DrawColumn in case one is later.
                Color boxColor = COLOR_HIGHLIGHT_BG;
                if (isCursor) { boxColor = LerpColor(COLOR_HIGHLIGHT_BG, COLOR_HIGHLIGHT_BG_LIGHT, pulse); }

                SDL_FRect bar{ static_cast<float>(x), static_cast<float>(rowY), static_cast<float>(width), static_cast<float>(rowHeight - scale * 2) };
                DrawRoundedRect(renderer, bar, static_cast<float>(scale * 2), boxColor);

                Color color = isEquipped ? COLOR_EQUIPPED : (isCursor ? COLOR_CURSOR : COLOR_DIM);
                DrawBitmapText(renderer, DisplayLabel(items[i], language), x + scale * 4, rowY + (rowHeight - TextHeight(scale)) / 2, scale, color);
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

        // Renders a real PICKMOD model (via ModelRenderer, using the
        // shared PICKGFX texture scheme), falling back to
        // DrawModelPlaceholder if the GPU pipeline isn't available or
        // this specific model fails to load - the menu stays fully
        // usable either way, same resilience as the boot menu's models.
        void DrawPickModModel(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory, int modelIndex,
                               int x, int y, int w, int h, int scale, float rotationAngle)
        {
            if (modelIndex < 0) { return; }

            ALTEngine::Renderer::ModelPreviewSource source = ALTEngine::Renderer::ModelPreviewSource::ForPickmod(cdDirectory, modelIndex);

            if (!ALTEngine::Renderer::DrawModelPreview(renderer, source, x, y, w, h, rotationAngle))
            {
                DrawModelPlaceholder(renderer, modelIndex, x, y, w, h, scale);
            }
        }

        // Same as DrawPickModModel above, but for the pause menu's two
        // OPTOBJ entries (Save Game/Load Game's Harddrive Left/Right
        // models) - same PICKGFX/OPTGFX distinction SaveSlotScreen and
        // MenuController's Options screen already draw from (Edward,
        // 2026).
        void DrawOptobjModel(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory, int modelIndex,
                              int x, int y, int w, int h, int scale, float rotationAngle)
        {
            if (modelIndex < 0) { return; }

            ALTEngine::Renderer::ModelPreviewSource source = ALTEngine::Renderer::ModelPreviewSource::ForOptobj(cdDirectory, modelIndex);

            if (!ALTEngine::Renderer::DrawModelPreview(renderer, source, x, y, w, h, rotationAngle))
            {
                DrawModelPlaceholder(renderer, modelIndex, x, y, w, h, scale);
            }
        }

        // barX is explicit (not derived from label's own width) so
        // SFX/Music's bars align at the same X regardless of their
        // different label lengths, and sit in their own space to the
        // right of the button's highlight box rather than overlapping
        // it (Edward, 2026: "the sliders should actually be in their own
        // space to the right of the Music/SFX volume buttons... not
        // within the highlight around the text itself").

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
        Language& language,
        const std::string& missionLevelCode,
        const PlayerInventoryState& inventory,
        AudioSettings& audioSettings,
        RenderSettings& renderSettings,
        ResolutionSettings& resolutionSettings,
        DifficultySettings& difficultySettings,
        CameraSwaySettings& cameraSwaySettings,
        LanguageSettings& languageSettings,
        KeyBindings& keyBindings,
        const ALTEngine::Formats::LevelGeometry* level,
        float playerGridX,
        float playerGridZ,
        float playerYaw,
        const std::vector<uint8_t>* minimapVisited)
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

        MenuNode root = BuildPauseMenuTree(language);

        // Cheats are opt-in. The list is built into the tree unconditionally and
        // added or removed here, so the state is re-evaluated whenever this runs
        // - both when the pause menu opens and again after returning from the
        // Options menu, since Enable Cheats can be flipped in there (Edward,
        // 2026).
        MenuNode cheatsTemplate;
        for (const MenuNode& child : root.children)
        {
            if (child.label == "Cheats") { cheatsTemplate = child; break; }
        }

        auto syncCheatsEntry = [&root, &cheatsTemplate]() {
            ALTEngine::Bootstrap::Config cheatConfig;
            ALTEngine::Bootstrap::ModernSettings cheatModern(cheatConfig);
            bool wanted = cheatModern.IsActive(ALTEngine::Bootstrap::ModernFeature::EnableCheats);

            int at = -1;
            for (size_t i = 0; i < root.children.size(); ++i)
            {
                if (root.children[i].label == "Cheats") { at = static_cast<int>(i); break; }
            }

            if (!wanted)
            {
                if (at >= 0) { root.children.erase(root.children.begin() + at); }
                return;
            }

            if (at < 0)
            {
                // Reinsert just before Exit Game, which is where it belongs.
                size_t insertAt = root.children.size();
                for (size_t i = 0; i < root.children.size(); ++i)
                {
                    if (root.children[i].label == "Exit Game") { insertAt = i; break; }
                }
                root.children.insert(root.children.begin() + static_cast<long>(insertAt), cheatsTemplate);
                at = static_cast<int>(insertAt);
            }

            // Refresh each cheat's shown state from config.
            MenuNode& cheats = root.children[static_cast<size_t>(at)];
            for (MenuNode& cheat : cheats.children)
            {
                if (cheat.label != "Fully Loaded") { continue; }
                auto stored = cheatConfig.Get("CheatFullyLoaded");
                bool on = stored.has_value() && *stored == "On";
                cheat.initialSelectedChild = on ? 1 : 0;
                cheat.descriptionStringId = static_cast<int>(ALTEngine::Bootstrap::StringId::DescFullyLoaded);
            }
        };
        syncCheatsEntry();

        std::vector<int> path = { 0 };

        PauseMenuResult result;
        bool running = true;

        while (running)
        {
            // Keep the level music fed while paused. PlayLooped only starts the
            // stream; Update() pushes more audio into it. This loop blocks the
            // gameplay loop that normally does it, so without this the music
            // starves a fraction of a second after the pause menu opens - which
            // is what made it "sometimes" stop, depending on how much was
            // already buffered when you paused (Edward, 2026).
            ALTEngine::Audio::MusicPlayer::Update();

            // Shared Enter/Escape logic, callable from both their own
            // dedicated keys and from left/right (Edward, 2026: same
            // arrow-key behaviour as the boot menu - right enters a
            // menu, left escapes one, and terminal selection lists like
            // Exit Game's Yes/No move the cursor with left/right too).
            auto doEnter = [&]() {
                const MenuNode& deepest = WalkPath(root, path);
                std::vector<int> parentPath(path.begin(), path.end() - 1);
                std::string parentLabel = WalkPath(root, parentPath).label;

                EnterResult r = Enter(root, path);
                if (r == EnterResult::Toggled && parentLabel == "Exit Game" && deepest.label == "Yes")
                {
                    result.outcome = PauseMenuOutcome::ExitGame;
                    running = false;
                }
                else if (r == EnterResult::Toggled && parentLabel == "Exit Game" && deepest.label == "No")
                {
                    // Backs out, matching Escape, rather than doing
                    // nothing at all (Edward, 2026).
                    ALTEngine::Menu::Back(path);
                }
                else if (parentLabel == "Fully Loaded")
                {
                    // Persisted so it survives the menu closing, and reported so
                    // the caller (which owns the inventory) can apply it.
                    bool on = (deepest.label == "Yes");
                    ALTEngine::Bootstrap::Config cheatConfig;
                    cheatConfig.Set("CheatFullyLoaded", on ? "On" : "Off");
                    result.cheatFullyLoaded = on;
                    SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                }
                else if (r == EnterResult::Toggled && deepest.label == "Options")
                {
                    // Opens the same full Options menu the boot menu
                    // uses (Edward, 2026: "update the options button in
                    // the pause menu so that it opens the [options]
                    // menu") - startInOptionsOnly renders it without the
                    // main-menu list behind it and skips its music,
                    // returning control back here (resuming this same
                    // pause menu loop) once backed all the way out,
                    // rather than falling back to a main menu that was
                    // never shown. includeCredits=false since Credits
                    // doesn't make sense mid-game (Edward, 2026: "when
                    // coming from the pause menu, the Credits button
                    // does not spawn").
                    std::vector<int> unusedMainPath = { 0 };
                    ALTEngine::Menu::MenuResult optionsResult = MenuController::Run(cdDirectory, renderSettings, resolutionSettings,
                        difficultySettings, cameraSwaySettings, languageSettings, keyBindings, audioSettings,
                        language, unusedMainPath, /*startInOptionsOnly=*/true);
                    if (optionsResult.windowClosed)
                    {
                        result.outcome = PauseMenuOutcome::WindowClosed;
                        running = false;
                    }

                    // Enable Cheats may have just been toggled in there, so the
                    // Cheats entry is added or removed now. The cursor is clamped
                    // because the list can have shrunk under it.
                    syncCheatsEntry();
                    if (path.empty() || path[0] >= static_cast<int>(root.children.size()))
                    {
                        path = { root.children.empty() ? 0 : static_cast<int>(root.children.size()) - 1 };
                    }
                }
                else if (r == EnterResult::Toggled && (deepest.label == "Save Game" || deepest.label == "Load Game"))
                {
                    // Same SaveSlotScreen::Run the main menu's own "Load
                    // Game" already uses - fully self-contained, no
                    // dependency on this loop's own state, so it already
                    // has no menu background behind it, matching the
                    // main-menu path. Just returns control back here
                    // afterward, resuming this same pause menu loop
                    // (still on Save Game/Load Game) rather than jumping
                    // to the main menu or exiting gameplay.
                    SaveSlotMode mode = (deepest.label == "Save Game") ? SaveSlotMode::Save : SaveSlotMode::Load;
                    SaveSlotResult slotResult = SaveSlotScreen::Run(cdDirectory, mode, StubSaveSlots(), false);
                    if (slotResult.windowClosed)
                    {
                        result.outcome = PauseMenuOutcome::WindowClosed;
                        running = false;
                    }
                    // NEXT: actually save/load real data once a real save
                    // system exists - same TODO as main.cpp's own Load
                    // Game handling.
                }
                if (r != EnterResult::NoOp) { SfxPlayer::Play(SfxId::MenuSelect, cdDirectory); }
            };

            auto doEscape = [&]() {
                if (!ALTEngine::Menu::Back(path)) { running = false; } // backed out of the top level - resume
                SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
            };

            // Left/right moving the cursor is specifically for the Exit
            // Game Yes/No confirmation, matching the original - not a
            // general rule for any list of leaf Actions (Edward, 2026).
            auto currentListIsExitGameConfirmation = [&]() {
                if (path.empty()) { return false; }
                std::vector<int> parentPath(path.begin(), path.end() - 1);
                const MenuNode& parent = parentPath.empty() ? root : WalkPath(root, parentPath);
                return parent.label == "Exit Game";
            };

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
                        doEnter();
                        break;
                    case SDLK_ESCAPE:
                        doEscape();
                        break;
                    case SDLK_RIGHT:
                        if (currentListIsExitGameConfirmation())
                        {
                            MoveSelection(root, path, 1);
                            SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        }
                        else
                        {
                            doEnter();
                        }
                        break;
                    case SDLK_LEFT:
                        if (currentListIsExitGameConfirmation())
                        {
                            MoveSelection(root, path, -1);
                            SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        }
                        else
                        {
                            doEscape();
                        }
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

            int scale = ComputeMenuScale(renderer);
            int rowHeight = TextHeight(scale) + scale * 6;
            int margin = scale * 8;

            int leftWidth = 0;
            DrawLeftColumn(renderer, root.children, path[0], inventory, margin, margin, rowHeight, scale, leftWidth, language);

            int panelX = margin + leftWidth + scale * 8;
            int panelY = margin;

            // Every model on every panel centres within this one width, so they
            // line up with each other and with the Auto Mapper's map rather than
            // each sitting flush against panelX at its own size (Edward, 2026).
            //
            // 520 is the map's width; clamped to what is left of the window
            // after the inventory column, for the same reason the map is.
            int windowWidth = 0, windowHeightPx = 0;
            SDL_GetRenderOutputSize(renderer, &windowWidth, &windowHeightPx);
            int contentWidth = std::min(520, windowWidth - panelX - margin);
            if (contentWidth < 80) { contentWidth = 80; }

            // A model's drawn width, shrunk to the column if the column is
            // narrower. Without this a 260-wide model kept its size on a narrow
            // window and ran off the right edge - the centring only moved it,
            // it could not make it fit.
            auto fitWidth = [contentWidth](int w) { return std::min(w, contentWidth); };

            // X for a model of `w` centred in that column, never left of panelX.
            auto centredX = [panelX, contentWidth](int w) {
                int x = panelX + (contentWidth - std::min(w, contentWidth)) / 2;
                return x < panelX ? panelX : x;
            };
            const std::string& topLabel = root.children[static_cast<size_t>(path[0])].label;

            if (topLabel == "Options")
            {
                // Nothing to preview here anymore - Options is a plain
                // trigger now (Edward, 2026: "update the options button
                // in the pause menu so that it opens the [options]
                // menu"), pressing it launches the real Options menu
                // directly rather than showing anything in this panel.
            }
            else if (topLabel == "Exit Game")
            {
                // "Are You Sure ?" -> No / Yes - only shown once Exit
                // Game has actually been entered (path.size() >= 2),
                // not just while it's highlighted (Edward, 2026: "Don't
                // show the 'Are You Sure? / No / Yes' in the pause menu
                // unless 'Exit Game' has been pressed"). Explicitly
                // empty otherwise, rather than falling through to the
                // generic model-preview branch below (Exit Game has no
                // modelIndex of its own).
                if (path.size() >= 2)
                {
                    // Now a top-level entry (Edward, 2026: "move the
                    // Exit Game button into the main list in the pause
                    // menu") rather than nested within Options, so
                    // path[1] is directly the No/Yes cursor (was
                    // path[2] when nested one level deeper).
                    int confirmIndex = path[1];
                    std::string confirmLabel = ALTEngine::Bootstrap::Tr(confirmIndex == 1 ? ALTEngine::Bootstrap::StringId::Yes : ALTEngine::Bootstrap::StringId::No, language);
                    Color confirmColor = confirmIndex == 1 ? COLOR_CURSOR : COLOR_DIM;
                    std::string prefix = ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::AreYouSure, language);
                    // Same row-Y formula DrawLeftColumn itself uses (y +
                    // index*rowHeight, vertically centred within the
                    // row) so this text lines up with the Exit Game
                    // button in the left column, rather than always
                    // sitting at the panel's top edge regardless of
                    // where Exit Game actually is in the list (Edward,
                    // 2026: "align the 'Are You Sure ? No / Yes' with
                    // the Exit Game button").
                    int exitGameTextY = margin + path[0] * rowHeight + (rowHeight - TextHeight(scale)) / 2;
                    DrawBitmapText(renderer, prefix, panelX, exitGameTextY, scale, COLOR_DIM);
                    DrawBitmapText(renderer, confirmLabel, panelX + TextWidth(prefix, scale), exitGameTextY, scale, confirmColor);
                }
            }
            else if (topLabel == "Cheats")
            {
                // Its own branch, so Cheats does NOT fall through to the generic
                // inventory-item branch below - that one treats an unknown label
                // as an unowned item and prints "NOT AVAILABLE" (Edward, 2026).
                //
                // Cheat rows in the panel, and the chosen row's No/Yes beside it
                // once entered, mirroring how Exit Game draws its confirm.
                const MenuNode& cheats = root.children[static_cast<size_t>(path[0])];
                int cheatIndex = (path.size() >= 2 && path[1] >= 0
                                  && static_cast<size_t>(path[1]) < cheats.children.size()) ? path[1] : -1;

                for (size_t i = 0; i < cheats.children.size(); ++i)
                {
                    const MenuNode& cheat = cheats.children[i];
                    bool onThisRow = (cheatIndex == static_cast<int>(i));
                    int rowY = panelY + static_cast<int>(i) * rowHeight;

                    std::string label = DisplayLabel(cheat, language);
                    DrawBitmapText(renderer, label, panelX, rowY, scale,
                                   onThisRow ? COLOR_CURSOR : COLOR_DIM);

                    // No / Yes only once this cheat has been entered, the same
                    // rule Exit Game uses for its confirm.
                    if (onThisRow && path.size() >= 3 && !cheat.children.empty())
                    {
                        int choice = path[2];
                        int x = panelX + TextWidth(label, scale) + scale * 8;
                        for (size_t j = 0; j < cheat.children.size(); ++j)
                        {
                            std::string option = DisplayLabel(cheat.children[j], language);
                            bool selected = (static_cast<int>(j) == choice);
                            DrawBitmapText(renderer, option, x, rowY, scale,
                                           selected ? COLOR_CURSOR : COLOR_DIM);
                            x += TextWidth(option, scale) + scale * 6;
                        }
                    }
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
                    DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::NotAvailable, language), panelX, panelY, scale, COLOR_STATUS);
                }
            }
            else if (auto weaponInfo = WeaponInfoFor(topLabel, inventory, root.children[static_cast<size_t>(path[0])]))
            {
                std::string ammoText = weaponInfo->state->available
                    ? (std::to_string(weaponInfo->state->ammo) + " rounds available")
                    : "No ammo available";
                DrawBitmapText(renderer, ammoText, panelX, panelY, scale, COLOR_STATUS);

                DrawPickModModel(renderer, cdDirectory, weaponInfo->ammoModel, centredX(200), panelY + rowHeight * 2, fitWidth(200), 150, scale, rotationAngle);
                DrawPickModModel(renderer, cdDirectory, weaponInfo->weaponModel, centredX(260), panelY + rowHeight * 2 + 170, fitWidth(260), 150, scale, rotationAngle);

                std::string statusText = weaponInfo->state->equipped ? "Selected" : ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::NotAvailable, language);
                DrawBitmapText(renderer, statusText, panelX, panelY + rowHeight * 2 + 340, scale, COLOR_STATUS);
            }
            else if (topLabel == "Save Game" || topLabel == "Load Game")
            {
                // Harddrive Left/Right - always available (no owned/
                // not-owned concept the way equipment has), so no
                // "Not available" fallback text here unlike the generic
                // branch below.
                const MenuNode& node = root.children[static_cast<size_t>(path[0])];
                DrawOptobjModel(renderer, cdDirectory, node.modelIndex, centredX(260), panelY, fitWidth(260), 200, scale, rotationAngle);
            }
            else // Auto Mapper / Shoulder Lamp / Batteries - single model, no ammo
            {
                const MenuNode& node = root.children[static_cast<size_t>(path[0])];
                bool owned = (topLabel == "Auto Mapper" && inventory.hasAutoMapper) ||
                             (topLabel == "Shoulder Lamp" && inventory.hasShoulderLamp) ||
                             (topLabel == "Batteries" && inventory.HasBatteries());
                // The Auto Mapper IS the map, so when the player has one, show
                // the level rather than a spinning model of the device. This is
                // where the original put the map - it is an inventory item, not
                // a separate screen - so it needs no new menu entry.
                // Show the map when the player owns the device, OR when the
                // Live Minimap option is on - otherwise the pause map is
                // unreachable until an Auto Mapper pickup is found, which made
                // it look like it was not implemented.
                bool mapAllowed = owned;
                if (!mapAllowed)
                {
                    ALTEngine::Bootstrap::Config modernConfig;
                    ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
                    mapAllowed = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::LiveMinimap);
                }

                // Where the "not available" note goes. Moves below whatever was
                // actually drawn: with the map now 400 tall, the old fixed
                // panelY + 220 landed in the middle of it.
                int notAvailableY = panelY + 220;

                if (topLabel == "Auto Mapper" && mapAllowed && level != nullptr)
                {
                    // Twice the old 260x200, with the device's own model beneath
                    // it - the map is the point of this panel, the model is the
                    // item being described.
                    //
                    // Clamped to what is actually on screen rather than fixed at
                    // 520x400: the panel starts after the left column, so a
                    // small window or a wide inventory list would otherwise push
                    // the map (and the model under it) off the edge.
                    const int modelHeight = 150;
                    const int gap = scale * 6;

                    int mapW = contentWidth;
                    int mapH = std::min(400, windowHeightPx - panelY - margin - modelHeight - gap);
                    if (mapH < 80) { mapH = 80; }

                    ALTEngine::Renderer::MinimapStyle style;
                    style.drawTriggers = true; // room to be informative here
                    SDL_FRect mapRect{ static_cast<float>(panelX), static_cast<float>(panelY),
                                       static_cast<float>(mapW), static_cast<float>(mapH) };
                    // `owned` here means the player has the Auto Mapper, which
                    // reveals the level; otherwise only visited cells show.
                    ALTEngine::Renderer::DrawMinimap(renderer, *level, mapRect,
                                                     playerGridX, playerGridZ, playerYaw, style,
                                                     owned ? nullptr : minimapVisited);

                    DrawPickModModel(renderer, cdDirectory, node.modelIndex,
                                     centredX(260), panelY + mapH + gap, fitWidth(260), modelHeight, scale, rotationAngle);
                    notAvailableY = panelY + mapH + gap + modelHeight + gap;
                }
                else
                {
                    DrawPickModModel(renderer, cdDirectory, node.modelIndex, centredX(260), panelY, fitWidth(260), 200, scale, rotationAngle);
                }
                if (!owned)
                {
                    DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::NotAvailable, language),
                                   panelX, notAvailableY, scale, COLOR_STATUS);
                }
            }

            // Description line, drawn exactly like the Options menu's: the
            // deepest node on the path that HAS one, greedy word-wrapped to the
            // window, growing upward from a fixed bottom so extra lines never
            // push into anything (Edward, 2026 - "same location as the
            // description for the Modern options").
            {
                const MenuNode* node = &root;
                const MenuNode* described = nullptr;
                for (size_t depth = 0; depth < path.size(); ++depth)
                {
                    int childIndex = path[depth];
                    if (childIndex < 0 || static_cast<size_t>(childIndex) >= node->children.size()) { break; }
                    node = &node->children[static_cast<size_t>(childIndex)];
                    if (node->descriptionStringId >= 0) { described = node; }
                }

                if (described)
                {
                    int windowW = 0, windowH = 0;
                    SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

                    std::string description = ALTEngine::Bootstrap::Tr(
                        static_cast<ALTEngine::Bootstrap::StringId>(described->descriptionStringId), language);

                    int maxWidth = windowW - scale * 32;
                    if (maxWidth < scale * 64) { maxWidth = windowW; }

                    std::vector<std::string> lines;
                    {
                        std::string line;
                        size_t pos = 0;
                        while (pos <= description.size())
                        {
                            size_t space = description.find(' ', pos);
                            std::string word = description.substr(pos, space == std::string::npos ? std::string::npos : space - pos);
                            std::string candidate = line.empty() ? word : line + " " + word;
                            if (!line.empty() && TextWidth(candidate, scale) > maxWidth)
                            {
                                lines.push_back(line);
                                line = word;
                            }
                            else { line = candidate; }
                            if (space == std::string::npos) { break; }
                            pos = space + 1;
                        }
                        if (!line.empty()) { lines.push_back(line); }
                    }

                    int bottom = windowH - rowHeight * 4;
                    int top = bottom - rowHeight * static_cast<int>(lines.size());
                    for (size_t i = 0; i < lines.size(); ++i)
                    {
                        DrawBitmapText(renderer, lines[i],
                                       (windowW - TextWidth(lines[i], scale)) / 2,
                                       top + rowHeight * static_cast<int>(i), scale, COLOR_DIM);
                    }
                }
            }

            SDL_RenderPresent(renderer);
        }

        return result;
    }
}
