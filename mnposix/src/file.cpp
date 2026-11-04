// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string_view>
#include <span>
#include <memory>
#include <mutex>

import nt;
import ntdll;
import unicode;
import unique_handle;
import print;
import ucrtbase;
import moxnox_ioctls;

#undef errno
import errno;

using namespace std;

extern "C"
ssize_t getline(char** lineptr, size_t* pn, FILE* stream) {
    if (!lineptr || !pn) {
        *_errno() = EINVAL;
        return -1;
    }

    auto& line = *lineptr;
    auto& n = *pn;
    size_t read = 0;

    if (!line || n == 0) {
        auto newptr = (char*)realloc(line, 128);
        if (!newptr) {
            *_errno() = ENOMEM;
            return -1;
        }

        line = newptr;
        n = 128;
    }

    auto add_char = [&read, &n, &line](char c) {
        if (read == n) {
            auto newptr = (char*)realloc(line, n * 2);
            if (!newptr) {
                *_errno() = ENOMEM;
                return false;
            }

            line = newptr;
            n *= 2;
        }

        line[read] = c;

        return true;
    };

    while (true) {
        auto c = fgetc(stream);

        if (c == EOF)
            break;

        if (!add_char((char)c))
            return -1;

        read++;

        if ((char)c == '\n')
            break;
    }

    if (read == 0)
        return -1;

    if (!add_char(0))
        return -1;

    return (ssize_t)read;
}

