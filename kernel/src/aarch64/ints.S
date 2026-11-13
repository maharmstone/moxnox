/* SPDX-License-Identifier: GPL-2.0-only */
#include "../context_offsets.h"
#include "../tick_rate.h"
#include "../dwarf.S"

.equ GICC_IAR_OFFSET, 0xc
.equ GICC_EOIR_OFFSET, 0x10

.section .debug_abbrev, "dr"

.uleb128 1 // entry num
.uleb128 DW_TAG_compile_unit
.byte 1 // has children
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_udata
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_language
.uleb128 DW_FORM_data2
.uleb128 0
.uleb128 0

.uleb128 2 // entry num
.uleb128 DW_TAG_subprogram
.byte 0 // has children
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_data8
.uleb128 0
.uleb128 0

.uleb128 0

.section .debug_info, "dr"

.long .debug_info_end - .debug_info_start

.debug_info_start:
.short 5 // version
.byte DW_UT_compile
.byte 8 // address size
.secrel32 .debug_abbrev

.uleb128 1
.quad .text // low_pc
.uleb128 .text_end - .text // high_pc
.asciz __FILE__ // name
.short DW_LANG_Mips_Assembler // language

dwarf_func_asm kernel_irq_entry_bsp
dwarf_func_asm kernel_irq_entry_ap
dwarf_func_asm user_irq_entry_bsp
dwarf_func_asm user_irq_entry_ap
dwarf_func_asm kernel_exception_entry
dwarf_func_asm user_exception_entry
dwarf_func_asm do_syscall

.text

// FIXME - SEH(?)

