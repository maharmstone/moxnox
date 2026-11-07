// SPDX-License-Identifier: GPL-2.0-only
module;

#include <optional>
#include <mutex>
#include <signal.h>
#include "timer-asm.h"
#include "trace.h"

#ifdef __x86_64__
#include <x86intrin.h>
#endif

module timer;

import print;
import interrupts;
import thread;
import disable_interrupts;
import spinlock;
import handle;
import scheduler;
import acpica;
import trace;
import kernel_time_page;

#ifdef __x86_64__
import inout;
import apic;
#elifdef __aarch64__
import aarch64;
#endif

using namespace std;

#ifdef __aarch64__

static uint32_t CNTFRQ;

#elifdef __x86_64__

static const uint8_t timer_interrupt = 0x20;
static const uint8_t timer_ap_interrupt = 0x24;

static uint32_t apic_timer_freq;

static const uint32_t PIT_HZ = 1193182;

#endif

static const unsigned int TIMER_PERIOD_RESOLUTION = 1000; // milliseconds

#ifdef __x86_64__

void init_lapic_timer(bool ap) {
    if (ap) {
        auto timer_div = (volatile uint32_t*)&local_apic[0x3e0];
        *timer_div = 3; // 16
    }

    auto lvt_timer = (volatile uint32_t*)&local_apic[0x320];
    auto timer_initial = (volatile uint32_t*)&local_apic[0x380];

    if (ap)
        *lvt_timer = 0x20000 | timer_ap_interrupt; // periodic
    else
        *lvt_timer = 0x20000 | timer_interrupt; // periodic

    *timer_initial = apic_timer_freq / TICK_RATE;
}

static void pit_delay(uint16_t delay) {
    outb(0x43, 0x30); // initialize channel 0 of PIT
    outb(0x40, (uint8_t)(delay & 0xff)); // push lower byte of param
    outb(0x40, (uint8_t)(delay >> 8)); // push higher byte of param

    while (true) {
        outb(0x43, 0xe2); // check PIT status

        if (inb(0x40) & 0x80) // loop again if not yet signalled
            break;
    }
}
#endif

#ifdef __aarch64__

static optional<uint32_t> find_timer_irq() {
    ACPI_TABLE_HEADER* tbl;

    // FIXME - devicetree version

    auto status = AcpiGetTable((char*)ACPI_SIG_GTDT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for GTDT\n", status);
        return nullopt;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_GTDT)) {
        print("GTDT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_GTDT));
        return nullopt;
    }

    auto& gtdt = *(ACPI_TABLE_GTDT*)tbl;

    return gtdt.NonSecureEl1Interrupt;
}

bool init_timer() {
    disable_interrupts();

    if (auto opt = find_timer_irq(); !opt.has_value())
        return false;
    else
        timer_irq_no = *opt;

    // get hardware frequency
    __asm __volatile (
        "mrs %x[res], cntfrq_el0"
        : [res] "=r" (CNTFRQ)
    );

    hardware_tick_rate = CNTFRQ / TICK_RATE;

    // set tick rate and enable interrupt
    __asm __volatile (
        "msr cntp_tval_el0, %x[res]"
        "\n\tmov x0, #1"
        "\n\tmsr cntp_ctl_el0, x0"
        :
        : [res] "r" (hardware_tick_rate)
        : "x0"
    );

    // allow userspace to read timers
    ARM64_CNTKCTL cntkctl;

    cntkctl.value = cntkctl_el1_load();
    cntkctl.EL0VCTEN = 1;
    cntkctl_el1_store(cntkctl.value);

    clear_irq_pending_bit(timer_irq_no);
    enable_irq(timer_irq_no);

    return true;
}

#elifdef __x86_64__

