// The session table, tested without a socket.
//
// protocol_test already walks the handshake through ServerSessions: a hello is
// admitted, a second one closes, the player limit counts players. This is the
// rest of what core/net/session.cpp decides on its own -- what happens after a
// player is in. A peer leaving, a peer going quiet, a body losing health and
// coming back at the spawn, the world's clock, whose ground a cell is, and the
// openings a mod puts up and takes down. Each is state the server keeps per
// session and gets wrong silently: a seat never freed, a bar never resent, a
// death nobody is moved for.
//
// `now` is a parameter everywhere, so every timeout here is arithmetic rather
// than a sleep.

#include "core/net/protocol.h"
#include "core/net/session.h"
#include "core/world/daylight.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace cnp;
using namespace cnp::net;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

constexpr uint64_t kTableHash = 0x00c0ffee00c0ffeeull;

ServerConfig make_server()
{
    ServerConfig c;
    c.engine_version = "0.3.0-m3";
    c.content_hash   = kTableHash;
    c.features       = {"zstd-chunks", "portals"};
    c.seed           = 1;
    return c;
}

Hello make_hello(const std::string &name, Scope scope = Scope::Enter)
{
    Hello h;
    h.player_name    = name;
    h.engine_version = "0.3.0-m3";
    h.content_hash   = kTableHash;
    h.features       = {"portals", "zstd-chunks"};
    h.scope          = scope;
    return h;
}

// Connects `peer` at `now` and sends its hello a hundredth of a second later,
// which is a loopback handshake: join_patience gives it the floor.
SessionAction join(ServerSessions &s, PeerId peer, const std::string &name, double now,
                   Scope scope = Scope::Enter)
{
    s.on_connect(peer, now);
    const auto buf = make_hello(name, scope).encode();
    return s.on_message(peer, (uint8_t)Channel::Handshake, buf.data(), buf.size(), now + 0.01);
}

bool contains(const std::vector<PeerId> &v, PeerId p)
{
    for (PeerId q : v)
        if (q == p) return true;
    return false;
}

Portal make_portal(PortalAxis axis, Vec3 centre)
{
    Portal p;
    p.axis        = axis;
    p.normal_sign = 1;
    p.center      = centre;
    return p;
}

} // namespace

