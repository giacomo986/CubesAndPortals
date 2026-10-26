// Isolation, as a gate rather than a claim.
//
// DESIGN.md 8, M2 exit test: "un mod che va in loop infinito viene interrotto
// senza fermare il server". That sentence has two halves and only the first one
// is easy. This test asserts both: the runaway mod dies, and every other mod in
// the process keeps stepping afterwards -- as does the one that exhausted its
// memory quota's neighbour, and the one whose manifest was refused.
//
// No GPU, no window, no world: the host is a stub that records what mods do, so
// this runs anywhere ctest runs.

#include "script/mod_host.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace cnp;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

// The stub host. It answers the same script_host the engine fills in, and it
// remembers everything, which is what the assertions read.
struct StubWorld {
    std::vector<std::string> log;
    int  blocks_set  = 0;
    int  good_ticks  = 0;   // the control mod writes its tick count as a block x
    int  next_id     = 1;
    std::vector<std::string> registered;
};

void h_set_block(void *ud, int x, int y, int z, int id)
{
    StubWorld *w = static_cast<StubWorld *>(ud);
    w->blocks_set++;

    // `good` writes at (ticks, 0, 0). Recording the highest x it reached is how
    // the test knows it was still running at the end.
    const ModManifest *m = mod_host_current();
    if (m && m->name == "good" && y == 0 && z == 0 && x > w->good_ticks) w->good_ticks = x;
}

int h_get_block(void *ud, int x, int y, int z) { return 0; }

void h_set_portal(void *ud, int index, float x, float y, float z) {}

void h_log(void *ud, const char *message)
{
    StubWorld *w = static_cast<StubWorld *>(ud);
    const ModManifest *m = mod_host_current();
    w->log.push_back(std::string(m ? m->name : "?") + ": " + (message ? message : ""));
}

int h_register_node(void *ud, const char *name, int r, int g, int b, int solid, int light,
                    const char *turn, int dig_level, double hardness, const char *material)
{
    StubWorld *w = static_cast<StubWorld *>(ud);
    w->registered.push_back(name ? name : "");
    return w->next_id++;
}

int h_node_id(void *ud, const char *name) { return 0; }

int log_index(const StubWorld &w, const char *needle)
{
    for (size_t i = 0; i < w.log.size(); ++i) {
        if (w.log[i].find(needle) != std::string::npos) return (int)i;
    }
    return -1;
}

bool log_contains(const StubWorld &w, const char *needle)
{
    return log_index(w, needle) >= 0;
}

const Mod *find_mod(const ModHost &host, const char *name)
{
    for (const Mod &m : host.mods()) {
        if (m.manifest.name == name) return &m;
    }
    return nullptr;
}

void test_manifest_parsing()
{
    std::printf("manifest\n");

    ModManifest m;
    std::string err;
    check(mod_manifest_parse("name = a\n# comment\ncapabilities = os, debug\nmemory_kb = 4096\n",
                             m, err),
          "parses a well-formed manifest");
    check(m.name == "a" && m.capabilities.size() == 2 && m.memory_limit_bytes == 4096u * 1024u,
          "reads name, capability list and memory quota");

    ModManifest bad;
    check(!mod_manifest_parse("capabilities = os\n", bad, err), "refuses a manifest with no name");
    check(!mod_manifest_parse("name = a\ncapabilties = os\n", bad, err),
          "refuses an unknown key instead of ignoring it");

    std::vector<std::string> keep;
    check(mod_capability_globals({"os"}, keep, err) && keep.size() == 1, "grants os");
    check(!mod_capability_globals({"native"}, keep, err), "refuses native");
    check(!mod_capability_globals({"fs:read"}, keep, err), "refuses fs:read");
    check(!mod_capability_globals({"telepathy"}, keep, err), "refuses an unknown capability");
}

