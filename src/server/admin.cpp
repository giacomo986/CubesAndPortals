#include "server/admin.h"

#include "core/net/crypto.h"
#include "core/ui/page.h"

#include <sqlite3.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>
#include <thread>

namespace cnp {
namespace admin {
namespace {

// One table for every kind of row, because every kind is the same shape: what
// it is about, who said so, when, and until when (0 for always).
//   role    value = the role's name
//   ban     value = the reason
//   mute    value = the reason
//   allow   on the whitelist
//   setting target "whitelist", value "on" or "off"
// Its own CREATE IF NOT EXISTS, like mod_store: a world file from before it
// grows it empty, and an empty table is a server nobody has been banned from.
const char *kTables =
    "CREATE TABLE IF NOT EXISTS admin ("
    "  kind TEXT NOT NULL,"
    "  target TEXT NOT NULL,"
    "  label TEXT NOT NULL,"
    "  value TEXT NOT NULL,"
    "  by TEXT NOT NULL,"
    "  at INTEGER NOT NULL,"
    "  until INTEGER NOT NULL,"
    "  PRIMARY KEY (kind, target)"
    ") WITHOUT ROWID;"
    // Every admin action, for whoever asks later who banned whom and why.
    "CREATE TABLE IF NOT EXISTS admin_log ("
    "  at INTEGER NOT NULL,"
    "  by TEXT NOT NULL,"
    "  action TEXT NOT NULL,"
    "  target TEXT NOT NULL,"
    "  detail TEXT NOT NULL"
    ");";

::sqlite3  *g_db = nullptr;
std::string g_owner;

int64_t now() { return (int64_t)std::time(nullptr); }

struct Row {
    std::string target, label, value, by;
    int64_t     at = 0, until = 0;
};

// A statement with text arguments bound in order, stepped by `each`.
bool query(const char *sql, std::initializer_list<std::string> args,
           const std::function<void(sqlite3_stmt *)> &each = nullptr)
{
    if (!g_db) return false;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        std::printf("server: admin: %s\n", sqlite3_errmsg(g_db));
        return false;
    }
    int i = 1;
    for (const std::string &a : args) sqlite3_bind_text(st, i++, a.c_str(), -1, SQLITE_TRANSIENT);
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW)
        if (each) each(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

std::string col(sqlite3_stmt *st, int i)
{
    const unsigned char *t = sqlite3_column_text(st, i);
    return t ? (const char *)t : "";
}

Row read_row(sqlite3_stmt *st)
{
    return Row{col(st, 0), col(st, 1), col(st, 2), col(st, 3), sqlite3_column_int64(st, 4),
               sqlite3_column_int64(st, 5)};
}

// The row of this kind for this target, if it has not run out. One that has
// is deleted on the way: a ban that ended is not a ban.
bool find(const std::string &kind, const std::string &target, Row &out)
{
    bool found = false;
    query("SELECT target, label, value, by, at, until FROM admin WHERE kind = ?1 AND target = ?2",
          {kind, target}, [&](sqlite3_stmt *st) {
              out   = read_row(st);
              found = true;
          });
    if (found && out.until != 0 && out.until <= now()) {
        query("DELETE FROM admin WHERE kind = ?1 AND target = ?2", {kind, target});
        return false;
    }
    return found;
}

std::vector<Row> all(const std::string &kind)
{
    query("DELETE FROM admin WHERE kind = ?1 AND until != 0 AND until <= ?2",
          {kind, std::to_string(now())});
    std::vector<Row> rows;
    query("SELECT target, label, value, by, at, until FROM admin WHERE kind = ?1 ORDER BY at",
          {kind}, [&](sqlite3_stmt *st) { rows.push_back(read_row(st)); });
    return rows;
}

void put(const std::string &kind, const std::string &target, const std::string &label,
         const std::string &value, const std::string &by, int64_t until)
{
    query("INSERT OR REPLACE INTO admin (kind, target, label, value, by, at, until) "
          "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
          {kind, target, label, value, by, std::to_string(now()), std::to_string(until)});
}

bool drop(const std::string &kind, const std::string &target)
{
    query("DELETE FROM admin WHERE kind = ?1 AND target = ?2", {kind, target});
    return g_db && sqlite3_changes(g_db) > 0;
}

void log(const Who &by, const std::string &action, const std::string &target,
         const std::string &detail)
{
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, ?2, ?3, ?4, ?5)",
          {std::to_string(now()), by.name, action, target, detail});
    std::printf("        admin: %s %s %s%s%s\n", by.name.c_str(), action.c_str(), target.c_str(),
                detail.empty() ? "" : " -- ", detail.c_str());
}

std::string key_target(const uint8_t key[net::kSignPublicBytes])
{
    return "key:" + net::to_hex(key, net::kSignPublicBytes);
}

// What a player's rows are kept under: the key when there is one.
std::string target_of(const Who &w)
{
    return w.has_identity ? key_target(w.identity) : "name:" + w.name;
}

// Every target a player at the door answers to.
std::vector<std::string> targets_of(const Who &w)
{
    std::vector<std::string> t;
    if (w.has_identity) t.push_back(key_target(w.identity));
    if (!w.name.empty()) t.push_back("name:" + w.name);
    if (!w.address.empty()) t.push_back("ip:" + w.address);
    return t;
}

std::string when_text(int64_t t)
{
    char buf[32] = "?";
    const std::time_t tt = (std::time_t)t;
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&tt));
    return buf;
}