.macro save_exception_context user
	.if \user
		mrs	x0, tpidr_el1
		add	x0, x0, GS_CONTEXT
	.else
		add	x0, x18, GS_CONTEXT
	.endif

	stp	x2, x3, [x0, #CONTEXT_X0 + 0x10]
	stp	x4, x5, [x0, #CONTEXT_X0 + 0x20]
	stp	x6, x7, [x0, #CONTEXT_X0 + 0x30]
	stp	x8, x9, [x0, #CONTEXT_X0 + 0x40]
	stp	x10, x11, [x0, #CONTEXT_X0 + 0x50]
	stp	x12, x13, [x0, #CONTEXT_X0 + 0x60]
	stp	x14, x15, [x0, #CONTEXT_X0 + 0x70]
	stp	x16, x17, [x0, #CONTEXT_X0 + 0x80]
	stp	x18, x19, [x0, #CONTEXT_X0 + 0x90]
	stp	x20, x21, [x0, #CONTEXT_X0 + 0xa0]
	stp	x22, x23, [x0, #CONTEXT_X0 + 0xb0]
	stp	x24, x25, [x0, #CONTEXT_X0 + 0xc0]
	stp	x26, x27, [x0, #CONTEXT_X0 + 0xd0]
	stp	x28, fp, [x0, #CONTEXT_X0 + 0xe0]
	str	lr, [x0, #CONTEXT_X0 + 0xf0]

	stp	q0, q1, [x0, #CONTEXT_V0]
	stp	q2, q3, [x0, #CONTEXT_V0 + 0x20]
	stp	q4, q5, [x0, #CONTEXT_V0 + 0x40]
	stp	q6, q7, [x0, #CONTEXT_V0 + 0x60]
	stp	q8, q9, [x0, #CONTEXT_V0 + 0x80]
	stp	q10, q11, [x0, #CONTEXT_V0 + 0xa0]
	stp	q12, q13, [x0, #CONTEXT_V0 + 0xc0]
	stp	q14, q15, [x0, #CONTEXT_V0 + 0xe0]
	stp	q16, q17, [x0, #CONTEXT_V0 + 0x100]
	stp	q18, q19, [x0, #CONTEXT_V0 + 0x120]
	stp	q20, q21, [x0, #CONTEXT_V0 + 0x140]
	stp	q22, q23, [x0, #CONTEXT_V0 + 0x160]
	stp	q24, q25, [x0, #CONTEXT_V0 + 0x180]
	stp	q26, q27, [x0, #CONTEXT_V0 + 0x1a0]
	stp	q28, q29, [x0, #CONTEXT_V0 + 0x1c0]
	stp	q30, q31, [x0, #CONTEXT_V0 + 0x1e0]

	mrs	x1, fpcr
	str	w1, [x0, #CONTEXT_FPCR]

	mrs	x1, fpsr
	str	w1, [x0, #CONTEXT_FPSR]

	mrs	x1, elr_el1
	str	x1, [x0, #CONTEXT_PC]

	mrs	x1, spsr_el1
	str	w1, [x0, #CONTEXT_CPSR]

	.if \user
		mrs	x1, tpidr_el1
		add	x3, x1, GS_CURRENT_THREAD
	.else
		add	x3, x18, GS_CURRENT_THREAD
	.endif
	ldr	x3, [x3]

	.if \user
		add	x1, sp, 0x10
	.else
		ldr	x1, [x18, #GS_EMERGENCY_SCRATCH + 0x8]
		cbz	x3, 16f // handle case when not in thread
	.endif

	add	x2, x3, THREAD_SYSCALL_STACK
	str	x1, [x2]
	add	x2, x3, THREAD_USER_STACK
	mrs	x1, sp_el0
	str	x1, [x2]

	// FIXME - debug registers

16:
	// x0/xzr and x1 already pushed on stack beforehand

	ldr	x1, [sp, #8]
	str	x1, [x0, #CONTEXT_X0 + 0x8]

	.if \user
		ldr	x1, [sp]
	.else
		ldr	x1, [x18, #GS_EMERGENCY_SCRATCH]
	.endif
	str	x1, [x0, #CONTEXT_X0]
.endm

.macro restore_user_context
	add x0, x18, GS_CONTEXT

	ldp x2, x3, [x0, #CONTEXT_X0 + 0x10]
	ldp x4, x5, [x0, #CONTEXT_X0 + 0x20]
	ldp x6, x7, [x0, #CONTEXT_X0 + 0x30]
	ldp x8, x9, [x0, #CONTEXT_X0 + 0x40]
	ldp x10, x11, [x0, #CONTEXT_X0 + 0x50]
	ldp x12, x13, [x0, #CONTEXT_X0 + 0x60]
	ldp x14, x15, [x0, #CONTEXT_X0 + 0x70]
	ldp x16, x17, [x0, #CONTEXT_X0 + 0x80]
	ldp x18, x19, [x0, #CONTEXT_X0 + 0x90]
	ldp x20, x21, [x0, #CONTEXT_X0 + 0xa0]
	ldp x22, x23, [x0, #CONTEXT_X0 + 0xb0]
	ldp x24, x25, [x0, #CONTEXT_X0 + 0xc0]
	ldp x26, x27, [x0, #CONTEXT_X0 + 0xd0]
	ldp x28, x29, [x0, #CONTEXT_X0 + 0xe0]
	ldr x30, [x0, #CONTEXT_X0 + 0xf0]

	ldp q0, q1, [x0, #CONTEXT_V0]
	ldp q2, q3, [x0, #CONTEXT_V0 + 0x20]
	ldp q4, q5, [x0, #CONTEXT_V0 + 0x40]
	ldp q6, q7, [x0, #CONTEXT_V0 + 0x60]
	ldp q8, q9, [x0, #CONTEXT_V0 + 0x80]
	ldp q10, q11, [x0, #CONTEXT_V0 + 0xa0]
	ldp q12, q13, [x0, #CONTEXT_V0 + 0xc0]
	ldp q14, q15, [x0, #CONTEXT_V0 + 0xe0]
	ldp q16, q17, [x0, #CONTEXT_V0 + 0x100]
	ldp q18, q19, [x0, #CONTEXT_V0 + 0x120]
	ldp q20, q21, [x0, #CONTEXT_V0 + 0x140]
	ldp q22, q23, [x0, #CONTEXT_V0 + 0x160]
	ldp q24, q25, [x0, #CONTEXT_V0 + 0x180]
	ldp q26, q27, [x0, #CONTEXT_V0 + 0x1a0]
	ldp q28, q29, [x0, #CONTEXT_V0 + 0x1c0]
	ldp q30, q31, [x0, #CONTEXT_V0 + 0x1e0]

	// FIXME - debug registers

	ldr	w1, [x0, #CONTEXT_FPCR]
	msr	fpcr, x1

	ldr	w1, [x0, #CONTEXT_FPSR]
	msr	fpsr, x1

	ldr	x1, [x0, #CONTEXT_PC]
	msr	elr_el1, x1

	ldr	w1, [x0, #CONTEXT_CPSR]
	msr	spsr_el1, x1

	ldr	x1, [x0, #CONTEXT_SP]
	msr	sp_el0, x1

	ldp x0, x1, [x0, #CONTEXT_X0]
.endm

.section .text.interrupts, "x"

.balign 2048
.global vector_table_bsp
vector_table_bsp:
// Synchronous Exception, EL1 using SP_EL0 stack
b	.
.balign 128
// IRQ, EL1 using SP_EL0 stack
b	.
.balign 128
// FIQ, EL1 using SP_EL0 stack
b	.
.balign 128
// SError, EL1 using SP_EL0 stack
b	.
.balign 128
// Synchronous Exception, EL1 using SP_EL1 stack
b	kernel_exception_entry
.balign 128
// IRQ, EL1 using SP_EL1 stack
b	kernel_irq_entry_bsp
.balign 128
// FIQ, EL1 using SP_EL1 stack
b	.
.balign 128
// SError, EL1 using SP_EL1 stack
b	kernel_exception_entry
.balign 128
// Synchronous Exception, 64-bit EL0
b	user_exception_entry
.balign 128
// IRQ, 64-bit EL0
b	user_irq_entry_bsp
.balign 128
// FIQ, 64-bit EL0
b	.
.balign 128
// SError, 64-bit EL0
b	user_serror_exception_entry
.balign 128
// Synchronous Exception, 32-bit EL0
b	.
.balign 128
// IRQ, 32-bit EL0
b	.
.balign 128
// FIQ, 32-bit EL0
b	.
.balign 128
// SError, 32-bit EL0
b	.
.balign 128

.global vector_table_ap
vector_table_ap:
// Synchronous Exception, EL1 using SP_EL0 stack
b	.
.balign 128
// IRQ, EL1 using SP_EL0 stack
b	.
.balign 128
// FIQ, EL1 using SP_EL0 stack
b	.
.balign 128
// SError, EL1 using SP_EL0 stack
b	kernel_exception_entry
.balign 128
// Synchronous Exception, EL1 using SP_EL1 stack
b	kernel_exception_entry
.balign 128
// IRQ, EL1 using SP_EL1 stack
b	kernel_irq_entry_ap
.balign 128
// FIQ, EL1 using SP_EL1 stack
b	.
.balign 128
// SError, EL1 using SP_EL1 stack
b	.
.balign 128
// Synchronous Exception, 64-bit EL0
b	user_exception_entry
.balign 128
// IRQ, 64-bit EL0
b	user_irq_entry_ap
.balign 128
// FIQ, 64-bit EL0
b	.
.balign 128
// SError, 64-bit EL0
b	user_serror_exception_entry
.balign 128
// Synchronous Exception, 32-bit EL0
b	.
.balign 128
// IRQ, 32-bit EL0
b	.
.balign 128
// FIQ, 32-bit EL0
b	.
.balign 128
// SError, 32-bit EL0
b	.
.balign 128

.section .text

kernel_exception_entry:
str	x0, [x18, #GS_EMERGENCY_SCRATCH]
mov	x0, sp
str	x0, [x18, #GS_EMERGENCY_SCRATCH + 0x8]

ldr	x0, [x18, #GS_EMERGENCY_SP]
mov	sp, x0

sub	sp, sp, #0x10
stp	xzr, x1, [sp]
save_exception_context 0

mrs	x0, esr_el1
b	handle_kernel_exception
kernel_exception_entry_end:

.macro syscall_stub name
	.quad "stub_\name"

	.text
	"stub_\name":
		adr	x0, 0f
		b	unimplemented_syscall
	0:
		.ascii "\name\0"
		.p2align 2

	.section .rdata
.endm

.macro syscall_func name, args
	.ifle \args - 8
		.quad "user_\name"
	.else
		.quad "stack_\name"

		.text
		.seh_proc "stack_\name"
		"stack_\name":
			.if \args % 2 == 1
				sub	sp, sp, 0x50 + (\args - 7) * 8
				.seh_stackalloc 0x50 + (\args - 7) * 8
			.else
				sub	sp, sp, 0x50 + (\args - 8) * 8
				.seh_stackalloc 0x50 + (\args - 8) * 8
			.endif

			str	lr, [sp, #0x40 + ((\args - 8) * 8)]
			.seh_save_reg lr, 0x40 + ((\args - 8) * 8)
			.seh_endprologue

			stp	x0, x1, [sp]
			stp	x2, x3, [sp, #0x10]
			stp	x4, x5, [sp, #0x20]
			stp	x6, x7, [sp, #0x30]

			// ks_addr
			add	x0, sp, #0x40
			// us_addr
			mrs	x1, sp_el0
			// size
			mov	x2, #(\args - 8) * 8

			// bool copy_from_process(void* ks_addr, const void* us_addr, size_t size);
			bl	copy_from_process
			cbnz	x0, 0f

			// return STATUS_ACCESS_VIOLATION if copy_from_process returned false
			mov	w0, 5
			movk	w0, #0xc000, lsl 16

			.seh_startepilogue

			ldr	lr, [sp, #0x40 + ((\args - 8) * 8)]
			.seh_save_reg lr, 0x40 + ((\args - 8) * 8)

			.if \args % 2 == 1
				add	sp, sp, 0x50 + (\args - 7) * 8
				.seh_stackalloc 0x50 + (\args - 7) * 8
			.else
				add	sp, sp, 0x50 + (\args - 8) * 8
				.seh_stackalloc 0x50 + (\args - 8) * 8
			.endif
			.seh_endepilogue

			ret

0:
			ldp	x0, x1, [sp]
			ldp	x2, x3, [sp, #0x10]
			ldp	x4, x5, [sp, #0x20]
			ldp	x6, x7, [sp, #0x30]
			add	sp, sp, #0x40

			bl	"user_\name"

			ldr	lr, [sp, #((\args - 8) * 8)]

			.if \args % 2 == 1
				add	sp, sp, 0x10 + ((\args - 7) * 8)
			.else
				add	sp, sp, 0x10 + ((\args - 8) * 8)
			.endif
			ret
		.seh_endproc
		stack_\name\()_end:

		.section .debug_info, "dr"
		dwarf_func_asm stack_\name

		.section .rdata
	.endif
.endm

.section .rdata
syscalls:
#include "../syscall-list.S"
end_syscalls:

.section .text

do_syscall:
and	x1, x0, 0xffff
cmp	x1, #0x1000
b.lt	unknown_syscall
sub	x1, x1, 0x1000
cmp	x1, #NUM_SYSCALLS
b.ge	unknown_syscall

// save context

mrs	x0, tpidr_el1
add	x0, x0, #GS_CURRENT_THREAD
ldr	x0, [x0]
add	x0, x0, #THREAD_SYSCALL_CONTEXT

str	x1, [x0, #CONTEXT_X0 + 0x8]
stp	x2, x3, [x0, #CONTEXT_X0 + 0x10]
stp	x4, x5, [x0, #CONTEXT_X0 + 0x20]
stp	x6, x7, [x0, #CONTEXT_X0 + 0x30]
stp	x8, x9, [x0, #CONTEXT_X0 + 0x40]
stp	x10, x11, [x0, #CONTEXT_X0 + 0x50]
stp	x12, x13, [x0, #CONTEXT_X0 + 0x60]
stp	x14, x15, [x0, #CONTEXT_X0 + 0x70]
stp	x16, x17, [x0, #CONTEXT_X0 + 0x80]
stp	x18, x19, [x0, #CONTEXT_X0 + 0x90]
stp	x20, x21, [x0, #CONTEXT_X0 + 0xa0]
stp	x22, x23, [x0, #CONTEXT_X0 + 0xb0]
stp	x24, x25, [x0, #CONTEXT_X0 + 0xc0]
stp	x26, x27, [x0, #CONTEXT_X0 + 0xd0]
stp	x28, fp, [x0, #CONTEXT_X0 + 0xe0]
str	lr, [x0, #CONTEXT_X0 + 0xf0]

stp	q0, q1, [x0, #CONTEXT_V0]
stp	q2, q3, [x0, #CONTEXT_V0 + 0x20]
stp	q4, q5, [x0, #CONTEXT_V0 + 0x40]
stp	q6, q7, [x0, #CONTEXT_V0 + 0x60]
stp	q8, q9, [x0, #CONTEXT_V0 + 0x80]
stp	q10, q11, [x0, #CONTEXT_V0 + 0xa0]
stp	q12, q13, [x0, #CONTEXT_V0 + 0xc0]
stp	q14, q15, [x0, #CONTEXT_V0 + 0xe0]
stp	q16, q17, [x0, #CONTEXT_V0 + 0x100]
stp	q18, q19, [x0, #CONTEXT_V0 + 0x120]
stp	q20, q21, [x0, #CONTEXT_V0 + 0x140]
stp	q22, q23, [x0, #CONTEXT_V0 + 0x160]
stp	q24, q25, [x0, #CONTEXT_V0 + 0x180]
stp	q26, q27, [x0, #CONTEXT_V0 + 0x1a0]
stp	q28, q29, [x0, #CONTEXT_V0 + 0x1c0]
stp	q30, q31, [x0, #CONTEXT_V0 + 0x1e0]

mrs	x1, fpcr
str	w1, [x0, #CONTEXT_FPCR]

mrs	x1, fpsr
str	w1, [x0, #CONTEXT_FPSR]

mrs	x1, elr_el1
str	x1, [x0, #CONTEXT_PC]

mrs	x1, spsr_el1
str	w1, [x0, #CONTEXT_CPSR]

mrs	x1, sp_el0
str	x1, [x0, #CONTEXT_SP]

ldr	x1, [x0, #CONTEXT_X0 + 0x8]
adrp	x0, syscalls
add	x0, x0, :lo12:syscalls
lsl	x1, x1, #3
add	x0, x0, x1
ldr	x19, [x0]

// switch to kernel x18
mrs	x18, tpidr_el1

add	x0, x18, #GS_CURRENT_THREAD
ldr	x0, [x0]
add	x0, x0, #THREAD_KERNEL_MODE
mov	x1, #1
strb	w1, [x0]

// update time_running_in_userspace
add	x0, x18, #GS_CURRENT_THREAD
ldr	x0, [x0]
ldr	x0, [x0, #THREAD_LAST_USERSPACE_TRANSITION]
mrs	x1, cntvct_el0
sub	x1, x1, x0
add	x0, x18, #GS_CURRENT_THREAD
ldr	x0, [x0]
ldr	x0, [x0, #THREAD_TIME_RUNNING_IN_USERSPACE]
add	x1, x1, x0
add	x0, x18, #GS_CURRENT_THREAD
ldr	x0, [x0]
add	x0, x0, #THREAD_TIME_RUNNING_IN_USERSPACE
str	x1, [x0]

msr	DAIFClr, #0xf // enable interrupts

// load x0 and x1 again and call handler
ldr	x0, [sp]
ldr	x1, [sp, #0x8]
blr	x19

msr	DAIFSet, #0xf // disable interrupts

mov	x19, x0
bl	acquire_sync_lock

mov	x0, x19
bl	maybe_yield_after_syscall
mov	x19, x0

bl	release_sync_lock
mov	x0, x19

add	x19, x18, #GS_CURRENT_THREAD
ldr	x19, [x19]
add	x19, x19, #THREAD_KERNEL_MODE
strb	wzr, [x19]

// update last_userspace_transition
add	x19, x18, #GS_CURRENT_THREAD
ldr	x19, [x19]
add	x19, x19, #THREAD_LAST_USERSPACE_TRANSITION
mrs	x1, cntvct_el0
str	x1, [x19]

add	x19, x18, #GS_CURRENT_THREAD
ldr	x19, [x19]
add	x19, x19, #THREAD_SYSCALL_CONTEXT
ldp	q30, q31, [x19, #CONTEXT_V0 + 0x1e0]
ldp	q28, q29, [x19, #CONTEXT_V0 + 0x1c0]
ldp	q26, q27, [x19, #CONTEXT_V0 + 0x1a0]
ldp	q24, q25, [x19, #CONTEXT_V0 + 0x180]
ldp	q22, q23, [x19, #CONTEXT_V0 + 0x160]
ldp	q20, q21, [x19, #CONTEXT_V0 + 0x140]
ldp	q18, q19, [x19, #CONTEXT_V0 + 0x120]
ldp	q16, q17, [x19, #CONTEXT_V0 + 0x100]
ldp	q14, q15, [x19, #CONTEXT_V0 + 0xe0]
ldp	q12, q13, [x19, #CONTEXT_V0 + 0xc0]
ldp	q10, q11, [x19, #CONTEXT_V0 + 0xa0]
ldp	q8, q9, [x19, #CONTEXT_V0 + 0x80]
ldp	q6, q7, [x19, #CONTEXT_V0 + 0x60]
ldp	q4, q5, [x19, #CONTEXT_V0 + 0x40]
ldp	q2, q3, [x19, #CONTEXT_V0 + 0x20]
ldp	q0, q1, [x19, #CONTEXT_V0]

ldr	w1, [x19, #CONTEXT_FPCR]
msr	fpcr, x1
ldr	w1, [x19, #CONTEXT_FPSR]
msr	fpsr, x1

// restore elr_e1, spsr_el1, and sp_el0
ldr	x18, [x19, #CONTEXT_PC]
msr	elr_el1, x18
ldr	w18, [x19, #CONTEXT_CPSR]
msr	spsr_el1, x18
ldr	x18, [x19, #CONTEXT_SP]
msr	sp_el0, x18

// restore x18 and lr
ldr	x18, [x19, #CONTEXT_X0 + 0x90]
ldr	lr, [x19, #CONTEXT_X0 + 0xf0]

sub	x0, x0, #STATUS_USER_APC
cbnz	x0, 0f
ldr	x0, [x19, #CONTEXT_X0]
b	1f

0:
add	x0, x0, #STATUS_USER_APC

1:
ldr	x1, [x19, #CONTEXT_X0 + 0x8]
ldp	x2, x3, [x19, #CONTEXT_X0 + 0x10]
ldp	x4, x5, [x19, #CONTEXT_X0 + 0x20]
ldp	x6, x7, [x19, #CONTEXT_X0 + 0x30]
ldp	x8, x9, [x19, #CONTEXT_X0 + 0x40]
ldp	x10, x11, [x19, #CONTEXT_X0 + 0x50]
ldp	x12, x13, [x19, #CONTEXT_X0 + 0x60]
ldp	x14, x15, [x19, #CONTEXT_X0 + 0x70]
ldp	x16, x17, [x19, #CONTEXT_X0 + 0x80]
ldp	x20, x21, [x19, #CONTEXT_X0 + 0xa0]
ldp	x22, x23, [x19, #CONTEXT_X0 + 0xb0]
ldp	x24, x25, [x19, #CONTEXT_X0 + 0xc0]
ldp	x26, x27, [x19, #CONTEXT_X0 + 0xd0]
ldp	x28, fp, [x19, #CONTEXT_X0 + 0xe0]

ldr	x19, [x19, #CONTEXT_X0 + 0x98]

add	sp, sp, #0x10
eret

unknown_syscall:
// return STATUS_NOT_IMPLEMENTED
mov	w0, 2
movk	w0, #0xc000, lsl 16
mov	x1, xzr // don't leak to userspace
add	sp, sp, #0x10
eret
do_syscall_end:

user_exception_entry:
sub	sp, sp, #0x10
stp	x0, x1, [sp]

mrs	x0, esr_el1
and	x1, x0, 0xfc000000
asr	x1, x1, 26
sub	x1, x1, 0x15
cbnz	x1, 0f
b	do_syscall

0:
save_exception_context 1

mrs	x18, tpidr_el1
mrs	x0, esr_el1
bl	handle_user_exception

msr	DAIFSet, #0xf // disable interrupts
bl	acquire_sync_lock
bl	maybe_yield_after_interrupt
bl	release_sync_lock

1:
restore_user_context
add	sp, sp, #0x10
eret
user_exception_entry_end:

user_serror_exception_entry:
mrs	x18, tpidr_el1
b	kernel_exception_entry

# irq number is in w2; clobbers x0 and x1
.macro do_irq
	sub	sp, sp, #0x280
	stp	x4, x5, [sp]
	stp	x6, x7, [sp, #0x10]
	stp	x8, x9, [sp, #0x20]
	stp	x10, x11, [sp, #0x30]
	stp	x12, x13, [sp, #0x40]
	stp	x14, x15, [sp, #0x50]
	stp	x16, x17, [sp, #0x60]
	stp	x29, x30, [sp, #0x70]

	stp	q0, q1, [sp, #0x80]
	stp	q2, q3, [sp, #0xa0]
	stp	q4, q5, [sp, #0xc0]
	stp	q6, q7, [sp, #0xe0]
	stp	q8, q9, [sp, #0x100]
	stp	q10, q11, [sp, #0x120]
	stp	q12, q13, [sp, #0x140]
	stp	q14, q15, [sp, #0x160]
	stp	q16, q17, [sp, #0x180]
	stp	q18, q19, [sp, #0x1a0]
	stp	q20, q21, [sp, #0x1c0]
	stp	q22, q23, [sp, #0x1e0]
	stp	q24, q25, [sp, #0x200]
	stp	q26, q27, [sp, #0x220]
	stp	q28, q29, [sp, #0x240]
	stp	q30, q31, [sp, #0x260]

	mov	w0, w2
	bl	handle_irq

	ldp	q30, q31, [sp, #0x260]
	ldp	q28, q29, [sp, #0x240]
	ldp	q26, q27, [sp, #0x220]
	ldp	q24, q25, [sp, #0x200]
	ldp	q22, q23, [sp, #0x1e0]
	ldp	q20, q21, [sp, #0x1c0]
	ldp	q18, q19, [sp, #0x1a0]
	ldp	q16, q17, [sp, #0x180]
	ldp	q14, q15, [sp, #0x160]
	ldp	q12, q13, [sp, #0x140]
	ldp	q10, q11, [sp, #0x120]
	ldp	q8, q9, [sp, #0x100]
	ldp	q6, q7, [sp, #0xe0]
	ldp	q4, q5, [sp, #0xc0]
	ldp	q2, q3, [sp, #0xa0]
	ldp	q0, q1, [sp, #0x80]

	ldp	x29, x30, [sp, #0x70]
	ldp	x16, x17, [sp, #0x60]
	ldp	x14, x15, [sp, #0x50]
	ldp	x12, x13, [sp, #0x40]
	ldp	x10, x11, [sp, #0x30]
	ldp	x8, x9, [sp, #0x20]
	ldp	x6, x7, [sp, #0x10]
	ldp	x4, x5, [sp]
	add	sp, sp, #0x280
.endm

.macro timer_save_context user
	add	x0, x18, GS_CONTEXT
	stp	x4, x5, [x0, #CONTEXT_X0 + 0x20]
	stp	x6, x7, [x0, #CONTEXT_X0 + 0x30]
	stp	x8, x9, [x0, #CONTEXT_X0 + 0x40]
	stp	x10, x11, [x0, #CONTEXT_X0 + 0x50]
	stp	x12, x13, [x0, #CONTEXT_X0 + 0x60]
	stp	x14, x15, [x0, #CONTEXT_X0 + 0x70]
	stp	x16, x17, [x0, #CONTEXT_X0 + 0x80]

	.if \user
		ldr	x1, [sp, #0x20]
		stp	x1, x19, [x0, #CONTEXT_X0 + 0x90]
	.else
		stp	x18, x19, [x0, #CONTEXT_X0 + 0x90]
	.endif

	stp	x20, x21, [x0, #CONTEXT_X0 + 0xa0]
	stp	x22, x23, [x0, #CONTEXT_X0 + 0xb0]
	stp	x24, x25, [x0, #CONTEXT_X0 + 0xc0]
	stp	x26, x27, [x0, #CONTEXT_X0 + 0xd0]
	stp	x28, fp, [x0, #CONTEXT_X0 + 0xe0]

	.if \user
		ldr	lr, [sp, #0x28]
	.else
		ldr	lr, [sp, #0x20]
	.endif
	str	lr, [x0, #CONTEXT_X0 + 0xf0]

	stp	q0, q1, [x0, #CONTEXT_V0]
	stp	q2, q3, [x0, #CONTEXT_V0 + 0x20]
	stp	q4, q5, [x0, #CONTEXT_V0 + 0x40]
	stp	q6, q7, [x0, #CONTEXT_V0 + 0x60]
	stp	q8, q9, [x0, #CONTEXT_V0 + 0x80]
	stp	q10, q11, [x0, #CONTEXT_V0 + 0xa0]
	stp	q12, q13, [x0, #CONTEXT_V0 + 0xc0]
	stp	q14, q15, [x0, #CONTEXT_V0 + 0xe0]
	stp	q16, q17, [x0, #CONTEXT_V0 + 0x100]
	stp	q18, q19, [x0, #CONTEXT_V0 + 0x120]
	stp	q20, q21, [x0, #CONTEXT_V0 + 0x140]
	stp	q22, q23, [x0, #CONTEXT_V0 + 0x160]
	stp	q24, q25, [x0, #CONTEXT_V0 + 0x180]
	stp	q26, q27, [x0, #CONTEXT_V0 + 0x1a0]
	stp	q28, q29, [x0, #CONTEXT_V0 + 0x1c0]
	stp	q30, q31, [x0, #CONTEXT_V0 + 0x1e0]

	// FIXME - debug registers

	mrs	x1, fpcr
	str	w1, [x0, #CONTEXT_FPCR]

	mrs	x1, fpsr
	str	w1, [x0, #CONTEXT_FPSR]

	mrs	x1, elr_el1
	str	x1, [x0, #CONTEXT_PC]

	mrs	x1, spsr_el1
	str	w1, [x0, #CONTEXT_CPSR]

	add	x3, x18, GS_CURRENT_THREAD
	ldr	x3, [x3]
	add	x2, x3, THREAD_SYSCALL_STACK
	.if \user
		add	x1, sp, #0x30
	.else
		add	x1, sp, #0x40
	.endif
	str	x1, [x2]
	add	x2, x3, THREAD_USER_STACK
	mrs	x1, sp_el0
	str	x1, [x2]

	ldp	x2, x3, [sp, #0x10]
	stp	x2, x3, [x0, #CONTEXT_X0 + 0x10]
	ldr	x1, [sp, #0x8]
	str	x1, [x0, #CONTEXT_X0 + 0x8]
	ldr	x1, [sp]
	str	x1, [x0, #CONTEXT_X0]
.endm

.macro call_func func
	sub	sp, sp, #0x2a0
	stp	x0, x1, [sp]
	stp	x2, x3, [sp, #0x10]
	stp	x4, x5, [sp, #0x20]
	stp	x6, x7, [sp, #0x30]
	stp	x8, x9, [sp, #0x40]
	stp	x10, x11, [sp, #0x50]
	stp	x12, x13, [sp, #0x60]
	stp	x14, x15, [sp, #0x70]
	stp	x16, x17, [sp, #0x80]
	stp	x29, x30, [sp, #0x90]

	stp	q0, q1, [sp, #0xa0]
	stp	q2, q3, [sp, #0xc0]
	stp	q4, q5, [sp, #0xe0]
	stp	q6, q7, [sp, #0x100]
	stp	q8, q9, [sp, #0x120]
	stp	q10, q11, [sp, #0x140]
	stp	q12, q13, [sp, #0x160]
	stp	q14, q15, [sp, #0x180]
	stp	q16, q17, [sp, #0x1a0]
	stp	q18, q19, [sp, #0x1c0]
	stp	q20, q21, [sp, #0x1e0]
	stp	q22, q23, [sp, #0x200]
	stp	q24, q25, [sp, #0x220]
	stp	q26, q27, [sp, #0x240]
	stp	q28, q29, [sp, #0x260]
	stp	q30, q31, [sp, #0x280]

	ldr	x0, [sp]
	bl	\func

	ldp	q30, q31, [sp, #0x280]
	ldp	q28, q29, [sp, #0x260]
	ldp	q26, q27, [sp, #0x240]
	ldp	q24, q25, [sp, #0x220]
	ldp	q22, q23, [sp, #0x200]
	ldp	q20, q21, [sp, #0x1e0]
	ldp	q18, q19, [sp, #0x1c0]
	ldp	q16, q17, [sp, #0x1a0]
	ldp	q14, q15, [sp, #0x180]
	ldp	q12, q13, [sp, #0x160]
	ldp	q10, q11, [sp, #0x140]
	ldp	q8, q9, [sp, #0x120]
	ldp	q6, q7, [sp, #0x100]
	ldp	q4, q5, [sp, #0xe0]
	ldp	q2, q3, [sp, #0xc0]
	ldp	q0, q1, [sp, #0xa0]

	ldp	x29, x30, [sp, #0x90]
	ldp	x16, x17, [sp, #0x80]
	ldp	x14, x15, [sp, #0x70]
	ldp	x12, x13, [sp, #0x60]
	ldp	x10, x11, [sp, #0x50]
	ldp	x8, x9, [sp, #0x40]
	ldp	x6, x7, [sp, #0x30]
	ldp	x4, x5, [sp, #0x20]
	ldp	x2, x3, [sp, #0x10]
	ldp	x0, x1, [sp]
	add	sp, sp, #0x2a0
.endm

// can only use x0, x1, x2, and x3
.macro timer_isr user, bsp
	// start clock again
	adrp	x1, hardware_tick_rate
	add	x1, x1, :lo12:hardware_tick_rate
	ldr	x1, [x1]
	msr	cntp_tval_el0, x1
	mov	x0, #1
	msr	cntp_ctl_el0, x0

	.if \bsp
		// system time

		adrp	x0, kernel_time_page
		add	x0, x0, TIME_PAGE_SYSTEM_TIME
		ldr	x1, [x0]
		mov	x2, #(_SYSTEM_TIME_RESOLUTION / _TICK_RATE)
		add	x1, x1, x2
		str	x1, [x0]

		add	x0, x0, #(TIME_PAGE_SYSTEM_TIME_TSC - TIME_PAGE_SYSTEM_TIME)
		mrs	x1, cntvct_el0
		str	x1, [x0]

		// timers
		bl	acquire_sync_lock

		adrp	x1, active_timer_list  // set x1 to &active_timer_list
		add	x1, x1, :lo12:active_timer_list
		ldr	x0, [x1]  // set x0 to active_timer_list.next

8:
		cmp	x0, x1
		b.eq	7f

		ldr	x2, [x0] // set x2 to le->next
		sub	x0, x0, #TIMER_LE

		// reduce ticks_left
		ldr	x3, [x0, #TIMER_TICKS_LEFT]
		sub	x3, x3, #1
		str	x3, [x0, #TIMER_TICKS_LEFT]
		cbnz	x3, 6f

		call_func timer_expired

6:
		mov	x0, x2
		b	8b

		// wait timeouts
7:
		adrp	x1, thread_list_timeout // set x1 to &thread_list_timeout
		add	x1, x1, :lo12:thread_list_timeout
		ldr	x0, [x1] // set x0 to thread_list_timeout.next

4:
		cmp	x0, x1
		b.eq	5f

		ldr	x2, [x0] // set x2 to le->next
		sub	x0, x0, #THREAD_TIMEOUT_LE

		// reduce wait_timeout
		ldr	x3, [x0, #THREAD_WAIT_TIMEOUT]
		sub	x3, x3, #1
		str	x3, [x0, #THREAD_WAIT_TIMEOUT]
		cbnz	x3, 3f

		call_func wait_timeout_expired

3:
		mov	x0, x2
		b	4b
5:
		bl	release_sync_lock
	.endif

	// decrease time slice

	add	x0, x18, GS_CURRENT_THREAD
	ldr	x0, [x0]
	cbz	x0, 9f

	add	x0, x18, GS_DISABLE_PREEMPT
	ldr	x0, [x0]
	cbnz	x0, 9f

	add	x0, x18, GS_TIME_SLICE_LEFT
	ldr	x1, [x0]
	sub	x1, x1, #1
	str	x1, [x0]
	cbnz	x1, 9f

	// do context switch

	timer_save_context \user

	// set end-of-interrupt
	adrp	x1, timer_irq_no
	add	x1, x1, :lo12:timer_irq_no
	ldr	w1, [x1]
	add	x0, x18, GS_GICC
	ldr	x0, [x0]
	cbnz	x0, 10f

	// GICv3
	msr	ICC_EOIR1_EL1, x1
	b	11f

10:
	// GICv2
	add	x0, x0, GICC_EOIR_OFFSET
	str	w1, [x0]

11:
	bl	acquire_sync_lock

	add	x0, x18, GS_SCHEDULER_STACK
	ldr	x0, [x0]
	mov	sp, x0
	b	scheduler

9:
	// set end-of-interrupt
	adrp	x1, timer_irq_no
	add	x1, x1, :lo12:timer_irq_no
	ldr	w1, [x1]
	add	x0, x18, GS_GICC
	ldr	x0, [x0]
	cbnz	x0, 12f

	// GICv3
	msr	ICC_EOIR1_EL1, x1
	b	13f

12:
	// GICv2
	add	x0, x0, GICC_EOIR_OFFSET
	str	w1, [x0]
13:
.endm

.macro kernel_irq_entry bsp
	// extra 0x10 bytes for ABI
	sub	sp, sp, #0x40

	stp	x0, x1, [sp]
	stp	x2, x3, [sp, #0x10]
	stp	lr, xzr, [sp, #0x20]

	add	x2, x18, GS_GICC
	ldr	x2, [x2]
	cbnz	x2, 1f

	// GICv3
	mrs	x2, ICC_IAR1_EL1
	b	2f

1:
	// GICv2
	add	x2, x2, GICC_IAR_OFFSET
	ldr	w2, [x2]

2:
	adrp	x3, timer_irq_no
	add	x3, x3, :lo12:timer_irq_no
	ldr	w3, [x3]
	sub	w3, w2, w3
	cbnz	w3, 0f
	timer_isr 0, \bsp
	b	kernel_irq_entry_finish

	0:
	do_irq
	b	kernel_irq_entry_finish
.endm

kernel_irq_entry_finish:
ldp	lr, xzr, [sp, #0x20]
ldp	x2, x3, [sp, #0x10]
ldp	x0, x1, [sp]

add	sp, sp, #0x40
eret

kernel_irq_entry_bsp:
kernel_irq_entry 1
kernel_irq_entry_bsp_end:

kernel_irq_entry_ap:
kernel_irq_entry 0
kernel_irq_entry_ap_end:

.macro user_irq_entry bsp
	sub	sp, sp, #0x30

	stp	x0, x1, [sp]
	stp	x2, x3, [sp, #0x10]
	stp	x18, lr, [sp, #0x20]
	mrs	x18, tpidr_el1

	add	x2, x18, GS_GICC
	ldr	x2, [x2]
	cbnz	x2, 15f

	// GICv3
	mrs	x2, ICC_IAR1_EL1
	b	14f

15:
	// GICv2
	add	x2, x2, GICC_IAR_OFFSET
	ldr	w2, [x2]

14:
	adrp	x3, timer_irq_no
	add	x3, x3, :lo12:timer_irq_no
	ldr	w3, [x3]
	sub	w3, w2, w3
	cbnz	w3, 0f
	timer_isr 1, \bsp
	b	user_irq_entry_finish

0:
	do_irq
	b	user_irq_entry_finish
.endm

user_irq_entry_finish:
ldp	x18, lr, [sp, #0x20]
ldp	x2, x3, [sp, #0x10]
ldp	x0, x1, [sp]

add	sp, sp, #0x30
eret

user_irq_entry_bsp:
user_irq_entry 1
user_irq_entry_bsp_end:

user_irq_entry_ap:
user_irq_entry 0
user_irq_entry_ap_end:

.text_end:

.section .debug_info, "dr"

.uleb128 0

.debug_info_end:
