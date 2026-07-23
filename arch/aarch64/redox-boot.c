// SPDX-License-Identifier: GPL-2.0-only
/*
 * Redox microkernel boot path for uniLoader (AArch64).
 *
 * Ported from redox-boot-shim. uniLoader runs with the MMU off and x0 = the
 * embedded devicetree; the Redox kernel instead wants the MMU on with a
 * specific virtual layout and x0 = &KernelArgs. This builds that handoff:
 *
 *   - place the embedded kernel ELF in free RAM (whole-file copy, as the
 *     Redox linker script makes file layout == memory layout),
 *   - place the embedded initfs (ramdisk) in free RAM as the bootstrap image,
 *   - build page tables (identity + PHYS_OFFSET linear + KERNEL_OFFSET),
 *   - forward a simple-framebuffer as FRAMEBUFFER_* env (mapped non-cacheable),
 *   - synthesize KernelArgs (RAM areas from /memory), drop to EL1 if needed,
 *     enable the MMU, and jump to the ELF entry with x0 = &KernelArgs.
 *
 * The devicetree is uniLoader's own embedded blob, so it must carry the board
 * /memory nodes and the simple-framebuffer node; reserved-memory is left to
 * the kernel, which subtracts it. Board-specific bring-up (e.g. the DECON
 * panel unlock) belongs in the board's early_init, not here.
 */

#include <main/redox-boot.h>
#include <lib/libfdt/libfdt.h>
#include <lib/simplefb.h>
#include <lib/debug.h>
#include <string.h>

#include <generated/autoconf.h>

#define PAGE_SIZE   0x1000UL
#define TWO_MIB     0x200000UL
#define GIB         0x40000000UL

/* Page-table entry bits. */
#define PF_PRESENT          (1UL << 0)
#define PF_TABLE            (1UL << 1)
#define PF_PAGE             (1UL << 1)
#define PF_OUTER_SHAREABLE  (1UL << 8)
#define PF_INNER_SHAREABLE  (3UL << 8)
#define PF_ACCESS           (1UL << 10)

/* MAIR indices programmed below: 0 = WB, 1 = normal NC, 2 = device. */
#define ATTR_NORMAL_WB  (0UL << 2)
#define ATTR_NORMAL_NC  (1UL << 2)
#define ATTR_DEVICE     (2UL << 2)

#define PF_NORMAL_WB    (PF_INNER_SHAREABLE | ATTR_NORMAL_WB)
#define PF_NORMAL_NC    (PF_INNER_SHAREABLE | ATTR_NORMAL_NC)
#define PF_DEV          (PF_OUTER_SHAREABLE | ATTR_DEVICE)

#define ENTRY_ADDR_MASK 0x0000FFFFFFFFF000UL

#define MAX_AREAS       32
#define MAX_RAM_RANGES  16
#define POOL_TABLES     96

/* uniLoader image bounds, so we can reserve the region that holds the page
 * tables / args / env the kernel keeps using after we jump. */
/* Image bounds (linker-script symbols, defined for both PIE and non-PIE)
 * so the placed kernel/initfs avoid uniLoader's own image, which holds the
 * copy sources and page tables. */
extern char _start[];
extern char _stack_end[];
extern unsigned long kernel_size;
extern unsigned long ramdisk_size;

struct table {
	uint64_t entry[512];
} __attribute__((aligned(4096)));

/* Everything the kernel keeps referencing lives here so it can be reserved
 * as one range. The kernel reuses our page tables (its paging::init only
 * sets MAIR), and reads env/args/areas, so none of this may be reclaimed. */
static struct {
	struct table tables[POOL_TABLES];
	unsigned int next_table;

	struct redox_kernel_args args;
	struct redox_memory_entry areas[MAX_AREAS];
	unsigned int area_count;

	uint8_t env[PAGE_SIZE];
	unsigned int env_len;

	uint8_t stack[0x4000] __attribute__((aligned(16)));
} handoff;

struct range {
	uint64_t base;
	uint64_t size;
};

/* ---- devicetree helpers (big-endian cells via libfdt) ------------------ */

