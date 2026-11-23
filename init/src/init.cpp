// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <span>
#include <string_view>
#include <utility>
#include <algorithm>
#include <mutex>
#include <optional>

using namespace std;

import nt;
import ntdll;
import pe;
import print;
import unique_handle;
import moxnox_ioctls;
import evdev;
import unicode;
import win32error;
import pagesize;

static span<char16_t> environment;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static size_t calc_environment_size(const char16_t* env) {
    size_t ret = 0;

    while (*env) {
        auto sv = u16string_view(env);

        ret += sv.size() * sizeof(char16_t);
        ret += sizeof(char16_t);

        env += sv.size() + 1;
    }

    ret += sizeof(char16_t);

    return ret;
}

static nt_status create_process(u16string_view exe, u16string_view cmdline,
                                const char16_t* environment, optional<HANDLE> orig_stdin,
                                optional<HANDLE> orig_stdout, optional<HANDLE> orig_stderr,
                                unique_handle& proc, unique_handle& thread) {
    HANDLE h, stdinh = 0, stdouth, stderrh, curdirh = 0;
    unique_handle file, sect;
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

    // open file

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    us.Length = us.MaximumLength = (uint16_t)(exe.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)exe.data();

    status = NtOpenFile(&h, FILE_READ_DATA, &oa, &iosb, 0,
                        FILE_NON_DIRECTORY_FILE);
    if (!success(status))
        return status;

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());
    if (!success(status))
        return status;

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation,
                            &sii, sizeof(SECTION_IMAGE_INFORMATION), nullptr);
    if (!success(status))
        return status;

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect.get(), 0, 0);
    if (!success(status))
        return status;

    proc.reset(h);

    // duplicate handles

    if (!orig_stdin.has_value())
        orig_stdin = peb->ProcessParameters->StandardInput;

    if (!orig_stdout.has_value())
        orig_stdout = peb->ProcessParameters->StandardOutput;

    if (!orig_stderr.has_value())
        orig_stderr = peb->ProcessParameters->StandardError;

    status = NtDuplicateObject(NtCurrentProcess(), *orig_stdin,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return status;

    status = NtDuplicateObject(NtCurrentProcess(), *orig_stdout,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return status;

    status = NtDuplicateObject(NtCurrentProcess(), *orig_stderr,
                               proc.get(), &stderrh, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return status;

    status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle,
                               proc.get(), &curdirh, 0, 0,
                               DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status))
        return status;

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return status;

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status))
        return status;

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status))
        return status;

    // change handles and set params

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stderrh;
    params.CurrentDirectory.Handle = curdirh;
    params.StartingX = 0;
    params.StartingY = 0;
    params.CountX = 0;
    params.CountY = 0;
    params.CountCharsX = 0;
    params.CountCharsY = 0;
    params.FillAttribute = 0;
    params.WindowFlags = 0;
    params.ShowWindowFlags = 0;

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        // copy current directory string

        params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                     peb->ProcessParameters->CurrentDirectory.DosPath.Length);

        status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer,
                                      peb->ProcessParameters->CurrentDirectory.DosPath.Buffer,
                                      params.CurrentDirectory.DosPath.Length, nullptr);
        if (!success(status))
            return status;
    }

    // allocate misc block

    size_t misc_block_size = (cmdline.size() + 1) * sizeof(char16_t);
    unique_ptr<uint8_t[]> misc_block;
    void* proc_misc_block = nullptr;

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        size_t environment_size;

        if (environment)
            environment_size = calc_environment_size(environment);
        else
            environment_size = peb->ProcessParameters->EnvironmentSize;

        misc_block_size += environment_size;

        misc_block.reset(new uint8_t[misc_block_size]);
        if (!misc_block) {
            print_stderr("new failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        // allocate misc block in process

        size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

        status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                         &proc_misc_block_size,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!success(status))
            return status;

        // copy cmdline

        auto ptr = misc_block.get();

        memcpy(misc_block.get(), cmdline.data(), cmdline.size() * sizeof(char16_t));
        ptr += cmdline.size() * sizeof(char16_t);

        *ptr = 0;
        ptr++;

        params.CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
        params.CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
        params.CommandLine.Buffer = (char16_t*)proc_misc_block;

        if (environment)
            memcpy(ptr, environment, environment_size);
        else {
            memcpy(ptr, peb->ProcessParameters->Environment,
                   peb->ProcessParameters->EnvironmentSize);
        }

        params.Environment = (uint8_t*)proc_misc_block + (ptr - misc_block.get());
        params.EnvironmentSize = environment_size;
    }

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block.get(),
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status))
        return status;

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                  sizeof(params), nullptr);
    if (!success(status))
        return status;

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    auto stack_size = max((size_t)0x100000, sii.MaximumStackSize);

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status))
        return status;

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
    if (!success(status))
        return status;

    thread.reset(h);

    status = NtResumeThread(thread.get(), nullptr);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

