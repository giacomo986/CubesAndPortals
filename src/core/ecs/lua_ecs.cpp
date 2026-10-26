#include "core/ecs/lua_ecs.h"

#include "core/ecs/health.h"
#include "core/ecs/mob.h"

#include <algorithm>
#include <cstring>

namespace cnp::ecs {

namespace {

// An id crosses to Lua as a double and comes back as one. Exact both ways: a
// flecs id is an index in its low 32 bits and a generation above, which is well
// inside what a double holds without rounding.
ecs_entity_t entity_of(double id)
{
    if (id <= 0.0) return 0;
    return (ecs_entity_t)id;
}

} // namespace

void LuaEcs::set_world(flecs::world *w)
{
    m_world = w;
    mint_pending();
}

// Every component declared before there was a world to put entities in. Called
// when the world arrives and again on the paths that need an id, so that a mod
// declaring one during a reload -- after the world exists -- costs nothing.
void LuaEcs::mint_pending()
{
    if (!m_world) return;
    ecs_world_t *w = m_world->c_ptr();
    for (Component &c : m_components) {
        if (c.id) continue;
        ecs_component_desc_t cd{};
        cd.entity         = ecs_new(w);
        cd.type.size      = (ecs_size_t)(c.fields.size() * sizeof(double));
        cd.type.alignment = (ecs_size_t)alignof(double);
        c.id              = ecs_component_init(w, &cd);
    }
}

const LuaEcs::Component *LuaEcs::find_component(const std::string &name) const
{
    for (const Component &c : m_components)
        if (c.name == name) return &c;
    return nullptr;
}

LuaEcs::Component *LuaEcs::find_component(const std::string &name)
{
    for (Component &c : m_components)
        if (c.name == name) return &c;
    return nullptr;
}

double *LuaEcs::data_of(ecs_entity_t e, const Component &c) const
{
    if (!m_world || !e || !c.id) return nullptr;
    ecs_world_t *w = m_world->c_ptr();
    if (!ecs_is_alive(w, e)) return nullptr;
    return (double *)ecs_get_mut_id(w, e, c.id);
}

bool LuaEcs::component(const std::string &name, const std::vector<std::string> &fields,
                       const std::string &owner, std::string &error)
{
    if (name.empty()) {
        error = "a component needs a name";
        return false;
    }
    if (fields.empty()) {
        error = "a component with no fields is a tag, and '" + name + "' asked for neither";
        return false;
    }
    for (const std::string &f : fields)
        if (f.empty() || f == "x" || f == "y" || f == "z" || f == "yaw" || f == "id" || f == "n" ||
            f == "dt" || f == "tick") {
            error = "'" + f + "' is a name a batch already uses";
            return false;
        }

    if (Component *existing = find_component(name)) {
        if (existing->owner != owner) {
            error = "'" + name + "' belongs to mod '" + existing->owner + "'";
            return false;
        }
        // The same mod again: a reload. The fields have to be the same, because
        // entities of the old shape are still in the world and the new code
        // would read them through the new one.
        if (existing->fields != fields) {
            error = "'" + name + "' was declared with different fields, and there are " +
                    std::to_string(count(name)) + " of it in the world";
            return false;
        }
        return true;
    }

    // Registered at runtime and left unnamed: flecs reads '.' in a name as a
    // scope separator and a mod's names carry ':', so the registry that matters
    // is the one here -- m_components, keyed by the name the mod chose.
    Component c;
    c.name   = name;
    c.owner  = owner;
    c.fields = fields;
    m_components.push_back(std::move(c));
    mint_pending();
    return true;
}

bool LuaEcs::phase(const std::string &name, int order, const std::string &owner,
                   std::string &error)
{
    if (name.empty()) {
        error = "a phase needs a name";
        return false;
    }
    for (Phase &p : m_phases) {
        if (p.name != name) continue;
        if (p.order == order) return true;   // the same phase again, said the same way
        error = "phase '" + name + "' is " + std::to_string(p.order) + " to mod '" + p.owner +
                "' and " + std::to_string(order) + " to '" + owner + "'";
        return false;
    }

    Phase p;
    p.name  = name;
    p.owner = owner;
    p.order = order;
    p.seq   = m_seq++;
    m_phases.push_back(std::move(p));
    return true;
}

bool LuaEcs::system(const std::string &name, const std::string &phase_name,
                    const std::string &component_name, script_vm *vm, int handler_ref,
                    const std::string &owner, std::string &error)
{
    if (name.empty()) {
        error = "a system needs a name";
        return false;
    }
    if (!find_component(component_name)) {
        error = "no component is called '" + component_name + "'";
        return false;
    }

    bool known_phase = false;
    for (const Phase &p : m_phases)
        if (p.name == phase_name) known_phase = true;
    if (!known_phase) {
        // Listening on a phase nobody declared is an error, and the message
        // names it -- the same rule the event bus has, for the same reason: a
        // typo becomes a system that never runs and a bug report that says the
        // mod does nothing.
        error = "no phase is called '" + phase_name + "'";
        return false;
    }

    for (System &s : m_systems)
        if (s.name == name && s.owner == owner) {
            s.phase     = phase_name;
            s.component = component_name;
            s.vm        = vm;
            s.ref       = handler_ref;
            return true;
        }

    System s;
    s.name      = name;
    s.owner     = owner;
    s.phase     = phase_name;
    s.component = component_name;
    s.vm        = vm;
    s.ref       = handler_ref;
    s.seq       = m_seq++;
    m_systems.push_back(std::move(s));
    return true;
}

double LuaEcs::spawn(const std::string &component_name, double x, double y, double z)
{
    if (!m_world) return 0.0;
    const Component *c = find_component(component_name);
    if (!c || !c->id) return 0.0;

    // A Position and nothing else. No Body: what a client is sent is "the
    // entities near them with a Body", and a mod's thing is not one of those
    // until somebody asks for it to be -- which is a call this does not have.
    Position p{};
    p.x     = (float)x;
    p.y     = (float)y;
    p.z     = (float)z;
    p.valid = true;

    flecs::entity e = m_world->entity().set<Position>(p);

    std::vector<double> zeros(c->fields.size(), 0.0);
    ecs_set_id(m_world->c_ptr(), e.id(), c->id, c->fields.size() * sizeof(double), zeros.data());
    return (double)e.id();
}

bool LuaEcs::set(double id, const std::string &field, double value)
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return false;

