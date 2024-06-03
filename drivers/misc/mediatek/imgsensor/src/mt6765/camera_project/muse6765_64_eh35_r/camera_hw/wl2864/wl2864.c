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

#include "wl2864.h"

#include <soc/mediatek/lge/board_lge.h>

struct WL2864_LDOMAP  ldolist[] = {
	{IMGSENSOR_SENSOR_IDX_MAIN,   AVDD,    CAMERA_LDO_AVDD1},//for rear main(AVDD)
	{IMGSENSOR_SENSOR_IDX_MAIN,   DOVDD,   CAMERA_LDO_VDDIO},//for rear main(DOVDD)
	{IMGSENSOR_SENSOR_IDX_MAIN,   DVDD,   CAMERA_LDO_DVDD1},//for rear main(DVDD)
	{IMGSENSOR_SENSOR_IDX_SUB,    AVDD,    CAMERA_LDO_AVDD1},//for front(AVDD)
	//{IMGSENSOR_SENSOR_IDX_SUB,    DVDD,    CAMERA_LDO_DVDD1},//for front(DVDD)
	{IMGSENSOR_SENSOR_IDX_SUB,    DVDD,    CAMERA_LDO_DVDD2},//for front(DVDD)
	{IMGSENSOR_SENSOR_IDX_MAIN3,  AVDD,    CAMERA_LDO_AVDD2},//for rear main2(AVDD)
	//{IMGSENSOR_SENSOR_IDX_SUB2,   DVDD,    CAMERA_LDO_DVDD2},//for sub2(DVDD)
	//{IMGSENSOR_SENSOR_IDX_MAIN2,   DVDD,    CAMERA_LDO_DVDD1},//for sub2(DVDD)
	{IMGSENSOR_SENSOR_IDX_SUB2,  AVDD,    CAMERA_LDO_AVDD2},//for main3(AVDD)
	{IMGSENSOR_SENSOR_IDX_SUB2,  DOVDD,   CAMERA_LDO_VDDIO},//for main3(DOVDD)
};
	
/*LGE_CHANGE_S, 2020-05-27, H/W changes from RevB, seonyung.kim@lge.com*/
struct WL2864_LDOMAP  ldolist_revA[] = {
	{IMGSENSOR_SENSOR_IDX_MAIN,   AVDD,    CAMERA_LDO_AVDD1},//for rear main(AVDD)
	{IMGSENSOR_SENSOR_IDX_MAIN,   DOVDD,   CAMERA_LDO_VDDIO},//for rear main(DOVDD)
	{IMGSENSOR_SENSOR_IDX_SUB,    AVDD,    CAMERA_LDO_AVDD1},//for front(AVDD)
	//{IMGSENSOR_SENSOR_IDX_SUB,    DVDD,    CAMERA_LDO_DVDD1},//for front(DVDD)
	{IMGSENSOR_SENSOR_IDX_SUB,    DVDD,    CAMERA_LDO_DVDD2},//for front(DVDD)
	{IMGSENSOR_SENSOR_IDX_MAIN3,  AVDD,    CAMERA_LDO_AVDD2},//for rear main2(AVDD)
	//{IMGSENSOR_SENSOR_IDX_SUB2,   DVDD,    CAMERA_LDO_DVDD2},//for sub2(DVDD)
	{IMGSENSOR_SENSOR_IDX_MAIN2,   DVDD,    CAMERA_LDO_DVDD1},//for sub2(DVDD)
	{IMGSENSOR_SENSOR_IDX_SUB2,  AVDD,    CAMERA_LDO_AVDD2},//for main3(AVDD)
	{IMGSENSOR_SENSOR_IDX_SUB2,  DOVDD,   CAMERA_LDO_VDDIO},//for main3(DOVDD)
};
/*LGE_CHANGE_E, 2020-05-27, H/W changes from RevB, seonyung.kim@lge.com*/

static const int extldo_regulator_voltage[] = {
	EXTLDO_REGULATOR_VOLTAGE_0,
	EXTLDO_REGULATOR_VOLTAGE_1000,
	EXTLDO_REGULATOR_VOLTAGE_1100,
	EXTLDO_REGULATOR_VOLTAGE_1200,
	EXTLDO_REGULATOR_VOLTAGE_1210,
	EXTLDO_REGULATOR_VOLTAGE_1220,
	EXTLDO_REGULATOR_VOLTAGE_1500,
	EXTLDO_REGULATOR_VOLTAGE_1800,
	EXTLDO_REGULATOR_VOLTAGE_2500,
	EXTLDO_REGULATOR_VOLTAGE_2800,
	EXTLDO_REGULATOR_VOLTAGE_2900,
};


