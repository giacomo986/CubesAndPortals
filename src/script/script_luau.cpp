// Luau backend for script.h.
//
// Luau is the primary runtime because it is the only modern Lua designed to run
// code from strangers: globals frozen by default, no getfenv/setfenv/loadstring,
// no io/os/package to remove because they were never there, and an interrupt
// hook plus a memory ceiling as runtime primitives rather than conventions
// (DESIGN.md 5.2, 5.3).
//
// The bindings below are registered by hand. That is fine for M0's handful;
// M2 replaces it with the generator of DESIGN.md 5.4, which emits the C++ glue,
// the .d.luau type definitions and the documentation from one description
// table, so that adding an API call never costs fifty lines of boilerplate.

#include "script/script.h"

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {

struct VmState {
    lua_State  *L = nullptr;   // main state: globals frozen, shared library code
    lua_State  *T = nullptr;   // the mod's thread: its own writable globals
    int         thread_ref = -1;
    script_host host{};

    size_t memory_limit = 0;
    size_t memory_used  = 0;

    unsigned interrupt_budget = 0;
    unsigned interrupt_count  = 0;
    bool     interrupted      = false;

    int chunks_loaded = 0;
};

// Allocator that enforces the quota. Returning null makes Lua raise a normal
// "not enough memory" error inside the offending script instead of taking the
// process down with it.
void *vm_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    VmState *vm = static_cast<VmState *>(ud);

    if (nsize == 0) {
        vm->memory_used -= osize;
        std::free(ptr);
        return nullptr;
    }

    if (vm->memory_limit > 0) {
        const size_t after = vm->memory_used - osize + nsize;
        if (after > vm->memory_limit && nsize > osize) return nullptr;
    }

    void *np = std::realloc(ptr, nsize);
    if (!np) return nullptr;

    vm->memory_used = vm->memory_used - osize + nsize;
    return np;
}

void vm_interrupt(lua_State *L, int gc)
{
    if (gc >= 0) return;   // a garbage-collection step, not a script safepoint

    VmState *vm = static_cast<VmState *>(lua_getthreaddata(lua_mainthread(L)));
    if (!vm || vm->interrupt_budget == 0) return;

    if (++vm->interrupt_count > vm->interrupt_budget) {
        vm->interrupted = true;
        // Raising here unwinds the script. The VM stays usable afterwards,
        // which is the whole point: one runaway mod must not end the tick.
        luaL_error(L, "script exceeded its interrupt budget");
    }
}

VmState *host_of(lua_State *L)
{
    return static_cast<VmState *>(lua_getthreaddata(lua_mainthread(L)));
}

// The glue for the scalar half of the API is generated from src/script/api.luau
// by tools/api_gen (DESIGN.md 5.4): argument checking, defaults, return values
// and the registration calls. Adding a function there costs six lines and
// updates the .d.luau and the reference documentation at the same time, which
// is the difference between an API somebody extends and one they work around.
#include "script/api_generated.inc"

// ------------------------------------------------------------------ events
// A payload crosses as an array of script_field. Building it from a Lua table
// costs a walk of that table, which is the price of a boundary the mod cannot
// reach past -- the alternative is handing a lua_State to the engine, and then
// "one VM per mod" stops meaning anything.

constexpr int kMaxFields = 32;

// Scratch for one emit or one handler call. Not re-entrant, and it does not
// need to be: an event handler that emits another event runs to completion
// inside the emit that called it, and the bus above serialises those.
struct FieldScratch {
    script_field fields[kMaxFields];
    std::string  names[kMaxFields];
    std::string  strings[kMaxFields];
    int          count = 0;
};

// Reads a table at `index` into scratch. Types come from the values themselves;
// the schema check happens in the bus, which is the only place that knows what
// the event was declared to carry.
bool read_table_fields(lua_State *L, int index, FieldScratch &out, const char **err)
{
    out.count = 0;

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (out.count >= kMaxFields) {
            lua_pop(L, 2);
            *err = "too many fields in an event payload";
            return false;
        }
        if (lua_type(L, -2) != LUA_TSTRING) {
            lua_pop(L, 2);
            *err = "event payload keys must be strings";
            return false;
        }

        const int i = out.count;
        out.names[i] = lua_tostring(L, -2);

        script_field &f = out.fields[i];
        f.name = nullptr;   // filled in after the walk: the strings may move
        f.str  = nullptr;

        switch (lua_type(L, -1)) {
            case LUA_TNUMBER:
                f.type = SCRIPT_V_NUMBER;
                f.num  = lua_tonumber(L, -1);
                break;
            case LUA_TBOOLEAN:
                f.type = SCRIPT_V_BOOL;
                f.b    = lua_toboolean(L, -1);
                break;
            case LUA_TSTRING:
                f.type       = SCRIPT_V_STRING;
                out.strings[i] = lua_tostring(L, -1);
                break;
            default:
                lua_pop(L, 2);
                *err = "event fields must be numbers, strings or booleans";
                return false;
        }

        ++out.count;
        lua_pop(L, 1);
    }

    for (int i = 0; i < out.count; ++i) {
        out.fields[i].name = out.names[i].c_str();
        if (out.fields[i].type == SCRIPT_V_STRING) out.fields[i].str = out.strings[i].c_str();
    }
    return true;
}

void push_fields_table(lua_State *L, const script_field *fields, int count, int cancellable)
{
    lua_newtable(L);
    for (int i = 0; i < count; ++i) {
        switch (fields[i].type) {
            case SCRIPT_V_NUMBER: lua_pushnumber(L, fields[i].num); break;
            case SCRIPT_V_BOOL:   lua_pushboolean(L, fields[i].b); break;
            case SCRIPT_V_STRING: lua_pushstring(L, fields[i].str ? fields[i].str : ""); break;
        }
        lua_setfield(L, -2, fields[i].name);
    }
    if (cancellable) {
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "cancel");
    }
}

