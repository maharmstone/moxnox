// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>
#include <sys/types.h>

typedef long __time32_t;
typedef long long __time64_t;
typedef __time64_t time_t;

// long on Windows
typedef long long clock_t;

// 1000 on Windows, but XSI requires 1000000
#define CLOCKS_PER_SEC ((clock_t)1000000)

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    // Windows doesn't have these last two fields
    long tm_gmtoff;
    const char* tm_zone;
};

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

typedef int clockid_t;

#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 2

#define TIME_UTC 1

#ifdef __cplusplus
extern "C" {
#endif

// don't confuse 32-bit bootloader by requiring it to handle DLL imports
#ifndef __i386__
extern __declspec(dllimport) int daylight;
extern __declspec(dllimport) long timezone;
extern __declspec(dllimport) char* tzname[2];
#endif

clock_t clock(void);
size_t strftime(char* s, size_t maxsize, const char* format,
                const struct tm* timeptr);
char* asctime(const struct tm* timeptr);
time_t time(time_t* timer);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm* timeptr);
struct tm* gmtime(const time_t* timer);
struct tm* localtime(const time_t* timer);
char* ctime(const time_t* timer);
void tzset(void);
struct tm* gmtime_r(const time_t* timer, struct tm* result);
struct tm* localtime_r(const time_t* timer, struct tm* result);
int clock_gettime(clockid_t clock_id, struct timespec* tp);
int clock_getres(clockid_t clock_id, struct timespec* res);
int nanosleep(const struct timespec* duration, struct timespec* rem);
int timespec_get(struct timespec* ts, int base);

#ifdef __cplusplus
}
#endif
