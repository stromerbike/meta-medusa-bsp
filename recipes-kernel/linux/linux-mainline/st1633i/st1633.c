/*
 * Copyright (C) 2017 Christian Duenki, Escatec Switzerland AG
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
#define DRIVER_PATCHLEVEL       2

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
	uint8_t y_h:3;
	uint8_t reserved:1;
	uint8_t x_h:3;
	uint8_t valid:1;
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

struct sitronix_ts_data_s {
	uint16_t addr;
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
	int suspend_state;
	int irq_gpio;
};

static int i2cErrorCount = 0;

static MTD_STRUCTURE sitronix_ts_gMTDPreStructure[SITRONIX_MAX_SUPPORTED_POINT]={{0}};

static struct sitronix_ts_data_s *sitronix_ts_gpts = NULL;
static int sitronix_ts_irq_on = 0;
static int sitronix_invert_x = 0;   /* Optionally invert X axis value */
static int sitronix_invert_y = 0;   /* Optionally invert Y axis value */

/**
 * @brief Local function to write data to i2c slave
 * @param[in,out] client I2C client data
 * @param[in] txbuf      Output data buffer
 * @param[in] len        Data length
 * @return Error code
 */
static int sitronix_i2c_write_bytes(struct i2c_client *client, u8 *txbuf, int len);

/**
 * @brief Local function to write data to i2c slave
 * @param[in,out] client I2C client data
 * @param[in] txbuf      Output data buffer
 * @param[in] len        Data length
 * @return Error code
 */
static int sitronix_i2c_write_bytes(struct i2c_client *client, u8 *txbuf, int len);

static int sitronix_get_fw_revision(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[4];

	buffer[0] = FIRMWARE_REVISION_3;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send fw revision command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 4);
	if (ret < 0){
		printk("read fw revision error (%d)\n", ret);
		return ret;
	}else{
		memcpy(ts->fw_revision, buffer, 4);
		printk("fw revision (hex) = %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
	return 0;
}
static int sitronix_get_max_touches(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[1];


	buffer[0] = MAX_NUM_TOUCHES;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0) {
		printk("send max touches command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0) {
		printk("read max touches error (%d)\n", ret);
		return ret;
	} else {
		ts->max_touches = buffer[0];
		printk("max touches = %d \n",ts->max_touches);
	}
	return 0;
}

static int sitronix_get_protocol_type(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[1];

	buffer[0] = I2C_PROTOCOL;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send i2c protocol command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0){
		printk("read i2c protocol error (%d)\n", ret);
		return ret;
	}else{
		ts->touch_protocol_type = buffer[0] & I2C_PROTOCOL_BMSK;
		if(ts->touch_protocol_type == SITRONIX_A_TYPE)
			ts->pixel_length = PIXEL_DATA_LENGTH_A;
		else if(ts->touch_protocol_type == SITRONIX_B_TYPE)
			ts->pixel_length = PIXEL_DATA_LENGTH_B;
		else
			ts->pixel_length = PIXEL_DATA_LENGTH_A;
		printk("i2c protocol = %d \n", ts->touch_protocol_type);
	}
	return 0;
}