static uint32_t be32(const void *p)
{
	const uint8_t *b = p;
	return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
	       ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static uint64_t read_cells(const uint8_t *p, int cells)
{
	uint64_t v = 0;
	for (int i = 0; i < cells; i++)
		v = (v << 32) | be32(p + i * 4);
	return v;
}

static uint32_t prop_u32(const void *fdt, int node, const char *name, uint32_t def)
{
	int len;
	const void *p = fdt_getprop(fdt, node, name, &len);
	if (!p || len < 4)
		return def;
	return be32(p);
}

/* ---- memory areas ------------------------------------------------------- */

static unsigned int collect_ram(const void *fdt, struct range *out, unsigned int max)
{
	int root = fdt_path_offset(fdt, "/");
	/* fdt_address_cells()/fdt_size_cells() live in fdt_addresses.c, which
	 * uniLoader does not build, so read the root properties directly. */
	int addr_cells = prop_u32(fdt, root, "#address-cells", 2);
	int size_cells = prop_u32(fdt, root, "#size-cells", 1);
	unsigned int count = 0;

	for (int node = fdt_next_node(fdt, -1, NULL); node >= 0;
	     node = fdt_next_node(fdt, node, NULL)) {
		int len;
		const void *dt = fdt_getprop(fdt, node, "device_type", &len);
		if (!dt || strcmp(dt, "memory") != 0)
			continue;

		const uint8_t *reg = fdt_getprop(fdt, node, "reg", &len);
		if (!reg)
			continue;

		int stride = (addr_cells + size_cells) * 4;
		for (int off = 0; off + stride <= len && count < max; off += stride) {
			uint64_t base = read_cells(reg + off, addr_cells);
			uint64_t size = read_cells(reg + off + addr_cells * 4, size_cells);
			if (size == 0)
				continue;
			out[count].base = base;
			out[count].size = size;
			count++;
		}
	}
	return count;
}

/* ---- simple-framebuffer ------------------------------------------------- */

#ifdef CONFIG_SIMPLE_FB
struct framebuffer {
	uint64_t base;
	uint32_t width;
	uint32_t height;
	uint32_t stride_px;
	bool present;
};

static uint32_t bytes_per_pixel(video_format fmt)
{
	if (fmt == FB_FORMAT_RGB565)
		return 2;
	return 4; /* FB_FORMAT_ARGB8888, FB_FORMAT_ABGR8888, etc. */
}

static struct framebuffer find_framebuffer(const void *fdt)
{
	extern struct video_info *fb_info;
	struct framebuffer fb = { 0 };

	fb.base = (uint64_t)fb_info->address;
	fb.width = fb_info->width;
	fb.height = fb_info->height;
	if (fb_info->stride > 8) {
		uint32_t bpp = bytes_per_pixel(fb_info->format);
		fb.stride_px = bpp ? (fb_info->stride / bpp) : fb.width;
	} else {
		fb.stride_px = fb.width;
	}

	fb.present = (fb.width > 0 && fb.height > 0 && fb.base != 0);
	return fb;
}
#endif

/* ---- env page ----------------------------------------------------------- */

#ifdef CONFIG_SIMPLE_FB
static void env_line(const char *key, uint64_t value)
{
	char *dst = (char *)handoff.env;
	unsigned int i = handoff.env_len;

	for (const char *k = key; *k && i < sizeof(handoff.env) - 1; k++)
		dst[i++] = *k;

	if (i < sizeof(handoff.env) - 1)
		dst[i++] = '=';

	char tmp[16];
	int n = 0;
	if (value == 0) {
		tmp[n++] = '0';
	} else {
		while (value && n < 16) {
			uint8_t nib = value & 0xf;
			tmp[n++] = nib < 10 ? '0' + nib : 'a' + (nib - 10);
			value >>= 4;
		}
	}
	while (n-- > 0 && i < sizeof(handoff.env) - 1)
		dst[i++] = tmp[n];

	if (i < sizeof(handoff.env) - 1)
		dst[i++] = '\n';

	handoff.env_len = i;
}
#endif

/* ---- page tables -------------------------------------------------------- */

static struct table *alloc_table(void)
{
	if (handoff.next_table >= POOL_TABLES) {
		printk(KERN_EMERG, "redox: page-table pool exhausted\n");
		for (;;)
			;
	}
	return &handoff.tables[handoff.next_table++];
}

static uint64_t table_entry(struct table *t)
{
	return (uint64_t)(uintptr_t)t | PF_ACCESS | PF_TABLE | PF_PRESENT;
}

static uint64_t block_entry(uint64_t addr, uint64_t attr)
{
	return addr | PF_ACCESS | attr | PF_PRESENT;
}

static bool overlaps(uint64_t a, uint64_t asz, uint64_t b, uint64_t bsz)
{
	return a < b + bsz && b < a + asz;
}

/* Find a 2 MiB-aligned physical region of `size` inside RAM that avoids
 * every forbidden range (uniLoader's own image, already-placed blobs).
 * Returns 0 if none fits -- addresses are never 0 on these platforms. */
static uint64_t find_free(const struct range *ram, unsigned int ram_n,
			  const struct range *forbid, unsigned int forb_n,
			  uint64_t size)
{
	for (unsigned int b = 0; b < ram_n; b++) {
		uint64_t cand = (ram[b].base + TWO_MIB - 1) & ~(TWO_MIB - 1);
		while (cand + size <= ram[b].base + ram[b].size) {
			bool clear = true;
			for (unsigned int f = 0; f < forb_n; f++) {
				if (forbid[f].size &&
				    overlaps(cand, size, forbid[f].base, forbid[f].size)) {
					cand = (forbid[f].base + forbid[f].size +
						TWO_MIB - 1) & ~(TWO_MIB - 1);
					clear = false;
					break;
				}
			}
			if (clear)
				return cand;
		}
	}
	return 0;
}

/* Build L0 -> {identity, PHYS_OFFSET linear, KERNEL_OFFSET kernel}. `image`
 * is uniLoader's own footprint: it is physical RAM (we run and hold the copy
 * sources there) even when the DTB /memory does not list its bank, so it must
 * be mapped Normal, or the post-MMU memcpy reads its source as Device. */
static struct table *build_tables(const struct range *ram, unsigned int ram_n,
				  struct range image,
				  uint64_t kernel_phys, uint64_t kernel_span)
{
	struct table *l0 = alloc_table();
	struct table *l1 = alloc_table();

	for (int i = 0; i < 512; i++) {
		uint64_t base = (uint64_t)i * GIB;
		uint64_t attr = PF_DEV;
		for (unsigned int r = 0; r < ram_n; r++) {
			if (overlaps(base, GIB, ram[r].base, ram[r].size)) {
				attr = PF_NORMAL_WB;
				break;
			}
		}
		if (attr == PF_DEV && overlaps(base, GIB, image.base, image.size))
			attr = PF_NORMAL_WB;
		l1->entry[i] = block_entry(base, attr);
	}
	l0->entry[0] = table_entry(l1);
	l0->entry[256] = table_entry(l1);

	struct table *l1k = alloc_table();
	l0->entry[510] = table_entry(l1k);
	uint64_t mapped = 0;
	int l1i = 0;
	while (mapped < kernel_span) {
		struct table *l2 = alloc_table();
		l1k->entry[l1i++] = table_entry(l2);
		int l2i = 0;
		while (mapped < kernel_span && l2i < 512) {
			struct table *l3 = alloc_table();
			l2->entry[l2i++] = table_entry(l3);
			for (int l3i = 0; l3i < 512 && mapped < kernel_span; l3i++) {
				l3->entry[l3i] = (kernel_phys + mapped) | PF_ACCESS |
						 PF_NORMAL_WB | PF_PAGE | PF_PRESENT;
				mapped += PAGE_SIZE;
			}
		}
	}
	return l0;
}

#ifdef CONFIG_SIMPLE_FB
/* Remap the framebuffer non-cacheable in the shared identity+linear L1, so
 * the kernel console reaches DRAM instead of ghosting from stale cache. */
static void framebuffer_nc(struct table *l0, uint64_t fb_base, uint64_t fb_size)
{
	struct table *l1 = (struct table *)(uintptr_t)(l0->entry[256] & ENTRY_ADDR_MASK);
	uint64_t start = fb_base & ~(TWO_MIB - 1);
	uint64_t end = (fb_base + fb_size + TWO_MIB - 1) & ~(TWO_MIB - 1);

	for (uint64_t gib = start >> 30; gib <= (end - 1) >> 30; gib++) {
		struct table *l2 = alloc_table();
		uint64_t gib_base = gib * GIB;
		for (int j = 0; j < 512; j++) {
			uint64_t addr = gib_base + (uint64_t)j * TWO_MIB;
			uint64_t attr = (addr < end && addr + TWO_MIB > start)
					? PF_NORMAL_NC : PF_NORMAL_WB;
			l2->entry[j] = block_entry(addr, attr);
		}
		l1->entry[gib] = table_entry(l2);
	}
	/* No TLB maintenance here: this runs before the MMU is enabled, so
	 * there is nothing cached to invalidate. enable_mmu() does the TLBI
	 * once the tables are installed. An inner-shareable TLBI/dsb ish this
	 * early can stall waiting on a coherency domain that is not up yet. */
}
#endif

/* ---- ELF ---------------------------------------------------------------- */

static uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint64_t le64(const uint8_t *p)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--)
		v = (v << 8) | p[i];
	return v;
}

