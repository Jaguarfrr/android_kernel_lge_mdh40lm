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
//#include <platform/upmu_hw.h>
#endif

#include "ddp_hal.h"
#include "ddp_path.h"
#include "../../lcd_bias/lcd_bias.h"
#include "disp_recovery.h"
#include <linux/lcd_power_mode.h>
#include <linux/lge_panel_notify.h>
#include <soc/mediatek/lge/board_lge.h>
#ifdef CONFIG_MT6370_PMU_DSV
#include "../../pmic/mt6370/inc/mt6370_pmu_dsv.h"
#endif

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

/* pixel */
#define FRAME_WIDTH             (720)
#define FRAME_HEIGHT            (1440)
#define LCM_DENSITY             (320)

/* physical dimension */
#define PHYSICAL_WIDTH          (62)
#define PHYSICAL_HEIGHT         (124)

#define ENABLE                  1
#define DISABLE                 0

#define DSV_VOLTAGE             6000
#define USE_DEEP_SLEEP          DISABLE
#define USE_FUNCTION            DISABLE

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static struct LCM_UTIL_FUNCS lcm_util = { 0 };

#if USE_DEEP_SLEEP
static bool panel_suspend_state = false;
static bool panel_deep_sleep_state = false;
#endif

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

extern int ddp_dsi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);
extern int ddp_dsi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle);

/* --------------------------------------------------------------------------- */
/* GPIO Set */
/* --------------------------------------------------------------------------- */

//dir out : mode 00 dir in : mode 01

#define LGE_LPWG_SUPPORT

