/*
 * Copyright (C) 2017 Christian Duenki, Escatec Switzerland AG
 */

/* #define DEBUG */

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

/** SPI driver name */
#define SPI_DRIVER_NAME "st7789v"

/** Perform SPI init sequence */
#define DO_SPI_INIT_SEQUENCE
/** Do panning in spi driver */
#define DO_SPI_PAN
/** Register SPI device */
#define DO_REGISTER_SPI_DEVICE
/** Do panning in lcdif driver */
//#define DO_LCDIF_PAN

/** Reset pin responsibility */
/* define RESET_DONE_BY_LCDIF */

#if defined (RESET_DONE_BY_LCDIF)
 #define GPIO_LCDIF_RESET  (int)68
#else
 #define GPIO_SPI_RESET    (int)68
#endif

#define NUMARGS(...) (sizeof((int[]){__VA_ARGS__})/sizeof(int))

#define st7789_write_reg(...)				\
	st7789_write_register(NUMARGS(__VA_ARGS__), __VA_ARGS__)

struct mxsfb_info {
	struct fb_info fb_info;
	struct platform_device *pdev;
	struct clk *clk;
	struct clk *clk_axi;
	struct clk *clk_disp_axi;
	void __iomem *base;	/* registers */
	unsigned allocated_size;
	int enabled;
	unsigned ld_intf_width;
	unsigned dotclk_delay;
	const struct mxsfb_devdata *devdata;
	u32 sync;
	struct regulator *reg_lcd;
#if defined (GPIO_LCDIF_RESET)
	int gpio_reset;
#endif
};