static int sitronix_get_resolution(struct sitronix_ts_data_s *ts)
{
	int ret = 0;
	uint8_t buffer[3];

	buffer[0] = XY_RESOLUTION_HIGH;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0) {
		printk("send resolution command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 3);
	if (ret < 0) {
		printk("read resolution error (%d)\n", ret);
		return ret;
	} else {
		ts->resolution_x = ((buffer[0] & (X_RES_H_BMSK << X_RES_H_SHFT)) << 4) | buffer[1];
		ts->resolution_y = ((buffer[0] & Y_RES_H_BMSK) << 8) | buffer[2];
		printk("resolution = %d x %d\n", ts->resolution_x, ts->resolution_y);
	}
	return 0;
}

static int sitronix_ts_set_powerdown_bit(struct sitronix_ts_data_s *ts, int value)
{
	int ret = 0;
	uint8_t buffer[2];

	DbgMsg("%s, value = %d\n", __func__, value);
	buffer[0] = DEVICE_CONTROL_REG;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0) {
		printk("send device control command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 1);
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

static int sitronix_ts_get_touch_info(struct sitronix_ts_data_s *ts)
{
	int ret = 0;

	ret = sitronix_get_resolution(ts);
	if (ret < 0) {
		return ret;
	}

	ret = sitronix_get_fw_revision(ts);
	if (ret < 0) {
		return ret;
	}

	ret = sitronix_get_max_touches(ts);
	if (ret < 0) {
		return ret;
	}

	if ((ts->fw_revision[0] == 0) && (ts->fw_revision[1] == 0)) {
		ts->touch_protocol_type = SITRONIX_B_TYPE;
		ts->pixel_length = PIXEL_DATA_LENGTH_B;
		printk("i2c protocol = %d \n", ts->touch_protocol_type);
		printk("max touches = %d \n",ts->max_touches);
	} else {
		ret = sitronix_get_protocol_type(ts);
		if (ret < 0) {
			return ret;
		}
		if (ts->touch_protocol_type == SITRONIX_B_TYPE) {
			ts->max_touches = 2;
			printk("max touches = %d \n",ts->max_touches);
		} else {
			ret = sitronix_get_max_touches(ts);
			if(ret < 0) {
				return ret;
			}
		}
	}

	return 0;
}

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
#ifdef SITRONIX_TOUCH_KEY
	int j;
#endif /* SITRONIX_TOUCH_KEY */
	int ret;
#ifndef SITRONIX_INT_POLLING_MODE
	struct sitronix_ts_data_s *ts = \
		container_of(work, struct sitronix_ts_data_s, work);
#else
	struct sitronix_ts_data_s *ts = \
		container_of(to_delayed_work(work), struct sitronix_ts_data_s, work);
#endif /* SITRONIX_INT_POLLING_MODE */
	uint8_t buffer[2 + SITRONIX_MAX_SUPPORTED_POINT * PIXEL_DATA_LENGTH_A];
	static MTD_STRUCTURE MTDStructure[SITRONIX_MAX_SUPPORTED_POINT]={{0}};
	uint8_t PixelCount = 0;
	static uint8_t all_clear = 1;

	DbgMsg("%s\n",  __func__);
	if (ts->suspend_state) {
		goto exit_invalid_data;
	}

	/* get finger count */
	buffer[0] = FINGERS;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0) {
		printk("send finger command error (%d)\n", ret);
	}
	ret = i2c_master_recv(ts->client,
						  buffer, 2 + ts->max_touches * ts->pixel_length);
	if (ret < 0) {
		printk("read finger error (%d)\n", ret);
		i2cErrorCount ++;
		goto exit_invalid_data;
	} else {
		i2cErrorCount = 0;
#ifdef SITRONIX_FINGER_COUNT_REG_ENABLE
		/* PixelCount = buffer[0] & FINGERS_BMSK; */
		PixelCount = ((struct stx_report_data_s *)buffer)->fingers;
#else
		for (i = 0; i < ts->max_touches; i++) {
			if (buffer[2 + i * ts->pixel_length] >= 0x80) {
				PixelCount++;
			}
		}
#endif /* SITRONIX_FINGER_COUNT_REG_ENABLE */
		DbgMsg("fingers = %d\n", PixelCount);
	}

#ifdef SITRONIX_SENSOR_KEY
	for (i = 0; i < SITRONIX_NUMBER_SENSOR_KEY; i++) {
		if (buffer[1] & (1 << i)) {
			DbgMsg("key[%d] down\n", i);
			input_report_key(ts->keyevent_input, sitronix_sensor_key[i], 1);
		} else {
			DbgMsg("key[%d] up\n", i);
			input_report_key(ts->keyevent_input, sitronix_sensor_key[i], 0);
		}
	}
#endif /* SITRONIX_SENSOR_KEY */

	for (i = 0; i < ts->max_touches; i++) {
#ifndef SITRONIX_TOUCH_KEY
		if ((buffer[2 + ts->pixel_length * i] >> X_COORD_VALID_SHFT) == 1) {
			MTDStructure[i].Pixel_X = ((buffer[2 + ts->pixel_length * i] & (X_COORD_H_BMSK << X_COORD_H_SHFT)) << 4) | (buffer[2 + ts->pixel_length * i + X_COORD_L]);
			MTDStructure[i].Pixel_Y = ((buffer[2 + ts->pixel_length * i] & Y_COORD_H_BMSK) << 8) |  (buffer[2 + ts->pixel_length * i + Y_COORD_L]);
			MTDStructure[i].Current_Pressed_area = AREA_DISPLAY;
		} else {
			MTDStructure[i].Current_Pressed_area = AREA_NONE;
		}
#else
		MTDStructure[i].Pixel_X = ((buffer[2 + ts->pixel_length * i] & (X_COORD_H_BMSK << X_COORD_H_SHFT)) << 4) |  (buffer[2 + ts->pixel_length * i + X_COORD_L]);
		MTDStructure[i].Pixel_Y = ((buffer[2 + ts->pixel_length * i] & Y_COORD_H_BMSK) << 8) |  (buffer[2 + ts->pixel_length * i + Y_COORD_L]);
		if ((buffer[2 + ts->pixel_length * i] >> X_COORD_VALID_SHFT) == 1) {
			MTDStructure[i].Current_Pressed_area = AREA_INVALID;
#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
			if ((MTDStructure[i].Pixel_X < ts->resolution_x) && (MTDStructure[i].Pixel_Y < sitronix_key_array[0].y_low)) {
#else
				if ((MTDStructure[i].Pixel_X < ts->resolution_x) && (MTDStructure[i].Pixel_Y < (ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y))) {
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
					MTDStructure[i].Current_Pressed_area = AREA_DISPLAY;
				} else {
					for (j = 0; j < SITRONIX_NUMBER_TOUCH_KEY; j++) {
						if ((MTDStructure[i].Pixel_X >= sitronix_key_array[j].x_low) && \
							(MTDStructure[i].Pixel_X <= sitronix_key_array[j].x_high) && \
							(MTDStructure[i].Pixel_Y >= sitronix_key_array[j].y_low) && \
							(MTDStructure[i].Pixel_Y <= sitronix_key_array[j].y_high)) {
							MTDStructure[i].Current_Pressed_area = AREA_KEY;
							MTDStructure[i].Current_key_index = j;
							break;
						}
					}
				}
			}
		} else {
			MTDStructure[i].Current_Pressed_area = AREA_NONE;
		}
#endif // SITRONIX_TOUCH_KEY
	}
	if (PixelCount != 0) {
		for (i = 0; i < ts->max_touches; i++) {
#ifndef SITRONIX_TOUCH_KEY
			input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
			if (MTDStructure[i].Current_Pressed_area == AREA_DISPLAY) {
				if (sitronix_invert_x == 1) {
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, (ts->resolution_x - MTDStructure[i].Pixel_X));
				} else {
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, MTDStructure[i].Pixel_X);
				}
				if (sitronix_invert_y == 1) {
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (MTDStructure[i].Pixel_Y));
				} else {
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (ts->resolution_y - MTDStructure[i].Pixel_Y));
				}
				input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 1);
				DbgMsg("[%d](%d, %d)+\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
				input_mt_sync(ts->input_dev);
			} else if (MTDStructure[i].Current_Pressed_area == AREA_NONE) {
				input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
				DbgMsg("[%d](%d, %d)-\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
				input_mt_sync(ts->input_dev);
			}
			memcpy(&sitronix_ts_gMTDPreStructure[i], &MTDStructure[i], sizeof(MTD_STRUCTURE));
#else
			if (sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_NONE) {
				if (MTDStructure[i].Current_Pressed_area == AREA_DISPLAY) {
					input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
					if (sitronix_invert_x == 1) {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, (ts->resolution_x - MTDStructure[i].Pixel_X));
					} else {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, MTDStructure[i].Pixel_X);
					}
					if (sitronix_invert_y == 1) {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (MTDStructure[i].Pixel_Y));
					} else {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (ts->resolution_y - MTDStructure[i].Pixel_Y));
					}
					input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 1);
					input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 1);
					input_mt_sync(ts->input_dev);
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_DISPLAY;
					DbgMsg("[%d](%d, %d)\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
				} else if (MTDStructure[i].Current_Pressed_area == AREA_KEY) {
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_KEY;
					sitronix_ts_gMTDPreStructure[i].First_key_index = MTDStructure[i].Current_key_index;
					input_report_key(ts->keyevent_input, sitronix_key_array[MTDStructure[i].Current_key_index].code, 1);
					DbgMsg("key [%d] down\n", MTDStructure[i].Current_key_index);
				}
			} else if (sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_DISPLAY) {
				if (MTDStructure[i].Current_Pressed_area == AREA_DISPLAY) {
					input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
					if (sitronix_invert_x == 1) {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, (ts->resolution_x - MTDStructure[i].Pixel_X));
					} else {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, MTDStructure[i].Pixel_X);
					}
					if (sitronix_invert_y == 1) {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (MTDStructure[i].Pixel_Y));
					} else {
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (ts->resolution_y - MTDStructure[i].Pixel_Y));
					}
					input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 1);
					input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 1);
					input_mt_sync(ts->input_dev);
					DbgMsg("[%d](%d, %d)+\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
				} else if (MTDStructure[i].Current_Pressed_area == AREA_KEY) {
					input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
					input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
					input_mt_sync(ts->input_dev);
					DbgMsg("[%d](%d, %d)-\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
				} else if (MTDStructure[i].Current_Pressed_area == AREA_NONE) {
					input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
					input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
					input_mt_sync(ts->input_dev);
					DbgMsg("[%d](%d, %d)-\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
				}
			} else if (sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_KEY) {
				if (MTDStructure[i].Current_Pressed_area == AREA_KEY) {
					input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 1);
					DbgMsg("key [%d] down+\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
				} else if (MTDStructure[i].Current_Pressed_area == AREA_DISPLAY) {
					input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 0);
					DbgMsg("key [%d] up\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
				} else if (MTDStructure[i].Current_Pressed_area == AREA_NONE) {
					input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 0);
					DbgMsg("key [%d] up\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
				}
			}
#endif // SITRONIX_TOUCH_KEY
		}
		all_clear = 0;
