/*
 * Copyright (C) 2017 Christian Duenki, Escatec Switzerland AG
 * christian.duenki@escatec.com
 *
 * This code is based on:
 * drivers/input/touchscreen/sitronix_i2c_touch.c
 *
 * Touchscreen driver for Sitronix (I2C bus)
 *
 * Copyright (C) 2011 Sitronix Technology Co., Ltd.
 *	Rudy Huang <rudy_huang@sitronix.com.tw>
 */
/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include "st1633.h"
#include <linux/i2c.h>
#include <linux/input.h>
#ifdef SITRONIX_SUPPORT_MT_SLOT
 #include <linux/input/mt.h>
#endif /* SITRONIX_SUPPORT_MT_SLOT */
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#define DRIVER_AUTHOR           "Escatec Switzerland AG"
#define DRIVER_NAME             "st1633i"
#define DRIVER_DESC             "Sitronix I2C touch"
#define DRIVER_DATE             "20170626"
#define DRIVER_MAJOR            2
#define DRIVER_MINOR         	9
#define DRIVER_PATCHLEVEL       24

#define IRQF_DISABLED           0

#ifdef SITRONIX_SENSOR_KEY
#define SITRONIX_NUMBER_SENSOR_KEY 3
int sitronix_sensor_key[SITRONIX_NUMBER_SENSOR_KEY] = {
	/* bit 0 */
	KEY_BACK,
	/* bit 1 */
	KEY_HOME,
	/* bit 2 */
	KEY_MENU,
};
#endif /* SITRONIX_SENSOR_KEY */

#ifdef SITRONIX_TOUCH_KEY
#define SITRONIX_NUMBER_TOUCH_KEY 4

#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#define SITRONIX_TOUCH_RESOLUTION_X 240 /* max of X value in display area */
#define SITRONIX_TOUCH_RESOLUTION_Y 320 /* max of Y value in display area */
#define SITRONIX_TOUCH_GAP_Y	10      /* Gap between bottom of display and top of touch key */
#define SITRONIX_TOUCH_MAX_Y 915        /* resolution of y axis of touch ic */
struct sitronix_AA_key sitronix_key_array[SITRONIX_NUMBER_TOUCH_KEY] = {
	{15, 105, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_MENU},  /* MENU */
	{135, 225, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_HOME},
	{255, 345, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_BACK}, /* KEY_EXIT */
	{375, 465, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_SEARCH},
};
#else
#define SCALE_KEY_HIGH_Y 15
struct sitronix_AA_key sitronix_key_array[SITRONIX_NUMBER_TOUCH_KEY] = {
	{0, 0, 0, 0, KEY_MENU}, /* MENU */
	{0, 0, 0, 0, KEY_HOME},
	{0, 0, 0, 0, KEY_BACK}, /* KEY_EXIT */
	{0, 0, 0, 0, KEY_SEARCH},
};
#endif /* SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY */
#endif /* SITRONIX_TOUCH_KEY */

struct sitronix_xy_data_s {
	union {
		struct {
			uint8_t y_h:3;
			uint8_t reserved:1;
			uint8_t x_h:3;
			uint8_t valid:1;
		} fields;
		uint8_t byte;
	} xy_info;
	uint8_t x_l;
	uint8_t y_l;
	uint8_t z;
};

struct stx_report_data_s {
	uint8_t fingers:4;
	uint8_t reserved:4;
	uint8_t keys;
	struct sitronix_xy_data_s xy_data[SITRONIX_MAX_SUPPORTED_POINT];
};

/**
 * Struct containing all relevant data to implement touch function
 */
struct sitronix_ts_data_s {
	struct i2c_client *client;
	struct input_dev *input_dev;
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	struct input_dev *keyevent_input;
#endif /* defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY) */
	int use_irq;
	struct hrtimer timer;
#ifndef SITRONIX_INT_POLLING_MODE
	struct work_struct  work;
#else
	struct delayed_work work;
#endif /* SITRONIX_INT_POLLING_MODE */
	void (*reset_ic)(void);
	uint8_t fw_revision[4];
	int resolution_x;
	int resolution_y;
	uint8_t max_touches;
	uint8_t touch_protocol_type;
	uint8_t pixel_length;
	uint8_t chip_id;
	uint8_t Num_X;
	uint8_t Num_Y;
	int suspend_state;
	int irq_gpio;
};

static int i2cErrorCount = 0;

static struct sitronix_ts_data_s *sitronix_ts_gpts = NULL;
static int sitronix_ts_irq_on = 0;

