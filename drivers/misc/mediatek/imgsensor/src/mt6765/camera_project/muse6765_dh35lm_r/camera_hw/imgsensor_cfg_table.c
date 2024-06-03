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



#include "imgsensor_cfg_table.h"

enum IMGSENSOR_RETURN
	(*hw_open[IMGSENSOR_HW_ID_MAX_NUM])(struct IMGSENSOR_HW_DEVICE **) = {
	imgsensor_hw_regulator_open,
	imgsensor_hw_gpio_open,
	/*imgsensor_hw_mt6306_open,*/
	imgsensor_hw_mclk_open
};

struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,  //S5KGD1
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AFVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,   //HI1336
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef MIPI_SWITCH
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,  //HI556
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef MIPI_SWITCH
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,   //GC2375hd
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN3,  //GC2375hm
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};

/*LGE_CHANGE_S, 2019-12-04, H/W changes for RevA, sunjae.jung@lge.com*/
struct IMGSENSOR_HW_CFG imgsensor_custom_config_revA[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,  //S5KGD1
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AFVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,   //HI1336
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef MIPI_SWITCH
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,  //HI556
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef MIPI_SWITCH
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,   //GC2375hd
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN3,  //GC2375hm
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};
/*LGE_CHANGE_E, 2019-12-04, H/W changes for RevA, sunjae.jung@lge.com*/


struct IMGSENSOR_HW_POWER_SEQ platform_power_sequence[] = {
#ifdef MIPI_SWITCH
	{
		IMGSENSOR_SENSOR_IDX_NAME_SUB,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		}
	},
	{
		IMGSENSOR_SENSOR_IDX_NAME_MAIN2,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		}
	},
#endif

	{NULL}
};

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#if defined(S5KGD1_MIPI_RAW)
		{
			SENSOR_DRVNAME_S5KGD1_MIPI_RAW,
			{
				{RST, Vol_Low, 0},
				{DVDD, Vol_1100, 1},
				{AVDD, Vol_2800, 1},
				{DOVDD, Vol_1800, 1},
				{AFVDD, Vol_2800, 0},
				{PDN, Vol_Low, 0},
				{PDN, Vol_High, 0},
				{RST, Vol_High, 1},
				{SensorMCLK, Vol_High, 5},
			},
		},
#endif
#if defined(HI1336_MIPI_RAW)
		{
			SENSOR_DRVNAME_HI1336_MIPI_RAW,
			{
				//{PDN, Vol_Low, 0},
				{RST, Vol_Low, 5},
				{AVDD, Vol_2800, 1},
				{DOVDD, Vol_1800, 1},
				{DVDD, Vol_1100, 5},
				{SensorMCLK, Vol_High, 1},
				//{PDN, Vol_High, 0},
				{RST, Vol_High, 1},
			},
		},
#endif
#if defined(HI556_MIPI_RAW)
		{
			SENSOR_DRVNAME_HI556_MIPI_RAW,
			{
				{PDN, Vol_Low, 0},
				{RST, Vol_Low, 5},
				{DOVDD, Vol_1800, 1},
				{AVDD, Vol_2800, 1},
				{DVDD, Vol_1100, 5},
				{SensorMCLK, Vol_High, 1},
				{PDN, Vol_High, 1},
				{RST, Vol_High, 1},
			},
		},
#endif
#if defined(GC2375HD_MIPI_RAW)
		{
			SENSOR_DRVNAME_GC2375HD_MIPI_RAW,
			{
				{PDN, Vol_High, 1},
				{RST, Vol_Low, 5},
				{DOVDD, Vol_1800, 1},
				{DVDD, Vol_1100, 1},
				{AVDD, Vol_2800, 1},
				{SensorMCLK, Vol_High, 1},
				{PDN, Vol_Low, 1},
				{RST, Vol_High, 1},
			},
		},
#endif
#if defined(GC2375HM_MIPI_RAW)
		{
			SENSOR_DRVNAME_GC2375HM_MIPI_RAW,
			{
				{PDN, Vol_High, 1},
				{RST, Vol_Low, 5},
				{DOVDD, Vol_1800, 1},
				{DVDD, Vol_1100, 1},
				{AVDD, Vol_2800, 1},
				{SensorMCLK, Vol_High, 1},
				{PDN, Vol_Low, 1},
				{RST, Vol_High, 1},
			},
		},
#endif
	/* add new sensor before this line */
	{NULL,},
};

/*LGE_CHANGE_S, 2019-12-04, H/W changes for RevA, sunjae.jung@lge.com*/
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence_revA[] = {
#if defined(S5KGD1_MIPI_RAW)
            {
                SENSOR_DRVNAME_S5KGD1_MIPI_RAW,
                {
                    {RST, Vol_Low, 0},
                    {DVDD, Vol_High, 1},
                    {AVDD, Vol_2800, 1},
                    {DOVDD, Vol_1800, 1},
                    {AFVDD, Vol_2800, 0},
                    {PDN, Vol_Low, 0},
                    {PDN, Vol_High, 0},
                    {RST, Vol_High, 1},
                    {SensorMCLK, Vol_High, 5},
                },
            },
#endif
#if defined(HI1336_MIPI_RAW)
            {
                SENSOR_DRVNAME_HI1336_MIPI_RAW,
                {
                    //{PDN, Vol_Low, 0},
                    {RST, Vol_Low, 5},
                    {AVDD, Vol_2800, 1},
                    {DOVDD, Vol_1800, 1},
                    {DVDD, Vol_1100, 5},
                    {SensorMCLK, Vol_High, 1},
                    //{PDN, Vol_High, 0},
                    {RST, Vol_High, 1},
                },
            },
#endif
#if defined(HI556_MIPI_RAW)
            {
                SENSOR_DRVNAME_HI556_MIPI_RAW,
                {
                    {PDN, Vol_Low, 0},
                    {RST, Vol_Low, 5},
                    {DOVDD, Vol_1800, 1},
                    {AVDD, Vol_2800, 1},
                    {DVDD, Vol_1100, 5},
                    {SensorMCLK, Vol_High, 1},
                    {PDN, Vol_High, 1},
                    {RST, Vol_High, 1},
                },
            },
#endif
#if defined(GC2375HD_MIPI_RAW)
            {
                SENSOR_DRVNAME_GC2375HD_MIPI_RAW,
                {
                    {PDN, Vol_High, 1},
                    {RST, Vol_Low, 5},
                    {DOVDD, Vol_1800, 1},
                    {DVDD, Vol_1100, 1},
                    {AVDD, Vol_2800, 1},
                    {SensorMCLK, Vol_High, 1},
                    {PDN, Vol_Low, 1},
                    {RST, Vol_High, 1},
                },
            },
#endif
#if defined(GC2375HM_MIPI_RAW)
            {
                SENSOR_DRVNAME_GC2375HM_MIPI_RAW,
                {
                    {PDN, Vol_High, 1},
                    {RST, Vol_Low, 5},
                    {DOVDD, Vol_1800, 1},
                    {DVDD, Vol_1100, 1},
                    {AVDD, Vol_2800, 1},
                    {SensorMCLK, Vol_High, 1},
                    {PDN, Vol_Low, 1},
                    {RST, Vol_High, 1},
                },
            },
#endif
/*LGE_CHANGE_E, 2019-12-04, H/W changes for RevA, sunjae.jung@lge.com*/

	/* add new sensor before this line */
	{NULL,},
};

