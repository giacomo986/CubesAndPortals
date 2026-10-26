#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "flecs.h"

#include "core/ecs/components.h"
#include "script/script.h"

// Entities a mod owns, and the systems that step them (DESIGN.md 6.1, and the
// half of M8 its exit test did not reach).
//
// Three claims are what make this worth having over "a mod keeps a Lua table of
// things and walks it in on_step":
//
//   - the entities are the ENGINE's entities. A pellet is a flecs entity with a
//     Position, in the same world the mobs and the drops are in, so a mod's
//     thing and the engine's thing are one kind of thing and not two. A second
//     entity store would mean two id spaces for one idea, which is the shape
//     this repo keeps finding at the bottom of its defects.
//   - a system is called ONCE for many entities, with columns of numbers it
//     indexes and writes back into. A call per entity spends more time crossing
//     the boundary than inside it.
//   - systems run in PHASES ordered by a number, so two mods that never met
//     still run in a stated order instead of in whatever order they loaded.
//
// What this is NOT, stated rather than discovered: a system names ONE component
// and is given the entities that have it. There is no join over two components,
// and a system that needs a second one reads it by id -- which is a call per
// entity and gives back exactly what the batch was for. Fields are numbers;
// strings belong on a cell (core/world/node_meta.h) or in the mod.
//
// This header includes script/script.h for script_batch rather than describing
// a batch a second time. It does not include a Lua header and does not link
// against the VM: running a system is a function pointer handed in by whoever
// owns the mods.

namespace cnp::ecs {

// One call into Lua per this many entities. Not "all of them in one call": a
// mod with a hundred thousand entities would build a hundred thousand Lua
// numbers per column in a VM with a 32 MB quota, and fail its allocation
// instead of running. The number is stated in the docs because a mod measuring
// its own call count will see it.
inline constexpr int kLuaBatchMax = 4096;

class LuaEcs {
public:
    struct Component {
        std::string              name;
        std::string              owner;
        std::vector<std::string> fields;
        ecs_entity_t             id = 0;
    };

    struct System {
        std::string name;
        std::string owner;
        std::string phase;
        std::string component;
        script_vm  *vm  = nullptr;
        int         ref = 0;
        uint64_t    seq = 0;     // ties inside a phase go to whoever asked first
    };

    struct Phase {
        std::string name;
        std::string owner;
        int         order = 0;
        uint64_t    seq   = 0;
    };

    // How a system is run. The signature is script_call_system's, so whoever
    // owns the mods passes that function itself rather than a wrapper: one
    // description of the call, not two.
    using RunFn = bool (*)(script_vm *vm, int handler_ref, script_batch *batch, char *err,
                           size_t err_size);

    // Told when a system errors, so the mod that owns it can be disabled. This
    // layer does not know what a mod is.
    using ErrorFn = std::function<void(const std::string &owner, script_vm *vm,
                                       const std::string &system, const std::string &error)>;

    // The world arrives AFTER the mods have loaded: on a server the sessions
    // that own it are built once the socket is, and a mod declares its
    // components while it loads. So a declaration is remembered and the flecs
    // component is minted when there is somewhere to put it.
    void set_world(flecs::world *w);
    void set_error(ErrorFn fn) { m_on_error = std::move(fn); }

    // Called around every system, so that whatever is on the other side of a
    // host call can name the mod whose code is running. The same pair the event
    // bus has, and for the same reason.
    using ScopeFn = std::function<void(script_vm *)>;
    void set_scope(ScopeFn enter, ScopeFn leave)
    {
        m_enter = std::move(enter);
        m_leave = std::move(leave);
    }

    // Declaring the same component twice is fine if it is the same mod: that is
    // a reload. A different mod is an error -- a component's shape belongs to
    // whoever declared it, exactly like an event's.
    bool component(const std::string &name, const std::vector<std::string> &fields,
                   const std::string &owner, std::string &error);

    // A phase is a name and a number. Two mods giving one name two different
    // numbers is an error and names both: silently keeping the first would put
    // one of them in an order it did not ask for and never say so.
    bool phase(const std::string &name, int order, const std::string &owner, std::string &error);

    bool system(const std::string &name, const std::string &phase, const std::string &component,
                script_vm *vm, int handler_ref, const std::string &owner, std::string &error);

    // 0 when the component is not one anybody declared. Ids are doubles all the
    // way out to Lua: an entity id carries a generation above its index, so it
    // leaves 32 bits behind the first time anything is recycled.
    double spawn(const std::string &component, double x, double y, double z);
    bool   set(double id, const std::string &field, double value);
    bool   get(double id, const std::string &field, double &out) const;
    bool   kill(double id);

    // Whether other players are sent this entity (TODO.md OPEN-20). On adds a
    // Body, a Look and the Thing tag, so the streaming query that already sends
    // people, creatures and stacks sends it too, with no change to that query;
    // off takes them away again. Only a mod's own: an entity that already has a
    // body the engine gave it -- a player, a mob, a stack -- is refused both
    // ways, so a mod cannot hide a player or give one a second body.
    bool   visible(double id, bool on);
    // What a visible one is drawn as: a model id from core.register_model, 0
    // for the plain box. Only on an entity visible() made visible.
    bool   model(double id, uint16_t model);
    // Its health: `set` 0..65535 gives it that much (and that much as its
    // most), so a player's hit takes points off it; below 0 only asks. The
    // health it has after, or -1 for none or not a visible one of ours. At 0
    // it is left where it is: the mod that gave it health decides what dying
    // is.
    int    health(double id, int set);
    // A clip of its model played by name in place of the one its motion picks
    // (core.ecs.clip): an empty name gives it back to its motion. "death"
    // plays once and holds its last pose. Only on a visible one of ours.
    bool   clip(double id, const std::string &name);
    // How big it is to a shot or a burst: half its width round its middle,
    // its height up from its feet (0.05..8, 0.05..16). Only on a visible one.
    bool   box(double id, double half, double height);
    int    count(const std::string &component) const;

    // Every system, phase by phase, lowest order first. Ties inside a phase go
    // to whoever registered first.
    void step(double dt, RunFn run);

    // A reloaded mod's old VM takes its systems with it. Keyed by VM and not by
    // name because during a reload both VMs of one mod exist at once.
    void drop_vm(script_vm *vm);

    uint64_t tick() const { return m_tick; }
    size_t   component_count() const { return m_components.size(); }
    size_t   system_count() const { return m_systems.size(); }
    size_t   phase_count() const { return m_phases.size(); }
    uint64_t calls() const { return m_calls; }
    uint64_t entity_steps() const { return m_entity_steps; }

private:
    void             mint_pending();
    const Component *find_component(const std::string &name) const;
    Component       *find_component(const std::string &name);
    double          *data_of(ecs_entity_t e, const Component &c) const;

    flecs::world *m_world = nullptr;
    ErrorFn       m_on_error;
    ScopeFn       m_enter;
    ScopeFn       m_leave;

    std::vector<Component> m_components;
    std::vector<System>    m_systems;
    std::vector<Phase>     m_phases;

    uint64_t m_seq          = 0;
    uint64_t m_tick         = 0;
    uint64_t m_calls        = 0;
    uint64_t m_entity_steps = 0;

    // Scratch, kept between ticks so a system that runs every tick does not
    // allocate every tick.
    mutable std::vector<ecs_entity_t>   m_ids;
    mutable std::vector<double>         m_id_column;
    mutable std::vector<double>         m_values;    // column-major, count per column
    mutable std::vector<script_column>  m_columns;
    mutable std::vector<std::string>    m_column_names;
};

} // namespace cnp::ecs
