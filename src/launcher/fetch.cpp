#include "launcher/fetch.h"

#include "core/paths.h"
#include "core/release.h"
#include "core/update_check.h"
#include "core/version.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

namespace cnp::launcher {

namespace fs = std::filesystem;

std::string g_download_base = std::string(kReleasesPage) + "/download";
std::string g_releases_api  = "https://api.github.com/repos/giacomo986/CubesAndPortals/releases?per_page=30";

namespace {

#ifdef _WIN32
constexpr const char *kNull = "NUL";
#else
constexpr const char *kNull = "/dev/null";
#endif

// A version that goes into a URL, a path and a shell line: digits, letters,
// dots and dashes only. What the releases page says is not trusted further.
bool plain_version(const std::string &v)
{
    return !v.empty() && v.size() < 40 && v[0] != '.' && v[0] != '-' &&
           std::all_of(v.begin(), v.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '.' || c == '-';
           });
}

std::string in_quotes(const std::string &s) { return "\"" + s + "\""; }

bool run(const std::string &cmd) { return std::system(cmd.c_str()) == 0; }

bool fetch_to(const std::string &url, const std::string &path)
{
    return run("curl -fsSL --max-time 900 -o " + in_quotes(path) + " " + in_quotes(url) + " 2>" + kNull);
}

std::string versions_dir() { return user_data_dir() + "/versions"; }

} // namespace

std::vector<std::string> remote_versions()
{
    const std::string cmd = "curl -fsSL --max-time 8 -H \"Accept: application/vnd.github+json\" " +
                            in_quotes(g_releases_api) + " 2>" + kNull;
    std::vector<std::string> out;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) return out;
    std::string json;
    char        buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), pipe)) > 0;) json.append(buf, n);
    pclose(pipe);

    for (size_t at = json.find("\"tag_name\""); at != std::string::npos;
         at = json.find("\"tag_name\"", at + 1)) {
        const size_t open  = json.find('"', json.find(':', at));
        const size_t close = open == std::string::npos ? open : json.find('"', open + 1);
        if (close == std::string::npos) break;
        std::string v = json.substr(open + 1, close - open - 1);
        if (!v.empty() && v[0] == 'v') v.erase(0, 1);
        if (plain_version(v) && std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
    }
    std::sort(out.begin(), out.end(),
              [](const std::string &a, const std::string &b) { return update::compare(a, b) > 0; });
    return out;
}

std::string package_name(const std::string &version)
{
#if defined(_WIN32)
    return "cubes-and-portals-" + version + "-windows-x86_64.zip";
#elif defined(__APPLE__)
    (void)version;
    return "";   // ponytail: a .dmg wants hdiutil and an .app; add when macOS is played
#elif defined(__aarch64__)
    return "cubes-and-portals-" + version + "-linux-aarch64.tar.gz";
#else
    return "cubes-and-portals-" + version + "-linux-x86_64.tar.gz";
#endif
}

std::string install(const std::string &version, Progress &progress)
{
    if (!plain_version(version)) return "not a version: " + version;
    const std::string package = package_name(version);
    if (package.empty()) return "this system's packages are not installed by the launcher yet";

    std::error_code ec;
    const fs::path  stage = fs::path(versions_dir()) / (".fetch-" + version);
    const fs::path  final = fs::path(versions_dir()) / version;
    if (fs::exists(final, ec)) return version + " is already here";
    fs::remove_all(stage, ec);
    fs::create_directories(stage, ec);
    if (ec) return "cannot make " + stage.string();
    struct Clean {
        fs::path p;
        ~Clean() { std::error_code e; fs::remove_all(p, e); }
    } clean{stage};

    // The manifest first, and the package only if the manifest is the release
    // key's and names it: nothing unsigned is downloaded past this point.
    const std::string base = g_download_base + "/v" + version + "/";
    const fs::path    manifest_path = stage / release::kManifestName;
    if (!fetch_to(base + release::kManifestName, manifest_path.string()))
        return version + " has no signed manifest";   // a release from before the launcher
    std::ifstream     in(manifest_path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    uint8_t key[net::kSignPublicBytes];
    release::Manifest m;
    std::string       err;
    if (!release::release_key(key) || !release::read(text, key, m, err)) return "refused: " + err;
    if (m.version != version) return "refused: the manifest is for " + m.version;
    const auto file = std::find_if(m.files.begin(), m.files.end(),
                                   [&](const release::File &f) { return f.name == package; });
    if (file == m.files.end()) return "refused: the release has no " + package;

    const fs::path archive = download_path(version);
    progress.size = file->size;
    if (!fetch_to(base + package, archive.string())) return "the download of " + package + " failed";
    if (!release::check_file(*file, archive.string(), err)) return "refused: " + err;

    // Unpacked into the stage, then moved whole: a half-unpacked build is never
    // in versions/. tar refuses members that climb out with "..", and the
    // archive is the one the release key signed.
    const fs::path out = stage / "out";
    fs::create_directories(out, ec);
    if (!run("tar -xf " + in_quotes(archive.string()) + " -C " + in_quotes(out.string()) + " 2>" + kNull))
        return "could not unpack " + package;
    fs::path top = out / package.substr(0, package.find(".tar.gz") != std::string::npos
                                               ? package.size() - 7
                                               : package.size() - 4);
    if (!fs::is_directory(top, ec)) return package + " does not hold the folder it should";
    fs::rename(top, final, ec);
    if (ec) return "cannot move it into " + final.string();
    return "";
}

std::string download_path(const std::string &version)
{
    return (fs::path(versions_dir()) / (".fetch-" + version) / package_name(version)).string();
}

std::vector<std::string> worlds_made_with(const std::string &version)
{
    std::vector<std::string> out;
    std::error_code          ec;
    for (const fs::directory_entry &e : fs::directory_iterator(user_data_dir() + "/worlds", ec)) {
        std::ifstream in(e.path() / "version");
        std::string   v;
        if (std::getline(in, v) && v == version) out.push_back(e.path().filename().string());
    }
    return out;
}

std::string remove(const std::string &version)
{
    if (!plain_version(version)) return "not a version: " + version;
    std::error_code ec;
    fs::remove_all(fs::path(versions_dir()) / version, ec);
    return ec ? ec.message() : "";
}

} // namespace cnp::launcher