std::string until_text(int64_t until)
{
    return until == 0 ? "for good" : "until " + when_text(until);
}

// "NAME [duration] [reason...]".
struct Args {
    std::string name, reason;
    int64_t     seconds = 0;   // 0: no duration given
    std::vector<std::string> words;
};

Args split(const std::string &line)
{
    Args a;
    std::istringstream in(line);
    for (std::string w; in >> w;) a.words.push_back(w);
    size_t i = 0;
    if (i < a.words.size()) a.name = a.words[i++];
    if (i < a.words.size() && parse_duration(a.words[i], a.seconds)) ++i;
    for (; i < a.words.size(); ++i) a.reason += (a.reason.empty() ? "" : " ") + a.words[i];
    return a;
}

// A player by name: online in any room first, then whoever owns the name on
// this world, then the bare name.
Who resolve(const std::string &name, const Hooks &hooks, net::PeerId *peer = nullptr)
{
    if (hooks.online)
        for (const Online &o : hooks.online())
            if (o.who.name == name) {
                if (peer) *peer = o.peer;
                return o.who;
            }
    Who w;
    w.name = name;
    // No names table in a store kept in memory, and nobody owns a name there.
    bool names = false;
    query("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'names'", {},
          [&](sqlite3_stmt *) { names = true; });
    if (names)
        query("SELECT identity FROM names WHERE name = ?1", {name}, [&](sqlite3_stmt *st) {
            if (sqlite3_column_bytes(st, 0) != (int)net::kSignPublicBytes) return;
            std::memcpy(w.identity, sqlite3_column_blob(st, 0), net::kSignPublicBytes);
            w.has_identity = true;
        });
    // Where they last came from (admits), for a ban that takes the address.
    Row r;
    if (find("addr", target_of(w), r)) w.address = r.value;
    return w;
}

bool looks_like_address(const std::string &s)
{
    if (s.empty()) return false;
    for (const char c : s)
        if (!(std::isxdigit((unsigned char)c) || c == '.' || c == ':')) return false;
    return s.find('.') != std::string::npos || s.find(':') != std::string::npos;
}

// Who is online that this target names: a key, a name, or an address.
std::vector<Online> online_matching(const std::string &target, const Hooks &hooks)
{
    std::vector<Online> hit;
    if (!hooks.online) return hit;
    for (const Online &o : hooks.online())
        for (const std::string &t : targets_of(o.who))
            if (t == target) {
                hit.push_back(o);
                break;
            }
    return hit;
}

// Whether `caller` outranks `target`: nobody acts on an equal or a superior.
bool outranks(const Who &caller, const Who &target)
{
    return caller.console || role_of(caller) > role_of(target);
}

struct PermRow {
    const char *perm;
    Role        role;
};
const PermRow kPerms[] = {
    {"mute", Role::Helper},        {"kick", Role::Moderator}, {"ban", Role::Moderator},
    {"creative", Role::Moderator}, {"pages", Role::Helper},   {"ban.ip", Role::Admin},
    {"access", Role::Admin},       {"roles", Role::Admin},    {"perms", Role::Owner},
};

struct Command {
    const char *name;
    const char *perm;   // nullptr: anybody
    const char *help;
    void (*fn)(const Who &, const Args &, const Hooks &, std::vector<std::string> &);
};

