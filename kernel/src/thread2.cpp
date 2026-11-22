// SPDX-License-Identifier: GPL-2.0-only
module;

#include <optional>
#include <mutex>
#include <new>
#include <bit>
#include <string_view>
#include <algorithm>
#include <atomic>
#include <signal.h>
#include <poll.h>
#include "timer-asm.h"
#include "trace.h"

#ifdef __x86_64__
#include <x86intrin.h>
#endif

module thread;

import print;
import mem;
import config;
import disable_interrupts;
import cpu_block;
import spinlock;
import handle;
import crt;
import process;
import scheduler;
import interrupts;
import trace;
import visit;
import vfs;

#ifdef __aarch64__
import aarch64;
#endif

using std::lock_guard;
using std::nullopt;
using std::countr_zero;
using std::string_view;
using std::pair;
using std::swap;
using std::memory_order;
using std::numeric_limits;
using std::optional;

#ifdef __x86_64__
static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_DS = 0x10;
#endif

static const int LAST_SIG = SIGTTOU;

thread* get_current_thread() {
    disable_preempt();

    auto t = get_cpu_block()->current_thread;

    enable_preempt();

    return t;
}

void thread_died(thread& t, bool* last_thread, bool remove_from_proc) {
    t.status = thread_status::dead;
    t.signalled = true;

    wake_up_waiters(t.waiters, WAIT_TYPE_READING);

    remove_entry_list(t.le);

    if (t.proc && remove_from_proc) {
        lock_guard lock(t.proc->threads_lock);

        if (!t.detached) {
            remove_entry_list(t.process_le);
            t.detached = true;
        }

        if (last_thread)
            *last_thread = is_list_empty(t.proc->threads);
    }

    t.release();
}

static void terminate_thread2(thread& t, bool* last_thread,
                              bool remove_from_proc) {
    if (t.status == thread_status::dying || t.status == thread_status::dead)
        return;

    if (t.kernel_mode) {
        if (t.wait_interruptible) {
            switch (t.status) {
                case thread_status::waiting: {
                    remove_entry_list(t.wait_obj.wait_le);

                    if (t.wait_timeout != 0)
                        remove_entry_list(t.timeout_le);
                    break;
                }

                case thread_status::waiting_multiple: {
                    auto& block = *t.multwait;

                    for (unsigned int i = 0; i < block.num_entries; i++) {
                        if (!block.entries[i].done)
                            remove_entry_list(block.entries[i].wait_le);
                    }

                    if (t.wait_timeout != 0)
                        remove_entry_list(t.timeout_le);

                    t.multwait = nullptr;
                    break;
                }

                case thread_status::waiting_no_object: {
                    if (t.wait_timeout != 0)
                        remove_entry_list(t.timeout_le);
                    break;
                }

                default:
                    break;
            }

            if (t.current_cpu == 0xffffffff && (t.status == thread_status::waiting || t.status == thread_status::waiting_multiple || t.status == thread_status::waiting_no_object)) {
                t.status = thread_status::dying;
                return;
            }
        } else if (t.status == thread_status::waiting || t.status == thread_status::waiting_multiple || t.status == thread_status::waiting_no_object) {
            t.kill_on_wake = true;
            return;
        }

        t.status = thread_status::dying;
    } else { // user mode
        if (t.current_cpu == 0xffffffff)
            thread_died(t, last_thread, remove_from_proc);
        else {
            t.status = thread_status::dying;
            // FIXME - send yield ISR
        }
    }
}

void terminate_thread(thread& t, bool remove_from_proc) {
    bool thread_is_dead, last_thread = false;

    t.acquire();

    disable_interrupts();
    acquire_sync_lock();

    terminate_thread2(t, &last_thread, remove_from_proc);

    thread_is_dead = t.status == thread_status::dead;

    release_sync_lock();
    enable_interrupts();

    if (!t.proc || !thread_is_dead) {
        t.release();
        return;
    }

    if (&t == get_current_thread()) {
#ifdef __x86_64__
        auto tsc = __rdtsc();
#elifdef __aarch64__
        auto tsc = cntvct_el0_load();
#endif

        t.time_running += tsc - t.last_scheduled_tsc;
    }

    t.proc->dead_threads_time_running += t.time_running;
    t.proc->dead_threads_time_running_in_userspace += t.time_running_in_userspace;

    if (last_thread) {
        t.proc->exit_status = t.exit_status;
        t.proc->terminate();
    } else {
        size_t region_size;

        if (t.user_stack_base) {
            region_size = 0;

            t.proc->free_vm(&t.user_stack_base, &region_size, MEM_RELEASE);
        }

        region_size = 0;

        t.proc->free_vm((void**)&t.teb, &region_size, MEM_RELEASE);
    }

    t.release();
}

