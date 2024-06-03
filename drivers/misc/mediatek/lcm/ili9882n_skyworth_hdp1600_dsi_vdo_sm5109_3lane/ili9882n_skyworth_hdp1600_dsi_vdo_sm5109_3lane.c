// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#include "ddp_hal.h"
#include "ddp_path.h"
#include "disp_recovery.h"

#include "primary_display.h"
#include <linux/lcd_power_mode.h>
#include <linux/lge_panel_notify.h>
#include <linux/input/lge_touch_notify.h>
#include <soc/mediatek/lge/board_lge.h>

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_DSI_CMD_MODE    0
#define LCM_ID_ILITEK9882N 0x83
static struct LCM_UTIL_FUNCS lcm_util;
static bool flag_is_panel_deep_sleep = false;
static bool flag_deep_sleep_ctrl_available = false;

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
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
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

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "lcm_i2c.h"

#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT		(1600)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(68040)
#define LCM_PHYSICAL_HEIGHT		(151200)
#define LCM_DENSITY			    (240)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern int ddp_path_top_clock_on(void);
extern int ddp_path_top_clock_off(void);
extern int ddp_dsi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);
extern int ddp_dsi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle);

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
    {0xFF, 3, {0x98, 0x82, 0x00} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
    // GIP Setting
    {0xFF,3,{0x98,0x82,0x01}},
    {0x00,1,{0x85}},
    {0x01,1,{0x32}},
    {0x02,1,{0x2A}},
    {0x03,1,{0x05}},
    {0x04,1,{0xC9}},
    {0x05,1,{0x32}},
    {0x06,1,{0x2A}},
    {0x07,1,{0x05}},
    {0x08,1,{0x85}},
    {0x09,1,{0x02}},
    {0x0A,1,{0x72}},
    {0x0B,1,{0x00}},
    {0x28,1,{0x00}},
    {0x29,1,{0x00}},
    {0xEE,1,{0x17}},
    {0x2A,1,{0x85}},
    {0x2B,1,{0x4A}},
    {0xF0,1,{0x37}},
    {0xE0,1,{0x7E}},
    {0x31,1,{0x07}},
    {0x32,1,{0x07}},
    {0x33,1,{0x07}},
    {0x34,1,{0x07}},
    {0x35,1,{0x00}},
    {0x36,1,{0x07}},
    {0x37,1,{0x01}},
    {0x38,1,{0x22}},
    {0x39,1,{0x23}},
    {0x3A,1,{0x07}},
    {0x3B,1,{0x10}},
    {0x3C,1,{0x12}},
    {0x3D,1,{0x14}},
    {0x3E,1,{0x16}},
    {0x3F,1,{0x07}},
    {0x40,1,{0x0C}},
    {0x41,1,{0x0E}},
    {0x42,1,{0x07}},
    {0x43,1,{0x07}},
    {0x44,1,{0x07}},
    {0x45,1,{0x07}},
    {0x46,1,{0x07}},
    {0x47,1,{0x07}},
    {0x48,1,{0x07}},
    {0x49,1,{0x07}},
    {0x4A,1,{0x07}},
    {0x4B,1,{0x00}},
    {0x4C,1,{0x07}},
    {0x4D,1,{0x01}},
    {0x4E,1,{0x22}},
    {0x4F,1,{0x23}},
    {0x50,1,{0x07}},
    {0x51,1,{0x11}},
    {0x52,1,{0x13}},
    {0x53,1,{0x15}},
    {0x54,1,{0x17}},
    {0x55,1,{0x07}},
    {0x56,1,{0x0D}},
    {0x57,1,{0x0F}},
    {0x58,1,{0x07}},
    {0x59,1,{0x07}},
    {0x5A,1,{0x07}},
    {0x5B,1,{0x07}},
    {0x5C,1,{0x07}},
    {0x61,1,{0x07}},
    {0x62,1,{0x07}},
    {0x63,1,{0x07}},
    {0x64,1,{0x07}},
    {0x65,1,{0x00}},
    {0x66,1,{0x07}},
    {0x67,1,{0x01}},
    {0x68,1,{0x22}},
    {0x69,1,{0x23}},
    {0x6A,1,{0x07}},
    {0x6B,1,{0x17}},
    {0x6C,1,{0x15}},
    {0x6D,1,{0x13}},
    {0x6E,1,{0x11}},
    {0x6F,1,{0x07}},
    {0x70,1,{0x0F}},
    {0x71,1,{0x0D}},
    {0x72,1,{0x07}},
    {0x73,1,{0x07}},
    {0x74,1,{0x07}},
    {0x75,1,{0x07}},
    {0x76,1,{0x07}},
    {0x77,1,{0x07}},
    {0x78,1,{0x07}},
    {0x79,1,{0x07}},
    {0x7A,1,{0x07}},
    {0x7B,1,{0x00}},
    {0x7C,1,{0x07}},
    {0x7D,1,{0x01}},
    {0x7E,1,{0x22}},
    {0x7F,1,{0x23}},
    {0x80,1,{0x07}},
    {0x81,1,{0x16}},
    {0x82,1,{0x14}},
    {0x83,1,{0x12}},
    {0x84,1,{0x10}},
    {0x85,1,{0x07}},
    {0x86,1,{0x0E}},
    {0x87,1,{0x0C}},
    {0x88,1,{0x07}},
    {0x89,1,{0x07}},
    {0x8A,1,{0x07}},
    {0x8B,1,{0x07}},
    {0x8C,1,{0x07}},
    {0xA0,1,{0x01}},
    {0xA1,1,{0x00}},
    {0xA2,1,{0x00}},
    {0xA3,1,{0x00}},
    {0xA4,1,{0x00}},
    {0xA7,1,{0x00}},
    {0xA8,1,{0x00}},
    {0xA9,1,{0x00}},
    {0xAA,1,{0x00}},
    {0xB0,1,{0x34}},
    {0xB2,1,{0x04}},
    {0xB9,1,{0x10}},
    {0xBA,1,{0x02}},
    {0xC1,1,{0x10}},
    {0xD0,1,{0x01}},
    {0xD1,1,{0x10}},
    {0xD2,1,{0x40}},
    {0xD5,1,{0x56}},
    {0xD6,1,{0x91}},
    {0xDc,1,{0x40}},
    {0xE2,1,{0x42}},
    {0xE6,1,{0x31}},
    {0xEA,1,{0x0F}},
    {0xE7,1,{0x54}},

    {0xFF,3,{0x98,0x82,0x02}},
    {0xF1,1,{0x1C}},
    {0x4B,1,{0x5A}},
    {0x50,1,{0xCA}},
    {0x51,1,{0x00}},
    {0x06,1,{0x8B}},
    {0x0B,1,{0xA0}},
    {0x0C,1,{0x80}},
    {0x0D,1,{0x1A}},
    {0x0E,1,{0x09}},

    // REGISTER,4D,01,CE
    {0x4E,1,{0x11}},
    {0x01,1,{0x14}},
    {0x02,1,{0x0A}},

    {0xFF,3,{0x98,0x82,0x05}},
    {0x03,1,{0x01}},
    {0x04,1,{0x1A}},
    {0x58,1,{0x61}},
    {0x63,1,{0x88}},
    {0x64,1,{0x88}},
    {0x68,1,{0xB5}},
    {0x69,1,{0xBB}},
    {0x6A,1,{0x8D}},
    {0x6B,1,{0x7F}},
    {0x85,1,{0x77}},
    {0x47,1,{0x0A}},
    {0xC8,1,{0xF1}},
    {0xC9,1,{0xF1}},
    {0xCA,1,{0xB5}},
    {0xCB,1,{0xB5}},
    {0xD0,1,{0xF7}},
    {0xD1,1,{0xF7}},
    {0xD2,1,{0xBB}},
    {0xD3,1,{0xBB}},
    {0x46,1,{0x00}},

    // Resolution
    {0xFF,3,{0x98,0x82,0x06}},
    {0xD9,1,{0x10}},
    {0x08,1,{0x00}},
    {0xC0,1,{0x40}},
    {0xC1,1,{0x16}},

    {0xFF,3,{0x98,0x82,0x07}},
    {0xC0,1,{0x01}},
    {0xCB,1,{0xC0}},
    {0xCC,1,{0xBB}},
    {0xCD,1,{0x83}},
    {0xCE,1,{0x83}},

    // Gamma Register
    {0xFF,3,{0x98,0x82,0x08}},
    {0xE0,27,{0x00,0x24,0x4B,0x6E,0x9F,0x50,0xCB,0xF1,0x1F,0x45,0x95,0x84,0xB8,0xE8,0x17,0xAA,0x48,0x84,0xA9,0xD7,0xFE,0xFD,0x2E,0x68,0x97,0x03,0xEC}},
    {0xE1,27,{0x00,0x24,0x4B,0x6E,0x9F,0x50,0xCB,0xF1,0x1F,0x45,0x95,0x84,0xB8,0xE8,0x17,0xAA,0x48,0x84,0xA9,0xD7,0xFE,0xFD,0x2E,0x68,0x97,0x03,0xEC}},

    // OSC Auto Trim Setting
    {0xFF,3,{0x98,0x82,0x0B}},
    {0x9A,1,{0x44}},
    {0x9B,1,{0x78}},
    {0x9C,1,{0x03}},
    {0x9D,1,{0x03}},
    {0x9E,1,{0x70}},
    {0x9F,1,{0x70}},
    {0xAB,1,{0xE0}},

    {0xFF,3,{0x98,0x82,0x0E}},
    {0x11,1,{0x10}},
    {0x13,1,{0x10}},
    {0x00,1,{0xA0}},

    {0xFF,3,{0x98,0x82,0x00}},
    {0x11,1,{0x00}},
    {REGFLAG_DELAY,60,{}},
    {0x29,1,{0x00}},
    {REGFLAG_DELAY,20,{}},
    {0x35,1,{0x00}},
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
			break;
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
	dfps_params[0].vertical_frontporch = 245;
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
	dfps_params[1].vertical_frontporch = 245;
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
#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	//lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	//lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	params->dsi.switch_mode_enable = 0;
	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;
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
	params->dsi.vertical_backporch = 20;
	params->dsi.vertical_frontporch = 260;
	//params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 18;
	params->dsi.horizontal_backporch = 48;
	params->dsi.horizontal_frontporch = 48;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 400;    /* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 400;    /* this value must be in MTK suggested table */
#endif
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
#ifdef CONFIG_LGE_DISPLAY_COMMON
	params->lcm_seq_resume = LCM_MIPI_READY_VIDEO_FRAME_RESUME;
	params->lcm_seq_suspend = LCM_MIPI_VIDEO_FRAME_DONE_SUSPEND;
	params->lcm_seq_shutdown = LCM_SHUTDOWN_AFTER_DSI_OFF;
	params->lcm_bl_on_delay = 40;//msec
#endif
	set_disp_esd_check_lcm(true);
#if defined(CONFIG_LGE_MULTI_FRAME_RATE)
	lcm_dfps_int(&(params->dsi));
#endif
	LCM_LOGI("[LCD] %s\n",__func__);
}

