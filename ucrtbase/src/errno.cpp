// SPDX-License-Identifier: LGPL-2.1-or-later
#include <string.h>
#include <errno.h>
#include <string_view>

import print;
import ucrtbase;

#undef errno
import errno;

using namespace std;

static thread_local int errno = 0;

extern "C"
int* __errno() {
    return &errno;
}

extern "C"
errno_t strerror_s(char* buffer, size_t size, int errnum) {
    string_view sv;

    switch (errnum) {
        case 0:
            sv = "Success";
            break;

        case EPERM:
            sv = "Operation not permitted";
            break;

        case ENOENT:
            sv = "No such file or directory";
            break;

        case ESRCH:
            sv = "No such process";
            break;

        case EINTR:
            sv = "Interrupted system call";
            break;

        case EIO:
            sv = "Input/output error";
            break;

        case ENXIO:
            sv = "No such device or address";
            break;

        case E2BIG:
            sv = "Argument list too long";
            break;

        case ENOEXEC:
            sv = "Exec format error";
            break;

        case EBADF:
            sv = "Bad file descriptor";
            break;

        case ECHILD:
            sv = "No child processes";
            break;

        case EAGAIN:
            sv = "Resource temporarily unavailable";
            break;

        case ENOMEM:
            sv = "Cannot allocate memory";
            break;

        case EACCES:
            sv = "Permission denied";
            break;

        case EFAULT:
            sv = "Bad address";
            break;

        case EBUSY:
            sv = "Device or resource busy";
            break;

        case EEXIST:
            sv = "File exists";
            break;

        case EXDEV:
            sv = "Invalid cross-device link";
            break;

        case ENODEV:
            sv = "No such device";
            break;

        case ENOTDIR:
            sv = "Not a directory";
            break;

        case EISDIR:
            sv = "Is a directory";
            break;

        case EINVAL:
            sv = "Invalid argument";
            break;

        case ENFILE:
            sv = "Too many open files in system";
            break;

        case EMFILE:
            sv = "Too many open files";
            break;

        case ENOTTY:
            sv = "Inappropriate ioctl for device";
            break;

        case EFBIG:
            sv = "File too large";
            break;

        case ENOSPC:
            sv = "No space left on device";
            break;

        case ESPIPE:
            sv = "Illegal seek";
            break;

        case EROFS:
            sv = "Read-only file system";
            break;

        case EMLINK:
            sv = "Too many links";
            break;

        case EPIPE:
            sv = "Broken pipe";
            break;

        case EDOM:
            sv = "Numerical argument out of domain";
            break;

        case ERANGE:
            sv = "Numerical result out of range";
            break;

        case EDEADLK:
            sv = "Resource deadlock avoided";
            break;

        case ENAMETOOLONG:
            sv = "File name too long";
            break;

        case ENOLCK:
            sv = "No locks available";
            break;

        case ENOSYS:
            sv = "Function not implemented";
            break;

        case ENOTEMPTY:
            sv = "Directory not empty";
            break;

        case EILSEQ:
            sv = "Invalid or incomplete multibyte or wide character";
            break;

        default:
            sv = "Unknown error";
            break;
    }

    if (size < sv.size() + 1) {
        memcpy(buffer, sv.data(), size - 1);
        buffer[size - 1] = 0;
        return ERANGE;
    }

    memcpy(buffer, sv.data(), sv.size());
    buffer[sv.size()] = 0;

    return 0;
}

extern "C"
char* __strerror(int errnum) {
    static char strerror_buf[256];

    strerror_s(strerror_buf, sizeof(strerror_buf), errnum);

    return strerror_buf;
}

extern "C"
void perror(const char* message) {
    auto err = __strerror(*__errno());

    if (message && message[0])
        print_stderr("{}: {}\n", message, err);
    else
        print_stderr("{}\n", err);
}
