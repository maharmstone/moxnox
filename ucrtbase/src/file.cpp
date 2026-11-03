// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <stdio.h>
#include <wchar.h>
#include <string_view>
#include <span>
#include <optional>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <utility>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

export module file;

import print;
import nt;
import unicode;
import ntdll;
import list;
import ucrtbase;
import kernelbase;
import unique_handle;
import time;
import moxnox_ioctls;
import format;
import exec;

#undef errno
import errno;

using namespace std;

#undef st_atime
#undef st_mtime
#undef st_ctime

static const size_t BUFFER_SIZE = 65536;

enum class mode_type {
    invalid,
    r,
    rplus,
    w,
    wplus,
    a,
    aplus,
};

struct _FILE {
    _FILE(HANDLE h, int fd, bool readable, bool writable);
    _FILE(char** bufp, size_t* sizep);
    ~_FILE();

    HANDLE h;
    int fd;
    RTL_CRITICAL_SECTION cs;
    list_entry le;
    optional<unsigned char> ungetc_char;
    optional<wchar_t> ungetwc_char;
    HANDLE proc = 0;
    bool error = false;
    bool eof = false;
    wchar_t getwc_surrogate = 0;
    wchar_t putwc_surrogate = 0;
    bool memstream = false;
    bool readable;
    bool writable;

    struct {
        char** bufp;
        size_t* sizep;
        char* buffer;
        size_t position;
        size_t length;
        size_t capacity;
    } mem;

    struct {
        uint8_t* data = nullptr;
        size_t size = BUFFER_SIZE;
        size_t filled = 0;
        size_t consumed = 0;
        size_t read_bytes = 0;
        bool reading;
        bool user_allocated = false;
        int mode = _IOFBF;
    } buffer;
};

struct fd {
    HANDLE h;
    bool cloexec;
};

// ntdll
extern "C" void (*mn_handle_exec_info)(exec_info* info) __declspec(dllimport);

static void handle_exec_info(exec_info* info);
extern "C" size_t __fwrite_nolock(const void* ptr, size_t size, size_t n, FILE* file);
extern "C" int _fflush_nolock(FILE* file);
extern "C" int __isatty(int fd);

static array<fd, 2048> fds;

static uint8_t _stdin[sizeof(_FILE)], _stdout[sizeof(_FILE)], _stderr[sizeof(_FILE)];
static RTL_CRITICAL_SECTION files_lock, fds_lock;
static list_entry files_list = { &files_list, &files_list };

static int umask_value = S_IWGRP | S_IWOTH;

// FIXME - should be getting from sys/stat.h
extern "C" int mkdir(const char* path, mode_t mode);

extern "C" int _getpid();

extern "C" int* __errno();

// dir.cpp
extern "C" int __chdir(const char* path);
extern "C++" bool get_dos_path(HANDLE h, UNICODE_STRING& us);

extern "C++"
void* operator new(size_t count) {
    return malloc(count);
}

extern "C++"
void* operator new[](size_t count) {
    return malloc(count);
}

extern "C++"
void operator delete(void* ptr) noexcept {
    free(ptr);
}

extern "C++"
void operator delete[](void* ptr) noexcept {
    free(ptr);
}

extern "C++"
void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

extern "C++"
void operator delete[](void* ptr, size_t) noexcept {
    free(ptr);
}

extern "C"
FILE* ___acrt_iob_func(unsigned int idx) {
    switch (idx) {
        case 0:
            return reinterpret_cast<FILE*>(&_stdin);

        case 1:
            return reinterpret_cast<FILE*>(&_stdout);

        case 2:
            return reinterpret_cast<FILE*>(&_stderr);

        default:
            return nullptr;
    }
}

extern "C"
void __lock_file(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    RtlEnterCriticalSection(&f.cs);
}

extern "C"
void __unlock_file(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    RtlLeaveCriticalSection(&f.cs);
}

extern "C"
int __fputc_nolock(int c, FILE* file) {
    auto v = (uint8_t)c;

    auto ret = __fwrite_nolock(&v, 1, 1, file);

    if (ret == 0)
        return EOF;

    return (int)v;
}

extern "C"
int _fputc(int c, FILE* file) {
    __lock_file(file);
    auto ret = __fputc_nolock(c, file);
    __unlock_file(file);

    return ret;
}

_FILE::_FILE(HANDLE h, int fd, bool readable, bool writable) : h(h), fd(fd), readable(readable), writable(writable) {
    RtlInitializeCriticalSection(&cs);

    buffer.reading = !writable;
}

_FILE::_FILE(char** bufp, size_t* sizep) {
    h = 0;
    fd = 0;
    memstream = true;
    readable = true;
    writable = true;

    mem.bufp = bufp;
    mem.sizep = sizep;
    mem.buffer = nullptr;
    mem.position = 0;
    mem.length = 0;
    mem.capacity = 0;

    buffer.reading = false;

    RtlInitializeCriticalSection(&cs);
}

_FILE::~_FILE() {
    RtlDeleteCriticalSection(&cs);

    if (!memstream) {
        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)fd].h = 0;
        RtlLeaveCriticalSection(&fds_lock);
    }

    if (buffer.data && !buffer.user_allocated)
        free(buffer.data);
}

export void init_files() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    RtlInitializeCriticalSection(&files_lock);
    RtlInitializeCriticalSection(&fds_lock);

    auto f1 = new (_stdin) _FILE(peb->ProcessParameters->StandardInput, 0,
                                 true, false);
    auto f2 = new (_stdout) _FILE(peb->ProcessParameters->StandardOutput, 1,
                                  false, true);
    auto f3 = new (_stderr) _FILE(peb->ProcessParameters->StandardError, 2,
                                  false, true);

    insert_tail_list(files_list, f1->le);
    insert_tail_list(files_list, f2->le);
    insert_tail_list(files_list, f3->le);

    fds[0].h = peb->ProcessParameters->StandardInput;
    fds[0].cloexec = false;
    fds[1].h = peb->ProcessParameters->StandardOutput;
    fds[1].cloexec = false;
    fds[2].h = peb->ProcessParameters->StandardError;
    fds[2].cloexec = false;

    if (__isatty(1))
        reinterpret_cast<_FILE*>(&_stdout)->buffer.mode = _IOLBF;

    reinterpret_cast<_FILE*>(&_stderr)->buffer.mode = _IONBF;

    mn_handle_exec_info = handle_exec_info;
}

static mode_type parse_mode(const char* mode) {
    mode_type type;

    switch (mode[0]) {
        case 'r':
            mode++;

            if (*mode == 'b')
                mode++;

            if (*mode == '+') {
                type = mode_type::rplus;
                mode++;
            } else
                type = mode_type::r;

            break;

        case 'w':
            mode++;

            if (*mode == 'b')
                mode++;

            if (*mode == '+') {
                type = mode_type::wplus;
                mode++;
            } else
                type = mode_type::w;

            break;

        case 'a':
            mode++;

            if (*mode == 'b')
                mode++;

            if (*mode == '+') {
                type = mode_type::aplus;
                mode++;
            } else
                type = mode_type::a;

            break;

        default:
            return mode_type::invalid;
    }

    // FIXME - implement MS' extensions?

    while (*mode != 0) {
        if (*mode != 't' && *mode != 'b')
            return mode_type::invalid;

        mode++;
    }

    return type;
}

static bool do_open(const char* pathname, const char* mode, HANDLE& h, int& fd,
                    bool for_delete, bool cloexec, bool& readable,
                    bool& writable) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    ACCESS_MASK perms;
    unsigned int create_options;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;

    if (!mode || !pathname || pathname[0] == 0) {
        *__errno() = EINVAL;
        return false;
    }

    auto type = parse_mode(mode);

    if (type == mode_type::invalid) {
        *__errno() = EINVAL;
        return false;
    }

    switch (type) {
        using enum mode_type;

        case r:
            perms = FILE_READ_DATA;
            create_options = FILE_OPEN;
            readable = true;
            writable = false;
            break;

        case rplus:
            perms = FILE_READ_DATA | FILE_WRITE_DATA;
            create_options = FILE_OPEN;
            readable = true;
            writable = true;
            break;

        case w:
            perms = FILE_WRITE_DATA;
            create_options = FILE_OVERWRITE_IF;
            readable = false;
            writable = true;
            break;

        case wplus:
            perms = FILE_READ_DATA | FILE_WRITE_DATA;
            create_options = FILE_OVERWRITE_IF;
            readable = true;
            writable = true;
            break;

        case a:
            perms = FILE_APPEND_DATA;
            create_options = FILE_OPEN_IF;
            readable = false;
            writable = true;
            break;

        case aplus:
            perms = FILE_READ_DATA | FILE_APPEND_DATA;
            create_options = FILE_OPEN_IF;
            readable = true;
            writable = true;
            break;

        default:
            unreachable();
    }

    if (for_delete)
        perms |= DELETE;

    auto sv = string_view(pathname);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *__errno() = ENOMEM;
        return false;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    auto status = NtCreateFile(&h, perms, &oa, &iosb, nullptr,
                               FILE_ATTRIBUTE_NORMAL, 0, create_options,
                               0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return false;
    }

    free(u16pathname);

    optional<int> optfd;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = h;
            fds[i].cloexec = cloexec;
            optfd = i;
            break;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    if (!optfd.has_value()) {
        NtClose(h);
        *__errno() = EMFILE;
        return false;
    }

    fd = *optfd;

    return true;
}

