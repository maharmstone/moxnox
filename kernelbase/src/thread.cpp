// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <string.h>
#include <utility>
#include <mutex>
#include <algorithm>

import nt;
import ntdll;
import kernelbase;
import print;
import win32error;
import pe;

using namespace std;

extern "C"
uint32_t TlsAlloc() {
    auto& teb = *NtCurrentTeb();
    auto& peb = *teb.ProcessEnvironmentBlock;

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        // FIXME - TlsExpansionBitmap

        // FIXME - should be proper RTL_BITMAP
        auto& bitmap = *(uint64_t*)peb.TlsBitmap;

        for (uint32_t i = 0; i < sizeof(bitmap) * 8; i++) {
            if (!(bitmap & (1 << i))) {
                bitmap |= 1 << i;
                teb.TlsSlots[i] = 0;

                return i;
            }
        }
    }

    RtlSetLastWin32Error(ERROR_NO_MORE_ITEMS);
    return 0xffffffff;
}

extern "C"
bool TlsFree(uint32_t tls_index) {
    auto& teb = *NtCurrentTeb();
    auto& peb = *teb.ProcessEnvironmentBlock;

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        // FIXME - should be proper RTL_BITMAP
        auto& bitmap = *(uint64_t*)peb.TlsBitmap;

        // FIXME - TlsExpansion
        if (tls_index >= sizeof(teb.TlsSlots) / sizeof(*teb.TlsSlots)) {
            RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
            return false;
        }

        if (!(bitmap & 1 << tls_index)) {
            RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
            return false;
        }

        bitmap |= 1 << tls_index;

        // FIXME - ThreadZeroTlsCell?
    }

    return true;
}

extern "C"
uint32_t GetLastError() {
    return NtCurrentTeb()->LastErrorValue;
}

extern "C"
void* TlsGetValue(uint32_t tls_index) {
    auto& teb = *NtCurrentTeb();

    // FIXME - TlsExpansion
    if (tls_index >= sizeof(teb.TlsSlots) / sizeof(*teb.TlsSlots)) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    RtlSetLastWin32Error(ERROR_SUCCESS);

    return teb.TlsSlots[tls_index];
}

extern "C"
bool TlsSetValue(uint32_t tls_index, void* tls_value) {
    auto& teb = *NtCurrentTeb();

    // FIXME - TlsExpansion
    if (tls_index >= sizeof(teb.TlsSlots) / sizeof(*teb.TlsSlots)) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return false;
    }

    teb.TlsSlots[tls_index] = tls_value;

    return true;
}

static void thread_start(THREAD_START_ROUTINE func, void* param) {
    auto ret = func(param);

    RtlExitUserThread((nt_status)ret);
}

extern "C"
HANDLE CreateThread(SECURITY_ATTRIBUTES* sa, size_t stack_size,
                    THREAD_START_ROUTINE start, void* param,
                    uint32_t flags, uint32_t* id) {
    nt_status status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    void* stack = nullptr;

    static const uint32_t supported_flags = CREATE_SUSPENDED;

    if (sa) {
        print_stderr("FIXME - non-NULL SECURITY_ATTRIBUTES passed to CreateThread\n");
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return 0;
    }

    if (id) {
        print_stderr("FIXME - non-NULL id pointer passed to CreateThread\n");
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return 0;
    }

    if (flags & ~supported_flags) {
        print_stderr("FIXME - unsupported CreateThread flags {:x}\n",
                     flags & ~supported_flags);
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return 0;
    }

    // FIXME - should just be passing to ntdll!RtlCreateUserThread?

    memset(&initial_teb, 0, sizeof(initial_teb));

    if (stack_size == 0) {
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
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return 0;
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

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, NtCurrentProcess(), &client_id, &context,
                            &initial_teb, true);

    if (!success(status)) {
        print_stderr("NtCreateThread returned {}\n", status);
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return 0;
    }

    if (!(flags & CREATE_SUSPENDED)) {
        status = NtResumeThread(h, nullptr);
        if (!success(status)) {
            print_stderr("NtResumeThread returned {}\n", status);
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return 0;
        }
    }

    return h;
}