extern "C"
int futimens(int fd, const timespec* times) {
    FILE_BASIC_INFORMATION fbi;
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER now;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1) {
        *_errno() = EBADF;
        return -1;
    }

    auto timespec_to_filetime = [](const struct timespec& ts) {
        return (int64_t)((uint64_t)116444736000000000 + ((uint64_t)ts.tv_sec * 10000000) + ((uint64_t)ts.tv_nsec / 100));
    };

    fbi.CreationTime.QuadPart = 0;
    fbi.ChangeTime.QuadPart = 0;
    fbi.FileAttributes = 0;

    NtQuerySystemTime(&now);

    if (!times) {
        fbi.LastAccessTime.QuadPart = now.QuadPart;
        fbi.LastWriteTime.QuadPart = now.QuadPart;
    } else {
        if (times[0].tv_nsec == UTIME_NOW)
            fbi.LastAccessTime.QuadPart = now.QuadPart;
        else if (times[0].tv_nsec == UTIME_OMIT)
            fbi.LastAccessTime.QuadPart = 0;
        else
            fbi.LastAccessTime.QuadPart = timespec_to_filetime(times[0]);

        if (times[1].tv_nsec == UTIME_NOW)
            fbi.LastWriteTime.QuadPart = now.QuadPart;
        else if (times[1].tv_nsec == UTIME_OMIT)
            fbi.LastWriteTime.QuadPart = 0;
        else
            fbi.LastWriteTime.QuadPart = timespec_to_filetime(times[1]);
    }

    auto status = NtSetInformationFile(h, &iosb, &fbi, sizeof(fbi),
                                       FILE_INFORMATION_CLASS::BasicInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int utimensat(int dirfd, const char* path, const timespec* times, int flags) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;
    FILE_BASIC_INFORMATION fbi;
    LARGE_INTEGER now;
    uint32_t options;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (flags & ~AT_SYMLINK_NOFOLLOW) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    options = 0;

    if (flags & AT_SYMLINK_NOFOLLOW)
        options |= FILE_OPEN_REPARSE_POINT;

    status = NtCreateFile(&h, FILE_WRITE_ATTRIBUTES, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, options,
                          nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    unique_handle f{h};

    auto timespec_to_filetime = [](const struct timespec& ts) {
        return (int64_t)((uint64_t)116444736000000000 + ((uint64_t)ts.tv_sec * 10000000) + ((uint64_t)ts.tv_nsec / 100));
    };

    fbi.CreationTime.QuadPart = 0;
    fbi.ChangeTime.QuadPart = 0;
    fbi.FileAttributes = 0;

    NtQuerySystemTime(&now);

    if (!times) {
        fbi.LastAccessTime.QuadPart = now.QuadPart;
        fbi.LastWriteTime.QuadPart = now.QuadPart;
    } else {
        if (times[0].tv_nsec == UTIME_NOW)
            fbi.LastAccessTime.QuadPart = now.QuadPart;
        else if (times[0].tv_nsec == UTIME_OMIT)
            fbi.LastAccessTime.QuadPart = 0;
        else
            fbi.LastAccessTime.QuadPart = timespec_to_filetime(times[0]);

        if (times[1].tv_nsec == UTIME_NOW)
            fbi.LastWriteTime.QuadPart = now.QuadPart;
        else if (times[1].tv_nsec == UTIME_OMIT)
            fbi.LastWriteTime.QuadPart = 0;
        else
            fbi.LastWriteTime.QuadPart = timespec_to_filetime(times[1]);
    }

    status = NtSetInformationFile(f.get(), &iosb, &fbi, sizeof(fbi),
                                  FILE_INFORMATION_CLASS::BasicInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int utimes(const char* path, const timeval* times) {
    timespec ts[2];

    if (!times)
        return utimensat(AT_FDCWD, path, nullptr, 0);

    ts[0].tv_sec = times[0].tv_sec;
    ts[0].tv_nsec = times[0].tv_usec * 1000;
    ts[1].tv_sec = times[1].tv_sec;
    ts[1].tv_nsec = times[1].tv_usec * 1000;

    return utimensat(AT_FDCWD, path, ts, 0);
}

extern "C"
int ftruncate(int fd, off_t length) {
    FILE_END_OF_FILE_INFORMATION eof;
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1) {
        *_errno() = EBADF;
        return -1;
    }

    eof.EndOfFile.QuadPart = length;

    auto status = NtSetInformationFile(h, &iosb, &eof, sizeof(eof),
                                       FILE_INFORMATION_CLASS::EndOfFileInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int truncate(const char* path, off_t length) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    FILE_END_OF_FILE_INFORMATION eof;
    HANDLE h;

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb.ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_WRITE_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          FILE_NON_DIRECTORY_FILE, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    unique_handle f{h};

    eof.EndOfFile.QuadPart = length;

    status = NtSetInformationFile(f.get(), &iosb, &eof, sizeof(eof),
                                  FILE_INFORMATION_CLASS::EndOfFileInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int pipe(int* pfds) {
    return _pipe(pfds, 0, 0);
}

extern "C"
ssize_t readlinkat(int dirfd, const char* path, char* buf, size_t bufsiz) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;

    struct {
        REPARSE_DATA_BUFFER header;
        char16_t buffer[255];
    } rdb;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          FILE_OPEN_REPARSE_POINT, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    unique_handle f{h};

    status = NtFsControlFile(f.get(), 0, nullptr, nullptr, &iosb,
                             FSCTL_GET_REPARSE_POINT, nullptr, 0,
                             &rdb, sizeof(rdb));
    if (!success(status)) {
        if (status == nt_status::NOT_A_REPARSE_POINT)
            *_errno() = EINVAL;
        else
            *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (rdb.header.ReparseTag != IO_REPARSE_TAG_SYMLINK) {
        *_errno() = EINVAL;
        return -1;
    }

    auto u16sv = u16string_view(rdb.header.SymbolicLinkReparseBuffer.PathBuffer + (rdb.header.SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(char16_t)),
                                rdb.header.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(u16sv);

    utf16_to_utf8(span(buf, bufsiz), u16sv);

    return (ssize_t)min(u8len, bufsiz);
}

extern "C"
int readlink(const char* path, char* buf, size_t bufsiz) {
    return (int)readlinkat(AT_FDCWD, path, buf, bufsiz);
}

extern "C"
int symlinkat(const char* target, int newdirfd, const char* linkpath) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;
    unique_handle f;

    if (newdirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(newdirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!linkpath || !linkpath[0]) {
        *_errno() = EINVAL;
        return -1;
    }

    if (!target || !target[0]) {
        *_errno() = EINVAL;
        return -1;
    }

    auto linkpathsv = string_view(linkpath);
    auto targetsv = string_view(target);

    auto u16len = utf8_to_utf16_len(linkpathsv);
    auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16, u16len), linkpathsv);

    // try to create symlink

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES | DELETE, &oa, &iosb, nullptr,
                          0, 0, FILE_CREATE, FILE_NON_DIRECTORY_FILE, nullptr, 0);

    free(u16);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        *_errno() = EEXIST;
        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    } else
        f.reset(h);

    auto u16targetlen = utf8_to_utf16_len(targetsv);

    size_t rdblen = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) + (u16targetlen * sizeof(char16_t));

    auto rdbbuf = malloc(rdblen);
    if (!rdbbuf) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        NtSetInformationFile(f.get(), &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);

        *_errno() = ENOMEM;
        return -1;
    }

    auto& rdb = *(REPARSE_DATA_BUFFER*)rdbbuf;

    rdb.ReparseTag = IO_REPARSE_TAG_SYMLINK;
    rdb.ReparseDataLength = (uint16_t)(rdblen - offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer));
    rdb.Reserved = 0;
    rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
    rdb.SymbolicLinkReparseBuffer.SubstituteNameLength = (uint16_t)(u16targetlen * sizeof(char16_t));
    rdb.SymbolicLinkReparseBuffer.PrintNameOffset = 0;
    rdb.SymbolicLinkReparseBuffer.PrintNameLength = 0;
    rdb.SymbolicLinkReparseBuffer.Flags = 0;

    utf8_to_utf16(span(rdb.SymbolicLinkReparseBuffer.PathBuffer, u16targetlen), targetsv);

    status = NtFsControlFile(f.get(), 0, nullptr, nullptr, &iosb,
                             FSCTL_SET_REPARSE_POINT, &rdb, (uint32_t)rdblen,
                             nullptr, 0);

    free(rdbbuf);

    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        NtSetInformationFile(f.get(), &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);

        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int symlink(const char* target, const char* linkpath) {
    return symlinkat(target, AT_FDCWD, linkpath);
}