extern "C"
FILE* _fopen(const char* pathname, const char* mode) {
    HANDLE h;
    int fd;
    bool readable, writable;

    // FIXME - cloexec if "e" is in mode

    if (!do_open(pathname, mode, h, fd, false, false, readable, writable))
        return nullptr;

    auto f = new _FILE(h, fd, readable, writable);
    if (!f) {
        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)fd].h = 0;
        RtlLeaveCriticalSection(&fds_lock);

        NtClose(h);

        *__errno() = ENOMEM;
        return nullptr;
    }

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
size_t __fread_nolock(void* ptr, size_t size, size_t n, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    IO_STATUS_BLOCK iosb;
    size_t bytes = size * n, read = 0;
    nt_status status;

    if (size == 0 || n == 0)
        return 0;

    if (f.memstream) {
        *_errno() = EBADF;
        return 0;
    }

    if (bytes == 0)
        return 0;

    if (f.ungetc_char.has_value()) {
        *(uint8_t*)ptr = *f.ungetc_char;
        f.ungetc_char = nullopt;

        ptr = (uint8_t*)ptr + 1;
        bytes--;
        read = 1;
    }

    if (!f.buffer.reading) {
        _fflush_nolock(file);
        f.buffer.reading = true;
    }

    if (f.buffer.consumed < f.buffer.read_bytes) {
        auto to_copy = min(f.buffer.read_bytes - f.buffer.consumed, bytes);

        memcpy(ptr, f.buffer.data + f.buffer.consumed, to_copy);
        f.buffer.consumed += to_copy;
        ptr = (uint8_t*)ptr + to_copy;
        bytes -= to_copy;
        read += to_copy;
    }

    if (f.buffer.consumed == f.buffer.read_bytes && bytes > 0 && f.buffer.mode != _IONBF) {
        if (!f.buffer.data) {
            f.buffer.data = (uint8_t*)malloc(f.buffer.size);
            if (!f.buffer.data) {
                *__errno() = ENOMEM;
                f.error = true;
                return 0;
            }
        }

        auto sp = span(f.buffer.data, f.buffer.size);
        size_t read_bytes = 0;

        while (true) {
            status = NtReadFile(f.h, 0, nullptr, nullptr, &iosb, sp.data(),
                                (uint32_t)sp.size(), nullptr, nullptr);
            if (status == nt_status::USER_APC)
                continue;

            break;
        }

        if (status != nt_status::END_OF_FILE && status != nt_status::PIPE_DISCONNECTED) {
            if (!success(status)) {
                if (read == 0) {
                    *__errno() = nt_status_to_errno(status);
                    f.error = true;
                }

                return read / size;
            }

            read_bytes = iosb.Information;
        }

        f.buffer.read_bytes = read_bytes;
        f.buffer.consumed = 0;

        auto to_copy = min(read_bytes, bytes);

        memcpy(ptr, f.buffer.data + f.buffer.consumed, to_copy);
        f.buffer.consumed += to_copy;
        ptr = (uint8_t*)ptr + to_copy;
        bytes -= to_copy;
        read += to_copy;
    }

    if (bytes == 0)
        return read / size;

    while (true) {
        status = NtReadFile(f.h, 0, nullptr, nullptr, &iosb, ptr, (uint32_t)bytes,
                            nullptr, nullptr);
        if (status == nt_status::USER_APC)
            continue;
        else if (!success(status))
            break;

        read += iosb.Information;
        ptr = (uint8_t*)ptr + iosb.Information;
        bytes -= iosb.Information;

        if (bytes == 0)
            break;

        if (iosb.Information == 0) {
            f.eof = true;
            break;
        }
    }

    if (status == nt_status::END_OF_FILE || status == nt_status::PIPE_DISCONNECTED)
        f.eof = true;
    else if (!success(status) && read == 0) {
        *__errno() = nt_status_to_errno(status);
        f.error = true;
        return 0;
    }

    return read / size;
}

extern "C"
size_t _fread(void* ptr, size_t size, size_t n, FILE* file) {
    __lock_file(file);
    auto ret = __fread_nolock(ptr, size, n, file);
    __unlock_file(file);

    return ret;
}

extern "C"
size_t __fwrite_nolock(const void* ptr, size_t size, size_t n, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    auto bytes = size * n;

    if (size == 0 || n == 0)
        return 0;

    if (f.memstream) {
        if (f.mem.capacity < f.mem.position + bytes + 1) {
            size_t newcapacity = max(2 * f.mem.capacity, f.mem.position + bytes + 1);

            auto newbuf = (char*)realloc(f.mem.buffer, newcapacity);
            if (!newbuf) {
                *__errno() = ENOMEM;
                f.error = true;
                return 0;
            }

            f.mem.buffer = newbuf;
            f.mem.capacity = newcapacity;
        }

        if (f.mem.length < f.mem.position)
            memset(f.mem.buffer + f.mem.length, 0, f.mem.position - f.mem.length);

        memcpy(f.mem.buffer + f.mem.position, ptr, bytes);
        f.mem.position += bytes;
        f.mem.length = max(f.mem.length, f.mem.position);

        return n;
    } else {
        IO_STATUS_BLOCK iosb;
        nt_status status;
        size_t written = 0;

        if (f.buffer.reading) {
            f.buffer.consumed = 0;
            f.buffer.read_bytes = 0;
            f.buffer.reading = false;
        }

        if (f.buffer.mode != _IONBF) {
            size_t to_flush = 0;

            if (f.buffer.filled < f.buffer.size) {
                if (!f.buffer.data) {
                    f.buffer.data = (uint8_t*)malloc(f.buffer.size);
                    if (!f.buffer.data) {
                        *__errno() = ENOMEM;
                        f.error = true;
                        return 0;
                    }
                }

                auto to_copy = min(f.buffer.size - f.buffer.filled, bytes);

                memcpy(f.buffer.data + f.buffer.filled, ptr, to_copy);
                f.buffer.filled += to_copy;
                ptr = (uint8_t*)ptr + to_copy;
                bytes -= to_copy;
                written += to_copy;
            }

            if (f.buffer.filled == f.buffer.size)
                to_flush = f.buffer.size;
            else if (f.buffer.mode == _IOLBF) {
                auto nl = string_view((char*)f.buffer.data, f.buffer.filled).rfind('\n');

                if (nl != string_view::npos)
                    to_flush = nl + 1;
            }

            if (to_flush != 0) {
                size_t buffer_written = 0;
                auto sp = span(f.buffer.data, to_flush);

                while (!sp.empty()) {
                    while (true) {
                        status = NtWriteFile(f.h, 0, nullptr, nullptr, &iosb,
                                             sp.data(), (uint32_t)sp.size(),
                                             nullptr, nullptr);
                        if (status == nt_status::USER_APC)
                            continue;

                        break;
                    }

                    if (!success(status)) {
                        *__errno() = nt_status_to_errno(status);
                        f.error = true;

                        if (buffer_written > 0) {
                            memmove(f.buffer.data, f.buffer.data + buffer_written,
                                    f.buffer.filled - buffer_written);
                            f.buffer.filled -= buffer_written;
                        }

                        return written / size;
                    }

                    if (iosb.Information == sp.size())
                        break;

                    buffer_written += iosb.Information;
                    sp = span(sp.data() + iosb.Information,
                              sp.size() - iosb.Information);
                }

                if (to_flush == f.buffer.size)
                    f.buffer.filled = 0;
                else {
                    memmove(f.buffer.data, f.buffer.data + to_flush,
                            f.buffer.filled - to_flush);
                    f.buffer.filled -= to_flush;
                }
            }

            if (bytes == 0)
                return written / size;
        }

        while (true) {
            status = NtWriteFile(f.h, 0, nullptr, nullptr, &iosb, ptr,
                                 (uint32_t)bytes, nullptr, nullptr);
            if (status == nt_status::USER_APC)
                continue;

            break;
        }

        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            f.error = true;
            return 0;
        }

        if (success(status))
            written += iosb.Information;

        return written / size;
    }
}

extern "C"
size_t _fwrite(const void* ptr, size_t size, size_t n, FILE* file) {
    __lock_file(file);
    auto ret = __fwrite_nolock(ptr, size, n, file);
    __unlock_file(file);

    return ret;
}

