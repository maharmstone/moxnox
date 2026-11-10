// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <optional>
#include <atomic>
#include <mutex>
#include <stddef.h>
#include <poll.h>
#include "assert.h"
#include "timer-asm.h"
#include "trace.h"

#ifdef __x86_64__
#include <x86intrin.h>
#endif

module scheduler;

import bsod;
import print;
import mem;
import config;
import visit;
import thread;
import disable_interrupts;
import cpu_block;
import crt;
import power;
import interrupts;
import vfs;
import object_ref;
import trace;

#ifdef __aarch64__
import aarch64;
#endif

using namespace std;

static const uint64_t time_slice = 50; // ticks

extern "C" {
    volatile uint64_t sync_lock_val = 0;
}

static atomic<uint64_t> cpu_block_va = 0xfffffa3000000000;

extern "C" {
#ifdef DEBUG
    ::thread* sync_lock_thread = nullptr;
#endif
}

static ::thread* pick_runnable_thread(::thread* current) {
    list_entry* le;
    auto cb = get_cpu_block();
    auto mask = (uint64_t)1 << cb->cpu_no; // FIXME - is the local APIC ID guaranteed to start at 0?

    // FIXME - proper scheduling algorithm (currently just loops through runnable threads)

    auto sdc = shutting_down_cpu.load(memory_order::acquire);

    if (sdc != 0xffffffff && sdc != cb->cpu_no)
        return nullptr;

    auto consider = [&](::thread& t) -> ::thread* {
        if (t.status != thread_status::runnable && t.status != thread_status::dying)
            return nullptr;

        if (t.status == thread_status::dying && t.current_cpu != 0xffffffff)
            return nullptr;

        if (!(t.affinity & mask))
            return nullptr;

        if (!t.kernel_mode && t.suspend_count != 0)
            return nullptr;

        t.acquire();

        return &t;
    };

    if (current) { // loop through list from current thread
        assert(current->status != thread_status::dead);

        for (auto le = current->le.next; le != &thread_list; le = le->next) {
            auto& t = containing_record<::thread>(le);

            auto ret = consider(t);
            if (ret) {
                current->release();
                return ret;
            }
        }
    }

    le = thread_list.next;
    while (le != &thread_list) {
        auto& t = containing_record<::thread>(le);

        auto ret = consider(t);
        if (ret) {
            if (current)
                current->release();
            return ret;
        }

        if (&t == current)
            break;

        le = le->next;
    }

    if (current)
        current->release();

    return nullptr;
}

