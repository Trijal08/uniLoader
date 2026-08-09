/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 */
#include "asmdefs.h"

#ifdef CONFIG_LINUX_KRNL_HEADER_IMG
#include "linux-kernel-image-header.h"
#endif

.section .text

#if defined(CONFIG_LINUX_KRNL_HEADER_IMG) && !defined(CONFIG_POSITION_INDEPENDENT)
_head:
	linux_image_header _start

_stack_base:
	.quad 0x0

_stack_size:
	.quad _stack_end

.align 4
#endif
ENTRY(_start)
	/* Set up the base address for the stack */
	adrp    x0, _stack_end      /* Load page of the _stack_end symbol */
	add     x0, x0, #:lo12:_stack_end  /* Add the offset within the page */

	/* Set up the stack pointer (SP) */
	mov sp, x0

	/* Zero BSS section */
	adrp    x0, __bss_start
	add     x0, x0, #:lo12:__bss_start
	adrp    x1, __bss_end
	add     x1, x1, #:lo12:__bss_end
1:
	cmp     x0, x1
	b.ge    2f
	str     xzr, [x0], #8
	b       1b
2:

	/* Fall through */
	adrp    x0, dtb             /* Load page of dtb */
	add     x0, x0, #:lo12:dtb  /* Add offset within the page */
	adrp    x1, kernel          /* Load page of kernel */
	add     x1, x1, #:lo12:kernel /* Add offset within the page */
#ifndef CONFIG_RAMDISK_NO_COPY
	adrp    x2, ramdisk          /* Load page of kernel */
	add     x2, x2, #:lo12:ramdisk /* Add offset within the page */
#endif

	bl main
END(_start)

ENTRY(load_kernel_and_jump)
	br	x4
END(load_kernel_and_jump)

/*
 * arch_current_el - return the current Exception Level (0..3).
 */
ENTRY(arch_current_el)
	mrs	x0, CurrentEL
	ubfx	x0, x0, #2, #2		/* CurrentEL[3:2] holds the EL */
	ret
END(arch_current_el)

/*
 * arch_el3_to_el1 - drop from EL3 to EL1 (AArch64, non-secure) and return to
 * the caller running at EL1. Only valid when called at EL3. Apple A-series
 * checkm8 SoCs (e.g. A9/s8003) don't give us a usable EL2, so - like m1n1 - we
 * go straight to EL1. SCTLR_EL1 is cleared so EL1 starts MMU/caches-off in a
 * known state, and the live EL3 stack is aliased to SP_EL1 so C execution
 * continues seamlessly across the eret.
 */
ENTRY(arch_el3_to_el1)
	msr	sctlr_el1, xzr		/* EL1: MMU off, caches off, known-good */

	mrs	x0, scr_el3
	orr	x0, x0, #(1 << 10)	/* RW: EL1 execution state is AArch64 */
	orr	x0, x0, #(1 << 0)	/* NS: EL1 is non-secure */
	msr	scr_el3, x0

	mov	x0, #0x3c5		/* SPSR: DAIF masked (0x3c0) | EL1h (0x5) */
	msr	spsr_el3, x0

	mov	x9, sp
	msr	sp_el1, x9		/* EL1h uses SP_EL1; point it at the live stack */

	msr	elr_el3, x30		/* eret back to the caller... */
	eret				/* ...now executing at EL1 */
END(arch_el3_to_el1)

/*
 * arch_el2_implemented - return nonzero if the CPU implements EL2.
 */
ENTRY(arch_el2_implemented)
	mrs	x0, id_aa64pfr0_el1
	ubfx	x0, x0, #8, #4		/* ID_AA64PFR0_EL1.EL2 field */
	ret
END(arch_el2_implemented)

/*
 * arch_el3_to_el2 - drop from EL3 to EL2 (AArch64, non-secure) and return to
 * the caller running at EL2. Only valid at EL3 on a CPU that implements EL2.
 * SCTLR_EL2 is cleared for a known MMU/caches-off state; the live EL3 stack is
 * aliased to SP_EL2 so C execution continues seamlessly across the eret.
 */
ENTRY(arch_el3_to_el2)
	msr	sctlr_el2, xzr		/* EL2: MMU off, caches off, known-good */

	mrs	x0, scr_el3
	orr	x0, x0, #(1 << 10)	/* RW: EL2 execution state is AArch64 */
	orr	x0, x0, #(1 << 0)	/* NS: non-secure */
	msr	scr_el3, x0

	mov	x0, #0x3c9		/* SPSR: DAIF masked (0x3c0) | EL2h (0x9) */
	msr	spsr_el3, x0

	mov	x9, sp
	msr	sp_el2, x9		/* EL2h uses SP_EL2; point it at the live stack */

	msr	elr_el3, x30		/* eret back to the caller... */
	eret				/* ...now executing at EL2 */
END(arch_el3_to_el2)

