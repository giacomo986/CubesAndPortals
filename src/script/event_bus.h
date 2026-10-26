#pragma once

#include "script/script.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The typed event bus of DESIGN.md 6.3.
//
// Three properties are what make it worth having over a table of callbacks:
//
//   - the shape of an event is **declared**, so emitting the wrong field or the
//     wrong type is an error that names the field rather than a nil somebody
//     reads three handlers later;
//   - handlers have a **priority**, so ordering is stated instead of being
//     whatever order the mods happened to load in;
//   - an event can be **cancelled or modified**, so a mod can say "not here"
//     without the engine having a special case for it. The core emits the same
//     events every mod does; there is no privileged path.
//
// The bus lives above the VMs because an event emitted in one mod's VM has to
// reach handlers in another's, and no VM can be trusted to hold that list.

namespace cnp {

struct EventField {
    std::string  name;
    script_vtype type = SCRIPT_V_NUMBER;
};

struct EventSchema {
    std::string             name;
    std::string             owner;        // the mod that declared it, "core" for the engine
    std::vector<EventField> fields;
    bool                    cancellable = false;
};

class EventBus {
public:
    // Called when a handler errors. The bus does not know what a mod is, so
    // deciding what happens to one is somebody else's job -- ModHost disables it.
    using HandlerErrorFn = std::function<void(const std::string &owner, script_vm *vm,
                                              const std::string &event, const std::string &error)>;

    void set_handler_error(HandlerErrorFn fn) { m_on_error = std::move(fn); }

    // Called around each handler, so that whatever is on the other side of a
    // host call can name the mod whose code is running.
    //
    // The bus does not know what a mod is -- it holds a VM and an owner string
    // -- so it cannot set that itself; ModHost can, and does. Without it every
    // host callback made from inside a handler is attributed to nobody, which
    // is not a crash and not an error message: it is a mod being refused
    // permission to touch its own panel, because the owner it is checked
    // against is the empty string. That is what this pair was written for.
    using ScopeFn = std::function<void(script_vm *vm)>;
    void set_scope(ScopeFn enter, ScopeFn leave)
    {
        m_enter = std::move(enter);
        m_leave = std::move(leave);
    }

    // Declaring the same event twice is fine if it is the same mod doing it --
    // that is a reload. A different mod gets an error: an event's shape belongs
    // to whoever declared it.
    bool define(const EventSchema &schema, std::string &error);

    // Listening to an event nobody declared is an error, and the message names
    // it. Silently accepting it is how a typo becomes a handler that never runs
    // and a bug report that says "my mod does nothing".
    bool on(const std::string &event, int priority, script_vm *vm, int handler_ref,
            const std::string &owner, std::string &error);

    // Runs every handler in priority order, then registration order. `fields` is
    // validated against the schema, and updated in place by handlers that change
    // something. Cancelling stops the dispatch: a handler that said "no" has
    // said it, and running the rest would only give them a vote they cannot use.
    // `only_owner`, when given, limits the dispatch to that mod's handlers: a
    // message that came in on one mod's channel is that mod's post (OPEN-50).
    bool emit(const std::string &event, script_field *fields, int count, bool *cancelled,
              std::string &error, const std::string *only_owner = nullptr);

    // Drops the handlers registered by one VM. Keyed by VM and not by mod name
    // because during a reload both the old and the new VM of the same mod exist
    // at once, and only one of them is going away.
    void drop_vm(script_vm *vm);

    size_t schema_count() const { return m_schemas.size(); }
    size_t handler_count() const;
    const EventSchema *schema(const std::string &name) const;

    // Counters, so a bench can say what the bus cost rather than guess.
    uint64_t emits() const { return m_emits; }
    uint64_t dispatches() const { return m_dispatches; }

private:
    struct Handler {
        std::string owner;
        script_vm  *vm       = nullptr;
        int         ref      = 0;
        int         priority = 0;
        uint64_t    seq      = 0;   // ties broken by registration order, not by luck
    };

    std::unordered_map<std::string, EventSchema>           m_schemas;
    std::unordered_map<std::string, std::vector<Handler>>  m_handlers;

    HandlerErrorFn m_on_error;
    ScopeFn        m_enter;
    ScopeFn        m_leave;
    uint64_t       m_seq        = 0;
    uint64_t       m_emits      = 0;
    uint64_t       m_dispatches = 0;

    // Storage for strings a handler wrote, since those point into the VM that
    // wrote them and stop being valid at the next call into it.
    std::vector<std::string> m_string_scratch;

    // Keys a handler set that its event does not have, already reported.
    std::unordered_set<std::string> m_strays_told;
};

} // namespace cnp