    if (field == "x" || field == "y" || field == "z") {
        if (!ecs_is_alive(m_world->c_ptr(), e)) return false;
        flecs::entity ent(m_world->c_ptr(), e);
        Position     *p = ent.try_get_mut<Position>();
        if (!p) return false;
        if (field == "x") p->x = (float)value;
        if (field == "y") p->y = (float)value;
        if (field == "z") p->z = (float)value;
        return true;
    }
    if (field == "yaw") {   // which way it faces, as a player's yaw: 0 looks down -z
        if (!ecs_is_alive(m_world->c_ptr(), e)) return false;
        flecs::entity ent(m_world->c_ptr(), e);
        Look         *l = ent.try_get_mut<Look>();
        if (!l) return false;
        l->yaw = (float)value;
        return true;
    }

    for (const Component &c : m_components) {
        for (size_t i = 0; i < c.fields.size(); ++i) {
            if (c.fields[i] != field) continue;
            double *data = data_of(e, c);
            if (!data) continue;   // that entity is not one of THIS component's
            data[i] = value;
            return true;
        }
    }
    return false;
}

bool LuaEcs::get(double id, const std::string &field, double &out) const
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return false;

    if (field == "x" || field == "y" || field == "z") {
        if (!ecs_is_alive(m_world->c_ptr(), e)) return false;
        flecs::entity      ent(m_world->c_ptr(), e);
        const Position    *p = ent.try_get<Position>();
        if (!p) return false;
        out = field == "x" ? p->x : field == "y" ? p->y : p->z;
        return true;
    }
    if (field == "yaw") {
        if (!ecs_is_alive(m_world->c_ptr(), e)) return false;
        flecs::entity ent(m_world->c_ptr(), e);
        const Look   *l = ent.try_get<Look>();
        if (!l) return false;
        out = l->yaw;
        return true;
    }

    for (const Component &c : m_components)
        for (size_t i = 0; i < c.fields.size(); ++i) {
            if (c.fields[i] != field) continue;
            const double *data = data_of(e, c);
            if (!data) continue;
            out = data[i];
            return true;
        }
    return false;
}