extern "C"
void scheduler() {
#ifdef DEBUG
#ifdef __aarch64__
    uint64_t daif = 0;

    __asm __volatile(
        "mrs %x[res], daif"
        : [res] "=r" (daif)
    );

    if (!daif)
        bsod("calling scheduler with interrupts enabled");

    {
        auto current_priority = get_interrupt_priority();

        if ((current_priority & 0xf0) != 0xf0)
            bsod_format("calling scheduler with GICC_PMR of {:x}\n", current_priority);
    }
#elifdef __x86_64__
    uint64_t rflags = 0;

    __asm __volatile(
        "pushf\n"
        "pop rax\n"
        "mov %0, rax\n"
        :
        : "m" (rflags)
        : "rax"
    );

    if (rflags & 0x200) // IF flag set
        bsod("calling scheduler with interrupts enabled");

    // FIXME - check cr8
#endif
#endif

    auto& cb = *get_cpu_block();

    // scheduler is called with sync_lock acquired

    if (cb.current_thread) {
        auto& t = *cb.current_thread;

        memcpy(&t.context, &cb.thread_context, sizeof(CONTEXT));
#ifdef __x86_64__
        memcpy(&t.xsave, &cb.xsave, XSAVE_SIZE);
#endif
        t.current_cpu = 0xffffffff;

        if (t.proc)
            t.proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

        active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);

#ifdef __x86_64__
        auto tsc = __rdtsc();
#elifdef __aarch64__
        auto tsc = cntvct_el0_load();
#endif

        t.time_running += tsc - t.last_scheduled_tsc;

        if (!t.kernel_mode)
            t.time_running_in_userspace += tsc - t.last_userspace_transition;

        if (t.kernel_mode && t.status == thread_status::dying && !t.proc) // kernel thread died
            thread_died(t, nullptr, false);

        if (t.status == thread_status::running)
            t.status = thread_status::runnable;
        else if (t.status == thread_status::dead) {
            t.release();
            cb.current_thread = nullptr;
        }
    }

    release_sync_lock();

    do {
        acquire_sync_lock();

        cb.current_thread = pick_runnable_thread(cb.current_thread);

        if (cb.current_thread) {
            assert(cb.current_thread->status == thread_status::runnable || cb.current_thread->status == thread_status::dying);
            assert(cb.current_thread->current_cpu == 0xffffffff);

            if (cb.current_thread->status == thread_status::runnable)
                cb.current_thread->status = thread_status::running;

            cb.current_thread->current_cpu = cb.cpu_no;

            if (cb.current_thread->proc)
                cb.current_thread->proc->running_cpus |= (uint64_t)1 << cb.cpu_no;

            active_cpus.fetch_or((uint64_t)1 << cb.cpu_no, memory_order::relaxed);
        }

        release_sync_lock();

        if (cb.current_thread) {
            auto& t = *cb.current_thread;

            memcpy(&cb.thread_context, &t.context, sizeof(CONTEXT));
#ifdef __x86_64__
            memcpy(&cb.xsave, &t.xsave, XSAVE_SIZE);
#endif
            cb.time_slice_left = time_slice;

#if 0
            // FIXME - what about AVX512 registers??

            if (t.proc && !t.kernel_mode) {
                bool has_apcs;

                {
                    lock_guard lock(t.apc_lock);
                    has_apcs = !is_list_empty(t.pending_apcs);
                }

                if (has_apcs) {
                    memcpy(&t.syscall_context, &cb.thread_context,
                           sizeof(CONTEXT));

                    enable_interrupts();
                    setup_user_apc();
                    disable_interrupts();

                    memcpy(&cb.thread_context, &t.syscall_context,
                           sizeof(CONTEXT));
                }
            }
#endif

#ifdef __x86_64__
            auto tsc = __rdtsc();
#elifdef __aarch64__
            auto tsc = cntvct_el0_load();
#endif

            t.last_scheduled_tsc = tsc;

            if (!t.kernel_mode)
                t.last_userspace_transition = tsc;

#ifdef __aarch64__
            __asm("b scheduler_restore");
#elifdef __x86_64__
            __asm("jmp scheduler_restore"); // iretq will re-enable interrupts
#endif
        }

        enable_interrupts();
#ifdef __aarch64__
        __asm __volatile("wfi");
#elifdef __x86_64__
        __asm __volatile("hlt");
#endif
        disable_interrupts();
    } while (true);
}

