/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

#ifdef BUILD_LK
#include <string.h>
#include <mt_gpio.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "upmu_common.h"
#include <linux/string.h>
#endif

#include "lcm_drv.h"
#if defined(BUILD_LK)
#define LCM_PRINT printf
#elif defined(BUILD_UBOOT)
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif
#if defined(BUILD_LK)
#include <boot_mode.h>
#else
#include <linux/types.h>
#include <upmu_hw.h>
#endif

#include "ddp_hal.h"
#include "ddp_path.h"
#include "disp_dts_gpio.h"
#include "disp_recovery.h"
#include "../../pmic/mt6370/inc/mt6370_pmu_dsv.h"
#include <linux/lcd_power_mode.h>
#include <soc/mediatek/lge/board_lge.h>
#include <linux/lge_panel_notify.h>
#include "sm5109.h"
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

/* pixel */
#define FRAME_WIDTH             (720)
#define FRAME_HEIGHT            (1560)
#define LCM_DENSITY             (320)

/* physical dimension */
#define PHYSICAL_WIDTH          (69)
#define PHYSICAL_HEIGHT         (150)

#define ENABLE  1
#define DISABLE 0

#define REGFLAG_DELAY		    0xFC
#define REGFLAG_UDELAY	        0xFB
#define REGFLAG_END_OF_TABLE	0xFD

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static struct LCM_UTIL_FUNCS lcm_util = { 0 };
static bool flag_is_panel_deep_sleep = false;
static bool flag_deep_sleep_ctrl_available = false;

#define SET_RESET_PIN(v)        (lcm_util.set_reset_pin((v)))
#define UDELAY(n)               (lcm_util.udelay(n))
#define MDELAY(n)               (lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V3(para_tbl, size, force_update)\
lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)\
lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)\
lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)\
lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)\
lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)\
lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)\
lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

/* --------------------------------------------------------------------------- */
/* External Functions */
/* --------------------------------------------------------------------------- */

/* touch irq handle according to display suspend in MFTS */

#define LGE_LPWG_SUPPORT

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_display_off_setting[] = {
	{0x28, 0, {} },
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	{0x10, 0, {} },
};

static struct LCM_setting_table lcm_display_on_setting[] = {
	{0x29, 0, {} },
};

static struct LCM_setting_table lcm_te_on_setting[] = {
	{0x35,01,{0x00}},//TE enable
};