int l_event_define(lua_State *L)
{
    VmState *vm = host_of(L);
    const char *event = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    // { fields = { power = "number" }, cancellable = true }
    lua_getfield(L, 2, "cancellable");
    const int cancellable = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "fields");
    if (!lua_istable(L, -1)) luaL_error(L, "core.event.define needs a `fields` table");

    script_field fields[kMaxFields];
    std::string  names[kMaxFields];
    int          count = 0;

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (count >= kMaxFields) luaL_error(L, "too many fields declared for '%s'", event);

        names[count] = luaL_checkstring(L, -2);
        const char *type_name = luaL_checkstring(L, -1);

        script_vtype t;
        if      (std::strcmp(type_name, "number") == 0) t = SCRIPT_V_NUMBER;
        else if (std::strcmp(type_name, "string") == 0) t = SCRIPT_V_STRING;
        else if (std::strcmp(type_name, "boolean") == 0) t = SCRIPT_V_BOOL;
        else luaL_error(L, "field '%s' of '%s': unknown type '%s'",
                        names[count].c_str(), event, type_name);

        fields[count].type = t;
        ++count;
        lua_pop(L, 1);
    }
    for (int i = 0; i < count; ++i) fields[i].name = names[i].c_str();

    char err[256] = {0};
    if (!vm || !vm->host.event_define ||
        !vm->host.event_define(vm->host.ud, event, fields, count, cancellable, err, sizeof(err)))
        luaL_error(L, "%s", err[0] ? err : "core.event.define failed");

    return 0;
}

int l_event_on(lua_State *L)
{
    VmState *vm = host_of(L);
    const char *event = luaL_checkstring(L, 1);

    // core.event.on(name, fn) and core.event.on(name, priority, fn) both work:
    // a priority is something most handlers do not care about.
    int priority = 0;
    int fn_index = 2;
    if (lua_isnumber(L, 2)) {
        priority = (int)lua_tointeger(L, 2);
        fn_index = 3;
    }
    luaL_checktype(L, fn_index, LUA_TFUNCTION);

    lua_pushvalue(L, fn_index);
    const int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    char err[256] = {0};
    if (!vm || !vm->host.event_on ||
        !vm->host.event_on(vm->host.ud, event, priority, ref, err, sizeof(err))) {
        lua_unref(L, ref);
        luaL_error(L, "%s", err[0] ? err : "core.event.on failed");
    }

    lua_pushinteger(L, ref);
    return 1;
}

int l_event_emit(lua_State *L)
{
    VmState *vm = host_of(L);
    const char *event = luaL_checkstring(L, 1);

    FieldScratch scratch;
    if (lua_istable(L, 2)) {
        const char *why = nullptr;
        if (!read_table_fields(L, 2, scratch, &why)) luaL_error(L, "%s", why);
    }

    char err[256] = {0};
    int  cancelled = 0;
    if (!vm || !vm->host.event_emit ||
        !vm->host.event_emit(vm->host.ud, event, scratch.fields, scratch.count, &cancelled,
                             err, sizeof(err)))
        luaL_error(L, "%s", err[0] ? err : "core.event.emit failed");

    lua_pushboolean(L, cancelled);
    return 1;
}

// --------------------------------------------------------------------- ecs
// Hand-written, like core.event and for the same two reasons: one of these
// takes a FUNCTION, which the generator has no shape for, and the ids are
// doubles rather than ints -- an entity id carries a generation above its
// index, so it leaves 32 bits behind the first time anything is recycled and
// luaL_checkinteger would quietly truncate it.

constexpr int kMaxComponentFields = 16;

// core.register_mapgen(name, fn) -- hand-written, because the generator emits
// scalars and this takes a function.
//
// The ref is taken BEFORE the host is asked, and released if the host refuses.
// The other order leaks a ref on every refusal, and the refusals here are the
// ordinary case: a mod loaded after the world was generated gets told no.
int l_register_mapgen(lua_State *L)
{
    VmState    *vm   = host_of(L);
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushvalue(L, 2);
    const int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    char err[256] = {0};
    if (!vm || !vm->host.register_mapgen ||
        !vm->host.register_mapgen(vm->host.ud, name, ref, err, sizeof(err))) {
        lua_unref(L, ref);
        luaL_error(L, "%s", err[0] ? err : "core.register_mapgen failed");
    }
    return 0;
}

// A modifier's block list: however many names, as a Lua array of strings. One
// name is the common case and a table of one is what a mod writes for it, so
// this accepts a bare string too -- the alternative is every registration in
// every mod carrying braces for a list of one.
constexpr int kMaxModifierNodes = 32;

int read_modifier_nodes(lua_State *L, int arg, const char *name, std::string *out,
                        const char **ptrs)
{
    int count = 0;

    if (lua_isstring(L, arg)) {
        out[0] = lua_tostring(L, arg);
        ptrs[0] = out[0].c_str();
        return 1;
    }

    luaL_checktype(L, arg, LUA_TTABLE);
    for (int i = 1;; ++i) {
        lua_rawgeti(L, arg, i);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            break;
        }
        if (!lua_isstring(L, -1))
            luaL_error(L, "'%s' names its blocks as strings", name);
        if (count >= kMaxModifierNodes)
            luaL_error(L, "'%s' names more than %d blocks", name, kMaxModifierNodes);
        out[count] = lua_tostring(L, -1);
        ++count;
        lua_pop(L, 1);
    }

    for (int i = 0; i < count; ++i) ptrs[i] = out[i].c_str();
    return count;
}

// core.register_abm(name, nodes, interval, chance, action)
//
// The ref is taken BEFORE the host is asked and released if the host refuses,
// which is register_mapgen's rule and for the same reason: the refusals are the
// ordinary case here too.
int l_register_abm(lua_State *L)
{
    VmState    *vm   = host_of(L);
    const char *name = luaL_checkstring(L, 1);

    std::string names[kMaxModifierNodes];
    const char *ptrs[kMaxModifierNodes] = {nullptr};
    const int   count = read_modifier_nodes(L, 2, name, names, ptrs);

    const double interval = luaL_checknumber(L, 3);
    const int    chance   = (int)luaL_checkinteger(L, 4);
    luaL_checktype(L, 5, LUA_TFUNCTION);

    lua_pushvalue(L, 5);
    const int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    char err[256] = {0};
    if (!vm || !vm->host.register_abm ||
        !vm->host.register_abm(vm->host.ud, name, ptrs, count, interval, chance, ref, err,
                               sizeof(err))) {
        lua_unref(L, ref);
        luaL_error(L, "%s", err[0] ? err : "core.register_abm failed");
    }
    return 0;
}

// core.register_lbm(name, nodes, action)
int l_register_lbm(lua_State *L)
{
    VmState    *vm   = host_of(L);
    const char *name = luaL_checkstring(L, 1);

    std::string names[kMaxModifierNodes];
    const char *ptrs[kMaxModifierNodes] = {nullptr};
    const int   count = read_modifier_nodes(L, 2, name, names, ptrs);

    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    const int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    char err[256] = {0};
    if (!vm || !vm->host.register_lbm ||
        !vm->host.register_lbm(vm->host.ud, name, ptrs, count, ref, err, sizeof(err))) {
        lua_unref(L, ref);
        luaL_error(L, "%s", err[0] ? err : "core.register_lbm failed");
    }
    return 0;
}

