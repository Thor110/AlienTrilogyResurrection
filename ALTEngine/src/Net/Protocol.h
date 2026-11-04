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
    inline constexpr uint8_t PROTOCOL_VERSION = 5;   // 4: game modes, co-operative level ends; 5: the shared world
    // The original's four records are its limit; the port allows a host to
    // take up to eight (MIN_PLAYER_LIMIT - MAX_PLAYERS, bots included).
    inline constexpr int MAX_PLAYERS = 8;
    inline constexpr int MIN_PLAYER_LIMIT = 4;
    inline constexpr int ORIGINAL_PLAYERS = 4;
    inline constexpr uint16_t DEFAULT_PORT = 27960;
    inline constexpr size_t MAX_NAME = 16;
    inline constexpr size_t MAX_GAME_NAME = 24;
    inline constexpr size_t MAX_CHAT = 64;
    // A shot carries one of the player's five rounds (Particles TYPE_PISTOL
    // 0 to TYPE_SMARTGUN 4) - nothing else (blood, vent jets) is accepted.
    inline constexpr uint8_t MAX_SHOT_TYPE = 4;
    // Positions and velocities beyond this are not from a level.
    inline constexpr float MAX_COORDINATE = 1.0e6f;

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
        AtExit = 14,         // client -> host: I reached the End Level trigger and am waiting
        LevelEnd = 15,       // host -> everyone: every player is at the exit, the level is over
        LevelStart = 16,     // host -> everyone: the level now being played, by number
        WorldEvent = 17,     // co-operative: something that changed the level (see WorldEvent)
        WorldAck = 18,       // co-operative: world events received, up to `seq`
        Creatures = 19,      // co-operative, host -> everyone: where the creatures are
        CreatureDamage = 20, // co-operative, client -> host: my round hit creature `index`
        CreatureHit = 21,    // co-operative, host -> client: a creature hurt you
        CreatureShot = 22,   // co-operative, host -> everyone: a creature fired
    };

    // THE PORT'S GAME MODES. The original's network game is a deathmatch on
    // the ten multiplayer maps (L900-L909). Co-operative is the port's own:
    // the players go through the story missions together. Each machine runs
    // its own creatures, doors and pickups; what the session shares is who
    // is where and when the level is over - it ends only once every player
    // has reached the exit (AtExit, LevelEnd), and then the host names the
    // next one (LevelStart).
    enum class GameMode : uint8_t { Deathmatch = 0, Coop = 1, Count };
    inline constexpr const char* GameModeName(GameMode mode)
    {
        return mode == GameMode::Coop ? "CO-OPERATIVE" : "DEATHMATCH";
    }

    struct DiscoverQuery {};
    struct DiscoverReply
    {
        std::string gameName; std::string level; uint8_t players = 0; uint8_t maxPlayers = MAX_PLAYERS;
        GameMode mode = GameMode::Deathmatch;
    };
    // How a player looks to the others: the creature whose sprites draw them.
    // The original draws every player as the Synthetic (type 0x0d: the four
    // 0x0d monster records of each multiplayer level, parked outside the map,
    // are the bodies of the other players, run by their own task, LAB_00035290,
    // which picks walk 0-7, hit 8/9, death 10/11 and firing 12-19 by view).
    // The Soldier (0x0c), the Guard (0x0b) and the Handler (0x0e) are the
    // port's own choices; they draw views 5-7 as 3-1 flipped.
    enum class Skin : uint8_t { Synthetic = 0, Soldier = 1, Guard = 2, Handler = 3, Count };
    inline constexpr int SkinCreatureType(Skin skin)
    {
        switch (skin)
        {
        case Skin::Soldier: return 0x0c;
        case Skin::Guard: return 0x0b;
        case Skin::Handler: return 0x0e;
        default: return 0x0d;
        }
    }
    inline constexpr const char* SkinName(Skin skin)
    {
        switch (skin)
        {
        case Skin::Soldier: return "SOLDIER";
        case Skin::Guard: return "GUARD";
        case Skin::Handler: return "HANDLER";
        default: return "SYNTHETIC";
        }
    }

    struct Join { std::string name; Skin skin = Skin::Synthetic; };
    // `levelSerial` counts the levels the game has been through, so a
    // LevelStart repeated, or arriving late, is told from a new one.
    struct Welcome { uint8_t slot = 0; std::string level; GameMode mode = GameMode::Deathmatch; uint16_t levelSerial = 0; };
    enum class RejectReason : uint8_t { Full = 1, Version = 2 };
    struct Reject { RejectReason reason = RejectReason::Full; };
    // `bot`: run by the host (Game::Bots), not a machine of its own.
    // `atExit`: co-operative, this player has reached the exit and is waiting.
    struct RosterEntry
    {
        bool used = false; std::string name; int16_t frags = 0; Skin skin = Skin::Synthetic; bool bot = false;
        bool atExit = false;
    };
    struct Roster { std::array<RosterEntry, MAX_PLAYERS> players{}; };
    struct State
    {
        uint8_t slot = 0;
        uint32_t tick = 0;
        float x = 0, y = 0, z = 0;       // the player's feet, world units
        int16_t yaw = 0, pitch = 0;      // the original's 4096 to the turn
        int16_t health = 0;
        uint8_t weapon = 0;
        uint8_t flags = 0;               // bit 0 dead, bit 1 firing, bit 2 moving, bit 3 at the exit (not drawn)
    };
    struct Shot { uint8_t slot = 0; uint8_t type = 0; float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0; };
    struct Hit { uint8_t attacker = 0; uint8_t target = 0; int16_t damage = 0; };
    struct Frag { uint8_t killer = 0; uint8_t victim = 0; };
    struct Chat { uint8_t slot = 0; std::string text; };
    struct Leave { uint8_t slot = 0; };
    struct Ping { uint8_t slot = 0; };
    struct AtExit { uint8_t slot = 0; };
    // Both of these are the host's standing word, sent again every second,
    // so a lost datagram costs a second rather than a stuck game.
    struct LevelEnd { uint16_t levelSerial = 0; };
    // An empty level: the campaign is over.
    struct LevelStart { uint16_t levelSerial = 0; std::string level; };

    // ---- THE SHARED WORLD (co-operative) -----------------------------------
    //
    // The host runs the creatures; every machine runs the level - doors,
    // lifts, lights, pickups, objects - and keeps it the same as the others'
    // by passing on what changed it, as WorldEvents. Those must all arrive,
    // in order: each carries a sequence number and is sent again until it is
    // acknowledged (WorldAck). A guest's own go to the host first, which
    // numbers them into the level's one list and passes them on; a player
    // joining late is sent the whole list and so catches up.
    enum class WorldKind : uint8_t
    {
        Trigger = 1,         // a trigger's chain ran: a = trigger (0: a bare chain), b = its first command, flags below
        PickupTaken = 2,     // a = pickup record: someone picked it up
        ObjectDestroyed = 3, // a = damage target: it was destroyed
        PickupDropped = 4,   // host: a = pickup record, b = cell; at x,y,z flying at vx,vy,vz (a creature's drop)
        Count,
    };
    // Trigger flags: the source (Game::Triggers::Source) in the low three
    // bits, then fresh and the use key.
    inline constexpr uint8_t WORLD_TRIGGER_FRESH = 0x08;
    inline constexpr uint8_t WORLD_TRIGGER_USE_KEY = 0x10;
    struct WorldEvent
    {
        uint16_t levelSerial = 0;
        uint32_t seq = 0;        // the sender's (guest -> host) or the level's (host -> guest)
        uint8_t origin = 0;      // the player whose machine it happened on
        WorldKind kind = WorldKind::Trigger;
        int16_t a = 0, b = 0;
        uint8_t flags = 0;
        float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0;
    };
    struct WorldAck { uint16_t levelSerial = 0; uint32_t seq = 0; };

    // One creature as the host has it, for the others to draw. Sent many
    // times a second and never repeated, so a lost one is just replaced.
    inline constexpr uint16_t CREATURE_ACTIVE = 0x01;
    inline constexpr uint16_t CREATURE_ALIVE = 0x02;
    inline constexpr uint16_t CREATURE_BODY_REMOVED = 0x04;
    inline constexpr uint16_t CREATURE_ON_CEILING = 0x08;
    inline constexpr uint16_t CREATURE_MOVING = 0x10;
    inline constexpr uint16_t CREATURE_POUNCING = 0x20;
    // Walking: each machine picks the walk's view from where it sees the
    // creature (Enemies::PlayWalkByView), so only the fact is sent.
    inline constexpr uint16_t CREATURE_WALKING = 0x40;
    inline constexpr uint16_t CREATURE_MIRRORED = 0x80;
    inline constexpr uint16_t CREATURE_HOLD = 0x100;   // holds its last frame
    struct CreatureEntry
    {
        uint16_t index = 0;
        uint16_t flags = 0;
        float x = 0, y = 0, z = 0;
        uint16_t facing = 0;
        int16_t animation = -1;
        uint8_t frame = 0;
        uint8_t state = 0;
        int16_t health = 0;
        // The sound its animation cued since the last message, or 0xff.
        uint8_t sound = 0xff;
    };
    // Up to this many a datagram, so one stays well inside a network packet.
    inline constexpr size_t MAX_CREATURES_PER_MESSAGE = 48;
    struct Creatures { uint16_t levelSerial = 0; std::vector<CreatureEntry> entries; };
    struct CreatureDamage { uint16_t levelSerial = 0; uint8_t slot = 0; uint16_t index = 0; int16_t damage = 0; };
    // `knock`: a directed hit that throws the player along `yaw`.
    struct CreatureHit { uint8_t target = 0; int16_t damage = 0; uint8_t cause = 0; uint8_t knock = 0; int16_t yaw = 0; };
    // kind 0 a gunner's round, 1 a handler's grenade.
    struct CreatureShot { uint16_t levelSerial = 0; uint8_t kind = 0; float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0; };

    using Message = std::variant<DiscoverQuery, DiscoverReply, Join, Welcome, Reject, Roster, State, Shot, Hit,
                                 Frag, Chat, Leave, Ping, AtExit, LevelEnd, LevelStart,
                                 WorldEvent, WorldAck, Creatures, CreatureDamage, CreatureHit, CreatureShot>;

    std::vector<uint8_t> Encode(const Message& message);
    std::optional<Message> Decode(const std::vector<uint8_t>& bytes);
}