static struct LCM_setting_table init_setting_vdo[] = {
//GIP timing
{0xFF,03,{0x98,0x81,0x01}},
{0x00,01,{0x55}},
{0x01,01,{0x2E}},
{0x02,01,{0x00}},//10
{0x03,01,{0x00}},

{0x04,01,{0x03}},
{0x05,01,{0x25}},
{0x06,01,{0x00}},
{0x07,01,{0x00}},

{0x08,01,{0x84}},
{0x09,01,{0x85}},//DUMMT CK
{0x0A,01,{0xF5}},
{0x0B,01,{0x00}},//clk keep 10 off 00  10
{0x0C,01,{0x06}},//01
{0x0D,01,{0x06}},//01
{0x0E,01,{0x00}},//10
{0x0F,01,{0x00}},//10

{0x10,01,{0x00}},
{0x11,01,{0x00}},
{0x12,01,{0x00}},

{0x14,01,{0x87}},
{0x15,01,{0x87}},
{0x16,01,{0x84}},
{0x17,01,{0x85}},
{0x18,01,{0x75}},
{0x19,01,{0x00}},
{0x1A,01,{0x06}},
{0x1B,01,{0x06}},
{0x1C,01,{0x00}},
{0x1D,01,{0x00}},
{0x1E,01,{0x00}},
{0x1F,01,{0x00}},
{0x20,01,{0x00}},
{0x22,01,{0x87}},

{0x23,01,{0x87}},
{0x2A,01,{0x8B}},
{0x2B,01,{0x4E}},

//FW
{0x31,01,{0x2A}},//STV_C
{0x32,01,{0x2A}},
{0x33,01,{0x0C}},//FW
{0x34,01,{0x0C}},//BW
{0x35,01,{0x23}},//VGL
{0x36,01,{0x23}},
{0x37,01,{0x2A}},
{0x38,01,{0x2A}},
{0x39,01,{0x10}},
{0x3A,01,{0x07}},//CLK8
{0x3B,01,{0x18}},//CLK6
{0x3C,01,{0x12}},
{0x3D,01,{0x1A}},
{0x3E,01,{0x14}},//CLK4
{0x3F,01,{0x1C}},//CLK2
{0x40,01,{0x16}},//STV_A
{0x41,01,{0x1E}},
{0x42,01,{0x08}},
{0x43,01,{0x08}},
{0x44,01,{0x2A}},
{0x45,01,{0x2A}},
{0x46,01,{0x2A}},

{0x47,01,{0x2A}},//STV_C
{0x48,01,{0x2A}},
{0x49,01,{0x0D}},//FW
{0x4A,01,{0x0D}},//BW
{0x4B,01,{0x23}},//VGL
{0x4C,01,{0x23}},
{0x4D,01,{0x2A}},
{0x4E,01,{0x2A}},
{0x4F,01,{0x11}},
{0x50,01,{0x07}},//CLK7
{0x51,01,{0x19}},//CLK5
{0x52,01,{0x13}},
{0x53,01,{0x1B}},
{0x54,01,{0x15}},//CLK3
{0x55,01,{0x1D}},//CLK1
{0x56,01,{0x17}},//STV_A
{0x57,01,{0x1F}},
{0x58,01,{0x09}},
{0x59,01,{0x09}},
{0x5A,01,{0x2A}},
{0x5B,01,{0x2A}},
{0x5C,01,{0x2A}},

//BW
{0x61,01,{0x2A}},
{0x62,01,{0x2A}},
{0x63,01,{0x09}},
{0x64,01,{0x09}},
{0x65,01,{0x2A}},
{0x66,01,{0x2A}},
{0x67,01,{0x23}},
{0x68,01,{0x23}},
{0x69,01,{0x17}},
{0x6A,01,{0x07}},
{0x6B,01,{0x1F}},
{0x6C,01,{0x15}},
{0x6D,01,{0x1D}},
{0x6E,01,{0x13}},
{0x6F,01,{0x1B}},
{0x70,01,{0x11}},
{0x71,01,{0x19}},
{0x72,01,{0x0D}},
{0x73,01,{0x0D}},
{0x74,01,{0x2A}},
{0x75,01,{0x2A}},
{0x76,01,{0x2A}},

{0x77,01,{0x2A}},
{0x78,01,{0x2A}},
{0x79,01,{0x08}},
{0x7A,01,{0x08}},
{0x7B,01,{0x2A}},
{0x7C,01,{0x2A}},
{0x7D,01,{0x23}},
{0x7E,01,{0x23}},
{0x7F,01,{0x16}},
{0x80,01,{0x07}},
{0x81,01,{0x1E}},
{0x82,01,{0x14}},
{0x83,01,{0x1C}},
{0x84,01,{0x12}},
{0x85,01,{0x1A}},
{0x86,01,{0x10}},
{0x87,01,{0x18}},
{0x88,01,{0x0C}},
{0x89,01,{0x0C}},
{0x8A,01,{0x2A}},
{0x8B,01,{0x2A}},
{0x8C,01,{0x2A}},

{0xB9,01,{0x10}},
{0xC3,01,{0x00}},
{0xC4,01,{0x80}},

{0xD3,01,{0x20}},
{0xDD,01,{0x20}},

{0xD1,01,{0x23}},
{0xD5,01,{0x05}},
{0xD6,01,{0x91}},
{0xD7,01,{0x01}},
{0xD8,01,{0x15}},
{0xD9,01,{0x55}},
{0xDA,01,{0x65}},

{0xE2,01,{0x55}},
{0xE6,01,{0x45}},

{0xFF,03,{0x98,0x81,0x02}},
{0x4B,01,{0x5A}},
{0x4D,01,{0x4E}},
{0x4E,01,{0x00}},
{0x1A,01,{0x48}},
{0x06,01,{0x90}},

// GVDDP GVDDN VCOM VGH VGHO VGL VGLO setup
{0xFF,03,{0x98,0x81,0x05}},
{0x03,01,{0x00}},//VCOM
{0x04,01,{0xB0}},//VCOM
{0x58,01,{0x62}},//VGL x2
{0x63,01,{0x88}},//GVDDN   -5.14V
{0x64,01,{0x88}},//GVDDP    5.14V
{0x68,01,{0x79}},//VGHO   15V
{0x69,01,{0x7F}},//VGH    16V
{0x6A,01,{0x79}},//VGLO  79  -10V    -12V
{0x6B,01,{0x6B}},//VGL   6B -11V   -13V

// Resolution 720RGB*1560
{0xFF,03,{0x98,0x81,0x06}},
{0x2E,01,{0x01}},//NL enable
{0xC0,01,{0x0B}},
{0xC1,01,{0x03}},
//frame target 60Hz
{0x11,01,{0x03}},
{0x13,01,{0x45}},
{0x14,01,{0x41}},
{0x15,01,{0xF1}},
{0x16,01,{0x40}},
{0x17,01,{0xFF}},
{0x18,01,{0x00}},

{0xC2,01,{0x04}},
{0x27,01,{0xFF}},
{0x28,01,{0x20}},
{0x48,01,{0x0F}},
{0x4D,01,{0x80}},
{0x4E,01,{0x40}},
{0x7F,01,{0x78}},
{0xD6,01,{0x85}},//FTE=TSVD1,0x FTE1=TSHD

{0xFF,03,{0x98,0x81,0x06}},
{0x12,01,{0x00}},
{0x94,01,{0x01}},
{0xC7,01,{0x05}},   //20191212 for ESD check

{0xFF,03,{0x98,0x81,0x08}},	  //0809
{0xE0,27,{0x40,0x1A,0x96,0xCB,0x09,0x55,0x3C,0x61,0x8E,0xB0,0xA9,0xE5,0x0F,0x35,0x59,0xAA,0x7F,0xAE,0xCC,0xF4,0xFF,0x16,0x41,0x73,0x97,0x03,0xE6}},
{0xE1,27,{0x40,0x6D,0x96,0xCB,0x09,0x55,0x3C,0x61,0x8E,0xB0,0xA9,0xE5,0x0F,0x35,0x59,0xAA,0x7F,0xAE,0xCC,0xF4,0xFF,0x16,0x41,0x73,0x97,0x03,0x93}},


{0xFF,03,{0x98,0x81,0x0E}},
{0x00,01,{0xA0}},//LV mode
{0x01,01,{0x26}},//LV mode
{0x13,01,{0x10}}, // THSD time updata 0703 0F


{0xFF,03,{0x98,0x81,0x04}},
{0x00,01,{0x07}},//Hue enable
{0x02,01,{0x45}},//CE & Hue 24 axis enable

{0x0A,01,{0x00}},//0 Red
{0x0B,01,{0x00}},//1
{0x0C,01,{0x00}},//2
{0x0D,01,{0x21}},//3
{0x0E,01,{0x22}},//4 Yellow
{0x0F,01,{0x21}},//5
{0x10,01,{0x00}},//6
{0x11,01,{0x00}},//7
{0x12,01,{0x00}},//8
{0x13,01,{0x00}},//9
{0x14,01,{0x00}},//10
{0x15,01,{0x00}},//11
{0x16,01,{0x00}},//12
{0x17,01,{0x00}},//13
{0x18,01,{0x00}},//14
{0x19,01,{0x00}},//15
{0x1A,01,{0x00}},//16
{0x1B,01,{0x00}},//17
{0x1C,01,{0x00}},//18
{0x1D,01,{0x00}},//19
{0x1E,01,{0x00}},//20
{0x1F,01,{0x00}},//21
{0x20,01,{0x00}},//22
{0x21,01,{0x00}},//23

{0x3C,01,{0x00}},//0 Red
{0x3D,01,{0x00}},//1
{0x3E,01,{0x00}},//2
{0x3F,01,{0x11}},//3
{0x40,01,{0x12}},//4 Yellow
{0x41,01,{0x11}},//5
{0x42,01,{0x00}},//6
{0x43,01,{0x00}},//7
{0x44,01,{0x00}},//8
{0x45,01,{0x00}},//9
{0x46,01,{0x00}},//10
{0x47,01,{0x00}},//11
{0x48,01,{0x00}},//12
{0x49,01,{0x00}},//13
{0x4A,01,{0x00}},//14
{0x4B,01,{0x00}},//15
{0x4C,01,{0x00}},//16
{0x4D,01,{0x00}},//17
{0x4E,01,{0x00}},//18
{0x4F,01,{0x00}},//19
{0x50,01,{0x00}},//20
{0x51,01,{0x00}},//21
{0x52,01,{0x00}},//22
{0x53,01,{0x00}},//23


{0xFF,03,{0x98,0x81,0x00}},//Page0

{0x11,00,{}},

