// ALTTextureBlankList - works out data/TextureBlankList.json, the texture
// blanking's plan: for each level's sheet, which texture rectangles nothing in
// the level draws (Game/TextureUse.h), so they can be filled with the sheet's
// background colour.
//
//   ALTTextureBlankList <CD folder> [--out data/TextureBlankList.json]
//                       [--report ../Notes/texture-use.md]
//                       [--cleanup data/LevelCleanupList.json]
//
// Run it on the CD folder as the fixes (Patches.json, model removals) leave it,
// with the Level Cleanup off - the same files ALTLevelCleanupList reads - and
// after ALTLevelCleanupList: the faces the cleanup adds ("add") count as used
// too (the faces it removes still do, since the cleanup is optional).
#include "../Formats/BndTextureLoader.h"
#include "../Formats/ModelRemoval.h"
#include "../Formats/ModelLoader.h"
#include "../Game/LevelCleanup.h"
#include "../Game/TextureBlank.h"
#include "../Game/TextureUse.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <map>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace F = ALTEngine::Formats;
namespace G = ALTEngine::Game;

namespace
{
    // A sheet's chunks (FORM ... PSXT, then TPnn / CLnn / BXnn): where each's
    // data starts, by tag.
    std::map<std::string, std::pair<size_t, size_t>> Chunks(const std::vector<uint8_t>& b)
    {
        std::map<std::string, std::pair<size_t, size_t>> out;
        for (size_t i = 12; i + 8 <= b.size();)
        {
            const std::string tag(reinterpret_cast<const char*>(&b[i]), 4);
            const size_t n = (size_t(b[i + 4]) << 24) | (size_t(b[i + 5]) << 16) | (size_t(b[i + 6]) << 8) | size_t(b[i + 7]);
            out[tag] = { i + 8, n };
            i += 8 + n + (n & 1);
        }
        return out;
    }

    struct Rgb { int r, g, b; };
    Rgb Colour(const std::vector<uint8_t>& b, size_t clut, int index)   // RGB555 after CL's 4-byte header, as 0-248
    {
        const int v = b[clut + 4 + size_t(index) * 2] | (b[clut + 5 + size_t(index) * 2] << 8);
        return { (v & 31) * 8, ((v >> 5) & 31) * 8, ((v >> 10) & 31) * 8 };
    }
    // The placeholder magenta some sheets fill spare slots with: never a background.
    bool Magenta(const Rgb& c) { return c.r >= 96 && c.b >= 96 && c.g * 2 < c.r && c.g * 2 < c.b; }

    // A page's background: the most common colour of the space the blanking
    // fills - the texels of its unused textures that no used one covers, and
    // the texels no texture covers - leaving out placeholder magenta (the
    // colour some sheets park in spare slots, which bleeds purple at face
    // edges). A page with no such space, or only magenta: the page's most
    // common colour, magenta again left out.
    int Background(const std::vector<uint8_t>& gfx, size_t pixels, size_t clut, const std::array<uint8_t, 65536>& cover, std::string& rule)
    {
        std::array<size_t, 256> count{}, all{};
        size_t n = 0;
        for (size_t i = 0; i < 65536; ++i)
        {
            const int v = gfx[pixels + i];
            if (Magenta(Colour(gfx, clut, v))) { continue; }
            ++all[size_t(v)];
            if (cover[i] != 2) { ++count[size_t(v)]; ++n; }
        }
        const auto& from = n ? count : all;
        int best = 0;
        for (int v = 1; v < 256; ++v) { if (from[size_t(v)] > from[size_t(best)]) { best = v; } }
        rule = n ? "most common colour of the space blanked" : "page's most common colour (nothing to blank but placeholder magenta, or nothing)";
        return best;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::printf("usage: ALTTextureBlankList <CD folder> [--out file] [--report file]\n"); return 2; }
    const fs::path cd = argv[1];
    fs::path out = "data/TextureBlankList.json", reportPath = "../Notes/texture-use.md", cleanupPath = "data/LevelCleanupList.json";
    for (int a = 2; a + 1 < argc; a += 2)
    {
        const std::string opt = argv[a];
        if (opt == "--out") { out = argv[a + 1]; }
        else if (opt == "--report") { reportPath = argv[a + 1]; }
        else if (opt == "--cleanup") { cleanupPath = argv[a + 1]; }
    }

