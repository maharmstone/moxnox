// SPDX-License-Identifier: LGPL-2.1-or-later
#include <unistd.h>
#include <errno.h>
#include <limits.h>

import nt;

#undef errno
import errno;

extern "C"
long sysconf(int name) {
    switch (name) {
        case _SC_CLK_TCK:
            return 1000;

        case _SC_ARG_MAX:
            // 65535 bytes maximum for UNICODE_STRING, divided by sizeof(char16_t)
            return 32767;

        case _SC_PHYS_PAGES: {
            SYSTEM_BASIC_INFORMATION sbi;

            auto status = NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::BasicInformation,
                                                   &sbi, sizeof(sbi), nullptr);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            return sbi.NumberOfPhysicalPages;
        }

        case _SC_NPROCESSORS_ONLN:
        case _SC_NPROCESSORS_CONF: {
            SYSTEM_BASIC_INFORMATION sbi;

            auto status = NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::BasicInformation,
                                                   &sbi, sizeof(sbi), nullptr);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            return sbi.NumberOfProcessors;
        }

        case _SC_PAGESIZE: {
            SYSTEM_BASIC_INFORMATION sbi;

            auto status = NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::BasicInformation,
                                                   &sbi, sizeof(sbi), nullptr);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            return sbi.PageSize;
        }

        case _SC_OPEN_MAX:
            return 2048; // hardcoded in ucrtbase

        case _SC_HOST_NAME_MAX:
            return 255; // minimum allowed by spec

        case _SC_GETPW_R_SIZE_MAX:
            return 1024; // size of static_buf in mnposix/src/pwd.cpp

        case _SC_TTY_NAME_MAX:
            return TTY_NAME_MAX;

        case _SC_IOV_MAX:
            return IOV_MAX;

        default:
            *_errno() = EINVAL;
            return -1;
    }
}
