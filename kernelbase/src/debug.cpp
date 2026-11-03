// SPDX-License-Identifier: LGPL-2.1-or-later
#include <utility>
#include <algorithm>
#include <string.h>
#include <stdint.h>

import nt;
import ntdll;
import kernelbase;
import print;

using namespace std;

extern "C"
void RaiseException(uint32_t code, uint32_t flags, uint32_t count,
                    const uintptr_t* args) {
    EXCEPTION_RECORD rec;

    rec.ExceptionCode = code;
    rec.ExceptionFlags = flags & EH_NONCONTINUABLE;
    rec.ExceptionRecord = nullptr;
    rec.ExceptionAddress = (void*)RaiseException;

    if (!args)
        rec.NumberParameters = 0;
    else {
        rec.NumberParameters = min(count, EXCEPTION_MAXIMUM_PARAMETERS);

        memcpy(rec.ExceptionInformation, args,
               rec.NumberParameters * sizeof(*args));
    }

    RtlRaiseException(&rec);
}
