/*
 *  Copyright (C) 2017 MediaTek Inc.
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

#ifndef __LINUX_MT6370_PMU_BLED_H
#define __LINUX_MT6370_PMU_BLED_H

struct mt6370_dsv_config_t {
	u8 addr;
	u8 val;
};

enum DSV_EN_CTRL{
	DISABLE = 0,
	ENABLE = 1,
};

enum DSV_CTRL_PROPERTY{
	DB_EXT_EN,
	DB_PERIODIC_MODE,
	DB_FREQ_PM,
	DB_SINGLE_PIN,
	DB_PERIODIC_FIX,
	DB_VPOS_EN,
	DB_VNEG_EN,
	DB_VPOS_VNEG_DISCHARGE,
	DB_STARTUP,
	DB_VPOS_VOLTAGE,
	DB_VNEG_VOLTAGE,
	DB_VPOS_GET_VOLTAGE,
	DB_VNEG_GET_VOLTAGE,
};

#define DB_CTRL1 0xB0

#define DB_PERIODIC_MODE_MASK 1
#define DB_PERIODIC_MODE_SHIFT 7
#define DB_PERIODIC_MODE_ENABLE 1
#define DB_PERIODIC_MODE_DISABLE 0
#define DB_FREQ_PM_MASK 1
#define DB_FREQ_PM_SHIFT 6
#define DB_FREQ_PM_20HZ 0
#define DB_FREQ_PM_33HZ 1
#define DB_SINGLE_PIN_MASK 1
#define DB_SINGLE_PIN_SHIFT 5
#define DB_SINGLE_PIN_DISABLE 0
#define DB_SINGLE_PIN_ENABLE 1
#define DB_PERIODIC_FIX_MASK 1
#define DB_PERIODIC_FIX_SHIFT 4
#define DB_PERIODIC_FIX_DISABLE 0
#define DB_PERIODIC_FIX_ENABLE 1
#define DB_SCP_HICCUP_MASK 1
#define DB_SCP_HICCUP_SHIFT 3
#define DB_SCP_HICCUP_ENABLE 1
#define DB_SCP_HICCUP_DISABLE 0
#define DB_EXT_EN_MASK 1
#define DB_EXT_EN_SHIFT 0
#define DB_EXT_EN_I2C 0
#define DB_EXT_EN_EXTERNAL_PIN 1


#define DB_CTRL2 0xB1

#define DSV_SCP_HICCUP_MASK 1
#define DSV_SCP_HICCUP_SHIFT 7
#define DSV_SCP_HICCUP_ENABLE 1
#define DSV_SCP_HICCUP_DISABLE 0
#define DB_VPOS_EN_MASK 1
#define DB_VPOS_EN_SHIFT 6
#define DB_VPOS_EN_ENABLE 1
#define DB_VPOS_EN_DISABLE 0
#define DB_VPOS_DISC_MASK 1
#define DB_VPOS_DISC_SHIFT 5
#define DB_VPOS_DISC_FLOATING 0
#define DB_VPOS_DISC_DISCHARGE 1
#define DB_VPOS_20MS_MASK 1
#define DB_VPOS_20MS_SHIFT 4
#define DB_VPOS_20MS_DISABLE 0
#define DB_VPOS_20MS_ENABLE 1
#define DB_VNEG_EN_MASK 1
#define DB_VNEG_EN_SHIFT 3
#define DB_VNEG_EN_ENABLE 1
#define DB_VNEG_EN_DISABLE 0
#define DB_VNEG_DISC_MASK 1
#define DB_VNEG_DISC_SHIFT 2
#define DB_VNEG_DISC_FLOATING 0
#define DB_VNEG_DISC_DISCHARGE 1
#define DB_VNEG_20MS_MASK 1
#define DB_VNEG_20MS_SHIFT 1
#define DB_VNEG_20MS_DISABLE 0
#define DB_VNEG_20MS_ENABLE 1
#define DB_STARTUP_MASK 1
#define DB_STARTUP_SHIFT 0
#define DB_STARTUP_CLOSE_LOOP 0
#define DB_STARTUP_OPEN_LOOP 1

#define DB_VBST  0xB2

#define DB_DELAY_MASK 2
#define DB_DELAY_SHIFT 6
#define DB_DELAY_NO_CONSTRAINT 0
#define DB_DELAY_0MS 1
#define DB_DELAY_1MS 2
#define DB_DELAY_4MS 3

#define DB_VBST_VOLSEL_MASK 6
#define DB_VBST_VOLSEL_SHIFT 0
#define DB_VBST_VOLSEL_5V 0x14

#define DB_VPOS  0xB3

#define DB_VPOS_SLEW_MASK 2
#define DB_VPOS_SLEW_SHIFT 6
#define DB_VPOS_SLEW_845V 0
#define DB_VPOS_SLEW_584V 1
#define DB_VPOS_SLEW_483V 2
#define DB_VPOS_SLEW_300V 3

#define DB_VPOS_VOLSEL_MASK 6
#define DB_VPOS_VOLSEL_SHIFT 0
#define DB_VPOS_VOLSEL_4V 0x00
#define DB_VPOS_VOLSEL_5V 0x14
#define DB_VPOS_VOLSEL_5_5V 0x1E

#define DB_VNEG  0xB4

#define DB_VNEG_SLEW_MASK 2
#define DB_VNEG_SLEW_SHIFT 6
#define DB_VNEG_SLEW_1009V 0
#define DB_VNEG_SLEW_631V 1
#define DB_VNEG_SLEW_505V 2
#define DB_VNEG_SLEW_315V 3

#define DB_VNEG_VOLSEL_MASK 6
#define DB_VNEG_VOLSEL_SHIFT 0
#define DB_VNEG_VOLSEL_4V 0x00
#define DB_VNEG_VOLSEL_5V 0x14
#define DB_VNEG_VOLSEL_5_5V 0x1E


#define MT6370_DSV_REG_NUM 5

extern int mt6370_dsv_set_property(enum DSV_CTRL_PROPERTY prop, u8 val);

struct mt6370_pmu_dsv_platform_data {
	union {
		uint8_t raw;
		struct {
			uint8_t db_ext_en:1;
			uint8_t reserved:3;
			uint8_t db_periodic_fix:1;
			uint8_t db_single_pin:1;
			uint8_t db_freq_pm:1;
			uint8_t db_periodic_mode:1;
		};
	} db_ctrl1;

	union {
		uint8_t raw;
		struct {
			uint8_t db_startup:1;
			uint8_t db_vneg_20ms:1;
			uint8_t db_vneg_disc:1;
			uint8_t reserved:1;
			uint8_t db_vpos_20ms:1;
			uint8_t db_vpos_disc:1;
		};
	} db_ctrl2;

	union {
		uint8_t raw;
		struct {
			uint8_t vbst:6;
			uint8_t delay:2;
		} bitfield;
	} db_vbst;

	uint8_t db_vpos_slew;
	uint8_t db_vneg_slew;
};

#endif
