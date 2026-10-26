#include "core/registry/model_registry.h"

namespace cnp {

namespace {
const ModelRegistry::ModelDef kNothing{};
}

bool model_file_is_plain(const std::string &file)
{
    if (file.empty() || file.size() > 128) return false;
    if (file[0] == '.') return false;   // ".", "..", and hidden files, in one rule
    if (file.find("..") != std::string::npos) return false;
    for (const char c : file)
        if (c == '/' || c == '\\' || c == '\0') return false;
    return true;
}

bool model_file_name(const std::string &name, std::string &out)
{
    out.clear();
    if (name.empty() || name.size() > 96) return false;
    if (name[0] == '.') return false;

    std::string file;
    file.reserve(name.size() + 5);
    for (const char c : name) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (plain) {
            file.push_back(c);
        } else if (c == ':') {
            // The one piece of punctuation a name is made of, and it becomes
            // the one thing a file name can hold: "mymod:crab" is
            // "mymod_crab.gltf" and not a directory called mymod.
            file.push_back('_');
        } else {
            return false;   // a dot, a slash, a space, anything else
        }
    }

    out = file + ".gltf";
    return true;
}

std::string model_media_name(const std::string &model_name, const std::string &file)
{
    std::string safe;
    if (!model_file_name(model_name, safe)) return std::string();

    if (!model_file_is_plain(file)) return std::string();

    safe.resize(safe.size() - 5);   // without the ".gltf"
    return "model." + safe + "." + file;
}

ModelId ModelRegistry::add(const std::string &name, const std::string &file)
{
    if (name.empty()) return kNoModel;

    const auto it = m_by_name.find(name);
    if (it != m_by_name.end()) {
        m_defs[(size_t)it->second - 1].file = file;
        return it->second;
    }

    // 65535 models is well past anything the protocol carries (kMaxModelTable),
    // and the refusal is here as well because a registry that wrapped would
    // hand out id 0 -- which every body reads as "wearing nothing".
    if (m_defs.size() >= 0xFFFEu) return kNoModel;

    m_defs.push_back(ModelDef{name, file});
    const ModelId id = (ModelId)m_defs.size();   // 1 based
    m_by_name[name]  = id;
    return id;
}

ModelId ModelRegistry::find(const std::string &name) const
{
    const auto it = m_by_name.find(name);
    return it == m_by_name.end() ? kNoModel : it->second;
}

const ModelRegistry::ModelDef &ModelRegistry::get(ModelId id) const
{
    if (id == kNoModel || (size_t)id > m_defs.size()) return kNothing;
    return m_defs[(size_t)id - 1];
}

std::vector<std::string> ModelRegistry::table() const
{
    std::vector<std::string> out;
    out.reserve(m_defs.size());
    for (const ModelDef &d : m_defs) out.push_back(d.name);
    return out;
}

} // namespace cnp
