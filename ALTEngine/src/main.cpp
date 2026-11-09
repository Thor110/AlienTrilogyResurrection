#include <iostream>
#include <optional>

#include "Audio/SfxPlayer.h"
#include "Bootstrap/FsUtil.h"
#include "Bootstrap/MuthurConsole.h"
#include "Bootstrap/ModernSettings.h"
#include "Bootstrap/AppWindow.h"
#include "Bootstrap/Config.h"
#include "Bootstrap/DiscLocator.h"
#include "Bootstrap/GameLocator.h"
#include "Bootstrap/GameplaySettings.h"
#include "Bootstrap/AudioSettings.h"
#include "Bootstrap/KeyBindings.h"
#include "Bootstrap/Strings.h"
#include "Bootstrap/ImageDisplay.h"
#include "Bootstrap/InstallPipeline.h"
#include "Bootstrap/Localization.h"
#include "Bootstrap/PlatformPaths.h"
#include "Bootstrap/RenderSettings.h"
#include "Bootstrap/ResolutionSettings.h"
#include "Formats/BootSequence.h"
#include "Formats/SplashImageLoader.h"
#include "Menu/MenuController.h"
#include "Renderer/ModelPreview.h"
#include "Renderer/ModelRenderer.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MissionBriefingScreen.h"
#include "Screens/MultiplayerScreens.h"
#include "Net/Session.h"

#include <algorithm>
#include "Screens/SaveSlotScreen.h"
#include "Screens/EndLevelPromptScreen.h"
#include "Screens/MissionAssessmentScreen.h"
#include "Formats/LevelList.h"
#include "Game/Progression.h"
#include "Game/SaveGame.h"
#include "Video/OverrideVideo.h"
#include "Video/VideoPlayer.h"

using namespace ALTEngine::Bootstrap;
using namespace ALTEngine::Formats;
using namespace ALTEngine::Video;
using namespace ALTEngine::Menu;
using namespace ALTEngine::Screens;

namespace
{
    // A console app launched by double-clicking in Explorer closes its
    // window the instant main() returns, which hides any diagnostic
    // output entirely - looks exactly like "nothing happened". Pause
    // before any early-exit path so that's not a dead end while debugging.
    void PauseBeforeExit()
    {
        std::cout << "\nPress Enter to close...";
        std::cin.get();
    }

    enum class CampaignEnd { BackToMenu, WindowClosed };

    // "1.1.1" -> "111".
    std::string DigitsOf(const std::string& code)
    {
        std::string digits;
        for (char c : code) { if (c >= '0' && c <= '9') { digits += c; } }
        return digits;
    }

    // "111" -> "1.1.1", for a level that is not in the manifest.
    std::string DottedOf(const std::string& digits)
    {
        std::string dotted;
        for (char c : digits)
        {
            if (!dotted.empty()) { dotted += '.'; }
            dotted += c;
        }
        return dotted;
    }

    // GraphicsViewer.cs tries these extensions in this order, since which
    // one's actually present varies by file/edition. Same here.
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

    // Shows CD/GFX/LEGAL - the first thing in the documented boot order
    // (data/BootSequence.json). Returns false only if the user closed the
    // window outright (treated the same as DirectoryBrowser's abort).
    bool ShowLegalSplash(const std::filesystem::path& cdDirectory)
    {
        std::filesystem::path gfxDir = cdDirectory / "GFX";
        std::filesystem::path palPath = cdDirectory / "PALS" / "LEGAL.PAL";

        auto bndPath = ResolveGfxFile(gfxDir, "LEGAL");
        if (!bndPath.has_value())
        {
            std::cout << "Could not find LEGAL graphics file under " << ALTEngine::Bootstrap::PathText(gfxDir) << " - skipping splash.\n";
            return true;
        }
        std::error_code ec;
        if (!std::filesystem::exists(palPath, ec))
        {
            std::cout << "Could not find " << ALTEngine::Bootstrap::PathText(palPath) << " - skipping splash.\n";
            return true;
        }

        try
        {
            SplashImage image = SplashImageLoader::Load(*bndPath, palPath, /*paletteTrimmed*/ false);
            std::cout << "Showing LEGAL splash (" << image.width << "x" << image.height << ")...\n";
            return ImageDisplay::Show(image.rgba, image.width, image.height, /*maxDurationMs*/ 5000);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load/show LEGAL splash: " << e.what() << "\n";
            return true; // don't abort the whole boot over a missing splash
        }
    }

