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
#include "lcd_bias.h"
#include "disp_recovery.h"
#include <linux/lcd_power_mode.h>
#include <linux/lge_panel_notify.h>
#include <soc/mediatek/lge/board_lge.h>

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#ifdef BUILD_LK
    #define LCM_PRINT printf
#else
    #define LCM_PRINT printk
#endif

#ifndef FALSE
  #define FALSE   0
#endif

#ifndef TRUE
  #define TRUE    1
#endif
#define ENABLE                  1
#define DISABLE                 0
#define DSV_VOLTAGE             5800
#define USE_DEEP_SLEEP          DISABLE

#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
#define FRAME_WIDTH               (720)
#define FRAME_HEIGHT              (1600)
#define LCM_DENSITY               (267)

#define PHYSICAL_WIDTH            (68)
#define PHYSICAL_HEIGHT           (151)

#define LCM_MODULE_NAME    "TIANMA-NT36526"

#ifdef BUILD_LK
#define LCM_ID                    (0x9881)
#endif

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#ifdef BUILD_LK
static LCM_UTIL_FUNCS lcm_util;
#else
static struct LCM_UTIL_FUNCS lcm_util;
#endif

static bool flag_is_panel_deep_sleep = false;
static bool flag_deep_sleep_ctrl_available = false;

#define SET_RESET_PIN(v)                                          lcm_util.set_reset_pin((v))
#define UDELAY(n)                                                 lcm_util.udelay(n)
#define MDELAY(n)                                                 lcm_util.mdelay(n)
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
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)\
lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)

struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
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
                UDELAY(1000 * table[i].count);
            else
                MDELAY(table[i].count);
            break;

        case REGFLAG_UDELAY:
            UDELAY(table[i].count);
            break;

        case REGFLAG_END_OF_TABLE:
            break;

        default:
            dsi_set_cmdq_V22(cmdq, cmd,
                table[i].count,
                table[i].para_list,
                force_update);
        }
    }
}

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF, 3, {0x98,0x81,0x01}},
	{0x00, 1, {0x46}},
	{0x01, 1, {0x43}},
	{0x02, 1, {0x00}},
	{0x03, 1, {0x00}},
	{0x04, 1, {0x42}},
	{0x05, 1, {0x43}},
	{0x06, 1, {0x00}},
	{0x07, 1, {0x00}},
	{0x08, 1, {0x82}},
	{0x09, 1, {0x0d}},
	{0x0a, 1, {0xf3}},
	{0x0b, 1, {0x00}},
	{0x0C, 1, {0x1A}},
	{0x0D, 1, {0x1A}},
	{0x0e, 1, {0x02}},
	{0x0f, 1, {0x02}},
	{0x10, 1, {0x08}}, //gate rise EQ 20
	{0x11, 1, {0x08}}, //gate rise EQ 18
	{0x12, 1, {0x0C}},
	{0x13, 1, {0x20}},
	{0x24, 1, {0x04}},
	{0x25, 1, {0x13}},
	{0x26, 1, {0x00}},
	{0x27, 1, {0x00}},
	{0x28, 1, {0x4a}},
	{0x29, 1, {0x80}},
	{0x2c, 1, {0x34}},
	{0x31, 1, {0x6A}},
	{0x32, 1, {0x88}},
	{0x33, 1, {0xA6}},
	{0x34, 1, {0xD1}},
	{0x35, 1, {0x54}},
	{0x36, 1, {0x5D}},
	{0x37, 1, {0xD9}},
	{0x38, 1, {0xD6}},
	{0x39, 1, {0x7D}},
	{0x3A, 1, {0xC9}},
	{0x3B, 1, {0x71}},
	{0x3C, 1, {0x1C}},
	{0x3D, 1, {0xC7}},
	{0x3E, 1, {0x71}},
	{0x3F, 1, {0x1C}},
	{0x40, 1, {0xC7}},
	{0x41, 1, {0xA1}},
	{0x42, 1, {0x82}},
	{0x43, 1, {0x68}},
	{0x44, 1, {0x0A}},
	{0x45, 1, {0x49}},
	{0x46, 1, {0x94}},
	{0x47, 1, {0x85}},
	{0x48, 1, {0x69}},
	{0x49, 1, {0x9C}},
	{0x4A, 1, {0x87}},

	{0x4B, 1, {0x1C}},
	{0x4C, 1, {0xC7}},
	{0x4D, 1, {0x71}},
	{0x4E, 1, {0x1C}},
	{0x4F, 1, {0xC7}},
	{0x50, 1, {0x71}},
	{0x51, 1, {0x1C}},
	{0x52, 1, {0x6A}},
	{0x53, 1, {0x88}},
	{0x54, 1, {0xA6}},
	{0x55, 1, {0xD1}},
	{0x56, 1, {0x54}},
	{0x57, 1, {0x5D}},
	{0x58, 1, {0xD9}},
	{0x59, 1, {0xD6}},
	{0x5A, 1, {0x7D}},
	{0x5B, 1, {0xC9}},
	{0x5C, 1, {0x71}},
	{0x5D, 1, {0x1C}},
	{0x5E, 1, {0xC7}},
	{0x5F, 1, {0x71}},
	{0x60, 1, {0x1C}},
	{0x61, 1, {0xC7}},
	{0x62, 1, {0xA1}},
	{0x63, 1, {0x82}},
	{0x64, 1, {0x68}},
	{0x65, 1, {0x0A}},
	{0x66, 1, {0x49}},
	{0x67, 1, {0x94}},
	{0x68, 1, {0x85}},
	{0x69, 1, {0x69}},
	{0x6A, 1, {0x9C}},
	{0x6B, 1, {0x87}},
	{0x6C, 1, {0x1C}},
	{0x6D, 1, {0xC7}},
	{0x6E, 1, {0x71}},
	{0x6F, 1, {0x1C}},
	{0x70, 1, {0xC7}},
	{0x71, 1, {0x71}},
	{0x72, 1, {0x1C}},
	{0xb0, 1, {0x33}},
	{0xb9, 1, {0x00}},
	{0xba, 1, {0x01}},
	{0xc1, 1, {0x7f}},
	{0xd0, 1, {0x01}},
	{0xd1, 1, {0x00}},
	{0xd3, 1, {0x00}},
	{0xdc, 1, {0x17}},
	{0xdd, 1, {0x42}},
	{0xe2, 1, {0x46}},
	{0x98, 1, {0x00}},
	{0x99, 1, {0x02}},
	{0xFA, 1, {0x00}},

	{0xFF, 3, {0x98,0x81,0x02}},
	{0x06, 1, {0x8F}},
	{0x0D, 1, {0x1C}},
	{0x0E, 1, {0xD2}},
	{0x01, 1, {0x55}},
	{0x5C, 1, {0x8C}},
	{0x19, 1, {0xB5}},
	{0x1A, 1, {0xB5}},
	{0x80, 1, {0x08}},
	{0x81, 1, {0x40}},
	{0x82, 1, {0xFF}},
	{0x83, 1, {0x13}},
	{0x84, 1, {0xa0}},
	{0x85, 1, {0x09}},
	{0x0A, 1, {0xF5}},
	{0x40, 1, {0x0A}},
	{0x4A, 1, {0x20}},
	{0x4B, 1, {0x08}},
	{0x5E, 1, {0x0B}},
	{0x5F, 1, {0x20}},
	{0x47, 1, {0x00}},
	{0x48, 1, {0x12}},
	{0x49, 1, {0x02}},
	{0x4D, 1, {0x2A}},
	{0xF1, 1, {0x40}},
	{0x4C, 1, {0xD1}},
	{0x51, 1, {0x40}},

	{0xF0, 1, {0x11}},

	{0xFF, 3, {0x98,0x81,0x05}},
	{0x26, 1, {0x39}},
	{0xA5, 1, {0x92}},
	{0xA4, 1, {0x92}},
	{0x4A, 1, {0x0C}},
	{0x4B, 1, {0x10}},
	{0x03, 1, {0x01}},
	{0x04, 1, {0x00}},
	{0xA7, 1, {0xBB}},
	{0xA9, 1, {0x93}},
	{0x4E, 1, {0x51}},
	{0xA6, 1, {0xB5}},
	{0xA8, 1, {0xA1}},
	{0x47, 1, {0xC7}},
	{0x49, 1, {0x54}},
	{0x86, 1, {0x06}},
	{0x94, 1, {0xB5}},
	{0x95, 1, {0xB5}},
	{0x96, 1, {0xB5}},
	{0x97, 1, {0x6F}},
	{0x98, 1, {0xBB}},
	{0x99, 1, {0xBB}},
	{0x9A, 1, {0xBB}},
	{0x9B, 1, {0x75}},

	{0xFF, 3, {0x98,0x81,0x06}},
	{0x2E, 1, {0x01}},
	{0xC0, 1, {0x40}},
	{0xC1, 1, {0x06}},
	{0xC3, 1, {0x0E}},
	{0x20, 1, {0x45}},
	{0xDD, 1, {0x18}},

	{0xFF, 3, {0x98,0x81,0x07}},
	{0x00, 1, {0x33}},
	{0x01, 1, {0x03}},
	{0x06, 1, {0x00}},

	{0xFF, 3, {0x98,0x81,0x08}},
	{0xE0, 27, {0x00,0x24,0x69,0x9A,0xDA,0x55,0x0F,0x38,0x67,0x8F,0xA5,0xCF,0xFE,0x28,0x54,0xAA,0x7E,0xB4,0xD3,0xFC,0xFF,0x1F,0x4A,0x7E,0xA9,0x03,0xEC}},
	{0xE1, 27, {0x00,0x24,0x69,0x9A,0xDA,0x55,0x0F,0x38,0x67,0x8F,0xA5,0xCF,0xFE,0x28,0x54,0xAA,0x7E,0xB4,0xD3,0xFC,0xFF,0x1F,0x4A,0x7E,0xA9,0x03,0xEC}},

	//ILI9881X ESC check setting
	{0xFF,3,{0x98,0x81,0x06}},
	{0x48,1,{0x0F}},
	{0x4D,1,{0x80}},
	{0x4E,1,{0x40}},
	{0xC7,1,{0x05}},
	{0x3E,1,{0x60}},  //20200116

	{0xFF, 3, {0x98,0x81,0x0A}},
	{0xE0, 1, {0x01}},
	{0xE1, 1, {0x10}},
	{0xE2, 1, {0x02}},

	{0xFF, 3, {0x98,0x81,0x0B}},
	{0x94, 1, {0x88}},
	{0x95, 1, {0x21}},
	{0x96, 1, {0x06}},
	{0x97, 1, {0x06}},
	{0x98, 1, {0xCB}},
	{0x99, 1, {0xCB}},
	{0x9A, 1, {0x44}},
	{0x9B, 1, {0x99}},
	{0x9C, 1, {0x03}},
	{0x9D, 1, {0x03}},
	{0x9E, 1, {0x73}},
	{0x9F, 1, {0x73}},
	{0xA0, 1, {0x44}},
	{0xA1, 1, {0x99}},
	{0xA2, 1, {0x03}},
	{0xA3, 1, {0x03}},
	{0xA4, 1, {0x73}},
	{0xA5, 1, {0x73}},
	{0xA6, 1, {0x04}},
	{0xA7, 1, {0x42}},
	{0xA8, 1, {0x02}},
	{0xA9, 1, {0xFF}},
	{0xAA, 1, {0x02}},
	{0xAB, 1, {0xF0}},
	{0xAC, 1, {0x7F}},
	{0xAD, 1, {0x00}},
	{0xAE, 1, {0x00}},
	{0xC0, 1, {0x01}},
	{0xCB, 1, {0xCF}},
	{0xCC, 1, {0xB1}},
	{0xCD, 1, {0x9D}},
	{0xCE, 1, {0x7C}},

	{0xFF, 3, {0x98,0x81,0x0E}},
	{0x00, 1, {0xA0}},
	{0x11, 1, {0x10}},
	{0x13, 1, {0x08}},

	{0xFF, 3, {0x98,0x81,0x00}},
	{0x11, 1, {0x00}},
	{0xFF, 3, {0x98,0x81,0x0B}},	//20200116
	{0xAC, 1, {0x64}},
	{0xAD, 1, {0x24}},

	{0xFF, 3, {0x98,0x81,0x00}},
};

