// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <variant>
#include <string_view>
#include <optional>
#include <functional>
#include <atomic>
#include <mutex>
#include <stdint.h>
#include <stddef.h>
#include <signal.h>
#include "context_offsets.h"

export module thread;

import nt;
import handle;
import object_ref;
import objects;
import list;
import process;
import spinlock;
import scheduler;

#ifdef __x86_64__
import x86;
#endif

using std::atomic;
using std::span;
using std::align_val_t;

export struct multiple_wait_block;

export enum class waiter_object_type {
    thread,
    multiple
};

export class waiter_object {
public:
    waiter_object(enum waiter_object_type type) : type(type) { }

    list_entry wait_le;
    enum waiter_object_type type;
    wait_type_t wait_type;
};

export enum class thread_status {
    runnable,
    running,
    waiting,
    waiting_multiple,
    waiting_no_object,
    dead,
    dying
};

export struct pending_apc {
    list_entry le;
    PS_APC_ROUTINE routine;
    uintptr_t arg1;
    uintptr_t arg2;
    uintptr_t arg3;
};

#ifdef __x86_64__
export size_t XSAVE_SIZE = 576; // FIXME
#endif

export class thread : public sync_object {
public:
    thread() : sync_object(object_type::thread, false) { }
    ~thread();

#ifdef __x86_64__
    static void* operator new(size_t) {
        return ::operator new(offsetof(::thread, xsave) + XSAVE_SIZE,
                              align_val_t{alignof(::thread)});
    }

    static void operator delete(void* p) {
        ::operator delete(p, align_val_t{alignof(::thread)});
    }
#endif

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status query_information(THREAD_INFORMATION_CLASS info_class,
                                span<uint8_t> buffer, uint32_t& return_length);
    nt_status set_information(THREAD_INFORMATION_CLASS info_class,
                              span<const uint8_t> buffer);
    nt_status set_pending_signal(span<const uint8_t> buffer);
    nt_status block_signals(span<const uint8_t> buffer);
    nt_status unblock_signals(span<const uint8_t> buffer);
    nt_status set_signals_mask(span<const uint8_t> buffer);
    nt_status query_signals_mask(span<uint8_t> buffer, uint32_t& return_length);
    nt_status queue_apc(PS_APC_ROUTINE apc_routine, void* apc_arg1,
                        void* apc_arg2, void* apc_arg3);
    nt_status wait_for_signal(span<const uint8_t> buffer);
    nt_status query_signals_pending(span<uint8_t> buffer, uint32_t& return_length);
    void break_apc_waits();

    list_entry le;
    CONTEXT context;
    CONTEXT syscall_context;
    enum thread_status status;
    uintptr_t tid;
    void* kernel_stack = nullptr;
    process* proc = nullptr;
    uint64_t affinity = 0xffffffffffffffff;
    void* syscall_stack = nullptr;
#ifdef __aarch64__
    uint64_t user_stack;
#endif
    bool kernel_mode;
    enum nt_status exit_status = nt_status::PENDING;
    list_entry process_le;
    atomic<uint32_t> suspend_count = 0;
    TEB* teb;
    uint32_t current_cpu = 0xffffffff;
    bool wait_interruptible;
    waiter_object wait_obj{waiter_object_type::thread};
    list_entry timeout_le;
    uint64_t wait_timeout;
    multiple_wait_block* multwait;
    bool kill_on_wake = false;
    spinlock apc_lock;
    list_entry pending_apcs = { &pending_apcs, &pending_apcs };
    sigset_t pending_signals = 0;
    sigset_t blocked_signals = 0;
    void* user_stack_base = nullptr;
    uint64_t last_scheduled_tsc;
    uint64_t time_running = 0;
    uint64_t last_userspace_transition;
    uint64_t time_running_in_userspace = 0;
    unsigned int in_allocator = 0;
#ifdef __x86_64__
    xsave_area xsave; // must be last
#endif
} __attribute__ ((aligned (16)));

static_assert(is_sync_object<::thread>);

static_assert(offsetof(::thread, context) == THREAD_CONTEXT);
static_assert(offsetof(::thread, proc) == THREAD_PROC);
static_assert(offsetof(::thread, syscall_stack) == THREAD_SYSCALL_STACK);
static_assert(offsetof(::thread, kernel_mode) == THREAD_KERNEL_MODE);
static_assert(offsetof(::thread, timeout_le) == THREAD_TIMEOUT_LE);
static_assert(offsetof(::thread, wait_timeout) == THREAD_WAIT_TIMEOUT);
static_assert(offsetof(::thread, syscall_context) == THREAD_SYSCALL_CONTEXT);
static_assert(offsetof(::thread, last_userspace_transition) == THREAD_LAST_USERSPACE_TRANSITION);
static_assert(offsetof(::thread, time_running_in_userspace) == THREAD_TIME_RUNNING_IN_USERSPACE);

#ifdef __aarch64__
static_assert(offsetof(::thread, user_stack) == THREAD_USER_STACK);
#elifdef __x86_64__
static_assert(offsetof(::thread, xsave) == THREAD_XSAVE);
static_assert(offsetof(::thread, status) == THREAD_STATUS);
static_assert(offsetof(::thread, suspend_count) == THREAD_SUSPEND_COUNT);
static_assert(offsetof(::thread, teb) == THREAD_TEB);

