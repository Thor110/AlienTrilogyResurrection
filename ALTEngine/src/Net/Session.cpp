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
        playerLimit = ORIGINAL_PLAYERS;
        mode = GameMode::Deathmatch;
        levelSerial = 0;
        levelEnded = false;
        atExitWanted = false;
        botsWanted = 0;
        botSkill = 3;
        localSlot = 0;
        welcomed = true;
        roster = {};
        skin = chosen;
        roster.players[0] = RosterEntry{ true, player.substr(0, MAX_NAME), 0, chosen };
        ClearWorld();
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
        mode = GameMode::Deathmatch;
        levelSerial = 0;
        levelEnded = false;
        atExitWanted = false;
        lastAtExitTry = -1.0;
        lastJoinTry = -1.0;
        ClearWorld();
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
        ClearWorld();
    }

    void Session::ClearWorld()
    {
        worldLog.clear();
        worldApplied = 0;
        outSeq = 0;
        outbox.clear();
        lastWorldResend = -1.0;
        for (auto& p : peers) { p.worldAcked = 0; p.worldApplied = 0; }
    }

    void Session::SendWorldEvent(WorldEvent event)
    {
        if (!Connected() || !Coop()) { return; }
        event.levelSerial = levelSerial;
        event.origin = static_cast<uint8_t>(localSlot);
        if (role == Role::Host)
        {
            event.seq = static_cast<uint32_t>(worldLog.size() + 1);
            worldLog.push_back(event);
            Broadcast(event);
            return;
        }
        event.seq = ++outSeq;
        outbox.push_back(event);
        SendToHost(event);
    }

    void Session::ResendWorld(double now)
    {
        if (lastWorldResend >= 0.0 && now - lastWorldResend < WORLD_RESEND_SECONDS) { return; }
        lastWorldResend = now;
        if (role == Role::Host)
        {
            for (int i = 1; i < MAX_PLAYERS; ++i)
            {
                const Peer& peer = peers[static_cast<size_t>(i)];
                if (!peer.used) { continue; }
                const size_t from = peer.worldAcked;
                const size_t to = std::min(worldLog.size(), from + WORLD_RESEND_BATCH);
                for (size_t k = from; k < to; ++k) { Send(peer.address, worldLog[k]); }
            }
            return;
        }
        size_t sent = 0;
        for (const auto& e : outbox)
        {
            if (sent++ >= WORLD_RESEND_BATCH) { break; }
            SendToHost(e);
        }
    }

    void Session::SendCreatures(const std::vector<CreatureEntry>& entries)
    {
        if (role != Role::Host || !Coop()) { return; }
        // An empty list still goes: it says the host is in the level.
        if (entries.empty()) { Broadcast(Creatures{ levelSerial, {} }); return; }
        for (size_t i = 0; i < entries.size(); i += MAX_CREATURES_PER_MESSAGE)
        {
            Creatures m;
            m.levelSerial = levelSerial;
            const size_t end = std::min(entries.size(), i + MAX_CREATURES_PER_MESSAGE);
            m.entries.assign(entries.begin() + static_cast<std::ptrdiff_t>(i), entries.begin() + static_cast<std::ptrdiff_t>(end));
            Broadcast(m);
        }
    }

    void Session::SendCreatureDamage(uint16_t index, int16_t damage)
    {
        if (role != Role::Client || !welcomed || !Coop()) { return; }
        SendToHost(CreatureDamage{ levelSerial, static_cast<uint8_t>(localSlot), index, damage });
    }

    void Session::SendCreatureHit(uint8_t target, int16_t damage, uint8_t cause, bool knock, int16_t yaw)
    {
        if (role != Role::Host || !Coop() || target == 0 || target >= MAX_PLAYERS || !peers[target].used) { return; }
        Send(peers[target].address, CreatureHit{ target, damage, cause, static_cast<uint8_t>(knock ? 1 : 0), yaw });
    }

    void Session::SendThrow(Throw thrown)
    {
        if (!Connected()) { return; }
        thrown.slot = static_cast<uint8_t>(localSlot);
        if (role == Role::Host) { Broadcast(thrown); } else { SendToHost(thrown); }
    }

    void Session::DropStaleEvents()
    {
        events.erase(std::remove_if(events.begin(), events.end(), [&](const Event& e) {
            return e.levelSerial != levelSerial || e.kind == Event::Kind::LevelEnded || e.kind == Event::Kind::LevelStarted;
        }), events.end());
    }

    void Session::SendCreatureShot(CreatureShot shot)
    {
        if (role != Role::Host || !Coop()) { return; }
        shot.levelSerial = levelSerial;
        Broadcast(shot);
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
        Push(e);
        Event r; r.kind = Event::Kind::RosterChanged;
        Push(r);
        CheckLevelEnd();   // no longer waited for
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
        Event e; e.kind = Event::Kind::Frag; e.frag = frag; Push(e);
        Event r; r.kind = Event::Kind::RosterChanged; Push(r);
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
            if (Coop()) { ResendWorld(now); }
            if (now - lastPing >= PING_SECONDS)
            {
                lastPing = now;
                Broadcast(Ping{ 0 });
                // The co-operative word, again: a guest who missed it catches up.
                if (Coop())
                {
                    Broadcast(LevelStart{ levelSerial, level });
                    if (levelEnded) { Broadcast(LevelEnd{ levelSerial }); }
                }
            }
            return;
        }

        // Client.
        if (!welcomed)
        {
            if (joinStarted < 0.0) { joinStarted = now; }
            if (now - joinStarted > TIMEOUT_SECONDS)
            {
                Event e; e.kind = Event::Kind::Rejected; e.reason = RejectReason::Full;
                Push(e);
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
            Event e; e.kind = Event::Kind::Disconnected; Push(e);
            role = Role::None;
            welcomed = false;
            socket.Close();
            return;
        }
        if (now - lastPing >= PING_SECONDS) { lastPing = now; SendToHost(Ping{ static_cast<uint8_t>(localSlot) }); }
        if (Coop()) { ResendWorld(now); }
        // Said again until the host's roster shows it.
        if (atExitWanted && !levelEnded && localSlot >= 0
            && !roster.players[static_cast<size_t>(localSlot)].atExit
            && (lastAtExitTry < 0.0 || now - lastAtExitTry >= JOIN_RETRY_SECONDS))
        {
            lastAtExitTry = now;
            SendToHost(AtExit{ static_cast<uint8_t>(localSlot) });
        }
    }

    void Session::HandleAsHost(const Address& from, const Message& message, double now)
    {
        if (std::holds_alternative<DiscoverQuery>(message))
        {
            // Players only: a joining player takes a bot's place, so a game
            // whose places are all bots is not shown as full.
            uint8_t count = 0;
            for (const auto& p : roster.players) { count = static_cast<uint8_t>(count + (p.used && !p.bot ? 1 : 0)); }
            Send(from, DiscoverReply{ gameName, level, count, static_cast<uint8_t>(playerLimit), mode });
            return;
        }
        if (const auto* join = std::get_if<Join>(&message))
        {
            int slot = SlotOf(from);
            if (slot < 0)
            {
                // Full: a bot gives up its place to a player.
                if (UsedCount() >= playerLimit)
                {
                    for (int i = MAX_PLAYERS - 1; i > 0; --i) { if (IsBot(i)) { RemoveBot(i); break; } }
                }
                if (UsedCount() < playerLimit)
                {
                    for (int i = 1; i < MAX_PLAYERS && slot < 0; ++i) { if (!roster.players[static_cast<size_t>(i)].used) { slot = i; } }
                }
                if (slot < 0) { Send(from, Reject{ RejectReason::Full }); return; }
                peers[static_cast<size_t>(slot)] = Peer{ true, from, now };
                roster.players[static_cast<size_t>(slot)] = RosterEntry{ true, join->name.substr(0, MAX_NAME), 0, join->skin };
                Event e; e.kind = Event::Kind::PlayerJoined; e.slot = static_cast<uint8_t>(slot); Push(e);
                Event r; r.kind = Event::Kind::RosterChanged; Push(r);
            }
            // A repeated Join (the Welcome was lost) is answered again.
            Send(from, Welcome{ static_cast<uint8_t>(slot), level, mode, levelSerial, rules });
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
        else if (const auto* thrown = std::get_if<Throw>(&message))
        {
            Throw fixed = *thrown;
            fixed.slot = s;
            Broadcast(fixed, slot);
            Event e; e.kind = Event::Kind::Throw; e.slot = s; e.thrown = fixed; Push(e);
        }
        else if (const auto* shot = std::get_if<Shot>(&message))
        {
            Shot fixed = *shot;
            fixed.slot = s;
            Broadcast(fixed, slot);
            Event e; e.kind = Event::Kind::Shot; e.slot = s; e.shot = fixed; Push(e);
        }
        else if (const auto* hit = std::get_if<Hit>(&message))
        {
            Hit fixed = *hit;
            fixed.attacker = s;
            if (fixed.target == 0) { Event e; e.kind = Event::Kind::Hit; e.slot = s; e.hit = fixed; Push(e); }
            else if (IsBot(fixed.target)) { HitBot(fixed.attacker, fixed.target, fixed.damage); }
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
            Event e; e.kind = Event::Kind::Chat; e.slot = s; e.chat = fixed; Push(e);
        }
        else if (std::holds_alternative<Leave>(message))
        {
            DropPeer(slot);
        }
        else if (const auto* world = std::get_if<WorldEvent>(&message))
        {
            if (!Coop() || world->levelSerial != levelSerial) { return; }
            Peer& peer = peers[static_cast<size_t>(slot)];
            if (world->seq == peer.worldApplied + 1)
            {
                peer.worldApplied = world->seq;
                WorldEvent entry = *world;
                entry.origin = s;
                entry.seq = static_cast<uint32_t>(worldLog.size() + 1);
                worldLog.push_back(entry);
                Broadcast(entry);
                Event e; e.kind = Event::Kind::World; e.slot = s; e.world = entry; Push(e);
            }
            Send(from, WorldAck{ levelSerial, peer.worldApplied });
        }
        else if (const auto* ack = std::get_if<WorldAck>(&message))
        {
            if (ack->levelSerial != levelSerial) { return; }
            Peer& peer = peers[static_cast<size_t>(slot)];
            peer.worldAcked = std::max(peer.worldAcked, std::min(ack->seq, static_cast<uint32_t>(worldLog.size())));
        }
        else if (const auto* damage = std::get_if<CreatureDamage>(&message))
        {
            if (!Coop() || damage->levelSerial != levelSerial) { return; }
            Event e; e.kind = Event::Kind::CreatureDamage; e.slot = s; e.creatureDamage = *damage;
            e.creatureDamage.slot = s;
            Push(e);
        }
        else if (std::holds_alternative<AtExit>(message))
        {
            if (Coop() && !levelEnded && !roster.players[static_cast<size_t>(slot)].atExit)
            {
                roster.players[static_cast<size_t>(slot)].atExit = true;
                Broadcast(roster);
                Event r; r.kind = Event::Kind::RosterChanged; Push(r);
                CheckLevelEnd();
            }
            else if (roster.players[static_cast<size_t>(slot)].atExit)
            {
                Broadcast(roster);   // their copy did not show it yet
            }
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
                mode = welcome->mode;
                levelSerial = welcome->levelSerial;
                rules = welcome->rules;
                Event e; e.kind = Event::Kind::Welcomed; e.slot = welcome->slot; Push(e);
            }
            return;
        }
        if (const auto* reject = std::get_if<Reject>(&message))
        {
            if (!welcomed)
            {
                Event e; e.kind = Event::Kind::Rejected; e.reason = reject->reason; Push(e);
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
                if (i != localSlot && is && !was) { Event e; e.kind = Event::Kind::PlayerJoined; e.slot = static_cast<uint8_t>(i); Push(e); }
                if (i != localSlot && was && !is) { remote[static_cast<size_t>(i)].reset(); }
            }
            roster = *list;
            Event e; e.kind = Event::Kind::RosterChanged; Push(e);
        }
        else if (const auto* state = std::get_if<State>(&message))
        {
            if (state->slot < MAX_PLAYERS && state->slot != localSlot) { remote[state->slot] = *state; }
        }
        else if (const auto* shot = std::get_if<Shot>(&message))
        {
            if (shot->slot != localSlot) { Event e; e.kind = Event::Kind::Shot; e.slot = shot->slot; e.shot = *shot; Push(e); }
        }
        else if (const auto* hit = std::get_if<Hit>(&message))
        {
            if (hit->target == localSlot) { Event e; e.kind = Event::Kind::Hit; e.slot = hit->attacker; e.hit = *hit; Push(e); }
        }
        else if (const auto* frag = std::get_if<Frag>(&message))
        {
            Event e; e.kind = Event::Kind::Frag; e.frag = *frag; Push(e);
        }
        else if (const auto* chat = std::get_if<Chat>(&message))
        {
            Event e; e.kind = Event::Kind::Chat; e.slot = chat->slot; e.chat = *chat; Push(e);
        }
        else if (const auto* world = std::get_if<WorldEvent>(&message))
        {
            if (world->levelSerial != levelSerial) { return; }
            if (world->seq == worldApplied + 1)
            {
                worldApplied = world->seq;
                // Our own come back numbered too; they are already done here.
                if (world->origin != localSlot) { Event e; e.kind = Event::Kind::World; e.slot = world->origin; e.world = *world; Push(e); }
            }
            SendToHost(WorldAck{ levelSerial, worldApplied });
        }
        else if (const auto* ack = std::get_if<WorldAck>(&message))
        {
            if (ack->levelSerial != levelSerial) { return; }
            outbox.erase(std::remove_if(outbox.begin(), outbox.end(), [&](const WorldEvent& e) { return e.seq <= ack->seq; }),
                         outbox.end());
        }
        else if (const auto* thrown = std::get_if<Throw>(&message))
        {
            if (thrown->slot != localSlot) { Event e; e.kind = Event::Kind::Throw; e.slot = thrown->slot; e.thrown = *thrown; Push(e); }
        }
        else if (const auto* creatures = std::get_if<Creatures>(&message))
        {
            if (creatures->levelSerial != levelSerial) { return; }
            Event e; e.kind = Event::Kind::Creatures; e.creatures = *creatures; Push(e);
        }
        else if (const auto* creatureHit = std::get_if<CreatureHit>(&message))
        {
            if (creatureHit->target != localSlot) { return; }
            Event e; e.kind = Event::Kind::CreatureHit; e.creatureHit = *creatureHit; Push(e);
        }
        else if (const auto* creatureShot = std::get_if<CreatureShot>(&message))
        {
            if (creatureShot->levelSerial != levelSerial) { return; }
            Event e; e.kind = Event::Kind::CreatureShot; e.creatureShot = *creatureShot; Push(e);
        }
        else if (const auto* end = std::get_if<LevelEnd>(&message))
        {
            if (end->levelSerial == levelSerial && !levelEnded)
            {
                levelEnded = true;
                Event e; e.kind = Event::Kind::LevelEnded; Push(e);
            }
        }
        else if (const auto* start = std::get_if<LevelStart>(&message))
        {
            // Newer only (the serial wraps after 65535 levels, which no game reaches).
            if (static_cast<int16_t>(start->levelSerial - levelSerial) > 0) { NewLevel(start->level, start->levelSerial); }
        }
        else if (const auto* leave = std::get_if<Leave>(&message))
        {
            if (leave->slot == 0)
            {
                Event e; e.kind = Event::Kind::Disconnected; Push(e);
                role = Role::None;
                welcomed = false;
                socket.Close();
            }
            else if (leave->slot < MAX_PLAYERS)
            {
                remote[leave->slot].reset();
                Event e; e.kind = Event::Kind::PlayerLeft; e.slot = leave->slot; Push(e);
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
        if (IsBot(target)) { HitBot(hit.attacker, target, damage); return; }
        if (peers[target].used) { Send(peers[target].address, hit); }
    }

    void Session::ReportAtExit()
    {
        if (!Connected() || !Coop() || atExitWanted) { return; }
        atExitWanted = true;
        if (role == Role::Host)
        {
            roster.players[0].atExit = true;
            Broadcast(roster);
            Event r; r.kind = Event::Kind::RosterChanged; Push(r);
            CheckLevelEnd();
        }
        else
        {
            lastAtExitTry = -1.0;   // at once, then again until the roster shows it
        }
    }

    int Session::PlayersPlaying() const
    {
        int n = 0;
        for (const auto& p : roster.players) { n += (p.used && !p.bot) ? 1 : 0; }
        return n;
    }

    int Session::PlayersAtExit() const
    {
        int n = 0;
        for (const auto& p : roster.players) { n += (p.used && !p.bot && p.atExit) ? 1 : 0; }
        return n;
    }

    void Session::CheckLevelEnd()
    {
        if (role != Role::Host || !Coop() || levelEnded) { return; }
        const int playing = PlayersPlaying();
        if (playing == 0 || PlayersAtExit() < playing) { return; }
        levelEnded = true;
        Broadcast(LevelEnd{ levelSerial });
        Event e; e.kind = Event::Kind::LevelEnded; Push(e);
    }

    void Session::NewLevel(const std::string& levelDigits, uint16_t serial)
    {
        level = levelDigits;
        levelSerial = serial;
        levelEnded = false;
        atExitWanted = false;
        lastAtExitTry = -1.0;
        for (auto& p : roster.players) { p.atExit = false; }
        // What the players did on the last level is no guide to where they are.
        remote = {};
        ClearWorld();
        Event e; e.kind = Event::Kind::LevelStarted; Push(e);
    }

    void Session::AdvanceLevel(const std::string& levelDigits)
    {
        if (role != Role::Host || !Coop()) { return; }
        NewLevel(levelDigits, static_cast<uint16_t>(levelSerial + 1));
        Broadcast(LevelStart{ levelSerial, level });
        Broadcast(roster);
    }

    void Session::SetPlayerLimit(int limit)
    {
        playerLimit = std::clamp(limit, MIN_PLAYER_LIMIT, MAX_PLAYERS);
    }

    int Session::UsedCount() const
    {
        int n = 0;
        for (const auto& p : roster.players) { n += p.used ? 1 : 0; }
        return n;
    }

    int Session::BotCount() const
    {
        int n = 0;
        for (const auto& p : roster.players) { n += (p.used && p.bot) ? 1 : 0; }
        return n;
    }

    int Session::AddBot(const std::string& name, Skin look)
    {
        if (role != Role::Host || UsedCount() >= playerLimit) { return -1; }
        for (int i = 1; i < MAX_PLAYERS; ++i)
        {
            auto& entry = roster.players[static_cast<size_t>(i)];
            if (entry.used) { continue; }
            entry = RosterEntry{ true, name.substr(0, MAX_NAME), 0, look, true };
            Broadcast(roster);
            Event e; e.kind = Event::Kind::PlayerJoined; e.slot = static_cast<uint8_t>(i); Push(e);
            Event r; r.kind = Event::Kind::RosterChanged; Push(r);
            return i;
        }
        return -1;
    }

    void Session::RemoveBot(int slot)
    {
        if (role != Role::Host || !IsBot(slot)) { return; }
        roster.players[static_cast<size_t>(slot)] = {};
        remote[static_cast<size_t>(slot)].reset();
        Broadcast(Leave{ static_cast<uint8_t>(slot) });
        Broadcast(roster);
        Event e; e.kind = Event::Kind::PlayerLeft; e.slot = static_cast<uint8_t>(slot); Push(e);
        Event r; r.kind = Event::Kind::RosterChanged; Push(r);
    }

    void Session::SendBotState(int slot, State state)
    {
        if (role != Role::Host || !IsBot(slot)) { return; }
        state.slot = static_cast<uint8_t>(slot);
        remote[static_cast<size_t>(slot)] = state;
        Broadcast(state);
    }

    void Session::SendBotShot(int slot, Shot shot)
    {
        if (role != Role::Host || !IsBot(slot)) { return; }
        shot.slot = static_cast<uint8_t>(slot);
        Broadcast(shot);
    }

    void Session::SendBotHit(int attacker, uint8_t target, int16_t damage)
    {
        if (role != Role::Host || !IsBot(attacker) || target >= MAX_PLAYERS || static_cast<int>(target) == attacker) { return; }
        const Hit hit{ static_cast<uint8_t>(attacker), target, damage };
        if (target == 0) { Event e; e.kind = Event::Kind::Hit; e.slot = hit.attacker; e.hit = hit; Push(e); }
        else if (IsBot(target)) { HitBot(hit.attacker, target, damage); }
        else if (peers[target].used) { Send(peers[target].address, hit); }
    }

    void Session::HitBot(uint8_t attacker, uint8_t target, int16_t damage)
    {
        Event e; e.kind = Event::Kind::BotHit; e.slot = attacker; e.hit = Hit{ attacker, target, damage };
        Push(e);
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
