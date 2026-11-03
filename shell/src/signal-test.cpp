// SPDX-License-Identifier: GPL-2.0-only
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

import nt;
import kernelbase;
import unique_handle;
import print;

using namespace std;

static bool sigmask_has(sigset_t mask, int signum) {
    return (mask & ((sigset_t)1 << (signum - 1))) != 0;
}

// --- Child process for test 4 ---

static volatile int child_got_signal = 0;

static void child_sigint_handler(int) {
    child_got_signal = 1;
}

static void run_child() {
    signal(SIGINT, child_sigint_handler);

    while (!child_got_signal) {
        LARGE_INTEGER timeout;

        timeout.QuadPart = 0;

        NtDelayExecution(true, &timeout);
    }

    exit(42);
}

// --- Test 1: signal() + raise() ---

static int test1_count = 0;

static void test1_handler(int sig) {
    if (sig == SIGINT)
        test1_count++;
}

static void test_signal_and_raise() {
    print("=== Test 1: signal() and raise() ===\n");

    signal(SIGINT, test1_handler);
    raise(SIGINT);

    if (test1_count != 1) {
        print("Test 1: expected handler to run once, got {}\n", test1_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_IGN);
    raise(SIGINT);

    if (test1_count != 1) {
        print("Test 1: SIG_IGN handler still ran (count = {})\n", test1_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 1 passed\n\n");
}

// --- Test 2: sigprocmask blocking ---

static int test2_count = 0;

static void test2_handler(int) {
    test2_count++;
}

static void test_sigprocmask_blocking() {
    print("=== Test 2: sigprocmask blocking ===\n");

    signal(SIGINT, test2_handler);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    raise(SIGINT);

    if (test2_count != 0) {
        print("Test 2: handler ran while signal was blocked (count = {})\n", test2_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    sigprocmask(SIG_UNBLOCK, &mask, nullptr);

    if (test2_count != 1) {
        print("Test 2: expected handler to run once on unblock, got {}\n", test2_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 2 passed\n\n");
}

// --- Test 3: blocked signal raised twice fires exactly once ---

static int test3_count = 0;

static void test3_handler(int) {
    test3_count++;
}

static void test_pending_once() {
    print("=== Test 3: blocked signal raised twice fires once on unblock ===\n");

    signal(SIGINT, test3_handler);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    raise(SIGINT);
    raise(SIGINT);

    if (test3_count != 0) {
        print("Test 3: handler ran while signal was blocked (count = {})\n", test3_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    sigprocmask(SIG_UNBLOCK, &mask, nullptr);

    if (test3_count != 1) {
        print("Test 3: expected handler to run exactly once, got {}\n", test3_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 3 passed\n\n");
}

// --- Test 4: kill() cross-process ---

static void test_kill_cross_process() {
    print("=== Test 4: kill() cross-process ===\n");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    char16_t cmdline[] = u"signal-test --child";
    if (!CreateProcessW(u"/bin/signal-test.exe", cmdline,
                        nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi)) {
        print("Test 4: CreateProcessW failed\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    unique_handle proc{pi.hProcess};
    NtClose(pi.hThread);

    LARGE_INTEGER t;
    t.QuadPart = -1000000; // 100ms
    NtDelayExecution(false, &t);

    if (kill((pid_t)pi.dwProcessId, SIGINT) != 0) {
        print("Test 4: kill() failed\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtWaitForSingleObject(proc.get(), false, nullptr);

    PROCESS_BASIC_INFORMATION pbi;
    NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                              &pbi, sizeof(pbi), nullptr);

    if ((uint32_t)pbi.ExitStatus != 42) {
        print("Test 4: expected child exit code 42, got {}\n", (uint32_t)pbi.ExitStatus);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    print("Test 4 passed\n\n");
}

// --- Test 5: basic sigaction() ---

static int test5_count = 0;

static void test5_handler(int sig) {
    if (sig == SIGINT)
        test5_count++;
}

static void test_sigaction_basic() {
    print("=== Test 5: basic sigaction() ===\n");

    struct sigaction act = {};
    act.sa_handler = test5_handler;
    sigaction(SIGINT, &act, nullptr);

    raise(SIGINT);

    if (test5_count != 1) {
        print("Test 5: expected handler to run once, got {}\n", test5_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    struct sigaction oldact = {};
    act.sa_handler = SIG_DFL;
    sigaction(SIGINT, &act, &oldact);

    if (oldact.sa_handler != test5_handler) {
        print("Test 5: oldact.sa_handler was not test5_handler\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    print("Test 5 passed\n\n");
}

// --- Test 6: sa_mask blocks additional signal during handler ---

static int test6_sigint_count = 0;
static int test6_sigusr1_count = 0;
static int test6_sigusr1_during_handler = -1;

static void test6_sigusr1_handler(int) {
    test6_sigusr1_count++;
}

static void test6_sigint_handler(int) {
    test6_sigint_count++;
    raise(SIGUSR1);
    test6_sigusr1_during_handler = test6_sigusr1_count;
}

static void test_sa_mask() {
    print("=== Test 6: sa_mask blocks signal during handler ===\n");

    signal(SIGUSR1, test6_sigusr1_handler);

    struct sigaction act = {};
    act.sa_handler = test6_sigint_handler;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGUSR1);
    sigaction(SIGINT, &act, nullptr);

    raise(SIGINT);

    if (test6_sigint_count != 1) {
        print("Test 6: expected SIGINT handler to run once, got {}\n", test6_sigint_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    if (test6_sigusr1_during_handler != 0) {
        print("Test 6: SIGUSR1 fired during SIGINT handler\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    if (test6_sigusr1_count != 1) {
        print("Test 6: expected SIGUSR1 to fire once after SIGINT handler, got {}\n", test6_sigusr1_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    print("Test 6 passed\n\n");
}

// --- Test 7: signal self-blocked during handler ---

static int test7_count = 0;
static bool test7_in_handler = false;
static bool test7_saw_reentry = false;

static void test7_handler(int) {
    if (test7_in_handler) {
        test7_saw_reentry = true;
        return;
    }
    test7_in_handler = true;
    test7_count++;
    if (test7_count == 1)
        raise(SIGINT);
    test7_in_handler = false;
}

static void test_sa_self_block() {
    print("=== Test 7: signal self-blocked during handler ===\n");

    struct sigaction act = {};
    act.sa_handler = test7_handler;
    sigaction(SIGINT, &act, nullptr);

    raise(SIGINT);

    if (test7_saw_reentry) {
        print("Test 7: SIGINT handler was re-entered (signal not self-blocked)\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    if (test7_count != 2) {
        print("Test 7: expected count 2 (original + deferred), got {}\n", test7_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 7 passed\n\n");
}

// --- Test 8: SA_NODEFER allows re-entrant delivery ---

static int test8_count = 0;

static void test8_handler(int) {
    test8_count++;
    if (test8_count < 3)
        raise(SIGINT);
}

static void test_sa_nodefer() {
    print("=== Test 8: SA_NODEFER allows re-entrant delivery ===\n");

    struct sigaction act = {};
    act.sa_handler = test8_handler;
    act.sa_flags = SA_NODEFER;
    sigaction(SIGINT, &act, nullptr);

    raise(SIGINT);

    if (test8_count != 3) {
        print("Test 8: expected count 3, got {}\n", test8_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 8 passed\n\n");
}

// --- Test 9: SA_RESETHAND resets disposition after delivery ---

static int test9_count = 0;

static void test9_handler(int) {
    test9_count++;
}

static void test_sa_resethand() {
    print("=== Test 9: SA_RESETHAND resets disposition after delivery ===\n");

    struct sigaction act = {};
    act.sa_handler = test9_handler;
    act.sa_flags = (int)SA_RESETHAND;
    sigaction(SIGINT, &act, nullptr);

    raise(SIGINT);

    if (test9_count != 1) {
        print("Test 9: expected handler to run once, got {}\n", test9_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    struct sigaction oldact = {};
    sigaction(SIGINT, nullptr, &oldact);

    if (oldact.sa_handler != SIG_DFL) {
        print("Test 9: expected disposition to be SIG_DFL after SA_RESETHAND delivery\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    print("Test 9 passed\n\n");
}

// --- Test 10: SA_SIGINFO three-argument handler ---

static int test10_signo = -1;

static void test10_handler(int, siginfo_t* info, void*) {
    test10_signo = info->si_signo;
}

static void test_sa_siginfo() {
    print("=== Test 10: SA_SIGINFO three-argument handler ===\n");

    struct sigaction act = {};
    act.sa_sigaction = test10_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &act, nullptr);

    raise(SIGINT);

    if (test10_signo != SIGINT) {
        print("Test 10: expected si_signo = {}, got {}\n", SIGINT, test10_signo);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 10 passed\n\n");
}

// --- Test 11: signal mask restored after handler ---

static sigset_t test11_mask_in_handler = 0;

static void test11_handler(int) {
    sigprocmask(SIG_BLOCK, nullptr, &test11_mask_in_handler);
}

static void test_mask_restored() {
    print("=== Test 11: signal mask restored after handler ===\n");

    struct sigaction act = {};
    act.sa_handler = test11_handler;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGUSR1);
    sigaction(SIGINT, &act, nullptr);

    raise(SIGINT);

    if (!sigmask_has(test11_mask_in_handler, SIGINT)) {
        print("Test 11: SIGINT was not blocked during its own handler\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    if (!sigmask_has(test11_mask_in_handler, SIGUSR1)) {
        print("Test 11: SIGUSR1 (sa_mask) was not blocked during SIGINT handler\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    sigset_t mask_after = 0;
    sigprocmask(SIG_BLOCK, nullptr, &mask_after);

    if (sigmask_has(mask_after, SIGINT)) {
        print("Test 11: SIGINT still blocked after handler returned\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    if (sigmask_has(mask_after, SIGUSR1)) {
        print("Test 11: SIGUSR1 still blocked after handler returned\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 11 passed\n\n");
}

int main(int argc, char* argv[]) {
    if (argc >= 2 && strcmp(argv[1], "--child") == 0) {
        run_child();
        return 0;
    }

    print("=== Signal test suite ===\n\n");

    test_signal_and_raise();
    test_sigprocmask_blocking();
    test_pending_once();
    test_kill_cross_process();
    test_sigaction_basic();
    test_sa_mask();
    test_sa_self_block();
    test_sa_nodefer();
    test_sa_resethand();
    test_sa_siginfo();
    test_mask_restored();

    print("All tests passed\n");
    return 0;
}