/**
 * @brief Used to report a pen down event
 * @param[out] input_dev Input device to send the event
 * @param[in]  id        Id of the event
 * @param[in]  x         X address
 * @param[in]  y         Y address
 */
static inline void sitronix_ts_pen_down(struct input_dev *input_dev, int id, u16 x, u16 y);

/**
 * @brief Used to report a pen up event
 * @param[out] input_dev Input device to send the event
 * @param[in]  id        Id of the event
 */
static inline void sitronix_ts_pen_up(struct input_dev *input_dev, int id);

/**
 * @brief Local function to write data to i2c slave
 * @param[in,out] client I2C client data
 * @param[in] txbuf      Output data buffer
 * @param[in] len        Data length
 * @return Error code
 */
static int sitronix_i2c_write_bytes(struct i2c_client *client, u8 *txbuf, int len);

/**
 * @brief Local function to read data from i2c slave
 * @param[in,out] client I2C client data
 * @param[in] addr       Address to read data from slave
 * @param[in] rxbuf      Input data buffer
 * @param[in] len        Data length
 * @return Error code
 */
static int sitronix_i2c_read_bytes(struct i2c_client *client, u8 addr, u8 *rxbuf, int len);

/**
 * @brief Reading out touch controller firmware revision
 * @param[in,out] ts Internal touch data struct
 * @return Error code
 */
