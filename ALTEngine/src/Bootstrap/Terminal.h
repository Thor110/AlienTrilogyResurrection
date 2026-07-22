#pragma once

#include <chrono>
#include <string>

namespace ALTEngine::Bootstrap
{
    // Console-only MUTHUR-style output. Deliberately just wraps stdio -
    // if this ever grows a graphical CRT front end later, that becomes a
    // separate implementation behind the same interface rather than
    // bolted onto this one.
    class Terminal
    {
    public:
        // Prints a line character-by-character with a small delay, like
        // the film's teletype output. Pass delay = 0 to disable (e.g. for
        // automated/CI runs).
        void Type(const std::string& line,
                   std::chrono::milliseconds charDelay = std::chrono::milliseconds(12)) const;

        // Blank-line separated block, each line typed in sequence.
        void TypeBlock(std::initializer_list<std::string> lines,
                        std::chrono::milliseconds charDelay = std::chrono::milliseconds(12)) const;

        // Prints "> " and reads a line of input, trimmed.
        std::string Prompt(const std::string& promptText = "> ") const;

        // Yes/no prompt, keeps asking until it gets a Y/N answer.
        bool Confirm(const std::string& question) const;

        void Pause(std::chrono::milliseconds duration) const;
    };
}
