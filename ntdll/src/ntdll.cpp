// SPDX-License-Identifier: LGPL-2.1-or-later
#include "list.h"
#include <optional>
#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <span>
#include <utility>
#include <mutex>

using namespace std;

import print;
import format;
import pe;
import unicode;
import ntdll;
import ntdll_private;
import crt;
import unique_handle;
import exec;
import loader;

extern uint8_t __image_base__;

static_assert(sizeof(PEB_LDR_DATA) == 0x58);
static_assert(sizeof(LDR_DATA_TABLE_ENTRY) == 0x88);

static RTL_CRITICAL_SECTION peb_lock, loader_lock;
static uint64_t tls_bitmap; // FIXME - should be RTL_BITMAP
static bool initialized = false;

extern "C" {
    void (*mn_handle_exec_info)(exec_info* info) = nullptr;
}

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}

extern "C++"
void do_print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    while (true) {
        auto status = NtWriteFile(peb->ProcessParameters->StandardError, 0,
                                  nullptr, nullptr, &iosb, s.data(),
                                  (uint32_t)s.size(), nullptr, nullptr);

        if (!success(status) || iosb.Information == s.size())
            return;

        s = string_view(s.data() + iosb.Information, s.size() - iosb.Information);
    }
}

void call_tls_callbacks(const LDR_DATA_TABLE_ENTRY& ent, uint32_t reason) {
    using IMAGE_TLS_CALLBACK = void (*)(void* dll_handle, uint32_t reason,
                                        void* reserved);

    if (ent.TlsIndex == 0xffff)
        return;

    auto& dir = *tls_dirs[ent.TlsIndex];

    if (dir.AddressOfCallBacks == 0)
        return;

    auto ptr = (IMAGE_TLS_CALLBACK*)dir.AddressOfCallBacks;

    while (*ptr) {
        auto func = *ptr;

        func(ent.DllBase, reason, nullptr);

        ptr++;
    }
}

void call_process_detach_img(LDR_DATA_TABLE_ENTRY& ent, bool process_exit) {
    auto dll_main = (DLL_ENTRY_PROC)ent.EntryPoint;

    dll_main(ent.DllBase, DLL_PROCESS_DETACH,
             process_exit ? (void*)1 : nullptr);
    ent.ProcessAttachCalled = 0;

    call_tls_callbacks(ent, DLL_PROCESS_DETACH);
}

static void call_process_detach() {
    while (true) {
        bool changed = false;

        {
            mn_loader_lock lock;
            lock_guard lg(lock);

            for (auto le = ldr_data.InInitializationOrderModuleList.Blink; le != &ldr_data.InInitializationOrderModuleList; le = le->Blink) {
                auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);

                if (ent.ProcessAttachCalled && ent.ImageDll) {
                    call_process_detach_img(ent, true);
                    changed = true;
                    break;
                }
            }
        }

        if (!changed)
            break;
    }
}

static void call_thread_attach() {
    mn_loader_lock lock;
    lock_guard lg(lock);

    for (auto le = ldr_data.InInitializationOrderModuleList.Flink; le != &ldr_data.InInitializationOrderModuleList; le = le->Flink) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);

        call_tls_callbacks(ent, DLL_THREAD_ATTACH);

        if (ent.ProcessAttachCalled && ent.ImageDll) {
            auto dll_main = (DLL_ENTRY_PROC)ent.EntryPoint;

            if (!dll_main(ent.DllBase, DLL_THREAD_ATTACH, nullptr)) {
                print_stderr("DllMain returned false for DLL_THREAD_ATTACH for {}\n",
                             ent.BaseDllName);
            }
        }
    }
}

