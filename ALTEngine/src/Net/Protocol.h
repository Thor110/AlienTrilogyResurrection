#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ALTEngine::Net
{
    // THE PORT'S MULTIPLAYER PROTOCOL. The original spoke IPX through its own
    // packet loop (FUN_00054a60: types 4-0x16, four player records of 0x3c at
    // 0x402b50, damage from another player delivered by type 8 and applied
    // through FUN_0003e5c4), which no current network carries; this is its
    // job done over UDP, with the same four players.
    //
    // Every datagram: "ALT" + a version byte, a type byte, then the fields,
    // little-endian. A datagram that doesn't start so, or whose fields run
    // short, is dropped.
    inline constexpr uint8_t PROTOCOL_VERSION = 1;
    inline constexpr int MAX_PLAYERS = 4;          // the original's four records
    inline constexpr uint16_t DEFAULT_PORT = 27960;
    inline constexpr size_t MAX_NAME = 16;
    inline constexpr size_t MAX_GAME_NAME = 24;
    inline constexpr size_t MAX_CHAT = 64;

    enum class MessageType : uint8_t
    {
        DiscoverQuery = 1,   // client -> broadcast: any games here?
        DiscoverReply = 2,   // host -> client: this one
        Join = 3,            // client -> host: let me in, as `name`
        Welcome = 4,         // host -> client: you are `slot`, the level is `level`
        Reject = 5,          // host -> client: no (full, started, wrong version)
        Roster = 6,          // host -> everyone: who is in which slot, and their frags
        State = 7,           // a player's own position and state, every tick
        Shot = 8,            // a player fired: what, from where, which way
        Hit = 9,             // `attacker` hit `target` for `damage` (the original's type 8)
        Frag = 10,           // `killer` killed `victim`
        Chat = 11,           // one of the F2-F9 messages, or typed
        Leave = 12,          // going
        Ping = 13,           // still here
    };

    struct DiscoverQuery {};
    struct DiscoverReply { std::string gameName; std::string level; uint8_t players = 0; uint8_t maxPlayers = MAX_PLAYERS; };
    struct Join { std::string name; };
    struct Welcome { uint8_t slot = 0; std::string level; };
    enum class RejectReason : uint8_t { Full = 1, Version = 2 };
    struct Reject { RejectReason reason = RejectReason::Full; };
    struct RosterEntry { bool used = false; std::string name; int16_t frags = 0; };
    struct Roster { std::array<RosterEntry, MAX_PLAYERS> players{}; };
    struct State
    {
        uint8_t slot = 0;
        uint32_t tick = 0;
        float x = 0, y = 0, z = 0;       // the player's feet, world units
        int16_t yaw = 0, pitch = 0;      // the original's 4096 to the turn
        int16_t health = 0;
        uint8_t weapon = 0;
        uint8_t flags = 0;               // bit 0 dead, bit 1 firing, bit 2 moving
    };
    struct Shot { uint8_t slot = 0; uint8_t type = 0; float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0; };
    struct Hit { uint8_t attacker = 0; uint8_t target = 0; int16_t damage = 0; };
    struct Frag { uint8_t killer = 0; uint8_t victim = 0; };
    struct Chat { uint8_t slot = 0; std::string text; };
    struct Leave { uint8_t slot = 0; };
    struct Ping { uint8_t slot = 0; };

    using Message = std::variant<DiscoverQuery, DiscoverReply, Join, Welcome, Reject, Roster, State, Shot, Hit,
                                 Frag, Chat, Leave, Ping>;

    std::vector<uint8_t> Encode(const Message& message);
    std::optional<Message> Decode(const std::vector<uint8_t>& bytes);
}
