#pragma once

#include "../Formats/LevelLoader.h"

#include <cstdint>
#include <vector>

namespace ALTEngine::Game::LevelOcclusion
{
    // HIDDEN FACES - part of the Level Cleanup (LevelCleanup.h), the port's
    // own. Finds the opaque faces of a level that a player can never see
    // because other opaque faces always stand in front of them: the backs of
    // the sealed doors on L321-L324, say, whose cells the view walk takes in
    // although nothing past the door can be seen.
    //
    // It is a test by sight lines, so it errs towards keeping a face:
    //
    //   - Only faces in cells the player cannot walk into are judged (the
    //     walls, door recesses and the like round the playable area), only
    //     opaque ones (routines 0, 1, 2 and their animated 8 and 10, and
    //     cut-outs with no clear texel - see `solid`), and only faces one
    //     cell's run lists.
    //   - The eye can be anywhere over a cell the player reaches: over the
    //     whole square (the walkable half of a diagonal cell), at any height
    //     from a dying player's lowest (0x180 over the floor, less the bob's
    //     64) to a standing one's 0x300 over the highest floor round it, with
    //     slopes, a lift's whole travel and a Modern jump added.
    //   - From each such cell whose view walk takes in the face's cell, sight
    //     lines are cast from points over the eye's region to points over the
    //     face; one that no opaque face blocks keeps the face. Only faces that
    //     face the eye block it. A first, coarse pass finds the candidates; a
    //     second, denser one rechecks them strictly:
    //       - a blocker counts only if its cell is drawn at every facing and
    //         screen width (a 4:3 window up to all round, Free Look's steep
    //         pitches) at which the walk draws the face's cell with that sight
    //         line on screen - the walk is by cell and zone, so a far cell can
    //         be drawn at a screen's edge while a nearer one in front is not;
    //       - a sight line must pass 8 units inside a blocker's open edges (a
    //         wall's top, a door's side; edges another blocker runs along,
    //         within 3 units, are closed) to count as blocked, for the lines
    //         between the points tried.
    //   - A face with any see-through face (cut-out, translucent or flat)
    //     between it and the eye's cell is kept: sight lines could slip
    //     through a grille's gaps between the points tried.
    //   - Nothing a face could be seen through ever moves: doors, lifts,
    //     windows and crates are objects, not level faces, and are never
    //     counted as blocking.
    //
    // The faces it returns can be taken out without changing what is drawn,
    // as long as every cell keeps a run start (see LevelCleanup.cpp): the
    // walk spreads through a cell's zones only when the cell has faces.
    //
    // `records` is the level file's face records (record 0 the blank), as
    // `level` (the same file parsed) and `cellStarts` (each cell's run start)
    // name them. `solid` (one flag a record, or empty) marks the cut-out faces
    // whose texture has no clear texel: they block as opaque faces do, from
    // either side, and are judged as such. `only` (one flag a record), when
    // given, judges just those faces, whatever their routine. With
    // `strictWalk` off, a blocker counts wherever the all-round walk draws its
    // cell: the sight lines alone decide, not which cells the walk happens to
    // take in at a screen's edge (used for the sealed lifts, see
    // LevelCleanupPlan.h). Returns one flag a record: 1 = hidden.
    std::vector<uint8_t> HiddenRecords(const Formats::LevelGeometry& level, const std::vector<uint8_t>& records,
                                       size_t recordCount, const std::vector<int16_t>& vertexXyz,
                                       const std::vector<uint32_t>& cellStarts, const std::vector<uint8_t>& reachable,
                                       const std::vector<uint8_t>& solid, const std::vector<uint8_t>* only = nullptr,
                                       bool strictWalk = true);
}
