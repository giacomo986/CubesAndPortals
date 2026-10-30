#include "server/admin.h"

#include "core/net/crypto.h"
#include "core/ui/page.h"

#include <sqlite3.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <climits>
#include <filesystem>
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
//   setting target "whitelist" ("on"/"off"), "maintenance" (its text),
//           "motd" (the line a player reads coming in)
//   addr    the address a player last came from
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
    ");"
    // One row a visit: a player in one room from `joined` to `left` (0 while
    // there). What /seen, /players and --report read.
    "CREATE TABLE IF NOT EXISTS sessions ("
    "  target TEXT NOT NULL,"
    "  name TEXT NOT NULL,"
    "  address TEXT NOT NULL,"
    "  room TEXT NOT NULL,"
    "  joined INTEGER NOT NULL,"
    "  left INTEGER NOT NULL,"
    "  reason TEXT NOT NULL"
    ");"
    // Every block a player changed, for /inspect and /rollback; a month kept.
    "CREATE TABLE IF NOT EXISTS edits ("
    "  room TEXT NOT NULL,"
    "  x INTEGER NOT NULL, y INTEGER NOT NULL, z INTEGER NOT NULL,"
    "  old INTEGER NOT NULL, old_facing INTEGER NOT NULL, new INTEGER NOT NULL,"
    "  target TEXT NOT NULL,"
    "  name TEXT NOT NULL,"
    "  at INTEGER NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS edits_cell ON edits (room, x, y, z);"
    "CREATE INDEX IF NOT EXISTS edits_target ON edits (target, at);"
    "CREATE INDEX IF NOT EXISTS sessions_target ON sessions (target);"
    "CREATE INDEX IF NOT EXISTS admin_log_target ON admin_log (target, action);";

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

// --webhook URL: every line of the admin log, and the server's start, stop
// and reports, POSTed as JSON -- with "content" and "text" for a chat
// service's webhook to show as it is. curl, as the client's update check.
std::string g_webhook;

std::string json_text(const std::string &t)
{
    std::string o = "\"";
    for (const char c : t) {
        if (c == '"' || c == '\\') o += '\\', o += c;
        else if ((unsigned char)c < 0x20) o += ' ';
        else o += c;
    }
    return o + "\"";
}

void webhook(const std::string &event, const std::string &by, const std::string &target,
             const std::string &detail, bool wait = false)
{
    if (g_webhook.empty()) return;
    const std::string line = "[" + event + "] " + by + (target.empty() ? "" : " " + target) +
                             (detail.empty() ? "" : ": " + detail);
    const std::string body = "{\"event\":" + json_text(event) + ",\"by\":" + json_text(by) +
                             ",\"target\":" + json_text(target) + ",\"detail\":" + json_text(detail) +
                             ",\"at\":" + std::to_string(now()) + ",\"content\":" + json_text(line) +
                             ",\"text\":" + json_text(line) + "}";
    const std::string cmd = "curl -fsS --max-time 5 -H \"Content-Type: application/json\" --data-binary @- \"" +
                            g_webhook + "\" >" +
#ifdef _WIN32
                            "NUL 2>NUL";
#else
                            "/dev/null 2>&1";
#endif
    auto post = [cmd, body] {
        if (FILE *pipe = popen(cmd.c_str(), "w")) {
            std::fwrite(body.data(), 1, body.size(), pipe);
            pclose(pipe);
        }
    };
    if (wait) post();
    else std::thread(post).detach();
}

void log(const Who &by, const std::string &action, const std::string &target,
         const std::string &detail)
{
    webhook(action, by.name, target, detail);
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

// Above the owner: a permission put there is nobody's (/perm creative
// nobody). Only ever a permission's role, never a player's -- parse_role does
// not know the word, so /role cannot hand it out.
constexpr Role kNobody = (Role)((int)Role::Owner + 1);

struct PermRow {
    const char *perm;
    Role        role;
};
const PermRow kPerms[] = {
    {"mute", Role::Helper},        {"kick", Role::Moderator}, {"ban", Role::Moderator},
    {"creative", Role::Moderator}, {"pages", Role::Helper},   {"ban.ip", Role::Admin},
    {"access", Role::Admin},       {"roles", Role::Admin},    {"perms", Role::Owner},
    {"restart", Role::Admin},      {"maintenance", Role::Admin}, {"motd", Role::Admin},
    {"announce", Role::Moderator}, {"staffchat", Role::Helper}, {"warn", Role::Helper},
    {"reports", Role::Helper},     {"notes", Role::Helper},   {"players", Role::Helper},
    {"whois", Role::Moderator},    {"logs", Role::Admin},     {"backup", Role::Admin},
    {"portals", Role::Admin},      {"federation", Role::Owner},
    {"inspect", Role::Helper},     {"rollback", Role::Moderator},
    {"bag", Role::Moderator},      {"give", Role::Admin},     {"rules", Role::Admin},
    {"alts", Role::Moderator},
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
        for (const Row &r : all("role")) {
            const size_t at = r.target.find('@');
            out.push_back(r.label + ": " + r.value +
                          (at == std::string::npos ? "" : " in " + r.target.substr(at + 1)) +
                          " (by " + r.by + ", " + when_text(r.at) + ")");
        }
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
    const std::string room = a.words.size() > 2 ? a.words[2] : "";
    const std::string at   = room.empty() ? "" : "@" + room;
    if (want == Role::Player) {
        drop("role", target_of(t) + at);
    } else {
        put("role", target_of(t) + at, t.name, role_name(want), c.name, 0);
    }
    log(c, "role", t.name, role_name(want) + (room.empty() ? "" : " in " + room));
    out.push_back(a.name + " is now " + role_name(want) + (room.empty() ? "" : " in " + room) + ".");
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
    // A friend server's ban lifted here stays lifted when the friend sends
    // its list again.
    if (Row r; find("ban", target, r) && r.by.rfind("friend:", 0) == 0)
        put("fedskip", target, r.label, r.by, c.name, 0);
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
    bool any = false;
    for (const std::string &t : targets_of(resolve(a.name, h))) any = drop("mute", t) || any;
    if (!any) {
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
    const bool nobody = a.words.size() >= 2 && a.words[1] == "nobody";
    if (nobody) want = kNobody;
    if (!known || a.words.size() < 2 || (!nobody && !parse_role(a.words[1], want))) {
        out.push_back("/perm PERMISSION ROLE -- /perm alone lists them; ROLE nobody shuts it to all");
        return;
    }
    // Nobody hands a permission down past where they could still use it --
    // shutting one to everybody takes nothing from anyone it was not for.
    if (!c.console && !nobody && want > role_of(c)) {
        out.push_back("You cannot put a permission above your own role.");
        return;
    }
    if (want == known->role) drop("perm", a.name);
    else put("perm", a.name, a.name, role_name(want), c.name, 0);
    log(c, "perm", a.name, role_name(want));
    out.push_back(a.name + " now wants " + role_name(want) + ".");
}


// "in 5 minutes", for a sentence.
std::string left_text(int64_t s)
{
    if (s >= 3600 && s % 3600 == 0) return std::to_string(s / 3600) + (s == 3600 ? " hour" : " hours");
    if (s >= 60 && s % 60 == 0) return std::to_string(s / 60) + (s == 60 ? " minute" : " minutes");
    return std::to_string(s) + (s == 1 ? " second" : " seconds");
}

std::string setting(const std::string &name)
{
    Row r;
    return find("setting", name, r) ? r.value : "";
}

// Every word from the `from`th on, as one line.
std::string rest(const Args &a, size_t from)
{
    std::string t;
    for (size_t i = from; i < a.words.size(); ++i) t += (t.empty() ? "" : " ") + a.words[i];
    return t;
}

// A restart or a stop the server is counting down to, and the daily one.
struct Pending {
    int64_t     at = 0;   // 0: nothing
    bool        restart = false;
    std::string reason;
    int64_t     warned = INT64_MAX;   // the last warning's threshold
};
Pending g_pending;

std::vector<std::string> g_world_files;   // --backup-every, /backup
int64_t                  g_backup_every = 0, g_next_backup = 0;
int                      g_backup_keep  = 5;
std::atomic<bool>        g_backing_up{false};
int     g_daily      = -1;   // --restart-at, minutes after midnight
int64_t g_next_daily = 0;

int64_t next_daily(int64_t from)
{
    const std::time_t t  = (std::time_t)from;
    std::tm           tm = *std::localtime(&t);
    tm.tm_hour = g_daily / 60, tm.tm_min = g_daily % 60, tm.tm_sec = 0, tm.tm_isdst = -1;
    int64_t at = (int64_t)std::mktime(&tm);
    if (at <= from) {
        ++tm.tm_mday;
        at = (int64_t)std::mktime(&tm);
    }
    return at;
}

std::string going_text(const Pending &p, int64_t left)
{
    return std::string("The server ") + (p.restart ? "restarts" : "stops") +
           (left > 0 ? " in " + left_text(left) : std::string(" now")) +
           (p.reason.empty() ? "." : ": " + p.reason + ".");
}

// /restart and /stop: [time|now|cancel] [reason]. A minute when no time.
void schedule(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out, bool restart)
{
    const char *what = restart ? "restart" : "stop";
    if (a.name == "cancel") {
        if (!g_pending.at) {
            out.push_back("Nothing is scheduled.");
            return;
        }
        // A cancelled daily restart is tomorrow's now.
        if (g_daily >= 0 && g_pending.at >= g_next_daily) g_next_daily = next_daily(g_pending.at);
        g_pending = {};
        log(c, "cancel", what, "");
        if (h.notice) h.notice("The server is staying up after all.");
        out.push_back("Cancelled.");
        return;
    }
    int64_t secs = 60;
    size_t  from = 0;
    if (a.name == "now") {
        secs = 0, from = 1;
    } else if (!a.words.empty() && parse_duration(a.words[0], secs)) {
        from = 1;
    }
    g_pending = Pending{now() + secs, restart, rest(a, from), INT64_MAX};
    log(c, what, "", (secs ? "in " + left_text(secs) : std::string("now")) +
                         (g_pending.reason.empty() ? "" : ", " + g_pending.reason));
    out.push_back(going_text(g_pending, secs));
}

void cmd_restart(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    schedule(c, a, h, out, true);
}

void cmd_stop(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    schedule(c, a, h, out, false);
}

// /maintenance on [text] | off: only staff come in, and the rest are put out.
void cmd_maintenance(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.name == "off") {
        if (!drop("setting", "maintenance")) {
            out.push_back("The server is not in maintenance.");
            return;
        }
        log(c, "maintenance", "off", "");
        out.push_back("Open again.");
        return;
    }
    if (a.name == "on") {
        std::string text = rest(a, 1);
        if (text.empty()) text = "back soon";
        put("setting", "maintenance", "maintenance", text, c.name, 0);
        log(c, "maintenance", "on", text);
        if (h.online)
            for (const Online &o : h.online())
                if (role_of(o.who) < Role::Helper) h.remove(o.peer, net::RefuseReason::Maintenance, text);
        out.push_back("In maintenance: only staff come in (" + text + ").");
        return;
    }
    const std::string m = setting("maintenance");
    out.push_back(m.empty() ? "Not in maintenance. /maintenance on [text] | off"
                            : "In maintenance: " + m + ". /maintenance off opens it.");
}

void cmd_motd(const Who &c, const Args &a, const Hooks &, std::vector<std::string> &out)
{
    if (a.words.empty() || !may(c, "motd")) {
        const std::string m = setting("motd");
        out.push_back(m.empty() ? "No message of the day." : m);
        return;
    }
    if (a.name == "off") {
        drop("setting", "motd");
        log(c, "motd", "off", "");
        out.push_back("No message of the day now.");
        return;
    }
    const std::string text = rest(a, 0);
    put("setting", "motd", "motd", text, c.name, 0);
    log(c, "motd", "", text);
    out.push_back("Message of the day: " + text);
}

// The rules: shown on a page to each player who has not accepted these ones
// (the time they were set is their version), who cannot speak or build
// until they /accept. Staff are taken to have.
void cmd_rules(const Who &c, const Args &a, const Hooks &, std::vector<std::string> &out)
{
    if (a.name == "set" && may(c, "rules")) {
        const std::string text = rest(a, 1);
        if (text.empty()) {
            out.push_back("/rules set TEXT");
            return;
        }
        set_rules(text);
        log(c, "rules", "", text);
        out.push_back("New rules; everybody accepts them again.");
        return;
    }
    if (a.name == "off" && may(c, "rules")) {
        drop("setting", "rules");
        log(c, "rules", "off", "");
        out.push_back("No rules now.");
        return;
    }
    const std::string r = setting("rules");
    out.push_back(r.empty() ? "This server has no rules to accept." : r);
    if (!r.empty() && !accepted(c)) out.push_back("/accept them, or /leave.");
}

void cmd_accept(const Who &c, const Args &, const Hooks &, std::vector<std::string> &out)
{
    Row r;
    if (!find("setting", "rules", r)) {
        out.push_back("There are no rules to accept.");
        return;
    }
    put("accepted", target_of(c), c.name, std::to_string(r.at), "", 0);
    log(c, "accept", "", "");
    out.push_back("Thank you. Have fun.");
}

void cmd_leave(const Who &c, const Args &, const Hooks &h, std::vector<std::string> &out)
{
    net::PeerId peer = net::kNoPeer;
    resolve(c.name, h, &peer);
    if (peer != net::kNoPeer && h.remove) h.remove(peer, net::RefuseReason::Kicked, "you did not accept the rules");
    out.push_back("Goodbye.");
}

void cmd_announce(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const std::string text = rest(a, 0);
    if (text.empty()) {
        out.push_back("/announce TEXT");
        return;
    }
    if (h.notice) h.notice(text);
    log(c, "announce", "", text);
    out.push_back("Announced.");
}

// What people with a role at least `least` hear: staff chat, reports.
void tell_staff(const Hooks &h, const std::string &line, Role least = Role::Helper)
{
    if (h.online && h.say)
        for (const Online &o : h.online())
            if (role_of(o.who) >= least) h.say(o.peer, line);
    std::printf("        %s\n", line.c_str());
}

void cmd_sc(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const std::string text = rest(a, 0);
    if (text.empty()) {
        out.push_back("/sc TEXT -- to the staff online");
        return;
    }
    tell_staff(h, "[staff] " + c.name + ": " + text);
}

// The log rows about one player of an action: warns, notes, reports.
std::vector<Row> log_rows(const std::string &target, const char *action, int64_t since = 0)
{
    std::vector<Row> rows;
    query("SELECT target, '', detail, by, at, 0 FROM admin_log WHERE target = ?1 AND action = ?2 "
          "AND at >= ?3 ORDER BY at",
          {target, action, std::to_string(since)}, [&](sqlite3_stmt *st) { rows.push_back(read_row(st)); });
    return rows;
}

constexpr int     kWarnsToBan  = 3;
constexpr int64_t kWarnWindow  = 30 * 86400;
constexpr int64_t kWarnBanTime = 86400;

// /warn NAME reason: three in thirty days are a day's ban.
void cmd_warn(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    net::PeerId peer = net::kNoPeer;
    const Who   t    = resolve(a.name, h, &peer);
    if (a.name.empty() || !outranks(c, t)) {
        out.push_back(a.name.empty() ? "/warn NAME reason" : "You cannot warn " + a.name + ".");
        return;
    }
    const std::string why = rest(a, 1);
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, ?2, 'warn', ?3, ?4)",
          {std::to_string(now()), c.name, target_of(t), why});
    const int n = (int)log_rows(target_of(t), "warn", now() - kWarnWindow).size();
    if (peer != net::kNoPeer && h.say)
        h.say(peer, "You are warned" + (why.empty() ? std::string() : ": " + why) + " (" +
                        std::to_string(n) + " of " + std::to_string(kWarnsToBan) + ").");
    out.push_back(a.name + " is warned (" + std::to_string(n) + " in 30 days).");
    if (n < kWarnsToBan) return;
    const int64_t until = now() + kWarnBanTime;
    put("ban", target_of(t), t.name, std::to_string(n) + " warnings", "warns", until);
    if (!t.has_identity && !t.address.empty())
        put("ban", "ip:" + t.address, t.name + " (" + t.address + ")", std::to_string(n) + " warnings",
            "warns", until);
    log(c, "ban", t.name, until_text(until) + ", " + std::to_string(n) + " warnings");
    for (const Online &o : online_matching(target_of(t), h))
        h.remove(o.peer, net::RefuseReason::Banned, std::to_string(n) + " warnings, " + until_text(until));
    out.push_back(a.name + " is banned for a day: " + std::to_string(n) + " warnings.");
}

