#pragma once

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
struct Hooks {
    std::function<std::vector<Online>()>                                           online;
    std::function<void(net::PeerId, net::RefuseReason, const std::string &text)> remove;
};

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
