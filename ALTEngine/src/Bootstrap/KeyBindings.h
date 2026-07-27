#pragma once

#include "Config.h"
#include "InputActions.h"

#include <cstdlib>
#include <string>

namespace ALTEngine::Bootstrap
{
    // Persists per-(device, action) bindings via Config, matching
    // RenderSettings/DifficultySettings' own established pattern. Keyed
    // by BOTH device and action ("Key_0_4" = Keyboard's Fire1,
    // "Key_1_4" = Mouse's Fire1) - Edward, 2026: Mouse now shares the
    // exact same action list as Keyboard, so a single action-only key
    // would have made both devices silently overwrite each other's
    // binding for the same action. Falls back to DefaultScancode/
    // DefaultMouseBinding (InputActions.h) for anything never
    // explicitly rebound.
    class KeyBindings
    {
    public:
        explicit KeyBindings(Config& config) : config(config) {}

        // KEYBOARD - used directly by gameplay code that reads a
        // specific action's keyboard binding (GameplayScreen's
        // movement/pause).
        SDL_Scancode GetKey(InputAction action) const
        {
            return static_cast<SDL_Scancode>(GetRaw(DeviceKind::Keyboard, action, static_cast<int>(DefaultScancode(action))));
        }

        void SetKey(InputAction action, SDL_Scancode scancode)
        {
            SetRaw(DeviceKind::Keyboard, action, static_cast<int>(scancode));
        }

        // MOUSE - a single Uint8 slot shared between real buttons and
        // the two reserved wheel pseudo-values (see InputActions.h).
        Uint8 GetMouseBinding(InputAction action) const
        {
            return static_cast<Uint8>(GetRaw(DeviceKind::Mouse, action, static_cast<int>(DefaultMouseBinding(action))));
        }

        void SetMouseButton(InputAction action, Uint8 button)
        {
            SetRaw(DeviceKind::Mouse, action, static_cast<int>(button));
        }

        void SetMouseWheel(InputAction action, bool up)
        {
            SetRaw(DeviceKind::Mouse, action, static_cast<int>(up ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN));
        }

        // Resets every action's binding for `device` back to its
        // default (Edward, 2026 - "Restore Defaults -> Are You Sure ? ->
        // No / Yes" below the Redefine entry).
        void ResetToDefaults(DeviceKind device)
        {
            for (auto action : AllActions())
            {
                SetRaw(device, action, DefaultRaw(device, action));
            }
        }

        // Generic raw accessor - used by the menu's Redefine capture,
        // which needs to store whatever raw value a device reports
        // without device-specific typed wrappers for every future
        // peripheral (Edward, 2026: "turn the list into a helper that
        // takes an input type").
        int GetRaw(DeviceKind device, InputAction action) const
        {
            return GetRaw(device, action, DefaultRaw(device, action));
        }

        void SetRaw(DeviceKind device, InputAction action, int value)
        {
            config.Set(ConfigKey(device, action), std::to_string(value));
        }

        // Human-readable current binding, for the Redefine list's own
        // label (e.g. "W", "LEFT CTRL", "MOUSE LEFT", "MOUSE WHEEL UP").
        std::string DisplayBinding(DeviceKind device, InputAction action) const
        {
            if (device == DeviceKind::Mouse)
            {
                Uint8 raw = GetMouseBinding(action);
                if (raw == MOUSE_UNBOUND) { return "(unbound)"; }
                if (raw == MOUSE_WHEEL_UP) { return "MOUSE WHEEL UP"; }
                if (raw == MOUSE_WHEEL_DOWN) { return "MOUSE WHEEL DOWN"; }
                if (raw == SDL_BUTTON_LEFT) { return "MOUSE LEFT"; }
                if (raw == SDL_BUTTON_RIGHT) { return "MOUSE RIGHT"; }
                if (raw == SDL_BUTTON_MIDDLE) { return "MOUSE MIDDLE"; }
                return "MOUSE " + std::to_string(raw);
            }
            if (device == DeviceKind::Keyboard)
            {
                const char* name = SDL_GetScancodeName(GetKey(action));
                return (name && *name) ? name : "(unbound)";
            }
            // Other devices (Joystick/Gravis Grip/Gravis Pad/SpaceOrb
            // 360/VFX-1) - no real hardware to test binding storage
            // against yet, so there's nothing meaningful to show
            // beyond this placeholder (Edward, 2026: these stay
            // disabled in Controls until that changes).
            return "(unavailable)";
        }

    private:
        static std::string ConfigKey(DeviceKind device, InputAction action)
        {
            return "Key_" + std::to_string(static_cast<int>(device)) + "_" + std::to_string(static_cast<int>(action));
        }

        static int DefaultRaw(DeviceKind device, InputAction action)
        {
            if (device == DeviceKind::Keyboard) { return static_cast<int>(DefaultScancode(action)); }
            if (device == DeviceKind::Mouse) { return static_cast<int>(DefaultMouseBinding(action)); }
            return 0;
        }

        int GetRaw(DeviceKind device, InputAction action, int fallback) const
        {
            auto value = config.Get(ConfigKey(device, action));
            return value.has_value() ? std::atoi(value->c_str()) : fallback;
        }

        Config& config;
    };
}