void cmd_perms(const Who &c, const Args &, const Hooks &, std::vector<std::string> &out)
{
    out.push_back(std::string("You are ") + role_name(role_of(c)) + " here.");
}

void cmd_role(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.name.empty()) {
        for (const Row &r : all("role"))
            out.push_back(r.label + ": " + r.value + " (by " + r.by + ", " + when_text(r.at) + ")");
        if (!g_owner.empty()) out.push_back(g_owner + ": owner (--owner)");
        if (out.empty()) out.push_back("Nobody here has a role.");
        return;
    }
    const Who t = resolve(a.name, h);
    if (a.words.size() < 2) {
        out.push_back(a.name + " is " + role_name(role_of(t)) + ".");
        return;
    }
    Role want;
    if (!parse_role(a.words[1], want) || want == Role::Guest) {
        out.push_back("No role '" + a.words[1] + "': player, helper, moderator, admin, owner.");
        return;
    }
    if (!t.has_identity) {
        out.push_back(a.name + " has no identity here, so a role has nothing to hold on to.");
        return;
    }
    if (!outranks(c, t) || (!c.console && want >= role_of(c))) {
        out.push_back("You can only give roles below your own, to players below you.");
        return;
    }
    if (want == Role::Player) {
        drop("role", target_of(t));
    } else {
        put("role", target_of(t), t.name, role_name(want), c.name, 0);
    }
    log(c, "role", t.name, role_name(want));
    out.push_back(a.name + " is now " + role_name(want) + ".");
}

void cmd_kick(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    net::PeerId peer = net::kNoPeer;
    const Who   t    = resolve(a.name, h, &peer);
    if (peer == net::kNoPeer) {
        out.push_back("Nobody called '" + a.name + "' is online.");
        return;
    }
    if (!outranks(c, t)) {
        out.push_back("You cannot kick " + a.name + ".");
        return;
    }
    // Every session they have: a portal window into another room is one too.
    // A duration means nothing to a kick: every word after the name is the reason.
    std::string why;
    for (size_t i = 1; i < a.words.size(); ++i) why += (i > 1 ? " " : "") + a.words[i];
    log(c, "kick", t.name, why);
    for (const Online &o : online_matching(target_of(t), h))
        h.remove(o.peer, net::RefuseReason::Kicked, why.empty() ? "by " + c.name : why);
    out.push_back(a.name + " was kicked.");
}

// /ban and /banip, which differ in what they ban.
void ban(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out, bool by_ip)
{
    std::string target, label = a.name;
    if (by_ip && looks_like_address(a.name)) {
        target = "ip:" + a.name;
    } else {
        const Who t = resolve(a.name, h);
        if (!outranks(c, t)) {
            out.push_back("You cannot ban " + a.name + ".");
            return;
        }
        if (by_ip) {
            if (t.address.empty()) {
                out.push_back("No address known for " + a.name + ": give it instead.");
                return;
            }
            target = "ip:" + t.address;
            label  = a.name + " (" + t.address + ")";
        } else {
            target = target_of(t);
            // No identity: a name is a word anybody may type, so the address
            // they came from goes too, or the ban is a rename away.
            if (!t.has_identity && !t.address.empty())
                put("ban", "ip:" + t.address, a.name + " (" + t.address + ")", a.reason, c.name,
                    a.seconds ? now() + a.seconds : 0);
        }
    }
    const int64_t until = a.seconds ? now() + a.seconds : 0;
    put("ban", target, label, a.reason, c.name, until);
    log(c, by_ip ? "banip" : "ban", label, until_text(until) + (a.reason.empty() ? "" : ", " + a.reason));

    const std::string text = (a.reason.empty() ? std::string() : a.reason + ", ") + until_text(until);
    for (const Online &o : online_matching(target, h)) h.remove(o.peer, net::RefuseReason::Banned, text);
    out.push_back(label + " is banned " + until_text(until) + ".");
}

void cmd_ban(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    ban(c, a, h, out, false);
}

void cmd_banip(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    ban(c, a, h, out, true);
}

void cmd_unban(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const std::string target =
        looks_like_address(a.name) ? "ip:" + a.name : target_of(resolve(a.name, h));
    // A name banned before it had an owner is under its name: both go.
    bool gone = drop("ban", target) | drop("ban", "name:" + a.name);
    // And the address a ban by name took with it (ban, above).
    for (const Row &r : all("ban"))
        if (r.target.rfind("ip:", 0) == 0 && r.label.rfind(a.name + " (", 0) == 0)
            gone = drop("ban", r.target) | gone;
    if (!gone) {
        out.push_back(a.name + " is not banned.");
        return;
    }
    log(c, "unban", a.name, "");
    out.push_back(a.name + " is no longer banned.");
}