int l_ecs_component(lua_State *L)
{
    VmState    *vm   = host_of(L);
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    std::string names[kMaxComponentFields];
    const char *ptrs[kMaxComponentFields] = {nullptr};
    int         count = 0;

    for (int i = 1;; ++i) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            break;
        }
        if (!lua_isstring(L, -1)) luaL_error(L, "a component's fields are names, as strings");
        if (count >= kMaxComponentFields)
            luaL_error(L, "'%s' declares more than %d fields", name, kMaxComponentFields);
        names[count] = lua_tostring(L, -1);
        ++count;
        lua_pop(L, 1);
    }
    for (int i = 0; i < count; ++i) ptrs[i] = names[i].c_str();

    char err[256] = {0};
    if (!vm || !vm->host.ecs_component ||
        !vm->host.ecs_component(vm->host.ud, name, ptrs, count, err, sizeof(err)))
        luaL_error(L, "%s", err[0] ? err : "core.ecs.component failed");
    return 0;
}

int l_ecs_phase(lua_State *L)
{
    VmState    *vm    = host_of(L);
    const char *name  = luaL_checkstring(L, 1);
    const int   order = (int)luaL_checkinteger(L, 2);

    char err[256] = {0};
    if (!vm || !vm->host.ecs_phase ||
        !vm->host.ecs_phase(vm->host.ud, name, order, err, sizeof(err)))
        luaL_error(L, "%s", err[0] ? err : "core.ecs.phase failed");
    return 0;
}

int l_ecs_system(lua_State *L)
{
    VmState    *vm        = host_of(L);
    const char *name      = luaL_checkstring(L, 1);
    const char *phase     = luaL_checkstring(L, 2);
    const char *component = luaL_checkstring(L, 3);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    lua_pushvalue(L, 4);
    const int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    char err[256] = {0};
    if (!vm || !vm->host.ecs_system ||
        !vm->host.ecs_system(vm->host.ud, name, phase, component, ref, err, sizeof(err))) {
        lua_unref(L, ref);
        luaL_error(L, "%s", err[0] ? err : "core.ecs.system failed");
    }
    return 0;
}

int l_ecs_spawn(lua_State *L)
{
    VmState    *vm        = host_of(L);
    const char *component = luaL_checkstring(L, 1);
    const double x = luaL_optnumber(L, 2, 0.0);
    const double y = luaL_optnumber(L, 3, 0.0);
    const double z = luaL_optnumber(L, 4, 0.0);

    const double id = (vm && vm->host.ecs_spawn)
                          ? vm->host.ecs_spawn(vm->host.ud, component, x, y, z)
                          : 0.0;
    if (id == 0.0) {
        // Nothing, not zero: a spawn that failed did not make entity 0, and a
        // mod that stored the number would go on setting fields on nobody.
        return 0;
    }
    lua_pushnumber(L, id);
    return 1;
}