static void call_thread_detach() {
    mn_loader_lock lock;
    lock_guard lg(lock);

    for (auto le = ldr_data.InInitializationOrderModuleList.Blink; le != &ldr_data.InInitializationOrderModuleList; le = le->Blink) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);

        if (ent.ProcessAttachCalled && ent.ImageDll) {
            auto dll_main = (DLL_ENTRY_PROC)ent.EntryPoint;

            if (!dll_main(ent.DllBase, DLL_THREAD_DETACH, nullptr)) {
                print_stderr("DllMain returned false for DLL_THREAD_DETACH for {}\n",
                             ent.BaseDllName);
            }
        }

        call_tls_callbacks(ent, DLL_THREAD_DETACH);
    }
}

static nt_status alloc_thread_tls_mod(LDR_DATA_TABLE_ENTRY& ent) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.DllBase + dos_header.e_lfanew);
    uintptr_t dir_addr;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_TLS) {
            ent.TlsIndex = 0xffff;
            return nt_status::SUCCESS;
        }

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader64.DataDirectory);
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_TLS) {
            ent.TlsIndex = 0xffff;
            return nt_status::SUCCESS;
        }

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader32.DataDirectory);
    }

    dir_addr += IMAGE_DIRECTORY_ENTRY_TLS * sizeof(IMAGE_DATA_DIRECTORY);

    auto& data_dir = *(IMAGE_DATA_DIRECTORY*)((uint8_t*)ent.DllBase + dir_addr);

    if (data_dir.VirtualAddress == 0 || data_dir.Size == 0) {
        ent.TlsIndex = 0xffff;
        return nt_status::SUCCESS;
    }

    // FIXME - IMAGE_TLS_DIRECTORY32 if 32-bit

    if (data_dir.Size < sizeof(IMAGE_TLS_DIRECTORY64)) {
        print_stderr("alloc_thread_tls_mod: TLS directory was {} bytes, expected {}\n",
                     data_dir.Size, sizeof(IMAGE_TLS_DIRECTORY64));
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    auto& dir = *(IMAGE_TLS_DIRECTORY64*)((uint8_t*)ent.DllBase + data_dir.VirtualAddress);

    size_t size = dir.EndAddressOfRawData - dir.StartAddressOfRawData + dir.SizeOfZeroFill;

    if (size == 0 && dir.AddressOfCallBacks == 0) {
        ent.TlsIndex = 0xffff;
        return nt_status::SUCCESS;
    }

    tls_dirs = (IMAGE_TLS_DIRECTORY64**)RtlReAllocateHeap(GetProcessHeap(), 0, tls_dirs,
                                                          sizeof(IMAGE_TLS_DIRECTORY64*) * (num_tls_dirs + 1));
    if (!tls_dirs) {
        print_stderr("RtlReAllocateHeap failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    tls_dirs[num_tls_dirs] = &dir;

    ent.TlsIndex = num_tls_dirs;
    num_tls_dirs++;

    *(uint16_t*)dir.AddressOfIndex = ent.TlsIndex;

    return nt_status::SUCCESS;
}

static nt_status alloc_thread_tls() {
    for (auto le = ldr_data.InLoadOrderModuleList.Flink; le != &ldr_data.InLoadOrderModuleList; le = le->Flink) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        auto ret = alloc_thread_tls_mod(ent);
        if (!success(ret))
            return ret;
    }

    return nt_status::SUCCESS;
}

static nt_status init_tls_block() {
    auto& teb = *NtCurrentTeb();

    if (num_tls_dirs == 0)
        return nt_status::SUCCESS;

    auto ptr = (void**)malloc(sizeof(void*) * num_tls_dirs);
    if (!ptr)
        return nt_status::INSUFFICIENT_RESOURCES;

    memset(ptr, 0, sizeof(void*) * num_tls_dirs);

    for (auto le = ldr_data.InLoadOrderModuleList.Flink; le != &ldr_data.InLoadOrderModuleList; le = le->Flink) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        if (ent.TlsIndex == 0xffff)
            continue;

        auto& dir = *tls_dirs[ent.TlsIndex];

        size_t size = dir.EndAddressOfRawData - dir.StartAddressOfRawData + dir.SizeOfZeroFill;

        if (size == 0)
            continue;

        auto& p = ptr[ent.TlsIndex];

        p = malloc(size);
        if (!p)
            return nt_status::INSUFFICIENT_RESOURCES;

        memcpy(p, (void*)dir.StartAddressOfRawData,
               dir.EndAddressOfRawData - dir.StartAddressOfRawData);
        memset((uint8_t*)p + size - dir.SizeOfZeroFill, 0, dir.SizeOfZeroFill);
    }

    teb.ThreadLocalStoragePointer = ptr;

    return nt_status::SUCCESS;
}

