#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// A mod as one file.
//
// DESIGN.md M10 asks for a "formato pacchetto mod". A mod is a directory, and a
// directory is not a thing you can hand somebody: there is nothing that says
// which files belong to it, nothing that says they all arrived, and nothing a
// build can check before it runs somebody else's Lua. This is that file.
//
// It knows nothing about Luau, a registry or a world, and it is its own library
// for that reason: the engine reads packages and a tool writes them, and
// neither should have to link the other. The only things under it are zstd,
// which the chunks on the wire already use, and monocypher's BLAKE2b, which is
// what core/media/content_hash.h hashes media with -- the same 32 bytes, so a
// package hash and a media hash mean the same kind of thing.
//
// The layout, little endian throughout:
//
//     magic[8]              "CNPMOD\x1a\n"
//     format        u16     1
//     reserved      u16     0
//     entries       u32
//     plain_bytes   u64     the payload, unpacked
//     stored_bytes  u64     the payload, as it sits in the file
//     payload_hash  [32]    BLAKE2b-256 over the UNPACKED payload
//     index         entries x { path_len u16, path, offset u64, size u64 }
//     payload               one zstd frame
//
// **One zstd frame over all of the files rather than one per file.** A mod is a
// dozen small text files that compress against each other; compressing them
// apart would throw that away to buy random access nothing here wants, because
// a mod is unpacked whole or not at all.
//
// **The header does not carry the mod's name or version.** They are in the
// mod.conf inside, which is the only place a manifest should be read from: a
// name in two places is two names, and the one on the outside is the one that
// can be a lie. What the outside is for is deciding whether the inside is
// intact, which is what the hash does.
//
// **A package is canonical.** The entries are sorted by path and their bytes
// are contiguous and cover the payload exactly, so packing one directory twice
// produces the same file to the byte -- a property worth having when a package
// is something people send each other and compare.

namespace cnp {

inline constexpr char     kModPackageMagic[8]   = {'C', 'N', 'P', 'M', 'O', 'D', '\x1a', '\n'};
inline constexpr uint16_t kModPackageFormat     = 1;

// Caps, because a package is the first thing in this engine that arrives from
// somewhere else and is read before anything about it is known. A refusal that
// names the limit is the answer; running out of memory is not.
inline constexpr size_t kModPackageMaxFiles     = 4096;
inline constexpr size_t kModPackageMaxPathBytes = 200;
inline constexpr size_t kModPackageMaxFileBytes = 16u * 1024u * 1024u;
inline constexpr size_t kModPackageMaxPlainBytes = 64u * 1024u * 1024u;

struct ModPackageFile {
    std::string          path;    // relative, '/' separated, never leaving the mod
    std::vector<uint8_t> bytes;
};

struct ModPackage {
    std::vector<ModPackageFile> files;

    // BLAKE2b-256 over the unpacked payload, as hex. Empty until the package
    // has been read or written: it is what the bytes ARE, not a field somebody
    // fills in.
    std::string hash;

    const ModPackageFile *find(const std::string &path) const;
};

// Whether a path may be inside a package: relative, '/' separated, no '..', no
// component that is empty or a lone dot, nothing that a Windows filesystem
// would read as a drive or a separator, and short enough to write down.
//
// Its own function because the answer has to be the same when a package is
// written, when it is read, and when it is unpacked -- three places, and a mod
// that escapes its own directory only has to get past one of them.
bool mod_package_path_is_safe(const std::string &path, std::string &error);

// Reads a directory into a package: every file under it, recursively, sorted by
// path. Anything whose name begins with a dot is skipped, at every level --
// a .git directory is not part of a mod, and neither is an editor's backup.
bool mod_package_read_dir(const std::string &dir, ModPackage &out, std::string &error);

// Serialises. `level` is zstd's, and 19 is what a package is written at once and
// read many times deserves.
bool mod_package_write(const ModPackage &in, std::vector<uint8_t> &out, std::string &error,
                       int level = 19);

// Parses and VERIFIES: the hash over the unpacked payload has to be the one in
// the header, or nothing comes back. Half a mod is worse than no mod -- it is a
// mod that registers half its blocks and leaves a world full of them.
bool mod_package_parse(const void *data, size_t size, ModPackage &out, std::string &error);

// Writes the package's files into `dir`, creating it and whatever directories
// the paths need. An existing file is overwritten; nothing outside `dir` is
// ever touched, which is what mod_package_path_is_safe is checked for again
// here rather than trusted from the parse.
bool mod_package_unpack(const ModPackage &in, const std::string &dir, std::string &error);

// Convenience for the two callers that have a filename rather than bytes.
bool mod_package_read_file(const std::string &path, ModPackage &out, std::string &error);
bool mod_package_write_file(const ModPackage &in, const std::string &path, std::string &error,
                            int level = 19);

// The extension a package is found by, dot included.
inline constexpr const char *kModPackageExtension = ".cnpmod";

} // namespace cnp
