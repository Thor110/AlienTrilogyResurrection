#include <iostream>
#include <optional>

#include "Bootstrap/AppWindow.h"
#include "Bootstrap/Config.h"
#include "Bootstrap/DiscLocator.h"
#include "Bootstrap/GameLocator.h"
#include "Bootstrap/ImageDisplay.h"
#include "Bootstrap/Localization.h"
#include "Bootstrap/PlatformPaths.h"
#include "Bootstrap/RenderSettings.h"
#include "Formats/CddaRipper.h"
#include "Formats/DiscManifest.h"
#include "Formats/Installer.h"
#include "Formats/PatchLoader.h"
#include "Formats/PatchRunner.h"
#include "Formats/SplashImageLoader.h"
#include "Menu/MenuController.h"
#include "Video/VideoPlayer.h"

using namespace ALTEngine::Bootstrap;
using namespace ALTEngine::Formats;
using namespace ALTEngine::Video;
using namespace ALTEngine::Menu;

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

    void RunPatches(const std::filesystem::path& cdDirectory)
    {
        std::filesystem::path patchesJson = ExecutableDirectory() / "data" / "Patches.json";

        std::vector<PatchOperation> operations;
        try
        {
            operations = PatchLoader::Load(patchesJson);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load " << patchesJson.string() << ": " << e.what() << "\n";
            return;
        }

        std::cout << "Applying " << operations.size() << " patch operations...\n";
        std::vector<PatchResult> results = PatchRunner::ApplyAll(cdDirectory, operations);

        size_t applied = 0, alreadyApplied = 0, failed = 0;
        for (const auto& result : results)
        {
            switch (result.outcome)
            {
            case PatchOutcome::Applied:
                ++applied;
                std::cout << "  [applied]  " << result.operation->targetFile;
                if (!result.operation->note.empty()) { std::cout << "  (" << result.operation->note << ")"; }
                std::cout << "\n";
                break;
            case PatchOutcome::AlreadyApplied:
                ++alreadyApplied;
                break;
            case PatchOutcome::Failed:
                ++failed;
                std::cout << "  [FAILED]   " << result.operation->targetFile << " - " << result.error << "\n";
                break;
            }
        }

        std::cout << "Patches: " << applied << " applied, " << alreadyApplied
                   << " already applied, " << failed << " failed.\n";
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
            std::cout << "Could not find LEGAL graphics file under " << gfxDir.string() << " - skipping splash.\n";
            return true;
        }
        std::error_code ec;
        if (!std::filesystem::exists(palPath, ec))
        {
            std::cout << "Could not find " << palPath.string() << " - skipping splash.\n";
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

    // Plays the four intro videos in the documented order (see
    // data/BootSequence.json), resolved for `language` via
    // LocalizedBaseName. A missing file is skipped, same philosophy as
    // ShowLegalSplash - only an actual window-close aborts the boot.
    // TODO: drive this list from BootSequence.json directly rather than
    // hardcoding it here, once there's a general boot-step runner.
    bool PlayIntroVideos(const std::filesystem::path& cdDirectory, Language language)
    {
        std::filesystem::path aviDir = cdDirectory / "AVI";
        const char* baseNames[] = { "FOXDKAUD", "ALOGODUK", "PRBLOGO", "INTRO" };

        for (const char* baseName : baseNames)
        {
            std::string localizedName = LocalizedBaseName(baseName, language);
            std::filesystem::path path = aviDir / (localizedName + ".AVI");
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
            {
                std::cout << "Could not find " << path.string() << " - skipping.\n";
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

    // Runs a fresh install from a located disc: copies the file manifest,
    // then rips the 16 CDDA music tracks. File copying is well-tested;
    // CDDA ripping (Windows raw CD-ROM IOCTLs) is NOT - see CddaRipper.h.
    // A CDDA failure is logged but doesn't fail the whole install, since
    // the game is playable without music, just quieter.
    bool InstallFromDisc(const std::filesystem::path& discRoot, const std::filesystem::path& destination)
    {
        std::cout << "Installing from disc (" << discRoot.string() << ") to " << destination.string() << "...\n";

        std::filesystem::path manifestPath = ExecutableDirectory() / "data" / "DiscFileManifest.json";
        DiscManifest manifest;
        try
        {
            manifest = DiscManifestLoader::Load(manifestPath);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load " << manifestPath.string() << ": " << e.what() << "\n";
            return false;
        }

        InstallResult copyResult = Installer::CopyFiles(discRoot, destination, manifest,
            [](const InstallProgress& p) {
                if (p.filesCompleted % 25 == 0 || p.filesCompleted == p.filesTotal)
                {
                    std::cout << "  [" << p.filesCompleted << "/" << p.filesTotal << "] " << p.currentFile << "\n";
                }
            });

        if (!copyResult.success)
        {
            std::cout << "Install incomplete - " << copyResult.failedFiles.size() << " file(s) failed:\n";
            for (const auto& f : copyResult.failedFiles) { std::cout << "  " << f << "\n"; }
            return false;
        }
        std::cout << "File copy complete.\n";

        // CDDA rip - drive letter only (e.g. "D:"), not the trailing
        // backslash discRoot carries.
        std::string driveLetter = discRoot.string().substr(0, 2);
        std::filesystem::path musicDir = destination / "CD" / "MUSIC";
        std::error_code ec;
        std::filesystem::create_directories(musicDir, ec);

        try
        {
            std::vector<CddaTrack> tracks = CddaRipper::ReadToc(driveLetter);
            int ripped = 0, failed = 0;
            for (const auto& track : tracks)
            {
                if (!track.isAudio) { continue; } // track 1 is the data track
                try
                {
                    char nameBuf[32];
                    std::snprintf(nameBuf, sizeof(nameBuf), "track%02d.wav", track.trackNumber);
                    std::cout << "  Ripping " << nameBuf << "...\n";
                    CddaRipper::RipTrackToWav(driveLetter, track, musicDir / nameBuf);
                    ++ripped;
                }
                catch (const std::exception& e)
                {
                    std::cout << "  Failed to rip track " << track.trackNumber << ": " << e.what() << "\n";
                    ++failed;
                }
            }
            std::cout << "CDDA rip: " << ripped << " tracks ripped, " << failed << " failed.\n";
        }
        catch (const std::exception& e)
        {
            std::cout << "CDDA rip failed entirely (TOC read): " << e.what() << "\n";
            std::cout << "Continuing without music - this can be retried later.\n";
        }

        return true;
    }
}

int main(int, char**)
{
    std::cout << "ALTEngine boot starting...\n";

    Config config;
    std::cout << "Config file: " << config.FilePath().string() << "\n";

    GameLocator locator(config);
    std::cout << "Locating install directory...\n";

    if (!locator.TryAutoLocate().has_value())
    {
        DiscLocateResult disc = DiscLocator::FindDisc();
        if (disc.found)
        {
            std::filesystem::path destination = ExecutableDirectory() / "GameData";
            std::cout << "Found disc at " << disc.discRoot.string() << "\n";
            if (InstallFromDisc(disc.discRoot, destination))
            {
                config.Set("GameDirectory", destination.string());
            }
        }
    }

    LocateResult result = locator.Locate();
    if (!result.success)
    {
        std::cout << "No install directory selected. Aborting.\n";
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    std::cout << "Installation directory: " << result.gameDirectory.string() << "\n";

    std::filesystem::path cdDirectory = result.gameDirectory / "CD";
    RunPatches(cdDirectory);

    if (!ShowLegalSplash(cdDirectory))
    {
        std::cout << "Boot window closed. Aborting.\n";
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    // No settings/persistence system yet - defaults to English until
    // there's somewhere for a language choice to actually live.
    Language language = Language::English;

    if (!PlayIntroVideos(cdDirectory, language))
    {
        std::cout << "Boot window closed. Aborting.\n";
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    RenderSettings renderSettings(config);
    MenuResult menuResult = MenuController::Run(cdDirectory, renderSettings, language);
    if (menuResult.windowClosed)
    {
        std::cout << "Boot window closed. Aborting.\n";
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }
    std::cout << "Menu selection: " << menuResult.action << "\n";

    // NEXT: actually act on menuResult.action (Start Game / Multiplayer /
    // Load Game), and the real 3D model renderer (SDL GPU API) to replace
    // the menu's placeholder boxes.

    AppWindow::Instance().Shutdown();
    PauseBeforeExit();
    return 0;
}
