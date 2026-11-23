// SPDX-License-Identifier: LGPL-2.1-or-later
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <span>
#include <algorithm>

import nt;
import ucrtbase;
import moxnox_ioctls;
import unique_handle;
import ws2_32;
import ipv4;
import ipv6;

#undef errno
import errno;

using namespace std;

static const char16_t netctl_path[] = u"/dev/netctl";

static size_t proto_idx = 0, host_idx = 0;

extern "C"
int socketpair(int domain, int type, int protocol, int* socket_vector) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle netctl;
    MOXNOX_SOCKETPAIR_IN params_in;
    MOXNOX_SOCKETPAIR_OUT params_out;

    // FIXME - correct errno values

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT,
                               nullptr, 0);
    if (success(status))
        netctl.reset(h);
    else {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    params_in.domain = domain;
    params_in.type = type;
    params_in.protocol = protocol;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_SOCKETPAIR, &params_in,
                                   (uint32_t)sizeof(params_in),
                                   &params_out, (uint32_t)sizeof(params_out));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    socket_vector[0] = _open_osfhandle(params_out.h1, 0);

    if (socket_vector[0] == -1) {
        *_errno() = ENFILE;
        NtClose(params_out.h1);
        NtClose(params_out.h2);
        return -1;
    }

    socket_vector[1] = _open_osfhandle(params_out.h2, 0);

    if (socket_vector[1] == -1) {
        close(socket_vector[0]);
        *_errno() = ENFILE;
        NtClose(params_out.h2);
        return -1;
    }

    return 0;
}

extern "C"
ssize_t sendmsg(int s, const msghdr* msg, int flags) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_SENDMSG params, *p;
    size_t sent_bytes = 0, num_fds = 0;

    if (!msg) {
        *_errno() = EINVAL;
        return -1;
    }

    if (msg->msg_iovlen < 0 || msg->msg_namelen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (msg->msg_controllen != 0) {
        auto cmsg = CMSG_FIRSTHDR(msg);

        while (cmsg) {
            if (cmsg->cmsg_len < (int)CMSG_LEN(0)) {
                *_errno() = EINVAL;
                return -1;
            }

            if ((uint8_t*)cmsg + cmsg->cmsg_len > (uint8_t*)msg->msg_control + msg->msg_controllen) {
                *_errno() = EINVAL;
                return -1;
            }

            if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
                *_errno() = EINVAL;
                return -1;
            }

            num_fds += (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);

            cmsg = CMSG_NXTHDR(msg, cmsg);
        }
    }

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return -1;

    if (num_fds == 0) {
        params.num_handles = 0;

        p = &params;
    } else {
        p = (MOXNOX_SOCKET_SENDMSG*)malloc(offsetof(MOXNOX_SOCKET_SENDMSG, handles) + (num_fds * sizeof(HANDLE)));
        if (!p) {
            *_errno() = ENOMEM;
            return -1;
        }

        p->num_handles = num_fds;

        auto* ptr = p->handles;

        auto cmsg = CMSG_FIRSTHDR(msg);

        while (cmsg) {
            if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
                cmsg = CMSG_NXTHDR(msg, cmsg);
                continue;
            }

            auto fds = span((int*)CMSG_DATA(cmsg), (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int));

            for (auto f : fds) {
                *ptr = _get_osfhandle(f);
                if ((intptr_t)*ptr == -1) {
                    free(p);
                    return -1;
                }

                ptr++;
            }

            cmsg = CMSG_NXTHDR(msg, cmsg);
        }
    }

    p->vec = msg->msg_iov;
    p->vec_count = msg->msg_iovlen;
    p->flags = flags & ~MSG_NOSIGNAL;
    p->to = msg->msg_name;
    p->tolen = msg->msg_namelen;

    while (true) {
        size_t sent_bytes2;

        p->sent_bytes = &sent_bytes2;

        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_SOCKET_SENDMSG, p,
                                            (uint32_t)(offsetof(MOXNOX_SOCKET_SENDMSG, handles) + (num_fds * sizeof(HANDLE))),
                                            nullptr, 0);
        if (success(status))
            sent_bytes += sent_bytes2;

        if (status == nt_status::USER_APC) {
            if (sent_bytes > 0)
                break;

            if (mn_handled_nonrestartable_signal_count() != sig_count) {
                *_errno() = EINTR;

                if (num_fds > 0)
                    free(p);

                return -1;
            }

            continue;
        }

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);

            if (num_fds > 0)
                free(p);

            return -1;
        }

        break;
    }

    if (num_fds > 0)
        free(p);

    return (ssize_t)sent_bytes;
}