extern "C"
void flockfile(FILE* f) {
    _lock_file(f);
}

extern "C"
void funlockfile(FILE* f) {
    _unlock_file(f);
}

extern "C"
int getc_unlocked(FILE* f) {
    return _fgetc_nolock(f);
}

static bool get_dos_path(HANDLE h, UNICODE_STRING& us) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;

    status = NtQueryInformationFile(h, &iosb, &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        *_errno() = nt_status_to_errno(status);
        return false;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        *_errno() = ENOMEM;
        return false;
    }

    status = NtQueryInformationFile(h, &iosb, fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        free(fni2);
        return false;
    }

    auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    static const u16string_view prefix = u"C:";

    us.Length = min(us.MaximumLength, (uint16_t)((sv.size() + prefix.size()) * sizeof(char16_t)));
    memcpy(us.Buffer, prefix.data(), prefix.size() * sizeof(char16_t));
    memcpy(us.Buffer + prefix.size(), sv.data(), us.Length - (prefix.size() * sizeof(char16_t)));

    for (size_t i = prefix.size(); i < us.Length / sizeof(char16_t); i++) {
        if (us.Buffer[i] == u'/')
            us.Buffer[i] = u'\\';
    }

    free(fni2);

    return true;
}

extern "C"
int fchdir(int fd) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    HANDLE h2;
    nt_status status;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    status = NtDuplicateObject(NtCurrentProcess(), h, NtCurrentProcess(), &h2,
                               0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        if (!get_dos_path(h, peb.ProcessParameters->CurrentDirectory.DosPath)) {
            NtClose(h2);
            return -1;
        }

        if (peb.ProcessParameters->CurrentDirectory.Handle != 0)
            NtClose(peb.ProcessParameters->CurrentDirectory.Handle);

        peb.ProcessParameters->CurrentDirectory.Handle = h2;
    }

    return 0;
}

static int do_fchmod(HANDLE h, mode_t mode) {
    IO_STATUS_BLOCK iosb;

    auto v = (uint32_t)mode;

    auto status = NtSetInformationFile(h, &iosb, &v, sizeof(v),
                                       FILE_INFORMATION_CLASS::MoxnoxFchmod);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int fchmodat(int dirfd, const char* path, mode_t mode, int flags) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;
    uint32_t options;
    unique_handle f;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (flags & ~AT_SYMLINK_NOFOLLOW) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    options = 0;

    if (flags & AT_SYMLINK_NOFOLLOW)
        options |= FILE_OPEN_REPARSE_POINT;

    status = NtCreateFile(&h, FILE_WRITE_ATTRIBUTES, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, options, nullptr, 0);

    free(u16pathname);

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    } else
        f.reset(h);

    return do_fchmod(f.get(), mode);
}

extern "C"
int fchmod(int fd, mode_t mode) {
    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    return do_fchmod(h, mode);
}

extern "C"
int fsync(int /*fd*/) {
    // FIXME - do this properly

    return 0;
}

