// SPDX-License-Identifier: GPL-2.0-only
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <array>
#include <string_view>
#include <span>

import nt;
import kernelbase;
import print;
import format;

using namespace std;

// Test 1: Static TLS (compile-time)
thread_local int static_tls_var = 100;
thread_local char static_tls_string[64] = "Initial value";

// Thread function
static unsigned int thread_func(void* arg) {
    int thread_id = (int)(intptr_t)arg;

    print("\n=== Thread {} started ===\n", thread_id);

    // Test static TLS - each thread should have independent copy
    print("Thread {}: static_tls_var initial value: {}\n",
          thread_id, static_tls_var);

    static_tls_var = 100 + thread_id;  // Modify
    format(span(static_tls_string), "Thread {} string\0", thread_id);

    print("Thread {}: static_tls_var modified to: {}\n",
          thread_id, static_tls_var);
    print("Thread {}: static_tls_string: {}\n",
          thread_id, string_view(static_tls_string));

    // Sleep a bit so threads interleave
    LARGE_INTEGER t;
    t.QuadPart = -1000000; // 100ms
    NtDelayExecution(false, &t);

    // Read again to verify values didn't change from other threads
    print("Thread {}: Final check - static_tls_var: {}, string: {}\n",
           thread_id, static_tls_var, string_view(static_tls_string));

    print("=== Thread {} finished ===\n\n", thread_id);

    return 0;
}

int main() {
    print("=== TLS Test Program ===\n\n");

    // Test 1: Static TLS in main thread
    print("Main thread: Initial static_tls_var = {}\n", static_tls_var);
    static_tls_var = 42;
    print("Main thread: Modified static_tls_var = {}\n", static_tls_var);

    // Create multiple threads

    {
        array<HANDLE, 4> threads;

        for (size_t i = 0; i < threads.size(); i++) {
            threads[i] = CreateThread(nullptr, 0, thread_func, (void*)i, 0, nullptr);

            if (threads[i] == 0)
                print("Failed to create thread {}\n", i);
        }

        // Wait for all threads
        auto status = NtWaitForMultipleObjects(threads.size(), threads.data(),
                                               false, false, nullptr);
        if (!success(status))
            print("NtWaitForMultipleObjects returned {}\n", status);

        // Close handles
        for (size_t i = 0; i < threads.size(); i++) {
            NtClose(threads[i]);
        }
    }

    // Verify main thread's TLS wasn't corrupted
    print("\n=== Back in main thread ===\n");
    print("Main thread: Final static_tls_var = {} (should be 42)\n",
          static_tls_var);

    print("\n=== All tests completed ===\n");
    return 0;
}
