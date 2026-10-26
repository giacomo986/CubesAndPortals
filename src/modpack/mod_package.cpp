#include "modpack/mod_package.h"

#include <monocypher.h>
#include <zstd.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace cnp {
namespace {

namespace fs = std::filesystem;

void put_u16(std::vector<uint8_t> &out, uint16_t v)
{
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)(v >> 8));
}

void put_u32(std::vector<uint8_t> &out, uint32_t v)
{
    for (int i = 0; i < 4; ++i) out.push_back((uint8_t)((v >> (8 * i)) & 0xFF));
}

void put_u64(std::vector<uint8_t> &out, uint64_t v)
{
    for (int i = 0; i < 8; ++i) out.push_back((uint8_t)((v >> (8 * i)) & 0xFF));
}

// A cursor that cannot read past the end. Every field below goes through it,
// because a truncated package is the first thing this will be handed and
// "checked the length up here" is how that gets read out of bounds anyway.
struct Reader {
    const uint8_t *data = nullptr;
    size_t         size = 0;
    size_t         at   = 0;

    bool take(void *dst, size_t n)
    {
        if (n > size - at) return false;
        std::memcpy(dst, data + at, n);
        at += n;
        return true;
    }
    bool u16(uint16_t &v)
    {
        uint8_t b[2];
        if (!take(b, 2)) return false;
        v = (uint16_t)(b[0] | (b[1] << 8));
        return true;
    }
    bool u32(uint32_t &v)
    {
        uint8_t b[4];
        if (!take(b, 4)) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) v |= (uint32_t)b[i] << (8 * i);
        return true;
    }
    bool u64(uint64_t &v)
    {
        uint8_t b[8];
        if (!take(b, 8)) return false;
        v = 0;
        for (int i = 0; i < 8; ++i) v |= (uint64_t)b[i] << (8 * i);
        return true;
    }
};

const char *kHexDigits = "0123456789abcdef";

std::string hash_hex(const uint8_t *bytes, size_t n)
{
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(kHexDigits[bytes[i] >> 4]);
        out.push_back(kHexDigits[bytes[i] & 0x0F]);
    }
    return out;
}

// BLAKE2b-256, the same hash and the same length core/media/content_hash.h uses
// for a file a world serves. Deliberately the same: a package and a texture are
// both "these exact bytes", and two answers to one question is one too many.
std::string payload_hash(const std::vector<uint8_t> &plain)
{
    uint8_t h[32];
    crypto_blake2b(h, sizeof(h), plain.data(), plain.size());
    return hash_hex(h, sizeof(h));
}

bool read_whole_file(const fs::path &p, std::vector<uint8_t> &out)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

} // namespace

const ModPackageFile *ModPackage::find(const std::string &path) const
{
    for (const ModPackageFile &f : files) {
        if (f.path == path) return &f;
    }
    return nullptr;
}

bool mod_package_path_is_safe(const std::string &path, std::string &error)
{
    error.clear();

    if (path.empty()) {
        error = "a file with no name";
        return false;
    }
    if (path.size() > kModPackageMaxPathBytes) {
        error = "'" + path.substr(0, 40) + "...' is longer than " +
                std::to_string(kModPackageMaxPathBytes) + " bytes";
        return false;
    }
    if (path[0] == '/') {
        error = "'" + path + "' is an absolute path";
        return false;
    }

    // A backslash is a separator on the system a package may be unpacked on,
    // and a colon is a drive there. Refusing both everywhere is what keeps a
    // package meaning the same thing on both, rather than being safe here and
    // an escape hatch on somebody else's machine.
    for (const char c : path) {
        if (c == '\\' || c == ':') {
            error = "'" + path + "' has a '" + c + "' in it";
            return false;
        }
        if ((unsigned char)c < 0x20) {
            error = "'" + path + "' has a control character in it";
            return false;
        }
    }

    size_t start = 0;
    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const std::string part =
            path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);

        if (part.empty()) {
            error = "'" + path + "' has an empty name in it";
            return false;
        }
        if (part == "." || part == "..") {
            error = "'" + path + "' walks out of the mod";
            return false;
        }

        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