/*
static struct LCM_setting_table lcm_exit_sleep[] = {
    {0x05, 0x11, 1, {0x00}},
};
*/
static struct LCM_setting_table lcm_disp_on[] = {
    {0x29, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_sleep_in[] = {
    {0x05, 0x10, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_display_off[] = {
    {0x05, 0x28, 1, {0x00}},
};


static struct LCM_setting_table lcm_open_te[] = {
    {0x35, 1, {0x00}},
};


static void init_lcm_registers_sleep_in(void)
{
    MDELAY(5);

    dsi_set_cmdq_V3(lcm_initial_display_off, sizeof(lcm_initial_display_off) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(20);

    dsi_set_cmdq_V3(lcm_initial_sleep_in, sizeof(lcm_initial_sleep_in) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(120);

    LCM_PRINT("[LCD] %s\n",__func__);
}
/*
static void init_lcm_registers_sleep_out(void)
{
    dsi_set_cmdq_V3(lcm_initial_command_1, sizeof(lcm_initial_command_1) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(10);
    dsi_set_cmdq_V3(lcm_initial_command_2, sizeof(lcm_initial_command_2) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(10);
    dsi_set_cmdq_V3(lcm_initial_command_3, sizeof(lcm_initial_command_3) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(1);
    dsi_set_cmdq_V3(lcm_initial_command_4, sizeof(lcm_initial_command_4) / sizeof(struct LCM_setting_table_V3), 1);

    dsi_set_cmdq_V3(lcm_disp_on, sizeof(lcm_disp_on) / sizeof(struct LCM_setting_table_V3), 1);
    dsi_set_cmdq_V3(lcm_exit_sleep, sizeof(lcm_exit_sleep) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(140);

    LCM_PRINT("[LCD] %s\n",__func__);
}
*/
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
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
        dfps_params[0].vertical_frontporch = 180;
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
        dfps_params[1].vertical_frontporch = 180;
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

    //The following defined the fomat for data coming from LCD engine.
    params->dsi.mode                       = SYNC_PULSE_VDO_MODE;   //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE;
    params->dsi.data_format.format         = LCM_DSI_FORMAT_RGB888;
    params->dsi.PS                         = LCM_PACKED_PS_24BIT_RGB888;
    params->type                           = LCM_TYPE_DSI;
    params->width                          = FRAME_WIDTH;
    params->height                         = FRAME_HEIGHT;

    // DSI
    params->dsi.LANE_NUM                   = LCM_FOUR_LANE;
    params->dsi.vertical_sync_active                       = 4;
    params->dsi.vertical_backporch                         = 16;
    params->dsi.vertical_frontporch                        = 180;
    params->dsi.vertical_active_line       = FRAME_HEIGHT;
    params->dsi.horizontal_sync_active                     = 8;
    params->dsi.horizontal_backporch                       = 48;
    params->dsi.horizontal_frontporch                      = 56;
    params->dsi.horizontal_active_pixel    = FRAME_WIDTH;
    params->dsi.PLL_CLOCK                                  = 290;

    params->physical_width                 = PHYSICAL_WIDTH;
    params->physical_height                = PHYSICAL_HEIGHT;
#ifndef BUILD_LK
    params->density                        = LCM_DENSITY;
#endif

    params->dsi.cont_clock = FALSE;

    params->dsi.ssc_disable = 1;//default enable SSC

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

#if defined(CONFIG_LGE_MULTI_FRAME_RATE)
  lcm_dfps_int(&(params->dsi));
#endif

}

static void lcd_reset_pin(unsigned int mode)
{
    lcd_bias_set_gpio_ctrl(LCM_RST, mode);

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
	lcm_reset_ctrl(0, 2);
	MDELAY(1);
	lcm_reset_ctrl(1, 10);
	MDELAY(10);
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	MDELAY(120);
	push_table(NULL, lcm_disp_on, sizeof(lcm_disp_on) / sizeof(struct LCM_setting_table), 1);
	MDELAY(20);
	push_table(NULL, lcm_open_te, sizeof(lcm_open_te) / sizeof(struct LCM_setting_table), 1);

	LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_suspend(void)
{
    init_lcm_registers_sleep_in();
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
		lcd_reset_pin(DISABLE);
		MDELAY(2);
		lcd_reset_pin(ENABLE);
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


#ifdef BUILD_LK
LCM_DRIVER ili9881x_hdplus_dsi_vdo_inx_drv =
#else
struct LCM_DRIVER ili9881x_hdplus_dsi_vdo_inx_drv =
#endif
{
    .name           = "INNOLUX-ILI9881X",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .set_deep_sleep = lcm_set_deep_sleep,
};