/* turn on gate ic & control voltage to 5.5V */
//extern bool g_gesture;   //add for tp gesture mode
static void lcm_init_power(void)
{
	LCM_LOGI("skyworth-ili9882n lcm_init_power");

	lcm_power_enable(20,5);

/*	JDM code

	if (g_gesture == 1)
	{
		printk("[ILITEK]TP is gesture mode,power not enable");
	} else {
		lcm_power_enable();
	}
*/
}

static void lcm_suspend_power(void)
{
	LCM_LOGI("skyworth-ili9882n lcm_suspend_power");

	if(primary_get_shutdown_scenario() != NORMAL_SUSPEND) {
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, 0, LGE_PANEL_RESET_LOW);
		lcm_reset_pin(0);
		MDELAY(2);

		lcm_power_disable(5);
	} else {
		LCM_LOGI("[LCD] normal suspend, power not disable\n");
	}

/*	//JDM code

	if(g_gesture == 1)
	{
		printk("[ILITEK]TP is gesture mode,power not disable");
	} else {
		lcm_power_disable();
	}
*/
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	LCM_LOGI("skyworth-ili9882n lcm_resume_power");

	if(primary_get_shutdown_scenario() != NORMAL_SUSPEND) {
		lcm_reset_pin(0);
		MDELAY(2);
		lcm_init_power();
	} else {
		LCM_LOGI("[LCD] normal resume_power, power not control\n");
	}
}