static enum IMGSENSOR_RETURN wl2864_init(void *instance)
{
    pr_err("wl2864_init IMGSENSOR_RETURN_SUCCESS");
	return IMGSENSOR_RETURN_SUCCESS;
}
static enum IMGSENSOR_RETURN wl2864_release(void *instance)
{
	pr_err("wl2864_release IMGSENSOR_RETURN_SUCCESS");
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN wl2864_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
int i,ret;
pr_debug("%s stoneadd hwpin=%d idx=%d pinstate=%d\n", __func__, pin,sensor_idx, pin_state);

if (pin < IMGSENSOR_HW_PIN_AVDD ||
	   pin > IMGSENSOR_HW_PIN_DOVDD ||
	   pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	   pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		ret = IMGSENSOR_RETURN_ERROR;

/*LGE_CHANGE_S, 2020-05-27, H/W changes from RevB, seonyung.kim@lge.com*/
if(lge_get_board_revno() < HW_REV_A) {
	pr_err("%s This is rev.A\n",__func__);
	for(i=0;i<(sizeof(ldolist_revA)/sizeof(ldolist_revA[0]));i++)
	{
		if ((pin == ldolist_revA[i].hwpin) && (sensor_idx == ldolist_revA[i].idx))
		{
		    pr_err("%s stoneadd got the wl2864 ldo(%d) to %d mV ! \n",__func__, ldolist_revA[i].wl2864ldo,extldo_regulator_voltage[pin_state-EXTLDO_REGULATOR_VOLTAGE_0]);
		
			if(pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0)
			{
				pr_err("%s stoneadd power on ++++++wl2864  ldo(%d) to  %d mV !\n",__func__,
						ldolist_revA[i].wl2864ldo,
						extldo_regulator_voltage[pin_state-EXTLDO_REGULATOR_VOLTAGE_0]);
				
				camera_ldo_set_ldo_value(ldolist_revA[i].wl2864ldo,extldo_regulator_voltage[pin_state-EXTLDO_REGULATOR_VOLTAGE_0]);
				camera_ldo_set_en_ldo(ldolist_revA[i].wl2864ldo,1);
			}
			else
			{
				pr_err("%s stoneadd power off ++++++wl2864  ldo(%d) \n",__func__, ldolist_revA[i].wl2864ldo);
				camera_ldo_set_en_ldo(ldolist_revA[i].wl2864ldo,0);
			}
			break;
		}
	}
}
else {
	pr_err("%s This is rev.B\n",__func__);
	for(i=0;i<(sizeof(ldolist)/sizeof(ldolist[0]));i++)
	{
		if ((pin == ldolist[i].hwpin) && (sensor_idx == ldolist[i].idx))
		{
			pr_err("%s stoneadd got the wl2864 ldo(%d) to %d mV ! \n",__func__, ldolist[i].wl2864ldo,extldo_regulator_voltage[pin_state-EXTLDO_REGULATOR_VOLTAGE_0]);
		
			if(pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0)
			{
				pr_err("%s stoneadd power on ++++++wl2864  ldo(%d) to  %d mV !\n",__func__,
						ldolist[i].wl2864ldo,
						extldo_regulator_voltage[pin_state-EXTLDO_REGULATOR_VOLTAGE_0]);
				
				camera_ldo_set_ldo_value(ldolist[i].wl2864ldo,extldo_regulator_voltage[pin_state-EXTLDO_REGULATOR_VOLTAGE_0]);
				camera_ldo_set_en_ldo(ldolist[i].wl2864ldo,1);
			}
			else
			{
				pr_err("%s stoneadd power off ++++++wl2864	ldo(%d) \n",__func__, ldolist[i].wl2864ldo);
				camera_ldo_set_en_ldo(ldolist[i].wl2864ldo,0);
			}
			break;
		}
	}
}	
/*LGE_CHANGE_E, 2020-05-27, H/W changes from RevB, seonyung.kim@lge.com*/

ret = IMGSENSOR_RETURN_SUCCESS;

    pr_err("wl2864_set IMGSENSOR_RETURN_SUCCESS");
	return ret;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.init      = wl2864_init,
	.set       = wl2864_set,
	.release   = wl2864_release,
	.id        = IMGSENSOR_HW_ID_WL2864
};

enum IMGSENSOR_RETURN imgsensor_hw_wl2864_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	
	pr_err("imgsensor_hw_wl2864_open IMGSENSOR_RETURN_SUCCESS");
	return IMGSENSOR_RETURN_SUCCESS;
}

