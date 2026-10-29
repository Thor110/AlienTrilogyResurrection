#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
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

    struct CddaRipSummary
    {
        int ripped = 0;
        int failed = 0;
        // Set when nothing could be read at all (no drive, no audio, an
        // unsupported system): why, for the install log.
        std::string error;
    };

    // Raw CD audio extraction. There is no higher-level API for this on any
    // system: every CD ripper reads the table of contents and the raw 2352-byte
    // audio sectors from the drive itself.
    //
    //   Windows  IOCTL_CDROM_READ_TOC / IOCTL_CDROM_RAW_READ on \\.\D:
    //   Linux    CDROMREADTOCHDR / CDROMREADTOCENTRY / CDROMREADAUDIO on the
    //            device the disc is mounted from (/dev/sr0 and the like)
    //   macOS    the .aiff files macOS itself mounts for an audio CD's tracks,
    //            converted to WAV; failing that, DKIOCCDREADTOC / DKIOCCDREAD
    //            on the whole disc's raw device (/dev/rdiskN)
    //
    // IMPORTANT: the drive code is UNTESTED on all three systems - written
    // against each system's documented interface, but no machine it has been
    // written on had an optical drive. The pure parts (WAV and AIFF, the mount
    // table, the macOS TOC) are covered by tests.
    class CddaRipper
    {
    public:
        // The drive to read for a disc mounted at `discRoot`: "D:" on
        // Windows, the device node elsewhere ("/dev/sr0", "/dev/rdisk2").
        // Empty if it can't be found.
        static std::string DeviceForDisc(const std::filesystem::path& discRoot);

        // Reads the disc's table of contents from `device` (DeviceForDisc).
        // Returns all tracks, audio and data alike - filter by `isAudio` for
        // CDDA tracks (2-17 for this disc, per the game's own track layout).
        static std::vector<CddaTrack> ReadToc(const std::string& device);

        // Rips one audio track to a 16-bit/44100Hz/stereo PCM .wav file at
        // `outputPath`. Throws on a non-audio track, or on any read or file
        // failure.
        static void RipTrackToWav(const std::string& device, const CddaTrack& track, const std::filesystem::path& outputPath);

        // The whole job for the installer: every audio track of the disc
        // mounted at `discRoot` into `musicDir` as trackNN.wav, by whichever
        // route this system has. `log` gets one line per track.
        static CddaRipSummary RipDisc(const std::filesystem::path& discRoot, const std::filesystem::path& musicDir,
                                      const std::function<void(const std::string&)>& log);
    };

    // The system-independent pieces, exposed for the tests.
    namespace CddaDetail
    {
        // Raw CDDA sectors are already 16-bit/44100Hz/stereo little-endian PCM
        // (the Red Book format), so a WAV is the 44-byte RIFF header and them.
        void WriteWav(const std::filesystem::path& outputPath, const std::vector<uint8_t>& pcm);

        // An AIFF's sound data as little-endian 16-bit PCM, if it is the CD's
        // own format (16-bit, 2 channels, 44100 Hz) - what macOS's cddafs
        // serves for an audio CD's tracks. Empty if it isn't.
        std::vector<uint8_t> AiffToPcm(const std::vector<uint8_t>& aiff);

        // The device a mount point comes from, from a mount table in
        // /proc/mounts form ("device mountpoint type options 0 0" a line):
        // the longest mount point that `path` sits under. Octal escapes in
        // the table (\040 for a space) are undone.
        std::string DeviceFromMountTable(const std::string& table, const std::string& path);

        // macOS: a mounted CD's device ("/dev/disk2s1", "/dev/disk2") as the
        // whole disc's raw device ("/dev/rdisk2"). Empty if it isn't one.
        std::string WholeRawDisk(const std::string& device);

        // macOS: a raw TOC (DKIOCCDREADTOC with kCDTOCFormatTOC: a 4-byte
        // header, then 11-byte descriptors) as tracks. Points 1-99 are the
        // tracks, 0xA2 the lead-out; times are MSF, less the 150-frame
        // lead-in.
        std::vector<CddaTrack> ParseRawToc(const std::vector<uint8_t>& toc);

        // The track number an audio-CD file's name starts with ("3 Audio
        // Track.aiff" -> 3), or nothing.
        std::optional<int> LeadingTrackNumber(const std::string& fileName);
    }
}
