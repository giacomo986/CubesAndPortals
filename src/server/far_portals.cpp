#include "server/far_portals.h"

#include "core/net/crypto.h"
#include "core/version.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>

namespace cnp {
namespace farp {

namespace {

constexpr double kHeardFor = 90.0;   // an offer older than this is a friend not heard from
constexpr double kAskFor   = 60.0;   // a link asked for and not taken in this long comes undone

enum class St { None, Ask, Ok };

struct Entry {
    std::string              room, name;
    Portal                   shape;
    std::vector<std::string> offered;
    bool                     ig_offline = false, ig_protocol = false, ig_size = false;
    std::string              fr, far_room, far_name;   // fr empty: linked to nothing
    // Who the link was made with and under, so it outlives a restart and a
    // friend that is off: its key, where it is, its UDP port; and this
    // server's own key then.
    std::string              fr_key, fr_host, own_key;
    int                      fr_port   = 0;
    St                       st        = St::None;
    double                   ask_at    = 0.0;
    int                      far_index = -1;
    int                      index     = -1;   // the room's mouth it has, or -1
    std::string              why;              // why the last link came undone
};

// What a friend offers this server, from its last "portals" line.
struct Offered {
    std::string room, name, st, link_room, link_name;   // st: free, busy, ask, ok
    int         index = -1;
    float       hw = 0.0f, hh = 0.0f;
};
struct Remote {
    int                  port = 0, pmin = 0, pmax = 0;
    std::string          engine;
    double               heard = -1.0;   // -1: not since this server started
    std::vector<Offered> portals;
};

std::mutex                                       g_m;
std::vector<Entry>                               g_entries;
std::vector<Friend>                              g_friends;
std::vector<std::pair<std::string, Remote>>      g_remote;   // by friend name
std::vector<std::pair<std::string, std::string>> g_unlinks;  // (friend, line) to send
int                                              g_port = 0;
std::string                                      g_key;      // this server's, hex
bool                                             g_dirty = false;   // offers to go out now
bool                                             g_save  = false;   // and the world file to be written

double now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Entry *find(const std::string &room, const std::string &name)
{
    for (Entry &e : g_entries)
        if (e.room == room && e.name == name) return &e;
    return nullptr;
}
const Friend *friend_of(const std::string &name)
{
    for (const Friend &f : g_friends)
        if (f.name == name) return &f;
    return nullptr;
}
Remote &remote(const std::string &name)
{
    for (auto &r : g_remote)
        if (r.first == name) return r.second;
    g_remote.emplace_back(name, Remote{});
    return g_remote.back().second;
}
const Offered *offered_by(const Remote &r, const std::string &room, const std::string &name)
{
    for (const Offered &o : r.portals)
        if (o.room == room && o.name == name) return &o;
    return nullptr;
}
bool offers(const Entry &e, const std::string &fr)
{
    return std::find(e.offered.begin(), e.offered.end(), fr) != e.offered.end();
}
bool points_at(const Offered &o, const Entry &e)
{
    return (o.st == "ask" || o.st == "ok") && o.link_room == e.room && o.link_name == e.name;
}
bool leads_to(const Entry &e, const std::string &fr, const Offered &o)
{
    return e.st != St::None && e.fr == fr && e.far_room == o.room && e.far_name == o.name;
}

std::string size_text(float hw, float hh)
{
    char b[48];
    std::snprintf(b, sizeof(b), "%gx%g", (double)(hw * 2.0f), (double)(hh * 2.0f));
    return b;
}

// Whether a portal of friend `fr` can be linked to with this server at all,
// and (with `o`) to `e`, with the checks `e` does without left out: "" or why
// not.
std::string compat(const std::string &fr, const Remote &r, const Entry *e, const Offered *o)
{
    const bool offline = e && e->ig_offline;
    if (!offline && (r.heard < 0.0 || now() - r.heard > kHeardFor))
        return fr + " has not been heard from (its --federation port, and does it list this server?)";
    if (!offline && r.port <= 0) return fr + " takes no players: it said no UDP port";
    const int lo = std::max(r.pmin, (int)net::kMinProtocol), hi = std::min(r.pmax, (int)net::kProtocol);
    if (!(e && e->ig_protocol) && r.pmax > 0 && lo > hi)
        return fr + " speaks protocol " + std::to_string(r.pmin) + "-" + std::to_string(r.pmax) +
               ", this server " + std::to_string(net::kMinProtocol) + "-" + std::to_string(net::kProtocol) +
               ": no client speaks both";
    if (e && o && !e->ig_size &&
        (std::fabs(o->hw - e->shape.half_w) > 0.01f || std::fabs(o->hh - e->shape.half_h) > 0.01f))
        return "the holes differ: " + size_text(e->shape.half_w, e->shape.half_h) + " here, " +
               size_text(o->hw, o->hh) + " there";
    return "";
}

// A portals_unlink line for `fr`: ours (room, name) lets go of its (room, name).
void tell_unlink(const std::string &fr, const std::string &room, const std::string &name,
                 const std::string &far_room, const std::string &far_name, const std::string &why)
{
    g_unlinks.emplace_back(fr, "portals_unlink\n" + room + "\t" + name + "\t" + far_room + "\t" + far_name +
                                   "\t" + why + "\n");
}

// `tell`: this side let go, and the other is to hear it.
void undo(Entry &e, const std::string &why, bool tell)
{
    if (e.st != St::None) {
        std::printf("        far portals: %s/%s no longer leads to %s %s/%s: %s\n", e.room.c_str(), e.name.c_str(),
                    e.fr.c_str(), e.far_room.c_str(), e.far_name.c_str(), why.c_str());
        if (tell) tell_unlink(e.fr, e.room, e.name, e.far_room, e.far_name, why);
    }
    e.fr.clear();
    e.far_room.clear();
    e.far_name.clear();
    e.fr_key.clear();
    e.fr_host.clear();
    e.own_key.clear();
    e.fr_port   = 0;
    e.st        = St::None;
    e.far_index = -1;
    e.why       = why;
    g_dirty     = true;
}

// Made, or asked for: who with, kept for when that server is not heard.
void link_to(Entry &e, const std::string &fr, const Offered &o, St st)
{
    const Friend *f = friend_of(fr);
    e.fr        = fr;
    e.far_room  = o.room;
    e.far_name  = o.name;
    e.st        = st;
    e.ask_at    = now();
    e.far_index = o.index;
    e.fr_key    = f ? f->key_hex : std::string();
    e.fr_host   = f ? f->host : std::string();
    e.fr_port   = remote(fr).port;
    e.own_key   = g_key;
    e.why.clear();
    g_dirty = true;
}

bool good_name(const std::string &s)
{
    if (s.empty() || s.size() > 96) return false;
    for (const unsigned char c : s)
        if (c < 32 || c == 127) return false;
    return true;
}

std::vector<std::string> split(const std::string &s, char at)
{
    std::vector<std::string> out;
    std::string              cur;
    for (const char c : s) {
        if (c == at) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<std::string> words(const std::string &csv)
{
    std::vector<std::string> list;
    for (std::string w : split(csv, ',')) {
        w.erase(0, w.find_first_not_of(' '));
        w.erase(w.find_last_not_of(' ') + 1);
        if (!w.empty() && std::find(list.begin(), list.end(), w) == list.end()) list.push_back(w);
    }
    return list;
}

std::string joined(const std::vector<std::string> &list)
{
    std::string out;
    for (const std::string &w : list) out += (out.empty() ? "" : ",") + w;
    return out;
}

std::string flags_of(const Entry &e)
{
    std::vector<std::string> f;
    if (e.ig_offline) f.push_back("offline");
    if (e.ig_protocol) f.push_back("protocol");
    if (e.ig_size) f.push_back("size");
    return joined(f);
}
void set_flags(Entry &e, const std::string &csv)
{
    const std::vector<std::string> f = words(csv);
    const auto has = [&](const char *w) { return std::find(f.begin(), f.end(), w) != f.end(); };
    e.ig_offline  = has("offline");
    e.ig_protocol = has("protocol");
    e.ig_size     = has("size");
}

Offered offer_line(const std::vector<std::string> &w, size_t at)
{
    Offered o;
    o.room      = w[at];
    o.name      = w[at + 1];
    o.index     = std::atoi(w[at + 2].c_str());
    o.hw        = (float)std::atof(w[at + 3].c_str());
    o.hh        = (float)std::atof(w[at + 4].c_str());
    o.st        = w[at + 5];
    o.link_room = w[at + 6];
    o.link_name = w[at + 7];
    if (o.index >= net::kMaxMouths) o.index = -1;
    return o;
}

} // namespace

bool set(const std::string &room, const std::string &name, const Portal &shape, const std::string &offered,
         const std::string &ignore, std::string &why)
{
    if (!good_name(name)) {
        why = "a name of 1 to 96 printable characters";
        return false;
    }
    if (shape.axis == PortalAxis::XZ) {
        why = "a portal to another server stands up";
        return false;
    }
    std::lock_guard<std::mutex> hold(g_m);
    Entry *e     = find(room, name);
    bool   fresh = false;
    if (!e) {
        g_entries.push_back(Entry{});
        e       = &g_entries.back();
        e->room = room;
        e->name = name;
        fresh   = true;
    }
    const bool resized = !fresh && (std::fabs(e->shape.half_w - shape.half_w) > 0.01f ||
                                    std::fabs(e->shape.half_h - shape.half_h) > 0.01f);
    e->shape   = shape;
    e->offered = words(offered);
    set_flags(*e, ignore);
    // Offered no more to the friend it leads to, or a size the far hole is
    // not: the link goes, on both sides.
    if (e->st != St::None && !offers(*e, e->fr)) undo(*e, "it is no longer offered to " + e->fr, true);
    if (e->st != St::None && resized && !e->ig_size) undo(*e, "its hole changed size", true);
    g_dirty = true;
    return true;
}

bool close(const std::string &room, const std::string &name)
{
    std::lock_guard<std::mutex> hold(g_m);
    Entry *e = find(room, name);
    if (!e) return false;
    undo(*e, "closed", true);
    // Its mouth, if it holds one, is given back by the next sync: kept as an
    // entry with nothing offered until then.
    e->offered.clear();
    e->name = "\n";   // a name set() never takes: found by nobody, swept by sync
    return true;
}

std::string link(const std::string &room, const std::string &name, const std::string &fr,
                 const std::string &far_room, const std::string &far_name)
{
    std::lock_guard<std::mutex> hold(g_m);
    Entry *e = find(room, name);
    if (!e) return "no portal to another server called " + name + " here";
    if (fr.empty()) {
        undo(*e, "unlinked here", true);
        return "";
    }
    if (!friend_of(fr)) return "no friend server called " + fr + " shares portals (/friend list)";
    if (!offers(*e, fr)) return "offer " + name + " to " + fr + " first";
    if (e->st != St::None && e->fr == fr && e->far_room == far_room && e->far_name == far_name) return "";
    const Remote  &r = remote(fr);
    const Offered *o = offered_by(r, far_room, far_name);
    if (!o) return fr + " offers this server no portal called " + far_name;
    if (o->st == "busy" || ((o->st == "ask" || o->st == "ok") && !points_at(*o, *e)))
        return far_name + " on " + fr + " is linked to another portal";
    if (const std::string no = compat(fr, r, e, o); !no.empty()) return no;
    if (e->st != St::None) undo(*e, "linked elsewhere", true);
    link_to(*e, fr, *o, points_at(*o, *e) ? St::Ok : St::Ask);
    std::printf("        far portals: %s/%s asks %s for %s/%s%s\n", room.c_str(), name.c_str(), fr.c_str(),
                far_room.c_str(), far_name.c_str(), r.heard < 0.0 ? " (not heard from yet: it waits)" : "");
    return "";
}

std::string state(const std::string &room, const std::string &name)
{
    std::lock_guard<std::mutex> hold(g_m);
    const Entry *e = find(room, name);
    if (!e) return "none";
    if (e->st == St::None) return e->why.empty() ? "none" : "none\t" + e->why;
    return std::string(e->st == St::Ok ? "linked " : "asking ") + e->fr + "\t" + e->far_room + "\t" + e->far_name;
}

std::string list(const std::string &room, const std::string &name)
{
    std::lock_guard<std::mutex> hold(g_m);
    const Entry *e = find(room, name);
    std::string  out;
    for (const Friend &f : g_friends) {
        const Remote     &r  = remote(f.name);
        const std::string no = compat(f.name, r, nullptr, nullptr);
        out += "S\t" + f.name + "\t" + (no.empty() ? "ok" : no) + "\t" + r.engine + "\n";
        for (const Offered &o : r.portals) {
            std::string st = o.st == "free" ? "free" : "busy";
            if (e && points_at(o, *e)) st = "yours";
            out += "P\t" + f.name + "\t" + o.room + "\t" + o.name + "\t" + st + "\t" +
                   (e ? compat(f.name, r, e, &o) : std::string()) + "\n";
        }
    }
    return out;
}

void set_self(int udp_port)
{
    std::lock_guard<std::mutex> hold(g_m);
    g_port = udp_port;
}

void set_friends(const std::vector<Friend> &friends)
{
    std::lock_guard<std::mutex> hold(g_m);
    g_friends = friends;
    for (Entry &e : g_entries) {
        if (e.st == St::None) continue;
        const Friend *f = friend_of(e.fr);
        if (!f)
            undo(e, e.fr + " is no longer a friend sharing portals", false);
        else if (!e.fr_key.empty() && f->key_hex != e.fr_key)
            undo(e, "the signature of " + e.fr + " changed", true);
        else if (e.fr_host != f->host)
            e.fr_host = f->host, g_dirty = true;   // /friend add gave it another address
    }
    g_remote.erase(std::remove_if(g_remote.begin(), g_remote.end(),
                                  [](const auto &r) { return friend_of(r.first) == nullptr; }),
                   g_remote.end());
}

void load(const std::string &my_key, const std::string &kept)
{
    std::lock_guard<std::mutex> hold(g_m);
    g_key = my_key;
    std::istringstream in(kept);
    Remote            *r = nullptr;
    int entries = 0, links = 0, dropped = 0;
    for (std::string l; std::getline(in, l);) {
        const std::vector<std::string> w = split(l, '\t');
        if (w[0] == "R" && w.size() >= 3) {
            r = &remote(w[1]);
            std::istringstream h(w[2]);
            h >> r->port >> r->pmin >> r->pmax >> r->engine;
            r->portals.clear();
        } else if (w[0] == "O" && w.size() >= 9 && r) {
            r->portals.push_back(offer_line(w, 1));
        } else if (w[0] == "E" && w.size() >= 17 && good_name(w[2])) {
            Entry  loaded;
            Entry *e = find(w[1], w[2]);
            if (!e) {
                loaded.room = w[1];
                loaded.name = w[2];
                std::istringstream sh(w[3]);
                int                axis = 0;
                sh >> loaded.shape.center.x >> loaded.shape.center.y >> loaded.shape.center.z >> axis >>
                    loaded.shape.normal_sign >> loaded.shape.half_w >> loaded.shape.half_h;
                loaded.shape.axis = axis == 1 ? PortalAxis::YZ : PortalAxis::XY;
                loaded.offered    = words(w[4]);
                set_flags(loaded, w[5]);
                e = &loaded;
            }
            ++entries;
            if (w[6] == "ok" || w[6] == "ask") {
                if (w[15] != my_key) {
                    ++dropped;
                    e->why = "this server's signature changed";
                } else if (e->st == St::None) {
                    e->st        = w[6] == "ok" ? St::Ok : St::Ask;
                    e->ask_at    = now();
                    e->fr        = w[7];
                    e->fr_key    = w[8];
                    e->fr_host   = w[9];
                    e->fr_port   = std::atoi(w[10].c_str());
                    e->far_room  = w[11];
                    e->far_name  = w[12];
                    e->far_index = std::atoi(w[13].c_str());
                    e->own_key   = w[15];
                    ++links;
                }
            }
            if (e == &loaded) g_entries.push_back(loaded);
        }
    }
    if (entries > 0)
        std::printf("        far portals: %d kept, %d link(s) as they were%s\n", entries, links,
                    dropped > 0 ? (", " + std::to_string(dropped) + " dropped: this server's signature changed").c_str()
                                : "");
    g_dirty = true;
}

std::string dump()
{
    std::lock_guard<std::mutex> hold(g_m);
    std::string out;
    for (const auto &[name, r] : g_remote) {
        out += "R\t" + name + "\t" + std::to_string(r.port) + " " + std::to_string(r.pmin) + " " +
               std::to_string(r.pmax) + " " + (r.engine.empty() ? "?" : r.engine) + "\n";
        for (const Offered &o : r.portals) {
            char size[64];
            std::snprintf(size, sizeof(size), "%.3f\t%.3f", (double)o.hw, (double)o.hh);
            out += "O\t" + o.room + "\t" + o.name + "\t" + std::to_string(o.index) + "\t" + size + "\t" + o.st +
                   "\t" + o.link_room + "\t" + o.link_name + "\n";
        }
    }
    for (const Entry &e : g_entries) {
        if (e.name == "\n") continue;
        char shape[160];
        std::snprintf(shape, sizeof(shape), "%.3f %.3f %.3f %d %d %.3f %.3f", (double)e.shape.center.x,
                      (double)e.shape.center.y, (double)e.shape.center.z, e.shape.axis == PortalAxis::YZ ? 1 : 0,
                      e.shape.normal_sign, (double)e.shape.half_w, (double)e.shape.half_h);
        const char *st = e.st == St::Ok ? "ok" : e.st == St::Ask ? "ask" : "none";
        out += "E\t" + e.room + "\t" + e.name + "\t" + shape + "\t" + joined(e.offered) + "\t" + flags_of(e) +
               "\t" + st + "\t" + e.fr + "\t" + e.fr_key + "\t" + e.fr_host + "\t" + std::to_string(e.fr_port) +
               "\t" + e.far_room + "\t" + e.far_name + "\t" + std::to_string(e.far_index) + "\t-\t" + e.own_key +
               "\t-\n";
    }
    return out;
}

std::string offer_for(const std::string &fr)
{
    std::lock_guard<std::mutex> hold(g_m);
    std::string out = "portals\n" + std::to_string(g_port) + " " + std::to_string(net::kMinProtocol) + " " +
                      std::to_string(net::kProtocol) + " " + kEngineVersion + "\n";
    for (const Entry &e : g_entries) {
        if (e.name == "\n" || (!offers(e, fr) && e.fr != fr)) continue;
        std::string st = "free";
        if (e.st != St::None) st = e.fr != fr ? "busy" : e.st == St::Ok ? "ok" : "ask";
        char size[64];
        std::snprintf(size, sizeof(size), "%.3f\t%.3f", (double)e.shape.half_w, (double)e.shape.half_h);
        out += e.room + "\t" + e.name + "\t" + std::to_string(e.index) + "\t" + size + "\t" + st + "\t" +
               (st == "busy" ? std::string() : e.far_room) + "\t" + (st == "busy" ? std::string() : e.far_name) +
               "\n";
    }
    return out;
}

// A line from `fr` signed by `key`: links made with another key of its come
// undone, before anything it says is read.
void rekey(const std::string &fr, const std::string &key)
{
    for (Friend &f : g_friends)
        if (f.name == fr) f.key_hex = key;
    for (Entry &e : g_entries)
        if (e.st != St::None && e.fr == fr && !e.fr_key.empty() && e.fr_key != key)
            undo(e, "the signature of " + fr + " changed", true);
}

void offer_in(const std::string &fr, const std::string &key, const std::string &body)
{
    std::lock_guard<std::mutex> hold(g_m);
    if (!friend_of(fr)) return;
    rekey(fr, key);
    Remote            &r = remote(fr);
    std::istringstream in(body);
    std::string        head;
    std::getline(in, head);
    {
        std::istringstream h(head);
        h >> r.port >> r.pmin >> r.pmax >> r.engine;
    }
    r.portals.clear();
    for (std::string l; std::getline(in, l);) {
        const std::vector<std::string> w = split(l, '\t');
        if (w.size() < 8 || !good_name(w[1]) || r.portals.size() >= 256) continue;
        r.portals.push_back(offer_line(w, 0));
    }
    r.heard = now();
    g_save = true;   // what it offers, kept for a friend that is off after a restart

    // Ours that lead there. Asked: taken, or refused by its being linked to
    // another. Linked: kept whatever the offer says -- only a portals_unlink,
    // or a key that is not the one it was made with, undoes a made link.
    for (Entry &e : g_entries) {
        if (e.st == St::None || e.fr != fr) continue;
        if (r.port > 0 && e.fr_port != r.port) e.fr_port = r.port, g_dirty = true;
        const Offered *o = offered_by(r, e.far_room, e.far_name);
        if (o && points_at(*o, e)) {
            if (e.st == St::Ask)
                std::printf("        far portals: %s/%s and %s %s/%s are linked\n", e.room.c_str(), e.name.c_str(),
                            fr.c_str(), e.far_room.c_str(), e.far_name.c_str());
            if (e.st == St::Ask || e.far_index != o->index) g_dirty = true;
            e.st        = St::Ok;
            e.far_index = o->index;
        } else if (o && e.st == St::Ask && o->st != "free") {
            undo(e, e.far_name + " on " + fr + " is linked to another portal", false);
        }
    }
    // Theirs that point at ours. Asked: taken when ours is offered to them,
    // free, and fits by its own checks; else they are told why. Said linked
    // to one of ours that is not linked back: they are told so, which undoes
    // it there -- the side that let go while the other was off.
    for (const Offered &o : r.portals) {
        if (o.st != "ask" && o.st != "ok") continue;
        Entry *e = find(o.link_room, o.link_name);
        if (e && leads_to(*e, fr, o)) continue;
        std::string no;
        if (!e)
            no = "there is no portal " + o.link_name + " there any more";
        else if (e->st != St::None)
            no = o.link_name + " there is linked to another portal";
        else if (o.st == "ok")
            no = o.link_name + " there let go of it";
        else if (!offers(*e, fr))
            no = o.link_name + " there is not offered to this server";
        else
            no = compat(fr, r, e, &o);
        if (!no.empty()) {
            std::printf("        far portals: %s %s/%s points at %s/%s: told no, %s\n", fr.c_str(), o.room.c_str(),
                        o.name.c_str(), o.link_room.c_str(), o.link_name.c_str(), no.c_str());
            tell_unlink(fr, o.link_room, o.link_name, o.room, o.name, "refused there: " + no);
            continue;
        }
        link_to(*e, fr, o, St::Ok);
        std::printf("        far portals: %s/%s taken by %s %s/%s\n", e->room.c_str(), e->name.c_str(), fr.c_str(),
                    o.room.c_str(), o.name.c_str());
    }
}

void unlink_in(const std::string &fr, const std::string &key, const std::string &body)
{
    std::lock_guard<std::mutex> hold(g_m);
    if (friend_of(fr)) rekey(fr, key);
    std::istringstream in(body);
    std::string        l;
    std::getline(in, l);
    const std::vector<std::string> w = split(l, '\t');
    if (w.size() < 4 || !friend_of(fr)) return;
    // Theirs first, then ours.
    Entry *e = find(w[2], w[3]);
    if (!e || e->st == St::None || e->fr != fr || e->far_room != w[0] || e->far_name != w[1]) return;
    undo(*e, w.size() > 4 && !w[4].empty() ? fr + ": " + w[4].substr(0, 200) : w[1] + " on " + fr + " let go",
         false);
}

std::vector<std::pair<std::string, std::string>> take_unlinks()
{
    std::lock_guard<std::mutex> hold(g_m);
    std::vector<std::pair<std::string, std::string>> out;
    out.swap(g_unlinks);
    return out;
}

bool take_dirty()
{
    std::lock_guard<std::mutex> hold(g_m);
    const bool was = g_dirty;
    g_dirty        = false;
    g_save         = g_save || was;
    return was;
}

bool take_save()
{
    std::lock_guard<std::mutex> hold(g_m);
    const bool was = g_save;
    g_save         = false;
    return was;
}

std::vector<Mouth> sync(const std::string &room, const std::function<bool(int)> &leads_away,
                        std::vector<int> &freed, std::vector<std::string> &trusted)
{
    std::lock_guard<std::mutex> hold(g_m);
    const double t = now();
    freed.clear();
    trusted.clear();

    bool held[net::kMaxMouths] = {};
    for (Entry &e : g_entries) {
        if (e.room != room) continue;
        if (e.st == St::Ask && !e.ig_offline && t - e.ask_at > kAskFor)
            undo(e, e.fr + " did not take the link in a minute (is " + e.name + " offered there, and does " +
                        e.far_name + " fit?)",
                 true);
        if (e.st == St::None && e.index >= 0) {
            freed.push_back(e.index);
            e.index = -1;
            g_dirty = true;
        }
        if (e.index >= 0) held[e.index] = true;
    }
    g_entries.erase(std::remove_if(g_entries.begin(), g_entries.end(),
                                   [](const Entry &e) { return e.name == "\n" && e.index < 0; }),
                    g_entries.end());
    const auto was_freed = [&](int k) { return std::find(freed.begin(), freed.end(), k) != freed.end(); };
    const auto is_free   = [&](int k) { return !held[k] && (!leads_away(k) || was_freed(k)); };

    // A mouth for each that wants one, the lowest free.
    for (Entry &e : g_entries) {
        if (e.room != room || e.st == St::None || e.index >= 0) continue;
        int k = 0;
        while (k < net::kMaxMouths && !is_free(k)) ++k;
        if (k == net::kMaxMouths) {
            undo(e, "all of this world's " + std::to_string(net::kMaxMouths) + " mouths are in use", true);
            continue;
        }
        e.index = k;
        held[k] = true;
        g_dirty = true;
    }
    // No hole below a mouth: a world's mouths are its first openings, and the
    // wire says how many there are. The highest moves down.
    for (;;) {
        int hole = 0;
        while (hole < net::kMaxMouths && !is_free(hole)) ++hole;
        Entry *top = nullptr;
        for (Entry &e : g_entries)
            if (e.room == room && e.index > hole && (!top || e.index > top->index)) top = &e;
        if (!top) break;
        held[top->index] = false;
        freed.push_back(top->index);
        top->index = hole;
        held[hole] = true;
        g_dirty    = true;
    }
    freed.erase(std::remove_if(freed.begin(), freed.end(), [&](int k) { return held[k]; }), freed.end());

    // Where the friend is: what /friend add says now, else what it was when
    // linked -- a friend that is off, or a server started without
    // --federation, still has its mouth.
    std::vector<Mouth> out;
    for (const Entry &e : g_entries) {
        if (e.room != room || e.index < 0) continue;
        const Friend *f = friend_of(e.fr);
        Mouth         m;
        m.index    = e.index;
        m.shape    = e.shape;
        m.host     = f ? f->host : e.fr_host;
        m.far_room = e.far_room;
        const std::string key = f ? f->key_hex : e.fr_key;
        const Remote     &r   = remote(e.fr);
        m.port      = (uint16_t)std::max(0, r.port > 0 ? r.port : e.fr_port);
        m.far_index = (uint16_t)std::max(0, e.far_index);
        m.open      = e.st == St::Ok && e.far_index >= 0 && m.port != 0 && !m.host.empty() &&
                 net::from_hex(key, m.key, sizeof(m.key));
        if (m.open && std::find(trusted.begin(), trusted.end(), key) == trusted.end()) trusted.push_back(key);
        out.push_back(m);
    }
    return out;
}

std::vector<std::string> lines()
{
    std::lock_guard<std::mutex> hold(g_m);
    std::vector<std::string> out;
    for (const Entry &e : g_entries) {
        if (e.name == "\n") continue;
        std::string l = (e.room.empty() ? std::string("this world") : e.room) + ", server portal '" + e.name + "'";
        if (e.index >= 0) l += " (opening " + std::to_string(e.index) + ")";
        l += ": offered to " + (e.offered.empty() ? std::string("nobody") : joined(e.offered));
        if (e.st != St::None)
            l += std::string(e.st == St::Ok ? ", linked to " : ", asking ") + e.fr + " " + e.far_room + "/" +
                 e.far_name;
        else if (!e.why.empty())
            l += ", leads nowhere (" + e.why + ")";
        if (const std::string f = flags_of(e); !f.empty()) l += "; checks ignored: " + f;
        out.push_back(l);
    }
    return out;
}

} // namespace farp
} // namespace cnp
