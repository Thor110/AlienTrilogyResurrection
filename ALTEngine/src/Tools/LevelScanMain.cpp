// ALTLevelScan - looks through every level for faces whose texture is turned
// or mirrored against its neighbours (Formats/UvSeamScan.h).
//
//   ALTLevelScan <CD folder> [options]
//     --manifest <file>   the level list (default data/LevelManifest.json)
//     --patches <file>    Patches.json: faces its faceUvRotations already
//                         fix are judged as fixed (default data/Patches.json)
//     --level <digits>    only this level, e.g. 391
//     --out <file>        the report (default LevelUvScan.json)
//     --min-mismatch <n>  how many texels apart a face must put its shared
//                         vertices from its neighbours' (default 4)
//     --ratio <r>         and the suggestion at most this fraction of that
//                         (default 0.25)
//     --min-edges <n>     shared edges with picture neighbours a face needs
//                         (default 2)
//
// The CD folder is the one holding SECT11, SECT12 ... The report lists each
// level's suspects, strongest first, and ends with a faceUvRotations array
// in Patches.json's own form, so an entry that checks out in a viewer can be
// copied across as it is.

#include "Formats/BndTextureLoader.h"
#include "Formats/FaceUvPatches.h"
#include "Formats/LevelList.h"
#include "Formats/LevelLoader.h"
#include "Formats/UvSeamScan.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
namespace F = ALTEngine::Formats;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: ALTLevelScan <CD folder> [--manifest f] [--patches f] [--level digits] "
                             "[--out f] [--min-mismatch n] [--ratio r] [--min-edges n]\n");
        return 2;
    }
    fs::path cd = argv[1];
    fs::path manifest = "data/LevelManifest.json";
    fs::path patches = "data/Patches.json";
    fs::path outPath = "LevelUvScan.json";
    std::string only;
    F::UvSeamOptions base;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        const std::string flag = argv[i];
        const std::string value = argv[i + 1];
        if (flag == "--manifest") { manifest = value; }
        else if (flag == "--patches") { patches = value; }
        else if (flag == "--level") { only = value; }
        else if (flag == "--out") { outPath = value; }
        else if (flag == "--min-mismatch") { base.minMismatch = std::strtof(value.c_str(), nullptr); }
        else if (flag == "--min-edges") { base.minEdges = std::atoi(value.c_str()); }
        else if (flag == "--ratio") { base.maxRatio = std::strtof(value.c_str(), nullptr); }
        else { std::fprintf(stderr, "unknown option %s\n", flag.c_str()); return 2; }
    }

    const auto levels = F::LoadLevelList(manifest);
    if (levels.empty())
    {
        std::fprintf(stderr, "no levels in %s\n", manifest.string().c_str());
        return 1;
    }
    const auto existing = F::FaceUvPatchLoader::Load(patches);

    nlohmann::json report;
    report["_note"] = "ALTLevelScan: faces whose texture does not run on into their neighbours', "
                      "and the orientation that agrees best. Costs are how many texels apart, per shared edge, "
                      "a face and its neighbour put the edge's vertices on the texture page. Suggestions are heuristic - check each one in a viewer first. "
                      "`flip` then `steps` are applied to the face's unpatched mapping, as faceUvRotations are.";
    report["levels"] = nlohmann::json::array();
    nlohmann::json rotations = nlohmann::json::array();
    int scanned = 0, missing = 0, total = 0;

    for (const auto& entry : levels)
    {
        if (!only.empty() && entry.digits != only) { continue; }
        const fs::path mapPath = cd / entry.sectorFolder / entry.mapFile;
        const fs::path gfxPath = cd / entry.sectorFolder / (entry.digits + "GFX.B16");
        std::error_code ec;
        if (!fs::is_regular_file(mapPath, ec) || !fs::is_regular_file(gfxPath, ec))
        {
            ++missing;
            continue;
        }
        F::LevelGeometry level;
        F::BndTextureSet textures;
        try
        {
            level = F::LevelLoader::Load(mapPath);
            textures = F::BndTextureLoader::LoadLevelPages(gfxPath);
        }
        catch (const std::exception& e)
        {
            std::fprintf(stderr, "%s: %s\n", entry.mapFile.c_str(), e.what());
            ++missing;
            continue;
        }

        F::UvSeamOptions options = base;
        options.applied = F::FaceUvPatchLoader::ForLevelFile(existing, entry.mapFile);
        const auto suspects = F::ScanLevelUvSeams(level, textures, options);
        ++scanned;
        total += static_cast<int>(suspects.size());
        std::printf("%s  %-14s %5zu faces  %3zu suspects\n", entry.digits.c_str(), entry.mapFile.c_str(),
                    level.quads.size(), suspects.size());

        const std::string target = entry.sectorFolder + "/" + entry.mapFile;
        nlohmann::json lj;
        lj["code"] = entry.digits;
        lj["target"] = target;
        lj["suspects"] = nlohmann::json::array();
        for (const auto& s : suspects)
        {
            nlohmann::json sj;
            sj["face"] = s.face;
            sj["vertices"] = s.vertices;
            sj["texIndex"] = s.texIndex;
            sj["drawRoutine"] = s.drawRoutine;
            sj["edges"] = s.edges;
            sj["currentCost"] = std::round(s.currentCost * 10.0f) / 10.0f;
            sj["bestCost"] = std::round(s.bestCost * 10.0f) / 10.0f;
            sj["flip"] = s.flip;
            sj["steps"] = s.steps;
            if (s.alreadyPatched) { sj["alreadyPatched"] = true; }
            lj["suspects"].push_back(sj);

            nlohmann::json rj;
            rj["target"] = target;
            char note[128];
            std::snprintf(note, sizeof note, "ALTLevelScan: face %d, seam %.0f -> %.0f over %d edges%s",
                          s.face, s.currentCost, s.bestCost, s.edges,
                          s.alreadyPatched ? " (replaces its existing entry)" : "");
            rj["note"] = note;
            rj["vertices"] = s.vertices;
            if (s.steps != 0) { rj["steps"] = s.steps; }
            if (s.flip) { rj["flip"] = true; }
            rotations.push_back(rj);
        }
        report["levels"].push_back(lj);
    }
    report["faceUvRotations"] = rotations;

    std::ofstream out(outPath);
    if (!out)
    {
        std::fprintf(stderr, "could not write %s\n", outPath.string().c_str());
        return 1;
    }
    out << report.dump(2) << "\n";
    std::printf("%d levels scanned, %d missing, %d suspects -> %s\n", scanned, missing, total, outPath.string().c_str());
    return scanned > 0 ? 0 : 1;
}
