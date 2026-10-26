#include "core/diag/stack_dump.h"

#ifdef _WIN32
// ponytail: no stack dumps and no watchdog on Windows -- signals, backtrace()
// and pthread_kill are POSIX. A crash there is Windows' own dialog. Upgrade:
// SetUnhandledExceptionFilter + CaptureStackBackTrace.
namespace cnp {
void install_stack_dumper() {}
void install_crash_handler() {}
void watchdog_beat() {}
void watchdog_start(double) {}
} // namespace cnp
#else

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <execinfo.h>
#include <pthread.h>
#include <unistd.h>

namespace cnp {
namespace {

// Everything the handler touches. Nothing here is allocated, and nothing is
// touched by the handler that is not one of these.
constexpr int kMaxFrames = 64;

void                *g_frames[kMaxFrames];
std::atomic<unsigned> g_beats{0};
bool                  g_installed = false;
bool                  g_crash_installed = false;

pthread_t             g_main_thread;
std::atomic<bool>     g_watching{false};
std::atomic<double>   g_quiet_limit{0.0};

// Set before the watchdog signals, so the handler knows which of its two jobs
// it is doing. It matters because the process has to END inside the handler:
// signalling and then aborting from the watchdog thread is a race the target
// can win -- the signal breaks whatever it was waiting in, it carries on, and
// it exits normally while the watchdog is still counting to its abort.
std::atomic<bool>     g_for_watchdog{false};

void write_text(const char *s)
{
    // write(2), not fprintf: the handler may be running inside malloc.
    const size_t n = std::strlen(s);
    ssize_t      ignored = ::write(STDERR_FILENO, s, n);
    (void)ignored;
}

void dump_stack(const char *why)
{
    write_text("\n=== ");
    write_text(why);
    write_text(" ===\n");

    const int n = ::backtrace(g_frames, kMaxFrames);
    ::backtrace_symbols_fd(g_frames, n, STDERR_FILENO);
    write_text("=== end of stack ===\n");
}

void on_quit(int)
{
    if (g_for_watchdog.load(std::memory_order_relaxed)) {
        dump_stack("watchdog: the main loop has not moved");
        write_text("watchdog: aborting so the reason is attached to the failure\n");
        ::_exit(134);   // the shell's code for a process killed by SIGABRT
    }

    dump_stack("SIGQUIT: the stack of the thread that took the signal");
}

// Names, written by hand. strsignal() allocates on some libcs and this runs in
// a handler; and the number alone would make somebody count on their fingers.
const char *signal_name(int sig)
{
    switch (sig) {
    case SIGSEGV: return "SIGSEGV (a bad memory access)";
    case SIGBUS:  return "SIGBUS (a misaligned or unmapped access)";
    case SIGFPE:  return "SIGFPE (an arithmetic fault)";
    case SIGILL:  return "SIGILL (an illegal instruction)";
    case SIGABRT: return "SIGABRT (something called abort)";
    default:      return "a fatal signal";
    }
}

void on_fatal(int sig)
{
    write_text("\n=== ");
    write_text(signal_name(sig));
    write_text(": this process is going down ===\n");

    const int n = ::backtrace(g_frames, kMaxFrames);
    ::backtrace_symbols_fd(g_frames, n, STDERR_FILENO);
    write_text("=== end of stack ===\n");

    // Die of it. The default disposition back, then raise the same signal: a
    // handler that returned would fault again on the same instruction, and one
    // that called _exit would make the status say the process finished.
    struct sigaction dfl;
    std::memset(&dfl, 0, sizeof(dfl));
    dfl.sa_handler = SIG_DFL;
    ::sigaction(sig, &dfl, nullptr);
    ::raise(sig);
}

void nap(double seconds)
{
    timespec ts;
    ts.tv_sec  = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    ::nanosleep(&ts, nullptr);
}

// The watchdog is a THREAD that sleeps, and not a timer that signals.
//
// It was a repeating one-second SIGALRM, and that is why it had to change.
// SA_RESTART does not cover poll or select -- they return EINTR whatever the
// flag says -- so an alarm once a second quietly breaks any blocking wait the
// process is in. TODO.md OPEN-6 is a hang that every observer makes go away,
// and the watchdog built to catch it was one of the observers: 28 runs of the
// shaped scenario with it armed, none hung, against two hangs in five without.
// An instrument that suppresses the fault it was built to catch is not an
// instrument.
//
// A sleeping thread sends nothing. The one signal this ever delivers goes out
// AFTER it has already decided the loop is stuck, and it goes to the main
// thread by name: pthread_kill rather than raise, so the stack that gets
// written is the loop's and not whichever driver thread the kernel picked.
void *watchdog_main(void *)
{
    // This thread must never be the one that takes the dump.
    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGQUIT);
    ::pthread_sigmask(SIG_BLOCK, &block, nullptr);

