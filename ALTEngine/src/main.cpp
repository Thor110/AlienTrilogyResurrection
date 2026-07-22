#include <iostream>
#include <optional>

#include "Bootstrap/Config.h"
#include "Bootstrap/GameLocator.h"
#include "Bootstrap/ImageDisplay.h"
#include "Bootstrap/PlatformPaths.h"
#include "Formats/PatchLoader.h"
#include "Formats/PatchRunner.h"
#include "Formats/SplashImageLoader.h"

using namespace ALTEngine::Bootstrap;
using namespace ALTEngine::Formats;

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

    void RunPatches(const std::filesystem::path& gameDirectory)
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
        std::vector<PatchResult> results = PatchRunner::ApplyAll(gameDirectory, operations);

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
    // (data/BootSequence.json), after everything needed to find and patch
    // the install is already done. Returns false only if the user closed
    // the window outright (treated the same as DirectoryBrowser's abort).
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
            return ImageDisplay::Show(image.rgba, image.width, image.height);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load/show LEGAL splash: " << e.what() << "\n";
            return true; // don't abort the whole boot over a missing splash
        }
    }
}

int main(int, char**)
{
    std::cout << "ALTEngine boot starting...\n";

    Config config;
    std::cout << "Config file: " << config.FilePath().string() << "\n";

    GameLocator locator(config);
    std::cout << "Locating install directory...\n";

    LocateResult result = locator.Locate();
    if (!result.success)
    {
        std::cout << "No install directory selected. Aborting.\n";
        PauseBeforeExit();
        return 1;
    }

    std::cout << "Installation directory: " << result.gameDirectory.string() << "\n";

    std::filesystem::path cdDirectory = result.gameDirectory / "CD";
    RunPatches(cdDirectory);

    // NEXT per data/BootSequence.json: AVI playback (FOXDKAUD, ALOGODUK,
    // PRBLOGO, INTRO), then LOGOSGFX as the main menu background.
    if (!ShowLegalSplash(cdDirectory))
    {
        std::cout << "Boot window closed. Aborting.\n";
        PauseBeforeExit();
        return 1;
    }

    PauseBeforeExit();
    return 0;
}