#ifdef SITRONIX_INT_POLLING_MODE
		schedule_delayed_work(&ts->work, msecs_to_jiffies(INT_POLLING_MODE_INTERVAL));
#endif // SITRONIX_INT_POLLING_MODE
	} else {
		if (all_clear == 0) {
			input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
			input_mt_sync(ts->input_dev);
			all_clear = 1;
#ifdef SITRONIX_TOUCH_KEY
			for (i = 0; i < ts->max_touches; i++) {
				if (sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_KEY) {
					input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 0);
					DbgMsg("key [%d] up\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
				}
			}
#endif // SITRONIX_TOUCH_KEY
		} else {
			DbgMsg("ignore dummy finger leave\n");
		}
#ifdef SITRONIX_INT_POLLING_MODE
		if (ts->use_irq) {
			sitronix_ts_irq_on = 1;
			enable_irq(ts->client->irq);
		}
#endif // SITRONIX_INT_POLLING_MODE
	}
	input_sync(ts->input_dev);
exit_invalid_data:
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
#endif // defined(SITRONIX_LEVEL_TRIGGERED) || defined(SITRONIX_INT_POLLING_MODE)
#ifndef SITRONIX_INT_POLLING_MODE
	schedule_work(&ts->work);
#else
	schedule_delayed_work(&ts->work, msecs_to_jiffies(0));