bool LuaEcs::kill(double id)
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return false;
    if (!ecs_is_alive(m_world->c_ptr(), e)) return false;
    ecs_delete(m_world->c_ptr(), e);
    return true;
}

bool LuaEcs::visible(double id, bool on)
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return false;
    if (!ecs_is_alive(m_world->c_ptr(), e)) return false;

    flecs::entity ent(m_world->c_ptr(), e);

    if (on) {
        // Already one of ours and already visible: yes. A body the engine gave
        // it: not ours to touch.
        if (ent.has<Body>()) return ent.has<Thing>();
        ent.add<Thing>();
        ent.add<Body>();
        if (!ent.has<Look>()) ent.set<Look>({});
        return true;
    }

    if (!ent.has<Thing>()) return false;   // only what a mod made visible is hidden again
    ent.remove<Body>();
    ent.remove<Thing>();
    return true;
}

bool LuaEcs::model(double id, uint16_t model)
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return false;
    if (!ecs_is_alive(m_world->c_ptr(), e)) return false;

    flecs::entity ent(m_world->c_ptr(), e);
    if (!ent.has<Thing>()) return false;   // a person's or a creature's look is not a mod's
    ent.set<Wears>({model});
    return true;
}

bool LuaEcs::clip(double id, const std::string &name)
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return false;
    if (!ecs_is_alive(m_world->c_ptr(), e)) return false;

    flecs::entity ent(m_world->c_ptr(), e);
    if (!ent.has<Thing>()) return false;   // a creature's clips are its mod's through core.mob_act
    if (name.empty())
        ent.remove<Act>();
    else
        ent.set<Act>({name.substr(0, 32), kNoItem});
    return true;
}

bool LuaEcs::box(double id, double half, double height)
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return false;
    if (!ecs_is_alive(m_world->c_ptr(), e)) return false;
    if (!(half >= 0.05 && half <= 8.0 && height >= 0.05 && height <= 16.0)) return false;

    flecs::entity ent(m_world->c_ptr(), e);
    if (!ent.has<Thing>()) return false;
    ent.set<ThingBox>({(float)half, (float)height});
    return true;
}

int LuaEcs::health(double id, int set)
{
    const ecs_entity_t e = entity_of(id);
    if (!e || !m_world) return -1;
    if (!ecs_is_alive(m_world->c_ptr(), e)) return -1;

    flecs::entity ent(m_world->c_ptr(), e);
    if (!ent.has<Thing>()) return -1;   // a person's or a creature's is the engine's
    if (set >= 0) {
        const uint16_t hp = (uint16_t)std::min(set, 65535);
        ent.set<Health>({hp, hp});
    }
    const Health *h = ent.try_get<Health>();
    return h ? (int)h->hp : -1;
}

int LuaEcs::count(const std::string &component_name) const
{
    if (!m_world) return 0;
    const Component *c = find_component(component_name);
    if (!c || !c->id) return 0;
    return (int)ecs_count_id(m_world->c_ptr(), c->id);
}

void LuaEcs::drop_vm(script_vm *vm)
{
    m_systems.erase(std::remove_if(m_systems.begin(), m_systems.end(),
                                   [vm](const System &s) { return s.vm == vm; }),
                    m_systems.end());
}

