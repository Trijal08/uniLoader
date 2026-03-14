/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, Trijal Saha <97483939+Trijal08@users.noreply.github.com>
 */

#ifndef EXYNOS990_H_	/* Include guard */
#define EXYNOS990_H_

#define DECON_F_BASE		0x19470000
#define HW_SW_TRIG_CONTROL	0x30

#define ZUMA_UART_BASE		0x10870000
#define ZUMA_UART_UFCON		0x08
#define ZUMA_UART_UTRSTAT	0x10
#define ZUMA_UART_UFSTAT	0x18
#define ZUMA_UART_UTXH		0x20

#define ZUMA_UART_UFCON_FIFOMODE	(1 << 0)
#define ZUMA_UART_UTRSTAT_TXFE		(1 << 1)
#define ZUMA_UART_UFSTAT_TXFULL		(1 << 24)

#define ZUMA_WDT_BASE			0x10060000
#define ZUMA_WDT_WTCON			0x00
#define ZUMA_WDT_WTDAT			0x04
#define ZUMA_WDT_WTCNT			0x08
#define ZUMA_WDT_WTCLRINT		0x0c

#define ZUMA_WDT_WTCON_ENABLE		(1 << 5)

#define ZUMA_PMU_BASE			0x15460000
#define ZUMA_PMU_CLUSTER0_NONCPU_OUT	0x1220
#define ZUMA_PMU_WDT_CNT_EN		(1 << 8)

void decon_init(void) {
    /* Allow framebuffer to be written to */
    *(int*) (DECON_F_BASE + HW_SW_TRIG_CONTROL) = 0x3061;
}

void uart_putc(char ch)
{
	volatile uint32_t *uart_base = (volatile uint32_t *)ZUMA_UART_BASE;

	if (readl(uart_base + (ZUMA_UART_UFCON / sizeof(*uart_base))) &
	    ZUMA_UART_UFCON_FIFOMODE) {
		while (readl(uart_base + (ZUMA_UART_UFSTAT / sizeof(*uart_base))) &
		       ZUMA_UART_UFSTAT_TXFULL)
			;
	} else {
		while (!(readl(uart_base + (ZUMA_UART_UTRSTAT / sizeof(*uart_base))) &
			 ZUMA_UART_UTRSTAT_TXFE))
			;
	}

	writel((unsigned int)ch, (void *)(ZUMA_UART_BASE + ZUMA_UART_UTXH));
}

void uart_puts(const char *s)
{
	while (*s != '\0') {
		uart_putc(*s);
		s++;
	}
}

void zuma_disable_wdt(void)
{
	/* Disable watchdogs for cluster 0 and 1 */
	/* watchdog_cl0 */
	volatile int val = *(int*)(0x10060000 + 0x00);
	val &= ~((1 << 5) | (1 << 2));
	*(int*)(0x10060000) = val;
	/* watchdog_cl1 */
	val = *(int*)(0x10070000 + 0x00);
	val &= ~((1 << 5) | (1 << 2));
	*(int*)(0x10070000) = val;
}

/*
 *
 * General specifications for each Pixel 9 series device target
 *
 */
#ifdef CONFIG_GOOGLE_KOMODO
#define FRAMEBUFFER_WIDTH 1344
#define FRAMEBUFFER_HEIGHT 2992

#define BOARD_NAME "google-komodo"
#endif

#ifdef CONFIG_GOOGLE_CAIMAN
#define FRAMEBUFFER_WIDTH 1280
#define FRAMEBUFFER_HEIGHT 2856

#define BOARD_NAME "google-caiman"
#endif

#ifdef CONFIG_GOOGLE_TOKAY
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2424

#define BOARD_NAME "google-tokay"
#endif

#ifdef CONFIG_GOOGLE_TEGU
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2424

#define BOARD_NAME "google-tegu"
#endif

#ifdef CONFIG_GOOGLE_COMET
#define FRAMEBUFFER_WIDTH 2076
#define FRAMEBUFFER_HEIGHT 2152

#define BOARD_NAME "google-comet"
#endif

/*
 *
 * General specifications for each Pixel 8 series device target
 *
 */
#ifdef CONFIG_GOOGLE_HUSKY
#define FRAMEBUFFER_WIDTH 1344
#define FRAMEBUFFER_HEIGHT 2992

#define BOARD_NAME "google-husky"
#endif

#ifdef CONFIG_GOOGLE_SHIBA
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2400

#define BOARD_NAME "google-shiba"
#endif

#ifdef CONFIG_GOOGLE_AKITA
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2400

#define BOARD_NAME "google-akita"
#endif

#ifdef CONFIG_GOOGLE_FELIX
#define FRAMEBUFFER_WIDTH 1840
#define FRAMEBUFFER_HEIGHT 2208

#define BOARD_NAME "google-felix"
#endif

#endif // EXYNOS990_H_