extern "C"
int linkat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath,
           int flags) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_ptr<char16_t[]> u16;
    HANDLE h, olddirh, newdirh;
    unique_handle f;
    unique_ptr<char[]> flibuf;
    uint32_t options;
    string_view src(oldpath), dest(newpath);

    if (!oldpath || oldpath[0] == 0 || !newpath || newpath[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (flags & ~AT_SYMLINK_FOLLOW) {
        *_errno() = EINVAL;
        return -1;
    }

    if (olddirfd == AT_FDCWD)
        olddirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        olddirh = (HANDLE)_get_osfhandle(olddirfd);
        if (olddirh == (HANDLE)-1)
            return -1;
    }

    if (newdirfd == AT_FDCWD)
        newdirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        newdirh = (HANDLE)_get_osfhandle(newdirfd);
        if (newdirh == (HANDLE)-1)
            return -1;
    }

    auto u16len = utf8_to_utf16_len(src);
    u16.reset(new char16_t[u16len]);
    if (!u16) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16.get(), u16len), src);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16.get();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = olddirh;

    options = FILE_NON_DIRECTORY_FILE;
    if (!(flags & AT_SYMLINK_FOLLOW))
        options |= FILE_OPEN_REPARSE_POINT;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, options, nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    } else
        f.reset(h);

    auto u16destlen = utf8_to_utf16_len(dest);

    size_t flilen = offsetof(FILE_LINK_INFORMATION, FileName) + (u16destlen * sizeof(char16_t));

    flibuf.reset(new char[flilen]);
    if (!flibuf) {
        *_errno() = ENOMEM;
        return -1;
    }

    auto& fli = *(FILE_LINK_INFORMATION*)flibuf.get();

    fli.ReplaceIfExists = false;
    fli.RootDirectory = newdirh;
    fli.FileNameLength = (uint32_t)(u16destlen * sizeof(char16_t));

    utf8_to_utf16(span(fli.FileName, u16destlen), dest);

    status = NtSetInformationFile(f.get(), &iosb, &fli, (uint32_t)flilen,
                                  FILE_INFORMATION_CLASS::LinkInformation);

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int link(const char* oldpath, const char* newpath) {
    return linkat(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}

static int do_statvfs(HANDLE h, struct statvfs& buf) {
    IO_STATUS_BLOCK iosb;
    FILE_FS_SIZE_INFORMATION size;
    nt_status status;

    struct {
        FILE_FS_ATTRIBUTE_INFORMATION info;
        char buf[48];
    } __attribute((packed)) attr;

    status = NtQueryVolumeInformationFile(h, &iosb, &size, sizeof(size),
                                          FS_INFORMATION_CLASS::SizeInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    status = NtQueryVolumeInformationFile(h, &iosb, &attr, sizeof(attr),
                                          FS_INFORMATION_CLASS::AttributeInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    auto name = u16string_view(attr.info.FileSystemName,
                               (iosb.Information - offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName)) / sizeof(char16_t));

    buf.f_bsize = size.BytesPerSector * size.SectorsPerAllocationUnit;
    buf.f_frsize = buf.f_bsize;
    buf.f_blocks = (uint64_t)size.TotalAllocationUnits.QuadPart;
    buf.f_bfree = (uint64_t)size.AvailableAllocationUnits.QuadPart;
    buf.f_bavail = buf.f_bfree;
    buf.f_files = 0; // FIXME?
    buf.f_ffree = 0; // FIXME?
    buf.f_favail = 0; // FIXME?
    buf.f_fsid = 0; // FIXME?
    buf.f_flag = 0;

    if (attr.info.FileSystemAttributes & FILE_READ_ONLY_VOLUME)
        buf.f_flag |= ST_RDONLY;

    // half-true, as this is UTF-16 characters
    buf.f_namemax = attr.info.MaximumComponentNameLength;

    auto len = utf16_to_utf8_len(name);

    utf16_to_utf8(span(buf.f_basetype, sizeof(buf.f_basetype) - 1), name);

    if (len >= sizeof(buf.f_basetype))
        buf.f_basetype[sizeof(buf.f_basetype) - 1] = 0;
    else
        buf.f_basetype[len] = 0;

    return 0;
}

extern "C"
int statvfs(const char* path, struct statvfs* buf) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_ptr<char16_t[]> u16;
    HANDLE h;
    unique_handle f;

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto fn = string_view(path);

    auto u16len = utf8_to_utf16_len(fn);
    u16.reset(new char16_t[u16len]);
    if (!u16) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16.get(), u16len), fn);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16.get();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, 0, nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    } else
        f.reset(h);

    return do_statvfs(f.get(), *buf);
}

extern "C"
int fstatvfs(int fd, struct statvfs* buf) {
    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    return do_statvfs(h, *buf);
}

