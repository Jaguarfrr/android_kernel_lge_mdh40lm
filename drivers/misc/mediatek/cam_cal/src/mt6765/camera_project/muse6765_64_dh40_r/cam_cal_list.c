/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "kd_imgsensor.h"

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*M510 otp*/
	{HI1336_SENSOR_ID,  0xA0, Common_read_region},
	{HI846_SENSOR_ID,   0xA4, Common_read_region},
	{HI556_SENSOR_ID,   0xA2, Common_read_region},
	/*M710 otp*/
	{S5KGM2_SENSOR_ID,   0xA4, Common_read_region},
	{S5K3P9SX_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4H7YX_SENSOR_ID, 0xA8, Common_read_region},
	{HI1634_SENSOR_ID,   0xA0, Common_read_region},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


