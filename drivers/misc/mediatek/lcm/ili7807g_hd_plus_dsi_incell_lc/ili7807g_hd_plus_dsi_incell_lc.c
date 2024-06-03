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
#if defined(BUILD_LK)
#define LCM_PRINT printf
#elif defined(BUILD_UBOOT)
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif

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

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif
#include <linux/lge_panel_notify.h>
#include <linux/lcd_power_mode.h>

#include "ddp_path.h"
#include "ddp_hal.h"
extern int ddp_dsi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);
extern int ddp_dsi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle);

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


#include "sm5109.h"

/* static unsigned char lcd_id_pins_value = 0xFF; */
#define FRAME_WIDTH		(720)
#define FRAME_HEIGHT		(1600)

#define LCM_PHYSICAL_WIDTH	(69120)
#define LCM_PHYSICAL_HEIGHT	(149760)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	        0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0xFF,03,{0x78,0x07,0x00}},
	{0x28, 01, {0x00} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 01, {0x00} },
	{REGFLAG_DELAY, 150, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
//GIP timing
{0xFF,03,{0x78,0x07,0x01}},		//page1

{0x00,01,{0x43}},
{0x01,01,{0xe2}},
{0x02,01,{0x4A}},   //FTI rising  0916
{0x03,01,{0x4E}},   //FTI falling 0916
{0x04,01,{0x03}},
{0x05,01,{0x42}},
{0x06,01,{0x10}},
{0x07,01,{0x00}},
{0x9a,01,{0x00}},
{0x9b,01,{0x00}},
{0x9c,01,{0x00}},
{0x08,01,{0x83}},
{0x09,01,{0x04}},
{0x0a,01,{0x30}},   //0930
{0x0b,01,{0x01}},
{0x0c,01,{0x03}},   //CLW rising 0930
{0x0e,01,{0x81}},   //CLW falling 0930

{0x31,01,{0x07}},   //GOUTR_01
{0x32,01,{0x07}},   //GOUTR_02
{0x33,01,{0x07}},   //GOUTR_03
{0x34,01,{0x07}},   //GOUTR_04
{0x35,01,{0x3d}},   //GOUTR_05 TP_SW
{0x36,01,{0x28}},   //GOUTR_06 CT_SW
{0x37,01,{0x01}},   //GOUTR_07 U2D
{0x38,01,{0x00}},   //GOUTR_08 D2U
{0x39,01,{0x3c}},   //GOUTR_09 GOFF
{0x3a,01,{0x28}},   //GOUTR_10 XDONB
{0x3b,01,{0x28}},   //GOUTR_11 XDONB
{0x3c,01,{0x2c}},   //GOUTR_12 RST
{0x3d,01,{0x0c}},   //GOUTR_13 VEND
{0x3e,01,{0x09}},   //GOUTR_14 STV_L
{0x3f,01,{0x13}},   //GOUTR_15 XCLK_E
{0x40,01,{0x13}},   //GOUTR_16 XCLK_E
{0x41,01,{0x11}},   //GOUTR_17 CLK_E
{0x42,01,{0x11}},   //GOUTR_18 CLK_E
{0x43,01,{0x30}},   //GOUTR_19 MUX_B
{0x44,01,{0x30}},   //GOUTR_20 MUX_B
{0x45,01,{0x2f}},   //GOUTR_21 MUX_G
{0x46,01,{0x2f}},   //GOUTR_22 MUX_G
{0x47,01,{0x2e}},   //GOUTR_23 MUX_R
{0x48,01,{0x2e}},   //GOUTR_24 MUX_R

{0x49,01,{0x07}},   //GOUTL_01
{0x4a,01,{0x07}},   //GOUTL_02
{0x4b,01,{0x07}},   //GOUTL_03
{0x4c,01,{0x07}},   //GOUTL_04
{0x4d,01,{0x3d}},   //GOUTL_05 TP_SW
{0x4e,01,{0x28}},   //GOUTL_06 CT_SW
{0x4f,01,{0x01}},   //GOUTL_07 U2D
{0x50,01,{0x00}},   //GOUTL_08 D2U
{0x51,01,{0x3c}},   //GOUTL_09 GOFF
{0x52,01,{0x28}},   //GOUTL_10 XDONB
{0x53,01,{0x28}},   //GOUTL_11 XDONB
{0x54,01,{0x2c}},   //GOUTL_12 RST
{0x55,01,{0x0c}},   //GOUTL_13 VEND
{0x56,01,{0x08}},   //GOUTL_14 STV_R
{0x57,01,{0x12}},   //GOUTL_15 XCLK_O
{0x58,01,{0x12}},   //GOUTL_16 XCLK_O
{0x59,01,{0x10}},   //GOUTL_17 CLK_O
{0x5a,01,{0x10}},   //GOUTL_18 CLK_O
{0x5b,01,{0x30}},   //GOUTL_19 MUX_B
{0x5c,01,{0x30}},   //GOUTL_20 MUX_B
{0x5d,01,{0x2f}},   //GOUTL_21 MUX_G
{0x5e,01,{0x2f}},   //GOUTL_22 MUX_G
{0x5f,01,{0x2e}},   //GOUTL_23 MUX_R
{0x60,01,{0x2e}},   //GOUTL_24 MUX_R

{0x61,01,{0x07}},
{0x62,01,{0x07}},
{0x63,01,{0x07}},
{0x64,01,{0x07}},
{0x65,01,{0x3d}},
{0x66,01,{0x28}},
{0x67,01,{0x01}},
{0x68,01,{0x00}},
{0x69,01,{0x3c}},
{0x6a,01,{0x28}},
{0x6b,01,{0x28}},
{0x6c,01,{0x2c}},
{0x6d,01,{0x09}},
{0x6e,01,{0x0c}},
{0x6f,01,{0x10}},
{0x70,01,{0x10}},
{0x71,01,{0x12}},
{0x72,01,{0x12}},
{0x73,01,{0x30}},
{0x74,01,{0x30}},
{0x75,01,{0x2f}},
{0x76,01,{0x2f}},
{0x77,01,{0x2e}},
{0x78,01,{0x2e}},

{0x79,01,{0x07}},
{0x7a,01,{0x07}},
{0x7b,01,{0x07}},
{0x7c,01,{0x07}},
{0x7d,01,{0x3d}},
{0x7e,01,{0x28}},
{0x7f,01,{0x3c}},
{0x80,01,{0x00}},
{0x81,01,{0x01}},
{0x82,01,{0x28}},
{0x83,01,{0x28}},
{0x84,01,{0x2c}},
{0x85,01,{0x08}},
{0x86,01,{0x0c}},
{0x87,01,{0x11}},
{0x88,01,{0x11}},
{0x89,01,{0x13}},
{0x8a,01,{0x13}},
{0x8b,01,{0x30}},
{0x8c,01,{0x30}},
{0x8d,01,{0x2f}},
{0x8e,01,{0x2f}},
{0x8f,01,{0x2e}},
{0x90,01,{0x2e}},

{0x91,01,{0xc1}},
{0x92,01,{0x19}},
{0x93,01,{0x08}},
{0x94,01,{0x00}},
{0x95,01,{0x01}},
{0x96,01,{0x19}},
{0x97,01,{0x08}},
{0x98,01,{0x00}},
{0xa0,01,{0x83}},
{0xa1,01,{0x44}},
{0xa2,01,{0x83}},
{0xa3,01,{0x44}},
{0xa4,01,{0x61}},
{0xa5,01,{0x10}},  //STCH FTI1
{0xa6,01,{0x10}},  //STCH FTI2
{0xa7,01,{0x50}},
{0xa8,01,{0x1a}},
{0xb0,01,{0x00}},
{0xb1,01,{0x00}},
{0xb2,01,{0x00}},
{0xb3,01,{0x00}},
{0xb4,01,{0x00}},
{0xc5,01,{0x29}},
{0xc6,01,{0x90}},
{0xc7,01,{0x20}},
{0xc8,01,{0x1f}},
{0xc9,01,{0x1f}},
{0xca,01,{0x01}},
{0xd1,01,{0x11}},
{0xd2,01,{0x00}},
{0xd3,01,{0x01}},
{0xd4,01,{0x00}},
{0xd5,01,{0x00}},
{0xd6,01,{0x3d}},
{0xd7,01,{0x00}},
{0xd8,01,{0x01}},
{0xd9,01,{0x54}},
{0xda,01,{0x00}},
{0xdb,01,{0x00}},
{0xdc,01,{0x00}},
{0xdd,01,{0x00}},
{0xde,01,{0x00}},
{0xdf,01,{0x00}},
{0xe0,01,{0x00}},
{0xe1,01,{0x00}},
{0xe2,01,{0x00}},
{0xe3,01,{0x1f}},
{0xe4,01,{0x52}},
{0xe5,01,{0x09}},
{0xe6,01,{0x44}},		//sleep in/out blanking 3 frame black
{0xe7,01,{0x00}},
{0xe8,01,{0x01}},
{0xed,01,{0x55}},
{0xef,01,{0x30}},
{0xf0,01,{0x00}},
{0xf4,01,{0x54}},

{0xFF,03,{0x78,0x07,0x02}},		//page2
{0x01,01,{0x35}},               //time out 7th frame GIP toggle
{0x06,01,{0xAC}},		//BIST mode line time
{0x07,01,{0xAC}},		//IDLE mode line time

{0x78,01,{0x26}},		//1+2 SRC chopperr 1018
{0x79,01,{0x00}},		//Gamma chopper OFF 1018
{0x80,01,{0x77}},		//SRC chopperr 1018

{0xf4,01,{0x00}},
{0xf5,01,{0x00}},
{0xf6,01,{0x00}},
{0xf7,01,{0x00}},

{0x40,01,{0x0A}},		//T8 = 0.74(0.5)us 0930
{0x41,01,{0x00}},
{0x42,01,{0x07}},		//T6 = 0.4375(spec 0.5)us
{0x43,01,{0x22}},		//T5 = 2.13(2.15)MUX WIDTH
{0x46,01,{0x22}},		//0930
{0x53,01,{0x04}},		//SDT  1014
{0x54,01,{0x05}},
{0x56,01,{0x02}},
{0x5d,01,{0x07}},
{0x47,01,{0x00}},		//RGBBGR = 02

//{0x89,01,{0x20}},		//CKH EQT
//{0x8A,01,{0x02}},		//CKH EQT
//{0x8B,01,{0x00}},		//CKH EQT

{0xFF,03,{0x78,0x07,0x05}},		//page5
{0x27,01,{0x44}},		//
{0x28,01,{0x54}},		//
{0xA0,01,{0x44}},		//
{0xA2,01,{0x54}},		//

{0x2b,01,{0x08}},

{0x03,01,{0x00}},  //VCOM MSB
{0x04,01,{0x80}},  //VCOM LSB

{0x63,01,{0x7E}},  //GVDDP = 5.0V 1014
{0x64,01,{0x7E}},  //GVDDN = -5.0V 1014

{0x69,01,{0x60}},  //VGH=  9.5V
{0x6B,01,{0x56}},  //VGL= -9.0V

{0x68,01,{0x5B}},  //VGHO= 8.5V
{0x6A,01,{0x51}},  //VGL0= -8.0V

{0xFF,03,{0x78,0x07,0x06}},		//page6

{0xD6,01,{0x67}}, //FTE1=TSVD, FTE=TSHD
//{0xD8,01,{0x06}}, //LEDPWM=Hsync
{0x2E,01,{0x01}}, //NL enable
{0xC0,01,{0x1F}}, //Resolution Y=1600
{0xC1,01,{0x03}}, //Resolution Y=1600
{0xC2,01,{0x07}}, //Resolution X=720
{0xC3,01,{0x06}}, //SS direction  1015
//{0xDD,01,{0x10}}, //3LANE
//{0x7C,01,{0x40}}, //3LANE

{0x11,01,{0x03}}, //auto trim enable
{0x12,01,{0x10}}, //

{0x13,01,{0x49}},
{0x14,01,{0x41}},
{0x15,01,{0x06}},
{0x16,01,{0x41}},
{0x17,01,{0x47}},
{0x18,01,{0x3A}},
{0x1D,01,{0x44}}, //External line 10.12us 20191018
{0x1E,01,{0x03}}, //External line 10.12us 20191018

{0xC7,01,{0x05}}, //1 bit ESD check(0Ah)
{0x48,01,{0x09}}, //1 bit ESD check(0Ah)

{0xB4,01,{0xCB}},      //C_2019 11
{0xB5,01,{0x25}},      //Date 25

{0xFF,03,{0x78,0x07,0x07}},
{0x06,01,{0x90}},
{0x03,01,{0x40}},		//Gamma chopper OFF 1018


{0xFF,03,{0x78,0x07,0x08}},	 //1015
{0xE0,40,{0x00,0x30,0x44,0x68,0x00,0x86,0x9F,0xB6,0x00,0xCA,0xDC,0xEC,0x15,0x23,0x4D,0x8D,0x29,0xBD,0x04,0x3E,0x2A,0x40,0x76,0xB5,0x3E,0xDC,0x0F,0x30,0x3F,0x5A,0x66,0x72,0x3F,0x81,0x91,0xA6,0x3F,0xBC,0xCE,0xCF}},
{0xE1,40,{0x00,0x30,0x44,0x68,0x00,0x86,0x9F,0xB6,0x00,0xCA,0xDC,0xEC,0x15,0x23,0x4D,0x8D,0x29,0xBD,0x04,0x3E,0x2A,0x40,0x76,0xB5,0x3E,0xDC,0x0F,0x30,0x3F,0x5A,0x66,0x72,0x3F,0x81,0x91,0xA6,0x3F,0xBC,0xCE,0xCF}},

{0xFF,03,{0x78,0x07,0x09}},	 //1015
{0xE0,40,{0x00,0x30,0x45,0x6C,0x00,0x8B,0xA5,0xBC,0x00,0xD0,0xE2,0xF2,0x15,0x29,0x53,0x92,0x29,0xC1,0x08,0x41,0x2A,0x43,0x79,0xB8,0x3E,0xE0,0x12,0x33,0x3F,0x5B,0x67,0x75,0x3F,0x83,0x93,0xA5,0x3F,0xB9,0xCA,0xCF}},
{0xE1,40,{0x00,0x30,0x45,0x6C,0x00,0x8B,0xA5,0xBC,0x00,0xD0,0xE2,0xF2,0x15,0x29,0x53,0x92,0x29,0xC1,0x08,0x41,0x2A,0x43,0x79,0xB8,0x3E,0xE0,0x12,0x33,0x3F,0x5B,0x67,0x75,0x3F,0x83,0x93,0xA5,0x3F,0xB9,0xCA,0xCF}},

{0xFF,03,{0x78,0x07,0x0A}}, //1015
{0xE0,40,{0x00,0x30,0x4A,0x75,0x00,0x96,0xB1,0xC8,0x00,0xDC,0xEE,0xFF,0x15,0x35,0x5D,0x99,0x29,0xC7,0x0C,0x44,0x2A,0x46,0x7B,0xB9,0x3E,0xE0,0x11,0x32,0x3F,0x5A,0x66,0x73,0x3F,0x81,0x91,0xA4,0x3F,0xC0,0xCE,0xCF}},
{0xE1,40,{0x00,0x30,0x4A,0x75,0x00,0x96,0xB1,0xC8,0x00,0xDC,0xEE,0xFF,0x15,0x35,0x5D,0x99,0x29,0xC7,0x0C,0x44,0x2A,0x46,0x7B,0xB9,0x3E,0xE0,0x11,0x32,0x3F,0x5A,0x66,0x73,0x3F,0x81,0x91,0xA4,0x3F,0xC0,0xCE,0xCF}},


{0xFF,03,{0x78,0x07,0x0E}},		//pageE

{0x00,01,{0xA3}}, //long VRA
{0x4D,01,{0x88}}, //RTN
{0x47,01,{0x80}},
{0x41,01,{0x08}}, //TSVD1 setting
{0x43,01,{0x6E}}, //TSVD2 setting
{0x49,01,{0x85}}, //unit line number


{0xB0,01,{0x31}}, //term1 number
{0xB1,01,{0x89}}, //term1 period
{0xB3,01,{0x00}},
{0xB4,01,{0x33}},

{0x45,01,{0x0A}}, //term2 period
{0x46,01,{0xF6}},
{0xBC,01,{0x04}},
{0xBD,01,{0xFC}},

{0xC0,01,{0x34}}, //term3 number
{0xC6,01,{0x89}},
{0xC7,01,{0x89}},
{0xC8,01,{0x89}},
{0xC9,01,{0x89}},
{0xD2,01,{0x00}},
{0xD3,01,{0xA8}},
{0xD4,01,{0x00}},
{0xD5,01,{0xA8}},
{0xD6,01,{0x00}},
{0xD7,01,{0xA8}},
{0xD8,01,{0x00}},
{0xd9,01,{0xA8}},

{0xe0,01,{0x0A}},
{0xe1,01,{0x00}},
{0xe2,01,{0x07}},
{0xe3,01,{0x22}},

{0xe4,01,{0x04}},
{0xe5,01,{0x04}},
{0xe6,01,{0x00}},
{0xe7,01,{0x05}},
{0xe8,01,{0x00}},
{0xe9,01,{0x02}},
{0xea,01,{0x07}},

{0xFF,03,{0x78,0x07,0x0E}},		//pageE
{0x07,01,{0x21}},	//modulation on
{0x4B,01,{0x13}},	//modulation step

{0xFF,03,{0x78,0x07,0x0C}},
{0x00,01,{0x1F}},
{0x01,01,{0x29}},
{0x02,01,{0x22}},
{0x03,01,{0x43}},
{0x04,01,{0x24}},
{0x05,01,{0x56}},
{0x06,01,{0x21}},
{0x07,01,{0x36}},
{0x08,01,{0x1F}},
{0x09,01,{0x28}},
{0x0A,01,{0x20}},
{0x0B,01,{0x2F}},
{0x0C,01,{0x1E}},
{0x0D,01,{0x22}},
{0x0E,01,{0x23}},
{0x0F,01,{0x50}},
{0x10,01,{0x22}},
{0x11,01,{0x49}},
{0x12,01,{0x1E}},
{0x13,01,{0x1B}},
{0x14,01,{0x20}},
{0x15,01,{0x35}},
{0x16,01,{0x23}},
{0x17,01,{0x51}},
{0x18,01,{0x1E}},
{0x19,01,{0x1C}},
{0x1A,01,{0x23}},
{0x1B,01,{0x4A}},
{0x1C,01,{0x21}},
{0x1D,01,{0x3C}},
{0x1E,01,{0x1E}},
{0x1F,01,{0x15}},
{0x20,01,{0x1F}},
{0x21,01,{0x23}},
{0x22,01,{0x22}},
{0x23,01,{0x44}},
{0x24,01,{0x20}},
{0x25,01,{0x30}},
{0x26,01,{0x21}},
{0x27,01,{0x3D}},

//{0xFF,03,78,07,02	//Page2
//{0x36,01,01		//BIST

{0xFF,03,{0x78,0x07,00}},	//Page0

};

static struct LCM_setting_table panel_on_vdo[] = {
{0x11,01,{0x00}},		//Sleep out
//{0x36,01,{0x02}},      //SS=1
{REGFLAG_DELAY,80, {} },
{0x35,01,{0x00}},  //test
{0x29,01,{0x00}},		//Display on
//Delay,10
{REGFLAG_DELAY, 60, {} }

};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;

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
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
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
        dfps_params[0].vertical_frontporch = 38;
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
        dfps_params[1].vertical_frontporch = 38;
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
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;
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
	params->dsi.vertical_backporch = 6;
	params->dsi.vertical_frontporch = 38;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 12;
        params->dsi.horizontal_backporch = 75;
        params->dsi.horizontal_frontporch = 75;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;

        params->dsi.PLL_CLOCK = 277;    /* this value must be in MTK suggested table */

	params->lcm_seq_suspend = LCM_MIPI_VIDEO_FRAME_DONE_SUSPEND;
	params->lcm_seq_shutdown = LCM_SHUTDOWN_AFTER_DSI_OFF;
	params->lcm_seq_resume = LCM_MIPI_READY_VIDEO_FRAME_RESUME;
	params->lcm_seq_power_on = NOT_USE_RESUME;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

#if defined(CONFIG_LGE_MULTI_FRAME_RATE)
  lcm_dfps_int(&(params->dsi));
#endif

}

static void lcd_reset_pin(unsigned int mode)
{
    disp_set_gpio_ctrl(LCM_RST, mode);

    LCM_PRINT("[LCD] LCD Reset %s \n",(mode)? "High":"Low");
}

static void lcm_reset_ctrl(unsigned int enable, unsigned int delay)
{
    if(enable)
        lcd_reset_pin(enable);
    else
        lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, 0, LGE_PANEL_RESET_LOW);

    if(delay)
        MDELAY(delay);

    if(enable)
        lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, 0, LGE_PANEL_RESET_HIGH);
    else
        lcd_reset_pin(enable);

    LCM_PRINT("[LCD] %s\n",__func__);
}


static void lcm_init(void)
{
	MDELAY(5);
	lcm_reset_ctrl(0, 1);
	MDELAY(2);
	lcm_reset_ctrl(1, 1);
	MDELAY(25);
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	push_table(NULL, panel_on_vdo, sizeof(panel_on_vdo) / sizeof(struct LCM_setting_table), 1);

	LCM_PRINT("[LCD] %s\n", __func__);
}

static void lcm_suspend(void)
{

	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	flag_deep_sleep_ctrl_available = true;

	LCM_PRINT("[LCD] %s\n", __func__);
}

static void lcm_resume(void)
{
	flag_deep_sleep_ctrl_available = false;
	if(flag_is_panel_deep_sleep) {
	flag_is_panel_deep_sleep = false;
	LCM_PRINT("[LCD] %s : deep sleep mode state. call lcm_init(). \n", __func__);
	}
	lcm_init();

	LCM_PRINT("[LCD] %s\n", __func__);
}

