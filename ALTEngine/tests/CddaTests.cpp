#include "TestFramework.h"

#include "Formats/CddaRipper.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace CD = ALTEngine::Formats::CddaDetail;
using ALTEngine::Formats::CddaTrack;

namespace
{
    void Be32(std::vector<uint8_t>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) { v.push_back(static_cast<uint8_t>(x >> s)); } }
    void Be16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(static_cast<uint8_t>(x >> 8)); v.push_back(static_cast<uint8_t>(x)); }
    void Tag(std::vector<uint8_t>& v, const char* t) { v.insert(v.end(), t, t + 4); }

    // An AIFF in the CD's format holding `samples` (big-endian 16-bit,
    // left and right interleaved), as macOS's cddafs serves one.
    std::vector<uint8_t> MakeAiff(const std::vector<int16_t>& samples, uint16_t channels = 2)
    {
        std::vector<uint8_t> comm, ssnd, out;
        Be16(comm, channels);
        Be32(comm, static_cast<uint32_t>(samples.size() / channels));
        Be16(comm, 16);
        // 44100 as an 80-bit extended float.
        const uint8_t rate[10] = { 0x40, 0x0e, 0xac, 0x44, 0, 0, 0, 0, 0, 0 };
        comm.insert(comm.end(), rate, rate + 10);
        Be32(ssnd, 0);   // offset
        Be32(ssnd, 0);   // block size
        for (int16_t s : samples) { Be16(ssnd, static_cast<uint16_t>(s)); }

        Tag(out, "FORM");
        Be32(out, static_cast<uint32_t>(4 + 8 + comm.size() + 8 + ssnd.size()));
        Tag(out, "AIFF");
        Tag(out, "COMM"); Be32(out, static_cast<uint32_t>(comm.size())); out.insert(out.end(), comm.begin(), comm.end());
        Tag(out, "SSND"); Be32(out, static_cast<uint32_t>(ssnd.size())); out.insert(out.end(), ssnd.begin(), ssnd.end());
        return out;
    }

    // One 11-byte raw TOC descriptor: session, ADR/control, TNO, point,
    // MSF, zero, PMSF.
    void Descriptor(std::vector<uint8_t>& v, uint8_t control, uint8_t point, uint8_t m, uint8_t s, uint8_t f)
    {
        const uint8_t d[11] = { 1, static_cast<uint8_t>(0x10 | control), 0, point, 0, 0, 0, 0, m, s, f };
        v.insert(v.end(), d, d + 11);
    }
}

TEST_CASE("CD audio: an AIFF track becomes little-endian PCM")
{
    const std::vector<uint8_t> pcm = CD::AiffToPcm(MakeAiff({ 0x0102, -2, 0x7fff, -32768 }));
    REQUIRE(pcm.size() == 8u);
    CHECK_EQ(pcm[0], 0x02); CHECK_EQ(pcm[1], 0x01);   // 0x0102
    CHECK_EQ(pcm[2], 0xfe); CHECK_EQ(pcm[3], 0xff);   // -2
    CHECK_EQ(pcm[6], 0x00); CHECK_EQ(pcm[7], 0x80);   // -32768
    // Not the CD's format: mono is refused rather than mangled.
    CHECK(CD::AiffToPcm(MakeAiff({ 1, 2 }, 1)).empty());
    CHECK(CD::AiffToPcm({ 'R', 'I', 'F', 'F' }).empty());
}

