#include "Session.h"

#include <algorithm>

namespace ALTEngine::Net
{
    bool Session::StartHost(const std::string& name, const std::string& levelDigits, const std::string& player, uint16_t port,
                            Skin chosen)
    {
        Stop();
        if (!socket.Open(port, true)) { return false; }
        role = Role::Host;
        gameName = name;
        level = levelDigits;
        playerName = player;
        localSlot = 0;
        welcomed = true;
        roster = {};
        skin = chosen;
        roster.players[0] = RosterEntry{ true, player.substr(0, MAX_NAME), 0, chosen };
        return true;
    }

    bool Session::StartJoin(const Address& host, const std::string& player, Skin chosen)
    {
        Stop();
        if (!socket.Open(0)) { return false; }
        role = Role::Client;
        skin = chosen;
        hostAddress = host;
        playerName = player;
        localSlot = -1;
        welcomed = false;
        lastJoinTry = -1.0;
        joinStarted = -1.0;
        return true;
    }

    void Session::Stop()
    {
        if (role == Role::Host) { Broadcast(Leave{ 0 }); }
        else if (role == Role::Client && welcomed) { SendToHost(Leave{ static_cast<uint8_t>(localSlot) }); }
        socket.Close();
        role = Role::None;
        welcomed = false;
        localSlot = -1;
        peers = {};
        remote = {};
        roster = {};
    }

    void Session::Send(const Address& to, const Message& message) { socket.SendTo(to, Encode(message)); }

    void Session::Broadcast(const Message& message, int except)
    {
        const auto bytes = Encode(message);
        for (int i = 1; i < MAX_PLAYERS; ++i)
        {
            if (peers[static_cast<size_t>(i)].used && i != except) { socket.SendTo(peers[static_cast<size_t>(i)].address, bytes); }
        }
    }

    int Session::SlotOf(const Address& from) const
    {
        for (int i = 1; i < MAX_PLAYERS; ++i)
        {
            if (peers[static_cast<size_t>(i)].used && peers[static_cast<size_t>(i)].address == from) { return i; }
        }
        return -1;
    }

    void Session::DropPeer(int slot)
    {
        if (slot <= 0 || slot >= MAX_PLAYERS || !peers[static_cast<size_t>(slot)].used) { return; }
        peers[static_cast<size_t>(slot)] = {};
        roster.players[static_cast<size_t>(slot)] = {};
        remote[static_cast<size_t>(slot)].reset();
        Broadcast(Leave{ static_cast<uint8_t>(slot) });
        Broadcast(roster);
        Event e; e.kind = Event::Kind::PlayerLeft; e.slot = static_cast<uint8_t>(slot);
        events.push_back(e);
        Event r; r.kind = Event::Kind::RosterChanged;
        events.push_back(r);
    }

    void Session::ApplyFrag(const Frag& frag)
    {
        // A kill scores the killer one; killing yourself costs one.
        if (frag.killer < MAX_PLAYERS)
        {
            auto& entry = roster.players[frag.killer];
            entry.frags = static_cast<int16_t>(entry.frags + (frag.killer == frag.victim ? -1 : 1));
        }
        Broadcast(frag);
        Broadcast(roster);
        Event e; e.kind = Event::Kind::Frag; e.frag = frag; events.push_back(e);
        Event r; r.kind = Event::Kind::RosterChanged; events.push_back(r);
    }

    void Session::Update(double now)
    {
        if (role == Role::None) { return; }
        Address from;
        while (auto bytes = socket.Receive(from))
        {
            const auto message = Decode(*bytes);
            if (!message) { continue; }
            if (role == Role::Host) { HandleAsHost(from, *message, now); }
            else if (from == hostAddress) { HandleAsClient(*message, now); }
        }

        if (role == Role::Host)
        {
            for (int i = 1; i < MAX_PLAYERS; ++i)
            {
                if (peers[static_cast<size_t>(i)].used && now - peers[static_cast<size_t>(i)].lastHeard > TIMEOUT_SECONDS) { DropPeer(i); }
            }
            if (now - lastPing >= PING_SECONDS) { lastPing = now; Broadcast(Ping{ 0 }); }
            return;
        }

        // Client.
        if (!welcomed)
        {
            if (joinStarted < 0.0) { joinStarted = now; }
            if (now - joinStarted > TIMEOUT_SECONDS)
            {
                Event e; e.kind = Event::Kind::Rejected; e.reason = RejectReason::Full;
                events.push_back(e);
                role = Role::None;
                socket.Close();
                return;
            }
            if (lastJoinTry < 0.0 || now - lastJoinTry >= JOIN_RETRY_SECONDS)
            {
                lastJoinTry = now;
                SendToHost(Join{ playerName.substr(0, MAX_NAME), skin });
            }
            return;
        }
        if (now - lastHeardHost > TIMEOUT_SECONDS)
        {
            Event e; e.kind = Event::Kind::Disconnected; events.push_back(e);
            role = Role::None;
            welcomed = false;
            socket.Close();
            return;
        }
        if (now - lastPing >= PING_SECONDS) { lastPing = now; SendToHost(Ping{ static_cast<uint8_t>(localSlot) }); }
    }