bool init_timer() {
    disable_interrupts();

    // setup APIC timer

    auto lvt_timer = (volatile uint32_t*)&local_apic[0x320];
    *lvt_timer = 0xff; // one shot, vector ff (spurious interrupt)

    auto timer_div = (volatile uint32_t*)&local_apic[0x3e0];
    *timer_div = 3; // 16

    auto timer_initial = (volatile uint32_t*)&local_apic[0x380];
    *timer_initial = 0xffffffff;

    pit_delay(PIT_HZ / 100);

    *lvt_timer = 0x10000; // disable

    auto timer_current = (volatile uint32_t*)&local_apic[0x390];

    apic_timer_freq = (0xffffffff - *timer_current) * 100;

    add_idt_entry(timer_interrupt, (void*)timer_isr, false, 1, false);
    add_idt_entry(timer_ap_interrupt, (void*)timer_isr_ap, false, 1, false);

    init_lapic_timer(false);

    // measure RDTSC frequency

    uint32_t aux;

    auto r1 = __rdtscp(&aux);
    pit_delay(PIT_HZ / 100); // spin for 10ms
    auto r2 = __rdtscp(&aux);

    tsc_freq = (r2 - r1) * 100;

    // FIXME - cpuid 0x15 actually tells you the frequency (if it's supported)

    return true;
}

#endif

object_ref create_timer(bool autoreset) {
    auto tptr = new timer;
    if (!tptr) {
        print("new failed\n");
        return {};
    }

    auto& t = *tptr;
    t.autoreset = autoreset;

    return t;
}

extern "C"
void timer_expired(timer* tptr) {
    // interrupts disabled and sync_lock held

    auto& t = *tptr;

    if (t.apc_routine) {
        if (t.apc_thread->status != thread_status::dead && t.apc_thread->status != thread_status::dying) {
            auto p = new pending_apc;

            if (p) { // if we OOM, just drop the wakeup
                auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

                p->routine = (PS_APC_ROUTINE)(uintptr_t)t.apc_routine;
                p->arg1 = (uintptr_t)t.apc_context;
                p->arg2 = cur_time & 0xffffffff;
                p->arg3 = cur_time >> 32;

                {
                    lock_guard lock(t.apc_thread->apc_lock);

                    insert_tail_list(t.apc_thread->pending_apcs, p->le);
                }

                t.apc_thread->break_apc_waits();
            }
        }
    }

    if (t.timer_proc && !t.timer_proc->dying) {
        object_ref victim;

        set_pending_signal3(*t.timer_proc, SIGALRM, victim);

        if (victim) {
            auto& v = static_cast<::thread&>(victim.get());

            auto p = new pending_apc;

            if (p) { // if we OOM, just drop the signal
                p->routine = (PS_APC_ROUTINE)(t.timer_proc->ntdll_addr + mn_posix_signal);
                p->arg1 = (uintptr_t)SIGALRM;
                p->arg2 = 0;
                p->arg3 = 0;

                {
                    lock_guard lock(v.apc_lock);

                    insert_tail_list(v.pending_apcs, p->le);
                }

                v.break_apc_waits();
            }
        }
    }

    // signal waiting threads

    if (t.autoreset) {
        if (is_list_empty(t.waiters))
            t.signalled = true; // if autoreset, only signal if nothing waiting when fired
        else {
            list_entry tmp = { &tmp, &tmp };

            auto le = t.waiters.next;

            remove_entry_list(*le);
            insert_tail_list(tmp, *le);
            wake_up_waiters(tmp, WAIT_TYPE_READING);
        }
    } else {
        wake_up_waiters(t.waiters, WAIT_TYPE_READING);

        t.signalled = true;
    }

    if (!t.periodic) {
        t.active = false;
        remove_entry_list(t.le);
    } else {
        t.ticks_left = t.orig_ticks;
        t.signalled = false;
    }
}

void timer::on_wake() {
    if (autoreset)
        signalled = false;
}

timer::~timer() {
    if (apc_thread)
        apc_thread->release();

    if (timer_proc)
        timer_proc->release();

    if (!active)
        return;

    {
        sync_lock sl;
        lock_guard lock(sl);

        remove_entry_list(le);
    }
}

extern "C"
void trace_NtCreateTimer(uintptr_t* args) {
    auto timer_type = (TIMER_TYPE)args[0];

    print("NtCreateTimer: timer_type = {}\n", timer_type);
}

