// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <main/boot.h>
#include <string.h>

static void sync_payload_for_exec(void *addr, unsigned long size)
{
	unsigned long ctr_el0;
	unsigned long line_size;
	unsigned long start;
	unsigned long end;

	asm volatile("mrs %0, ctr_el0" : "=r" (ctr_el0));
	line_size = 4UL << ((ctr_el0 >> 16) & 0xf);
	start = (unsigned long)addr & ~(line_size - 1);
	end = ((unsigned long)addr + size + line_size - 1) & ~(line_size - 1);

	for (unsigned long p = start; p < end; p += line_size)
		asm volatile("dc cvau, %0" :: "r" (p) : "memory");

	asm volatile("dsb ish" ::: "memory");

	for (unsigned long p = start; p < end; p += line_size)
		asm volatile("ic ivau, %0" :: "r" (p) : "memory");

	asm volatile("dsb ish" ::: "memory");
	asm volatile("isb" ::: "memory");
}

void arch_load_kernel(void* kernel, void* dt, void* ramdisk)
{
	memcpy((void*)CONFIG_PAYLOAD_ENTRY, kernel, (unsigned long) &kernel_size);
	sync_payload_for_exec((void *)CONFIG_PAYLOAD_ENTRY,
				   (unsigned long)&kernel_size);
#ifndef CONFIG_RAMDISK_NO_COPY
	__optimized_memcpy((void*)CONFIG_RAMDISK_ENTRY, ramdisk, (unsigned long) &ramdisk_size);
	sync_payload_for_exec((void *)CONFIG_RAMDISK_ENTRY,
							   (unsigned long)&ramdisk_size);
#endif
	load_kernel_and_jump(dt, 0, 0, 0, (void*)CONFIG_PAYLOAD_ENTRY);
}