extern "C"
ssize_t recvmsg(int s, msghdr* msg, int flags) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_RECVMSG params;
    size_t recv_bytes = 0, fromlen;
    size_t num_handles, orig_num_handles;
    HANDLE* handles = nullptr;

    if (!msg) {
        *_errno() = EINVAL;
        return -1;
    }

    if (msg->msg_iovlen < 0 || msg->msg_namelen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (msg->msg_controllen < (int)CMSG_LEN(0))
        num_handles = 0;
    else {
        num_handles = (msg->msg_controllen - CMSG_LEN(0)) / sizeof(int);
        num_handles = min(num_handles, (size_t)256);
    }

    if (num_handles > 0) {
        handles = (HANDLE*)malloc(sizeof(HANDLE) * num_handles);
        if (!handles) {
            *_errno() = ENOMEM;
            return -1;
        }
    }

    orig_num_handles = num_handles;
    num_handles = 0;

    fromlen = msg->msg_namelen;

    params.vec = msg->msg_iov;
    params.vec_count = msg->msg_iovlen;
    params.flags = flags & ~MSG_NOSIGNAL;
    params.from = msg->msg_name;
    params.fromlen = &fromlen;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return -1;

    while (true) {
        size_t num_handles2 = orig_num_handles - num_handles;
        size_t recv_bytes2;

        params.num_handles = &num_handles2;
        params.handles = handles + num_handles;
        params.recv_bytes = &recv_bytes2;

        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_SOCKET_RECVMSG, &params,
                                            (uint32_t)sizeof(params), nullptr, 0);
        if (success(status)) {
            recv_bytes += recv_bytes2;
            num_handles += num_handles2;
        }

        if (status == nt_status::USER_APC) {
            if (recv_bytes > 0)
                break;

            if (mn_handled_nonrestartable_signal_count() != sig_count) {
                *_errno() = EINTR;

                if (handles)
                    free(handles);

                return -1;
            }

            continue;
        }

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);

            if (handles)
                free(handles);

            return -1;
        }

        break;
    }

    if (num_handles > 0 && orig_num_handles > 0) {
        auto n = min(num_handles, orig_num_handles);
        auto cmsg = CMSG_FIRSTHDR(msg);

        cmsg->cmsg_len = (int)CMSG_LEN(n * sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        msg->msg_controllen = min((int)CMSG_SPACE(n * sizeof(int)), msg->msg_controllen);

        auto fds = span((int*)CMSG_DATA(cmsg), n);

        for (size_t i = 0; i < n; i++) {
            fds[i] = _open_osfhandle(handles[i], 0);

            if (fds[i] == (intptr_t)-1) {
                for (size_t j = 0; j < i; j++) {
                    close(fds[j]);
                }

                for (size_t j = i; j < n; j++) {
                    NtClose(handles[j]);
                }

                free(handles);

                return -1;
            }
        }
    } else
        msg->msg_controllen = 0;

    if (handles)
        free(handles);

    msg->msg_namelen = (int)fromlen;
    msg->msg_flags = 0;

    if (num_handles > orig_num_handles)
        msg->msg_flags |= MSG_CTRUNC;

    return (ssize_t)recv_bytes;
}