static uint32_t do_serial(void*) {
    NTSTATUS status;
    UNICODE_STRING us;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    // FIXME - should be soliciting username and password

#ifdef __aarch64__
    static const char16_t dev[] = u"\\dev\\ttyAMA0";
#else
    static const char16_t dev[] = u"\\dev\\ttyS0";
#endif

    static const char16_t dev2[] = u"\\dev\\hvc0";

    us.Length = us.MaximumLength = (uint16_t)(sizeof(dev) - sizeof(char16_t));
    us.Buffer = (char16_t*)dev;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, 0, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        us.Length = us.MaximumLength = (uint16_t)(sizeof(dev2) - sizeof(char16_t));
        us.Buffer = (char16_t*)dev2;

        status = NtCreateFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb,
                              nullptr, 0, 0, FILE_OPEN, 0, nullptr, 0);
    }

    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::SHARING_VIOLATION) {
        return 1;
    }

    if (!success(status)) {
        print("NtCreateFile returned {}\n", status);
        return 1;
    }

    while (true) {
        nt_status status;
        unique_handle proc, thread;

        status = create_process(u"/bin/usercon.exe", u"usercon.exe serial",
                                environment.data(), h, h, h, proc, thread);
        if (!success(status)) {
            if (status == nt_status::SYSTEM_SHUTDOWN) {
                while (true) {
                    NtDelayExecution(false, nullptr);
                }
            }

            print_stderr("create_process returned {}\n", status);
            continue;
        }

        do {
            status = NtWaitForSingleObject(proc.get(), true, nullptr);

            if (status == nt_status::ALERTED)
                continue;

            if (!success(status)) {
                print_stderr("NtWaitForSingleObject returned {}\n", status);
                break;
            }

            break;
        } while (true);
    }

    NtClose(h);

    return 0;
}

static void prepare_environment() {
    struct passwd pwd, *pwdres;
    char pwdbuf[1024];

    // FIXME - better way of doing this
    static const u16string_view vars[] = {
        u"PATH=/bin",
        u"LD_LIBRARY_PATH=/lib:.",
        u"CONFIG_SITE=/share/config.site",
        u"TZ=GB" // FIXME - should be set by user
    };

    // FIXME - USER environment variable

    static const u16string_view home_prefix = u"HOME=";
    static const u16string_view shell_prefix = u"SHELL=";

    if (getpwuid_r(0, &pwd, pwdbuf, sizeof(pwdbuf), &pwdres) != 0 || !pwdres) {
        print_stderr("getpwuid_r failed for root\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    auto u16homelen = utf8_to_utf16_len(pwdres->pw_dir);
    auto u16shelllen = utf8_to_utf16_len(pwdres->pw_shell);

    size_t len = 1;

    for (auto v : vars) {
        len += v.size() + 1;
    }

    len += home_prefix.size() + u16homelen + 1;
    len += shell_prefix.size() + u16shelllen + 1;

    auto env = (char16_t*)malloc(len * sizeof(char16_t));
    if (!env) {
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    auto ptr = env;

    for (auto v : vars) {
        memcpy(ptr, v.data(), v.size() * sizeof(char16_t));
        ptr += v.size();
        *ptr = 0;
        ptr++;
    }

    memcpy(ptr, home_prefix.data(), home_prefix.size() * sizeof(char16_t));
    ptr += home_prefix.size();
    utf8_to_utf16(span(ptr, u16homelen), pwdres->pw_dir);
    ptr += u16homelen;
    *ptr = 0;
    ptr++;

    memcpy(ptr, shell_prefix.data(), shell_prefix.size() * sizeof(char16_t));
    ptr += shell_prefix.size();
    utf8_to_utf16(span(ptr, u16shelllen), pwdres->pw_shell);
    ptr += u16shelllen;
    *ptr = 0;
    ptr++;

    *ptr = 0;

    environment = span(env, len);
}

static void mount_dev() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_handle vfsctl, dir;
    nt_status status;

    static const u16string_view directory = u"/dev";
    static const string_view mount_type = "devfs";

    auto e = getenv("VFSCTL");

    if (!e || !e[0]) {
        print("Could not read VFSCTL environment variable.\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    uintptr_t h = 0;

    auto e2 = string_view(e);
    while (!e2.empty()) {
        if (e2.front() < '0' || e2.front() > '9') {
            print("Could not parse VFSCTL value \"{}\" as integer.\n", e);
            NtTerminateThread(NtCurrentThread(), nt_status::INTERNAL_ERROR);
            unreachable();
        }

        h *= 10;
        h += (uintptr_t)(e2.front() - '0');
        e2 = string_view(e2.data() + 1, e2.size() - 1);
    }

    vfsctl.reset(h);

    // open or create directory

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(directory.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)directory.data();

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN_IF, FILE_DIRECTORY_FILE, nullptr, 0);
    if (success(status))
        dir.reset(h);
    else {
        print("NtCreateFile returned {} for {}\n", status, directory);
        NtTerminateThread(NtCurrentThread(), status);
        unreachable();
    }

    size_t buflen = sizeof(MOXNOX_VFSCTL_MOUNT) + mount_type.size();
    auto buf = (MOXNOX_VFSCTL_MOUNT*)malloc(buflen);
    if (!buf) {
        print("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    buf->mount_type = sizeof(MOXNOX_VFSCTL_MOUNT);
    buf->source = 0;
    buf->directory = dir.get();
    buf->options = 0;
    memcpy((char*)buf + buf->mount_type, mount_type.data(), mount_type.size());

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_MOUNT, buf,
                                   (uint32_t)buflen, nullptr, 0);
    if (!success(status)) {
        print("IOCTL_MOXNOX_VFSCTL_MOUNT returned {}\n", status);
        NtTerminateThread(NtCurrentThread(), status);
        unreachable();
    }

    free(buf);
}

static unsigned int power_button_thread(void*) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE h;

    static const u16string_view fn = u"/dev/keyboard";

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return 1;
    }

    unique_handle power_button{h};

    while (true) {
        input_event_with_id ev;

        status = NtReadFile(power_button.get(), 0, nullptr, nullptr,
                            &iosb, &ev, sizeof(ev), nullptr, nullptr);

        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print_stderr("NtReadFile returned {}\n", status);
            return 1;
        }

        if (ev.ev.type != evdev_type::EV_KEY)
            continue;

        auto code = (evdev_key_code)ev.ev.code;

        if (code == evdev_key_code::KEY_POWER && ev.ev.value == 1) {
            status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownPowerOff);

            // shouldn't return

            print_stderr("NtShutdownSystem returned {}\n", status);
        }
    }

    return 0;
}

