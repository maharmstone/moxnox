// SPDX-License-Identifier: MIT
module;

#include <string_view>
#undef errno

export module errno;

import nt;

using namespace std;

// FIXME - should be re-exporting these from errno.h

export {
    const int EPERM = 1;
    const int ENOENT = 2;
    const int ESRCH = 3;
    const int EINTR = 4;
    const int EIO = 5;
    const int ENXIO = 6;
    const int E2BIG = 7;
    const int ENOEXEC = 8;
    const int EBADF = 9;
    const int ECHILD = 10;
    const int EAGAIN = 11;
    const int ENOMEM = 12;
    const int EACCES = 13;
    const int EFAULT = 14;
    const int EBUSY = 16;
    const int EEXIST = 17;
    const int EXDEV = 18;
    const int ENODEV = 19;
    const int ENOTDIR = 20;
    const int EISDIR = 21;
    const int EINVAL = 22;
    const int ENFILE = 23;
    const int EMFILE = 24;
    const int ENOTTY = 25;
    const int EFBIG = 27;
    const int ENOSPC = 28;
    const int ESPIPE = 29;
    const int EROFS = 30;
    const int EMLINK = 31;
    const int EPIPE = 32;
    const int EDOM = 33;
    const int ERANGE = 34;
    const int EDEADLK = 36;
    const int ENAMETOOLONG = 38;
    const int ENOLCK = 39;
    const int ENOSYS = 40;
    const int ENOTEMPTY = 41;
    const int EILSEQ = 42;
    const int STRUNCATE = 80;
    const int EADDRINUSE = 100;
    const int EADDRNOTAVAIL = 101;
    const int EAFNOSUPPORT = 102;
    const int EALREADY = 103;
    const int EBADMSG = 104;
    const int ECANCELED = 105;
    const int ECONNABORTED = 106;
    const int ECONNREFUSED = 107;
    const int ECONNRESET = 108;
    const int EDESTADDRREQ = 109;
    const int EHOSTUNREACH = 110;
    const int EIDRM = 111;
    const int EINPROGRESS = 112;
    const int EISCONN = 113;
    const int ELOOP = 114;
    const int EMSGSIZE = 115;
    const int ENETDOWN = 116;
    const int ENETRESET = 117;
    const int ENETUNREACH = 118;
    const int ENOBUFS = 119;
    const int ENODATA = 120;
    const int ENOLINK = 121;
    const int ENOMSG = 122;
    const int ENOPROTOOPT = 123;
    const int ENOSR = 124;
    const int ENOSTR = 125;
    const int ENOTCONN = 126;
    const int ENOTRECOVERABLE = 127;
    const int ENOTSOCK = 128;
    const int ENOTSUP = 129;
    const int EOPNOTSUPP = 130;
    const int EOTHER = 131;
    const int EOVERFLOW = 132;
    const int EOWNERDEAD = 133;
    const int EPROTO = 134;
    const int EPROTONOSUPPORT = 135;
    const int EPROTOTYPE = 136;
    const int ETIME = 137;
    const int ETIMEDOUT = 138;
    const int ETXTBSY = 139;
    const int EWOULDBLOCK = 140;
    const int ESTALE = 141;
    const int EDQUOT = 142;
};

export int nt_status_to_errno(nt_status status) {
    switch (status) {
        case nt_status::ACCESS_DENIED:
        case nt_status::SHARING_VIOLATION:
            return EACCES;

        case nt_status::NO_MORE_FILES:
        case nt_status::OBJECT_NAME_NOT_FOUND:
        case nt_status::OBJECT_PATH_NOT_FOUND:
            return ENOENT;

        case nt_status::DISK_CORRUPT_ERROR:
        case nt_status::CRC_ERROR:
        case nt_status::IO_DEVICE_ERROR:
            return EIO;

        case nt_status::INVALID_HANDLE:
            return EBADF;

        case nt_status::INSUFFICIENT_RESOURCES:
        case nt_status::NO_MEMORY:
            return ENOMEM;

        case nt_status::DISK_FULL:
            return ENOSPC;

        case nt_status::DEVICE_NOT_READY:
        case nt_status::CANT_WAIT:
            return EAGAIN;

        case nt_status::CONNECTION_RESET:
            return ECONNRESET;

        case nt_status::HOST_UNREACHABLE:
            return EHOSTUNREACH;

        case nt_status::CONNECTION_ACTIVE:
            return EISCONN;

        case nt_status::BUFFER_OVERFLOW:
        case nt_status::INVALID_BUFFER_SIZE:
            return EMSGSIZE;

        case nt_status::NETWORK_UNREACHABLE:
            return ENETDOWN;

        case nt_status::CONNECTION_DISCONNECTED:
            return ENOTCONN;

        case nt_status::PIPE_BROKEN:
        case nt_status::CONNECTION_ABORTED:
            return EPIPE;

        case nt_status::ACCESS_VIOLATION:
            return EFAULT;

        case nt_status::ADDRESS_ALREADY_ASSOCIATED:
            return EADDRINUSE;

        case nt_status::INVALID_ADDRESS_COMPONENT:
            return EADDRNOTAVAIL;

        case nt_status::PROTOCOL_UNREACHABLE:
            return EPROTO;

        case nt_status::CONNECTION_REFUSED:
            return ECONNREFUSED;

        case nt_status::NOT_SAME_DEVICE:
            return EXDEV;

        case nt_status::NOT_A_DIRECTORY:
            return ENOTDIR;

        case nt_status::FILE_IS_A_DIRECTORY:
            return EISDIR;

        case nt_status::DIRECTORY_NOT_EMPTY:
            return ENOTEMPTY;

        case nt_status::INVALID_IMAGE_FORMAT:
            return ENOEXEC;

        case nt_status::MEDIA_WRITE_PROTECTED:
            return EROFS;

        case nt_status::OBJECT_NAME_COLLISION:
            return EEXIST;

        default:
            return EINVAL;
    }
}