bool redox_is_kernel(const void *kernel)
{
	const uint8_t *e = kernel;
	if (e[0] != 0x7f || e[1] != 'E' || e[2] != 'L' || e[3] != 'F')
		return false;
	if (e[4] != 2 || e[5] != 1) /* ELF64 little-endian */
		return false;
	uint64_t entry = le64(e + 0x18);
	return entry >= REDOX_KERNEL_OFFSET;
}

/* Returns the entry vaddr; fills *span with the physical span to reserve. */
static uint64_t elf_prepare(const uint8_t *file, uint64_t file_len, uint64_t *span)
{
	uint64_t entry = le64(file + 0x18);
	uint64_t phoff = le64(file + 0x20);
	uint16_t phentsize = le16(file + 0x36);
	uint16_t phnum = le16(file + 0x38);
	uint64_t s = file_len;

	for (uint16_t i = 0; i < phnum; i++) {
		const uint8_t *ph = file + phoff + (uint64_t)i * phentsize;
		if (be32(ph) == 0 && le16(ph) != 1) {
			/* p_type is LE; PT_LOAD == 1 */
		}
		uint32_t p_type = ph[0] | (ph[1] << 8) | (ph[2] << 16) | (ph[3] << 24);
		if (p_type != 1)
			continue;
		uint64_t p_offset = le64(ph + 0x08);
		uint64_t p_vaddr = le64(ph + 0x10);
		uint64_t p_memsz = le64(ph + 0x28);
		/* Redox layout: file offset equals vaddr - KERNEL_OFFSET. */
		if (p_vaddr - REDOX_KERNEL_OFFSET != p_offset) {
			printk(KERN_EMERG, "redox: unexpected ELF layout\n");
			for (;;)
				;
		}
		if (p_offset + p_memsz > s)
			s = p_offset + p_memsz;
	}
	*span = (s + TWO_MIB - 1) & ~(TWO_MIB - 1);
	return entry;
}

