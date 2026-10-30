// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <string.h>
#include <string_view>

import nt;
import ntdll;
import kernelbase;
import print;
import win32error;

using namespace std;

extern "C"
bool FreeLibrary(HMODULE mod) {
    auto status = LdrUnloadDll(mod);

    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    return true;
}

extern "C"
void* GetProcAddress(HMODULE mod, const char* function) {
    nt_status status;
    void* addr;
    ANSI_STRING as;

    if (!mod) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    // FIXME - resolving by ordinal

    as.Buffer = (char*)function;
    as.Length = as.MaximumLength = (uint16_t)strlen(function);

    status = LdrGetProcedureAddress(mod, &as, 0, &addr);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return nullptr;
    }

    return addr;
}
