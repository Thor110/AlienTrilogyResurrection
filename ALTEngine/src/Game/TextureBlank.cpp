#include "TextureBlank.h"

#include "LevelCleanup.h"

#include <algorithm>
#include <array>

namespace ALTEngine::Game::TextureBlank
{
    namespace
    {
        struct Chunk { size_t data = 0, size = 0; };

        // FORM <size> PSXT, then TPnn (256 x 256 palette indices), CLnn (4-byte
        // header, 256 RGB555 colours) and BXnn (int16 count, then per texture
        // x, y, width - 1, height - 1 as bytes and a uint16) chunks.
        bool Chunks(const std::vector<uint8_t>& b, std::map<std::string, Chunk>& out, std::string& error)
        {
            if (b.size() < 12 || b[0] != 'F' || b[1] != 'O' || b[2] != 'R' || b[3] != 'M') { error = "not a FORM file"; return false; }
            for (size_t i = 12; i + 8 <= b.size();)
            {
                const std::string tag(reinterpret_cast<const char*>(&b[i]), 4);
                const size_t n = (size_t(b[i + 4]) << 24) | (size_t(b[i + 5]) << 16) | (size_t(b[i + 6]) << 8) | size_t(b[i + 7]);
                if (i + 8 + n > b.size()) { error = "chunk " + tag + " runs past the end"; return false; }
                out[tag] = { i + 8, n };
                i += 8 + n + (n & 1);
            }
            return true;
        }

        std::string Tag(const char* kind, int page)
        {
            std::string t = kind;
            t += char('0' + (page / 10) % 10);
            t += char('0' + page % 10);
            return t;
        }
    }

    bool Apply(std::vector<uint8_t>& bytes, const Plan& plan, Report& report, std::string& error)
    {
        report = {};
        std::map<std::string, Chunk> chunks;
        if (!Chunks(bytes, chunks, error)) { return false; }

        // Every texture rectangle, in the flat order texture indices count in.
        struct Rect { int page, x, y, w, h; };
        std::vector<Rect> rects;
        for (int page = 0; page < 100; ++page)
        {
            const auto it = chunks.find(Tag("BX", page));
            if (it == chunks.end()) { continue; }
            const Chunk& c = it->second;
            if (c.size < 2) { error = "a texture list too short"; return false; }
            const int count = int(int16_t(uint16_t(bytes[c.data] | (bytes[c.data + 1] << 8))));
            if (count < 0 || 2 + size_t(count) * 6 > c.size) { error = "a texture list past its chunk"; return false; }
            for (int r = 0; r < count; ++r)
            {
                const size_t o = c.data + 2 + size_t(r) * 6;
                rects.push_back({ page, bytes[o], bytes[o + 1], bytes[o + 2] + 1, bytes[o + 3] + 1 });
            }
        }
        std::vector<uint8_t> isUnused(rects.size(), 0);
        for (uint32_t t : plan.unused)
        {
            if (t >= rects.size()) { error = "the plan names texture " + std::to_string(t) + ", which the sheet does not have"; return false; }
            isUnused[t] = 1;
        }
        for (const auto& [page, index] : plan.background)
        {
            const auto tp = chunks.find(Tag("TP", page));
            if (tp == chunks.end() || tp->second.size < 65536) { error = "the plan names page " + std::to_string(page) + ", which the sheet does not have"; return false; }
            if (index < 0 || index > 255) { error = "a background that is no palette index"; return false; }
        }

        // Per page: 2 = a used texture covers the texel, 1 = only unused ones, 0 = none.
        std::vector<uint8_t> out = bytes;
        for (const auto& [page, index] : plan.background)
        {
            std::array<uint8_t, 65536> cover{};
            for (size_t t = 0; t < rects.size(); ++t)
            {
                const Rect& r = rects[t];
                if (r.page != page) { continue; }
                for (int y = r.y; y < r.y + r.h && y < 256; ++y)
                {
                    for (int x = r.x; x < r.x + r.w && x < 256; ++x)
                    {
                        uint8_t& v = cover[size_t(y * 256 + x)];
                        v = isUnused[t] ? std::max<uint8_t>(v, 1) : 2;
                    }
                }
            }
            const size_t pixels = chunks.at(Tag("TP", page)).data;
            for (size_t i = 0; i < 65536; ++i)
            {
                if (cover[i] == 2) { continue; }
                ++report.texelsFilled;
                if (out[pixels + i] != uint8_t(index)) { out[pixels + i] = uint8_t(index); ++report.texelsChanged; }
            }
            ++report.pages;
        }
        bytes = std::move(out);
        return true;
    }

    std::vector<std::filesystem::path> SheetFiles(const std::filesystem::path& cdDirectory)
    {
        std::vector<std::filesystem::path> sheets;
        for (const auto& level : LevelCleanup::LevelFiles(cdDirectory))
        {
            const std::string name = level.filename().string();   // L111LEV.MAP
            sheets.push_back(level.parent_path() / (name.substr(1, 3) + "GFX.B16"));
        }
        return sheets;
    }
}
