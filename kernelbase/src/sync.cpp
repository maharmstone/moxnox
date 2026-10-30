// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>

import nt;

extern "C"
void Sleep(uint32_t timeout) {
    LARGE_INTEGER t;

    // convert from milliseconds to units of 100ns
    t.QuadPart = -(int64_t)timeout * 10000;

    NtDelayExecution(false, &t);
}
