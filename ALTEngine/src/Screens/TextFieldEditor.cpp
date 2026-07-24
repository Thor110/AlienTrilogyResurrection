#include "TextFieldEditor.h"
#include "../Bootstrap/AppWindow.h"

namespace ALTEngine::Screens
{
    void TextFieldEditor::BeginEdit(const std::string& initialValue, size_t maxLen)
    {
        editing = true;
        value = initialValue;
        maxLength = maxLen;
        SDL_StartTextInput(ALTEngine::Bootstrap::AppWindow::Instance().Window());
    }

    void TextFieldEditor::EndEdit()
    {
        if (!editing) { return; }
        editing = false;
        SDL_StopTextInput(ALTEngine::Bootstrap::AppWindow::Instance().Window());
    }

    bool TextFieldEditor::HandleEvent(const SDL_Event& event)
    {
        if (!editing) { return false; }

        if (event.type == SDL_EVENT_TEXT_INPUT)
        {
            std::string incoming(event.text.text);
            for (char c : incoming)
            {
                if (value.size() < maxLength) { value += c; }
            }
            return true;
        }
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_BACKSPACE)
            {
                if (!value.empty()) { value.pop_back(); }
                return true;
            }
            if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_ESCAPE)
            {
                EndEdit();
                return true;
            }
        }
        return false;
    }
}