// /report NAME what happened: for the staff, now if any is online.
void cmd_report(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const std::string what = rest(a, 1);
    if (a.name.empty() || what.empty()) {
        out.push_back("/report NAME what happened");
        return;
    }
    const Who t = resolve(a.name, h);
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, ?2, 'report', ?3, ?4)",
          {std::to_string(now()), c.name, target_of(t), a.name + ": " + what});
    tell_staff(h, "[report] " + c.name + " about " + a.name + ": " + what);
    webhook("report", c.name, a.name, what);
    out.push_back("Reported. Thank you.");
}

void cmd_reports(const Who &, const Args &, const Hooks &, std::vector<std::string> &out)
{
    query("SELECT at, by, detail FROM admin_log WHERE action = 'report' ORDER BY at DESC LIMIT 20", {},
          [&](sqlite3_stmt *st) {
              out.push_back(when_text(sqlite3_column_int64(st, 0)) + " " + col(st, 1) + " about " +
                            col(st, 2));
          });
    if (out.empty()) out.push_back("No reports.");
}

void cmd_note(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const std::string text = rest(a, 1);
    if (a.name.empty() || text.empty()) {
        out.push_back("/note NAME text -- for the staff, in /whois");
        return;
    }
    const Who t = resolve(a.name, h);
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, ?2, 'note', ?3, ?4)",
          {std::to_string(now()), c.name, target_of(t), text});
    out.push_back("Noted.");
}

std::string span_text(int64_t s)
{
    if (s >= 86400) return std::to_string(s / 86400) + "d " + std::to_string(s % 86400 / 3600) + "h";
    if (s >= 3600) return std::to_string(s / 3600) + "h " + std::to_string(s % 3600 / 60) + "m";
    return std::to_string(s / 60) + "m";
}

// /seen NAME: online, or when they were last, and how much they have played.
// Edits wait here and go to the file together, every few seconds: one
// transaction a dig would be a disk write a block.
struct Edit {
    std::string room, target, name;
    int         x, y, z, old, old_facing, now_block;
    int64_t     at;
};
std::vector<Edit> g_edits;

void flush_edits()
{
    if (g_edits.empty() || !g_db) return;
    sqlite3_exec(g_db, "BEGIN", nullptr, nullptr, nullptr);
    for (const Edit &e : g_edits)
        query("INSERT INTO edits (room, x, y, z, old, old_facing, new, target, name, at) "
              "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
              {e.room, std::to_string(e.x), std::to_string(e.y), std::to_string(e.z), std::to_string(e.old),
               std::to_string(e.old_facing), std::to_string(e.now_block), e.target, e.name,
               std::to_string(e.at)});
    sqlite3_exec(g_db, "COMMIT", nullptr, nullptr, nullptr);
    g_edits.clear();
}

std::string block_text(const Hooks &h, const std::string &room, int id)
{
    const std::string n = h.block_name ? h.block_name(room, id) : "";
    return n.empty() ? "#" + std::to_string(id) : n;
}

void cmd_inspect(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    int x, y, z;
    if (a.words.size() < 3 || std::sscanf((a.words[0] + " " + a.words[1] + " " + a.words[2]).c_str(), "%d %d %d", &x, &y, &z) != 3) {
        out.push_back("/inspect X Y Z [ROOM]");
        return;
    }
    const std::string room = a.words.size() > 3 ? a.words[3] : c.room;
    flush_edits();
    query("SELECT name, old, new, at FROM edits WHERE room = ?1 AND x = ?2 AND y = ?3 AND z = ?4 "
          "ORDER BY at DESC, rowid DESC LIMIT 10",
          {room, std::to_string(x), std::to_string(y), std::to_string(z)}, [&](sqlite3_stmt *st) {
              out.push_back(col(st, 0) + ": " + block_text(h, room, sqlite3_column_int(st, 1)) + " to " +
                            block_text(h, room, sqlite3_column_int(st, 2)) + ", " +
                            when_text(sqlite3_column_int64(st, 3)));
          });
    if (out.empty()) out.push_back("No player changed " + a.words[0] + " " + a.words[1] + " " + a.words[2] + " this month.");
}

