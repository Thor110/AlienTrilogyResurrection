#pragma once

#include <iosfwd>
#include <memory>
#include <string>

namespace ALTEngine::Bootstrap
{
    // THE MU/TH/UR 9000 READOUT FOR THE BOOT.
    //
    // While one is alive, everything the boot writes to std::cout - finding
    // the game, installing it from the disc or an installation, ripping the
    // music, the patches, the Level Cleanup and the texture blanking - is also
    // traced out on the game window in the MU/TH/UR style the folder picker
    // uses: green on black, upper case, the headline lines typed out a
    // character at a time and the per-file lines shown as they come. The
    // command window still gets every line, behind (Edward, 2026: to be
    // hidden or removed later).
    //
    // Any key or click skips the typing. Closing the window asks the boot to
    // stop (QuitRequested). Only the thread that made it draws; a line from
    // another thread is kept and shown with the next one.
    class MuthurConsole
    {
    public:
        explicit MuthurConsole(const std::string& title);
        ~MuthurConsole();   // puts std::cout back, after the closing hold

        MuthurConsole(const MuthurConsole&) = delete;
        MuthurConsole& operator=(const MuthurConsole&) = delete;

        // The closing hold: wait for a key (after an install, so it can be
        // read) rather than the short pause an ordinary start gets.
        static void WaitForKeyAtEnd();

        // The window was closed while the readout was up.
        static bool QuitRequested();

        // Draws now (the folder picker took the screen and gave it back).
        static void Redraw();

        struct State;

    private:
        std::unique_ptr<State> state;
        std::streambuf* previous = nullptr;
        std::unique_ptr<std::streambuf> tee;
    };
}
