#include "core/release.h"

#include "core/version.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace cnp::release {

namespace {

bool read_whole(const std::string &path, std::vector<uint8_t> &out, std::string &err)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "cannot open " + path;
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

std::string trim(const std::string &s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

} // namespace

std::string write(const Manifest &m, const net::SignKeyPair &key)
{
    std::ostringstream o;
    o << "# Cubes and Portals release\n"
      << "version = " << m.version << "\n"
      << "protocol = " << m.protocol << "\n"
      << "min_protocol = " << m.min_protocol << "\n"
      << "mod_api = " << m.mod_api_oldest << " " << m.mod_api << "\n";
    for (const File &f : m.files) o << "file = " << f.name << " " << f.size << " " << f.hash.hex() << "\n";
    const std::string body = o.str();
    uint8_t sig[net::kSignatureBytes];
    net::sign_message(key, body.data(), body.size(), sig);
    return body + "signature = " + net::to_hex(sig, sizeof sig) + "\n";
}

bool read(const std::string &text, const uint8_t public_key[net::kSignPublicBytes], Manifest &out,
          std::string &err)
{
    out = Manifest{};
    const size_t at = text.rfind("signature = ");
    if (at == std::string::npos || (at > 0 && text[at - 1] != '\n')) {
        err = "the manifest is not signed";
        return false;
    }
    uint8_t sig[net::kSignatureBytes];
    if (!net::from_hex(trim(text.substr(at + 12)), sig, sizeof sig) ||
        !net::verify_message(public_key, text.data(), at, sig)) {
        err = "the manifest's signature is not the release key's";
        return false;
    }

    std::istringstream in(text.substr(0, at));
    bool               has_protocol = false, has_api = false;
    for (std::string line; std::getline(in, line);) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            err = "a line without '=': " + line;
            return false;
        }
        const std::string key = trim(line.substr(0, eq));
        std::istringstream v(trim(line.substr(eq + 1)));
        if (key == "version") {
            v >> out.version;
        } else if (key == "protocol") {
            has_protocol = (bool)(v >> out.protocol);
        } else if (key == "min_protocol") {
            v >> out.min_protocol;
        } else if (key == "mod_api") {
            has_api = (bool)(v >> out.mod_api_oldest >> out.mod_api);
        } else if (key == "file") {
            File        f;
            std::string hex;
            if (!(v >> f.name >> f.size >> hex) || !content_hash_from_hex(hex, f.hash) ||
                f.name.find_first_of("/\\:") != std::string::npos || f.name == ".." ||
                f.name == ".") {
                err = "a file line it cannot read: " + line;
                return false;
            }
            out.files.push_back(std::move(f));
        } else {
            err = "an unknown key: " + key;
            return false;
        }
    }
    if (out.version.empty() || !has_protocol || !has_api) {
        err = "the manifest lacks its version, protocol or mod_api";
        return false;
    }
    return true;
}

bool describe_file(const std::string &path, File &out, std::string &err)
{
    std::vector<uint8_t> bytes;
    if (!read_whole(path, bytes, err)) return false;
    out.name = std::filesystem::path(path).filename().string();
    out.size = bytes.size();
    out.hash = hash_content(bytes);
    return true;
}

bool check_file(const File &f, const std::string &path, std::string &err)
{
    File here;
    if (!describe_file(path, here, err)) return false;
    if (here.size != f.size || here.hash != f.hash) {
        err = f.name + " is not the file the release lists";
        return false;
    }
    return true;
}

bool release_key(uint8_t out[net::kSignPublicBytes])
{
    return net::from_hex(kReleaseKeyHex, out, net::kSignPublicBytes);
}

} // namespace cnp::release
