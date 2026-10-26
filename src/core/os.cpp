#include "core/os.h"

#include <cerrno>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#endif

namespace cnp {

#ifdef _WIN32

namespace {

std::wstring wide(const std::string &s)
{
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

// One argument as CommandLineToArgvW reads it back: quoted, with the
// backslashes before a quote doubled.
void append_quoted(std::wstring &line, const std::wstring &arg)
{
    line += L'"';
    size_t slashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        line.append(c == L'"' ? 2 * slashes + 1 : slashes, L'\\');
        slashes = 0;
        line += c;
    }
    line.append(2 * slashes, L'\\');
    line += L'"';
}

// The polite stop has no signal to ride on: it is an event named after the
// server's process id, which the server waits on and the client sets.
std::string stop_event_name(DWORD pid)
{
    return "Local\\cnp-stop-" + std::to_string(pid);
}

volatile std::sig_atomic_t *g_flag = nullptr;

void on_interrupt(int)
{
    if (g_flag) *g_flag = 1;
    std::signal(SIGINT, SIG_DFL);
}

} // namespace

int hold_lock_file(const std::string &path)
{
    // No sharing at all: a second open fails for as long as this handle lives,
    // which is the life of the process -- it is never closed.
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) return 0;
    if (GetLastError() == ERROR_SHARING_VIOLATION) return 1;
    errno = EACCES;
    return -1;
}

bool lock_file_held(const std::string &path)
{
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return false;
    }
    return GetLastError() == ERROR_SHARING_VIOLATION;
}

bool ChildProcess::start(const std::string &prog, const std::vector<std::string> &args,
                         const std::string &dir, const std::string &log, std::string &err)
{
    std::wstring line;
    append_quoted(line, wide(prog));
    for (const std::string &a : args) {
        line += L' ';
        append_quoted(line, wide(a));
    }

    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    HANDLE out = CreateFileW(wide(log).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    if (out != INVALID_HANDLE_VALUE) {
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdOutput = out;
        si.hStdError  = out;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    }

    // In a job that dies with the client: Windows' PR_SET_PDEATHSIG.
    // ponytail: a client that crashes takes its server down unsaved; Linux
    // gets a SIGTERM and saves. Upgrade: the server waits on its parent too.
    static HANDLE job = [] {
        HANDLE j = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim{};
        lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(j, JobObjectExtendedLimitInformation, &lim, sizeof(lim));
        return j;
    }();

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                   nullptr, wide(dir).c_str(), &si, &pi);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (!ok) {
        err = "cannot start " + prog + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    CloseHandle(pi.hThread);
    id = (intptr_t)pi.hProcess;
    return true;
}

bool ChildProcess::exited()
{
    if (id == -1) return true;
    if (WaitForSingleObject((HANDLE)id, 0) != WAIT_OBJECT_0) return false;
    CloseHandle((HANDLE)id);
    id = -1;
    return true;
}

void ChildProcess::stop()
{
    if (id == -1) return;
    HANDLE h = (HANDLE)id;
    const std::string name = stop_event_name(GetProcessId(h));
    if (HANDLE ev = OpenEventA(EVENT_MODIFY_STATE, FALSE, name.c_str())) {
        SetEvent(ev);
        CloseHandle(ev);
    } else {
        TerminateProcess(h, 1);   // no event: it never got as far as listening
    }
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    id = -1;
}

void install_stop_flag(volatile std::sig_atomic_t *flag)
{
    g_flag = flag;
    std::signal(SIGINT, on_interrupt);

    const std::string name = stop_event_name(GetCurrentProcessId());
    HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, name.c_str());
    if (ev == nullptr) return;
    std::thread([ev, flag] {
        WaitForSingleObject(ev, INFINITE);
        *flag = 1;
    }).detach();
}

#else // POSIX

namespace {

volatile std::sig_atomic_t *g_flag = nullptr;

void on_stop(int)
{
    if (g_flag) *g_flag = 1;
}

} // namespace

int hold_lock_file(const std::string &path)
{
    // The descriptor is deliberately never closed: closing it would let go of
    // the lock, and what holds it is the life of the process.
    const int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) return -1;
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return 1;
    }
    return 0;
}

bool lock_file_held(const std::string &path)
{
    const int fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0) return false;   // no lock file: never served
    const bool held = ::flock(fd, LOCK_EX | LOCK_NB) != 0;
    ::close(fd);
    return held;
}

