#pragma once

#include "Protocol.h"
#include "UdpSocket.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Net
{
    // What happened on the network since the last TakeEvents.
    struct Event
    {
        enum class Kind
        {
            Welcomed,       // we joined: our slot and the level are set
            Rejected,       // the host said no, or never answered
            PlayerJoined,   // `slot` came in
            PlayerLeft,     // `slot` went, or timed out
            Disconnected,   // we lost the host
            Shot,           // `shot` was fired by another player
            Hit,            // we were hit: `hit`
            Frag,           // `frag`: someone killed someone
            Chat,           // `chat`
            RosterChanged,  // names or frags changed
            BotHit,         // host: our bot in `hit.target` was hit by `hit.attacker`
        };
        Kind kind = Kind::RosterChanged;
        uint8_t slot = 0;
        Shot shot{};
        Hit hit{};
        Frag frag{};
        Chat chat{};
        RejectReason reason = RejectReason::Full;
    };

    // A game in progress: one host (slot 0) and up to seven more (the host's
    // player limit, bots included). The host
    // keeps the roster and the frag count and passes each player's state,
    // shots, hits and chat on to the others; a hit goes to the player hit,
    // who takes the damage and reports a frag if it killed them.
    class Session
    {
    public:
        enum class Role { None, Host, Client };

        // Long enough to sit in the pause menu (which stops the game loop, and
        // with it this session) without being dropped.
        static constexpr double TIMEOUT_SECONDS = 30.0;
        static constexpr double PING_SECONDS = 1.0;
        static constexpr double JOIN_RETRY_SECONDS = 0.5;

        ~Session() { Stop(); }

        // Hosting on `port` (DEFAULT_PORT unless testing); false if the port
        // can't be had.
        bool StartHost(const std::string& gameName, const std::string& level, const std::string& playerName,
                       uint16_t port = DEFAULT_PORT, Skin skin = Skin::Synthetic);
        // Asks `host` to be let in; Welcomed or Rejected follows from Update.
        bool StartJoin(const Address& host, const std::string& playerName, Skin skin = Skin::Synthetic);
        // Leaves (telling the others) and closes.
        void Stop();

        // Reads everything waiting and keeps the session alive. `now` in seconds.
        void Update(double now);

        void SendState(State state);
        void SendShot(Shot shot);
        void SendHit(uint8_t target, int16_t damage);
        void SendFrag(uint8_t killer, uint8_t victim);
        void SendChat(const std::string& text);

        // Host: how many may play, bots included (MIN_PLAYER_LIMIT-MAX_PLAYERS).
        void SetPlayerLimit(int limit);
        int PlayerLimit() const { return playerLimit; }
        // Host: the bots asked for (Game::Bots) and their skill; the game
        // keeps that many, less any place a player has taken.
        void SetBots(int count, int skillLevel) { botsWanted = count; botSkill = skillLevel; }
        int BotsWanted() const { return botsWanted; }
        int BotSkill() const { return botSkill; }
        // Host: a bot takes the first free place (-1 if none) or leaves.
        int AddBot(const std::string& name, Skin look);
        void RemoveBot(int slot);
        bool IsBot(int slot) const { return slot >= 0 && slot < MAX_PLAYERS && roster.players[static_cast<size_t>(slot)].used && roster.players[static_cast<size_t>(slot)].bot; }
        int UsedCount() const;
        int BotCount() const;
        // Host: a bot's state, shot, hit and death, sent as that player's.
        void SendBotState(int slot, State state);
        void SendBotShot(int slot, Shot shot);
        void SendBotHit(int attacker, uint8_t target, int16_t damage);

        // The F2-F9 messages (Multiplayer Options), sent from play by key.
        void SetQuickChat(const std::array<std::string, 8>& messages) { quickChat = messages; }
        const std::string& QuickChat(int index) const { return quickChat[static_cast<size_t>(index & 7)]; }

        std::vector<Event> TakeEvents() { std::vector<Event> out; out.swap(events); return out; }

        Role GetRole() const { return role; }
        bool Connected() const { return role == Role::Host || (role == Role::Client && welcomed); }
        int LocalSlot() const { return localSlot; }
        const std::string& Level() const { return level; }
        const std::string& GameName() const { return gameName; }
        const Roster& GetRoster() const { return roster; }
        // The latest state each other player sent; empty for us and for empty slots.
        const std::array<std::optional<State>, MAX_PLAYERS>& RemoteStates() const { return remote; }
        uint16_t Port() const { return socket.LocalPort(); }

    private:
        std::array<std::string, 8> quickChat{};
        int playerLimit = ORIGINAL_PLAYERS;
        int botsWanted = 0;
        int botSkill = 3;
        // Host: a hit for one of the bots becomes a BotHit event.
        void HitBot(uint8_t attacker, uint8_t target, int16_t damage);
        struct Peer { bool used = false; Address address; double lastHeard = 0.0; };

        void Send(const Address& to, const Message& message);
        void SendToHost(const Message& message) { Send(hostAddress, message); }
        // Host: to every client but `except`.
        void Broadcast(const Message& message, int except = -1);
        void HandleAsHost(const Address& from, const Message& message, double now);
        void HandleAsClient(const Message& message, double now);
        void DropPeer(int slot);
        void ApplyFrag(const Frag& frag);
        int SlotOf(const Address& from) const;

        UdpSocket socket;
        Role role = Role::None;
        std::string gameName, level, playerName;
        Skin skin = Skin::Synthetic;
        int localSlot = -1;
        bool welcomed = false;
        Address hostAddress;
        double lastHeardHost = 0.0, lastPing = 0.0, lastJoinTry = -1.0, joinStarted = -1.0;
        std::array<Peer, MAX_PLAYERS> peers{};
        Roster roster{};
        std::array<std::optional<State>, MAX_PLAYERS> remote{};
        std::vector<Event> events;
    };

    // "SEARCHING FOR NET GAMES": asks the local network (and this machine) for
    // games, once a second, and keeps what answered.
    class GameBrowser
    {
    public:
        struct Found { Address address; DiscoverReply info; double lastSeen = 0.0; };

        bool Start(uint16_t hostPort = DEFAULT_PORT);
        void Stop() { socket.Close(); games.clear(); }
        void Update(double now);
        const std::vector<Found>& Games() const { return games; }

    private:
        UdpSocket socket;
        uint16_t hostPort = DEFAULT_PORT;
        double lastQuery = -10.0;
        std::vector<Found> games;
    };
}
