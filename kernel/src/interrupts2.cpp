// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <string_view>
#include <optional>
#include <mutex>
#include <source_location>
#include <string.h>
#include "assert.h"
#include "ints.h"
#include <stdint.h>

#ifdef __x86_64__
#include <x86intrin.h>
#endif

module interrupts;

#ifdef __aarch64__
import aarch64;
#elifdef __x86_64__
import apic;
#endif

import nt;
import format;
import print;
import bsod;
import mem;
import config;
import list;
import thread;
import disable_interrupts;
import cpu_block;
import spinlock;
import loaderblock;
import cmdline;
import acpica;
import trace;
import urandom;

using namespace std;

#ifdef __aarch64__

static const unsigned int MSI_TYPER = 0x08;
static const unsigned int MSI_SETSPI_NS = 0x40;

static uint64_t msi_frame = 0;
static uint16_t msi_base_spi;

static spinlock msi_alloc_lock;
static uint8_t msi_alloc[128]; // initialized to 0

#elifdef __x86_64__

// tlb.S
extern "C" void invalidate_tlb_isr();
extern "C" void invalidate_all_tlb_isr();

static const uint32_t CPUID_FEAT_EDX_APIC = 0x200;
static const uint32_t IA32_APIC_BASE_MSR = 0x1b;

static const uint16_t interrupt_cs = 0x8;

static auto idt = span((idt_entry*)0xfffffac000000000, 256);

static const uint8_t MIN_DYNAMIC_INTERRUPT = 0x30;

static spinlock isr_lock;

struct isr_func_info {
    isr_func_type func;
    void* ctx;
};

static isr_func_info isr_funcs[0x20];

#endif

static spinlock irq_lock;

struct irq {
    list_entry le;
    uint32_t number;
    bool level;
    spinlock lock;
    list_entry handlers = { &handlers, &handlers };
};

struct irq_handler_info {
    list_entry le;
    irq_func_type func;
    void* ctx;
};

static list_entry irqs = { &irqs, &irqs };

#ifdef __x86_64__

extern "C" void dynamic_isr_30();
extern "C" void dynamic_isr_31();
extern "C" void dynamic_isr_32();
extern "C" void dynamic_isr_33();
extern "C" void dynamic_isr_34();
extern "C" void dynamic_isr_35();
extern "C" void dynamic_isr_36();
extern "C" void dynamic_isr_37();
extern "C" void dynamic_isr_38();
extern "C" void dynamic_isr_39();
extern "C" void dynamic_isr_3a();
extern "C" void dynamic_isr_3b();
extern "C" void dynamic_isr_3c();
extern "C" void dynamic_isr_3d();
extern "C" void dynamic_isr_3e();
extern "C" void dynamic_isr_3f();
extern "C" void dynamic_isr_40();
extern "C" void dynamic_isr_41();
extern "C" void dynamic_isr_42();
extern "C" void dynamic_isr_43();
extern "C" void dynamic_isr_44();
extern "C" void dynamic_isr_45();
extern "C" void dynamic_isr_46();
extern "C" void dynamic_isr_47();
extern "C" void dynamic_isr_48();
extern "C" void dynamic_isr_49();
extern "C" void dynamic_isr_4a();
extern "C" void dynamic_isr_4b();
extern "C" void dynamic_isr_4c();
extern "C" void dynamic_isr_4d();
extern "C" void dynamic_isr_4e();
extern "C" void dynamic_isr_4f();

static bool check_cpu_for_apic() {
    uint32_t eax, ebx, ecx, edx;

    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (1)
    );

    if (!(edx & CPUID_FEAT_EDX_APIC)) {
        print("APIC disabled or not supported by CPU.\n");
        return false;
    }

    return true;
}