extern "C"
int _fflush_nolock(FILE* file) {
    if (!file)
        return 0;

    auto& f = *reinterpret_cast<_FILE*>(file);

    if (f.memstream) {
        if (f.mem.capacity < f.mem.position + 1) {
            size_t newcapacity = max(2 * f.mem.capacity, f.mem.position + 1);
            auto newbuf = (char*)realloc(f.mem.buffer, newcapacity);

            if (!newbuf) {
                f.error = true;
                *__errno() = ENOMEM;
                return EOF;
            }

            f.mem.buffer = newbuf;
            f.mem.capacity = newcapacity;
        }

        if (f.mem.position > f.mem.length) {
            memset(f.mem.buffer + f.mem.length, 0, f.mem.position - f.mem.length);
            f.mem.length = f.mem.position;
        }

        f.mem.buffer[f.mem.position] = 0;

        *f.mem.bufp = f.mem.buffer;
        *f.mem.sizep = f.mem.position;

        return 0;
    } else {
        if (f.buffer.reading && (f.buffer.read_bytes > f.buffer.consumed || f.ungetc_char.has_value())) {
            FILE_POSITION_INFORMATION fpi;
            IO_STATUS_BLOCK iosb;

            auto status = NtQueryInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                                 FILE_INFORMATION_CLASS::PositionInformation);
            if (!success(status))
                return 0;

            fpi.CurrentByteOffset.QuadPart -= f.buffer.read_bytes - f.buffer.consumed;

            if (f.ungetc_char.has_value())
                fpi.CurrentByteOffset.QuadPart--;

            status = NtSetInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                          FILE_INFORMATION_CLASS::PositionInformation);
            if (!success(status))
                return 0;

            f.buffer.consumed = 0;
            f.buffer.read_bytes = 0;
            f.ungetc_char = nullopt;
            f.ungetwc_char = nullopt;
        } else if (!f.buffer.reading && f.buffer.filled > 0) {
            size_t buffer_written = 0;
            auto sp = span(f.buffer.data, f.buffer.filled);

            while (!sp.empty()) {
                IO_STATUS_BLOCK iosb;

                while (true) {
                    auto status = NtWriteFile(f.h, 0, nullptr, nullptr, &iosb,
                                              sp.data(), (uint32_t)sp.size(),
                                              nullptr, nullptr);
                    if (status == nt_status::USER_APC)
                        continue;

                    if (!success(status)) {
                        *__errno() = nt_status_to_errno(status);
                        f.error = true;

                        if (buffer_written > 0) {
                            memmove(f.buffer.data, f.buffer.data + buffer_written,
                                    f.buffer.filled - buffer_written);
                            f.buffer.filled -= buffer_written;
                        }

                        return EOF;
                    }

                    break;
                }

                if (iosb.Information == sp.size())
                    break;

                buffer_written += iosb.Information;
                sp = span(sp.data() + iosb.Information,
                          sp.size() - iosb.Information);
            }

            f.buffer.filled = 0;
        }
    }

    return 0;
}

export extern "C"
int _fflush(FILE* file) {
    if (!file) {
        int ret = 0;

        // FIXME - can we avoid doing NtWriteFile calls while holding files_lock?

        RtlEnterCriticalSection(&files_lock);

        for (auto le = files_list.next; le != &files_list; le = le->next) {
            auto& f = containing_record<_FILE>(le);

            if (auto ret2 = _fflush(reinterpret_cast<FILE*>(&f)); ret2 != 0)
                ret = ret2;
        }

        RtlLeaveCriticalSection(&files_lock);

        return ret;
    }

    __lock_file(file);
    auto ret = _fflush_nolock(file);
    __unlock_file(file);

    return ret;
}

extern "C"
int _fclose(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    int ret = 0;

    {
        RtlEnterCriticalSection(&files_lock);
        remove_entry_list(f.le);
        RtlLeaveCriticalSection(&files_lock);
    }

    if (_fflush(file) != 0)
        ret = EOF;

    if (!f.memstream)
        NtClose(f.h);

    if (file != (FILE*)_stdin && file != (FILE*)_stdout && file != (FILE*)_stderr)
        delete &f;

    return ret;
}

extern "C"
int _fputs(const char* s, FILE* file) {
    auto len = strlen(s);

    __lock_file(file);
    auto ret = __fwrite_nolock(s, 1, len, file);
    __unlock_file(file);

    if (ret != len)
        return EOF;
    else
        return 0;
}

extern "C"
int puts(const char* s) {
    auto len = strlen(s);
    auto* f = reinterpret_cast<FILE*>(_stdout);
    const char c = '\n';

    __lock_file(f);
    auto ret = __fwrite_nolock(s, 1, len, f);

    if (ret != len) {
        __unlock_file(f);
        return EOF;
    }

    ret = __fwrite_nolock(&c, 1, 1, f);
    __unlock_file(f);

    if (ret != len)
        return EOF;
    else
        return 0;
}

extern "C"
int __fgetc_nolock(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    unsigned char c;

    if (f.ungetc_char.has_value()) {
        c = *f.ungetc_char;
        f.ungetc_char = nullopt;
        return (int)c;
    }

    auto ret = __fread_nolock(&c, 1, 1, file);
    if (ret == 0)
        return EOF;

    return (int)c;
}

extern "C"
int _fgetc(FILE* file) {
    __lock_file(file);
    auto ret = __fgetc_nolock(file);
    __unlock_file(file);

    return ret;
}

extern "C"
int __ungetc_nolock(int c, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    if (f.ungetc_char.has_value())
        return EOF;

    f.ungetc_char = (unsigned char)c;

    f.error = false;
    f.eof = false;

    return c;
}

extern "C"
int _ungetc(int c, FILE* file) {
    __lock_file(file);
    auto ret = __ungetc_nolock(c, file);
    __unlock_file(file);

    return ret;
}

extern "C"
char* _fgets(char* s, int size, FILE* file) {
    int copied = 0;

    __lock_file(file);

    for (int i = 0; i < size - 1; i++) {
        auto c = __fgetc_nolock(file);
        if (c == EOF)
            break;

        s[i] = (char)c;
        copied++;

        if (c == '\n')
            break;
    }

    __unlock_file(file);

    if (copied == 0)
        return nullptr;

    s[copied] = 0;

    return s;
}

extern "C"
int64_t __ftelli64_nolock(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;
    int64_t ret;

    if (f.memstream)
        ret = f.mem.position;
    else {
        auto status = NtQueryInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                             FILE_INFORMATION_CLASS::PositionInformation);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            return -1;
        }

        ret = fpi.CurrentByteOffset.QuadPart;
    }

    if (f.ungetc_char.has_value())
        ret--;

    if (f.buffer.reading)
        ret -= f.buffer.read_bytes - f.buffer.consumed;
    else
        ret += f.buffer.filled;

    return ret;
}

extern "C"
int64_t __ftelli64(FILE* file) {
    __lock_file(file);
    auto ret = __ftelli64_nolock(file);
    __unlock_file(file);

    return ret;
}

extern "C"
int32_t __ftell_nolock(FILE* file) {
    return (int32_t)__ftelli64_nolock(file);
}

extern "C"
int32_t _ftell(FILE* file) {
    __lock_file(file);
    auto ret = __ftelli64_nolock(file);
    __unlock_file(file);

    return (int32_t)ret;
}

extern "C"
int __fseeki64_nolock(FILE* file, int64_t offset, int whence) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    int64_t start;
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;
    nt_status status;

    auto ret = _fflush_nolock(file);
    if (ret == EOF)
        return -1;

    switch (whence) {
        case SEEK_SET:
            start = 0;
            break;

        case SEEK_CUR:
            start = __ftelli64_nolock(file);

            if (start == -1)
                return -1;
            break;

        case SEEK_END: {
            if (f.memstream)
                start = f.mem.length;
            else {
                FILE_STANDARD_INFORMATION fsi;

                status = NtQueryInformationFile(f.h, &iosb, &fsi, sizeof(fsi),
                                                FILE_INFORMATION_CLASS::StandardInformation);
                if (!success(status)) {
                    *__errno() = nt_status_to_errno(status);
                    return -1;
                }

                start = fsi.EndOfFile.QuadPart;
            }

            break;
        }

        default:
            *__errno() = EINVAL;
            return -1;
    }

    if (f.memstream) {
        if (start + offset < 0) {
            *__errno() = EINVAL;
            return -1;
        }

        f.mem.position = start + offset;
    } else {
        fpi.CurrentByteOffset.QuadPart = start + offset;

        status = NtSetInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                      FILE_INFORMATION_CLASS::PositionInformation);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            return -1;
        }
    }

    f.ungetc_char = nullopt;
    f.ungetwc_char = nullopt;
    f.getwc_surrogate = 0;
    f.putwc_surrogate = 0;
    f.eof = false;

    return 0;
}

