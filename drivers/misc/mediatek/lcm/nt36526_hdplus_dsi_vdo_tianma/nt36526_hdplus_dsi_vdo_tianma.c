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
#define USE_FUNCTION            DISABLE
#define USE_DEEP_SLEEP          DISABLE


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


static struct LCM_setting_table_V3 lcm_initial_command_1[] = {
	{0x15, 0xFF, 1,   {0x24}},
	// DELAY 10
};

static struct LCM_setting_table_V3 lcm_initial_command_2[] = {
	{0x39, 0x5C, 2,   {0xD5, 0x15}},
	{0x39, 0x5D, 2,   {0x0B, 0x0B}},
	{0x39, 0x5E, 3,   {0x00, 0x00, 0x00}},

	{0x15, 0xFF, 1,   {0x2A}},
	// DELAY 10
};

static struct LCM_setting_table_V3 lcm_initial_command_3[] = {
	{0x15, 0xFB, 1,   {0x01}},
	{0x15, 0xAF, 1,   {0x73}},

	{0x39, 0xB0, 16,  {0x32,0x32,0x30,0x2B,0x26,0x21,0x1D,0x19,0x14,0x12,0x0E,0x0C,0x0A,0x08,0x04,0x00}},
	{0x39, 0xB1, 16,  {0x2D,0x2D,0x2B,0x26,0x22,0x1E,0x1A,0x16,0x11,0x10,0x0D,0x0B,0x09,0x07,0x03,0x00}},
	{0x39, 0xB2, 16,  {0x24,0x22,0x21,0x1F,0x1C,0x19,0x14,0x10,0x0D,0x0C,0x08,0x07,0x04,0x02,0x00,0x00}},
	{0x39, 0xB3, 16,  {0x20,0x1F,0x1E,0x1C,0x19,0x16,0x11,0x0F,0x0C,0x0B,0x07,0x06,0x03,0x01,0x00,0x00}},
	{0x39, 0xB4, 16,  {0x6B,0x5F,0x64,0x7C,0x92,0xB2,0xA0,0xA0,0xC8,0xE5,0xE5,0xFF,0xE5,0xE5,0xFF,0xFF}},
	{0x39, 0xB5, 16,  {0x73,0x6B,0x73,0x92,0xA0,0xB2,0xA0,0xC8,0xFF,0xFF,0xE5,0xFF,0xE5,0xE5,0xFF,0xFF}},
	{0x39, 0xB6, 8,   {0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44}},

	{0x15, 0xFF, 1,   {0x10}},
	// DELAY 1
};

static struct LCM_setting_table_V3 lcm_initial_command_4[] = {
	{0x15, 0xFB, 1,   {0x01}},
};

