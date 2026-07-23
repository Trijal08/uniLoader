/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Redox microkernel boot path for uniLoader.
 *
 * uniLoader boots Linux by jumping to an arm64 Image with the MMU off and
 * x0 = DTB. The Redox kernel instead expects the MMU already on, a specific
 * virtual layout, and x0 = &KernelArgs (the handoff the Redox bootloader
 * normally provides). This path detects a Redox kernel ELF among the
 * embedded blobs and performs that handoff.
 *
 * Ported from redox-boot-shim.
 */

#ifndef REDOX_BOOT_H_
#define REDOX_BOOT_H_

#include <stdbool.h>
#include <stdint.h>

/* Kernel linked here; identity map + PHYS_OFFSET linear map also built. */
#define REDOX_KERNEL_OFFSET 0xFFFFFF0000000000UL
#define REDOX_PHYS_OFFSET   0xFFFF800000000000UL

/* Keep synced with KernelArgs in the Redox kernel (src/startup/mod.rs). */
struct redox_kernel_args {
	uint64_t kernel_base;
	uint64_t kernel_size;

	uint64_t stack_base;
	uint64_t stack_size;

	uint64_t env_base;
	uint64_t env_size;

	/* Devicetree blob (the kernel calls this hwdesc). */
	uint64_t hwdesc_base;
	uint64_t hwdesc_size;

	uint64_t areas_base;
	uint64_t areas_size;

	uint64_t bootstrap_base;
	uint64_t bootstrap_size;
};
/* All-u64, so the natural layout matches Redox's repr(C, packed(8)). Do NOT
 * mark packed(1): with the MMU off (Device memory), unaligned 64-bit access
 * faults, and packing this after a u32 in the handoff struct misaligns it. */

/* Keep synced with BootloaderMemoryKind in the Redox kernel. */
enum redox_memory_kind {
	REDOX_MEM_NULL     = 0,
	REDOX_MEM_FREE     = 1,
	REDOX_MEM_RECLAIM  = 2,
	REDOX_MEM_RESERVED = 3,
};

/* Keep synced with BootloaderMemoryEntry in the Redox kernel. */
struct redox_memory_entry {
	uint64_t base;
	uint64_t size;
	uint64_t kind;
};

/* True if the blob looks like a Redox kernel ELF (ELF64 linked high). */
bool redox_is_kernel(const void *kernel);

/* Boot the Redox kernel. Does not return on success. */
void redox_boot(void *dt, void *kernel, void *ramdisk);

#endif /* REDOX_BOOT_H_ */