extern "C"
int __fseeki64(FILE* file, int64_t offset, int whence) {
    __lock_file(file);
    auto ret = __fseeki64_nolock(file, offset, whence);
    __unlock_file(file);

    return ret;
}

extern "C"
int __fseek_nolock(FILE* file, int32_t offset, int whence) {
    return __fseeki64_nolock(file, offset, whence);
}

extern "C"
int _fseek(FILE* file, int32_t offset, int whence) {
    __lock_file(file);
    auto ret = __fseeki64_nolock(file, offset, whence);
    __unlock_file(file);

    return ret;
}

extern "C"
int _remove(const char* pathname) {
    if (!pathname || pathname[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(pathname);

    auto u16len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), sv);

    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    auto status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0,
                               FILE_OPEN, 0, nullptr, 0);
    if (!success(status)) {
        free(buf);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(buf);

    fdi.DeleteFile = true;

    status = NtSetInformationFile(h, &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        NtClose(h);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    NtClose(h);

    return 0;
}

extern "C"
int atexit(_PVFV) {
    // stub - __mingw_vfprintf uses this through gdtoa

    return 0;
}

export extern "C"
int __stdio_common_vfprintf(uint64_t /*options*/, FILE* file,
                            const char* format, _locale_t /*locale*/,
                            va_list valist) {
    // FIXME - options
    // FIXME - locale

    return __mingw_vfprintf(file, format, valist);
}

extern "C"
int __stdio_common_vsprintf(uint64_t /*options*/, char* str, size_t len,
                            const char* format, _locale_t /*locale*/,
                            va_list valist) {
    // FIXME - options
    // FIXME - locale

    if (len == (size_t)-1)
        return __mingw_vsprintf(str, format, valist);
    else
        return __mingw_vsnprintf(str, len, format, valist);
}

extern "C"
int __stdio_common_vfwprintf(uint64_t /*options*/, FILE* file,
                             const wchar_t* format, _locale_t /*locale*/,
                             va_list valist) {
    // FIXME - options
    // FIXME - locale

    return __mingw_vfwprintf(file, format, valist);
}

extern "C"
int setvbuf(FILE* file, char* buffer, int mode, size_t size) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) {
        *__errno() = EINVAL;
        return -1;
    }

    __lock_file(file);

    if (f.buffer.data) {
        __unlock_file(file);
        *__errno() = EINVAL;
        return -1;
    }

    if (buffer) {
        f.buffer.data = (uint8_t*)buffer;
        f.buffer.size = size;
        f.buffer.user_allocated = true;
    } else if (size != 0)
        f.buffer.size = size;

    f.buffer.mode = mode;

    __unlock_file(file);

    return 0;
}

extern "C"
void setbuf(FILE* file, char* buf) {
    setvbuf(file, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

extern "C"
int ferror(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    return f.error ? 1 : 0;
}

extern "C"
int feof(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    return f.eof ? 1 : 0;
}

static int open2(const char* path, int flags, int pmode) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    ACCESS_MASK access = FILE_READ_ATTRIBUTES;
    uint32_t file_attributes, create_disposition, create_options;
    nt_status status;

    if (flags & _O_TEMPORARY) {
        print_stderr("FIXME - _open with _O_TEMPORARY\n");
        flags &= ~_O_TEMPORARY;
    }

    if (flags & _O_NOINHERIT) {
        print_stderr("FIXME - _open with _O_NOINHERIT\n");
        flags &= ~_O_NOINHERIT;
    }

    if (flags & _O_WTEXT) {
        print_stderr("FIXME - _open with _O_WTEXT\n");
        flags &= ~_O_WTEXT;
    }

    if (flags & _O_U16TEXT) {
        print_stderr("FIXME - _open with _O_U16TEXT\n");
        flags &= ~_O_U16TEXT;
    }

    if (flags & _O_U8TEXT) {
        print_stderr("FIXME - _open with _O_U8TEXT\n");
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
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, access, &oa, &iosb, nullptr, file_attributes,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, create_disposition,
                          create_options, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    if (iosb.Information == FILE_CREATED) {
        // FIXME - this should be atomic

        auto mode = (uint32_t)pmode & ~umask_value;

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
            *__errno() = nt_status_to_errno(status);
            NtClose(h);
            return -1;
        }

        if (fbi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            *__errno() = ELOOP;
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
            *__errno() = nt_status_to_errno(status);
            NtClose(h);
            return -1;
        }
    }

    optional<int> fd;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = h;
            fds[i].cloexec = !!(flags & O_CLOEXEC);
            fd = i;
            break;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    if (!fd.has_value()) {
        NtClose(h);
        *__errno() = ENFILE;
        return -1;
    }

    return *fd;
}

extern "C"
int __open(const char* path, int flags, ...) {
    va_list ap;

    if (flags & _O_CREAT) {
        int pmode;

        va_start(ap, flags);
        pmode = va_arg(ap, int);
        va_end(ap);

        return open2(path, flags, pmode);
    } else
        return open2(path, flags, 0);
}

static int sopen2(const char* path, int oflags, int /*shflags*/, int pmode) {
    // FIXME - shflags

    return open2(path, oflags, pmode);
}

extern "C"
int __sopen(const char* path, int oflags, int shflags, ...) {
    va_list ap;

    if (oflags & _O_CREAT) {
        int pmode;

        va_start(ap, shflags);
        pmode = va_arg(ap, int);
        va_end(ap);

        return sopen2(path, oflags, shflags, pmode);
    } else
        return sopen2(path, oflags, shflags, 0);
}

extern "C"
FILE* __fdopen(int fd, const char* mode) {
    HANDLE h;
    bool readable, writable;

    if (!mode) {
        *__errno() = EINVAL;
        return nullptr;
    } else if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EBADF;
        return nullptr;
    }

    auto type = parse_mode(mode);

    if (type == mode_type::invalid) {
        *__errno() = EINVAL;
        return nullptr;
    }

    switch (type) {
        using enum mode_type;

        case r:
            readable = true;
            writable = false;
            break;

        case w:
        case a:
            readable = false;
            writable = true;
            break;

        case rplus:
        case wplus:
        case aplus:
            readable = true;
            writable = true;
            break;

        case invalid:
            *__errno() = EINVAL;
            return nullptr;
    }

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd].h;
    RtlLeaveCriticalSection(&fds_lock);

    if (!h) {
        *__errno() = EBADF;
        return nullptr;
    }

    auto f = new _FILE(h, fd, readable, writable);
    if (!f) {
        *__errno() = ENOMEM;
        return nullptr;
    }

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
int __fileno(FILE* file) {
    if (!file) {
        *__errno() = EINVAL;
        return -1;
    }

    auto& f = *reinterpret_cast<_FILE*>(file);

    return f.fd;
}

extern "C"
int __chmod(const char* path, mode_t mode) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle f;

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
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_WRITE_ATTRIBUTES, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, 0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    f.reset(h);

    auto v = (uint32_t)mode;

    status = NtSetInformationFile(h, &iosb, &v, sizeof(v),
                                  FILE_INFORMATION_CLASS::MoxnoxFchmod);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int __utime64(const char* /*path*/, struct __utimbuf64* /*times*/) {
    // FIXME - implement

    return 0;
}

extern "C"
int __utime32(const char* path, struct __utimbuf32* times) {
    struct __utimbuf64 t;

    t.actime = times->actime;
    t.modtime = times->modtime;

    return __utime64(path, &t);
}

extern "C"
int getchar() {
    return _fgetc((FILE*)&_stdin);
}

extern "C"
intptr_t __get_osfhandle(int fd) {
    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    HANDLE h;

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd].h;
    RtlLeaveCriticalSection(&fds_lock);

    if (h == 0) {
        *__errno() = EBADF;
        return -1;
    }

    return (intptr_t)h;
}

extern "C"
int __open_osfhandle(intptr_t osfhandle, int flags) {
    optional<int> fd;

    // FIXME - flags?

    // FIXME - record last fd used somewhere, before we resort to searching the whole array?

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = (HANDLE)osfhandle;
            fds[i].cloexec = !!(flags & O_CLOEXEC);
            fd = i;
            break;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    if (!fd.has_value()) {
        *__errno() = ENFILE;
        return -1;
    }

    return *fd;
}

extern "C"
int __isatty(int fd) {
    IO_STATUS_BLOCK iosb;

    auto h = __get_osfhandle(fd);
    if (h == -1)
        return 0; // FIXME - is this right?

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_ISATTY, nullptr, 0,
                                        nullptr, 0);
    if (!success(status))
        return 0;

    return 1;
}

