#include "DiscLocator.h"
#include "FsUtil.h"

#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#include <windows.h>
#endif

namespace ALTEngine::Bootstrap
{
    bool DiscLocator::Validate(const std::filesystem::path& driveRoot)
    {
        // TRILOGY.ICO is present on every release regardless of the
        // executable name (which varies - e.g. WTRILOGY.EXE on the German
        // release, not TRILOGY.EXE).
        if (!HasEntryCaseInsensitive(driveRoot, "TRILOGY.ICO")) { return false; }

        auto cdDir = FindEntryCaseInsensitive(driveRoot, "CD");
        if (!cdDir.has_value()) { return false; }

        auto gfxDir = FindEntryCaseInsensitive(*cdDir, "GFX");
        if (!gfxDir.has_value()) { return false; }

        // Specifically LEGAL.BND - not every file has a .B16 compressed
        // counterpart (that's inconsistent per-file, not something to
        // rely on for a marker check), but LEGAL.BND itself is confirmed
        // present on every release.
        return HasEntryCaseInsensitive(*gfxDir, "LEGAL.BND");
    }

    DiscLocateResult DiscLocator::FindDisc()
    {
#if defined(_WIN32)
        std::vector<std::filesystem::path> cdromDrives;
        std::vector<std::filesystem::path> otherDrives;

        DWORD mask = GetLogicalDrives();
        for (int i = 0; i < 26; ++i)
        {
            if (!((mask >> i) & 1)) { continue; }
            std::string letter(1, static_cast<char>('A' + i));
            std::filesystem::path drive(letter + ":\\");

            UINT type = GetDriveTypeA(drive.string().c_str());
            if (type == DRIVE_NO_ROOT_DIR || type == DRIVE_UNKNOWN) { continue; }

            if (type == DRIVE_CDROM) { cdromDrives.push_back(drive); }
            else { otherDrives.push_back(drive); }
        }

        // Prefer actual CD-ROM-typed drives first (real drive or most
        // virtual-mount tools), then fall back to checking everything
        // else structurally.
        for (const auto& drive : cdromDrives)
        {
            if (Validate(drive)) { return { true, drive }; }
        }
        for (const auto& drive : otherDrives)
        {
            if (Validate(drive)) { return { true, drive }; }
        }
#endif
        return { false, {} };
    }
}
