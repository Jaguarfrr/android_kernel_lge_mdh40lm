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
#include  "../../pmic/mt6370/inc/mt6370_pmu_dsv.h"
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

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static struct LCM_UTIL_FUNCS lcm_util = { 0 };

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
extern bool mfts_check_shutdown;
static bool flag_is_panel_deep_sleep = false;

extern int ddp_dsi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);
extern int ddp_dsi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle);

#define LGE_LPWG_SUPPORT

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
    dsi_set_cmdq_V3(lcm_initial_display_off, sizeof(lcm_initial_display_off) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(20);

    dsi_set_cmdq_V3(lcm_initial_sleep_in, sizeof(lcm_initial_sleep_in) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(150);

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void init_lcm_registers(void)
{
    dsi_set_cmdq_V3(lcm_initial_sleep_out, sizeof(lcm_initial_sleep_out) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(120);

    dsi_set_cmdq_V3(lcm_initial_disp_on, sizeof(lcm_initial_disp_on) / sizeof(struct LCM_setting_table_V3), 1);
    MDELAY(30);

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
        dfps_params[0].vertical_frontporch = 165;
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
        dfps_params[1].vertical_frontporch = 165;
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
    params->dsi.cont_clock = 0;

    params->dsi.vertical_sync_active = 8;
    params->dsi.vertical_backporch = 88;
    params->dsi.vertical_frontporch = 165;
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 14;
    params->dsi.horizontal_backporch = 42;
    params->dsi.horizontal_frontporch = 49;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 285; // mipi clk : 570Mhz
    params->dsi.ssc_disable = 1;

    params->dsi.HS_PRPR = 6;
    params->dsi.HS_TRAIL = 6;

#if defined(CONFIG_LGE_DISPLAY_ESD_RECOVERY)
    params->dsi.esd_check_enable = 0;
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
        sm5109_set_vspn();

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

static void lcm_suspend(void)
{
    init_lcm_registers_sleep_in();

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_suspend_mfts(void)
{
    //mfts_check_shutdown=true;

    lcm_dsv_ctrl(DISABLE, 5);
    MDELAY(5);

    lcm_reset_ctrl(DISABLE, 0);
    MDELAY(5);

    //mfts_check_shutdown=false;

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_init(void)
{
    lcm_dsv_ctrl(ENABLE, 5);
    MDELAY(5);

    lcm_reset_ctrl(ENABLE, 0);
    MDELAY(55);

    init_lcm_registers();

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_resume(void)
{
   if(flag_is_panel_deep_sleep) {
      lcm_init();
      flag_is_panel_deep_sleep = false;

      LCM_PRINT("[LCD] %s : deep sleep mode state. call lcm_init(). \n", __func__);
    } else {
      lcm_reset_ctrl(DISABLE, 5);
      MDELAY(5);

      lcm_reset_ctrl(ENABLE, 5);
      MDELAY(50);

      init_lcm_registers();

      LCM_PRINT("[LCD] %s\n",__func__);
    }
}

static void lcm_resume_mfts(void)
{
    //mfts_check_shutdown=true;

    lcm_dsv_ctrl(ENABLE, 5);
    MDELAY(3);

    lcm_reset_ctrl(ENABLE, 10);
    MDELAY(50);

    //mfts_check_shutdown=false;

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_shutdown(void)
{
    MDELAY(150);

    lcm_dsv_ctrl(DISABLE, 5);
    MDELAY(5);

    lcm_reset_ctrl(DISABLE, 0);

    flag_is_panel_deep_sleep = true;

    LCM_PRINT("[LCD] %s\n",__func__);
}

static void lcm_set_deep_sleep(unsigned int mode)
{
    switch (mode){
      case DEEP_SLEEP_ENTER:
        break;
      case DEEP_SLEEP_EXIT:
        break;
      default :
        break;
    }

    LCM_PRINT("[LCD] %s : %d \n", __func__,  mode);
}

/* --------------------------------------------------------------------------- */
/* Get LCM Driver Hooks */
/* --------------------------------------------------------------------------- */
struct LCM_DRIVER ft8006p_hdplus_dsi_vdo_auo_drv = {
    .name = "AUO-FT8006P",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params = lcm_get_params,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
    .shutdown = lcm_shutdown,
    .suspend_mfts = lcm_suspend_mfts,
    .resume_mfts = lcm_resume_mfts,
    .set_deep_sleep = lcm_set_deep_sleep,
};