    void Session::HandleAsHost(const Address& from, const Message& message, double now)
    {
        if (std::holds_alternative<DiscoverQuery>(message))
        {
            uint8_t count = 0;
            for (const auto& p : roster.players) { count = static_cast<uint8_t>(count + (p.used ? 1 : 0)); }
            Send(from, DiscoverReply{ gameName, level, count, MAX_PLAYERS });
            return;
        }
        if (const auto* join = std::get_if<Join>(&message))
        {
            int slot = SlotOf(from);
            if (slot < 0)
            {
                for (int i = 1; i < MAX_PLAYERS && slot < 0; ++i) { if (!peers[static_cast<size_t>(i)].used) { slot = i; } }
                if (slot < 0) { Send(from, Reject{ RejectReason::Full }); return; }
                peers[static_cast<size_t>(slot)] = Peer{ true, from, now };
                roster.players[static_cast<size_t>(slot)] = RosterEntry{ true, join->name.substr(0, MAX_NAME), 0, join->skin };
                Event e; e.kind = Event::Kind::PlayerJoined; e.slot = static_cast<uint8_t>(slot); events.push_back(e);
                Event r; r.kind = Event::Kind::RosterChanged; events.push_back(r);
            }
            // A repeated Join (the Welcome was lost) is answered again.
            Send(from, Welcome{ static_cast<uint8_t>(slot), level });
            Broadcast(roster);
            return;
        }

        const int slot = SlotOf(from);
        if (slot < 0) { return; }   // not one of ours
        peers[static_cast<size_t>(slot)].lastHeard = now;
        const auto s = static_cast<uint8_t>(slot);

        if (const auto* state = std::get_if<State>(&message))
        {
            State fixed = *state;
            fixed.slot = s;
            remote[static_cast<size_t>(slot)] = fixed;
            Broadcast(fixed, slot);
        }
        else if (const auto* shot = std::get_if<Shot>(&message))
        {
            Shot fixed = *shot;
            fixed.slot = s;
            Broadcast(fixed, slot);
            Event e; e.kind = Event::Kind::Shot; e.slot = s; e.shot = fixed; events.push_back(e);
        }
        else if (const auto* hit = std::get_if<Hit>(&message))
        {
            Hit fixed = *hit;
            fixed.attacker = s;
            if (fixed.target == 0) { Event e; e.kind = Event::Kind::Hit; e.slot = s; e.hit = fixed; events.push_back(e); }
            else if (fixed.target < MAX_PLAYERS && peers[fixed.target].used) { Send(peers[fixed.target].address, fixed); }
        }
        else if (const auto* frag = std::get_if<Frag>(&message))
        {
            // Only the player killed reports their own death.
            Frag fixed = *frag;
            fixed.victim = s;
            ApplyFrag(fixed);
        }
        else if (const auto* chat = std::get_if<Chat>(&message))
        {
            Chat fixed = *chat;
            fixed.slot = s;
            Broadcast(fixed, slot);
            Event e; e.kind = Event::Kind::Chat; e.slot = s; e.chat = fixed; events.push_back(e);
        }
        else if (std::holds_alternative<Leave>(message))
        {
            DropPeer(slot);
        }
    }

