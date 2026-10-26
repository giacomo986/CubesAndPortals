#pragma once

#include "script/event_bus.h"
#include "script/script.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// One VM per mod, and a manifest that says what the mod is allowed to reach
// (DESIGN.md 5.6). This layer owns the policy -- which capability grants which
// global -- and script.h stays the mechanism: it is handed a list of globals to
// remove and removes them before a line of mod code runs.
//
// The isolation this buys is not theoretical. A mod that fails at load, errors
// in a tick, exhausts its memory quota or spins forever is disabled on its own
// while every other mod keeps stepping. tests/script_isolation_test.cpp is that
// claim written as a gate.

namespace cnp {

// What this build's mod API is called, and the oldest one it still answers.
//
// A number and not the engine's version string, for the reason core/version.h
// gives about the protocol: a version string is written by a person and can be
// wrong, while this changes when the API a mod is written against changes and
// at no other time. It is the number a refusal is made on.
//
// Bump kModApi when something a mod can call changes shape or goes away; raise
// kModApiOldest to the same number on the day this build stops carrying the old
// shape. While the two are equal there is exactly one API and a mod written for
// any other one is refused by name and number rather than failing on line 40.
inline constexpr unsigned kModApi       = 1;
inline constexpr unsigned kModApiOldest = 1;

// A mod's own version: three numbers, so that one mod can ask another for a
// version RANGE and be refused when it is not met. A free string cannot be
// compared, which is what the string this replaced could not do.
struct ModVersion {
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;

    std::string str() const;
};

bool operator==(const ModVersion &a, const ModVersion &b);
bool operator<(const ModVersion &a, const ModVersion &b);
inline bool operator!=(const ModVersion &a, const ModVersion &b) { return !(a == b); }
inline bool operator>(const ModVersion &a, const ModVersion &b) { return b < a; }
inline bool operator<=(const ModVersion &a, const ModVersion &b) { return !(b < a); }
inline bool operator>=(const ModVersion &a, const ModVersion &b) { return !(a < b); }

// `1`, `1.2` and `1.2.3` are all versions; what is missing is zero. Anything
// else -- a letter, an empty component, a fourth number -- is a refusal, since
// a version nobody can compare is a dependency nobody can check.
bool mod_version_parse(const std::string &text, ModVersion &out, std::string &error);

enum class ModVersionOp { Any, Ge, Gt, Le, Lt, Eq };

// One entry of `depends`: a name, and optionally what version of it. `Any` is
// what a bare name means, and it is what every dependency in this repo was
// before versions were comparable.
struct ModDependency {
    std::string  name;
    ModVersionOp op = ModVersionOp::Any;
    ModVersion   version;

    // "spike >= 1.2.0", for the sentence a refusal prints. Built rather than
    // kept from the file so that the refusal says the same thing whatever
    // spacing the manifest used.
    std::string str() const;
    bool        satisfied_by(const ModVersion &have) const;
};

// `depends = spike >= 1.2, guard` and `depends = spike guard` are both lists of
// two. Commas and whitespace separate; an operator binds to the name before it,
// attached or not.
bool mod_depends_parse(const std::string &text, std::vector<ModDependency> &out,
                       std::string &error);

// Where a mod runs (TODO.md OPEN-49). `server` is every mod there was before a
// world could send code: it runs where the world is. `client` is sent to a
// player's machine and not run where the world is. `both` runs in both places,
// which is what a mod needs to talk to its other half on its own channel.
enum class ModSide : uint8_t { Server, Client, Both };

struct ModManifest {
    std::string dir;                       // absolute path to the mod's directory
    std::string name;                      // "spike"; also the registry namespace
    ModVersion  version;
    std::string description;
    std::string entry = "init.luau";

    // Which mod API the mod was written against. A manifest that says nothing
    // is taken to mean the oldest this build still serves, which is the only
    // assumption that cannot silently promote an old mod onto a newer API: on
    // the day kModApiOldest rises past it, that mod is refused and told to say
    // what it wants.
    unsigned api          = kModApiOldest;
    bool     api_declared = false;

