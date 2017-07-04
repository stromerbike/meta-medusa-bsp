/*
 * drivers/stromer/st1633.h
 *
 * Touchscreen driver for Sitronix ST1633i touch controller
 *
 * Copyright (C) 2017 Christian Duenki, Escatec Switzerland AG
 * christian.duenki@escatec.com
 *
 * This code is based on:
 * drivers/input/touchscreen/sitronix_i2c_touch.h
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

#ifndef __SITRONIX_I2C_TOUCH_h
#define __SITRONIX_I2C_TOUCH_h

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#define SITRONIX_TOUCH_DRIVER_VERSION 0x03
#define SITRONIX_MAX_SUPPORTED_POINT 10
//#define SITRONIX_MAX_SUPPORTED_POINT 5
#define SITRONIX_I2C_TOUCH_DRV_NAME "sitronix"
#define SITRONIX_I2C_TOUCH_DEV_NAME "sitronixDev"

#define SITRONIX_WAKE_UP_TOUCH_BY_INT

/*
 * When enable_irq() is invoked, irq will be sent once while INT is not
 * triggered if CONFIG_HARDIRQS_SW_RESEND is set. This behavior is implemented
 * by linux kernel, it is used to prevent irq from losting when irq is
 * edge-triggered mode.
 */
#define SITRONIX_LEVEL_TRIGGERED

#ifndef SITRONIX_LEVEL_TRIGGERED
#define SITRONIX_INT_POLLING_MODE
#define INT_POLLING_MODE_INTERVAL 14
#endif /* SITRONIX_LEVEL_TRIGGERED */
#define SITRONIX_FINGER_COUNT_REG_ENABLE

/*
 * MT SLOT feature is implmented in linux kernel 2.6.38 and later.
 * Make sure that version of your linux kernel before using this feature.
 */
#define SITRONIX_SUPPORT_MT_SLOT
/* #define SITRONIX_SWAP_XY */

/**
 * Enable to used combined i2c function instead of single send/recv calls
 */
#define SITRONIX_I2C_COMBINED_MESSAGE
#ifndef SITRONIX_I2C_COMBINED_MESSAGE
#define SITRONIX_I2C_SINGLE_MESSAGE
#endif /* SITRONIX_I2C_COMBINED_MESSAGE */

/**
 * Enable to allow printing debug messages to kernel log
 */
/* #define EnableDbgMsg */
#ifdef EnableDbgMsg
 #define DbgMsg(arg...) printk(arg)
#else
 #define DbgMsg(arg...)
#endif

/**
 * Enumerates all required controller registers
 */
typedef enum {
	/** Firmware version register */
	FIRMWARE_VERSION = 0x00,
	/** Status register */
	STATUS_REG = 0x01,
	/** Device control register */
	DEVICE_CONTROL_REG = 0x02,
	/** Timeout to Idle register */
	TIMEOUT_TO_IDLE_REG = 0x03,
	/** XY resolution high value register */
	XY_RESOLUTION_HIGH = 0x04,
	/** X resolution low register */
	X_RESOLUTION_LOW = 0x05,
	/** Y resolution low register */
	Y_RESOLUTION_LOW = 0x06,
	/** Sensing counter high register */
	SENSING_COUNTER_HIGH = 0x07,
	/** Sensing counter low register */
	SENSING_COUNTER_LOW = 0x08,
	/** Firmware revision register (byte 3) */
	FIRMWARE_REVISION_3 = 0x0C,
	/** Firmware revision register (byte 2) */
	FIRMWARE_REVISION_2 = 0x0D,
	/** Firmware revision register (byte 1) */
	FIRMWARE_REVISION_1 = 0x0E,
	/** Firmware revision register (byte 0) */
	FIRMWARE_REVISION_0 = 0x0F,
	/** Advanced touch information register */
	FINGERS = 0x10,
	/** Keys register */
	KEYS_REG = 0x11,
	/** XY0 Coordination high register */
	XY0_COORD_H = 0x12,
	/** X0 coordiantion low register */
	X0_COORD_L = 0x13,
	/** Y0 coordination low register */
	Y0_COORD_L = 0x14,
	/** XY1 Coordination high register */
	YY1_COORD_H = 0x16,
	/** X1 coordiantion low register */
	X1_COORD_L = 0x17,
	/** Y1 coordination low register */
	Y1_COORD_L = 0x18,
	/** XY2 Coordination high register */
	XY2_COORD_H = 0x20,
	/** X2 coordiantion low register */
	X2_COORD_L = 0x21,
	/** Y2 coordination low register */
	Y2_COORD_L = 0x22,
	/** I2C protocol register */
	I2C_PROTOCOL = 0x3E,
	/** Maximum number of contacts support register */
	MAX_NUM_TOUCHES = 0x3F,
	/** Chip ID register */
	CHIP_ID = 0xF4,
	/** Page selection register */
	PAGE_REG = 0xff,
} RegisterOffset;

