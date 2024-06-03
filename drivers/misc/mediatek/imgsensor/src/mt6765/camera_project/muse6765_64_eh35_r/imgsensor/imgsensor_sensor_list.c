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
/*48M*/
#if defined(OV48B2Q_MIPI_RAW)
	{OV48B2Q_SENSOR_ID,
	SENSOR_DRVNAME_OV48B2Q_MIPI_RAW,
	OV48B2Q_MIPI_RAW_SensorInit},
#endif
/*Front 32M 4Cell*/
#if defined(S5KGD1_MIPI_RAW)
	{S5KGD1_SENSOR_ID,
	SENSOR_DRVNAME_S5KGD1_MIPI_RAW,
	S5KGD1_MIPI_RAW_SensorInit},
#endif
/*Front 13M**/
#if defined(OV13B10_MIPI_RAW)
	{OV13B10_SENSOR_ID,
	SENSOR_DRVNAME_OV13B10_MIPI_RAW,
	OV13B10_MIPI_RAW_SensorInit},
#endif

/*Back Wide 5M*/
#if defined(GC5035_MIPI_RAW)
	{GC5035_SENSOR_ID,
	SENSOR_DRVNAME_GC5035_MIPI_RAW,
	GC5035_MIPI_RAW_SensorInit},
#endif

/*Rear M2M */
#if defined(GC02M1_MIPI_RAW)
	{GC02M1_SENSOR_ID,
	SENSOR_DRVNAME_GC02M1_MIPI_RAW,
	GC02M1_MIPI_RAW_SensorInit},
#endif
/*Rear D2M */
#if defined(GC02M1B_MIPI_RAW)
	{GC02M1B_SENSOR_ID,
	SENSOR_DRVNAME_GC02M1B_MIPI_RAW,
	GC02M1B_MIPI_RAW_SensorInit},
#endif

	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};
/* e_add new sensor driver here */