void cmd_rollback(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.name.empty() || a.seconds <= 0) {
        out.push_back("/rollback NAME DURATION [ROOM] -- e.g. /rollback bob 2h");
        return;
    }
    const Who t = resolve(a.name, h);
    if (!outranks(c, t)) {
        out.push_back("You cannot roll back " + a.name + ".");
        return;
    }
    flush_edits();
    const std::string since = std::to_string(now() - a.seconds);
    // Each cell back to what it was before their first change in the time,
    // if it still holds what they left there: somebody else's later change
    // stays.
    std::map<std::tuple<std::string, int, int, int>, Restore> cells;
    query("SELECT room, x, y, z, old, old_facing, new FROM edits WHERE target = ?1 AND at >= ?2 "
          "ORDER BY at, rowid",
          {target_of(t), since}, [&](sqlite3_stmt *st) {
              const std::string room = col(st, 0);
              if (!a.reason.empty() && room != a.reason) return;
              const auto key = std::make_tuple(room, sqlite3_column_int(st, 1), sqlite3_column_int(st, 2),
                                               sqlite3_column_int(st, 3));
              auto it = cells.find(key);
              if (it == cells.end())
                  it = cells.emplace(key, Restore{room, std::get<1>(key), std::get<2>(key), std::get<3>(key),
                                                  sqlite3_column_int(st, 4), sqlite3_column_int(st, 5), 0}).first;
              it->second.expect = sqlite3_column_int(st, 6);
          });
    std::vector<Restore> list;
    for (const auto &kv : cells) list.push_back(kv.second);
    if (list.empty()) {
        out.push_back(a.name + " changed nothing in the last " + duration_text(a.seconds) + ".");
        return;
    }
    if (h.restore) h.restore(list);
    query("DELETE FROM edits WHERE target = ?1 AND at >= ?2" , {target_of(t), since});
    log(c, "rollback", t.name, duration_text(a.seconds) + ", " + std::to_string(list.size()) + " cell(s)");
    out.push_back(std::to_string(list.size()) + " cell(s) " + a.name + " changed in the last " +
                  duration_text(a.seconds) + " go back as they were (a cell changed since stays).");
}

void cmd_seen(const Who &, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.name.empty()) {
        out.push_back("/seen NAME");
        return;
    }
    if (h.online)
        for (const Online &o : h.online())
            if (o.who.name == a.name) {
                out.push_back(a.name + " is online" + (o.room.empty() ? "" : " in " + o.room) + ".");
                return;
            }
    const Who t = resolve(a.name, h);
    int64_t   last = 0, first = 0, total = 0, visits = 0;
    std::string room;
    query("SELECT MAX(left), MIN(joined), SUM(left - joined), COUNT(*) FROM sessions "
          "WHERE target = ?1 AND left > 0",
          {target_of(t)}, [&](sqlite3_stmt *st) {
              last   = sqlite3_column_int64(st, 0);
              first  = sqlite3_column_int64(st, 1);
              total  = sqlite3_column_int64(st, 2);
              visits = sqlite3_column_int64(st, 3);
          });
    if (!visits) {
        out.push_back("Never seen " + a.name + " here.");
        return;
    }
    query("SELECT room FROM sessions WHERE target = ?1 AND left = ?2", {target_of(t), std::to_string(last)},
          [&](sqlite3_stmt *st) { room = col(st, 0); });
    out.push_back(a.name + " was last here " + when_text(last) + (room.empty() ? "" : " in " + room) +
                  "; " + std::to_string(visits) + " visits since " + when_text(first) + ", " +
                  span_text(total) + " in all.");
}

// /players [7d]: who came in the last week, or in the time given.
void cmd_players(const Who &, const Args &a, const Hooks &, std::vector<std::string> &out)
{
    int64_t span = 7 * 86400;
    if (!a.name.empty() && !parse_duration(a.name, span)) {
        out.push_back("/players [30m|2h|7d|1w]");
        return;
    }
    const int64_t t = now();
    query("SELECT name, COUNT(*), SUM((CASE WHEN left = 0 THEN ?2 ELSE left END) - joined), "
          "MAX(CASE WHEN left = 0 THEN ?2 ELSE left END), MIN(left) FROM sessions "
          "WHERE joined >= ?1 OR left = 0 OR left >= ?1 GROUP BY target ORDER BY 4 DESC LIMIT 30",
          {std::to_string(t - span), std::to_string(t)}, [&](sqlite3_stmt *st) {
              const bool on = sqlite3_column_int64(st, 4) == 0;
              out.push_back(col(st, 0) + ": " + std::to_string(sqlite3_column_int64(st, 1)) +
                            " visits, " + span_text(sqlite3_column_int64(st, 2)) +
                            (on ? ", online" : ", last " + when_text(sqlite3_column_int64(st, 3))));
          });
    if (out.empty()) out.push_back("Nobody in that time.");
    else out.insert(out.begin(), std::to_string(out.size()) + " players in " + duration_text(span) + ":");
}

// /whois NAME: what the staff knows about a player.
void cmd_bag(const Who &, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.name.empty()) {
        out.push_back("/bag NAME");
        return;
    }
    const Who t = resolve(a.name, h);
    Row       away;
    if (t.has_identity && find("bagaway", key_target(t.identity), away))
        out.push_back("Their bag went to " + away.label + " " + when_text(away.at) + "; this is an old copy.");
    if (h.bag_lines)
        for (const std::string &l : h.bag_lines(t)) out.push_back(l);
    if (t.has_identity && !hidden(t.identity).empty())
        out.push_back("And things this world does not know, kept aside for another.");
    if (out.empty()) out.push_back(a.name + " carries nothing here.");
}

void cmd_give(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.words.size() < 2) {
        out.push_back("/give NAME ITEM [COUNT]");
        return;
    }
    net::PeerId peer = net::kNoPeer;
    const Who   t    = resolve(a.name, h, &peer);
    const int   n    = a.words.size() > 2 ? std::max(1, std::min(9999, std::atoi(a.words[2].c_str()))) : 1;
    if (peer == net::kNoPeer) {
        out.push_back("Nobody called '" + a.name + "' is online.");
        return;
    }
    const std::string why = h.give ? h.give(peer, a.words[1], n) : "not on this server";
    if (!why.empty()) {
        out.push_back(why);
        return;
    }
    log(c, "give", t.name, std::to_string(n) + " " + a.words[1]);
    out.push_back(t.name + " gets " + std::to_string(n) + " " + a.words[1] + ".");
}

// Other players who came in from an address this one came from: the same
// person under another key, or a household. `banned_only` for the warning
// to the staff when one comes in.
std::vector<std::string> alts_of(const Who &t, bool banned_only)
{
    std::vector<std::string> out;
    const std::string        target = target_of(t);
    std::set<std::string>    addrs;
    query("SELECT DISTINCT address FROM sessions WHERE target = ?1 AND address != ''", {target},
          [&](sqlite3_stmt *st) { addrs.insert(col(st, 0)); });
    if (!t.address.empty()) addrs.insert(t.address);
    std::map<std::string, std::pair<std::string, std::string>> seen;   // target -> name, address
    for (const std::string &ad : addrs)
        query("SELECT target, name FROM sessions WHERE address = ?1 AND target != ?2 GROUP BY target",
              {ad, target}, [&](sqlite3_stmt *st) { seen[col(st, 0)] = {col(st, 1), ad}; });
    for (const auto &[other, na] : seen) {
        Row        ban;
        const bool banned = find("ban", other, ban);
        if (banned_only && !banned) continue;
        out.push_back(na.first + (banned ? " (banned)" : "") + ", from " + na.second);
    }
    return out;
}

void cmd_alts(const Who &, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.name.empty()) {
        out.push_back("/alts NAME");
        return;
    }
    for (const std::string &l : alts_of(resolve(a.name, h), false)) out.push_back(l);
    if (out.empty()) out.push_back("Nobody else came in from where " + a.name + " did.");
}

void cmd_whois(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (a.name.empty()) {
        out.push_back("/whois NAME");
        return;
    }
    const Who         t      = resolve(a.name, h);
    const std::string target = target_of(t);
    out.push_back(a.name + ": " + role_name(role_of(t)) + ", " +
                  (t.has_identity ? "key " + target.substr(4, 16) : std::string("no identity")));
    std::string names, addrs;
    query("SELECT DISTINCT name FROM sessions WHERE target = ?1", {target},
          [&](sqlite3_stmt *st) { names += (names.empty() ? "" : ", ") + col(st, 0); });
    query("SELECT address FROM sessions WHERE target = ?1 GROUP BY address ORDER BY MAX(joined) DESC "
          "LIMIT 5",
          {target}, [&](sqlite3_stmt *st) { addrs += (addrs.empty() ? "" : ", ") + col(st, 0); });
    query("SELECT MIN(joined) FROM sessions WHERE target = ?1", {target}, [&](sqlite3_stmt *st) {
        if (sqlite3_column_int64(st, 0))
            out.push_back("First seen " + when_text(sqlite3_column_int64(st, 0)) + " as " + names + ".");
    });
    if (may(c, "ban.ip") && !addrs.empty()) out.push_back("Addresses: " + addrs + ".");
    Row r;
    if (find("ban", target, r)) out.push_back("Banned " + until_text(r.until) + " by " + r.by + ": " + r.value);
    if (find("mute", target, r)) out.push_back("Muted " + until_text(r.until) + " by " + r.by + ": " + r.value);
    for (const char *kind : {"warn", "note", "report"})
        for (const Row &w : log_rows(target, kind))
            out.push_back(std::string(kind) + " " + when_text(w.at) + " by " + w.by + ": " + w.value);
    query("SELECT at, by, action, detail FROM admin_log WHERE target = ?1 AND action IN ('ban', 'kick', "
          "'mute') ORDER BY at DESC LIMIT 5",
          {t.name}, [&](sqlite3_stmt *st) {
              out.push_back(col(st, 2) + " " + when_text(sqlite3_column_int64(st, 0)) + " by " +
                            col(st, 1) + ": " + col(st, 3));
          });
}

// /logs [N] [ACTION]: the latest admin actions.
void cmd_logs(const Who &, const Args &a, const Hooks &, std::vector<std::string> &out)
{
    int         n      = 15;
    std::string action = "%";
    for (const std::string &w : a.words) {
        if (!w.empty() && std::isdigit((unsigned char)w[0])) n = std::clamp(std::atoi(w.c_str()), 1, 200);
        else action = w;
    }
    std::vector<std::string> rows;
    query("SELECT at, by, action, target, detail FROM admin_log WHERE action LIKE ?1 "
          "ORDER BY at DESC, rowid DESC LIMIT ?2",
          {action, std::to_string(n)}, [&](sqlite3_stmt *st) {
              const std::string tgt = col(st, 3);
              rows.push_back(when_text(sqlite3_column_int64(st, 0)) + " " + col(st, 1) + " " + col(st, 2) +
                             (tgt.empty() ? "" : " " + tgt.substr(0, 40)) +
                             (col(st, 4).empty() ? "" : " -- " + col(st, 4)));
          });
    out.assign(rows.rbegin(), rows.rend());
    if (out.empty()) out.push_back("Nothing logged.");
}

// /portals: where this server's portals lead, and who sends players here.
void cmd_portals(const Who &, const Args &, const Hooks &h, std::vector<std::string> &out)
{
    if (h.portals) out = h.portals();
    if (out.empty()) out.push_back("No links out (--portal-to).");
    const std::vector<Row> in = all("inbound");
    if (in.empty()) out.push_back("Nobody has come in through a portal from another server.");
    for (const Row &r : in)
        out.push_back("In from server " + r.target.substr(4, 16) + ": " + r.value + " crossings, last " +
                      when_text(r.at) + " from " + r.label + ".");
}

void cmd_metrics(const Who &, const Args &, const Hooks &h, std::vector<std::string> &out)
{
    if (h.metrics) out = h.metrics();
    if (out.empty()) out.push_back("No numbers from this server.");
}

