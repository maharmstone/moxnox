// SPDX-License-Identifier: LGPL-2.1-or-later
#include <poll.h>
#include <sys/select.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <limits>

import nt;
import ucrtbase;
import print;

#undef errno
import errno;

using namespace std;

extern "C"
int ppoll(pollfd* fds, nfds_t nfds, const timespec* timeout,
          const sigset_t* sigmask) {
    nt_status status;
    MN_POLL p;
    mn_poll_entry* entries = nullptr;
    sigset_t oldset;

    if (sigmask) {
        uint32_t retlen;

        p.sigmask = *sigmask;
        p.sigmask_set = true;

        status = NtQueryInformationThread(NtCurrentThread(),
                                          THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                          &oldset, sizeof(oldset), &retlen);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }
    } else
        p.sigmask_set = false;

    if (!timeout)
        p.timeout = numeric_limits<uint64_t>::max(); // wait forever
    else {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000) {
            *_errno() = EINVAL;
            return -1;
        }

        // convert to units of 100ns
        p.timeout = (timeout->tv_sec * 10000000) + (timeout->tv_nsec / 100);

        if (timeout->tv_nsec % 100)
            p.timeout++;
    }

    if (nfds > 0) {
        entries = (mn_poll_entry*)malloc(sizeof(mn_poll_entry) * nfds);
        if (!entries) {
            *_errno() = ENOMEM;
            return -1;
        }

        for (size_t i = 0; i < nfds; i++) {
            auto e = *_errno();

            if (fds[i].fd < 0) {
                entries[i].h = 0;
                entries[i].revents = 0;
            } else {
                auto h = (HANDLE)_get_osfhandle((int)fds[i].fd);

                if ((intptr_t)h == -1) {
                    entries[i].h = 0;
                    entries[i].revents = POLLNVAL;
                    p.timeout = 0;

                    *_errno() = e;
                } else {
                    entries[i].h = h;
                    entries[i].revents = 0;
                }
            }

            entries[i].events = (uint16_t)fds[i].events;
        }
    }

    p.nfds = nfds;
    p.fds = entries;

    while (true) {
        auto sig_count = mn_handled_signal_count();

        status = NtSetInformationThread(NtCurrentThread(),
                                        THREAD_INFORMATION_CLASS::MoxnoxPoll,
                                        &p, sizeof(p));

        if (status == nt_status::USER_APC && mn_handled_signal_count() == sig_count)
            continue;

        if (status == nt_status::ALERTED)
            continue;

        break;
    }

    if (sigmask) {
        auto status2 = NtSetInformationThread(NtCurrentThread(),
                                              THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                              &oldset, sizeof(oldset));
        if (!success(status2) && success(status))
            status = status2;
    }

    if (status == nt_status::USER_APC) {
        *_errno() = EINTR;

        if (entries)
            free(entries);

        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);

        if (entries)
            free(entries);

        return -1;
    }

    int ret = 0;
    for (size_t i = 0; i < nfds; i++) {
        fds[i].revents = (int16_t)entries[i].revents;

        if (fds[i].revents != 0)
            ret++;
    }

    if (entries)
        free(entries);

    return ret;
}

extern "C"
int poll(pollfd* fds, nfds_t nfds, int timeout) {
    if (timeout < 0)
        return ppoll(fds, nfds, nullptr, nullptr);
    else {
        timespec tm;

        tm.tv_sec = timeout / 1000;
        tm.tv_nsec = (timeout % 1000) * 1000000;

        return ppoll(fds, nfds, &tm, nullptr);
    }
}

extern "C"
int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds,
            const timespec* timeout, const sigset_t* sigmask) {
    pollfd* pfds = nullptr;
    int npfds = 0;

    if (nfds > 0) {
        pfds = (pollfd*)malloc(sizeof(pollfd) * (size_t)nfds);
        if (!pfds) {
            *_errno() = ENOMEM;
            return -1;
        }

        for (int i = 0; i < nfds; i++) {
            int16_t events = 0;

            if (readfds && FD_ISSET((size_t)i, readfds))
                events |= POLLIN;

            if (writefds && FD_ISSET((size_t)i, writefds))
                events |= POLLOUT;

            if (errorfds && FD_ISSET((size_t)i, errorfds))
                events |= POLLPRI;

            if (events != 0) {
                pfds[npfds].fd = i;
                pfds[npfds].events = events;
                pfds[npfds].revents = 0;
                npfds++;
            }
        }
    }

    auto r = ppoll(pfds, (nfds_t)npfds, timeout, sigmask);

    for (int i = 0; i < npfds; i++) {
        if (pfds[i].revents & POLLNVAL) {
            free(pfds);
            *_errno() = EBADF;
            return -1;
        }
    }

    if (r >= 0) {
        if (readfds)
            FD_ZERO(readfds);

        if (writefds)
            FD_ZERO(writefds);

        if (errorfds)
            FD_ZERO(errorfds);
    }

    if (r > 0) {
        r = 0;

        for (int i = 0; i < npfds; i++) {
            auto rev = pfds[i].revents;

            if (rev == 0)
                continue;

            auto fd = pfds[i].fd;

            if (readfds && (pfds[i].events & POLLIN) && (rev & (POLLIN | POLLHUP | POLLERR))) {
                FD_SET((size_t)fd, readfds);
                r++;
            }

            if (writefds && (pfds[i].events & POLLOUT) && (rev & (POLLOUT | POLLERR))) {
                FD_SET((size_t)fd, writefds);
                r++;
            }

            if (errorfds && (pfds[i].events & POLLPRI) && (rev & POLLPRI)) {
                FD_SET((size_t)fd, errorfds);
                r++;
            }
        }
    }

    if (pfds)
        free(pfds);

    return r;
}

extern "C"
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           const timeval* timeout) {
    timespec ts;

    if (timeout) {
        ts.tv_sec = timeout->tv_sec;
        ts.tv_nsec = timeout->tv_usec * 1000;
    }

    return pselect(nfds, readfds, writefds, exceptfds, timeout ? &ts : nullptr,
                   nullptr);
}
