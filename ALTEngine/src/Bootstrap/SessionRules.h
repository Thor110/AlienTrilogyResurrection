#pragma once

#include <cstdint>

namespace ALTEngine::Bootstrap
{
    // THE HOST'S RULES FOR A NETWORK GAME (deathmatch and co-operative).
    //
    // Some Modern and Testing options change how the game plays - how fast
    // the player and creatures move, whether a hit always lands, whether you
    // can jump or look up, how far you can see - and in a network game every
    // machine must play by the same ones, or one player runs at twice the
    // speed of the others. So the host's choices travel in the Welcome
    // (Net::Welcome::rules), and while a network game is on they replace
    // each player's own (ModernSettings::IsActive, TestingSettings::IsOn,
    // WeaponPacingSettings::Get). Each player's own settings are left as they
    // were and come back when the game ends.
    //
    // What each option does in a network game is ModernSettings::NetworkRuleFor.
    struct SessionRules
    {
        // Bit (1 << feature) for each Modern feature the host has on, by the
        // ModernFeature enum's value (new features go at its end, so the bits
        // keep their meaning; the protocol version keeps builds apart).
        uint32_t modernFeatures = 0;
        // The host's weapon pacing (WeaponPacingSettings), 1-8.
        uint8_t weaponPacing = 3;
    };

    // The rules in force: set while a network game is on, otherwise unset.
    struct ActiveSessionRules
    {
        bool active = false;
        SessionRules rules;

        static ActiveSessionRules& Get()
        {
            static ActiveSessionRules current;
            return current;
        }
    };

    // Puts a network game's rules in force for as long as it lives.
    class SessionRulesScope
    {
    public:
        explicit SessionRulesScope(const SessionRules& rules)
        {
            ActiveSessionRules::Get().active = true;
            ActiveSessionRules::Get().rules = rules;
        }
        ~SessionRulesScope() { ActiveSessionRules::Get().active = false; }
        SessionRulesScope(const SessionRulesScope&) = delete;
        SessionRulesScope& operator=(const SessionRulesScope&) = delete;
    };
}
