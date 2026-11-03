#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Game::LevelCleanup
{
    // LEVEL CLEANUP - the port's own, behind Options > Modern > Testing >
    // Level Cleanup (off by default). It takes out of each level file the
    // faces no player can ever see, with the vertices only they use, and gives
    // the first cell its faces back.
    //
    // The work of finding them is done once, by the ALTLevelCleanupList tool
    // (src/Tools/LevelCleanupListMain.cpp, LevelCleanupPlan.h), against the
    // level files as the fixes in Patches.json leave them. Its result ships as
    // data/LevelCleanupList.json: for each level, the CRC32 of the file it was
    // worked out from and a Plan. At install the plan is only applied
    // (Apply), and only to a file with that CRC; any other is left alone.
    //
    // Applying a plan: the faces it names go; a run that loses its last faces
    // ends at its last kept one (the 0x80 bit, +0x13, moves there); a cell
    // whose run loses its first faces starts at its first kept one. The first
    // cell's run begins at record 0, which its start of 0 hides; when the cell
    // is given back, a copy of record 0 goes in at its head (record 0 stays,
    // as the start of the cells left empty). A cell
    // left with no faces starts at FF FF FF FF if the plan says the view walk
    // never takes it in, and at 0 otherwise - the blank record, which draws
    // nothing (as the first cell's 0 does): the walk spreads through a cell's
    // zones only while the cell has a start, so FF FF FF FF there would change
    // what else is drawn. Everything that names a face or a vertex by number
    // is renumbered: the cells' run starts, the faces' vertex indices, the
    // header's counts, and the MAP0 and FORM sizes. Nothing else in a level
    // names them.
    // What Apply makes of a plan, by version: raised whenever the same plan
    // comes to give a different file (2: record 0's copy heads the first
    // cell's run), so cleaned copies made by an older Apply are made again.
    inline constexpr int APPLY_VERSION = 2;

    struct Plan
    {
        std::vector<uint32_t> remove;   // face records to take out, as the file numbers them
        std::vector<uint32_t> clear;    // cells the walk never takes in: FF FF FF FF once empty
        int firstCell = -1;             // the cell whose run (records 1..) is given back, or -1

        // What the faces are, for the report only.
        int outOfView = 0;              // in cells the walk never takes in
        int facingAway = 0;             // fronts no place the player can be looks at
        int hidden = 0;                 // always behind other opaque faces
        int inLifts = 0;                // the sealed lifts behind solid doors (L321-L325)
        int byHand = 0;                 // named in data/LevelCleanupExtra.json
    };

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

    // Applies `plan` to one level file's whole image in place. With
    // `oldRecordOfNew`, receives for each record of the result the record of
    // the input it came from. False (and `error`) if the file is not a level
    // this understands or the plan names a record or cell it does not have;
    // `bytes` is then untouched.
    bool Apply(std::vector<uint8_t>& bytes, const Plan& plan, Report& report, std::string& error,
               std::vector<uint32_t>* oldRecordOfNew = nullptr);

    // Record and cell lists as the list file writes them: "1-5,9,12-40".
    std::string EncodeList(const std::vector<uint32_t>& values);
    bool DecodeList(const std::string& text, std::vector<uint32_t>& values);

    // The level files the cleanup applies to: every SECT*/L*LEV.MAP under
    // the game's CD folder, relative to it.
    std::vector<std::filesystem::path> LevelFiles(const std::filesystem::path& cdDirectory);
}
