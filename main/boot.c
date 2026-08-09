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
#ifdef __aarch64__
	/* checkm8/pongoOS 'bootr' hands off at EL3, but a Linux (or Redox)
	 * kernel must be entered at EL2/EL1. Prefer EL2 (needed for KVM); fall
	 * back to EL1 when the SoC has no EL2 (e.g. Apple A9/s8003). On boards
	 * entered at EL2/EL1 by their vendor bootloader this is a no-op, so
	 * nothing is printed there. */
	if (arch_current_el() == 3) {
		if (arch_el2_implemented()) {
			printk(KERN_INFO, "EL3 detected, dropping to EL2 before handoff\n");
			arch_el3_to_el2();
		} else {
			printk(KERN_INFO, "EL3 detected (no EL2), dropping to EL1 before handoff\n");
			arch_el3_to_el1();
		}
	}
#endif

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