static inline uint64_t rdmsr(uint64_t msr) {
    uint32_t low, high;

    __asm __volatile (
        "rdmsr"
        : "=a" (low), "=d" (high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static pair<uint32_t, uint16_t> get_irq_gsi(span<const uint8_t> madt, uint32_t irq) {
    auto data = madt;

    // check source overrides in MADT

    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        if (h.Type == ACPI_MADT_TYPE_INTERRUPT_OVERRIDE && h.Length >= sizeof(ACPI_MADT_INTERRUPT_OVERRIDE)) {
            auto& ent = *(ACPI_MADT_INTERRUPT_OVERRIDE*)data.data();

            if (ent.Bus == 0 && ent.SourceIrq == irq)
                return { ent.GlobalIrq, ent.IntiFlags };
        }

        data = data.subspan(h.Length);
    }

    return { irq, 0 };
}

static ACPI_MADT_IO_APIC* find_ioapic(span<const uint8_t> madt, uint32_t gsi) {
    auto data = madt;

    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        if (h.Type == ACPI_MADT_TYPE_IO_APIC && h.Length >= sizeof(ACPI_MADT_IO_APIC)) {
            auto& ent = *(ACPI_MADT_IO_APIC*)data.data();

            if (gsi >= ent.GlobalIrqBase) {
                volatile uint32_t* IOREGSEL = (uint32_t*)phys_address(ent.Address);
                volatile uint32_t* IOREGWIN = (uint32_t*)phys_address(ent.Address + 0x10);

                auto read_reg = [&](uint32_t reg) -> uint32_t {
                    *IOREGSEL = reg;

                    return *IOREGWIN;
                };

                auto IOAPICVER = read_reg(1);
                auto max_entry = (IOAPICVER & 0xff0000) >> 16;

                if (gsi <= ent.GlobalIrqBase + max_entry)
                    return &ent;
            }
        }

        data = data.subspan(h.Length);
    }

    return nullptr;
}

uint8_t add_isr(isr_func_type isr_func, void* ctx) {
    lock_guard lock(isr_lock);

    auto sp = span(isr_funcs);

    for (uint8_t i = 0; i < sp.size(); i++) {
        if (!sp[i].func) {
            sp[i].func = isr_func;
            sp[i].ctx = ctx;
            return MIN_DYNAMIC_INTERRUPT + i;
        }
    }

    return 0;
}

// FIXME - we could remove the search if we add the int_num as a parameter instead
void remove_isr(isr_func_type isr_func, void* ctx) {
    lock_guard lock(isr_lock);

    auto sp = span(isr_funcs);

    for (uint8_t i = 0; i < sp.size(); i++) {
        if (sp[i].func == isr_func && sp[i].ctx == ctx) {
            sp[i].func = nullptr;
            break;
        }
    }
}

static bool irq_handler(void* ctx) {
    auto& i = *(irq*)ctx;

    {
        lock_guard lock(i.lock);

        for (auto le = i.handlers.next; le != &i.handlers; le = le->next) {
            auto& h = containing_record<irq_handler_info>(le);

            if (h.func(h.ctx))
                return true;
        }
    }

    return false;
}

static irq* find_irq(uint32_t number, uint32_t apic_id, bool level) {
    bool wrong_trigger = false;

    {
        lock_guard lock(irq_lock);

        for (auto le = irqs.next; le != &irqs; le = le->next) {
            auto& i = containing_record<irq>(le);

            if (i.number == number) {
                if (!!level != !!i.level) {
                    wrong_trigger = true;
                    break;
                }

                return &i;
            }
        }
    }

    if (wrong_trigger) {
        print("find_irq: trying to add {}-triggered IRQ {} but {}-triggered handler already set up",
              level ? "level" : "edge", number, level ? "edge" : "level");
        return nullptr;
    }

    ACPI_TABLE_HEADER* tbl;
    auto status = AcpiGetTable((char*)ACPI_SIG_MADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return nullptr;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MADT)) {
        print("MADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MADT));
        return nullptr;
    }

    span madt((const uint8_t*)tbl + sizeof(ACPI_TABLE_MADT),
              tbl->Length - sizeof(ACPI_TABLE_MADT));

    auto [gsi, inti_flags] = get_irq_gsi(madt, number);
    ACPI_MADT_IO_APIC* ioapic = nullptr;
    uint8_t vec = 0;

    // FIXME - double-dipping on spinlock

    auto i = new irq;
    if (!i) {
        print("new failed\n");
        return nullptr;
    }

    i->number = number;
    i->level = level;

    do {
        lock_guard lock(irq_lock);

        vec = add_isr(irq_handler, i);
        if (vec == 0)
            break;

        ioapic = find_ioapic(madt, gsi);

        if (!ioapic)
            break;

        auto ioapic_address = phys_address(ioapic->Address);

        volatile uint32_t* IOREGSEL = (uint32_t*)ioapic_address;
        volatile uint32_t* IOREGWIN = (uint32_t*)((uint8_t*)ioapic_address + 0x10);

        auto write_reg = [&](uint32_t reg, uint32_t val) {
            *IOREGSEL = reg;
            *IOREGWIN = val;
        };

        uint32_t redir = vec;

        if ((inti_flags & 0x3) == 3) // active-low
            redir |= 0x2000;

        if (((inti_flags >> 2) & 0x3) == 3) // level-triggered
            redir |= 0x8000;

        if (inti_flags == 0 && level) // fallback if no override entry
            redir |= 0x8000;

        write_reg(0x10 + ((gsi - ioapic->GlobalIrqBase) * 2), redir);
        write_reg(0x11 + ((gsi - ioapic->GlobalIrqBase) * 2), apic_id);

        insert_tail_list(irqs, i->le);
    } while (false);

    if (vec == 0) {
        print("add_isr failed\n");
        return nullptr;
    }

    if (!ioapic) { // print outside of lock
        print("Could not find I/O APIC for GSI {:x}.\n", gsi);
        // FIXME - free interrupt number
        return nullptr;
    }

    return i;
}

#endif

#ifdef __aarch64__

void clear_irq_pending_bit(uint32_t number) {
    auto& GICD_ICPENDR = *(volatile uint32_t*)(gicd + 0x280 + ((number / 32) * 4));
    GICD_ICPENDR = (uint32_t)1 << (number % 32);
}

void enable_irq(uint32_t number) {
    if (gicr && number < 32) { // PPIs and SGIs use GICR if GICv3
        disable_preempt();

        auto& cb = *get_cpu_block();

        auto& GICR_ISENABLER0 = *(volatile uint32_t*)(cb.cpu_gicr + 0x10100);
        GICR_ISENABLER0 = (uint32_t)1 << number;

        enable_preempt();
    } else {
        auto& GICD_ISENABLER = *(volatile uint32_t*)(gicd + 0x100 + ((number / 32) * 4));
        GICD_ISENABLER = (uint32_t)1 << (number % 32);
    }
}

static irq* find_irq(uint32_t number, bool level) {
    bool wrong_trigger = false;

    {
        lock_guard lock(irq_lock);

        for (auto le = irqs.next; le != &irqs; le = le->next) {
            auto& i = containing_record<irq>(le);

            if (i.number == number) {
                if (!!level != !!i.level) {
                    wrong_trigger = true;
                    break;
                }

                return &i;
            }
        }
    }

    if (wrong_trigger) {
        print("find_irq: trying to add {}-triggered IRQ {} but {}-triggered handler already set up",
              level ? "level" : "edge", number, level ? "edge" : "level");
        return nullptr;
    }

    auto i = new irq;
    if (!i) {
        print("new failed\n");
        return nullptr;
    }

    i->number = number;
    i->level = level;

    {
        lock_guard lock(irq_lock);

        for (auto le = irqs.next; le != &irqs; le = le->next) {
            auto& i2 = containing_record<irq>(le);

            if (i2.number == number) {
                delete i;
                return &i2;
            }
        }

        insert_tail_list(irqs, i->le);

        clear_irq_pending_bit(number);
    }

    return i;
}

static void set_irq_level_triggered(uint32_t number, bool level) {
    auto& GICD_ICFGR = *(volatile uint32_t*)(gicd + 0xc00 + ((number / 16) * 4));

    if (level)
        GICD_ICFGR &= ~(uint32_t)(1 << ((2 * (number % 16)) + 1));
    else
        GICD_ICFGR |= 1 << ((2 * (number % 16)) + 1);
}

bool add_irq_handler(uint32_t number, bool level, irq_func_type func,
                     void* ctx) {
    auto irq = find_irq(number, level);
    if (!irq)
        return false;

    auto info = new irq_handler_info;
    if (!info)
        return false;

    info->func = func;
    info->ctx = ctx;

    // FIXME - double-dipping on spinlock

    {
        lock_guard lock(irq->lock);
        insert_tail_list(irq->handlers, info->le);
    }

    enable_irq(number);
    set_irq_level_triggered(number, level);

    return true;
}

#elifdef __x86_64__

bool add_irq_handler(uint32_t number, bool level, irq_func_type func, void* ctx) {
    // FIXME - allow IRQs to be spread around CPUs
    auto apic_id = *(volatile uint32_t*)&local_apic[0x20];

    auto irq = find_irq(number, apic_id, level);
    if (!irq)
        return false;

    auto info = new irq_handler_info;
    if (!info)
        return false;

    info->func = func;
    info->ctx = ctx;

    {
        lock_guard lock(irq->lock);
        insert_tail_list(irq->handlers, info->le);
    }

    return true;
};

#endif

void remove_irq_handler(uint32_t /*number*/, irq_func_type /*func*/, void* /*ctx*/) {
    // FIXME
}

static void add_entropy() {
    uint64_t r;

#ifdef __x86_64__
    r = __rdtsc();
#elifdef __aarch64__
    r = cntvct_el0_load();
#endif

    add_randomness(span((uint8_t*)&r, sizeof(r)));
}

#ifdef __x86_64__

bool init_apic() {
    // FIXME - can we assume that nothing's disabled the APIC? (Check EFI specs?)

    if (!check_cpu_for_apic())
        return false;

    // disable 8259 PIC
    __asm __volatile (
        "mov al, 0xff\n"
        "out 0xa1, al\n"
        "out 0x21, al\n"
        :
        :
        : "al"
    );

    auto apic_base = rdmsr(IA32_APIC_BASE_MSR) & 0xfffff000;

    if (!map_page(apic_base, (uintptr_t)local_apic, mapping_type::no_cache)) {
        print("map_page failed for local APIC\n");
        return false;
    }

    auto spurious_interrupt_reg = (volatile uint32_t*)&local_apic[0xf0];

    *spurious_interrupt_reg = spurious_interrupt | 0x100;

    return true;
}

void add_idt_entry(uint8_t vector, void* func, bool trap, uint8_t ist,
                   bool user_mode) {
    auto& i = idt[vector];
    auto addr = (uintptr_t)func;

    i.offset_1 = addr & 0xffff;
    i.selector = interrupt_cs;
    i.ist = ist;
    i.present = 1;
    i.dpl = user_mode ? 3 : 0;
    i.zero1 = 0;

    if (trap)
        i.gate_type = 0xf; // trap gate
    else
        i.gate_type = 0xe; // interrupt gate

    i.offset_2 = (addr >> 16) & 0xffff;
    i.offset_3 = (uint32_t)(addr >> 32);
    i.zero2 = 0;
}

extern "C"
void handle_dynamic_isr(uint8_t int_num) {
    isr_func_type func;
    void* ctx;

    if (int_num < MIN_DYNAMIC_INTERRUPT)
        return;

    if ((uint8_t)(int_num - MIN_DYNAMIC_INTERRUPT) > sizeof(isr_funcs) / sizeof(isr_funcs[0]))
        return;

    {
        lock_guard lock(isr_lock);
        func = isr_funcs[int_num - MIN_DYNAMIC_INTERRUPT].func;
        ctx = isr_funcs[int_num - MIN_DYNAMIC_INTERRUPT].ctx;
    }

    if (!func)
        return;

    add_entropy();

    func(ctx);
}

static bool setup_idt() {
    auto idt_page = allocate_page();
    if (idt_page == 0) {
        print("Could not allocate page for IDT.\n");
        return false;
    }

    if (!map_page(idt_page, (uintptr_t)idt.data(), mapping_type::read_write)) {
        print("map_page failed for IDT\n");
        return false;
    }

    memset(idt.data(), 0, PAGE_SIZE);

    add_idt_entry(0x0, (void*)divide_by_zero_isr, false, 0, false);
    add_idt_entry(0x1, (void*)single_step_isr, false, 0, false);
    add_idt_entry(0x2, (void*)nmi_isr, false, 0, false);
    add_idt_entry(0x3, (void*)breakpoint_isr, false, 0, true);
    // FIXME - int 4 (overflow)
    // FIXME - int 5 (bound range exceeded)
    add_idt_entry(0x6, (void*)invalid_opcode_isr, false, 0, false);
    // FIXME - int 7 (device not available)
    add_idt_entry(0x8, (void*)double_fault_isr, false, 1, false);
    // FIXME - int 9 (coprocessor segment overrun)
    // FIXME - int a (invalid TSS)
    // FIXME - int b (segment not present)
    // FIXME - int c (stack-segment fault)
    add_idt_entry(0xd, (void*)general_protection_fault_isr, false, 0, false);
    add_idt_entry(0xe, (void*)page_fault_isr, false, 0, false);
    // FIXME - int 10 (x87 exception)
    // FIXME - int 11 (alignment check)
    // FIXME - int 12 (machine check)
    add_idt_entry(0x13, (void*)simd_exception_isr, false, 0, false);
    // FIXME - int 14 (virtualization exception)
    // FIXME - int 15 (control protection exception)

    add_idt_entry(0x30, (void*)dynamic_isr_30, false, 0, false);
    add_idt_entry(0x31, (void*)dynamic_isr_31, false, 0, false);
    add_idt_entry(0x32, (void*)dynamic_isr_32, false, 0, false);
    add_idt_entry(0x33, (void*)dynamic_isr_33, false, 0, false);
    add_idt_entry(0x34, (void*)dynamic_isr_34, false, 0, false);
    add_idt_entry(0x35, (void*)dynamic_isr_35, false, 0, false);
    add_idt_entry(0x36, (void*)dynamic_isr_36, false, 0, false);
    add_idt_entry(0x37, (void*)dynamic_isr_37, false, 0, false);
    add_idt_entry(0x38, (void*)dynamic_isr_38, false, 0, false);
    add_idt_entry(0x39, (void*)dynamic_isr_39, false, 0, false);
    add_idt_entry(0x3a, (void*)dynamic_isr_3a, false, 0, false);
    add_idt_entry(0x3b, (void*)dynamic_isr_3b, false, 0, false);
    add_idt_entry(0x3c, (void*)dynamic_isr_3c, false, 0, false);
    add_idt_entry(0x3d, (void*)dynamic_isr_3d, false, 0, false);
    add_idt_entry(0x3e, (void*)dynamic_isr_3e, false, 0, false);
    add_idt_entry(0x3f, (void*)dynamic_isr_3f, false, 0, false);
    add_idt_entry(0x40, (void*)dynamic_isr_40, false, 0, false);
    add_idt_entry(0x41, (void*)dynamic_isr_41, false, 0, false);
    add_idt_entry(0x42, (void*)dynamic_isr_42, false, 0, false);
    add_idt_entry(0x43, (void*)dynamic_isr_43, false, 0, false);
    add_idt_entry(0x44, (void*)dynamic_isr_44, false, 0, false);
    add_idt_entry(0x45, (void*)dynamic_isr_45, false, 0, false);
    add_idt_entry(0x46, (void*)dynamic_isr_46, false, 0, false);
    add_idt_entry(0x47, (void*)dynamic_isr_47, false, 0, false);
    add_idt_entry(0x48, (void*)dynamic_isr_48, false, 0, false);
    add_idt_entry(0x49, (void*)dynamic_isr_49, false, 0, false);
    add_idt_entry(0x4a, (void*)dynamic_isr_4a, false, 0, false);
    add_idt_entry(0x4b, (void*)dynamic_isr_4b, false, 0, false);
    add_idt_entry(0x4c, (void*)dynamic_isr_4c, false, 0, false);
    add_idt_entry(0x4d, (void*)dynamic_isr_4d, false, 0, false);
    add_idt_entry(0x4e, (void*)dynamic_isr_4e, false, 0, false);
    add_idt_entry(0x4f, (void*)dynamic_isr_4f, false, 0, false);

    add_idt_entry(halt_interrupt, (void*)halt_isr, false, 0, false);
    add_idt_entry(spurious_interrupt, (void*)spurious_isr, false, 0, false);
    add_idt_entry(yield_interrupt, (void*)yield_isr, false, 0, false);
    add_idt_entry(invalidate_tlb_interrupt, (void*)invalidate_tlb_isr, false,
                  0, false);
    add_idt_entry(invalidate_all_tlb_interrupt, (void*)invalidate_all_tlb_isr,
                  false, 0, false);

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idt_ptr;

    idt_ptr.base = (uintptr_t)idt.data();
    idt_ptr.limit = (uint16_t)(idt.size_bytes() - 1);

    __asm__ __volatile__ (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );

    return true;
}

task_state_segment* init_tss() {
    auto pa = allocate_page();
    if (!pa)
        return nullptr;

    auto& tss = *(task_state_segment*)phys_address(pa);
    memset(&tss, 0, sizeof(tss));

    auto stack = allocate_stack();
    if (!stack) {
        free_folio(pa);
        return nullptr;
    }

    tss.ist1 = (uintptr_t)((uint8_t*)stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT));

    stack = allocate_stack();
    if (!stack) {
        free_folio(pa);
        return nullptr;
    }

    tss.rsp0 = (uintptr_t)((uint8_t*)stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT));

    return &tss;
}