    // Plays the intro videos data/BootSequence.json lists, in its order
    // (the original's four if the file is missing), each resolved for
    // `language` via LocalizedBaseName. A missing file is skipped, same
    // philosophy as ShowLegalSplash - only an actual window-close aborts the
    // boot.
    bool PlayIntroVideos(const std::filesystem::path& cdDirectory, Language language)
    {
        std::filesystem::path aviDir = cdDirectory / "AVI";
        const std::vector<std::string> baseNames =
            ALTEngine::Formats::BootSequence::LoadVideos(ExecutableDirectory() / "data" / "BootSequence.json");

        for (const std::string& baseName : baseNames)
        {
            if (auto overridePath = FindOverrideVideo(cdDirectory, baseName))
            {
                std::cout << "Playing override " << overridePath->string() << "...\n";
                if (!VideoPlayer::Play(*overridePath))
                {
                    return false; // window closed - abort boot
                }
                continue;
            }

            std::string localizedName = LocalizedBaseName(baseName, language);
            std::filesystem::path path = aviDir / (localizedName + ".AVI");
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
            {
                std::cout << "Could not find " << ALTEngine::Bootstrap::PathText(path) << " - skipping.\n";
                continue;
            }

            std::cout << "Playing " << localizedName << ".AVI...\n";
            if (!VideoPlayer::Play(path))
            {
                return false; // window closed - abort boot
            }
        }
        return true;
    }
}

