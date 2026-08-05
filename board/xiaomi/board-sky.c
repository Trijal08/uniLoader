/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, Trijal Saha <97483939+Trijal08@users.noreply.github.com>
 */
#include <board.h>
#include <util.h>
#include <drivers/framework.h>
#include <lib/debug.h>
#include <lib/simplefb.h>

static struct video_info sky_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = 1080,
	.height = 2460,
	.stride = 4,
	.address = (void *)0xb8000000,

	// The scale that is set by default is extremely large.
	.scale = 2
};

static const struct device sky_devices[] = {
	{ "simplefb", &sky_fb, "fb" },
};

struct board_data board_ops = {
	.name = "xiaomi-sky",
	.ops = {
	},
	.devices = sky_devices,
	.num_devices = ARRAY_SIZE(sky_devices),
	.quirks = 0
};