static void thread_start(uint32_t (*func)(void*), void* param) {
    auto ret = func(param);

    RtlExitUserThread((nt_status)ret);
}

static nt_status create_thread(uint32_t (*start)(void*), void* param,
                               unique_handle& handle) {
    nt_status status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    void* stack = nullptr;
    size_t stack_size;

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
        return status;
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
        return status;
    }

    handle.reset(h);

    status = NtResumeThread(h, nullptr);
    if (!success(status)) {
        print_stderr("NtResumeThread returned {}\n", status);
        return status;
    }

    return nt_status::SUCCESS;
}

int main(int argc, char** argv) {
    bool serial_mode = false;

    // FIXME - start services etc.

    for (int i = 1; i < argc; i++) {
        static const char search[] = "console=";

        string_view sv = argv[i];

        if (sv.size() >= sizeof(search) - 1 && !memcmp(sv.data(), search, sizeof(search) - 1)) {
            serial_mode = true;
            break;
        }
    }

    mount_dev();

    prepare_environment();

    if (!serial_mode) {
        unique_handle h;

        auto status = create_thread(do_serial, nullptr, h);
        if (!success(status))
            print_stderr("create_thread returned {}\n", status);
    }

    {
        unique_handle h;

        auto status = create_thread(power_button_thread, nullptr, h);
        if (!success(status))
            print_stderr("create_thread returned {}\n", status);
    }

    while (true) {
        nt_status status;
        unique_handle proc, thread;

        u16string_view cmdline;

        if (serial_mode)
            cmdline = u"usercon.exe serial";
        else
            cmdline = u"usercon.exe";

        status = create_process(u"/bin/usercon.exe", cmdline, environment.data(),
                                nullopt, nullopt, nullopt, proc, thread);
        if (!success(status)) {
            if (status == nt_status::SYSTEM_SHUTDOWN) {
                while (true) {
                    NtDelayExecution(false, nullptr);
                }
            }

            print_stderr("create_process returned {}\n", status);

            continue;
        }

        do {
            status = NtWaitForSingleObject(proc.get(), true, nullptr);

            if (status == nt_status::ALERTED)
                continue;

            if (!success(status)) {
                print_stderr("NtWaitForSingleObject returned {}\n", status);
                break;
            }

            break;
        } while (true);
    }
}
