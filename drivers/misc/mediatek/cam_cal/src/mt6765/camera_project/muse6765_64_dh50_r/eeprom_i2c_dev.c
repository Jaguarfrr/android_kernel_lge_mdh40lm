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
#include <linux/module.h>

#include "eeprom_i2c_dev.h"

#include <linux/string.h>
#include <soc/mediatek/lge/board_lge.h>//[LGE_UPDATE][bk.bae@lge.com][2020-02-13] revision check for rev11

static enum EEPROM_I2C_DEV_IDX gi2c_dev_sel[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {
	I2C_DEV_IDX_1, /* main */
	I2C_DEV_IDX_2, /* sub */
	I2C_DEV_IDX_1, /* main2 */
	I2C_DEV_IDX_3, /* sub2 */
	I2C_DEV_IDX_2, /* main3 */
};

//[LGE_UPDATE_S][bk.bae@lge.com][2020-02-13] revision check for rev11
static enum EEPROM_I2C_DEV_IDX gi2c_dev_sel_rev11[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {
	I2C_DEV_IDX_1, /* main */
	I2C_DEV_IDX_2, /* sub */
	I2C_DEV_IDX_2, /* main2 */
	I2C_DEV_IDX_3, /* sub2 */
	I2C_DEV_IDX_2, /* main3 */
};
/*LGE_CHANGE_S, 2020-04-20, Bringup DH50 SCA, soojong.jin@lge.com*/
static enum EEPROM_I2C_DEV_IDX gi2c_dev_sel_sca[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {
	I2C_DEV_IDX_1, /* main */
	I2C_DEV_IDX_2, /* sub */
	I2C_DEV_IDX_2, /* main2 */ //I2C_DEV_IDX_1, /* main2 */
	I2C_DEV_IDX_3, /* sub2 */
	I2C_DEV_IDX_2, /* main3 */
};
/*LGE_CHANGE_E, 2020-04-20, Bringup DH50 SCA, soojong.jin@lge.com*/

extern char g_lge_product_name[16]; /*LGE_CHANGE, 2020-04-20, Bringup DH50 SCA, soojong.jin@lge.com*/

enum EEPROM_I2C_DEV_IDX get_i2c_dev_sel(enum IMGSENSOR_SENSOR_IDX idx)
{

/*LGE_CHANGE_S, 2020-04-20, Bringup DH50 SCA, soojong.jin@lge.com*/
	pr_info("[LGE] get_i2c_dev_sel : g_lge_product_name : %s\n", g_lge_product_name);

	if(!strcmp(g_lge_product_name, "mdh50lm_2mfg")){
		if (idx >= IMGSENSOR_SENSOR_IDX_MIN_NUM &&
			idx < IMGSENSOR_SENSOR_IDX_MAX_NUM)
				return gi2c_dev_sel_sca[idx];
	}
	else {
		if (idx >= IMGSENSOR_SENSOR_IDX_MIN_NUM &&
			idx < IMGSENSOR_SENSOR_IDX_MAX_NUM) {
				if(lge_get_board_revno() == HW_REV_1_1) {
					return gi2c_dev_sel_rev11[idx];
			} else {
				return gi2c_dev_sel[idx];
			}
		}
	}
/*LGE_CHANGE_E, 2020-04-20, Bringup DH50 SCA, soojong.jin@lge.com*/

	return I2C_DEV_IDX_1;
}
//[LGE_UPDATE_E][bk.bae@lge.com][2020-02-13] revision check for rev11

int gi2c_dev_timing[I2C_DEV_IDX_MAX] = {
	100, /* dev1, 100k */
	100, /* dev2, 100k */
	100, /* dev3, 100k */
};

