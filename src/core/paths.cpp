#include "core/paths.h"

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

} // namespace cnp
