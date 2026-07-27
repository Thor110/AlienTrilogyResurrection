#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <string>

namespace ALTEngine::Bootstrap
{
    // Every rebindable action, keyboard and mouse together - based on
    // the disc's own README.TXT control scheme (Edward, 2026, quoting
    // it directly: Ctrl=Fire1, "5" on keypad=Fire2, Space=Do/Use,
    // </>=Strafe, 1-5=Select weapon, 6=Next weapon, Backspace=
    // Turnaround, Caps Lock=Run mode, Shift=Run modifier, Alt=Strafe
    // modifier, Tab/Esc=Weapon select, Pause=Pause; Mouse: Left=Fire1,
    // Right=Fire2). Move Forward/Backward/Strafe Left/Right aren't in
    // the original keyboard scheme by those names (it used keypad
    // arrows for "directions") - named this way to match the WASD
    // scheme GameplayScreen already implements, which these four
    // actions are wired to directly.
    enum class InputAction
    {
        MoveForward,
        MoveBackward,
        StrafeLeft,
        StrafeRight,
        Fire1,
        Fire2,
        Use,
        StrafeModifier,
        RunMode,
        RunModifier,
        SelectWeapon1,
        SelectWeapon2,
        SelectWeapon3,
        SelectWeapon4,
        SelectWeapon5,
        NextWeapon,
        Turnaround,
        WeaponSelectMenu,
        Pause,
        // Mouse-only - not in KeyboardActions() below
        MouseFire1,
        MouseFire2,
    };

    // Every keyboard-bindable action, in the order the Redefine list
    // shows them.
    inline const std::array<InputAction, 18>& KeyboardActions()
    {
        static const std::array<InputAction, 18> actions{
            InputAction::MoveForward, InputAction::MoveBackward,
            InputAction::StrafeLeft, InputAction::StrafeRight,
            InputAction::Fire1, InputAction::Fire2, InputAction::Use,
            InputAction::StrafeModifier, InputAction::RunMode, InputAction::RunModifier,
            InputAction::SelectWeapon1, InputAction::SelectWeapon2, InputAction::SelectWeapon3,
            InputAction::SelectWeapon4, InputAction::SelectWeapon5,
            InputAction::NextWeapon, InputAction::Turnaround,
            InputAction::WeaponSelectMenu,
        };
        return actions;
    }

    // Pause is deliberately not in KeyboardActions() above - it's
    // listed separately here since it's shown as its own last entry
    // (see MenuTree.cpp), not mixed in with weapon/movement actions.
    inline const std::array<InputAction, 2>& MouseActions()
    {
        static const std::array<InputAction, 2> actions{ InputAction::MouseFire1, InputAction::MouseFire2 };
        return actions;
    }

    // Display label shown in the Redefine list (e.g. "Move Forward",
    // "Select Weapon 1").
    inline std::string ActionLabel(InputAction action)
    {
        switch (action)
        {
        case InputAction::MoveForward: return "Move Forward";
        case InputAction::MoveBackward: return "Move Backward";
        case InputAction::StrafeLeft: return "Strafe Left";
        case InputAction::StrafeRight: return "Strafe Right";
        case InputAction::Fire1: return "Fire 1";
        case InputAction::Fire2: return "Fire 2";
        case InputAction::Use: return "Do / Use";
        case InputAction::StrafeModifier: return "Strafe Modifier";
        case InputAction::RunMode: return "Run Mode";
        case InputAction::RunModifier: return "Run Modifier";
        case InputAction::SelectWeapon1: return "Select Weapon 1";
        case InputAction::SelectWeapon2: return "Select Weapon 2";
        case InputAction::SelectWeapon3: return "Select Weapon 3";
        case InputAction::SelectWeapon4: return "Select Weapon 4";
        case InputAction::SelectWeapon5: return "Select Weapon 5";
        case InputAction::NextWeapon: return "Next Weapon";
        case InputAction::Turnaround: return "Turnaround";
        case InputAction::WeaponSelectMenu: return "Weapon Select";
        case InputAction::Pause: return "Pause";
        case InputAction::MouseFire1: return "Fire 1";
        case InputAction::MouseFire2: return "Fire 2";
        default: return "";
        }
    }

    // Default scancode for a keyboard action - matches the README
    // where the action exists in the original scheme (Fire1=Ctrl,
    // Fire2=Keypad 5, Use=Space, weapon select=1-6, Turnaround=
    // Backspace, RunMode=Caps Lock, RunModifier=Shift, StrafeModifier=
    // Alt, WeaponSelectMenu=Tab, Pause=Escape - matching the pause key
    // GameplayScreen already hardcodes). Move/Strafe default to WASD,
    // matching the scheme already implemented, not the original
    // keypad-arrows scheme.
    inline SDL_Scancode DefaultScancode(InputAction action)
    {
        switch (action)
        {
        case InputAction::MoveForward: return SDL_SCANCODE_W;
        case InputAction::MoveBackward: return SDL_SCANCODE_S;
        case InputAction::StrafeLeft: return SDL_SCANCODE_A;
        case InputAction::StrafeRight: return SDL_SCANCODE_D;
        case InputAction::Fire1: return SDL_SCANCODE_LCTRL;
        case InputAction::Fire2: return SDL_SCANCODE_KP_5;
        case InputAction::Use: return SDL_SCANCODE_SPACE;
        case InputAction::StrafeModifier: return SDL_SCANCODE_LALT;
        case InputAction::RunMode: return SDL_SCANCODE_CAPSLOCK;
        case InputAction::RunModifier: return SDL_SCANCODE_LSHIFT;
        case InputAction::SelectWeapon1: return SDL_SCANCODE_1;
        case InputAction::SelectWeapon2: return SDL_SCANCODE_2;
        case InputAction::SelectWeapon3: return SDL_SCANCODE_3;
        case InputAction::SelectWeapon4: return SDL_SCANCODE_4;
        case InputAction::SelectWeapon5: return SDL_SCANCODE_5;
        case InputAction::NextWeapon: return SDL_SCANCODE_6;
        case InputAction::Turnaround: return SDL_SCANCODE_BACKSPACE;
        case InputAction::WeaponSelectMenu: return SDL_SCANCODE_TAB;
        case InputAction::Pause: return SDL_SCANCODE_ESCAPE;
        default: return SDL_SCANCODE_UNKNOWN;
        }
    }

    // Default mouse button for a mouse action (SDL_BUTTON_LEFT/RIGHT -
    // matches the README: "Left button: Fire 1, Right button: Fire 2").
    inline Uint8 DefaultMouseButton(InputAction action)
    {
        return (action == InputAction::MouseFire2) ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
    }
}