static bool setup_gdt(task_state_segment* tss) {
    auto& cb = *get_cpu_block();
    uint64_t pa;

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdt_ptr;

    pa = allocate_page();
    if (pa == 0)
        return false;

    cb.gdt = phys_address(pa);

    auto gdtarr = span((gdt_entry*)cb.gdt, 8);

    *(uint64_t*)&gdtarr[0] = 0;

    // code
    gdtarr[1].LimitLow = 0xffff;
    gdtarr[1].BaseLow = 0x0000;
    gdtarr[1].BaseMid = 0x00;
    gdtarr[1].Type = 0x1a;
    gdtarr[1].Dpl = 0;
    gdtarr[1].Pres = 1;
    gdtarr[1].LimitHi = 0xf;
    gdtarr[1].Sys = 0;
    gdtarr[1].Long = 1;
    gdtarr[1].Default_Big = 0;
    gdtarr[1].Granularity = 1;
    gdtarr[1].BaseHi = 0x00;

    // data
    gdtarr[2].LimitLow = 0xffff;
    gdtarr[2].BaseLow = 0x0000;
    gdtarr[2].BaseMid = 0x00;
    gdtarr[2].Type = 0x12;
    gdtarr[2].Dpl = 0;
    gdtarr[2].Pres = 1;
    gdtarr[2].LimitHi = 0xf;
    gdtarr[2].Sys = 0;
    gdtarr[2].Long = 1;
    gdtarr[2].Default_Big = 0;
    gdtarr[2].Granularity = 1;
    gdtarr[2].BaseHi = 0x00;

    // TSS
    gdtarr[3].LimitLow = (uint16_t)sizeof(task_state_segment);
    gdtarr[3].BaseLow = ((uintptr_t)tss & 0xffff);
    gdtarr[3].BaseMid = ((uintptr_t)tss >> 16) & 0xff;
    gdtarr[3].Type = 0x9;
    gdtarr[3].Dpl = 0;
    gdtarr[3].Pres = 1;
    gdtarr[3].LimitHi = 0x0;
    gdtarr[3].Sys = 0;
    gdtarr[3].Long = 0;
    gdtarr[3].Default_Big = 0;
    gdtarr[3].Granularity = 0;
    gdtarr[3].BaseHi = ((uintptr_t)tss >> 24) & 0xff;

    *(uint64_t*)&gdtarr[4] = (uintptr_t)tss >> 32;

    // FIXME - gdtarr[5] would be 32-bit user-mode code

    // user-mode data
    gdtarr[6].LimitLow = 0xffff;
    gdtarr[6].BaseLow = 0x0000;
    gdtarr[6].BaseMid = 0x00;
    gdtarr[6].Type = 0x12;
    gdtarr[6].Dpl = 3;
    gdtarr[6].Pres = 1;
    gdtarr[6].LimitHi = 0xf;
    gdtarr[6].Sys = 0;
    gdtarr[6].Long = 1;
    gdtarr[6].Default_Big = 0;
    gdtarr[6].Granularity = 1;
    gdtarr[6].BaseHi = 0x00;

    // user-mode code
    gdtarr[7].LimitLow = 0xffff;
    gdtarr[7].BaseLow = 0x0000;
    gdtarr[7].BaseMid = 0x00;
    gdtarr[7].Type = 0x1a;
    gdtarr[7].Dpl = 3;
    gdtarr[7].Pres = 1;
    gdtarr[7].LimitHi = 0xf;
    gdtarr[7].Sys = 0;
    gdtarr[7].Long = 1;
    gdtarr[7].Default_Big = 0;
    gdtarr[7].Granularity = 1;
    gdtarr[7].BaseHi = 0x00;

    gdt_ptr.limit = (uint16_t)(gdtarr.size_bytes() - 1);
    gdt_ptr.base = (uintptr_t)gdtarr.data();

    __asm__ __volatile__ (
        "lgdt %0\n"
        "push 0x8\n"
        "lea rax, [rip+label]\n"
        "push rax\n"
        "retfq\n"
        "label:\n"
        :
        : "m" (gdt_ptr)
        : "rax"
    );

    __asm__ __volatile__ (
        "mov ax, %0\n"
        "ltr ax\n"
        :
        : "i" (TSS_SEGMENT)
        : "ax"
    );

    return true;
}
#endif

