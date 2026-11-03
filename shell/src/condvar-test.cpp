// SPDX-License-Identifier: GPL-2.0-only
#include <stddef.h>
#include <stdint.h>
#include <array>

import nt;
import ntdll;
import kernelbase;
import print;

using namespace std;

// Test 1: Basic signal/wait
static void test_basic_signal() {
    print("=== Test 1: Basic Signal/Wait ===\n");

    RTL_CRITICAL_SECTION cs;
    RTL_CONDITION_VARIABLE cv;
    bool ready = false;

    RtlInitializeCriticalSection(&cs);
    RtlInitializeConditionVariable(&cv);

    // Proper data structure
    struct thread_data {
        bool* ready;
        RTL_CRITICAL_SECTION* cs;
        RTL_CONDITION_VARIABLE* cv;
    } data = {&ready, &cs, &cv};

    // Worker thread
    HANDLE worker = CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& data = *(thread_data*)arg;

        print("[Worker] Waiting for signal...\n");

        RtlEnterCriticalSection(data.cs);
        while (!*data.ready) {
            RtlSleepConditionVariableCS(data.cv, data.cs, nullptr);
        }
        print("[Worker] Received signal!\n");
        RtlLeaveCriticalSection(data.cs);

        return 0;
    }, &data, 0, nullptr);

    LARGE_INTEGER t;
    t.QuadPart = -5000000; // 500ms
    NtDelayExecution(false, &t);

    print("[Main] Signalling...\n");
    RtlEnterCriticalSection(&cs);
    ready = true;
    RtlWakeConditionVariable(&cv);
    RtlLeaveCriticalSection(&cs);

    NtWaitForSingleObject(worker, false, nullptr);
    NtClose(worker);

    RtlDeleteCriticalSection(&cs);
    print("✓ Test 1 passed\n\n");
}

// Test 2: Multiple waiters
static void test_multiple_waiters_signal() {
    print("=== Test 2: Multiple Waiters (Signal) ===\n");

    RTL_CRITICAL_SECTION cs;
    RTL_CONDITION_VARIABLE cv;
    int counter = 0;

    RtlInitializeCriticalSection(&cs);
    RtlInitializeConditionVariable(&cv);

    struct shared_data {
        RTL_CRITICAL_SECTION* cs;
        RTL_CONDITION_VARIABLE* cv;
        int* counter;
    } data = {&cs, &cv, &counter};

    array<HANDLE, 5> workers;

    for (size_t i = 0; i < workers.size(); i++) {
        workers[i] = CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
            auto& data = *(shared_data*)arg;
            auto tid = (uint32_t)NtCurrentTeb()->ClientId.UniqueThread;

            RtlEnterCriticalSection(data.cs);
            print("[Worker {}] Waiting...\n", tid);

            while (*data.counter == 0) {
                RtlSleepConditionVariableCS(data.cv, data.cs, nullptr);
            }

            (*data.counter)--;
            print("[Worker {}] Woke up! Counter now: {}\n", tid, *data.counter);

            RtlLeaveCriticalSection(data.cs);
            return 0;
        }, &data, 0, nullptr);
    }

    LARGE_INTEGER t;
    t.QuadPart = -5000000; // 500ms
    NtDelayExecution(false, &t);

    for (size_t i = 0; i < workers.size(); i++) {
        t.QuadPart = -2000000; // 200ms
        NtDelayExecution(false, &t);

        print("[Main] Signalling worker {}...\n", i + 1);

        RtlEnterCriticalSection(&cs);
        counter++;
        RtlWakeConditionVariable(&cv);
        RtlLeaveCriticalSection(&cs);
    }

    NtWaitForMultipleObjects(workers.size(), workers.data(), false, false,
                             nullptr);

    for (size_t i = 0; i < workers.size(); i++) {
        NtClose(workers[i]);
    }

    RtlDeleteCriticalSection(&cs);
    print("✓ Test 2 passed\n\n");
}

// Test 3: Broadcast
static void test_broadcast() {
    print("=== Test 3: Broadcast (Wake All) ===\n");

    RTL_CRITICAL_SECTION cs;
    RTL_CONDITION_VARIABLE cv;
    bool ready = false;

    RtlInitializeCriticalSection(&cs);
    RtlInitializeConditionVariable(&cv);

    struct shared_data {
        RTL_CRITICAL_SECTION* cs;
        RTL_CONDITION_VARIABLE* cv;
        bool* ready;
    } data = {&cs, &cv, &ready};

    array<HANDLE, 5> workers;

    for (size_t i = 0; i < workers.size(); i++) {
        workers[i] = CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
            auto& data = *(shared_data*)arg;
            auto tid = (uint32_t)NtCurrentTeb()->ClientId.UniqueThread;

            RtlEnterCriticalSection(data.cs);
            print("[Worker {}] Waiting for broadcast...\n", tid);

            while (!*data.ready) {
                RtlSleepConditionVariableCS(data.cv, data.cs, nullptr);
            }

            print("[Worker {}] Woke up from broadcast!\n", tid);
            RtlLeaveCriticalSection(data.cs);

            return 0;
        }, &data, 0, nullptr);
    }

    LARGE_INTEGER t;
    t.QuadPart = -5000000; // 500ms
    NtDelayExecution(false, &t);

    print("[Main] Broadcasting to all workers...\n");
    RtlEnterCriticalSection(&cs);
    ready = true;
    RtlWakeAllConditionVariable(&cv);
    RtlLeaveCriticalSection(&cs);

    NtWaitForMultipleObjects(workers.size(), workers.data(), false, false,
                             nullptr);

    for (size_t i = 0; i < workers.size(); i++) {
        NtClose(workers[i]);
    }

    RtlDeleteCriticalSection(&cs);
    print("✓ Test 3 passed\n\n");
}

int main() {
    print("╔════════════════════════════════════════╗\n");
    print("║  NT Condition Variable Test Suite      ║\n");
    print("╚════════════════════════════════════════╝\n\n");

    test_basic_signal();
    test_multiple_waiters_signal();
    test_broadcast();

    print("╔════════════════════════════════════════╗\n");
    print("║  All tests passed! ✓                   ║\n");
    print("╚════════════════════════════════════════╝\n");

    return 0;
}
