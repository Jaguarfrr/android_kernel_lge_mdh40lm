/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "kd_imgsensor.h"

#include "regulator/regulator.h"
#include "gpio/gpio.h"
/*#include "mt6306/mt6306.h"*/
#include "mclk/mclk.h"
#include "wl2864/wl2864.h"

#include "imgsensor_cfg_table.h"

enum IMGSENSOR_RETURN
	(*hw_open[IMGSENSOR_HW_ID_MAX_NUM])(struct IMGSENSOR_HW_DEVICE **) = {
	/*wl2864c 7 channel ldo open,*/
	imgsensor_hw_wl2864_open,
	imgsensor_hw_regulator_open,
	imgsensor_hw_gpio_open,
	/*imgsensor_hw_mt6306_open,*/
	imgsensor_hw_mclk_open
};

struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AFVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,
        IMGSENSOR_I2C_DEV_1,
        {
            {IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
            {IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
            {IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
            {IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_DVDD},
            {IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
            {IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
            {IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
        },
	},
    {
        IMGSENSOR_SENSOR_IDX_SUB2,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
            {IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},	
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
		{
		IMGSENSOR_SENSOR_IDX_MAIN3,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_WL2864, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
		
	},
	{IMGSENSOR_SENSOR_IDX_NONE}
};

struct IMGSENSOR_HW_POWER_SEQ platform_power_sequence[] = {
#ifdef MIPI_SWITCH
	{
		IMGSENSOR_SENSOR_IDX_NAME_SUB2,
		{
			/*{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},*/
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				1,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				1
			},
		}
	},
	{
		IMGSENSOR_SENSOR_IDX_NAME_MAIN3,
		{
			/*{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},*/
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				1,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				1
			},
		}
	},
#endif

	{NULL}
};

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#if defined(OV13B10_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV13B10_MIPI_RAW,
		{
            {PDN, Vol_Low, 5},
            {RST, Vol_Low, 5},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 1},
		},
	},
#endif

/*Back 2nd 13M*/
#if defined(HI1336_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI1336_MIPI_RAW,
		{
            {PDN, Vol_Low, 5},
            {RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High, 1},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Front 8M*/
#if defined(GC8034_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC8034_MIPI_RAW,
		{
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 5},
			{SensorMCLK, Vol_High, 50},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Front 2nd 8M*/
#if defined(HI846_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI846_MIPI_RAW,
		{
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 5},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Back Wide 5M*/
#if defined(GC5035_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC5035_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Back 2nd Wide 5M*/
#if defined(HI556_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI556_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Rear M2M*/
#if defined(GC02M1_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC02M1_MIPI_RAW,
		{
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Rear 2nd M2M*/
#if defined(OV02B_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B_MIPI_RAW,
		{
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Rear D2M*/
#if defined(GC02M1B_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC02M1B_MIPI_RAW,
		{
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

/*Rear 2nd D2M*/
#if defined(OV02B1B_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B1B_MIPI_RAW,
		{
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

	/* add new sensor before this line */
	{NULL,},
};

