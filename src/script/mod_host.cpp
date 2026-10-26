#include "script/mod_host.h"

#include "modpack/mod_package.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace cnp {
namespace {

const ModManifest *g_current    = nullptr;
script_vm         *g_current_vm = nullptr;

// Set around every entry into a VM. Saving and restoring rather than clearing,
// because one mod's event handler can run inside another mod's emit: when that
// handler returns, the mod that was running is still running.
struct CurrentMod {
    const ModManifest *prev;
    script_vm         *prev_vm;

    CurrentMod(const ModManifest &m, script_vm *vm) : prev(g_current), prev_vm(g_current_vm)
    {
        g_current    = &m;
        g_current_vm = vm;
    }
    ~CurrentMod()
    {
        g_current    = prev;
        g_current_vm = prev_vm;
    }
};

// The same save-and-restore as CurrentMod, for the one caller that cannot use
// RAII: the event bus enters and leaves a mod in two separate calls. A stack
// rather than a single saved value, for the same reason CurrentMod saves one --
// a handler may emit, and one mod's handler can run inside another's.
std::vector<std::pair<const ModManifest *, script_vm *>> g_scope_stack;

std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> split_list(const std::string &s)
{
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s) {
        if (c == ',' || std::isspace((unsigned char)c)) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool parse_bool(const std::string &v)
{
    return v == "true" || v == "1" || v == "yes";
}

// Every global that a capability could grant. A mod gets the ones its manifest
// asked for and this list minus those is what script_create removes.
//
// The list is short, and that is Luau doing the work rather than us: there is no
// io, no package, no loadstring, no os.execute, no os.getenv, no dofile to take
// away, because Luau never had them. What is left that a mod should not get for
// free is `os` -- clock, date, difftime, time, all of them nondeterministic,
// and DESIGN.md 5.7 wants a tick that is deterministic for seed plus input --
// and `debug`, which is an introspection surface, not a gameplay one.
const char *const kOptionalGlobals[] = {"os", "debug"};

// A mod's state has to fit in this to survive a reload. Generous for a counter
// or a table of positions, small enough that a mod stuffing a world into it
// gets told so instead of quietly making reloads expensive.
constexpr size_t kMaxStateBytes = 64u * 1024u;

long long file_mtime(const std::filesystem::path &p)
{
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(p, ec);
    if (ec) return 0;
    return (long long)t.time_since_epoch().count();
}

} // namespace

const ModManifest *mod_host_current()
{
    return g_current;
}

script_vm *mod_host_current_vm()
{
    return g_current_vm;
}

ModHost::ModHost()
{
    // A handler that errors costs its mod, exactly like a failing on_step: the
    // event keeps going to the other handlers, and the offender stops running.
    // Who is running, while a handler runs.
    //
    // Without this every host call a mod makes from inside an event handler is
    // attributed to the empty string: not an error, not a crash -- a mod being
    // told its own panel belongs to somebody else. The bus holds a VM and an
    // owner name and cannot look a manifest up; this can.
    m_bus.set_scope([this](script_vm *vm) { enter_vm(vm); },
                    [this](script_vm *vm) { leave_vm(vm); });

    m_bus.set_handler_error([this](const std::string &owner, script_vm *vm,
                                   const std::string &event, const std::string &error) {
        disable_vm(vm, "handler for '" + event + "': " + error);
        (void)owner;
    });
}

void ModHost::enter_vm(script_vm *vm)
{
    g_scope_stack.emplace_back(g_current, g_current_vm);

    g_current    = nullptr;
    g_current_vm = vm;
    for (const Mod &m : m_mods) {
        if (m.vm != vm) continue;
        g_current = &m.manifest;
        break;
    }
}

void ModHost::leave_vm(script_vm *)
{
    if (g_scope_stack.empty()) return;
    g_current    = g_scope_stack.back().first;
    g_current_vm = g_scope_stack.back().second;
    g_scope_stack.pop_back();
}

void ModHost::forget_vm(script_vm *vm)
{
    m_bus.drop_vm(vm);
    if (m_on_vm_gone) m_on_vm_gone(vm);
}

void ModHost::disable_vm(script_vm *vm, const std::string &why)
{
    for (Mod &m : m_mods) {
        if (m.vm != vm) continue;
        m.running = false;
        m.error   = why;
        std::fprintf(stderr, "mod '%s' disabled: %s\n", m.manifest.name.c_str(), why.c_str());
    }
    forget_vm(vm);
}

std::string ModVersion::str() const
{
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

bool operator==(const ModVersion &a, const ModVersion &b)
{
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

bool operator<(const ModVersion &a, const ModVersion &b)
{
    if (a.major != b.major) return a.major < b.major;
    if (a.minor != b.minor) return a.minor < b.minor;
    return a.patch < b.patch;
}

bool mod_version_parse(const std::string &text, ModVersion &out, std::string &error)
{
    error.clear();
    out = ModVersion{};

    const std::string s = trim(text);
    if (s.empty()) {
        error = "expected a version like 1, 1.2 or 1.2.3, and the value is empty";
        return false;
    }

    unsigned parts[3] = {0, 0, 0};
    int      count    = 0;
    size_t   i        = 0;

    while (true) {
        if (count == 3) {
            error = "'" + s + "' has more than three numbers in it";
            return false;
        }

        const size_t start = i;
        unsigned long long value = 0;
        while (i < s.size() && std::isdigit((unsigned char)s[i])) {
            value = value * 10 + (unsigned)(s[i] - '0');
            // A version nobody can hold is not a version. Refusing here rather
            // than wrapping keeps a typo from comparing as a small number.
            if (value > 999999ull) {
                error = "'" + s + "' has a number too large to be a version";
                return false;
            }
            ++i;
        }
        if (i == start) {
            error = "'" + s + "' is not a version: expected 1, 1.2 or 1.2.3";
            return false;
        }

        parts[count++] = (unsigned)value;

        if (i == s.size()) break;
        if (s[i] != '.') {
            error = "'" + s + "' is not a version: expected 1, 1.2 or 1.2.3";
            return false;
        }
        ++i;   // the dot, and the loop above refuses what follows if it is not a digit
    }

    out.major = parts[0];
    out.minor = parts[1];
    out.patch = parts[2];
    return true;
}

namespace {

const char *op_text(ModVersionOp op)
{
    switch (op) {
    case ModVersionOp::Ge: return ">=";
    case ModVersionOp::Gt: return ">";
    case ModVersionOp::Le: return "<=";
    case ModVersionOp::Lt: return "<";
    case ModVersionOp::Eq: return "=";
    case ModVersionOp::Any: break;
    }
    return "";
}

} // namespace

std::string ModDependency::str() const
{
    if (op == ModVersionOp::Any) return name;
    return name + " " + op_text(op) + " " + version.str();
}

bool ModDependency::satisfied_by(const ModVersion &have) const
{
    switch (op) {
    case ModVersionOp::Any: return true;
    case ModVersionOp::Ge:  return have >= version;
    case ModVersionOp::Gt:  return have >  version;
    case ModVersionOp::Le:  return have <= version;
    case ModVersionOp::Lt:  return have <  version;
    case ModVersionOp::Eq:  return have == version;
    }
    return false;
}

bool mod_depends_parse(const std::string &text, std::vector<ModDependency> &out,
                       std::string &error)
{
    error.clear();
    out.clear();

    // Commas and whitespace both separate, because `depends = spike, guard` and
    // `depends = spike guard` were both a list of two before a dependency could
    // carry a version, and a manifest that loaded yesterday has to load today.
    const std::vector<std::string> tokens = split_list(text);

    // Set while the previous token was an operator with nothing after it:
    // `spike >= 1.2` is three tokens and `spike >=1.2` is two.
    bool want_version = false;

    auto is_op_char = [](char c) { return c == '<' || c == '>' || c == '='; };

    // Reads the operator at the front of `s` and leaves the rest in `s`.
    auto take_op = [&](std::string &rest, ModVersionOp &op) -> bool {
        if (rest.rfind(">=", 0) == 0) { op = ModVersionOp::Ge; rest = rest.substr(2); return true; }
        if (rest.rfind("<=", 0) == 0) { op = ModVersionOp::Le; rest = rest.substr(2); return true; }
        if (rest.rfind("==", 0) == 0) { op = ModVersionOp::Eq; rest = rest.substr(2); return true; }
        if (rest.rfind(">", 0)  == 0) { op = ModVersionOp::Gt; rest = rest.substr(1); return true; }
        if (rest.rfind("<", 0)  == 0) { op = ModVersionOp::Lt; rest = rest.substr(1); return true; }
        if (rest.rfind("=", 0)  == 0) { op = ModVersionOp::Eq; rest = rest.substr(1); return true; }
        return false;
    };

    for (const std::string &tok : tokens) {
        if (want_version) {
            if (!mod_version_parse(tok, out.back().version, error)) {
                error = "depends on '" + out.back().name + "': " + error;
                return false;
            }
            want_version = false;
            continue;
        }

        if (is_op_char(tok[0])) {
            if (out.empty()) {
                error = "'" + tok + "' has no mod name before it";
                return false;
            }
            if (out.back().op != ModVersionOp::Any) {
                error = "'" + out.back().name + "' is asked for twice in one entry";
                return false;
            }

            std::string rest = tok;
            ModVersionOp op  = ModVersionOp::Any;
            if (!take_op(rest, op)) {
                error = "'" + tok + "' is not one of >=, >, =, <= or <";
                return false;
            }
            out.back().op = op;

            if (rest.empty()) want_version = true;
            else if (!mod_version_parse(rest, out.back().version, error)) {
                error = "depends on '" + out.back().name + "': " + error;
                return false;
            }
            continue;
        }

        // A name, with the operator possibly attached to it: `spike>=1.2`.
        size_t cut = std::string::npos;
        for (size_t i = 0; i < tok.size(); ++i) {
            if (is_op_char(tok[i])) { cut = i; break; }
        }

        ModDependency dep;
        dep.name = (cut == std::string::npos) ? tok : tok.substr(0, cut);
        if (dep.name.empty()) {
            error = "'" + tok + "' has no mod name in it";
            return false;
        }

        // A name and not whatever is left over. Without this, an operator this
        // format does not have -- `spike ~> 1.0` -- parses as a dependency on a
        // mod called "~", which loads on the day somebody writes a mod called
        // "~" and is refused for the wrong reason until then.
        for (const char c : dep.name) {
            if (std::isalnum((unsigned char)c) || c == '_' || c == '-') continue;
            error = "'" + dep.name + "' is not a mod name";
            return false;
        }
        out.push_back(dep);

        if (cut == std::string::npos) continue;

        std::string  rest = tok.substr(cut);
        ModVersionOp op   = ModVersionOp::Any;
        if (!take_op(rest, op)) {
            error = "'" + tok + "' is not one of >=, >, =, <= or <";
            return false;
        }
        out.back().op = op;

        if (rest.empty()) want_version = true;
        else if (!mod_version_parse(rest, out.back().version, error)) {
            error = "depends on '" + out.back().name + "': " + error;
            return false;
        }
    }

    if (want_version) {
        error = "depends on '" + out.back().name + "': nothing after '" +
                op_text(out.back().op) + "'";
        return false;
    }
    return true;
}

bool mod_capability_globals(const std::vector<std::string> &caps,
                            std::vector<std::string>       &keep_globals,
                            std::string                    &error)
{
    keep_globals.clear();
    error.clear();

    for (const std::string &cap : caps) {
        if (cap == "os" || cap == "debug") {
            keep_globals.push_back(cap);
            continue;
        }

        // Declared in DESIGN.md 5.6 and not implemented here. Refusing loudly is
        // the honest answer: a mod that says it needs to read files must not
        // load and then silently discover at runtime that it cannot, and a
        // capability that is quietly ignored is a capability system that lies.
        if (cap == "native" || cap == "exec" || cap.rfind("fs:", 0) == 0 ||
            cap.rfind("net:", 0) == 0) {
            error = "capability '" + cap + "' is not granted by this build";
            return false;
        }

        error = "unknown capability '" + cap + "'";
        return false;
    }
    return true;
}

bool mod_manifest_parse(const std::string &text, ModManifest &out, std::string &error)
{
    error.clear();

    std::istringstream in(text);
    std::string        line;
    int                lineno = 0;

    while (std::getline(in, line)) {
        ++lineno;
        const std::string s = trim(line);
        if (s.empty() || s[0] == '#') continue;

        const size_t eq = s.find('=');
        if (eq == std::string::npos) {
            error = "line " + std::to_string(lineno) + ": expected key = value";
            return false;
        }

        const std::string key = trim(s.substr(0, eq));
        const std::string val = trim(s.substr(eq + 1));

        if      (key == "name")        out.name = val;
        else if (key == "version") {
            if (!mod_version_parse(val, out.version, error)) {
                error = "line " + std::to_string(lineno) + ": version " + error;
                return false;
            }
        }
        else if (key == "api") {
            ModVersion api_version;
            if (!mod_version_parse(val, api_version, error) ||
                api_version.minor != 0 || api_version.patch != 0) {
                error = "line " + std::to_string(lineno) +
                        ": api is one whole number, not '" + val + "'";
                return false;
            }
            out.api          = api_version.major;
            out.api_declared = true;
        }
        else if (key == "description") out.description = val;
        else if (key == "entry")       out.entry = val;
        else if (key == "enabled")     out.enabled = parse_bool(val);
        else if (key == "capabilities") out.capabilities = split_list(val);
        else if (key == "depends") {
            if (!mod_depends_parse(val, out.depends, error)) {
                error = "line " + std::to_string(lineno) + ": " + error;
                return false;
            }
        }
        else if (key == "memory_kb")   out.memory_limit_bytes = (size_t)std::strtoul(val.c_str(), nullptr, 10) * 1024u;
        else if (key == "interrupt_budget") out.interrupt_budget = (unsigned)std::strtoul(val.c_str(), nullptr, 10);
        else if (key == "side") {
            if      (val == "server") out.side = ModSide::Server;
            else if (val == "client") out.side = ModSide::Client;
            else if (val == "both")   out.side = ModSide::Both;
            else {
                error = "line " + std::to_string(lineno) +
                        ": side is server, client or both, not '" + val + "'";
                return false;
            }
        }
        else {
            // Unknown keys are a typo in a hand-written file, and a typo in
            // `capabilities` that silently means "no capabilities" is exactly
            // the kind of quiet failure this whole layer exists to avoid.
            error = "line " + std::to_string(lineno) + ": unknown key '" + key + "'";
            return false;
        }
    }

    if (out.name.empty()) {
        error = "manifest has no name";
        return false;
    }
    return true;
}

bool ModHost::read_manifest(const std::string &mod_dir, ModManifest &out, std::string &error)
{
    namespace fs = std::filesystem;

    out.dir  = mod_dir;
    out.name = fs::path(mod_dir).filename().string();

    std::ifstream f(fs::path(mod_dir) / "mod.conf");
    if (!f) {
        // No manifest, no mod. DESIGN.md 5.6 puts it plainly: the manifest is
        // where a mod says what it needs, so a directory without one is not a
        // mod that gets everything -- it is not a mod.
        error = "no mod.conf in " + mod_dir;
        return false;
    }

    std::stringstream buf;
    buf << f.rdbuf();

    if (!mod_manifest_parse(buf.str(), out, error)) {
        error = "mod.conf: " + error;
        return false;
    }
    return true;
}

bool ModHost::admissible(const ModManifest &manifest, std::string &error) const
{
    error.clear();

    // The API first, because a mod written for another one may declare
    // capabilities and dependencies this build has no word for, and being told
    // "unknown capability" when the real answer is "this mod is for a different
    // engine" sends somebody editing the wrong line.
    if (manifest.api > kModApi || manifest.api < kModApiOldest) {
        error = "written for mod API " + std::to_string(manifest.api) + ", and this build serves " +
                std::to_string(kModApiOldest) + " to " + std::to_string(kModApi);
        if (!manifest.api_declared)
            error += " (its mod.conf says no api, so it was taken for " +
                     std::to_string(kModApiOldest) + ")";
        return false;
    }

    // One name, one mod. Before packages there was no way to have two: a
    // directory listing has one entry per name. A package sitting beside a
    // directory of the same mod is two copies of it, both registering the same
    // block names into one registry, and which one wins would be decided by the
    // order a filesystem listed them in.
    for (const Mod &other : m_mods) {
        if (!other.running || other.manifest.name != manifest.name) continue;
        if (other.manifest.dir == manifest.dir) continue;   // itself, being reloaded
        error = "a mod called '" + manifest.name + "' is already loaded, from " +
                other.manifest.dir;
        return false;
    }

    for (const ModDependency &dep : manifest.depends) {
        const Mod *have = nullptr;
        for (const Mod &other : m_mods) {
            if (other.running && other.manifest.name == dep.name) have = &other;
        }

        // Two different failures and two different sentences. "Not loaded" sends
        // somebody to install a mod; "the one loaded is 0.2.0" sends them to
        // upgrade the one they have, and telling them the first when the truth
        // is the second is an afternoon lost.
        if (!have) {
            // World code is a world's client halves and nothing else. A mod it
            // depends on that the world did not send is one that runs on the
            // server alone -- track_game on track_core, whose road the server
            // builds -- and its absence here says nothing is wrong. One the
            // world DID send and that has not loaded yet is still missing.
            if (m_world_code && std::find(m_world_code_mods.begin(), m_world_code_mods.end(),
                                          dep.name) == m_world_code_mods.end())
                continue;
            error = "depends on '" + dep.name + "', which is not loaded";
            return false;
        }
        if (!dep.satisfied_by(have->manifest.version)) {
            error = "depends on " + dep.str() + ", and the one loaded is " +
                    have->manifest.version.str();
            return false;
        }
    }
    return true;
}

bool ModHost::start_mod(ModManifest manifest)
{
    namespace fs = std::filesystem;

    Mod mod;
    mod.manifest = std::move(manifest);

    auto refuse = [&](const std::string &why) {
        mod.error = why;
        m_mods.push_back(std::move(mod));
        return false;
    };

    if (!mod.manifest.enabled) return refuse("disabled in mod.conf");

    // Which side it runs on, against which side this host is (TODO.md OPEN-49).
    if (m_side == ModSide::Server && mod.manifest.side == ModSide::Client)
        return refuse("a mod for clients: this world offers it and does not run it");
    if (m_side == ModSide::Client && mod.manifest.side == ModSide::Server)
        return refuse("a server's mod: a world does not send it, and a client does not run it");

    // And a world's code on a player's machine gets nothing it asks for. Not
    // even `os`: the clock is harmless where the world is and a fingerprint on
    // somebody else's machine, and a capability list a world can write is a
    // capability list the player did not.
    if (m_world_code) {
        if (!mod.manifest.capabilities.empty())
            return refuse("world code gets no capabilities, and this one declares '" +
                          mod.manifest.capabilities.front() + "'");
        const ModManifest defaults{};
        if (mod.manifest.memory_limit_bytes > defaults.memory_limit_bytes)
            mod.manifest.memory_limit_bytes = defaults.memory_limit_bytes;
        if (mod.manifest.interrupt_budget > defaults.interrupt_budget)
            mod.manifest.interrupt_budget = defaults.interrupt_budget;
    }

    std::string why;
    if (!admissible(mod.manifest, why)) return refuse(why);

    std::string err;
    std::vector<std::string> keep;
    if (!mod_capability_globals(mod.manifest.capabilities, keep, err)) return refuse(err);

    std::vector<const char *> strip;
    for (const char *g : kOptionalGlobals) {
        if (std::find(keep.begin(), keep.end(), g) == keep.end()) strip.push_back(g);
    }

    script_desc sd{};
    sd.host               = m_host;
    sd.memory_limit_bytes = mod.manifest.memory_limit_bytes;
    sd.interrupt_budget   = mod.manifest.interrupt_budget;
    sd.strip_globals      = strip.data();
    sd.strip_count        = (int)strip.size();

    mod.vm = script_create(&sd);
    if (!mod.vm) return refuse("could not create a VM");

    const std::string entry = (fs::path(mod.manifest.dir) / mod.manifest.entry).string();

    char errbuf[512];
    {
        CurrentMod cur(mod.manifest, mod.vm);
        if (!script_run_file(mod.vm, entry.c_str(), errbuf, sizeof(errbuf))) {
            // A mod that fails PARTWAY through has already run some of itself,
            // and some of that may have been core.event.on. Those handlers hold
            // a pointer to this VM, so destroying it without dropping them
            // leaves the bus calling into freed memory the next time anything
            // emits -- a use-after-free with a delay, which is the worst kind.
            //
            // Measured: a mod that registered core:broken and then failed on a
            // later line took the SERVER down the first time somebody broke a
            // block, and the only symptom a client saw was five seconds of
            // silence.
            forget_vm(mod.vm);

            script_destroy(mod.vm);
            mod.vm = nullptr;
            return refuse(errbuf);
        }
    }

    mod.running     = true;
    mod.conf_mtime  = file_mtime(fs::path(mod.manifest.dir) / "mod.conf");
    mod.entry_mtime = file_mtime(entry);
    m_mods.push_back(std::move(mod));
    return true;
}

bool ModHost::load_mod(const std::string &mod_dir)
{
    ModManifest m;
    std::string err;
    if (!read_manifest(mod_dir, m, err)) {
        Mod failed;
        failed.manifest = m;
        failed.error    = err;
        m_mods.push_back(std::move(failed));
        return false;
    }
    return start_mod(std::move(m));
}

bool ModHost::unpack_package(const std::string &file, std::string &dir, std::string &error) const
{
    namespace fs = std::filesystem;

    ModPackage pkg;
    if (!mod_package_read_file(file, pkg, error)) return false;

    const fs::path cache = m_package_cache.empty()
                               ? fs::temp_directory_path() / "cnp_mod_packages"
                               : fs::path(m_package_cache);

    // Named by the file AND by what is inside it. The hash is what makes two
    // versions of one package two directories instead of one directory somebody
    // half overwrote, and it is why a package that has not changed is unpacked
    // once and read from then on.
    const std::string stem = fs::path(file).stem().string();
    const fs::path    here = cache / (stem + "-" + pkg.hash.substr(0, 16));

    std::error_code ec;
    const fs::path  marker = here / ".cnpmod-hash";

    std::ifstream have(marker);
    if (have) {
        std::string written;
        std::getline(have, written);
        if (written == pkg.hash) {
            dir = here.string();
            return true;
        }
    }

    fs::remove_all(here, ec);
    if (!mod_package_unpack(pkg, here.string(), error)) return false;

    std::ofstream stamp(marker, std::ios::trunc);
    if (!stamp) {
        error = "could not write " + marker.string();
        return false;
    }
    stamp << pkg.hash << "\n";
    stamp.close();

    dir = here.string();
    return true;
}

int ModHost::load_dir(const std::string &mods_dir)
{
    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::is_directory(mods_dir, ec)) return 0;

    // Alphabetical first, so that whatever the dependency order leaves free is
    // still the same on every machine: a directory iteration order is not a
    // load order, and a mod that only works when it happens to go first is a
    // bug worth finding on the developer's machine rather than on a user's.
    std::vector<std::string> dirs;
    std::vector<std::string> packages;
    for (const fs::directory_entry &e : fs::directory_iterator(mods_dir, ec)) {
        if (e.is_directory(ec)) {
            dirs.push_back(e.path().string());
        } else if (e.path().extension() == kModPackageExtension) {
            packages.push_back(e.path().string());
        }
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(packages.begin(), packages.end());

    // A package becomes a directory before anything else happens to it, so
    // everything below this line -- the manifest, the order, the refusals, the
    // reload -- is the one path it always was, and a mod cannot behave
    // differently for having arrived as one file.
    for (const std::string &p : packages) {
        std::string dir, err;
        if (unpack_package(p, dir, err)) {
            dirs.push_back(dir);
            continue;
        }

        // Named by the file, because there is no manifest to take a name from:
        // that is what failed. A package nobody can read has to be visible in
        // the log, not a mod that is quietly not there.
        Mod failed;
        failed.manifest.dir  = p;
        failed.manifest.name = fs::path(p).filename().string();
        failed.error         = err;
        m_mods.push_back(std::move(failed));
    }

    return load_dirs(dirs);
}

int ModHost::load_dirs(const std::vector<std::string> &dirs)
{
    std::vector<ModManifest> pending;
    for (const std::string &d : dirs) {
        ModManifest m;
        std::string err;
        if (read_manifest(d, m, err)) {
            pending.push_back(std::move(m));
        } else {
            Mod failed;
            failed.manifest = m;
            failed.error    = err;
            m_mods.push_back(std::move(failed));
        }
    }

    // By the NAME a mod loads under and not by where its files are. A package is
    // unpacked somewhere else entirely, so sorting by path would put the mods
    // that arrived as files in a different order from the ones that arrived as
    // directories -- and the reason that order is sorted at all is that what
    // `depends` leaves free has to be the same on every machine.
    // By name, and by where the files are when two mods claim one name -- a
    // total order, because std::sort is free to put equal elements either way
    // round and "which copy of this mod loaded" must not be a coin flipped
    // again on every start. The tie is refused either way; what this decides is
    // which one gets to say so.
    std::sort(pending.begin(), pending.end(), [](const ModManifest &a, const ModManifest &b) {
        if (a.name != b.name) return a.name < b.name;
        return a.dir < b.dir;
    });

    // Topological order by `depends`, taking the alphabetically first mod whose
    // dependencies are already satisfied. A mod whose dependency is missing --
    // or part of a cycle -- never becomes ready and is started last, where
    // start_mod refuses it and says which name it was waiting for.
    std::vector<bool> done(pending.size(), false);
    size_t started = 0;

    while (started < pending.size()) {
        bool progress = false;

        for (size_t i = 0; i < pending.size(); ++i) {
            if (done[i]) continue;

            bool ready = true;
            for (const ModDependency &dep : pending[i].depends) {
                bool satisfied = false;
                for (const Mod &m : m_mods) {
                    satisfied |= (m.running && m.manifest.name == dep.name);
                }
                if (!satisfied) {
                    // Only wait for a dependency that is actually in this
                    // directory; one that is simply absent is refused now
                    // rather than after every other mod has loaded.
                    //
                    // By NAME and not by version: a mod whose dependency is
                    // present but too old has to be started so that start_mod
                    // refuses it with the two numbers in the sentence, and
                    // waiting for a version that will never arrive would hold
                    // it back until the cycle branch started it with no
                    // explanation at all.
                    for (size_t j = 0; j < pending.size(); ++j) {
                        if (!done[j] && j != i && pending[j].name == dep.name) ready = false;
                    }
                }
            }
            if (!ready) continue;

            start_mod(pending[i]);
            done[i] = true;
            ++started;
            progress = true;
        }

        // A cycle: nothing is ready and nothing ever will be. Start what is
        // left in order so each gets its own refusal naming the dependency it
        // was waiting for, instead of the whole set vanishing silently.
        if (!progress) {
            for (size_t i = 0; i < pending.size(); ++i) {
                if (done[i]) continue;
                start_mod(pending[i]);
                done[i] = true;
                ++started;
            }
        }
    }

    for (const Mod &m : m_mods) {
        if (!m.running) {
            std::fprintf(stderr, "mod '%s' not loaded: %s\n",
                         m.manifest.name.c_str(), m.error.c_str());
            continue;
        }

        std::string caps;
        for (const std::string &c : m.manifest.capabilities) {
            caps += caps.empty() ? "" : ", ";
            caps += c;
        }
        std::printf("mod '%s' %s (api %u%s) loaded, capabilities: %s\n",
                    m.manifest.name.c_str(),
                    m.manifest.version.str().c_str(),
                    m.manifest.api,
                    m.manifest.api_declared ? "" : ", assumed",
                    caps.empty() ? "none" : caps.c_str());
    }

    return running_count();
}

void ModHost::step(double dt)
{
    for (Mod &m : m_mods) {
        if (!m.running || !m.vm) continue;

        char err[512];
        const auto t0 = std::chrono::steady_clock::now();
        {
            CurrentMod cur(m.manifest, m.vm);
            if (!script_call_step(m.vm, "on_step", dt, err, sizeof(err))) {
                // The whole point of a VM per mod: this one stops, the loop
                // does not. Disabling rather than retrying is deliberate --
                // a mod that spins forever would otherwise burn its interrupt
                // budget every tick for the rest of the session.
                m.running = false;
                m.error   = err;
                std::fprintf(stderr, "mod '%s' disabled: %s\n", m.manifest.name.c_str(), err);
                continue;
            }
        }
        m.step_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        m.step_calls++;
    }
}

bool ModHost::reload(const std::string &name, std::string &error)
{
    namespace fs = std::filesystem;

    Mod *mod = nullptr;
    for (Mod &m : m_mods) {
        if (m.manifest.name == name) mod = &m;
    }
    if (!mod) {
        error = "no mod named '" + name + "'";
        return false;
    }

    // The manifest is re-read: a reload that kept the old capabilities would
    // let a mod grant itself something by editing a file the loader ignores.
    ModManifest manifest;
    if (!read_manifest(mod->manifest.dir, manifest, error)) return false;

    // The same rule a load is refused by, for the same reason the capabilities
    // are re-read: a mod that edited its manifest into something this build does
    // not serve must keep running the version that it does serve, not swap in.
    if (!admissible(manifest, error)) return false;

    std::vector<std::string> keep;
    if (!mod_capability_globals(manifest.capabilities, keep, error)) return false;

    // State out of the old VM first, while it is still the running one.
    std::string state;
    if (mod->vm && mod->running) {
        std::vector<char> buf(kMaxStateBytes, '\0');
        char err[512];
        CurrentMod cur(mod->manifest, mod->vm);
        if (!script_call_save(mod->vm, "on_save", buf.data(), buf.size(), err, sizeof(err))) {
            error = std::string("on_save failed: ") + err;
            return false;
        }
        state = buf.data();
    }

    if (m_cb.before_reload) m_cb.before_reload(m_cb.ud, name.c_str());

    std::vector<const char *> strip;
    for (const char *g : kOptionalGlobals) {
        if (std::find(keep.begin(), keep.end(), g) == keep.end()) strip.push_back(g);
    }

    script_desc sd{};
    sd.host               = m_host;
    sd.memory_limit_bytes = manifest.memory_limit_bytes;
    sd.interrupt_budget   = manifest.interrupt_budget;
    sd.strip_globals      = strip.data();
    sd.strip_count        = (int)strip.size();

    script_vm *fresh = script_create(&sd);
    if (!fresh) {
        error = "could not create a VM";
        if (m_cb.after_reload) m_cb.after_reload(m_cb.ud, name.c_str(), false);
        return false;
    }

    const std::string entry = (fs::path(manifest.dir) / manifest.entry).string();

    char errbuf[512];
    bool ok = false;
    {
        // The new code runs with the new manifest as the current mod, so
        // anything it registers is attributed to it and not to its predecessor.
        CurrentMod cur(manifest, fresh);
        ok = script_run_file(fresh, entry.c_str(), errbuf, sizeof(errbuf));
        if (ok && !state.empty())
            ok = script_call_load(fresh, "on_load", state.c_str(), errbuf, sizeof(errbuf));
    }

    if (!ok) {
        // The whole reason the new VM is built before the old one is destroyed:
        // a typo in a mod being edited live must cost the edit, not the mod.
        // Anything the failed version managed to register goes with it.
        forget_vm(fresh);
        script_destroy(fresh);
        error = errbuf;
        if (m_cb.after_reload) m_cb.after_reload(m_cb.ud, name.c_str(), false);
        return false;
    }

    if (mod->vm) {
        // The old version's handlers go now. Keyed by VM, because both versions
        // of this mod were registered under the same name a moment ago, and
        // dropping by name would have taken the new ones too.
        forget_vm(mod->vm);
        script_destroy(mod->vm);
    }
    mod->vm          = fresh;
    mod->manifest    = manifest;
    mod->running     = true;
    mod->error.clear();
    mod->reloads++;
    mod->conf_mtime  = file_mtime(fs::path(manifest.dir) / "mod.conf");
    mod->entry_mtime = file_mtime(entry);

    if (m_cb.after_reload) m_cb.after_reload(m_cb.ud, name.c_str(), true);
    return true;
}

int ModHost::reload_changed()
{
    namespace fs = std::filesystem;

    int reloaded = 0;

    for (size_t i = 0; i < m_mods.size(); ++i) {
        const std::string dir  = m_mods[i].manifest.dir;
        const std::string name = m_mods[i].manifest.name;
        if (dir.empty()) continue;

        const long long conf  = file_mtime(fs::path(dir) / "mod.conf");
        const long long entry = file_mtime(fs::path(dir) / m_mods[i].manifest.entry);

        // A zero means the file went away mid-edit -- an editor writing through
        // a temporary. Waiting for it to come back is better than reloading a
        // mod out of a file that does not exist yet.
        if (conf == 0 || entry == 0) continue;
        if (conf == m_mods[i].conf_mtime && entry == m_mods[i].entry_mtime) continue;

        std::string err;
        if (reload(name, err)) {
            std::printf("mod '%s' reloaded\n", name.c_str());
            ++reloaded;
        } else {
            // Record the timestamps anyway: a file that does not compile must
            // not be retried sixty times a second until it is fixed.
            m_mods[i].conf_mtime  = conf;
            m_mods[i].entry_mtime = entry;
            std::fprintf(stderr, "mod '%s' reload failed, keeping the running version: %s\n",
                         name.c_str(), err.c_str());
        }
    }

    return reloaded;
}

void ModHost::shutdown()
{
    for (Mod &m : m_mods) {
        if (m.vm) {
            forget_vm(m.vm);
            script_destroy(m.vm);
        }
        m.vm      = nullptr;
        m.running = false;
    }
    m_mods.clear();
}

int ModHost::running_count() const
{
    int n = 0;
    for (const Mod &m : m_mods) n += m.running ? 1 : 0;
    return n;
}

size_t ModHost::memory_used() const
{
    size_t total = 0;
    for (const Mod &m : m_mods) {
        if (m.vm) total += script_memory_used(m.vm);
    }
    return total;
}

double ModHost::step_seconds() const
{
    double t = 0.0;
    for (const Mod &m : m_mods) t += m.step_seconds;
    return t;
}

uint64_t ModHost::step_calls() const
{
    uint64_t n = 0;
    for (const Mod &m : m_mods) n += m.step_calls;
    return n;
}

} // namespace cnp
