// SPDX-License-Identifier: GPL-2.0-only
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <string_view>
#include <span>
#include <utility>
#include <algorithm>

import nt;
import pe;
import ntdll;
import print;
import format;
import unique_handle;

using namespace std;

// Test 1: Static TLS (compile-time)
thread_local int static_tls_var = 100;
thread_local char static_tls_string[64] = "Initial value";

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

// Thread function
static unsigned int thread_func(void* arg) {
    int thread_id = (int)(intptr_t)arg;

    print("\n=== Thread {} started ===\n", thread_id);

    // Test static TLS - each thread should have independent copy
    print("Thread {}: static_tls_var initial value: {}\n",
          thread_id, static_tls_var);

    static_tls_var = 100 + thread_id;  // Modify
    format(span(static_tls_string), "Thread {} string\0", thread_id);

    print("Thread {}: static_tls_var modified to: {}\n",
          thread_id, static_tls_var);
    print("Thread {}: static_tls_string: {}\n",
          thread_id, string_view(static_tls_string));

    // Sleep a bit so threads interleave
    LARGE_INTEGER t;
    t.QuadPart = -1000000; // 100ms
    NtDelayExecution(false, &t);

    // Read again to verify values didn't change from other threads
    print("Thread {}: Final check - static_tls_var: {}, string: {}\n",
           thread_id, static_tls_var, string_view(static_tls_string));

    print("=== Thread {} finished ===\n\n", thread_id);

    return 0;
}

int main() {
    print("=== TLS Test Program ===\n\n");

    // Test 1: Static TLS in main thread
    print("Main thread: Initial static_tls_var = {}\n", static_tls_var);
    static_tls_var = 42;
    print("Main thread: Modified static_tls_var = {}\n", static_tls_var);

    // Create multiple threads

    {
        array<unique_handle, 4> threadsuh;
        array<HANDLE, 4> threads;

        for (size_t i = 0; i < threads.size(); i++) {
            threadsuh[i] = create_thread(thread_func, (void*)i);
            threads[i] = threadsuh[i].get();
        }

        // Wait for all threads
        auto status = NtWaitForMultipleObjects(threads.size(), threads.data(),
                                               false, false, nullptr);
        if (!success(status))
            print("NtWaitForMultipleObjects returned {}\n", status);
    }

    // Verify main thread's TLS wasn't corrupted
    print("\n=== Back in main thread ===\n");
    print("Main thread: Final static_tls_var = {} (should be 42)\n",
          static_tls_var);

    print("\n=== All tests completed ===\n");
    return 0;
}