static_assert(sizeof(::thread::status) == 4);
static_assert((unsigned int)thread_status::dead == ENUM_THREAD_STATUS_DEAD);
#endif

#ifdef __aarch64__

static_assert(offsetof(ARM64_NT_CONTEXT, Cpsr) == CONTEXT_CPSR);
static_assert(offsetof(ARM64_NT_CONTEXT, X0) == CONTEXT_X0);
static_assert(offsetof(ARM64_NT_CONTEXT, Sp) == CONTEXT_SP);
static_assert(offsetof(ARM64_NT_CONTEXT, Pc) == CONTEXT_PC);
static_assert(offsetof(ARM64_NT_CONTEXT, V[0]) == CONTEXT_V0);
static_assert(offsetof(ARM64_NT_CONTEXT, Fpcr) == CONTEXT_FPCR);
static_assert(offsetof(ARM64_NT_CONTEXT, Fpsr) == CONTEXT_FPSR);

static_assert(sizeof(ARM64_NT_CONTEXT) == CONTEXT_SIZEOF);

#elifdef __x86_64__

static_assert(sizeof(AMD64_CONTEXT) == 0x4d0);

static_assert(offsetof(AMD64_CONTEXT, ContextFlags) == CONTEXT_CONTEXT_FLAGS);
static_assert(offsetof(AMD64_CONTEXT, MxCsr) == CONTEXT_MXCSR);
static_assert(offsetof(AMD64_CONTEXT, Rax) == CONTEXT_RAX);
static_assert(offsetof(AMD64_CONTEXT, Rcx) == CONTEXT_RCX);
static_assert(offsetof(AMD64_CONTEXT, Rdx) == CONTEXT_RDX);
static_assert(offsetof(AMD64_CONTEXT, Rbx) == CONTEXT_RBX);
static_assert(offsetof(AMD64_CONTEXT, Rsp) == CONTEXT_RSP);
static_assert(offsetof(AMD64_CONTEXT, Rbp) == CONTEXT_RBP);
static_assert(offsetof(AMD64_CONTEXT, Rsi) == CONTEXT_RSI);
static_assert(offsetof(AMD64_CONTEXT, Rdi) == CONTEXT_RDI);
static_assert(offsetof(AMD64_CONTEXT, R8) == CONTEXT_R8);
static_assert(offsetof(AMD64_CONTEXT, R9) == CONTEXT_R9);
static_assert(offsetof(AMD64_CONTEXT, R10) == CONTEXT_R10);
static_assert(offsetof(AMD64_CONTEXT, R11) == CONTEXT_R11);
static_assert(offsetof(AMD64_CONTEXT, R12) == CONTEXT_R12);
static_assert(offsetof(AMD64_CONTEXT, R13) == CONTEXT_R13);
static_assert(offsetof(AMD64_CONTEXT, R14) == CONTEXT_R14);
static_assert(offsetof(AMD64_CONTEXT, R15) == CONTEXT_R15);
static_assert(offsetof(AMD64_CONTEXT, Rip) == CONTEXT_RIP);
static_assert(offsetof(AMD64_CONTEXT, EFlags) == CONTEXT_EFLAGS);
static_assert(offsetof(AMD64_CONTEXT, SegCs) == CONTEXT_CS);
static_assert(offsetof(AMD64_CONTEXT, SegDs) == CONTEXT_DS);
static_assert(offsetof(AMD64_CONTEXT, SegEs) == CONTEXT_ES);
static_assert(offsetof(AMD64_CONTEXT, SegFs) == CONTEXT_FS);
static_assert(offsetof(AMD64_CONTEXT, SegGs) == CONTEXT_GS);
static_assert(offsetof(AMD64_CONTEXT, SegSs) == CONTEXT_SS);
static_assert(offsetof(AMD64_CONTEXT, FltSave) == CONTEXT_FXSAVE); // must be aligned on 16-byte boundary

#endif

export atomic<uintptr_t> next_tid = 1;

export ::thread* get_current_thread();
export void thread_died(::thread& t, bool* last_thread, bool remove_from_proc);
export void terminate_thread(::thread& t, bool remove_from_proc);
export object_ref deref_handle(handle_t h);
export object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask);
export object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2);

export object_ref add_kernel_thread(void (*func)(void*), void* param) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return add_kernel_thread((void(*)(void*, void*))func, param, nullptr);
#pragma GCC diagnostic pop
}

export object_ref add_kernel_thread(void (*func)()) {
    return add_kernel_thread((void(*)(void*, void*))func, nullptr, nullptr);
}

export void print_thread_access_mask(ACCESS_MASK access_mask);

export class waiter_object_multiple : public waiter_object {
public:
    waiter_object_multiple() : waiter_object(waiter_object_type::multiple) { }

    multiple_wait_block* block;
    bool done = false;
};

export struct multiple_wait_block {
    ::thread* t;
    atomic<unsigned int> left;
    unsigned int index;
    wait_type_t wait_type;
    unsigned int num_entries;
    waiter_object_multiple entries[0];
};
