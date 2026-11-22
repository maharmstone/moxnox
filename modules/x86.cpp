// SPDX-License-Identifier: MIT
module;

#include <stdint.h>

export module x86;

export const uint64_t SELFMAP_PDT = 0xfffff6fb40000000;
export const uint64_t SELFMAP_PDPT = 0xfffff6fb7da00000;
export const uint64_t SELFMAP_PML4 = 0xfffff6fb7dbed000;

export const uint64_t CR0_MP = 1 << 1;
export const uint64_t CR0_WP = 1 << 16;
export const uint64_t CR0_PG = (uint64_t)1 << 31;

export const uint64_t CR4_PAE = 1 << 5;
export const uint64_t CR4_OSFXSR = 1 << 9;
export const uint64_t CR4_OSXMMEXCPT = 1 << 10;
export const uint64_t CR4_OSXSAVE = 1 << 18;

export const uint32_t EFER_MSR = 0xc0000080;
export const uint64_t EFER_SCE = 1 << 0;
export const uint64_t EFER_LME = 1 << 8;
export const uint64_t EFER_NX = 1 << 11;

export const uint64_t XCR0_X87 = 1 << 0;
export const uint64_t XCR0_SSE = 1 << 1;
export const uint64_t XCR0_AVX = 1 << 2;
export const uint64_t XCR0_OPMASK = 1 << 5;
export const uint64_t XCR0_ZMM_HI256 = 1 << 6;
export const uint64_t XCR0_HI16_ZMM = 1 << 7;

export union HARDWARE_PTE {
    struct {
        uint64_t p : 1;
        uint64_t w : 1;
        uint64_t u : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t d : 1;
        uint64_t ps : 1;
        uint64_t g : 1;
        uint64_t allow_write : 1; // custom
        uint64_t cow : 1; // custom
        uint64_t reserved : 1; // custom
        uint64_t pfn : 40;
        uint64_t : 6;
        uint64_t owned : 1; // custom
        uint64_t protection_key : 4;
        uint64_t xd : 1;
    };
    uint64_t value;
};

static_assert(sizeof(HARDWARE_PTE) == sizeof(uint64_t));

export struct gdt_entry {
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint32_t BaseMid:8;
    uint32_t Type:5;
    uint32_t Dpl:2;
    uint32_t Pres:1;
    uint32_t LimitHi:4;
    uint32_t Sys:1;
    uint32_t Long:1;
    uint32_t Default_Big:1;
    uint32_t Granularity:1;
    uint32_t BaseHi:8;
};

static_assert(sizeof(gdt_entry) == 8);

export struct idt_entry {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t ist;
    uint8_t gate_type : 4;
    uint8_t zero1 : 1;
    uint8_t dpl : 2;
    uint8_t present : 1;
    uint16_t offset_2;
    uint32_t offset_3;
    uint32_t zero2;
};

static_assert(sizeof(idt_entry) == 16);

export struct GDTIDT {
    uint16_t Limit;
    uint32_t Base;
} __attribute__((packed));

export struct task_state_segment {
    uint32_t reserved1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iopb_offset;
} __attribute__((packed));

static_assert(sizeof(task_state_segment) == 0x68);

export const uint16_t TSS_SEGMENT = 0x18;

export enum class e820_mem_type : uint32_t {
    free = 1,
    reserved = 2,
    acpi_reclaimable = 3,
    acpi_nvs = 4,
    bad = 5,
};

export struct e820_entry {
    uint64_t address;
    uint64_t length;
    enum e820_mem_type type;
} __attribute__((packed));

export struct xsave_area {
    uint8_t buf[576];
} __attribute__ ((aligned (64)));

export __attribute__((always_inline))
uintptr_t inline __readcr0() {
    uintptr_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr0"
        : "=r" (ret)
    );

    return ret;
}

export __attribute__((always_inline))
void inline __writecr0(uintptr_t v) {
    __asm__ __volatile__ (
        "mov\tcr0, %0"
        :
        : "r" (v)
    );
}

export __attribute__((always_inline))
void inline __writecr3(uintptr_t v) {
    __asm__ __volatile__ (
        "mov\tcr3, %0"
        :
        : "r" (v)
    );
}

export __attribute__((always_inline))
uintptr_t inline __readcr4() {
    uintptr_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr4"
        : "=r" (ret)
    );

    return ret;
}

export __attribute__((always_inline))
void inline __writecr4(uintptr_t v) {
    __asm__ __volatile__ (
        "mov\tcr4, %0"
        :
        : "r" (v)
    );
}

export __attribute__((always_inline))
uint64_t inline __readmsr(uint32_t r) {
    uint32_t high, low;

    __asm__ __volatile__ (
        "rdmsr"
        : "=d" (high), "=a" (low)
        : "c" (r)
    );

    return ((uint64_t)high << 32) | low;
}

export __attribute__((always_inline))
void inline __writemsr(uint32_t r, uint64_t v) {
    auto high = (uint32_t)(v >> 32);
    uint32_t low = v & 0xffffffff;

    __asm__ __volatile__ (
        "wrmsr"
        :
        : "c" (r), "d" (high), "a" (low)
    );
}

export __attribute__((always_inline))
void inline _disable() {
    __asm__ __volatile__ (
        "cli"
    );
}

// FIXME - commented out because bug in GCC 15 means clobbers aren't working properly
#if 0
export __attribute__((always_inline))
uint64_t inline _xgetbv(uint32_t r) {
    uint32_t high, low;

    __asm__ __volatile__ (
        "xgetbv"
        : "=d" (high), "=a" (low)
        : "c" (r)
    );

    return ((uint64_t)high << 32) | low;
}

export __attribute__((always_inline))
void inline _xsetbv(uint32_t r, uint64_t v) {
    auto high = (uint32_t)(v >> 32);
    uint32_t low = v & 0xffffffff;

    __asm__ __volatile__ (
        "xsetbv"
        :
        : "c" (r), "d" (high), "a" (low)
    );
}
#endif
