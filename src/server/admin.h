#pragma once

#include "core/net/crypto.h"
#include "core/net/protocol.h"
#include "core/net/transport.h"

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

// Who may do what on this server, and who may not come in at all.
//
// One store for the whole process, whatever the rooms: a ban in the sandbox has
// to be a ban in the arena too, or a portal is a way round it. It is its own
// connection to the FIRST room's world file (the one core.shared_storage_*
// lives in), so it does not wait on a room that has not opened its file yet;
// WAL lets the two connections share it. No world file: an in-memory store,
// good until the server stops.
//
// Every row is keyed by a TARGET, a string that says what kind of thing it is:
//   key:<hex>   a player's identity, which is what a player is (DESIGN.md 4.4)
//   name:<n>    a name, for a player who brought no identity
//   ip:<addr>   an address
// A name with an owner on this world is always turned into that owner's key
// first: a ban on a name a key owns is a ban on the key.

namespace cnp {
namespace admin {

enum class Role : int { Guest = 0, Player, Helper, Moderator, Admin, Owner };

const char *role_name(Role r);
bool        parse_role(const std::string &text, Role &out);

// The least role a permission wants. A role's name is a permission too
// ("moderator"), which is what a mod passes when it has no better word. False
// for a word nobody has heard of.
bool perm_role(const std::string &perm, Role &out);

// "30m", "2h", "7d", "1w", "45s" in seconds; false for anything else.
bool        parse_duration(const std::string &text, int64_t &seconds);
std::string duration_text(int64_t seconds);

// `path` is the world file; empty for a store kept in memory.
bool open(const std::string &path, std::string &err);
void close();

// --webhook URL: every admin action, start, stop and /report POSTed there as
// JSON. False for anything but an http(s) URL of plain characters.
bool set_webhook(const std::string &url);

// --owner NAME: whoever plays under that name with an identity is the owner.
// The name check already makes a name its first key's on a kept world.
void set_owner(const std::string &name);

// A player as the store sees them: online, at the door, or remembered.
struct Who {
    std::string name;
    bool        has_identity = false;
    uint8_t     identity[net::kSignPublicBytes]{};
    std::string address;   // "" when not connected
    bool        console = false;   // the server's own stdin: above every role
    std::string room;      // the room they are in, for a role given in one room only
};

Role role_of(const Who &who);
bool may(const Who &who, const std::string &perm);

// Whether this player may come in. False with the reason and the sentence for
// the refusal: a ban (theirs or their address's), or a whitelist they are not on.
bool admits(const Who &who, net::RefuseReason &reason, std::string &why);

// Whether this player's chat is held, and the sentence that tells them so.
bool muted(const Who &who, std::string &why);

// What a command needs from the server that runs it: the players online in
// every room, and a way to put one out.
struct Online {
    net::PeerId peer = net::kNoPeer;
    Who         who;
    std::string room;   // "" on a server of one world
};
// A block to put back (/rollback): in `room`, if it still holds `expect`.
struct Restore {
    std::string room;
    int         x = 0, y = 0, z = 0, block = 0, facing = 0, expect = 0;
};

struct Hooks {
    std::function<std::vector<Online>()>                                           online;
    std::function<void(net::PeerId, net::RefuseReason, const std::string &text)> remove;
    std::function<void(const std::string &text)>                                  notice;   // every screen
    std::function<void(net::PeerId, const std::string &line)>                     say;      // one chat
    std::function<std::vector<std::string>()>                                     metrics;
    std::function<std::vector<std::string>()>                                     portals;  // links out
    std::function<std::string(const std::string &room, int block)>                block_name;
    std::function<void(const std::vector<Restore> &)>                             restore;

    // Bags between friend servers (bag_ask). bag_give: a friend wants this
    // player's bag -- take it from wherever it is in this process (a session
    // holding it lets go of it), or from the world file when `from_file`;
    // false for no bag here at all. bag_take: the bag a friend sent, by name
    // (core/item/item_codec.h), and what this player carries that some world
    // did not know (`hidden`). bag_late: the friend did not send it; use the
    // one here.
    std::function<bool(const uint8_t *identity, bool from_file, std::string &bag)> bag_give;
    std::function<void(const uint8_t *identity, const std::string &bag, const std::string &hidden,
                       const std::string &from)>                                   bag_take;
    std::function<void(const uint8_t *identity)>                                   bag_late;
    std::function<int64_t(const uint8_t *identity)> bag_seen;   // kept here when, -1 for never or not held

