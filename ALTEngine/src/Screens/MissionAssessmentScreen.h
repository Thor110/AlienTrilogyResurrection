#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "../Bootstrap/Localization.h"

namespace ALTEngine::Screens
{
    struct MissionAssessmentStats
    {
        float aliensPercent = 0.0f;
        float secretsPercent = 0.0f;
        float missionPercent = 0.0f;
        int requiredPercent = 15;   // the mission figure the level needs (15, 25 or 45)
        std::string levelDigits;    // the level just played, for its palette
    };

    struct MissionAssessmentResult
    {
        bool windowClosed = false;
    };

    // The end-of-level "MISSION ASSESSMENT" screen, as the original draws it
    // (FUN_00012abc each frame, set up by FUN_00014f44).
    //
    // On a 320 x 240 screen: LOGOSGFX's second picture behind everything
    // (FUN_00013dd0, pieces 7 and 8); the title centred at y 24; three rows -
    // Aliens, Secrets, Mission - each a label at x 26, the figure and "%" at
    // (110, row - 8), and a bar at (100, row - 12); and, once every figure has
    // counted up, "press any key" centred at y 210. The text is FONT1GFX's
    // 16-pixel face (FUN_000187f4); the bar is FONT1GFX descriptor 118, a
    // frame whose rising silhouettes are see-through, over up to ten flat
    // segments (FUN_00012858). The figures count up one a frame, Aliens first,
    // with a tick (sound 0x27) every eighth frame, and the label and figure of
    // each row are coloured by its value from a red-yellow-green table
    // (FUN_00014ec0) that turns at the level's pass mark.
    class MissionAssessmentScreen
    {
    public:
        static MissionAssessmentResult Run(
            const std::filesystem::path& cdDirectory,
            const MissionAssessmentStats& stats,
            Bootstrap::Language language = Bootstrap::Language::English);

        // FUN_00014ec0's colour table: entry v is the colour of a figure of v,
        // (red, green, 0) on the original's 0x80-is-full-brightness scale.
        struct Rgb { uint8_t r, g, b; };
        static std::array<Rgb, 101> FigureColours(int requiredPercent);
    };

    inline std::array<MissionAssessmentScreen::Rgb, 101> MissionAssessmentScreen::FigureColours(int requiredPercent)
    {
        // FUN_00014ec0, step for step: red 0x99 at first; from 10 below the
        // pass mark green climbs 9 a step to 0x99; past 75 red falls 9 a step,
        // to nothing once it is under 10.
        std::array<Rgb, 101> out{};
        unsigned u = 0x9900;
        for (int i = 0; i <= 100; ++i)
        {
            unsigned v = u;
            const unsigned red = u >> 8;
            if (red != 0 && i > 0x4b)
            {
                const unsigned dropped = (red - 9) & 0xffu;
                v = (dropped << 8) | (u & 0xffu);
                if (dropped < 10) { v = u & 0xffu; }
            }
            if ((v & 0xffu) != 0x99u && (requiredPercent - 10) < i)
            {
                unsigned g = ((v & 0xffu) + 9) & 0xffu;
                if (g > 0x8f) { g = 0x99; }
                v = (v & 0xff00u) | g;
            }
            out[static_cast<size_t>(i)] = Rgb{ uint8_t(v >> 8), uint8_t(v & 0xffu), 0 };
            u = v;
        }
        return out;
    }
}