    std::vector<std::string> capabilities;

    // Mods that must have loaded first. Without this the load order is
    // alphabetical, and a mod looking up another's block by name gets 0 --
    // which is exactly what happened the first time two mods ran here.
    std::vector<ModDependency> depends;

    bool enabled = true;

    // `side = server | client | both`; server when the manifest says nothing.
    ModSide side = ModSide::Server;

    // Per-mod, because a mod that needs more should have to say so in a file a
    // human can read rather than get it by being loaded first.
    size_t   memory_limit_bytes = 32u * 1024u * 1024u;
    unsigned interrupt_budget   = 2000000;
};

struct Mod {
    ModManifest manifest;

    // Modification times of mod.conf and the entry file, as reported by the
    // filesystem. Polling these is what makes editing a mod while the game runs
    // a reload rather than a restart.
    long long conf_mtime  = 0;
    long long entry_mtime = 0;
    int       reloads     = 0;

    script_vm *vm      = nullptr;
    bool       running = false;   // false: refused at load, or disabled after an error
    std::string error;            // why, if it is not running

    double   step_seconds = 0.0;
    uint64_t step_calls   = 0;
};

// What the engine has to do around a reload, since this layer must not know
// what a registry or a chunk is. `before` is called with the mod about to be
// replaced; `after` says whether the replacement took. On failure the old
// version is still running, so `after(false)` means "put everything back".
struct ModHostCallbacks {
    void (*before_reload)(void *ud, const char *mod_name) = nullptr;
    void (*after_reload)(void *ud, const char *mod_name, bool ok) = nullptr;
    void *ud = nullptr;
};

class ModHost {
public:
    ModHost();

    // The bus every mod and the engine share. Owned here because reloading a
    // mod has to drop that VM's handlers, and because a handler that errors has
    // to cost its mod and nobody else.
    EventBus       &events()       { return m_bus; }
    const EventBus &events() const { return m_bus; }

    void set_host(const script_host &h) { m_host = h; }

    // Told whenever a VM is about to go away -- a mod disabled, reloaded or
    // shut down -- so whatever else holds references into it can drop them.
    // The event bus is dropped here because this class owns it; anything the
    // engine owns (the systems of core/ecs/lua_ecs.h) hears about it through
    // this, which keeps src/script free of any dependency on core/.
    void set_vm_gone(std::function<void(script_vm *)> fn) { m_on_vm_gone = std::move(fn); }

    // Disables one mod, by the VM its code is running in. Public because a
    // system that errored is not an event handler, and whoever runs those has
    // to be able to say so through the one place that knows what disabling a
    // mod means.
    void disable_vm(script_vm *vm, const std::string &why);

    // Who is running, while somebody else's code runs a mod's function. The
    // event bus is wired to these; so is whatever runs the systems. Without
    // them every host call a mod makes from inside one is attributed to the
    // empty string -- not a crash, a mod being told its own panel belongs to
    // somebody else.
    void enter_vm(script_vm *vm);
    void leave_vm(script_vm *vm);
    void set_callbacks(const ModHostCallbacks &c) { m_cb = c; }

    // Which side this host runs mods for (TODO.md OPEN-49). A server runs server
    // and both mods and refuses a client's; a client running a world's code
    // runs client and both mods and refuses a server's. Both, the default, is a
    // process that is its own server, and runs everything it always did.
    void set_side(ModSide side) { m_side = side; }

    // Whether what this host loads is a WORLD's code on a player's machine: no
    // capability of any kind is granted, and a manifest may not raise its own
    // memory quota or interrupt budget above the defaults.
    void set_world_code(bool on) { m_world_code = on; }
    // The mods a world sent as code. A dependency of one of them that is not
    // among these runs on the server only, and is not missing here.
    void set_world_code_mods(std::vector<std::string> names) { m_world_code_mods = std::move(names); }

