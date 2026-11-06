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