void cmd_bans(const Who &, const Args &, const Hooks &, std::vector<std::string> &out)
{
    for (const Row &r : all("ban"))
        out.push_back(r.label + " " + until_text(r.until) + ", by " + r.by +
                      (r.value.empty() ? "" : ": " + r.value));
    if (out.empty()) out.push_back("Nobody is banned.");
}

void cmd_mute(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const Who t = resolve(a.name, h);
    if (!outranks(c, t)) {
        out.push_back("You cannot mute " + a.name + ".");
        return;
    }
    const int64_t until = a.seconds ? now() + a.seconds : 0;
    put("mute", target_of(t), t.name, a.reason, c.name, until);
    log(c, "mute", t.name, until_text(until) + (a.reason.empty() ? "" : ", " + a.reason));
    out.push_back(a.name + " is muted " + until_text(until) + ".");
}

void cmd_unmute(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (!drop("mute", target_of(resolve(a.name, h)))) {
        out.push_back(a.name + " is not muted.");
        return;
    }
    log(c, "unmute", a.name, "");
    out.push_back(a.name + " may talk again.");
}

bool whitelist_on()
{
    Row r;
    return find("setting", "whitelist", r) && r.value == "on";
}

void cmd_whitelist(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const std::string &verb = a.name;
    const std::string  who  = a.words.size() > 1 ? a.words[1] : "";
    if (verb == "on" || verb == "off") {
        put("setting", "whitelist", "whitelist", verb, c.name, 0);
        log(c, "whitelist", verb, "");
        out.push_back(verb == "on" ? "Only the players on the list, and staff, may come in now."
                                   : "Anybody may come in now.");
    } else if ((verb == "add" || verb == "remove") && !who.empty()) {
        const Who t = resolve(who, h);
        if (verb == "add") {
            put("allow", target_of(t), t.name, "", c.name, 0);
        } else if (!drop("allow", target_of(t))) {
            out.push_back(who + " is not on the list.");
            return;
        }
        log(c, "whitelist " + verb, t.name, "");
        out.push_back(who + (verb == "add" ? " is on the list." : " is off the list."));
    } else if (verb == "list" || verb.empty()) {
        std::string names;
        for (const Row &r : all("allow")) names += (names.empty() ? "" : ", ") + r.label;
        out.push_back(std::string("The list is ") + (whitelist_on() ? "on" : "off") + ": " +
                      (names.empty() ? "nobody on it." : names));
    } else {
        out.push_back("/whitelist on|off|list|add NAME|remove NAME");
    }
}

void cmd_perm(const Who &c, const Args &a, const Hooks &, std::vector<std::string> &out)
{
    if (a.name.empty()) {
        for (const PermRow &p : kPerms) {
            Role r;
            perm_role(p.perm, r);
            out.push_back(std::string(p.perm) + ": " + role_name(r) +
                          (r != p.role ? std::string(" (default ") + role_name(p.role) + ")" : ""));
        }
        return;
    }
    const PermRow *known = nullptr;
    for (const PermRow &p : kPerms)
        if (a.name == p.perm) known = &p;
    Role want;
    if (!known || a.words.size() < 2 || !parse_role(a.words[1], want)) {
        out.push_back("/perm PERMISSION ROLE -- /perm alone lists them");
        return;
    }
    // Nobody hands a permission down past where they could still use it.
    if (!c.console && want > role_of(c)) {
        out.push_back("You cannot put a permission above your own role.");
        return;
    }
    if (want == known->role) drop("perm", a.name);
    else put("perm", a.name, a.name, role_name(want), c.name, 0);
    log(c, "perm", a.name, role_name(want));
    out.push_back(a.name + " now wants " + role_name(want) + ".");
}

