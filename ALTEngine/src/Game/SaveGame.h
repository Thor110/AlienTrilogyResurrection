#pragma once

#include "../Screens/PlayerInventoryState.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace ALTEngine::Game
{
    // A saved game is a checkpoint at the START of a level: which level, at
    // what difficulty, carrying what. That matches where the original offers to
    // save (the end-of-level prompt, before the next mission). With the Modern
    // "Save Anywhere" option it also carries the level as it stood
    // (`snapshot`, Screens/LevelSnapshot.h), and loading puts that back.
    struct SaveData
    {
        std::string name;          // shown in the slot list
        std::string levelDigits;   // "112"
        int difficulty = 0;        // Bootstrap::Difficulty as an int
        Screens::PlayerInventoryState inventory;
        // The level's state as JSON text, for a mid-level save; empty for a
        // checkpoint at the level's start. Kept as text so this header needs
        // no JSON library.
        std::string snapshot;
    };

    namespace SaveGame
    {
        inline constexpr int SLOT_COUNT = 10;
        inline constexpr int FORMAT_VERSION = 1;

        // slot is 1-based, matching the slot screen.
        std::filesystem::path SlotPath(const std::filesystem::path& directory, int slot);

        bool Write(const std::filesystem::path& directory, int slot, const SaveData& data);
        std::optional<SaveData> Read(const std::filesystem::path& directory, int slot);

        // Each slot's name, or nullopt for an empty (or unreadable) slot.
        std::array<std::optional<std::string>, SLOT_COUNT> ListNames(const std::filesystem::path& directory);

        // "112" -> "MISSION 1.1.2", the default name for a checkpoint.
        std::string DefaultName(const std::string& levelDigits);
    }
}
