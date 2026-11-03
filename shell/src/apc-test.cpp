// SPDX-License-Identifier: GPL-2.0-only
#include <utility>
#include <atomic>
#include <array>

import nt;
import kernelbase;
import unique_handle;
import print;

using namespace std;

static unique_handle make_event(EVENT_TYPE type = EVENT_TYPE::SynchronizationEvent,
                                bool signalled = false) {
    HANDLE h;
    auto status = NtCreateEvent(&h, SYNCHRONIZE | EVENT_MODIFY_STATE, nullptr, type, signalled);
    if (!success(status)) {
        print("NtCreateEvent returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }
    return unique_handle{h};
}

// --- Test 1: APC delivered to thread in alertable NtWaitForSingleObject ---

static void test_alertable_wait() {
    print("=== Test 1: APC delivered to alertable NtWaitForSingleObject ===\n");

    atomic<bool> apc_ran = false;
    auto wait_evt  = make_event();
    auto ready_evt = make_event(EVENT_TYPE::NotificationEvent);

    struct ctx { atomic<bool>* ran; HANDLE wait; HANDLE ready; };
    ctx c = { &apc_ran, wait_evt.get(), ready_evt.get() };

    unique_handle thread{CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& c = *(ctx*)arg;
        NtSetEvent(c.ready, nullptr);
        auto status = NtWaitForSingleObject(c.wait, true, nullptr);
        if (status != nt_status::USER_APC) {
            print("Test 1: expected nt_status::USER_APC, got {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (!c.ran->load()) {
            print("Test 1: APC did not run\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        return 0;
    }, &c, 0, nullptr)};

    NtWaitForSingleObject(ready_evt.get(), false, nullptr);

    LARGE_INTEGER t;
    t.QuadPart = -100000; // 10ms
    NtDelayExecution(false, &t); // let thread settle into wait

    auto status = NtQueueApcThread(thread.get(), [](void* arg, void*, void*) {
        ((atomic<bool>*)arg)->store(true);
    }, &apc_ran, nullptr, nullptr);
    if (!success(status)) {
        print("Test 1: NtQueueApcThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtWaitForSingleObject(thread.get(), false, nullptr);

    print("Test 1 passed\n\n");
}

// --- Test 2: APC wakes thread from alertable NtDelayExecution ---

static void test_alertable_delay() {
    print("=== Test 2: APC delivered to alertable NtDelayExecution ===\n");

    atomic<bool> apc_ran = false;
    auto ready_evt = make_event(EVENT_TYPE::NotificationEvent);

    struct ctx { atomic<bool>* ran; HANDLE ready; };
    ctx c = { &apc_ran, ready_evt.get() };

    unique_handle thread{CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& c = *(ctx*)arg;
        NtSetEvent(c.ready, nullptr);
        LARGE_INTEGER timeout;
        timeout.QuadPart = -50000000LL; // 5 seconds relative
        auto status = NtDelayExecution(true, &timeout);
        if (status != nt_status::USER_APC) {
            print("Test 2: expected nt_status::USER_APC, got {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (!c.ran->load()) {
            print("Test 2: APC did not run\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        return 0;
    }, &c, 0, nullptr)};

    NtWaitForSingleObject(ready_evt.get(), false, nullptr);

    LARGE_INTEGER t;
    t.QuadPart = -100000; // 10ms
    NtDelayExecution(false, &t);

    auto status = NtQueueApcThread(thread.get(), [](void* arg, void*, void*) {
        ((atomic<bool>*)arg)->store(true);
    }, &apc_ran, nullptr, nullptr);
    if (!success(status)) {
        print("Test 2: NtQueueApcThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtWaitForSingleObject(thread.get(), false, nullptr);

    print("Test 2 passed\n\n");
}

// --- Test 3: APC queued before alertable wait fires immediately on entry ---
//
// Thread enters a non-alertable wait while main queues the APC, then
// proceeds to an alertable wait. The APC should fire immediately on entry
// rather than requiring main to do anything further.

static void test_apc_queued_before_wait() {
    print("=== Test 3: APC queued before alertable wait entry ===\n");

    atomic<bool> apc_ran = false;
    auto proceed_evt = make_event();
    auto wait_evt = make_event();

    struct ctx { atomic<bool>* ran; HANDLE proceed; HANDLE wait; };
    ctx c = { &apc_ran, proceed_evt.get(), wait_evt.get() };

    unique_handle thread{CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& c = *(ctx*)arg;
        // Block non-alertably so main can queue the APC first
        NtWaitForSingleObject(c.proceed, false, nullptr);
        // APC is now pending - alertable wait should fire it immediately
        auto status = NtWaitForSingleObject(c.wait, true, nullptr);
        if (status != nt_status::USER_APC) {
            print("Test 3: expected nt_status::USER_APC, got {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (!c.ran->load()) {
            print("Test 3: APC did not run\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        return 0;
    }, &c, 0, nullptr)};

    LARGE_INTEGER t;
    t.QuadPart = -100000; // 10ms
    NtDelayExecution(false, &t); // let thread reach non-alertable wait

    auto status = NtQueueApcThread(thread.get(), [](void* arg, void*, void*) {
        ((atomic<bool>*)arg)->store(true);
    }, &apc_ran, nullptr, nullptr);
    if (!success(status)) {
        print("Test 3: NtQueueApcThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtSetEvent(proceed_evt.get(), nullptr);

    NtWaitForSingleObject(thread.get(), false, nullptr);

    print("Test 3 passed\n\n");
}

// --- Test 4: Non-alertable wait does not deliver pending APC ---

static void test_non_alertable_no_delivery() {
    print("=== Test 4: Non-alertable wait does not deliver APC ===\n");

    atomic<unsigned int> apc_count = 0;
    auto block_evt = make_event();
    auto ready_evt = make_event(EVENT_TYPE::NotificationEvent);

    struct ctx { atomic<unsigned int>* count; HANDLE block; HANDLE ready; };
    ctx c = { &apc_count, block_evt.get(), ready_evt.get() };

    unique_handle thread{CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& c = *(ctx*)arg;
        NtSetEvent(c.ready, nullptr);
        auto status = NtWaitForSingleObject(c.block, false, nullptr);
        if (status != nt_status::SUCCESS) {
            print("Test 4: expected SUCCESS from non-alertable wait, got {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (c.count->load() != 0) {
            print("Test 4: APC ran during non-alertable wait (count = {})\n", c.count->load());
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        // Drain the pending APC via a short alertable wait
        LARGE_INTEGER timeout;
        timeout.QuadPart = -50000000LL; // 5 seconds relative
        status = NtDelayExecution(true, &timeout);
        if (status != nt_status::USER_APC) {
            print("Test 4: expected nt_status::USER_APC from alertable wait, got {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (c.count->load() != 1) {
            print("Test 4: expected apc_count 1 after alertable wait, got {}\n", c.count->load());
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        return 0;
    }, &c, 0, nullptr)};

    NtWaitForSingleObject(ready_evt.get(), false, nullptr);

    auto status = NtQueueApcThread(thread.get(), [](void* arg, void*, void*) {
        auto* count = (atomic<unsigned int>*)arg;
        count->store(count->load() + 1);
    }, &apc_count, nullptr, nullptr);
    if (!success(status)) {
        print("Test 4: NtQueueApcThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtSetEvent(block_evt.get(), nullptr);

    NtWaitForSingleObject(thread.get(), false, nullptr);

    print("Test 4 passed\n\n");
}

// --- Test 5: Multiple APCs all run in FIFO order from a single alertable wait ---
//
// This also verifies that the NtContinue(ctx, true) loop in mn_handle_apc
// drains all pending APCs before returning nt_status::USER_APC to the caller.

static void test_multiple_apc_fifo() {
    print("=== Test 5: Multiple APCs run FIFO, all drained by one wait ===\n");

    static constexpr unsigned int num_apcs = 5;

    struct shared_data {
        array<unsigned int, num_apcs> order = {};
        atomic<unsigned int> pos{0};
    } s;

    auto wait_evt = make_event();

    struct ctx { shared_data* s; HANDLE wait; };
    ctx c = { &s, wait_evt.get() };

    unique_handle thread{CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& c = *(ctx*)arg;
        auto status = NtWaitForSingleObject(c.wait, true, nullptr);
        if (status != nt_status::USER_APC) {
            print("Test 5: expected nt_status::USER_APC, got {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        unsigned int ran = c.s->pos.load();
        if (ran != num_apcs) {
            print("Test 5: expected {} APCs to run, got {}\n", num_apcs, ran);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        for (unsigned int i = 0; i < num_apcs; i++) {
            if (c.s->order[i] != i) {
                print("Test 5: at position {}, expected APC {} but got {}\n",
                      i, i, c.s->order[i]);
                NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
                unreachable();
            }
        }
        return 0;
    }, &c, 0, nullptr)};

    LARGE_INTEGER t;
    t.QuadPart = -100000; // 10ms
    NtDelayExecution(false, &t);

    struct apc_arg { shared_data* s; unsigned int idx; };
    array<apc_arg, num_apcs> apc_args;

    for (unsigned int i = 0; i < num_apcs; i++) {
        apc_args[i] = { &s, i };
        auto status = NtQueueApcThread(thread.get(), [](void* arg, void*, void*) {
            auto& a = *(apc_arg*)arg;
            unsigned int pos = a.s->pos.load();
            a.s->order[pos] = a.idx;
            a.s->pos.store(pos + 1);
        }, &apc_args[i], nullptr, nullptr);
        if (!success(status)) {
            print("Test 5: NtQueueApcThread returned {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
    }

    NtWaitForSingleObject(thread.get(), false, nullptr);

    print("Test 5 passed\n\n");
}

// --- Test 6: All three APC arguments are passed correctly ---

static void test_argument_passing() {
    print("=== Test 6: All three APC arguments passed correctly ===\n");

    struct arg_result {
        void* got1 = nullptr;
        void* got2 = nullptr;
        void* got3 = nullptr;
    } result;

    static int sentinel2, sentinel3;
    auto wait_evt = make_event();

    struct ctx { arg_result* result; HANDLE wait; };
    ctx c = { &result, wait_evt.get() };

    unique_handle thread{CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& c = *(ctx*)arg;
        NtWaitForSingleObject(c.wait, true, nullptr);
        if (c.result->got1 != c.result) {
            print("Test 6: arg1: expected {:x}, got {:x}\n",
                  (uintptr_t)c.result, (uintptr_t)c.result->got1);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (c.result->got2 != &sentinel2) {
            print("Test 6: arg2: expected {:x}, got {:x}\n",
                  (uintptr_t)&sentinel2, (uintptr_t)c.result->got2);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (c.result->got3 != &sentinel3) {
            print("Test 6: arg3: expected {:x}, got {:x}\n",
                  (uintptr_t)&sentinel3, (uintptr_t)c.result->got3);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        return 0;
    }, &c, 0, nullptr)};

    LARGE_INTEGER t;
    t.QuadPart = -100000; // 10ms
    NtDelayExecution(false, &t);

    // arg1 = &result (used by APC to store what it received)
    // arg2 = &sentinel2, arg3 = &sentinel3 (distinct pointer values to verify)
    auto status = NtQueueApcThread(thread.get(), [](void* a1, void* a2, void* a3) {
        auto& res = *(arg_result*)a1;
        res.got1 = a1;
        res.got2 = a2;
        res.got3 = a3;
    }, &result, &sentinel2, &sentinel3);
    if (!success(status)) {
        print("Test 6: NtQueueApcThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtWaitForSingleObject(thread.get(), false, nullptr);

    print("Test 6 passed\n\n");
}

// --- Test 7: Already-signalled object takes priority over pending APC ---
//
// When the waited object is already signalled, the wait returns SUCCESS
// immediately even if an APC is pending. The APC stays in the queue and
// is delivered on the next alertable wait.

static void test_signalled_object_priority() {
    print("=== Test 7: Signalled object takes priority over pending APC ===\n");

    atomic<bool> apc_ran = false;
    auto signalled_evt = make_event(EVENT_TYPE::SynchronizationEvent, true);

    auto status = NtQueueApcThread(NtCurrentThread(), [](void* arg, void*, void*) {
        ((atomic<bool>*)arg)->store(true);
    }, &apc_ran, nullptr, nullptr);
    if (!success(status)) {
        print("Test 7: NtQueueApcThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    // Object is already signalled - should return SUCCESS, not dispatch APC
    status = NtWaitForSingleObject(signalled_evt.get(), true, nullptr);
    if (status != nt_status::SUCCESS) {
        print("Test 7: expected STATUS_SUCCESS, got {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }
    if (apc_ran.load()) {
        print("Test 7: APC ran during wait on already-signalled object\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    // Now drain the pending APC
    LARGE_INTEGER timeout;
    timeout.QuadPart = -50000000LL; // 5 seconds relative
    status = NtDelayExecution(true, &timeout);
    if (status != nt_status::USER_APC) {
        print("Test 7: expected nt_status::USER_APC from alertable wait, got {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }
    if (!apc_ran.load()) {
        print("Test 7: APC did not run after alertable wait\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    print("Test 7 passed\n\n");
}

// --- Test 8: APC delivered to thread in alertable NtWaitForMultipleObjects ---

static void test_alertable_wait_multiple() {
    print("=== Test 8: APC delivered to alertable NtWaitForMultipleObjects ===\n");

    atomic<bool> apc_ran = false;
    auto wait_evt1 = make_event();
    auto wait_evt2 = make_event();
    auto ready_evt = make_event(EVENT_TYPE::NotificationEvent);

    struct ctx { atomic<bool>* ran; HANDLE wait1; HANDLE wait2; HANDLE ready; };
    ctx c = { &apc_ran, wait_evt1.get(), wait_evt2.get(), ready_evt.get() };

    unique_handle thread{CreateThread(nullptr, 0, [](void* arg) -> uint32_t {
        auto& c = *(ctx*)arg;
        NtSetEvent(c.ready, nullptr);
        HANDLE handles[] = { c.wait1, c.wait2 };
        auto status = NtWaitForMultipleObjects(2, handles, true, true, nullptr);
        if (status != nt_status::USER_APC) {
            print("Test 8: expected nt_status::USER_APC, got {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        if (!c.ran->load()) {
            print("Test 8: APC did not run\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
        return 0;
    }, &c, 0, nullptr)};

    NtWaitForSingleObject(ready_evt.get(), false, nullptr);

    LARGE_INTEGER t;
    t.QuadPart = -100000; // 10ms
    NtDelayExecution(false, &t); // let thread settle into wait

    auto status = NtQueueApcThread(thread.get(), [](void* arg, void*, void*) {
        ((atomic<bool>*)arg)->store(true);
    }, &apc_ran, nullptr, nullptr);
    if (!success(status)) {
        print("Test 8: NtQueueApcThread returned {}\n", status);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtWaitForSingleObject(thread.get(), false, nullptr);

    print("Test 8 passed\n\n");
}

int main() {
    print("=== APC test suite ===\n\n");

    test_alertable_wait();
    test_alertable_delay();
    test_apc_queued_before_wait();
    test_non_alertable_no_delivery();
    test_multiple_apc_fifo();
    test_argument_passing();
    test_signalled_object_priority();
    test_alertable_wait_multiple();

    print("All tests passed\n");

    return 0;
}
