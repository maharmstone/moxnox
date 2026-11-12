// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <string_view>
#include <bit>
#include <utility>
#include <span>
#include <tuple>
#include <optional>
#include <algorithm>

#undef errno
import errno;

import nt;
import pe;
import ntdll;
import ws2_32;
import print;
import ipv6;
import unique_handle;
import kernelbase;
import format;
import unicode;
import moxnox_ioctls;
import ucrtbase;

using namespace std;

class unique_fd {
public:
    unique_fd() : fd(-1) {
    }

    explicit unique_fd(int fd) : fd(fd) {
    }

    unique_fd(unique_fd&& that) noexcept {
        fd = that.fd;
        that.fd = -1;
    }

    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;

    unique_fd& operator=(unique_fd&& that) noexcept {
        if (fd >= 0)
            _close(fd);

        fd = that.fd;
        that.fd = -1;

        return *this;
    }

    ~unique_fd() {
        if (fd < 0)
            return;

        _close(fd);
    }

    explicit operator bool() const noexcept {
        return fd != -1;
    }

    void reset(int new_fd = -1) noexcept {
        if (fd >= 0)
            _close(fd);

        fd = new_fd;
    }

    int get() const noexcept {
        return fd;
    }

private:
    int fd;
};

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

static void print_to_handle2(HANDLE h, string_view msg) {
    auto msg2 = span(msg.data(), msg.size());

    while (true) {
        nt_status status;
        IO_STATUS_BLOCK iosb;

        status = NtWriteFile((HANDLE)h, 0, nullptr, nullptr, &iosb, msg2.data(),
                                (uint32_t)msg2.size(), nullptr, nullptr);
        if (status == nt_status::END_OF_FILE)
            return;
        else if (!success(status)) {
            print_stderr("NtWriteFile returned {}\n", status);
            return;
        }

        if (iosb.Information == msg2.size())
            break;

        msg2 = span(msg2.data() + iosb.Information,
                    msg2.size() - iosb.Information);
    }
}

template<typename... Args>
void print_to_handle(HANDLE h, type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];

    auto sp = span(buf, sizeof(buf));

    sp = format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    print_to_handle2(h, sv);
}

static optional<pair<unique_handle, unique_handle>> create_pty(HANDLE out) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    nt_status status;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_IN ptyparams;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_OUT ptyinfo;
    unique_handle ptyctl;

    static const u16string_view ptyctl_fn = u"/dev/ptyctl";

    // open ptyctl

    us.Length = us.MaximumLength = (uint16_t)(ptyctl_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)ptyctl_fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print_to_handle(out, "NtOpenFile returned {} for /dev/ptyctl\n", status);
        return nullopt;
    }

    ptyctl.reset(h);

    // create pty

    ptyparams.cols = 80;
    ptyparams.rows = 25;

    status = NtDeviceIoControlFile(ptyctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTYCTL_CREATE_PTY, &ptyparams,
                                   sizeof(ptyparams), &ptyinfo, sizeof(ptyinfo));
    if (!success(status)) {
        print_to_handle(out, "IOCTL_MOXNOX_PTYCTL_CREATE_PTY returned {}\n", status);
        return nullopt;
    }

    ptyctl.reset();

    return make_pair(unique_handle{ptyinfo.client_handle}, unique_handle{ptyinfo.server_handle});
}

