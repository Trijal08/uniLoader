/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, Trijal Saha <97483939+Trijal08@users.noreply.github.com>
 */

#include <stdint.h>
#include <string.h>
#include <lib/console.h>
#include <lib/debug.h>

#ifndef GS201_H_	/* Include guard */
#define GS201_H_

#define DECON_F_BASE		0x1c240000
#define HW_SW_TRIG_CONTROL	0x30

#define GS201_UART_BASE		0x10a00000
#define GS201_UART_UFCON		0x08
#define GS201_UART_UTRSTAT	0x10
#define GS201_UART_UFSTAT	0x18
#define GS201_UART_UTXH		0x20

#define GS201_UART_UFCON_FIFOMODE	(1 << 0)
#define GS201_UART_UTRSTAT_TXFE		(1 << 1)
#define GS201_UART_UFSTAT_TXFULL		(1 << 24)

#define GS201_WDT_BASE			0x10060000
#define GS201_WDT_WTCON			0x00
#define GS201_WDT_WTDAT			0x04
#define GS201_WDT_WTCNT			0x08
#define GS201_WDT_WTCLRINT		0x0c

#define GS201_WDT_WTCON_ENABLE		(1 << 5)

#define GS201_PMU_BASE			0x18060000
#define GS201_PMU_CLUSTER0_NONCPU_OUT	0x1220
#define GS201_PMU_WDT_CNT_EN		(1 << 8)

void decon_init(void) {
    /* Allow framebuffer to be written to */
    *(volatile uint32_t *)(DECON_F_BASE + HW_SW_TRIG_CONTROL) = 0x3061;
}

#ifdef CONFIG_EARLYCON
void uart_putc(char ch)
{
	volatile uint32_t *uart_base = (volatile uint32_t *)GS201_UART_BASE;

	if (readl(uart_base + (GS201_UART_UFCON / sizeof(*uart_base))) &
	    GS201_UART_UFCON_FIFOMODE) {
		while (readl(uart_base + (GS201_UART_UFSTAT / sizeof(*uart_base))) &
		       GS201_UART_UFSTAT_TXFULL)
			;
	} else {
		while (!(readl(uart_base + (GS201_UART_UTRSTAT / sizeof(*uart_base))) &
			 GS201_UART_UTRSTAT_TXFE))
			;
	}

	writel((unsigned int)ch, (void *)(GS201_UART_BASE + GS201_UART_UTXH));
}

void uart_puts(const char *s)
{
	while (*s != '\0') {
		uart_putc(*s);
		s++;
	}
}
#endif

void gs201_disable_wdt(void)
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


/* ================================================================
 * =                                                              =
 * = General specifications for each Pixel 7 series device target =
 * =                                                              =
 * ================================================================ */
#ifdef CONFIG_GOOGLE_CHEETAH
#define FRAMEBUFFER_WIDTH 1440
#define FRAMEBUFFER_HEIGHT 3120
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-cheetah"
#endif

#ifdef CONFIG_GOOGLE_PANTHER
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2400
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-panther"
#endif

#ifdef CONFIG_GOOGLE_LYNX
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2400
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-lynx"
#endif

#ifdef CONFIG_GOOGLE_FELIX
#define FRAMEBUFFER_WIDTH 1840
#define FRAMEBUFFER_HEIGHT 2208
#define FRAMEBUFFER_SCALE 3

#define BOARD_NAME "google-felix"
#endif

#ifdef CONFIG_GOOGLE_TANGORPRO
#define FRAMEBUFFER_WIDTH 1600
#define FRAMEBUFFER_HEIGHT 2560
#define FRAMEBUFFER_SCALE 3

#define BOARD_NAME "google-tangorpro"
#endif

#endif // GS201_H_
