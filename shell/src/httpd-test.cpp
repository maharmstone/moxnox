// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string_view>
#include <bit>
#include <utility>
#include <span>
#include <algorithm>

#undef errno
import errno;

import nt;
import pe;
import ntdll;
import ws2_32;
import print;
import ipv6;

using namespace std;

static void thread_start(uint32_t (*func)(void*), void* param) {
    auto ret = func(param);

    RtlExitUserThread((nt_status)ret);
}

static HANDLE create_thread(uint32_t (*start)(void*), void* param) {
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

    status = NtResumeThread(h, nullptr);
    if (!success(status)) {
        NtClose(h);

        print_stderr("NtResumeThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), status);
        unreachable();
    }

    return h;
}

static uint32_t socket_thread(void* ctx) {
    auto sock = (SOCKET)(uintptr_t)ctx;
    char buf[4096];
    char* req = nullptr;
    size_t reqlen = 0;

    while (true) {
#if 0
        auto status = NtReadFile((HANDLE)sock, 0, nullptr, nullptr, &iosb,
                                 buf, sizeof(buf), nullptr, nullptr);
        if (status == nt_status::END_OF_FILE) {
            closesocket(sock);
            return 0;
        } else if (!success(status)) {
            print_stderr("NtReadFile returned {}\n", status);
            closesocket(sock);
            return 1;
        }

        auto bytes = iosb.Information;
#endif

        auto bytes = recv(sock, buf, sizeof(buf), 0);
        if (bytes == 0) {
            closesocket(sock);
            return 0;
        } else if (bytes < 0) {
            print_stderr("recv returned {} (errno {})\n", bytes,
                            errno_to_string_view(*_errno()));
            closesocket(sock);
            return 1;
        }

        req = (char*)realloc(req, reqlen + (size_t)bytes);
        memcpy(req + reqlen, buf, (size_t)bytes);
        reqlen += (size_t)bytes;

        auto sv = string_view(req, reqlen);

        auto end = sv.find("\r\n\r\n");

        if (end == string_view::npos)
            continue;

        sv = string_view(sv.data(), end);

        auto first_line = string_view(sv.data(), sv.find("\r\n"));

        string_view verb;
        if (auto sp = first_line.find(" "); sp != string_view::npos) {
            verb = string_view(first_line.data(), sp);
            first_line = string_view(first_line.data() + sp + 1,
                                     first_line.size() - sp - 1);
        } else {
            verb = first_line;
            first_line = "";
        }

        string_view path;
        if (auto sp = first_line.find(" "); sp != string_view::npos) {
            path = string_view(first_line.data(), sp);
            first_line = string_view(first_line.data() + sp + 1,
                                     first_line.size() - sp - 1);
        } else {
            path = first_line;
            first_line = "";
        }

        string_view resp;

        if (verb != "GET")
            resp = "HTTP/1.0 400 Bad Request\r\n\r\n";
        else if (path != "/")
            resp = "HTTP/1.0 404 Not Found\r\n\r\n";
        else
            resp = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n<!DOCTYPE html>\n<html><body><h1>Hello from Moxnox</h1><p>Hello world</p></body></html>";

        print("Received: \"{}\", \"{}\"\n", verb, first_line);

        auto resp2 = span(resp.data(), resp.size());

        while (true) {
#if 0
            status = NtWriteFile((HANDLE)sock, 0, nullptr, nullptr, &iosb, resp2.data(),
                                 (uint32_t)resp2.size(), nullptr, nullptr);
            if (status == nt_status::END_OF_FILE) {
                closesocket(sock);
                return 0;
            } else if (!success(status)) {
                print_stderr("NtWriteFile returned {}\n", status);
                closesocket(sock);
                return 1;
            }

            if (iosb.Information == resp2.size())
                break;

            resp2 = span(resp2.data() + iosb.Information,
                         resp2.size() - iosb.Information);
#endif
            auto bytes2 = send(sock, resp2.data(), (int)resp2.size(), 0);
            if (bytes2 < 0) {
                print_stderr("send returned {} (errno {})\n", bytes2,
                            errno_to_string_view(*_errno()));
                closesocket(sock);
                return 1;
            }

            if ((size_t)bytes2 == resp2.size())
                break;

            resp2 = span(resp2.data() + bytes2, resp2.size() - (size_t)bytes2);
        }

        break;
    }

    closesocket(sock);

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
    addr.sin6_port = byteswap((uint16_t)80);

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

    print("Web server listening on port 80...\n");

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

        auto h = create_thread(socket_thread, (void*)(uintptr_t)client_sock);

        NtClose(h);
    }

    closesocket(server_sock);

    return 0;
}
