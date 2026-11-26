module;

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <array>

module init;

import nt;
import unique_fd;
import print;
import ucrtbase;
import pagesize;

#undef errno
import errno;

using namespace std;

static const char SOCKET_PATH[] = "/run/log";

unsigned int log_thread(void*) {
    unique_fd file, sock;
    sockaddr_un name;
    pollfd pfd;

    if (auto ret = mkdir("/var", 0755); ret == -1) {
        if (*_errno() != EEXIST) {
            print_stderr("mkdir returned {} for /var (errno {})\n", ret,
                         errno_to_string_view(*_errno()));
            NtSetEvent(log_started_event.get(), nullptr);
            return 1;
        }
    }

    if (auto ret = mkdir("/var/log", 0755); ret == -1) {
        if (*_errno() != EEXIST) {
            print_stderr("mkdir returned {} for /var/log (errno {})\n", ret,
                         errno_to_string_view(*_errno()));
            NtSetEvent(log_started_event.get(), nullptr);
            return 1;
        }
    }

    if (auto ret = mkdir("/run", 0755); ret == -1) {
        if (*_errno() != EEXIST) {
            print_stderr("mkdir returned {} for /run (errno {})\n", ret,
                         errno_to_string_view(*_errno()));
            NtSetEvent(log_started_event.get(), nullptr);
            return 1;
        }
    }

    if (auto fd = open("/var/log/messages", O_CREAT | O_APPEND | O_WRONLY, 0644); fd < 0) {
        print_stderr("open returned {} for /var/log/messages (errno {})\n", fd,
                     errno_to_string_view(*_errno()));
        NtSetEvent(log_started_event.get(), nullptr);
        return 1;
    } else
        file.reset(fd);

    if (auto fd = socket(AF_UNIX, SOCK_DGRAM, 0); fd < 0) {
        print_stderr("socket returned {} (errno {})\n", fd,
                     errno_to_string_view(*_errno()));
        NtSetEvent(log_started_event.get(), nullptr);
        return 1;
    } else
        sock.reset(fd);

    auto flags = fcntl(sock.get(), F_GETFL);
    if (flags == -1) {
        print_stderr("fcntl returned {} (errno {})\n", flags,
                     errno_to_string_view(*_errno()));
        NtSetEvent(log_started_event.get(), nullptr);
        return 1;
    }

    if (auto ret = fcntl(sock.get(), F_SETFL, flags | O_NONBLOCK); ret == -1) {
        print_stderr("fcntl returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        NtSetEvent(log_started_event.get(), nullptr);
        return 1;
    }

    unlink(SOCKET_PATH);

    name.sun_family = AF_UNIX;
    strcpy(name.sun_path, SOCKET_PATH);

    if (auto ret = bind(sock.get(), (sockaddr*)&name, sizeof(name))) {
        print_stderr("bind returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        NtSetEvent(log_started_event.get(), nullptr);
        return 1;
    }

    NtSetEvent(log_started_event.get(), nullptr);

    pfd.fd = sock.get();
    pfd.events = POLLIN;

    while (true) {
        auto ret = poll(&pfd, 1, -1);
        if (ret == -1) {
            print_stderr("poll returned {} (errno {})\n", ret,
                         errno_to_string_view(*_errno()));
            return 1;
        }

        if (pfd.revents & POLLIN) {
            char buf[PAGE_SIZE];
            char timestamp[18];
            array<iovec, 3> iov;
            time_t t;
            struct tm tm;

            static const char nl[] = "\n";

            auto bytes = recvfrom(sock.get(), buf, sizeof(buf), 0, nullptr,
                                  nullptr);
            if (bytes < 0) {
                print_stderr("recvfrom returned {} (errno {})\n", bytes,
                             errno_to_string_view(*_errno()));
                return 1;
            }

            while (bytes > 0 && (buf[bytes - 1] == '\r' || buf[bytes - 1] == '\n')) {
                bytes--;
            }

            t = time(nullptr);

            if (!localtime_r(&t, &tm)) {
                print_stderr("localtime_r failed (errno {})\n",
                             errno_to_string_view(*_errno()));
                return 1;
            }

            auto timestamplen = strftime(timestamp, sizeof(timestamp), "%b %e %H:%M:%S: ", &tm);
            if (timestamplen == 0) {
                print_stderr("strftime failed (errno {})\n",
                             errno_to_string_view(*_errno()));
                return 1;
            }

            iov[0].iov_base = timestamp;
            iov[0].iov_len = timestamplen;
            iov[1].iov_base = buf;
            iov[1].iov_len = bytes;
            iov[2].iov_base = (void*)nl;
            iov[2].iov_len = sizeof(nl) - 1;

            if (auto ret = writev(file.get(), iov.data(), iov.size()); ret == -1) {
                print_stderr("writev returned {} (errno {})\n", ret,
                             errno_to_string_view(*_errno()));
                return 1;
            }
        }
    }

    return 0;
}
