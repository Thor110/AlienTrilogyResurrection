#include "CddaRipper.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <ntddcdrm.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <unistd.h>
#endif

namespace ALTEngine::Formats
{
    namespace
    {
        constexpr uint32_t RAW_SECTOR_SIZE = 2352;
        constexpr uint32_t LEAD_IN_FRAMES = 150;   // the 2-second lead-in every MSF address counts

        uint32_t MsfToLba(uint8_t minutes, uint8_t seconds, uint8_t frames)
        {
            return (static_cast<uint32_t>(minutes) * 60 + seconds) * 75 + frames - LEAD_IN_FRAMES;
        }

        // Track ends: each track runs to the next one's start, the last to the
        // lead-out.
        void FillEnds(std::vector<CddaTrack>& tracks, uint32_t leadOutLba)
        {
            for (size_t i = 0; i < tracks.size(); ++i)
            {
                tracks[i].endLba = (i + 1 < tracks.size()) ? tracks[i + 1].startLba : leadOutLba;
            }
        }

        uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
        uint16_t Be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

        // ---- the drive, one per system -------------------------------------
        //
        // Open, read the TOC, read raw audio sectors, close. Every failure
        // throws with the system's own reason.

#if defined(_WIN32)
        struct Drive
        {
            HANDLE handle = INVALID_HANDLE_VALUE;
            explicit Drive(const std::string& device)
            {
                const std::string path = "\\\\.\\" + device;
                handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     nullptr, OPEN_EXISTING, 0, nullptr);
                if (handle == INVALID_HANDLE_VALUE)
                {
                    throw std::runtime_error("could not open drive " + device);
                }
            }
            ~Drive() { if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); } }
            Drive(const Drive&) = delete;
            Drive& operator=(const Drive&) = delete;

            std::vector<CddaTrack> ReadToc()
            {
                CDROM_TOC toc{};
                DWORD bytesReturned = 0;
                if (!DeviceIoControl(handle, IOCTL_CDROM_READ_TOC, nullptr, 0, &toc, sizeof(toc), &bytesReturned, nullptr))
                {
                    throw std::runtime_error("IOCTL_CDROM_READ_TOC failed, GetLastError=" + std::to_string(GetLastError()));
                }
                const int trackCount = toc.LastTrack - toc.FirstTrack + 1;
                if (trackCount <= 0 || trackCount >= MAXIMUM_NUMBER_TRACKS)
                {
                    throw std::runtime_error("implausible track count in the TOC");
                }
                std::vector<CddaTrack> tracks;
                for (int i = 0; i < trackCount; ++i)
                {
                    CddaTrack t;
                    t.trackNumber = toc.TrackData[i].TrackNumber;
                    const UCHAR* a = toc.TrackData[i].Address;   // { reserved, M, S, F }
                    t.startLba = MsfToLba(a[1], a[2], a[3]);
                    // Control bit 0x04 set => data track; clear => audio (CDDA).
                    t.isAudio = (toc.TrackData[i].Control & 0x04) == 0;
                    tracks.push_back(t);
                }
                // TrackData[trackCount] is the lead-out entry (TrackNumber 0xAA).
                const UCHAR* lo = toc.TrackData[trackCount].Address;
                FillEnds(tracks, MsfToLba(lo[1], lo[2], lo[3]));
                return tracks;
            }

            size_t ReadAudio(uint32_t lba, uint32_t sectors, uint8_t* out)
            {
                RAW_READ_INFO info{};
                // Microsoft's convention for IOCTL_CDROM_RAW_READ: DiskOffset is
                // a byte offset in 2048-byte units, though raw sectors are 2352
                // bytes - NOT a typo.
                info.DiskOffset.QuadPart = static_cast<LONGLONG>(lba) * 2048;
                info.SectorCount = sectors;
                info.TrackMode = CDDA;
                DWORD bytesReturned = 0;
                if (!DeviceIoControl(handle, IOCTL_CDROM_RAW_READ, &info, sizeof(info),
                                     out, sectors * RAW_SECTOR_SIZE, &bytesReturned, nullptr))
                {
                    throw std::runtime_error("IOCTL_CDROM_RAW_READ failed at sector " + std::to_string(lba) +
                                             ", GetLastError=" + std::to_string(GetLastError()));
                }
                return bytesReturned;
            }
        };
