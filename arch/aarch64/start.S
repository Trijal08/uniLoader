/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 */
#include "asmdefs.h"

.section .text

#ifdef CONFIG_LINUX_KRNL_HEADER_IMG
	/*
	 * Emit a 64-bit absolute little endian symbol reference in a way that
	 * ensures that it will be resolved at build time, even when building a
	 * PIE binary. This requires cooperation from the linker script, which
	 * must emit the lo32/hi32 halves individually.
	 */
	.macro	le64sym, sym
	.long	\sym\()_lo32
	.long	\sym\()_hi32
	.endm

	.macro linux_image_header entry
	/*
	 * DO NOT MODIFY. Image header expected by Linux boot-loaders.
	 */
	b	\entry
	.long	0				/* reserved */
	le64sym	_kernel_offset_le		/* Image load offset from start of RAM, little-endian */
	le64sym	_kernel_size_le			/* Effective size of kernel image, little-endian */
	le64sym	_kernel_flags_le		/* Informative flags, little-endian */
	.quad	0				/* reserved */
	.quad	0				/* reserved */
	.quad	0				/* reserved */
	.ascii	"ARM\x64"			/* Magic number */
	.long	0				/* reserved */
	.endm
#endif

#if defined(CONFIG_LINUX_KRNL_HEADER_IMG) && !defined (CONFIG_POSITION_INDEPENDENT)
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

