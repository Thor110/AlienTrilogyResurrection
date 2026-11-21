#pragma once

#include "../Bootstrap/Strings.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ALTEngine::Screens
{
    // THE MULTIPLAYER RESULTS - FUN_0001560c, its table drawn by FUN_00015228.
    //
    // After a network game: "SCORES" (string 112) centred at y 0x14 in the
    // menu font, then a table in the HUD's font C. With n players:
    //     left = (4 - n) * 0x1b + 8,  top = (4 - n) * 10 + 0x40
    //   - a header row at `top`: each player's name over its column, column k
    //     at left + 0x3c + k * 0x37, moved right 3, 2, 1 or 0 spaces (5 px) for
    //     a name under 4, 4-5, 6-7 or more letters; then "=" at
    //     left + n * 0x37 + 0x4b, grey;
    //   - a row per player, 0x14 apart from top + 0x14: the name, right-set in
    //     8 places (left + (8 - letters) * 5), in the player's colour; under
    //     each column the kills of that player ("%4d", grey) - in its own
    //     column, minus its own deaths; and the score under "=" at
    //     left + 0x3c + n * 0x37.
    // The rows go by score, highest first (LAB_000155f4). The colours are
    // 0xa6588's: grey, red, green, blue for players 1-4 (0x80 full).
    // "Press any key" (110) at y 0xbe; a key, or 700 frames, and the screen
    // fades out (FUN_0004eefc).
    //
    // OURS: up to eight players (the original has four) - past four the
    // columns close up to fit, and players 5-8 take yellow, cyan, magenta and
    // orange. The original then waits for every machine to be done ("Waiting
    // for players", 111, up to 0x348 frames); here each machine leaves on its
    // own key.
    struct MultiplayerResultsPlayer
    {
        std::string name;
        int number = 1;           // 1-8, the colour
        int score = 0;            // the roster's frags
        std::vector<int> kills;   // against each player, in `players` order
    };

    namespace MultiplayerResults
    {
        struct Cell { std::string text; int x = 0; int y = 0; int colour = 0; };   // colour: player number, 0 grey

        // Where everything goes, in 320x240.
        inline std::vector<Cell> Layout(std::vector<MultiplayerResultsPlayer> players)
        {
            std::vector<Cell> cells;
            const int n = static_cast<int>(players.size());
            if (n == 0) { return cells; }
            // Highest score first (LAB_000155f4); equal scores keep their order.
            std::vector<int> order(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) { order[static_cast<size_t>(i)] = i; }
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                return players[static_cast<size_t>(a)].score > players[static_cast<size_t>(b)].score;
            });

            const int fit = std::min(n, 4);
            const int left = (4 - fit) * 0x1b + 8;
            const int top = (4 - fit) * 10 + 0x40;
            // OURS past four players: the columns close up to fit 320.
            const int step = n <= 4 ? 0x37 : (320 - left - 0x3c - 0x28) / n;

            auto format4 = [](int v) { char b[16]; std::snprintf(b, sizeof b, "%4d", v); return std::string(b); };

            // The header: the names over their columns, then "=".
            for (int k = 0; k < n; ++k)
            {
                const auto& p = players[static_cast<size_t>(order[static_cast<size_t>(k)])];
                const int len = static_cast<int>(p.name.size());
                const int pad = len < 4 ? 3 : len < 6 ? 2 : len < 8 ? 1 : 0;
                cells.push_back({ p.name, left + 0x3c + k * step + pad * 5, top, p.number });
            }
            cells.push_back({ "=", left + n * step + 0x4b, top, 0 });

            // A row a player.
            for (int r = 0; r < n; ++r)
            {
                const int ri = order[static_cast<size_t>(r)];
                const auto& p = players[static_cast<size_t>(ri)];
                const int y = top + 0x14 + r * 0x14;
                const int len = std::min(static_cast<int>(p.name.size()), 8);
                cells.push_back({ p.name, left + (8 - len) * 5, y, p.number });
                for (int k = 0; k < n; ++k)
                {
                    const int ci = order[static_cast<size_t>(k)];
                    const int kills = ci < static_cast<int>(p.kills.size()) ? p.kills[static_cast<size_t>(ci)] : 0;
                    cells.push_back({ format4(ci == ri ? -kills : kills), left + 0x46 + k * step, y, 0 });
                }
                cells.push_back({ format4(p.score), left + 0x3c + n * step, y, 0 });
            }
            return cells;
        }

        inline constexpr int TITLE_Y = 0x14;
        inline constexpr int PROMPT_Y = 0xbe;
        inline constexpr int FRAMES_TO_LEAVE = 700;    // at 34.87 a second
    }

    class MultiplayerResultsScreen
    {
    public:
        // False if the window was closed. `idle` keeps the session read.
        static bool Run(const std::filesystem::path& cdDirectory, Bootstrap::Language language,
                        const std::string& levelDigits, const std::vector<MultiplayerResultsPlayer>& players,
                        const std::function<void()>& idle = {});
    };
}