static int openat2(int dirfd, const char* path, int flags, int pmode) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;
    ACCESS_MASK access = FILE_READ_ATTRIBUTES;
    uint32_t file_attributes, create_disposition, create_options;
    nt_status status;

    // FIXME - merge this with open2 in ucrtbase?

    if (flags & _O_TEMPORARY) {
        print_stderr("FIXME - openat with _O_TEMPORARY\n");
        flags &= ~_O_TEMPORARY;
    }

    if (flags & _O_NOINHERIT) {
        print_stderr("FIXME - openat with _O_NOINHERIT\n");
        flags &= ~_O_NOINHERIT;
    }

    if (flags & _O_WTEXT) {
        print_stderr("FIXME - openat with _O_WTEXT\n");
        flags &= ~_O_WTEXT;
    }

    if (flags & _O_U16TEXT) {
        print_stderr("FIXME - openat with _O_U16TEXT\n");
        flags &= ~_O_U16TEXT;
    }

    if (flags & _O_U8TEXT) {
        print_stderr("FIXME - openat with _O_U8TEXT\n");
        flags &= ~_O_U8TEXT;
    }

    // FIXME - do we need to treat _O_TEXT and _O_BINARY differently?
    // FIXME - _O_RANDOM
    // FIXME - _O_SEQUENTIAL
    // FIXME - _O_SHORT_LIVED?

    if (flags & _O_WRONLY)
        access |= FILE_GENERIC_WRITE;
    else if (flags & _O_RDWR)
        access |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
    else
        access |= FILE_GENERIC_READ;

    if (flags & (_O_WRONLY | _O_RDWR)) {
        if (flags & _O_APPEND)
            access &= ~FILE_WRITE_DATA;
        else
            access &= ~FILE_APPEND_DATA;
    }

    if (flags & _O_CREAT) {
        if (flags & _O_EXCL)
            create_disposition = FILE_CREATE;
        else if (flags & _O_TRUNC)
            create_disposition = FILE_OVERWRITE_IF;
        else
            create_disposition = FILE_OPEN_IF;
    } else {
        if (flags & _O_TRUNC)
            create_disposition = FILE_OVERWRITE;
        else
            create_disposition = FILE_OPEN;
    }

    create_options = 0;

    if (flags & O_DIRECTORY)
        create_options |= FILE_DIRECTORY_FILE;

    if (flags & O_NOFOLLOW)
        create_options |= FILE_OPEN_REPARSE_POINT;

    if (!(pmode & _S_IWRITE))
        file_attributes = FILE_ATTRIBUTE_READONLY;
    else
        file_attributes = FILE_ATTRIBUTE_NORMAL;

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1) {
            free(u16pathname);
            return -1;
        }
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    status = NtCreateFile(&h, access, &oa, &iosb, nullptr, file_attributes,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, create_disposition,
                          create_options, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    if (iosb.Information == FILE_CREATED) {
        // FIXME - this should be atomic

        auto mode = (uint32_t)pmode & ~mn_get_umask();

        status = NtSetInformationFile(h, &iosb, &mode, sizeof(mode),
                                      FILE_INFORMATION_CLASS::MoxnoxFchmod);
        if (!success(status)) {
            FILE_DISPOSITION_INFORMATION fdi;

            fdi.DeleteFile = true;

            NtSetInformationFile(h, &iosb, &fdi, sizeof(fdi),
                                 FILE_INFORMATION_CLASS::DispositionInformation);
            NtClose(h);

            *_errno() = nt_status_to_errno(status);
            return -1;
        }
    }

    if (flags & O_NOFOLLOW) {
        // check if reparse point

        FILE_BASIC_INFORMATION fbi;

        status = NtQueryInformationFile(h, &iosb, &fbi, sizeof(fbi),
                                        FILE_INFORMATION_CLASS::BasicInformation);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            NtClose(h);
            return -1;
        }

        if (fbi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            *_errno() = ELOOP;
            NtClose(h);
            return -1;
        }
    }

    if (flags & O_NONBLOCK) {
        bool nonblock = true;

        // FIXME - behaviour is different for FIFOs (means open itself should not block)

        auto status = NtSetInformationFile(h, &iosb, &nonblock, sizeof(nonblock),
                                           FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            NtClose(h);
            return -1;
        }
    }

    auto fd = _open_osfhandle(h, flags & O_CLOEXEC);

    if (fd == -1) {
        NtClose(h);
        return -1;
    }

    return fd;
}

extern "C"
int openat(int dirfd, const char *path, int flags, ...) {
    va_list ap;

    if (flags & _O_CREAT) {
        int pmode;

        va_start(ap, flags);
        pmode = va_arg(ap, int);
        va_end(ap);

        return openat2(dirfd, path, flags, pmode);
    } else
        return openat2(dirfd, path, flags, 0);
}

extern "C"
int unlinkat(int dirfd, const char* path, int flags) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h, dirh;
    unique_handle file;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    uint32_t create_options;

    if (flags & ~AT_REMOVEDIR) {
        *_errno() = EINVAL;
        return -1;
    }

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1) {
            free(buf);
            return -1;
        }
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    create_options = FILE_OPEN_REPARSE_POINT;

    if (flags & AT_REMOVEDIR)
        create_options |= FILE_DIRECTORY_FILE;
    else
        create_options |= FILE_NON_DIRECTORY_FILE;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                          create_options, nullptr, 0);

    free(buf);

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file.get(), &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER off;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    off.QuadPart = offset;

    while (true) {
        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtReadFile(h, 0, nullptr, nullptr, &iosb, buf,
                                (uint32_t)count, &off, nullptr);

        if (status == nt_status::USER_APC) {
            if (mn_handled_nonrestartable_signal_count() != sig_count) {
                *_errno() = EINTR;
                return -1;
            }

            continue;
        }

        if (status == nt_status::END_OF_FILE || status == nt_status::PIPE_DISCONNECTED)
            return 0;
        else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        break;
    }

    return (int)iosb.Information;
}