#elif defined(__linux__)
        struct Drive
        {
            int fd = -1;
            explicit Drive(const std::string& device)
            {
                // O_NONBLOCK: a CD drive otherwise refuses to open while it
                // considers itself not ready, as cdparanoia and others do.
                fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
                if (fd < 0)
                {
                    throw std::runtime_error("could not open " + device + ": " + std::strerror(errno));
                }
            }
            ~Drive() { if (fd >= 0) { close(fd); } }
            Drive(const Drive&) = delete;
            Drive& operator=(const Drive&) = delete;

            std::vector<CddaTrack> ReadToc()
            {
                cdrom_tochdr header{};
                if (ioctl(fd, CDROMREADTOCHDR, &header) != 0)
                {
                    throw std::runtime_error(std::string("CDROMREADTOCHDR failed: ") + std::strerror(errno));
                }
                std::vector<CddaTrack> tracks;
                for (int number = header.cdth_trk0; number <= header.cdth_trk1; ++number)
                {
                    cdrom_tocentry entry{};
                    entry.cdte_track = static_cast<__u8>(number);
                    entry.cdte_format = CDROM_LBA;
                    if (ioctl(fd, CDROMREADTOCENTRY, &entry) != 0)
                    {
                        throw std::runtime_error("CDROMREADTOCENTRY failed for track " + std::to_string(number) +
                                                 ": " + std::strerror(errno));
                    }
                    CddaTrack t;
                    t.trackNumber = number;
                    t.startLba = static_cast<uint32_t>(entry.cdte_addr.lba);   // LBA already: no lead-in to take off
                    t.isAudio = (entry.cdte_ctrl & CDROM_DATA_TRACK) == 0;
                    tracks.push_back(t);
                }
                cdrom_tocentry leadOut{};
                leadOut.cdte_track = CDROM_LEADOUT;
                leadOut.cdte_format = CDROM_LBA;
                if (ioctl(fd, CDROMREADTOCENTRY, &leadOut) != 0)
                {
                    throw std::runtime_error(std::string("CDROMREADTOCENTRY failed for the lead-out: ") + std::strerror(errno));
                }
                FillEnds(tracks, static_cast<uint32_t>(leadOut.cdte_addr.lba));
                return tracks;
            }

            size_t ReadAudio(uint32_t lba, uint32_t sectors, uint8_t* out)
            {
                cdrom_read_audio request{};
                request.addr.lba = static_cast<int>(lba);
                request.addr_format = CDROM_LBA;
                request.nframes = static_cast<int>(sectors);   // the kernel takes at most 75 at a time
                request.buf = out;
                if (ioctl(fd, CDROMREADAUDIO, &request) != 0)
                {
                    throw std::runtime_error("CDROMREADAUDIO failed at sector " + std::to_string(lba) + ": " +
                                             std::strerror(errno));
                }
                return static_cast<size_t>(sectors) * RAW_SECTOR_SIZE;
            }
        };