#ifdef __aarch64__
static optional<uint64_t> find_gicc_address(uint64_t mpidr) {
    ACPI_TABLE_HEADER* tbl;
    optional<uint64_t> gicc_addr;

    // FIXME - devicetree version

    auto status = AcpiGetTable((char*)ACPI_SIG_MADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return nullopt;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MADT)) {
        print("MADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MADT));
        return nullopt;
    }

    span madt((const uint8_t*)tbl + sizeof(ACPI_TABLE_MADT),
              tbl->Length - sizeof(ACPI_TABLE_MADT));

    auto data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        switch (h.Type) {
            case ACPI_MADT_TYPE_GENERIC_INTERRUPT: {
                auto& ent = *(ACPI_MADT_GENERIC_INTERRUPT*)data.data();

                if ((ent.ArmMpidr & MPIDR_MASK) == mpidr && !gicc_addr.has_value())
                    gicc_addr = ent.BaseAddress;

                break;
            }
        }

        data = data.subspan(h.Length);
    }

    return gicc_addr;
}

static optional<pair<uint64_t, uint8_t>> find_gicd() {
    ACPI_TABLE_HEADER* tbl;

    // FIXME - devicetree version

    auto status = AcpiGetTable((char*)ACPI_SIG_MADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return nullopt;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MADT)) {
        print("MADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MADT));
        return nullopt;
    }

    span madt((const uint8_t*)tbl + sizeof(ACPI_TABLE_MADT),
              tbl->Length - sizeof(ACPI_TABLE_MADT));

    auto data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        switch (h.Type) {
            case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR: {
                auto& ent = *(ACPI_MADT_GENERIC_DISTRIBUTOR*)data.data();

                return make_pair(ent.BaseAddress, ent.Version);
            }
        }

        data = data.subspan(h.Length);
    }

    return nullopt;
}