static uint32_t socket_thread(void* ctx) {
    unique_fd sockfd{(int)(uintptr_t)ctx};
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    struct passwd pwd, *pwdres;
    nt_status status;
    IO_STATUS_BLOCK iosb;
    char pwdbuf[1024];

    auto sockh = (HANDLE)_get_osfhandle(sockfd.get());
    if ((intptr_t)sockh == -1)
        return 1;

    // FIXME - this isn't really telnet, telnet has an actual protocol

    auto ptyinfo = create_pty(sockh);
    if (!ptyinfo.has_value())
        return 1;

    auto& client_handle = ptyinfo->first;
    auto& server_handle = ptyinfo->second;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = client_handle.get();
    si.hStdOutput = client_handle.get();
    si.hStdError = client_handle.get();

    if (getpwuid_r(getuid(), &pwd, pwdbuf, sizeof(pwdbuf), &pwdres) != 0 || !pwdres) {
        print_stderr("getpwuid_r failed\n");
        return 1;
    }

    if (!pwdres->pw_shell[0]) {
        print_stderr("shell not set\n");
        return 1;
    }

    if (!pwdres->pw_dir[0]) {
        print_stderr("home directory not set\n");
        return 1;
    }

    auto u16shelllen = utf8_to_utf16_len(pwdres->pw_shell);
    auto u16homelen = utf8_to_utf16_len(pwdres->pw_dir);

    auto u16shell = (char16_t*)malloc((u16shelllen + 1) * sizeof(char16_t));
    if (!u16shell) {
        print_stderr("malloc failed\n");
        return 1;
    }

    auto u16home = (char16_t*)malloc((u16homelen + 1) * sizeof(char16_t));
    if (!u16home) {
        print_stderr("malloc failed\n");
        free(u16shell);
        return 1;
    }

    utf8_to_utf16(span(u16shell, u16shelllen), pwdres->pw_shell);
    u16shell[u16shelllen] = 0;

    utf8_to_utf16(span(u16home, u16homelen), pwdres->pw_dir);
    u16home[u16homelen] = 0;

    if (!CreateProcessW(u16shell, nullptr, nullptr, nullptr, false, CREATE_SUSPENDED,
                        nullptr, u16home, &si, &pi)) {
        print_to_handle(sockh, "CreateProcessW failed (last error {})\n", GetLastError());

        free(u16home);
        free(u16shell);

        return 1;
    }

    free(u16home);
    free(u16shell);

    unique_handle thread{pi.hThread};
    unique_handle proc{pi.hProcess};

    status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxNewSession,
                                     nullptr, 0);
    if (!success(status)) {
        print_stderr("NtSetInformationProcess returned {} for MoxnoxNewSession\n", status);
        NtTerminateProcess(proc.get(), status);
        return 1;
    }

    HANDLE pty = client_handle.get();

    status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxSetControllingTerminal,
                                     &pty, sizeof(pty));
    if (!success(status)) {
        print_stderr("NtSetInformationProcess returned {} for MoxnoxSetControllingTerminal\n", status);
        NtTerminateProcess(proc.get(), status);
        return 1;
    }

    auto pgid = (uintptr_t)pi.dwProcessId;

    status = NtDeviceIoControlFile(pty, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP,
                                   &pgid, sizeof(pgid), nullptr, 0);
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP returned {}\n", status);
        NtTerminateProcess(proc.get(), status);
        return false;
    }

    status = NtResumeThread(thread.get(), nullptr);
    if (!success(status)) {
        print_stderr("NtResumeThread returned {}\n", status);
        NtTerminateProcess(proc.get(), status);
        return 1;
    }

    bool program_running = true;

    while (program_running) {
        array<HANDLE, 3> objs;
        nt_status status;
        IO_STATUS_BLOCK iosb;
        char s[4096];

        objs[0] = server_handle.get();
        objs[1] = proc.get();
        objs[2] = sockh;

        status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                          false, nullptr);
        if (!success(status)) {
            print_to_handle(sockh, "NtWaitForMultipleObjects returned {}\n", status);
            break;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: { // program wrote to stdout or stderr, forward to socket
                status = NtReadFile(server_handle.get(), 0, nullptr, nullptr, &iosb,
                                    s, sizeof(s), nullptr, nullptr);

                if (!success(status)) {
                    print_to_handle(sockh, "NtReadFile returned {}\n", status);
                    program_running = false;
                    break;
                }

                if (iosb.Information == 0)
                    continue;

                auto sp = span(s, iosb.Information);

                while (!sp.empty()) {
                    status = NtWriteFile(sockh, 0, nullptr, nullptr,
                                         &iosb, sp.data(), (uint32_t)sp.size(), nullptr, nullptr);
                    if (!success(status)) {
                        print_to_handle(sockh, "NtReadFile returned {}\n", status);
                        program_running = false;
                        break;
                    }

                    sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
                }

                break;
            }

            case 1: // program finished
                program_running = false;
                break;

            case 2: { // received data from socket, forward to program stdin
                status = NtReadFile(sockh, 0, nullptr, nullptr, &iosb,
                                    s, sizeof(s), nullptr, nullptr);

                if (!success(status)) {
                    print_to_handle(sockh, "NtReadFile returned {}\n", status);
                    program_running = false;
                    break;
                }

                if (iosb.Information == 0)
                    continue;

                auto sp = span(s, iosb.Information);

                while (!sp.empty()) {
                    status = NtWriteFile(server_handle.get(), 0, nullptr, nullptr,
                                         &iosb, sp.data(), (uint32_t)sp.size(), nullptr, nullptr);
                    if (!success(status)) {
                        print_to_handle(sockh, "NtReadFile returned {}\n", status);
                        program_running = false;
                        break;
                    }

                    sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
                }

                break;
            }
        }
    }

    return 0;
}

int main() {
    struct sockaddr_in6 addr;

    auto server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_sock < 0) {
        print_stderr("socket returned {} (errno {})\n", server_sock, errno_to_string_view(*_errno()));
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = byteswap((uint16_t)23);

    int ret = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret) {
        print_stderr("bind returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    ret = listen(server_sock, 5);
    if (ret) {
        print_stderr("listen returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    print("Telnet server listening on port 23...\n");

    while (true) {
        struct sockaddr_in6 client_addr;
        int client_addrlen = sizeof(client_addr);

        auto client_sock = accept(server_sock, (struct sockaddr*)&client_addr,
                                  &client_addrlen);
        if (client_sock < 0) {
            print_stderr("accept returned {} (errno {})\n", client_sock,
                         errno_to_string_view(*_errno()));
            closesocket(server_sock);
            return 1;
        }

        if (client_addrlen != sizeof(client_addr)) {
            print_stderr("client_addrlen was {}, expected {}\n", client_addrlen,
                         sizeof(client_addr));
            closesocket(client_sock);
            closesocket(server_sock);
            return 1;
        }

        print("Connected from [{}]:{}, socket {}\n", *(ipv6_address*)&client_addr.sin6_addr,
              byteswap(client_addr.sin6_port), (uintptr_t)client_sock);

        create_thread(socket_thread, (void*)(uintptr_t)client_sock);
    }

    closesocket(server_sock);

    return 0;
}
