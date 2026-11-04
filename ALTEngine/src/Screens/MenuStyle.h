#pragma once

#include "../Bootstrap/Font8x8.h"

namespace ALTEngine::Screens::MenuStyle
{
    // The colours the port's own menu screens share (end-of-level prompt,
    // multiplayer, pause, save slots, the co-operative syncing screen).
    inline constexpr Bootstrap::Color COLOR_CURSOR{ 51, 255, 102, 255 };
    inline constexpr Bootstrap::Color COLOR_DIM{ 24, 130, 52, 255 };
    inline constexpr Bootstrap::Color COLOR_WHITE{ 255, 255, 255, 255 };   // the main menu's selected-item white

    // The selected item pulses between green and white rather than sitting
    // at a static light green (Edward, 2026).
    inline Bootstrap::Color SelectedTextColor()
    {
        return Bootstrap::LerpColor(COLOR_CURSOR, COLOR_WHITE, Bootstrap::PulsePhase());
    }
}
