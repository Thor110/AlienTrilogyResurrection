#pragma once

#include "../Bootstrap/Strings.h"
#include "../Renderer/HudPanel.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

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
    // THE TIMING IS THE ORIGINAL'S (FUN_0003b1f0 queues, the task at 0x3b0c8
    // shows, FUN_0003b044 picks the next): four slots; a message goes into
    // the highest free one, and is dropped if none is free; ONE message shows
    // at a time - the lowest occupied slot - a character a pass, with a caret
    // after it that is solid while it types and blinks four passes on, four
    // off, once it is done; 40 passes after its last character it goes, and
    // the next starts. Where it sits on the HUD is ours (HudPanel.h).
    class HudMessages
    {
    public:
        static constexpr int SLOT_COUNT = 4;
        // Passes a finished message stays (the task's `+ 0x28`).
        static constexpr int HOLD_TICKS = 0x28;
        // The caret's blink: off while bit 2 of the count is set.
        static constexpr int CARET_BLINK_BIT = 4;

        // Queues one message by string id.
        void Show(ALTEngine::Bootstrap::StringId id, ALTEngine::Bootstrap::Language language)
        {
            Show(ALTEngine::Bootstrap::Tr(id, language));
        }

        // FUN_0003b1f0: into the highest free slot, or nowhere.
        void Show(const std::string& text)
        {
            if (text.empty()) { return; }
            for (int i = SLOT_COUNT - 1; i >= 0; --i)
            {
                Slot& slot = slots[static_cast<size_t>(i)];
                if (!slot.used)
                {
                    slot = Slot{ true, text, 0 };
                    return;
                }
            }
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

        // One pass of the message task (0x3b0c8).
        void Tick()
        {
            if (current < 0)
            {
                // FUN_0003b044: the lowest occupied slot.
                for (int i = 0; i < SLOT_COUNT; ++i)
                {
                    if (slots[static_cast<size_t>(i)].used) { current = i; break; }
                }
                if (current < 0) { return; }
            }
            Slot& slot = slots[static_cast<size_t>(current)];
            slot.count++;
            if (slot.count >= static_cast<int>(slot.text.size()) + HOLD_TICKS)
            {
                slot = Slot{};
                current = -1;
            }
        }

        // What to draw: the message showing, cut to what has typed so far.
        std::vector<std::string> Visible() const
        {
            if (current < 0) { return {}; }
            const Slot& slot = slots[static_cast<size_t>(current)];
            const size_t shown = std::min(slot.text.size(), static_cast<size_t>(slot.count));
            return { slot.text.substr(0, shown) };
        }

        // Which line the caret sits on: the one showing, while the caret is
        // lit - always while it types, then on the blink. NO_CARET otherwise.
        static constexpr size_t NO_CARET = static_cast<size_t>(-1);
        size_t TypingLine() const
        {
            if (current < 0) { return NO_CARET; }
            const Slot& slot = slots[static_cast<size_t>(current)];
            if (slot.count > static_cast<int>(slot.text.size()) && (slot.count & CARET_BLINK_BIT) != 0) { return NO_CARET; }
            return 0;
        }

        // ---------------------------------------------------------------
        // The world notices
        // ---------------------------------------------------------------
        //
        // Six of them, at the original's string indices 82-87, now the Gs keys:
        //     GsDoorActivated     "Door activated"
        //     GsDoorPoweredUp     "Door powered up"
        //     GsSteamValveClosed  "Steam valve closed"
        //     GsFlameJetShutDown  "Flame jet shut down"
        //     GsLiftActivated     "Lift activated"
        //     GsBatteryRequired   "Battery required"
        //
        // TRACED. The strings load into a pointer array at DAT_00402f20, so
        // 82-87 are DAT_00403068 to DAT_0040307c, and FUN_0003b1f0 is the
        // printer:
        //   - FUN_00029594, a door's switch count: "powered up" for each switch
        //     short of the number the door needs, "activated" (single player
        //     only) when the last one is thrown;
        //   - FUN_000358a4, a vent shut off: "steam valve closed" for types
        //     0x10 and 0x12, "flame jet shut down" for the others;
        //   - FUN_00036f4c, a battery switch (0x16, 0x1a) used with no battery:
        //     "battery required";
        //   - FUN_0003ccb4, a pickup taken: its name, from the table at
        //     DAT_000acd78.
        // "Lift activated" (DAT_00403078) is not named by any call in the
        // export; the port shows it when a lift first runs.
        void ShowDoorActivated(ALTEngine::Bootstrap::Language language)
        {
            Show(ALTEngine::Bootstrap::StringId::GsDoorActivated, language);
        }
        void ShowDoorPoweredUp(ALTEngine::Bootstrap::Language language)
        {
            Show(ALTEngine::Bootstrap::StringId::GsDoorPoweredUp, language);
        }
        void ShowLiftActivated(ALTEngine::Bootstrap::Language language)
        {
            Show(ALTEngine::Bootstrap::StringId::GsLiftActivated, language);
        }
        void ShowBatteryRequired(ALTEngine::Bootstrap::Language language)
        {
            Show(ALTEngine::Bootstrap::StringId::GsBatteryRequired, language);
        }
        void ShowSteamValveClosed(ALTEngine::Bootstrap::Language language)
        {
            Show(ALTEngine::Bootstrap::StringId::GsSteamValveClosed, language);
        }
        void ShowFlameJetShutDown(ALTEngine::Bootstrap::Language language)
        {
            Show(ALTEngine::Bootstrap::StringId::GsFlameJetShutDown, language);
        }

        bool Empty() const { return current < 0 && !slots[0].used && !slots[1].used && !slots[2].used && !slots[3].used; }
        void Clear() { slots = {}; current = -1; }

    private:
        struct Slot
        {
            bool used = false;
            std::string text;
            int count = 0;     // passes since it started showing (+6)
        };

        std::array<Slot, SLOT_COUNT> slots{};
        int current = -1;      // the slot showing, -1 for none
    };
}
