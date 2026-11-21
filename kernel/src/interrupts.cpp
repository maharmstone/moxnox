// SPDX-License-Identifier: GPL-2.0-only
module;

#include <atomic>
#include <stdint.h>

export module interrupts;

#ifdef __x86_64__
import x86;
#endif

using namespace std;

export extern "C++" {
    uint64_t tlb_va, tlb_num_pages;
    atomic<uint32_t> tlb_cpus_left;
    atomic<uint32_t> all_tlb_cpus_left;
}

export uintptr_t mn_signal, mn_handle_apc, mn_posix_signal;
export bool msi_supported = false;

export using irq_func_type = bool (*)(void*);

export bool add_irq_handler(uint32_t number, bool level, irq_func_type func,
                            void* ctx);
export void remove_irq_handler(uint32_t number, irq_func_type func,
                               void* ctx);
export bool init_interrupts();

#ifdef __aarch64__

export const uint8_t HALT_SGI = 0;
export const uint8_t TLB_SGI = 1;
export const uint8_t ALL_TLB_SGI = 2;

export const uint64_t MPIDR_MASK = 0xff00ffffff;
export uint64_t gicd, gicr;
export uint64_t msi_frame_setspi = 0;

export atomic<unsigned int> processors_left_to_halt;

export void clear_irq_pending_bit(uint32_t number);
export void enable_irq(uint32_t number);
export void __attribute__((noreturn)) halt_isr();
export uint16_t allocate_msi_irq();
export void free_msi_irq(uint16_t num);

export template<bool no_lock>
uint64_t init_interrupts_cpu(uint64_t mpidr);

export uint64_t find_cpu_gicr(uint64_t mpidr);

#elifdef __x86_64__

using isr_func_type = bool (*)(void*);

export const uint8_t halt_interrupt = 0x60;
export const uint8_t invalidate_tlb_interrupt = 0x21;
export const uint8_t invalidate_all_tlb_interrupt = 0x22;
export const uint8_t spurious_interrupt = 0xff;

export uint8_t add_isr(isr_func_type isr_func, void* ctx);
export void remove_isr(isr_func_type isr_func, void* ctx);
export bool init_apic();
export void add_idt_entry(uint8_t vector, void* func, bool trap, uint8_t ist,
                          bool user_mode);
export task_state_segment* init_tss();

#endif