int l_ecs_set(lua_State *L)
{
    VmState     *vm    = host_of(L);
    const double id    = luaL_checknumber(L, 1);
    const char  *field = luaL_checkstring(L, 2);
    const double value = luaL_checknumber(L, 3);

    const int ok = (vm && vm->host.ecs_set) ? vm->host.ecs_set(vm->host.ud, id, field, value) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

int l_ecs_get(lua_State *L)
{
    VmState     *vm    = host_of(L);
    const double id    = luaL_checknumber(L, 1);
    const char  *field = luaL_checkstring(L, 2);

    double    out = 0.0;
    const int ok  = (vm && vm->host.ecs_get) ? vm->host.ecs_get(vm->host.ud, id, field, &out) : 0;
    if (!ok) return 0;   // nothing, for the same reason spawn answers with nothing
    lua_pushnumber(L, out);
    return 1;
}

int l_ecs_kill(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double id = luaL_checknumber(L, 1);

    const int ok = (vm && vm->host.ecs_kill) ? vm->host.ecs_kill(vm->host.ud, id) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

int l_ecs_visible(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double id = luaL_checknumber(L, 1);
    const int    on = lua_toboolean(L, 2);

    const int ok = (vm && vm->host.ecs_visible) ? vm->host.ecs_visible(vm->host.ud, id, on) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.ecs.model(id, model) -- what a visible entity is drawn as.
int l_ecs_model(lua_State *L)
{
    VmState     *vm    = host_of(L);
    const double id    = luaL_checknumber(L, 1);
    const int    model = (int)luaL_checkinteger(L, 2);

    const int ok = (vm && vm->host.ecs_model) ? vm->host.ecs_model(vm->host.ud, id, model) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.ecs.health(id, hp?) -- a visible entity's health, given or asked.
int l_ecs_health(lua_State *L)
{
    VmState     *vm  = host_of(L);
    const double id  = luaL_checknumber(L, 1);
    const int    set = lua_isnoneornil(L, 2) ? -1 : (int)luaL_checkinteger(L, 2);

    const int hp = (vm && vm->host.ecs_health) ? vm->host.ecs_health(vm->host.ud, id, set) : -1;
    if (hp < 0) return 0;
    lua_pushinteger(L, hp);
    return 1;
}

// core.craft.make(id, kind, mass, half_w, half_h, half_l, accel, top, turn) --
// a visible thing becomes a machine of that kind.
int l_craft_make(lua_State *L)
{
    VmState     *vm   = host_of(L);
    const double id   = luaL_checknumber(L, 1);
    const char  *kind = luaL_checkstring(L, 2);
    double       n[7];
    for (int i = 0; i < 7; ++i) n[i] = luaL_checknumber(L, 3 + i);
    const int ok = (vm && vm->host.craft_make)
                       ? vm->host.craft_make(vm->host.ud, id, kind, n[0], n[1], n[2], n[3], n[4], n[5], n[6])
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.craft.keys(id, forward, right, up, yaw) -- what its driver asks, -1 to 1.
int l_craft_keys(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double id = luaL_checknumber(L, 1);
    double       k[4];
    for (int i = 0; i < 4; ++i) k[i] = luaL_optnumber(L, 2 + i, 0.0);
    const int ok = (vm && vm->host.craft_keys) ? vm->host.craft_keys(vm->host.ud, id, k[0], k[1], k[2], k[3]) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.craft.pose(id) -- a table: where it is, how it sits, how it moves.
int l_craft_pose(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double id = luaL_checknumber(L, 1);
    double       out[13] = {};
    if (!vm || !vm->host.craft_pose || !vm->host.craft_pose(vm->host.ud, id, out)) return 0;
    static const char *const names[13] = {"x", "y", "z", "yaw", "pitch", "roll", "vx", "vy", "vz",
                                          "speed", "grounded", "impact", "throttle"};
    lua_createtable(L, 0, 13);
    for (int i = 0; i < 13; ++i) {
        if (i == 10)
            lua_pushboolean(L, out[i] != 0.0);
        else
            lua_pushnumber(L, out[i]);
        lua_setfield(L, -2, names[i]);
    }
    return 1;
}

// core.craft.push(id, vx, vy, vz) -- added to its velocity at once.
int l_craft_push(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double id = luaL_checknumber(L, 1);
    const int ok = (vm && vm->host.craft_push)
                       ? vm->host.craft_push(vm->host.ud, id, luaL_checknumber(L, 2), luaL_checknumber(L, 3),
                                             luaL_checknumber(L, 4))
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.craft.mount(id, on, x, y, z) -- a thing carried on another; on 0 lets go.
int l_craft_mount(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double id = luaL_checknumber(L, 1);
    const double on = luaL_checknumber(L, 2);
    const int ok = (vm && vm->host.craft_mount)
                       ? vm->host.craft_mount(vm->host.ud, id, on, luaL_optnumber(L, 3, 0.0),
                                              luaL_optnumber(L, 4, 0.0), luaL_optnumber(L, 5, 0.0))
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.craft.aim(id, yaw, pitch) -- turned on what carries it, or in the world.
int l_craft_aim(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double id = luaL_checknumber(L, 1);
    const int ok = (vm && vm->host.craft_aim)
                       ? vm->host.craft_aim(vm->host.ud, id, luaL_checknumber(L, 2), luaL_optnumber(L, 3, 0.0))
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.craft.seat(player, id, x, y, z) -- a player carried on a thing.
int l_craft_seat(lua_State *L)
{
    VmState     *vm     = host_of(L);
    const int    player = (int)luaL_checkinteger(L, 1);
    const double id     = luaL_checknumber(L, 2);
    const int ok = (vm && vm->host.craft_seat)
                       ? vm->host.craft_seat(vm->host.ud, player, id, luaL_optnumber(L, 3, 0.0),
                                             luaL_optnumber(L, 4, 0.0), luaL_optnumber(L, 5, 0.0))
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.ecs.clip(id, name) -- a clip of a visible entity's model, by name; "" for
// the one its motion picks again.
int l_ecs_clip(lua_State *L)
{
    VmState     *vm   = host_of(L);
    const double id   = luaL_checknumber(L, 1);
    const char  *name = luaL_optstring(L, 2, "");

    const int ok = (vm && vm->host.ecs_clip) ? vm->host.ecs_clip(vm->host.ud, id, name) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.ecs.box(id, half, height) -- how big a visible entity is to a shot.
int l_ecs_box(lua_State *L)
{
    VmState     *vm     = host_of(L);
    const double id     = luaL_checknumber(L, 1);
    const double half   = luaL_checknumber(L, 2);
    const double height = luaL_checknumber(L, 3);

    const int ok = (vm && vm->host.ecs_box) ? vm->host.ecs_box(vm->host.ud, id, half, height) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

int l_ecs_count(lua_State *L)
{
    VmState    *vm        = host_of(L);
    const char *component = luaL_checkstring(L, 1);

    const int n = (vm && vm->host.ecs_count) ? vm->host.ecs_count(vm->host.ud, component) : 0;
    lua_pushinteger(L, n);
    return 1;
}

// ------------------------------------------------------------------ render
// core.render.pass and core.render.box. Hand-written for the same reason
// core.ecs.system is: one of them takes a function.

int l_render_pass(lua_State *L)
{
    VmState    *vm    = host_of(L);
    const char *name  = luaL_checkstring(L, 1);
    const int   order = (int)luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // A process with no screen is not a failure to explain, it is a different
    // process. A mod that draws should ask whether it can, and a server that
    // said "core.render.pass failed" would send its author looking for a bug.
    if (!vm || !vm->host.render_pass)
        luaL_error(L, "this process does not draw: core.render.pass exists on a client");

    lua_pushvalue(L, 3);
    const int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    char err[256] = {0};
    if (!vm->host.render_pass(vm->host.ud, name, order, ref, err, sizeof(err))) {
        lua_unref(L, ref);
        luaL_error(L, "%s", err[0] ? err : "core.render.pass failed");
    }
    return 0;
}

int l_render_box(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double x  = luaL_checknumber(L, 1);
    const double y  = luaL_checknumber(L, 2);
    const double z  = luaL_checknumber(L, 3);
    const double w  = luaL_optnumber(L, 4, 1.0);
    const double h  = luaL_optnumber(L, 5, 1.0);
    const double d  = luaL_optnumber(L, 6, 1.0);
    const int    r  = (int)luaL_optinteger(L, 7, 255);
    const int    g  = (int)luaL_optinteger(L, 8, 255);
    const int    b  = (int)luaL_optinteger(L, 9, 255);
    const int    a  = (int)luaL_optinteger(L, 10, 255);

    const int ok = (vm && vm->host.render_box)
                       ? vm->host.render_box(vm->host.ud, x, y, z, w, h, d, r, g, b, a)
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.render.fx(kind, x, y, z, size?, r?, g?, b?, a?, t?, x2?, y2?, z2?)
int l_render_fx(lua_State *L)
{
    VmState     *vm   = host_of(L);
    const char  *kind = luaL_checkstring(L, 1);
    const double x    = luaL_checknumber(L, 2);
    const double y    = luaL_checknumber(L, 3);
    const double z    = luaL_checknumber(L, 4);
    const double size = luaL_optnumber(L, 5, 1.0);
    const int    r    = (int)luaL_optinteger(L, 6, 255);
    const int    g    = (int)luaL_optinteger(L, 7, 255);
    const int    b    = (int)luaL_optinteger(L, 8, 255);
    const int    a    = (int)luaL_optinteger(L, 9, 255);
    const double t    = luaL_optnumber(L, 10, 0.0);
    const double x2   = luaL_optnumber(L, 11, x);
    const double y2   = luaL_optnumber(L, 12, y);
    const double z2   = luaL_optnumber(L, 13, z);
    const int ok = (vm && vm->host.render_fx)
                       ? vm->host.render_fx(vm->host.ud, kind, x, y, z, size, r, g, b, a, t, x2, y2, z2)
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

int l_render_model(lua_State *L)
{
    VmState     *vm    = host_of(L);
    const int    model = (int)luaL_checkinteger(L, 1);
    const double x     = luaL_checknumber(L, 2);
    const double y     = luaL_checknumber(L, 3);
    const double z     = luaL_checknumber(L, 4);
    const double yaw   = luaL_optnumber(L, 5, 0.0);
    const double scale = luaL_optnumber(L, 6, 1.0);
    const double pitch = luaL_optnumber(L, 7, 0.0);
    const double roll  = luaL_optnumber(L, 8, 0.0);

    const int ok = (vm && vm->host.render_model)
                       ? vm->host.render_model(vm->host.ud, model, x, y, z, yaw, scale, pitch, roll)
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// --------------------------------------------------------------------- hud
// core.hud.pass, core.hud.text and core.hud.rect: over the screen, once a
// frame. Hand-written beside core.render for the same reason.

int l_hud_pass(lua_State *L)
{
    VmState    *vm    = host_of(L);
    const char *name  = luaL_checkstring(L, 1);
    const int   order = (int)luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    if (!vm || !vm->host.hud_pass)
        luaL_error(L, "this process has no screen: core.hud.pass exists on a client");

    lua_pushvalue(L, 3);
    const int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    char err[256] = {0};
    if (!vm->host.hud_pass(vm->host.ud, name, order, ref, err, sizeof(err))) {
        lua_unref(L, ref);
        luaL_error(L, "%s", err[0] ? err : "core.hud.pass failed");
    }
    return 0;
}

int l_hud_text(lua_State *L)
{
    VmState     *vm    = host_of(L);
    const double x     = luaL_checknumber(L, 1);
    const double y     = luaL_checknumber(L, 2);
    const char  *text  = luaL_checkstring(L, 3);
    const double size  = luaL_optnumber(L, 4, 0.03);
    const int    align = (int)luaL_optinteger(L, 5, 1);
    const int    r     = (int)luaL_optinteger(L, 6, 255);
    const int    g     = (int)luaL_optinteger(L, 7, 255);
    const int    b     = (int)luaL_optinteger(L, 8, 255);
    const int    a     = (int)luaL_optinteger(L, 9, 255);

    const int ok = (vm && vm->host.hud_text)
                       ? vm->host.hud_text(vm->host.ud, x, y, text, size, align, r, g, b, a)
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

int l_hud_rect(lua_State *L)
{
    VmState     *vm = host_of(L);
    const double x  = luaL_checknumber(L, 1);
    const double y  = luaL_checknumber(L, 2);
    const double w  = luaL_checknumber(L, 3);
    const double h  = luaL_checknumber(L, 4);
    const int    r  = (int)luaL_optinteger(L, 5, 255);
    const int    g  = (int)luaL_optinteger(L, 6, 255);
    const int    b  = (int)luaL_optinteger(L, 7, 255);
    const int    a  = (int)luaL_optinteger(L, 8, 255);

    const int ok = (vm && vm->host.hud_rect)
                       ? vm->host.hud_rect(vm->host.ud, x, y, w, h, r, g, b, a)
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.hud.image(name, x, y, w, h, a) -- a picture from this world's media, its
// middle at x, y and its size fractions of the screen's height like hud.rect.
int l_hud_image(lua_State *L)
{
    VmState     *vm   = host_of(L);
    const char  *name = luaL_checkstring(L, 1);
    const double x    = luaL_checknumber(L, 2);
    const double y    = luaL_checknumber(L, 3);
    const double w    = luaL_checknumber(L, 4);
    const double h    = luaL_checknumber(L, 5);
    const int    a    = (int)luaL_optinteger(L, 6, 255);

    const int ok = (vm && vm->host.hud_image)
                       ? vm->host.hud_image(vm->host.ud, name, x, y, w, h, a)
                       : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.hud.item(name, x, y, side) -- an item's square as the bag draws it, its
// middle at x, y and its side a fraction of the screen's height.
int l_hud_item(lua_State *L)
{
    VmState     *vm   = host_of(L);
    const char  *name = luaL_checkstring(L, 1);
    const double x    = luaL_checknumber(L, 2);
    const double y    = luaL_checknumber(L, 3);
    const double side = luaL_checknumber(L, 4);

    const int ok = (vm && vm->host.hud_item) ? vm->host.hud_item(vm->host.ud, name, x, y, side) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

// core.hud.mouse() -- the cursor, as fractions of the screen like hud.text's
// x, y; NOTHING where there is no screen.
int l_hud_mouse(lua_State *L)
{
    VmState *vm = host_of(L);
    double   x = 0.0, y = 0.0;
    if (!vm || !vm->host.hud_mouse || !vm->host.hud_mouse(vm->host.ud, &x, &y)) return 0;
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// core.hud.aspect() -- the screen's width over its height, so a mod can lay a
// thing out against the engine's bars, whose sizes are fractions of the
// height. 0 where there is no screen.
int l_hud_aspect(lua_State *L)
{
    VmState *vm = host_of(L);
    lua_pushnumber(L, (vm && vm->host.hud_aspect) ? vm->host.hud_aspect(vm->host.ud) : 0.0);
    return 1;
}

// --------------------------------------------------------------------- net
// core.net.open and core.net.send. Hand-written beside core.event and core.ecs
// rather than generated, because open takes no arguments and answers with
// NOTHING when there is no channel -- a mod that stored a 0 would go on sending
// on the handshake channel.

int l_net_open(lua_State *L)
{
    VmState  *vm = host_of(L);
    const int ch = (vm && vm->host.net_open) ? vm->host.net_open(vm->host.ud) : 0;
    if (ch <= 0) return 0;
    lua_pushinteger(L, ch);
    return 1;
}

int l_net_send(lua_State *L)
{
    VmState    *vm     = host_of(L);
    const int   player = (int)luaL_checkinteger(L, 1);
    const char *text   = luaL_checkstring(L, 2);

    const int ok = (vm && vm->host.net_send) ? vm->host.net_send(vm->host.ud, player, text) : 0;
    lua_pushboolean(L, ok);
    return 1;
}

void register_api(lua_State *L)
{
    // Registered before luaL_sandbox, which freezes the global table: after the
    // sandbox call this would fail.
    lua_newtable(L);

    register_generated_api(L);

    // core.register_mapgen: a stage of world generation, in Lua. Beside the
    // generated calls rather than in a table of its own -- there is one of it,
    // and core.register_node is what it reads like.
    lua_pushcfunction(L, l_register_mapgen, "core.register_mapgen");
    lua_setfield(L, -2, "register_mapgen");

    // core.register_abm / core.register_lbm: things that happen to cells on
    // their own. Beside register_mapgen for the same reason it is beside
    // register_node -- there is one of each and they read like registrations.
    lua_pushcfunction(L, l_register_abm, "core.register_abm");
    lua_setfield(L, -2, "register_abm");
    lua_pushcfunction(L, l_register_lbm, "core.register_lbm");
    lua_setfield(L, -2, "register_lbm");

    // core.event: define a schema, listen, emit. The bus is above this layer;
    // these are the three doors into it.
    lua_newtable(L);
    lua_pushcfunction(L, l_event_define, "core.event.define");
    lua_setfield(L, -2, "define");
    lua_pushcfunction(L, l_event_on, "core.event.on");
    lua_setfield(L, -2, "on");
    lua_pushcfunction(L, l_event_emit, "core.event.emit");
    lua_setfield(L, -2, "emit");
    lua_setfield(L, -2, "event");

    // core.ecs: entities a mod owns, and the systems that step them.
    lua_newtable(L);
    lua_pushcfunction(L, l_ecs_component, "core.ecs.component");
    lua_setfield(L, -2, "component");
    lua_pushcfunction(L, l_ecs_phase, "core.ecs.phase");
    lua_setfield(L, -2, "phase");
    lua_pushcfunction(L, l_ecs_system, "core.ecs.system");
    lua_setfield(L, -2, "system");
    lua_pushcfunction(L, l_ecs_spawn, "core.ecs.spawn");
    lua_setfield(L, -2, "spawn");
    lua_pushcfunction(L, l_ecs_set, "core.ecs.set");
    lua_setfield(L, -2, "set");
    lua_pushcfunction(L, l_ecs_get, "core.ecs.get");
    lua_setfield(L, -2, "get");
    lua_pushcfunction(L, l_ecs_kill, "core.ecs.kill");
    lua_setfield(L, -2, "kill");
    lua_pushcfunction(L, l_ecs_visible, "core.ecs.visible");
    lua_setfield(L, -2, "visible");
    lua_pushcfunction(L, l_ecs_model, "core.ecs.model");
    lua_setfield(L, -2, "model");
    lua_pushcfunction(L, l_ecs_health, "core.ecs.health");
    lua_setfield(L, -2, "health");
    lua_pushcfunction(L, l_ecs_clip, "core.ecs.clip");
    lua_setfield(L, -2, "clip");
    lua_pushcfunction(L, l_ecs_box, "core.ecs.box");
    lua_setfield(L, -2, "box");
    lua_pushcfunction(L, l_ecs_count, "core.ecs.count");
    lua_setfield(L, -2, "count");
    lua_setfield(L, -2, "ecs");

    // core.craft: a mod's things made machines, and what rides them.
    lua_newtable(L);
    lua_pushcfunction(L, l_craft_make, "core.craft.make");
    lua_setfield(L, -2, "make");
    lua_pushcfunction(L, l_craft_keys, "core.craft.keys");
    lua_setfield(L, -2, "keys");
    lua_pushcfunction(L, l_craft_pose, "core.craft.pose");
    lua_setfield(L, -2, "pose");
    lua_pushcfunction(L, l_craft_push, "core.craft.push");
    lua_setfield(L, -2, "push");
    lua_pushcfunction(L, l_craft_mount, "core.craft.mount");
    lua_setfield(L, -2, "mount");
    lua_pushcfunction(L, l_craft_aim, "core.craft.aim");
    lua_setfield(L, -2, "aim");
    lua_pushcfunction(L, l_craft_seat, "core.craft.seat");
    lua_setfield(L, -2, "seat");
    lua_setfield(L, -2, "craft");

    // core.net: a channel of this mod's own, and what it says on it.
    lua_newtable(L);
    lua_pushcfunction(L, l_net_open, "core.net.open");
    lua_setfield(L, -2, "open");
    lua_pushcfunction(L, l_net_send, "core.net.send");
    lua_setfield(L, -2, "send");
    lua_setfield(L, -2, "net");

    // core.render: a place in the frame, and something to draw in it.
    lua_newtable(L);
    lua_pushcfunction(L, l_render_pass, "core.render.pass");
    lua_setfield(L, -2, "pass");
    lua_pushcfunction(L, l_render_box, "core.render.box");
    lua_setfield(L, -2, "box");
    lua_pushcfunction(L, l_render_model, "core.render.model");
    lua_setfield(L, -2, "model");
    lua_pushcfunction(L, l_render_fx, "core.render.fx");
    lua_setfield(L, -2, "fx");
    lua_setfield(L, -2, "render");

    // core.hud: over the screen, once a frame.
    lua_newtable(L);
    lua_pushcfunction(L, l_hud_pass, "core.hud.pass");
    lua_setfield(L, -2, "pass");
    lua_pushcfunction(L, l_hud_text, "core.hud.text");
    lua_setfield(L, -2, "text");
    lua_pushcfunction(L, l_hud_rect, "core.hud.rect");
    lua_setfield(L, -2, "rect");
    lua_pushcfunction(L, l_hud_aspect, "core.hud.aspect");
    lua_setfield(L, -2, "aspect");
    lua_pushcfunction(L, l_hud_image, "core.hud.image");
    lua_setfield(L, -2, "image");
    lua_pushcfunction(L, l_hud_item, "core.hud.item");
    lua_setfield(L, -2, "item");
    lua_pushcfunction(L, l_hud_mouse, "core.hud.mouse");
    lua_setfield(L, -2, "mouse");
    lua_setfield(L, -2, "hud");

    lua_setglobal(L, "core");
}

void copy_error(lua_State *L, char *err, size_t err_size)
{
    if (!err || err_size == 0) return;
    const char *msg = lua_tostring(L, -1);
    std::snprintf(err, err_size, "%s", msg ? msg : "unknown error");
}

} // namespace

script_vm *script_create(const script_desc *desc)
{
    VmState *vm = new (std::nothrow) VmState();
    if (!vm) return nullptr;

    vm->host             = desc->host;
    vm->memory_limit     = desc->memory_limit_bytes;
    vm->interrupt_budget = desc->interrupt_budget;

    vm->L = lua_newstate(vm_alloc, vm);
    if (!vm->L) {
        delete vm;
        return nullptr;
    }

    lua_setthreaddata(vm->L, vm);
    luaL_openlibs(vm->L);   // Luau's set: no io, no package, no loadstring
    register_api(vm->L);

    // Whatever the caller's capability policy did not grant goes now, while the
    // global table is still writable. After luaL_sandbox it is read-only, and
    // after that the only way to hide a name would be to shadow it per mod --
    // which does not work, because a sandboxed thread's globals proxy reads
    // straight through to this table.
    for (int i = 0; i < desc->strip_count; ++i) {
        const char *name = desc->strip_globals[i];
        if (!name || !*name) continue;
        lua_pushnil(vm->L);
        lua_setglobal(vm->L, name);
    }

    lua_callbacks(vm->L)->interrupt = vm_interrupt;

    // Freezes the global table and the standard libraries. From here nothing
    // can reach outside what was registered above, and nothing can redefine a
    // standard function for everyone else either.
    luaL_sandbox(vm->L);

    // The mod does not run in that frozen environment: it gets its own thread
    // with its own globals table, chained to the frozen one for reads. Without
    // this, a mod cannot so much as declare a global function -- the first
    // `function on_step()` fails with "attempt to modify a readonly table" --
    // and every mod would be forced into a return-a-table shape for no reason.
    // It is also the isolation DESIGN.md 5.6 asks for: two mods cannot see or
    // clobber each other's globals.
    vm->T = lua_newthread(vm->L);
    if (!vm->T) {
        lua_close(vm->L);
        delete vm;
        return nullptr;
    }
    luaL_sandboxthread(vm->T);
    // Anchor it, or the collector reclaims the thread out from under us.
    vm->thread_ref = lua_ref(vm->L, -1);
    lua_pop(vm->L, 1);

    return reinterpret_cast<script_vm *>(vm);
}

void script_destroy(script_vm *handle)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (!vm) return;
    if (vm->L) lua_close(vm->L);
    delete vm;
}

bool script_run_source(script_vm *handle, const char *chunk_name, const char *source,
                       char *err, size_t err_size)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (!vm || !vm->T) return false;

    size_t bytecode_size = 0;
    lua_CompileOptions opts{};
    opts.optimizationLevel = 2;
    opts.debugLevel        = 1;

    char *bytecode = luau_compile(source, std::strlen(source), &opts, &bytecode_size);
    if (!bytecode) {
        if (err && err_size) std::snprintf(err, err_size, "compile failed");
        return false;
    }

    // luaL_sandboxthread marks the thread's globals "safe", which lets the VM
    // assume no global was reassigned since the first chunk was compiled
    // against them. That assumption only holds for one chunk: a mod split over
    // several files needs the flag cleared, or the second file can read a stale
    // constant-folded global. Luau's own sandboxthread carries the same warning.
    if (vm->chunks_loaded > 0) lua_setsafeenv(vm->T, LUA_GLOBALSINDEX, false);

    const int load_rc = luau_load(vm->T, chunk_name, bytecode, bytecode_size, 0);
    std::free(bytecode);

    if (load_rc != 0) {
        copy_error(vm->T, err, err_size);
        lua_pop(vm->T, 1);
        return false;
    }

    vm->interrupt_count = 0;
    vm->interrupted     = false;

    ++vm->chunks_loaded;

    if (lua_pcall(vm->T, 0, 0, 0) != 0) {
        copy_error(vm->T, err, err_size);
        lua_pop(vm->T, 1);
        return false;
    }
    return true;
}

bool script_run_file(script_vm *handle, const char *path, char *err, size_t err_size)
{
    std::FILE *f = std::fopen(path, "rb");
    if (!f) {
        if (err && err_size) std::snprintf(err, err_size, "cannot open %s", path);
        return false;
    }

    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0) {
        std::fclose(f);
        if (err && err_size) std::snprintf(err, err_size, "cannot size %s", path);
        return false;
    }

    char *buf = static_cast<char *>(std::malloc((size_t)size + 1));
    if (!buf) {
        std::fclose(f);
        if (err && err_size) std::snprintf(err, err_size, "out of memory reading %s", path);
        return false;
    }
    const size_t got = std::fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    std::fclose(f);

    const bool ok = script_run_source(handle, path, buf, err, err_size);
    std::free(buf);
    return ok;
}

bool script_call_step(script_vm *handle, const char *fn_name, double dt,
                      char *err, size_t err_size)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (!vm || !vm->T) return false;

    lua_getglobal(vm->T, fn_name);
    if (!lua_isfunction(vm->T, -1)) {
        lua_pop(vm->T, 1);
        return true;   // no hook installed is not a failure
    }

    lua_pushnumber(vm->T, dt);

    vm->interrupt_count = 0;
    vm->interrupted     = false;

    if (lua_pcall(vm->T, 1, 0, 0) != 0) {
        copy_error(vm->T, err, err_size);
        lua_pop(vm->T, 1);
        return false;
    }
    return true;
}

bool script_call_save(script_vm *handle, const char *fn_name, char *out, size_t out_size,
                      char *err, size_t err_size)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (!vm || !vm->T || !out || out_size == 0) return false;
    out[0] = '\0';

    lua_getglobal(vm->T, fn_name);
    if (!lua_isfunction(vm->T, -1)) {
        lua_pop(vm->T, 1);
        return true;   // a mod with no state to carry across is the normal case
    }

    vm->interrupt_count = 0;
    vm->interrupted     = false;

    if (lua_pcall(vm->T, 0, 1, 0) != 0) {
        copy_error(vm->T, err, err_size);
        lua_pop(vm->T, 1);
        return false;
    }

    if (lua_isnil(vm->T, -1)) {
        lua_pop(vm->T, 1);
        return true;
    }

    size_t      len = 0;
    const char *s   = lua_tolstring(vm->T, -1, &len);
    if (!s) {
        lua_pop(vm->T, 1);
        if (err && err_size) std::snprintf(err, err_size, "%s must return a string or nil", fn_name);
        return false;
    }

    if (len + 1 > out_size) {
        lua_pop(vm->T, 1);
        if (err && err_size)
            std::snprintf(err, err_size, "%s returned %zu bytes, the limit is %zu",
                          fn_name, len, out_size - 1);
        return false;
    }

    std::memcpy(out, s, len);
    out[len] = '\0';
    lua_pop(vm->T, 1);
    return true;
}

bool script_call_load(script_vm *handle, const char *fn_name, const char *state,
                      char *err, size_t err_size)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (!vm || !vm->T) return false;

    lua_getglobal(vm->T, fn_name);
    if (!lua_isfunction(vm->T, -1)) {
        lua_pop(vm->T, 1);
        return true;
    }

    if (state && *state) lua_pushstring(vm->T, state);
    else                 lua_pushnil(vm->T);

    vm->interrupt_count = 0;
    vm->interrupted     = false;

    if (lua_pcall(vm->T, 1, 0, 0) != 0) {
        copy_error(vm->T, err, err_size);
        lua_pop(vm->T, 1);
        return false;
    }
    return true;
}

bool script_call_handler(script_vm *handle, int handler_ref, script_field *fields, int count,
                         int cancellable, script_event_result *result,
                         char *err, size_t err_size)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (!vm || !vm->T) return false;
    if (result) result->cancelled = 0;

    // Payload table first, function second: lua_pcall wants the function
    // immediately below its arguments, so anything pushed after the function
    // has to BE an argument. Building the table on top of the function put a
    // table where the callee should have been -- "attempt to call a table
    // value", caught by the bus test before this ever reached a mod.
    push_fields_table(vm->T, fields, count, cancellable);
    const int table_index = lua_gettop(vm->T);

    lua_getref(vm->T, handler_ref);
    if (!lua_isfunction(vm->T, -1)) {
        lua_pop(vm->T, 2);
        if (err && err_size) std::snprintf(err, err_size, "handler %d is gone", handler_ref);
        return false;
    }

    vm->interrupt_count = 0;
    vm->interrupted     = false;

    // The table is passed AND kept below: after the call it is read back, so
    // that a handler changing a value changes it for the next handler and for
    // the engine. That is what makes an event modifiable rather than a
    // notification (DESIGN.md 6.3).
    lua_pushvalue(vm->T, table_index);
    if (lua_pcall(vm->T, 1, 0, 0) != 0) {
        copy_error(vm->T, err, err_size);
        lua_pop(vm->T, 2);   // the error and the table
        return false;
    }

    for (int i = 0; i < count; ++i) {
        lua_getfield(vm->T, table_index, fields[i].name);
        switch (fields[i].type) {
            case SCRIPT_V_NUMBER:
                if (lua_isnumber(vm->T, -1)) fields[i].num = lua_tonumber(vm->T, -1);
                break;
            case SCRIPT_V_BOOL:
                fields[i].b = lua_toboolean(vm->T, -1);
                break;
            case SCRIPT_V_STRING:
                if (lua_isstring(vm->T, -1)) fields[i].str = lua_tostring(vm->T, -1);
                break;
        }
        lua_pop(vm->T, 1);
    }

    if (cancellable && result) {
        lua_getfield(vm->T, table_index, "cancel");
        result->cancelled = lua_toboolean(vm->T, -1);
        lua_pop(vm->T, 1);
    }

    if (result) {
        result->stray[0] = '\0';
        lua_pushnil(vm->T);
        while (lua_next(vm->T, table_index) != 0) {
            lua_pop(vm->T, 1);   // the value; the key stays for lua_next
            if (result->stray[0] || lua_type(vm->T, -1) != LUA_TSTRING) continue;
            const char *key   = lua_tostring(vm->T, -1);
            bool        known = cancellable && std::strcmp(key, "cancel") == 0;
            for (int i = 0; i < count && !known; ++i) known = std::strcmp(key, fields[i].name) == 0;
            if (!known) std::snprintf(result->stray, sizeof(result->stray), "%s", key);
        }
    }

    lua_pop(vm->T, 1);   // the table
    return true;
}

bool script_call_system(script_vm *handle, int handler_ref, script_batch *batch, char *err,
                        size_t err_size)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (!vm || !vm->T || !batch) return false;

    // The batch table, built the same way round as an event's payload: the
    // table first, the function above it, and the table kept below the call so
    // it can be read back afterwards.
    lua_createtable(vm->T, 0, batch->column_count + 4);
    const int table_index = lua_gettop(vm->T);

    lua_pushinteger(vm->T, batch->count);
    lua_setfield(vm->T, table_index, "n");
    lua_pushnumber(vm->T, batch->dt);
    lua_setfield(vm->T, table_index, "dt");
    lua_pushnumber(vm->T, batch->tick);
    lua_setfield(vm->T, table_index, "tick");

    if (batch->ids) {
        lua_createtable(vm->T, batch->count, 0);
        for (int i = 0; i < batch->count; ++i) {
            lua_pushnumber(vm->T, batch->ids[i]);
            lua_rawseti(vm->T, -2, i + 1);
        }
        lua_setfield(vm->T, table_index, "id");
    }

    for (int c = 0; c < batch->column_count; ++c) {
        lua_createtable(vm->T, batch->count, 0);
        for (int i = 0; i < batch->count; ++i) {
            lua_pushnumber(vm->T, batch->columns[c].values[i]);
            lua_rawseti(vm->T, -2, i + 1);
        }
        lua_setfield(vm->T, table_index, batch->columns[c].name);
    }

    lua_getref(vm->T, handler_ref);
    if (!lua_isfunction(vm->T, -1)) {
        lua_pop(vm->T, 2);
        if (err && err_size) std::snprintf(err, err_size, "system %d is gone", handler_ref);
        return false;
    }

    vm->interrupt_count = 0;
    vm->interrupted     = false;

    lua_pushvalue(vm->T, table_index);
    if (lua_pcall(vm->T, 1, 0, 0) != 0) {
        copy_error(vm->T, err, err_size);
        lua_pop(vm->T, 2);
        return false;
    }

    // Read back. Only the columns: `n`, `dt`, `tick` and the ids are the
    // engine's side of the conversation, and a system that overwrote one of
    // them has changed nothing outside its own table.
    for (int c = 0; c < batch->column_count; ++c) {
        lua_getfield(vm->T, table_index, batch->columns[c].name);
        if (lua_istable(vm->T, -1)) {
            for (int i = 0; i < batch->count; ++i) {
                lua_rawgeti(vm->T, -1, i + 1);
                if (lua_isnumber(vm->T, -1))
                    batch->columns[c].values[i] = lua_tonumber(vm->T, -1);
                lua_pop(vm->T, 1);
            }
        }
        lua_pop(vm->T, 1);
    }

    lua_pop(vm->T, 1);   // the table
    return true;
}

void script_unref_handler(script_vm *handle, int handler_ref)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    if (vm && vm->L) lua_unref(vm->L, handler_ref);
}

size_t script_memory_used(script_vm *handle)
{
    VmState *vm = reinterpret_cast<VmState *>(handle);
    return vm ? vm->memory_used : 0;
}

const char *script_backend_name(void)
{
    return "luau";
}