void cmd_backup(const Who &c, const Args &, const Hooks &, std::vector<std::string> &out)
{
    std::string err;
    if (!backup(err)) {
        out.push_back("No backup: " + err);
        return;
    }
    log(c, "backup", "", "");
    out.push_back("Backing up the worlds; the log says when it is done.");
}

// Federation, below the sockets.
void cmd_friend(const Who &, const Args &, const Hooks &, std::vector<std::string> &);
void cmd_fed(const Who &, const Args &, const Hooks &, std::vector<std::string> &);
void cmd_where(const Who &, const Args &, const Hooks &, std::vector<std::string> &);

const Command kCommands[] = {
    {"perms", nullptr, "-- your role here", cmd_perms},
    {"role", "roles", "[NAME [ROLE [ROOM]]] -- see or give roles, in one room only with ROOM", cmd_role},
    {"kick", "kick", "NAME [reason] -- put a player out", cmd_kick},
    {"ban", "ban", "NAME [30m|2h|7d] [reason] -- keep a player out", cmd_ban},
    {"banip", "ban.ip", "NAME|ADDRESS [time] [reason] -- keep an address out", cmd_banip},
    {"unban", "ban", "NAME|ADDRESS", cmd_unban},
    {"bans", "ban", "-- who is banned", cmd_bans},
    {"mute", "mute", "NAME [time] [reason] -- hold a player's chat", cmd_mute},
    {"unmute", "mute", "NAME", cmd_unmute},
    {"whitelist", "access", "on|off|list|add NAME|remove NAME", cmd_whitelist},
    {"perm", "perms", "[PERMISSION ROLE] -- which role each permission wants", cmd_perm},
    {"restart", "restart", "[5m|now|cancel] [reason] -- restart, warning everybody", cmd_restart},
    {"stop", "restart", "[5m|now|cancel] [reason] -- stop the server, warning everybody", cmd_stop},
    {"maintenance", "maintenance", "on [text]|off -- only staff come in", cmd_maintenance},
    {"motd", nullptr, "[TEXT|off] -- the message of the day", cmd_motd},
    {"announce", "announce", "TEXT -- a notice on every screen", cmd_announce},
    {"rules", nullptr, "[set TEXT|off] -- the rules everybody accepts", cmd_rules},
    {"accept", nullptr, "-- accept the rules", cmd_accept},
    {"leave", nullptr, "-- leave, not accepting the rules", cmd_leave},
    {"sc", "staffchat", "TEXT -- to the staff online", cmd_sc},
    {"warn", "warn", "NAME reason -- 3 in 30 days ban for a day", cmd_warn},
    {"report", nullptr, "NAME what happened -- to the staff", cmd_report},
    {"reports", "reports", "-- the latest reports", cmd_reports},
    {"note", "notes", "NAME text -- a note for the staff", cmd_note},
    {"bag", "bag", "NAME -- what a player carries", cmd_bag},
    {"give", "give", "NAME ITEM [COUNT] -- into a player's bag", cmd_give},
    {"inspect", "inspect", "X Y Z [ROOM] -- who changed that block", cmd_inspect},
    {"rollback", "rollback", "NAME DURATION [ROOM] -- put back what a player changed", cmd_rollback},
    {"seen", nullptr, "NAME -- when a player was last here", cmd_seen},
    {"players", "players", "[7d] -- who came in lately", cmd_players},
    {"whois", "whois", "NAME -- what the staff knows of a player", cmd_whois},
    {"alts", "alts", "NAME -- other players from the same addresses", cmd_alts},
    {"logs", "logs", "[N] [ACTION] -- the latest admin actions", cmd_logs},
    {"metrics", "logs", "-- how the server is doing", cmd_metrics},
    {"backup", "backup", "-- copy every world file now", cmd_backup},
    {"portals", "portals", "-- links out, and who sends players in", cmd_portals},
    {"friend", "federation", "add NAME HOST:PORT KEY [bans,presence,chat,bags]|remove NAME|list -- friend servers", cmd_friend},
    {"fed", nullptr, "TEXT -- to the players of every friend server", cmd_fed},
    {"where", nullptr, "NAME -- here, or on which friend server", cmd_where},
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
    if (r == kNobody) return "nobody";
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
            if (find("perm", perm, r) && r.value == "nobody") out = kNobody;
            else if (!(find("perm", perm, r) && parse_role(r.value, out))) out = p.role;
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
    // Visits a crash left open end where the last thing was heard of them.
    int open_visits = 0;
    query("SELECT COUNT(*) FROM sessions WHERE left = 0", {},
          [&](sqlite3_stmt *st) { open_visits = sqlite3_column_int(st, 0); });
    if (open_visits) {
        query("UPDATE sessions SET left = joined, reason = 'server went down' WHERE left = 0", {});
        query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, 'server', 'unclean', '', ?2)",
              {std::to_string(now()), std::to_string(open_visits) + " visits left open"});
    }
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, 'server', 'start', '', '')",
          {std::to_string(now())});
    webhook(open_visits ? "unclean start" : "start", "server", "", open_visits ? "the last run did not stop cleanly" : "");
    query("DELETE FROM edits WHERE at < ?1", {std::to_string(now() - 30 * 86400)});
    return true;
}

void close()
{
    flush_edits();
    if (g_db) webhook("stop", "server", "", "", true);
    query("UPDATE sessions SET left = ?1, reason = 'server stopped' WHERE left = 0", {std::to_string(now())});
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, 'server', 'stop', '', '')",
          {std::to_string(now())});
    if (g_db) sqlite3_close(g_db);
    g_db = nullptr;
}

void set_owner(const std::string &name) { g_owner = name; }

void default_perm(const std::string &perm, const std::string &role)
{
    Row r;
    if (!find("perm", perm, r)) put("perm", perm, perm, role, "server", 0);
}

bool set_webhook(const std::string &url)
{
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return false;
    for (const char c : url)
        if (!std::isalnum((unsigned char)c) && std::strchr(":/?&=._~%#+-@", c) == nullptr) return false;
    g_webhook = url;
    return true;
}

void edited(const Who &who, const std::string &room, int x, int y, int z, int old_block, int old_facing,
            int new_block)
{
    if (old_block == new_block) return;
    g_edits.push_back({room, target_of(who), who.name, x, y, z, old_block, old_facing, new_block, now()});
}

Role role_of(const Who &w)
{
    if (w.console) return Role::Owner;
    if (!w.has_identity) return Role::Guest;
    if (!g_owner.empty() && w.name == g_owner) return Role::Owner;
    Row  r;
    Role role;
    Role best = Role::Player;
    if (find("role", key_target(w.identity), r) && parse_role(r.value, role)) best = role;
    // /role NAME ROLE ROOM: that role in that room only, never less than the rest.
    if (!w.room.empty() && find("role", key_target(w.identity) + "@" + w.room, r) &&
        parse_role(r.value, role) && role > best)
        best = role;
    return best;
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
    if (const std::string m = setting("maintenance"); !m.empty() && role_of(w) < Role::Helper) {
        reason = net::RefuseReason::Maintenance;
        why    = m;
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
    // By key, or by the name it was given under before anybody owned it (as a
    // ban is), or by address.
    for (const std::string &t : targets_of(w)) {
        Row r;
        if (!find("mute", t, r)) continue;
        why = "You are muted " + until_text(r.until) + (r.value.empty() ? "" : ": " + r.value) + ".";
        return true;
    }
    return false;
}

bool restart_at(const std::string &hhmm)
{
    int h = -1, m = -1;
    char extra = 0;
    if (std::sscanf(hhmm.c_str(), "%d:%d%c", &h, &m, &extra) != 2 || h < 0 || h > 23 || m < 0 || m > 59)
        return false;
    g_daily      = h * 60 + m;
    g_next_daily = next_daily(now());
    return true;
}

Stop tick(const Hooks &h)
{
    const int64_t t = now();
    static int64_t next_flush = 0;
    if (t >= next_flush) {
        next_flush = t + 5;
        flush_edits();
    }
    if (g_backup_every > 0 && t >= g_next_backup) {
        g_next_backup = t + g_backup_every;
        std::string err;
        if (!backup(err)) std::printf("        backup: %s\n", err.c_str());
    }
    if (g_daily >= 0 && !g_pending.at && g_next_daily - t <= 600)
        g_pending = Pending{g_next_daily, true, "the daily restart", INT64_MAX};
    if (!g_pending.at) return Stop::None;

    const int64_t left = g_pending.at - t;
    if (left > 0) {
        // Once as each of these is crossed: the bucket the time left is in.
        static const int64_t kWarn[] = {1, 2, 3, 4, 5, 10, 30, 60, 120, 300, 600, 1800, 3600};
        for (const int64_t w : kWarn)
            if (left <= w) {
                if (w < g_pending.warned) {
                    g_pending.warned = w;
                    if (h.notice) h.notice(going_text(g_pending, left));
                }
                break;
            }
        return Stop::None;
    }
    const Pending p = g_pending;
    g_pending       = {};
    const std::string text = p.reason.empty() ? (p.restart ? "back in a moment" : "stopped") : p.reason;
    if (h.online)
        for (const Online &o : h.online())
            h.remove(o.peer, p.restart ? net::RefuseReason::Restarting : net::RefuseReason::Stopping, text);
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, 'server', ?2, '', ?3)",
          {std::to_string(t), p.restart ? "restarting" : "stopping", text});
    return p.restart ? Stop::Restart : Stop::Stop;
}

std::string motd() { return setting("motd"); }
std::string maintenance() { return setting("maintenance"); }

void set_motd(const std::string &text)
{
    put("setting", "motd", "motd", text, "config", 0);
}

void set_rules(const std::string &text)
{
    if (setting("rules") != text) put("setting", "rules", "rules", text, "config", 0);
}

bool accepted(const Who &w)
{
    Row rules, mine;
    if (!find("setting", "rules", rules) || role_of(w) >= Role::Helper) return true;
    return find("accepted", target_of(w), mine) && mine.value == std::to_string(rules.at);
}

void check_alts(const Who &w, const Hooks &h)
{
    for (const std::string &l : alts_of(w, true))
        tell_staff(h, "[alts] " + w.name + " came in from where " + l.substr(0, l.find(" (banned)")) +
                          " did, who is banned");
}

void joined(const Who &w, const std::string &room)
{
    query("INSERT INTO sessions (target, name, address, room, joined, left, reason) "
          "VALUES (?1, ?2, ?3, ?4, ?5, 0, '')",
          {target_of(w), w.name, w.address, room, std::to_string(now())});
}

void left(const Who &w, const std::string &room, const std::string &reason)
{
    query("UPDATE sessions SET left = ?1, reason = ?2 WHERE target = ?3 AND room = ?4 AND left = 0",
          {std::to_string(now()), reason, target_of(w), room});
}

