#include "update_check.h"

#include "core/version.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

namespace cnp::update {

namespace {

std::mutex  g_mutex;
std::string g_newer;
bool        g_started = false;

// "v0.9.0-alpha.2" -> {0, 9, 0, 2}; a release without "-alpha" gets a last
// number above any alpha's, so it sorts after them.
std::vector<long> parts(const std::string &v)
{
    std::vector<long> out;
    size_t            i     = v.empty() || v[0] != 'v' ? 0 : 1;
    const size_t      dash  = v.find('-', i);
    const std::string base  = v.substr(i, dash == std::string::npos ? std::string::npos : dash - i);
    for (size_t at = 0; at <= base.size();) {
        const size_t dot = base.find('.', at);
        out.push_back(std::strtol(base.c_str() + at, nullptr, 10));
        if (dot == std::string::npos) break;
        at = dot + 1;
    }
    out.resize(3, 0);
    const size_t n = dash == std::string::npos ? std::string::npos : v.rfind('.');
    out.push_back(n == std::string::npos || n < dash ? 1L << 30 : std::strtol(v.c_str() + n + 1, nullptr, 10));
    return out;
}

// The first "tag_name" in GitHub's answer: the list comes newest first, and it
// holds pre-releases too, which every alpha is (releases/latest skips them).
std::string first_tag(const std::string &json)
{
    const size_t key = json.find("\"tag_name\"");
    if (key == std::string::npos) return "";
    const size_t open = json.find('"', json.find(':', key));
    const size_t close = open == std::string::npos ? open : json.find('"', open + 1);
    return close == std::string::npos ? "" : json.substr(open + 1, close - open - 1);
}

void ask()
{
    // ponytail: curl through the shell, which Linux, macOS and Windows 10+
    // all carry; on Windows it may flash a console. WinHTTP if that matters.
    const std::string cmd = std::string("curl -fsSL --max-time 8 -H \"Accept: application/vnd.github+json\" \"") +
                            kReleasesApi + "\" 2>" +
#ifdef _WIN32
                            "NUL";
#else
                            "/dev/null";
#endif
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) return;
    std::string json;
    char        buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), pipe)) > 0;) json.append(buf, n);
    pclose(pipe);

    const std::string tag = first_tag(json);
    if (tag.empty() || compare(tag, kEngineVersion) <= 0) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_newer = tag;
}

} // namespace

int compare(const std::string &a, const std::string &b)
{
    const std::vector<long> x = parts(a), y = parts(b);
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

void start()
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_started) return;
        g_started = true;
    }
    std::thread(ask).detach();
}

std::string newer()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_newer;
}

} // namespace cnp::update