extern "C"
int __write(int fd, const void* buffer, unsigned int count) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)__get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    while (true) {
        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtWriteFile(h, 0, nullptr, nullptr, &iosb, buffer,
                                count, nullptr, nullptr);

        if (status == nt_status::USER_APC) {
            if (mn_handled_nonrestartable_signal_count() != sig_count) {
                *__errno() = EINTR;
                return -1;
            }

            continue;
        }

        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            return -1;
        }

        break;
    }

    return (int)iosb.Information;
}

extern "C"
int __read(int fd, void* buffer, size_t count) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)__get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    while (true) {
        auto sig_count = mn_handled_nonrestartable_signal_count();

        auto status = NtReadFile(h, 0, nullptr, nullptr, &iosb, buffer,
                                (uint32_t)count, nullptr, nullptr);

        if (status == nt_status::USER_APC) {
            if (mn_handled_nonrestartable_signal_count() != sig_count) {
                *__errno() = EINTR;
                return -1;
            }

            continue;
        }

        if (status == nt_status::END_OF_FILE || status == nt_status::PIPE_DISCONNECTED)
            return 0;
        else if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            return -1;
        }

        break;
    }

    return (int)iosb.Information;
}

extern "C"
int __access(const char* path, int /*mode*/) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    nt_status status;

    if (!path || path[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    // FIXME - translate mode into access mask
    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    return 0;
}

extern "C"
int putchar(int c) {
    return _fputc(c, reinterpret_cast<FILE*>(_stdout));
}

extern "C"
int __setmode(int /*fd*/, int /*mode*/) {
    // FIXME - actually do this?

    return 0;
}

extern "C"
int __close(int fd) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    HANDLE h = 0;

    RtlEnterCriticalSection(&fds_lock);
    swap(fds[(size_t)fd].h, h);
    RtlLeaveCriticalSection(&fds_lock);

    if (h == 0) {
        *__errno() = EBADF;
        return -1;
    }

    NtClose(h);

    // FIXME - should be HANDLE to /dev/null maybe?
    if (fd == 0)
        peb.ProcessParameters->StandardInput = 0;
    else if (fd == 1)
        peb.ProcessParameters->StandardOutput = 0;
    else if (fd == 2)
        peb.ProcessParameters->StandardError = 0;

    return 0;
}

extern "C"
int __mkdir(const char* path) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file;
    IO_STATUS_BLOCK iosb;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    // FIXME - mode?

    auto pathsv = string_view(path);

    while (pathsv.size() > 1 && (pathsv.back() == '/' || pathsv.back() == '\\')) {
        pathsv = string_view(pathsv.data(), pathsv.size() - 1);
    }

    if (pathsv.empty()) {
        *__errno() = EINVAL;
        return -1;
    }

    auto u16len = utf8_to_utf16_len(pathsv);
    auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));

    if (!u16) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16, u16len), pathsv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)u16;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&file, 0, &oa, &iosb, nullptr, FILE_ATTRIBUTE_DIRECTORY,
                          0, FILE_CREATE, FILE_DIRECTORY_FILE, nullptr, 0);

    free(u16);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        *__errno() = EEXIST;
        return -1;
    } else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        *__errno() = ENOENT;
        return -1;
    } else if (!success(status)) {
        *__errno() = EINVAL;
        return -1;
    }

    NtClose(file);

    return 0;
}

extern "C"
void clearerr(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    f.error = false;
    f.eof = false;
}

extern "C"
int rename(const char* oldpath, const char* newpath) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle file;

    if (!oldpath || !newpath || oldpath[0] == 0 || newpath[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto oldsv = string_view(oldpath);
    auto u16len = utf8_to_utf16_len(oldsv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), oldsv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                          0, nullptr, 0);

    free(buf);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    auto newsv = string_view(newpath);
    u16len = utf8_to_utf16_len(newsv);

    size_t frilen = offsetof(FILE_RENAME_INFORMATION, FileName) + (u16len * sizeof(char16_t));

    auto fri = (FILE_RENAME_INFORMATION*)malloc(frilen);
    if (!fri) {
        *__errno() = ENOMEM;
        return -1;
    }

    fri->ReplaceIfExists = true; // FIXME? --interactive option
    fri->RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;
    fri->FileNameLength = (uint32_t)(u16len * sizeof(char16_t));
    utf8_to_utf16(span(fri->FileName, fri->FileNameLength / sizeof(char16_t)),
                  newsv);

    status = NtSetInformationFile(file.get(), &iosb, fri, (uint32_t)frilen,
                                  FILE_INFORMATION_CLASS::RenameInformation);

    free(fri);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int __unlink(const char* path) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h;
    unique_handle file;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!path || path[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT,
                          nullptr, 0);

    free(buf);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file.get(), &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int __dup2(int oldfd, int newfd) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (oldfd < 0 || (size_t)oldfd >= fds.size() || newfd < 0 || (size_t)newfd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    if (newfd == 0) {
        auto& f = *reinterpret_cast<_FILE*>(&_stdin);

        _fflush((FILE*)&_stdin);

        f.buffer.consumed = 0;
        f.buffer.read_bytes = 0;
        f.ungetc_char = nullopt;
        f.ungetwc_char = nullopt;
    } else if (newfd == 1)
        _fflush((FILE*)&_stdout);
    else if (newfd == 2)
        _fflush((FILE*)&_stderr);

    RtlEnterCriticalSection(&fds_lock);

    auto oldh = fds[(size_t)oldfd].h;

    if (!oldh) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = EBADF;
        return -1;
    }

    if (oldfd == newfd) {
        RtlLeaveCriticalSection(&fds_lock);
        return newfd;
    }

    HANDLE newh;

    auto status = NtDuplicateObject(NtCurrentProcess(), oldh, NtCurrentProcess(),
                                    &newh, 0, 0,
                                    DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status)) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    if (fds[(size_t)newfd].h != 0)
        NtClose(fds[(size_t)newfd].h);

    fds[(size_t)newfd].h = newh;
    fds[(size_t)newfd].cloexec = false;

    RtlLeaveCriticalSection(&fds_lock);

    if (newfd == 0) {
        peb.ProcessParameters->StandardInput = newh;
        reinterpret_cast<_FILE*>(&_stdin)->h = newh;
    } else if (newfd == 1) {
        peb.ProcessParameters->StandardOutput = newh;
        reinterpret_cast<_FILE*>(&_stdout)->h = newh;
    } else if (newfd == 2) {
        peb.ProcessParameters->StandardError = newh;
        reinterpret_cast<_FILE*>(&_stderr)->h = newh;
    }

    return newfd;
}

static int dup2_from_handle(HANDLE h, int newfd, bool cloexec) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (newfd < 0 || (size_t)newfd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    if (newfd == 0) {
        auto& f = *reinterpret_cast<_FILE*>(&_stdin);

        _fflush((FILE*)&_stdin);

        f.buffer.consumed = 0;
        f.buffer.read_bytes = 0;
        f.ungetc_char = nullopt;
        f.ungetwc_char = nullopt;
    } else if (newfd == 1)
        _fflush((FILE*)&_stdout);
    else if (newfd == 2)
        _fflush((FILE*)&_stderr);

    RtlEnterCriticalSection(&fds_lock);

    if (fds[(size_t)newfd].h != 0)
        NtClose(fds[(size_t)newfd].h);

    fds[(size_t)newfd].h = h;
    fds[(size_t)newfd].cloexec = cloexec;

    RtlLeaveCriticalSection(&fds_lock);

    if (newfd == 0) {
        peb.ProcessParameters->StandardInput = h;
        reinterpret_cast<_FILE*>(&_stdin)->h = h;
    } else if (newfd == 1) {
        peb.ProcessParameters->StandardOutput = h;
        reinterpret_cast<_FILE*>(&_stdout)->h = h;
    } else if (newfd == 2) {
        peb.ProcessParameters->StandardError = h;
        reinterpret_cast<_FILE*>(&_stderr)->h = h;
    }

    return newfd;
}

extern "C"
int mn_dupfd(int oldfd, int minfd, bool cloexec) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (oldfd < 0 || (size_t)oldfd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    if (minfd < 0 || (size_t)minfd >= fds.size()) {
        *__errno() = EINVAL;
        return -1;
    }

    RtlEnterCriticalSection(&fds_lock);

    auto oldh = fds[(size_t)oldfd].h;

    if (!oldh) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = EBADF;
        return -1;
    }

    HANDLE newh;

    auto status = NtDuplicateObject(NtCurrentProcess(), oldh, NtCurrentProcess(),
                                    &newh, 0, 0,
                                    DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status)) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    for (size_t i = minfd; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = newh;
            fds[i].cloexec = cloexec;
            RtlLeaveCriticalSection(&fds_lock);

            if (i == 0) {
                peb.ProcessParameters->StandardInput = newh;
                reinterpret_cast<_FILE*>(&_stdin)->h = newh;
            } else if (i == 1) {
                peb.ProcessParameters->StandardOutput = newh;
                reinterpret_cast<_FILE*>(&_stdout)->h = newh;
            } else if (i == 2) {
                peb.ProcessParameters->StandardError = newh;
                reinterpret_cast<_FILE*>(&_stderr)->h = newh;
            }

            return (int)i;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    NtClose(newh);

    *__errno() = EMFILE;

    return -1;
}