/* ---- MMU + jump --------------------------------------------------------- */

static void clean_range_pou(uint64_t base, uint64_t size)
{
	uint64_t ctr;
	asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
	uint64_t line = 4UL << ((ctr >> 16) & 0xf);
	for (uint64_t a = base & ~(line - 1); a < base + size; a += line)
		asm volatile("dc cvau, %0" :: "r"(a) : "memory");
	asm volatile("dsb ish" ::: "memory");
}

/* Turn the MMU on with the built tables. After this, RAM is Normal cached
 * (identity + PHYS_OFFSET), so the large kernel/initfs copies are fast and
 * safe -- with the MMU off everything is Device-nGnRnE, which makes a 40 MB
 * memcpy crawl and can fault on unaligned access. uniLoader keeps running
 * because its image is inside the identity map. */
static void enable_mmu(uint64_t l0)
{
	/* MAIR: Attr0 WB(0xFF), Attr1 NC(0x44), Attr2 device(0x00), Attr3 WT(0xBB). */
	asm volatile("msr mair_el1, %0" :: "r"(0x000000BB0044FFUL));
	uint64_t tcr = 0x1085100510UL, mmfr0;
	asm volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
	tcr |= (mmfr0 & 0x7) << 32;
	asm volatile("msr tcr_el1, %0; isb" :: "r"(tcr));

	asm volatile("dsb sy; msr ttbr1_el1, %0; msr ttbr0_el1, %0; isb;"
		     "dsb ishst; tlbi vmalle1is; dsb ish; isb" :: "r"(l0));

	uint64_t sctlr;
	asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr &= ~0x32802c2UL;
	sctlr |= 0x3485d13dUL;
	asm volatile("msr sctlr_el1, %0; isb" :: "r"(sctlr));

	asm volatile("dsb ish; ic iallu; dsb ish; isb" ::: "memory");
}

static __attribute__((noreturn)) void jump_to_kernel(uint64_t stack,
						     uint64_t entry,
						     const struct redox_kernel_args *args)
{
	register uint64_t x0 asm("x0") = (uint64_t)(uintptr_t)args;
	asm volatile("mov sp, %0; br %1" :: "r"(stack), "r"(entry), "r"(x0));
	__builtin_unreachable();
}

