#pragma once

// A line of text being typed, with a caret in it: the chat's line, a mod's
// panel field, the title screen's fields, the chest's search box.
//
// Pure, and in core/ rather than in the client, for the reason the panel is:
// every one of those boxes edits the same way, and the moment two of them
// carry their own copy of "back over a whole character" is the moment one of
// them cuts a letter in half. The client feeds it keys; it never sees a key.
//
// The text is UTF-8 and the caret a byte offset that always sits between two
// whole characters. Only what the HUD's font draws gets typed in (Latin-1), so
// a character is one or two bytes -- but the stepping does not assume that.

#include <cstddef>
#include <string>
#include <vector>

namespace cnp::ui {

struct TextEdit {
    std::string text;
    size_t      caret = 0;   // bytes before it; text.size() is the end

    // The whole line replaced (history, a field taken up), the caret after it.
    void set(std::string t);

    // A character typed (`ch` its UTF-8), at the caret, which moves past it.
    // False when it would take the line over `cap` bytes.
    bool insert(const std::string &ch, size_t cap);

    void backspace();      // the character before the caret
    void erase_forward();  // the one after it (Delete)
    void left();
    void right();
    void home() { caret = 0; }
    void end() { caret = text.size(); }
};

// What one press of Tab leaves behind, so the next one goes on to the next
// match instead of starting again from what the first one wrote.
struct TabCycle {
    bool                     on = false;
    std::vector<std::string> matches;
    size_t                   next  = 0;
    size_t                   start = 0;   // where the word being completed begins
    std::string              after;       // the line as the last Tab left it
    size_t                   caret = 0;   // and its caret

    void reset() { on = false; }
};

// Tab in a line: the word under the caret completed from `commands` when it is
// the line's first word and starts with '/' ("help", not "/help"), or from
// `names` otherwise. Case does not matter for the match; the completion is
// written as the list spells it. Pressed again with nothing typed between, it
// moves on to the next match, round to the first. False when nothing matched.
bool tab_complete(TextEdit &e, TabCycle &tab, const std::vector<std::string> &commands,
                  const std::vector<std::string> &names);

} // namespace cnp::ui
