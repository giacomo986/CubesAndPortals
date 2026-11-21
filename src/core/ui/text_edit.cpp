#include "core/ui/text_edit.h"

#include <algorithm>
#include <cctype>

namespace cnp::ui {

namespace {

bool continuation(char c) { return ((unsigned char)c & 0xC0) == 0x80; }

char lower(char c) { return (char)std::tolower((unsigned char)c); }

bool starts_with_nocase(const std::string &s, const std::string &stem)
{
    if (s.size() < stem.size()) return false;
    for (size_t i = 0; i < stem.size(); ++i)
        if (lower(s[i]) != lower(stem[i])) return false;
    return true;
}

} // namespace

void TextEdit::set(std::string t)
{
    text  = std::move(t);
    caret = text.size();
}

bool TextEdit::insert(const std::string &ch, size_t cap)
{
    if (caret > text.size()) caret = text.size();
    if (text.size() + ch.size() > cap) return false;
    text.insert(caret, ch);
    caret += ch.size();
    return true;
}

void TextEdit::left()
{
    if (caret > text.size()) caret = text.size();
    if (caret == 0) return;
    --caret;
    while (caret > 0 && continuation(text[caret])) --caret;
}

void TextEdit::right()
{
    if (caret >= text.size()) {
        caret = text.size();
        return;
    }
    ++caret;
    while (caret < text.size() && continuation(text[caret])) ++caret;
}

void TextEdit::backspace()
{
    const size_t to = std::min(caret, text.size());
    left();
    text.erase(caret, to - caret);
}

void TextEdit::erase_forward()
{
    const size_t from = std::min(caret, text.size());
    right();
    text.erase(from, caret - from);
    caret = from;
}

bool tab_complete(TextEdit &e, TabCycle &tab, const std::vector<std::string> &commands,
                  const std::vector<std::string> &names)
{
    if (e.caret > e.text.size()) e.caret = e.text.size();

    // Again, on what the last Tab wrote: the next match in its place.
    if (tab.on && e.text == tab.after && e.caret == tab.caret && !tab.matches.empty()) {
        const std::string &was  = tab.matches[(tab.next + tab.matches.size() - 1) % tab.matches.size()];
        const std::string &word = tab.matches[tab.next % tab.matches.size()];
        e.text.replace(tab.start, was.size(), word);
        e.caret   = tab.start + word.size();
        tab.next  = (tab.next + 1) % tab.matches.size();
        tab.after = e.text;
        tab.caret = e.caret;
        return true;
    }
    tab.reset();

    // The word the caret is in or at the end of: back to a space, on to one.
    size_t start = 0;
    if (e.caret > 0)
        if (const size_t space = e.text.rfind(' ', e.caret - 1); space != std::string::npos) start = space + 1;
    size_t stop = e.text.find(' ', e.caret);
    if (stop == std::string::npos) stop = e.text.size();

    // "/he" is a command's name when it is the first word; the slash stays.
    const bool command = start == 0 && !e.text.empty() && e.text[0] == '/';
    if (command) start = 1;
    const std::string stem = e.text.substr(start, e.caret - start);
    if (!command && stem.empty()) return false;   // a space is not a word to finish

    std::vector<std::string> matches;
    for (const std::string &w : command ? commands : names)
        if (!w.empty() && starts_with_nocase(w, stem) &&
            std::find(matches.begin(), matches.end(), w) == matches.end())
            matches.push_back(w);
    if (matches.empty()) return false;
    std::sort(matches.begin(), matches.end(), [](const std::string &a, const std::string &b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                            [](char x, char y) { return lower(x) < lower(y); });
    });

    e.text.replace(start, stop - start, matches[0]);
    e.caret     = start + matches[0].size();
    tab.on      = true;
    tab.matches = std::move(matches);
    tab.next    = 1 % tab.matches.size();
    tab.start   = start;
    tab.after   = e.text;
    tab.caret   = e.caret;
    return true;
}

} // namespace cnp::ui
