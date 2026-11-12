// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>

import nt;

extern "C"
uint32_t GetLastError() {
    return NtCurrentTeb()->LastErrorValue;
}