const Command kCommands[] = {
    {"perms", nullptr, "-- your role here", cmd_perms},
    {"role", "roles", "[NAME [ROLE]] -- see or give roles", cmd_role},
    {"kick", "kick", "NAME [reason] -- put a player out", cmd_kick},
    {"ban", "ban", "NAME [30m|2h|7d] [reason] -- keep a player out", cmd_ban},
    {"banip", "ban.ip", "NAME|ADDRESS [time] [reason] -- keep an address out", cmd_banip},
    {"unban", "ban", "NAME|ADDRESS", cmd_unban},
    {"bans", "ban", "-- who is banned", cmd_bans},
    {"mute", "mute", "NAME [time] [reason] -- hold a player's chat", cmd_mute},
    {"unmute", "mute", "NAME", cmd_unmute},
    {"whitelist", "access", "on|off|list|add NAME|remove NAME", cmd_whitelist},
    {"perm", "perms", "[PERMISSION ROLE] -- which role each permission wants", cmd_perm},
};

std::mutex               g_console_m;
std::vector<ConsoleLine> g_console_lines;

void queue_line(ConsoleLine l)
{
    std::lock_guard<std::mutex> hold(g_console_m);
    g_console_lines.push_back(std::move(l));
}

#ifdef _WIN32
using Sock = SOCKET;
void close_sock(Sock s) { closesocket(s); }
long read_fd(int fd, char *buf, size_t n) { return (long)_read(fd, buf, (unsigned)n); }
long read_sock(Sock s, char *buf, size_t n) { return (long)recv(s, buf, (int)n, 0); }
bool send_sock(Sock s, const std::string &t) { return send(s, t.data(), (int)t.size(), 0) == (int)t.size(); }
constexpr Sock kNoSock = INVALID_SOCKET;
#else
using Sock = int;
void close_sock(Sock s) { ::close(s); }
long read_fd(int fd, char *buf, size_t n) { return (long)::read(fd, buf, n); }
long read_sock(Sock s, char *buf, size_t n) { return (long)::recv(s, buf, n, 0); }
bool send_sock(Sock s, const std::string &t)
{
    return ::send(s, t.data(), t.size(), MSG_NOSIGNAL) == (ssize_t)t.size();
}
constexpr Sock kNoSock = -1;
#endif

// Lines off something that reads like read(2), until it stops: each one to `each`,
// which says whether to go on.
template <typename Read>
void read_lines(Read rd, const std::function<bool(std::string)> &each)
{
    std::string pending;
    char        buf[512];
    for (long n; (n = rd(buf, sizeof(buf))) > 0;) {
        pending.append(buf, (size_t)n);
        for (size_t nl; (nl = pending.find('\n')) != std::string::npos;) {
            std::string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!each(std::move(line))) return;
        }
        if (pending.size() > 4096) return;   // a line nobody types
    }
}

// Compared in time that does not say how much of it was right.
bool same_secret(const std::string &a, const std::string &b)
{
    unsigned diff = (unsigned)(a.size() ^ b.size());
    for (size_t i = 0; i < a.size(); ++i) diff |= (unsigned)(a[i] ^ (i < b.size() ? b[i] : 0));
    return diff == 0;
}

std::atomic<int> g_rcon_clients{0};

void rcon_client(Sock c, std::string password)
{
    bool told = false;
    read_lines([c](char *buf, size_t n) { return read_sock(c, buf, n); },
               [&](std::string line) {
                   if (!told) {
                       if (!same_secret(line, password)) {
                           // Slow, so guessing it is slow too.
                           std::this_thread::sleep_for(std::chrono::seconds(1));
                           send_sock(c, "denied\n");
                           return false;
                       }
                       told = true;
                       return send_sock(c, "ok\n");
                   }
                   ConsoleLine l;
                   l.line  = line;
                   l.who   = console();
                   l.who.name = "rcon";
                   l.reply = std::make_shared<std::promise<std::vector<std::string>>>();
                   std::future<std::vector<std::string>> answer = l.reply->get_future();
                   queue_line(std::move(l));
                   std::string out;
                   if (answer.wait_for(std::chrono::seconds(10)) == std::future_status::ready)
                       for (const std::string &a : answer.get()) out += a + "\n";
                   else
                       out = "no answer: the server is busy or stopping\n";
                   return send_sock(c, out + ".\n");
               });
    close_sock(c);
    --g_rcon_clients;
}

} // namespace

const char *role_name(Role r)
{
    switch (r) {
    case Role::Guest:     return "guest";
    case Role::Player:    return "player";
    case Role::Helper:    return "helper";
    case Role::Moderator: return "moderator";
    case Role::Admin:     return "admin";
    case Role::Owner:     return "owner";
    }
    return "guest";
}

