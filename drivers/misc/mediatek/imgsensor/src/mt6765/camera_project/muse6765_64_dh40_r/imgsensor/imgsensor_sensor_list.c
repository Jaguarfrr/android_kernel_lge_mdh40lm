// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
/*M710 imagesensor*/
#if defined(S5KGM2_MIPI_RAW)
	{S5KGM2_SENSOR_ID,
	SENSOR_DRVNAME_S5KGM2_MIPI_RAW,
	S5KGM2_MIPI_RAW_SensorInit},
#endif
#if defined(S5K3P9SX_MIPI_RAW)
        {S5K3P9SX_SENSOR_ID,
        SENSOR_DRVNAME_S5K3P9SX_MIPI_RAW,
        S5K3P9SX_MIPI_RAW_SensorInit},
#endif
#if defined(S5K4H7YX_MIPI_RAW)
        {S5K4H7YX_SENSOR_ID,
        SENSOR_DRVNAME_S5K4H7YX_MIPI_RAW,
        S5K4H7YX_MIPI_RAW_SensorInit},
#endif
#if defined(GC2375HDH40_MIPI_RAW)
        {GC2375HDH40_SENSOR_ID,
        SENSOR_DRVNAME_GC2375HDH40_MIPI_RAW,
        GC2375HDH40_MIPI_RAW_SensorInit},
#endif
#if defined(GC5035_MIPI_RAW)
        {GC5035_SENSOR_ID,
        SENSOR_DRVNAME_GC5035_MIPI_RAW,
        GC5035_MIPI_RAW_SensorInit},
#endif
#if defined(OV48B2Q_MIPI_RAW)
	{OV48B2Q_SENSOR_ID,
	SENSOR_DRVNAME_OV48B2Q_MIPI_RAW,
	OV48B2Q_MIPI_RAW_SensorInit},
#endif
#if defined(HI1634_MIPI_RAW)
        {HI1634_SENSOR_ID,
        SENSOR_DRVNAME_HI1634_MIPI_RAW,
        HI1634_MIPI_RAW_SensorInit},
#endif
#if defined(HI846DH40_MIPI_RAW)
        {HI846DH40_SENSOR_ID,
        SENSOR_DRVNAME_HI846DH40_MIPI_RAW,
        HI846DH40_MIPI_RAW_SensorInit},
#endif
#if defined(GC02M1_MIPI_RAW)
        {GC02M1_SENSOR_ID,
        SENSOR_DRVNAME_GC02M1_MIPI_RAW,
        GC02M1_MIPI_RAW_SensorInit},
#endif
#if defined(GC5035UNION_MIPI_RAW)
        {GC5035UNION_SENSOR_ID,
        SENSOR_DRVNAME_GC5035UNION_MIPI_RAW,
        GC5035UNION_MIPI_RAW_SensorInit},
#endif
	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};
/* e_add new sensor driver here */