static struct LCM_setting_table_V3 lcm_exit_sleep[] = {
    {0x05, 0x11, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_disp_on[] = {
    {0x05, 0x29, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_sleep_in[] = {
    {0x05, 0x10, 1, {0x00}},
};

static struct LCM_setting_table_V3 lcm_initial_display_off[] = {
    {0x05, 0x28, 1, {0x00}},
};

/*
static struct LCM_setting_table_V3 lcm_initial_open_te[] = {
    {0x15, 0x35, 1, {0x00}},
};
*/
static void init_lcm_registers_sleep_in(void)
{
    MDELAY(5);

    dsi_set_cmdq_V3(lcm_initial_display_off, sizeof(lcm_initial_display_off) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(20);

    dsi_set_cmdq_V3(lcm_initial_sleep_in, sizeof(lcm_initial_sleep_in) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(120);

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void init_lcm_registers_sleep_out(void)
{
    dsi_set_cmdq_V3(lcm_initial_command_1, sizeof(lcm_initial_command_1) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(10);
    dsi_set_cmdq_V3(lcm_initial_command_2, sizeof(lcm_initial_command_2) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(10);
    dsi_set_cmdq_V3(lcm_initial_command_3, sizeof(lcm_initial_command_3) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(1);
    dsi_set_cmdq_V3(lcm_initial_command_4, sizeof(lcm_initial_command_4) / sizeof(struct LCM_setting_table_V3), 1);

    dsi_set_cmdq_V3(lcm_disp_on,sizeof(lcm_disp_on) / sizeof(struct LCM_setting_table_V3), 1);
    dsi_set_cmdq_V3(lcm_exit_sleep, sizeof(lcm_exit_sleep) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(140);

    LCM_PRINT("[LCD] %s\n",__func__);
}
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

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
        dfps_params[0].vertical_frontporch = 20;
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
        dfps_params[1].vertical_frontporch = 20;
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

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
    memcpy((void *)&lcm_util, (void *)util, sizeof(struct LCM_UTIL_FUNCS));
}

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
    params->dsi.vertical_sync_active                       = 2;
    params->dsi.vertical_backporch                         = 194;
    params->dsi.vertical_frontporch                        = 20;
    params->dsi.vertical_active_line       = FRAME_HEIGHT;
    params->dsi.horizontal_sync_active                     = 4;
    params->dsi.horizontal_backporch                       = 124;
    params->dsi.horizontal_frontporch                      = 124;
    params->dsi.horizontal_active_pixel    = FRAME_WIDTH;
    params->dsi.PLL_CLOCK                                  = 333;

    params->physical_width                 = PHYSICAL_WIDTH;
    params->physical_height                = PHYSICAL_HEIGHT;
#ifndef BUILD_LK
    params->density                        = LCM_DENSITY;
#endif

    // Non-continuous clock
    params->dsi.noncont_clock = TRUE;
    //params->dsi.noncont_clock_period = 2; // Unit : frames
    params->dsi.clk_lp_per_line_enable              = 1;

    params->dsi.ssc_disable = 1;//default enable SSC
    //params->dsi.ssc_range = 5;

    params->dsi.esd_check_enable                    = 0;
    params->dsi.customization_esd_check_enable      = 0;

    params->dsi.lcm_esd_check_table[0].cmd          = 0x09;
    params->dsi.lcm_esd_check_table[0].count        = 3;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x80;
    params->dsi.lcm_esd_check_table[0].para_list[1] = 0x03;
    params->dsi.lcm_esd_check_table[0].para_list[2] = 0x06;
    params->lcm_seq_suspend = LCM_MIPI_VIDEO_FRAME_DONE_SUSPEND;
    params->lcm_seq_shutdown = LCM_SHUTDOWN_AFTER_DSI_OFF;
    params->lcm_seq_resume = LCM_MIPI_READY_VIDEO_FRAME_RESUME;
    params->lcm_seq_power_on = NOT_USE_RESUME;

#if defined(CONFIG_LGE_MULTI_FRAME_RATE)
  lcm_dfps_int(&(params->dsi));
#endif

}
static void init_lcm_registers(void)
{

    dsi_set_cmdq_V3(lcm_initial_command_1, sizeof(lcm_initial_command_1) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(10);
    dsi_set_cmdq_V3(lcm_initial_command_2, sizeof(lcm_initial_command_2) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(10);
    dsi_set_cmdq_V3(lcm_initial_command_3, sizeof(lcm_initial_command_3) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(1);
    dsi_set_cmdq_V3(lcm_initial_command_4, sizeof(lcm_initial_command_4) / sizeof(struct LCM_setting_table_V3), 1);

    dsi_set_cmdq_V3(lcm_disp_on,sizeof(lcm_disp_on) / sizeof(struct LCM_setting_table_V3), 1);
    dsi_set_cmdq_V3(lcm_exit_sleep, sizeof(lcm_exit_sleep) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(140);

    LCM_PRINT("[LCD] %s\n",__func__);
}


static void lcm_dsv_ctrl(unsigned int enable, unsigned int delay)
{
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
    LCM_PRINT("[LCD] %s\n",__func__);
}


static void lcm_resume(void)
{
    lcd_reset_pin(DISABLE);
    MDELAY(3);

    lcd_reset_pin(ENABLE);
    MDELAY(10);

    lcd_reset_pin(DISABLE);
    MDELAY(5);

    lcd_reset_pin(ENABLE);
    MDELAY(10);

    init_lcm_registers_sleep_out();

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_init(void)
{
    lcm_dsv_ctrl(ENABLE, 1);
    MDELAY(5);

    lcd_reset_pin(DISABLE);
    MDELAY(3);

    lcd_reset_pin(ENABLE);
    MDELAY(10);

    lcd_reset_pin(DISABLE);
    MDELAY(5);

    lcd_reset_pin(ENABLE);
    MDELAY(10);

    init_lcm_registers();

    LCM_PRINT("[LCD] %s\n",__func__);
}


#ifdef BUILD_LK
LCM_DRIVER nt36526_hdplus_dsi_vdo_tianma_drv =
#else
struct LCM_DRIVER nt36526_hdplus_dsi_vdo_tianma_drv =
#endif
{
    .name           = "TIANMA-NT36526",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
};