bool parse_role(const std::string &text, Role &out)
{
    for (int i = (int)Role::Guest; i <= (int)Role::Owner; ++i)
        if (text == role_name((Role)i)) {
            out = (Role)i;
            return true;
        }
    return false;
}

bool perm_role(const std::string &perm, Role &out)
{
    for (const PermRow &p : kPerms)
        if (perm == p.perm) {
            Row r;
            if (!(find("perm", perm, r) && parse_role(r.value, out))) out = p.role;
            return true;
        }
    return parse_role(perm, out);
}

bool parse_duration(const std::string &text, int64_t &seconds)
{
    if (text.size() < 2) return false;
    int64_t n = 0;
    size_t  i = 0;
    for (; i + 1 < text.size(); ++i) {
        if (!std::isdigit((unsigned char)text[i]) || n > 100000000) return false;
        n = n * 10 + (text[i] - '0');
    }
    int64_t unit = 0;
    switch (text[i]) {
    case 's': unit = 1; break;
    case 'm': unit = 60; break;
    case 'h': unit = 3600; break;
    case 'd': unit = 86400; break;
    case 'w': unit = 7 * 86400; break;
    default:  return false;
    }
    if (n == 0) return false;
    seconds = n * unit;
    return true;
}

std::string duration_text(int64_t s)
{
    if (s % 86400 == 0) return std::to_string(s / 86400) + "d";
    if (s % 3600 == 0) return std::to_string(s / 3600) + "h";
    if (s % 60 == 0) return std::to_string(s / 60) + "m";
    return std::to_string(s) + "s";
}

bool open(const std::string &path, std::string &err)
{
    close();
    const std::string at = path.empty() ? ":memory:" : path;
    if (sqlite3_open_v2(at.c_str(), &g_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) !=
            SQLITE_OK ||
        sqlite3_busy_timeout(g_db, 2000) != SQLITE_OK ||
        sqlite3_exec(g_db, kTables, nullptr, nullptr, nullptr) != SQLITE_OK) {
        err = "admin store: " + std::string(g_db ? sqlite3_errmsg(g_db) : "cannot open") + " (" +
              at + ")";
        close();
        return false;
    }
    return true;
}

void close()
{
    if (g_db) sqlite3_close(g_db);
    g_db = nullptr;
}

void set_owner(const std::string &name) { g_owner = name; }

Role role_of(const Who &w)
{
    if (w.console) return Role::Owner;
    if (!w.has_identity) return Role::Guest;
    if (!g_owner.empty() && w.name == g_owner) return Role::Owner;
    Row  r;
    Role role;
    if (find("role", key_target(w.identity), r) && parse_role(r.value, role)) return role;
    return Role::Player;
}

bool may(const Who &w, const std::string &perm)
{
    Role need;
    return perm_role(perm, need) && role_of(w) >= need;
}

bool admits(const Who &w, net::RefuseReason &reason, std::string &why)
{
    for (const std::string &t : targets_of(w)) {
        Row r;
        if (!find("ban", t, r)) continue;
        reason = net::RefuseReason::Banned;
        why    = (r.value.empty() ? std::string() : r.value + ", ") + until_text(r.until);
        return false;
    }
    if (whitelist_on() && role_of(w) < Role::Helper) {
        Row r;
        if (!find("allow", target_of(w), r)) {
            reason = net::RefuseReason::NotAllowed;
            why    = "ask an admin";
            return false;
        }
    }
    if (!w.address.empty()) put("addr", target_of(w), w.name, w.address, "", 0);
    return true;
}

bool muted(const Who &w, std::string &why)
{
    Row r;
    if (!find("mute", target_of(w), r)) return false;
    why = "You are muted " + until_text(r.until) + (r.value.empty() ? "" : ": " + r.value) + ".";
    return true;
}

Who console()
{
    Who w;
    w.name    = "console";
    w.console = true;
    return w;
}

bool run(const Who &caller, const std::string &name, const std::string &args, const Hooks &hooks,
         std::vector<std::string> &out)
{
    for (const Command &c : kCommands) {
        if (name != c.name) continue;
        if (c.perm && !may(caller, c.perm)) {
            out.push_back("You may not use /" + name + ".");
            return true;
        }
        const Args a = split(args);
        if (a.name.empty() && c.fn != cmd_perms && c.fn != cmd_role && c.fn != cmd_bans &&
            c.fn != cmd_whitelist && c.fn != cmd_perm) {
            out.push_back(std::string("/") + c.name + " " + c.help);
            return true;
        }
        c.fn(caller, a, hooks, out);
        return true;
    }
    return false;
}

