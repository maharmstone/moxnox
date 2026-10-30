// SPDX-License-Identifier: LGPL-2.1-or-later
#include <sys/time.h>
#include <stdint.h>

#undef errno
import errno;

import nt;
import ntdll;
import ucrtbase;

extern "C"
int _gettimeofday(struct timeval* tv, struct timezone* /*tz*/) {
    LARGE_INTEGER li;

    // FIXME - tz generally ignored?

    if (!tv) {
        *_errno() = EFAULT;
        return -1;
    }

    auto status = NtQuerySystemTime(&li);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    auto t = li.QuadPart; // units of 100ns

    t -= 116444736000000000; // change from NT to Unix epoch
    t /= 10; // change to microseconds

    tv->tv_sec = (long)(t / 1000000);
    tv->tv_usec = (long)(t % 1000000);

    return 0;
}

static int clock_gettime_monotonic(timespec& tp) {
    LARGE_INTEGER count, freq;

    if (!RtlQueryPerformanceCounter(&count)) {
        *_errno() = EINVAL;
        return -1;
    }

    if (!RtlQueryPerformanceFrequency(&freq)) {
        *_errno() = EINVAL;
        return -1;
    }

    tp.tv_sec = count.QuadPart / freq.QuadPart;

    auto frac = (uint64_t)(count.QuadPart % freq.QuadPart);

    tp.tv_nsec = (long)((frac * 1000000000) / (uint64_t)freq.QuadPart);

    return 0;
}

static int clock_gettime_realtime(timespec& tp) {
    LARGE_INTEGER li;

    auto status = NtQuerySystemTime(&li);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    auto t = (uint64_t)li.QuadPart; // units of 100ns

    t -= 116444736000000000; // change from NT to Unix epoch

    tp.tv_sec = t / 10000000;
    tp.tv_nsec = (long)((t % 10000000) * 100);

    return 0;
}

extern "C"
int clock_gettime(clockid_t clockid, timespec* tp) {
    switch (clockid) {
        case CLOCK_MONOTONIC:
            return clock_gettime_monotonic(*tp);

        case CLOCK_REALTIME:
            return clock_gettime_realtime(*tp);

        // FIXME - CLOCK_PROCESS_CPUTIME_ID
        // FIXME - CLOCK_THREAD_CPUTIME_ID

        default:
            *_errno() = EINVAL;
            return -1;
    }
}

static int clock_getres_monotonic(timespec& res) {
    LARGE_INTEGER freq;

    if (!RtlQueryPerformanceFrequency(&freq)) {
        *_errno() = EINVAL;
        return -1;
    }

    auto val = (uint64_t)(1000000000 / freq.QuadPart);

    res.tv_sec = val / 1000000000;
    res.tv_nsec = (long)(val % 1000000000);

    return 0;
}

static int clock_getres_realtime(timespec& res) {
    // FIXME - derive frm kernel TICK_RATE

    res.tv_sec = 0;
    res.tv_nsec = 1000000; // 1 ms

    return 0;
}

extern "C"
int clock_getres(clockid_t clockid, timespec* res) {
    if (!res) { // surprisingly valid
        switch (clockid) {
            case CLOCK_MONOTONIC:
            case CLOCK_REALTIME:
                return 0;

            default:
                *_errno() = EINVAL;
                return -1;
        }
    }

    switch (clockid) {
        case CLOCK_MONOTONIC:
            return clock_getres_monotonic(*res);

        case CLOCK_REALTIME:
            return clock_getres_realtime(*res);

        default:
            *_errno() = EINVAL;
            return -1;
    }
}

extern "C"
struct tm* gmtime_r(const time_t* t, struct tm* tm) {
    auto ret = _gmtime64_s(tm, t);

    if (ret) {
        *_errno() = ret;
        return nullptr;
    }

    return tm;
}

extern "C"
struct tm* localtime_r(const time_t* t, struct tm* tm) {
    auto ret = _localtime64_s(tm, t);

    if (ret) {
        *_errno() = ret;
        return nullptr;
    }

    return tm;
}

extern "C"
unsigned int alarm(unsigned int seconds) {
    uint64_t time, left;

    time = (uint64_t)seconds * 10000000;

    auto status = mn_alarm(time, &left);
    if (!success(status))
        return 0;

    return (unsigned int)((left + 9999999) / 10000000);
}