static struct LCM_setting_table_V3 lcm_initial_setting[] = {
    {0x39, 0xFF, 3, {0x98,0x81,0x03}},
    {0x15, 0x01, 1, {0x00}},
    {0x15, 0x02, 1, {0x00}},
    {0x15, 0x03, 1, {0x73}},
    {0x15, 0x04, 1, {0x00}},
    {0x15, 0x05, 1, {0x00}},
    {0x15, 0x06, 1, {0x08}},
    {0x15, 0x07, 1, {0x00}},
    {0x15, 0x08, 1, {0x00}},
    {0x15, 0x09, 1, {0x00}},
    {0x15, 0x0a, 1, {0x00}},
    {0x15, 0x0b, 1, {0x00}},
    {0x15, 0x0c, 1, {0x0B}},
    {0x15, 0x0d, 1, {0x00}},
    {0x15, 0x0e, 1, {0x00}},
    {0x15, 0x0f, 1, {0x26}},
    {0x15, 0x10, 1, {0x26}},
    {0x15, 0x11, 1, {0x00}},
    {0x15, 0x12, 1, {0x00}},
    {0x15, 0x13, 1, {0x02}},
    {0x15, 0x14, 1, {0x00}},
    {0x15, 0x15, 1, {0x00}},
    {0x15, 0x16, 1, {0x00}},
    {0x15, 0x17, 1, {0x00}},
    {0x15, 0x18, 1, {0x00}},
    {0x15, 0x19, 1, {0x00}},
    {0x15, 0x1a, 1, {0x00}},
    {0x15, 0x1b, 1, {0x00}},
    {0x15, 0x1c, 1, {0x00}},
    {0x15, 0x1d, 1, {0x00}},
    {0x15, 0x1e, 1, {0x40}},
    {0x15, 0x1f, 1, {0xC0}},
    {0x15, 0x20, 1, {0x06}},
    {0x15, 0x21, 1, {0x01}},
    {0x15, 0x22, 1, {0x08}},
    {0x15, 0x23, 1, {0x03}},
    {0x15, 0x24, 1, {0x8C}},
    {0x15, 0x25, 1, {0x8C}},
    {0x15, 0x26, 1, {0x00}},
    {0x15, 0x27, 1, {0x00}},
    {0x15, 0x28, 1, {0x33}},
    {0x15, 0x29, 1, {0x03}},
    {0x15, 0x2a, 1, {0x00}},
    {0x15, 0x2b, 1, {0x00}},
    {0x15, 0x2c, 1, {0x01}},
    {0x15, 0x2d, 1, {0x01}},
    {0x15, 0x2e, 1, {0x00}},
    {0x15, 0x2f, 1, {0x00}},
    {0x15, 0x30, 1, {0x00}},
    {0x15, 0x31, 1, {0x00}},
    {0x15, 0x32, 1, {0x31}},
    {0x15, 0x33, 1, {0x00}},
    {0x15, 0x34, 1, {0x23}},
    {0x15, 0x35, 1, {0x00}},
    {0x15, 0x36, 1, {0x03}},
    {0x15, 0x37, 1, {0x00}},
    {0x15, 0x38, 1, {0x00}},
    {0x15, 0x39, 1, {0x00}},
    {0x15, 0x3A, 1, {0x00}},
    {0x15, 0x3B, 1, {0x00}},
    {0x15, 0x3C, 1, {0x00}},
    {0x15, 0x3D, 1, {0x00}},
    {0x15, 0x3E, 1, {0x00}},
    {0x15, 0x3F, 1, {0x00}},
    {0x15, 0x40, 1, {0x00}},
    {0x15, 0x41, 1, {0x00}},
    {0x15, 0x42, 1, {0x00}},
    {0x15, 0x43, 1, {0x08}},
    {0x15, 0x44, 1, {0x00}},
    {0x15, 0x50, 1, {0x01}},
    {0x15, 0x51, 1, {0x23}},
    {0x15, 0x52, 1, {0x45}},
    {0x15, 0x53, 1, {0x67}},
    {0x15, 0x54, 1, {0x89}},
    {0x15, 0x55, 1, {0xab}},
    {0x15, 0x56, 1, {0x01}},
    {0x15, 0x57, 1, {0x23}},
    {0x15, 0x58, 1, {0x45}},
    {0x15, 0x59, 1, {0x67}},
    {0x15, 0x5a, 1, {0x89}},
    {0x15, 0x5b, 1, {0xab}},
    {0x15, 0x5c, 1, {0xcd}},
    {0x15, 0x5d, 1, {0xef}},
    {0x15, 0x5e, 1, {0x11}},
    {0x15, 0x5f, 1, {0x17}},
    {0x15, 0x60, 1, {0x00}},
    {0x15, 0x61, 1, {0x07}},
    {0x15, 0x62, 1, {0x06}},
    {0x15, 0x63, 1, {0x0E}},
    {0x15, 0x64, 1, {0x0F}},
    {0x15, 0x65, 1, {0x0C}},
    {0x15, 0x66, 1, {0x0D}},
    {0x15, 0x67, 1, {0x02}},
    {0x15, 0x68, 1, {0x02}},
    {0x15, 0x69, 1, {0x02}},
    {0x15, 0x6a, 1, {0x02}},
    {0x15, 0x6b, 1, {0x02}},
    {0x15, 0x6c, 1, {0x02}},
    {0x15, 0x6d, 1, {0x02}},
    {0x15, 0x6e, 1, {0x02}},
    {0x15, 0x6f, 1, {0x02}},
    {0x15, 0x70, 1, {0x02}},
    {0x15, 0x71, 1, {0x02}},
    {0x15, 0x72, 1, {0x02}},
    {0x15, 0x73, 1, {0x16}},
    {0x15, 0x74, 1, {0x01}},
    {0x15, 0x75, 1, {0x17}},
    {0x15, 0x76, 1, {0x00}},
    {0x15, 0x77, 1, {0x07}},
    {0x15, 0x78, 1, {0x06}},
    {0x15, 0x79, 1, {0x0E}},
    {0x15, 0x7a, 1, {0x0F}},
    {0x15, 0x7b, 1, {0x0C}},
    {0x15, 0x7c, 1, {0x0D}},
    {0x15, 0x7d, 1, {0x02}},
    {0x15, 0x7e, 1, {0x02}},
    {0x15, 0x7f, 1, {0x02}},
    {0x15, 0x80, 1, {0x02}},
    {0x15, 0x81, 1, {0x02}},
    {0x15, 0x82, 1, {0x02}},
    {0x15, 0x83, 1, {0x02}},
    {0x15, 0x84, 1, {0x02}},
    {0x15, 0x85, 1, {0x02}},
    {0x15, 0x86, 1, {0x02}},
    {0x15, 0x87, 1, {0x02}},
    {0x15, 0x88, 1, {0x02}},
    {0x15, 0x89, 1, {0x16}},
    {0x15, 0x8A, 1, {0x01}},
    {0x39, 0xFF, 3, {0x98,0x81,0x04}},
    {0x15, 0x00, 1, {0x80}},
    {0x15, 0x6C, 1, {0x15}},
    {0x15, 0x6E, 1, {0x1A}},
    {0x15, 0x6F, 1, {0xA5}},
    {0x15, 0x8D, 1, {0x2A}},
    {0x15, 0x87, 1, {0xBA}},
    {0x15, 0xB2, 1, {0xD1}},
    {0x15, 0x26, 1, {0x76}},
    {0x15, 0x3B, 1, {0x98}},
    {0x15, 0x3A, 1, {0x12}},
    {0x15, 0x35, 1, {0x1F}},
    {0x15, 0xB5, 1, {0x27}},
    {0x15, 0x31, 1, {0x75}},
    {0x15, 0x30, 1, {0x03}},
    {0x15, 0x33, 1, {0x14}},
    {0x15, 0x38, 1, {0x01}},
    {0x15, 0x39, 1, {0x00}},
    {0x15, 0x7A, 1, {0x0F}},
    {0x39, 0xFF, 3, {0x98,0x81,0x01}},
    {0x15, 0x22, 1, {0x0A}},
    {0x15, 0x2E, 1, {0xF0}},
    {0x15, 0x31, 1, {0x00}},
    {0x15, 0x53, 1, {0x86}},
    {0x15, 0x55, 1, {0x8A}},
    {0x15, 0x50, 1, {0xE9}},
    {0x15, 0x51, 1, {0xE4}},
    {0x15, 0x60, 1, {0x14}},
    {0x15, 0x61, 1, {0x00}},
    {0x15, 0x62, 1, {0x20}},
    {0x15, 0x63, 1, {0x10}},
    {0x15, 0xA0, 1, {0x08}},
    {0x15, 0xA1, 1, {0x18}},
    {0x15, 0xA2, 1, {0x23}},
    {0x15, 0xA3, 1, {0x11}},
    {0x15, 0xA4, 1, {0x13}},
    {0x15, 0xA5, 1, {0x25}},
    {0x15, 0xA6, 1, {0x1C}},
    {0x15, 0xA7, 1, {0x1E}},
    {0x15, 0xA8, 1, {0x81}},
    {0x15, 0xA9, 1, {0x1C}},
    {0x15, 0xAA, 1, {0x27}},
    {0x15, 0xAB, 1, {0x79}},
    {0x15, 0xAC, 1, {0x1E}},
    {0x15, 0xAD, 1, {0x1F}},
    {0x15, 0xAE, 1, {0x53}},
    {0x15, 0xAF, 1, {0x25}},
    {0x15, 0xB0, 1, {0x2A}},
    {0x15, 0xB1, 1, {0x53}},
    {0x15, 0xB2, 1, {0x61}},
    {0x15, 0xB3, 1, {0x39}},
    {0x15, 0xC0, 1, {0x08}},
    {0x15, 0xC1, 1, {0x18}},
    {0x15, 0xC2, 1, {0x23}},
    {0x15, 0xC3, 1, {0x11}},
    {0x15, 0xC4, 1, {0x13}},
    {0x15, 0xC5, 1, {0x25}},
    {0x15, 0xC6, 1, {0x1C}},
    {0x15, 0xC7, 1, {0x1E}},
    {0x15, 0xC8, 1, {0x81}},
    {0x15, 0xC9, 1, {0x1C}},
    {0x15, 0xCA, 1, {0x27}},
    {0x15, 0xCB, 1, {0x79}},
    {0x15, 0xCC, 1, {0x1E}},
    {0x15, 0xCD, 1, {0x1F}},
    {0x15, 0xCE, 1, {0x53}},
    {0x15, 0xCF, 1, {0x25}},
    {0x15, 0xD0, 1, {0x2A}},
    {0x15, 0xD1, 1, {0x53}},
    {0x15, 0xD2, 1, {0x61}},
    {0x15, 0xD3, 1, {0x39}},
    {0x39, 0xFF, 3, {0x98,0x81,0x00}},
    //{0x15, 0x35, 1, {0x00}}, //TE on
};