TEST_CASE("CD audio: the WAV header is the CD's format")
{
    const auto path = std::filesystem::temp_directory_path() / "alt_cdda_test.wav";
    CD::WriteWav(path, { 1, 2, 3, 4 });
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> wav((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::filesystem::remove(path);
    REQUIRE(wav.size() == 48u);
    CHECK(std::memcmp(wav.data(), "RIFF", 4) == 0);
    CHECK(std::memcmp(wav.data() + 8, "WAVE", 4) == 0);
    CHECK_EQ(wav[22], 2);                               // channels
    CHECK_EQ(wav[24] | (wav[25] << 8), 44100);          // sample rate
    CHECK_EQ(wav[34], 16);                              // bits
    CHECK_EQ(wav[40], 4);                               // data size
    CHECK_EQ(wav[47], 4);
}

TEST_CASE("CD audio: Linux finds the disc's drive in the mount table")
{
    const std::string table =
        "/dev/nvme0n1p2 / ext4 rw 0 0\n"
        "/dev/sr0 /media/edward/ALIEN\\040TRILOGY iso9660 ro 0 0\n"
        "tmpfs /run tmpfs rw 0 0\n";
    CHECK(CD::DeviceFromMountTable(table, "/media/edward/ALIEN TRILOGY") == std::string("/dev/sr0"));
    CHECK(CD::DeviceFromMountTable(table, "/media/edward/ALIEN TRILOGY/GFX") == std::string("/dev/sr0"));
    // A folder that only starts the same isn't on it.
    CHECK(CD::DeviceFromMountTable(table, "/media/edward/ALIEN TRILOGY2") == std::string("/dev/nvme0n1p2"));
    CHECK(CD::DeviceFromMountTable(table, "/home/edward/alien") == std::string("/dev/nvme0n1p2"));
}

TEST_CASE("CD audio: macOS reads the whole disc's raw device")
{
    CHECK(CD::WholeRawDisk("/dev/disk2s1") == std::string("/dev/rdisk2"));
    CHECK(CD::WholeRawDisk("/dev/disk12") == std::string("/dev/rdisk12"));
    CHECK(CD::WholeRawDisk("/dev/rdisk3s0") == std::string("/dev/rdisk3"));
    CHECK(CD::WholeRawDisk("map auto_home").empty());
    CHECK(CD::WholeRawDisk("/dev/sr0").empty());
}

TEST_CASE("CD audio: a macOS raw TOC gives tracks, ends and the data track")
{
    // Track 1 data at 00:02:00 (LBA 0), track 2 audio at 10:00:00, track 3
    // audio at 12:30:00, lead-out at 15:00:00.
    std::vector<uint8_t> toc = { 0, 0, 1, 1 };
    Descriptor(toc, 0x0, 0xa0, 0, 0, 0);   // first track number, ignored
    Descriptor(toc, 0x4, 1, 0, 2, 0);
    Descriptor(toc, 0x0, 2, 10, 0, 0);
    Descriptor(toc, 0x0, 3, 12, 30, 0);
    Descriptor(toc, 0x0, 0xa2, 15, 0, 0);
    const uint16_t length = static_cast<uint16_t>(toc.size() - 2);
    toc[0] = static_cast<uint8_t>(length >> 8);
    toc[1] = static_cast<uint8_t>(length);

    const std::vector<CddaTrack> tracks = CD::ParseRawToc(toc);
    REQUIRE(tracks.size() == 3u);
    CHECK(!tracks[0].isAudio);
    CHECK_EQ(tracks[0].startLba, 0u);
    CHECK(tracks[1].isAudio);
    CHECK_EQ(tracks[1].startLba, 10u * 60 * 75 - 150);
    CHECK_EQ(tracks[1].endLba, (12u * 60 + 30) * 75 - 150);
    CHECK_EQ(tracks[2].endLba, 15u * 60 * 75 - 150);
}

TEST_CASE("CD audio: a track file's number comes from its name")
{
    CHECK_EQ(CD::LeadingTrackNumber("3 Audio Track.aiff").value_or(-1), 3);
    CHECK_EQ(CD::LeadingTrackNumber("17 Audio Track.aiff").value_or(-1), 17);
    CHECK(!CD::LeadingTrackNumber("Audio Track.aiff").has_value());
    CHECK(!CD::LeadingTrackNumber("0 Audio Track.aiff").has_value());
}
