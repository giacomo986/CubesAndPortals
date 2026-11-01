#include "core/paths.h"

#include <cstdlib>
#include <sstream>
#include <fstream>
#include <cctype>
#include <algorithm>

#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace cnp {
namespace {

namespace fs = std::filesystem;

bool exists_dir(const fs::path &p)
{
    std::error_code ec;
    return fs::is_directory(p, ec);
}

} // namespace

std::string exe_path()
{
    std::error_code ec;
#if defined(__linux__)
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return self.string();
#elif defined(_WIN32)
    wchar_t   buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, sizeof(buf) / sizeof(buf[0]));
    if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) return fs::path(buf).string();
#elif defined(__APPLE__)
    char     buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        const fs::path self = fs::canonical(buf, ec);
        if (!ec) return self.string();
    }
#endif
    // Elsewhere: no answer rather than a guess. argv[0] is not one -- it is
    // whatever the caller felt like passing, and a program that resolved its
    // own data out of it would follow a PATH lookup into a directory nobody
    // meant.
    return {};
}

std::string exe_dir()
{
    const std::string self = exe_path();
    return self.empty() ? self : fs::path(self).parent_path().string();
}

std::string data_dir(const std::string &name, const std::string &built_at)
{
    const std::string here = exe_dir();
    if (!here.empty()) {
        // A release is binaries and data side by side: cnp_server and mods/ in
        // one directory. This is the case the whole file exists for.
        const fs::path beside = fs::path(here) / name;
        if (exists_dir(beside)) return beside.string();

#if defined(__APPLE__)
        // A macOS app keeps its data in Contents/Resources, beside Contents/MacOS.
        const fs::path bundled = fs::path(here) / ".." / "Resources" / name;
        if (exists_dir(bundled)) return bundled.lexically_normal().string();
#endif

        // And a build tree is binaries a few directories down from the files --
        // build/src/server/cnp_server against mods/ at the top. Two levels,
        // then three, which covers this project's layout without walking up to
        // the root of the filesystem looking for a directory called "mods".
        fs::path up = fs::path(here);
        for (int i = 0; i < 3; ++i) {
            up = up.parent_path();
            if (up.empty()) break;
            const fs::path candidate = up / name;
            if (exists_dir(candidate)) return candidate.string();
        }
    }

    // The source tree this was built from. Right on the machine that built it,
    // wrong everywhere else, and last for that reason.
    return built_at;
}

std::string data_dir_reason(const std::string &chosen, const std::string &built_at)
{
    if (chosen == built_at) return "the tree it was built from";

    const std::string here = exe_dir();
    if (!here.empty() && chosen.rfind(here, 0) == 0) return "next to the program";
    return "found";
}

std::string user_data_dir()
{
#ifdef _WIN32
    if (const char *app = std::getenv("APPDATA"); app != nullptr && app[0] != '\0')
        return std::string(app) + "/cubes-and-portals";
#endif
    if (const char *xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && xdg[0] != '\0')
        return std::string(xdg) + "/cubes-and-portals";
    if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
        return std::string(home) + "/.local/share/cubes-and-portals";
    return "";
}

std::string options_path()
{
#ifdef _WIN32
    if (const char *app = std::getenv("APPDATA"); app != nullptr && app[0] != '\0')
        return std::string(app) + "/cubes-and-portals/options";
#endif
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && xdg[0] != '\0')
        return std::string(xdg) + "/cubes-and-portals/options";
    if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
        return std::string(home) + "/.config/cubes-and-portals/options";
    return "";
}

std::string user_mods_dir()
{
#ifdef _WIN32
    return user_data_dir() + "/mods";
#else
    if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
        return std::string(home) + "/.cubes-and-portals/mods";
    return user_data_dir() + "/mods";
#endif
}

std::string user_mods_conf()
{
    return std::filesystem::path(user_mods_dir()).parent_path().string() + "/mods.conf";
}

std::vector<std::string> read_mods_off(const std::string &path)
{
    std::vector<std::string> out;
    std::ifstream            in(path);
    const auto strip = [](std::string &t) {
        std::erase_if(t, [](unsigned char c) { return std::isspace(c); });
    };
    for (std::string line; std::getline(in, line);) {
        line = line.substr(0, line.find('#'));
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        strip(key);
        if (key != "off") continue;
        std::stringstream names(line.substr(eq + 1));
        for (std::string n; std::getline(names, n, ',');) {
            strip(n);
            if (!n.empty() && std::find(out.begin(), out.end(), n) == out.end()) out.push_back(n);
        }
    }
    return out;
}

bool write_mods_off(const std::string &path, const std::vector<std::string> &names)
{
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path);
    out << "# Mods turned off here (the launcher writes this line).\noff = ";
    for (size_t i = 0; i < names.size(); ++i) out << (i ? ", " : "") << names[i];
    out << "\n";
    return (bool)out;
}

} // namespace cnp
