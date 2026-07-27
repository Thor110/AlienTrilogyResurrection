#include "PauseMenuScreen.h"
#include "PauseMenuTree.h"
#include "SaveSlotScreen.h"
#include "../Audio/MusicPlayer.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/MissionText.h"
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
    using ALTEngine::Audio::MusicPlayer;
    using ALTEngine::Audio::SfxId;
    using ALTEngine::Audio::SfxPlayer;
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::AudioSettings;
    using ALTEngine::Bootstrap::ComputeMenuScale;
    using ALTEngine::Bootstrap::LerpColor;
    using ALTEngine::Bootstrap::PulsePhase;
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
                bool enabled = items[i].enabled;
                bool isEquipped = false;
                if (auto info = WeaponInfoFor(items[i].label, inventory, items[i])) { isEquipped = info->state->equipped; }

                // Every row gets a box now (Edward, 2026: "a disabled
                // version of the current highlight around all options
                // and pause menu items") - very dark green by default,
                // brighter and pulsing only for the cursor's row, and
                // only if that item is enabled.
                Color boxColor = COLOR_HIGHLIGHT_BG;
                if (isCursor && enabled) { boxColor = LerpColor(COLOR_HIGHLIGHT_BG, COLOR_HIGHLIGHT_BG_LIGHT, pulse); }

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
        Language language,
        const std::string& missionLevelCode,
        const PlayerInventoryState& inventory,
        AudioSettings& audioSettings)
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

        // Functional volume sliders (Edward, 2026: "The pause menu also
        // needs functional Music/SFX volume sliders") - Options is
        // always the last top-level entry in this fixed tree, with SFX
        // Volume/Music Volume as its first two children (see
        // PauseMenuTree.cpp).
        {
            MenuNode& optionsForInit = root.children.back();
            optionsForInit.children[0].sliderValue = audioSettings.SfxVolume();
            optionsForInit.children[1].sliderValue = audioSettings.MusicVolume();
        }
        std::vector<int> path = { 0 };

        // SFX/Music Volume (Edward, 2026: "Separate the buttons from the
        // sliders, so that you have to press the button to access the
        // slider... only requiring escape if you have selected the
        // music or sfx slider button") - false while just navigating
        // (left/right behave like every other entry), true only once
        // the button's been pressed.
        bool adjustingSlider = false;

        PauseMenuResult result;
        bool running = true;

        while (running)
        {
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

            // Functional volume sliders (Edward, 2026: "The pause menu
            // also needs functional Music/SFX volume sliders") - same
            // shape as currentListIsExitGameConfirmation above, but for
            // adjusting a value rather than moving a cursor. Returns
            // false (no-op) if the cursor isn't on one of these two
            // specific entries, so the caller falls back to the normal
            // enter/escape behaviour.
            auto AdjustVolumeIfOnSliderEntry = [&](int delta) {
                if (path.size() < 2) { return false; }
                std::vector<int> parentPath(path.begin(), path.end() - 1);
                const MenuNode& parent = WalkPath(root, parentPath);
                if (parent.label != "Options") { return false; }

                MenuNode& leaf = WalkPath(root, path);
                bool isSfx = (leaf.label == "SFX Volume");
                bool isMusic = (leaf.label == "Music Volume");
                if (!isSfx && !isMusic) { return false; }

                int current = isMusic ? audioSettings.MusicVolume() : audioSettings.SfxVolume();
                int updated = std::clamp(current + delta, 0, 10);
                if (isMusic)
                {
                    audioSettings.SetMusicVolume(updated);
                    MusicPlayer::SetVolume(updated); // applies immediately, same as the boot menu's own Volume > Music entry
                }
                else
                {
                    audioSettings.SetSfxVolume(updated);
                }
                leaf.sliderValue = updated;
                SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                return true;
            };

            // The "press the button" half of the same feature (Edward,
            // 2026) - checked before the normal doEnter() flow on Enter/
            // Right.
            auto TryEnterSliderIfOnEntry = [&]() {
                if (path.size() < 2) { return false; }
                std::vector<int> parentPath(path.begin(), path.end() - 1);
                const MenuNode& parent = WalkPath(root, parentPath);
                if (parent.label != "Options") { return false; }

                const MenuNode& leaf = WalkPath(root, path);
                if (leaf.label != "SFX Volume" && leaf.label != "Music Volume") { return false; }

                adjustingSlider = true;
                SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                return true;
            };

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.outcome = PauseMenuOutcome::WindowClosed; running = false; }

                // SFX/Music Volume, once entered (Edward, 2026) - only
                // left/right (adjust) and Escape (exit back to normal
                // navigation, cursor still on the button) are processed;
                // up/down/enter are ignored while adjusting.
                else if (adjustingSlider)
                {
                    if (event.type == SDL_EVENT_KEY_DOWN)
                    {
                        if (event.key.key == SDLK_ESCAPE)
                        {
                            adjustingSlider = false;
                            SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                        }
                        else if (event.key.key == SDLK_RIGHT) { AdjustVolumeIfOnSliderEntry(1); }
                        else if (event.key.key == SDLK_LEFT) { AdjustVolumeIfOnSliderEntry(-1); }
                    }
                }
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
                        if (!TryEnterSliderIfOnEntry()) { doEnter(); }
                        break;
                    case SDLK_ESCAPE:
                        doEscape();
                        break;
                    // Edward, 2026: "Separate the buttons from the
                    // sliders, so that you have to press the button to
                    // access the slider... you can still back out using
                    // left, only requiring escape if you have selected
                    // the music or sfx slider button" - Right on the
                    // button enters slider-adjust mode (handled above);
                    // once entered, left/right adjust and only Escape
                    // exits. Until then, left/right behave like every
                    // other entry (with Exit Game's own Yes/No
                    // confirmation as the one pre-existing exception).
                    case SDLK_RIGHT:
                        if (TryEnterSliderIfOnEntry())
                        {
                            // handled
                        }
                        else if (currentListIsExitGameConfirmation())
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
            const std::string& topLabel = root.children[static_cast<size_t>(path[0])].label;

            if (topLabel == "Options")
            {
                const MenuNode& optionsNode = root.children[static_cast<size_t>(path[0])];
                int cursorHere = (path.size() >= 2) ? path[1] : -1; // -1 = haven't descended into Options yet, nothing highlighted

                // Highlight box width matches just the button/label area
                // (like every other entry in this menu) - the slider bar
                // gets its own separate space to the right, not enclosed
                // within the box (Edward, 2026: "the sliders should
                // actually be in their own space to the right of the
                // Music/SFX volume buttons in the pause menu, not within
                // the highlight around the text itself").
                int buttonWidth = std::max({ TextWidth("SFX VOLUME", scale), TextWidth("MUSIC VOLUME", scale), TextWidth("EXIT GAME", scale) }) + scale * 8;
                int barX = panelX - scale * 4 + buttonWidth + scale * 8; // fixed X so SFX/Music's bars align regardless of their different label widths
                float pulse = PulsePhase();

                // Row boxes at rowY..rowY+rowHeight-scale*2 (matching
                // DrawColumn/DrawLeftColumn's own box-per-row convention
                // exactly), text/sliders vertically centred within the
                // full rowHeight - Edward, 2026: "the text spacing still
                // isn't quite right" - these previously drew flush to
                // the row's top instead of centred like every other
                // entry.
                auto drawRow = [&](int rowY, bool isCursor) {
                    Color boxColor = isCursor ? LerpColor(COLOR_HIGHLIGHT_BG, COLOR_HIGHLIGHT_BG_LIGHT, pulse) : COLOR_HIGHLIGHT_BG;
                    SDL_FRect bar{ static_cast<float>(panelX - scale * 4), static_cast<float>(rowY), static_cast<float>(buttonWidth), static_cast<float>(rowHeight - scale * 2) };
                    DrawRoundedRect(renderer, bar, static_cast<float>(scale * 2), boxColor);
                    return rowY + (rowHeight - TextHeight(scale)) / 2;
                };

                // One rowHeight apart now, matching the left column's own
                // spacing (Edward, 2026: "they also do not line up with
                // the entries to the left of them") - now that the
                // slider's bar sits beside its label instead of below it,
                // each row only needs a single rowHeight of vertical
                // space, the same as every other row in this menu.
                int boxHeight = rowHeight - scale * 2;
                drawRow(panelY, cursorHere == 0);
                DrawSlider(renderer, "SFX VOLUME", optionsNode.children[0].sliderValue, panelX, barX, panelY, boxHeight, scale, COLOR_CURSOR, COLOR_CURSOR, COLOR_DIM);

                int musicY = panelY + rowHeight;
                drawRow(musicY, cursorHere == 1);
                DrawSlider(renderer, "MUSIC VOLUME", optionsNode.children[1].sliderValue, panelX, barX, musicY, boxHeight, scale, COLOR_CURSOR, COLOR_CURSOR, COLOR_DIM);

                bool exitCursor = (cursorHere == 2);
                int exitY = panelY + rowHeight * 2;
                int exitTextY = drawRow(exitY, exitCursor);
                Color exitColor = exitCursor ? COLOR_CURSOR : COLOR_DIM;
                DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::ExitGameTitle, language), panelX, exitTextY, scale, exitColor);

                if (exitCursor) // Exit Game is the active/previewed column
                {
                    // Edward, 2026: "Are You Sure ? should appear below
                    // Exit Game in the list when selected. Then the
                    // No/Yes remains as it is on the right side of Are
                    // You Sure ?" - moved to its own row below Exit Game
                    // (was inline to the right on the same row before);
                    // Yes/No's own text-colour highlighting (not a box)
                    // stays exactly as it was.
                    int confirmIndex = (path.size() >= 3) ? path[2] : 0;
                    std::string confirmLabel = confirmIndex == 1 ? "Yes" : "No";
                    Color confirmColor = confirmIndex == 1 ? COLOR_CURSOR : COLOR_DIM;
                    std::string prefix = "ARE YOU SURE ? ";
                    int confirmY = exitY + rowHeight + (rowHeight - TextHeight(scale)) / 2;
                    DrawBitmapText(renderer, prefix, panelX, confirmY, scale, COLOR_DIM);
                    DrawBitmapText(renderer, confirmLabel, panelX + TextWidth(prefix, scale), confirmY, scale, confirmColor);
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

                DrawPickModModel(renderer, cdDirectory, weaponInfo->ammoModel, panelX, panelY + rowHeight * 2, 200, 150, scale, rotationAngle);
                DrawPickModModel(renderer, cdDirectory, weaponInfo->weaponModel, panelX, panelY + rowHeight * 2 + 170, 260, 150, scale, rotationAngle);

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
                DrawOptobjModel(renderer, cdDirectory, node.modelIndex, panelX, panelY, 260, 200, scale, rotationAngle);
            }
            else // Auto Mapper / Shoulder Lamp / Batteries - single model, no ammo
            {
                const MenuNode& node = root.children[static_cast<size_t>(path[0])];
                bool owned = (topLabel == "Auto Mapper" && inventory.hasAutoMapper) ||
                             (topLabel == "Shoulder Lamp" && inventory.hasShoulderLamp) ||
                             (topLabel == "Batteries" && inventory.hasBatteries);
                DrawPickModModel(renderer, cdDirectory, node.modelIndex, panelX, panelY, 260, 200, scale, rotationAngle);
                if (!owned) { DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::NotAvailable, language), panelX, panelY + 220, scale, COLOR_STATUS); }
            }

            SDL_RenderPresent(renderer);
        }

        return result;
    }
}