extern "C"
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER off;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    off.QuadPart = offset;

    while (true) {
        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtWriteFile(h, 0, nullptr, nullptr, &iosb, buf,
                                  (uint32_t)count, &off, nullptr);

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

    return (int)iosb.Information;
}

extern "C"
int mkdirat(int dirfd, const char* path, mode_t mode) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file, dirh;
    IO_STATUS_BLOCK iosb;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    auto pathsv = string_view(path);

    while (pathsv.size() > 1 && (pathsv.back() == '/' || pathsv.back() == '\\')) {
        pathsv = string_view(pathsv.data(), pathsv.size() - 1);
    }

    if (pathsv.empty()) {
        *_errno() = EINVAL;
        return -1;
    }

    auto u16len = utf8_to_utf16_len(pathsv);
    auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));

    if (!u16) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16, u16len), pathsv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)u16;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    status = NtCreateFile(&file, 0, &oa, &iosb, nullptr, FILE_ATTRIBUTE_DIRECTORY,
                          0, FILE_CREATE, FILE_DIRECTORY_FILE, nullptr, 0);

    free(u16);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        *_errno() = EEXIST;
        return -1;
    } else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        *_errno() = ENOENT;
        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    // FIXME - this should be atomic

    auto m = (uint32_t)mode & ~mn_get_umask();

    status = NtSetInformationFile(file, &iosb, &m, sizeof(m),
                                  FILE_INFORMATION_CLASS::MoxnoxFchmod);
    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        NtSetInformationFile(file, &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);
        NtClose(file);

        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    NtClose(file);

    return 0;
}

extern "C"
int mkdir(const char* path, mode_t mode) {
    return mkdirat(AT_FDCWD, path, mode);
}

extern "C"
int renameat(int olddirfd, const char* oldpath, int newdirfd,
             const char* newpath) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, olddirh, newdirh;
    unique_handle file;

    if (olddirfd == AT_FDCWD)
        olddirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        olddirh = (HANDLE)_get_osfhandle(olddirfd);
        if (olddirh == (HANDLE)-1)
            return -1;
    }

    if (newdirfd == AT_FDCWD)
        newdirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        newdirh = (HANDLE)_get_osfhandle(newdirfd);
        if (newdirh == (HANDLE)-1)
            return -1;
    }

    if (!oldpath || !newpath || oldpath[0] == 0 || newpath[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto oldsv = string_view(oldpath);
    auto u16len = utf8_to_utf16_len(oldsv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), oldsv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = olddirh;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                          0, nullptr, 0);

    free(buf);

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    auto newsv = string_view(newpath);
    u16len = utf8_to_utf16_len(newsv);

    size_t frilen = offsetof(FILE_RENAME_INFORMATION, FileName) + (u16len * sizeof(char16_t));

    auto fri = (FILE_RENAME_INFORMATION*)malloc(frilen);
    if (!fri) {
        *_errno() = ENOMEM;
        return -1;
    }

    fri->ReplaceIfExists = true; // FIXME? --interactive option
    fri->RootDirectory = newdirh;
    fri->FileNameLength = (uint32_t)(u16len * sizeof(char16_t));
    utf8_to_utf16(span(fri->FileName, fri->FileNameLength / sizeof(char16_t)),
                  newsv);

    status = NtSetInformationFile(file.get(), &iosb, fri, (uint32_t)frilen,
                                  FILE_INFORMATION_CLASS::RenameInformation);

    free(fri);

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

static void win_to_unix_time(int64_t t, timespec& v) {
    t -= 116444736000000000;

    v.tv_sec = (__time64_t)(t / 10000000);
    v.tv_nsec = (long)((t % 10000000) * 100);
}

static int do_fstat(HANDLE h, struct stat& st) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_STAT_LX_INFORMATION fsli;

    status = NtQueryInformationFile(h, &iosb, &fsli, sizeof(fsli),
                                    FILE_INFORMATION_CLASS::StatLxInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    memset(&st, 0, sizeof(st));

    // FIXME - st.st_dev
    st.st_ino = (ino_t)fsli.FileId.QuadPart;
    st.st_mode = (mode_t)fsli.LxMode;
    st.st_nlink = (nlink_t)fsli.NumberOfLinks;
    st.st_uid = fsli.LxUid;
    st.st_gid = fsli.LxGid;
    // FIXME - st.st_rdev

    st.st_size = fsli.EndOfFile.QuadPart;
    win_to_unix_time(fsli.LastAccessTime.QuadPart, st.st_atim);
    win_to_unix_time(fsli.LastWriteTime.QuadPart, st.st_mtim);
    win_to_unix_time(fsli.ChangeTime.QuadPart, st.st_ctim);

    st.st_blksize = 0x1000; // FIXME
    st.st_blocks = (fsli.AllocationSize.QuadPart + 511) / 512;

    return 0;
}

extern "C"
int fstatat(int dirfd, const char* path, struct stat* st, int flags) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;
    uint32_t options;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (flags & ~AT_SYMLINK_NOFOLLOW) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    options = 0;

    if (flags & AT_SYMLINK_NOFOLLOW)
        options |= FILE_OPEN_REPARSE_POINT;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, options,
                          nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    unique_handle f{h};

    return do_fstat(f.get(), *st);
}