void LuaEcs::step(double dt, RunFn run)
{
    if (!m_world || !run || m_systems.empty()) return;
    ++m_tick;

    // Phases first, by the number they declared. Sorted here rather than kept
    // sorted because a phase is declared once at load and this runs every tick
    // over a handful of them.
    std::vector<const Phase *> order;
    order.reserve(m_phases.size());
    for (const Phase &p : m_phases) order.push_back(&p);
    std::sort(order.begin(), order.end(), [](const Phase *a, const Phase *b) {
        if (a->order != b->order) return a->order < b->order;
        return a->seq < b->seq;
    });

    ecs_world_t *w = m_world->c_ptr();

    for (const Phase *ph : order) {
        // A copy of the list, because a system that spawns or kills does not
        // change what runs this tick -- and because a system that errors takes
        // its mod down, which erases entries from m_systems while this walks it.
        std::vector<System> due;
        for (const System &s : m_systems)
            if (s.phase == ph->name) due.push_back(s);
        std::sort(due.begin(), due.end(),
                  [](const System &a, const System &b) { return a.seq < b.seq; });

        for (const System &s : due) {
            const Component *c = find_component(s.component);
            if (!c || !c->id || !s.vm) continue;

            m_ids.clear();
            ecs_iter_t it = ecs_each_id(w, c->id);
            while (ecs_each_next(&it))
                for (int i = 0; i < it.count; ++i) m_ids.push_back(it.entities[i]);
            if (m_ids.empty()) continue;

            const int ncols = 3 + (int)c->fields.size();
            m_column_names.assign({"x", "y", "z"});
            for (const std::string &f : c->fields) m_column_names.push_back(f);

            for (size_t first = 0; first < m_ids.size(); first += (size_t)kLuaBatchMax) {
                const int n =
                    (int)std::min((size_t)kLuaBatchMax, m_ids.size() - first);

                m_id_column.assign((size_t)n, 0.0);
                m_values.assign((size_t)n * (size_t)ncols, 0.0);

                // Read out. An entity may have been killed by an earlier system
                // in this same tick, so every row is checked rather than
                // assumed -- a dead one is left as a zero row and written back
                // to nothing.
                for (int i = 0; i < n; ++i) {
                    const ecs_entity_t e = m_ids[first + (size_t)i];
                    m_id_column[(size_t)i] = (double)e;
                    if (!ecs_is_alive(w, e)) continue;

                    flecs::entity   ent(w, e);
                    const Position *p = ent.try_get<Position>();
                    if (p) {
                        m_values[(size_t)0 * (size_t)n + (size_t)i] = p->x;
                        m_values[(size_t)1 * (size_t)n + (size_t)i] = p->y;
                        m_values[(size_t)2 * (size_t)n + (size_t)i] = p->z;
                    }
                    const double *data = data_of(e, *c);
                    if (data)
                        for (size_t f = 0; f < c->fields.size(); ++f)
                            m_values[(3 + f) * (size_t)n + (size_t)i] = data[f];
                }

                m_columns.assign((size_t)ncols, script_column{});
                for (int col = 0; col < ncols; ++col) {
                    m_columns[(size_t)col].name   = m_column_names[(size_t)col].c_str();
                    m_columns[(size_t)col].values = m_values.data() + (size_t)col * (size_t)n;
                }

                script_batch batch{};
                batch.count        = n;
                batch.dt           = dt;
                batch.tick         = (double)m_tick;
                batch.ids          = m_id_column.data();
                batch.columns      = m_columns.data();
                batch.column_count = ncols;

                char err[256] = {0};
                ++m_calls;
                m_entity_steps += (uint64_t)n;
                if (m_enter) m_enter(s.vm);
                const bool ok = run(s.vm, s.ref, &batch, err, sizeof(err));
                if (m_leave) m_leave(s.vm);
                if (!ok) {
                    if (m_on_error) m_on_error(s.owner, s.vm, s.name, err);
                    break;   // this system is done for the tick; its mod is going away
                }

                // And back in. Position is the engine's copy of where the thing
                // is, so a system that moved something has moved it.
                for (int i = 0; i < n; ++i) {
                    const ecs_entity_t e = m_ids[first + (size_t)i];
                    if (!ecs_is_alive(w, e)) continue;

                    flecs::entity ent(w, e);
                    Position     *p = ent.try_get_mut<Position>();
                    if (p) {
                        p->x = (float)m_values[(size_t)0 * (size_t)n + (size_t)i];
                        p->y = (float)m_values[(size_t)1 * (size_t)n + (size_t)i];
                        p->z = (float)m_values[(size_t)2 * (size_t)n + (size_t)i];
                    }
                    double *data = data_of(e, *c);
                    if (data)
                        for (size_t f = 0; f < c->fields.size(); ++f)
                            data[f] = m_values[(3 + f) * (size_t)n + (size_t)i];
                }
            }
        }
    }
}

} // namespace cnp::ecs