bool is_command(const std::string &name)
{
    for (const Command &c : kCommands)
        if (name == c.name) return true;
    return false;
}

std::vector<std::string> help(const Who &caller)
{
    std::vector<std::string> out;
    for (const Command &c : kCommands)
        if (!c.perm || may(caller, c.perm)) out.push_back(std::string("/") + c.name + " " + c.help);
    return out;
}

using ui::page_text;

namespace {

// A button that runs a command, when the viewer may run it and the words in
// it are safe to put in a link.
std::string cmd_link(const Who &viewer, const char *perm, const std::string &label,
                     const std::string &line, const std::string &name)
{
    if ((perm && !may(viewer, perm)) || page_text(name) != name) return "";
    return " [" + label + "](cmd:" + line + ")";
}

std::string admin_page(const Who &v, const Hooks &h)
{
    std::string m = "You are " + std::string(role_name(role_of(v))) + " here.\n";
    if (may(v, "ban")) m += "[Bans](page:bans) ";
    if (may(v, "access")) m += "[Whitelist](page:whitelist) ";
    if (may(v, "roles")) m += "[Roles](page:roles) ";
    if (may(v, "perms")) m += "[Permissions](page:perms) ";
    m += "[Server](page:server) [Mods](page:mods) [Here](page:here)\n";

    const std::vector<Online> on = h.online ? h.online() : std::vector<Online>{};
    m += "## Online: " + std::to_string(on.size()) + "\n";
    if (!on.empty()) m += "| name | role | room | |\n";
    for (const Online &o : on) {
        const std::string &n = o.who.name;
        std::string        acts;
        if (outranks(v, o.who)) {
            acts += cmd_link(v, "kick", "kick", "/kick " + n, n);
            acts += cmd_link(v, "ban", "ban 1h", "/ban " + n + " 1h", n);
            std::string why;
            acts += muted(o.who, why) ? cmd_link(v, "mute", "unmute", "/unmute " + n, n)
                                      : cmd_link(v, "mute", "mute 10m", "/mute " + n + " 10m", n);
        }
        m += "| " + page_text(n) + " | " + role_name(role_of(o.who)) + " | " +
             page_text(o.room.empty() ? "-" : o.room) + " |" + acts + " |\n";
    }
    m += "\n";
    if (may(v, "kick")) m += "?[kick NAME reason](cmd:/kick {text})\n";
    if (may(v, "ban")) m += "?[ban NAME 2h reason](cmd:/ban {text})\n";
    if (may(v, "mute")) m += "?[mute NAME 10m](cmd:/mute {text})\n";
    return m;
}

std::string bans_page(const Who &v)
{
    std::string m = "[Back](page:admin)\n";
    const std::vector<Row> rows = all("ban");
    if (rows.empty()) return m + "Nobody is banned.\n";
    m += "| who | until | by | why | |\n";
    for (const Row &r : rows) {
        const std::string arg = r.target.rfind("ip:", 0) == 0 ? r.target.substr(3) : r.label;
        m += "| " + page_text(r.label) + " | " + page_text(r.until ? when_text(r.until) : "for good") +
             " | " + page_text(r.by) + " | " + page_text(r.value) + " |" +
             cmd_link(v, "ban", "unban", "/unban " + arg, arg) + " |\n";
    }
    if (may(v, "ban.ip")) m += "\n?[ban an address: ADDRESS 1d reason](cmd:/banip {text})\n";
    return m;
}

std::string whitelist_page(const Who &v)
{
    const bool  on = whitelist_on();
    std::string m  = "[Back](page:admin)\n";
    m += std::string("The list is ") + (on ? "ON: only the players on it, and staff, come in." :
                                             "off: anybody comes in.") +
         (on ? " [Turn off](cmd:/whitelist off)" : " [Turn on](cmd:/whitelist on)") + "\n";
    m += "## On the list\n";
    const std::vector<Row> rows = all("allow");
    if (rows.empty()) m += "Nobody.\n";
    for (const Row &r : rows)
        m += "| " + page_text(r.label) + " | by " + page_text(r.by) + " |" +
             cmd_link(v, "access", "remove", "/whitelist remove " + r.label, r.label) + " |\n";
    m += "\n?[add NAME](cmd:/whitelist add {text})\n";
    return m;
}

std::string roles_page(const Who &v, const Hooks &h)
{
    std::string m = "[Back](page:admin)\n";
    if (!g_owner.empty()) m += "Owner: " + page_text(g_owner) + " (--owner)\n";
    const std::vector<Row> rows = all("role");
    m += "## Roles given\n";
    if (rows.empty()) m += "None yet.\n";
    for (const Row &r : rows) {
        std::string acts;
        if (outranks(v, resolve(r.label, h)))
            acts = cmd_link(v, "roles", "take away", "/role " + r.label + " player", r.label);
        m += "| " + page_text(r.label) + " | " + r.value + " | by " + page_text(r.by) + " |" +
             acts + " |\n";
    }
    m += "\n?[NAME ROLE](cmd:/role {text})\n";
    m += "> Roles: player, helper, moderator, admin, owner. You give only roles below "
         "your own.\n";
    return m;
}

std::string perms_page(const Who &v)
{
    std::string m = "[Back](page:admin)\n| permission | role | |\n";
    for (const PermRow &p : kPerms) {
        Role r;
        perm_role(p.perm, r);
        std::string acts;
        for (Role to : {Role::Helper, Role::Moderator, Role::Admin, Role::Owner})
            if (to != r && (v.console || to <= role_of(v)))
                acts += cmd_link(v, "perms", role_name(to),
                                 std::string("/perm ") + p.perm + " " + role_name(to), p.perm);
        m += std::string("| ") + p.perm + " | " + role_name(r) +
             (r != p.role ? " (was " + std::string(role_name(p.role)) + ")" : "") + " |" + acts +
             " |\n";
    }
    return m;
}

} // namespace

