// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <algorithm>

#ifdef __x86_64__
#include <x86intrin.h>
#endif

import nt;
import ntdll;
import timepage;

#ifdef __aarch64__
import aarch64;
#endif

using namespace std;

static RTL_SRWLOCK alarm_lock;
static HANDLE alarm_timer;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

extern "C"
NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    do {
        auto t1 = (uint64_t)time_page.system_time + time_page.system_time_offset;
        uint64_t r, delta;

#ifdef __x86_64__
        r = __rdtsc();
#elifdef __aarch64__
        r = cntvct_el0_load();
#endif

        r -= time_page.system_time_tsc;

        delta = r * SYSTEM_TIME_RESOLUTION / time_page.tsc_freq;
        delta = min(time_page.tick_rate - 1, delta);

        auto t2 = (uint64_t)time_page.system_time + time_page.system_time_offset;

        if (t1 == t2) {
            SystemTime->QuadPart = (int64_t)(t1 + delta);
            return nt_status::SUCCESS;
        }
    } while (true);
}

extern "C"
bool RtlQueryPerformanceFrequency(LARGE_INTEGER* freq) {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    freq->QuadPart = (int64_t)time_page.tsc_freq;

    return true;
}

extern "C"
bool RtlQueryPerformanceCounter(LARGE_INTEGER* count) {
#ifdef __x86_64__
    count->QuadPart = (int64_t)__rdtsc();
#elifdef __aarch64__
    count->QuadPart = (int64_t)cntvct_el0_load();
#endif

    return true;
}

extern "C"
nt_status mn_alarm(uint64_t time, uint64_t* left) {
    nt_status status;
    LARGE_INTEGER due_time;

    RtlAcquireSRWLockExclusive(&alarm_lock);

    if (alarm_timer) {
        TIMER_BASIC_INFORMATION tbi;

        status = NtQueryTimer(alarm_timer, TIMER_INFORMATION_CLASS::BasicInformation,
                              &tbi, sizeof(tbi), nullptr);
        if (!success(status)) {
            RtlReleaseSRWLockExclusive(&alarm_lock);
            return status;
        }

        *left = (uint64_t)tbi.RemainingTime.QuadPart;

        if (time == 0) {
            NtCancelTimer(alarm_timer, nullptr);
            RtlReleaseSRWLockExclusive(&alarm_lock);

            return nt_status::SUCCESS;
        }
    } else {
        *left = 0;

        if (time == 0) {
            RtlReleaseSRWLockExclusive(&alarm_lock);
            return nt_status::SUCCESS;
        }

        status = NtCreateTimer(&alarm_timer, TIMER_QUERY_STATE | TIMER_MODIFY_STATE,
                               nullptr, TIMER_TYPE::NotificationTimer);
        if (!success(status)) {
            RtlReleaseSRWLockExclusive(&alarm_lock);
            return status;
        }

        status = NtSetInformationProcess(NtCurrentProcess(), PROCESS_INFORMATION_CLASS::MoxnoxAlarmTimer,
                                         &alarm_timer, sizeof(HANDLE));
        if (!success(status)) {
            NtClose(alarm_timer);
            alarm_timer = 0;

            RtlReleaseSRWLockExclusive(&alarm_lock);

            return status;
        }
    }

    due_time.QuadPart = -(int64_t)time;

    status = NtSetTimer(alarm_timer, &due_time, nullptr, nullptr, false, 0,
                        nullptr);
    if (!success(status)) {
        RtlReleaseSRWLockExclusive(&alarm_lock);
        return status;
    }

    RtlReleaseSRWLockExclusive(&alarm_lock);

    return nt_status::SUCCESS;
}

void clear_alarm_on_fork() {
    // deliberately not taking alarm_lock

    if (alarm_timer) {
        NtClose(alarm_timer);
        alarm_timer = 0;
    }
}

extern "C"
uint64_t mn_get_alarm_left() {
    uint64_t ret;

    RtlAcquireSRWLockShared(&alarm_lock);

    if (!alarm_timer)
        ret = 0;
    else {
        TIMER_BASIC_INFORMATION tbi;

        auto status = NtQueryTimer(alarm_timer, TIMER_INFORMATION_CLASS::BasicInformation,
                                   &tbi, sizeof(tbi), nullptr);
        if (!success(status))
            ret = 0;
        else
            ret = (uint64_t)tbi.RemainingTime.QuadPart;
    }

    RtlReleaseSRWLockShared(&alarm_lock);

    return ret;
}