#endif // SITRONIX_INT_POLLING_MODE

	return IRQ_HANDLED;
}

static int st1633_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	int i;
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
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
	pdata = client->dev.platform_data; //dev_get_platdata(dev);

	if (NULL == pdata) {
		printk("No valid platform data available.\n");
		oftree = 0;
		/* ret = -ENODEV; */
		/* goto err_device_info_error; */
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
		/* printk("Can not allocate memory for input device."); */
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
#endif // SITRONIX_INT_POLLING_MODE

	ts->input_dev->name = "sitronix-i2c-touch-mt";
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);
	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(BTN_TOUCH, ts->input_dev->keybit);

#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	ts->keyevent_input = input_allocate_device();
	if (ts->keyevent_input == NULL) {
		dev_err(dev, "Can not allocate memory for key input device.\n");
		/* printk("Can not allocate memory for key input device."); */
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}
	ts->keyevent_input->name  = "sitronix-i2c-touch-key";
	set_bit(EV_KEY, ts->keyevent_input->evbit);
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
#if defined(SITRONIX_SENSOR_KEY)
	for (i = 0; i < SITRONIX_NUMBER_SENSOR_KEY; i++) {
		set_bit(sitronix_sensor_key[i], ts->keyevent_input->keybit);
	}
#endif // defined(SITRONIX_SENSOR_KEY)

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
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#endif // SITRONIX_TOUCH_KEY
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	ret = input_register_device(ts->keyevent_input);
	if (ret < 0) {
		dev_err(dev, "Can not register key input device.\n");
		/* printk("Can not register key input device."); */
		goto err_input_register_device_failed;
	}
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)

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

	ret = input_register_device(ts->input_dev);
	if(ret < 0) {
		dev_err(dev, "Can not register input device.\n");
		/* printk("Can not register input device."); */
		goto err_input_register_device_failed;
	}

	ts->suspend_state = 0;
	if (client->irq) {
#ifdef SITRONIX_LEVEL_TRIGGERED
		ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_LOW | IRQF_DISABLED, client->name, ts);
