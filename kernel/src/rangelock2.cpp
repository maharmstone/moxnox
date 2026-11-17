// SPDX-License-Identifier: GPL-2.0-only
module;

#include "timer-asm.h"
#include <stdint.h>
#include "assert.h"

module rangelock;

import bsod;
import disable_interrupts;
import cpu_block;
import thread;
import scheduler;

static bool rlr_overlap(const rangelock_req& r1, const rangelock_req& r2) {
    if (r1.start >= r2.start && r1.start < r2.start + r2.length)
        return true;

    if (r1.start + r1.length > r2.start && r1.start + r1.length <= r2.start + r2.length)
        return true;

    if (r1.start < r2.start && r1.start + r1.length > r2.start + r2.length)
        return true;

    return false;
}

void rangelock_req::lock(bool kick) {
#ifdef DEBUG
    if (interrupts_disabled())
        bsod("calling rangelock_req::lock with interrupts disabled");

#ifdef __aarch64__
    {
        disable_interrupts();

        auto current_priority = get_interrupt_priority();

        enable_interrupts();

        if ((current_priority & 0xf0) != 0xf0)
            bsod_format("calling rangelock_req::lock with GICC_PMR of {:x}\n", current_priority);
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
        bsod("calling rangelock_req::lock when cr8 != 0");
#endif
#endif

    disable_interrupts();
    acquire_sync_lock();

    t = (uintptr_t)get_cpu_block()->current_thread;

    bool overlaps = false;

    for (auto le = rl.active.next; le != &rl.active; le = le->next) {
        auto& rlr = containing_record<rangelock_req>(le);

        if (rlr_overlap(rlr, *this) && (exclusive || rlr.exclusive)) {
            overlaps = true;
            break;
        }
    }

    // also wait if overlaps with any pending exclusive waiters
    if (!overlaps && !exclusive) {
        for (auto le = rl.waiting.next; le != &rl.waiting; le = le->next) {
            auto& rlr = containing_record<rangelock_req>(le);

            if (!rlr.exclusive)
                continue;

            if (rlr_overlap(rlr, *this)) {
                overlaps = true;
                break;
            }
        }
    }

    if (overlaps && kick) {
        for (auto le = rl.active.next; le != &rl.active; le = le->next) {
            auto& rlr = containing_record<rangelock_req>(le);

            if (rlr_overlap(rlr, *this) && (exclusive || rlr.exclusive))
                ((thread*)rlr.t)->alert();
        }
    }

    if (!overlaps) {
        insert_tail_list(rl.active, le);
        release_sync_lock();
        enable_interrupts();
        return;
    }

    insert_tail_list(rl.waiting, le);

    set_current_thread_waiting(false);
}

void rangelock_req::unlock() {
    disable_interrupts();
    acquire_sync_lock();

    remove_entry_list(le);

    list_entry waking = { &waking, &waking };
    list_entry* next;

    auto le = rl.waiting.next;
    while (le != &rl.waiting) {
        auto& rlr = containing_record<rangelock_req>(le);

        next = le->next;

        if (!rlr_overlap(rlr, *this)) {
            le = next;
            continue;
        }

        bool okay = true;

        // check no conflict with anything still in the active list

        for (auto le2 = rl.active.next; le2 != &rl.active; le2 = le2->next) {
            auto& rlr2 = containing_record<rangelock_req>(le2);

            if (rlr_overlap(rlr, rlr2) && (rlr.exclusive || rlr2.exclusive)) {
                okay = false;
                break;
            }
        }

        if (!okay) {
            le = next;
            continue;
        }

        // check no conflict with anything already moved to the waking list

        for (auto le2 = waking.next; le2 != &waking; le2 = le2->next) {
            auto& rlr2 = containing_record<rangelock_req>(le2);

            if (rlr_overlap(rlr, rlr2) && (rlr.exclusive || rlr2.exclusive)) {
                okay = false;
                break;
            }
        }

        if (!okay) {
            le = next;
            continue;
        }

        // check no conflict with any entry still ahead of it in the waiting list

        for (auto le2 = rl.waiting.next; le2 != le; le2 = le2->next) {
            auto& rlr2 = containing_record<rangelock_req>(le2);

            if (rlr_overlap(rlr, rlr2) && (rlr.exclusive || rlr2.exclusive)) {
                okay = false;
                break;
            }
        }

        if (!okay) {
            le = next;
            continue;
        }

        remove_entry_list(rlr.le);
        insert_tail_list(waking, rlr.le);

        le = next;
    }

    while (!is_list_empty(waking)) {
        auto& rlr = containing_record<rangelock_req>(waking.next);

        remove_entry_list(rlr.le);
        insert_tail_list(rl.active, rlr.le);

        auto t = (thread*)rlr.t;

        assert(t->status == thread_status::waiting);
        t->status = thread_status::runnable;
    }

    release_sync_lock();
    enable_interrupts();
}
