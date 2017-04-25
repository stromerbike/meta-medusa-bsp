/*
 * Copyright (C) 2017 Christian Duenki, Escatec Switzerland AG
 *
 * This driver is inspired by:
 *   fbtft, Copyright (C) 2013 Noralf Tronnes
 *   fb_st7789v, Copyright (C) 2015 Dennis Menschel
 */

#include "fb_imx6.h"

#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <video/mipi_display.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/kernel.h>

#define DEFAULT_GAMMA \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25\n" \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25"

/**
 * enum st7789v_command - ST7789V display controller commands
 *
 * @PORCTRL: porch setting
 * @GCTRL: gate control
 * @VCOMS: VCOM setting
 * @VDVVRHEN: VDV and VRH command enable
 * @VRHS: VRH set
 * @VDVS: VDV set
 * @VCMOFSET: VCOM offset set
 * @PWCTRL1: power control 1
 * @PVGAMCTRL: positive voltage gamma control
 * @NVGAMCTRL: negative voltage gamma control
 *
 * The command names are the same as those found in the datasheet to ease
 * looking up their semantics and usage.
 *
 * Note that the ST7789V display controller offers quite a few more commands
 * which have been omitted from this list as they are not used at the moment.
 * Furthermore, commands that are compliant with the MIPI DCS have been left
 * out as well to avoid duplicate entries.
 */

enum st7789v_command {
	RAMWR = 0x2C,
	RAMCTRL = 0xB0,
	RGBCTRL = 0xB1,
	PORCTRL = 0xB2,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	LCMCTRL = 0xC0,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	FRCTRL2 = 0xC6,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
};

struct st7789_driver_par {
	struct spi_device *spi;
	struct fb_info *p_fb_info;
#if defined (GPIO_SPI_RESET)
	int gpio_reset;
#endif
	u8 *buf;
};

#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_MV BIT(5) /* bitmask for page/column order */
#define MADCTL_MX BIT(6) /* bitmask for column address order */
#define MADCTL_MY BIT(7) /* bitmask for page address order */

extern void mxsfb_enable_controller(struct fb_info *fb_info);
extern void mxsfb_disable_controller(struct fb_info *fb_info);
extern int mxsfb_pan_display(struct fb_var_screeninfo *var,
							 struct fb_info *fb_info);

void st7789_write_register(int len, ...);
int st7789_write_spi(void *buf, size_t len);
int st7789_trx_spi(void *buf_out, void *buf_in, size_t trx_len);
int st7789_init_display(void);

/** Local driver information */
static struct st7789_driver_par disp;

void st7789_write_register(int len, ...)
{
	va_list args;
	u16 *buf = NULL;
	int res, cnt = 0;

	buf = (u16*)kmalloc((size_t)(len * sizeof(u16)), GFP_NOIO);

	va_start(args, len);
	for (cnt = 0; cnt < len; cnt++) {
		buf[cnt] = (u16)(((u16)va_arg(args, unsigned int) & 0x00ff) | 0x0100);
	}
	va_end(args);

	/* Set data(!command) bit of forst byte */
	buf[0] &= 0x00ff;
#if defined (DEBUG)
	pr_debug("SPI Data: 0x%04x", buf[0]);
	for (cnt = 1; cnt < len; cnt++) {
		pr_debug(", 0x%04x", buf[cnt]);
	}
	pr_debug("\n");
#endif

	res = st7789_write_spi(buf, (size_t)(sizeof(u16) * len));

	kfree(buf);

	return;
}

/**
 * @brief Transmit spi messages to the stromer-display
 *
 * @param buf[in] Data to be sent
 * @param len[in] Length of message
 * @return Success
 */
int st7789_write_spi(void *buf, size_t len)
{
	struct spi_device *spi = disp.spi;
	struct spi_message m;
	struct spi_transfer t = {
		.tx_buf = buf,
		.len = len,
	};

	if (!spi) {
		pr_err("%s: spi is unexpectedly NULL\n", __func__);
		return PTR_ERR(spi);
	}

	spi_message_init(&m);

	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}
/* EXPORT_SYMBOL(st7789_write_spi); */

/**
 * @brief Transmit and receive spi messages
 *
 * @param buf_out[in] Buffer containing data to be sent
 * @param buf_in[out] Buffer to store received data
 * @param trx_len[in] Length of transmitt and receive (sum)
 * @return zero on success, else a negative error code
 */
int st7789_trx_spi(void *buf_out, void *buf_in,
				   size_t trx_len)
{
	struct spi_device *spi = disp.spi;
	struct spi_message m;
	struct spi_transfer t = {
		.tx_buf = buf_out,
		.rx_buf = buf_in,
		.len = trx_len,
	};

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return spi_sync(spi, &m);
}
/* EXPORT_SYMBOL(st7789_trx_spi); */

#if 0
/**
 * blank() - blank the display
 *
 * @on: whether to enable or disable blanking the display
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int st7789_spi_blank(bool on)
{
	if (on)
		st7789_write_reg(MIPI_DCS_SET_DISPLAY_OFF);
	else
		st7789_write_reg(MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}
/* EXPORT_SYMBOL(st7789_spi_blank); */
#endif