/* If entered at EL2 (as Android bootloaders do), drop to EL1 so the EL1 MMU
 * setup below actually governs execution and the Redox kernel starts at EL1.
 * Only the Redox path needs this; Linux is entered MMU-off at whatever EL the
 * platform bootloader provided. General registers survive the eret; SP is
 * banked, so SP_EL1 is seeded from the current SP. */
static void drop_to_el1(void)
{
	uint64_t el;
	asm volatile("mrs %0, currentel" : "=r"(el));
	if (((el >> 2) & 3) != 2)
		return; /* already EL1 (or nothing we can do at EL0/EL3) */

	asm volatile(
		"mrs x9, cnthctl_el2\n"
		"orr x9, x9, #3\n"          /* allow EL1 physical/virtual timer */
		"msr cnthctl_el2, x9\n"
		"msr cntvoff_el2, xzr\n"
		"mrs x9, midr_el1\n"
		"msr vpidr_el2, x9\n"
		"mrs x9, mpidr_el1\n"
		"msr vmpidr_el2, x9\n"
		"mov x9, #0x33ff\n"         /* disable EL2 coproc traps */
		"msr cptr_el2, x9\n"
		"msr hstr_el2, xzr\n"
		"mov x9, #(3 << 20)\n"      /* enable FP/SIMD at EL1 */
		"msr cpacr_el1, x9\n"
		"mov x9, #0x0800\n"         /* EL1 SCTLR reset value 0x30d00800 */
		"movk x9, #0x30d0, lsl #16\n"
		"msr sctlr_el1, x9\n"
		"mov x9, #(1 << 31)\n"      /* HCR_EL2.RW: EL1 is AArch64 */
		"orr x9, x9, #(1 << 29)\n"  /* HCR_EL2.HCD: disable HVC */
		"msr hcr_el2, x9\n"
		"mov x9, sp\n"              /* SP is banked; seed SP_EL1 */
		"msr sp_el1, x9\n"
		"mrs x9, vbar_el2\n"
		"msr vbar_el1, x9\n"
		"mov x9, #0x3c5\n"          /* SPSR: return to EL1h, DAIF masked */
		"msr spsr_el2, x9\n"
		"adr x9, 1f\n"
		"msr elr_el2, x9\n"
		"eret\n"
		"1:\n"
		::: "x9", "memory");
}

/* ---- entry -------------------------------------------------------------- */