    // The Level Cleanup's list, for the faces it adds.
    nlohmann::json cleanup;
    {
        std::ifstream c(cleanupPath);
        if (c) { try { c >> cleanup; } catch (...) { cleanup = nlohmann::json(); } }
        if (!cleanup.is_object() || !cleanup.contains("levels")) { std::printf("note: %s not read - faces the cleanup adds are not counted\n", cleanupPath.generic_string().c_str()); }
    }
    nlohmann::ordered_json sheets = nlohmann::ordered_json::object();
    std::string report = "# Texture use per level\n\nWritten by ALTTextureBlankList (src/Tools/TextureBlankListMain.cpp) from the level files as the fixes "
                         "leave them. A texture counts as used if a face names it (an animated face: its animator's frames), an animator's program "
                         "shows it, or a door or lift model names it (Game/TextureUse.h). Area is in texels: an unused rectangle's texels that no used "
                         "rectangle covers too, and the texels no rectangle covers at all.\n\n"
                         "Background, per page: the palette index (and colour, 0-248) the blanked space is filled with - the most common colour of that "
                         "space (the unused textures' texels no used texture covers, and the texels no texture covers), placeholder magenta left out; "
                         "marked page, the page's most common colour, where there is nothing else to go by.\n\n"
                         "| Level | Sheet | Textures | Used (faces / frames / models) | Unused | Unused area | Uncovered area | Background per page |\n|---|---|---|---|---|---|---|---|\n";
    int failed = 0;
    size_t totalUnused = 0, totalUnusedArea = 0, totalUncovered = 0, totalTextures = 0, totalChanged = 0;
    for (const auto& relative : G::LevelCleanup::LevelFiles(cd))
    {
        const std::string name = relative.filename().string();      // L111LEV.MAP
        const std::string digits = name.substr(1, 3);
        const fs::path gfxRelative = relative.parent_path() / (digits + "GFX.B16");
        std::ifstream in(cd / relative, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::ifstream gin(cd / gfxRelative, std::ios::binary);
        const std::vector<uint8_t> gfx((std::istreambuf_iterator<char>(gin)), std::istreambuf_iterator<char>());
        if (bytes.empty() || gfx.empty()) { std::printf("%s: FAILED - level or sheet missing\n", relative.generic_string().c_str()); ++failed; continue; }

        F::BndTextureSet set;
        try { set = F::BndTextureLoader::LoadLevelPages(cd / gfxRelative); }
        catch (const std::exception& e) { std::printf("%s: FAILED - %s\n", gfxRelative.generic_string().c_str(), e.what()); ++failed; continue; }
        std::vector<F::ModelMesh> models;
        for (const char* prefix : { "D", "L0" })
        {
            try { for (auto& m : F::ModelLoader::Load(cd / relative, prefix)) { models.push_back(std::move(m)); } }
            catch (...) {}
        }
        G::TextureUse::Result use;
        std::string error;
        if (!G::TextureUse::Scan(bytes, models, set.uvRects.size(), use, error))
        {
            std::printf("%s: FAILED - %s\n", relative.generic_string().c_str(), error.c_str());
            ++failed;
            continue;
        }

        // The faces the Level Cleanup adds to this level.
        if (cleanup.is_object() && cleanup.contains("levels") && cleanup["levels"].contains(relative.generic_string())
            && cleanup["levels"][relative.generic_string()].contains("add"))
        {
            std::vector<G::LevelCleanup::Addition> added;
            std::string addError;
            if (G::LevelCleanup::ReadAdditions(cleanup["levels"][relative.generic_string()]["add"].dump(), added, addError))
            {
                for (const auto& a : added)
                {
                    if (a.texture >= 0 && size_t(a.texture) < use.use.size()) { use.use[size_t(a.texture)] |= G::TextureUse::FACE; }
                }
            }
        }

        // Texel coverage per page: used, unused-only, none.
        int pages = 0;
        for (const auto& r : set.uvRects) { pages = std::max(pages, r.page + 1); }
        std::vector<std::array<uint8_t, 65536>> cover(static_cast<size_t>(pages));
        for (auto& c : cover) { c.fill(0); }
        int byFace = 0, byFrame = 0, byModel = 0;
        std::vector<uint32_t> unused;
        for (size_t t = 0; t < set.uvRects.size(); ++t)
        {
            const auto& r = set.uvRects[t];
            const uint8_t u = use.use[t];
            byFace += (u & G::TextureUse::FACE) != 0; byFrame += (u & G::TextureUse::FRAME) != 0; byModel += (u & G::TextureUse::MODEL) != 0;
            if (!u) { unused.push_back(uint32_t(t)); }
            for (int y = r.y; y < r.y + r.height && y < 256; ++y)
            {
                for (int x = r.x; x < r.x + r.width && x < 256; ++x)
                {
                    auto& c = cover[size_t(r.page)][size_t(y * 256 + x)];
                    c = u ? 2 : std::max<uint8_t>(c, 1);
                }
            }
        }
        size_t unusedArea = 0, uncovered = 0;
        for (const auto& c : cover) { for (uint8_t v : c) { unusedArea += v == 1; uncovered += v == 0; } }

        // Each page's background colour.
        const auto chunks = Chunks(gfx);
        nlohmann::ordered_json backgrounds = nlohmann::ordered_json::array();
        std::string colours;
        for (int pg = 0; pg < pages; ++pg)
        {
            char tp[16], cl[16];
            std::snprintf(tp, sizeof(tp), "TP%02d", pg);
            std::snprintf(cl, sizeof(cl), "CL%02d", pg);
            if (!chunks.count(tp) || !chunks.count(cl) || chunks.at(tp).second < 65536 || chunks.at(cl).second < 516) { continue; }
            std::string rule;
            const int bg = Background(gfx, chunks.at(tp).first, chunks.at(cl).first, cover[size_t(pg)], rule);
            const Rgb c = Colour(gfx, chunks.at(cl).first, bg);
            nlohmann::ordered_json b;
            b["page"] = pg; b["background"] = bg; b["rgb"] = { c.r, c.g, c.b }; b["rule"] = rule;
            backgrounds.push_back(b);
            char cell[96];
            std::snprintf(cell, sizeof(cell), "%s%d: %d (%d, %d, %d)%s", pg ? "<br>" : "", pg, bg, c.r, c.g, c.b,
                          rule.rfind("page's", 0) == 0 ? " page" : "");
            colours += cell;
        }
        totalUnused += unused.size(); totalUnusedArea += unusedArea; totalUncovered += uncovered; totalTextures += set.uvRects.size();

        // Applied to a copy, as the install will: no texel any used texture
        // covers may change (checked against the loader's own rectangles).
        G::TextureBlank::Plan plan;
        plan.unused = unused;
        for (const auto& b : backgrounds) { plan.background[b["page"].get<int>()] = b["background"].get<int>(); }
        std::vector<uint8_t> blanked = gfx;
        G::TextureBlank::Report blankReport;
        if (!G::TextureBlank::Apply(blanked, plan, blankReport, error))
        {
            std::printf("%s: FAILED - %s\n", gfxRelative.generic_string().c_str(), error.c_str());
            ++failed;
            continue;
        }
        size_t touched = 0;
        for (size_t t = 0; t < set.uvRects.size(); ++t)
        {
            if (!use.use[t]) { continue; }
            const auto& r = set.uvRects[t];
            char tp[16];
            std::snprintf(tp, sizeof(tp), "TP%02d", r.page);
            if (!chunks.count(tp)) { continue; }
            const size_t base = chunks.at(tp).first;
            for (int y = r.y; y < r.y + r.height && y < 256; ++y)
            {
                for (int x = r.x; x < r.x + r.width && x < 256; ++x)
                {
                    const size_t i = base + size_t(y * 256 + x);
                    touched += blanked[i] != gfx[i];
                }
            }
        }
        if (touched)
        {
            std::printf("%s: FAILED - blanking would change %zu texels of used textures\n", gfxRelative.generic_string().c_str(), touched);
            ++failed;
            continue;
        }
        totalChanged += blankReport.texelsChanged;

        nlohmann::ordered_json e;
        e["level"] = relative.generic_string();
        e["crc"] = F::Crc32(gfx.data(), gfx.size());
        e["textures"] = set.uvRects.size();
        e["unused"] = G::LevelCleanup::EncodeList(unused);
        e["backgrounds"] = backgrounds;
        e["texelsChanged"] = blankReport.texelsChanged;
        sheets[gfxRelative.generic_string()] = e;
        std::printf("%s (%s): %zu textures, %zu unused, %zu unused texels, %zu uncovered, %zu bytes changed%s\n", gfxRelative.generic_string().c_str(),
                    name.c_str(), set.uvRects.size(), unused.size(), unusedArea, uncovered, blankReport.texelsChanged,
                    use.outOfRange ? (" (" + std::to_string(use.outOfRange) + " references past the sheet)").c_str() : "");
        report += "| " + name.substr(0, 4) + " | " + gfxRelative.generic_string() + " | " + std::to_string(set.uvRects.size()) + " | "
                + std::to_string(byFace) + " / " + std::to_string(byFrame) + " / " + std::to_string(byModel) + " | "
                + std::to_string(unused.size()) + " | " + std::to_string(unusedArea) + " | " + std::to_string(uncovered) + " | " + colours + " |\n";
    }
    report += "\nIn all: " + std::to_string(totalTextures) + " textures, " + std::to_string(totalUnused) + " unused, "
            + std::to_string(totalUnusedArea) + " unused texels and " + std::to_string(totalUncovered) + " uncovered; the blanking changes "
            + std::to_string(totalChanged) + " bytes of the sheets (texels already the background stay as they are).\n";

    nlohmann::ordered_json root;
    root["about"] = "The texture blanking's plan, worked out by ALTTextureBlankList from the level files as the fixes leave them: per level "
                    "sheet, the CRC32 of the sheet it was worked out from and the texture rectangles nothing in the level draws.";
    root["sheets"] = sheets;
    std::ofstream o(out);
    o << root.dump(1) << "\n";
    std::ofstream r(reportPath);
    r << report;
    std::printf("%zu textures, %zu unused, %zu unused texels, %zu uncovered, %zu bytes changed, %d failed -> %s\n", totalTextures, totalUnused,
                totalUnusedArea, totalUncovered, totalChanged, failed, out.generic_string().c_str());
    return failed ? 1 : 0;
}
