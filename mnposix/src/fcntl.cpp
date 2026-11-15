// SPDX-License-Identifier: LGPL-2.1-or-later
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits>

import nt;
import print;
import ucrtbase;

#undef errno
import errno;

using namespace std;

// ucrtbase
extern "C" bool mn_get_cloexec(int fd, bool* cloexec);
extern "C" bool mn_set_cloexec(int fd, bool cloexec);

static int getfl(int fd) {
    IO_STATUS_BLOCK iosb;
    OBJECT_BASIC_INFORMATION obi;
    nt_status status;
    uint32_t retlen;
    bool nonblock;
    int ret = 0;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    status = NtQueryObject(h, OBJECT_INFORMATION_CLASS::BasicInformation,
                           &obi, sizeof(obi), &retlen);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    bool can_read = obi.GrantedAccess & FILE_READ_DATA;
    bool can_write = obi.GrantedAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA);

    if (can_read && can_write)
        ret |= O_RDWR;
    else if (can_read)
        ret |= O_RDONLY;
    else if (can_write)
        ret |= O_WRONLY;

    if (obi.GrantedAccess & FILE_APPEND_DATA)
        ret |= O_APPEND;

    status = NtQueryInformationFile(h, &iosb, &nonblock, sizeof(nonblock),
                                    FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (nonblock)
        ret |= O_NONBLOCK;

    return ret;
}

static int setfl(int fd, int arg) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    // FIXME - O_APPEND

    bool nonblock = !!(arg & O_NONBLOCK);

    auto status = NtSetInformationFile(h, &iosb, &nonblock, sizeof(nonblock),
                                       FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

static int getfd(int fd) {
    bool cloexec;

    if (!mn_get_cloexec(fd, &cloexec)) {
        *_errno() = EBADF;
        return -1;
    }

    return cloexec ? FD_CLOEXEC : 0;
}

static int setfd(int fd, int arg) {
    if (arg & ~FD_CLOEXEC) {
        *_errno() = EINVAL;
        return -1;
    }

    if (!mn_set_cloexec(fd, arg & FD_CLOEXEC)) {
        *_errno() = EBADF;
        return -1;
    }

    return 0;
}

static int setlk(int fd, const flock* lock, bool nowait) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    int64_t start;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    if (lock->l_type != F_RDLCK && lock->l_type != F_WRLCK && lock->l_type != F_UNLCK) {
        *_errno() = EINVAL;
        return -1;
    }

    switch (lock->l_whence) {
        case SEEK_SET:
            start = lock->l_start;
            break;

        case SEEK_CUR: {
            FILE_POSITION_INFORMATION fpi;

            auto status = NtQueryInformationFile(h, &iosb, &fpi, sizeof(fpi),
                                                 FILE_INFORMATION_CLASS::PositionInformation);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            start = fpi.CurrentByteOffset.QuadPart + lock->l_start;

            // FIXME - should be decrementing if ungetc_char set in ucrtbase?
            break;
        }

        case SEEK_END: {
            FILE_STANDARD_INFORMATION fsi;

            status = NtQueryInformationFile(h, &iosb, &fsi, sizeof(fsi),
                                            FILE_INFORMATION_CLASS::StandardInformation);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            start = fsi.EndOfFile.QuadPart + lock->l_start;
            break;
        }

        default:
            *_errno() = EINVAL;
            return -1;
    }

    auto length = lock->l_len;

    if (length < 0) {
        if (length == numeric_limits<off_t>::min()) {
            *_errno() = EINVAL;
            return -1;
        }

        start += length;
        length = -length;
    }

    if (start < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (length > numeric_limits<off_t>::max() - start) {
        *_errno() = EOVERFLOW;
        return -1;
    }

    if (lock->l_type == F_RDLCK || lock->l_type == F_WRLCK) {
        FILE_MN_SET_POSIX_LOCK params;

        params.write = !!(lock->l_type == F_WRLCK);
        params.nowait = nowait;
        params.start = start;
        params.length = length == 0 ? (0xffffffffffffffff - start) : length;

        status = NtSetInformationFile(h, &iosb, &params, sizeof(params),
                                      FILE_INFORMATION_CLASS::MoxnoxSetPosixLock);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }
    } else { // F_UNLCK
        FILE_MN_RELEASE_POSIX_LOCK params;

        params.start = start;
        params.length = length == 0 ? (0xffffffffffffffff - start) : length;

        status = NtSetInformationFile(h, &iosb, &params, sizeof(params),
                                      FILE_INFORMATION_CLASS::MoxnoxReleasePosixLock);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }
    }

    return 0;
}

extern "C"
int fcntl(int fd, int op, ...) {
    va_list ap;

    switch (op) {
        case F_DUPFD: {
            va_start(ap, op);
            auto arg = va_arg(ap, int);
            va_end(ap);

            return mn_dupfd(fd, arg, false);
        }

        case F_GETFD:
            return getfd(fd);

        case F_SETFD: {
            va_start(ap, op);
            auto arg = va_arg(ap, int);
            va_end(ap);

            return setfd(fd, arg);
        }

        case F_GETFL:
            return getfl(fd);

        case F_SETFL: {
            va_start(ap, op);
            auto arg = va_arg(ap, int);
            va_end(ap);

            return setfl(fd, arg);
        }

        case F_GETLK:
            print("FIXME - fcntl with F_GETLK\n");
        break;

        case F_SETLK: {
            va_start(ap, op);
            auto lock = va_arg(ap, const flock*);
            va_end(ap);

            return setlk(fd, lock, true);
        }

        case F_SETLKW: {
            va_start(ap, op);
            auto lock = va_arg(ap, const flock*);
            va_end(ap);

            return setlk(fd, lock, false);
        }

        case F_DUPFD_CLOEXEC: {
            va_start(ap, op);
            auto arg = va_arg(ap, int);
            va_end(ap);

            return mn_dupfd(fd, arg, true);
        }

        default:
            print("FIXME - fcntl with unknown op {}\n", op);
        break;
    }

    *_errno() = EINVAL;
    return -1;
}
