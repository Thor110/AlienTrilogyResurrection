#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace ALTEngine::Screens
{
    // In-memory only - there's no real networking backend, so nothing
    // here is actually sent anywhere or persisted. This exists purely so
    // the menu UI has somewhere to hold what the player typed, matching
    // "throw together using the current systems" for the pieces that
    // can't be real yet.
    struct MultiplayerSettings
    {
        std::string playerName = "Player";
        std::array<std::string, 8> messages{}; // F2-F9 quick-chat presets
        std::string gameName;
        int startLevel = 1;    // 1-10
        int minGameLength = 2;
    };

    enum class MultiplayerMainChoice
    {
        StartGame,
        JoinGame,
        Options,
        Back,
    };

    struct MultiplayerMainResult
    {
        bool windowClosed = false;
        MultiplayerMainChoice choice = MultiplayerMainChoice::Back;
    };

    // "Start Multiplayer Game" / "Join Multiplayer Game" / "Multiplayer
    // Options" - matches the reference screenshot exactly. Model is
    // NetworkedComputers (OPTOBJ index 10), shown across all four
    // multiplayer screens.
    class MultiplayerMainScreen
    {
    public:
        static MultiplayerMainResult Run(const std::filesystem::path& cdDirectory);
    };

    struct MultiplayerSubResult
    {
        bool windowClosed = false;
    };

    // "EDIT YOUR DATA" - YOUR NAME + F2-F9 Message presets, all editable
    // in place via TextFieldEditor.
    class MultiplayerOptionsScreen
    {
    public:
        static MultiplayerSubResult Run(const std::filesystem::path& cdDirectory, MultiplayerSettings& settings);
    };

    // "GAME SETUP" - Start Game / Name Of Game / Start at level (1-10) /
    // Minimum game length. No real networking, so "Start Game" has
    // nowhere to hand off to yet - just reported back to the caller.
    struct MultiplayerStartResult
    {
        bool windowClosed = false;
        bool startedGame = false;
    };

    class MultiplayerStartScreen
    {
    public:
        static MultiplayerStartResult Run(const std::filesystem::path& cdDirectory, MultiplayerSettings& settings);
    };

    // "SEARCHING FOR NET GAMES...." - no real network discovery exists,
    // so this always shows 5 empty slots. Escape returns to the main
    // multiplayer menu.
    class MultiplayerJoinScreen
    {
    public:
        static MultiplayerSubResult Run(const std::filesystem::path& cdDirectory);
    };
}
