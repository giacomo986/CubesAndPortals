#pragma once

#include <csignal>
#include <cstdint>
#include <string>
#include <vector>

// What the programs need from the operating system that POSIX and Windows spell
// differently (OPEN-40): a lock a process holds for its life, a child server
// that can be told to save and stop, and the way that stop arrives.

namespace cnp {

// Takes the lock at `path` (created if missing) and keeps it until the process
// ends, however it ends. 0 taken, 1 somebody else holds it, -1 cannot open the
// file (errno says why).
int hold_lock_file(const std::string &path);

// Whether somebody holds the lock at `path` now. False when there is no file.
bool lock_file_held(const std::string &path);

// A program started in the background, its output in a log file. Stopping it is
// the polite stop -- a server saves and closes -- and then waiting for it.
struct ChildProcess {
    intptr_t id = -1;   // pid on POSIX, process HANDLE on Windows

    bool running() const { return id != -1; }
    bool start(const std::string &prog, const std::vector<std::string> &args,
               const std::string &dir, const std::string &log, std::string &err);
    bool exited();   // true once it ended (and then id is -1)
    void stop();
};

// Ctrl+C, a SIGTERM, or (Windows) ChildProcess::stop from the client: `flag`
// goes to 1. The second Ctrl+C is the default one again.
void install_stop_flag(volatile std::sig_atomic_t *flag);

// The address somebody else on this network types to reach a world hosted
// here: this machine's first IPv4 that is not the loopback. Empty when there
// is none (no network, or IPv6 only), and then the port alone is what the
// menu can say.
std::string local_ipv4();

// `host` as a dotted IPv4 (ENet speaks IPv4), empty when it does not resolve.
// Blocks for as long as the resolver takes.
std::string resolve_ipv4(const std::string &host);

} // namespace cnp