bool mod_package_read_dir(const std::string &dir, ModPackage &out, std::string &error)
{
    error.clear();
    out = ModPackage{};

    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        error = dir + " is not a directory";
        return false;
    }

    const fs::path root = fs::path(dir);

    // Recursive, and skipping anything whose name starts with a dot at any
    // level: .git is not part of a mod, and neither is the editor swap file
    // that happens to be open while somebody packs one.
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied,
                                             ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            error = "could not read " + dir + ": " + ec.message();
            return false;
        }

        const std::string name = it->path().filename().string();
        if (!name.empty() && name[0] == '.') {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            continue;
        }
        if (it->is_directory(ec)) continue;
        if (!it->is_regular_file(ec)) continue;

        const std::string rel = fs::relative(it->path(), root, ec).generic_string();
        if (!mod_package_path_is_safe(rel, error)) return false;

        ModPackageFile f;
        f.path = rel;
        if (!read_whole_file(it->path(), f.bytes)) {
            error = "could not read " + rel;
            return false;
        }
        if (f.bytes.size() > kModPackageMaxFileBytes) {
            error = rel + " is larger than " + std::to_string(kModPackageMaxFileBytes) + " bytes";
            return false;
        }
        out.files.push_back(std::move(f));

        if (out.files.size() > kModPackageMaxFiles) {
            error = dir + " holds more than " + std::to_string(kModPackageMaxFiles) + " files";
            return false;
        }
    }

    // Sorted, because a directory iteration order is not an order: two machines
    // packing the same mod have to produce the same file, or a hash means
    // "whoever packed it" rather than "what is in it".
    std::sort(out.files.begin(), out.files.end(),
              [](const ModPackageFile &a, const ModPackageFile &b) { return a.path < b.path; });

    if (out.files.empty()) {
        error = dir + " has no files in it";
        return false;
    }
    if (!out.find("mod.conf")) {
        error = dir + " has no mod.conf, so it is not a mod";
        return false;
    }
    return true;
}

bool mod_package_write(const ModPackage &in, std::vector<uint8_t> &out, std::string &error,
                       int level)
{
    error.clear();
    out.clear();

    if (in.files.empty()) {
        error = "a package with no files in it";
        return false;
    }
    if (in.files.size() > kModPackageMaxFiles) {
        error = "more than " + std::to_string(kModPackageMaxFiles) + " files";
        return false;
    }

    std::vector<ModPackageFile> files = in.files;
    std::sort(files.begin(), files.end(),
              [](const ModPackageFile &a, const ModPackageFile &b) { return a.path < b.path; });

    std::vector<uint8_t> plain;
    for (size_t i = 0; i < files.size(); ++i) {
        if (!mod_package_path_is_safe(files[i].path, error)) return false;
        if (i > 0 && files[i].path == files[i - 1].path) {
            error = "'" + files[i].path + "' is in the package twice";
            return false;
        }
        if (files[i].bytes.size() > kModPackageMaxFileBytes) {
            error = files[i].path + " is larger than " +
                    std::to_string(kModPackageMaxFileBytes) + " bytes";
            return false;
        }
        plain.insert(plain.end(), files[i].bytes.begin(), files[i].bytes.end());
        if (plain.size() > kModPackageMaxPlainBytes) {
            error = "the files come to more than " + std::to_string(kModPackageMaxPlainBytes) +
                    " bytes";
            return false;
        }
    }

    if (!in.find("mod.conf")) {
        error = "a package with no mod.conf in it is not a mod";
        return false;
    }

    std::vector<uint8_t> stored(ZSTD_compressBound(plain.size()));
    const size_t n = ZSTD_compress(stored.data(), stored.size(), plain.data(), plain.size(), level);
    if (ZSTD_isError(n)) {
        error = std::string("zstd: ") + ZSTD_getErrorName(n);
        return false;
    }
    stored.resize(n);

    const std::string hex = payload_hash(plain);

    out.insert(out.end(), kModPackageMagic, kModPackageMagic + sizeof(kModPackageMagic));
    put_u16(out, kModPackageFormat);
    put_u16(out, 0);
    put_u32(out, (uint32_t)files.size());
    put_u64(out, (uint64_t)plain.size());
    put_u64(out, (uint64_t)stored.size());

    uint8_t h[32];
    crypto_blake2b(h, sizeof(h), plain.data(), plain.size());
    out.insert(out.end(), h, h + sizeof(h));

    uint64_t offset = 0;
    for (const ModPackageFile &f : files) {
        put_u16(out, (uint16_t)f.path.size());
        out.insert(out.end(), f.path.begin(), f.path.end());
        put_u64(out, offset);
        put_u64(out, (uint64_t)f.bytes.size());
        offset += f.bytes.size();
    }

    out.insert(out.end(), stored.begin(), stored.end());
    return true;
}