export string_view errno_to_string_view(int err) {
    switch (err) {
        case EPERM:
            return "EPERM";
        case ENOENT:
            return "ENOENT";
        case ESRCH:
            return "ESRCH";
        case EINTR:
            return "EINTR";
        case EIO:
            return "EIO";
        case ENXIO:
            return "ENXIO";
        case E2BIG:
            return "E2BIG";
        case ENOEXEC:
            return "ENOEXEC";
        case EBADF:
            return "EBADF";
        case ECHILD:
            return "ECHILD";
        case EAGAIN:
            return "EAGAIN";
        case ENOMEM:
            return "ENOMEM";
        case EACCES:
            return "EACCES";
        case EFAULT:
            return "EFAULT";
        case EBUSY:
            return "EBUSY";
        case EEXIST:
            return "EEXIST";
        case EXDEV:
            return "EXDEV";
        case ENODEV:
            return "ENODEV";
        case ENOTDIR:
            return "ENOTDIR";
        case EISDIR:
            return "EISDIR";
        case EINVAL:
            return "EINVAL";
        case ENFILE:
            return "ENFILE";
        case EMFILE:
            return "EMFILE";
        case ENOTTY:
            return "ENOTTY";
        case EFBIG:
            return "EFBIG";
        case ENOSPC:
            return "ENOSPC";
        case ESPIPE:
            return "ESPIPE";
        case EROFS:
            return "EROFS";
        case EMLINK:
            return "EMLINK";
        case EPIPE:
            return "EPIPE";
        case EDOM:
            return "EDOM";
        case ERANGE:
            return "ERANGE";
        case EDEADLK:
            return "EDEADLK";
        case ENAMETOOLONG:
            return "ENAMETOOLONG";
        case ENOLCK:
            return "ENOLCK";
        case ENOSYS:
            return "ENOSYS";
        case ENOTEMPTY:
            return "ENOTEMPTY";
        case EILSEQ:
            return "EILSEQ";
        case STRUNCATE:
            return "STRUNCATE";
        case EADDRINUSE:
            return "EADDRINUSE";
        case EADDRNOTAVAIL:
            return "EADDRNOTAVAIL";
        case EAFNOSUPPORT:
            return "EAFNOSUPPORT";
        case EALREADY:
            return "EALREADY";
        case EBADMSG:
            return "EBADMSG";
        case ECANCELED:
            return "ECANCELED";
        case ECONNABORTED:
            return "ECONNABORTED";
        case ECONNREFUSED:
            return "ECONNREFUSED";
        case ECONNRESET:
            return "ECONNRESET";
        case EDESTADDRREQ:
            return "EDESTADDRREQ";
        case EHOSTUNREACH:
            return "EHOSTUNREACH";
        case EIDRM:
            return "EIDRM";
        case EINPROGRESS:
            return "EINPROGRESS";
        case EISCONN:
            return "EISCONN";
        case ELOOP:
            return "ELOOP";
        case EMSGSIZE:
            return "EMSGSIZE";
        case ENETDOWN:
            return "ENETDOWN";
        case ENETRESET:
            return "ENETRESET";
        case ENETUNREACH:
            return "ENETUNREACH";
        case ENOBUFS:
            return "ENOBUFS";
        case ENODATA:
            return "ENODATA";
        case ENOLINK:
            return "ENOLINK";
        case ENOMSG:
            return "ENOMSG";
        case ENOPROTOOPT:
            return "ENOPROTOOPT";
        case ENOSR:
            return "ENOSR";
        case ENOSTR:
            return "ENOSTR";
        case ENOTCONN:
            return "ENOTCONN";
        case ENOTRECOVERABLE:
            return "ENOTRECOVERABLE";
        case ENOTSOCK:
            return "ENOTSOCK";
        case ENOTSUP:
            return "ENOTSUP";
        case EOPNOTSUPP:
            return "EOPNOTSUPP";
        case EOTHER:
            return "EOTHER";
        case EOVERFLOW:
            return "EOVERFLOW";
        case EOWNERDEAD:
            return "EOWNERDEAD";
        case EPROTO:
            return "EPROTO";
        case EPROTONOSUPPORT:
            return "EPROTONOSUPPORT";
        case EPROTOTYPE:
            return "EPROTOTYPE";
        case ETIME:
            return "ETIME";
        case ETIMEDOUT:
            return "ETIMEDOUT";
        case ETXTBSY:
            return "ETXTBSY";
        case EWOULDBLOCK:
            return "EWOULDBLOCK";
        case ESTALE:
            return "ESTALE";
        case EDQUOT:
            return "EDQUOT";
        default:
            return "?";
    }
}