static optional<pair<uint64_t, uint64_t>> find_gicr() {
    ACPI_TABLE_HEADER* tbl;

    // FIXME - devicetree version

    auto status = AcpiGetTable((char*)ACPI_SIG_MADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return nullopt;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MADT)) {
        print("MADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MADT));
        return nullopt;
    }

    span madt((const uint8_t*)tbl + sizeof(ACPI_TABLE_MADT),
              tbl->Length - sizeof(ACPI_TABLE_MADT));

    auto data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        switch (h.Type) {
            case ACPI_MADT_TYPE_GENERIC_REDISTRIBUTOR: {
                auto& ent = *(ACPI_MADT_GENERIC_REDISTRIBUTOR*)data.data();

                return make_pair(ent.BaseAddress, ent.Length);
            }
        }

        data = data.subspan(h.Length);
    }

    return nullopt;
}

template<bool no_lock>
uint64_t init_interrupts_cpu(uint64_t mpidr) {
    auto gicc_addr = find_gicc_address(mpidr);
    if (!gicc_addr.has_value()) {
        print("unable to find address of GIC controller\n");
        return 0;
    }

    auto va = allocate_va<no_lock>(1);
    if (!va) {
        print("allocate_va failed\n");
        return 0;
    }

    if (!map_page<no_lock>(*gicc_addr, va, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(va, 1);
        return 0;
    }

    return va;
}

template uint64_t init_interrupts_cpu<false>(uint64_t);

static optional<uint64_t> find_msi_frame_address() {
    ACPI_TABLE_HEADER* tbl;
    optional<uint64_t> msi_frame_pa;

    // FIXME - devicetree version

    auto status = AcpiGetTable((char*)ACPI_SIG_MADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return nullopt;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MADT)) {
        print("MADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MADT));
        return nullopt;
    }

    span madt((const uint8_t*)tbl + sizeof(ACPI_TABLE_MADT),
              tbl->Length - sizeof(ACPI_TABLE_MADT));

    auto data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        switch (h.Type) {
            case ACPI_MADT_TYPE_GENERIC_MSI_FRAME:
                if (!msi_frame_pa.has_value()) {
                    auto& ent = *(ACPI_MADT_GENERIC_MSI_FRAME*)data.data();

                    msi_frame_pa = ent.BaseAddress;
                }
                break;
        }

        data = data.subspan(h.Length);
    }

    return msi_frame_pa;
}

static bool init_interrupts_global(uint64_t mpidr) {
    __asm __volatile (
        "msr vbar_el1, %x[res]"
        :
        : [res] "r" (&vector_table_bsp)
    );

    auto gicd_pair = find_gicd();
    if (!gicd_pair.has_value()) {
        print("unable to find address of GIC distributor\n");
        return false;
    }

    auto gicd_addr = gicd_pair->first;

    gic_ver = gicd_pair->second;

    if (gic_ver != 2 && gic_ver != 3)
        bsod_format("unexpected GIC version {}", gic_ver);

    auto va = allocate_va<true>(1);
    if (!va) {
        print("allocate_va failed\n");
        return false;
    }

    if (!map_page<true>(gicd_addr, va, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(va, 1);
        return false;
    }

    gicd = va;

    ARM64_GICD_CTLR ctlr;

    auto& GICD_CTLR = *(volatile ARM64_GICD_CTLR*)gicd;

    ctlr.value = GICD_CTLR.value;

    // enable GICD
    ctlr.EnableGrp0 = 1;
    ctlr.EnableGrp1NS = 1;

    GICD_CTLR.value = ctlr.value;

    bool nomsi = false;
    loop_through_cmdline([&](string_view s) {
        if (s == "nomsi") {
            nomsi = true;
            return false;
        }

        return true;
    });

    if (gic_ver > 2) {
        auto gicr_pair = find_gicr();
        if (!gicr_pair.has_value()) {
            print("unable to find address of GIC redistributor\n");
            return false;
        }

        auto [gicr_addr, gicr_size] = *gicr_pair;

        auto num_pages = bytes_to_pages(gicr_size);

        auto va = allocate_va<true>(num_pages);
        if (!va) {
            print("allocate_va failed\n");
            return false;
        }

        for (unsigned int i = 0; i < num_pages; i++) {
            if (!map_page<true>(gicr_addr + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT),
                                mapping_type::no_cache)) {
                print("map_page failed\n");
                free_va(va, num_pages);
                return false;
            }
        }

        gicr = va;

        auto& cb = *get_cpu_block();

        cb.cpu_gicr = find_cpu_gicr(mpidr);
        if (!cb.cpu_gicr) {
            print("find_cpu_gicr failed\n");
            return false;
        }

        // put all SGIs and PPIs in group 1
        auto& GICR_IGROUPR0 = *(volatile uint32_t*)(cb.cpu_gicr + 0x10080);
        GICR_IGROUPR0 = 0xffffffff;

        // enable group 1 interrupts
        __asm __volatile (
            "msr ICC_IGRPEN1_EL1, %x[val]"
            :
            : [val] "r" (1)
        );

        enable_irq(HALT_SGI);
        enable_irq(TLB_SGI);
        enable_irq(ALL_TLB_SGI);
    } else { // gic_ver == 2
        if (!nomsi) {
            auto msi_frame_pa = find_msi_frame_address();

            if (msi_frame_pa.has_value()) {
                {
                    auto va = allocate_va<true>(1);
                    if (!va) {
                        print("allocate_va failed\n");
                        return false;
                    }

                    if (!map_page<true>(*msi_frame_pa, va, mapping_type::no_cache)) {
                        print("map_page failed\n");
                        free_va(va, 1);
                        return false;
                    }

                    msi_frame = va;
                }

                msi_frame_setspi = *msi_frame_pa + MSI_SETSPI_NS;

                ARM64_GICv2m_MSI_TYPER t;

                t.value = *(volatile uint32_t*)(msi_frame + MSI_TYPER);

                msi_base_spi = t.base_spi;

                auto num_spis = t.num_spis;
                auto ptr = msi_alloc;

                while (num_spis >= 8) {
                    *ptr = 0xff;

                    ptr++;
                    num_spis -= 8;
                }

                if (num_spis != 0)
                    *ptr = (uint8_t)((1 << num_spis) - 1);
            }
        }
    }

    return true;
}

uint64_t find_cpu_gicr(uint64_t mpidr) {
    uint64_t mpidr2 = (mpidr & 0xffffff) | ((mpidr >> 8) & 0xff000000);
    uint64_t cpu_gicr = gicr;

    // FIXME - different for GICv4
    // FIXME - in theory could have multiple GICR regions

    while (true) {
        auto& GICR_TYPER = *(volatile uint64_t*)(cpu_gicr + 0x8);

        if (GICR_TYPER >> 32 == mpidr2)
            return cpu_gicr;

        if (GICR_TYPER & 0x10) // Last
            break;

        cpu_gicr += 0x20000;
    }

    return 0;
}