thread::~thread() {
    if (kernel_stack) {
        auto va = (uintptr_t)kernel_stack + (KERNEL_GUARD_PAGES << PAGE_SHIFT);
        uint64_t pages_left = KERNEL_STACK_PAGES;

        while (pages_left > 0) {
            auto pa = va_to_pa_lookup(va);
            auto order = get_folio_order(pa);

            free_folio(pa);

            for (unsigned int i = 0; i < (unsigned int)(1 << order); i++) {
                unmap_page(va);
                va += PAGE_SIZE;
            }

            pages_left -= 1 << order;
        }

        free_va((uintptr_t)kernel_stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    }

    while (true) {
        pending_apc* p = nullptr;

        {
            lock_guard lock(apc_lock);

            if (!is_list_empty(pending_apcs)) {
                p = &containing_record<pending_apc>(pending_apcs.next);
                remove_entry_list(p->le);
            }
        }

        if (!p)
            break;

        delete p;
    }

    if (proc)
        proc->release();
}

object_ref deref_handle(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h);
}

object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h, access_mask);
}

extern "C"
void trace_NtTerminateThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];
    auto exit_status = (NTSTATUS)args[1];

    print("NtTerminateThread: thread_handle = {}, exit_status = {}\n",
          thread_handle, exit_status);
}

extern "C"
NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status) {
    object_ref obj;

    TRACE2("NtTerminateThread", thread_handle, exit_status);

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        auto obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    t.exit_status = exit_status;
    terminate_thread(t, true);

    return nt_status::SUCCESS;
}

void print_thread_access_mask(ACCESS_MASK access_mask) {
    static constexpr pair<ACCESS_MASK, string_view> values[] = {
        { THREAD_ALL_ACCESS, "THREAD_ALL_ACCESS" },
        { THREAD_TERMINATE, "THREAD_TERMINATE" },
        { THREAD_SUSPEND_RESUME, "THREAD_SUSPEND_RESUME" },
        { THREAD_ALERT, "THREAD_ALERT" },
        { THREAD_GET_CONTEXT, "THREAD_GET_CONTEXT" },
        { THREAD_SET_CONTEXT, "THREAD_SET_CONTEXT" },
        { THREAD_SET_INFORMATION, "THREAD_SET_INFORMATION" },
        { THREAD_QUERY_INFORMATION, "THREAD_QUERY_INFORMATION" },
        { THREAD_SET_THREAD_TOKEN, "THREAD_SET_THREAD_TOKEN" },
        { THREAD_IMPERSONATE, "THREAD_IMPERSONATE" },
        { THREAD_DIRECT_IMPERSONATION, "THREAD_DIRECT_IMPERSONATION" },
        { THREAD_SET_LIMITED_INFORMATION, "THREAD_SET_LIMITED_INFORMATION" },
        { THREAD_QUERY_LIMITED_INFORMATION, "THREAD_QUERY_LIMITED_INFORMATION" },
        { THREAD_RESUME, "THREAD_RESUME" },
        { THREAD_RESERVED1, "THREAD_RESERVED1" },
        { THREAD_RESERVED2, "THREAD_RESERVED2" },
        { THREAD_RESERVED3, "THREAD_RESERVED3" },
        { DELETE, "DELETE" },
        { READ_CONTROL, "READ_CONTROL" },
        { WRITE_DAC, "WRITE_DAC" },
        { WRITE_OWNER, "WRITE_OWNER" },
        { SYNCHRONIZE, "SYNCHRONIZE" },
        { ACCESS_SYSTEM_SECURITY, "ACCESS_SYSTEM_SECURITY" },
        { MAXIMUM_ALLOWED, "MAXIMUM_ALLOWED" },
        { GENERIC_ALL, "GENERIC_ALL" },
        { GENERIC_EXECUTE, "GENERIC_EXECUTE" },
        { GENERIC_WRITE, "GENERIC_WRITE" },
        { GENERIC_READ, "GENERIC_READ" },
    };

    bool first = true;

    for (auto& v : values) {
        if ((access_mask & v.first) == v.first) {
            access_mask &= ~v.first;

            if (!first)
                print(" | ");

            print("{}", v.second);
            first = false;
        }
    }

    if (access_mask != 0) {
        if (!first)
            print(" | ");

        print("{:x}", access_mask);
    }

    if (first && access_mask == 0)
        print("0");
}

extern "C"
void trace_NtCreateThread(uintptr_t* args) {
    auto desired_access = (ACCESS_MASK)args[0];
    auto* object_attributes = (OBJECT_ATTRIBUTES*)args[1];
    auto process_handle = (HANDLE)args[2];
    auto& thread_context = *(CONTEXT*)args[3];
    auto create_suspended = (bool)args[4];

    start_deferred_print();

    print("NtCreateThread: desired_access = ");
    print_thread_access_mask(desired_access);
    print(", object_attributes = {}, process_handle = ", object_attributes);
    print_handle(process_handle);
    print(", thread_context = {:x}, create_suspended = {}\n",
          (uintptr_t)&thread_context, create_suspended ? "true" : "false");

    end_deferred_print();
}

extern "C"
NTSTATUS NtCreateThread(HANDLE* thread_handle, ACCESS_MASK desired_access,
                        const OBJECT_ATTRIBUTES* object_attributes, HANDLE process_handle,
                        CLIENT_ID* client_id, const CONTEXT* thread_context,
                        const INITIAL_TEB* initial_teb, bool create_suspended) {
    object_ref objrw;
    process* p;

    TRACE5("NtCreateThread", desired_access, object_attributes, process_handle,
           thread_context, create_suspended);

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->create_thread(*thread_handle, desired_access, object_attributes, *client_id,
                            *thread_context, *initial_teb, create_suspended);
}

