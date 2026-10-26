#pragma once

// Translations, by the text itself: a language is a file of lines
//
//     text as written = the same in this language
//
// in assets/lang/<code>.txt, and every string the client puts on the screen --
// its own menus, a mod's panels, notices, chat and hud text -- is looked up
// there as it is drawn. A string the file does not have is shown as it is, so
// a half-done translation is half translated. The engine writes English; a mod
// written in another language is put into English by en.txt.
//
// {} stands for any text, on both sides, in order: "Starting {} ..." matches
// "Starting Moon ..." and puts "Moon" where the translation has its {}. Lines
// starting with # are comments; "@name = Italiano" is the language's own name.

#include <string>
#include <utility>
#include <vector>

namespace cnp::lang {

// Reads <dir>/<code>.txt in place of the language before. "en", or a file that
// is not there, is no translation at all. False only for a file that was there
// and could not be read.
bool load(const std::string &dir, const std::string &code);

// The language load() last chose, "en" when none.
const std::string &current();

// Every language there is, as code and own name: "en" first, then one per file.
std::vector<std::pair<std::string, std::string>> available(const std::string &dir);

// The code for this machine's language (LANG, LC_ALL...): "it" for it_IT.UTF-8,
// "en" when nothing says.
std::string from_environment();

// `text` in the current language. The pointer is `text` itself when there is
// no translation, else one that stays good until the next call.
const char *tr(const char *text);
inline const char *tr(const std::string &text) { return tr(text.c_str()); }

}   // namespace cnp::lang