extern "C"
int stat(const char* path, struct stat* st) {
    return fstatat(AT_FDCWD, path, st, 0);
}


extern "C"
int lstat(const char* path, struct stat* st) {
    return fstatat(AT_FDCWD, path, st, AT_SYMLINK_NOFOLLOW);
}

extern "C"
int fstat(int fd, struct stat* st) {
    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    return do_fstat(h, *st);
}

static int do_fchown(HANDLE h, uid_t owner, gid_t group) {
    nt_status status;
    MN_FCHOWN p;
    IO_STATUS_BLOCK iosb;

    p.uid = owner;
    p.gid = group;

    status = NtSetInformationFile(h, &iosb, &p, sizeof(p),
                                  FILE_INFORMATION_CLASS::MoxnoxFchown);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int fchownat(int dirfd, const char* path, uid_t owner, gid_t group, int flags) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;
    uint32_t options;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (flags & ~AT_SYMLINK_NOFOLLOW) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    options = 0;

    if (flags & AT_SYMLINK_NOFOLLOW)
        options |= FILE_OPEN_REPARSE_POINT;

    status = NtCreateFile(&h, FILE_WRITE_ATTRIBUTES, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, options,
                          nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    unique_handle f{h};

    return do_fchown(f.get(), owner, group);
}

extern "C"
int fchown(int fd, uid_t owner, gid_t group) {
    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    return do_fchown(h, owner, group);
}

extern "C"
int chown(const char* path, uid_t owner, gid_t group) {
    return fchownat(AT_FDCWD, path, owner, group, 0);
}

extern "C"
int lchown(const char* path, uid_t owner, gid_t group) {
    return fchownat(AT_FDCWD, path, owner, group, AT_SYMLINK_NOFOLLOW);
}

extern "C"
int faccessat(int dirfd, const char* path, int /*mode*/, int flags) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;

    if (dirfd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(dirfd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (flags & ~AT_EACCESS) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    // FIXME - AT_EACCESS

    // FIXME - translate mode into access mask
    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    // FIXME - do this properly

    NtClose(h);

    return 0;
}

extern "C"
int posix_fadvise(int fd, off_t /*offset*/, off_t len, int advice) {
    if (len < 0)
        return EINVAL;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return EBADF;

    switch (advice) {
        case POSIX_FADV_NORMAL:
        case POSIX_FADV_SEQUENTIAL:
        case POSIX_FADV_RANDOM:
        case POSIX_FADV_WILLNEED:
        case POSIX_FADV_DONTNEED:
        case POSIX_FADV_NOREUSE:
            // FIXME - actually do something
            return 0;

        default:
            return EINVAL;
    }
}

extern "C"
long fpathconf(int /*fd*/, int name) {
    switch (name) {
        case _PC_PATH_MAX:
            return PATH_MAX;

        case _PC_NAME_MAX:
            return 255;

        case _PC_PIPE_BUF:
            return 1; // FIXME - POSIX says this should be at least 512

        default:
            *_errno() = EINVAL;
            return -1;
    }
}

extern "C"
long pathconf(const char* /*path*/, int name) {
    // FIXME

    return fpathconf(0, name);
}

extern "C"
size_t confstr(int name, char* buf, size_t len) {
    static const string_view path = "/bin";

    switch (name) {
        case _CS_PATH:
            if (buf && len > 0) {
                size_t n = min(len, path.size() + 1);

                memcpy(buf, path.data(), n - 1);
                buf[n - 1] = 0;
            }

            return path.size() + 1;

        default:
            *_errno() = EINVAL;
            return 0;
    }
}

extern "C"
FILE* open_memstream(char** bufp, size_t* sizep) {
    return mn_open_memstream(bufp, sizep);
}

extern "C"
int mkfifoat(int fd, const char* path, mode_t mode) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h, dirh;
    unique_handle f;
    REPARSE_DATA_BUFFER rdb;

    if (fd == AT_FDCWD)
        dirh = peb->ProcessParameters->CurrentDirectory.Handle;
    else {
        dirh = (HANDLE)_get_osfhandle(fd);
        if (dirh == (HANDLE)-1)
            return -1;
    }

    if (!path || !path[0]) {
        *_errno() = EINVAL;
        return -1;
    }

    auto pathsv = string_view(path);

    auto u16len = utf8_to_utf16_len(pathsv);
    auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16, u16len), pathsv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = dirh;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES | DELETE, &oa, &iosb, nullptr,
                          0, 0, FILE_CREATE, FILE_NON_DIRECTORY_FILE, nullptr, 0);

    free(u16);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        *_errno() = EEXIST;
        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    } else
        f.reset(h);

    rdb.ReparseTag = IO_REPARSE_TAG_LX_FIFO;
    rdb.ReparseDataLength = 0;
    rdb.Reserved = 0;

    status = NtFsControlFile(f.get(), 0, nullptr, nullptr, &iosb,
                             FSCTL_SET_REPARSE_POINT, &rdb,
                             offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer),
                             nullptr, 0);
    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        NtSetInformationFile(f.get(), &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);

        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    // FIXME - this should be atomic

    auto m = (uint32_t)mode & ~mn_get_umask();

    status = NtSetInformationFile(f.get(), &iosb, &m, sizeof(m),
                                  FILE_INFORMATION_CLASS::MoxnoxFchmod);
    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        NtSetInformationFile(f.get(), &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);

        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int mkfifo(const char* path, mode_t mode) {
    return mkfifoat(AT_FDCWD, path, mode);
}