extern "C"
int __dup(int oldfd) {
    return mn_dupfd(oldfd, 0, false);
}

extern "C"
int64_t _lseeki64(int fd, int64_t offset, int whence) {
    int64_t start;
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;
    nt_status status;

    auto h = (HANDLE)__get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    switch (whence) {
        case SEEK_SET:
            start = 0;
            break;

        case SEEK_CUR:
            status = NtQueryInformationFile(h, &iosb, &fpi, sizeof(fpi),
                                            FILE_INFORMATION_CLASS::PositionInformation);
            if (!success(status)) {
                *__errno() = nt_status_to_errno(status);
                return -1;
            }

            start = fpi.CurrentByteOffset.QuadPart;
            break;

        case SEEK_END: {
            FILE_STANDARD_INFORMATION fsi;

            status = NtQueryInformationFile(h, &iosb, &fsi, sizeof(fsi),
                                            FILE_INFORMATION_CLASS::StandardInformation);
            if (!success(status)) {
                *__errno() = nt_status_to_errno(status);
                return -1;
            }

            start = fsi.EndOfFile.QuadPart;
            break;
        }

        default:
            *__errno() = EINVAL;
            return -1;
    }

    fpi.CurrentByteOffset.QuadPart = start + offset;

    status = NtSetInformationFile(h, &iosb, &fpi, sizeof(fpi),
                                  FILE_INFORMATION_CLASS::PositionInformation);
    if (status == nt_status::INVALID_DEVICE_REQUEST) {
        *__errno() = ESPIPE;
        return -1;
    } else if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return start + offset;
}

extern "C"
long __lseek(int fd, long offset, int whence) {
    return (long)_lseeki64(fd, (int64_t)offset, whence);
}

extern "C"
void rewind(FILE* file) {
    __lock_file(file);

    auto& f = *reinterpret_cast<_FILE*>(file);

    __fseeki64_nolock(file, 0, SEEK_SET);

    f.error = false;

    __unlock_file(file);
}

extern "C"
int __umask(int pmode) {
    // FIXME - actually use this when creating files or directories

    swap(pmode, umask_value);

    return pmode;
}

extern "C"
int __rmdir(const char* path) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h;
    unique_handle file;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!path || path[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);

    free(buf);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file.get(), &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int __creat(const char* path, int mode) {
    // FIXME - mode should actually be mode_t rather than int

    return __open(path, _O_CREAT | _O_WRONLY | _O_TRUNC, mode);
}

extern "C"
FILE* freopen(const char* path, const char* mode, FILE* file) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!file || !mode) {
        *__errno() = EINVAL;
        return nullptr;
    }

    auto& f = *reinterpret_cast<_FILE*>(file);
    HANDLE h;
    int new_fd;
    bool readable, writable;

    if (!path)
        return file; // FIXME - should also be changing mode

    // FIXME - cloexec if "e" flag set
    if (!do_open(path, mode, h, new_fd, false, false, readable, writable))
        return nullptr;

    __lock_file(file);

    _fflush_nolock(file);

    RtlEnterCriticalSection(&fds_lock);

    fds[(size_t)f.fd].h = h;  // keeping fd the same, and closing new_fd

    if (new_fd != f.fd)
        fds[(size_t)new_fd].h = 0;

    RtlLeaveCriticalSection(&fds_lock);

    NtClose(f.h);

    f.h = h;
    f.ungetc_char.reset();
    f.ungetwc_char.reset();
    f.error = false;
    f.eof = false;
    f.getwc_surrogate = 0;
    f.putwc_surrogate = 0;
    f.buffer.filled = 0;
    f.buffer.consumed = 0;
    f.buffer.read_bytes = 0;
    f.readable = readable;
    f.writable = writable;

    if (f.fd == 0) {
        peb.ProcessParameters->StandardInput = h;
        reinterpret_cast<_FILE*>(&_stdin)->h = h;
    } else if (f.fd == 1) {
        peb.ProcessParameters->StandardOutput = h;
        reinterpret_cast<_FILE*>(&_stdout)->h = h;
    } else if (f.fd == 2) {
        peb.ProcessParameters->StandardError = h;
        reinterpret_cast<_FILE*>(&_stderr)->h = h;
    }

    __unlock_file(file);

    return file;
}

extern "C"
FILE* tmpfile() {
    nt_status status;
    FILE_DISPOSITION_INFORMATION fdi;
    IO_STATUS_BLOCK iosb;
    static atomic<unsigned int> counter = 1;
    char path[PATH_MAX];
    span sp(path, sizeof(path));
    HANDLE h;
    int fd;
    bool readable, writable;

    // create file

    sp = format(sp, "/tmp/tmpfile_{}_{}", _getpid(), counter.fetch_add(1));
    sp[0] = 0;

    if (!do_open(path, "w+", h, fd, true, false, readable, writable))
        return nullptr;

    // unlink file

    fdi.DeleteFile = true;

    status = NtSetInformationFile(h, &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);

        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)fd].h = 0;
        RtlLeaveCriticalSection(&fds_lock);

        NtClose(h);

        return nullptr;
    }

    // create FILE

    auto f = new _FILE(h, fd, readable, writable);
    if (!f) {
        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)fd].h = 0;
        RtlLeaveCriticalSection(&fds_lock);

        NtClose(h);

        *__errno() = ENOMEM;
        return nullptr;
    }

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
int __pipe(int* pfds, unsigned int /*psize*/, int /*textmode*/) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE server_handle, client_handle;
    nt_status status;
    char16_t pipename[50];
    optional<int> server_fd, client_fd;

    static atomic<unsigned int> counter = 1;

    // FIXME - psize?
    // FIXME - textmode?

    // FIXME - SIGPIPE and EPIPE, etc.

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    {
        char s[50];
        span sp(s, sizeof(s));

        sp = format(sp, "/dev/pipe_{}_{}", _getpid(), counter.fetch_add(1));

        auto name = string_view(s, (size_t)(sp.data() - s));

        utf8_to_utf16(pipename, name);

        us.Length = us.MaximumLength = (uint16_t)(utf8_to_utf16_len(name) * sizeof(char16_t));
        us.Buffer = pipename;
    }

    status = NtCreateNamedPipeFile(&server_handle, FILE_WRITE_DATA, &oa, &iosb,
                                   0, FILE_CREATE, 0, 0, 0, 0, 1, 0, 0,
                                   nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    status = NtOpenFile(&client_handle, FILE_READ_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        NtClose(server_handle);
        return -1;
    }

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h != 0)
            continue;

        if (!client_fd.has_value()) {
            fds[i].h = client_handle;
            fds[i].cloexec = false;
            client_fd = i;
        } else {
            fds[i].h = server_handle;
            fds[i].cloexec = false;
            server_fd = i;
            break;
        }
    }

    if (client_fd.has_value() && !server_fd.has_value())
        fds[(size_t)*client_fd].h = 0;

    RtlLeaveCriticalSection(&fds_lock);

    if (!client_fd.has_value() || !server_fd.has_value()) {
        NtClose(client_handle);
        NtClose(server_handle);
        *__errno() = EMFILE;
        return -1;
    }

    pfds[0] = *client_fd;
    pfds[1] = *server_fd;

    return 0;
}

