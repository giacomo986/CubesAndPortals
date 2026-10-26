#pragma once

// Getting a stack out of a process that is not answering.
//
// Written for TODO.md OPEN-6: the graphical client stops making progress about
// once in fifty runs on loopback and two times in five through a shaped link,
// and neither of the two obvious ways to look at it works here. Attaching after
// the fact is refused -- /proc/sys/kernel/yama/ptrace_scope -- and running it
// under a debugger from the start makes it stop happening, six runs for six.
//
// So the process observes itself. A signal handler that writes its own stack
// costs nothing while nothing is wrong, and does not move the timing the way an
// attached debugger does.
//
// Two ways in:
//
//   - **SIGQUIT**, for a hang somebody is watching: `timeout -s QUIT 30 ...`,
//     or `kill -QUIT <pid>`. The stack goes to stderr and the process carries
//     on, so this is also how to look at a slow process without ending it.
//   - **A watchdog**, for a hang nobody is watching: the main loop says it is
//     alive once a frame, and if it stops saying so the stack is written and
//     the process aborts. That turns a test that hangs until ctest kills it
//     into a test that fails with the reason attached.
//
// The watchdog is a THREAD THAT SLEEPS and not a timer that signals, and that
// is not an implementation detail. It was a repeating one-second SIGALRM, and
// SA_RESTART does not cover poll or select -- they return EINTR whatever the
// flag says -- so an alarm once a second quietly breaks any blocking wait the
// process is in. The hang this was built for is one that every observer makes
// go away, and with the alarm armed it went away too: 28 shaped runs, none
// hung, against two hangs in five without. So the watchdog now delivers exactly
// one signal, after it has decided the loop is stuck, and it delivers it to the
// main thread BY NAME -- the stack worth having is the loop's, not whichever
// driver thread the kernel happened to pick.
//
// What it does NOT give: line numbers, or the frames of any thread but the one
// the signal happened to land on. `backtrace_symbols_fd` prints what the
// dynamic symbol table holds, which is why the binaries that use this are
// linked with -rdynamic. Addresses can be turned into lines afterwards with
// addr2line; doing it in a signal handler would mean allocating.

namespace cnp {

// SIGQUIT writes this process's stack to stderr and returns. Safe to call more
// than once. Warms up the unwinder so the handler itself never allocates.
void install_stack_dumper();

// The signals a process DIES of: SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT.
//
// A different job from the one above, and the difference is the whole design.
// SIGQUIT is for a process that is stuck and that somebody is watching: the
// stack is written and the process CARRIES ON. These are for a process that is
// already gone, and the handler has three things to do -- name the signal,
// write the stack, and then let the process die of it.
//
// The third is the one a first version gets wrong. Returning from a SIGSEGV
// handler hands control back to the instruction that faulted, which faults
// again, for ever; and catching the signal and exiting cleanly makes the exit
// status say the process finished, which is a lie that costs somebody an
// afternoon. So the handler puts the default disposition back and re-raises,
// and the shell still reports 139 for a segfault.
//
// On its own stack (sigaltstack), because the most common way to get a SIGSEGV
// is to run out of stack -- and a handler that needs stack to run is not there
// for exactly the crash it was written for.
void install_crash_handler();

// The main loop is alive. Cheap enough to call every frame: one store.
void watchdog_beat();

// Starts the watchdog. If watchdog_beat() is not called for `seconds`, the
// stack is written to stderr and the process aborts. Zero or less disables it,
// which is what every ordinary run does.
void watchdog_start(double seconds);

} // namespace cnp
