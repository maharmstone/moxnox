// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <stdint.h>

import nt;
import kernelbase;
import print;

class TLSTest {
public:
    TLSTest() {
        print("Constructor: thread {}\n", NtCurrentTeb()->ClientId.UniqueThread);
    }

    ~TLSTest() {
        print("Destructor: thread {}\n", NtCurrentTeb()->ClientId.UniqueThread);
    }

    void foo() {
        bar = 1;
    }

    volatile int bar = 0;
};

thread_local TLSTest obj;

static uint32_t thread_func(void*) {
    obj.foo();
    return 0;
}

int main() {
    obj.foo();

    auto t1 = CreateThread(nullptr, 0, thread_func, nullptr, 0, nullptr);
    auto t2 = CreateThread(nullptr, 0, thread_func, nullptr, 0, nullptr);

    NtWaitForSingleObject(t1, false, nullptr);
    NtWaitForSingleObject(t2, false, nullptr);

    NtClose(t1);
    NtClose(t2);

    return 0;
}