extern "C"
FILE* __popen(const char* command, const char* type) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    string_view cmd(command);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    bool write_mode;
    int pipefds[2];

    static const u16string_view DASH_C = u" -c ";

    enum class mode_type {
        read,
        write,
    };

    if (!type || !command || command[0] == 0) {
        *__errno() = EINVAL;
        return nullptr;
    }

    switch (type[0]) {
        case 'r':
            write_mode = false;
            break;

        case 'w':
            write_mode = true;
            break;

        default:
            *__errno() = EINVAL;
            return nullptr;
    }

    type++;

    while (*type != 0) {
        if (*type != 't' && *type != 'b') {
            *__errno() = EINVAL;
            return nullptr;
        }

        type++;
    }

    static const char16_t u16shell[] = u"/bin/sh";
    static const size_t u16shelllen = (sizeof(u16shell) / sizeof(char16_t)) - 1;

    auto u16cmdlen = utf8_to_utf16_len(cmd);

    auto cmdlinelen = u16shelllen + DASH_C.size() + u16cmdlen + 1;
    auto cmdline = (char16_t*)malloc(cmdlinelen * sizeof(char16_t));
    if (!cmdline) {
        *__errno() = ENOMEM;
        return nullptr;
    }

    memcpy(cmdline, u16shell, u16shelllen * sizeof(char16_t));
    memcpy(cmdline + u16shelllen, DASH_C.data(), DASH_C.size() * sizeof(char16_t));
    utf8_to_utf16(span(cmdline + u16shelllen + DASH_C.size(), u16cmdlen), cmd);
    cmdline[u16shelllen + DASH_C.size() + u16cmdlen] = 0;

    // create pipe

    if (__pipe(pipefds, 0, 0)) {
        free(cmdline);
        return nullptr;
    }

    // launch process

    memset(&pi, 0, sizeof(pi));

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    if (write_mode) {
        si.hStdInput = (HANDLE)__get_osfhandle(pipefds[0]);
        si.hStdOutput = peb.ProcessParameters->StandardOutput;
    } else {
        si.hStdInput = peb.ProcessParameters->StandardInput;
        si.hStdOutput = (HANDLE)__get_osfhandle(pipefds[1]);
    }

    si.hStdError = peb.ProcessParameters->StandardError;

    if (!CreateProcessW(u16shell, cmdline, nullptr, nullptr, false, 0, nullptr,
                        nullptr, &si, &pi)) {
        free(cmdline);
        __close(pipefds[0]);
        __close(pipefds[1]);

        *__errno() = EINVAL;
        return nullptr;
    }

    if (write_mode)
        __close(pipefds[0]);
    else
        __close(pipefds[1]);

    free(cmdline);

    NtClose(pi.hThread);

    // create FILE

    auto fd = write_mode ? pipefds[1] : pipefds[0];

    auto f = new _FILE((HANDLE)__get_osfhandle(fd), fd, !write_mode, write_mode);
    if (!f) {
        __close(fd);
        NtClose(pi.hProcess);

        *__errno() = ENOMEM;
        return nullptr;
    }

    f->proc = pi.hProcess;

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
int __pclose(FILE* file) {
    nt_status status;
    PROCESS_BASIC_INFORMATION pbi;
    auto& f = *reinterpret_cast<_FILE*>(file);
    bool fail = false;

    if (f.proc == 0) {
        *__errno() = EBADF;
        return -1;
    }

    _fflush(file);

    // close stream

    NtClose(f.h);
    f.h = 0;

    // wait for process

    while (true) {
        status = NtWaitForSingleObject(f.proc, true, nullptr);
        if (status == nt_status::USER_APC || status == nt_status::ALERTED)
            continue;

        if (success(status))
            break;

        *__errno() = nt_status_to_errno(status);
        fail = true;
        break;
    }

    if (!fail) {
        // get exit status

        status = NtQueryInformationProcess(f.proc, PROCESS_INFORMATION_CLASS::BasicInformation,
                                        &pbi, sizeof(pbi), nullptr);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            fail = true;
        }
    }

    NtClose(f.proc);

    RtlEnterCriticalSection(&files_lock);
    remove_entry_list(f.le);
    RtlLeaveCriticalSection(&files_lock);

    delete &f;

    return fail ? -1 : (int)pbi.ExitStatus;
}

extern "C"
nt_status mn_prepare_exec_block(HANDLE proc, exec_info** exec_block,
                                size_t* num_exec_block_entries) {
    unsigned int num_entries = 1;
    nt_status status;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = STDERR_FILENO + 1; i < fds.size(); i++) {
        if (fds[i].h == 0)
            continue;

        if (fds[i].cloexec)
            continue;

        num_entries++;
    }

    auto entries = (exec_info*)malloc(sizeof(exec_info) * num_entries);
    if (!entries) {
        print_stderr("malloc failed\n");
        RtlLeaveCriticalSection(&fds_lock);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto e = entries;

    {
        e->type = exec_info_type::umask;
        e->umask.value = umask_value;
        e++;
    }

    for (size_t i = STDERR_FILENO + 1; i < fds.size(); i++) {
        if (fds[i].h == 0)
            continue;

        if (fds[i].cloexec)
            continue;

        status = NtDuplicateObject(NtCurrentProcess(), fds[i].h, proc,
                                   &e->inherited_fd.h, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
        if (!success(status)) {
            print_stderr("NtDuplicateObject returned {}\n", status);
            free(entries);
            RtlLeaveCriticalSection(&fds_lock);
            return status;
        }

        e->type = exec_info_type::inherited_fd;
        e->inherited_fd.fd = (int)i;

        e++;
    }

    RtlLeaveCriticalSection(&fds_lock);

    *exec_block = entries;
    *num_exec_block_entries = num_entries;

    return nt_status::SUCCESS;
}

static void handle_exec_info(exec_info* info) {
    switch (info->type) {
        using enum exec_info_type;

        case inherited_fd: {
            if (info->inherited_fd.fd < 0 || (size_t)info->inherited_fd.fd >= fds.size()) {
                print_stderr("handle_exec_info: fd {} out of range\n", info->inherited_fd.fd);
                return;
            }

            RtlEnterCriticalSection(&fds_lock);

            auto& f = fds[(size_t)info->inherited_fd.fd];

            if (f.h)
                NtClose(f.h);

            f.h = info->inherited_fd.h;
            f.cloexec = false;

            RtlLeaveCriticalSection(&fds_lock);

            break;
        }

        case open: {
            auto fd = open2(info->open.path, info->open.oflag, info->open.mode);
            if (fd == -1) {
                print_stderr("_open2(\"{}\", {}, {}) failed (errno {})\n",
                             info->open.path, info->open.oflag, info->open.mode,
                             errno_to_string_view(*__errno()));
                _exit(127);
            }

            // FIXME - if stdin, stdout, or stderr, also update PEB

            if (fd != info->open.fd) {
                auto ret = __dup2(fd, info->open.fd);
                if (ret == -1) {
                    print_stderr("_dup2 failed (errno {})\n",
                                 errno_to_string_view(*__errno()));
                    _exit(127);
                }

                __close(fd);
            }

            break;
        }

        case close: {
            // FIXME - if stdin, stdout, or stderr, also update PEB

            auto ret = __close(info->close.fd);
            if (ret == -1 && *__errno() != EBADF) {
                print_stderr("_close({}) failed (errno {})\n",
                             info->close.fd, errno_to_string_view(*__errno()));
                _exit(127);
            }

            break;
        }

        case dup2: {
            // FIXME - if stdin, stdout, or stderr, also update PEB

            auto ret = __dup2(info->dup2.fd, info->dup2.newfd);
            if (ret == -1) {
                print_stderr("_dup2({}, {}) failed (errno {})\n",
                             info->dup2.fd, info->dup2.newfd,
                             errno_to_string_view(*__errno()));
                _exit(127);
            }

            break;
        }

        case dup2_cloexec: {
            auto ret = dup2_from_handle(info->dup2_cloexec.h, info->dup2_cloexec.newfd,
                                        false);
            if (ret == -1) {
                print_stderr("dup2_from_handle failed (errno {})\n",
                             errno_to_string_view(*__errno()));
                _exit(127);
            }

            break;
        }

        case chdir: {
            auto ret = __chdir(info->chdir.path);
            if (ret == -1) {
                print_stderr("_chdir(\"{}\") failed (errno {})\n",
                             info->chdir.path, errno_to_string_view(*__errno()));
                _exit(127);
            }

            break;
        }

        case fchdir: {
            auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
            HANDLE h2;
            nt_status status;

            auto h = (HANDLE)__get_osfhandle(info->fchdir.fd);
            if (h == (HANDLE)-1) {
                print_stderr("_get_osfhandle({}) failed (errno {})\n",
                             info->fchdir.fd, errno_to_string_view(*__errno()));
                _exit(127);
            }

            status = NtDuplicateObject(NtCurrentProcess(), h, NtCurrentProcess(),
                                       &h2, 0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
            if (!success(status)) {
                print_stderr("NtDuplicateObject returned {}\n", status);
                _exit(127);
            }

            {
                mn_peb_lock lock;
                lock_guard lg(lock);

                if (!get_dos_path(h, peb.ProcessParameters->CurrentDirectory.DosPath)) {
                    NtClose(h2);
                    print_stderr("get_dos_path failed\n");
                    _exit(127);
                }

                if (peb.ProcessParameters->CurrentDirectory.Handle != 0)
                    NtClose(peb.ProcessParameters->CurrentDirectory.Handle);

                peb.ProcessParameters->CurrentDirectory.Handle = h2;
            }

            break;
        }

        case alarm: {
            uint64_t left;

            mn_alarm(info->alarm.left, &left);

            break;
        }

        case umask:
            umask_value = info->umask.value;
            break;
    }

}

extern "C"
bool mn_get_cloexec(int fd, bool* cloexec) {
    if (fd < 0 || (size_t)fd >= fds.size())
        return false;

    RtlEnterCriticalSection(&fds_lock);

    if (fds[(size_t)fd].h == 0) {
        RtlLeaveCriticalSection(&fds_lock);
        return false;
    }

    *cloexec = fds[(size_t)fd].cloexec;
    RtlLeaveCriticalSection(&fds_lock);

    return true;
}

extern "C"
bool mn_set_cloexec(int fd, bool cloexec) {
    if (fd < 0 || (size_t)fd >= fds.size())
        return false;

    RtlEnterCriticalSection(&fds_lock);

    if (fds[(size_t)fd].h == 0) {
        RtlLeaveCriticalSection(&fds_lock);
        return false;
    }

    fds[(size_t)fd].cloexec = cloexec;
    RtlLeaveCriticalSection(&fds_lock);

    return true;
}

static char mktemp_char(unsigned int i) {
    if (i < 10)
        return (char)('0' + i);
    else
        return (char)('a' + i - 10);
}

extern "C"
char* __mktemp(char* templ) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto len = strlen(templ);

    if (len < 6)
        return nullptr;

    auto sp = span(templ + len - 6, 6);

    for (auto c : sp) {
        if (c != 'X')
            return nullptr;
    }

    while (true) {
        UNICODE_STRING us;
        OBJECT_ATTRIBUTES oa;
        IO_STATUS_BLOCK iosb;
        HANDLE h;

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

        auto sv = string_view(templ, len);
        auto u16len = utf8_to_utf16_len(sv);

        auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16pathname)
            return nullptr;

        utf8_to_utf16(span(u16pathname, u16len), sv);

        us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
        us.Buffer = u16pathname;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);
        oa.ObjectName = &us;
        oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

        auto status = NtCreateFile(&h, MAXIMUM_ALLOWED, &oa, &iosb, nullptr,
                                   FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                                   0, nullptr, 0);

        free(u16pathname);

        if (success(status)) { // already exists, try again
            NtClose(h);
            continue;
        }

        if (status != nt_status::OBJECT_NAME_NOT_FOUND)
            return nullptr;

        break;
    }

    return templ;
}

