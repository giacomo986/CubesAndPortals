#pragma once

#include <map>
#include <string>
#include <vector>

// What a server and the single-process client both do with a line of chat that
// starts with '/': find the command, or say it is not there. Two hosts, one
// rule, so /help reads the same whichever side answers it.

namespace cnp {
namespace net {

// A /command a mod declared (core.command). Only `owner` hears it.
struct ChatCommand {
    std::string owner;
    std::string help;
    std::string perm;   // what a player needs to run it (src/server/admin.h); "" anybody
};
using ChatCommands = std::map<std::string, ChatCommand>;

// The engine's own, which no mod may take.
inline bool chat_builtin(const std::string &name) { return name == "help" || name == "list"; }

// 1 to 32 of a-z, 0-9, '_' and '-': what a player can type without guessing.
inline bool chat_command_name_ok(const std::string &name)
{
    if (name.empty() || name.size() > 32) return false;
    for (const char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    return true;
}

// "/tp 1 2 3" is "tp" and "1 2 3". False for a line that is not a command.
inline bool chat_split(const std::string &line, std::string &name, std::string &args)
{
    if (line.size() < 2 || line[0] != '/') return false;
    const size_t space = line.find(' ');
    name               = line.substr(1, space == std::string::npos ? std::string::npos : space - 1);
    args               = space == std::string::npos ? "" : line.substr(space + 1);
    args.erase(0, args.find_first_not_of(' '));
    return true;
}

// What /help answers, one line each.
inline std::vector<std::string> chat_help(const ChatCommands &mods)
{
    std::vector<std::string> out = {"/help -- this list", "/list -- who is in this world"};
    for (const auto &[name, c] : mods)
        out.push_back("/" + name + (c.help.empty() ? "" : " " + c.help));
    return out;
}

} // namespace net
} // namespace cnp
