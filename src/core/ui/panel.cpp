#include "core/ui/panel.h"

namespace cnp::ui {

namespace {

// A cell, as a fraction of the screen height. The same number a hotbar slot
// uses, so a mod's panel and the engine's own bag are drawn to one scale --
// two scales is a panel that looks like it belongs to a different program.
constexpr float kCellOfHeight = 0.075f;

bool bad_span(float v)
{
    return !(v == v) || v < 0.0f || v > kMaxPanelCells;   // NaN is not a size
}

} // namespace

void PanelPlacement::rect_of(const Element &e, float &out_x, float &out_y, float &out_w,
                             float &out_h) const
{
    out_x = x + e.x * cell;
    out_y = y + e.y * cell;
    out_w = e.w * cell;
    out_h = e.h * cell;
}

int PanelPlacement::button_at(const Panel &p, float px, float py) const
{
    // Backwards, so the element declared LAST wins where two overlap. That is
    // the order they are drawn in, and a hit test that disagreed with the
    // drawing would hand a click to the button underneath the one the player
    // can see.
    for (size_t i = p.elements.size(); i-- > 0;) {
        const Element &e = p.elements[i];
        if (e.kind != ElemKind::Button && e.kind != ElemKind::Field && e.kind != ElemKind::Item) continue;

        float ex, ey, ew, eh;
        rect_of(e, ex, ey, ew, eh);
        if (px >= ex && px < ex + ew && py >= ey && py < ey + eh) return (int)i;
    }
    return -1;
}

PanelPlacement place_panel(const Panel &p, int screen_w, int screen_h)
{
    PanelPlacement out;
    if (screen_w <= 0 || screen_h <= 0) return out;

    out.cell = (float)screen_h * kCellOfHeight;
    out.x    = ((float)screen_w - p.w * out.cell) * 0.5f;
    out.y    = ((float)screen_h - p.h * out.cell) * 0.5f;
    return out;
}

Panel *PanelSet::find_mutable(const std::string &name)
{
    for (Panel &p : m_panels)
        if (p.name == name) return &p;
    return nullptr;
}

const Panel *PanelSet::find(const std::string &name) const
{
    for (const Panel &p : m_panels)
        if (p.name == name) return &p;
    return nullptr;
}

bool PanelSet::define(const std::string &name, const std::string &owner, float w, float h,
                      std::string &err)
{
    if (name.empty()) {
        err = "a panel needs a name";
        return false;
    }
    if (bad_span(w) || bad_span(h) || w <= 0.0f || h <= 0.0f) {
        err = "a panel is between 0 and 30 cells on a side, and '" + name + "' is not";
        return false;
    }

    if (Panel *have = find_mutable(name)) {
        if (have->owner != owner) {
            err = "'" + name + "' belongs to mod '" + have->owner + "'";
            return false;
        }

        // Same mod, same name: this is a redefinition, and it starts empty.
        // Adding to a panel that already had elements is how a reload ends up
        // with two of every button. Who is looking at it is NOT touched: a mod
        // reloading is not a player closing it, and that was true when a panel
        // had one `open` flag and is truer now that it has one per viewer.
        have->elements.clear();
        have->w = w;
        have->h = h;

        // Declared again, so a reload that is under way no longer counts it as
        // dropped. Same rule as re-registering a block.
        m_stale[(size_t)(have - m_panels.data())] = 0;

        // Only the people who can SEE it. A mod that rebuilds a panel nobody
        // has up has changed nothing anybody is looking at, and a bump there
        // costs every player in the world a message per call -- exactly what
        // the first run of the wire gate measured.
        touched(name);
        return true;
    }

    if (m_panels.size() >= kMaxPanels) {
        err = "too many panels";
        return false;
    }

    Panel p;
    p.name  = name;
    p.owner = owner;
    p.w     = w;
    p.h     = h;
    m_panels.push_back(p);
    m_stale.push_back(0);
    // No bump: a panel starts on nobody's screen, so nothing anybody is looking
    // at moved.
    return true;
}

bool PanelSet::add(const std::string &panel, const std::string &owner, const Element &e,
                   std::string &err)
{
    Panel *p = find_mutable(panel);
    if (p == nullptr) {
        err = "no panel called '" + panel + "'";
        return false;
    }
    if (p->owner != owner) {
        err = "'" + panel + "' belongs to mod '" + p->owner + "'";
        return false;
    }
    if (p->elements.size() >= kMaxElements) {
        err = "'" + panel + "' already holds as much as a panel may";
        return false;
    }
    if (bad_span(e.w) || bad_span(e.h) || bad_span(e.x) || bad_span(e.y)) {
        err = "an element of '" + panel + "' is outside what a panel can hold";
        return false;
    }
    if (e.text.size() > kMaxPanelText || e.id.size() > kMaxPanelText) {
        err = "an element of '" + panel + "' carries more text than a panel may";
        return false;
    }

    // A button with no id answers a click with nothing, which reaches a mod as
    // an event it cannot tell from any other button's. Refused rather than
    // given a number, because a generated id is one a mod cannot write down.
    if ((e.kind == ElemKind::Button || e.kind == ElemKind::Field || e.kind == ElemKind::Item) && e.id.empty()) {
        err = "a button on '" + panel + "' needs an id, or a click means nothing";
        return false;
    }

    p->elements.push_back(e);
    touched(panel);
    return true;
}

bool PanelSet::show(const std::string &name, const std::string &owner, bool open,
                    PlayerId viewer, std::string &err)
{
    Panel *p = find_mutable(name);
    if (p == nullptr) {
        err = "no panel called '" + name + "'";
        return false;
    }
    if (p->owner != owner) {
        err = "'" + name + "' belongs to mod '" + p->owner + "'";
        return false;
    }

    if (!open) {
        // Before and after, because what matters is what THAT viewer would now
        // be looking at: closing the panel underneath the one on their screen
        // changes nothing they can see.
        const Panel *was = topmost_for(viewer);
        if (forget(name, viewer) == 0) return true;   // it was not up for them
        if (topmost_for(viewer) != was) bump(viewer);
        return true;
    }

    for (Shown &sh : m_shown) {
        if (sh.panel != name || sh.viewer != viewer) continue;

        // Already up for them. Re-stamping only matters if it moves the panel
        // to the top; a mod that calls show() every tick on the one already
        // there must not be a message every tick.
        const Panel *was = topmost_for(viewer);
        sh.at            = ++m_clock;
        if (topmost_for(viewer) != was) bump(viewer);
        return true;
    }

    if (m_shown.size() >= kMaxShown) {
        err = "too many panels are on screen at once";
        return false;
    }

    const Panel *was = topmost_for(viewer);
    m_shown.push_back(Shown{name, viewer, ++m_clock});
    if (topmost_for(viewer) != was) bump(viewer);
    return true;
}

const Panel *PanelSet::topmost_for(PlayerId viewer) const
{
    const Shown *best = nullptr;
    for (const Shown &sh : m_shown) {
        // Theirs, or everybody's. The two compete on one clock, so a chest
        // opened for one player covers the notice that was up for the world.
        if (sh.viewer != viewer && sh.viewer != kAllPlayers) continue;
        if (best == nullptr || sh.at > best->at) best = &sh;
    }
    return best ? find(best->panel) : nullptr;
}

uint64_t PanelSet::revision_for(PlayerId viewer) const
{
    const auto all = m_revision.find(kAllPlayers);
    const uint64_t world = all == m_revision.end() ? 0 : all->second;
    if (viewer == kAllPlayers) return world;

    const auto mine = m_revision.find(viewer);
    return world + (mine == m_revision.end() ? 0 : mine->second);
}

void PanelSet::bump(PlayerId viewer)
{
    ++m_revision[viewer];
}

void PanelSet::touched(const std::string &panel)
{
    // Everybody who has it up, and nobody else. A panel is a declaration shared
    // by every viewer, so rewriting a label is one change and any number of
    // messages -- which is the honest count, not a shortcut.
    for (const Shown &sh : m_shown)
        if (sh.panel == panel) bump(sh.viewer);
}

size_t PanelSet::forget(const std::string &panel, PlayerId viewer)
{
    const size_t before = m_shown.size();
    for (size_t i = m_shown.size(); i-- > 0;) {
        if (m_shown[i].panel != panel || m_shown[i].viewer != viewer) continue;
        m_shown.erase(m_shown.begin() + (long)i);
    }
    return before - m_shown.size();
}

size_t PanelSet::drop_player(PlayerId viewer)
{
    const size_t before = m_shown.size();
    for (size_t i = m_shown.size(); i-- > 0;) {
        if (m_shown[i].viewer != viewer) continue;
        m_shown.erase(m_shown.begin() + (long)i);
    }

    // No bump. There is nobody left to tell: this is a session that has gone,
    // and the next one to hold that id is a different player who has been told
    // nothing and whose panel_rev starts at zero.
    m_revision.erase(viewer);
    return before - m_shown.size();
}

void PanelSet::begin_reload(const std::string &owner)
{
    if (m_stale.size() < m_panels.size()) m_stale.resize(m_panels.size(), 0);
    for (size_t i = 0; i < m_panels.size(); ++i)
        if (m_panels[i].owner == owner) m_stale[i] = 1;
}

void PanelSet::cancel_reload(const std::string &owner)
{
    for (size_t i = 0; i < m_panels.size() && i < m_stale.size(); ++i)
        if (m_panels[i].owner == owner) m_stale[i] = 0;
}

size_t PanelSet::end_reload(const std::string &owner)
{
    size_t dropped = 0;
    for (size_t i = m_panels.size(); i-- > 0;) {
        if (i >= m_stale.size() || !m_stale[i] || m_panels[i].owner != owner) continue;

        // Everybody looking at it is told first, and then it stops existing.
        // The other order tells nobody: touched() walks the rows this erase is
        // about to remove.
        touched(m_panels[i].name);
        for (size_t k = m_shown.size(); k-- > 0;)
            if (m_shown[k].panel == m_panels[i].name) m_shown.erase(m_shown.begin() + (long)k);

        m_panels.erase(m_panels.begin() + (long)i);
        m_stale.erase(m_stale.begin() + (long)i);
        ++dropped;
    }
    return dropped;
}

size_t PanelSet::drop_owner(const std::string &owner, const std::string &only)
{
    size_t dropped = 0;
    for (size_t i = m_panels.size(); i-- > 0;) {
        if (m_panels[i].owner != owner || (!only.empty() && m_panels[i].name != only)) continue;

        touched(m_panels[i].name);
        for (size_t k = m_shown.size(); k-- > 0;)
            if (m_shown[k].panel == m_panels[i].name) m_shown.erase(m_shown.begin() + (long)k);

        m_panels.erase(m_panels.begin() + (long)i);
        if (i < m_stale.size()) m_stale.erase(m_stale.begin() + (long)i);
        ++dropped;
    }
    return dropped;
}

} // namespace cnp::ui