bool ChildProcess::start(const std::string &prog, const std::vector<std::string> &args,
                         const std::string &dir, const std::string &log, std::string &err)
{
    // Everything the child needs, made before the fork: between fork and exec
    // in a process with threads, allocating is not safe.
    std::vector<const char *> argv{prog.c_str()};
    for (const std::string &a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    const pid_t parent = ::getpid();
    const pid_t pid    = ::fork();
    if (pid < 0) {
        err = std::string("fork: ") + std::strerror(errno);
        return false;
    }
    if (pid == 0) {
#if defined(__linux__)
        // The kernel sends the stop by itself if the client dies first.
        ::prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (::getppid() != parent) ::_exit(1);
#else
        (void)parent;   // ponytail: no parent-death signal on macOS; a crashed client leaves its server running
#endif
        const int fd = ::open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            ::dup2(fd, 1);
            ::dup2(fd, 2);
        }
        if (::chdir(dir.c_str()) != 0) ::_exit(1);
        ::execv(prog.c_str(), (char *const *)argv.data());
        ::_exit(127);
    }
    id = pid;
    return true;
}

bool ChildProcess::exited()
{
    if (id == -1) return true;
    if (::waitpid((pid_t)id, nullptr, WNOHANG) != (pid_t)id) return false;
    id = -1;
    return true;
}

void ChildProcess::stop()
{
    if (id == -1) return;
    ::kill((pid_t)id, SIGTERM);
    ::waitpid((pid_t)id, nullptr, 0);
    id = -1;
}

// SA_RESETHAND on SIGINT: the second Ctrl+C is the default one again, so a save
// that hangs can still be killed from the same terminal. Not on SIGTERM, which
// can come more than once for one stop: the client's title screen has its world
// server sent SIGTERM when the client dies (PR_SET_PDEATHSIG), and the kernel
// sends it again each time one of the client's threads exits -- the second one
// killed the server before it saved.
void install_stop_flag(volatile std::sig_atomic_t *flag)
{
    g_flag = flag;
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGTERM, &sa, nullptr);
    sa.sa_flags = SA_RESETHAND;
    ::sigaction(SIGINT, &sa, nullptr);
}

#endif

std::string local_ipv4()
{
#ifdef _WIN32
    // No getifaddrs: the address a UDP socket would leave from towards the
    // internet. connect() on UDP sends nothing.
    std::string found;
    const SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return found;
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port   = htons(53);
    ::inet_pton(AF_INET, "8.8.8.8", &to.sin_addr);
    sockaddr_in self{};
    int         len = sizeof(self);
    char        text[INET_ADDRSTRLEN] = {0};
    if (::connect(sock, (sockaddr *)&to, sizeof(to)) == 0 &&
        ::getsockname(sock, (sockaddr *)&self, &len) == 0 &&
        ::inet_ntop(AF_INET, &self.sin_addr, text, sizeof(text)) != nullptr)
        found = text;
    ::closesocket(sock);
    return found;
#else
    ifaddrs *list = nullptr;
    if (::getifaddrs(&list) != 0) return "";

    std::string found;
    for (const ifaddrs *a = list; a != nullptr && found.empty(); a = a->ifa_next) {
        if (a->ifa_addr == nullptr || a->ifa_addr->sa_family != AF_INET) continue;
        if ((a->ifa_flags & IFF_LOOPBACK) != 0 || (a->ifa_flags & IFF_UP) == 0) continue;

        char text[INET_ADDRSTRLEN] = {0};
        const sockaddr_in *in = (const sockaddr_in *)(const void *)a->ifa_addr;
        if (::inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)) != nullptr) found = text;
    }
    ::freeifaddrs(list);
    return found;
#endif
}

std::string resolve_ipv4(const std::string &host)
{
    addrinfo  hints{};
    addrinfo *found = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    std::string ip;
    if (::getaddrinfo(host.c_str(), nullptr, &hints, &found) == 0 && found != nullptr) {
        char text[INET_ADDRSTRLEN] = {0};
        const sockaddr_in *in = (const sockaddr_in *)(const void *)found->ai_addr;
        if (::inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)) != nullptr) ip = text;
        ::freeaddrinfo(found);
    }
    return ip;
}

} // namespace cnp