static struct LCM_setting_table_V3 lcm_initial_suspend_cmd[] = {
    {0x39, 0xFF, 3, {0x98,0x81,0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_sleep_out[] = {
    {0x05, 0x11, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_disp_on[] = {
    {0x05, 0x29, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_sleep_in[] = {
    {0x05, 0x10, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_display_off[] = {
    {0x05, 0x28, 1, {0x00}},
};

static void init_lcm_registers_sleep_in(void)
{
    dsi_set_cmdq_V3(lcm_initial_suspend_cmd, sizeof(lcm_initial_suspend_cmd) / sizeof(struct LCM_setting_table_V3), 1);
    dsi_set_cmdq_V3(lcm_initial_display_off, sizeof(lcm_initial_display_off) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(20);

    dsi_set_cmdq_V3(lcm_initial_sleep_in, sizeof(lcm_initial_sleep_in) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(120);

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void init_lcm_registers_sleep_out(void)
{
    dsi_set_cmdq_V3(lcm_initial_setting, sizeof(lcm_initial_setting) / sizeof(struct LCM_setting_table_V3), 1);
    dsi_set_cmdq_V3(lcm_initial_sleep_out, sizeof(lcm_initial_sleep_out) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(120);

    dsi_set_cmdq_V3(lcm_initial_disp_on, sizeof(lcm_initial_disp_on) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(20);

    LCM_PRINT("[LCD] %s\n",__func__);
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
    memcpy((void *)&lcm_util, (void *)util, sizeof(struct LCM_UTIL_FUNCS));
}

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
    params->dsi.cont_clock = 0;

    params->dsi.vertical_sync_active = 4;
    params->dsi.vertical_backporch = 18;
    params->dsi.vertical_frontporch = 12;
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 16;
    params->dsi.horizontal_backporch = 64;
    params->dsi.horizontal_frontporch = 64;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 240; // mipi data rate : 480Mbps
    params->dsi.ssc_disable = 1;

    params->lcm_seq_suspend = LCM_MIPI_VIDEO_FRAME_DONE_SUSPEND;
    params->lcm_seq_shutdown = LCM_SHUTDOWN_AFTER_DSI_OFF;
    params->lcm_seq_resume = LCM_MIPI_READY_VIDEO_FRAME_RESUME;
    params->lcm_seq_power_on = NOT_USE_RESUME;

    params->esd_powerctrl_support = false;

#if defined(CONFIG_LGE_DISPLAY_ESD_RECOVERY)
    params->dsi.esd_check_enable = 1;
    params->dsi.customization_esd_check_enable = 1;

    params->dsi.lcm_esd_check_table[0].cmd  = 0x0A;
    params->dsi.lcm_esd_check_table[0].count  = 1;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

    set_disp_esd_check_lcm(true);
#endif

    LCM_PRINT("[LCD] %s\n",__func__);
}

#if USE_DEEP_SLEEP
static void set_panel_suspend_state(bool mode)
{
    panel_suspend_state = mode;
}

static void set_panel_deep_sleep_state(bool mode)
{
    panel_deep_sleep_state = mode;
}

static bool get_panel_suspend_state(void)
{
    return panel_suspend_state;
}

static bool get_panel_deep_sleep_state(void)
{
    return panel_deep_sleep_state;
}
#endif

static void lcm_dsv_ctrl(unsigned int enable, unsigned int delay)
{
#ifdef CONFIG_MT6370_PMU_DSV
    int vpos = 0;
    int vneg = 0;

    if(get_display_bias_dsv_voltage() != DSV_VOLTAGE)
      set_display_bias_dsv_voltage(DSV_VOLTAGE);

    vpos = get_display_bias_dsv_voltage();
    vneg = vpos;

    if(enable)
      display_bias_enable();
    else
      display_bias_disable();
#if 0
    vpos = mt6370_dsv_set_property(DB_VPOS_GET_VOLTAGE, enable);
    vneg = mt6370_dsv_set_property(DB_VNEG_GET_VOLTAGE, enable);

    if(enable)
        mt6370_dsv_set_property(DB_VPOS_EN, enable);
    else
        mt6370_dsv_set_property(DB_VNEG_EN, enable);

    if(delay)
        MDELAY(delay);

    if(enable)
        mt6370_dsv_set_property(DB_VNEG_EN, enable);
    else
        mt6370_dsv_set_property(DB_VPOS_EN, enable);
#endif

    LCM_PRINT("[LCD] %s : vpos = %d, vneg = %d\n",__func__,vpos,vneg);
#else
    if(!enable)
        lcd_bias_power_off_vspn();

    if(enable)
        lcd_bias_set_gpio_ctrl(DSV_VPOS_EN, enable);
    else
        lcd_bias_set_gpio_ctrl(DSV_VNEG_EN, enable);

    if(delay)
        MDELAY(delay);

    if(enable)
        lcd_bias_set_gpio_ctrl(DSV_VNEG_EN, enable);
    else
        lcd_bias_set_gpio_ctrl(DSV_VPOS_EN, enable);

    if(enable)
        lcd_bias_set_vspn(DSV_VOLTAGE);
#endif

    LCM_PRINT("[LCD] %s : %s\n",__func__,(enable)? "ON":"OFF");
}

static void lcd_reset_pin(unsigned int mode)
{
    lcd_bias_set_gpio_ctrl(LCM_RST, mode);

    LCM_PRINT("[LCD] LCD Reset %s \n",(mode)? "High":"Low");
}

#if USE_FUNCTION
static void tp_ldo_1v8_ctrl(unsigned int enable)
{
    lcd_bias_set_gpio_ctrl(LCD_LDO_EN, enable);

    LCM_PRINT("[LCD] %s : %s\n",__func__,(enable)? "ON":"OFF");
}

static void touch_reset_pin(unsigned int mode)
{
    lcd_bias_set_gpio_ctrl(TOUCH_RST, mode);

    LCM_PRINT("[LCD] Touch Reset %s \n",(mode)? "High":"Low");
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
#endif

static void lcm_suspend(void)
{
    init_lcm_registers_sleep_in();

    lcd_reset_pin(DISABLE);
    MDELAY(10);

    lcm_dsv_ctrl(DISABLE, 1);

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_suspend_mfts(void)
{
    LCM_PRINT("[LCD] %s : do nothing\n",__func__);
}

static void lcm_resume(void)
{
    lcm_dsv_ctrl(ENABLE, 1);
    MDELAY(15);

    lcd_reset_pin(ENABLE);
    MDELAY(5);

    lcd_reset_pin(DISABLE);
    MDELAY(10);

    lcd_reset_pin(ENABLE);
    MDELAY(20);

    if(get_esd_recovery_state())
      MDELAY(100);

    init_lcm_registers_sleep_out();

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_resume_mfts(void)
{
    LCM_PRINT("[LCD] %s : do nothing\n",__func__);
}

#if USE_DEEP_SLEEP
static void lcm_dsi_ctrl(unsigned int enable)
{
    if(enable) {
        ddp_path_top_clock_on();
        ddp_dsi_power_on(DISP_MODULE_DSI0, NULL);
    } else {
        ddp_dsi_power_off(DISP_MODULE_DSI0, NULL);
        ddp_path_top_clock_off();
    }

    LCM_PRINT("[LCD] %s : %s\n",__func__,(enable)? "on":"off");
}

static void lcm_enter_deep_sleep(void)
{
    if(get_panel_suspend_state()) {
        set_panel_deep_sleep_state(true);

        LCM_PRINT("[LCD] %s\n", __func__);
    }
}

static void lcm_exit_deep_sleep(void)
{
    if(get_panel_deep_sleep_state()) {
        set_panel_deep_sleep_state(false);

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

    LCM_PRINT("[LCD] %s : %d \n", __func__, mode);
}
#endif

static void lcm_shutdown(void)
{
    LCM_PRINT("[LCD] %s : do nothing\n",__func__);
}

#if defined(CONFIG_LGE_INIT_CMD_TUNING)
static struct LCM_setting_table_V3* lcm_get_lcm_init_cmd_str(void)
{
	struct LCM_setting_table_V3 * p_str = lcm_initial_setting;
	return p_str;
}

static int lcm_get_init_cmd_str_size(void)
{
	return (sizeof(lcm_initial_setting)/sizeof(struct LCM_setting_table_V3));
}
#endif

/* --------------------------------------------------------------------------- */
/* Get LCM Driver Hooks */
/* --------------------------------------------------------------------------- */
struct LCM_DRIVER ili9881c_hdplus_dsi_vdo_txd_lcm_drv = {
    .name = "TXD-ILI9881C",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params = lcm_get_params,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
    .shutdown = lcm_shutdown,
    .suspend_mfts = lcm_suspend_mfts,
    .resume_mfts = lcm_resume_mfts,
#if USE_DEEP_SLEEP
    .set_deep_sleep = lcm_set_deep_sleep,
#endif
#if defined(CONFIG_LGE_INIT_CMD_TUNING)
	.get_lcm_init_cmd_str = lcm_get_lcm_init_cmd_str,
	.get_init_cmd_str_size = lcm_get_init_cmd_str_size,
#endif
};