bool init_interrupts() {
    uint64_t mpidr;

    __asm __volatile (
        "mrs %x[res], mpidr_el1"
        : [res] "=r" (mpidr)
    );

    mpidr &= MPIDR_MASK;

    if (!init_interrupts_global(mpidr))
        return false;

    if (gic_ver == 2) {
        auto gicc = init_interrupts_cpu<true>(mpidr);
        if (!gicc)
            return false;

        auto& cb = *get_cpu_block();
        cb.gicc = gicc;

        auto& GICC_CTLR = *(volatile ARM64_GICC_CTLR*)cb.gicc;
        auto& GICC_PMR = *(volatile ARM64_GICC_PMR*)(cb.gicc + 0x4);

        GICC_CTLR.EnableGrp0 = 1; // enable GICC

        // allow all interrupts through
        GICC_PMR.Priority = 0xff;
    } else {
        // allow all interrupts through
        __asm __volatile (
            "msr ICC_PMR_EL1, %x[val]\n"
            :
            : [val] "r" (0xff)
            : "x0"
        );
    }

    return true;
}

#elifdef __x86_64__
bool init_interrupts() {
    if (!setup_idt())
        return false;

    auto tss = init_tss();
    if (!tss)
        return false;

    if (!setup_gdt(tss))
        return false;

    bool nomsi = false;
    loop_through_cmdline([&](string_view s) {
        if (s == "nomsi") {
            nomsi = true;
            return false;
        }

        return true;
    });

    msi_supported = !nomsi;

    return true;
}
#endif

static bool send_signal(const EXCEPTION_RECORD& er, CONTEXT& ctx,
                        nt_status exit_status) {
    auto& current_thread = *get_current_thread();
    auto& p = *current_thread.proc;

#ifdef __aarch64__
    uintptr_t us_context = ctx.Sp - sizeof(CONTEXT) - 0x10;
#elifdef __x86_64__
    uintptr_t us_context = ctx.Rsp - sizeof(CONTEXT);
#endif

    us_context &= ~(uint64_t)(alignof(CONTEXT) - 1);

    assert(!(us_context & (alignof(CONTEXT) - 1)));

    if (!p.copy_to((void*)us_context, &ctx, sizeof(CONTEXT))) {
        print("Unable to copy thread context to stack.\n");
        current_thread.exit_status = exit_status;
        terminate_thread(current_thread, true);
        return false;
    }

    size_t er_size = offsetof(EXCEPTION_RECORD, ExceptionInformation) + (sizeof(uintptr_t) * er.NumberParameters);
    uintptr_t us_er = us_context - er_size;

    us_er &= ~(uint64_t)0xf;

    assert(!(us_er & 0xf));

    if (!p.copy_to((void*)us_er, &er, er_size)) {
        print("Unable to copy exception record to stack.\n");
        current_thread.exit_status = exit_status;
        terminate_thread(current_thread, true);
        return false;
    }

#ifdef __aarch64__
    ctx.Pc = p.ntdll_addr + mn_signal;
    ctx.X0 = us_er;
    ctx.X1 = us_context;
    ctx.Sp = us_er;
#elifdef __x86_64__
    disable_interrupts();

    auto& cb = *get_cpu_block();

    memcpy(&cb.thread_context, &ctx, sizeof(CONTEXT));

    cb.thread_context.Rip = p.ntdll_addr + mn_signal;
    cb.thread_context.Rcx = us_er;
    cb.thread_context.Rdx = us_context;
    cb.thread_context.Rsp = us_er - 0x20 - sizeof(uint64_t); // home space, plus (unused) return pointer

    // align stack pointer for the sake of AVX2 / AVX512
    cb.thread_context.Rsp &= ~(uint64_t)0x3f;
    cb.thread_context.Rsp += sizeof(uint64_t);

    if (exit_status == nt_status::SINGLE_STEP) {
        // clear TF flag when we return
        cb.thread_context.EFlags &= ~(uint32_t)0x100;
    }
#endif

    return true;
}

#ifdef __x86_64__
extern "C"
bool page_fault_user(uintptr_t address, uint64_t error_code) {
    auto& current_thread = *get_current_thread();
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    memcpy(&ctx.FltSave, &get_cpu_block()->xsave, sizeof(ctx.FltSave));

    enable_interrupts();

    auto& p = *current_thread.proc;

    auto okay = p.handle_page_fault(address, error_code);

    if (okay) {
        disable_interrupts();
        memcpy(&get_cpu_block()->thread_context, &ctx, sizeof(CONTEXT));
        return false;
    }

    EXCEPTION_RECORD er;

    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.ExceptionCode = (uint32_t)nt_status::ACCESS_VIOLATION;

    er.NumberParameters = 2;
    er.ExceptionInformation[0] = (error_code & 0x12) >> 1;
    er.ExceptionInformation[1] = address;

    auto ret = send_signal(er, ctx, nt_status::ACCESS_VIOLATION);

    disable_interrupts();

    return ret;
}

extern "C"
void page_fault_kernel(uintptr_t address, uint64_t error_code) {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Page fault in kernel mode (address = {:x}, error code = {:x}, ", address, error_code);

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {})\n", cb.cpu_no);

    if (address & ((uint64_t)1 << 63)) {
        uint64_t pte = SELFMAP | ((address & 0xfffffffff000) >> 9);

        sp = format(sp, "pte {:x}\n", pte);
    }

    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void double_fault() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Double fault, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void nmi() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Non-maskable interrupt, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void general_protection_fault_kernel(uint64_t error_code) {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "General protection fault in kernel mode, error code = {:x}, ", error_code);

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void general_protection_fault_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    memcpy(&ctx.FltSave, &get_cpu_block()->xsave, sizeof(ctx.FltSave));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::ACCESS_VIOLATION; // sic
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 2;
    er.ExceptionInformation[0] = 0;
    er.ExceptionInformation[1] = 0xffffffffffffffff;

    send_signal(er, ctx, nt_status::ACCESS_VIOLATION);
}

extern "C"
void invalid_opcode_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Invalid opcode in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void invalid_opcode_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    memcpy(&ctx.FltSave, &get_cpu_block()->xsave, sizeof(ctx.FltSave));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::ILLEGAL_INSTRUCTION;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::ILLEGAL_INSTRUCTION);
}

extern "C"
void divide_by_zero_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Divide by zero in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void divide_by_zero_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    memcpy(&ctx.FltSave, &get_cpu_block()->xsave, sizeof(ctx.FltSave));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::INTEGER_DIVIDE_BY_ZERO;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::INTEGER_DIVIDE_BY_ZERO);
}

extern "C"
void breakpoint_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    if (is_trace_point(cb.thread_context.Rip - 1)) {
        cb.thread_context.Rip--; // move back to before cc
        return;
    }

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Unhandled breakpoint in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void breakpoint_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    memcpy(&ctx.FltSave, &get_cpu_block()->xsave, sizeof(ctx.FltSave));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::BREAKPOINT;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::BREAKPOINT);
}

