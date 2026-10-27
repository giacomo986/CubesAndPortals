#pragma once

#include "core/ui/panel.h"

#include <string>
#include <vector>

// A page: a few lines of markup laid out as a panel (src/core/ui/panel.h).
//
// The in-game web page. A mod or the engine writes text, links and tables the
// way it would write a README, and this turns it into the labels, buttons and
// fields a panel already has -- so a page costs the wire and the client nothing
// new: it is a panel like any other, and a click on it comes back as one.
//
// Laid out on the SERVER, which has no font: text is measured with the widths
// of the client's own glyphs (page_text_w), so it wraps where it is drawn.
//
// The markup, one construct per line:
//   # Heading                a bar across the page
//   ## Subheading            a line under it
//   text, [a link](target)   a paragraph, wrapped; links are buttons in the flow
//   | a | b | [x](target) |  a table row, its cells side by side; |---| is skipped
//   ?[hint](target)          a field: Enter runs `target` with {text} as what was typed
//   > text                   a quieter paragraph, on a darker band
//   ---                      a rule
//   (an empty line)          a gap
//
// A target says what a click does, and the page's host decides what each kind
// means: page:NAME opens another page, cmd:/line runs a chat command as the
// player who clicked, act:ID tells the mod that showed the page.

namespace cnp::ui {

// What a page is when it is on the screen: as wide as a 4:3 screen allows, and
// as tall as any (a cell is 7.5% of the screen's height).
inline constexpr float kPageWidth  = 17.0f;
inline constexpr float kPageHeight = 12.0f;

struct PageLink {
    std::string id;       // the element's id, what core:ui_click carries
    std::string target;   // page:..., cmd:..., act:...
    bool        field = false;   // answered on Enter with the typed text
};

// The ids of the page's own buttons, which the host answers itself.
inline constexpr const char *kPagePrev  = "pg:prev";
inline constexpr const char *kPageNext  = "pg:next";
inline constexpr const char *kPageClose = "pg:close";

struct PageLayout {
    Panel                 panel;
    std::vector<PageLink> links;
    int                   screens = 1;   // how many screens the markup fills
    int                   screen  = 0;   // which one `panel` is, clamped
};

// Lays out screen `screen` of `markup` under `title` into a panel called
// `name` owned by `owner`. Never fails: what does not fit in kMaxElements is
// cut, and the last line says so.
PageLayout layout_page(const std::string &name, const std::string &owner,
                       const std::string &title, const std::string &markup, int screen);

// How wide `text` is drawn on a panel, in cells: raylib's default font, the
// client's (gfx_2d_text), at a panel's 0.34 of a cell.
float page_text_w(const std::string &text);

// Text as a page shows it: the characters page markup reads as syntax
// ([ ] ( ) |) put as lookalikes, so a player's name cannot make a link.
std::string page_text(const std::string &text);

// The link an element id names, or nullptr.
const PageLink *page_link(const PageLayout &page, const std::string &id);

} // namespace cnp::ui