bool mod_package_parse(const void *data, size_t size, ModPackage &out, std::string &error)
{
    error.clear();
    out = ModPackage{};

    Reader r{(const uint8_t *)data, size, 0};

    char magic[sizeof(kModPackageMagic)];
    if (!r.take(magic, sizeof(magic)) ||
        std::memcmp(magic, kModPackageMagic, sizeof(magic)) != 0) {
        error = "not a mod package";
        return false;
    }

    uint16_t format = 0, reserved = 0;
    uint32_t entries = 0;
    uint64_t plain_bytes = 0, stored_bytes = 0;
    uint8_t  want_hash[32];

    if (!r.u16(format) || !r.u16(reserved) || !r.u32(entries) || !r.u64(plain_bytes) ||
        !r.u64(stored_bytes) || !r.take(want_hash, sizeof(want_hash))) {
        error = "the header is cut short";
        return false;
    }

    // A format number rather than a guess. A build that meets a package from a
    // later one refuses it by number instead of reading the fields it knows and
    // silently dropping whatever was added.
    if (format != kModPackageFormat) {
        error = "package format " + std::to_string(format) + ", and this build reads " +
                std::to_string(kModPackageFormat);
        return false;
    }
    if (entries == 0) {
        error = "the package has no files in it";
        return false;
    }
    if (entries > kModPackageMaxFiles) {
        error = "the package claims " + std::to_string(entries) + " files, over the " +
                std::to_string(kModPackageMaxFiles) + " this build reads";
        return false;
    }
    if (plain_bytes > kModPackageMaxPlainBytes) {
        error = "the package unpacks to " + std::to_string(plain_bytes) + " bytes, over the " +
                std::to_string(kModPackageMaxPlainBytes) + " this build reads";
        return false;
    }

    struct Entry {
        std::string path;
        uint64_t    offset = 0;
        uint64_t    size   = 0;
    };
    std::vector<Entry> index;
    index.reserve(entries);

    for (uint32_t i = 0; i < entries; ++i) {
        uint16_t len = 0;
        if (!r.u16(len) || len == 0 || len > kModPackageMaxPathBytes) {
            error = "the index is cut short or names a file with no name";
            return false;
        }
        Entry e;
        e.path.resize(len);
        if (!r.take(e.path.data(), len) || !r.u64(e.offset) || !r.u64(e.size)) {
            error = "the index is cut short";
            return false;
        }
        if (!mod_package_path_is_safe(e.path, error)) return false;
        index.push_back(std::move(e));
    }

    // The index has to describe the payload EXACTLY: sorted, contiguous, and
    // covering all of it. Anything else is a package that means two things --
    // bytes nobody accounts for, or two files claiming the same bytes -- and a
    // format that allows two meanings is one somebody will use to smuggle the
    // second past whoever checked the first.
    uint64_t expect = 0;
    for (size_t i = 0; i < index.size(); ++i) {
        if (i > 0 && !(index[i - 1].path < index[i].path)) {
            error = "the index is not sorted, or names '" + index[i].path + "' twice";
            return false;
        }
        if (index[i].offset != expect) {
            error = "'" + index[i].path + "' does not start where the file before it ends";
            return false;
        }
        if (index[i].size > kModPackageMaxFileBytes) {
            error = "'" + index[i].path + "' is larger than " +
                    std::to_string(kModPackageMaxFileBytes) + " bytes";
            return false;
        }
        expect += index[i].size;
        if (expect > plain_bytes) {
            error = "'" + index[i].path + "' runs past the end of the package";
            return false;
        }
    }
    if (expect != plain_bytes) {
        error = "the package carries " + std::to_string(plain_bytes - expect) +
                " bytes no file in it accounts for";
        return false;
    }

    if (stored_bytes != size - r.at) {
        error = "the package says its payload is " + std::to_string(stored_bytes) +
                " bytes and " + std::to_string(size - r.at) + " are here";
        return false;
    }

    std::vector<uint8_t> plain(plain_bytes);
    const size_t n = ZSTD_decompress(plain.data(), plain.size(), r.data + r.at, stored_bytes);
    if (ZSTD_isError(n)) {
        error = std::string("the payload does not unpack: ") + ZSTD_getErrorName(n);
        return false;
    }
    if (n != plain_bytes) {
        error = "the payload unpacks to " + std::to_string(n) + " bytes and the header says " +
                std::to_string(plain_bytes);
        return false;
    }

    // Before a single file is handed out, and not after. A mod is whole or it is
    // nothing: half of one registers half its blocks and leaves a world full of
    // them, and nobody looking at that world can tell which half it got.
    uint8_t have_hash[32];
    crypto_blake2b(have_hash, sizeof(have_hash), plain.data(), plain.size());
    if (crypto_verify32(have_hash, want_hash) != 0) {
        error = "the package says its contents hash to " +
                hash_hex(want_hash, sizeof(want_hash)).substr(0, 8) + " and they hash to " +
                hash_hex(have_hash, sizeof(have_hash)).substr(0, 8);
        return false;
    }

    out.files.reserve(index.size());
    for (const Entry &e : index) {
        ModPackageFile f;
        f.path = e.path;
        f.bytes.assign(plain.begin() + (ptrdiff_t)e.offset,
                       plain.begin() + (ptrdiff_t)(e.offset + e.size));
        out.files.push_back(std::move(f));
    }
    out.hash = hash_hex(have_hash, sizeof(have_hash));

    if (!out.find("mod.conf")) {
        error = "the package has no mod.conf in it, so it is not a mod";
        out = ModPackage{};
        return false;
    }
    return true;
}