void crossed_in(const uint8_t *issuer, const std::string &address)
{
    const std::string key = "key:" + net::to_hex(issuer, net::kSignPublicBytes);
    Row               r;
    const long long   n = find("inbound", key, r) ? std::atoll(r.value.c_str()) : 0;
    put("inbound", key, address, std::to_string(n + 1), "portal", 0);
}

void refused(const Who &w, const std::string &why)
{
    query("INSERT INTO admin_log (at, by, action, target, detail) VALUES (?1, 'door', 'refused', ?2, ?3)",
          {std::to_string(now()), w.name + (w.address.empty() ? "" : " (" + w.address + ")"), why});
}

void set_backups(const std::vector<std::string> &files, int64_t every, int keep)
{
    g_world_files  = files;
    g_backup_every = every;
    g_backup_keep  = std::max(1, keep);
    g_next_backup  = now() + every;
}

bool backup(std::string &err)
{
    if (g_world_files.empty()) {
        err = "no world files";
        return false;
    }
    if (g_backing_up.exchange(true)) {
        err = "one is running";
        return false;
    }
    char stamp[32];
    const std::time_t tt = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&tt));
    // Its own connections, on a thread: SQLite's online backup copies a world
    // in use, a page at a time, and the rooms keep ticking.
    std::thread([files = g_world_files, keep = g_backup_keep, stamp = std::string(stamp)] {
        namespace fs = std::filesystem;
        for (const std::string &f : files) {
            const std::string to = f + ".backup-" + stamp;
            sqlite3 *src = nullptr, *dst = nullptr;
            bool ok = sqlite3_open_v2(f.c_str(), &src, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK &&
                      sqlite3_open(to.c_str(), &dst) == SQLITE_OK;
            if (ok) {
                sqlite3_backup *b = sqlite3_backup_init(dst, "main", src, "main");
                ok = b != nullptr;
                while (ok) {
                    const int rc = sqlite3_backup_step(b, 256);
                    if (rc == SQLITE_DONE) break;
                    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        continue;
                    }
                    ok = rc == SQLITE_OK;
                }
                if (b) sqlite3_backup_finish(b);
            }
            if (src) sqlite3_close(src);
            if (dst) sqlite3_close(dst);
            std::printf("        backup: %s %s\n", to.c_str(), ok ? "written" : "FAILED");
            // The newest `keep`, by name, which is by time.
            std::error_code ec;
            const fs::path        p(f);
            std::vector<fs::path> old;
            for (const auto &e : fs::directory_iterator(p.parent_path().empty() ? "." : p.parent_path(), ec))
                if (e.path().filename().string().rfind(p.filename().string() + ".backup-", 0) == 0)
                    old.push_back(e.path());
            std::sort(old.begin(), old.end());
            for (size_t i = 0; i + (size_t)keep < old.size(); ++i) fs::remove(old[i], ec);
        }
        g_backing_up = false;
    }).detach();
    return true;
}