nt_status wait_for_object(object& obj, bool interruptible,
                          optional<uint64_t> timeout) {
#ifdef DEBUG
    if (interrupts_disabled())
        bsod("calling wait_for_object with interrupts disabled");

#ifdef __aarch64__
    {
        disable_interrupts();

        auto current_priority = get_interrupt_priority();

        enable_interrupts();

        if ((current_priority & 0xf0) != 0xf0)
            bsod_format("calling wait_for_object with GICC_PMR of {:x}\n", current_priority);
    }
#elifdef __x86_64__
    uint64_t cr8 = 0;

    __asm __volatile(
        "mov rax, cr8\n"
        "mov %0, rax\n"
        : "=m" (cr8)
        :
        : "rax"
    );

    if (cr8)
        bsod("calling wait_for_object when cr8 != 0");
#endif
#endif
    auto current_thread = get_current_thread();

    disable_interrupts();
    acquire_sync_lock();

    if (interruptible && current_thread->status == thread_status::dying) {
        release_sync_lock();
        enable_interrupts();
        return nt_status::THREAD_IS_TERMINATING;
    }

    {
        if (!object_is_sync_object(obj)) {
            release_sync_lock();
            enable_interrupts();
            // FIXME - exception
            print("Not a sync object\n");
            return nt_status::INVALID_PARAMETER;
        }

        auto& so = object_get_sync_object(obj);

        if (so.signalled) {
            object_on_wake(obj);
            release_sync_lock();
            enable_interrupts();

            return nt_status::SUCCESS;
        }

        if (interruptible) {
            bool has_apcs;

            {
                lock_guard lock(current_thread->apc_lock);
                has_apcs = !is_list_empty(current_thread->pending_apcs);
            }

            if (has_apcs) {
                release_sync_lock();
                enable_interrupts();
                return nt_status::USER_APC;
            }
        }

        if (timeout.has_value() && *timeout == 0) {
            release_sync_lock();
            enable_interrupts();
            return nt_status::TIMEOUT;
        }

        current_thread->wait_obj.wait_type = WAIT_TYPE_READING;

        insert_tail_list(so.waiters, current_thread->wait_obj.wait_le);

        auto& cb = *get_cpu_block();

        if (current_thread->status == thread_status::dying)
            current_thread->kill_on_wake = true;

        current_thread->status = thread_status::waiting;
        current_thread->current_cpu = 0xffffffff;
        current_thread->wait_interruptible = interruptible;

        if (current_thread->proc)
            current_thread->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

        active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);

        if (!timeout.has_value())
            current_thread->wait_timeout = 0;
        else {
            current_thread->wait_timeout = *timeout / (SYSTEM_TIME_RESOLUTION / TICK_RATE);

            if (*timeout % (SYSTEM_TIME_RESOLUTION / TICK_RATE))
                current_thread->wait_timeout++;

            insert_tail_list(thread_list_timeout, current_thread->timeout_le);
        }
    }

#ifdef __aarch64__
    __asm __volatile (
        "mov x0, lr"
        "\n\tbl yield"
        :
        :
        : "x0", "x1"
    );
#elifdef __x86_64__
    yield(); // releases sync_lock
#endif

    if (current_thread->kill_on_wake) {
        current_thread->kill_on_wake = false;
        current_thread->status = thread_status::dying;
        return nt_status::THREAD_IS_TERMINATING;
    }

    if (interruptible && current_thread->status == thread_status::dying)
        return nt_status::THREAD_IS_TERMINATING;

    if (interruptible && current_thread->suspend_count.load() != 0)
        return nt_status::ALERTED;

    if (interruptible) {
        lock_guard lock(current_thread->apc_lock);

        if (!is_list_empty(current_thread->pending_apcs))
            return nt_status::USER_APC;
    }

    if (timeout.has_value() && current_thread->wait_timeout == 0)
        return nt_status::TIMEOUT;

    return nt_status::SUCCESS;
}

unsigned int wake_up_waiters(list_entry& waiters, wait_type_t wait_type) {
    unsigned int ret = 0;

    auto le = waiters.next;

    while (le != &waiters) {
        auto& wo = *(waiter_object*)((uint8_t*)le - offsetof(waiter_object, wait_le));

        le = le->next;

        if (!(wait_type & wo.wait_type))
            continue;

        remove_entry_list(wo.wait_le);

        switch (wo.type) {
            case waiter_object_type::thread: {
                auto& t = *(::thread*)((uint8_t*)&wo - offsetof(::thread, wait_obj));

                assert(t.status == thread_status::waiting || t.status == thread_status::dying);

                if (t.wait_timeout != 0)
                    remove_entry_list(t.timeout_le);

                if (t.status != thread_status::dying)
                    t.status = thread_status::runnable;

                break;
            }

            case waiter_object_type::multiple: {
                auto& wom = static_cast<waiter_object_multiple&>(wo);
                auto& block = *wom.block;
                auto& t = *block.t;

                assert(t.status == thread_status::waiting_multiple || t.status == thread_status::dying);

                if (t.wait_timeout != 0)
                    remove_entry_list(t.timeout_le);

                wom.done = true;

                if (--block.left == 0) {
                    for (unsigned int i = 0; i < block.num_entries; i++) {
                        if (!block.entries[i].done)
                            remove_entry_list(block.entries[i].wait_le);
                        else if (&block.entries[i] == &wom) {
                            t.multwait->index = i;
                            t.multwait->wait_type = wait_type;
                        }
                    }

                    t.multwait = nullptr;

                    if (t.status != thread_status::dying)
                        t.status = thread_status::runnable;
                }

                break;
            }
        }

        ret++;
    }

    return ret;
}