    // /bag and /give: what a player carries, a line a stack; and `count` of an
    // item by name into an online player's bag ("" when it went in).
    std::function<std::vector<std::string>(const Who &)>                             bag_lines;
    std::function<std::string(net::PeerId, const std::string &item, int count)>     give;
};

// The server's own life: /restart and /stop count down, warning every screen,
// and --restart-at HH:MM does it every day with ten minutes' warning. Called
// by one room each tick; at the end everybody is put out (Restarting or
// Stopping) and it says which. A restart is the process leaving with 75, for
// tools/serve.sh to start it again.
enum class Stop { None, Stop, Restart };
Stop tick(const Hooks &hooks);
bool restart_at(const std::string &hhmm);   // false for anything but HH:MM

// The message of the day, said to each player coming in; --motd sets it.
std::string motd();
std::string maintenance();   // its text, "" when open to everybody
void        set_motd(const std::string &text);

// Visits, for /seen, /players, /whois and --report: one row from coming into a
// room to leaving it. A visit a crash left open is closed at the next start.
void joined(const Who &who, const std::string &room);
void left(const Who &who, const std::string &room, const std::string &reason);
// A player who came through a portal on a ticket `issuer` signed, from
// `address`: who sends players here, how many, from where (/portals).
void crossed_in(const uint8_t *issuer, const std::string &address);
// A block a player changed, for /inspect and /rollback.
void edited(const Who &who, const std::string &room, int x, int y, int z, int old_block, int old_facing,
            int new_block);
// A player the door turned away (admits), for --report.
void refused(const Who &who, const std::string &why);

// Copies of the world files next to them (NAME.backup-YYYYMMDD-HHMMSS), with
// SQLite's online backup on a thread of its own; the newest `keep` stay.
// `every` seconds, 0 for only /backup.
void set_backups(const std::vector<std::string> &files, int64_t every, int keep);
bool backup(std::string &err);

// --report: what a world file remembers of its players, printed. Offline,
// with the server stopped or not (it only reads).
int report(const std::string &path, int64_t since_seconds);

// Federation with friend servers (/friend, /fed, /where): a TCP port other
// servers send signed lines to, 0 for sending only; `me` is this server's
// signing key, which friends put in their list. federation_tick, from one
// room each tick, applies what friends sent and sends this server's share.
bool start_federation(int port, const net::SignKeyPair &me, std::string &err);
void federation_tick(const Hooks &hooks);

// One bag for friend servers: a player who arrives as a body on a ticket a
// friend signed, or whose bag went to a friend last, has it sent from there,
// and that friend lets go of it (Hooks::bag_give). True: asked for; the bag
// stays empty until bag_take, or bag_late after five seconds, or at once when
// the friend says it has none. A player who walked in, with nothing saying
// where their bag is, has every friend asked whose copy is newest
// (Hooks::bag_seen), and that one sends it if it is newer than the one here.
// False: the bag is here.
bool bag_ask(const Who &who, const uint8_t *issuer, const Hooks &hooks);

// What this player carries that a world did not know, kept aside (encoded,
// "" for nothing): it goes with the bag to the next friend and comes out where
// it is known.
std::string hidden(const uint8_t *identity);
void        set_hidden(const uint8_t *identity, const std::string &bytes);

// The console, which is the machine's owner.
Who console();

// Runs /name args for `caller`. False when `name` is not an admin command, so
// the caller tries the mods'. `out` is what to tell them, a line each.
bool run(const Who &caller, const std::string &name, const std::string &args, const Hooks &hooks,
         std::vector<std::string> &out);

// Whether `name` is one of these, which no mod may declare.
bool is_command(const std::string &name);

// What /help adds: the admin commands this caller may use.
std::vector<std::string> help(const Who &caller);

// The administration pages, as page markup (src/core/ui/page.h): admin, bans,
// whitelist, roles, perms. False for a name that is none of these. What a
// viewer may not do is not on their page at all.
bool page(const std::string &name, const Who &viewer, const Hooks &hooks, std::string &title,
          std::string &markup);


// Lines from outside the game, for the tick to run as admin commands: the
// server's stdin, and the RCON socket. A line with a `reply` wants its answer
// back on it; one without is the console's, and its answer is printed.
struct ConsoleLine {
    std::string                                             line;
    Who                                                     who;
    std::shared_ptr<std::promise<std::vector<std::string>>> reply;
};

// Started once; a closed stdin ends it.
void start_console();

// A TCP socket on 127.0.0.1 only: the first line is the password, then a
// command a line, each answered by its lines and a line holding one ".".
// False with `err` when it cannot listen, or for an empty password.
bool start_rcon(int port, const std::string &password, std::string &err);

std::vector<ConsoleLine> take_console_lines();

} // namespace admin
} // namespace cnp