#elif defined(__APPLE__)
        struct Drive
        {
            int fd = -1;
            explicit Drive(const std::string& device)
            {
                fd = open(device.c_str(), O_RDONLY);
                if (fd < 0)
                {
                    throw std::runtime_error("could not open " + device + ": " + std::strerror(errno));
                }
            }
            ~Drive() { if (fd >= 0) { close(fd); } }
            Drive(const Drive&) = delete;
            Drive& operator=(const Drive&) = delete;

            std::vector<CddaTrack> ReadToc()
            {
                std::vector<uint8_t> buffer(4 + 100 * 11 + 64);
                dk_cd_read_toc_t request{};
                request.format = kCDTOCFormatTOC;
                request.formatAsTime = 1;
                request.bufferLength = static_cast<UInt16>(buffer.size());
                request.buffer = buffer.data();
                if (ioctl(fd, DKIOCCDREADTOC, &request) != 0)
                {
                    throw std::runtime_error(std::string("DKIOCCDREADTOC failed: ") + std::strerror(errno));
                }
                buffer.resize(request.bufferLength);
                return CddaDetail::ParseRawToc(buffer);
            }

            size_t ReadAudio(uint32_t lba, uint32_t sectors, uint8_t* out)
            {
                dk_cd_read_t request{};
                // The offset counts sectors of the area being read: 2352 bytes
                // each for CDDA.
                request.offset = static_cast<UInt64>(lba) * kCDSectorSizeCDDA;
                request.sectorArea = kCDSectorAreaUser;
                request.sectorType = kCDSectorTypeCDDA;
                request.bufferLength = sectors * kCDSectorSizeCDDA;
                request.buffer = out;
                if (ioctl(fd, DKIOCCDREAD, &request) != 0)
                {
                    throw std::runtime_error("DKIOCCDREAD failed at sector " + std::to_string(lba) + ": " +
                                             std::strerror(errno));
                }
                return request.bufferLength;
            }
        };
#else
        struct Drive
        {
            explicit Drive(const std::string&) { throw std::runtime_error("CD audio isn't supported on this system"); }
            std::vector<CddaTrack> ReadToc() { return {}; }
            size_t ReadAudio(uint32_t, uint32_t, uint8_t*) { return 0; }
        };
#endif

#if defined(__APPLE__)
        std::vector<uint8_t> ReadWholeFile(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open()) { return {}; }
            const auto size = static_cast<size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            std::vector<uint8_t> data(size);
            if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) { return {}; }
            return data;
        }

        // macOS mounts an audio CD's tracks as files ("2 Audio Track.aiff")
        // on a volume of their own under /Volumes (cddafs). Which volume, and
        // its tracks by number; empty if there is none.
        std::map<int, std::filesystem::path> FindAudioCdTracks()
        {
            std::map<int, std::filesystem::path> best;
            std::error_code ec;
            for (std::filesystem::directory_iterator volume("/Volumes", ec), end; !ec && volume != end; volume.increment(ec))
            {
                std::map<int, std::filesystem::path> tracks;
                std::error_code inner;
                for (std::filesystem::directory_iterator file(volume->path(), inner), fileEnd; !inner && file != fileEnd; file.increment(inner))
                {
                    std::string ext = file->path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (ext != ".aiff" && ext != ".aif") { continue; }
                    if (auto number = CddaDetail::LeadingTrackNumber(file->path().filename().string()))
                    {
                        tracks[*number] = file->path();
                    }
                }
                if (tracks.size() > best.size()) { best = std::move(tracks); }
            }
            return best;
        }
