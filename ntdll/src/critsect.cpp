// SPDX-License-Identifier: LGPL-2.1-or-later
#include <atomic>

import nt;
import print;

using namespace std;

extern "C"
nt_status RtlInitializeCriticalSection(RTL_CRITICAL_SECTION* critical_section) {
    auto& cs = *critical_section;

    cs.DebugInfo = nullptr;
    cs.LockCount = -1;
    cs.RecursionCount = 0;
    cs.OwningThread = 0;
    cs.LockSemaphore = 0;
    cs.SpinCount = 4000; // FIXME - 0 if single-CPU machine

    return nt_status::SUCCESS;
}

extern "C"
nt_status RtlDeleteCriticalSection(RTL_CRITICAL_SECTION* critical_section) {
    auto& cs = *critical_section;

    cs.LockCount = -1;
    cs.RecursionCount = 0;
    cs.OwningThread = 0;

    if (cs.LockSemaphore != 0) {
        auto status = NtClose(cs.LockSemaphore);
        if (!success(status)) {
            print_stderr("NtClose returned {}\n", status);
            return status;
        }

        cs.LockSemaphore = 0;
    }

    return nt_status::SUCCESS;
}

extern "C"
nt_status RtlEnterCriticalSection(RTL_CRITICAL_SECTION* critical_section) {
    nt_status status;
    auto& cs = *critical_section;

    atomic_ref<int32_t> lock_count(cs.LockCount);

    auto this_thread = (HANDLE)(NtCurrentTeb()->ClientId.UniqueThread);

    // fast path
    int32_t expected = -1;
    if (lock_count.compare_exchange_strong(expected, 0, memory_order_acquire,
                                           memory_order_relaxed)) {
        cs.OwningThread = this_thread;
        cs.RecursionCount = 1;
        return nt_status::SUCCESS;
    }

    // recursive
    if (cs.OwningThread == this_thread) {
        lock_count.fetch_add(1, memory_order_relaxed);
        cs.RecursionCount++;
        return nt_status::SUCCESS;
    }

    // spin
    for (uintptr_t i = 0; i < cs.SpinCount; i++) {
        if (lock_count.load(memory_order_relaxed) == -1) {
            expected = -1;

            if (lock_count.compare_exchange_weak(expected, 0,
                                                 memory_order_acquire,
                                                 memory_order_relaxed)) {
                cs.OwningThread = this_thread;
                cs.RecursionCount = 1;
                return nt_status::SUCCESS;
            }
        }

#ifdef __x86_64__
        __asm __volatile ("pause");
#endif
    }

    // slow path

    if (!cs.LockSemaphore) {
        HANDLE h;

        // FIXME - access mask?
        auto status = NtCreateEvent(&h, 0, nullptr,
                                    EVENT_TYPE::SynchronizationEvent, false);
        if (!success(status)) {
            print_stderr("NtCreateEvent returned {}\n", status);
            return status;
        }

        atomic_ref<HANDLE> event_handle(cs.LockSemaphore);
        HANDLE expected_zero = 0;

        if (!event_handle.compare_exchange_strong(expected_zero, h,
                                                  memory_order_release,
                                                  memory_order_relaxed)) {
            NtClose(h);
        }
    }

    if (lock_count.fetch_add(1, memory_order_acquire) == -1) {
        cs.OwningThread = this_thread;
        cs.RecursionCount = 1;
        return nt_status::SUCCESS;
    }

    while (true) {
        status = NtWaitForSingleObject(cs.LockSemaphore, false, nullptr);
        if (status == nt_status::USER_APC)
            continue;

        break;
    }

    if (!success(status)) {
        print_stderr("NtWaitForSingleObject returned {}\n", status);
        return status;
    }

    cs.OwningThread = this_thread;
    cs.RecursionCount = 1;
    return nt_status::SUCCESS;
}

extern "C"
nt_status RtlLeaveCriticalSection(RTL_CRITICAL_SECTION* critical_section) {
    auto& cs = *critical_section;

    atomic_ref<int32_t> lock_count(cs.LockCount);

    cs.RecursionCount--;
    if (cs.RecursionCount > 0) {
        lock_count.fetch_sub(1, memory_order_relaxed);
        return nt_status::SUCCESS;
    }

    cs.OwningThread = 0;

    if (lock_count.fetch_sub(1, memory_order_release) > 0)
        return NtSetEvent(cs.LockSemaphore, nullptr);

    return nt_status::SUCCESS;
}

extern "C"
void RtlAcquirePebLock() {
    RtlEnterCriticalSection(NtCurrentTeb()->ProcessEnvironmentBlock->FastPebLock);
}

extern "C"
void RtlReleasePebLock() {
    RtlLeaveCriticalSection(NtCurrentTeb()->ProcessEnvironmentBlock->FastPebLock);
}

extern "C"
void RtlAcquireLoaderLock() {
    RtlEnterCriticalSection(NtCurrentTeb()->ProcessEnvironmentBlock->LoaderLock);
}

extern "C"
void RtlReleaseLoaderLock() {
    RtlLeaveCriticalSection(NtCurrentTeb()->ProcessEnvironmentBlock->LoaderLock);
}
