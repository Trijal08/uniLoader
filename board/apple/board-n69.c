/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, Trijal Saha <97483939+Trijal08@users.noreply.github.com>
 */
#include <board.h>
#include <util.h>
#include <drivers/framework.h>
#include <lib/simplefb.h>

static struct video_info n69_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = 640,
	.height = 1136,
	.stride = 4,
	.address = (void *)0x87ea5c000
};

static const struct device n69_devices[] = {
	{ "simplefb", &n69_fb, "fb" },
};

struct board_data board_ops = {
	.name = "apple-n69",
	.ops = {
	},
	.devices = n69_devices,
	.num_devices = ARRAY_SIZE(n69_devices),
	.quirks = 0
};
