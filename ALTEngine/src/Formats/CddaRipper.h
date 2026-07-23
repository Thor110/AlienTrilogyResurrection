#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    struct CddaTrack
    {
        int trackNumber = 0;
        uint32_t startLba = 0;
        uint32_t endLba = 0;    // exclusive - start of the next track (or lead-out for the last track)
        bool isAudio = false;   // false = data track (track 1)
    };

    // Windows-only raw CD-ROM audio extraction (IOCTL_CDROM_READ_TOC /
    // IOCTL_CDROM_RAW_READ - there is no higher-level API for this; every
    // CD ripper goes through the same raw sector interface). No-op /
    // throws on non-Windows builds.
    //
    // IMPORTANT: this code is UNTESTED. It's written directly against
    // documented Windows DDK structures and IOCTLs, but this sandbox has
    // no Windows headers to compile it against and no real optical drive
    // (physical or mounted image) to run it against - unlike everything
    // else in this codebase, this hasn't been compiled or exercised at
    // all. Treat it as a first draft that needs real testing on your
    // machine, not verified working code.
    class CddaRipper
    {
    public:
        // Reads the disc's table of contents from `driveLetter` (e.g.
        // "D:"). Returns all tracks, audio and data alike - filter by
        // `isAudio` for CDDA tracks (2-17 for this disc, per the game's
        // own track layout).
        static std::vector<CddaTrack> ReadToc(const std::string& driveLetter);

        // Rips one audio track to a 16-bit/44100Hz/stereo PCM .wav file
        // at `outputPath`. Throws on non-Windows, on a non-audio track,
        // or on any IOCTL/file failure.
        static void RipTrackToWav(const std::string& driveLetter, const CddaTrack& track, const std::filesystem::path& outputPath);
    };
}
