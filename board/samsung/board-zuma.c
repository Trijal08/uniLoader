/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, Trijal Saha <97483939+Trijal08@users.noreply.github.com>
 */
#include <board.h>
#include <drivers/framework.h>
#include <lib/simplefb.h>

#include <soc/zuma.h>

// volatile unsigned int* uart = (unsigned int*)0x10870000;
//
// void uart_putc(char ch)
// {
// 	*uart = ch;
// }
//
// void uart_puts(const char *s)
// {
// 	while (*s != '\0')
// 	{
// 		uart_putc(*s);
// 		s++;
// 	}
// }

// Early initialization
int zuma_init(void)
{
	decon_init();
	return 0;
}

#ifdef CONFIG_SIMPLE_FB
static struct video_info zuma_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = FRAMEBUFFER_WIDTH,
	.height = FRAMEBUFFER_HEIGHT,
	.stride = 4,
	.address = (void *)0xfac00000
};
#endif

int zuma_drv(void)
{
#ifdef CONFIG_SIMPLE_FB
	REGISTER_DRIVER("simplefb", simplefb_probe, &zuma_fb);
#endif
	return 0;
}

struct board_data board_ops = {
	.name = BOARD_NAME,
	.ops = {
		.early_init = zuma_init,
		.drivers_init = zuma_drv,
	},
	.quirks = 0
};