extern "C"
wint_t fgetwc(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    int i;
    uint8_t c;

    __lock_file(file);

    if (f.ungetwc_char.has_value()) {
        wint_t ret = f.ungetwc_char.value();
        f.ungetwc_char = nullopt;

        __unlock_file(file);
        return ret;
    }

    if (f.getwc_surrogate != 0) {
        wint_t ret = f.getwc_surrogate;
        f.getwc_surrogate = 0;

        __unlock_file(file);
        return ret;
    }

    i = __fgetc_nolock(file);
    if (i == EOF) {
        __unlock_file(file);
        return WEOF;
    }

    c = (uint8_t)i;
    if (c < 0x80) {
        __unlock_file(file);
        return c;
    }

    if ((c & 0xe0) == 0xc0) { // U+0080 to U+07ff
        i = __fgetc_nolock(file);
        if (i == EOF) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        auto c2 = (uint8_t)i;
        if ((c2 & 0xc0) != 0x80) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        __unlock_file(file);

        return ((c & 0x1f) << 6) | (c2 & 0x3f);
    } else if ((c & 0xf0) == 0xe0) { // U+0800 to U+ffff
        i = __fgetc_nolock(file);
        if (i == EOF) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        auto c2 = (uint8_t)i;
        if ((c2 & 0xc0) != 0x80) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        i = __fgetc_nolock(file);
        if (i == EOF) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        auto c3 = (uint8_t)i;
        if ((c3 & 0xc0) != 0x80) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        __unlock_file(file);

        return ((c & 0x0f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
    } else if ((c & 0xf8) == 0xf0) { // U+10000 to U+10ffff
        i = __fgetc_nolock(file);
        if (i == EOF) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        auto c2 = (uint8_t)i;
        if ((c2 & 0xc0) != 0x80) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        i = __fgetc_nolock(file);
        if (i == EOF) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        auto c3 = (uint8_t)i;
        if ((c3 & 0xc0) != 0x80) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        i = __fgetc_nolock(file);
        if (i == EOF) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        auto c4 = (uint8_t)i;
        if ((c4 & 0xc0) != 0x80) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        char32_t cp = ((c & 0x07) << 18) | ((c2 & 0x3f) << 12) | ((c3 & 0x3f) << 6) | (c4 & 0x3f);

        if (cp >= 0x110000) {
            *__errno() = EILSEQ;
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        f.getwc_surrogate = 0xdc00 | ((cp - 0x10000) & 0x3ff);

        __unlock_file(file);

        return (wint_t)(0xd800 | ((cp - 0x10000) >> 10));
    } else {
        *__errno() = EILSEQ;
        f.error = true;
        __unlock_file(file);
        return WEOF;
    }
}

extern "C"
wint_t ungetwc(wint_t wc, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    if (wc == WEOF)
        return WEOF;

    __lock_file(file);

    if (f.ungetwc_char.has_value()) {
        __unlock_file(file);
        return WEOF;
    }

    f.ungetwc_char = wc;
    f.eof = false;

    __unlock_file(file);

    return wc;
}

extern "C"
wint_t fputwc(wchar_t wc, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    __lock_file(file);

    if (f.putwc_surrogate != 0) {
        if (wc < 0xdc00 || wc > 0xdfff) {
            f.error = true;
            __unlock_file(file);
            *_errno() = EILSEQ;
            return WEOF;
        }

        char32_t cp = 0x10000 + (((f.putwc_surrogate & 0x3ff) << 10) | (wc & 0x3ff));
        char s[4];

        s[0] = (char)(0xf0 | (cp >> 18));
        s[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
        s[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
        s[3] = (char)(0x80 | (cp & 0x3f));

        auto ret = __fwrite_nolock(s, 1, 4, file);
        if (ret != 4) {
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }

        f.putwc_surrogate = 0;

        __unlock_file(file);
        return wc;
    } else if (wc >= 0xdc00 && wc <= 0xdfff) {
        f.error = true;
        __unlock_file(file);
        *_errno() = EILSEQ;
        return WEOF;
    }

    if (wc < 0x80) {
        if (__fputc_nolock((uint8_t)wc, file) == EOF) {
            __unlock_file(file);
            return WEOF;
        }
    } else if (wc < 0x800) {
        char s[2];

        s[0] = (char)(0xc0 | (wc >> 6));
        s[1] = (char)(0x80 | (wc & 0x3f));

        auto ret = __fwrite_nolock(s, 1, 2, file);
        if (ret != 2) {
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }
    } else if (wc >= 0xd800 && wc <= 0xdbff)
        f.putwc_surrogate = wc;
    else {
        char s[3];

        s[0] = (char)(0xe0 | (wc >> 12));
        s[1] = (char)(0x80 | ((wc >> 6) & 0x3f));
        s[2] = (char)(0x80 | (wc & 0x3f));

        auto ret = __fwrite_nolock(s, 1, 3, file);
        if (ret != 3) {
            f.error = true;
            __unlock_file(file);
            return WEOF;
        }
    }

    __unlock_file(file);

    return wc;
}

extern "C"
FILE* mn_open_memstream(char** bufp, size_t* sizep) {
    auto f = new _FILE(bufp, sizep);
    if (!f) {
        *__errno() = ENOMEM;
        return nullptr;
    }

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
size_t mn_fpending(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    size_t ret;

    __lock_file(file);

    ret = f.buffer.reading ? 0 : f.buffer.filled;

    __unlock_file(file);

    return ret;
}

extern "C"
int mn_freading(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    int ret;

    __lock_file(file);

    ret = !f.writable || f.buffer.reading ? 1 : 0;

    __unlock_file(file);

    return ret;
}

extern "C"
int mn_fwriting(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    int ret;

    __lock_file(file);

    ret = !f.readable || !f.buffer.reading ? 1 : 0;

    __unlock_file(file);

    return ret;
}

extern "C"
void mn_fpurge(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    __lock_file(file);

    f.buffer.consumed = 0;
    f.buffer.filled = 0;
    f.buffer.read_bytes = 0;
    f.ungetc_char = nullopt;
    f.ungetwc_char = nullopt;
    f.getwc_surrogate = 0;
    f.putwc_surrogate = 0;

    __unlock_file(file);
}

extern "C"
void mn_fseterr(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    f.error = true;
}

extern "C"
int fgetpos(FILE* file, fpos_t* pos) {
    auto p = __ftelli64(file);

    if (p == -1)
        return -1;

    *pos = p;

    return 0;
}

extern "C"
int fsetpos(FILE* file, const fpos_t* pos) {
    return __fseeki64(file, *pos, SEEK_SET);
}

extern "C"
int mn_get_umask() {
    return umask_value;
}
