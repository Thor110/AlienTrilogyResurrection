#pragma once

#include "../Formats/LevelLoader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Game
{
    // Live door state. Doors drive the collision grid directly: the ceiling
    // byte of the four cells they cover rises and falls with the mesh, one
    // height unit (32 world units) per tick, so movement clearance and the
    // visual stay in lockstep.
    struct DoorState
    {
        enum class Phase { Idle, Opening, Open, Closing };

        bool alongZ = false;
        int floorUnits = 0;
        int travel = 30;
        int progress = 0;            // 0 .. travel
        uint8_t threshold = 1;
        uint8_t unlockProgress = 0;

        // Doors opened by throwing a switch stay open; only doors you open by
        // walking up to them close behind you. Behavioural - the dumped Open
        // state has a single distance check with no such distinction.
        bool switchOperated = false;

        // No trigger that can run more than once opens or unlocks this door, so
        // once it is open nothing could ever open it again - it stays open
        // rather than close behind the player and seal them in. On the demo
        // levels these are exactly the doors with an open time of 0, which
        // suggests that is what time 0 means in the original; see
        // MarkSingleUseDoors.
        bool staysOpen = false;

        // Lifts will run this same state machine; the notice needs to know which.
        bool isLift = false;

        // Unlock progress is reset when the door finishes closing, except for
        // doors with flag 0x40, which go through a cooldown counted in hold ticks.
        bool keepsUnlockOnClose = false;
        int holdTicks = 0;
        int cooldown = 0;

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

        // Sets staysOpen on every door that no reusable (uses 0xFF) trigger
        // commands.
        void MarkSingleUseDoors(const Formats::LevelGeometry& level, std::vector<DoorState>& doors);

        // Trigger commands 1 (Unlock Door) and 6 (Open Door). Returns false when
        // the command fails, which aborts the rest of the trigger chain: the
        // door is busy or already fully unlocked, or - for Open - this would not
        // be the final increment, so a door needing two activations cannot be
        // opened by its own trigger until something else (a switch) has
        // supplied the first.
        bool ApplyCommand(DoorState& door, bool isOpenCommand, int amount, bool viaSwitch);

        // One fixed tick for one door. Writes the current height into the grid.
        DoorNotice Tick(DoorState& door, Formats::LevelGeometry& level, float playerX, float playerZ);
    }
}
