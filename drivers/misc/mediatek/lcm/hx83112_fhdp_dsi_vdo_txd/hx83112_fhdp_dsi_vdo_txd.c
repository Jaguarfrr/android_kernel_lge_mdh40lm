/*
 * Copyright (C) 2015 MediaTek Inc.
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

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"
#include "tps65132.h"
#include "disp_dts_gpio.h"
#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#include <debug.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif
#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif

#define LCM_PRINT printk


#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#include "disp_dts_gpio.h"
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif


#define LCM_ID_HX83112 (0x83112A)
static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;


#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH			(1080)
#define FRAME_HEIGHT		(2340)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH	(69900)
#define LCM_PHYSICAL_HEIGHT	(150980)
#define LCM_DENSITY (480)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY		0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

extern void disp_set_gpio_ctrl(unsigned int ctrl_pin, unsigned int en);
extern int tps65132_set_vspn(void);
extern bool get_esd_recovery_state(void);
//static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 1, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 1, {} },
	{REGFLAG_DELAY, 75, {} }
};

static struct LCM_setting_table init_setting[] = {
	{0xB9, 3, {0x83,0x11,0x2A} },
	{0x35, 1, {0x00} },			//open TE register
	{0xE5, 13, {0x06,0xF6,0x09,0x0C,0x00,0x00,0x00,0x00,0xF4,0xB4,
				0xE8,0x00,0xF8} },
	{0xE6, 60, {0x10,0x20,0x04,0x00,0xAA,0x00,0xF7,0xF0,0xF7,0x56,
				0xEC,0xE0,0xB0,0x88,0x55,0xC0,0x00,0x00,0x00,0xA9,
				0xF8,0xFE,0xFC,0x00,0x95,0x00,0x00,0x00,0x00,0xAA,
				0xF0,0xF0,0xF0,0xF8,0x55,0xE0,0xE0,0x88,0x80,0x55,
				0xE0,0xF0,0xF0,0xFC,0x55,0xC0,0xC0,0x90,0x90,0x55,
				0x40,0x40,0x40,0x20,0xAA,0x10,0x00,0x00,0xF0,0x6A} },
	{0xCE, 1, {0x25} },
	{0x11, 1, {} },				//Sleep out
	{REGFLAG_DELAY, 85, {} },
	{0x29, 1, {} },				//Display on
	{REGFLAG_DELAY, 40, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#if defined(CONFIG_LGE_MULTI_FRAME_RATE)
/*DynFPS*/
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
        struct dfps_info *dfps_params = dsi->dfps_params;

        dsi->dfps_enable = 1;
        dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
        dsi->dfps_def_vact_tim_fps = 6000;/*real vact timing fps * 100*/

        /*traversing array must less than DFPS_LEVELS*/
        /*DPFS_LEVEL0*/
        dfps_params[0].level = DFPS_LEVEL0;
        dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
        dfps_params[0].vact_timing_fps = 6000;/*real vact timing fps * 100*/
        /*if mipi clock solution*/
        /*dfps_params[0].PLL_CLOCK = xx;*/
        /*dfps_params[0].data_rate = xx; */
        /*if HFP solution*/
        /*dfps_params[0].horizontal_frontporch = xx;*/
        dfps_params[0].vertical_frontporch = 16;
        //dfps_params[1].vertical_frontporch_for_low_power = 540;

        /*if need mipi hopping params add here*/
        /*dfps_params[0].PLL_CLOCK_dyn =xx;
         *dfps_params[0].horizontal_frontporch_dyn =xx ;
         * dfps_params[0].vertical_frontporch_dyn = 1291;
         */

        /*DPFS_LEVEL1*/
        dfps_params[1].level = DFPS_LEVEL1;
        dfps_params[1].fps = 3000;/*real fps * 100, to support float*/
        dfps_params[1].vact_timing_fps = 3000;/*real vact timing fps * 100*/
        /*if mipi clock solution*/
        /*dfps_params[1].PLL_CLOCK = xx;*/
        /*dfps_params[1].data_rate = xx; */
        /*if HFP solution*/
        /*dfps_params[1].horizontal_frontporch = xx;*/
        dfps_params[1].vertical_frontporch = 16;
        //dfps_params[0].vertical_frontporch_for_low_power = 540;

        /*if need mipi hopping params add here*/
        /*dfps_params[1].PLL_CLOCK_dyn =xx;
         *dfps_params[1].horizontal_frontporch_dyn =xx ;
         * dfps_params[1].vertical_frontporch_dyn= 54;
         * dfps_params[1].vertical_frontporch_for_low_power_dyn =xx;
         */

        dsi->dfps_num = 2;
}
#endif

static void lcm_dsv_ctrl(unsigned int enable, unsigned int delay)
{
	if (enable) {
		disp_set_gpio_ctrl(DSV_LCD_BIAS_EN, 1);

		if (delay)
			MDELAY(delay);

		tps65132_set_vspn();

	} else {
		disp_set_gpio_ctrl(DSV_LCD_BIAS_EN, 0);

		if (delay)
			MDELAY(delay);
	}
	LCM_PRINT("[LCD] %s : %s\n",__func__,(enable)? "ON":"OFF");
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;
	params->dsi.cont_clock = 0;
#if (0)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	//params->dsi.mode = BURST_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 15;
	params->dsi.vertical_frontporch = 16;
	//params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 40;
	params->dsi.horizontal_frontporch = 60;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

#ifndef CONFIG_FPGA_EARLY_PORTING
#if (0)
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 420;
#else
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 548;
#endif
	//params->dsi.PLL_CK_CMD = 420;
	//params->dsi.PLL_CK_VDO = 440;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9D;

#if defined(CONFIG_LGE_MULTI_FRAME_RATE)
  lcm_dfps_int(&(params->dsi));
#endif

}


static void lcm_init(void)
{
	disp_set_gpio_ctrl(LCM_RST, 0);
	MDELAY(3);

	lcm_dsv_ctrl(1,1);

	MDELAY(5);
	disp_set_gpio_ctrl(LCM_RST, 1);

	MDELAY(50);

	push_table(NULL, init_setting,
		sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);

}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) /
		   sizeof(struct LCM_setting_table), 1);

	if (get_esd_recovery_state()) {
		disp_set_gpio_ctrl(LCM_RST, 0);
		MDELAY(5);
		lcm_dsv_ctrl(0, 0);
	}
}

static void lcm_resume(void)
{
	lcm_init();
}

struct LCM_DRIVER hx83112_fhdp_dsi_vdo_txd_lcm_drv = {
	.name = "TXD-HX83112",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
};
