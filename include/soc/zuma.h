/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, Trijal Saha <97483939+Trijal08@users.noreply.github.com>
 */

#include <stdint.h>
#include <string.h>
#include <lib/console.h>
#include <lib/debug.h>

#ifndef ZUMA_H_	/* Include guard */
#define ZUMA_H_

#define DECON_F_BASE		0x19470000
#define HW_SW_TRIG_CONTROL	0x30

void decon_init(void) {
    /* Allow framebuffer to be written to */
    *(volatile uint32_t *)(DECON_F_BASE + HW_SW_TRIG_CONTROL) = 0x3061;
}

/* ================================================================
 * =                                                              =
 * = General specifications for each Pixel 9 series device target =
 * =                                                              =
 * ================================================================ */
#ifdef CONFIG_GOOGLE_KOMODO
#define FRAMEBUFFER_WIDTH 1344
#define FRAMEBUFFER_HEIGHT 2992
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-komodo"
#endif

#ifdef CONFIG_GOOGLE_CAIMAN
#define FRAMEBUFFER_WIDTH 1280
#define FRAMEBUFFER_HEIGHT 2856
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-caiman"
#endif

#ifdef CONFIG_GOOGLE_TOKAY
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2424
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-tokay"
#endif

#ifdef CONFIG_GOOGLE_TEGU
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2424
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-tegu"
#endif

#ifdef CONFIG_GOOGLE_COMET
#define FRAMEBUFFER_WIDTH 2076
#define FRAMEBUFFER_HEIGHT 2152
#define FRAMEBUFFER_SCALE 3

#define BOARD_NAME "google-comet"
#endif

/* ================================================================
 * =                                                              =
 * = General specifications for each Pixel 8 series device target =
 * =                                                              =
 * ================================================================ */
#ifdef CONFIG_GOOGLE_HUSKY
#define FRAMEBUFFER_WIDTH 1344
#define FRAMEBUFFER_HEIGHT 2992
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-husky"
#endif

#ifdef CONFIG_GOOGLE_SHIBA
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2400
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-shiba"
#endif

#ifdef CONFIG_GOOGLE_AKITA
#define FRAMEBUFFER_WIDTH 1080
#define FRAMEBUFFER_HEIGHT 2400
#define FRAMEBUFFER_SCALE 2

#define BOARD_NAME "google-akita"
#endif

#endif // ZUMA_H_