    //{0xFF,03,{0x98,0x81,0x0F}}//PageF
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
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
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
	}
    }
}

static void init_lcm_registers_sleep_in(void)
{
    push_table(lcm_display_off_setting, sizeof(lcm_display_off_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(50);
    push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(120);

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void init_lcm_registers(void)
{
    push_table(init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
    MDELAY(120);
    push_table(lcm_display_on_setting, sizeof(lcm_display_on_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(100);
    push_table(lcm_te_on_setting, sizeof(lcm_te_on_setting) / sizeof(struct LCM_setting_table), 1);

    LCM_PRINT("[LCD] %s\n",__func__);
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
    memcpy((void *)&lcm_util, (void *)util, sizeof(struct LCM_UTIL_FUNCS));
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
	dfps_params[0].vertical_frontporch = 240;
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
	dfps_params[1].vertical_frontporch = 240;
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

    /* physical size */
    params->physical_width = PHYSICAL_WIDTH;
    params->physical_height = PHYSICAL_HEIGHT;
    params->density = LCM_DENSITY;

    params->dsi.mode = SYNC_PULSE_VDO_MODE;
    /* enable tearing-free */
    params->dbi.te_mode = LCM_DBI_TE_MODE_DISABLED;
    params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

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
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.vertical_sync_active = 2;
    params->dsi.vertical_backporch = 20;
    params->dsi.vertical_frontporch = 240;
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 20;
    params->dsi.horizontal_backporch = 28;
    params->dsi.horizontal_frontporch = 28;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 285; // mipi clk : 570Mhz
    params->dsi.ssc_disable = 1;

    params->dsi.HS_PRPR = 6;
    params->dsi.HS_TRAIL = 6;

#if defined(CONFIG_LGE_DISPLAY_ESD_RECOVERY)
    params->dsi.esd_check_enable = 1;
    params->dsi.customization_esd_check_enable = 1;

    params->dsi.lcm_esd_check_table[1].cmd = 0x0A;
    params->dsi.lcm_esd_check_table[1].count = 1;
    params->dsi.lcm_esd_check_table[1].para_list[0] = 0x9C;

    set_disp_esd_check_lcm(true);
#endif

    params->lcm_seq_suspend = LCM_MIPI_VIDEO_FRAME_DONE_SUSPEND;
    params->lcm_seq_shutdown = LCM_SHUTDOWN_AFTER_DSI_OFF;
    params->lcm_seq_resume = LCM_MIPI_READY_VIDEO_FRAME_RESUME;
    params->lcm_seq_power_on = NOT_USE_RESUME;

    params->esd_powerctrl_support = false;

#if defined(CONFIG_LGE_MULTI_FRAME_RATE)
  lcm_dfps_int(&(params->dsi));
#endif

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_dsv_ctrl(unsigned int enable, unsigned int delay)
{
    if(enable==1){
        disp_set_gpio_ctrl(DSV_VPOS_EN, 1);
        sm5109_set_vspn_value(6000);

        if(delay)
            MDELAY(delay);

        disp_set_gpio_ctrl(DSV_VNEG_EN, 1);
    } else {
        disp_set_gpio_ctrl(DSV_VNEG_EN, 0);

        if(delay)
            MDELAY(delay);

        disp_set_gpio_ctrl(DSV_VPOS_EN, 0);
    }

    LCM_PRINT("[LCD] %s : %s\n",__func__,(enable)? "ON":"OFF");
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
    lcm_dsv_ctrl(ENABLE, 5);
    MDELAY(5);
    lcm_reset_ctrl(DISABLE, 5);
    MDELAY(5);
    lcm_reset_ctrl(ENABLE, 5);
    MDELAY(55);

    init_lcm_registers();

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_suspend(void)
{
    init_lcm_registers_sleep_in();
    MDELAY(10);
    flag_deep_sleep_ctrl_available = true;

    LCM_PRINT("[LCD] %s\n",__func__);
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

static void lcm_shutdown(void)
{
    MDELAY(150);
    lcm_reset_ctrl(DISABLE, 5);
    MDELAY(5);
    lcm_dsv_ctrl(DISABLE, 5);
    MDELAY(5);

    flag_is_panel_deep_sleep = true;

    LCM_PRINT("[LCD] %s\n",__func__);
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
	lcd_reset_pin(0);
	MDELAY(2);
	lcd_reset_pin(1);
	MDELAY(10);

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

/* --------------------------------------------------------------------------- */
/* Get LCM Driver Hooks */
/* --------------------------------------------------------------------------- */
struct LCM_DRIVER ili9881h_hdplus_dsi_vdo_lce_drv = {
    .name = "MANTIX-ILI9881H",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params = lcm_get_params,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
    .shutdown = lcm_shutdown,
    .set_deep_sleep = lcm_set_deep_sleep,
};
