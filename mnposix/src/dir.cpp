// SPDX-License-Identifier: LGPL-2.1-or-later
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <string_view>
#include <span>
#include <atomic>

import nt;
import unicode;
import ucrtbase;

#undef errno
import errno;

using namespace std;

struct _DIR {
    HANDLE h;
    bool first;
    int fd = -1;
};

static dirent de;

extern "C"
DIR* opendir(const char* name) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    UNICODE_STRING us;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    nt_status status;
    HANDLE h;

    if (!name || name[0] == 0) {
        *_errno() = EINVAL;
        return nullptr;
    }

    auto sv = string_view(name);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return nullptr;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb.ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_TRAVERSE | SYNCHRONIZE,
                          &oa, &iosb, nullptr, FILE_ATTRIBUTE_NORMAL,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    free(u16pathname);

    auto d = new _DIR;
    if (!d) {
        NtClose(h);
        *_errno() = ENOMEM;
        return nullptr;
    }

    d->h = h;
    d->first = true;

    return (DIR*)d;
}

extern "C"
dirent* readdir(DIR* dir) {
    auto& d = *reinterpret_cast<_DIR*>(dir);
    IO_STATUS_BLOCK iosb;
    char buf[4096];
    nt_status status;

    // FIXME - get multiple entries at once and cache them?

    status = NtQueryDirectoryFile(d.h, 0, nullptr, nullptr, &iosb, buf, sizeof(buf),
                                  FILE_INFORMATION_CLASS::IdFullDirectoryInformation,
                                  true, nullptr, d.first);
    if (status == nt_status::NO_MORE_FILES)
        return nullptr;
    else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    auto& fdi = *(FILE_ID_FULL_DIR_INFORMATION*)buf;
    auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));
    auto u16len = utf16_to_utf8_len(u16sv);

    de.d_ino = fdi.FileId.QuadPart;

    if (fdi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        de.d_type = DT_LNK;
    else if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        de.d_type = DT_DIR;
    else
        de.d_type = DT_REG;

    utf16_to_utf8(span(de.d_name, sizeof(de.d_name) - 1), u16sv);

    if (u16len < sizeof(de.d_name))
        de.d_name[u16len] = 0;
    else
        de.d_name[sizeof(de.d_name) - 1] = 0;

    d.first = false;

    return &de;
}

extern "C"
int closedir(DIR* dir) {
    auto& d = *reinterpret_cast<_DIR*>(dir);

    // FIXME - return errors from NtClose?

    if (d.fd == -1)
        NtClose(d.h);
    else
        close(d.fd);

    delete &d;

    return 0;
}

extern "C"
void rewinddir(DIR* dir) {
    auto& d = *reinterpret_cast<_DIR*>(dir);

    d.first = true;
}

extern "C"
int dirfd(DIR* dir) {
    auto& d = *reinterpret_cast<_DIR*>(dir);

    if (d.fd == -1) {
        auto fd = _open_osfhandle(d.h, 0);
        if (fd == -1)
            return -1;

        d.fd = fd;
    }

    return d.fd;
}

extern "C"
DIR* fdopendir(int fd) {
    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return nullptr;

    auto d = new _DIR;
    if (!d) {
        *_errno() = ENOMEM;
        return nullptr;
    }

    d->h = h;
    d->first = true;
    d->fd = fd;

    return (DIR*)d;
}

extern "C"
int scandir(const char* dir, dirent*** namelist, int (*sel)(const dirent*),
            int (*compar)(const dirent**, const dirent**)) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    UNICODE_STRING us;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    nt_status status;
    HANDLE h;
    char buf[4096];
    size_t num_entries = 0;

    if (!dir || dir[0] == 0) {
        *_errno() = ENOENT;
        return -1;
    }

    auto sv = string_view(dir);
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

    status = NtCreateFile(&h, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_TRAVERSE | SYNCHRONIZE,
                          &oa, &iosb, nullptr, FILE_ATTRIBUTE_NORMAL,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    auto de = (dirent*)malloc(sizeof(dirent));
    if (!de) {
        *_errno() = ENOMEM;
        NtClose(h);
        return -1;
    }

    *namelist = nullptr;

    while (true) {
        // FIXME - get multiple entries at once and cache them?

        status = NtQueryDirectoryFile(h, 0, nullptr, nullptr, &iosb, buf,
                                      sizeof(buf), FILE_INFORMATION_CLASS::IdFullDirectoryInformation,
                                      false, nullptr, false);
        if (status == nt_status::NO_MORE_FILES)
            break;
        else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            NtClose(h);

            for (size_t i = 0; i < num_entries; i++) {
                free((*namelist)[i]);
            }

            free(*namelist);
            free(de);

            return -1;
        }

        auto buf2 = buf;

        while (true) {
            auto& fdi = *(FILE_ID_FULL_DIR_INFORMATION*)buf2;
            auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));
            auto u16len = utf16_to_utf8_len(u16sv);

            de->d_ino = fdi.FileId.QuadPart;

            if (fdi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                de->d_type = DT_LNK;
            else if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                de->d_type = DT_DIR;
            else
                de->d_type = DT_REG;

            utf16_to_utf8(span(de->d_name, sizeof(de->d_name) - 1), u16sv);

            if (u16len < sizeof(de->d_name))
                de->d_name[u16len] = 0;
            else
                de->d_name[sizeof(de->d_name) - 1] = 0;

            if (!sel || sel(de)) {
                num_entries++;

                *namelist = (dirent**)realloc(*namelist, num_entries * sizeof(dirent*));
                if (!*namelist) {
                    *_errno() = ENOMEM;
                    free(de);
                    NtClose(h);
                    return -1;
                }

                (*namelist)[num_entries - 1] = de;

                de = (dirent*)malloc(sizeof(dirent));
                if (!de) {
                    *_errno() = ENOMEM;
                    NtClose(h);

                    for (size_t i = 0; i < num_entries; i++) {
                        free((*namelist)[i]);
                    }

                    free(*namelist);

                    return -1;
                }
            }

            if (fdi.NextEntryOffset == 0)
                break;

            buf2 += fdi.NextEntryOffset;
        }
    }

    free(de);

    NtClose(h);

    if (compar) {
        qsort(*namelist, num_entries, sizeof(dirent*),
              (int(*)(const void*, const void*))compar);
    }

    return (int)num_entries;
}

extern "C"
int alphasort(const dirent** d1, const dirent** d2) {
    return strcoll((*d1)->d_name, (*d2)->d_name);
}
