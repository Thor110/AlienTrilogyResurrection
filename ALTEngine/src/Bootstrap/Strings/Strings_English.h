#pragma once

#include "../StringId.h"

#include <array>
#include <cstddef>

namespace ALTEngine::Bootstrap
{
    // English is the fallback every other language's table defers to
    // when its own entry is empty (Edward, 2026) - so this one must
    // always be fully populated. Order must exactly match StringId.h.
    inline const std::array<const char*, static_cast<std::size_t>(StringId::Count)>& StringsEnglish()
    {
        static const std::array<const char*, static_cast<std::size_t>(StringId::Count)> table = {
            // Main Menu
            "Start Game",
            "Multiplayer",
            "Load Game",

            // Options top level
            "Options",
            "Volume",
            "Music",
            "SFX",
            "Controls",
            "Keyboard",
            "Mouse",
            "Joystick",
            "Gravis Grip",
            "Gravis Pad",
            "SpaceOrb 360",
            "VFX-1",
            "Difficulty",
            "Acid Reign",
            "Raging Terror",
            "Xenomania",
            "Camera Sway",
            "Off",
            "On",
            "Graphics",
            "Quality",
            "Original",
            "Smoothed",
            "Resolution",
            "Language",
            "Credits",

            // Language names - a language names itself in every table
            "English",
            "Français",
            "Italiano",
            "Español",
            "Deutsch",
            "Japanese 日本語",

            // Redefine controls
            "Redefine",
            "Restore Defaults",
            "ARE YOU SURE ? ",
            "Yes",
            "No",

            // Redefine controls - action names
            "Move Forward",
            "Move Backward",
            "Strafe Left",
            "Strafe Right",
            "Fire 1",
            "Fire 2",
            "Do / Use",
            "Strafe Modifier",
            "Run Mode",
            "Run Modifier",
            "Select Weapon 1",
            "Select Weapon 2",
            "Select Weapon 3",
            "Select Weapon 4",
            "Select Weapon 5",
            "Next Weapon",
            "Turnaround",
            "Weapon Select",
            "Pause",

            // Pause menu - weapon/equipment list
            "Auto Mapper",
            "Shoulder Lamp",
            "9mm Pistol",
            "Shotgun",
            "Flamethrower",
            "Pulse Rifle",
            "Smart Gun",
            "Batteries",
            "Mission",
            "Save Game",
            "Exit Game",
            "SFX Volume",
            "Music Volume",

            // Standalone menu chrome
            "PRESS ESC TO GO BACK",
            "PRESS ENTER TO SELECT",
            "CREDITS",
            "(scroll not yet implemented - see CD/GFX/CREDITS.TXT)",
            "PRESS A KEY TO BIND",
            "PRESS A MOUSE BUTTON OR WHEEL TO BIND",
            "OR ESC TO CANCEL",
            "OPTIONS",
            "Not available",
            "EXIT GAME",
            "VSync",
            "Display Mode",
            "Windowed",
            "Fullscreen",
            "Borderless",
            "Mouse Sensitivity",
        };
        return table;
    }
}