extern "C"
NTSTATUS NtCreateTimer(HANDLE* timer_handle, ACCESS_MASK, const OBJECT_ATTRIBUTES*,
                       TIMER_TYPE timer_type) {
    auto t = get_current_thread();
    auto p = t->proc;

    TRACE1("NtCreateTimer", timer_type);

    // FIXME - desired_access
    // FIXME - object_attributes

    if (timer_type != TIMER_TYPE::NotificationTimer && timer_type != TIMER_TYPE::SynchronizationTimer)
        return nt_status::INVALID_PARAMETER;

    auto timerh = create_timer(timer_type == TIMER_TYPE::SynchronizationTimer);

    if (!timerh)
        return nt_status::INTERNAL_ERROR;

    handle_t h;

    auto status = p->add_handle(timerh.get(), 0, true, h); // FIXME - access mask
    if (!success(status))
        return status;

    *timer_handle = h;

    return nt_status::SUCCESS;
}

NTSTATUS timer::set(int64_t due_time, PTIMER_APC_ROUTINE apc_routine,
                    void* apc_context, bool, int32_t period, bool* previous_state) {
    auto& t = *get_current_thread();
    uint64_t ticks;

    // FIXME - resume_timer

    if (due_time > 0) {
        // FIXME - handle positive due_time (system time)
        return nt_status::INVALID_PARAMETER;
    } else
        ticks = (uint64_t)((-due_time * TICK_RATE) / SYSTEM_TIME_RESOLUTION);

    if (ticks == 0)
        ticks = 1;

    ::thread* old_apc_thread = nullptr;

    {
        sync_lock sl;
        lock_guard lock(sl);

        if (previous_state)
            *previous_state = signalled;

        ticks_left = ticks;
        signalled = false;
        periodic = period != 0;

        if (periodic)
            orig_ticks = (unsigned int)((uint32_t)period * TICK_RATE) / TIMER_PERIOD_RESOLUTION;

        old_apc_thread = apc_thread;

        if (t.proc && apc_routine) {
            t.acquire();

            this->apc_thread = &t;
            this->apc_routine = apc_routine;
            this->apc_context = apc_context;
        } else {
            this->apc_thread = nullptr;
            this->apc_routine = nullptr;
        }

        if (!active) {
            insert_tail_list(active_timer_list, le);
            active = true;
        }
    }

    if (old_apc_thread)
        old_apc_thread->release();

    return nt_status::SUCCESS;
}

