#pragma once

// What the game's title screen and the launcher (src/launcher) both draw: the
// sky with its two openings behind, the name over it, and the helpers that lay
// a menu panel out a row at a time.

#include "core/ui/panel.h"

#include <string>

namespace cnp {

// One row element: a label sits a third of a cell lower, to line up with a
// button's text beside it.
void panel_add(ui::Panel &p, ui::ElemKind kind, std::string id, std::string text, float x,
               float y, float w);
void panel_button(ui::Panel &p, const char *id, std::string text, float x, float y, float w);
void panel_label(ui::Panel &p, std::string text, float x, float y);

// Inside gfx_2d_begin/end.
void draw_title_backdrop();
void draw_title_name();

} // namespace cnp
