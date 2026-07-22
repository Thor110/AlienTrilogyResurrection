#include "Terminal.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <thread>

namespace ALTEngine::Bootstrap
{
    void Terminal::Type(const std::string& line, std::chrono::milliseconds charDelay) const
    {
        for (char c : line)
        {
            std::cout << c << std::flush;
            if (charDelay.count() > 0)
            {
                std::this_thread::sleep_for(charDelay);
            }
        }
        std::cout << "\n";
    }

    void Terminal::TypeBlock(std::initializer_list<std::string> lines,
                              std::chrono::milliseconds charDelay) const
    {
        for (const auto& line : lines)
        {
            Type(line, charDelay);
        }
    }

    std::string Terminal::Prompt(const std::string& promptText) const
    {
        std::cout << promptText << std::flush;
        std::string input;
        std::getline(std::cin, input);

        // trim whitespace
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        input.erase(input.begin(), std::find_if(input.begin(), input.end(), notSpace));
        input.erase(std::find_if(input.rbegin(), input.rend(), notSpace).base(), input.end());
        return input;
    }

    bool Terminal::Confirm(const std::string& question) const
    {
        while (true)
        {
            std::string answer = Prompt(question + " (Y/N) > ");
            if (!answer.empty())
            {
                char c = static_cast<char>(std::toupper(static_cast<unsigned char>(answer[0])));
                if (c == 'Y') { return true; }
                if (c == 'N') { return false; }
            }
            Type("INVALID RESPONSE.");
        }
    }

    void Terminal::Pause(std::chrono::milliseconds duration) const
    {
        std::this_thread::sleep_for(duration);
    }
}
