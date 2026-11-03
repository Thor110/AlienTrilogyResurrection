#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Game::LevelCleanup
{
    // LEVEL CLEANUP - the port's own, behind Options > Modern > Testing >
    // Level Cleanup (off by default). Two changes to a level file, made after
    // every fix in Patches.json is in (those are written against the files as
    // shipped), in this order:
    //
    //  1. TRIM. Faces that can never be drawn are taken out, with the vertices
    //     only they use. The original draws a level only through its cells'
    //     face runs (a cell's bytes 0-3 name the first face record of its run;
    //     the run ends at the face whose light byte, +0x13, has bit 0x80), and
    //     only the cells its view walk takes in (0x2745b,
    //     CreatureSight::DrawnCells). Every cell the player can walk to is
    //     found (from the level's start, and every path node on a multiplayer
    //     map, through anything that is not a wall to the player; doors all
    //     counted open and heights ignored, so this over-counts), the walk is
    //     run from each with its cones opened to a full circle (a superset of
    //     the original's at every facing), and a run no walk takes in is
    //     dropped. Its cell's run start becomes FF FF FF FF, as for a cell
    //     with no faces. Faces no cell lists are kept (see 2).
    //
    //  2. ADD FACES. Each level's first cell with faces has a run start of 0
    //     (the blank record) instead of 1, so the original never draws it. In
    //     35 of the 45 levels that cell's faces are already in the file as
    //     the first records, unlisted, their run ending in its 0x80 flag: the
    //     start is set to 1 and they are drawn. The other 10 lack them, and
    //     are left (they need the faces inserted).
    //
    // Everything that names a face or a vertex by number is renumbered: the
    // cells' run starts, the faces' vertex indices, the header's counts, and
    // the MAP0 and FORM sizes. Nothing else in a level names them.
    //
    // Running it again on a cleaned file changes nothing.
    struct Report
    {
        int facesRemoved = 0;
        int verticesRemoved = 0;
        bool firstCellRestored = false;
        int firstCellFaces = 0;
        size_t bytesBefore = 0;
        size_t bytesAfter = 0;
        bool Changed() const { return facesRemoved > 0 || verticesRemoved > 0 || firstCellRestored; }
    };

    // Cleans one level file's whole image in place. `multiplayer` adds every
    // path node as a place the player can be. False (and `error`) if the
    // file is not a level this understands; `bytes` is then untouched.
    bool Clean(std::vector<uint8_t>& bytes, bool multiplayer, Report& report, std::string& error);

    // The level files the cleanup applies to: every SECT*/L*LEV.MAP under
    // the game's CD folder, relative to it.
    std::vector<std::filesystem::path> LevelFiles(const std::filesystem::path& cdDirectory);
}