/**
 * init_display() - initialize the display controller
 *
 * Most of the commands in this init function set their parameters to the
 * same default values which are already in place after the display has been
 * powered up. (The main exception to this rule is the pixel format which
 * would default to 18 instead of 16 bit per pixel.)
 * Nonetheless, this sequence can be used as a template for concrete
 * displays which usually need some adjustments.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
int st7789_init_display(void)
{
#if defined (GPIO_SPI_RESET)
	/* Apply reset sequence */
	if (gpio_is_valid(disp.gpio_reset)) {
		/* dummy write to activate spi lines */
		st7789_write_reg(MIPI_DCS_EXIT_SLEEP_MODE);

		/* Apply clean reset sequence */
		gpio_set_value(disp.gpio_reset, 1);
		pr_debug("Set Display reset\n");
		mdelay(1);
		gpio_set_value(disp.gpio_reset, 0);
		pr_debug("Clear Display reset\n");
		mdelay(10);
		gpio_set_value(disp.gpio_reset, 1);
		pr_debug("Set Display reset\n");
		mdelay(120);
	}
#endif
	/* turn off sleep mode */
	st7789_write_reg(MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	/* Memory data access contrl */
	st7789_write_reg(MIPI_DCS_SET_ADDRESS_MODE, 0x00);

	/* Porch settings */
	st7789_write_reg(PORCTRL, 0x0c, 0x0c, 0x00,  0x33, 0x33);

	/* Gate control
	 * VGH = 13.26V
	 * VGL = -10.43V
	 */
	st7789_write_reg(GCTRL, 0x35);

	/* VCOM = 1.175 V */
	st7789_write_reg(VCOMS, 0x2B);

	/* LCMCTRL */
	st7789_write_reg(LCMCTRL, 0x2C);

	/*
	 * VDV and VRH register values come from command write
	 * (instead of NVM)
	 */
	st7789_write_reg(VDVVRHEN, 0x01);

	/*
	 * VAP =  4.1V + (VCOM + VCOM offset + 0.5 * VDV)
	 * VAN = -4.1V + (VCOM + VCOM offset + 0.5 * VDV)
	 */
	st7789_write_reg(VRHS, 0x20);

	/* VDV = 0V */
	st7789_write_reg(VDVS, 0x20);

	/* Frame rate control */
	st7789_write_reg(FRCTRL2, 0x0F);

	/*
	 * AVDD = 6.8V
	 * AVCL = -4.8V
	 * VDS = 2.3V
	 */
	st7789_write_reg(PWCTRL1, 0xA4, 0xA1);

	/* Positive voltage gamma control */
	st7789_write_reg(PVGAMCTRL, 0xD0, 0xCA, 0xE0, 0x08, 0x09, 0x07, 0x2D, 0x3B,
					 0x3D, 0x34, 0x0A, 0x0A, 0x1B, 0x28);

	/* Negative voltage gamma control */
	st7789_write_reg(NVGAMCTRL, 0xD0, 0xCA, 0x0F, 0x08, 0x08, 0x07, 0x2E, 0x5C,
					 0X40, 0x34, 0x09, 0x0B, 0x1B, 0x28);

	/* st7789_write_reg(MIPI_DCS_ENTER_INVERT_MODE); */

	/* RAM control */
	st7789_write_reg(RAMCTRL, 0x11, 0x00, 0x00);

	/* RGB Interface control */
	st7789_write_reg(RGBCTRL, 0x40, 0x04, 0x0A);

	/* set pixel format to RGB-565 */
	st7789_write_reg(MIPI_DCS_SET_PIXEL_FORMAT, 0x55);

	/* Frame rate control */
	st7789_write_reg(MIPI_DCS_SET_COLUMN_ADDRESS, 0x00, 0x00, 0x00, 0xEF);
	/* st7789_write_reg(MIPI_DCS_SET_COLUMN_ADDRESS, 0x00, 0x00, 0x01, 0x3F); */

	/* Display function control */
	st7789_write_reg(MIPI_DCS_SET_PAGE_ADDRESS, 0x00, 0x00, 0x01, 0x3F);
	/* st7789_write_reg(MIPI_DCS_SET_PAGE_ADDRESS, 0x00, 0x00, 0x00, 0xEF); */

	/* Display on */
	st7789_write_reg(MIPI_DCS_SET_DISPLAY_ON);

	/* Memory write control */
	st7789_write_reg(RAMWR);

	pr_debug("Display init done...\n");

	return 0;
}
/* EXPORT_SYMBOL(st7789_init_display); */

