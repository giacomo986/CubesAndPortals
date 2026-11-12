#include "server/room_portals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>

namespace cnp {
namespace roomp {

namespace {

struct Entry {
    std::string room, name;
    Portal      shape;
    std::string to_room, to_name;   // what it wishes to lead to; empty for nothing
    int         index = -1;         // the mouth it has in its room now
    float       near  = 1e30f;      // its room's nearest player, as last measured
};

std::mutex               g_m;
std::vector<Entry>       g_entries;
std::vector<std::string> g_rooms;

Entry *find(const std::string &room, const std::string &name)
{
    for (Entry &e : g_entries)
        if (e.room == room && e.name == name) return &e;
    return nullptr;
}

// The entry e is linked with: the one it names, naming it back.
Entry *partner(const Entry &e)
{
    if (e.to_room.empty()) return nullptr;
    Entry *p = find(e.to_room, e.to_name);
    return p && p->to_room == e.room && p->to_name == e.name ? p : nullptr;
}

bool good_name(const std::string &s)
{
    if (s.empty() || s.size() > 96) return false;
    for (const unsigned char c : s)
        if (c < 32 || c == 127) return false;
    return true;
}

bool same_size(const Portal &a, const Portal &b)
{
    return std::fabs(a.half_w - b.half_w) < 0.01f && std::fabs(a.half_h - b.half_h) < 0.01f;
}

std::string size_text(const Portal &p)
{
    char b[48];
    std::snprintf(b, sizeof(b), "%g x %g", (double)p.half_w * 2.0, (double)p.half_h * 2.0);
    return b;
}

// e leads to nothing, and whatever led to it leads to nothing.
void unlink(Entry &e)
{
    if (Entry *p = partner(e)) {
        p->to_room.clear();
        p->to_name.clear();
    }
    e.to_room.clear();
    e.to_name.clear();
}

// Whether the server has, or makes, a room of that name: one of its rooms, or
// a template's ("home-*", set_rooms) with something after the star's place.
bool known_room(const std::string &room)
{
    for (const std::string &r : g_rooms) {
        if (r == room) return true;
        if (!r.empty() && r.back() == '*' && room.size() >= r.size() && room.compare(0, r.size() - 1, r, 0, r.size() - 1) == 0)
            return true;
    }
    return false;
}

// Why `e` cannot be linked to `o`, or "".
std::string compat(const Entry &e, const Entry &o)
{
    if (!same_size(e.shape, o.shape))
        return "it is " + size_text(o.shape) + " and this one " + size_text(e.shape);
    return "";
}

} // namespace

void set_rooms(const std::vector<std::string> &rooms)
{
    std::lock_guard<std::mutex> hold(g_m);
    g_rooms = rooms;
}

bool set(const std::string &room, const std::string &name, const Portal &shape, std::string &why)
{
    std::lock_guard<std::mutex> hold(g_m);
    if (!good_name(name)) {
        why = "a portal's name is 1 to 96 printable characters";
        return false;
    }
    if (Entry *e = find(room, name)) {
        // Another size and it fits what it led to no longer.
        if (!same_size(e->shape, shape)) unlink(*e);
        e->shape = shape;
        return true;
    }
    Entry e;
    e.room  = room;
    e.name  = name;
    e.shape = shape;
    // Somebody was waiting for it (a link said before this room's mod had set
    // its portals again): it takes the first that fits.
    for (const Entry &w : g_entries)
        if (w.to_room == room && w.to_name == name && same_size(w.shape, shape)) {
            e.to_room = w.room;
            e.to_name = w.name;
            break;
        }
    g_entries.push_back(e);
    return true;
}

bool close(const std::string &room, const std::string &name)
{
    std::lock_guard<std::mutex> hold(g_m);
    Entry *e = find(room, name);
    if (!e) return false;
    unlink(*e);
    g_entries.erase(g_entries.begin() + (e - g_entries.data()));
    return true;
}

std::string link(const std::string &room, const std::string &name, const std::string &far_room,
                 const std::string &far_name)
{
    std::lock_guard<std::mutex> hold(g_m);
    Entry *e = find(room, name);
    if (!e) return "there is no portal called " + name + " here";
    if (far_room.empty()) {
        unlink(*e);
        return "";
    }
    if (far_room == room) return "that portal is in this room: link the two frames as a pair";
    if (!known_room(far_room)) return "this server has no room called " + far_room;
    if (!good_name(far_name)) return "a portal's name is 1 to 96 printable characters";
    Entry *o = find(far_room, far_name);
    if (o) {
        const std::string no = compat(*e, *o);
        if (!no.empty()) return no;
    }
    unlink(*e);
    if (o) unlink(*o);
    e = find(room, name);   // unlink moved nothing, but say so by looking again
    e->to_room = far_room;
    e->to_name = far_name;
    if (o) {
        o->to_room = room;
        o->to_name = name;
    }
    return "";
}

std::string state(const std::string &room, const std::string &name)
{
    std::lock_guard<std::mutex> hold(g_m);
    const Entry *e = find(room, name);
    if (!e || e->to_room.empty()) return "none";
    const Entry *p = partner(*e);
    if (!p) return "waiting " + e->to_room + "\t" + e->to_name;
    return "linked " + e->to_room + "\t" + e->to_name + "\t" + (e->index >= 0 && p->index >= 0 ? "live" : "idle");
}

std::string list(const std::string &room, const std::string &name)
{
    std::lock_guard<std::mutex> hold(g_m);
    const Entry *mine = find(room, name);
    std::string  out;
    for (const std::string &r : g_rooms) {
        if (r == room || (!r.empty() && r.back() == '*')) continue;   // a template's rooms are whoever's they are
        out += "R\t" + r + "\n";
        std::vector<const Entry *> there;
        for (const Entry &e : g_entries)
            if (e.room == r) there.push_back(&e);
        std::sort(there.begin(), there.end(), [](const Entry *a, const Entry *b) { return a->name < b->name; });
        for (const Entry *e : there) {
            const bool yours = mine && e->to_room == room && e->to_name == mine->name && partner(*e) == mine;
            out += "P\t" + r + "\t" + e->name + "\t" + (yours ? "yours" : partner(*e) ? "busy" : "free") + "\t" +
                   (mine ? compat(*mine, *e) : std::string()) + "\n";
        }
    }
    return out;
}

std::vector<Item> items(const std::string &room, const std::function<float(Vec3)> &near)
{
    std::lock_guard<std::mutex> hold(g_m);
    std::vector<Item> out;
    for (Entry &e : g_entries) {
        if (e.room != room) continue;
        e.near = near(e.shape.center);
        const Entry *p = partner(e);
        if (!p) continue;
        Item it;
        it.name      = e.name;
        it.shape     = e.shape;
        it.far_room  = p->room;
        it.far_name  = p->name;
        it.far_index = p->index;
        it.index     = e.index;
        it.key       = std::min(e.near, p->near);
        out.push_back(it);
    }
    return out;
}

std::vector<std::string> wanted_rooms(const std::string &room, float within)
{
    std::lock_guard<std::mutex> hold(g_m);
    std::vector<std::string> out;
    for (const Entry &e : g_entries)
        if (e.room == room && !e.to_room.empty() && e.near <= within &&
            std::find(out.begin(), out.end(), e.to_room) == out.end())
            out.push_back(e.to_room);
    return out;
}

void forget_room(const std::string &room)
{
    std::lock_guard<std::mutex> hold(g_m);
    g_entries.erase(std::remove_if(g_entries.begin(), g_entries.end(), [&](const Entry &e) { return e.room == room; }),
                    g_entries.end());
}

void took(const std::string &room, const std::string &name, int index)
{
    std::lock_guard<std::mutex> hold(g_m);
    if (Entry *e = find(room, name)) e->index = index;
}

std::vector<std::string> lines()
{
    std::lock_guard<std::mutex> hold(g_m);
    std::vector<std::string> out;
    for (const Entry &e : g_entries) {
        char at[96];
        std::snprintf(at, sizeof(at), "%.1f %.1f %.1f", (double)e.shape.center.x, (double)e.shape.center.y,
                      (double)e.shape.center.z);
        const Entry *p = partner(e);
        out.push_back("room portal " + (e.room.empty() ? std::string("") : e.room + "/") + e.name + " at " + at +
                      (p ? " leads to " + p->room + "/" + p->name +
                               (e.index >= 0 ? ", mouth " + std::to_string(e.index) : ", no mouth now")
                         : e.to_room.empty() ? std::string(" leads nowhere")
                                             : " waits for " + e.to_room + "/" + e.to_name));
    }
    return out;
}

} // namespace roomp
} // namespace cnp
