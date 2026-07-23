#pragma once

#include <filesystem>

namespace ALTEngine::Bootstrap
{
    struct DiscLocateResult
    {
        bool found = false;
        std::filesystem::path discRoot; // the drive root, e.g. "D:\"
    };

    // Scans available drives for a mounted Alien Trilogy disc - a
    // physical CD-ROM drive or a mounted disc image both just appear as
    // an ordinary drive letter from here, so the same check covers
    // either. A valid disc root contains README.TXT and CD\GFX\LEGAL.BND
    // (or .B16/.16).
    //
    // Prefers drives Windows reports as DRIVE_CDROM (the common case for
    // both a real drive and most virtual-mount tools), but falls back to
    // checking every drive structurally, since some mounting tools report
    // a different drive type.
    class DiscLocator
    {
    public:
        static DiscLocateResult FindDisc();
        static bool Validate(const std::filesystem::path& driveRoot);
    };
}
