#include "script/event_bus.h"

#include <algorithm>
#include <cstdio>

namespace cnp {
namespace {

const char *type_name(script_vtype t)
{
    switch (t) {
        case SCRIPT_V_NUMBER: return "number";
        case SCRIPT_V_STRING: return "string";
        case SCRIPT_V_BOOL:   return "boolean";
    }
    return "?";
}

} // namespace

bool EventBus::define(const EventSchema &schema, std::string &error)
{
    error.clear();

    if (schema.name.empty()) {
        error = "an event needs a name";
        return false;
    }

    auto it = m_schemas.find(schema.name);
    if (it != m_schemas.end() && it->second.owner != schema.owner) {
        error = "event '" + schema.name + "' is already declared by '" + it->second.owner + "'";
        return false;
    }

    m_schemas[schema.name] = schema;
    return true;
}

bool EventBus::on(const std::string &event, int priority, script_vm *vm, int handler_ref,
                  const std::string &owner, std::string &error)
{
    error.clear();

    if (m_schemas.find(event) == m_schemas.end()) {
        error = "no event named '" + event + "' has been declared";
        return false;
    }

    Handler h;
    h.owner    = owner;
    h.vm       = vm;
    h.ref      = handler_ref;
    h.priority = priority;
    h.seq      = m_seq++;

    std::vector<Handler> &list = m_handlers[event];
    list.push_back(h);

    std::stable_sort(list.begin(), list.end(), [](const Handler &a, const Handler &b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.seq < b.seq;
    });

    return true;
}

bool EventBus::emit(const std::string &event, script_field *fields, int count, bool *cancelled,
                    std::string &error, const std::string *only_owner)
{
    error.clear();
    if (cancelled) *cancelled = false;

    auto sit = m_schemas.find(event);
    if (sit == m_schemas.end()) {
        error = "no event named '" + event + "' has been declared";
        return false;
    }
    const EventSchema &schema = sit->second;

    // Validation against the declared shape. This is the entire point of a
    // schema: a payload that does not match is refused here, by name, instead
    // of arriving at a handler as a nil.
    if (count != (int)schema.fields.size()) {
        error = "event '" + event + "' takes " + std::to_string(schema.fields.size()) +
                " field(s), got " + std::to_string(count);
        return false;
    }

    for (const EventField &want : schema.fields) {
        const script_field *got = nullptr;
        for (int i = 0; i < count; ++i) {
            if (want.name == fields[i].name) got = &fields[i];
        }
        if (!got) {
            error = "event '" + event + "' is missing field '" + want.name + "'";
            return false;
        }
        if (got->type != want.type) {
            error = "event '" + event + "', field '" + want.name + "': expected " +
                    type_name(want.type) + ", got " + type_name(got->type);
            return false;
        }
    }

    ++m_emits;

    auto hit = m_handlers.find(event);
    if (hit == m_handlers.end() || hit->second.empty()) return true;

    // A copy, because a handler may register or unregister handlers -- and
    // because a mod disabled mid-dispatch is removed from the list under us.
    const std::vector<Handler> list = hit->second;

    m_string_scratch.assign((size_t)count, std::string());

    // Names the running mod for the length of one handler, and puts back
    // whatever was named before -- handlers nest, because a handler may emit.
    // A struct rather than two calls, so the `continue` on the error path below
    // cannot leave a mod named after it has stopped running.
    struct Scope {
        const EventBus *bus;
        script_vm      *vm;
        Scope(const EventBus *b, script_vm *v) : bus(b), vm(v)
        {
            if (bus->m_enter) bus->m_enter(vm);
        }
        ~Scope()
        {
            if (bus->m_leave) bus->m_leave(vm);
        }
    };

    for (const Handler &h : list) {
        if (only_owner && h.owner != *only_owner) continue;
        script_event_result result{};
        char err[512] = {0};

        Scope scope(this, h.vm);

        if (!script_call_handler(h.vm, h.ref, fields, count, schema.cancellable ? 1 : 0,
                                 &result, err, sizeof(err))) {
            if (m_on_error) m_on_error(h.owner, h.vm, event, err);
            continue;
        }
        ++m_dispatches;

        // Told once per mod, event and key: a handler runs every tick, and the
        // point is that somebody reads it.
        if (result.stray[0] &&
            m_strays_told.insert(h.owner + '\n' + event + '\n' + result.stray).second) {
            std::fprintf(stderr, "mod '%s': handler for '%s' set '%s', which the event does not have%s\n",
                         h.owner.c_str(), event.c_str(), result.stray,
                         schema.cancellable ? " (to cancel, set 'cancel')" : "");
        }

        // Anything the handler wrote into a string field points into its VM and
        // dies at the next call into it, so it is copied before the next one.
        for (int i = 0; i < count; ++i) {
            if (fields[i].type != SCRIPT_V_STRING) continue;
            m_string_scratch[(size_t)i] = fields[i].str ? fields[i].str : "";
            fields[i].str = m_string_scratch[(size_t)i].c_str();
        }

        if (schema.cancellable && result.cancelled) {
            if (cancelled) *cancelled = true;
            return true;
        }
    }

    return true;
}

void EventBus::drop_vm(script_vm *vm)
{
    for (auto &entry : m_handlers) {
        std::vector<Handler> &list = entry.second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [vm](const Handler &h) { return h.vm == vm; }),
                   list.end());
    }
}

size_t EventBus::handler_count() const
{
    size_t n = 0;
    for (const auto &entry : m_handlers) n += entry.second.size();
    return n;
}

const EventSchema *EventBus::schema(const std::string &name) const
{
    auto it = m_schemas.find(name);
    return (it == m_schemas.end()) ? nullptr : &it->second;
}

} // namespace cnp