int main()
{
    const SpawnPoint spawn{8.0f, 40.0f, -3.0f};

    std::printf("a peer that leaves\n");
    {
        ServerSessions s(make_server(), spawn);
        check(!join(s, 1, "alice", 0.0).close, "alice is admitted");
        check(s.ready_count() == 1 && s.find(1) != nullptr, "and is in the table");

        s.on_disconnect(1);
        check(s.find(1) == nullptr && s.ready_count() == 0 && s.size() == 0,
              "a disconnect takes the session out entirely");

        const SessionAction late = s.on_message(1, (uint8_t)Channel::Entity, "\x01", 1, 1.0);
        check(late.close, "a message from a peer that has gone is answered by closing");

        // The name was alice's while she was here. Once she has gone it is
        // free, or a dropped connection locks a player out of their own name.
        check(!join(s, 2, "alice", 2.0).close, "and the name is free for the next connection");

        s.on_disconnect(99);
        check(s.size() == 1, "disconnecting a peer nobody knows changes nothing");
    }

    std::printf("\nplayers and observers\n");
    {
        ServerSessions s(make_server(), spawn);
        join(s, 1, "alice", 0.0);
        join(s, 2, "bob", 0.0, Scope::Observe);
        s.on_connect(3, 0.0);   // still mid-handshake

        check(s.player_count() == 1, "one player");
        check(s.observer_count() == 1, "one observer, counted apart");
        check(s.ready_count() == 2, "both of them are admitted");
        check(s.size() == 3, "and the pending peer holds a slot too");

        const std::vector<PeerId> ready = s.ready_peers();
        check(ready.size() == 2 && ready[0] == 1 && ready[1] == 2,
              "ready_peers lists the admitted ones in ascending order");

        // An observer has no body, so it has nothing to hurt or heal: 0 is
        // "nothing happened", not "it died".
        check(s.hurt(2, 5, 1.0) == 0 && s.health_of(2) == 0, "an observer cannot be hurt");
        check(!s.set_max_health(2, 40), "nor given a maximum");

        // But a well formed message all the same.
        Vitals     v;
        const auto vb = s.vitals_message(2);
        ByteReader r(vb);
        check(Vitals::decode(r, v) && v.health == 0 && v.max == ecs::kFullHealth,
              "an observer's bar decodes, and says none of it");
    }

    std::printf("\nwho may come in\n");
    {
        ServerSessions s(make_server(), spawn);
        s.set_admit_check([](const std::string &name, const uint8_t *, PeerId, RefuseReason &reason,
                             std::string &why) {
            if (name != "mallory") return true;
            reason = RefuseReason::Banned;
            why    = "banned for griefing";
            return false;
        });

        check(!join(s, 1, "alice", 0.0).close, "a player the check allows is admitted");

        const SessionAction no = join(s, 2, "mallory", 0.0);
        Refuse     ref;
        ByteReader r(no.reply);
        check(no.close && Refuse::decode(r, ref) && ref.reason == RefuseReason::Banned &&
                  ref.detail == "banned for griefing",
              "one it refuses is closed, with the reason and the sentence it gave");
        check(s.find(2) != nullptr && s.find(2)->state == SessionState::Pending,
              "and is never made a player");

        ServerSessions n(make_server(), spawn);
        n.set_name_check([](const std::string &name, const uint8_t *identity) {
            return !(name == "giacomo" && identity == nullptr);
        });
        check(join(n, 1, "giacomo", 0.0).close,
              "a name this world keeps for an identity is refused to an anonymous join");
        check(!join(n, 2, "guest", 0.0).close, "while any other name is fine");

        ServerConfig tickets   = make_server();
        tickets.require_ticket = true;
        ServerSessions t(tickets, spawn);
        check(join(t, 1, "watcher", 0.0, Scope::Observe).close,
              "a world that requires tickets refuses a window with none");
        check(!join(t, 2, "walker", 0.0).close,
              "but a player typing its address is not coming through a portal");
    }

    std::printf("\nsilence\n");
    {
        ServerSessions s(make_server(), spawn);
        join(s, 1, "alice", 0.0);   // heard from at 0.01
        join(s, 2, "bob", 0.0);
        s.on_connect(3, 0.0);       // pending: expired()'s problem, not silent()'s

        check(s.silent(5.0).empty(), "nobody is silent five seconds in");
        check(s.silent(0.01 + kSessionSilence - 0.1).empty(), "nor just before the window");

        // Bob keeps talking; alice says nothing.
        const auto mv = Move{}.encode();
        s.on_message(2, (uint8_t)Channel::Entity, mv.data(), mv.size(), 10.0);

        const std::vector<PeerId> gone = s.silent(0.01 + kSessionSilence + 0.1);
        check(gone.size() == 1 && gone[0] == 1, "the one that went quiet is let go, and only it");
        check(!contains(gone, 3), "a peer still in the handshake is never counted as silent");

        // The server still sending to her holds the seat: the wait is its own.
        s.note_owed(1, 11.0);
        check(!contains(s.silent(0.01 + kSessionSilence + 0.1), 1),
              "a peer the server still owes an answer is not silent");
        check(contains(s.silent(11.0 + join_patience(*s.find(1)) + 0.1), 1),
              "but is, one patience after the server stopped owing it");

        s.note_owed(42, 1.0);   // no such peer
        check(s.size() == 3, "noting a debt to nobody creates nobody");
    }

    std::printf("\nhealth and death\n");
    {
        ServerSessions s(make_server(), spawn);
        join(s, 1, "alice", 0.0);
        s.vitals_to_send();     // the joining bar
        s.deaths_to_serve();

        check(s.health_of(1) == ecs::kFullHealth, "a new body is whole");
        check(s.hurt(1, 0, 1.0) == ecs::kFullHealth && s.vitals_to_send().empty(),
              "a blow of nothing changes nothing and owes nothing");

        check(s.hurt(1, 6, 1.0) == ecs::kFullHealth - 6, "a blow takes what it says");
        std::vector<PeerId> owed = s.vitals_to_send();
        check(owed.size() == 1 && owed[0] == 1, "and the bar is owed to its owner");
        check(s.vitals_to_send().empty(), "once");

        Vitals     v;
        const auto vb = s.vitals_message(1);
        ByteReader r(vb);
        check(Vitals::decode(r, v) && v.health == ecs::kFullHealth - 6 &&
                  v.max == ecs::kFullHealth,
              "and the message carries what is left");

        check(s.heal(1, 100) == ecs::kFullHealth, "healing stops at the maximum");

        check(s.set_max_health(1, 10) && s.health_of(1) == 10,
              "a lower maximum cuts what a body has to it");
        check(!s.set_max_health(1, 0), "and a maximum of nothing is refused");
        check(s.heal(1, 50) == 10, "healing respects the new maximum");
        s.vitals_to_send();

        // Regeneration waits after a blow, then pays a point a period.
        s.hurt(1, 4, 2.0);
        s.vitals_to_send();
        s.step_health(ecs::kRegenDelay - 0.5);
        check(s.health_of(1) == 6 && s.vitals_to_send().empty(),
              "nothing grows back straight after a blow");
        s.step_health(0.5 + ecs::kRegenPeriod * 2 + 0.01);
        check(s.health_of(1) == 8, "and after the wait a point grows back a period");
        check(contains(s.vitals_to_send(), 1), "which the player is told");

        // Dying is a teleport, not a corpse at zero.
        check(s.deaths_to_serve().empty(), "nobody has died yet");
        check(s.hurt(1, 100, 30.0) == 0, "a killing blow answers 0");
        check(s.health_of(1) == 10, "and the body is already whole for its way back");

        const std::vector<ServerSessions::Death> dead = s.deaths_to_serve();
        check(dead.size() == 1 && dead[0].peer == 1, "the death is served once");
        check(dead.size() == 1 && dead[0].back.x == spawn.x && dead[0].back.y == spawn.y &&
                  dead[0].back.z == spawn.z,
              "and sends them back to the spawn");
        check(s.deaths_to_serve().empty(), "and not twice");

        // The spawn can move, and the next death follows it.
        s.set_spawn(1.0f, 2.0f, 3.0f);
        s.hurt(1, 100, 40.0);
        const std::vector<ServerSessions::Death> again = s.deaths_to_serve();
        check(again.size() == 1 && again[0].back.x == 1.0f && again[0].back.y == 2.0f &&
                  again[0].back.z == 3.0f,
              "a moved spawn is where the next death goes");

        check(s.hurt(77, 5, 1.0) == 0 && s.health_of(77) == 0 && !s.set_max_health(77, 5),
              "a peer that is not here has no body to hurt");
    }

    std::printf("\nthe world's clock\n");
    {
        ServerSessions s(make_server(), spawn);
        join(s, 1, "alice", 0.0);
        join(s, 2, "bob", 0.0, Scope::Observe);
        s.on_connect(3, 0.0);

        std::vector<PeerId> owed = s.time_to_send();
        check(owed.size() == 2 && contains(owed, 1) && contains(owed, 2),
              "a joining player and a joining window are both owed the time");
        check(!contains(owed, 3), "a pending peer is not");
        check(s.time_to_send().empty(), "and once told they are not owed it again");

        s.set_time(0.25);
        check(s.time_to_send().size() == 2, "a time somebody sets is told to everybody at once");
        Time       t;
        const auto tb = s.time_message();
        ByteReader r(tb);
        check(Time::decode(r, t) && t.tod == clock_to_wire(0.25),
              "and the message says that time");

        // Drift is told on a schedule, not every tick.
        const double every = tell_interval(s.day_length());
        s.step_time(every * 0.5);
        check(s.time_to_send().empty(), "half an interval later nobody is owed it");
        s.step_time(every * 0.6);
        check(s.time_to_send().size() == 2, "a whole interval later everybody is");
        check(s.time_of_day() > 0.25, "and the clock has moved on");

        s.set_day_length(0.0);
        const double stood = s.time_of_day();
        s.step_time(100.0);
        check(s.time_of_day() == stood, "a day length of nothing stands the clock still");
        s.set_day_length(-5.0);
        check(s.day_length() == 0.0, "and a negative one is ignored");
    }

    std::printf("\nwhose ground\n");
    {
        ServerSessions s(make_server(), spawn);
        check(s.owned_at(0, 0) == 0, "nothing is owned to begin with");

        s.set_owned(10, 10, 0, 0, 7);   // corners in either order
        check(s.owned_at(5, 5) == 7 && s.owned_at(0, 10) == 7 && s.owned_at(10, 0) == 7,
              "an area covers its columns, edges included, whichever way its corners came");
        check(s.owned_at(11, 5) == 0 && s.owned_at(5, -1) == 0, "and nothing outside them");

        s.set_owned_cell(5, 5, 9);
        check(s.owned_at(5, 5) == 9 && s.owned_at(5, 6) == 7,
              "a single owned column comes before the area around it");
        s.set_owned_cell(5, 5, 0);
        check(s.owned_at(5, 5) == 7, "and taking it away gives the area back");

        s.set_owned(0, 0, 10, 10, 3);
        check(s.owned_at(5, 5) == 3, "the same corners again change the owner");

        s.set_own_cell([](int x, int y, int z) { return x == 2 && y == 4 && z == 2; });
        check(s.owned_tag(2, 4, 2) == 0 && s.owned_tag(2, 5, 2) == 3,
              "a block a player placed themselves is nobody's to steal");
        check(s.owned_at(2, 2) == 3, "while the column is still the owner's");

        Session villager;
        villager.owner_of = {3};
        Session stranger;
        check(s.owned_tag_for(villager, 6, 0, 6) == 0, "an owner does not steal from themselves");
        check(s.owned_tag_for(stranger, 6, 0, 6) == 3, "anybody else does");

        s.set_owned(10, 10, 0, 0, 0);
        check(s.owned_at(5, 5) == 0, "and a tag of 0 takes the area away");
    }

    std::printf("\nopenings\n");
    {
        ServerSessions s(make_server(), spawn);
        check(!s.has_portals(), "a fresh table has no portals");
        check(s.free_openings() == kMaxOpenings, "and every opening free");

        const Portal a = make_portal(PortalAxis::XY, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make_portal(PortalAxis::YZ, Vec3{40.0f, 20.0f, 10.0f});

        const int ia = s.open_portal(a);
        check(ia == 0, "the first opening takes the first index");
        check(!s.has_portals(), "and one opening on its own is not a portal");

        const int ib = s.open_portal(b);
        check(ib == 1 && s.has_portals(), "a second one pairs with it");
        check(s.free_openings() == kMaxOpenings - 2, "two are in use");

        Portal moved_a = a;
        moved_a.half_w = a.half_w + 1.0f;
        check(s.open_portal(moved_a) == ia && s.free_openings() == kMaxOpenings - 2,
              "opening again at the same centre reshapes it rather than taking another");

        check(s.link_portals(ia, -1) && !s.has_portals(), "unlinking breaks the pair");
        check(s.link_portals(ia, ib) && s.has_portals(), "and linking makes it again");
        check(!s.link_portals(-1, 0) && !s.link_portals(0, kMaxOpenings) &&
                  !s.link_portals(kMaxOpenings, 0),
              "an index out of range is refused");

        // Linking a to a third opening leaves b leading nowhere.
        const int ic = s.open_portal(make_portal(PortalAxis::XZ, Vec3{-30.0f, 5.0f, 0.0f}));
        check(s.link_portals(ia, ic), "a can be linked elsewhere");
        check(s.close_portal(Vec3{-30.0f, 5.0f, 0.0f}) == ic && !s.has_portals(),
              "and when that one closes, b was not quietly relinked to a");

        check(s.close_portal(Vec3{999.0f, 0.0f, 0.0f}) == -1,
              "closing where nothing stands closes nothing");
        check(s.close_portal(b.center) == ib, "closing is by centre");
        check(s.opening(ib) == nullptr, "and the index is empty afterwards");
        s.close_opening(ia);
        check(s.free_openings() == kMaxOpenings, "closing by index frees it too");

        // Full is full: one more than the table holds is refused, not wrapped
        // over the first.
        for (int i = 0; i < kMaxOpenings; ++i)
            s.open_portal(make_portal(PortalAxis::XY, Vec3{(float)i * 10.0f, 0.0f, 50.0f}));
        check(s.free_openings() == 0, "every opening can be used");
        check(s.open_portal(make_portal(PortalAxis::XY, Vec3{0.0f, 0.0f, -500.0f})) == -1,
              "and one more is refused");
    }

    std::printf("\n%s\n", g_failures == 0 ? "all session checks passed" : "SESSION CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