#endif
    }

    // ---- the system-independent pieces --------------------------------------

    namespace CddaDetail
    {
        void WriteWav(const std::filesystem::path& outputPath, const std::vector<uint8_t>& pcm)
        {
            std::ofstream out(outputPath, std::ios::binary);
            if (!out.is_open())
            {
                throw std::runtime_error("could not create " + outputPath.string());
            }
            auto le32 = [&](uint32_t v) { const char b[4] = { char(v), char(v >> 8), char(v >> 16), char(v >> 24) }; out.write(b, 4); };
            auto le16 = [&](uint16_t v) { const char b[2] = { char(v), char(v >> 8) }; out.write(b, 2); };
            const uint32_t dataSize = static_cast<uint32_t>(pcm.size());
            out.write("RIFF", 4); le32(36 + dataSize);
            out.write("WAVE", 4);
            out.write("fmt ", 4); le32(16);
            le16(1);                      // PCM
            le16(2);                      // stereo
            le32(44100);                  // samples a second
            le32(44100 * 2 * 2);          // bytes a second
            le16(4);                      // bytes a frame
            le16(16);                     // bits a sample
            out.write("data", 4); le32(dataSize);
            out.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(pcm.size()));
            if (!out) { throw std::runtime_error("could not write " + outputPath.string()); }
        }

        std::vector<uint8_t> AiffToPcm(const std::vector<uint8_t>& aiff)
        {
            if (aiff.size() < 12 || std::memcmp(aiff.data(), "FORM", 4) != 0) { return {}; }
            const bool aifc = std::memcmp(aiff.data() + 8, "AIFC", 4) == 0;
            if (!aifc && std::memcmp(aiff.data() + 8, "AIFF", 4) != 0) { return {}; }

            bool formatOk = false;
            const uint8_t* sound = nullptr;
            size_t soundSize = 0;
            size_t pos = 12;
            while (pos + 8 <= aiff.size())
            {
                const uint8_t* chunk = aiff.data() + pos;
                const uint32_t size = Be32(chunk + 4);
                const size_t body = pos + 8;
                if (body + size > aiff.size()) { break; }
                if (std::memcmp(chunk, "COMM", 4) == 0 && size >= 18)
                {
                    const uint8_t* c = aiff.data() + body;
                    const uint16_t channels = Be16(c);
                    const uint16_t bits = Be16(c + 6);
                    // The rate is an 80-bit extended float; 44100 is exponent
                    // 0x400e with mantissa 0xac44 << 48.
                    const int exponent = ((c[8] & 0x7f) << 8 | c[9]) - 16383;
                    uint64_t mantissa = 0;
                    for (int i = 0; i < 8; ++i) { mantissa = (mantissa << 8) | c[10 + i]; }
                    const uint64_t rate = (exponent >= 0 && exponent < 64) ? (mantissa >> (63 - exponent)) : 0;
                    bool uncompressed = true;
                    if (aifc && size >= 22) { uncompressed = std::memcmp(c + 18, "NONE", 4) == 0; }
                    formatOk = channels == 2 && bits == 16 && rate == 44100 && uncompressed;
                }
                else if (std::memcmp(chunk, "SSND", 4) == 0 && size >= 8)
                {
                    const uint32_t offset = Be32(aiff.data() + body);
                    if (8u + offset <= size)
                    {
                        sound = aiff.data() + body + 8 + offset;
                        soundSize = size - 8 - offset;
                    }
                }
                pos = body + size + (size & 1);   // chunks are padded to an even length
            }
            if (!formatOk || !sound) { return {}; }

            // AIFF is big-endian; a WAV little-endian.
            std::vector<uint8_t> pcm(sound, sound + (soundSize & ~size_t(1)));
            for (size_t i = 0; i + 1 < pcm.size(); i += 2) { std::swap(pcm[i], pcm[i + 1]); }
            return pcm;
        }

        std::string DeviceFromMountTable(const std::string& table, const std::string& path)
        {
            auto unescape = [](const std::string& s) {
                std::string out;
                for (size_t i = 0; i < s.size(); ++i)
                {
                    // "\\040": a backslash and three octal digits.
                    if (s[i] == '\\' && i + 3 < s.size()
                        && std::isdigit(static_cast<unsigned char>(s[i + 1])))
                    {
                        out.push_back(static_cast<char>(std::stoi(s.substr(i + 1, 3), nullptr, 8)));
                        i += 3;
                    }
                    else { out.push_back(s[i]); }
                }
                return out;
            };
            auto under = [](const std::string& p, const std::string& mount) {
                if (mount == "/") { return true; }
                return p == mount || (p.size() > mount.size() && p.compare(0, mount.size(), mount) == 0 && p[mount.size()] == '/');
            };

            std::string best, bestMount;
            std::istringstream lines(table);
            std::string line;
            while (std::getline(lines, line))
            {
                std::istringstream fields(line);
                std::string device, mount;
                if (!(fields >> device >> mount)) { continue; }
                mount = unescape(mount);
                if (under(path, mount) && mount.size() >= bestMount.size())
                {
                    best = unescape(device);
                    bestMount = mount;
                }
            }
            return best;
        }

        std::string WholeRawDisk(const std::string& device)
        {
            // "/dev/disk2s1" -> "disk2"; "/dev/rdisk2" stays.
            const std::string prefix = "/dev/";
            if (device.compare(0, prefix.size(), prefix) != 0) { return {}; }
            std::string name = device.substr(prefix.size());
            if (name.compare(0, 1, "r") == 0) { name = name.substr(1); }
            if (name.compare(0, 4, "disk") != 0) { return {}; }
            size_t end = 4;
            while (end < name.size() && std::isdigit(static_cast<unsigned char>(name[end]))) { ++end; }
            if (end == 4) { return {}; }
            return prefix + "r" + name.substr(0, end);
        }

        std::vector<CddaTrack> ParseRawToc(const std::vector<uint8_t>& toc)
        {
            if (toc.size() < 4) { throw std::runtime_error("the TOC is too short"); }
            // The length counts the bytes after itself.
            const size_t length = std::min<size_t>(toc.size(), size_t(Be16(toc.data())) + 2);
            std::vector<CddaTrack> tracks;
            std::optional<uint32_t> leadOut;
            for (size_t pos = 4; pos + 11 <= length; pos += 11)
            {
                const uint8_t* d = toc.data() + pos;
                const uint8_t control = d[1] & 0x0f;
                const uint8_t point = d[3];
                const uint32_t lba = MsfToLba(d[8], d[9], d[10]);
                if (point >= 1 && point <= 99)
                {
                    CddaTrack t;
                    t.trackNumber = point;
                    t.startLba = lba;
                    t.isAudio = (control & 0x04) == 0;
                    tracks.push_back(t);
                }
                else if (point == 0xa2 && !leadOut)
                {
                    // The first session's lead-out: a CD-Extra's data in a
                    // later session doesn't belong to the last audio track.
                    leadOut = lba;
                }
            }
            if (tracks.empty() || !leadOut) { throw std::runtime_error("no tracks or no lead-out in the TOC"); }
            std::sort(tracks.begin(), tracks.end(), [](const CddaTrack& a, const CddaTrack& b) { return a.trackNumber < b.trackNumber; });
            FillEnds(tracks, *leadOut);
            // A later session's first track starts after this session's
            // lead-out, so the last track of the first session ends there.
            for (auto& t : tracks) { if (t.endLba > *leadOut && t.startLba < *leadOut) { t.endLba = *leadOut; } }
            return tracks;
        }

        std::optional<int> LeadingTrackNumber(const std::string& fileName)
        {
            size_t end = 0;
            while (end < fileName.size() && std::isdigit(static_cast<unsigned char>(fileName[end]))) { ++end; }
            if (end == 0 || end > 2) { return std::nullopt; }
            const int number = std::stoi(fileName.substr(0, end));
            if (number < 1 || number > 99) { return std::nullopt; }
            return number;
        }
    }

    // ---- the ripper ---------------------------------------------------------

    std::string CddaRipper::DeviceForDisc(const std::filesystem::path& discRoot)
    {
#if defined(_WIN32)
        // The drive letter only ("D:"), not the trailing backslash.
        return discRoot.string().substr(0, 2);
#elif defined(__linux__)
        std::error_code ec;
        const auto path = std::filesystem::weakly_canonical(discRoot, ec);
        std::ifstream in("/proc/self/mounts");
        std::stringstream table;
        table << in.rdbuf();
        return CddaDetail::DeviceFromMountTable(table.str(), (ec ? discRoot : path).string());
#elif defined(__APPLE__)
        struct statfs info{};
        if (statfs(discRoot.string().c_str(), &info) != 0) { return {}; }
        return CddaDetail::WholeRawDisk(info.f_mntfromname);
#else
        (void)discRoot;
        return {};
#endif
    }

    std::vector<CddaTrack> CddaRipper::ReadToc(const std::string& device)
    {
        try
        {
            Drive drive(device);
            return drive.ReadToc();
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("CddaRipper::ReadToc: " + std::string(e.what()));
        }
    }

    void CddaRipper::RipTrackToWav(const std::string& device, const CddaTrack& track, const std::filesystem::path& outputPath)
    {
        if (!track.isAudio)
        {
            throw std::runtime_error("CddaRipper::RipTrackToWav: track " + std::to_string(track.trackNumber) + " is not an audio track");
        }
        if (track.endLba <= track.startLba)
        {
            throw std::runtime_error("CddaRipper::RipTrackToWav: track " + std::to_string(track.trackNumber) + " has an empty/invalid LBA range");
        }

        // A conservative batch: some drivers refuse larger single reads, and
        // Linux takes at most 75 sectors a request. Raise it only once larger
        // batches are known to work on real drives.
        constexpr uint32_t SECTORS_PER_READ = 16;

        std::vector<uint8_t> pcm;
        try
        {
            Drive drive(device);
            const uint32_t totalSectors = track.endLba - track.startLba;
            pcm.reserve(static_cast<size_t>(totalSectors) * RAW_SECTOR_SIZE);
            std::vector<uint8_t> buffer(static_cast<size_t>(SECTORS_PER_READ) * RAW_SECTOR_SIZE);
            for (uint32_t sector = track.startLba; sector < track.endLba; sector += SECTORS_PER_READ)
            {
                const uint32_t count = std::min(SECTORS_PER_READ, track.endLba - sector);
                const size_t got = std::min(drive.ReadAudio(sector, count, buffer.data()), buffer.size());
                pcm.insert(pcm.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(got));
            }
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("CddaRipper::RipTrackToWav: track " + std::to_string(track.trackNumber) + ": " + e.what());
        }
        CddaDetail::WriteWav(outputPath, pcm);
    }

    CddaRipSummary CddaRipper::RipDisc(const std::filesystem::path& discRoot, const std::filesystem::path& musicDir,
                                       const std::function<void(const std::string&)>& log)
    {
        CddaRipSummary summary;
        auto name = [](int number) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "track%02d.wav", number);
            return std::string(buffer);
        };