extern "C"
void trace_NtResumeThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtResumeThread: thread_handle = ");
    print_handle(thread_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtResumeThread(HANDLE thread_handle, uint32_t* suspend_count) {
    object_ref objrw;
    thread* t;
    uint32_t count;

    TRACE1("NtResumeThread", thread_handle);

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    count = t->suspend_count.fetch_sub(1);

    if (suspend_count)
        *suspend_count = count;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtQueryInformationThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];
    auto info_class = (THREAD_INFORMATION_CLASS)args[1];
    auto buffer = (void*)args[2];
    auto buffer_size = (uint32_t)args[3];

    start_deferred_print();

    print("NtQueryInformationThread: thread_handle = ");
    print_handle(thread_handle);
    print(", info_class = {}, buffer = {:x}, buffer_size = {}\n",
          info_class, (uintptr_t)buffer, buffer_size);

    end_deferred_print();
}

extern "C"
NTSTATUS NtQueryInformationThread(HANDLE thread_handle,
                                  THREAD_INFORMATION_CLASS info_class,
                                  void* buffer, uint32_t buffer_size,
                                  uint32_t* return_length) {
    object_ref obj;

    TRACE4("NtQueryInformationThread", thread_handle, info_class, buffer,
           buffer_size);

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    return t.query_information(info_class, span((uint8_t*)buffer, buffer_size),
                               *return_length);
}

