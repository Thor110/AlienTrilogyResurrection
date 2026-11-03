// ALTLevelCleanupList - works out the Level Cleanup's plan for every level
// (Game/LevelCleanupPlan.h) and writes data/LevelCleanupList.json, which the
// install then only applies (Game/LevelCleanup.h). Run it again whenever a
// fix in Patches.json changes a level file, or the planning changes.
//
//   ALTLevelCleanupList <CD folder> [options]
//     --out <file>        the list (default data/LevelCleanupList.json)
//     --extra <file>      faces picked by hand to go as well (default
//                         data/LevelCleanupExtra.json, if there is one):
//                           { "levels": { "SECT31/L321LEV.MAP": {
//                               "faces": [ 1203, 1204 ],
//                               "points": [ [ -1280, 1024, -8960 ], ... ] } } }
//                         "faces" are the numbers the Face Index Display shows
//                         with Level Cleanup off; "points" are corners, and a
//                         face all of whose corners are listed (within 3
//                         units) goes - corners stay put, cleaned or not.
//     --remainder <file>  where to list the lift faces kept because a sight
//                         line reaches them, for checking by hand (default
//                         ../Notes/level-cleanup-lift-remainder.md)
//     --level <digits>    only this level, e.g. 321 (the others are dropped)
//
// The CD folder must hold the level files as the fixes leave them (an
// install with Level Cleanup off): each plan is kept with the CRC32 of the
// file it was worked out from, and is applied only to a file with that CRC.

#include "Formats/BndTextureLoader.h"
#include "Formats/ModelRemoval.h"
#include "Formats/RenderMesh.h"
#include "Game/LevelCleanup.h"
#include "Game/LevelCleanupPlan.h"
#include "Game/LevelFileLayout.h"

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
namespace F = ALTEngine::Formats;
namespace G = ALTEngine::Game;