#else
		ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_FALLING | IRQF_DISABLED, client->name, ts);
#endif // SITRONIX_LEVEL_TRIGGERED
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
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
err_input_dev_alloc_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
err_device_info_error:
	return ret;
}

/**
 * @brief Local function to read data from i2c slave
 * @param[in,out] client I2C client data
 * @param[in] addr       Address to read data from slave
 * @param[in] rxbuf      Input data buffer
 * @param[in] len        Data length
 * @return Error code
 */
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
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)

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
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)
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
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)

	if(txbuf == NULL)
		return -1;
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	ret = i2c_transfer(client->adapter, &msg[0], 1);
#elif defined(SITRONIX_I2C_SINGLE_MESSAGE)
	ret = i2c_master_send(client, txbuf, len);
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)
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
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
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
#endif // SITRONIX_WAKE_UP_TOUCH_BY_INT
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
#endif // SITRONIX_WAKE_UP_TOUCH_BY_INT

	DbgMsg("%s\n", __func__);
#ifdef SITRONIX_WAKE_UP_TOUCH_BY_INT
	gpio = ts->irq_gpio;
	gpio_set_value(gpio, 0);
	gpio_direction_input(gpio);
#else
	ret = sitronix_ts_set_powerdown_bit(ts, 0);
#endif // SITRONIX_WAKE_UP_TOUCH_BY_INT

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
	{ .compatible = "stromer-st1633", .data = NULL}, //&stromer_st1633 },
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

/* module_init(st1633_ts_init); */
/* module_exit(st1633_ts_exit); */

MODULE_DESCRIPTION("Sitronix Multi-Touch Driver");
MODULE_AUTHOR("Christian Duenki <christian.duenki@escatec.com>");
MODULE_LICENSE("GPL");
