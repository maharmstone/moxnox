// SPDX-License-Identifier: GPL-2.0-only
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <array>
#include <utility>
#include <algorithm>

import nt;
import pe;
import ntdll;
import print;
import unique_handle;

using namespace std;

static void thread_start(uint32_t (*func)(void*), void* param) {
    auto ret = func(param);

    RtlExitUserThread((nt_status)ret);
}

static unique_handle create_thread(uint32_t (*start)(void*), void* param) {
    nt_status status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    void* stack = nullptr;
    size_t stack_size;
    unique_handle handle;

    memset(&initial_teb, 0, sizeof(initial_teb));

    {
        auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
        auto& dos_header = *(IMAGE_DOS_HEADER*)peb.ImageBaseAddress;
        auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)&dos_header + dos_header.e_lfanew);

        stack_size = 0x100000;

        if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            stack_size = max(stack_size, (size_t)nt_header.OptionalHeader64.SizeOfStackReserve);
        else
            stack_size = max(stack_size, (size_t)nt_header.OptionalHeader32.SizeOfStackReserve);
    }

    status = NtAllocateVirtualMemory(NtCurrentProcess(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print_stderr("NtAllocateVirtualMemory returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), status);
        unreachable();
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = (uintptr_t)thread_start;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10;
    context.X0 = (uintptr_t)start;
    context.X1 = (uintptr_t)param;
#elifdef __x86_64__
    context.Rip = (uintptr_t)thread_start;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
    context.Rcx = (uintptr_t)start;
    context.Rdx = (uintptr_t)param;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, NtCurrentProcess(),
                            &client_id, &context, &initial_teb, true);

    if (!success(status)) {
        print_stderr("NtCreateThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), status);
        unreachable();
    }

    handle.reset(h);

    status = NtResumeThread(h, nullptr);
    if (!success(status)) {
        print_stderr("NtResumeThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), status);
        unreachable();
    }

    return handle;
}

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
    auto worker = create_thread([](void* arg) -> uint32_t {
        auto& data = *(thread_data*)arg;

        print("[Worker] Waiting for signal...\n");

        RtlEnterCriticalSection(data.cs);
        while (!*data.ready) {
            RtlSleepConditionVariableCS(data.cv, data.cs, nullptr);
        }
        print("[Worker] Received signal!\n");
        RtlLeaveCriticalSection(data.cs);

        return 0;
    }, &data);

    LARGE_INTEGER t;
    t.QuadPart = -5000000; // 500ms
    NtDelayExecution(false, &t);

    print("[Main] Signalling...\n");
    RtlEnterCriticalSection(&cs);
    ready = true;
    RtlWakeConditionVariable(&cv);
    RtlLeaveCriticalSection(&cs);

    NtWaitForSingleObject(worker.get(), false, nullptr);

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

    array<unique_handle, 5> workersuh;
    array<HANDLE, 5> workers;

    for (size_t i = 0; i < workers.size(); i++) {
        workersuh[i] = create_thread([](void* arg) -> uint32_t {
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
        }, &data);

        workers[i] = workersuh[i].get();
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

    array<unique_handle, 5> workersuh;
    array<HANDLE, 5> workers;

    for (size_t i = 0; i < workers.size(); i++) {
        workersuh[i] = create_thread([](void* arg) -> uint32_t {
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
        }, &data);

        workers[i] = workersuh[i].get();
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