extern "C"
servent* getservent() {
    return nullptr;
}

extern "C"
void setservent(int /*stayopen*/) {
    // nop
}

extern "C"
void endservent() {
    // nop
}

extern "C"
hostent* gethostent() {
    static hostent ret;
    static const char* aliases = nullptr;
    static char* addrs[2];
    static const auto localhost6 = parse_ipv6_address("::1");
    static const auto localhost4 = parse_ipv4_address("127.0.0.1");

    if (host_idx > 1)
        return nullptr;

    ret.h_name = (char*)"localhost";
    ret.h_aliases = (char**)&aliases;

    if (host_idx == 0) {
        addrs[0] = (char*)&localhost6;
        addrs[1] = nullptr;

        ret.h_addrtype = AF_INET6;
        ret.h_length = sizeof(ipv6_address);
        ret.h_addr_list = addrs;
    } else if (host_idx == 1) {
        addrs[0] = (char*)&localhost4;
        addrs[1] = nullptr;

        ret.h_addrtype = AF_INET;
        ret.h_length = sizeof(ipv4_address);
        ret.h_addr_list = addrs;
    }

    host_idx++;

    return &ret;
}

extern "C"
void sethostent(int /*stayopen*/) {
    host_idx = 0;
}

extern "C"
void endhostent() {
    host_idx = 0;
}

extern "C"
int accept4(int s, struct sockaddr* addr, socklen_t* addrlen, int flag) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_ACCEPT_IN params_in;
    MOXNOX_SOCKET_ACCEPT_OUT params_out;

    if (flag & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
        *_errno() = EINVAL;
        return -1;
    }

    params_in.addr = addr;
    params_in.addrlen = addrlen ? *addrlen : 0;

    auto h = _get_osfhandle(s);
    if (h == -1)
        return -1;

    while (true) {
        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_SOCKET_ACCEPT, &params_in,
                                            (uint32_t)sizeof(params_in),
                                            &params_out, (uint32_t)sizeof(params_out));

        if (status == nt_status::USER_APC) {
            if (mn_handled_nonrestartable_signal_count() != sig_count) {
                *_errno() = EINTR;
                return -1;
            }

            continue;
        }

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        break;
    }

    if (addrlen)
        *addrlen = params_out.addrlen;

    if (flag & SOCK_NONBLOCK) {
        bool nonblock = true;

        auto status = NtSetInformationFile(params_out.sock, &iosb, &nonblock,
                                           sizeof(nonblock), FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            NtClose(params_out.sock);
            return -1;
        }
    }

    auto cloexec = !!(flag & SOCK_CLOEXEC);

    auto fd = _open_osfhandle((intptr_t)params_out.sock, cloexec ? O_CLOEXEC : 0);
    if (fd == -1) {
        NtClose(params_out.sock);
        return -1;
    }

    return fd;
}

extern "C"
protoent* getprotoent() {
    static protoent ret;
    static char* aliases[3];

    if (proto_idx == sizeof(protos) / sizeof(*protos))
        return nullptr;

    auto& p = protos[proto_idx];

    aliases[0] = (char*)p.alias1;
    aliases[1] = (char*)p.alias2;
    aliases[2] = nullptr;

    ret.p_name = (char*)p.name;
    ret.p_aliases = aliases;
    ret.p_proto = p.num;

    proto_idx++;

    return &ret;
}

extern "C"
void setprotoent(int /*stayopen*/) {
    proto_idx = 0;
}

extern "C"
void endprotoent() {
    proto_idx = 0;
}

extern "C"
netent* getnetent() {
    return nullptr;
}

extern "C"
void setnetent(int /*stayopen*/) {
    // nop
}

extern "C"
void endnetent() {
    // nop
}

extern "C"
netent* getnetbyname(const char* /*name*/) {
    return nullptr;
}

extern "C"
netent* getnetbyaddr(uint32_t /*net*/, int /*type*/) {
    return nullptr;
}
