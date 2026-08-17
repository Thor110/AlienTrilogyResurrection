#pragma once

#include "../Bootstrap/Strings.h"
#include "../Renderer/HudPanel.h"

#include <deque>
#include <string>

namespace ALTEngine::Screens
{
    // The typed-out messages the game prints during play - the pickup name when
    // you collect something, and the switch and door notices.
    //
    // THE TEXT IS THE ORIGINAL'S. Every message comes from the .BIN extraction
    // now carried in the language packs as Gs* keys: pickup names are
    // GsPickup<Name> at original indices 137-161, and the world notices are
    // GsDoorActivated, GsDoorPoweredUp, GsSteamValveClosed, GsFlameJetShutDown,
    // GsLiftActivated and GsBatteryRequired at 82-87. So it translates with
    // everything else, and German gets its real strings.
    //
    // THE PRESENTATION IS OURS, and marked as such. The original reveals its text
    // a character at a time in a panel at the top left of the HUD, but nothing
    // traced yet gives the reveal rate, how long a message holds, or how many can
    // stack - so the three constants below are chosen by eye.
    class HudMessages
    {
    public:
        // How fast the text appears and how long it stays once complete, in the
        // original's logic ticks. GUESSES.
        static constexpr int TICKS_PER_CHARACTER = 2;
        static constexpr int HOLD_TICKS = 90;
        static constexpr size_t MAX_QUEUED = 4;

        // Queues one message by string id. Duplicates already showing are
        // ignored, so walking over a cluster of the same pickup does not stack
        // four identical lines.
        void Show(ALTEngine::Bootstrap::StringId id, ALTEngine::Bootstrap::Language language)
        {
            std::string text = ALTEngine::Bootstrap::Tr(id, language);
            if (text.empty()) { return; }
            Show(text);
        }

        void Show(const std::string& text)
        {
            if (text.empty()) { return; }
            for (const Line& line : lines)
            {
                if (line.text == text) { return; }
            }
            if (lines.size() >= MAX_QUEUED) { lines.pop_front(); }
            lines.push_back(Line{ text, 0, 0 });
        }

        // A pickup's own name, index 137 + type in the original's list.
        void ShowPickup(int pickupType, ALTEngine::Bootstrap::Language language)
        {
            const int id = static_cast<int>(ALTEngine::Bootstrap::StringId::GsPickup9mmAutomatic) + pickupType;
            if (id < static_cast<int>(ALTEngine::Bootstrap::StringId::GsPickup9mmAutomatic)
                || id > static_cast<int>(ALTEngine::Bootstrap::StringId::GsPickupShoulderLamp))
            {
                return;
            }
            Show(static_cast<ALTEngine::Bootstrap::StringId>(id), language);
        }

        // One of the original's logic ticks.
        void Tick()
        {
            // ONE LINE AT A TIME, in order. Advancing every incomplete line at
            // once had three messages appearing letter by letter in parallel,
            // which does not read as a terminal printing - it reads as three
            // things happening at once. The oldest unfinished line types until it
            // is done, then the next starts.
            bool typedThisTick = false;
            for (Line& line : lines)
            {
                if (line.revealed < static_cast<int>(line.text.size()))
                {
                    if (typedThisTick) { break; }   // it waits its turn
                    if (++line.timer >= TICKS_PER_CHARACTER)
                    {
                        line.timer = 0;
                        line.revealed++;
                    }
                    typedThisTick = true;
                }
                else
                {
                    // Only start counting toward expiry once it is complete.
                    line.timer++;
                }
            }
            while (!lines.empty()
                   && lines.front().revealed >= static_cast<int>(lines.front().text.size())
                   && lines.front().timer > HOLD_TICKS)
            {
                lines.pop_front();
            }
        }

        // What to draw, newest last, already truncated to the revealed length.
        std::vector<std::string> Visible() const
        {
            std::vector<std::string> out;
            for (const Line& line : lines)
            {
                out.push_back(line.text.substr(0, static_cast<size_t>(line.revealed)));
            }
            return out;
        }

        // True while the newest line is still appearing, so the caller can show
        // a caret.
        // True while any line is still appearing, and the caret belongs on that
        // line rather than always on the last.
        bool Typing() const
        {
            for (const Line& line : lines)
            {
                if (line.revealed < static_cast<int>(line.text.size())) { return true; }
            }
            return false;
        }

        // Which line the caret sits on - the oldest unfinished one.
        size_t TypingLine() const
        {
            for (size_t i = 0; i < lines.size(); ++i)
            {
                if (lines[i].revealed < static_cast<int>(lines[i].text.size())) { return i; }
            }
            return lines.size();
        }

        bool Empty() const { return lines.empty(); }
        void Clear() { lines.clear(); }

    private:
        struct Line
        {
            std::string text;
            int revealed = 0;
            int timer = 0;
        };

        std::deque<Line> lines;
    };
}