bool mod_package_unpack(const ModPackage &in, const std::string &dir, std::string &error)
{
    error.clear();

    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        error = "could not make " + dir + ": " + ec.message();
        return false;
    }

    for (const ModPackageFile &f : in.files) {
        // Checked again here rather than trusted from the parse. This is the one
        // call that writes to somebody's disk, and a rule enforced only at the
        // other end of a program is a rule that holds until somebody calls this
        // directly.
        if (!mod_package_path_is_safe(f.path, error)) return false;

        const fs::path target = fs::path(dir) / fs::path(f.path);
        fs::create_directories(target.parent_path(), ec);

        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "could not write " + target.string();
            return false;
        }
        if (!f.bytes.empty())
            out.write((const char *)f.bytes.data(), (std::streamsize)f.bytes.size());
        out.close();
        if (!out) {
            error = "could not finish writing " + target.string();
            return false;
        }
    }
    return true;
}

bool mod_package_read_file(const std::string &path, ModPackage &out, std::string &error)
{
    std::vector<uint8_t> bytes;
    if (!read_whole_file(path, bytes)) {
        error = "could not read " + path;
        return false;
    }
    if (!mod_package_parse(bytes.data(), bytes.size(), out, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

bool mod_package_write_file(const ModPackage &in, const std::string &path, std::string &error,
                            int level)
{
    std::vector<uint8_t> bytes;
    if (!mod_package_write(in, bytes, error, level)) return false;

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        error = "could not write " + path;
        return false;
    }
    f.write((const char *)bytes.data(), (std::streamsize)bytes.size());
    f.close();
    if (!f) {
        error = "could not finish writing " + path;
        return false;
    }
    return true;
}

} // namespace cnp