extern "C"
int mknodat(int fd, const char* path, mode_t mode, dev_t /*dev*/) {
    if (S_ISFIFO(mode))
        return mkfifoat(fd, path, mode & ~S_IFMT);
    else if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISDIR(mode) || S_ISREG(mode)) {
        *_errno() = EPERM;
        return -1;
    } else {
        *_errno() = EINVAL;
        return -1;
    }
}

extern "C"
int mknod(const char* path, mode_t mode, dev_t dev) {
    return mknodat(AT_FDCWD, path, mode, dev);
}

extern "C"
ssize_t readv(int fd, const iovec* iov, int iovcnt) {
    IO_STATUS_BLOCK iosb;
    uint64_t bytes;

    if (iovcnt <= 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    while (true) {
        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtDeviceIoControlFile(h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_FILE_READV, iov,
                                            (uint32_t)sizeof(iovec) * iovcnt, &bytes,
                                            sizeof(bytes));
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

    return (ssize_t)bytes;
}

extern "C"
ssize_t writev(int fd, const iovec* iov, int iovcnt) {
    IO_STATUS_BLOCK iosb;
    uint64_t bytes;

    if (iovcnt <= 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    while (true) {
        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtDeviceIoControlFile(h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_FILE_WRITEV, iov,
                                            (uint32_t)sizeof(iovec) * iovcnt, &bytes,
                                            sizeof(bytes));
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

    return (ssize_t)bytes;
}

static char mktemp_char(unsigned int i) {
    if (i < 10)
        return (char)('0' + i);
    else
        return (char)('a' + i - 10);
}

extern "C"
int mkostemp(char* tmpl, int flag) {
    auto len = strlen(tmpl);

    if (len < 6) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sp = span(tmpl + len - 6, 6);

    for (auto c : sp) {
        if (c != 'X') {
            *_errno() = EINVAL;
            return -1;
        }
    }

    while (true) {
        int old_errno;

        auto r = rand();

        sp[0] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[1] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[2] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[3] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[4] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[5] = mktemp_char((unsigned int)(r % 36));

        old_errno = *_errno();
        *_errno() = 0;

        auto fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL | flag,
                       S_IRUSR | S_IWUSR);

        if (fd == -1) {
            if (*_errno() == EEXIST) {
                *_errno() = old_errno;
                continue;
            } else
                return -1;
        }

        *_errno() = old_errno;

        return fd;
    }
}

extern "C"
int mkstemp(char* tmpl) {
    return mkostemp(tmpl, 0);
}

extern "C"
char* mkdtemp(char* tmpl) {
    auto len = strlen(tmpl);

    if (len < 6) {
        *_errno() = EINVAL;
        return nullptr;
    }

    auto sp = span(tmpl + len - 6, 6);

    for (auto c : sp) {
        if (c != 'X') {
            *_errno() = EINVAL;
            return nullptr;
        }
    }

    while (true) {
        int old_errno;

        auto r = rand();

        sp[0] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[1] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[2] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[3] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[4] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[5] = mktemp_char((unsigned int)(r % 36));

        old_errno = *_errno();
        *_errno() = 0;

        auto ret = mkdir(tmpl, S_IRWXU);
        if (ret == -1) {
            if (*_errno() == EEXIST) {
                *_errno() = old_errno;
                continue;
            } else
                return nullptr;
        }

        *_errno() = old_errno;

        return tmpl;
    }
}
