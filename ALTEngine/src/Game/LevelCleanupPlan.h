#pragma once

#include "LevelCleanup.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Game::LevelCleanupPlan
{
    // Works out a level's cleanup plan (LevelCleanup::Plan) - the slow half,
    // run once by the ALTLevelCleanupList tool, never at install. Against the
    // level file as the fixes leave it:
    //
    //  1. OUT OF VIEW. Every cell the player can walk to is found (from the
    //     level's start, or on a multiplayer map from the path nodes a player
    //     is started at instead, through
    //     anything that is not a wall to the player; doors all counted open and
    //     heights ignored, so this over-counts), the view walk (0x2745b,
    //     CreatureSight::DrawnCells) is run from each with its cones opened to
    //     a full circle (a superset of the original's and the port's at every
    //     facing), and a run no walk takes in goes.
    //  2. FACING AWAY. In a run that is taken in, a one-sided face whose front
    //     no place the eye can be in those cells looks at goes - the backs of
    //     walls round the playable area. A quad is judged by all four of its
    //     triangles.
    //  3. LIFTS. The lifts of L321-L325 that no level uses (the rooms that
    //     look like a lift and act as a warp in the others) sit behind a
    //     solid door made of six tiles (`liftDoorTextures`). From each such
    //     door the faces behind it are crawled, corner to shared corner (within
    //     3 units: the levels' corners are often a unit or two apart), staying
    //     behind the door's plane. A crawl that stays small (a lift's size),
    //     touches no cell the player can reach and ends behind a door with no
    //     clear texel is the lift. Its faces go where no sight line reaches
    //     them (LevelOcclusion, judging by the sight lines alone): a low eye
    //     sees in under some doors' bottom edges.
    //  4. HIDDEN. Of what is left, the opaque faces other opaque faces always
    //     stand in front of (LevelOcclusion.h).
    //  5. FIRST CELL. Each level's first cell with faces has a run start of 0
    //     (the blank record) instead of 1, so the original never draws it. In
    //     35 of the 45 levels that cell's faces are already in the file as the
    //     first records, unlisted, their run ending in its 0x80 flag: they are
    //     given back (as judged by 1 and 2; L321-L324 and L902's first cells
    //     are out of every view, so theirs go instead). The other 10 lack them.
    //  6. BY HAND. Whatever Options::byHand names goes too.
    struct Options
    {
        bool multiplayer = false;      // started at its path nodes, not the header's start
        bool skipFirstNode = false;    // L909: the spawn never uses node 0
        // One flag per face record of the file: a cut-out face (routines 4, 5,
        // 6, 12, 13, 34 drawn as a cut-out) whose texture has no clear texel.
        // It blocks as an opaque face does. Empty: none.
        std::vector<uint8_t> solid;
        // The texture indices of a lift door's tiles, or empty for a level
        // with no such lifts.
        std::vector<uint16_t> liftDoorTextures;
        // Face records to take out as well, picked by hand (data/
        // LevelCleanupExtra.json): faces the judging keeps, checked in play.
        std::vector<uint32_t> byHand;
        // Door tiles (records) of sealed rooms named by hand: everything behind
        // the door is crawled as a lift's is, with no size limit, and goes
        // whole - unless the crawl reaches a cell the player can walk to.
        std::vector<uint32_t> roomDoors;
    };

    // What else Make found, for the people checking its work.
    struct Details
    {
        std::vector<uint32_t> liftKept;   // lift faces the crawl found that a sight line reaches, so stay
    };

    bool Make(const std::vector<uint8_t>& bytes, const Options& options, LevelCleanup::Plan& plan, std::string& error,
              Details* details = nullptr);
}
