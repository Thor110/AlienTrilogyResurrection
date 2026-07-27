#pragma once

#include "Config.h"
#include "InputActions.h"

#include <cstdlib>
#include <string>

namespace ALTEngine::Bootstrap
{
    // Persists keyboard/mouse rebindings via Config, matching
    // RenderSettings/DifficultySettings' own established pattern. One
    // Config key per action ("Key_MoveForward", "Key_Fire1", etc),
    // storing the scancode/button as a plain integer string. Falls back
    // to DefaultScancode/DefaultMouseButton (InputActions.h) for any
    // action that's never been explicitly rebound (Edward, 2026).
    class KeyBindings
    {
    public:
        explicit KeyBindings(Config& config) : config(config) {}

        SDL_Scancode GetKey(InputAction action) const
        {
            auto value = config.Get(ConfigKey(action));
            if (!value.has_value()) { return DefaultScancode(action); }
            int raw = std::atoi(value->c_str());
            return static_cast<SDL_Scancode>(raw);
        }

        void SetKey(InputAction action, SDL_Scancode scancode)
        {
            config.Set(ConfigKey(action), std::to_string(static_cast<int>(scancode)));
        }

        Uint8 GetMouseButton(InputAction action) const
        {
            auto value = config.Get(ConfigKey(action));
            if (!value.has_value()) { return DefaultMouseButton(action); }
            int raw = std::atoi(value->c_str());
            return static_cast<Uint8>(raw);
        }

        void SetMouseButton(InputAction action, Uint8 button)
        {
            config.Set(ConfigKey(action), std::to_string(static_cast<int>(button)));
        }

        // Human-readable current binding, for the Redefine list's own
        // label (e.g. "W", "LEFT CTRL", "MOUSE LEFT").
        std::string DisplayBinding(InputAction action, bool isMouseAction) const
        {
            if (isMouseAction)
            {
                Uint8 button = GetMouseButton(action);
                if (button == SDL_BUTTON_LEFT) { return "MOUSE LEFT"; }
                if (button == SDL_BUTTON_RIGHT) { return "MOUSE RIGHT"; }
                if (button == SDL_BUTTON_MIDDLE) { return "MOUSE MIDDLE"; }
                return "MOUSE " + std::to_string(button);
            }
            const char* name = SDL_GetScancodeName(GetKey(action));
            return (name && *name) ? name : "(unbound)";
        }

    private:
        static std::string ConfigKey(InputAction action)
        {
            return "Key_" + std::to_string(static_cast<int>(action));
        }

        Config& config;
    };
}