namespace
{
    // The cut-out faces whose texture has no clear texel (the palette words
    // the renderer keys out, BndTextureLoader::LoadLevelPages): they hide
    // what is behind them as an opaque face does. Animated faces name an
    // animator, not a texture, and are never counted.
    std::vector<uint8_t> SolidCutouts(const std::vector<uint8_t>& bytes, const fs::path& gfx)
    {
        G::LevelFileLayout::Layout lay;
        std::string error;
        if (!G::LevelFileLayout::Read(bytes, lay, error)) { return {}; }
        std::vector<uint8_t> solid(lay.recordCount, 0);
        std::error_code ec;
        if (!fs::exists(gfx, ec)) { return solid; }
        F::BndTextureSet set;
        try { set = F::BndTextureLoader::LoadLevelPages(gfx); }
        catch (const std::exception&) { return solid; }
        for (size_t r = 1; r < lay.recordCount; ++r)
        {
            if (lay.RecordA(bytes, r) == -1) { continue; }
            const auto dr = F::ClassifyDrawRoutine(lay.Routine(bytes, r));
            if (dr.fill != F::FaceFill::Cutout || dr.animated) { continue; }
            const uint16_t t = lay.Texture(bytes, r);
            if (t >= set.uvRects.size()) { continue; }
            const auto& rect = set.uvRects[t];
            const F::BndTexture* page = nullptr;
            for (const auto& tex : set.textures) { if (std::atoi(tex.index.c_str()) == rect.page) { page = &tex; } }
            if (!page || rect.width <= 0 || rect.height <= 0) { continue; }
            bool clear = false;
            for (int y = rect.y; y < rect.y + rect.height && !clear; ++y)
            {
                for (int x = rect.x; x < rect.x + rect.width && !clear; ++x)
                {
                    if (x < 0 || y < 0 || x >= page->width || y >= page->height) { clear = true; break; }
                    clear = page->rgba[(size_t(y) * size_t(page->width) + size_t(x)) * 4 + 3] == 0;
                }
            }
            solid[r] = clear ? 0 : 1;
        }
        return solid;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: ALTLevelCleanupList <CD folder> [--out data/LevelCleanupList.json] [--level 321]\n");
        return 2;
    }
    const fs::path cd = argv[1];
    fs::path outPath = "data/LevelCleanupList.json";
    fs::path extraPath = "data/LevelCleanupExtra.json";
    fs::path remainderPath = "../Notes/level-cleanup-lift-remainder.md";
    std::string only;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        const std::string a = argv[i];
        if (a == "--out") { outPath = argv[i + 1]; }
        else if (a == "--level") { only = argv[i + 1]; }
        else if (a == "--extra") { extraPath = argv[i + 1]; }
        else if (a == "--remainder") { remainderPath = argv[i + 1]; }
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }

    // The lifts of the chapter-3 hub (L321-L325): their doors' six tiles.
    const std::vector<uint16_t> LIFT_DOOR = { 254, 255, 262, 263, 270, 271 };

    nlohmann::json extra = nlohmann::json::object();
    {
        std::ifstream in(extraPath);
        if (in)
        {
            try { in >> extra; }
            catch (const std::exception& e) { std::fprintf(stderr, "%s: %s\n", extraPath.string().c_str(), e.what()); return 2; }
            std::printf("faces by hand from %s\n", extraPath.string().c_str());
        }
    }
    std::string remainder = "# Level Cleanup: lift faces kept\n\n"
        "Written by ALTLevelCleanupList. The faces the lift crawl found behind a sealed lift's door that a sight line\n"
        "still reaches, so the cleanup keeps them. Check them in play; any that should go can be named in\n"
        "data/LevelCleanupExtra.json (by face number as the Face Index Display shows it with Level Cleanup off, or by\n"
        "corners, which do not move) and the list made again.\n";
    nlohmann::ordered_json levels = nlohmann::ordered_json::object();
    int failed = 0;
    size_t removed = 0;
    for (const auto& relative : G::LevelCleanup::LevelFiles(cd))
    {
        const std::string name = relative.filename().string();     // L321LEV.MAP
        const std::string digits = name.substr(1, 3);
        if (!only.empty() && digits != only) { continue; }
        std::ifstream in(cd / relative, std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const auto t0 = std::chrono::steady_clock::now();

        G::LevelCleanupPlan::Options options;
        options.multiplayer = digits[0] == '9';
        options.solid = SolidCutouts(bytes, cd / relative.parent_path() / (digits + "GFX.B16"));
        if (digits >= "321" && digits <= "325") { options.liftDoorTextures = LIFT_DOOR; }

        // Faces picked by hand.
        G::LevelFileLayout::Layout lay;
        std::string error;
        if (!G::LevelFileLayout::Read(bytes, lay, error)) { std::printf("%s: FAILED - %s\n", relative.generic_string().c_str(), error.c_str()); ++failed; continue; }
        auto corner = [&](size_t r, int k, int axis) { return double(lay.Coordinate(bytes, size_t(lay.Corner(bytes, r, k)), axis)); };
        auto cornerCount = [&](size_t r) {
            int n = 0;
            for (int k = 0; k < 4; ++k) { const int32_t v = lay.Corner(bytes, r, k); if (v >= 0 && v < lay.vertexCount) { ++n; } else { break; } }
            return n;
        };
        const std::string key = relative.generic_string();
        if (extra.contains("levels") && extra["levels"].contains(key))
        {
            const auto& e = extra["levels"][key];
            if (e.contains("faces")) { for (const auto& f : e["faces"]) { options.byHand.push_back(f.get<uint32_t>() + 1); } }   // face n is record n + 1
            if (e.contains("points"))
            {
                std::vector<std::array<double, 3>> points;
                for (const auto& p : e["points"]) { points.push_back({ p[0].get<double>(), p[1].get<double>(), p[2].get<double>() }); }
                for (size_t r = 1; r < lay.recordCount; ++r)
                {
                    const int n = cornerCount(r);
                    if (lay.RecordA(bytes, r) == -1 || n < 3) { continue; }
                    bool all = true;
                    for (int k = 0; k < n && all; ++k)
                    {
                        bool listed = false;
                        for (const auto& p : points)
                        {
                            const double dx = corner(r, k, 0) - p[0], dy = corner(r, k, 1) - p[1], dz = corner(r, k, 2) - p[2];
                            if (dx * dx + dy * dy + dz * dz <= 9.0) { listed = true; break; }
                        }
                        all = listed;
                    }
                    if (all) { options.byHand.push_back(uint32_t(r)); }
                }
            }
        }

        G::LevelCleanup::Plan plan;
        G::LevelCleanupPlan::Details details;
        G::LevelCleanup::Report report;
        std::vector<uint8_t> check = bytes;
        if (!G::LevelCleanupPlan::Make(bytes, options, plan, error, &details) || !G::LevelCleanup::Apply(check, plan, report, error))
        {
            std::printf("%s: FAILED - %s\n", relative.generic_string().c_str(), error.c_str());
            ++failed;
            continue;
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (!details.liftKept.empty())
        {
            const int L = lay.mapLength;
            std::vector<int> owner(lay.recordCount, -1);
            for (size_t c = 0; c < lay.cells; ++c)
            {
                const uint32_t start = lay.CellStart(bytes, c);
                if (start == G::LevelFileLayout::NO_RUN || start == 0 || start >= lay.recordCount) { continue; }
                for (size_t r = start; r < lay.recordCount; ++r)
                {
                    if (lay.RecordA(bytes, r) == -1) { break; }
                    owner[r] = int(c);
                    if (lay.Light(bytes, r) & G::LevelFileLayout::RUN_END) { break; }
                }
            }
            remainder += "\n## " + key + " (" + std::to_string(details.liftKept.size()) + " faces)\n\n| Face | Cell | Texture | Routine | Corners |\n|---|---|---|---|---|\n";
            for (uint32_t r : details.liftKept)
            {
                std::string corners;
                for (int k = 0; k < cornerCount(r); ++k)
                {
                    char b[64];
                    std::snprintf(b, sizeof(b), "%s(%d, %d, %d)", k ? " " : "", int(corner(r, k, 0)), int(corner(r, k, 1)), int(corner(r, k, 2)));
                    corners += b;
                }
                const std::string cell = owner[r] >= 0 ? std::to_string(owner[r] % L) + "," + std::to_string(owner[r] / L) : "-";
                remainder += "| " + std::to_string(r - 1) + " | " + cell + " | " + std::to_string(lay.Texture(bytes, r)) + " | "
                           + std::to_string(lay.Routine(bytes, r)) + " | " + corners + " |\n";
            }
        }
        std::printf("%s: %d faces (%d out of view, %d facing away, %d in lifts, %d hidden, %d by hand), %d vertices%s - %.1fs\n",
                    relative.generic_string().c_str(), report.facesRemoved, plan.outOfView, plan.facingAway, plan.inLifts, plan.hidden, plan.byHand,
                    report.verticesRemoved, report.firstCellRestored ? (", first cell's " + std::to_string(report.firstCellFaces) + " given back").c_str() : "",
                    seconds);
        removed += size_t(report.facesRemoved);

        nlohmann::ordered_json entry;
        entry["crc"] = F::Crc32(bytes.data(), bytes.size());
        entry["remove"] = G::LevelCleanup::EncodeList(plan.remove);
        entry["clear"] = G::LevelCleanup::EncodeList(plan.clear);
        if (plan.firstCell >= 0) { entry["firstCell"] = plan.firstCell; }
        entry["outOfView"] = plan.outOfView;
        entry["facingAway"] = plan.facingAway;
        entry["inLifts"] = plan.inLifts;
        entry["hidden"] = plan.hidden;
        if (plan.byHand) { entry["byHand"] = plan.byHand; }
        levels[relative.generic_string()] = entry;
    }

    nlohmann::ordered_json root;
    root["about"] = "Level Cleanup plans, made by ALTLevelCleanupList from the level files as Patches.json leaves them. "
                    "Each applies only to a file with its crc. Remake this file whenever a fix changes a level.";
    root["levels"] = levels;
    if (only.empty())
    {
        std::ofstream notes(remainderPath, std::ios::trunc);
        notes << remainder;
    }
    std::ofstream out(outPath, std::ios::trunc);
    out << root.dump(1) << "\n";
    std::printf("%zu levels, %zu faces, %d failed -> %s\n", levels.size(), removed, failed, outPath.string().c_str());
    return failed ? 1 : 0;
}
