// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <bit>
#include <span>
#include <string_view>
#include <charconv>
#include <utility>

import nt;
import ucrtbase;
import ws2_32;
import unique_handle;
import moxnox_ioctls;
import print;
import format;
import ipv4;
import ipv6;
import timepage;

#undef errno
import errno;

static const char16_t netctl_path[] = u"/dev/netctl";

using namespace std;

extern "C"
SOCKET socket(int af, int type, int protocol) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle netctl;
    MOXNOX_NETCTL_SOCKET params;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (success(status))
        netctl.reset(h);
    else {
        *_errno() = nt_status_to_errno(status);
        return INVALID_SOCKET;
    }

    params.af = af;
    params.type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    params.protocol = protocol;
    params.h = &h;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_SOCKET, &params,
                                   (uint32_t)sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = EINVAL; // FIXME?
        return INVALID_SOCKET;
    }

    if (type & SOCK_NONBLOCK) {
        bool nonblock = true;

        auto status = NtSetInformationFile(h, &iosb, &nonblock, sizeof(nonblock),
                                           FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            NtClose(h);
            return -1;
        }
    }

    auto fd = _open_osfhandle((intptr_t)h, type & SOCK_CLOEXEC ? O_CLOEXEC : 0);
    if (fd == -1)
        return INVALID_SOCKET;

    return (SOCKET)fd;
}

extern "C"
int recvfrom(SOCKET s, char* buf, int len, int flags, sockaddr* from,
             int* fromlen) {
    msghdr msg;
    iovec iov;

    iov.iov_base = (void*)buf;
    iov.iov_len = len;

    msg.msg_name = (void*)from;
    msg.msg_namelen = fromlen ? *fromlen : 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = nullptr;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    auto ret = recvmsg(s, &msg, flags);
    if (ret == -1)
        return SOCKET_ERROR;

    if (fromlen)
        *fromlen = msg.msg_namelen;

    return (int)ret;
}

extern "C"
int recv(SOCKET s, char* buf, int len, int flags) {
    return recvfrom(s, buf, len, flags, nullptr, nullptr);
}

extern "C"
int sendto(SOCKET s, const char* buf, int len, int flags, const sockaddr* to,
           int tolen) {
    msghdr msg;
    iovec iov;

    iov.iov_base = (void*)buf;
    iov.iov_len = len;

    msg.msg_name = (void*)to;
    msg.msg_namelen = tolen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = nullptr;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    auto ret = sendmsg(s, &msg, flags);
    if (ret == -1)
        return SOCKET_ERROR;

    return (int)ret;
}

extern "C"
int send(SOCKET s, const char* buf, int len, int flags) {
    return sendto(s, buf, len, flags, nullptr, 0);
}

extern "C"
int bind(SOCKET s, const sockaddr* addr, int addrlen) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_BIND, addr,
                                        (uint32_t)addrlen, nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int connect(SOCKET s, const sockaddr* addr, int addrlen) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_CONNECT, addr,
                                        (uint32_t)addrlen, nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int listen(SOCKET s, int backlog) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_LISTEN, &backlog,
                                        (uint32_t)sizeof(backlog), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
SOCKET accept(SOCKET s, sockaddr* addr, int* addrlen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_ACCEPT_IN params_in;
    MOXNOX_SOCKET_ACCEPT_OUT params_out;

    params_in.addr = addr;
    params_in.addrlen = addrlen ? *addrlen : 0;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    while (true) {
        auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_SOCKET_ACCEPT, &params_in,
                                            (uint32_t)sizeof(params_in),
                                            &params_out, (uint32_t)sizeof(params_out));
        if (status == nt_status::USER_APC)
            continue;

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return SOCKET_ERROR;
        }

        break;
    }

    if (addrlen)
        *addrlen = params_out.addrlen;

    auto fd = _open_osfhandle((intptr_t)params_out.sock, 0);
    if (fd == -1) {
        NtClose(params_out.sock);
        return INVALID_SOCKET;
    }

    return (SOCKET)fd;
}

extern "C"
uint32_t htonl(uint32_t hostlong) {
    return byteswap(hostlong);
}

extern "C"
uint16_t htons(uint16_t hostshort) {
    return byteswap(hostshort);
}

extern "C"
uint32_t ntohl(uint32_t netlong) {
    return byteswap(netlong);
}

extern "C"
uint16_t ntohs(uint16_t netshort) {
    return byteswap(netshort);
}