void test_isolation(const char *mods_dir)
{
    std::printf("\nloading %s\n", mods_dir);

    StubWorld world;

    script_host host{};
    host.set_block     = h_set_block;
    host.get_block     = h_get_block;
    host.set_portal    = h_set_portal;
    host.log           = h_log;
    host.register_node = h_register_node;
    host.node_id       = h_node_id;
    host.ud            = &world;

    ModHost mods;
    mods.set_host(host);
    const int running = mods.load_dir(mods_dir);

    // dependent, good, hog, runaway and timekeeper load; badcap, nomanifest,
    // orphan and typo do not.
    check(running == 5, "five mods loaded, four refused");

    const Mod *good    = find_mod(mods, "good");
    const Mod *runaway = find_mod(mods, "runaway");
    const Mod *hog     = find_mod(mods, "hog");
    const Mod *tk      = find_mod(mods, "timekeeper");
    const Mod *badcap  = find_mod(mods, "badcap");
    const Mod *typo    = find_mod(mods, "typo");
    const Mod *orphan  = find_mod(mods, "orphan");
    const Mod *dep     = find_mod(mods, "dependent");

    check(good && good->running,       "the control mod is running");
    check(tk && tk->running,           "the mod that declared os is running");
    check(badcap && !badcap->running && badcap->error.find("native") != std::string::npos,
          "the mod asking for `native` was refused, by name");
    check(typo && !typo->running && typo->error.find("capabilties") != std::string::npos,
          "the misspelled key was refused, by name");
    check(orphan && !orphan->running && orphan->error.find("ghost") != std::string::npos,
          "the mod depending on something absent was refused, naming it");
    check(!log_contains(world, "SHOULD NOT RUN"), "no refused mod executed a line");

    // Load order is the dependency order, not the directory order: `dependent`
    // sorts before `timekeeper` and must still load after it.
    const int i_tk  = log_index(world, "loaded, os.clock works");
    const int i_dep = log_index(world, "dependent: loaded");
    check(dep && dep->running, "the mod with a satisfied dependency is running");
    check(i_tk >= 0 && i_dep > i_tk, "a dependency loads before the mod that declared it");

    // Capability isolation: `timekeeper` asserts at load that it has os and not
    // debug, `good` that it has neither. Both loading is the assertion.
    check(log_contains(world, "loaded, os.clock works"),
          "os is present in the VM that declared it");

    // Two steps. One is enough for the runaway mod to be interrupted and for the
    // hog to hit its ceiling; the second proves the survivors survived it.
    mods.step(1.0 / 60.0);
    const int ticks_after_first = world.good_ticks;
    mods.step(1.0 / 60.0);

    check(runaway && !runaway->running, "the runaway mod was stopped");
    check(runaway && runaway->error.find("interrupt budget") != std::string::npos,
          "and stopped for the stated reason, not by accident");

    check(hog && !hog->running, "the memory hog was stopped");
    check(hog && (hog->error.find("memory") != std::string::npos ||
                  hog->error.find("not enough") != std::string::npos),
          "and stopped by its quota");

    check(good && good->running, "the control mod is still running after both died");
    check(tk && tk->running,     "so is the one that declared a capability");
    check(world.good_ticks == ticks_after_first + 1,
          "and it stepped again: the tick continued past the failures");
    check(good && good->step_calls == 2, "the control mod was called both ticks");
    check(dep && dep->step_calls == 2, "and so was the dependent mod");

    // Registration is attributed, which is what a reload will need in order to
    // drop one mod's entries and nobody else's.
    bool saw_good_block = false;
    for (const std::string &n : world.registered) saw_good_block |= (n == "good:block");
    check(saw_good_block, "the control mod's node reached the registry");

    check(mods.memory_used() > 0, "per-VM memory is measurable");

    mods.shutdown();
    check(mods.running_count() == 0, "shutdown closes every VM");
}

} // namespace

int main(int argc, char **argv)
{
    const char *mods_dir = (argc > 1) ? argv[1] : "mods";

    test_manifest_parsing();
    test_isolation(mods_dir);

    std::printf("\n%s\n", g_failures == 0 ? "all script isolation checks passed"
                                          : "SCRIPT ISOLATION CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
