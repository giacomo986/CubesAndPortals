#pragma once

// What a release is, signed: the file the launcher reads before it runs anything
// it downloaded (TODO.md, "Launcher").
//
// A release on GitHub carries its packages and one more asset, `release.manifest`:
// text, one `key = value` a line, then a last line `signature = <hex>` over every
// byte before it, made with the release key (tools/release, `cnp_release sign`).
//
//   # Cubes and Portals release
//   version = 0.10.0-alpha.3
//   protocol = 113
//   min_protocol = 112
//   mod_api = 1 1
//   file = cubes-and-portals-0.10.0-alpha.3-linux-x86_64.tar.gz 24117248 <hash hex>
//
// The hash is hash_content (media/content_hash.h) of the whole file. A manifest
// whose signature is not the release key's is refused whole, and so is a package
// whose size or hash is not the one listed: the launcher never runs either.

#include "core/media/content_hash.h"
#include "core/net/crypto.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cnp::release {

inline constexpr const char *kManifestName = "release.manifest";

struct File {
    std::string name;   // the asset's name, no directory
    uint64_t    size = 0;
    ContentHash hash;
};

struct Manifest {
    std::string       version;
    uint16_t          protocol     = 0;
    uint16_t          min_protocol = 0;
    unsigned          mod_api_oldest = 0, mod_api = 0;
    std::vector<File> files;
};

// The manifest's text, signed with `key`.
std::string write(const Manifest &m, const net::SignKeyPair &key);

// Reads and checks a manifest's text against `public_key`. False, with `err`
// saying why, for a bad signature, a line it does not know, or a field missing.
bool read(const std::string &text, const uint8_t public_key[net::kSignPublicBytes],
          Manifest &out, std::string &err);

// Size and hash of the file at `path` against what the manifest lists.
bool check_file(const File &f, const std::string &path, std::string &err);

// The file's entry, from the file itself: what `write` is given.
bool describe_file(const std::string &path, File &out, std::string &err);

// The release key's public half (core/version.h, kReleaseKeyHex).
bool release_key(uint8_t out[net::kSignPublicBytes]);

} // namespace cnp::release
