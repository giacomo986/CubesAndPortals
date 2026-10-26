#include "client/ui/lang.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace cnp::lang {

namespace {

struct Pattern {
    std::vector<std::string> from;   // the literal pieces between the {}s
    std::string              to;
};

std::string                                  g_code = "en";
std::unordered_map<std::string, std::string> g_exact;
std::vector<Pattern>                         g_patterns;
// What patterns made of strings already seen, misses too: hud text is drawn
// every frame, and matching every pattern against it every frame is waste.
std::unordered_map<std::string, std::string> g_made;

std::string trim(const std::string &s)
{
    const size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

std::vector<std::string> split_holes(const std::string &s)
{
    std::vector<std::string> out;
    size_t                   at = 0;
    for (size_t hole; (hole = s.find("{}", at)) != std::string::npos; at = hole + 2)
        out.push_back(s.substr(at, hole - at));
    out.push_back(s.substr(at));
    return out;
}

// The holes' text when `text` is `from` with something in each hole, else false.
// The first place each piece fits, so a hole takes as little as it can.
bool match(const std::vector<std::string> &from, const std::string &text,
           std::vector<std::string> &holes)
{
    const std::string &head = from.front(), &tail = from.back();
    if (text.size() < head.size() + tail.size() || text.compare(0, head.size(), head) != 0 ||
        text.compare(text.size() - tail.size(), tail.size(), tail) != 0)
        return false;

    const size_t end = text.size() - tail.size();
    size_t       at  = head.size();
    holes.clear();
    for (size_t i = 1; i + 1 < from.size(); ++i) {
        const size_t found = text.find(from[i], at);
        if (found == std::string::npos || found + from[i].size() > end) return false;
        holes.push_back(text.substr(at, found - at));
        at = found + from[i].size();
    }
    holes.push_back(text.substr(at, end - at));
    // An empty hole is no match: "{} found" is not a translation of " found".
    return std::none_of(holes.begin(), holes.end(), [](const std::string &h) { return h.empty(); });
}

std::string fill(const std::string &to, const std::vector<std::string> &holes)
{
    std::string out;
    size_t      at = 0, n = 0;
    for (size_t hole; (hole = to.find("{}", at)) != std::string::npos; at = hole + 2)
        out += to.substr(at, hole - at) + (n < holes.size() ? holes[n++] : "");
    return out + to.substr(at);
}

std::string own_name(const std::string &path)
{
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);) {
        const size_t eq = line.find(" = ");
        if (eq != std::string::npos && trim(line.substr(0, eq)) == "@name")
            return trim(line.substr(eq + 3));
    }
    return std::filesystem::path(path).stem().string();
}

}   // namespace

bool load(const std::string &dir, const std::string &code)
{
    g_code = "en";
    g_exact.clear();
    g_patterns.clear();
    g_made.clear();
    if (code.empty()) return true;
    // English too may have a file: for the text a mod wrote in another language.
    g_code = code;

    const std::string path = dir + "/" + code + ".txt";
    std::error_code   ec;
    if (!std::filesystem::exists(path, ec)) return true;
    std::ifstream in(path);
    if (!in) return false;

    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#' || line[0] == '@') continue;
        const size_t eq = line.find(" = ");
        if (eq == std::string::npos) continue;
        const std::string from = trim(line.substr(0, eq)), to = trim(line.substr(eq + 3));
        if (from.empty() || to.empty()) continue;
        if (from.find("{}") == std::string::npos) g_exact[from] = to;
        else g_patterns.push_back({split_holes(from), to});
    }
    return true;
}

const std::string &current() { return g_code; }

std::vector<std::pair<std::string, std::string>> available(const std::string &dir)
{
    std::vector<std::pair<std::string, std::string>> out{{"en", "English"}};
    std::error_code                                  ec;
    std::vector<std::filesystem::path>               files;
    for (const auto &e : std::filesystem::directory_iterator(dir, ec))
        if (e.path().extension() == ".txt") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (const auto &f : files)
        if (f.stem() != "en") out.emplace_back(f.stem().string(), own_name(f.string()));
    return out;
}

std::string from_environment()
{
    for (const char *var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char *v = std::getenv(var);
        if (v == nullptr || v[0] == '\0') continue;
        const std::string s(v);
        if (s == "C" || s == "POSIX") break;
        return s.substr(0, s.find_first_of("_.@"));
    }
    return "en";
}

const char *tr(const char *text)
{
    if (text == nullptr || text[0] == '\0' || (g_exact.empty() && g_patterns.empty())) return text;
    const std::string key(text);
    if (const auto it = g_exact.find(key); it != g_exact.end()) return it->second.c_str();
    if (g_patterns.empty()) return text;

    if (const auto it = g_made.find(key); it != g_made.end())
        return it->second.empty() ? text : it->second.c_str();
    // ponytail: forgets everything at a bound rather than the oldest; a timer
    // in hud text makes a new string a frame, and this is all it costs.
    if (g_made.size() > 4096) g_made.clear();

    std::vector<std::string> holes;
    for (const Pattern &p : g_patterns)
        if (match(p.from, key, holes)) {
            // A hole's text is translated too: "Light: {}" over "new".
            for (std::string &h : holes) h = tr(h.c_str());
            return (g_made[key] = fill(p.to, holes)).c_str();
        }
    g_made[key];
    return text;
}

}   // namespace cnp::lang