#if defined(__APPLE__)
        // The tracks macOS has already mounted as AIFF files, which needs no
        // access to the raw device. On this disc track 1 is the data, so if
        // the files are numbered from 1 they count the audio tracks alone,
        // and track N is file N - 1.
        const auto aiffTracks = FindAudioCdTracks();
        if (!aiffTracks.empty())
        {
            const int shift = aiffTracks.begin()->first == 1 ? 1 : 0;
            for (const auto& [number, path] : aiffTracks)
            {
                const int trackNumber = number + shift;
                const std::vector<uint8_t> pcm = CddaDetail::AiffToPcm(ReadWholeFile(path));
                if (pcm.empty())
                {
                    log("  Failed to convert " + path.filename().string() + ": not CD audio");
                    ++summary.failed;
                    continue;
                }
                try
                {
                    log("  Converting " + path.filename().string() + " to " + name(trackNumber) + "...");
                    CddaDetail::WriteWav(musicDir / name(trackNumber), pcm);
                    ++summary.ripped;
                }
                catch (const std::exception& e)
                {
                    log("  Failed to write " + name(trackNumber) + ": " + e.what());
                    ++summary.failed;
                }
            }
            if (summary.ripped > 0) { return summary; }
            log("  The AIFF tracks couldn't be used; reading the disc directly instead.");
            summary = {};
        }
#endif

        const std::string device = DeviceForDisc(discRoot);
        if (device.empty())
        {
            summary.error = "couldn't tell which drive " + discRoot.string() + " is on";
            return summary;
        }

        std::vector<CddaTrack> tracks;
        try
        {
            tracks = ReadToc(device);
        }
        catch (const std::exception& e)
        {
            summary.error = e.what();
            return summary;
        }

        for (const auto& track : tracks)
        {
            if (!track.isAudio) { continue; }   // track 1 is the data track
            try
            {
                log("  Ripping " + name(track.trackNumber) + "...");
                RipTrackToWav(device, track, musicDir / name(track.trackNumber));
                ++summary.ripped;
            }
            catch (const std::exception& e)
            {
                log("  Failed to rip track " + std::to_string(track.trackNumber) + ": " + e.what());
                ++summary.failed;
            }
        }
        if (summary.ripped == 0 && summary.failed == 0) { summary.error = "the disc has no audio tracks"; }
        return summary;
    }
}