    // Loads every enabled mod found in `mods_dir`. Returns how many are running.
    // A mod that fails to load is kept in the list with its error, so the
    // failure is visible in the log and in --bench output instead of silent.
    //
    // A mod is a directory OR a `.cnpmod` package (modpack/mod_package.h). A
    // package is verified whole and unpacked into the cache below, and from
    // there it is loaded exactly like a directory -- so nothing downstream of
    // this call has to know which of the two a mod arrived as.
    int load_dir(const std::string &mods_dir);

    // The mods in these directories, in `depends` order: what load_dir does
    // once it has them, and what a world's code arriving as media goes through.
    int load_dirs(const std::vector<std::string> &dirs);

    // Where packages are unpacked to. Defaults to a directory in the system's
    // temporary space, because the directory a package SITS in may be read-only
    // -- which is what a mod shipped inside an installed game looks like, and
    // the case M10 is about.
    void set_package_cache_dir(const std::string &dir) { m_package_cache = dir; }

    // Loads one directory. Public because the tests point it at fixtures.
    bool load_mod(const std::string &mod_dir);

    // Calls `on_step(dt)` in every running mod. A mod that errors is disabled;
    // the rest of the tick continues.
    void step(double dt);

    // Reloads one mod: re-reads its manifest, builds a second VM and runs the
    // new code in it, and only then throws the old one away. A mod whose new
    // version does not compile keeps running the version that does.
    //
    // State crosses over as a string: the old VM's `on_save()` result is handed
    // to the new VM's `on_load(state)`.
    bool reload(const std::string &name, std::string &error);

    // Reloads every mod whose files changed since it was loaded. Returns how
    // many were reloaded.
    int reload_changed();

    void shutdown();

    const std::vector<Mod> &mods() const { return m_mods; }
    int      running_count() const;
    size_t   memory_used() const;
    double   step_seconds() const;
    uint64_t step_calls() const;

private:
    static bool read_manifest(const std::string &mod_dir, ModManifest &out, std::string &error);
    bool        start_mod(ModManifest manifest);

    // Whether this build will run that manifest at all: the API it was written
    // for, and the versions of the mods it depends on. One function because a
    // load and a reload have to refuse the same manifest for the same reason --
    // a rule written twice is a rule a mod can be on both sides of.
    bool admissible(const ModManifest &manifest, std::string &error) const;

    // Unpacks one `.cnpmod` and gives back the directory it now lives in.
    bool unpack_package(const std::string &file, std::string &dir, std::string &error) const;

    // One place that lets go of a VM, because there were five and every one of
    // them had to remember the same two calls.
    void forget_vm(script_vm *vm);

    script_host      m_host{};
    ModHostCallbacks m_cb{};
    std::function<void(script_vm *)> m_on_vm_gone;
    EventBus         m_bus;
    std::vector<Mod> m_mods;
    std::string      m_package_cache;
    ModSide          m_side       = ModSide::Both;
    bool             m_world_code = false;
    std::vector<std::string> m_world_code_mods;
};

// While mod code is on the stack this names the mod that is running. Host
// callbacks use it to attribute what a mod does -- a registered block, an event
// handler -- to the mod that did it, which is what will let a reload drop
// exactly that mod's entries and nobody else's (DESIGN.md 5.7).
const ModManifest *mod_host_current();

// The VM that mod is running in. Registering an event handler needs it: the
// handler reference is only meaningful inside the VM that created it.
script_vm *mod_host_current_vm();

// Parses a mod.conf. Exposed for the tests, which check the refusals.
bool mod_manifest_parse(const std::string &text, ModManifest &out, std::string &error);

// What a declared capability grants. Returns false and fills `error` for a
// capability this build will not hand out.
bool mod_capability_globals(const std::vector<std::string> &caps,
                            std::vector<std::string>       &keep_globals,
                            std::string                    &error);

} // namespace cnp