NTSTATUS timer::cancel(bool* current_state) {
    ::thread* old_apc_thread = nullptr;

    {
        sync_lock sl;
        lock_guard lock(sl);

        if (current_state)
            *current_state = signalled;

        ticks_left = 0;
        signalled = false;
        periodic = false;

        if (active) {
            remove_entry_list(le);
            active = false;
        }

        old_apc_thread = apc_thread;

        apc_thread = nullptr;
        apc_routine = nullptr;
    }

    if (old_apc_thread)
        old_apc_thread->release();

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtSetTimer(uintptr_t* args) {
    auto timer_handle = (HANDLE)args[0];
    auto due_time = (const LARGE_INTEGER*)args[1];
    auto timer_apc_routine = (PTIMER_APC_ROUTINE)args[2];
    auto timer_context = (void*)args[3];
    auto period = (int32_t)args[4];

    start_deferred_print();

    print("NtSetTimer: timer_handle = ");
    print_handle(timer_handle);
    print(", due_time = {}, timer_apc_routine = {:x}, timer_context = {:x}, period = {}\n",
          due_time, (uintptr_t)timer_apc_routine, (uintptr_t)timer_context,
          period);

    end_deferred_print();
}

extern "C"
NTSTATUS NtSetTimer(HANDLE timer_handle, const LARGE_INTEGER* due_time,
                    PTIMER_APC_ROUTINE timer_apc_routine, void* timer_context,
                    bool resume_timer, int32_t period, bool* previous_state) {
    TRACE5("NtSetTimer", timer_handle, due_time, timer_apc_routine,
           timer_context, period);

    auto objrw = deref_handle(timer_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::timer)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& t = static_cast<timer&>(obj);

    return t.set(due_time->QuadPart, timer_apc_routine, timer_context,
                 resume_timer, period, previous_state);
}

extern "C"
void trace_NtCancelTimer(uintptr_t* args) {
    auto timer_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtCancelTimer: timer_handle = ");
    print_handle(timer_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtCancelTimer(HANDLE timer_handle, bool* current_state) {
    TRACE1("NtCancelTimer", timer_handle);

    auto objrw = deref_handle(timer_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::timer)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& t = static_cast<timer&>(obj);

    return t.cancel(current_state);
}

extern "C"
void trace_NtDelayExecution(uintptr_t* args) {
    auto alertable = (bool)args[0];
    auto delay_interval = (const LARGE_INTEGER*)args[1];

    print("NtDelayExecution: alertable = {}, delay_interval = {}\n",
          alertable ? "true" : "false", delay_interval);
}

extern "C"
NTSTATUS NtDelayExecution(bool alertable, const LARGE_INTEGER* delay_interval) {
    optional<uint64_t> tm;

    TRACE2("NtDelayExecution", alertable, delay_interval);

    if (delay_interval) {
        if (delay_interval->QuadPart > 0) {
            print("FIXME - NtDelayExecution positive delay_interval\n");
            return nt_status::NOT_SUPPORTED;
        }

        tm = -delay_interval->QuadPart;
    }

    return wait_no_object(alertable, tm);
}

void msleep(unsigned int ms) {
    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("msleep::set_clock: create_timer failed\n");
        return;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    auto status = tim.set((int)ms * -ONE_MS_TICKS, nullptr, nullptr, false,
                          1, nullptr);
    if (!success(status)) {
        print("msleep: timer::set returned {}\n", status);
        return;
    }

    wait_for_object(tim, false);
}

void udelay(uint64_t us) {
#ifdef __x86_64__
    auto deadline = __rdtsc() + ((tsc_freq * us) / 1000000);

    while (true) {
        auto r = __rdtsc();

        if (r >= deadline)
            return;
    }
#elifdef __aarch64__
    auto deadline = cntvct_el0_load() + ((cntfrq_el0_load() * us) / 1000000);

    while (true) {
        auto r = cntvct_el0_load();

        if (r >= deadline)
            return;
    }
#endif
}

extern "C"
void trace_NtQueryTimer(uintptr_t* args) {
    auto handle = (HANDLE)args[0];
    auto info_class = (TIMER_INFORMATION_CLASS)args[1];
    auto buffer = (void*)args[2];
    auto buffer_length = (uint32_t)args[3];

    start_deferred_print();

    print("NtQueryTimer: handle = ");
    print_handle(handle);
    print(", info_class = {}, buffer = {:x}, buffer_length = {}\n",
          info_class, (uintptr_t)buffer, buffer_length);

    end_deferred_print();
}

extern "C"
nt_status NtQueryTimer(HANDLE handle, TIMER_INFORMATION_CLASS info_class,
                       void* buffer, uint32_t buffer_length,
                       uint32_t* return_length) {
    object_ref obj;

    TRACE4("NtQueryTimer", handle, info_class, buffer, buffer_length);

    obj = deref_handle(handle);
    if (!obj)
        return nt_status::INVALID_HANDLE;

    if (obj.get().type != object_type::timer)
        return nt_status::OBJECT_TYPE_MISMATCH;

    // FIXME - check access rights

    auto& t = static_cast<timer&>(obj.get());

    return t.query(info_class, span((uint8_t*)buffer, buffer_length),
                   *return_length);
}

nt_status timer::query(TIMER_INFORMATION_CLASS info_class, span<uint8_t> buffer,
                       uint32_t& return_length) {
    switch (info_class) {
        using enum TIMER_INFORMATION_CLASS;

        case BasicInformation: {
            if (buffer.size() < sizeof(TIMER_BASIC_INFORMATION))
                return nt_status::BUFFER_TOO_SMALL;

            auto& info = *(TIMER_BASIC_INFORMATION*)buffer.data();

            {
                sync_lock sl;
                lock_guard lock(sl);

                info.RemainingTime.QuadPart = ticks_left / (SYSTEM_TIME_RESOLUTION / TICK_RATE);
                info.TimerState = signalled;
            }

            return_length = sizeof(TIMER_BASIC_INFORMATION);

            return nt_status::SUCCESS;
        }

        default:
            return nt_status::INVALID_INFO_CLASS;
    }
}

extern "C"
void trace_NtSetSystemTime(uintptr_t* args) {
    auto& system_time = *(LARGE_INTEGER*)args[0];

    print("NtSetSystemTime: system_time = {}\n", system_time.QuadPart);
}

extern "C"
nt_status NtSetSystemTime(const LARGE_INTEGER* system_time,
                          LARGE_INTEGER* /*previous_time*/) {
    TRACE1("NtSetSystemTime", system_time);

    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}
