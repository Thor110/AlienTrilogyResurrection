#include "CddaRipper.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <ntddcdrm.h>
#endif

namespace ALTEngine::Formats
{
#if defined(_WIN32)
    namespace
    {
        uint32_t MsfToLba(const UCHAR addr[4])
        {
            // addr = { Reserved, Minutes, Seconds, Frames }. -150 for the
            // standard 2-second/150-frame lead-in offset.
            return (static_cast<uint32_t>(addr[1]) * 60 + addr[2]) * 75 + addr[3] - 150;
        }
    }
#endif

    std::vector<CddaTrack> CddaRipper::ReadToc(const std::string& driveLetter)
    {
#if defined(_WIN32)
        std::string path = "\\\\.\\" + driveLetter;
        HANDLE hDevice = CreateFileA(path.c_str(), GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDevice == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("CddaRipper::ReadToc: could not open drive " + driveLetter);
        }

        CDROM_TOC toc{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(hDevice, IOCTL_CDROM_READ_TOC, nullptr, 0,
                                   &toc, sizeof(toc), &bytesReturned, nullptr);
        CloseHandle(hDevice);
        if (!ok)
        {
            throw std::runtime_error("CddaRipper::ReadToc: IOCTL_CDROM_READ_TOC failed for drive " + driveLetter);
        }

        int trackCount = toc.LastTrack - toc.FirstTrack + 1;
        if (trackCount <= 0 || trackCount >= MAXIMUM_NUMBER_TRACKS)
        {
            throw std::runtime_error("CddaRipper::ReadToc: implausible track count from TOC for drive " + driveLetter);
        }

        std::vector<CddaTrack> tracks;
        tracks.reserve(static_cast<size_t>(trackCount));
        for (int i = 0; i < trackCount; ++i)
        {
            CddaTrack t;
            t.trackNumber = toc.TrackData[i].TrackNumber;
            t.startLba = MsfToLba(toc.TrackData[i].Address);
            // Control bit 0x04 set => data track; clear => audio (CDDA).
            t.isAudio = (toc.TrackData[i].Control & 0x04) == 0;
            tracks.push_back(t);
        }

        // TrackData[trackCount] is the lead-out entry (TrackNumber 0xAA) -
        // gives the end boundary of the last real track.
        uint32_t leadOutLba = MsfToLba(toc.TrackData[trackCount].Address);
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            tracks[i].endLba = (i + 1 < tracks.size()) ? tracks[i + 1].startLba : leadOutLba;
        }

        return tracks;
#else
        (void)driveLetter;
        throw std::runtime_error("CddaRipper::ReadToc: only implemented on Windows");
#endif
    }

    void CddaRipper::RipTrackToWav(const std::string& driveLetter, const CddaTrack& track, const std::filesystem::path& outputPath)
    {
#if defined(_WIN32)
        if (!track.isAudio)
        {
            throw std::runtime_error("CddaRipper::RipTrackToWav: track " + std::to_string(track.trackNumber) + " is not an audio track");
        }
        if (track.endLba <= track.startLba)
        {
            throw std::runtime_error("CddaRipper::RipTrackToWav: track " + std::to_string(track.trackNumber) + " has an empty/invalid LBA range");
        }

        std::string path = "\\\\.\\" + driveLetter;
        HANDLE hDevice = CreateFileA(path.c_str(), GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDevice == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("CddaRipper::RipTrackToWav: could not open drive " + driveLetter);
        }

        constexpr uint32_t RAW_SECTOR_SIZE = 2352;
        // Conservative batch size - some ATAPI/driver stacks reject
        // larger single IOCTL_CDROM_RAW_READ requests. Untested here, so
        // deliberately cautious rather than optimized; reduce only after
        // confirming larger batches work on real hardware.
        constexpr uint32_t SECTORS_PER_READ = 16;

        uint32_t totalSectors = track.endLba - track.startLba;
        std::vector<uint8_t> pcmData;
        pcmData.reserve(static_cast<size_t>(totalSectors) * RAW_SECTOR_SIZE);

        std::vector<uint8_t> buffer(static_cast<size_t>(SECTORS_PER_READ) * RAW_SECTOR_SIZE);

        for (uint32_t sector = track.startLba; sector < track.endLba; sector += SECTORS_PER_READ)
        {
            uint32_t sectorsThisRead = std::min(SECTORS_PER_READ, track.endLba - sector);

            RAW_READ_INFO rawReadInfo{};
            // Documented Microsoft convention for IOCTL_CDROM_RAW_READ:
            // DiskOffset is a byte offset on 2048-byte boundaries, even
            // though actual raw sectors are 2352 bytes - NOT a typo.
            rawReadInfo.DiskOffset.QuadPart = static_cast<LONGLONG>(sector) * 2048;
            rawReadInfo.SectorCount = sectorsThisRead;
            rawReadInfo.TrackMode = CDDA;

            DWORD bytesReturned = 0;
            BOOL ok = DeviceIoControl(hDevice, IOCTL_CDROM_RAW_READ, &rawReadInfo, sizeof(rawReadInfo),
                                       buffer.data(), sectorsThisRead * RAW_SECTOR_SIZE, &bytesReturned, nullptr);
            if (!ok)
            {
                CloseHandle(hDevice);
                throw std::runtime_error(
                    "CddaRipper::RipTrackToWav: IOCTL_CDROM_RAW_READ failed at sector " + std::to_string(sector) +
                    " (track " + std::to_string(track.trackNumber) + ") - GetLastError=" + std::to_string(GetLastError()));
            }

            pcmData.insert(pcmData.end(), buffer.begin(), buffer.begin() + bytesReturned);
        }

        CloseHandle(hDevice);

        // Raw CDDA sectors are already 16-bit/44100Hz/stereo little-endian
        // PCM (that's the Red Book spec) - no conversion needed, just
        // wrap in a standard 44-byte RIFF/WAVE header.
        std::ofstream out(outputPath, std::ios::binary);
        if (!out.is_open())
        {
            throw std::runtime_error("CddaRipper::RipTrackToWav: could not create " + outputPath.string());
        }

        uint32_t dataSize = static_cast<uint32_t>(pcmData.size());
        uint32_t riffSize = 36 + dataSize;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 2;
        uint32_t sampleRate = 44100;
        uint16_t bitsPerSample = 16;
        uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
        uint16_t blockAlign = static_cast<uint16_t>(numChannels * bitsPerSample / 8);
        uint32_t fmtSize = 16;

        out.write("RIFF", 4);
        out.write(reinterpret_cast<const char*>(&riffSize), 4);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        out.write(reinterpret_cast<const char*>(&fmtSize), 4);
        out.write(reinterpret_cast<const char*>(&audioFormat), 2);
        out.write(reinterpret_cast<const char*>(&numChannels), 2);
        out.write(reinterpret_cast<const char*>(&sampleRate), 4);
        out.write(reinterpret_cast<const char*>(&byteRate), 4);
        out.write(reinterpret_cast<const char*>(&blockAlign), 2);
        out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
        out.write("data", 4);
        out.write(reinterpret_cast<const char*>(&dataSize), 4);
        out.write(reinterpret_cast<const char*>(pcmData.data()), dataSize);
#else
        (void)driveLetter;
        (void)track;
        (void)outputPath;
        throw std::runtime_error("CddaRipper::RipTrackToWav: only implemented on Windows");
#endif
    }
}
