// SPDX-License-Identifier: GPL-2.0-only
module;

#include "context_offsets.h"
#include <atomic>
#include <stddef.h>

export module cpu_block;

import nt;
import thread;

#ifdef __x86_64__
import x86;
#endif

using namespace std;

export struct cpu_block {
    atomic<uint64_t> disable_preempt = 0; // must be first
#ifdef __x86_64__
    cpu_block* self;
#endif
    thread* current_thread;
    uint64_t time_slice_left;
    CONTEXT thread_context;
    uint32_t cpu_no;
    void* scheduler_stack;
#ifdef __aarch64__
    uintptr_t emergency_sp;
    uint64_t emergency_scratch[2];
    uint64_t gicc;
    uint64_t cpu_gicr;
#elifdef __x86_64__
    void* gdt;
    uint64_t kernel_cr3;
    uint64_t syscall_rax;
#endif
    char* printk_scratch;
    size_t deferred_print_size;
    unsigned int deferred_print = 0;
#ifdef __x86_64__
    xsave_area xsave;
#endif
};

static_assert(offsetof(cpu_block, current_thread) == GS_CURRENT_THREAD);
static_assert(offsetof(cpu_block, time_slice_left) == GS_TIME_SLICE_LEFT);
static_assert(offsetof(cpu_block, thread_context) == GS_CONTEXT);
static_assert(offsetof(cpu_block, cpu_no) == GS_CPU_NO);
static_assert(offsetof(cpu_block, scheduler_stack) == GS_SCHEDULER_STACK);
static_assert(offsetof(cpu_block, disable_preempt) == GS_DISABLE_PREEMPT);

#ifdef __aarch64__
static_assert(offsetof(cpu_block, emergency_sp) == GS_EMERGENCY_SP);
static_assert(offsetof(cpu_block, emergency_scratch) == GS_EMERGENCY_SCRATCH);
static_assert(offsetof(cpu_block, gicc) == GS_GICC);
#elifdef __x86_64__
static_assert(offsetof(cpu_block, self) == GS_SELF);
static_assert(offsetof(cpu_block, kernel_cr3) == GS_KERNEL_CR3);
static_assert(offsetof(cpu_block, syscall_rax) == GS_SYSCALL_RAX);
static_assert(offsetof(cpu_block, xsave) == GS_XSAVE);
#endif

#ifdef __aarch64__
export inline __attribute__((always_inline)) cpu_block* get_cpu_block() {
    uint64_t ret;

    __asm __volatile (
        "mov %x[res], x18"
        : [res] "=r" (ret)
        :
        : "memory"
    );

    return (cpu_block*)ret;
}
#elifdef __x86_64__
export inline __attribute__((always_inline)) cpu_block* get_cpu_block() {
#ifdef __llvm__
    return (cpu_block*)*(uint8_t* __attribute__((address_space(256)))*)offsetof(cpu_block, self);
#else
    cpu_block* ret;

    __asm("mov %0, gs:%1"
        : "=r" (ret)
        : "i" (offsetof(cpu_block, self))
        : "memory"
    );

    return ret;
#endif
}
#endif

export inline __attribute__((always_inline)) size_t cpu_block_size() {
#ifdef __x86_64__
    return offsetof(cpu_block, xsave) + XSAVE_SIZE;
#else
    return sizeof(cpu_block);
#endif
}

#ifdef __x86_64__
export bool avx512_supported = false;
#endif