extern "C"
void wait_timeout_expired(::thread* tptr) {
    // interrupts disabled and sync_lock held

    auto& t = *tptr;

    assert(t.status == thread_status::waiting || t.status == thread_status::waiting_multiple || t.status == thread_status::waiting_no_object);

    remove_entry_list(t.timeout_le);

    if (t.status == thread_status::waiting)
        remove_entry_list(t.wait_obj.wait_le);
    else if (t.status == thread_status::waiting_multiple) {
        auto& block = *t.multwait;

        for (unsigned int i = 0; i < block.num_entries; i++) {
            if (!block.entries[i].done)
                remove_entry_list(block.entries[i].wait_le);
        }

        t.multwait = nullptr;
    }

    t.status = thread_status::runnable;
}

#ifdef __aarch64__
template<bool no_lock>
uintptr_t allocate_emergency_stack() {
    auto pa = allocate_folio<no_lock>(0, page_state::used);
    if (!pa)
        return 0;

    return (uintptr_t)phys_address(pa) + PAGE_SIZE;
}
#endif

uintptr_t init_cpu_block(unsigned int cpu_no) {
    auto pages = bytes_to_pages(cpu_block_size());
    auto va = cpu_block_va.fetch_add(pages << PAGE_SHIFT);
    auto& cb = *(cpu_block*)va;

    for (unsigned int i = 0; i < pages; i++) {
        if (cpu_no == 0) {
            auto pa = allocate_folio<true>(0, page_state::used);
            if (!pa)
                return 0;

            if (!map_page<true>(pa, va, mapping_type::read_write)) {
                print("map_page failed\n");
                return 0;
            }
        } else {
            auto pa = allocate_page();
            if (!pa)
                return 0;

            if (!map_page(pa, va, mapping_type::read_write)) {
                print("map_page failed\n");
                return 0;
            }
        }

        va += PAGE_SIZE;
    }

    uint64_t printk_scratch_pa;

    if (cpu_no == 0)
        printk_scratch_pa = allocate_folio<true>(0, page_state::used);
    else
        printk_scratch_pa = allocate_page();

    if (!printk_scratch_pa)
        return 0;

    memset((void*)&cb, 0, sizeof(cb)); // avoid GCC warning

    new (&cb) cpu_block;

#ifdef __x86_64__
    cb.self = &cb;
#endif
    cb.current_thread = nullptr;
    cb.cpu_no = cpu_no;
    cb.printk_scratch = (char*)phys_address(printk_scratch_pa);

    void* stack;

    if (cpu_no == 0)
        stack = allocate_stack<true>();
    else
        stack = allocate_stack<false>();

    if (!stack)
        return 0;

    cb.scheduler_stack = (uint8_t*)stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT);

#ifdef __aarch64__
    cb.scheduler_stack = (uint8_t*)cb.scheduler_stack - 0x10;
#elifdef __x86_64__
    cb.scheduler_stack = (uint8_t*)cb.scheduler_stack - 0x20; // 0x20 for MS ABI
#endif

#ifdef __aarch64__
    if (cpu_no == 0)
        cb.emergency_sp = allocate_emergency_stack<true>();
    else
        cb.emergency_sp = allocate_emergency_stack<false>();
#endif

#ifdef __aarch64__
    if (cpu_no == 0) {
        __asm __volatile (
            "mov x18, %0\n\t"
            "msr tpidr_el1, x18\n\t"
            :
            : "r" (&cb)
        );
    }
#elifdef __x86_64__
    __asm __volatile (
        "mov rax, cr3"
        : "=a" (cb.kernel_cr3)
    );

    __asm __volatile(
        "mov ecx, 0xc0000101\n\t"
        "mov rax, %0\n\t"
        "mov rdx, %0\n\t"
        "shr rdx, 32\n\t"
        "wrmsr\n\t"
        :
        : "r" (&cb)
        : "ecx", "rax", "rdx"
    );