//extern void ili_resume_by_ddi(void);   //JDM, tp download fw
static void lcm_init(void)
{
	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, 0, LGE_PANEL_RESET_LOW);
	lcm_reset_pin(0);
	MDELAY(5);
	lcm_reset_pin(1);
	MDELAY(5);
	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, 0, LGE_PANEL_RESET_HIGH);

//	ili_resume_by_ddi();   //JDM, tp download fw
	push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	LCM_LOGI("skyworth-ili9882n-sm5109----lcm mode = vdo mode :%d----\n", lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting, ARRAY_SIZE(lcm_suspend_setting), 1);
	MDELAY(10); //[TODO]check this value...
	flag_deep_sleep_ctrl_available = true;
	LCM_LOGI("[LCD] %s\n",__func__);
}

static void lcm_resume(void)
{
	flag_deep_sleep_ctrl_available = false;

	if(flag_is_panel_deep_sleep) {
		flag_is_panel_deep_sleep = false;
		LCM_LOGI("[LCD] %s : deep sleep mode state. call lcm_init(). \n", __func__);
	}
	lcm_init();
}

static void lcm_enter_deep_sleep(void)
{
	if(flag_deep_sleep_ctrl_available) {
		flag_is_panel_deep_sleep = true;
		LCM_LOGI("[LCD] %s\n", __func__);
	}
}

