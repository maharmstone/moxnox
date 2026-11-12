// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <utility>
#include <algorithm>

import nt;
import pe;
import ntdll;
import print;
import unique_handle;

using namespace std;

class TLSTest {
public:
    TLSTest() {
        print("Constructor: thread {}\n", NtCurrentTeb()->ClientId.UniqueThread);
    }

    ~TLSTest() {
        print("Destructor: thread {}\n", NtCurrentTeb()->ClientId.UniqueThread);
    }

    void foo() {
        bar = 1;
    }

    volatile int bar = 0;
};

thread_local TLSTest obj;

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

static uint32_t thread_func(void*) {
    obj.foo();
    return 0;
}

int main() {
    obj.foo();

    auto t1 = create_thread(thread_func, nullptr);
    auto t2 = create_thread(thread_func, nullptr);

    NtWaitForSingleObject(t1.get(), false, nullptr);
    NtWaitForSingleObject(t2.get(), false, nullptr);

    return 0;
}