void redox_boot(void *dt, void *kernel, void *ramdisk)
{
	uint64_t klen = (uint64_t)&kernel_size;
	uint64_t rlen = (uint64_t)&ramdisk_size;

	/* Ensure we run (and hand off) at EL1 before touching the MMU. */
	drop_to_el1();

	printk(KERN_INFO, "redox: booting (kernel %lu bytes, initfs %lu bytes)\n",
	       (unsigned long)klen, (unsigned long)rlen);

	if (fdt_check_header(dt) != 0) {
		printk(KERN_EMERG, "redox: embedded devicetree invalid\n");
		return;
	}

	struct range ram[MAX_RAM_RANGES];
	unsigned int ram_n = collect_ram(dt, ram, MAX_RAM_RANGES);
	if (ram_n == 0) {
		printk(KERN_EMERG, "redox: no /memory in embedded devicetree\n");
		return;
	}
	for (unsigned int i = 0; i < ram_n; i++)
		printk(KERN_INFO, "redox: ram %lx..%lx\n",
		       (unsigned long)ram[i].base,
		       (unsigned long)(ram[i].base + ram[i].size));

	/* Small devicetree/ELF reads are fine with the MMU off; the big
	 * copies below are deferred until after it is on. */
	uint64_t span;
	uint64_t entry = elf_prepare(kernel, klen, &span);
	uint64_t bootstrap_span = (rlen + TWO_MIB - 1) & ~(TWO_MIB - 1);

#ifdef CONFIG_SIMPLE_FB
	struct framebuffer fb = find_framebuffer(dt);
	uint64_t fb_size = 0;
	if (fb.present) {
		fb_size = (uint64_t)fb.stride_px * fb.height * 4;
		printk(KERN_INFO, "redox: inherited uniLoader framebuffer %ux%u stride %u at %lx\n",
		       fb.width, fb.height, fb.stride_px, (unsigned long)fb.base);
	}
#endif

	/* Place the kernel and initfs in free RAM, avoiding uniLoader's own
	 * image (which holds the copy sources and our page tables). */
	struct range forbid[2];
	forbid[0].base = (uint64_t)(uintptr_t)_start;
	forbid[0].size = (uint64_t)(uintptr_t)_stack_end - forbid[0].base;

	uint64_t kernel_phys = find_free(ram, ram_n, forbid, 1, span);
	forbid[1].base = kernel_phys;
	forbid[1].size = span;
	uint64_t bootstrap_phys = find_free(ram, ram_n, forbid, 2, bootstrap_span);
	if (!kernel_phys || !bootstrap_phys) {
		printk(KERN_EMERG, "redox: no free RAM for kernel/initfs\n");
		return;
	}
	printk(KERN_INFO, "redox: kernel at %lx, bootstrap at %lx\n",
	       (unsigned long)kernel_phys, (unsigned long)bootstrap_phys);

	struct table *l0 = build_tables(ram, ram_n, forbid[0], kernel_phys, span);

#ifdef CONFIG_SIMPLE_FB
	if (fb.present)
		framebuffer_nc(l0, fb.base, fb_size);

	/* Build the env (framebuffer geometry for kernel console + vesad). */
	handoff.env_len = 0;
	if (fb.present) {
		env_line("FRAMEBUFFER_ADDR", fb.base);
		env_line("FRAMEBUFFER_VIRT", REDOX_PHYS_OFFSET + fb.base);
		env_line("FRAMEBUFFER_WIDTH", fb.width);
		env_line("FRAMEBUFFER_HEIGHT", fb.height);
		env_line("FRAMEBUFFER_STRIDE", fb.stride_px);
	}
#endif

	/* Memory areas: all RAM Free, plus this loader's handoff region
	 * Reserved (it holds the page tables/env/args the kernel keeps
	 * using -- its paging::init only sets MAIR, reusing our tables). */
	handoff.area_count = 0;
	for (unsigned int i = 0; i < ram_n && handoff.area_count < MAX_AREAS; i++) {
		handoff.areas[handoff.area_count].base = ram[i].base;
		handoff.areas[handoff.area_count].size = ram[i].size;
		handoff.areas[handoff.area_count].kind = REDOX_MEM_FREE;
		handoff.area_count++;
	}
	if (handoff.area_count < MAX_AREAS) {
		handoff.areas[handoff.area_count].base = (uint64_t)(uintptr_t)&handoff;
		handoff.areas[handoff.area_count].size =
			(sizeof(handoff) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		handoff.areas[handoff.area_count].kind = REDOX_MEM_RESERVED;
		handoff.area_count++;
	}

	uint64_t stack_top = (uint64_t)(uintptr_t)handoff.stack + sizeof(handoff.stack);

	handoff.args.kernel_base = kernel_phys;
	handoff.args.kernel_size = span;
	handoff.args.stack_base = (uint64_t)(uintptr_t)handoff.stack;
	handoff.args.stack_size = sizeof(handoff.stack);
	handoff.args.env_base = (uint64_t)(uintptr_t)handoff.env;
	handoff.args.env_size = handoff.env_len;
	handoff.args.hwdesc_base = (uint64_t)(uintptr_t)dt;
	handoff.args.hwdesc_size = fdt_totalsize(dt);
	handoff.args.areas_base = (uint64_t)(uintptr_t)handoff.areas;
	handoff.args.areas_size = handoff.area_count * sizeof(struct redox_memory_entry);
	handoff.args.bootstrap_base = bootstrap_phys;
	handoff.args.bootstrap_size = (rlen + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* Turn the MMU on so the copies run over Normal cached RAM. */
	enable_mmu((uint64_t)(uintptr_t)l0);

	memcpy((void *)(uintptr_t)kernel_phys, kernel, klen);
	memset((void *)(uintptr_t)(kernel_phys + klen), 0, span - klen);
	memcpy((void *)(uintptr_t)bootstrap_phys, ramdisk, rlen);

	/* Make the freshly written kernel image visible to instruction fetch. */
	clean_range_pou(kernel_phys, span);
	asm volatile("ic iallu; dsb ish; isb" ::: "memory");

#ifdef CONFIG_SIMPLE_FB
	/* Panel is now mapped non-cacheable; clear leftover content to black. */
	if (fb.present) {
		memset((void *)(uintptr_t)fb.base, 0, fb_size);
		asm volatile("dsb sy" ::: "memory");
	}
#endif

	printk(KERN_INFO, "redox: entering kernel at %lx\n", (unsigned long)entry);
	jump_to_kernel(stack_top + REDOX_PHYS_OFFSET, entry, &handoff.args);
}
