// A wall clock that is wrong, for one process.
//
// M6's exit test says "two servers on different machines". Everything measured
// so far runs on one, so the two servers share a clock -- and a signed ticket
// is the one thing in this engine that rests entirely on two clocks agreeing.
// docs/archive/M6_FINDINGS.md 28 gated the POLICY by signing tickets with hand-chosen
// dates; what it could not do is have the two processes actually read different
// times, through the code path they really use.
//
// Neither a container nor a VM is needed for that. Linux time namespaces do not
// cover CLOCK_REALTIME at all, so a container cannot do it; a VM can, at the
// price of a kernel. What is actually wanted is much smaller: make ONE process
// read a wall clock that is off by a known amount, on the same call the server
// makes.
//
// So: LD_PRELOAD, and three symbols.
//
//   CNP_CLOCK_SKEW=<signed seconds>
//
// What this deliberately does NOT move is every monotonic clock. Ticks, the
// round-trip estimate, the heartbeat windows and the backoff all run on
// CLOCK_MONOTONIC, and shifting that would not be a wrong wall clock -- it
// would be a machine that experiences time differently, which is a different
// claim and not one anybody makes about NTP drift.
//
// Limits, stated because a gate built on this must not claim more than it has:
// this is the C library's answer, not the kernel's. A caller that reads the
// clock some other way -- a raw syscall, the vDSO by hand, a statically linked
// binary -- sees the true time and this shim never knows. Our server calls
// std::time(nullptr) through the PLT, which is exactly what is interposed here,
// and tests/two_clocks.sh checks that the interposition took rather than
// assuming it.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

static long skew_seconds(void)
{
    // Read once. An engine calls this a great many times a second, and a getenv
    // per call would make the shim itself the thing being measured.
    static long  cached = 0;
    static int   read   = 0;
    if (!read) {
        const char *s = getenv("CNP_CLOCK_SKEW");
        cached        = s ? strtol(s, NULL, 10) : 0;
        read          = 1;
    }
    return cached;
}

time_t time(time_t *out)
{
    static time_t (*real)(time_t *) = NULL;
    if (!real) real = (time_t (*)(time_t *))dlsym(RTLD_NEXT, "time");

    const time_t t = real(NULL) + (time_t)skew_seconds();
    if (out) *out = t;
    return t;
}

int clock_gettime(clockid_t id, struct timespec *ts)
{
    static int (*real)(clockid_t, struct timespec *) = NULL;
    if (!real) real = (int (*)(clockid_t, struct timespec *))dlsym(RTLD_NEXT, "clock_gettime");

    const int rc = real(id, ts);

    // Only the clock that claims to be the time of day. CLOCK_MONOTONIC and
    // friends pass through untouched, on purpose -- see the header comment.
    if (rc == 0 && (id == CLOCK_REALTIME || id == CLOCK_REALTIME_COARSE))
        ts->tv_sec += (time_t)skew_seconds();

    return rc;
}

int gettimeofday(struct timeval *tv, void *tz)
{
    static int (*real)(struct timeval *, void *) = NULL;
    if (!real) real = (int (*)(struct timeval *, void *))dlsym(RTLD_NEXT, "gettimeofday");

    // No NULL check on tv: the declaration says it is nonnull, and glibc warns
    // about testing it. A caller that passes NULL has already lost.
    const int rc = real(tv, tz);
    if (rc == 0) tv->tv_sec += (time_t)skew_seconds();
    return rc;
}