int report(const std::string &path, int64_t since, bool json)
{
    // Its own read-only connection: not open(), which writes.
    if (sqlite3_open_v2(path.c_str(), &g_db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        std::printf("report: cannot open %s\n", path.c_str());
        close();
        return 1;
    }
    sqlite3_busy_timeout(g_db, 2000);
    bool kept = false;
    query("SELECT 1 FROM sqlite_master WHERE name = 'sessions'", {}, [&](sqlite3_stmt *) { kept = true; });
    if (!kept) {
        if (json) std::printf("{\"file\":%s,\"visits_kept\":false}\n", json_text(path).c_str());
        else std::printf("report: %s has no visits recorded (a server from 0.9 on keeps them)\n", path.c_str());
        sqlite3_close(g_db);
        g_db = nullptr;
        return 0;
    }
    // Text for a person, or one JSON object for a program (--json): the same
    // numbers either way. `j` gathers the object's members.
    std::string j  = "{\"file\":" + json_text(path) + ",\"since_seconds\":" + std::to_string(since);
    const auto  tx = [&](const char *fmt, auto... v) {
        if (!json) std::printf(fmt, v...);
    };
    const int64_t     t = now(), from_t = since > 0 ? t - since : 0;
    const std::string from = std::to_string(from_t), to = std::to_string(t);
    const char       *secs = "(CASE WHEN left = 0 THEN ?2 ELSE left END) - joined";
    tx("%s, %s\n\n", path.c_str(),
       since > 0 ? ("the last " + duration_text(since)).c_str() : "since it was first served");

    query((std::string("SELECT COUNT(DISTINCT target), COUNT(*), SUM(") + secs +
           ") FROM sessions WHERE joined >= ?1").c_str(),
          {from, to}, [&](sqlite3_stmt *st) {
              tx("players   %lld, %lld visits, %s played\n", sqlite3_column_int64(st, 0),
                 sqlite3_column_int64(st, 1), span_text(sqlite3_column_int64(st, 2)).c_str());
              j += ",\"players\":" + std::to_string(sqlite3_column_int64(st, 0)) +
                   ",\"visits\":" + std::to_string(sqlite3_column_int64(st, 1)) +
                   ",\"seconds_played\":" + std::to_string(sqlite3_column_int64(st, 2));
          });

    // The most at once, swept over every visit's two ends.
    std::vector<std::pair<int64_t, int>> ends;
    query("SELECT joined, CASE WHEN left = 0 THEN ?2 ELSE left END FROM sessions WHERE joined >= ?1",
          {from, to}, [&](sqlite3_stmt *st) {
              ends.push_back({sqlite3_column_int64(st, 0), 1});
              ends.push_back({sqlite3_column_int64(st, 1), -1});
          });
    std::sort(ends.begin(), ends.end());   // a leave before a join at the same second
    int     on = 0, peak = 0;
    int64_t peak_at = 0;
    for (const auto &[at, d] : ends)
        if ((on += d) > peak) peak = on, peak_at = at;
    if (peak) tx("most at once  %d, %s\n", peak, when_text(peak_at).c_str());
    j += ",\"most_at_once\":" + std::to_string(peak) + ",\"most_at_once_at\":" + std::to_string(peak_at);

    int hours[24] = {};
    query("SELECT joined FROM sessions WHERE joined >= ?1", {from}, [&](sqlite3_stmt *st) {
        const std::time_t tt = (std::time_t)sqlite3_column_int64(st, 0);
        ++hours[std::localtime(&tt)->tm_hour];
    });
    tx("joins by hour");
    j += ",\"joins_by_hour\":[";
    for (int h = 0; h < 24; ++h) {
        if (hours[h]) tx("  %02d:%d", h, hours[h]);
        j += (h ? "," : "") + std::to_string(hours[h]);
    }
    j += "]";
    tx("\n\nmost played\n");
    j += ",\"most_played\":[";
    bool first = true;
    query((std::string("SELECT name, COUNT(*), SUM(") + secs +
           ") AS s FROM sessions WHERE joined >= ?1 GROUP BY target ORDER BY s DESC LIMIT 10")
              .c_str(),
          {from, to}, [&](sqlite3_stmt *st) {
              tx("  %-20s %4lld visits  %s\n", col(st, 0).c_str(), sqlite3_column_int64(st, 1),
                 span_text(sqlite3_column_int64(st, 2)).c_str());
              j += std::string(first ? "" : ",") + "{\"name\":" + json_text(col(st, 0)) +
                   ",\"visits\":" + std::to_string(sqlite3_column_int64(st, 1)) +
                   ",\"seconds\":" + std::to_string(sqlite3_column_int64(st, 2)) + "}";
              first = false;
          });
    tx("\nrooms\n");
    j += "],\"rooms\":[";
    first = true;
    query((std::string("SELECT room, COUNT(*), SUM(") + secs +
           ") FROM sessions WHERE joined >= ?1 GROUP BY room ORDER BY 3 DESC")
              .c_str(),
          {from, to}, [&](sqlite3_stmt *st) {
              tx("  %-20s %4lld visits  %s\n", col(st, 0).empty() ? "(the world)" : col(st, 0).c_str(),
                 sqlite3_column_int64(st, 1), span_text(sqlite3_column_int64(st, 2)).c_str());
              j += std::string(first ? "" : ",") + "{\"room\":" + json_text(col(st, 0)) +
                   ",\"visits\":" + std::to_string(sqlite3_column_int64(st, 1)) +
                   ",\"seconds\":" + std::to_string(sqlite3_column_int64(st, 2)) + "}";
              first = false;
          });
    j += "]";
    const auto counts = [&](const char *title, const char *key, const char *sql) {
        tx("\n%s\n", title);
        j += std::string(",\"") + key + "\":{";
        bool any = false;
        query(sql, {from}, [&](sqlite3_stmt *st) {
            tx("  %-40s %lld\n", col(st, 0).substr(0, 40).c_str(), sqlite3_column_int64(st, 1));
            j += std::string(any ? "," : "") + json_text(col(st, 0)) + ":" + std::to_string(sqlite3_column_int64(st, 1));
            any = true;
        });
        if (!any) tx("  none\n");
        j += "}";
    };
    counts("turned away at the door", "refused",
           "SELECT substr(detail, 1, instr(detail || ',', ',') - 1), COUNT(*) FROM admin_log "
           "WHERE action = 'refused' AND at >= ?1 GROUP BY 1 ORDER BY 2 DESC LIMIT 10");
    counts("staff actions", "staff_actions",
           "SELECT action, COUNT(*) FROM admin_log WHERE by NOT IN ('server', 'door') AND at >= ?1 "
           "GROUP BY action ORDER BY 2 DESC");
    counts("the server", "server",
           "SELECT action, COUNT(*) FROM admin_log WHERE by = 'server' AND at >= ?1 GROUP BY action");
    counts("crossings in, by the server that sent them", "crossings_in",
           "SELECT label, CAST(value AS INTEGER) FROM admin WHERE kind = 'inbound' AND at >= ?1 "
           "ORDER BY 2 DESC");
    if (json) std::printf("%s}\n", j.c_str());
    sqlite3_close(g_db);
    g_db = nullptr;
    return 0;
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
        // A command whose help starts with what it needs gets its help back
        // without it.
        if (a.name.empty() && (std::strncmp(c.help, "NAME", 4) == 0 || std::strncmp(c.help, "TEXT", 4) == 0)) {
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
    if (may(v, "restart") || may(v, "maintenance") || may(v, "motd")) m += "[Running](page:running) ";
    if (may(v, "players")) m += "[Players](page:players) ";
    if (may(v, "reports")) m += "[Reports](page:reports) ";
    if (may(v, "logs")) m += "[Log](page:log) ";
    m += "[Server](page:server) [Mods](page:mods) [Here](page:here)\n";
    if (const std::string mt = setting("maintenance"); !mt.empty())
        m += "> In maintenance: " + page_text(mt) + "\n";
    if (g_pending.at)
        m += "> " + page_text(going_text(g_pending, std::max<int64_t>(0, g_pending.at - now()))) + "\n";

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
        const size_t      at   = r.target.find('@');
        const std::string room = at == std::string::npos ? "" : r.target.substr(at + 1);
        std::string acts;
        if (outranks(v, resolve(r.label, h)))
            acts = cmd_link(v, "roles", "take away", "/role " + r.label + " player" + (room.empty() ? "" : " " + room), r.label);
        m += "| " + page_text(r.label) + " | " + r.value + (room.empty() ? "" : " in " + page_text(room)) +
             " | by " + page_text(r.by) + " |" + acts + " |\n";
    }
    m += "\n?[NAME ROLE [ROOM]](cmd:/role {text})\n";
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

// Restarts, maintenance, the message of the day, backups.
std::string running_page(const Who &v, const Hooks &h)
{
    std::string m = "[Back](page:admin)\n## Restart\n";
    if (g_pending.at) {
        m += page_text(going_text(g_pending, std::max<int64_t>(0, g_pending.at - now()))) + " ";
        if (may(v, "restart")) m += "[cancel](cmd:/restart cancel)";
        m += "\n";
    } else if (may(v, "restart")) {
        m += "[in 1 minute](cmd:/restart 1m) [in 5 minutes](cmd:/restart 5m) [in 15 minutes](cmd:/restart 15m) "
             "[stop in 5 minutes](cmd:/stop 5m)\n";
    }
    if (g_daily >= 0) {
        char at[16];
        std::snprintf(at, sizeof(at), "%02d:%02d", g_daily / 60, g_daily % 60);
        m += std::string("Every day at ") + at + " (--restart-at).\n";
    }
    if (may(v, "maintenance")) {
        const std::string mt = setting("maintenance");
        m += "## Maintenance\n";
        m += mt.empty() ? "Open to everybody.\n?[close for maintenance: why](cmd:/maintenance on {text})\n"
                        : "Only staff come in: " + page_text(mt) + " [open again](cmd:/maintenance off)\n";
    }
    if (may(v, "motd")) {
        const std::string mt = setting("motd");
        m += "## Message of the day\n" + (mt.empty() ? std::string("None.") : page_text(mt)) + "\n";
        m += "?[a new one](cmd:/motd {text})\n";
    }
    if (may(v, "announce")) m += "?[announce to every screen](cmd:/announce {text})\n";
    if (may(v, "backup")) {
        m += "## Backups\n";
        m += g_backup_every > 0 ? "Every " + duration_text(g_backup_every) + ", the newest " +
                                      std::to_string(g_backup_keep) + " kept. "
                                : std::string("Only when asked. ");
        m += "[back up now](cmd:/backup)\n";
    }
    if (may(v, "logs") && h.metrics) {
        m += "## Now\n";
        for (const std::string &l : h.metrics()) m += page_text(l) + "\n";
    }
    return m;
}

std::string players_page(const Who &v, const Hooks &h)
{
    std::string m = "[Back](page:admin)\nThe last week.\n| name | visits | played | last | |\n";
    const int64_t t = now();
    query("SELECT name, COUNT(*), SUM((CASE WHEN left = 0 THEN ?2 ELSE left END) - joined), "
          "MAX(CASE WHEN left = 0 THEN ?2 ELSE left END), MIN(left) FROM sessions "
          "WHERE joined >= ?1 OR left = 0 OR left >= ?1 GROUP BY target ORDER BY 4 DESC LIMIT 60",
          {std::to_string(t - 7 * 86400), std::to_string(t)}, [&](sqlite3_stmt *st) {
              const std::string n = col(st, 0);
              m += "| " + page_text(n) + " | " + std::to_string(sqlite3_column_int64(st, 1)) + " | " +
                   span_text(sqlite3_column_int64(st, 2)) + " | " +
                   (sqlite3_column_int64(st, 4) == 0 ? std::string("online")
                                                     : when_text(sqlite3_column_int64(st, 3))) +
                   " |" + cmd_link(v, "whois", "whois", "/whois " + n, n) + " |\n";
          });
    (void)h;
    return m;
}

std::string lines_page(const std::vector<std::string> &lines)
{
    std::string m = "[Back](page:admin)\n";
    for (const std::string &l : lines) m += page_text(l) + "\n";
    return m;
}

} // namespace

bool page(const std::string &name, const Who &viewer, const Hooks &hooks, std::string &title,
          std::string &markup)
{
    if (name == "rules") {
        const std::string r = setting("rules");
        title  = "Rules";
        markup = r.empty() ? "This server has no rules to accept.\n" : page_text(r) + "\n";
        if (!r.empty() && !accepted(viewer)) markup += "\n[Accept](cmd:/accept) [Leave](cmd:/leave)\n";
        return true;
    }
    if (name == "running" && (may(viewer, "restart") || may(viewer, "maintenance") || may(viewer, "motd"))) {
        title  = "Running the server";
        markup = running_page(viewer, hooks);
        return true;
    }
    if (name == "players" && may(viewer, "players")) {
        title  = "Players";
        markup = players_page(viewer, hooks);
        return true;
    }
    if ((name == "reports" && may(viewer, "reports")) || (name == "log" && may(viewer, "logs"))) {
        std::vector<std::string> lines;
        Args                     a;
        if (name == "reports") {
            cmd_reports(viewer, a, hooks, lines);
        } else {
            a.words = {"40"};
            cmd_logs(viewer, a, hooks, lines);
        }
        title  = name == "reports" ? "Reports" : "Log";
        markup = lines_page(lines);
        return true;
    }
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


// ------------------------------------------------------------- federation
//
// Friend servers (DESIGN.md 4.3, since 0.9): two servers that each put the
// other in their list exchange, a line of TCP at a time, what both chose
// to share -- their permanent bans, who is online, and a chat between them.
//
// One TCP connection a line. The receiver speaks first, with a key of this
// connection only:
//
//   <- CNPFED3 R_EPH
//   -> CNPFED3 FROM TO MS S_EPH SEALED SIG
//
// FROM and TO are signing keys in hex, MS the sender's clock in milliseconds
// (later than its last line, and within five minutes of ours), S_EPH the
// sender's own throwaway key, SEALED the payload sealed for TO (net::seal_for,
// with the two throwaway keys' secret mixed in: only TO reads it, and not even
// TO once both throwaway keys are gone) in hex, SIG the ed25519 signature of
// "CNPFED3|TO|MS|R_EPH|S_EPH|sealed" by FROM. Sealed because bags travel this
// way, and a bag is the player's. A line from a key not in the list is
// dropped where it is read (federation_tick).

namespace {

net::SignKeyPair g_me;
bool             g_fed_on = false;
std::string      g_me_hex;

struct FedIn {
    std::string from, payload, address;
};
struct FedOut {
    std::string host;
    int         port = 0;
    std::string to, payload;   // sealed once the receiver has said its key
    int64_t     ms  = 0;
    bool        bag = false;   // carries a bag (CNP_TEST_FED_DELAY_MS)
};
// Never destroyed: the sender thread waits on them for as long as the
// process lives, and glibc's condition variable destructor waits for its
// waiters, so destroying them at exit would hang the server on its way out.
std::mutex                                    &g_fed_m    = *new std::mutex;
std::condition_variable                       &g_fed_wake = *new std::condition_variable;
std::vector<FedIn>                             g_fed_in;
std::vector<FedOut>                            g_fed_out;
std::unordered_map<std::string, int64_t>       g_fed_last_ms;   // replay guard, by sender
std::atomic<int>                               g_fed_readers{0};

struct Friend {
    std::string key, name, host, shares;   // key: "key:<hex>"
    int         port = 0;
    bool        shares_(const char *what) const { return shares.find(what) != std::string::npos; }
};

std::vector<Friend> friends()
{
    std::vector<Friend> out;
    for (const Row &r : all("friend")) {
        Friend f;
        f.key  = r.target;
        f.name = r.label;
        std::istringstream in(r.value);
        std::string        addr;
        in >> addr >> f.shares;
        const size_t colon = addr.rfind(':');
        f.host = addr.substr(0, colon);
        f.port = colon == std::string::npos ? 0 : std::atoi(addr.c_str() + colon + 1);
        out.push_back(f);
    }
    return out;
}

// Who is on each friend, and when it last said so; when each was last heard.
struct Presence {
    std::vector<std::string> names;
    int64_t                  at = 0;
};
std::unordered_map<std::string, Presence> g_presence;   // by friend key
std::unordered_map<std::string, int64_t>  g_heard;
int64_t     g_next_presence = 0, g_next_bans = 0, g_next_bans_look = 0;
std::string g_last_bans, g_last_presence;

int64_t now_ms()
{
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void fed_send(const Friend &f, const std::string &payload)
{
    if (!g_fed_on || f.host.empty() || f.port <= 0) return;
    static int64_t    last = 0;
    const int64_t     ms   = std::max(now_ms(), last + 1);
    last                   = ms;
    std::lock_guard<std::mutex> hold(g_fed_m);
    if (g_fed_out.size() < 256)
        g_fed_out.push_back({f.host, f.port, f.key.substr(4), payload, ms, payload.rfind("bag\n", 0) == 0});
    g_fed_wake.notify_one();
}

void fed_send_all(const char *share, const std::string &payload)
{
    for (const Friend &f : friends())
        if (f.shares_(share)) fed_send(f, payload);
}

// Connected within three seconds, or not at all: a friend that is down
// costs the sender thread, never a tick.
Sock dial(const std::string &host, int port)
{
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) return kNoSock;
    Sock s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == kNoSock) {
        freeaddrinfo(res);
        return kNoSock;
    }
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    const int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
    ::connect(s, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    fd_set w;
    FD_ZERO(&w);
    FD_SET(s, &w);
    timeval tv{3, 0};
    int     err = 0;
    socklen_t len = sizeof(err);
    if (select((int)s + 1, nullptr, &w, nullptr, &tv) != 1 ||
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len) != 0 || err != 0) {
        close_sock(s);
        return kNoSock;
    }
#ifdef _WIN32
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
#else
    fcntl(s, F_SETFL, fl);
#endif
    return s;
}

// The line for `o`, once the receiver on `s` has said its throwaway key; ""
// when it does not within three seconds, or says something else.
std::string sealed_line(Sock s, const FedOut &o)
{
#ifdef _WIN32
    DWORD tv = 3000;
#else
    timeval tv{3, 0};
#endif
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    std::string hello;
    char        buf[256];
    for (long n; hello.find('\n') == std::string::npos && hello.size() < 200 &&
                 (n = read_sock(s, buf, sizeof(buf))) > 0;)
        hello.append(buf, (size_t)n);
    std::istringstream in(hello.substr(0, hello.find('\n')));
    std::string        magic, r_eph_hex;
    in >> magic >> r_eph_hex;
    uint8_t      r_eph[net::kKeyBytes], to_key[net::kSignPublicBytes], mix[net::kKeyBytes];
    net::KeyPair eph;
    std::string  sealed;
    if (magic != "CNPFED3" || !net::from_hex(r_eph_hex, r_eph, sizeof(r_eph)) ||
        !net::from_hex(o.to, to_key, sizeof(to_key)) || !net::KeyPair::generate(eph))
        return "";
    net::x25519_shared(eph.secret_key, r_eph, mix);
    const bool ok = net::seal_for(g_me, to_key, o.payload, sealed, mix);
    const std::string s_eph = net::to_hex(eph.public_key, net::kKeyBytes);
    std::memset(&eph, 0, sizeof(eph));
    std::memset(mix, 0, sizeof(mix));
    if (!ok) return "";
    const std::string ms   = std::to_string(o.ms);
    const std::string body = "CNPFED3|" + o.to + "|" + ms + "|" + r_eph_hex + "|" + s_eph + "|" + sealed;
    uint8_t           sig[net::kSignatureBytes];
    net::sign_message(g_me, body.data(), body.size(), sig);
    return "CNPFED3 " + g_me_hex + " " + o.to + " " + ms + " " + s_eph + " " +
           net::to_hex((const uint8_t *)sealed.data(), sealed.size()) + " " + net::to_hex(sig, sizeof(sig)) + "\n";
}

void fed_reader(Sock c, std::string address)
{
#ifdef _WIN32
    DWORD tv = 5000;
#else
    timeval tv{5, 0};
#endif
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    net::KeyPair eph;
    if (!net::KeyPair::generate(eph)) {
        close_sock(c);
        --g_fed_readers;
        return;
    }
    const std::string r_eph = net::to_hex(eph.public_key, net::kKeyBytes);
    send_sock(c, "CNPFED3 " + r_eph + "\n");
    std::string line;
    char        buf[4096];
    for (long n; line.find('\n') == std::string::npos && line.size() < 200000 &&
                 (n = read_sock(c, buf, sizeof(buf))) > 0;)
        line.append(buf, (size_t)n);
    close_sock(c);
    --g_fed_readers;

    std::istringstream in(line.substr(0, line.find('\n')));
    std::string        magic, from, to, ms_text, s_eph_hex, payload_hex, sig_hex;
    in >> magic >> from >> to >> ms_text >> s_eph_hex >> payload_hex >> sig_hex;
    uint8_t     key[net::kSignPublicBytes], sig[net::kSignatureBytes], s_eph[net::kKeyBytes], mix[net::kKeyBytes];
    std::string sealed(payload_hex.size() / 2, '\0'), payload;
    const int64_t ms = std::atoll(ms_text.c_str());
    if (magic != "CNPFED3" || to != g_me_hex || !net::from_hex(from, key, sizeof(key)) ||
        !net::from_hex(sig_hex, sig, sizeof(sig)) || !net::from_hex(s_eph_hex, s_eph, sizeof(s_eph)) ||
        !net::from_hex(payload_hex, (uint8_t *)sealed.data(), sealed.size()) ||
        std::llabs(ms - now_ms()) > 300000)
        return;
    const std::string body = "CNPFED3|" + to + "|" + ms_text + "|" + r_eph + "|" + s_eph_hex + "|" + sealed;
    net::x25519_shared(eph.secret_key, s_eph, mix);
    const bool opened = net::verify_message(key, body.data(), body.size(), sig) &&
                        net::open_sealed(g_me, key, sealed, payload, mix);
    std::memset(&eph, 0, sizeof(eph));
    std::memset(mix, 0, sizeof(mix));
    if (!opened) return;
    std::lock_guard<std::mutex> hold(g_fed_m);
    int64_t &last = g_fed_last_ms[from];
    if (ms <= last) return;   // seen, or older: a replay
    last = ms;
    if (g_fed_in.size() < 1024) g_fed_in.push_back({from, std::move(payload), std::move(address)});
}

// The permanent bans of keys made here, for friends: not the ones a friend
// sent, which would go round in a circle.
std::string own_bans()
{
    std::string p = "bans";
    for (const Row &r : all("ban"))
        if (r.until == 0 && r.target.rfind("key:", 0) == 0 && r.by.rfind("friend:", 0) != 0)
            p += "\n" + r.target.substr(4) + "\t" + r.label + "\t" + r.value;
    return p;
}

// ------------------------------------------------- bags between friends
//
// One bag for a player across friend servers, moved when they move: the
// server they arrive at asks the one they came from (or the one it gave the
// bag to last) and that one lets go of it -- a session holding it is emptied,
// and a `bagaway` row says where it went, so a later visit asks there too.
//
//   bagask  ID          send me ID's bag
//   bag     ID BAG HID GEN  here it is, what some world did not know, and
//                           how many times it has moved between friends
//   bagat   ID KEY      not here: it went to that friend
//   bagnone ID          never had one
//   bagpeek ID          (to every friend, for a player who walked in) whose is newest?
//   bagseen ID GEN SEEN mine has moved GEN times, kept here at SEEN; -1 -1 not mine
//
// Newest is the one that moved most, which needs no clock: only the holder
// is ever given a higher count. Two copies that never moved (two servers
// that met when both had one) are told apart by when each was kept.

struct BagAsk {
    std::string friend_key;
    int64_t     until = 0;
    int         hops  = 0;
};
std::unordered_map<std::string, BagAsk> g_bag_asks;   // by identity hex

// A peek at every friend: the newest bag, or the one here, wins.
struct BagPeek {
    int64_t     until = 0;
    std::pair<int64_t, int64_t> best{-1, -1};   // moves, then when kept
    int         waiting = 0;
    std::string best_key;
};

int64_t bag_gen(const std::string &key)
{
    Row r;
    return find("baggen", key, r) ? std::atoll(r.value.c_str()) : 0;
}
std::unordered_map<std::string, BagPeek> g_bag_peeks;   // by identity hex

bool ask_bag(const Friend &f, const std::string &id_hex, int hops)
{
    if (!f.shares_("bags")) return false;
    g_bag_asks[id_hex] = {f.key, now() + 5, hops};
    fed_send(f, "bagask\n" + id_hex);
    std::printf("        federation: asked %s for a bag\n", f.name.c_str());
    return true;
}

void bag_done(const std::string &id_hex, const Hooks &h, bool late)
{
    g_bag_asks.erase(id_hex);
    uint8_t id[net::kSignPublicBytes];
    if (!net::from_hex(id_hex, id, sizeof(id))) return;
    if (late) {
        drop("bagaway", "key:" + id_hex);   // the one here is the one now
        if (h.bag_late) h.bag_late(id);
    }
}

void peek_done(const std::string &id_hex, const Hooks &h)
{
    const BagPeek p = g_bag_peeks[id_hex];
    g_bag_peeks.erase(id_hex);
    for (const Friend &f : friends())
        if (f.key == p.best_key && ask_bag(f, id_hex, 0)) return;
    bag_done(id_hex, h, true);
}

void apply_bag(const Friend &f, const std::string &kind, std::istringstream &in, const Hooks &h)
{
    std::string id_hex;
    std::getline(in, id_hex);
    uint8_t id[net::kSignPublicBytes];
    if (!net::from_hex(id_hex, id, sizeof(id))) return;
    const std::string key = "key:" + id_hex;

    if (kind == "bagpeek") {
        Row           away;
        const int64_t seen = find("bagaway", key, away) || !h.bag_seen ? -1 : h.bag_seen(id);
        fed_send(f, "bagseen\n" + id_hex + "\n" + std::to_string(seen < 0 ? -1 : bag_gen(key)) + " " +
                        std::to_string(seen));
        return;
    }
    if (kind == "bagseen") {
        const auto p = g_bag_peeks.find(id_hex);
        if (p == g_bag_peeks.end()) return;
        std::string seen_text;
        std::getline(in, seen_text);
        std::pair<int64_t, int64_t> theirs{-1, -1};
        std::istringstream(seen_text) >> theirs.first >> theirs.second;
        if (theirs.second >= 0 && theirs > p->second.best) {
            p->second.best     = theirs;
            p->second.best_key = f.key;
        }
        if (--p->second.waiting <= 0) peek_done(id_hex, h);
        return;
    }
    if (kind == "bagask") {
        Row        away;
        const bool is_away = find("bagaway", key, away);
        std::string bag, packs;
        if (h.bag_give && h.bag_give(id, !is_away, bag, packs)) {
            const std::string hid = hidden(id);
            set_hidden(id, "");
            fed_send(f, "bag\n" + id_hex + "\n" + net::to_hex((const uint8_t *)bag.data(), bag.size()) +
                            "\n" + net::to_hex((const uint8_t *)hid.data(), hid.size()) + "\n" +
                            std::to_string(bag_gen(key)) + "\n" +
                            net::to_hex((const uint8_t *)packs.data(), packs.size()));
            put("bagaway", key, f.name, f.key.substr(4), "friend:" + f.name, 0);
            std::printf("        federation: %s takes a bag\n", f.name.c_str());
        } else if (is_away) {
            fed_send(f, "bagat\n" + id_hex + "\n" + away.value);
        } else {
            fed_send(f, "bagnone\n" + id_hex);
        }
        return;
    }

    const auto asked = g_bag_asks.find(id_hex);
    const bool ours  = asked != g_bag_asks.end() && asked->second.friend_key == f.key;
    if (kind == "bag") {
        std::string bag_hex, hid_hex, gen_text, packs_hex;
        std::getline(in, bag_hex);
        std::getline(in, hid_hex);
        std::getline(in, gen_text);
        std::getline(in, packs_hex);
        std::string bag(bag_hex.size() / 2, '\0'), hid(hid_hex.size() / 2, '\0'),
            packs(packs_hex.size() / 2, '\0');
        if (!net::from_hex(bag_hex, (uint8_t *)bag.data(), bag.size()) ||
            !net::from_hex(hid_hex, (uint8_t *)hid.data(), hid.size()) ||
            !net::from_hex(packs_hex, (uint8_t *)packs.data(), packs.size()))
            return;
        g_bag_asks.erase(id_hex);
        drop("bagaway", key);
        put("baggen", key, "", std::to_string(std::atoll(gen_text.c_str()) + 1), "friend:" + f.name, 0);
        if (h.bag_take) h.bag_take(id, bag, hid, packs, f.name);
        return;
    }
    if (!ours) return;
    if (kind == "bagat") {
        std::string next;
        std::getline(in, next);
        for (const Friend &g : friends())
            if (g.key == "key:" + next && g.key != f.key && asked->second.hops < 3 &&
                ask_bag(g, id_hex, asked->second.hops + 1))
                return;
    }
    bag_done(id_hex, h, true);   // bagnone, or a friend of a friend we do not know
}

void apply(const Friend &f, const std::string &payload, const Hooks &h)
{
    std::istringstream in(payload);
    std::string        kind;
    std::getline(in, kind);
    const std::string by = "friend:" + f.name;
    if (kind == "presence" && f.shares_("presence")) {
        Presence p;
        p.at = now();
        for (std::string n; std::getline(in, n);)
            if (!n.empty()) p.names.push_back(n);
        g_presence[f.key] = std::move(p);
    } else if (kind == "bans" && f.shares_("bans")) {
        // The friend's whole list, again: what it no longer bans goes.
        query("DELETE FROM admin WHERE kind = 'ban' AND by = ?1", {by});
        int n = 0;
        for (std::string l; std::getline(in, l);) {
            const size_t t1 = l.find('\t'), t2 = l.find('\t', t1 + 1);
            const std::string key = "key:" + l.substr(0, t1);
            if (t1 == std::string::npos || t1 != 2 * net::kSignPublicBytes) continue;
            Row r;
            if (find("ban", key, r) || find("fedskip", key, r)) continue;   // ours, or lifted here
            put("ban", key, l.substr(t1 + 1, t2 - t1 - 1), t2 == std::string::npos ? "" : l.substr(t2 + 1), by, 0);
            ++n;
            for (const Online &o : online_matching(key, h))
                h.remove(o.peer, net::RefuseReason::Banned, "banned on " + f.name);
        }
        std::printf("        federation: %s bans %d key(s) here\n", f.name.c_str(), n);
    } else if (f.shares_("bags") && kind.rfind("bag", 0) == 0) {
        apply_bag(f, kind, in, h);
    } else if (kind == "chat" && f.shares_("chat")) {
        std::string line;
        std::getline(in, line);
        line = "[" + f.name + "] " + line.substr(0, 300);
        if (h.online && h.say)
            for (const Online &o : h.online()) h.say(o.peer, line);
        std::printf("        %s\n", line.c_str());
    }
}

void cmd_friend(const Who &c, const Args &a, const Hooks &, std::vector<std::string> &out)
{
    if (a.name == "add") {
        uint8_t key[net::kSignPublicBytes];
        if (a.words.size() < 4 || !net::from_hex(a.words[3], key, sizeof(key)) ||
            a.words[2].find(':') == std::string::npos) {
            out.push_back("/friend add NAME HOST:PORT KEY [bans,presence,chat,bags]");
            return;
        }
        const std::string shares = a.words.size() > 4 ? a.words[4] : "bans,presence,chat,bags";
        put("friend", "key:" + net::to_hex(key, sizeof(key)), a.words[1], a.words[2] + " " + shares, c.name, 0);
        log(c, "friend", a.words[1], a.words[2] + " " + shares);
        g_next_presence = g_next_bans = 0;   // tell it now
        out.push_back(a.words[1] + " is a friend: " + shares + ". It has to add this server too (" +
                      (g_fed_on ? "key " + g_me_hex : std::string("start with --federation PORT")) + ").");
        return;
    }
    if (a.name == "remove" && a.words.size() > 1) {
        for (const Friend &f : friends())
            if (f.name == a.words[1]) {
                drop("friend", f.key);
                query("DELETE FROM admin WHERE kind = 'ban' AND by = ?1", {"friend:" + f.name});
                g_presence.erase(f.key);
                log(c, "unfriend", f.name, "");
                out.push_back(f.name + " is no longer a friend; its bans are lifted here.");
                return;
            }
        out.push_back("No friend called " + a.words[1] + ".");
        return;
    }
    out.push_back(g_fed_on ? "This server's key: " + g_me_hex : "Federation off: --federation PORT.");
    for (const Friend &f : friends()) {
        const auto heard = g_heard.find(f.key);
        out.push_back(f.name + " " + f.host + ":" + std::to_string(f.port) + " " + f.shares + ", " +
                      (heard == g_heard.end() ? std::string("never heard from")
                                              : "heard " + when_text(heard->second)));
    }
}

void cmd_fed(const Who &c, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    const std::string text = rest(a, 0);
    std::string       why;
    if (text.empty()) {
        out.push_back("/fed TEXT");
    } else if (muted(c, why)) {
        out.push_back(why);
    } else if (!g_fed_on) {
        out.push_back("This server has no friends to talk to (--federation).");
    } else {
        fed_send_all("chat", "chat\n" + c.name + ": " + text);
        if (h.online && h.say)
            for (const Online &o : h.online()) h.say(o.peer, "[here] " + c.name + ": " + text);
    }
}

void cmd_where(const Who &, const Args &a, const Hooks &h, std::vector<std::string> &out)
{
    if (h.online)
        for (const Online &o : h.online())
            if (o.who.name == a.name) {
                out.push_back(a.name + " is here" + (o.room.empty() ? "." : ", in " + o.room + "."));
                return;
            }
    for (const Friend &f : friends()) {
        const auto p = g_presence.find(f.key);
        if (p == g_presence.end() || now() - p->second.at > 120) continue;
        for (const std::string &n : p->second.names)
            if (n == a.name) {
                out.push_back(a.name + " is on " + f.name + ".");
                return;
            }
    }
    out.push_back("Nobody called " + a.name + " here or on a friend server.");
}

} // namespace

bool start_federation(int port, const net::SignKeyPair &me, std::string &err)
{
    g_me     = me;
    g_me_hex = net::to_hex(me.public_key, net::kSignPublicBytes);
    if (port > 0) {
        const Sock l = ::socket(AF_INET, SOCK_STREAM, 0);
        int        yes = 1;
        setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
        sockaddr_in at{};
        at.sin_family      = AF_INET;
        at.sin_port        = htons((uint16_t)port);
        at.sin_addr.s_addr = htonl(INADDR_ANY);   // other machines: that is the point
        if (l == kNoSock || ::bind(l, (const sockaddr *)&at, sizeof(at)) != 0 || ::listen(l, 8) != 0) {
            if (l != kNoSock) close_sock(l);
            err = "federation: cannot listen on port " + std::to_string(port);
            return false;
        }
        std::thread([l] {
            for (;;) {
                sockaddr_in from{};
                socklen_t   len = sizeof(from);
                const Sock  c   = ::accept(l, (sockaddr *)&from, &len);
                if (c == kNoSock) continue;
                if (g_fed_readers >= 8) {
                    close_sock(c);
                    continue;
                }
                char ip[64] = "?";
                inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
                ++g_fed_readers;
                std::thread(fed_reader, c, std::string(ip)).detach();
            }
        }).detach();
    }
    // One sender, a line a connection. CNP_TEST_FED_DELAY_MS holds each bag
    // it sends back that long: a slow friend, for trying the late bag.
    const char *delay_env = std::getenv("CNP_TEST_FED_DELAY_MS");
    const int   delay_ms  = delay_env ? std::atoi(delay_env) : 0;
    std::thread([delay_ms] {
        for (;;) {
            FedOut o;
            {
                std::unique_lock<std::mutex> hold(g_fed_m);
                g_fed_wake.wait(hold, [] { return !g_fed_out.empty(); });
                o = std::move(g_fed_out.front());
                g_fed_out.erase(g_fed_out.begin());
            }
            if (delay_ms > 0 && o.bag) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            const Sock s = dial(o.host, o.port);
            if (s == kNoSock) continue;
            send_sock(s, sealed_line(s, o));
            close_sock(s);
        }
    }).detach();
    g_fed_on = true;
    return true;
}

void federation_tick(const Hooks &h)
{
    if (!g_fed_on) return;
    std::vector<FedIn> in;
    {
        std::lock_guard<std::mutex> hold(g_fed_m);
        in.swap(g_fed_in);
    }
    if (!in.empty()) {
        const std::vector<Friend> fs = friends();
        for (const FedIn &m : in) {
            const auto f = std::find_if(fs.begin(), fs.end(), [&](const Friend &x) { return x.key == "key:" + m.from; });
            if (f == fs.end()) {
                std::printf("        federation: a line from %s, a server not in the list\n", m.address.c_str());
                continue;
            }
            g_heard[f->key] = now();
            apply(*f, m.payload, h);
        }
    }
    const int64_t t = now();
    for (auto it = g_bag_peeks.begin(); it != g_bag_peeks.end();) {
        const std::string id = it->first;
        ++it;
        if (g_bag_peeks[id].until <= t) peek_done(id, h);
    }
    // A friend that does not answer within five seconds: the bag here, then.
    for (auto it = g_bag_asks.begin(); it != g_bag_asks.end();) {
        const std::string id = it->first;
        ++it;
        if (g_bag_asks[id].until <= t) {
            std::printf("        federation: no bag from a friend in 5 s; the one here\n");
            bag_done(id, h, true);
        }
    }
    // Who is here every thirty seconds, and as soon as it changes.
    std::string p = "presence";
    if (h.online)
        for (const Online &o : h.online()) p += "\n" + o.who.name;
    if (t >= g_next_presence || p != g_last_presence) {
        g_next_presence = t + 30;
        g_last_presence = p;
        fed_send_all("presence", p);
    }
    // The list again every five minutes, and as soon as it changes.
    if (t >= g_next_bans_look) {
        g_next_bans_look = t + 5;
        const std::string b = own_bans();
        if (t >= g_next_bans || b != g_last_bans) {
            g_next_bans = t + 300;
            g_last_bans = b;
            fed_send_all("bans", b);
        }
    }
}

bool bag_ask(const Who &who, const uint8_t *issuer, const Hooks &h)
{
    if (!g_fed_on || !who.has_identity) return false;
    const std::string id_hex = net::to_hex(who.identity, net::kSignPublicBytes);
    std::string       want;
    Row               away;
    if (issuer) want = "key:" + net::to_hex(issuer, net::kSignPublicBytes);
    for (int pass = 0; pass < 2; ++pass) {
        for (const Friend &f : friends())
            if (f.key == want) return ask_bag(f, id_hex, 0);
        if (pass == 0 && find("bagaway", key_target(who.identity), away)) want = "key:" + away.value;
    }
    // Walked in, with nothing saying where the bag is: whichever friend kept
    // it last has it, if that is later than here.
    BagPeek p;
    p.until             = now() + 3;
    const int64_t mine  = h.bag_seen ? h.bag_seen(who.identity) : -1;
    if (mine >= 0) p.best = {bag_gen(key_target(who.identity)), mine};
    for (const Friend &f : friends())
        if (f.shares_("bags")) {
            fed_send(f, "bagpeek\n" + id_hex);
            ++p.waiting;
        }
    if (p.waiting == 0) return false;
    g_bag_peeks[id_hex] = p;
    return true;
}

std::string hidden(const uint8_t *identity, const char *kind)
{
    Row         r;
    std::string bytes;
    if (find(kind, key_target(identity), r)) {
        bytes.assign(r.value.size() / 2, '\0');
        if (!net::from_hex(r.value, (uint8_t *)bytes.data(), bytes.size())) bytes.clear();
    }
    return bytes;
}

void set_hidden(const uint8_t *identity, const std::string &bytes, const char *kind)
{
    if (bytes.empty())
        drop(kind, key_target(identity));
    else
        put(kind, key_target(identity), "", net::to_hex((const uint8_t *)bytes.data(), bytes.size()), "", 0);
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