extern "C"
int setsockopt(SOCKET s, int level, int optname, const void* optval,
               int optlen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_SETSOCKOPT params;

    auto h = _get_osfhandle(s);
    if (h == -1)
        return -1;

    params.level = level;
    params.optname = optname;
    params.optval = optval;
    params.optlen = optlen;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_SETSOCKOPT, &params,
                                        sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int getsockname(SOCKET s, sockaddr* name, int* namelen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_GETSOCKNAME params;

    if (*namelen < 0) {
        *_errno() = EINVAL;
        return SOCKET_ERROR;
    }

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    params.addr = name;
    params.addrlen = (size_t)*namelen;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_GETSOCKNAME, &params,
                                        sizeof(params), namelen, sizeof(*namelen));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int getpeername(SOCKET s, sockaddr* name, int* namelen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_GETSOCKNAME params;

    if (*namelen < 0) {
        *_errno() = EINVAL;
        return SOCKET_ERROR;
    }

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    params.addr = name;
    params.addrlen = (size_t)*namelen;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_GETPEERNAME, &params,
                                        sizeof(params), namelen, sizeof(*namelen));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int closesocket(SOCKET s) {
    return _close((int)s);
}

extern "C"
int gethostname(char* name, int namelen) {
    char buf[sizeof(time_page_struct::hostname)];

    if (namelen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    read_host_name(buf);

    auto len = strlen(buf);

    memcpy(name, buf, min(len, (size_t)namelen));

    if (len < (size_t)namelen)
        name[len] = 0;

    return 0;
}

extern "C"
int getsockopt(int s, int level, int optname, void* optval, socklen_t* optlen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_GETSOCKOPT params;

    if (*optlen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto h = _get_osfhandle(s);
    if (h == -1)
        return -1;

    params.level = level;
    params.optname = optname;
    params.optval = optval;
    params.optlen = optlen;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_GETSOCKOPT, &params,
                                        sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int shutdown(int s, int how) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle(s);
    if (h == -1)
        return -1;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_SHUTDOWN, &how,
                                        sizeof(how), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

static const char* inet_ntop_v4(const in_addr_t& src, char* dst,
                                socklen_t size) {
    if (size < 0) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    span sp(dst, (size_t)size);

    sp = format(sp, "{}.{}.{}.{}", (uint8_t)(src & 0xff),
                (uint8_t)((src >> 8) & 0xff), (uint8_t)((src >> 16) & 0xff),
                (uint8_t)(src >> 24));

    if (sp.empty()) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    sp[0] = 0;

    return dst;
}

static const char* inet_ntop_v6(const ipv6_address& src, char* dst,
                                socklen_t size) {
    if (size < 0) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    span sp(dst, (size_t)size);

    sp = format(sp, "{}", src);

    if (sp.empty()) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    sp[0] = 0;

    return dst;
}

extern "C"
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    switch (af) {
        case AF_INET:
            return inet_ntop_v4(*(in_addr_t*)src, dst, size);

        case AF_INET6:
            return inet_ntop_v6(*(ipv6_address*)src, dst, size);

        default:
            *_errno() = EAFNOSUPPORT;
            return nullptr;
    }
}

static int inet_pton_v4(const char* src, in_addr_t& dst) {
    // FIXME - should only be accepting addresses of form x.x.x.x

    auto ret = try_parse_ipv4_address(src);

    if (!ret.has_value())
        return 0;

    dst = *(in_addr_t*)&*ret;

    return 1;
}

static int inet_pton_v6(const char* src, ipv6_address& dst) {
    // FIXME - should be accepting addresses of form x:x:x:x:x:x:d.d.d.d

    auto ret = try_parse_ipv6_address(src);

    if (!ret.has_value())
        return 0;

    dst = *ret;

    return 1;
}

extern "C"
int inet_pton(int af, const char* src, void* dst) {
    switch (af) {
        case AF_INET:
            return inet_pton_v4(src, *(in_addr_t*)dst);

        case AF_INET6:
            return inet_pton_v6(src, *(ipv6_address*)dst);

        default:
            *_errno() = EAFNOSUPPORT;
            return -1;
    }
}

extern "C"
in_addr_t inet_addr(const char* cp) {
    auto addr = try_parse_ipv4_address(cp);

    if (!addr.has_value())
        return (in_addr_t)-1;

    return (uint32_t)(addr->v[0] << 24) | (uint32_t)(addr->v[1] << 16) | (uint32_t)(addr->v[2] << 8) | (uint32_t)addr->v[3];
}

extern "C"
char* inet_ntoa(in_addr in) {
    auto addr = *(ipv4_address*)&in;

    static char buf[16];

    span sp(buf, sizeof(buf));

    sp = format(sp, "{}", addr);
    sp[0] = 0;

    return buf;
}

extern "C"
servent* getservbyname(const char* /*name*/, const char* /*proto*/) {
    return nullptr;
}

extern "C"
servent* getservbyport(int /*port*/, const char* /*proto*/) {
    return nullptr;
}

extern "C"
struct protoent* getprotobynumber(int proto) {
    static protoent ret;
    static char* aliases[3];

    for (const auto& p : protos) {
        if (p.num != proto)
            continue;

        aliases[0] = (char*)p.alias1;
        aliases[1] = (char*)p.alias2;
        aliases[2] = nullptr;

        ret.p_name = (char*)p.name;
        ret.p_aliases = aliases;
        ret.p_proto = p.num;

        return &ret;
    }

    return nullptr;
}

extern "C"
struct protoent* getprotobyname(const char* name) {
    static protoent ret;
    static char* aliases[3];

    for (const auto& p : protos) {
        if (strcmp(p.name, name) && (!p.alias1 || strcmp(p.alias1, name)) && (!p.alias2 || strcmp(p.alias2, name)))
            continue;

        aliases[0] = (char*)p.alias1;
        aliases[1] = (char*)p.alias2;
        aliases[2] = nullptr;

        ret.p_name = (char*)p.name;
        ret.p_aliases = aliases;
        ret.p_proto = p.num;

        return &ret;
    }

    return nullptr;
}