#if 0
/**
 * set_gamma() - set gamma curves
 *
 * @curves: gamma curves
 *
 * Before the gamma curves are applied, they are preprocessed with a bitmask
 * to ensure syntactically correct input for the display controller.
 * This implies that the curves input parameter might be changed by this
 * function and that illegal gamma values are auto-corrected and not
 * reported as errors.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_gamma(unsigned long *curves)
{
	/* int i; */
	/* int j; */
	/* int c; /\* curve index offset *\/ */

	/* /\* */
	/*  * Bitmasks for gamma curve command parameters. */
	/*  * The masks are the same for both positive and negative voltage */
	/*  * gamma curves. */
	/*  *\/ */
	/* const u8 gamma_par_mask[] = { */
	/* 	0xFF, /\* V63[3:0], V0[3:0]*\/ */
	/* 	0x3F, /\* V1[5:0] *\/ */
	/* 	0x3F, /\* V2[5:0] *\/ */
	/* 	0x1F, /\* V4[4:0] *\/ */
	/* 	0x1F, /\* V6[4:0] *\/ */
	/* 	0x3F, /\* J0[1:0], V13[3:0] *\/ */
	/* 	0x7F, /\* V20[6:0] *\/ */
	/* 	0x77, /\* V36[2:0], V27[2:0] *\/ */
	/* 	0x7F, /\* V43[6:0] *\/ */
	/* 	0x3F, /\* J1[1:0], V50[3:0] *\/ */
	/* 	0x1F, /\* V57[4:0] *\/ */
	/* 	0x1F, /\* V59[4:0] *\/ */
	/* 	0x3F, /\* V61[5:0] *\/ */
	/* 	0x3F, /\* V62[5:0] *\/ */
	/* }; */

	/* for (i = 0; i < par->gamma.num_curves; i++) { */
	/* 	c = i * par->gamma.num_values; */
	/* 	for (j = 0; j < par->gamma.num_values; j++) */
	/* 		curves[c + j] &= gamma_par_mask[j]; */
	/* 	st7789_write_reg( */
	/* 		par, PVGAMCTRL + i, */
	/* 		curves[c + 0], curves[c + 1], curves[c + 2], */
	/* 		curves[c + 3], curves[c + 4], curves[c + 5], */
	/* 		curves[c + 6], curves[c + 7], curves[c + 8], */
	/* 		curves[c + 9], curves[c + 10], curves[c + 11], */
	/* 		curves[c + 12], curves[c + 13]); */
	/* } */
	return 0;
}
#endif

int st7789_probe_spi(struct spi_device *spi)
{
	int ret = 0;
	struct fb_info *fb_info;
#if defined (DO_SPI_PAN)
	struct fb_var_screeninfo *var;
#endif
	if (NULL == spi) {
		return PTR_ERR(spi);
	}

	/* dev_dbg(spi, "Probing STROMER-ST7789v device"); */
	pr_debug("Probing STROMER-ST7789v device");

	/* store spi device locally */
	disp.spi = spi;
	/* Set mode to 9-Bit */
	spi->bits_per_word = 9;
#if defined (GPIO_SPI_RESET)
	/* Try getting the reset pin */
	ret = gpio_request_one(GPIO_SPI_RESET, GPIOF_OUT_INIT_LOW, "st7789v_reset");
	if (ret) {
		pr_debug("Failed to get the reset pin");
		return ret;
	}
	disp.gpio_reset = GPIO_SPI_RESET;
#endif

	/* Disable lcdif during initialization */
	if (!spi->controller_data) {
		pr_err("No LCDIF controller data!\n");
		return -1;
	}
#if defined (DO_SPI_INIT_SEQUENCE)
	fb_info = (struct fb_info *)spi->controller_data;
	mxsfb_disable_controller(fb_info);
	/* Init and configure display */
	pr_debug("Configuring display...\n");
	ret = st7789_init_display();
 #if defined (DO_SPI_PAN)
	pr_debug("Do panning\n");
	var = &(fb_info->var);

	var->xoffset = 0;
	var->yoffset = 0;
	mxsfb_pan_display(var, fb_info);
 #endif
	mxsfb_enable_controller(fb_info);
#endif
	return ret;
}

int st7789_remove_spi(struct spi_device *spi)
{
	/* ToDo! */
	return 0;
}

static const struct of_device_id dt_ids[] = {
	{ .compatible = "st7789v" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, dt_ids);

static struct spi_driver st7789_driver_spi_driver = {
	.driver = {
		.name   = "st7789v",
		/* .of_match_table = of_match_ptr(dt_ids), */
		.owner = THIS_MODULE,
	},
	.probe  = st7789_probe_spi,
	.remove = st7789_remove_spi,
};

static int __init st7789_driver_module_init(void)
{
	int ret;

	ret = spi_register_driver(&st7789_driver_spi_driver);

	return ret;
}

static void __exit st7789_driver_module_exit(void)
{
	spi_unregister_driver(&st7789_driver_spi_driver);
}

module_init(st7789_driver_module_init);
module_exit(st7789_driver_module_exit);

MODULE_ALIAS("spi:" SPI_DRIVER_NAME);
MODULE_ALIAS("platform:" SPI_DRIVER_NAME);
MODULE_ALIAS("spi:st7789v");
MODULE_ALIAS(SPI_DRIVER_NAME);

MODULE_DESCRIPTION("SPI driver for the ST7789V LCD Controller");
MODULE_AUTHOR("Christian Duenki");
MODULE_LICENSE("GPL");