    void Session::HandleAsClient(const Message& message, double now)
    {
        lastHeardHost = now;
        if (const auto* welcome = std::get_if<Welcome>(&message))
        {
            if (!welcomed)
            {
                welcomed = true;
                localSlot = welcome->slot;
                level = welcome->level;
                Event e; e.kind = Event::Kind::Welcomed; e.slot = welcome->slot; events.push_back(e);
            }
            return;
        }
        if (const auto* reject = std::get_if<Reject>(&message))
        {
            if (!welcomed)
            {
                Event e; e.kind = Event::Kind::Rejected; e.reason = reject->reason; events.push_back(e);
                role = Role::None;
                socket.Close();
            }
            return;
        }
        if (!welcomed) { return; }
        if (const auto* list = std::get_if<Roster>(&message))
        {
            for (int i = 0; i < MAX_PLAYERS; ++i)
            {
                const bool was = roster.players[static_cast<size_t>(i)].used;
                const bool is = list->players[static_cast<size_t>(i)].used;
                if (i != localSlot && is && !was) { Event e; e.kind = Event::Kind::PlayerJoined; e.slot = static_cast<uint8_t>(i); events.push_back(e); }
                if (i != localSlot && was && !is) { remote[static_cast<size_t>(i)].reset(); }
            }
            roster = *list;
            Event e; e.kind = Event::Kind::RosterChanged; events.push_back(e);
        }
        else if (const auto* state = std::get_if<State>(&message))
        {
            if (state->slot < MAX_PLAYERS && state->slot != localSlot) { remote[state->slot] = *state; }
        }
        else if (const auto* shot = std::get_if<Shot>(&message))
        {
            if (shot->slot != localSlot) { Event e; e.kind = Event::Kind::Shot; e.slot = shot->slot; e.shot = *shot; events.push_back(e); }
        }
        else if (const auto* hit = std::get_if<Hit>(&message))
        {
            if (hit->target == localSlot) { Event e; e.kind = Event::Kind::Hit; e.slot = hit->attacker; e.hit = *hit; events.push_back(e); }
        }
        else if (const auto* frag = std::get_if<Frag>(&message))
        {
            Event e; e.kind = Event::Kind::Frag; e.frag = *frag; events.push_back(e);
        }
        else if (const auto* chat = std::get_if<Chat>(&message))
        {
            Event e; e.kind = Event::Kind::Chat; e.slot = chat->slot; e.chat = *chat; events.push_back(e);
        }
        else if (const auto* leave = std::get_if<Leave>(&message))
        {
            if (leave->slot == 0)
            {
                Event e; e.kind = Event::Kind::Disconnected; events.push_back(e);
                role = Role::None;
                welcomed = false;
                socket.Close();
            }
            else if (leave->slot < MAX_PLAYERS)
            {
                remote[leave->slot].reset();
                Event e; e.kind = Event::Kind::PlayerLeft; e.slot = leave->slot; events.push_back(e);
            }
        }
    }

    void Session::SendState(State state)
    {
        if (!Connected()) { return; }
        state.slot = static_cast<uint8_t>(localSlot);
        if (role == Role::Host) { Broadcast(state); } else { SendToHost(state); }
    }

    void Session::SendShot(Shot shot)
    {
        if (!Connected()) { return; }
        shot.slot = static_cast<uint8_t>(localSlot);
        if (role == Role::Host) { Broadcast(shot); } else { SendToHost(shot); }
    }

    void Session::SendHit(uint8_t target, int16_t damage)
    {
        if (!Connected() || target >= MAX_PLAYERS || target == localSlot) { return; }
        const Hit hit{ static_cast<uint8_t>(localSlot), target, damage };
        if (role == Role::Client) { SendToHost(hit); return; }
        if (peers[target].used) { Send(peers[target].address, hit); }
    }

    void Session::SendFrag(uint8_t killer, uint8_t victim)
    {
        if (!Connected()) { return; }
        const Frag frag{ killer, victim };
        if (role == Role::Host) { ApplyFrag(frag); } else { SendToHost(frag); }
    }

    void Session::SendChat(const std::string& text)
    {
        if (!Connected()) { return; }
        const Chat chat{ static_cast<uint8_t>(localSlot), text.substr(0, MAX_CHAT) };
        if (role == Role::Host) { Broadcast(chat); } else { SendToHost(chat); }
    }

    bool GameBrowser::Start(uint16_t port)
    {
        hostPort = port;
        games.clear();
        lastQuery = -10.0;
        return socket.Open(0, true);
    }

    void GameBrowser::Update(double now)
    {
        if (!socket.IsOpen()) { return; }
        if (now - lastQuery >= 1.0)
        {
            lastQuery = now;
            const auto query = Encode(DiscoverQuery{});
            socket.SendTo(Address::Broadcast(hostPort), query);
            socket.SendTo(Address::Loopback(hostPort), query);   // a game on this machine
        }
        Address from;
        while (auto bytes = socket.Receive(from))
        {
            const auto message = Decode(*bytes);
            const auto* reply = message ? std::get_if<DiscoverReply>(&*message) : nullptr;
            if (!reply) { continue; }
            auto it = std::find_if(games.begin(), games.end(), [&](const Found& f) {
                return f.address == from || (f.info.gameName == reply->gameName && f.address.port == from.port
                                             && (f.address.ip == 0x7f000001u || from.ip == 0x7f000001u));
            });
            if (it == games.end()) { games.push_back(Found{ from, *reply, now }); }
            else { it->info = *reply; it->lastSeen = now; if (from.ip != 0x7f000001u) { it->address = from; } }
        }
        // Gone quiet for three queries: gone.
        games.erase(std::remove_if(games.begin(), games.end(), [&](const Found& f) { return now - f.lastSeen > 3.5; }),
                    games.end());
    }
}