extern "C"
void single_step_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Unhandled single-step breakpoint in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void single_step_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    memcpy(&ctx.FltSave, &get_cpu_block()->xsave, sizeof(ctx.FltSave));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::SINGLE_STEP;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::SINGLE_STEP);
}

extern "C"
void simd_exception_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "SIMD exception in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void simd_exception_user() {
    CONTEXT ctx;
    nt_status code;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    memcpy(&ctx.FltSave, &get_cpu_block()->xsave, sizeof(ctx.FltSave));

    auto flags = (ctx.FltSave.MxCsr & 0x3f) & ~((ctx.FltSave.MxCsr >> 7) & 0x3f);

    if (flags & 0x7) // IE, DE, ZE
        code = nt_status::FLOAT_MULTIPLE_FAULTS;
    else // OE, UE, PE
        code = nt_status::FLOAT_MULTIPLE_TRAPS;

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)code;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, code);
}

#elifdef __aarch64__

static void invalid_opcode_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Invalid opcode in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

static void stack_alignment_exception_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Stack alignment exception in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

static void unhandled_kernel_exception(uint64_t esr_value) {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;
    ARM64_ESR esr;

    esr.value = esr_value;

    char s[1024];
    auto sp = span(s, sizeof(s));

    if (esr.EC == 0x2f) {
        auto DFSC = esr.ISS & 0x3f;

        if (DFSC == 0x11)
            sp = format(sp, "Asynchronous SError exception");
        else
            sp = format(sp, "Uncategorized SError exception");
    } else
        sp = format(sp, "Unhandled exception in kernel mode");

    if (!current_thread)
        sp = format(sp, ", not in thread");
    else
        sp = format(sp, ", thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "ESR_EL1 = {:08x} (ISS = {:x}, IL = {:x}, EC = {:x}, ISS2 = {:x})\n",
                esr_value, +esr.ISS, +esr.IL, +esr.EC, +esr.ISS2);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

static string_view aarch64_dfsc_to_string(uint8_t dfsc) {
    switch (dfsc) {
        case 0b000000:
            return "Address size fault, level 0 of translation or translation table base register";
        case 0b000001:
            return "Address size fault, level 1";
        case 0b000010:
            return "Address size fault, level 2";
        case 0b000011:
            return "Address size fault, level 3";
        case 0b000100:
            return "Translation fault, level 0";
        case 0b000101:
            return "Translation fault, level 1";
        case 0b000110:
            return "Translation fault, level 2";
        case 0b000111:
            return "Translation fault, level 3";
        case 0b001001:
            return "Access flag fault, level 1";
        case 0b001010:
            return "Access flag fault, level 2";
        case 0b001011:
            return "Access flag fault, level 3";
        case 0b001000:
            return "Access flag fault, level 0";
        case 0b001100:
            return "Permission fault, level 0";
        case 0b001101:
            return "Permission fault, level 1";
        case 0b001110:
            return "Permission fault, level 2";
        case 0b001111:
            return "Permission fault, level 3";
        case 0b010000:
            return "Synchronous External abort, not on translation table walk or hardware update of translation table";
        case 0b010010:
            return "Synchronous External abort on translation table walk or hardware update of translation table, level -2";
        case 0b010011:
            return "Synchronous External abort on translation table walk or hardware update of translation table, level -1";
        case 0b010100:
            return "Synchronous External abort on translation table walk or hardware update of translation table, level 0";
        case 0b010101:
            return "Synchronous External abort on translation table walk or hardware update of translation table, level 1";
        case 0b010110:
            return "Synchronous External abort on translation table walk or hardware update of translation table, level 2";
        case 0b010111:
            return "Synchronous External abort on translation table walk or hardware update of translation table, level 3";
        case 0b011000:
            return "Synchronous parity or ECC error on memory access, not on translation table walk";
        case 0b011011:
            return "Synchronous parity or ECC error on memory access on translation table walk or hardware update of translation table, level -1";
        case 0b011100:
            return "Synchronous parity or ECC error on memory access on translation table walk or hardware update of translation table, level 0";
        case 0b011101:
            return "Synchronous parity or ECC error on memory access on translation table walk or hardware update of translation table, level 1";
        case 0b011110:
            return "Synchronous parity or ECC error on memory access on translation table walk or hardware update of translation table, level 2";
        case 0b011111:
            return "Synchronous parity or ECC error on memory access on translation table walk or hardware update of translation table, level 3";
        case 0b100010:
            return "Granule Protection Fault on translation table walk or hardware update of translation table, level -2";
        case 0b100011:
            return "Granule Protection Fault on translation table walk or hardware update of translation table, level -1";
        case 0b100100:
            return "Granule Protection Fault on translation table walk or hardware update of translation table, level 0";
        case 0b100101:
            return "Granule Protection Fault on translation table walk or hardware update of translation table, level 1";
        case 0b100110:
            return "Granule Protection Fault on translation table walk or hardware update of translation table, level 2";
        case 0b100111:
            return "Granule Protection Fault on translation table walk or hardware update of translation table, level 3";
        case 0b101000:
            return "Granule Protection Fault, not on translation table walk or hardware update of translation table";
        case 0b101001:
            return "Address size fault, level -1";
        case 0b101010:
            return "Translation fault, level -2";
        case 0b101011:
            return "Translation fault, level -1";
        case 0b101100:
            return "Address Size fault, level -2";
        case 0b110000:
            return "TLB conflict abort";
        case 0b110001:
            return "Unsupported atomic hardware update fault";
        default:
            return "";
    }
}

static void page_fault_kernel(uint64_t esr_value) {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;
    uint64_t address;

    __asm __volatile (
        "mrs %x[res], far_el1"
        : [res] "=r" (address)
    );

    // FIXME - print what variety of page fault it is, using ISS in ESR_EL1

    auto pte = find_pte(address);
    bool stack_overflow = false;

    char s[1024];
    auto sp = span(s, sizeof(s));

    if ((!pte || !pte->valid) && current_thread &&
        address >= (uintptr_t)current_thread->kernel_stack &&
        address < (uintptr_t)current_thread->kernel_stack + (KERNEL_GUARD_PAGES << PAGE_SHIFT)) {
        stack_overflow = true;
    }

    if (stack_overflow)
        sp = format(sp, "Stack overflow");
    else
        sp = format(sp, "Page fault");

    sp = format(sp, " in kernel mode (address = {:x}, ", address);

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}, ESR_EL1 = {:08x}, opcode = {:08x})\n", cb.cpu_no, esr_value, *(uint32_t*)cb.thread_context.Pc);

    auto dfsc_string = aarch64_dfsc_to_string(esr_value & 0x3f);
    if (!dfsc_string.empty())
        sp = format(sp, "{}\n", dfsc_string);

    // FIXME - parse PTE for explanation of why page fault happened
    if (pte && !stack_overflow)
        sp = format(sp, "pte: {}\n", *pte);

    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)), source_location::current(),
         &cb.thread_context);
}

