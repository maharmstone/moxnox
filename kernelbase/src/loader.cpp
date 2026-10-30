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
