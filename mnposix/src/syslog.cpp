// SPDX-License-Identifier: LGPL-2.1-or-later
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string_view>
#include <array>

import pagesize;
import ntdll;

using namespace std;

static int log_mask = 0xff;

static RTL_SRWLOCK syslog_lock;
static const char* syslog_ident;
static size_t syslog_ident_size;
static int syslog_socket = -1;

static const sockaddr_un addr = { AF_UNIX, "/run/log" };
static const string_view SPACE = " ";

extern "C"
void openlog(const char* ident, int /*logopt*/, int /*facility*/) {
    // FIXME - logopt
    // FIXME - facility

    RtlAcquireSRWLockExclusive(&syslog_lock);

    syslog_ident = ident;

    if (ident)
        syslog_ident_size = min(PAGE_SIZE - SPACE.size() - 1, strlen(ident));
    else
        syslog_ident_size = 0;

    if (syslog_socket == -1)
        syslog_socket = socket(AF_UNIX, SOCK_DGRAM, 0);

    RtlReleaseSRWLockExclusive(&syslog_lock);
}

extern "C"
void syslog(int /*priority*/, const char* message, ...) {
    char buf[PAGE_SIZE + 1];
    va_list ap;
    msghdr msg;
    array<iovec, 3> iov;

    RtlAcquireSRWLockExclusive(&syslog_lock);

    size_t max_msg = PAGE_SIZE - syslog_ident_size - SPACE.size();

    // FIXME - priority
    // FIXME - %m

    if (syslog_socket == -1)
        syslog_socket = socket(AF_UNIX, SOCK_DGRAM, 0);

    va_start(ap, message);
    auto n = vsnprintf(buf, max_msg + 1, message, ap);
    va_end(ap);

    if (n < 0) {
        RtlReleaseSRWLockExclusive(&syslog_lock);
        return;
    }

    iov[0].iov_base = (void*)syslog_ident;
    iov[0].iov_len = syslog_ident_size;
    iov[1].iov_base = (void*)SPACE.data();
    iov[1].iov_len = SPACE.size();
    iov[2].iov_base = buf;
    iov[2].iov_len = min((size_t)n, max_msg);

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov = iov.data();
    msg.msg_iovlen = iov.size();

    sendmsg(syslog_socket, &msg, 0);

    RtlReleaseSRWLockExclusive(&syslog_lock);
}

extern "C"
void closelog() {
    RtlAcquireSRWLockExclusive(&syslog_lock);

    if (syslog_socket != -1) {
        close(syslog_socket);
        syslog_socket = -1;
    }

    RtlReleaseSRWLockExclusive(&syslog_lock);
}

extern "C"
int setlogmask(int mask) {
    if (mask == 0)
        return log_mask;

    swap(log_mask, mask);

    return mask;
}
