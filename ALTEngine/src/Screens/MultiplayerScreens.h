#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace ALTEngine::Screens
{
    // What the multiplayer menus hold. main.cpp keeps the name, look and
    // host settings in the config and hands them to Net::Session.
    struct MultiplayerSettings
    {
        std::string playerName = "Player";
        // How the others see this player (Net::Skin): the original's Synthetic,
        // or the Soldier, Guard or Handler.
        int skin = 0;
        std::array<std::string, 8> messages{}; // F2-F9 quick-chat presets
        std::string gameName;
        int startLevel = 1;    // 1-10
        int minGameLength = 2;
        // The port's own: how many may play (4-8, bots included), how many
        // bots the host runs (0 for none, else 3-7, always leaving room for
        // the host) and how well they play (1-5). See Game::Bots.
        int playerLimit = 4;
        int botCount = 0;
        int botSkill = 3;
        // Bots allowed at a player limit: one place is always the host's.
        static int MaxBots(int limit) { return limit - 1 < 7 ? limit - 1 : 7; }
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
    //
    // `cursor` is owned by the caller and persists across repeated
    // calls (main.cpp calls this in a loop each time a sub-screen
    // returns) - Edward, 2026: "each of the multiplayer menus
    // returning to the multiplayer menu" should keep whatever was
    // previously selected, not reset to the first item.
    class MultiplayerMainScreen
    {
    public:
        static MultiplayerMainResult Run(const std::filesystem::path& cdDirectory, int& cursor);
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

    // "SEARCHING FOR NET GAMES...." - the games on the local network fill the
    // original's five slots as they answer (Net::GameBrowser); a sixth line
    // takes an address for a game further away. Enter on either picks it;
    // Escape returns to the main multiplayer menu.
    struct MultiplayerJoinResult
    {
        bool windowClosed = false;
        bool chosen = false;
        std::string address;   // "1.2.3.4:port", or as typed
    };

    class MultiplayerJoinScreen
    {
    public:
        static MultiplayerJoinResult Run(const std::filesystem::path& cdDirectory, std::string& typedAddress);
    };
}
