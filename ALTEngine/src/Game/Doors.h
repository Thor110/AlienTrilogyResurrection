#pragma once

#include "../Formats/LevelLoader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Game
{
    // Live door state, after the original's door process (TRILOGY.EXE states
    // 0x29218 closed, 0x29438 opening, 0x29078 hold, 0x29114 open, 0x292ac
    // closing). Doors drive the collision grid directly: the ceiling byte of
    // the four cells they cover rises and falls with the mesh, one height unit
    // (32 world units) per tick, so movement clearance and the visual stay in
    // lockstep.
    //
    // What the original does once a door is open:
    //   - open time 0: the unlock count is cleared and the door stays open. It
    //     closes only if something unlocks it again (the use key on it, or a
    //     trigger) and the player is then more than 1024 units away.
    //   - open time T: it holds open for T * 4 ticks with the unlock count kept,
    //     then closes as soon as the player is more than 1024 units away across
    //     the doorway and nothing stands in its cells.
    //   - anything in its cells while it closes sends it back up.
    struct DoorState
    {
        enum class Phase { Idle, Opening, Open, Closing, Hold };

        bool alongZ = false;
        int floorUnits = 0;
        int travel = 30;
        int counter = 0;             // the process's step count (+0x10), 0 .. travel
        int progress = 0;            // height above the floor in units; one short of travel when opened from shut
        uint8_t threshold = 1;
        uint8_t unlockProgress = 0;

        // The last unlock came from a switch. Only picks the message shown when
        // the door starts to move.
        bool switchOperated = false;

        // Lifts will run this same state machine; the notice needs to know which.
        bool isLift = false;

        bool startsOpen = false;     // flag 0x40
        bool isOpen = false;         // flag 0x01: the hold leads to Open rather than Idle
        int holdTicks = 0;           // open time * 4
        int holdCounter = 0;

        Phase phase = Phase::Idle;
        std::array<size_t, 4> cells{};
        int cellCount = 0;

        float worldX = 0.0f, worldZ = 0.0f;
        float baseY = 0.0f;          // mesh height when closed
        float startY = 0.0f;         // mesh height at level start (raised if it starts open)

        // The PlacedObjects that draw this door, owned by the screen.
        size_t placedFirst = 0;
        int placedCount = 0;

        float MeshY() const { return baseY + static_cast<float>(progress) * 32.0f; }
    };

    // What a door did this tick that the player should be told about.
    enum class DoorNotice { None, DoorActivated, DoorPoweredUp, LiftActivated };

    namespace Doors
    {
        // Door travel distance in height units: 30 on the first two levels,
        // 43 from there on.
        int TravelForLevel(const std::string& levelDigits);

        // Builds one state per level.doors record and writes each door's
        // starting floor and ceiling into the four cells it covers.
        // Positions are world space, shifted by the grid origin.
        std::vector<DoorState> Build(Formats::LevelGeometry& level, int travel, float originX, float originZ);

        // Trigger commands 1 (Unlock Door, FUN_00029594) and 6 (Open Door,
        // FUN_00029610). Returns false when the command fails, which aborts the
        // rest of the trigger chain: the door is opening or closing, or already
        // fully unlocked, or - for Open - this would not
        // be the final increment, so a door needing two activations cannot be
        // opened by its own trigger until something else (a switch) has
        // supplied the first.
        bool ApplyCommand(DoorState& door, bool isOpenCommand, int amount, bool viaSwitch);

        // One fixed tick for one door. Writes the current height into the grid.
        DoorNotice Tick(DoorState& door, Formats::LevelGeometry& level, float playerX, float playerZ);
    }
}