#define SITRONIX_TS_CHANGE_MODE_DELAY 150

typedef enum stx_pixel_data_format_e {
	XY_COORD_H,
	X_COORD_L,
	Y_COORD_L,
	PIXEL_DATA_LENGTH_B,
	PIXEL_DATA_LENGTH_A,
} stx_pixel_data_format_t;

#define X_RES_H_SHFT 4
#define X_RES_H_BMSK 0xf
#define Y_RES_H_SHFT 0
#define Y_RES_H_BMSK 0xf
#define FINGERS_SHFT 0
#define FINGERS_BMSK 0xf
#define X_COORD_VALID_SHFT 7
#define X_COORD_VALID_BMSK 0x1
#define X_COORD_H_SHFT 4
#define X_COORD_H_BMSK 0x7
#define Y_COORD_H_SHFT 0
#define Y_COORD_H_BMSK 0x7

typedef enum i2c_protcol_type_e {
	SITRONIX_RESERVED_TYPE_0 = 0,
	SITRONIX_A_TYPE = 1,
	SITRONIX_B_TYPE = 2,
} i2c_protcol_type_t;

#define I2C_PROTOCOL_SHFT 0x0
#define I2C_PROTOCOL_BMSK 0x3

#define SMT_IOC_MAGIC   0xf1

enum {
	SMT_GET_DRIVER_REVISION = 1,
	SMT_GET_FW_REVISION,
	SMT_ENABLE_IRQ,
	SMT_DISABLE_IRQ,
	SMT_RESUME,
	SMT_SUSPEND,
	SMT_HW_RESET,
	SMT_IOC_MAXNR,
};

#define IOCTL_SMT_GET_DRIVER_REVISION    _IOC(_IOC_READ, SMT_IOC_MAGIC, SMT_GET_DRIVER_REVISION, 1)
#define IOCTL_SMT_GET_FW_REVISION        _IOC(_IOC_READ, SMT_IOC_MAGIC, SMT_GET_FW_REVISION,     4)
#define IOCTL_SMT_ENABLE_IRQ             _IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_ENABLE_IRQ,          0)
#define IOCTL_SMT_DISABLE_IRQ            _IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_DISABLE_IRQ,         0)
#define IOCTL_SMT_RESUME                 _IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_RESUME,              0)
#define IOCTL_SMT_SUSPEND                _IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_SUSPEND,             0)
#define IOCTL_SMT_HW_RESET               _IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_HW_RESET,            0)

/* #define SITRONIX_SENSOR_KEY */
#define SITRONIX_TOUCH_KEY
/* #define SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY */

typedef struct mtd_structure_s {
	u16 Pixel_X;
	u16 Pixel_Y;
	u8 First_Pressed_area;   /* 0: no press; 1: display; 2: touch key */
	u8 Current_Pressed_area; /* 0: no press; 1: display; 2: touch key */
	unsigned int First_key_index;
	unsigned int Current_key_index;
} mtd_structure_t;

#ifndef SITRONIX_TOUCH_KEY
enum {
	AREA_NONE,
	AREA_DISPLAY,
};
#else
enum {
	AREA_NONE,
	AREA_DISPLAY,
	AREA_KEY,
	AREA_INVALID,
};

struct sitronix_AA_key {
	int x_low;
	int x_high;
	int y_low;
	int y_high;
	unsigned int code;
};
#endif /* SITRONIX_TOUCH_KEY */

struct sitronix_i2c_touch_platform_data {
	/*
	 * Use this entry for panels with
	 * (major << 8 | minor) version or above.
	 * If non-zero another array entry follows
	 */
	uint32_t version;
	int (*get_int_status)(void);
	void (*reset_ic)(void);
};

#endif /* __SITRONIX_I2C_TOUCH_h */
