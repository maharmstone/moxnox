// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string_view>
#include <span>
#include <mutex>
#include <algorithm>

import nt;
import ntdll;
import kernelbase;
import print;
import unique_handle;
import pe;
import pagesize;
import unicode;
import win32error;

using namespace std;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static size_t calc_environment_size(const void* environment, bool utf16) {
    size_t ret = 0;

    if (utf16) {
        auto env = (const char16_t*)environment;

        while (*env) {
            auto sv = u16string_view(env);

            ret += sv.size() * sizeof(char16_t);
            ret += sizeof(char16_t);

            env += sv.size() + 1;
        }
    } else {
        auto env = (const char*)environment;

        while (*env) {
            auto sv = string_view(env);

            ret += utf8_to_utf16_len(sv) * sizeof(char16_t);
            ret += sizeof(char16_t);

            env += sv.size() + 1;
        }
    }

    ret += sizeof(char16_t);

    return ret;
}

extern "C"
bool CreateProcessW(const char16_t* application_name, char16_t* command_line,
                    const SECURITY_ATTRIBUTES* process_attributes,
                    const SECURITY_ATTRIBUTES* thread_attributes,
                    bool inherit_handles, uint32_t creation_flags,
                    const void* environment, const char16_t* current_directory,
                    const STARTUPINFOW* startup_info,
                    PROCESS_INFORMATION* process_information) {
    HANDLE h, stdinh = 0, stdouth, stderrh, curdirh = 0;
    HANDLE orig_stdin, orig_stdout, orig_stderr;
    unique_handle file, sect, proc, thread, curdir;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    SECTION_IMAGE_INFORMATION sii;
    PROCESS_BASIC_INFORMATION pbi;
    RTL_USER_PROCESS_PARAMETERS params;
    PEB newpeb;
    INITIAL_TEB initial_teb;
    CONTEXT context;
    CLIENT_ID client_id;
    nt_status status;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    static const uint32_t supported_flags = CREATE_SUSPENDED | \
                                            CREATE_UNICODE_ENVIRONMENT;
    static const uint32_t supported_si_flags = STARTF_USESTDHANDLES;

    if (!application_name) {
        print_stderr("FIXME - CreateProcess with no application_name\n");
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (process_attributes) {
        print_stderr("FIXME - CreateProcess with non-NULL process_attributes\n");
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (thread_attributes) {
        print_stderr("FIXME - CreateProcess with non-NULL thread_attributes\n");
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (inherit_handles) {
        print_stderr("FIXME - CreateProcess with inherit_handles\n");
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (creation_flags & ~supported_flags) {
        print_stderr("FIXME - unsupported CreateProcess flags {:x}\n",
                     creation_flags & ~supported_flags);
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (!application_name && !command_line) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return false;
    }

    if (!startup_info || startup_info->cb < sizeof(*startup_info)) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return false;
    }

    if (startup_info->dwFlags & ~supported_si_flags) {
        print_stderr("FIXME - unsupported CreateProcess STARTUPINFO flags {:x}\n",
                     startup_info->dwFlags & ~supported_si_flags);
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    auto exe = u16string_view(application_name);

    // open file

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    us.Length = us.MaximumLength = (uint16_t)(exe.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)exe.data();

    status = NtOpenFile(&h, FILE_READ_DATA, &oa, &iosb, 0,
                        FILE_NON_DIRECTORY_FILE);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation,
                            &sii, sizeof(SECTION_IMAGE_INFORMATION), nullptr);

    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect.get(), 0, 0);

    if (status == nt_status::IMAGE_MACHINE_TYPE_MISMATCH_EXE) {
        RtlSetLastWin32Error(ERROR_EXE_MACHINE_TYPE_MISMATCH);
        return false;
    } else if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    proc.reset(h);

    // duplicate handles

    if (startup_info->dwFlags & STARTF_USESTDHANDLES) {
        orig_stdin = startup_info->hStdInput;
        orig_stdout = startup_info->hStdOutput;
        orig_stderr = startup_info->hStdError;
    } else {
        orig_stdin = peb->ProcessParameters->StandardInput;
        orig_stdout = peb->ProcessParameters->StandardOutput;
        orig_stderr = peb->ProcessParameters->StandardError;
    }

    status = NtDuplicateObject(NtCurrentProcess(), orig_stdin,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    status = NtDuplicateObject(NtCurrentProcess(), orig_stdout,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    status = NtDuplicateObject(NtCurrentProcess(), orig_stderr,
                               proc.get(), &stderrh, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    if (current_directory) {
        u16string_view sv{current_directory};

        us.Length = us.MaximumLength = (uint16_t)(sv.size() * sizeof(char16_t));
        us.Buffer = (char16_t*)sv.data();

        auto status = NtCreateFile(&h, FILE_TRAVERSE, &oa, &iosb, nullptr,
                                   FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                                   FILE_DIRECTORY_FILE, nullptr, 0);
        if (!success(status)) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        } else
            curdir = unique_handle{h};

        status = NtDuplicateObject(NtCurrentProcess(), curdir.get(),
                                   proc.get(), &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
        if (!success(status)) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }
    } else {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle,
                                   proc.get(), &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
        if (!success(status)) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }
    }

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    // change handles and set params

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stderrh;
    params.CurrentDirectory.Handle = curdirh;
    params.StartingX = startup_info->dwX;
    params.StartingY = startup_info->dwY;
    params.CountX = startup_info->dwXSize;
    params.CountY = startup_info->dwYSize;
    params.CountCharsX = startup_info->dwXCountChars;
    params.CountCharsY = startup_info->dwYCountChars;
    params.FillAttribute = startup_info->dwFillAttribute;
    params.WindowFlags = startup_info->dwFlags;
    params.ShowWindowFlags = startup_info->wShowWindow;

    if (current_directory) {
        FILE_NAME_INFORMATION fni;

        status = NtQueryInformationFile(curdir.get(), &iosb, &fni, sizeof(fni),
                                        FILE_INFORMATION_CLASS::NameInformation);
        if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }

        size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

        auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
        if (!fni2) {
            print_stderr("malloc failed\n");
            RtlSetLastWin32Error(ERROR_NO_SYSTEM_RESOURCES);
            return false;
        }

        status = NtQueryInformationFile(curdir.get(), &iosb, fni2, (uint32_t)len,
                                        FILE_INFORMATION_CLASS::NameInformation);
        if (!success(status)) {
            free(fni2);
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }

        auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

        static const u16string_view prefix = u"C:";

        for (size_t i = 0; i < sv.size(); i++) {
            if (fni2->FileName[i] == u'/')
                fni2->FileName[i] = u'\\';
        }

        status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer,
                                      prefix.data(), (uint32_t)(prefix.size() * sizeof(char16_t)),
                                      nullptr);
        if (!success(status)) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }

        status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer + prefix.size(),
                                      fni2->FileName,
                                      (uint32_t)min(sv.size() * sizeof(char16_t), (size_t)params.CurrentDirectory.DosPath.MaximumLength - (prefix.size() * sizeof(char16_t))),
                                      nullptr);
        if (!success(status)) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }

        params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength, (uint16_t)((prefix.size() * sizeof(char16_t)) + fni2->FileNameLength));

        free(fni2);
    } else {
        mn_peb_lock lock;
        lock_guard lg(lock);

        // copy current directory string

        params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                     peb->ProcessParameters->CurrentDirectory.DosPath.Length);

        status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer,
                                      peb->ProcessParameters->CurrentDirectory.DosPath.Buffer,
                                      params.CurrentDirectory.DosPath.Length, nullptr);
        if (!success(status)) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }
    }

    // allocate misc block

    u16string_view cmdline;

    if (command_line)
        cmdline = command_line;
    else
        cmdline = application_name;

    size_t misc_block_size = (cmdline.size() + 1) * sizeof(char16_t);
    uint8_t* misc_block;
    void* proc_misc_block = nullptr;

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        size_t environment_size;

        if (environment) {
            environment_size = calc_environment_size(environment,
                                                    creation_flags & CREATE_UNICODE_ENVIRONMENT);
        } else
            environment_size = peb->ProcessParameters->EnvironmentSize;

        misc_block_size += environment_size;

        misc_block = (uint8_t*)malloc(misc_block_size);
        if (!misc_block) {
            print_stderr("malloc failed\n");
            RtlSetLastWin32Error(ERROR_NO_SYSTEM_RESOURCES);
            return false;
        }

        // allocate misc block in process

        size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

        status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                        &proc_misc_block_size,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!success(status)) {
            free(misc_block);
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }

        // copy cmdline

        auto ptr = misc_block;

        memcpy(misc_block, cmdline.data(), cmdline.size() * sizeof(char16_t));
        ptr += cmdline.size() * sizeof(char16_t);

        *ptr = 0;
        ptr++;

        params.CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
        params.CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
        params.CommandLine.Buffer = (char16_t*)proc_misc_block;

        if (environment) {
            if (creation_flags & CREATE_UNICODE_ENVIRONMENT)
                memcpy(ptr, environment, environment_size);
            else {
                span sp((char16_t*)ptr, environment_size * sizeof(char16_t));
                auto env = (const char*)environment;

                while (*env) {
                    auto sv = string_view(env);

                    utf8_to_utf16(sp, sv);
                    sp = sp.subspan(utf8_to_utf16_len(sv));

                    sp[0] = u'\0';
                    sp = sp.subspan(1);

                    env += sv.size() + 1;
                }

                sp[0] = u'\0';
            }
        } else {
            memcpy(ptr, peb->ProcessParameters->Environment,
                   peb->ProcessParameters->EnvironmentSize);
        }

        params.Environment = (uint8_t*)proc_misc_block + (ptr - misc_block);
        params.EnvironmentSize = environment_size;
    }

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        free(misc_block);
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                  sizeof(params), nullptr);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    auto stack_size = max((size_t)0x100000, sii.MaximumStackSize);

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = (uintptr_t)sii.TransferAddress;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10;
#elifdef __x86_64__
    context.Rip = (uintptr_t)sii.TransferAddress;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc.get(),
                            &client_id, &context, &initial_teb, true);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    thread.reset(h);

    if (!(creation_flags & CREATE_SUSPENDED)) {
        status = NtResumeThread(thread.get(), nullptr);
        if (!success(status)) {
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
            return false;
        }
    }

    process_information->hProcess = 0;
    process_information->hThread = 0;

    swap(process_information->hProcess, proc.h);
    swap(process_information->hThread, thread.h);
    process_information->dwProcessId = (uint32_t)client_id.UniqueProcess;
    process_information->dwThreadId = (uint32_t)client_id.UniqueThread;

    return true;
}
