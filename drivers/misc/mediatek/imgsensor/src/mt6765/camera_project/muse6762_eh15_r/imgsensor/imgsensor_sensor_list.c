/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "kd_imgsensor.h"
#include "imgsensor_sensor_list.h"

/* Add Sensor Init function here
 * Note:
 * 1. Add by the resolution from ""large to small"", due to large sensor
 *    will be possible to be main sensor.
 *    This can avoid I2C error during searching sensor.
 * 2. This should be the same as
 *     mediatek\custom\common\hal\imgsensor\src\sensorlist.cpp
 */
struct IMGSENSOR_INIT_FUNC_LIST kdSensorList[MAX_NUM_OF_SUPPORT_SENSOR] = {

/*Main 13M*/
#if defined(OV13B10_MIPI_RAW)
	{OV13B10_SENSOR_ID,
	SENSOR_DRVNAME_OV13B10_MIPI_RAW,
	OV13B10_MIPI_RAW_SensorInit},
#endif

/*Back 2nd 13M*/
#if defined(HI1336_MIPI_RAW)
	{HI1336_SENSOR_ID,
	SENSOR_DRVNAME_HI1336_MIPI_RAW,
	HI1336_MIPI_RAW_SensorInit},
#endif

/*Front 8M*/
#if defined(GC8034_MIPI_RAW)
	{GC8034_SENSOR_ID,
        SENSOR_DRVNAME_GC8034_MIPI_RAW,
	GC8034_MIPI_RAW_SensorInit},
#endif

/*Front 2nd 8M*/
#if defined(HI846_MIPI_RAW)
	{HI846_SENSOR_ID,
        SENSOR_DRVNAME_HI846_MIPI_RAW,
	HI846_MIPI_RAW_SensorInit},
#endif

/*Wide 5M*/
#if defined(GC5035_MIPI_RAW)
	{GC5035_SENSOR_ID,
        SENSOR_DRVNAME_GC5035_MIPI_RAW,
	GC5035_MIPI_RAW_SensorInit},
#endif

/*Back 2nd Wide 5M*/
#if defined(HI556_MIPI_RAW)
	{HI556_SENSOR_ID,
	SENSOR_DRVNAME_HI556_MIPI_RAW,
	HI556_MIPI_RAW_SensorInit},
#endif

/*Rear M2M */
#if defined(GC02M1_MIPI_RAW)
	{GC02M1_SENSOR_ID,
	SENSOR_DRVNAME_GC02M1_MIPI_RAW,
	GC02M1_MIPI_RAW_SensorInit},
#endif

/*Rear 2nd M2M */
#if defined(OV02B_MIPI_RAW)
	{OV02B_SENSOR_ID,
	SENSOR_DRVNAME_OV02B_MIPI_RAW,
	OV02B_MIPI_RAW_SensorInit},
#endif

/*Rear D2M */
#if defined(GC02M1B_MIPI_RAW)
	{GC02M1B_SENSOR_ID,
	SENSOR_DRVNAME_GC02M1B_MIPI_RAW,
	GC02M1B_MIPI_RAW_SensorInit},
#endif

/*Rear 2nd D2M */
#if defined(OV02B1B_MIPI_RAW)
	{OV02B1B_SENSOR_ID,
	SENSOR_DRVNAME_OV02B1B_MIPI_RAW,
	OV02B1B_MIPI_RAW_SensorInit},
#endif

	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};
/* e_add new sensor driver here */

