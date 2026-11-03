// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

import nt;
import ntdll;
import ucrtbase;

#undef errno
import errno;

extern "C"
int nanosleep(const timespec* duration, timespec* rem) {
    nt_status status;
    int64_t units;
    LARGE_INTEGER t, start, freq;

    if (!duration || duration->tv_sec < 0 || duration->tv_nsec >= 1000000000) {
        *_errno() = EINVAL;
        return -1;
    }

    // convert to units to 100ns

    // FIXME - use std::saturating_mul and std::saturating_add once we move to LLVM 23
    if (__builtin_mul_overflow(duration->tv_sec, 10000000, &units))
        units = INT64_MAX;

    if (__builtin_add_overflow(units, duration->tv_nsec / 100, &units))
        units = INT64_MAX;

    if (duration->tv_nsec % 100) {
        if (__builtin_add_overflow(units, 1, &units))
            units = INT64_MAX;
    }

    t.QuadPart = -units;

    RtlQueryPerformanceCounter(&start);
    RtlQueryPerformanceFrequency(&freq);

    auto f = (uint64_t)freq.QuadPart;

    while (true) {
        auto sig_count = mn_handled_signal_count();

        status = NtDelayExecution(true, &t);

        if (status == nt_status::USER_APC && mn_handled_signal_count() == sig_count) {
            LARGE_INTEGER now;

            RtlQueryPerformanceCounter(&now);

            auto elapsed_ticks = (uint64_t)(now.QuadPart - start.QuadPart);
            auto elapsed = ((elapsed_ticks / f) * 10000000) + (((elapsed_ticks % f) * 10000000) / f);

            if ((uint64_t)units <= elapsed)
                return 0;

            t.QuadPart = -(int64_t)(units - elapsed);

            continue;
        }

        break;
    }

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (status == nt_status::USER_APC) {
        if (rem) {
            LARGE_INTEGER now;
            uint64_t elapsed_sec;
            long elapsed_nsec;

            RtlQueryPerformanceCounter(&now);

            auto elapsed_ticks = (uint64_t)(now.QuadPart - start.QuadPart);

            elapsed_sec = elapsed_ticks / f;
            elapsed_nsec = (long)(((elapsed_ticks % f) * 1000000000) / f);

            if ((time_t)elapsed_sec > duration->tv_sec || ((time_t)elapsed_sec == duration->tv_sec && elapsed_nsec >= duration->tv_nsec)) {
                rem->tv_sec = 0;
                rem->tv_nsec = 0;
            } else if (duration->tv_nsec < elapsed_nsec) {
                rem->tv_sec = duration->tv_sec - elapsed_sec - 1;
                rem->tv_nsec = 1000000000 + duration->tv_nsec - elapsed_nsec;
            } else {
                rem->tv_sec = duration->tv_sec - elapsed_sec;
                rem->tv_nsec = duration->tv_nsec - elapsed_nsec;
            }
        }

        *_errno() = EINTR;
        return -1;
    }

    return 0;
}

extern "C"
unsigned int sleep(unsigned int seconds) {
    timespec t, rem;

    t.tv_sec = seconds;
    t.tv_nsec = 0;

    rem.tv_sec = 0;
    rem.tv_nsec = 0;

    nanosleep(&t, &rem);

    if (rem.tv_nsec > 0)
        return (unsigned int)rem.tv_sec + 1;
    else
        return (unsigned int)rem.tv_sec;
}