static int sitronix_get_fw_revision(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[4];

	ret = sitronix_i2c_read_bytes(ts->client, FIRMWARE_REVISION_3, buffer, 4);
	if (ret < 0) {
		printk("read fw revision error (%d)\n", ret);
		return ret;
	} else {
		memcpy(ts->fw_revision, buffer, 4);
		printk("fw revision (hex) = %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	}

	ret = sitronix_i2c_read_bytes(ts->client, FIRMWARE_VERSION, buffer, 1);
	if (ret < 0) {
		printk("read fw version error (%d)\n", ret);
		return ret;
	} else {
		printk("fw version (hex) = %x\n", buffer[0]);
	}

	return 0;
}

/**
 * @brief Reading out Chip ID
 * @param[in,out] ts Internal touch data struct
 * @return Error code
 */
static int sitronix_ts_get_chip_id(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[3];

	DbgMsg("%s\n", __FUNCTION__);

	ret = sitronix_i2c_read_bytes(ts->client, CHIP_ID, buffer, 3);
	if (ret < 0) {
		printk("read Chip ID error (%d)\n", ret);
		return ret;
	} else {
		if (buffer[0] == 0) {
			if (buffer[1] + buffer[2] > 32) {
				ts->chip_id = 2;
			} else {
				ts->chip_id = 0;
			}
		} else {
			ts->chip_id = buffer[0];
		}
		ts->Num_X = buffer[1];
		ts->Num_Y = buffer[2];
		printk("Chip ID = %d\n", ts->chip_id);
		printk("Num_X = %d\n", ts->Num_X);
		printk("Num_Y = %d\n", ts->Num_Y);
	}

	return 0;
}

/**
 * @brief Reading out maximum allowed touches
 * @param[in,out] ts Internal touch data struct
 * @return Error code
 */
static int sitronix_get_max_touches(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[1];

	ret = sitronix_i2c_read_bytes(ts->client, MAX_NUM_TOUCHES, buffer, 1);
	if (ret < 0) {
		printk("read max touches error (%d)\n", ret);
		return ret;
	} else {
		ts->max_touches = buffer[0];
		if (ts->max_touches > SITRONIX_MAX_SUPPORTED_POINT) {
			ts->max_touches = SITRONIX_MAX_SUPPORTED_POINT;
		}
		printk("max touches = %d \n",ts->max_touches);
	}
	return 0;
}

/**
 * @brief Reading out touch controller i2c protocol type
 * @param[in,out] ts Internal touch data struct
 * @return Error code
 */
static int sitronix_get_protocol_type(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[1];

	ret = sitronix_i2c_read_bytes(ts->client, I2C_PROTOCOL, buffer, 1);
	if (ret < 0) {
		printk("read i2c protocol error (%d)\n", ret);
		return ret;
	} else {
		ts->touch_protocol_type = buffer[0] & I2C_PROTOCOL_BMSK;
		if (ts->touch_protocol_type == SITRONIX_A_TYPE) {
			ts->pixel_length = PIXEL_DATA_LENGTH_A;
		} else if (ts->touch_protocol_type == SITRONIX_B_TYPE) {
			ts->pixel_length = PIXEL_DATA_LENGTH_B;
		} else {
			ts->pixel_length = PIXEL_DATA_LENGTH_A;
		}

		printk("i2c protocol = %d \n", ts->touch_protocol_type);
	}

	return 0;
}

/**
 * @brief Reading out touch screen resolution
 * @param[in,out] ts Internal touch data struct
 * @return Error code
 */
static int sitronix_get_resolution(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[3];

	ret = sitronix_i2c_read_bytes(ts->client, XY_RESOLUTION_HIGH, buffer, 3);
	if (ret < 0) {
		DbgMsg("read resolution error (%d)\n", ret);
		return ret;
	} else {
		ts->resolution_x = ((buffer[0] & (X_RES_H_BMSK << X_RES_H_SHFT)) << 4) | buffer[1];
		ts->resolution_y = ((buffer[0] & Y_RES_H_BMSK) << 8) | buffer[2];
		DbgMsg("resolution = %d x %d\n", ts->resolution_x, ts->resolution_y);
	}

	return 0;
}

/**
 * @brief Set/Clear powerdown bit
 * @param[in,out] ts Internal touch data struct
 * @param[in] value  Value to be set
 * @return Error code
 */
static int sitronix_ts_set_powerdown_bit(struct sitronix_ts_data_s *ts, int value)
{
	int ret = 0;
	uint8_t buffer[2];

	DbgMsg("%s, value = %d\n", __func__, value);
	ret = sitronix_i2c_read_bytes(ts->client, DEVICE_CONTROL_REG, buffer, 1);
	if (ret < 0){
		printk("read device control status error (%d)\n", ret);
		return ret;
	} else {
		DbgMsg("dev status = %d \n", buffer[0]);
	}

	if (value == 0) {
		buffer[1] = buffer[0] & 0xfd;
	} else {
		buffer[1] = buffer[0] | 0x2;
	}

	buffer[0] = DEVICE_CONTROL_REG;
	ret = i2c_master_send(ts->client, buffer, 2);
	if (ret < 0) {
		printk("write power down error (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Collect touch controller data
 * @param[in,out] ts Internal touch data struct
 * @return Error code
 */
static int sitronix_ts_get_touch_info(struct sitronix_ts_data_s *ts)
{
	int ret = 0;

	ret = sitronix_get_resolution(ts);
	if (ret < 0) {
		return ret;
	}

	ret = sitronix_ts_get_chip_id(ts);
	if (ret < 0) {
		return ret;
	}

	ret = sitronix_get_fw_revision(ts);
	if (ret < 0) {
		return ret;
	}

	ret = sitronix_get_protocol_type(ts);
	if (ret < 0) {
		return ret;
	}

	ret = sitronix_get_max_touches(ts);
	if (ret < 0) {
		return ret;
	}

	if ((ts->fw_revision[0] == 0) && (ts->fw_revision[1] == 0)) {
		if (ts->touch_protocol_type == SITRONIX_RESERVED_TYPE_0) {
			ts->touch_protocol_type = SITRONIX_B_TYPE;
			printk("i2c protocol (revised) = %d \n", ts->touch_protocol_type);
		}
	}

	if (ts->touch_protocol_type == SITRONIX_A_TYPE) {
		ts->pixel_length = PIXEL_DATA_LENGTH_A;
	} else if (ts->touch_protocol_type == SITRONIX_B_TYPE) {
		ts->pixel_length = PIXEL_DATA_LENGTH_B;
		ts->max_touches = 2;
		printk("max touches (revised) = %d \n", ts->max_touches);
	}

	return 0;
}

/**
 * @brief Read touch controller status
 * @param[in,out] ts Internal touch data struct
 * @param[out] dev_status Device status
 * @return Error code
 */
static int sitronix_ts_get_device_status(struct sitronix_ts_data_s *ts, uint8_t *dev_status)
{
	int ret = 0;
	uint8_t buffer[8];

	DbgMsg("%s\n", __func__);
	buffer[0] = STATUS_REG;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send status reg command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 8);
	if (ret < 0) {
		printk("read status reg error (%d)\n", ret);
		return ret;
	} else {
		DbgMsg("status reg = %d \n", buffer[0]);
	}

	*dev_status = buffer[0] & 0xf;

	return 0;
}

/**
 * @brief Read Enhanced function control register
 * @param[in,out] ts Internal touch data struct
 * @param[out] value Value to write data to
 * @return Error code
 */
static int sitronix_ts_Enhance_Function_control(struct sitronix_ts_data_s *ts, uint8_t *value)
{
	int ret = 0;
	uint8_t buffer[4];

	DbgMsg("%s\n", __func__);
	buffer[0] = 0xF0;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0) {
		printk("send Enhance Function command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0) {
		printk("read Enhance Functions status error (%d)\n", ret);
		return ret;
	} else {
		DbgMsg("Enhance Functions status = %d \n", buffer[0]);
	}

	*value = buffer[0] & 0x4;

	return 0;
}

static int sitronix_ts_FW_Bank_Select(struct sitronix_ts_data_s *ts, uint8_t value)
{
	int ret = 0;
	uint8_t buffer[2];

	DbgMsg("%s\n", __func__);
	buffer[0] = 0xF1;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0) {
		printk("send FW Bank Select command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0) {
		printk("read FW Bank Select status error (%d)\n", ret);
		return ret;
	} else {
		DbgMsg("FW Bank Select status = %d \n", buffer[0]);
	}

	buffer[1] = ((buffer[0] & 0xfc) | value);
	buffer[0] = 0xF1;
	ret = i2c_master_send(ts->client, buffer, 2);
	if (ret < 0) {
		printk("send FW Bank Select command error (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int sitronix_get_id_info(struct sitronix_ts_data_s *ts, uint8_t *id_info)
{
	int ret = 0;
	uint8_t buffer[4];

	buffer[0] = 0x0C;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0) {
		printk("send id info command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 4);
	if (ret < 0) {
		printk("read id info error (%d)\n", ret);
		return ret;
	} else {
		memcpy(id_info, buffer, 4);
	}
	return 0;
}

static int sitronix_ts_identify(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t id[4];
	uint8_t Enhance_Function = 0;

	ret = sitronix_ts_FW_Bank_Select(ts, 1);
	if (ret < 0) {
		return ret;
	}
	ret = sitronix_ts_Enhance_Function_control(ts, &Enhance_Function);
	if (ret < 0) {
		return ret;
	}
	if (Enhance_Function == 0x4) {
		ret = sitronix_get_id_info(ts, &id[0]);
		if (ret < 0) {
			return ret;
		}
		printk("id (hex) = %x %x %x %x\n", id[0], id[1], id[2], id[3]);
		if ((id[0] == 1)&&(id[1] == 2)&&(id[2] == 0xb)&&(id[3] == 1)) {
			return 0;
		} else {
			printk("Error: It is not Sitronix IC\n");
			return -1;
		}
	} else {
		printk("Error: Can not get ID of Sitronix IC\n");
		return -1;
	}
}

static void sitronix_ts_work_func(struct work_struct *work)
{
	int i;
	int ret;
	uint16_t x, y;
#ifndef SITRONIX_INT_POLLING_MODE
	struct sitronix_ts_data_s *ts = \
		container_of(work, struct sitronix_ts_data_s, work);
#else
	struct sitronix_ts_data_s *ts = \
		container_of(to_delayed_work(work), struct sitronix_ts_data_s, work);
#endif /* SITRONIX_INT_POLLING_MODE */
	uint8_t buffer[2 + SITRONIX_MAX_SUPPORTED_POINT * PIXEL_DATA_LENGTH_A];
	uint8_t PixelCount = 0;
	struct sitronix_xy_data_s *p_xy_data = NULL;

	DbgMsg("%s\n",  __func__);
	if (ts->suspend_state) {
		goto exit_invalid_data;
	}

	/* get touch data */
	ret = sitronix_i2c_read_bytes(ts->client, KEYS_REG, buffer, 1 + ts->max_touches * ts->pixel_length);
	if (ret < 0) {
		printk("read finger error (%d)\n", ret);
		i2cErrorCount++;
		goto exit_invalid_data;
	} else {
		i2cErrorCount = 0;
	}

	p_xy_data = (struct sitronix_xy_data_s *)&buffer[1];
	for (i = 0; i < ts->max_touches; i++) {
		/* DbgMsg("Key %d: 0x%02x\n", i, p_xy_data[i].xy_info.byte); */
		if (p_xy_data[i].xy_info.fields.valid == 1) {
		/* if (buffer[1 + i * ts->pixel_length + XY_COORD_H] & 0x80) { */
			x = 0 | (u16)p_xy_data[i].xy_info.fields.x_h << 8 | (u16)p_xy_data[i].x_l;
			y = 0 | (u16)p_xy_data[i].xy_info.fields.y_h << 8 | (u16)p_xy_data[i].y_l;
			/* DbgMsg("X: 0x%04x, Y: 0x%04x", x, y); */
			PixelCount++;
			sitronix_ts_pen_down(ts->input_dev, i, x, y);
#ifndef SITRONIX_TOUCH_KEY
 #ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
			if (y < SITRONIX_TOUCH_RESOLUTION_Y) {
 #else
			//if (y < (ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y)) {
 #endif /* SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY */
				PixelCount++;
				sitronix_ts_pen_down(ts->input_dev, i, x, y);
				//DbgMsg("AREA_DISPLAY\n");
			} else {
				for (j = 0; j < (sizeof(sitronix_aa_key_array)/sizeof(struct sitronix_AA_key)); j++) {
					if((x >= sitronix_aa_key_array[j].x_low) &&
					   (x <= sitronix_aa_key_array[j].x_high) &&
					   (y >= sitronix_aa_key_array[j].y_low) &&
					   (y <= sitronix_aa_key_array[j].y_high)) {
						aa_key_status |= (1 << j);
						//DbgMsg("AREA_KEY [%d]\n", j);
						break;
					}
				}
			}
#endif
		} else {
			sitronix_ts_pen_up(ts->input_dev, i);
		}
	}
	input_report_key(ts->input_dev, BTN_TOUCH, PixelCount > 0);
	input_sync(ts->input_dev);

exit_invalid_data:
#ifdef SITRONIX_INT_POLLING_MODE
	if (PixelCount > 0) {
#ifdef SITRONIX_MONITOR_THREAD
		if (ts->enable_monitor_thread == 1) {
			atomic_set(&iMonitorThreadPostpone,1);
		}
#endif /* SITRONIX_MONITOR_THREAD */
		schedule_delayed_work(&ts->work, msecs_to_jiffies(INT_POLLING_MODE_INTERVAL));
	} else {
#ifdef CONFIG_HARDIRQS_SW_RESEND
		printk("Please not set HARDIRQS_SW_RESEND to prevent kernel from sending SW IRQ\n");
#endif /* CONFIG_HARDIRQS_SW_RESEND */
		if (ts->use_irq){
			sitronix_ts_irq_on = 1;
			/* atomic_set(&sitronix_ts_irq_on, 1); */
			enable_irq(ts->client->irq);
		}
	}
#endif /* SITRONIX_INT_POLLING_MODE */

#if defined(SITRONIX_LEVEL_TRIGGERED)
	if (ts->use_irq) {
		sitronix_ts_irq_on = 1;
		enable_irq(ts->client->irq);
	}
#endif // defined(SITRONIX_LEVEL_TRIGGERED)
	if ((2 <= i2cErrorCount)) {
		printk("I2C abnormal in work_func(), reset it!\n");
		if(sitronix_ts_gpts->reset_ic) {
			sitronix_ts_gpts->reset_ic();
		}
		i2cErrorCount = 0;
	}
}

static irqreturn_t sitronix_ts_irq_handler(int irq, void *dev_id)
{
	struct sitronix_ts_data_s *ts = dev_id;

	DbgMsg("%s\n", __func__);
#if defined(SITRONIX_LEVEL_TRIGGERED) || defined(SITRONIX_INT_POLLING_MODE)
	sitronix_ts_irq_on = 0;
	disable_irq_nosync(ts->client->irq);
#endif /* defined(SITRONIX_LEVEL_TRIGGERED) || defined(SITRONIX_INT_POLLING_MODE) */
#ifndef SITRONIX_INT_POLLING_MODE
	schedule_work(&ts->work);
#else
	schedule_delayed_work(&ts->work, msecs_to_jiffies(0));
#endif /* SITRONIX_INT_POLLING_MODE */

	return IRQ_HANDLED;
}

static int st1633_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	int i;
#endif /* defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY) */
	struct sitronix_ts_data_s *ts;
	int ret = 0;
	uint16_t max_x = 0, max_y = 0;
	struct sitronix_i2c_touch_platform_data *pdata;
	uint8_t dev_status = 0;
	int oftree = 0;

	DbgMsg("Start probing ST1633 device.\n");

	if (NULL == client) {
		DbgMsg("ST1633 - invalid parameter!.\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct sitronix_ts_data_s), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	DbgMsg("ST1633 - create links between data.\n");
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;

	if (NULL == pdata) {
		printk("No valid platform data available.\n");
		oftree = 0;
	} else {
		DbgMsg("Switching to oftree mode.\n");
		oftree = 1;
	}

	if (0 != oftree) {
		/* Only of platform data is available */
		if (pdata->reset_ic) {
			ts->reset_ic = pdata->reset_ic;
			pdata->reset_ic();
			mdelay(SITRONIX_TS_CHANGE_MODE_DELAY);
		}
	} else {
		ts->reset_ic = NULL;
	}
	sitronix_ts_gpts = ts;

	printk("ST1633 - reading device status.\n");
	ret = sitronix_ts_get_device_status(ts, &dev_status);
	if ((ret < 0) || (dev_status == 0x6)) {
		dev_err(dev, "ST1633 - invalid device status, probing failed\n");
		goto err_device_info_error;
	}

	printk("ST1633 - reading touch info.\n");
	ret = sitronix_ts_get_touch_info(ts);
	if(ret < 0) {
		goto err_device_info_error;
	}

	ret = sitronix_ts_identify(ts);

	printk("ST1633 - Allocate input device.\n");
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		dev_err(dev, "Can not allocate memory for input device.\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

#ifndef SITRONIX_INT_POLLING_MODE
	DbgMsg("ST1633 - adding work.\n");
	INIT_WORK(&ts->work, sitronix_ts_work_func);
	DbgMsg(" done\n");
#else
	DbgMsg("ST1633 - adding delayed work...");
	INIT_DELAYED_WORK(&ts->work, sitronix_ts_work_func);
	DbgMsg(" done\n");
#endif /* SITRONIX_INT_POLLING_MODE */

	ts->input_dev->name = "sitronix-i2c-touch-mt";
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);
	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(BTN_TOUCH, ts->input_dev->keybit);

#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	ts->keyevent_input = input_allocate_device();
	if (ts->keyevent_input == NULL) {
		dev_err(dev, "Can not allocate memory for key input device.\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}
	ts->keyevent_input->name  = "sitronix-i2c-touch-key";
	set_bit(EV_KEY, ts->keyevent_input->evbit);
#endif /* defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY) */
#if defined(SITRONIX_SENSOR_KEY)
	for (i = 0; i < SITRONIX_NUMBER_SENSOR_KEY; i++) {
		set_bit(sitronix_sensor_key[i], ts->keyevent_input->keybit);
	}
#endif /* defined(SITRONIX_SENSOR_KEY) */

#ifndef SITRONIX_TOUCH_KEY
	max_x = ts->resolution_x;
	max_y = ts->resolution_y;
#else
 #ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
	for (i = 0; i < SITRONIX_NUMBER_TOUCH_KEY; i++) {
		set_bit(sitronix_key_array[i].code, ts->keyevent_input->keybit);
	}
	max_x = SITRONIX_TOUCH_RESOLUTION_X;
	max_y = SITRONIX_TOUCH_RESOLUTION_Y;
 #else
	for (i = 0; i < SITRONIX_NUMBER_TOUCH_KEY; i++) {
		sitronix_key_array[i].x_low = ((ts->resolution_x / SITRONIX_NUMBER_TOUCH_KEY ) * i ) + 15;
		sitronix_key_array[i].x_high = ((ts->resolution_x / SITRONIX_NUMBER_TOUCH_KEY ) * (i + 1)) - 15;
		sitronix_key_array[i].y_low = ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y;
		sitronix_key_array[i].y_high = ts->resolution_y;
		DbgMsg("key[%d] %d, %d, %d, %d\n", i, sitronix_key_array[i].x_low, sitronix_key_array[i].x_high, sitronix_key_array[i].y_low, sitronix_key_array[i].y_high);
		set_bit(sitronix_key_array[i].code, ts->keyevent_input->keybit);
	}
	max_x = ts->resolution_x;
	max_y = ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y;
 #endif /* SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY */
#endif /* SITRONIX_TOUCH_KEY */
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	ret = input_register_device(ts->keyevent_input);
	if (ret < 0) {
		dev_err(dev, "Can not register key input device.\n");
		/* printk("Can not register key input device."); */
		goto err_input_register_device_failed;
	}
#endif /* defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY) */

#if defined(SITRONIX_SUPPORT_MT_SLOT)
	/* Multi touch */
	input_mt_init_slots(ts->input_dev, ts->max_touches, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
#else
	__set_bit(ABS_X, ts->input_dev->absbit);
	__set_bit(ABS_Y, ts->input_dev->absbit);
	__set_bit(ABS_MT_TOUCH_MAJOR, ts->input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, ts->input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, ts->input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, ts->input_dev->absbit);
	__set_bit(ABS_MT_TOOL_TYPE, ts->input_dev->absbit);
	__set_bit(ABS_MT_BLOB_ID, ts->input_dev->absbit);
	__set_bit(ABS_MT_TRACKING_ID, ts->input_dev->absbit);

	input_set_abs_params(ts->input_dev, ABS_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0,  255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0,  255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touches, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
#endif
#ifndef SITRONIX_SWAP_XY
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
#else
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_x, 0, 0);
#endif /* SITRONIX_SWAP_XY */

	ret = input_register_device(ts->input_dev);
	if(ret < 0) {
		dev_err(dev, "Can not register input device.\n");
		goto err_input_register_device_failed;
	}

	ts->suspend_state = 0;
	if (client->irq) {
#ifdef SITRONIX_LEVEL_TRIGGERED
		ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_LOW | IRQF_DISABLED, client->name, ts);
#else
		ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_FALLING | IRQF_DISABLED, client->name, ts);
#endif /* SITRONIX_LEVEL_TRIGGERED */
		if (ret == 0) {
			sitronix_ts_irq_on = 1;
			ts->use_irq = 1;
		} else {
			dev_err(&client->dev, "request_irq failed\n");
		}
	}
	/* Get irq gpio */
	if (dev->of_node) {
		ts->irq_gpio = of_get_named_gpio(dev->of_node, "pendown-gpio", 0);
		if (ts->irq_gpio > 100) {
			dev_err(dev, "Unable to read GPIO from device tree\n");
		}
		ret = gpio_request_one(ts->irq_gpio, GPIOF_IN, "st1633i-irq_pin");
		if (ret) {
			dev_err(dev, "Failed to request/setup irq GPIO");
		}
	} else {
		dev_err(dev, "Failed to request/setup irq GPIO");
	}

	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	input_free_device(ts->keyevent_input);
#endif /* defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY) */
err_input_dev_alloc_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
err_device_info_error:
	return ret;
}

static inline void sitronix_ts_pen_down(struct input_dev *input_dev, int id, u16 x, u16 y)
{
#ifdef SITRONIX_SUPPORT_MT_SLOT
	input_mt_slot(input_dev, id);
 #ifndef SITRONIX_SWAP_XY
	input_report_abs(input_dev,  ABS_MT_POSITION_X, x);
	input_report_abs(input_dev,  ABS_MT_POSITION_Y, y);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
 #else
	input_report_abs(input_dev,  ABS_MT_POSITION_X, y);
	input_report_abs(input_dev,  ABS_MT_POSITION_Y, x);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
 #endif /* SITRONIX_SWAP_XY */
#else
	input_report_abs(input_dev,  ABS_MT_TRACKING_ID, id);
 #ifndef SITRONIX_SWAP_XY
	input_report_abs(input_dev,  ABS_MT_POSITION_X, x);
	input_report_abs(input_dev,  ABS_MT_POSITION_Y, y);
 #else
	input_report_abs(input_dev,  ABS_MT_POSITION_X, y);
	input_report_abs(input_dev,  ABS_MT_POSITION_Y, x);
 #endif /* SITRONIX_SWAP_XY */
	input_report_abs(input_dev,  ABS_MT_TOUCH_MAJOR, 255);
	input_report_abs(input_dev,  ABS_MT_WIDTH_MAJOR, 255);
	input_report_abs(input_dev, ABS_MT_PRESSURE, 255);
	input_mt_sync(input_dev);
#endif /* SITRONIX_SUPPORT_MT_SLOT */
	DbgMsg("[%d](%d, %d)+\n", id, x, y);
}

static inline void sitronix_ts_pen_up(struct input_dev *input_dev, int id)
{
#ifdef SITRONIX_SUPPORT_MT_SLOT
	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
#else
	input_report_abs(input_dev,  ABS_MT_TRACKING_ID, id);
	input_report_abs(input_dev, ABS_MT_PRESSURE, 0);
#endif /* SITRONIX_SUPPORT_MT_SLOT */
	DbgMsg("[%d]-\n", id);
}

static int sitronix_i2c_read_bytes(struct i2c_client *client, u8 addr, u8 *rxbuf, int len)
{
	int ret = 0;
	u8 txbuf = addr;
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &txbuf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = rxbuf,
		},
	};
#endif /* defined(SITRONIX_I2C_COMBINED_MESSAGE) */

	if(rxbuf == NULL)
		return -1;
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	ret = i2c_transfer(client->adapter, &msg[0], 2);
#elif defined(SITRONIX_I2C_SINGLE_MESSAGE)
	ret = i2c_master_send(client, &txbuf, 1);
	if (ret < 0){
		printk("write 0x%x error (%d)\n", addr, ret);
		return ret;
	}
	ret = i2c_master_recv(client, rxbuf, len);
#endif /* defined(SITRONIX_I2C_COMBINED_MESSAGE) */
	if (ret < 0){
		DbgMsg("read 0x%x error (%d)\n", addr, ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Local function to write data to i2c slave
 * @param[in,out] client I2C client data
 * @param[in] txbuf      Output data buffer
 * @param[in] len        Data length
 * @return Error code
 */
static int sitronix_i2c_write_bytes(struct i2c_client *client, u8 *txbuf, int len)
{
	int ret = 0;
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	struct i2c_msg msg[1] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = len,
			.buf = txbuf,
		},
	};
#endif /* defined(SITRONIX_I2C_COMBINED_MESSAGE) */

	if(txbuf == NULL)
		return -1;
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	ret = i2c_transfer(client->adapter, &msg[0], 1);
#elif defined(SITRONIX_I2C_SINGLE_MESSAGE)
	ret = i2c_master_send(client, txbuf, len);
#endif /* defined(SITRONIX_I2C_COMBINED_MESSAGE) */
	if (ret < 0){
		printk("write 0x%x error (%d)\n", *txbuf, ret);
		return ret;
	}
	return 0;
}

static int st1633_i2c_remove(struct i2c_client *client)
{
	struct sitronix_ts_data_s *ts = i2c_get_clientdata(client);
	if (ts->use_irq) {
		free_irq(client->irq, ts);
	} else {
		hrtimer_cancel(&ts->timer);
	}
	input_unregister_device(ts->input_dev);
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	input_unregister_device(ts->keyevent_input);
#endif /* defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY) */
	kfree(ts);

	return 0;
}

static int __maybe_unused st1633_i2c_suspend(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct sitronix_ts_data_s *ts = i2c_get_clientdata(client);

	DbgMsg("%s\n", __func__);
	if(ts->use_irq){
		sitronix_ts_irq_on = 0;
		disable_irq_nosync(ts->client->irq);
	}
	ts->suspend_state = 1;

	ret = sitronix_ts_set_powerdown_bit(ts, 1);
#ifdef SITRONIX_WAKE_UP_TOUCH_BY_INT
	gpio_direction_output(ts->irq_gpio, 1);
#endif /* SITRONIX_WAKE_UP_TOUCH_BY_INT */
	DbgMsg("%s return\n", __func__);

	return 0;
}

static int __maybe_unused st1633_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sitronix_ts_data_s *ts = i2c_get_clientdata(client);
#ifdef SITRONIX_WAKE_UP_TOUCH_BY_INT
	unsigned int gpio;
#else
	int ret;
#endif /* SITRONIX_WAKE_UP_TOUCH_BY_INT */

	DbgMsg("%s\n", __func__);
#ifdef SITRONIX_WAKE_UP_TOUCH_BY_INT
	gpio = ts->irq_gpio;
	gpio_set_value(gpio, 0);
	gpio_direction_input(gpio);
#else
	ret = sitronix_ts_set_powerdown_bit(ts, 0);
#endif /* SITRONIX_WAKE_UP_TOUCH_BY_INT */

	ts->suspend_state = 0;
	if(ts->use_irq){
		sitronix_ts_irq_on = 1;
		enable_irq(ts->client->irq);
	}
	DbgMsg("%s return\n", __func__);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id st1633_of_match[] = {
	{ .compatible = "stromer-st1633", .data = NULL},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, st1633_of_match);
#endif

static SIMPLE_DEV_PM_OPS(st1633_i2c_pm,
			 st1633_i2c_suspend, st1633_i2c_resume);


static const struct i2c_device_id st1633_i2c_id[] = {
	{ .name="stromer-st1633", .driver_data = 0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(i2c, st1633_i2c_id);

static struct i2c_driver st1633_i2c_driver = {
	.driver = {
		.name = "stromer-st1633",
		.pm = &st1633_i2c_pm,
	},
	.id_table = st1633_i2c_id,
	.probe = st1633_i2c_probe,
	.remove = st1633_i2c_remove,
};
module_i2c_driver(st1633_i2c_driver);

static int __init st1633_i2c_init(void)
{
	printk("Sitronix st1633 touch driver %d.%d.%d\n", DRIVER_MAJOR, DRIVER_MINOR, DRIVER_PATCHLEVEL);
	printk("Release date: %s\n", DRIVER_DATE);
	return i2c_add_driver(&st1633_i2c_driver);
}

static void __exit st1633_i2c_exit(void)
{
	i2c_del_driver(&st1633_i2c_driver);
}

MODULE_DESCRIPTION("Sitronix Multi-Touch Driver");
MODULE_AUTHOR("Christian Duenki <christian.duenki@escatec.com>");
MODULE_LICENSE("GPL");