static void lcm_exit_deep_sleep(void)
{
	ddp_path_top_clock_on();
	ddp_dsi_power_on(DISP_MODULE_DSI0, NULL);
	LCM_LOGI("[LCD] mipi clk / DSI on \n");

	if(flag_deep_sleep_ctrl_available) {
		lcm_reset_pin(0);
		MDELAY(5);
		lcm_reset_pin(1);
		MDELAY(5);

		flag_is_panel_deep_sleep = false;
		LCM_LOGI("[LCD] %s\n", __func__);
	}

	ddp_dsi_power_off(DISP_MODULE_DSI0, NULL);
	ddp_path_top_clock_off();
	LCM_LOGI("[LCD] mipi clk / DSI off \n");
}

static void lcm_set_deep_sleep(unsigned int mode)
{
	switch (mode){
		case DEEP_SLEEP_ENTER:
			lcm_enter_deep_sleep();
			break;
		case DEEP_SLEEP_EXIT:
			lcm_exit_deep_sleep();
			break;
		default :
			break;
	}

	LCM_LOGI("[LCD] %s : %d \n", __func__,  mode);
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int id[3] = {0x83, 0x11, 0x2B};
	unsigned int data_array[3];
	unsigned char read_buf[3];

	data_array[0] = 0x00033700; /* set max return size = 3 */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, read_buf, 3); /* read lcm id */

	LCM_LOGI("ATA read = 0x%x, 0x%x, 0x%x\n",
		 read_buf[0], read_buf[1], read_buf[2]);

	if ((read_buf[0] == id[0]) &&
	    (read_buf[1] == id[1]) &&
	    (read_buf[2] == id[2]))
		ret = 1;
	else
		ret = 0;

	return ret;
#else
	return 0;
#endif
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

#ifdef LCM_SET_DISPLAY_ON_DELAY
	lcm_set_display_on();
#endif

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
static unsigned int lcm_compare_id(void)
{
    return 1;
}

#ifdef CONFIG_LGE_DISPLAY_COMMON
static void lcm_shutdown(void)
{
	lcm_reset_pin(0);
	MDELAY(2);
	lcm_power_disable(5);
	LCM_LOGI("[LCD %s : end\n", __func__);
}

static void lcm_resume_mfts(void)
{
    LCM_LOGI("[LCD] %s : do nothing\n",__func__);
}

static void lcm_suspend_mfts(void)
{
    LCM_LOGI("[LCD] %s : do nothing\n",__func__);
}
#endif

struct LCM_DRIVER ili9882n_skyworth_hdp1600_dsi_vdo_sm5109_3lane_lcm_drv = {
	.name = "ili9882n_skyworth_hdp1600_dsi_vdo_sm5109_3lane_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
#ifdef CONFIG_LGE_DISPLAY_COMMON
	.shutdown = lcm_shutdown,
	.suspend_mfts = lcm_suspend_mfts,
	.resume_mfts = lcm_resume_mfts,
#endif
	.compare_id = lcm_compare_id,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.set_deep_sleep = lcm_set_deep_sleep,
};