bool page(const std::string &name, const Who &viewer, const Hooks &hooks, std::string &title,
          std::string &markup)
{
    if (name == "admin" && may(viewer, "pages")) {
        title  = "Server administration";
        markup = admin_page(viewer, hooks);
    } else if (name == "bans" && may(viewer, "ban")) {
        title  = "Bans";
        markup = bans_page(viewer);
    } else if (name == "whitelist" && may(viewer, "access")) {
        title  = "Whitelist";
        markup = whitelist_page(viewer);
    } else if (name == "roles" && may(viewer, "roles")) {
        title  = "Roles";
        markup = roles_page(viewer, hooks);
    } else if (name == "perms" && may(viewer, "perms")) {
        title  = "Permissions";
        markup = perms_page(viewer);
    } else {
        return false;
    }
    return true;
}

void start_console()
{
    static std::once_flag once;
    std::call_once(once, [] {
        // read(2) and not std::getline: a thread blocked inside stdio holds
        // stdin's lock, and exit() takes every stream's lock to flush them, so
        // the server would hang on its way out.
        std::thread([] {
            read_lines([](char *buf, size_t n) { return read_fd(0, buf, n); },
                       [](std::string line) {
                           queue_line(ConsoleLine{std::move(line), console(), nullptr});
                           return true;
                       });
        }).detach();
    });
}

bool start_rcon(int port, const std::string &password, std::string &err)
{
    if (password.empty()) {
        err = "rcon wants a password (--rcon-password, or rcon-password in the config)";
        return false;
    }
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    const Sock l = ::socket(AF_INET, SOCK_STREAM, 0);
    if (l == kNoSock) {
        err = "rcon: no socket";
        return false;
    }
    int yes = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    sockaddr_in at{};
    at.sin_family      = AF_INET;
    at.sin_port        = htons((uint16_t)port);
    at.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // this machine only, always
    if (::bind(l, (const sockaddr *)&at, sizeof(at)) != 0 || ::listen(l, 4) != 0) {
        close_sock(l);
        err = "rcon: cannot listen on 127.0.0.1:" + std::to_string(port);
        return false;
    }
    std::thread([l, password] {
        for (;;) {
            const Sock c = ::accept(l, nullptr, nullptr);
            if (c == kNoSock) continue;
            if (g_rcon_clients >= 4) {   // a few admins, not a crowd
                close_sock(c);
                continue;
            }
            ++g_rcon_clients;
            std::thread(rcon_client, c, password).detach();
        }
    }).detach();
    return true;
}

std::vector<ConsoleLine> take_console_lines()
{
    std::lock_guard<std::mutex> hold(g_console_m);
    std::vector<ConsoleLine>    out;
    out.swap(g_console_lines);
    return out;
}

} // namespace admin
} // namespace cnp
