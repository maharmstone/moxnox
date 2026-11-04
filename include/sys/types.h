// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int64_t _off64_t;
typedef _off64_t off64_t;
typedef _off64_t off_t;

typedef uint64_t ino_t;

typedef uint32_t dev_t;

typedef uint16_t mode_t;

typedef int pid_t;

typedef long __time32_t;
typedef int64_t __time64_t;
typedef __time64_t time_t;

typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef int key_t;
typedef uint32_t id_t;
typedef uintptr_t pthread_t;
typedef struct { uintptr_t _x[4]; } pthread_mutex_t;
typedef size_t pthread_key_t;
typedef struct { uintptr_t _x[2]; } pthread_cond_t;
typedef uintptr_t pthread_condattr_t; // FIXME?
typedef uintptr_t pthread_mutexattr_t; // FIXME?
typedef uintptr_t pthread_attr_t;
typedef uintptr_t pthread_once_t;
typedef uintptr_t pthread_rwlock_t;
typedef uintptr_t pthread_rwlockattr_t; // FIXME?
typedef uintptr_t pthread_barrier_t; // FIXME?
typedef uint64_t fsblkcnt_t;
typedef uint64_t fsfilcnt_t;
typedef int64_t blksize_t;
typedef int64_t blkcnt_t;
typedef uint32_t nlink_t;
typedef long suseconds_t;
typedef uintptr_t timer_t;

#if __SIZEOF_POINTER__ == 8
typedef long long ssize_t;
#else
typedef int ssize_t;
#endif