    constexpr double kStep = 0.2;

    unsigned last  = g_beats.load(std::memory_order_relaxed);
    double   quiet = 0.0;

    while (g_watching.load(std::memory_order_relaxed)) {
        nap(kStep);

        const unsigned now = g_beats.load(std::memory_order_relaxed);
        if (now != last) {
            last  = now;
            quiet = 0.0;
            continue;
        }

        quiet += kStep;
        if (quiet < g_quiet_limit.load(std::memory_order_relaxed)) continue;

        g_for_watchdog.store(true, std::memory_order_relaxed);
        ::pthread_kill(g_main_thread, SIGQUIT);

        // The handler ends the process. Getting here means it did not: the
        // signal never arrived, or writing the stack did not finish. Give up
        // anyway, and say which of the two kinds of silence this was.
        nap(2.0);
        write_text("\nwatchdog: the main loop has not moved, and its stack could not be taken\n");
        ::_exit(134);
    }
    return nullptr;
}

} // namespace

void install_stack_dumper()
{
    if (g_installed) return;
    g_installed = true;

    // The first backtrace() in a process may load libgcc and allocate. Doing it
    // here means the handler never does.
    void *warm[4];
    (void)::backtrace(warm, 4);

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_quit;
    sa.sa_flags   = SA_RESTART;
    ::sigaction(SIGQUIT, &sa, nullptr);
}

void install_crash_handler()
{
    if (g_crash_installed) return;
    g_crash_installed = true;

    // Same warm-up as the dumper's, for the same reason: the first backtrace()
    // in a process may load libgcc and allocate.
    void *warm[4];
    (void)::backtrace(warm, 4);

    // A stack of its own, so a SIGSEGV caused by running OUT of stack still has
    // somewhere to run. Static rather than allocated: a handler installed
    // before main's first allocation should not need one to work.
    // 64 KB, a constant. SIGSTKSZ is not one on glibc any more -- it reads the
    // hardware's minimum at run time -- and a signal stack has to be sized at
    // compile time to live in .bss.
    static char alt[64 * 1024];
    stack_t     ss;
    std::memset(&ss, 0, sizeof(ss));
    ss.ss_sp    = alt;
    ss.ss_size  = sizeof(alt);
    ss.ss_flags = 0;
    ::sigaltstack(&ss, nullptr);

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_fatal;
    sa.sa_flags   = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    const int fatal[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT};
    for (const int sig : fatal) ::sigaction(sig, &sa, nullptr);
}

void watchdog_beat()
{
    g_beats.fetch_add(1, std::memory_order_relaxed);
}

void watchdog_start(double seconds)
{
    install_stack_dumper();

    if (seconds <= 0.0 || g_watching.load(std::memory_order_relaxed)) return;

    // Whoever arms the watchdog is the loop it is watching.
    g_main_thread = ::pthread_self();
    g_quiet_limit.store(seconds, std::memory_order_relaxed);
    g_watching.store(true, std::memory_order_relaxed);

    pthread_t thread;
    if (::pthread_create(&thread, nullptr, watchdog_main, nullptr) != 0) {
        g_watching.store(false, std::memory_order_relaxed);
        write_text("watchdog: no thread, so nothing is watching this loop\n");
        return;
    }
    ::pthread_detach(thread);
}

} // namespace cnp

#endif // _WIN32
