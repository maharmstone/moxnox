// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>
#include <sys/types.h>

typedef uint64_t sigset_t;
typedef int sig_atomic_t;

#define SA_NOCLDSTOP    0x00000001
#define SA_NOCLDWAIT    0x00000002
#define SA_SIGINFO      0x00000004
#define SA_ONSTACK      0x08000000
#define SA_RESTART      0x10000000
#define SA_NODEFER      0x40000000
#define SA_RESETHAND    0x80000000

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

union sigval {
    int sival_int;
    void* sival_ptr;
};

typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    pid_t si_pid;
    uid_t si_uid;
    int si_status;
    union sigval si_value;
    void* si_addr;
    long si_band;
} siginfo_t;

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t*, void*);
    };
    sigset_t sa_mask;
    int sa_flags;
};

struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void (*sigev_notify_function)(union sigval);
    pthread_attr_t* sigev_notify_attributes;
};

#define SIGEV_NONE 0
#define SIGEV_SIGNAL 1
#define SIGEV_THREAD 2

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGIOT 6
#define SIGEMT 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGBUS 10
#define SIGSEGV 11
#define SIGSYS 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGPOLL 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGBREAK 21
#define SIGABRT 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGUSR1 29
#define SIGUSR2 30
#define SIGTTIN 31
#define SIGTTOU 32
#define NSIG (SIGTTOU + 1)

typedef void (*__sighandler_t)(int sig);

#define SIG_ERR (__sighandler_t)-1
#define SIG_DFL (__sighandler_t)0
#define SIG_IGN (__sighandler_t)1
#define SIG_HOLD (__sighandler_t)2

#ifdef __cplusplus
extern "C" {
#endif

int kill(pid_t pid, int sig);
int killpg(pid_t pgrp, int sig);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int signum);
int sigdelset(sigset_t* set, int signum);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
int sigsuspend(const sigset_t* mask);
__sighandler_t signal(int sig, __sighandler_t func);
int raise(int sig);
int sigismember(const sigset_t* set, int signo);
int pthread_sigmask(int how, const sigset_t* set, sigset_t* oset);
int pthread_kill(pthread_t thread, int sig);
int sigwait(const sigset_t* set, int* sig);
int sigpending(sigset_t* set);

#ifdef __cplusplus
}
#endif