int main(int, char**)
{
#ifndef ALTENGINE_VERSION
#define ALTENGINE_VERSION "unknown"
#endif
    // The boot's readout, on the game window (MU/TH/UR style) as well as the
    // command window: finding the game, installing it, the patches. Ends,
    // and gives the window back, after RunPatches below.
    std::optional<ALTEngine::Bootstrap::MuthurConsole> muthur;
    muthur.emplace("MU/TH/UR 9000 - ALTENGINE " ALTENGINE_VERSION);
    std::cout << "ALTEngine " ALTENGINE_VERSION " boot starting...\n";

    Config config;
    std::cout << "Config file: " << ALTEngine::Bootstrap::PathText(config.FilePath()) << "\n";

    GameLocator locator(config);
    std::cout << "Locating install directory...\n";

    if (!locator.TryAutoLocate().has_value())
    {
        DiscLocateResult disc = DiscLocator::FindDisc();
        if (disc.found)
        {
            std::filesystem::path destination = ExecutableDirectory() / "GameData";
            std::cout << "Found disc at " << ALTEngine::Bootstrap::PathText(disc.discRoot) << "\n";
            if (InstallFromDisc(disc.discRoot, destination))
            {
                config.Set("GameDirectory", ALTEngine::Bootstrap::PathText(destination));
            }
        }
    }

    LocateResult result = locator.Locate();
    if (!result.success)
    {
        std::cout << "No install directory selected. Aborting.\n";
        ALTEngine::Bootstrap::MuthurConsole::WaitForKeyAtEnd();
        muthur.reset();
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    std::cout << "Installation directory: " << ALTEngine::Bootstrap::PathText(result.gameDirectory) << "\n";

    std::filesystem::path cdDirectory = result.gameDirectory / "CD";

    // Patching happens as part of installation (InstallFromDisc), and again
    // here on every launch: a fix added to Patches.json after the game was
    // installed never reached the files otherwise. Every patch checks its own
    // bytes first, so one already in place is left alone ("already applied")
    // and the pass costs a few file reads.
    RunPatches(cdDirectory);
    muthur.reset();
    if (ALTEngine::Bootstrap::MuthurConsole::QuitRequested())
    {
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        return 0;
    }

    // The player's loadout, owned here so it can persist across levels.
    ALTEngine::Screens::PlayerInventoryState playerInventory;

    // Launching a level FROM THE MENU always starts a fresh loadout - the
    // pistol and nothing else, which is the original's behaviour.
    //
    // Carrying a loadout is from one level into the NEXT one; it never carries
    // anything into a level launched from the main menu, which is the start of
    // a new run. When it did, turning on the Fully Loaded cheat once left 999
    // rounds in the inventory permanently - disabling cheats and restarting
    // the level did not clear it, because nothing ever reset the struct again
    // (Edward, 2026).
    auto prepareInventory = []( ALTEngine::Screens::PlayerInventoryState& inv) {
        inv = ALTEngine::Screens::PlayerInventoryState{};
    };

    // Level to level, the weapons and ammunition simply go with the player -
    // the original's own behaviour. It was a Modern option ("Keep Items") on
    // the mistaken belief that the original took them away each level; it
    // never did (Edward, 2026).

    // Must happen before anything below could possibly call Tr() -
    // LoadedLanguagePacks() only ever initializes its cache once, on
    // first use, so setting this any later would have no effect
    // (Edward, 2026: "move the LANGUAGE folder to GameData\CD\LANGUAGE
    // so that it sits alongside the original language files").
    ALTEngine::Bootstrap::SetLanguagePackDirectory(cdDirectory);

    if (!ShowLegalSplash(cdDirectory))
    {
        std::cout << "Boot window closed. Aborting.\n";
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    // Language now actually persists (Edward, 2026) - LanguageSettings
    // needs `config`, which already exists by this point, so it's
    // constructed here rather than alongside renderSettings/
    // resolutionSettings below.
    LanguageSettings languageSettings(config);
    Language language = languageSettings.Get();

    if (!PlayIntroVideos(cdDirectory, language))
    {
        std::cout << "Boot window closed. Aborting.\n";
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    RenderSettings renderSettings(config);
    ResolutionSettings resolutionSettings(config);
    DifficultySettings difficultySettings(config);
    CameraSwaySettings cameraSwaySettings(config);
    KeyBindings keyBindings(config);
    AudioSettings audioSettings(config);
    ALTEngine::Audio::SfxPlayer::SetVolume(audioSettings.SfxVolume());

    // Applied once, here, rather than than every time MenuController::Run
    // is re-entered (which would flicker the window every time the
    // player returns to the menu from gameplay) - EnsureCreated always
    // creates the window fullscreen regardless of what was last saved,
    // so a persisted Windowed/Borderless preference needs reapplying
    // explicitly on boot, or it's silently lost every restart.
    AppWindow::Instance().SetVSync(renderSettings.VSync());
    {
        auto savedResolution = resolutionSettings.Get();
        DisplayMode displayMode = renderSettings.GetDisplayMode();
        if (savedResolution.has_value())
        {
            if (displayMode == DisplayMode::Fullscreen)
            {
                // SetDisplayMode alone only switches which kind of
                // fullscreen is active - the exclusive resolution
                // itself needs setting separately, same as the menu's
                // own Resolution handling does.
                AppWindow::Instance().ApplyFullscreenResolution(savedResolution->first, savedResolution->second);
            }
            AppWindow::Instance().SetDisplayMode(displayMode, savedResolution->first, savedResolution->second);
        }
        else
        {
            AppWindow::Instance().SetDisplayMode(displayMode);
        }
    }

    SetSaveDirectory(ExecutableDirectory() / "Saves");
    const std::vector<LevelListEntry> levelList = LoadLevelList("data/LevelManifest.json");

    auto closeAndExit = [&]() {
        std::cout << "Boot window closed. Aborting.\n";
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    };

    // Plays from `startDigits` until the player quits, dies, runs out of
    // campaign or closes the window: briefing, level, assessment, the
    // end-of-level prompt, then the next level in LevelManifest.json order.
    // A save loaded from the pause menu restarts the loop at the saved level.
    auto playCampaign = [&](std::string digits) -> CampaignEnd {
        // Modern: Level Timer. The run's total: every level played to its
        // assessment since this game was started or a save was loaded, failed
        // missions included, as they are played again. A save loaded starts a
        // new total; a broken run is added up by hand.
        double runSeconds = 0.0;
        while (true)
        {
            const auto entry = ALTEngine::Game::Progression::Find(levelList, digits);
            const std::string dotted = entry ? entry->dottedCode : DottedOf(digits);
            std::cout << "Starting level " << dotted << "\n";

            // The briefing is not just presentation - it preloads PICKMOD and
            // OBJ3D, without which no object has a model to draw. Multiplayer
            // maps have no briefing text, so they get the preload alone.
            const bool multiplayerLevel = entry ? entry->multiplayer : (!digits.empty() && digits[0] == '9');
            if (!multiplayerLevel)
            {
                MissionBriefingResult briefing = MissionBriefingScreen::Run(cdDirectory, language, dotted);
                if (briefing.windowClosed) { return CampaignEnd::WindowClosed; }
            }
            else
            {
                MissionBriefingScreen::PreloadObjectModels(cdDirectory, language);
            }

            GameplayResult played = GameplayScreen::Run(cdDirectory, language, dotted, keyBindings, audioSettings,
                                                        renderSettings, resolutionSettings, difficultySettings,
                                                        cameraSwaySettings, languageSettings, &playerInventory);
            switch (played.outcome)
            {
            case GameplayOutcome::WindowClosed:
                return CampaignEnd::WindowClosed;
            case GameplayOutcome::ExitGame:
            case GameplayOutcome::PlayerDied:
                return CampaignEnd::BackToMenu;
            case GameplayOutcome::LoadGame:
            {
                auto save = ALTEngine::Game::SaveGame::Read(SaveDirectory(), played.loadSlot);
                if (!save) { return CampaignEnd::BackToMenu; }
                difficultySettings.Set(static_cast<Difficulty>(save->difficulty));
                playerInventory = save->inventory;
                digits = save->levelDigits;
                SetPendingRestore(save->levelDigits, save->snapshot);
                runSeconds = 0.0;
                continue;
            }
            case GameplayOutcome::LevelComplete:
                break;
            }

            // The assessment and where it leads (FUN_00014f44): a failed
            // mission is played again, and 85% or more opens the bonus level
            // that follows, if there is one. A level outside the original's
            // campaign goes on in manifest order.
            namespace PG = ALTEngine::Game::Progression;
            const int campaignIndex = PG::CampaignIndex(digits);
            const auto assessment = PG::Assess(played.tally, campaignIndex);
            MissionAssessmentStats stats;
            stats.aliensPercent = static_cast<float>(assessment.aliensPercent);
            stats.secretsPercent = static_cast<float>(assessment.secretsPercent);
            stats.missionPercent = static_cast<float>(assessment.missionPercent);
            stats.requiredPercent = assessment.requiredPercent;
            stats.levelDigits = digits;
            runSeconds += played.levelSeconds;
            {
                ALTEngine::Bootstrap::Config timerConfig;
                ALTEngine::Bootstrap::ModernSettings modern(timerConfig);
                stats.showTimes = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::LevelTimer);
            }
            stats.levelSeconds = played.levelSeconds;
            stats.runSeconds = runSeconds;
            if (stats.showTimes)
            {
                std::cout << "Level time " << FormatRunTime(played.levelSeconds) << ", run "
                          << FormatRunTime(runSeconds) << "\n";
            }
            if (MissionAssessmentScreen::Run(cdDirectory, stats, language).windowClosed) { return CampaignEnd::WindowClosed; }
            std::cout << "Assessment: aliens " << assessment.aliensPercent << "%, secrets " << assessment.secretsPercent
                      << "%, mission " << assessment.missionPercent << "% (needs more than "
                      << assessment.requiredPercent << "%), grade " << assessment.grade << "\n";

            std::optional<ALTEngine::Formats::LevelListEntry> next;
            if (campaignIndex >= 0)
            {
                const int nextIndex = PG::NextCampaignIndex(campaignIndex, assessment.grade);
                if (nextIndex == campaignIndex) { std::cout << "Mission failed - level " << dotted << " again.\n"; }
                if (nextIndex < PG::CAMPAIGN_LENGTH)
                {
                    next = PG::Find(levelList, PG::CAMPAIGN_ORDER[nextIndex]);
                    if (!next)
                    {
                        // A level the manifest doesn't list: its digits are enough.
                        ALTEngine::Formats::LevelListEntry unlisted;
                        unlisted.digits = PG::CAMPAIGN_ORDER[nextIndex];
                        unlisted.dottedCode = DottedOf(unlisted.digits);
                        next = unlisted;
                    }
                    if (PG::IsBonusLevel(nextIndex)) { std::cout << "Bonus level opened: " << next->dottedCode << "\n"; }
                }
            }
            else
            {
                next = PG::NextLevel(levelList, digits);
            }
            if (!next)
            {
                std::cout << "No level after " << dotted << " - back to the main menu.\n";
                return CampaignEnd::BackToMenu;
            }

            // Saving from the prompt checkpoints the NEXT level.
            ALTEngine::Game::SaveData checkpoint;
            checkpoint.name = ALTEngine::Game::SaveGame::DefaultName(next->digits);
            checkpoint.levelDigits = next->digits;
            checkpoint.difficulty = static_cast<int>(difficultySettings.Get());
            checkpoint.inventory = playerInventory;
            SetCheckpoint(checkpoint);

            // The Modern "Skip End Level Screen" option goes straight on to the
            // next mission without offering Continue / Save / Quit.
            bool skipPrompt = false;
            {
                ALTEngine::Bootstrap::Config modernConfig;
                ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
                skipPrompt = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::SkipEndLevelScreen);
            }
            if (!skipPrompt)
            {
                EndLevelPromptResult prompt = EndLevelPromptScreen::Run(cdDirectory);
                if (prompt.windowClosed) { return CampaignEnd::WindowClosed; }
                if (prompt.choice == EndLevelPromptChoice::Quit) { return CampaignEnd::BackToMenu; }
            }
            digits = next->digits;
        }
    };

    std::vector<int> mainPath = { 0 };

    while (true)
    {
        MenuResult menuResult = MenuController::Run(cdDirectory, renderSettings, resolutionSettings, difficultySettings,
                                                      cameraSwaySettings, languageSettings, keyBindings, audioSettings, language, mainPath);
        if (menuResult.windowClosed)
        {
            std::cout << "Boot window closed. Aborting.\n";
            ALTEngine::Renderer::ModelRenderer::Shutdown();
            AppWindow::Instance().Shutdown();
            PauseBeforeExit();
            return 1;
        }
        std::cout << "Menu selection: " << menuResult.action << "\n";

        if (menuResult.action == "Exit")
        {
            // Escape at the main menu root - previously fell through
            // unrecognized here, so the loop just called
            // MenuController::Run() again, which explains both observed
            // symptoms exactly: never actually quit, and restarted the
            // music (MusicPlayer::PlayLooped runs again at the top of
            // Run()).
            break;
        }

        if (menuResult.action == "Multiplayer")
        {
            // One multiplayer level, as host or guest, until the player leaves
            // or the game ends; then back to the multiplayer menu.
            auto playMultiplayer = [&](ALTEngine::Net::Session& session) -> bool {
                ALTEngine::Screens::PlayerInventoryState netInventory;
                namespace PG = ALTEngine::Game::Progression;
                // The host's Modern and Testing choices, for every player, for
                // as long as the game lasts (Bootstrap::SessionRules).
                const ALTEngine::Bootstrap::SessionRulesScope rulesInForce(session.Rules());
                std::cout << "Multiplayer: playing by the host's rules (Modern features 0x" << std::hex
                          << session.Rules().modernFeatures << std::dec << ", weapon pacing "
                          << static_cast<int>(session.Rules().weaponPacing) << ")\n";
                // While a screen between levels is up, the session is read
                // every frame: nobody times out, and what the others do in the
                // meantime waits for the level (and is caught up there, under
                // its syncing screen).
                const auto keepAlive = [&]() { session.Update(static_cast<double>(SDL_GetTicks()) / 1000.0); };
                while (true)
                {
                    const std::string digits = session.Level();
                    const uint16_t serial = session.LevelSerial();
                    session.DropStaleEvents();
                    if (session.Coop())
                    {
                        // The story missions keep their briefing.
                        MissionBriefingResult briefing = MissionBriefingScreen::Run(cdDirectory, language, DottedOf(digits), keepAlive);
                        if (briefing.windowClosed) { session.Stop(); return false; }
                        if (!session.Connected()) { session.Stop(); return true; }
                        // The host moved on while it was read (a guest who
                        // came in at the very end): that level, then.
                        if (session.LevelSerial() != serial)
                        {
                            if (session.Level().empty()) { session.Stop(); return true; }
                            continue;
                        }
                    }
                    else
                    {
                        MissionBriefingScreen::PreloadObjectModels(cdDirectory, language);
                    }
                    GameplayResult played = GameplayScreen::Run(cdDirectory, language, DottedOf(digits), keyBindings,
                                                                audioSettings, renderSettings, resolutionSettings,
                                                                difficultySettings, cameraSwaySettings, languageSettings,
                                                                &netInventory, &session);
                    if (played.outcome == GameplayOutcome::WindowClosed) { session.Stop(); return false; }
                    // Deathmatch is one map; co-operative goes on while every
                    // player finishes the level together.
                    if (played.outcome != GameplayOutcome::LevelComplete || !session.Coop() || !session.Connected())
                    {
                        session.Stop();
                        return true;
                    }

                    // The assessment, as after any story mission: this player's
                    // own (the creatures and objects are everyone's, the
                    // secrets their own), tagged as a co-operative game.
                    {
                        const int index = PG::CampaignIndex(digits);
                        const auto assessment = PG::Assess(played.tally, index);
                        MissionAssessmentStats stats;
                        stats.aliensPercent = static_cast<float>(assessment.aliensPercent);
                        stats.secretsPercent = static_cast<float>(assessment.secretsPercent);
                        stats.missionPercent = static_cast<float>(assessment.missionPercent);
                        stats.requiredPercent = assessment.requiredPercent;
                        stats.levelDigits = digits;
                        stats.coop = true;
                        if (MissionAssessmentScreen::Run(cdDirectory, stats, language, keepAlive).windowClosed)
                        {
                            session.Stop();
                            return false;
                        }
                    }

                    // CO-OPERATIVE: the next story mission. The host chooses
                    // it: the campaign's next level, by the host's own
                    // assessment (a bonus level opens on 85% or more), but
                    // never the same one again - a mission failed on one
                    // machine is not failed for all, as each runs its own
                    // creatures.
                    if (session.GetRole() == ALTEngine::Net::Session::Role::Host)
                    {
                        std::string nextDigits;
                        const int index = PG::CampaignIndex(digits);
                        if (index >= 0)
                        {
                            const int grade = std::max(1, PG::Assess(played.tally, index).grade);
                            const int next = PG::NextCampaignIndex(index, grade);
                            if (next < PG::CAMPAIGN_LENGTH) { nextDigits = PG::CAMPAIGN_ORDER[next]; }
                        }
                        std::cout << "Co-operative: level " << digits << " done, next "
                                  << (nextDigits.empty() ? std::string("none - the campaign is over") : nextDigits) << "\n";
                        session.AdvanceLevel(nextDigits);
                        if (nextDigits.empty())
                        {
                            // Say it a few times before leaving, so the guests hear.
                            for (int i = 0; i < 20; ++i) { session.Update(static_cast<double>(SDL_GetTicks()) / 1000.0); SDL_Delay(50); }
                            session.Stop();
                            return true;
                        }
                    }
                    else
                    {
                        // A guest waits for the host to name the next level
                        // (it may already have); Escape leaves the game.
                        bool leave = false;
                        while (session.LevelSerial() == serial && session.Connected())
                        {
                            SDL_Event event;
                            while (SDL_PollEvent(&event))
                            {
                                if (event.type == SDL_EVENT_QUIT) { session.Stop(); return false; }
                                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) { leave = true; }
                            }
                            if (leave) { break; }
                            session.Update(static_cast<double>(SDL_GetTicks()) / 1000.0);
                            session.DropStaleEvents();   // keeps what the next level has sent already
                            SDL_Delay(10);
                        }
                        if (leave || !session.Connected() || session.Level().empty())
                        {
                            if (session.Connected() && session.Level().empty()) { std::cout << "Co-operative: the campaign is over\n"; }
                            session.Stop();
                            return true;
                        }
                    }
                }
            };
            static std::string typedAddress;
            MultiplayerSettings mpSettings;
            // The name and look are kept between sessions.
            if (auto stored = config.Get("MultiplayerName"); stored && !stored->empty()) { mpSettings.playerName = *stored; }
            if (auto stored = config.Get("MultiplayerSkin"))
            {
                for (int k = 0; k < static_cast<int>(ALTEngine::Net::Skin::Count); ++k)
                {
                    if (*stored == ALTEngine::Net::SkinName(static_cast<ALTEngine::Net::Skin>(k))) { mpSettings.skin = k; }
                }
            }
            // The host's player limit and bots, kept too.
            auto readInt = [&](const char* key, int fallback, int lo, int hi) {
                if (auto stored = config.Get(key))
                {
                    try { return std::clamp(std::stoi(*stored), lo, hi); } catch (...) {}
                }
                return fallback;
            };
            mpSettings.playerLimit = readInt("MultiplayerPlayerLimit", 4, ALTEngine::Net::MIN_PLAYER_LIMIT, ALTEngine::Net::MAX_PLAYERS);
            mpSettings.botCount = readInt("MultiplayerBots", 0, 0, MultiplayerSettings::MaxBots(mpSettings.playerLimit));
            if (mpSettings.botCount > 0 && mpSettings.botCount < 3) { mpSettings.botCount = 0; }
            mpSettings.botSkill = readInt("MultiplayerBotSkill", 3, 1, 5);
            mpSettings.gameMode = readInt("MultiplayerGameMode", 0, 0, static_cast<int>(ALTEngine::Net::GameMode::Count) - 1);
            mpSettings.coopMission = readInt("MultiplayerCoopMission", 0, 0, ALTEngine::Game::Progression::CAMPAIGN_LENGTH - 1);
            const auto chosenSkin = [&]() {
                return static_cast<ALTEngine::Net::Skin>(
                    std::clamp(mpSettings.skin, 0, static_cast<int>(ALTEngine::Net::Skin::Count) - 1));
            };
            int multiplayerCursor = 0;
            bool inMultiplayerMenu = true;
            bool windowClosed = false;
            while (inMultiplayerMenu)
            {
                MultiplayerMainResult mainResult = MultiplayerMainScreen::Run(cdDirectory, multiplayerCursor);
                if (mainResult.windowClosed) { windowClosed = true; break; }

                switch (mainResult.choice)
                {
                case MultiplayerMainChoice::StartGame:
                {
                    MultiplayerStartResult r = MultiplayerStartScreen::Run(cdDirectory, mpSettings);
                    if (r.windowClosed) { windowClosed = true; inMultiplayerMenu = false; break; }
                    config.Set("MultiplayerPlayerLimit", std::to_string(mpSettings.playerLimit));
                    config.Set("MultiplayerBots", std::to_string(mpSettings.botCount));
                    config.Set("MultiplayerBotSkill", std::to_string(mpSettings.botSkill));
                    config.Set("MultiplayerGameMode", std::to_string(mpSettings.gameMode));
                    config.Set("MultiplayerCoopMission", std::to_string(mpSettings.coopMission));
                    if (!r.startedGame) { break; }
                    const auto mode = static_cast<ALTEngine::Net::GameMode>(mpSettings.gameMode);
                    const bool coop = mode == ALTEngine::Net::GameMode::Coop;
                    // Host: "Start at level" 1-10 is L900-L909; co-operative
                    // starts on the chosen story mission.
                    const int startIndex = std::clamp(mpSettings.startLevel, 1, 10) - 1;
                    const std::string digits = coop
                        ? std::string(ALTEngine::Game::Progression::CAMPAIGN_ORDER[std::clamp(
                              mpSettings.coopMission, 0, ALTEngine::Game::Progression::CAMPAIGN_LENGTH - 1)])
                        : "90" + std::to_string(startIndex);
                    ALTEngine::Net::Session session;
                    if (!session.StartHost(mpSettings.gameName.empty() ? mpSettings.playerName : mpSettings.gameName,
                                           digits, mpSettings.playerName, ALTEngine::Net::DEFAULT_PORT, chosenSkin()))
                    {
                        std::cout << "Multiplayer: could not host on port " << ALTEngine::Net::DEFAULT_PORT << "\n";
                        break;
                    }
                    session.SetGameMode(mode);
                    // The options that change how the game plays are the
                    // host's, for everyone (ModernSettings::NetworkRuleFor).
                    {
                        ALTEngine::Bootstrap::SessionRules hostRules;
                        hostRules.modernFeatures = ALTEngine::Bootstrap::ModernSettings(config).SharedFeatureBits();
                        hostRules.weaponPacing = static_cast<uint8_t>(ALTEngine::Bootstrap::WeaponPacingSettings(config).GetHere());
                        session.SetRules(hostRules);
                    }
                    session.SetPlayerLimit(mpSettings.playerLimit);
                    // No bots in a co-operative game (for now).
                    session.SetBots(coop ? 0 : mpSettings.botCount, mpSettings.botSkill);
                    std::cout << "Multiplayer: hosting " << ALTEngine::Net::GameModeName(mode) << ", level " << digits
                              << " on port " << session.Port() << ", up to " << session.PlayerLimit() << " players, "
                              << session.BotsWanted() << " bots\n";
                    session.SetQuickChat(mpSettings.messages);
                    if (!playMultiplayer(session)) { windowClosed = true; inMultiplayerMenu = false; }
                    break;
                }
                case MultiplayerMainChoice::JoinGame:
                {
                    MultiplayerJoinResult r = MultiplayerJoinScreen::Run(cdDirectory, typedAddress);
                    if (r.windowClosed) { windowClosed = true; inMultiplayerMenu = false; break; }
                    if (!r.chosen) { break; }
                    const auto address = ALTEngine::Net::Resolve(r.address, ALTEngine::Net::DEFAULT_PORT);
                    if (!address) { std::cout << "Multiplayer: can't read the address " << r.address << "\n"; break; }
                    ALTEngine::Net::Session session;
                    if (!session.StartJoin(*address, mpSettings.playerName, chosenSkin())) { break; }
                    std::cout << "Multiplayer: joining " << address->ToString() << "\n";
                    // Wait for the host's welcome (or its refusal, or nothing).
                    bool waiting = true;
                    std::string refusal;   // why we were kept out, to show
                    while (waiting)
                    {
                        SDL_Event event;
                        while (SDL_PollEvent(&event))
                        {
                            if (event.type == SDL_EVENT_QUIT) { windowClosed = true; waiting = false; }
                            else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) { waiting = false; }
                        }
                        session.Update(static_cast<double>(SDL_GetTicks()) / 1000.0);
                        for (const auto& e : session.TakeEvents())
                        {
                            if (e.kind == ALTEngine::Net::Event::Kind::Rejected)
                            {
                                refusal = JoinRefusedText(e.reason, e.hostVersion);
                                std::cout << "Multiplayer: " << refusal << "\n";
                                waiting = false;
                            }
                        }
                        if (session.Connected()) { break; }
                        if (session.GetRole() == ALTEngine::Net::Session::Role::None) { waiting = false; }
                        SDL_Delay(10);
                    }
                    if (windowClosed) { inMultiplayerMenu = false; break; }
                    if (!waiting || !session.Connected())
                    {
                        session.Stop();
                        if (!refusal.empty() && !MultiplayerNoticeScreen::Run(cdDirectory, "COULD NOT JOIN THE GAME", refusal))
                        {
                            windowClosed = true;
                            inMultiplayerMenu = false;
                        }
                        break;
                    }
                    std::cout << "Multiplayer: in as player " << session.LocalSlot() + 1 << ", "
                              << ALTEngine::Net::GameModeName(session.Mode()) << ", level " << session.Level() << "\n";
                    session.SetQuickChat(mpSettings.messages);
                    if (!playMultiplayer(session)) { windowClosed = true; inMultiplayerMenu = false; }
                    break;
                }
                case MultiplayerMainChoice::Options:
                {
                    MultiplayerSubResult r = MultiplayerOptionsScreen::Run(cdDirectory, mpSettings);
                    if (r.windowClosed) { windowClosed = true; inMultiplayerMenu = false; }
                    config.Set("MultiplayerName", mpSettings.playerName);
                    config.Set("MultiplayerSkin", ALTEngine::Net::SkinName(chosenSkin()));
                    break;
                }
                case MultiplayerMainChoice::Back:
                default:
                    inMultiplayerMenu = false;
                    break;
                }
            }
            if (windowClosed)
            {
                std::cout << "Boot window closed. Aborting.\n";
                ALTEngine::Renderer::ModelRenderer::Shutdown();
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }
            continue; // Escape/Back - return to the main menu, not exit
        }

        if (menuResult.action == "Load Game")
        {
            SaveSlotResult loadResult = SaveSlotScreen::Run(cdDirectory, SaveSlotMode::Load, CurrentSaveSlots());
            if (loadResult.windowClosed) { return closeAndExit(); }
            if (loadResult.selectedSlot)
            {
                if (auto save = ALTEngine::Game::SaveGame::Read(SaveDirectory(), *loadResult.selectedSlot))
                {
                    difficultySettings.Set(static_cast<Difficulty>(save->difficulty));
                    playerInventory = save->inventory;
                    SetPendingRestore(save->levelDigits, save->snapshot);
                    if (playCampaign(save->levelDigits) == CampaignEnd::WindowClosed) { return closeAndExit(); }
                }
                else
                {
                    std::cout << "Save slot " << *loadResult.selectedSlot << " could not be read.\n";
                }
            }
            continue;
        }

        // TEMPORARY level select - jump straight into any level for testing.
        // To remove: delete this block, the levelSelectLabels plumbing in
        // MenuController, the list in MenuTree, and MenuResult::levelCode.
        if (menuResult.action == "Level Select" && !menuResult.levelCode.empty()
            && menuResult.levelCode.find_first_of("0123456789") != std::string::npos)
        {
            std::cout << "Level Select: " << menuResult.levelCode << "\n";
            prepareInventory(playerInventory);
            if (playCampaign(DigitsOf(menuResult.levelCode)) == CampaignEnd::WindowClosed) { return closeAndExit(); }
            continue;
        }

        if (menuResult.action == "Start Game")
        {
            // The original has no level select: a new game starts at 1.1.1 and
            // the campaign carries on from there.
            prepareInventory(playerInventory);
            if (playCampaign("111") == CampaignEnd::WindowClosed) { return closeAndExit(); }
            continue;
        }

        // Any other selection (e.g. Options, handled entirely within
        // MenuController itself) - just re-show the main menu.
    }

    ALTEngine::Renderer::ModelRenderer::Shutdown();
    AppWindow::Instance().Shutdown();
    PauseBeforeExit();
    return 0;
}
