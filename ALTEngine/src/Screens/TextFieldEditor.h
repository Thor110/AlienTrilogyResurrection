#pragma once

#include <string>

#include <SDL3/SDL.h>

namespace ALTEngine::Screens
{
    // A small reusable text-entry helper for in-place field editing
    // (YOUR NAME, F2-F9 Message presets, Name Of Game) - shared between
    // MultiplayerOptionsScreen and MultiplayerStartScreen rather than
    // each reimplementing SDL's text-input event handling.
    //
    // Usage: call BeginEdit() when the player presses Enter on a field,
    // then call HandleEvent() for every SDL event while editing is
    // active, and check IsEditing() to know when to stop.
    class TextFieldEditor
    {
    public:
        void BeginEdit(const std::string& initialValue, size_t maxLength);
        void EndEdit(); // stops text input, keeps whatever was typed

        // Returns true if the event was consumed (caller shouldn't also
        // treat it as menu navigation).
        bool HandleEvent(const SDL_Event& event);

        bool IsEditing() const { return editing; }
        const std::string& Value() const { return value; }

    private:
        bool editing = false;
        std::string value;
        size_t maxLength = 32;
    };
}
