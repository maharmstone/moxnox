#include <time.h>
#include <signal.h>
#include <string.h>

import nt;
import ucrtbase;
import errno;
import unique_handle;

struct posix_timer {
    posix_timer(HANDLE h, clockid_t clock) : h(h), clock(clock) { }

    unique_handle h;
    clockid_t clock;
    sigevent ev;
    unsigned int period = 0;
};

extern "C"
int timer_create(clockid_t clockid, sigevent* evp, timer_t* timerid) {
    HANDLE h;

    if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC) {
        *_errno() = EINVAL;
        return -1;
    }

    if (evp && evp->sigev_notify != SIGEV_NONE) {
        if (evp->sigev_notify != SIGEV_SIGNAL && evp->sigev_notify != SIGEV_THREAD) {
            *_errno() = EINVAL;
            return -1;
        }

        if (evp->sigev_notify == SIGEV_SIGNAL && (evp->sigev_signo <= 0 || evp->sigev_signo >= NSIG)) {
            *_errno() = EINVAL;
            return -1;
        }
    }

    auto status = NtCreateTimer(&h, TIMER_QUERY_STATE | TIMER_MODIFY_STATE,
                                nullptr, TIMER_TYPE::NotificationTimer);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    auto t = new posix_timer(h, clockid);
    if (!t) {
        NtClose(h);
        *_errno() = ENOMEM;
        return -1;
    }

    if (evp)
        memcpy(&t->ev, evp, sizeof(sigevent));
    else {
        t->ev.sigev_notify = SIGEV_SIGNAL;
        t->ev.sigev_signo = SIGALRM;
        t->ev.sigev_value.sival_ptr = t;
    }

    *timerid = (uintptr_t)t;

    return 0;
}

extern "C"
int timer_delete(timer_t timerid) {
    auto& t = *(posix_timer*)timerid;
    bool current_state;

    NtCancelTimer(t.h.get(), &current_state);

    delete &t;

    return 0;
}

static void timer_apc(void* context, uint32_t /*low_value*/,
                      int32_t /*high_value*/) {
    auto& t = *(posix_timer*)context;

    switch (t.ev.sigev_notify) {
        case SIGEV_NONE:
            break;

        case SIGEV_SIGNAL:
            // FIXME - raise signal
            break;

        case SIGEV_THREAD:
            // FIXME - create thread
            break;
    }
}

extern "C"
int timer_settime(timer_t timerid, int flags, const itimerspec* value,
                  itimerspec* ovalue) {
    nt_status status;
    LARGE_INTEGER due_time;
    auto& t = *(posix_timer*)timerid;
    unsigned int period;

    if (flags & ~TIMER_ABSTIME || !value ||
        value->it_value.tv_nsec < 0 || value->it_value.tv_nsec >= 1000000000 ||
        value->it_interval.tv_nsec < 0 || value->it_interval.tv_nsec >= 1000000000 ||
        value->it_interval.tv_sec < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (ovalue) {
        TIMER_BASIC_INFORMATION tbi;

        status = NtQueryTimer(t.h.get(), TIMER_INFORMATION_CLASS::BasicInformation,
                              &tbi, sizeof(tbi), nullptr);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        if (!tbi.TimerState) {
            auto r = (uint64_t)tbi.RemainingTime.QuadPart;

            ovalue->it_value.tv_sec = r / 10000000;
            ovalue->it_value.tv_nsec = (long)((r % 10000000) * 100);
        } else {
            ovalue->it_value.tv_sec = 0;
            ovalue->it_value.tv_nsec = 0;
        }

        ovalue->it_interval.tv_sec = t.period / 1000;
        ovalue->it_interval.tv_nsec = (long)((t.period % 1000) * 1000000);
    }

    auto& v = value->it_value;
    auto& interval = value->it_interval;

    if (interval.tv_sec == 0 && interval.tv_nsec == 0)
        period = 0;
    else {
        period = (unsigned int)(interval.tv_sec * 1000);
        period += interval.tv_nsec / 1000000;

        if (interval.tv_nsec % 1000000)
            period++;
    }

    if (v.tv_sec == 0 && v.tv_nsec == 0) {
        bool current_state;

        status = NtCancelTimer(t.h.get(), &current_state);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        t.period = period;

        return 0;
    }

    if (flags & TIMER_ABSTIME) {
        timespec tp;

        // FIXME - what if CLOCK_REALTIME and time changes?

        if (clock_gettime(t.clock, &tp))
            return -1;

        if (v.tv_sec < tp.tv_sec || (v.tv_sec == tp.tv_sec && v.tv_nsec < tp.tv_nsec))
            due_time.QuadPart = -1; // already passed, expire immediately
        else {
            int64_t n;

            n = (v.tv_sec - tp.tv_sec) * 10000000;
            n += (v.tv_nsec - tp.tv_nsec) / 100;

            if ((v.tv_nsec - tp.tv_nsec) % 100)
                n++;

            due_time.QuadPart = -n;
        }
    } else {
        int64_t n;

        if (v.tv_sec < 0) {
            *_errno() = EINVAL;
            return -1;
        }

        n = v.tv_sec * 10000000;
        n += v.tv_nsec / 100;

        if (v.tv_nsec % 100)
            n++;

        due_time.QuadPart = -n;
    }

    // FIXME - APC should go to another thread in process if this one has died

    status = NtSetTimer(t.h.get(), &due_time, timer_apc, &t, false, period,
                        nullptr);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    t.period = period;

    return 0;
}