nt_status thread::query_signals_pending(span<uint8_t> buffer,
                                        uint32_t& return_length) {
    if (buffer.size() < sizeof(sigset_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto& s = *(sigset_t*)buffer.data();

    {
        lock_guard lock(proc->signals_lock);

        s = pending_signals | proc->pending_signals;
    }

    return_length = sizeof(sigset_t);

    return nt_status::SUCCESS;
}

nt_status thread::query_information(THREAD_INFORMATION_CLASS info_class,
                                    span<uint8_t> buffer, uint32_t& return_length) {
    switch (info_class) {
        using enum THREAD_INFORMATION_CLASS;

        case MoxnoxSignalsMask:
            return query_signals_mask(buffer, return_length);

        case MoxnoxSignalsPending:
            return query_signals_pending(buffer, return_length);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status thread::query_signals_mask(span<uint8_t> buffer, uint32_t& return_length) {
    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INTERNAL_ERROR;

    auto& set = *(sigset_t*)buffer.data();

    {
        lock_guard lock(proc->signals_lock);
        set = blocked_signals;
    }

    return_length = sizeof(sigset_t);

    return nt_status::SUCCESS;
}

static void kernel_thread_start(void (*func)(void*, void*), void* param1, void* param2) {
    func(param1, param2);

    disable_interrupts();

    auto& cb = *get_cpu_block();
    auto& t = *cb.current_thread;

    acquire_sync_lock();

    t.status = thread_status::dying;

#ifdef __aarch64__
    __asm __volatile ( // doesn't return, releases sync_lock
        "add x0, x18, #%0\n\t"
        "ldr x0, [x0]\n\t"
        "mov sp, x0\n\t"
        "b scheduler"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#else
    __asm __volatile ( // doesn't return, releases sync_lock
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#endif
}

object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2) {
    auto tptr = new thread;
    if (!tptr) {
        print("new failed\n");
        return {};
    }

    auto& t = *tptr;

    t.tid = next_tid.fetch_add(1);

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        print("allocate_stack failed\n");
        delete &t;
        return {};
    }

    memset(&t.context, 0, sizeof(t.context));

#ifdef __aarch64__
    t.context.Pc = (uintptr_t)kernel_thread_start;
    t.context.X0 = (uintptr_t)func;
    t.context.X1 = (uintptr_t)param1;
    t.context.X2 = (uintptr_t)param2;

    t.syscall_stack = (void*)((uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT) - 0x10);

    t.context.Cpsr = 0x5; // EL1 with SP_EL1
#elifdef __x86_64__
    memset(&t.xsave, 0, XSAVE_SIZE);

    t.context.Rip = (uintptr_t)kernel_thread_start;
    t.context.Rcx = (uintptr_t)func;
    t.context.Rdx = (uintptr_t)param1;
    t.context.R8 = (uintptr_t)param2;

    // 0x20 because of MS ABI, minus 8 for return address
    t.context.Rsp = (uintptr_t)((uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT) - 0x28);
    *(uint64_t*)t.context.Rsp = 0;

    t.context.EFlags = 0x202; // IF, reserved flag
    t.context.SegCs = kernel_CS;
    t.context.SegDs = kernel_DS;
    t.context.SegEs = kernel_DS;
    t.context.SegFs = kernel_DS;
    t.context.SegGs = kernel_DS;
    t.context.SegSs = kernel_DS;
#endif

    t.kernel_mode = true;

    t.teb = nullptr;

    t.status = thread_status::runnable;

    {
        sync_lock sl;
        lock_guard lock(sl);
        insert_tail_list(thread_list, t.le);
    }

    t.acquire(); // for active thread list

    return t;
}

extern "C"
void trace_NtSuspendThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtSuspendThread: thread_handle = ");
    print_handle(thread_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtSuspendThread(HANDLE thread_handle, uint32_t* previous_suspend_count) {
    object_ref objrw;
    thread* t;

    TRACE1("NtSuspendThread", thread_handle);

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    uint32_t count;

    // FIXME - send yield IPI if thread is currently running?

    count = t->suspend_count.fetch_add(1);

    if (previous_suspend_count)
        *previous_suspend_count = count;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtGetContextThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtGetContextThread: thread_handle = ");
    print_handle(thread_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtGetContextThread(HANDLE thread_handle, CONTEXT* context) {
    TRACE1("NtGetContextThread", thread_handle);

#ifdef __aarch64__ // FIXME
    (void)thread_handle;
    (void)context;

    return nt_status::NOT_IMPLEMENTED;
#else
    object_ref objrw;
    thread* t;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    // FIXME - wait until not in kernel mode

    {
        sync_lock sl;
        lock_guard lg(sl);

        if (t->kernel_mode)
            memcpy(context, &t->syscall_context, sizeof(CONTEXT));
        else
            memcpy(context, &t->context, sizeof(CONTEXT));
    }

    return nt_status::SUCCESS;
#endif
}

extern "C"
void trace_NtSetContextThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];
    auto& context = *(CONTEXT*)args[1];

    start_deferred_print();

    print("NtSetContextThread: thread_handle = ");
    print_handle(thread_handle);
    print(", context = {:x}\n", (uintptr_t)&context);

    end_deferred_print();
}

extern "C"
NTSTATUS NtSetContextThread(HANDLE thread_handle, const CONTEXT* context) {
    TRACE2("NtSetContextThread", thread_handle, context);

#ifdef __aarch64__ // FIXME
    (void)thread_handle;
    (void)context;

    return nt_status::NOT_IMPLEMENTED;
#else
    object_ref objrw;
    thread* t;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    // FIXME - look at ContextFlags to determine what to copy

    // if non-canonical, iretq will cause a kernel-mode GPF
    if (context->Rip & 0xffff800000000000)
        return nt_status::INVALID_PARAMETER;

    {
        sync_lock sl;
        lock_guard lg(sl);

        auto& dest = t->kernel_mode ? t->syscall_context : t->context;

        memcpy(&dest, context, sizeof(CONTEXT));

        dest.SegCs = user_CS;
        dest.SegDs = dest.SegEs = dest.SegFs = dest.SegSs = user_DS;
        dest.EFlags &= 0x40dd5; // whitelist: CF, PF, AF, ZF, SF, TF, DF, OF, AC
        dest.EFlags |= 0x200; // force IF on
    }

    return nt_status::SUCCESS;
#endif
}

void thread::break_apc_waits() {
    // holding sync_lock

    lock_guard lock(apc_lock);

    if (is_list_empty(pending_apcs))
        return;

    if ((status == thread_status::waiting || status == thread_status::waiting_multiple || status == thread_status::waiting_no_object) && wait_interruptible) {
        // break waits

        if (wait_timeout != 0)
            remove_entry_list(timeout_le);

        if (status == thread_status::waiting)
            remove_entry_list(wait_obj.wait_le);
        else if (status == thread_status::waiting_multiple) {
            auto& block = *multwait;

            for (unsigned int i = 0; i < block.num_entries; i++) {
                if (!block.entries[i].done)
                    remove_entry_list(block.entries[i].wait_le);
            }

            multwait = nullptr;
        }

        status = thread_status::runnable;
        woken_by_apc = true;
    }
}

nt_status thread::queue_apc(PS_APC_ROUTINE apc_routine, void* apc_arg1,
                            void* apc_arg2, void* apc_arg3) {
    auto p = new pending_apc;
    if (!p) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    p->routine = apc_routine;
    p->arg1 = (uintptr_t)apc_arg1;
    p->arg2 = (uintptr_t)apc_arg2;
    p->arg3 = (uintptr_t)apc_arg3;

    {
        lock_guard lock(apc_lock);

        insert_tail_list(pending_apcs, p->le);
    }

    {
        sync_lock sl;
        lock_guard lock1(sl);

        break_apc_waits();
    }

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtQueueApcThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];
    auto apc_routine = (PS_APC_ROUTINE)args[1];
    auto apc_arg1 = (void*)args[2];
    auto apc_arg2 = (void*)args[3];
    auto apc_arg3 = (void*)args[4];

    start_deferred_print();

    print("NtQueueApcThread: thread_handle = ");
    print_handle(thread_handle);
    print(", apc_routine = {:x}, apc_arg1 = {:x}, apc_arg2 = {:x}, apc_arg3 = {:x}\n",
          (uintptr_t)apc_routine, (uintptr_t)apc_arg1, (uintptr_t)apc_arg2,
          (uintptr_t)apc_arg3);

    end_deferred_print();
}

extern "C"
NTSTATUS NtQueueApcThread(HANDLE thread_handle, PS_APC_ROUTINE apc_routine,
                          void* apc_arg1, void* apc_arg2, void* apc_arg3) {
    object_ref obj;

    TRACE5("NtQueueApcThread", thread_handle, apc_routine, apc_arg1, apc_arg2,
           apc_arg3);

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    return t.queue_apc(apc_routine, apc_arg1, apc_arg2, apc_arg3);
}

static nt_status do_poll(span<const uint8_t> buffer) {
    nt_status status;
    uint32_t bytes_read, bytes_written;

    auto& t = *get_current_thread();

    if (buffer.size() < sizeof(MN_POLL))
        return nt_status::BUFFER_TOO_SMALL;

    auto& p = *(MN_POLL*)buffer.data();

    if (p.sigmask_set) {
        p.sigmask &= ((sigset_t)1 << LAST_SIG) - 1;

        sigset_t to_dispatch;

        {
            lock_guard lock(t.proc->signals_lock);

            t.blocked_signals = p.sigmask;

            to_dispatch = t.pending_signals & ~t.blocked_signals;
            t.pending_signals &= ~to_dispatch;

            auto from_proc = t.proc->pending_signals & ~t.blocked_signals;
            to_dispatch |= from_proc;
            t.proc->pending_signals &= ~from_proc;
        }

        if (to_dispatch != 0) {
            for (int i = 1; i <= LAST_SIG; i++) {
                if (to_dispatch & ((sigset_t)1 << (i - 1))) {
                    status = t.queue_apc((PS_APC_ROUTINE)(t.proc->ntdll_addr + mn_posix_signal),
                                         (void*)(uintptr_t)i, nullptr, nullptr);
                    if (!success(status))
                        return status;
                }
            }
        }
    }

    if (p.nfds == 0) {
        optional<uint64_t> tm;

        if (p.timeout != numeric_limits<uint64_t>::max())
            tm = p.timeout;

        return wait_no_object(true, tm);
    }

    if (!p.fds || p.nfds > PAGE_SIZE / sizeof(mn_poll_entry))
        return nt_status::INVALID_PARAMETER;

    auto entries = (mn_poll_entry*)malloc(sizeof(mn_poll_entry) * p.nfds);
    if (!entries) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = t.proc->read_vm(p.fds, entries, (uint32_t)(sizeof(mn_poll_entry) * p.nfds),
                             &bytes_read);
    if (!success(status)) {
        free(entries);
        return status;
    } else if (bytes_read != sizeof(mn_poll_entry) * p.nfds) {
        free(entries);
        return nt_status::NO_MEMORY;
    }

    auto objs = (object**)malloc(sizeof(object*) * p.nfds);
    if (!objs) {
        print("malloc failed\n");
        free(entries);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    size_t valid = 0, total_non_zero = 0;

    for (size_t i = 0; i < p.nfds; i++) {
        if (entries[i].h == 0) {
            objs[valid] = nullptr;
            valid++;
            continue;
        }

        // FIXME - check handles have SYNCHRONIZE access

        auto objrw = deref_handle(entries[i].h);
        if (!objrw) {
            entries[i].revents = POLLNVAL;
            continue;
        }

        if (objrw.get().type == object_type::file) {
            auto& f = static_cast<file&>(objrw.get());

            f.ino.acquire();

            object_ref objrw2{f.ino};

            swap(objrw, objrw2);
        }

        objrw.get().acquire();
        objs[valid] = &objrw.get();
        valid++;
        total_non_zero++;
    }

    if (valid != p.nfds) {
        for (size_t i = 0; i < valid; i++) {
            if (objs[i])
                object_release(*objs[i]);
        }

        free(objs);

        status = t.proc->write_vm(p.fds, entries, (uint32_t)(sizeof(mn_poll_entry) * p.nfds),
                                  &bytes_written);
        if (!success(status)) {
            free(entries);
            return status;
        } else if (bytes_written != sizeof(mn_poll_entry) * p.nfds) {
            free(entries);
            return nt_status::NO_MEMORY;
        }

        free(entries);

        return nt_status::SUCCESS;
    }

    // FIXME - POLLERR and POLLHUP

    if (total_non_zero == 0) {
        optional<uint64_t> timeout;

        if (p.timeout != numeric_limits<uint64_t>::max())
            timeout = p.timeout;

        auto ret = wait_no_object(true, timeout);

        free(objs);
        free(entries);

        return ret;
    }

    disable_interrupts();
    acquire_sync_lock();

    if (t.status == thread_status::dying) {
        release_sync_lock();
        enable_interrupts();

        for (size_t i = 0; i < p.nfds; i++) {
            if (objs[i])
                object_release(*objs[i]);
        }

        free(objs);
        free(entries);

        return nt_status::THREAD_IS_TERMINATING;
    }

    if (t.alerted) {
        t.alerted = false;
        release_sync_lock();
        enable_interrupts();

        for (size_t i = 0; i < p.nfds; i++) {
            if (objs[i])
                object_release(*objs[i]);
        }

        free(objs);
        free(entries);

        return nt_status::ALERTED;
    }

    {
        bool has_apcs;

        {
            lock_guard lock(t.apc_lock);
            has_apcs = !is_list_empty(t.pending_apcs);
        }

        if (has_apcs) {
            release_sync_lock();
            enable_interrupts();

            for (size_t i = 0; i < p.nfds; i++) {
                if (objs[i])
                    object_release(*objs[i]);
            }

            free(objs);
            free(entries);

            return nt_status::USER_APC;
        }
    }

    // FIXME - handle multiple wakeups at once

    for (size_t i = 0; i < p.nfds; i++) {
        bool signalled, write_signalled;
        uint16_t revents = 0;

        if (!objs[i])
            continue;

        auto& obj = *objs[i];

        if (object_is_sync_object(obj)) {
            auto& so = object_get_sync_object(obj);

            signalled = so.signalled;
        } else
            signalled = true;

        if (object_is_rw_sync_object(obj)) {
            auto& rwso = object_get_rw_sync_object(obj);

            write_signalled = rwso.write_signalled;
        } else
            write_signalled = true;

        if (signalled && entries[i].events & POLLIN)
            revents |= POLLIN;

        if (write_signalled && entries[i].events & POLLOUT)
            revents |= POLLOUT;

        if (revents != 0) {
            object_on_wake(obj);
            release_sync_lock();
            enable_interrupts();

            entries[i].revents = revents;

            for (size_t j = 0; j < p.nfds; j++) {
                if (objs[j])
                    object_release(*objs[j]);
            }

            free(objs);

            status = t.proc->write_vm(&p.fds[i], &entries[i], (uint32_t)sizeof(mn_poll_entry),
                                      &bytes_written);
            if (!success(status)) {
                free(entries);
                return status;
            } else if (bytes_written != sizeof(mn_poll_entry)) {
                free(entries);
                return nt_status::NO_MEMORY;
            }

            free(entries);

            return nt_status::SUCCESS;
        }
    }

    if (p.timeout == 0) {
        release_sync_lock();
        enable_interrupts();

        for (size_t i = 0; i < p.nfds; i++) {
            if (objs[i])
                object_release(*objs[i]);
        }

        free(objs);
        free(entries);

        return nt_status::TIMEOUT;
    }

    // do actual waiting

    size_t mwb_size = offsetof(multiple_wait_block, entries) + (total_non_zero * sizeof(waiter_object_multiple));
    auto mwb = (multiple_wait_block*)malloc(mwb_size);
    if (!mwb) {
        release_sync_lock();
        enable_interrupts();

        for (size_t i = 0; i < p.nfds; i++) {
            if (objs[i])
                object_release(*objs[i]);
        }

        free(objs);
        free(entries);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    mwb->t = &t;
    mwb->left = 1;
    mwb->num_entries = (unsigned int)total_non_zero;

    size_t j = 0;
    for (size_t i = 0; i < p.nfds; i++) {
        if (!objs[i])
            continue;

        auto& so = object_get_sync_object(*objs[i]);
        auto ent = new (&mwb->entries[j]) waiter_object_multiple;

        if (entries[i].events & (POLLIN | POLLOUT)) {
            ent->wait_type = 0;

            if (entries[i].events & POLLIN)
                ent->wait_type |= WAIT_TYPE_READING;

            if (entries[i].events & POLLOUT)
                ent->wait_type |= WAIT_TYPE_WRITING;

            insert_tail_list(so.waiters, ent->wait_le);
        } else
            ent->wait_le.next = ent->wait_le.prev = &ent->wait_le;

        ent->block = mwb;
        j++;
    }

    t.multwait = mwb;

    auto& cb = *get_cpu_block();

    if (t.status == thread_status::dying)
        t.kill_on_wake = true;

    t.status = thread_status::waiting_multiple;
    t.current_cpu = 0xffffffff;
    t.wait_interruptible = true;

    t.woken_by_alert = false;
    t.woken_by_apc = false;

    if (t.proc)
        t.proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

    active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);

    if (p.timeout == numeric_limits<uint64_t>::max())
        t.wait_timeout = 0;
    else {
        t.wait_timeout = p.timeout / (SYSTEM_TIME_RESOLUTION / TICK_RATE);

        if (p.timeout % (SYSTEM_TIME_RESOLUTION / TICK_RATE))
            t.wait_timeout++;

        insert_tail_list(thread_list_timeout, t.timeout_le);
    }

    yield(); // releases sync_lock

    if (t.woken_by_apc) {
        t.woken_by_apc = false;

        lock_guard lock(t.apc_lock);

        if (!is_list_empty(t.pending_apcs)) {
            free(mwb);

            for (size_t i = 0; i < p.nfds; i++) {
                if (objs[i])
                    object_release(*objs[i]);
            }

            free(objs);
            free(entries);

            return nt_status::USER_APC;
        }
    }

    unsigned int index, raw_index = mwb->index;
    wait_type_t wait_type = mwb->wait_type;

    free(mwb);

    if (total_non_zero == p.nfds)
        index = raw_index;
    else {
        unsigned int j = 0;
        for (unsigned int i = 0; i < p.nfds; i++) {
            if (objs[i]) {
                if (j == raw_index) {
                    index = i;
                    break;
                }

                j++;
            }
        }
    }

    status = nt_status::SUCCESS;

    if (t.kill_on_wake) {
        t.kill_on_wake = false;
        t.status = thread_status::dying;
        status = nt_status::THREAD_IS_TERMINATING;
    } else if (t.status == thread_status::dying)
        status = nt_status::THREAD_IS_TERMINATING;
    else if (t.suspend_count.load() != 0)
        status = nt_status::ALERTED;
    else if (t.alerted && t.woken_by_alert) {
        t.alerted = false;
        t.woken_by_alert = false;
        status = nt_status::ALERTED;
    } else if (p.timeout < numeric_limits<uint64_t>::max() && t.wait_timeout == 0)
        status = nt_status::TIMEOUT;

    if (status != nt_status::SUCCESS) {
        for (size_t i = 0; i < p.nfds; i++) {
            if (objs[i])
                object_release(*objs[i]);
        }

        free(objs);
        free(entries);

        return status;
    }

    entries[index].revents = 0;

    if (wait_type & WAIT_TYPE_READING && entries[index].events & POLLIN)
        entries[index].revents |= POLLIN;

    if (wait_type & WAIT_TYPE_WRITING && entries[index].events & POLLOUT)
        entries[index].revents |= POLLOUT;

    for (size_t i = 0; i < p.nfds; i++) {
        if (objs[i])
            object_release(*objs[i]);
    }

    free(objs);

    status = t.proc->write_vm(&p.fds[index], &entries[index],
                              (uint32_t)sizeof(mn_poll_entry), &bytes_written);
    if (!success(status)) {
        free(entries);
        return status;
    } else if (bytes_written != sizeof(mn_poll_entry)) {
        free(entries);
        return nt_status::NO_MEMORY;
    }

    free(entries);

    return nt_status::SUCCESS;
}

nt_status thread::set_information(THREAD_INFORMATION_CLASS info_class,
                                  span<const uint8_t> buffer) {
    switch (info_class) {
        using enum THREAD_INFORMATION_CLASS;

        case MoxnoxSetPendingSignal:
            return set_pending_signal(buffer);

        case MoxnoxBlockSignals:
            return block_signals(buffer);

        case MoxnoxUnblockSignals:
            return unblock_signals(buffer);

        case MoxnoxSignalsMask:
            return set_signals_mask(buffer);

        case MoxnoxWaitForSignal:
            return wait_for_signal(buffer);

        case MoxnoxPoll:
            return do_poll(buffer);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status thread::set_pending_signal(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(int))
        return nt_status::INVALID_PARAMETER;

    auto sig = *(int*)buffer.data();

    if (sig <= 0 || sig > LAST_SIG || sig == SIGKILL || sig == SIGSTOP)
        return nt_status::INVALID_PARAMETER;

    auto mask = (uint32_t)1 << (sig - 1);

    {
        lock_guard lock(proc->signals_lock);

        if (pending_signals & mask) // already pending
            return nt_status::SUCCESS;

        if (blocked_signals & mask) {
            pending_signals |= mask;
            return nt_status::SUCCESS;
        }
    }

    // not blocked, not already pending

    return queue_apc((PS_APC_ROUTINE)(proc->ntdll_addr + mn_posix_signal),
                     (void*)(uintptr_t)sig, nullptr, nullptr);
}

nt_status thread::block_signals(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INVALID_PARAMETER;

    auto set = *(sigset_t*)buffer.data();

    set &= ((sigset_t)1 << LAST_SIG) - 1;

    {
        lock_guard lock(proc->signals_lock);

        blocked_signals |= set;
    }

    return nt_status::SUCCESS;
}

nt_status thread::unblock_signals(span<const uint8_t> buffer) {
    nt_status status;

    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INVALID_PARAMETER;

    auto set = *(sigset_t*)buffer.data();

    set &= ((sigset_t)1 << LAST_SIG) - 1;

    sigset_t to_dispatch;

    {
        lock_guard lock(proc->signals_lock);

        blocked_signals &= ~set;

        to_dispatch = pending_signals & ~blocked_signals;
        pending_signals &= ~to_dispatch;

        auto from_proc = proc->pending_signals & ~blocked_signals;
        to_dispatch |= from_proc;
        proc->pending_signals &= ~from_proc;
    }

    if (to_dispatch != 0) {
        for (int i = 1; i <= LAST_SIG; i++) {
            if (to_dispatch & ((sigset_t)1 << (i - 1))) {
                status = queue_apc((PS_APC_ROUTINE)(proc->ntdll_addr + mn_posix_signal),
                                   (void*)(uintptr_t)i, nullptr, nullptr);
                if (!success(status))
                    return status;
            }
        }
    }

    return nt_status::SUCCESS;
}

nt_status thread::set_signals_mask(span<const uint8_t> buffer) {
    nt_status status;

    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INVALID_PARAMETER;

    auto set = *(sigset_t*)buffer.data();

    set &= ((sigset_t)1 << LAST_SIG) - 1;

    sigset_t to_dispatch;

    {
        lock_guard lock(proc->signals_lock);

        blocked_signals = set;

        to_dispatch = pending_signals & ~blocked_signals;
        pending_signals &= ~to_dispatch;

        auto from_proc = proc->pending_signals & ~blocked_signals;
        to_dispatch |= from_proc;
        proc->pending_signals &= ~from_proc;
    }

    if (to_dispatch != 0) {
        for (int i = 1; i <= LAST_SIG; i++) {
            if (to_dispatch & ((sigset_t)1 << (i - 1))) {
                status = queue_apc((PS_APC_ROUTINE)(proc->ntdll_addr + mn_posix_signal),
                                   (void*)(uintptr_t)i, nullptr, nullptr);
                if (!success(status))
                    return status;
            }
        }
    }

    return nt_status::SUCCESS;
}

nt_status thread::wait_for_signal(span<const uint8_t> buffer) {
    sig_waiter sw;

    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INVALID_PARAMETER;

    auto set = *(sigset_t*)buffer.data();
    auto& t = *get_current_thread();

    disable_interrupts();
    acquire_sync_lock();

    if (t.alerted) {
        t.alerted = false;
        release_sync_lock();
        enable_interrupts();
        return nt_status::ALERTED;
    }

    proc->signals_lock.lock();

    if (pending_signals & set) {
        auto u = pending_signals & set;
        auto sig = countr_zero(u) + 1;

        pending_signals &= ~((sigset_t)1 << (sig - 1));

        proc->signals_lock.unlock();
        release_sync_lock();
        enable_interrupts();

        return (nt_status)sig;
    }

    if (proc->pending_signals & set) {
        auto u = proc->pending_signals & set;
        auto sig = countr_zero(u) + 1;

        proc->pending_signals &= ~((sigset_t)1 << (sig - 1));

        proc->signals_lock.unlock();
        release_sync_lock();
        enable_interrupts();

        return (nt_status)sig;
    }

    sw.set = set;
    sw.t = &t;
    sw.sig = 0;
    insert_tail_list(proc->sig_waiters, sw.le);

    proc->signals_lock.unlock();

    if (t.status == thread_status::dying)
        t.kill_on_wake = true;

    t.status = thread_status::waiting_no_object;
    t.current_cpu = 0xffffffff;
    t.wait_interruptible = true;
    t.wait_timeout = 0;

    {
        auto& cb = *get_cpu_block();

        if (t.proc)
            t.proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

        active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);
    }

    t.woken_by_alert = false;
    t.woken_by_apc = false;

    yield();

    // after wait - sync_lock released and interrupts re-enabled

    {
        lock_guard lock(proc->signals_lock);

        if (sw.sig == 0) // woken up because interrupted, entry still on list
            remove_entry_list(sw.le);
    }

    if (t.status == thread_status::dying)
        return nt_status::THREAD_IS_TERMINATING;

    if (sw.sig != 0)
        return (nt_status)sw.sig;

    if (t.suspend_count.load() != 0)
        return nt_status::ALERTED;

    if (t.alerted && t.woken_by_alert) {
        t.alerted = false;
        t.woken_by_alert = false;
        return nt_status::ALERTED;
    }

    if (t.woken_by_apc) {
        t.woken_by_apc = false;

        lock_guard lock(t.apc_lock);

        if (!is_list_empty(t.pending_apcs))
            return nt_status::USER_APC;
    }

    // should be unreachable
    return nt_status::INTERNAL_ERROR;
}

extern "C"
void trace_NtSetInformationThread(uintptr_t* args) {
    auto thread_handle = (HANDLE)args[0];
    auto info_class = (THREAD_INFORMATION_CLASS)args[1];
    auto buffer = (void*)args[2];
    auto buffer_size = (uint32_t)args[3];

    start_deferred_print();

    print("NtSetInformationThread: thread_handle = ");
    print_handle(thread_handle);
    print(", info_class = {}, buffer = {:x}, buffer_size = {}\n",
          info_class, (uintptr_t)buffer, buffer_size);

    end_deferred_print();
}

extern "C"
NTSTATUS NtSetInformationThread(HANDLE thread_handle, THREAD_INFORMATION_CLASS info_class,
                                const void* buffer, uint32_t buffer_size) {
    object_ref obj;

    TRACE4("NtSetInformationThread", thread_handle, info_class, buffer,
           buffer_size);

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    return t.set_information(info_class, span((uint8_t*)buffer, buffer_size));
}

void thread::alert() {
    // must be holding sync_lock

    if (alerted)
        return;

    alerted = true;

    if ((status == thread_status::waiting || status == thread_status::waiting_multiple || status == thread_status::waiting_no_object) && wait_interruptible) {
        if (wait_timeout != 0)
            remove_entry_list(timeout_le);

        if (status == thread_status::waiting)
            remove_entry_list(wait_obj.wait_le);
        else if (status == thread_status::waiting_multiple) {
            auto& block = *multwait;

            for (unsigned int i = 0; i < block.num_entries; i++) {
                if (!block.entries[i].done)
                    remove_entry_list(block.entries[i].wait_le);
            }

            multwait = nullptr;
        }

        status = thread_status::runnable;
        woken_by_alert = true;
    }
}