extern "C"
void mn_start_thread(mn_image_info* ntdll_info, mn_image_info* img_info, CONTEXT* context) {
    nt_status status;

    if (!initialized) {
        auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

        auto heap = RtlCreateHeap(HEAP_GROWABLE, nullptr, 0, 0, nullptr, nullptr);

        if (!heap) {
            print("RtlCreateHeap failed\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::NO_MEMORY);
            unreachable();
        }

        peb.ProcessHeap = (HANDLE)heap;
        peb.Ldr = &ldr_data;
        peb.TlsBitmap = &tls_bitmap;
        peb.FastPebLock = &peb_lock;
        peb.LoaderLock = &loader_lock;

        RtlInitializeCriticalSection(&peb_lock);
        RtlInitializeCriticalSection(&loader_lock);

        init_ldr_data(*ntdll_info, *img_info);

        load_dlls();

        status = toggle_protections(false);
        if (!success(status)) {
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        status = resolve_relocations();
        if (!success(status)) {
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        status = resolve_imports();
        if (!success(status)) {
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        status = toggle_protections(true);
        if (!success(status)) {
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        status = alloc_thread_tls();
        if (!success(status)) {
            print_stderr("alloc_thread_tls returned {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        status = init_tls_block();
        if (!success(status)) {
            print_stderr("init_tls_block returned {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        status = call_process_attach();
        if (!success(status)) {
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        initialized = true;

        InsertTailList(&tls_list, &NtCurrentTeb()->TlsLinks);
    } else {
        {
            mn_peb_lock lock1;
            mn_loader_lock lock2;
            lock_guard lg1(lock1);
            lock_guard lg2(lock2);

            status = init_tls_block();
            if (!success(status)) {
                print_stderr("init_tls_block returned {}\n", status);
                NtTerminateProcess(NtCurrentProcess(), status);
                unreachable();
            }

            InsertTailList(&tls_list, &NtCurrentTeb()->TlsLinks);
        }

        call_thread_attach();
    }

#ifdef __aarch64__
    context->X18 = (uintptr_t)NtCurrentTeb();
#endif

    NtContinue(context, true);
}

extern "C"
void RtlExitUserProcess(uint32_t status) {
    mn_peb_lock lock1;
    mn_loader_lock lock2;
    lock_guard lg1(lock1);
    lock_guard lg2(lock2);

    // FIXME - also take heap lock?

    // kill other threads
    NtTerminateProcess(0, (nt_status)status);

    call_process_detach();

    NtTerminateProcess(NtCurrentProcess(), (nt_status)status);
    unreachable();
}

extern "C"
void __attribute__((noreturn)) RtlExitUserThread(nt_status exit_status) {
    auto& teb = *NtCurrentTeb();

    // FIXME - call RtlExitUserProcess instead if last thread (ThreadAmILastThread)

    call_thread_detach();

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        RemoveEntryList(&teb.TlsLinks);

        decltype(num_tls_dirs) read_num_tls_dirs;

        {
            mn_loader_lock lock2;
            lock_guard lg2(lock2);

            read_num_tls_dirs = num_tls_dirs;
        }

        auto tlsptr = (void**)teb.ThreadLocalStoragePointer;

        if (tlsptr) {
            for (unsigned int i = 0; i < read_num_tls_dirs; i++) {
                if (tlsptr[i])
                    free(tlsptr[i]);
            }

            free(tlsptr);
        }
    }

    while (true) {
        NtTerminateThread(NtCurrentThread(), exit_status);
    }
}

extern "C"
bool RtlIsProcessorFeaturePresent(uint32_t processor_feature) {
#ifdef __x86_64__
    switch (processor_feature) {
        case PF_COMPARE_EXCHANGE_DOUBLE:
        case PF_MMX_INSTRUCTIONS_AVAILABLE:
        case PF_XMMI_INSTRUCTIONS_AVAILABLE:
        case PF_RDTSC_INSTRUCTION_AVAILABLE:
        case PF_PAE_ENABLED:
        case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
        case PF_SSE3_INSTRUCTIONS_AVAILABLE:
        case PF_SSSE3_INSTRUCTIONS_AVAILABLE:
        case PF_XSAVE_ENABLED:
        case PF_COMPARE_EXCHANGE128:
        case PF_SSE_DAZ_MODE_AVAILABLE:
        case PF_NX_ENABLED:
        case PF_SSE4_1_INSTRUCTIONS_AVAILABLE:
        case PF_SSE4_2_INSTRUCTIONS_AVAILABLE:
        case PF_AVX_INSTRUCTIONS_AVAILABLE:
        case PF_AVX2_INSTRUCTIONS_AVAILABLE:
            return true;

        case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
            return false; // FIXME - check

        case PF_SECOND_LEVEL_ADDRESS_TRANSLATION:
            return false; // FIXME - check

        case PF_VIRT_FIRMWARE_ENABLED:
            return false; // FIXME - check

        case PF_RDWRFSGSBASE_AVAILABLE:
            return false; // FIXME - check

        // FIXME - PF_FASTFAIL_AVAILABLE?
        // FIXME - AVX512 features? All other x86 features?

        default:
            break;
    }
#elifdef __aarch64__
    switch (processor_feature) {
        case PF_ARM_V8_INSTRUCTIONS_AVAILABLE:
        case PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE:
            return true;

        case PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE:
            return false; // FIXME - check

        case PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE:
            return false; // FIXME - check

        // FIXME - all other ARM features?

        default:
            break;

    }
#endif

    return false;
}

extern "C"
void mn_exec(void (*entry_point)(), exec_info* exec_block,
             size_t num_exec_block_entries) {
    nt_status status;

    if (num_exec_block_entries > 0) {
        void* addr;
        size_t region_size;

        if (!mn_handle_exec_info) {
            for (size_t i = 0; i < num_exec_block_entries; i++) {
                if (exec_block[i].type == exec_info_type::inherited_fd)
                    NtClose(exec_block[i].inherited_fd.h);
            }
        } else {
            for (size_t i = 0; i < num_exec_block_entries; i++) {
                mn_handle_exec_info(&exec_block[i]);
            }
        }

        addr = exec_block;
        region_size = 0;

        status = NtFreeVirtualMemory(NtCurrentProcess(), &addr, &region_size,
                                     MEM_RELEASE);
        if (!success(status))
            print_stderr("NtFreeVirtualMemory returned {}\n", status);
    }

    entry_point();
}

extern "C"
uint32_t RtlRandom(uint32_t* seed) {
    auto v = (uint64_t)*seed;

    v = v * (uint64_t)1103515245 + 12345; // from Knuth

    *seed = (uint32_t)v;

    return (uint32_t)v;
}

static void clear_peb_lock_on_fork() {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    RtlDeleteCriticalSection(peb.FastPebLock);
    RtlInitializeCriticalSection(peb.FastPebLock);
}

static void clear_loader_lock_on_fork() {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    RtlDeleteCriticalSection(peb.LoaderLock);
    RtlInitializeCriticalSection(peb.LoaderLock);
}

extern "C"
void mn_on_fork() {
    clear_peb_lock_on_fork();
    clear_loader_lock_on_fork();
    clear_heap_lock_on_fork();
    clear_alarm_on_fork();
}