#endif

    return (uintptr_t)&cb;
}

nt_status setup_user_apc(uint64_t return_value) {
    auto& t = *get_current_thread();
    pending_apc* p;

    {
        lock_guard lock(t.apc_lock);

        assert(!is_list_empty(t.pending_apcs));

        p = &containing_record<pending_apc>(t.pending_apcs.next);
        remove_entry_list(p->le);
    }

#ifdef __x86_64__
    uintptr_t us_context = t.syscall_context.Rsp - sizeof(CONTEXT);
#elifdef __aarch64__
    uintptr_t us_context = t.syscall_context.Sp - sizeof(CONTEXT);
#endif

    us_context &= ~(uint64_t)(alignof(CONTEXT) - 1);

    assert(!(us_context & (alignof(CONTEXT) - 1)));

#ifdef __x86_64__
    t.syscall_context.Rax = return_value;
#elifdef __aarch64__
    t.syscall_context.X0 = return_value;
#endif

    if (!t.proc->copy_to((void*)us_context, &t.syscall_context, sizeof(CONTEXT))) {
        print("Unable to copy thread context to stack.\n");
        t.exit_status = nt_status::INSUFFICIENT_RESOURCES;
        terminate_thread(t, true);
        free(p);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

#ifdef __x86_64__
    auto us_arg3 = us_context - sizeof(uint64_t);

    us_arg3 -= sizeof(uint64_t); // add padding so stack is aligned correctly

    if (!t.proc->copy_to((void*)us_arg3, &p->arg3, sizeof(uint64_t))) {
        print("Unable to copy thread context to stack.\n");
        t.exit_status = nt_status::INSUFFICIENT_RESOURCES;
        terminate_thread(t, true);
        free(p);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    t.syscall_context.Rip = t.proc->ntdll_addr + mn_handle_apc;
    t.syscall_context.Rsp = us_arg3 - 0x20 - sizeof(uint64_t); // home space, plus (unused) return pointer

    t.syscall_context.Rcx = us_context;
    t.syscall_context.Rdx = (uintptr_t)p->routine;
    t.syscall_context.R8 = p->arg1;
    t.syscall_context.R9 = p->arg2;
#elifdef __aarch64__
    t.syscall_context.Pc = t.proc->ntdll_addr + mn_handle_apc;
    t.syscall_context.Sp = us_context;

    t.syscall_context.X0 = us_context;
    t.syscall_context.X1 = (uintptr_t)p->routine;
    t.syscall_context.X2 = p->arg1;
    t.syscall_context.X3 = p->arg2;
    t.syscall_context.X4 = p->arg3;
#endif

    free(p);

    return nt_status::USER_APC;
}

extern "C"
void trace_NtWaitForSingleObject(uintptr_t* args) {
    auto object_handle = (HANDLE)args[0];
    auto alertable = (bool)args[1];
    auto timeout = (const LARGE_INTEGER*)args[2];

    start_deferred_print();

    print("NtWaitForSingleObject: object_handle = ");
    print_handle(object_handle);
    print(", alertable = {}, timeout = {}\n", alertable ? "true" : "false",
          timeout);

    end_deferred_print();
}

extern "C" NTSTATUS NtWaitForSingleObject(HANDLE object_handle, bool alertable,
                               const LARGE_INTEGER* timeout) {
    TRACE3("NtWaitForSingleObject", object_handle, alertable, timeout);

    auto objrw = deref_handle(object_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - check handle has SYNCHRONIZE access

    if (objrw.get().type == object_type::file) {
        auto& f = static_cast<file&>(objrw.get());

        f.ino.acquire();

        object_ref objrw2{f.ino};

        swap(objrw, objrw2);
    }

    optional<uint64_t> tm;

    if (timeout) {
        if (timeout->QuadPart > 0) {
            print("FIXME - NtWaitForSingleObject positive timeout\n");
            return nt_status::NOT_SUPPORTED;
        }

        tm = -timeout->QuadPart;
    }

    return wait_for_object(objrw.get(), alertable, tm);
}

nt_status wait_for_objects(span<object*> objects, bool wait_any,
                           bool interruptible, optional<uint64_t> timeout) {
#ifdef DEBUG
    if (interrupts_disabled())
        bsod("calling wait_for_objects with interrupts disabled");

#ifdef __aarch64__
    {
        disable_interrupts();

        auto current_priority = get_interrupt_priority();

        enable_interrupts();

        if ((current_priority & 0xf0) != 0xf0)
            bsod_format("calling wait_for_objects with GICC_PMR of {:x}\n", current_priority);
    }
#elifdef __x86_64__
    uint64_t cr8 = 0;

    __asm __volatile(
        "mov rax, cr8\n"
        "mov %0, rax\n"
        : "=m" (cr8)
        :
        : "rax"
    );

    if (cr8)
        bsod("calling wait_for_objects when cr8 != 0");
#endif
#endif
    auto current_thread = get_current_thread();

    disable_interrupts();
    acquire_sync_lock();

    if (interruptible && current_thread->status == thread_status::dying) {
        release_sync_lock();
        enable_interrupts();
        return nt_status::THREAD_IS_TERMINATING;
    }

    if (wait_any) {
        for (unsigned int i = 0; i < objects.size(); i++) {
            auto& obj = *objects[i];
            auto& so = object_get_sync_object(obj);

            if (so.signalled) {
                object_on_wake(obj);
                release_sync_lock();
                enable_interrupts();

                return (enum nt_status)((unsigned int)nt_status::SUCCESS + i);
            }
        }
    } else {
        unsigned int signalled_count = 0;

        for (auto* obj : objects) {
            auto& so = object_get_sync_object(*obj);

            if (so.signalled)
                signalled_count++;
        }

        if (signalled_count == objects.size()) {
            for (auto* obj : objects) {
                object_on_wake(*obj);
            }

            release_sync_lock();
            enable_interrupts();

            return nt_status::SUCCESS;
        }
    }

    if (interruptible) {
        bool has_apcs;

        {
            lock_guard lock(current_thread->apc_lock);
            has_apcs = !is_list_empty(current_thread->pending_apcs);
        }

        if (has_apcs) {
            release_sync_lock();
            enable_interrupts();
            return nt_status::USER_APC;
        }
    }

    if (timeout.has_value() && *timeout == 0) {
        release_sync_lock();
        enable_interrupts();
        return nt_status::TIMEOUT;
    }

    // do actual waiting

    size_t mwb_size = offsetof(multiple_wait_block, entries) + (objects.size() * sizeof(waiter_object_multiple));
    auto mwb = (multiple_wait_block*)malloc(mwb_size); // FIXME - don't malloc with interrupts disabled!
    if (!mwb) {
        release_sync_lock();
        enable_interrupts();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    mwb->t = current_thread;
    mwb->left = wait_any ? 1 : (unsigned int)objects.size();
    mwb->num_entries = (unsigned int)objects.size();

    size_t j = 0;
    for (size_t i = 0; i < objects.size(); i++) {
        auto& so = object_get_sync_object(*objects[i]);

        if (!so.signalled) {
            auto ent = new (&mwb->entries[j]) waiter_object_multiple;

            ent->wait_type = WAIT_TYPE_READING;
            insert_tail_list(so.waiters, ent->wait_le);

            ent->block = mwb;
            j++;
        } else if (!wait_any) {
            object_on_wake(*objects[i]);
            mwb->left--;
            mwb->num_entries--;
        }
    }

    current_thread->multwait = mwb;

    auto& cb = *get_cpu_block();

    if (current_thread->status == thread_status::dying)
        current_thread->kill_on_wake = true;

    current_thread->status = thread_status::waiting_multiple;
    current_thread->current_cpu = 0xffffffff;
    current_thread->wait_interruptible = interruptible;

    if (current_thread->proc)
        current_thread->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

    active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);

    if (!timeout.has_value())
        current_thread->wait_timeout = 0;
    else {
        current_thread->wait_timeout = *timeout / (SYSTEM_TIME_RESOLUTION / TICK_RATE);

        if (*timeout % (SYSTEM_TIME_RESOLUTION / TICK_RATE))
            current_thread->wait_timeout++;

        insert_tail_list(thread_list_timeout, current_thread->timeout_le);
    }

    yield(); // releases sync_lock

    if (interruptible) {
        bool has_apcs;

        {
            lock_guard lock(current_thread->apc_lock);

            has_apcs = !is_list_empty(current_thread->pending_apcs);
        }

        if (has_apcs) {
            free(mwb);
            return nt_status::USER_APC;
        }
    }

    auto index = mwb->index;
    free(mwb);

    if (current_thread->kill_on_wake) {
        current_thread->kill_on_wake = false;
        current_thread->status = thread_status::dying;
        return nt_status::THREAD_IS_TERMINATING;
    }

    if (interruptible && current_thread->status == thread_status::dying)
        return nt_status::THREAD_IS_TERMINATING;

    if (interruptible && current_thread->suspend_count.load() != 0)
        return nt_status::ALERTED;

    if (timeout.has_value() && current_thread->wait_timeout == 0)
        return nt_status::TIMEOUT;

    if (!wait_any)
        return nt_status::SUCCESS;

    return (enum nt_status)((unsigned int)nt_status::SUCCESS + index);
}

extern "C"
void trace_NtWaitForMultipleObjects(uintptr_t* args) {
    auto object_count = (uint32_t)args[0];
    auto objects_array = (HANDLE*)args[1];
    auto wait_any = (bool)args[2];
    auto alertable = (bool)args[3];
    auto timeout = (const LARGE_INTEGER*)args[4];

    start_deferred_print();

    print("NtWaitForMultipleObjects: objects_array = [");

    for (uint32_t i = 0; i < object_count; i++) {
        if (i != 0)
            print(",");

        print_handle(objects_array[i]);
    }

    print("], wait_any = {}, alertable = {}, timeout = {}\n",
          wait_any ? "true" : "false", alertable ? "true" : "false",
          timeout);

    end_deferred_print();
}

extern "C" NTSTATUS NtWaitForMultipleObjects(uint32_t object_count, HANDLE* objects_array,
                                  bool wait_any, bool alertable,
                                  const LARGE_INTEGER* timeout) {
    if (object_count == 1)
        return NtWaitForSingleObject(objects_array[0], alertable, timeout);

    TRACE5("NtWaitForMultipleObjects", object_count, objects_array, wait_any,
           alertable, timeout);

    optional<uint64_t> t;

    if (timeout) {
        if (timeout->QuadPart > 0) {
            print("FIXME - NtWaitForMultipleObjects positive timeout\n");
            return nt_status::NOT_SUPPORTED;
        }

        t = -timeout->QuadPart;
    }

    array<object*, MAXIMUM_WAIT_OBJECTS> objs;

    for (unsigned int i = 0; i < object_count; i++) {
        // FIXME - check handles have SYNCHRONIZE access

        auto objrw = deref_handle(objects_array[i]);
        if (!objrw) {
            for (unsigned int j = 0; j < i; j++) {
                object_release(*objs[j]);
            }
            return nt_status::INVALID_HANDLE;
        }

        if (objrw.get().type == object_type::file) {
            auto& f = static_cast<file&>(objrw.get());

            f.ino.acquire();

            object_ref objrw2{f.ino};

            swap(objrw, objrw2);
        }

        if (!object_is_sync_object(objrw.get())) {
            for (unsigned int j = 0; j < i; j++) {
                object_release(*objs[j]);
            }
            return nt_status::INVALID_PARAMETER;
        }

        // if !wait_any, make sure that no duplicates (because of object_on_wake)
        if (!wait_any) {
            for (unsigned int j = 0; j < i; j++) {
                if (objs[j] == &objrw.get())
                    return nt_status::INVALID_PARAMETER;
            }
        }

        objrw.get().acquire();
        objs[i] = &objrw.get();
    }

    auto ret = wait_for_objects(span(objs.data(), object_count), wait_any,
                                alertable, t);

    for (unsigned int i = 0; i < object_count; i++) {
        object_release(*objs[i]);
    }

    return ret;
}

void set_current_thread_waiting(bool interruptible) {
    // must have synclock held

    auto& cb = *get_cpu_block();
    auto t = cb.current_thread;

    if (t->status == thread_status::dying)
        t->kill_on_wake = true;

    t->status = thread_status::waiting;
    t->current_cpu = 0xffffffff;
    t->wait_interruptible = interruptible;
    t->wait_timeout = 0;

    if (t->proc)
        t->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

    active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);

    yield();
}

nt_status wait_no_object(bool interruptible, optional<uint64_t> timeout) {
#ifdef DEBUG
    if (interrupts_disabled())
        bsod("calling wait_no_object with interrupts disabled");

#ifdef __aarch64__
    {
        disable_interrupts();

        auto current_priority = get_interrupt_priority();

        enable_interrupts();

        if ((current_priority & 0xf0) != 0xf0)
            bsod_format("calling wait_no_object with GICC_PMR of {:x}\n", current_priority);
    }
#elifdef __x86_64__
    uint64_t cr8 = 0;

    __asm __volatile(
        "mov rax, cr8\n"
        "mov %0, rax\n"
        : "=m" (cr8)
        :
        : "rax"
    );

    if (cr8)
        bsod("calling wait_no_object when cr8 != 0");
#endif
#endif
    auto current_thread = get_current_thread();

    disable_interrupts();
    acquire_sync_lock();

    if (interruptible && current_thread->status == thread_status::dying) {
        release_sync_lock();
        enable_interrupts();
        return nt_status::THREAD_IS_TERMINATING;
    }

    {
        if (interruptible) {
            bool has_apcs;

            {
                lock_guard lock(current_thread->apc_lock);
                has_apcs = !is_list_empty(current_thread->pending_apcs);
            }

            if (has_apcs) {
                release_sync_lock();
                enable_interrupts();
                return nt_status::USER_APC;
            }
        }

        if (timeout.has_value() && *timeout == 0) {
            release_sync_lock();
            enable_interrupts();
            return nt_status::TIMEOUT;
        }

        auto& cb = *get_cpu_block();

        if (current_thread->status == thread_status::dying)
            current_thread->kill_on_wake = true;

        current_thread->status = thread_status::waiting_no_object;
        current_thread->current_cpu = 0xffffffff;
        current_thread->wait_interruptible = interruptible;

        if (current_thread->proc)
            current_thread->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

        active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);

        if (!timeout.has_value())
            current_thread->wait_timeout = 0;
        else {
            current_thread->wait_timeout = *timeout / (SYSTEM_TIME_RESOLUTION / TICK_RATE);

            if (*timeout % (SYSTEM_TIME_RESOLUTION / TICK_RATE))
                current_thread->wait_timeout++;

            insert_tail_list(thread_list_timeout, current_thread->timeout_le);
        }
    }

#ifdef __aarch64__
    __asm __volatile (
        "mov x0, lr"
        "\n\tbl yield"
        :
        :
        : "x0", "x1"
    );
#elifdef __x86_64__
    yield(); // releases sync_lock
#endif

    if (current_thread->kill_on_wake) {
        current_thread->kill_on_wake = false;
        current_thread->status = thread_status::dying;
        return nt_status::THREAD_IS_TERMINATING;
    }

    if (interruptible && current_thread->status == thread_status::dying)
        return nt_status::THREAD_IS_TERMINATING;

    if (interruptible && current_thread->suspend_count.load() != 0)
        return nt_status::ALERTED;

    if (interruptible) {
        lock_guard lock(current_thread->apc_lock);

        if (!is_list_empty(current_thread->pending_apcs))
            return nt_status::USER_APC;
    }

    if (timeout.has_value() && current_thread->wait_timeout == 0)
        return nt_status::TIMEOUT;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtYieldExecution() {
    print("NtYieldExecution\n");
}

extern "C"
NTSTATUS NtYieldExecution() {
    TRACE0("NtYieldExecution");

    disable_interrupts();
    acquire_sync_lock();

    auto& cb = *get_cpu_block();
    auto t = cb.current_thread;

    if (t->proc)
        t->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

    active_cpus.fetch_and(~((uint64_t)1 << cb.cpu_no), memory_order::relaxed);

    yield(); // releases sync_lock

    return nt_status::SUCCESS;
}