extern "C"
void handle_kernel_exception(uint64_t esr_value) {
    auto& cb = *get_cpu_block();
    ARM64_ESR esr;

    esr.value = esr_value;

    if (cb.current_thread)
        cb.thread_context.Sp = (uint64_t)cb.current_thread->syscall_stack;

    switch (esr.EC) {
        case 0: // "Unknown reason"
            // FIXME - might actually be other things
            invalid_opcode_kernel();
            break;

        case 0x21: // Instruction Abort from same EL
        case 0x25: // Data Abort from same EL
            page_fault_kernel(esr_value);
            break;

        case 0x26:
            stack_alignment_exception_kernel();
            break;

        default:
            unhandled_kernel_exception(esr_value);
            break;
    }
}

static void invalid_opcode_user(CONTEXT& ctx) {
    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::ILLEGAL_INSTRUCTION;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Pc;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::ILLEGAL_INSTRUCTION);
}

static void unhandled_user_exception(uint64_t esr_value, CONTEXT& ctx) {
    ARM64_ESR esr;

    esr.value = esr_value;

    // FIXME - what exactly does NT do in this case?

    EXCEPTION_RECORD er;

    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Pc;
    er.ExceptionCode = (uint32_t)nt_status::UNHANDLED_EXCEPTION;

    er.NumberParameters = 1;
    er.ExceptionInformation[0] = esr_value;

    send_signal(er, ctx, nt_status::UNHANDLED_EXCEPTION);
}

static void page_fault_user(uint64_t address, uint64_t esr_value,
                            CONTEXT& ctx) {
    auto& current_thread = *get_current_thread();

    auto& p = *current_thread.proc;

    if (p.handle_page_fault(address, esr_value))
        return;

    EXCEPTION_RECORD er;

    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Pc;
    er.ExceptionCode = (uint32_t)nt_status::ACCESS_VIOLATION;

    er.NumberParameters = 2;
    er.ExceptionInformation[0] = 0; // FIXME - what goes here on aarch64?
    er.ExceptionInformation[1] = address;

    send_signal(er, ctx, nt_status::ACCESS_VIOLATION);
}

static void breakpoint_user(uint64_t esr_value, CONTEXT& ctx) {
    ARM64_ESR esr;

    esr.value = esr_value;

    EXCEPTION_RECORD er;

    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Pc;
    er.ExceptionCode = (uint32_t)nt_status::BREAKPOINT;

    // FIXME - does NT return the ISS value like this?

    er.NumberParameters = 1;
    er.ExceptionInformation[0] = esr.ISS;

    send_signal(er, ctx, nt_status::BREAKPOINT);
}

static void stack_alignment_exception_user(CONTEXT& ctx) {
    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::BAD_STACK;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Pc;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::BAD_STACK);
}

extern "C"
void handle_user_exception(uint64_t esr_value) {
    auto& t = *get_current_thread();
    CONTEXT ctx;
    ARM64_ESR esr;
    uint64_t address;

    esr.value = esr_value;

    if (esr.EC == 0x20 || esr.EC == 0x24) { // Instruction Abort or Data Abort
        __asm __volatile (
            "mrs %x[res], far_el1"
            : [res] "=r" (address)
        );
    }

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));
    ctx.Sp = t.user_stack;

    t.kernel_mode = true;
    t.time_running_in_userspace += cntvct_el0_load() - t.last_userspace_transition;

    enable_interrupts();

    switch (esr.EC) {
        case 0: // "Unknown reason"
            // FIXME - might actually be other things
            invalid_opcode_user(ctx);
            break;

        case 0x20: // Instruction Abort from lower EL
        case 0x24: // Data Abort from lower EL
            page_fault_user(address, esr_value, ctx);
            break;

        case 0x26:
            stack_alignment_exception_user(ctx);
            break;

        case 0x3c: // BRK instruction execution in AArch64 state
            breakpoint_user(esr_value, ctx);
            break;

        default:
            unhandled_user_exception(esr_value, ctx);
            break;
    }

    disable_interrupts();

    memcpy(&get_cpu_block()->thread_context, &ctx, sizeof(CONTEXT));
    t.user_stack = ctx.Sp;

    t.kernel_mode = false;
    t.last_userspace_transition = cntvct_el0_load();
}

void __attribute__((noreturn)) halt_isr() {
    processors_left_to_halt--;

    disable_interrupts();

    while (true) {
        __asm __volatile("wfi");
    }
}

static void tlb_isr() {
    for (unsigned int i = 0; i < tlb_num_pages; i++) {
        __asm __volatile (
            "tlbi vae1, %x[res]\n"
            :
            : [res] "r" ((tlb_va >> PAGE_SHIFT) + i)
        );
    }

    __asm __volatile (
        "dsb ish\n"
        "isb\n"
    );

    tlb_cpus_left--;
}

static void all_tlb_isr() {
    __asm __volatile (
        "tlbi vmalle1\n"
        "dsb ish\n"
        "isb\n"
    );

    all_tlb_cpus_left--;
}

extern "C"
void handle_irq(uint32_t number) {
    ARM64_GICC_IAR iar;

    iar.value = number;

    switch (iar.InterruptID) {
        case HALT_SGI:
            halt_isr();
            break;

        case TLB_SGI:
            tlb_isr();
            break;

        case ALL_TLB_SGI:
            all_tlb_isr();
            break;

        default: {
            lock_guard lock(irq_lock);

            for (auto le = irqs.next; le != &irqs; le = le->next) {
                auto& i = containing_record<irq>(le);

                if (i.number != iar.InterruptID)
                    continue;

                // FIXME - use refcounting so we can let go of irq_lock here(?)

                for (auto le2 = i.handlers.next; le2 != &i.handlers; le2 = le2->next) {
                    auto& h = containing_record<irq_handler_info>(le2);

                    if (h.func(h.ctx)) {
                        add_entropy();
                        break;
                    }
                }

                break;
            }

            break;
        }
    }

    auto& cb = *get_cpu_block();

    if (cb.gicc) {
        auto& GICC_EOIR = *(volatile uint32_t*)(cb.gicc + 0x10);
        GICC_EOIR = number;
    } else {
        __asm __volatile (
            "msr ICC_EOIR1_EL1, %x[val]"
            :
            : [val] "r" (number)
        );
    }
}

uint16_t allocate_msi_irq() {
    lock_guard lg(msi_alloc_lock);

    for (unsigned int i = 0; i < sizeof(msi_alloc); i++) {
        if (msi_alloc[i] == 0)
            continue;

        for (unsigned int j = 0; j < 8; j++) {
            if (msi_alloc[i] & (1 << j)) {
                msi_alloc[i] &= ~(1 << j);
                return (uint16_t)(msi_base_spi + (i * 8) + j);
            }
        }
    }

    return 0;
}

void free_msi_irq(uint16_t num) {
    lock_guard lg(msi_alloc_lock);

    num -= msi_base_spi;

    msi_alloc[num / 8] &= ~(1 << (num % 8));
}

#endif