static void lcm_enter_deep_sleep(void)
{
	if(flag_deep_sleep_ctrl_available) {
		flag_is_panel_deep_sleep = true;
		LCM_PRINT("[LCD] %s\n", __func__);
	}
}

static void lcm_exit_deep_sleep(void)
{
	if(flag_deep_sleep_ctrl_available) {
#if 0
		ddp_path_top_clock_on();
		ddp_dsi_power_on(DISP_MODULE_DSI0, NULL);
#endif
		MDELAY(5);
	//	SET_RESET_PIN(0);
		lcd_reset_pin(0);
		MDELAY(2);
	//	SET_RESET_PIN(1);
		lcd_reset_pin(1);
		MDELAY(5);
		MDELAY(20);
#if 0
		push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
		ddp_dsi_power_off(DISP_MODULE_DSI0, NULL);
		ddp_path_top_clock_off();
#endif
		flag_is_panel_deep_sleep = false;
		LCM_PRINT("[LCD] %s\n", __func__);
	}
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

	LCM_PRINT("[LCD] %s : %d \n", __func__,  mode);
}

 //#define WHITE_POINT_BASE_X 167

#if 1
static unsigned int lcm_compare_id(void)
{

	return 1;
}
#endif

struct LCM_DRIVER ili7807g_hd_plus_dsi_incell_lc_lcm_drv = {
	.name = "AUO-ILI7807G",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.set_deep_sleep = lcm_set_deep_sleep,
};
