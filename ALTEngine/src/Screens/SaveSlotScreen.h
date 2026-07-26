#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>

#include "../Bootstrap/Localization.h"

namespace ALTEngine::Screens
{
    enum class SaveSlotMode
    {
        Load,
        Save,
    };

    struct SaveSlotInfo
    {
        std::string name; // e.g. "TWENTYTHIRD" - empty/ignored if !used
        bool used = false;
    };

    // There's no real save/load system yet, so this always shows the
    // same 10-slot layout matching the reference screenshots exactly
    // (slot 1 = "TWENTYTHIRD", 2-10 unused) rather than reading anything
    // real from disk. Swap this out once real save data exists - nothing
    // else about SaveSlotScreen should need to change.
    std::array<SaveSlotInfo, 10> StubSaveSlots();

    struct SaveSlotResult
    {
        bool windowClosed = false;
        std::optional<int> selectedSlot; // 1-10, or nullopt if the player backed out without choosing
    };

    // Load Game / Save Game - same layout, same navigation, just a
    // different title and model (HarddriveRight/"Loading ->" for Load,
    // HarddriveLeft/"Saving <-" for Save, both confirmed OPTOBJ indices)
    // - matches the reference screenshots, which are otherwise identical
    // in structure.
    //
    // showBackground controls whether the LOGOSGFX generic-menu
    // background (imageIndex 1) is loaded and drawn - true (the
    // default) matches the main menu's own "Load Game" path, which
    // already shows this background. Pass false for the pause menu's
    // own Save Game/Load Game entries (Edward, 2026: "As the pause
    // menu is a black background, I feel we should match that so as to
    // not be so jarring") - just clears to black instead, same as
    // every other black-background screen already does.
    class SaveSlotScreen
    {
    public:
        static SaveSlotResult Run(
            const std::filesystem::path& cdDirectory,
            SaveSlotMode mode,
            const std::array<SaveSlotInfo, 10>& slots,
            bool showBackground = true);
    };
}
