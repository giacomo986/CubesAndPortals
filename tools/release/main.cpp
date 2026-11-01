// The release manifest (core/release.h): the key, the signature, the check.
//
//   cnp_release key   <key file>          creates the key if absent, prints its public half
//   cnp_release sign  <key file> <dir>    signs every file in <dir> into <dir>/release.manifest
//   cnp_release check <dir>               the manifest against this build's release key,
//                                         and every file it lists against the one in <dir>
//
// The key file is the signing seed as hex, readable by its owner only
// (net::load_or_create_sign_key), kept in ~/.config/cubes-and-portals-release/.
// It never goes in the repository: on the CI it
// is the secret RELEASE_SIGN_KEY.

#include "core/net/crypto.h"
#include "core/release.h"
#include "core/version.h"
#include "script/mod_host.h"
#include "core/net/protocol.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace cnp;
namespace fs = std::filesystem;

namespace {

int usage()
{
    std::fprintf(stderr, "usage: cnp_release key <key file>\n"
                         "       cnp_release sign <key file> <dir>\n"
                         "       cnp_release check <dir>\n");
    return 2;
}

bool load_key(const std::string &path, net::SignKeyPair &key)
{
    bool        created = false;
    std::string err;
    if (!net::load_or_create_sign_key(path, key, created, err)) {
        std::fprintf(stderr, "cnp_release: %s\n", err.c_str());
        return false;
    }
    if (!err.empty()) std::fprintf(stderr, "cnp_release: %s\n", err.c_str());
    if (created) std::printf("created %s\n", path.c_str());
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3) return usage();
    const std::string cmd = argv[1];

    if (cmd == "key" && argc == 3) {
        net::SignKeyPair key;
        if (!load_key(argv[2], key)) return 1;
        std::printf("public key %s\n", net::to_hex(key.public_key, net::kSignPublicBytes).c_str());
        return 0;
    }

    if (cmd == "sign" && argc == 4) {
        net::SignKeyPair key;
        if (!load_key(argv[2], key)) return 1;
        release::Manifest m;
        m.version        = kEngineVersion;
        m.protocol       = net::kProtocol;
        m.min_protocol   = net::kMinProtocol;
        m.mod_api_oldest = kModApiOldest;
        m.mod_api        = kModApi;
        std::error_code ec;
        for (const fs::directory_entry &e : fs::directory_iterator(argv[3], ec)) {
            if (!e.is_regular_file(ec) || e.path().filename() == release::kManifestName) continue;
            release::File f;
            std::string   err;
            if (!release::describe_file(e.path().string(), f, err)) {
                std::fprintf(stderr, "cnp_release: %s\n", err.c_str());
                return 1;
            }
            m.files.push_back(std::move(f));
        }
        std::sort(m.files.begin(), m.files.end(),
                  [](const release::File &a, const release::File &b) { return a.name < b.name; });
        if (m.files.empty()) {
            std::fprintf(stderr, "cnp_release: nothing to sign in %s\n", argv[3]);
            return 1;
        }
        const std::string out = (fs::path(argv[3]) / release::kManifestName).string();
        std::ofstream(out, std::ios::binary) << release::write(m, key);
        std::printf("%s: %zu file(s), version %s, protocol %u..%u\n", out.c_str(), m.files.size(),
                    m.version.c_str(), m.min_protocol, m.protocol);
        return 0;
    }

    if (cmd == "check" && argc == 3) {
        uint8_t pub[net::kSignPublicBytes];
        if (!release::release_key(pub)) {
            std::fprintf(stderr, "cnp_release: this build has no release key\n");
            return 1;
        }
        const fs::path dir = argv[2];
        std::ifstream  in(dir / release::kManifestName, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        release::Manifest m;
        std::string       err;
        if (!release::read(text, pub, m, err)) {
            std::fprintf(stderr, "cnp_release: refused: %s\n", err.c_str());
            return 1;
        }
        int bad = 0;
        for (const release::File &f : m.files) {
            if (!release::check_file(f, (dir / f.name).string(), err)) {
                std::fprintf(stderr, "cnp_release: refused: %s\n", err.c_str());
                ++bad;
            } else {
                std::printf("ok %s\n", f.name.c_str());
            }
        }
        std::printf("version %s, protocol %u..%u, mod api %u..%u\n", m.version.c_str(),
                    m.min_protocol, m.protocol, m.mod_api_oldest, m.mod_api);
        return bad == 0 ? 0 : 1;
    }

    return usage();
}
