#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <string>

namespace ALTEngine::Bootstrap
{
    // Every rebindable action - device-agnostic. The same list applies
    // to every device (Edward, 2026: "All options in that list will
    // eventually contain the same options but only accept input from
    // those specific devices... turn the list into a helper that takes
    // an input type, then we can reuse it for keyboard/mouse and the
    // other hardware peripherals"). Based on the disc's own README.TXT
    // control scheme (Ctrl=Fire1, "5" on keypad=Fire2, Space=Do/Use,
    // </>=Strafe, 1-5=Select weapon, 6=Next weapon, Backspace=
    // Turnaround, Caps Lock=Run mode, Shift=Run modifier, Alt=Strafe
    // modifier, Tab/Esc=Weapon select, Pause=Pause; Mouse: Left=Fire1,
    // Right=Fire2). Move Forward/Backward/Strafe Left/Right aren't in
    // the original keyboard scheme by those names (it used keypad
    // arrows for "directions") - named this way to match the WASD
    // scheme GameplayScreen already implements, which these four
    // actions are wired to directly for Keyboard.
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
    };

    // Every action, in the order every device's Redefine list shows
    // them - one shared list, not a separate one per device (Edward,
    // 2026).
    inline const std::array<InputAction, 19>& AllActions()
    {
        static const std::array<InputAction, 19> actions{
            InputAction::MoveForward, InputAction::MoveBackward,
            InputAction::StrafeLeft, InputAction::StrafeRight,
            InputAction::Fire1, InputAction::Fire2, InputAction::Use,
            InputAction::StrafeModifier, InputAction::RunMode, InputAction::RunModifier,
            InputAction::SelectWeapon1, InputAction::SelectWeapon2, InputAction::SelectWeapon3,
            InputAction::SelectWeapon4, InputAction::SelectWeapon5,
            InputAction::NextWeapon, InputAction::Turnaround,
            InputAction::WeaponSelectMenu, InputAction::Pause,
        };
        return actions;
    }

    // Which physical device a binding applies to - only Keyboard and
    // Mouse are actually usable right now (matching Controls' own
    // enabled/disabled split), the rest exist so the same Redefine
    // helper can be reused the moment real hardware exists to test
    // against (Edward, 2026).
    enum class DeviceKind
    {
        Keyboard,
        Mouse,
        Joystick,
        GravisGrip,
        GravisPad,
        SpaceOrb360,
        VFX1,
    };

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
        default: return "";
        }
    }

    // Default scancode for a KEYBOARD binding - matches the README
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

    // MOUSE binding storage - a single Uint8 slot per action, shared
    // between real buttons (SDL_BUTTON_LEFT etc, 1-5+, room for side/
    // extra buttons) and two reserved pseudo-button values for the
    // wheel, rather than a separate variant type. 0 means "unbound" -
    // deliberately not every action gets a sensible default, since
    // there are far fewer physical mouse inputs than actions (Edward,
    // 2026: "there are other mouse buttons, mouse wheel up/down/click
    // for example, sometimes side buttons, sometimes extra buttons").
    // Movement itself isn't given a mouse default (Edward: mouse look
    // already occupies the mouse's continuous-motion channel, so
    // movement-via-mouse-motion the original game supported can't
    // really be matched) - Move/Strafe are still in the list and
    // bindable to a button/wheel-tick if the player wants that,
    // they just don't get an out-of-the-box default.
    constexpr Uint8 MOUSE_UNBOUND = 0;
    constexpr Uint8 MOUSE_WHEEL_UP = 250;
    constexpr Uint8 MOUSE_WHEEL_DOWN = 251;

    inline Uint8 DefaultMouseBinding(InputAction action)
    {
        switch (action)
        {
        case InputAction::Fire1: return SDL_BUTTON_LEFT;
        case InputAction::Fire2: return SDL_BUTTON_RIGHT;
        case InputAction::Use: return SDL_BUTTON_MIDDLE;
        case InputAction::NextWeapon: return MOUSE_WHEEL_UP;
        case InputAction::WeaponSelectMenu: return MOUSE_WHEEL_DOWN;
        default: return MOUSE_UNBOUND;
        }
    }
}
