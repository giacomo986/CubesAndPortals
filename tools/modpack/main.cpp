// Makes a mod into one file, and reads one back.
//
// DESIGN.md M10, "formato pacchetto mod". The format is modpack/mod_package.h
// and knows nothing about a VM; this is the thirty lines of program around it,
// plus the one thing the format deliberately does not do -- reading the mod.conf
// inside to name the file, which is why this links the manifest parser the
// engine uses rather than looking for `name =` itself.
//
//   cnp_modpack pack   <mod dir> [-o <file>]   one file out of a directory
//   cnp_modpack list   <file>                  what is in it, and whether it is whole
//   cnp_modpack unpack <file> <dir>            the directory back

#include "modpack/mod_package.h"
#include "script/mod_host.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace cnp;

namespace {

namespace fs = std::filesystem;

int usage()
{
    std::fprintf(stderr,
                 "usage:\n"
                 "  cnp_modpack pack   <mod dir> [-o <file>]\n"
                 "  cnp_modpack list   <file>\n"
                 "  cnp_modpack unpack <file> <dir>\n");
    return 2;
}

// The name a package gets when nobody says: what the mod calls itself and what
// version it is, which are the two things somebody looking at a folder of
// packages needs to tell them apart.
bool default_name(const ModPackage &pkg, std::string &out, std::string &error)
{
    const ModPackageFile *conf = pkg.find("mod.conf");
    if (!conf) {
        error = "no mod.conf in the package";
        return false;
    }

    ModManifest m;
    const std::string text((const char *)conf->bytes.data(), conf->bytes.size());
    if (!mod_manifest_parse(text, m, error)) {
        error = "mod.conf: " + error;
        return false;
    }

    out = m.name + "-" + m.version.str() + kModPackageExtension;
    return true;
}

int do_pack(int argc, char **argv)
{
    if (argc < 3) return usage();

    const std::string dir = argv[2];
    std::string       out;

    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) out = argv[++i];
        else return usage();
    }

    ModPackage  pkg;
    std::string err;
    if (!mod_package_read_dir(dir, pkg, err)) {
        std::fprintf(stderr, "cnp_modpack: %s\n", err.c_str());
        return 1;
    }

    if (out.empty()) {
        std::string name;
        if (!default_name(pkg, name, err)) {
            std::fprintf(stderr, "cnp_modpack: %s\n", err.c_str());
            return 1;
        }
        out = name;
    }

    if (!mod_package_write_file(pkg, out, err)) {
        std::fprintf(stderr, "cnp_modpack: %s\n", err.c_str());
        return 1;
    }

    size_t plain = 0;
    for (const ModPackageFile &f : pkg.files) plain += f.bytes.size();

    std::error_code ec;
    const uintmax_t stored = fs::file_size(out, ec);

    std::printf("%s: %zu file(s), %zu bytes in, %ju bytes out\n", out.c_str(), pkg.files.size(),
                plain, (uintmax_t)stored);
    return 0;
}

int do_list(int argc, char **argv)
{
    if (argc < 3) return usage();

    ModPackage  pkg;
    std::string err;
    if (!mod_package_read_file(argv[2], pkg, err)) {
        std::fprintf(stderr, "cnp_modpack: %s\n", err.c_str());
        return 1;
    }

    std::printf("%s\n  contents hash to %s\n", argv[2], pkg.hash.c_str());
    for (const ModPackageFile &f : pkg.files) {
        std::printf("  %10zu  %s\n", f.bytes.size(), f.path.c_str());
    }

    // What the package says it is. Read from the mod.conf inside it and nowhere
    // else -- there is no name on the outside of a package to disagree with.
    std::string name;
    if (default_name(pkg, name, err))
        std::printf("  it calls itself %s\n", name.c_str());
    else
        std::printf("  and its manifest does not parse: %s\n", err.c_str());

    return 0;
}

int do_unpack(int argc, char **argv)
{
    if (argc < 4) return usage();

    ModPackage  pkg;
    std::string err;
    if (!mod_package_read_file(argv[2], pkg, err)) {
        std::fprintf(stderr, "cnp_modpack: %s\n", err.c_str());
        return 1;
    }
    if (!mod_package_unpack(pkg, argv[3], err)) {
        std::fprintf(stderr, "cnp_modpack: %s\n", err.c_str());
        return 1;
    }

    std::printf("%s: %zu file(s) into %s\n", argv[2], pkg.files.size(), argv[3]);
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) return usage();

    const std::string verb = argv[1];
    if (verb == "pack")   return do_pack(argc, argv);
    if (verb == "list")   return do_list(argc, argv);
    if (verb == "unpack") return do_unpack(argc, argv);
    return usage();
}
