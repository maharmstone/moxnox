// SPDX-License-Identifier: MIT
#pragma once

/* Prevent winsock2.h from defining its own array-based fd_set. */
#ifndef _SYS_TYPES_FD_SET
#define _SYS_TYPES_FD_SET
#endif
#ifndef USE_SYS_TYPES_FD_SET
#define USE_SYS_TYPES_FD_SET
#endif

#include <string.h>
#include <signal.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

#undef FD_SET
#undef FD_CLR
#undef FD_ISSET
#undef FD_ZERO

#define FD_SET(d, s)   ((s)->fds_bits[(d) / (8*sizeof(unsigned long))] |= (1UL << ((d) % (8*sizeof(unsigned long)))))
#define FD_CLR(d, s)   ((s)->fds_bits[(d) / (8*sizeof(unsigned long))] &= ~(1UL << ((d) % (8*sizeof(unsigned long)))))
#define FD_ISSET(d, s) (!!((s)->fds_bits[(d) / (8*sizeof(unsigned long))] & (1UL << ((d) % (8*sizeof(unsigned long))))))
#define FD_ZERO(s)     (memset((s), 0, sizeof(fd_set)))

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           const struct timeval *timeout);
int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds,
            const struct timespec* timeout, const sigset_t* sigmask);

#ifdef __cplusplus
}
#endif
