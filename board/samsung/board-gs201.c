/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, Trijal Saha <97483939+Trijal08@users.noreply.github.com>
 */
#include <board.h>
#include <util.h>
#include <drivers/framework.h>
#include <lib/simplefb.h>

#include <soc/gs201.h>

// Early initialization
int gs201_init(void)
{
#ifdef CONFIG_SIMPLE_FB
	decon_init();
#endif
	gs201_disable_wdt();
	return 0;
}

#ifdef CONFIG_SIMPLE_FB
static struct video_info gs201_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = FRAMEBUFFER_WIDTH,
	.height = FRAMEBUFFER_HEIGHT,
	.stride = 4,
	.scale = FRAMEBUFFER_SCALE,
	.address = (void *)0xfac00000
};
#endif

static const struct device gs201_devices[] = {
#ifdef CONFIG_SIMPLE_FB
	{ "simplefb", &gs201_fb, "fb" },
#endif
};

struct board_data board_ops = {
	.name = BOARD_NAME,
	.ops = {
		.early_init = gs201_init,
	},
	.devices = gs201_devices,
	.num_devices = ARRAY_SIZE(gs201_devices),
	.quirks = 0
};
