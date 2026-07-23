// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <lib/debug.h>
#include <main/boot.h>
#include <main/boot-fdt.h>
#include <string.h>
#include <main/main.h>
#include <board.h>

extern struct board_data board_ops;

#ifdef CONFIG_REDOX_BOOT
#include <main/redox-boot.h>

/* arm64 Linux Image magic "ARM\x64" at offset 0x38 (booting.rst). */
static bool is_linux_image(const void *kernel)
{
	const unsigned char *b = kernel;
	return b[0x38] == 'A' && b[0x39] == 'R' &&
	       b[0x3a] == 'M' && b[0x3b] == 0x64;
}
#endif

void boot_kernel(void* dt, void* kernel, void* ramdisk)
{
	printk(KERN_INFO, "Booting kernel...\n");

#ifdef CONFIG_REDOX_BOOT
	/* Choose the boot path by kernel magic. A Redox kernel is an ELF
	 * linked high; a Linux kernel is an arm64 Image. */
	if (redox_is_kernel(kernel)) {
		/* Redox: MMU on, x0 = &KernelArgs, embedded DTB used as-is
		 * (no Linux initrd fixup). */
		redox_boot(dt, kernel, ramdisk);
		return;
	}
	if (!is_linux_image(kernel)) {
		printk(KERN_EMERG,
		       "Embedded kernel is neither a Redox ELF nor an arm64 Image\n");
		return;
	}
#endif

	INITCALL(board_ops.ops.exit);

#ifdef CONFIG_LIBFDT
	patch_dtb(&dt);
#endif

	arch_load_kernel(kernel, dt, ramdisk);
}
