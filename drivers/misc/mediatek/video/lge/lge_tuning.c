/*
*lge_tuning.c
*
* Copyright (c) 2017 LGE.
*
* author : woonghwan.lee@lge.com and junil.cho@lge.com
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include "lge_tuning.h"
#if defined(CONFIG_LGE_DSI_PARAM_TUNING)
static void lge_set_dsi_value(uint32_t dsi_index, LGE_DISP_CONFIG mode,int value)
{
	uint32_t hbp_byte;
	struct DSI_REGS *dsi_reg[2];
	int fbconfig_dsiTmpBufBpp =0;
	struct LCM_DSI_PARAMS *dsi_params = get_dsi_params_handle(dsi_index);

	dsi_reg[0] = (struct DSI_REGS*)DISPSYS_DSI0_BASE;
	dsi_reg[1] = (struct DSI_REGS*)DISPSYS_DSI1_BASE;

	if(dsi_params->data_format.format == LCM_DSI_FORMAT_RGB565)
		fbconfig_dsiTmpBufBpp = 2;
	else
		fbconfig_dsiTmpBufBpp = 3;

	switch(mode)
	{
		case LGE_VFP:
		    if(dsi_index == 0){
			DSI_OUTREGBIT(NULL, struct DSI_VFP_NL_REG,dsi_reg[0]->DSI_VFP_NL,VFP_NL,value);
		    }
		    else if(dsi_index == 1){
			DSI_OUTREGBIT(NULL, struct DSI_VFP_NL_REG,dsi_reg[1]->DSI_VFP_NL,VFP_NL,value);
		    }
		    else{
			DSI_OUTREGBIT(NULL, struct DSI_VFP_NL_REG,dsi_reg[0]->DSI_VFP_NL,VFP_NL,value);
			DSI_OUTREGBIT(NULL, struct DSI_VFP_NL_REG,dsi_reg[1]->DSI_VFP_NL,VFP_NL,value);
		    }
		    break;
		case LGE_VBP:
		    if(dsi_index == 0){
			 DSI_OUTREGBIT(NULL, struct DSI_VBP_NL_REG,dsi_reg[0]->DSI_VBP_NL,VBP_NL,value);
		    }
		    else if(dsi_index == 1){
			DSI_OUTREGBIT(NULL, struct DSI_VBP_NL_REG,dsi_reg[1]->DSI_VBP_NL,VBP_NL,value);
		    }
		    else{
			DSI_OUTREGBIT(NULL, struct DSI_VBP_NL_REG,dsi_reg[0]->DSI_VBP_NL,VBP_NL,value);
			DSI_OUTREGBIT(NULL, struct DSI_VBP_NL_REG,dsi_reg[1]->DSI_VBP_NL,VBP_NL,value);
		    }
		    break;
		case LGE_VSA:
		    if(dsi_index == 0){
			DSI_OUTREGBIT(NULL, struct DSI_VSA_NL_REG,dsi_reg[0]->DSI_VSA_NL,VSA_NL,value);
		    }
		    else if(dsi_index == 1){
			DSI_OUTREGBIT(NULL, struct DSI_VSA_NL_REG,dsi_reg[1]->DSI_VSA_NL,VSA_NL,value);
		    }
		    else{
			DSI_OUTREGBIT(NULL, struct DSI_VSA_NL_REG,dsi_reg[0]->DSI_VSA_NL,VSA_NL,value);
			DSI_OUTREGBIT(NULL, struct DSI_VSA_NL_REG,dsi_reg[1]->DSI_VSA_NL,VSA_NL,value);
		    }
		    break;
		case LGE_HSA:
			dsi_params->horizontal_sync_active = value;

		    if (!(dsi_params->mode == SYNC_EVENT_VDO_MODE || dsi_params->mode == BURST_VDO_MODE)){
			value = (value * fbconfig_dsiTmpBufBpp - 10);
		    }
		    value = ALIGN_TO((value), 4);

		    if (dsi_index == 0){
		        DSI_OUTREG32(NULL, &dsi_reg[0]->DSI_HSA_WC, value);
		    }
		    else if (dsi_index == 1){
		        DSI_OUTREG32(NULL, &dsi_reg[1]->DSI_HSA_WC, value);
		    }
		    else{
		    	DSI_OUTREG32(NULL, &dsi_reg[0]->DSI_HSA_WC, value);
		    	DSI_OUTREG32(NULL, &dsi_reg[1]->DSI_HSA_WC, value);
		    }
		    break;
		case LGE_HFP:
			dsi_params->horizontal_frontporch = value;
		    value = value * fbconfig_dsiTmpBufBpp - 12;
		    value = ALIGN_TO(value, 4);

		    if (dsi_index == 0){
		        DSI_OUTREGBIT(NULL, struct DSI_HFP_WC_REG, dsi_reg[0]->DSI_HFP_WC, HFP_WC, value);
		    }
		    else if (dsi_index == 1){
		        DSI_OUTREGBIT(NULL, struct DSI_HFP_WC_REG, dsi_reg[1]->DSI_HFP_WC, HFP_WC,value);
		    }
		    else{
		        DSI_OUTREGBIT(NULL, struct DSI_HFP_WC_REG, dsi_reg[0]->DSI_HFP_WC, HFP_WC, value);
				DSI_OUTREGBIT(NULL, struct DSI_HFP_WC_REG, dsi_reg[1]->DSI_HFP_WC, HFP_WC, value);
		    }
		    break;
		case LGE_HBP:
			dsi_params->horizontal_backporch = value;

		    if (dsi_params->mode == SYNC_EVENT_VDO_MODE || dsi_params->mode == BURST_VDO_MODE){
		        hbp_byte = ((value + dsi_params->horizontal_sync_active)*fbconfig_dsiTmpBufBpp-10);
		    }
		    else{
                        hbp_byte = value * fbconfig_dsiTmpBufBpp - 10;
		    }
                    if (dsi_index == 0){
                        DSI_OUTREG32(NULL, &dsi_reg[0]->DSI_HBP_WC, ALIGN_TO((hbp_byte),4));
		    }
		    else if (dsi_index == 1){
		    	DSI_OUTREG32(NULL, &dsi_reg[1]->DSI_HBP_WC, ALIGN_TO((hbp_byte),4));
		    }
		    else{
	           	DSI_OUTREG32(NULL, &dsi_reg[0]->DSI_HBP_WC, ALIGN_TO((hbp_byte),4));
		   	DSI_OUTREG32(NULL, &dsi_reg[1]->DSI_HBP_WC, ALIGN_TO((hbp_byte),4));
		    }
		    break;
		case LGE_SSC:
			if(value != 0)
				dsi_params->ssc_disable = 0;
			else
				dsi_params->ssc_disable = 1;

		    DSI_ssc_enable(dsi_index,value);
		    break;
		default :
		    break;
		}


}
static uint32_t lge_get_dsi_value(uint32_t dsi_index, LGE_DISP_CONFIG mode)
{
	uint32_t dsi_val;
	struct DSI_REGS *dsi_reg;
	struct LCM_DSI_PARAMS *dsi_params = get_dsi_params_handle(dsi_index);

	struct DSI_VFP_NL_REG tmp_vfp;
	struct DSI_VBP_NL_REG tmp_vbp;
	struct DSI_VSA_NL_REG tmp_vsa;

	if(dsi_index == 0 || dsi_index == 2){
		dsi_reg=(struct DSI_REGS*)DISPSYS_DSI0_BASE;
		if((struct DSI_REGS*)DISPSYS_DSI0_BASE == NULL){
		    printk("DISPSYS_DSI0_BASE value is NULL\n");
		    return 0;
		}
	} else{
		dsi_reg=(struct DSI_REGS*)DISPSYS_DSI1_BASE;
		if((struct DSI_REGS*)DISPSYS_DSI1_BASE == NULL){
		    printk("DISPSYS_DSI1_BASE value is NULL\n");
		    return 0;
		}
	}

	tmp_vfp.VFP_NL = 0;
	tmp_vbp.VBP_NL = 0;
	tmp_vsa.VSA_NL = 0;

	switch(mode){
		case LGE_VFP:
			DSI_READREG32((struct DSI_VFP_NL_REG*),&tmp_vfp,&dsi_reg->DSI_VFP_NL);
			dsi_val = tmp_vfp.VFP_NL;
			break;
		case LGE_VBP:
			DSI_READREG32((struct DSI_VBP_NL_REG*),&tmp_vbp,&dsi_reg->DSI_VBP_NL);
			dsi_val = tmp_vbp.VBP_NL;
			break;
		case LGE_VSA:
			DSI_READREG32((struct DSI_VSA_NL_REG*),&tmp_vsa,&dsi_reg->DSI_VSA_NL);
			dsi_val = tmp_vsa.VSA_NL;
			break;
		case LGE_HFP:
			dsi_val = dsi_params->horizontal_frontporch;
			break;
		case LGE_HBP:
			dsi_val = dsi_params->horizontal_backporch;
			break;
		case LGE_HSA:
			dsi_val = dsi_params->horizontal_sync_active;
			break;
		case LGE_SSC:
			if(dsi_params->ssc_disable == 1)
				dsi_val = 0;
			else
				dsi_val = 1;
			break;
		default:
			return 0;
	}
	return dsi_val;
}

//for pllclk
static void _lge_path_lock(const char *caller)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
	disp_sw_mutex_lock(&(lge_pgc->lock));
	lge_pgc->mutex_locker = (char *)caller;
}

static void _lge_path_unlock(const char *caller)
{
	lge_pgc->mutex_locker = NULL;
	disp_sw_mutex_unlock(&(lge_pgc->lock));
	dprec_logger_done(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
}

static int lge_mtk_display_config_dsi(LGE_DISP_CONFIG mode, uint32_t config_value)
{
	int ret = 0;
	struct disp_ddp_path_config *data_config;
	data_config = dpmgr_path_get_last_config(lge_pgc->dpmgr_handle);

	/* modify below for config dsi*/
	printk("[LGE_DISP_CONFIG]display_set_config_lge start: %d :new_value: %d\n",mode , config_value);

	switch(mode){
	    case LGE_PLLCLK:
	        data_config->dispif_config.dsi.PLL_CLOCK = config_value;
	        break;
	    default:
		printk("[LGE_DISP_CONFIG_jun]lge_disp value error plz, input 0~7 input\n");
	}

	printk("[LGE_DISP_CONFIG]LG_disp_config dsi: will Run path_config()\n");
	ret = dpmgr_path_config(lge_pgc->dpmgr_handle, data_config, NULL);
	return ret;
}
int lge_mtk_dsi_config_entry(LGE_DISP_CONFIG mode, void *config_value)
{
	int ret = 0;
	int force_trigger_path = 0;
	uint32_t *config_dsi = (uint32_t *)config_value;
	struct LCM_PARAMS *lcm_param = NULL;

	DISPFUNC();

	_lge_path_lock(__func__);

	lcm_param = disp_lcm_get_params(lge_pgc->plcm);

	if (lge_pgc->state == DISP_SLEPT) {
		printk("[LGE_DISP_CONFIG]primary display path is slept??\n");
		goto done;
	}
	/* / the following code is to */
	/* / 0: lock path */
	/* / 1: stop path */
	/* / 2: do esd check (!!!) */
	/* / 3: start path */
	/* / 4: unlock path */
	/* / 1: stop path */
	_cmdq_stop_trigger_loop();

	if (dpmgr_path_is_busy(lge_pgc->dpmgr_handle)) {
		int event_ret;

		printk("[LGE_DISP_CONFIG][ESD]wait frame done ret:%d\n", ret);
		event_ret = dpmgr_wait_event_timeout(lge_pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		if (event_ret <= 0) {
			printk("[LGE_DISP_CONFIG]wait frame done in suspend timeout\n");
			primary_display_diagnose();
			ret = -1;
		}
	}
	dpmgr_path_stop(lge_pgc->dpmgr_handle, CMDQ_DISABLE);
	printk("[LGE_DISP_CONFIG][ESD]stop dpmgr path[end]\n");

	if (dpmgr_path_is_busy(lge_pgc->dpmgr_handle))
		printk("[LGE_DISP_CONFIG][ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_reset(lge_pgc->dpmgr_handle, CMDQ_DISABLE);
	if(mode == LGE_PLLCLK)
		lge_mtk_display_config_dsi(mode, *config_dsi);

	dpmgr_path_start(lge_pgc->dpmgr_handle, CMDQ_DISABLE);
	if (primary_display_is_video_mode()) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(lge_pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
		force_trigger_path = 0;
	}
	_cmdq_start_trigger_loop();

	/* when we stop trigger loop
	 * if no other thread is running, cmdq may disable its clock
	 * all cmdq event will be cleared after suspend */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);

	printk("[LGE_DISP_CONFIG]start cmdq trigger loop\n");
done:
	_lge_path_unlock(__func__);

	if (force_trigger_path) {
		primary_display_trigger(0, NULL, 0);
		printk("[LGE_DISP_CONFIG]force trigger display path\r\n");
	}
	return ret;
}
int lge_mtk_set_disp_config(unsigned int new_value, LGE_DISP_CONFIG mode)
{
	struct LCM_PARAMS *lcm_param = NULL;
	int cur_value;

	printk("[LGE_DISP_CONFIG]start\n");
	if(lge_pgc->plcm){
		lcm_param = disp_lcm_get_params(lge_pgc->plcm);
		if(lcm_param == NULL){
			printk("[LGE_DISP_CONFIG]lcm param is NULL\n");
			return 0;
		}
	}
	else{
		printk("[LGE_DISP_CONFIG]can't get lcm patam for mipi debug\n");
		return 0;
	}
	switch(mode){
		case LGE_PLLCLK:
			cur_value = lcm_param->dsi.PLL_CLOCK;
			break;
		default:
			printk("[LGE_DISP_CONFIG]invalid input \n");
			return 0;
	}
	if (new_value != cur_value){
		lge_mtk_dsi_config_entry(mode, &new_value);
		printk("[LGE_DISP_CONFIG]change [%d] value setting : old (%d) -> new (%d)\n", mode
				,cur_value, new_value);

		cur_value = new_value;
	}
	else{
		printk("[LGE_DISP_CONFIG]skip new dsi setting : %s\n", new_value?"same value":"minus value");
		return 0;
	}

	return 1;

}
struct LCM_PARAMS* lge_display_get_lcm_params(void)
{
    struct LCM_PARAMS *lcm_param = NULL;
    lge_pgc = (struct display_primary_path_context*)primary_get_pgc();
    if (lge_pgc->plcm)
	lcm_param = disp_lcm_get_params(lge_pgc->plcm);
    else
        printk("can't get lcm param for mipi debug\n");

    return lcm_param;
}
#endif

//sysfs start
#if defined(CONFIG_LGE_DSI_PARAM_TUNING)
static ssize_t lge_disp_config_show(struct kobject *kobj, struct
		kobj_attribute *attr, char *buf)
{
	ssize_t ret =0;
	struct LCM_PARAMS *lcm_param = lge_display_get_lcm_params();
	init_mipi_total_setting_value(0);

	if(lcm_param != NULL){
		ret=sprintf(buf," 0.PLLCLOCK : %d MHz\n 1.SSC_EN : %d\n 2.VFP : %d\n 3.VBP : %d\n 4.VSA : %d\n 5.HFP : %d\n 6.HBP : %d\n 7.HSA : %d\n 8.ESD_ON : %d\n",\
				lcm_param-> dsi.PLL_CLOCK,\
				dsi_total_val.ssc_en,\
				dsi_total_val.vfp,\
				dsi_total_val.vbp,\
				dsi_total_val.vsa,\
				dsi_total_val.hfp,\
				dsi_total_val.hbp,\
				dsi_total_val.hsa,\
				esd_on_flag);
	}

	return ret;
}
static ssize_t lge_disp_config_store(struct kobject *kobj, struct
		kobj_attribute *attr, const char *buf, size_t count){

	int new_value, mode;
	LGE_DISP_CONFIG disp_mode = LGE_PLLCLK;
	struct LCM_PARAMS *lcm_param = lge_display_get_lcm_params();

	sscanf(buf, "%d %d\n", &mode, &new_value);
	if(new_value <= 0)
		new_value = 0;

	primary_display_esd_check_enable(0);
	if(lcm_param != NULL){
		switch(mode){
		case 0: //PLL_CLOCK
			disp_mode = LGE_PLLCLK;
			lge_mtk_set_disp_config(new_value, disp_mode);
			lcm_param->dsi.PLL_CLOCK = new_value;
			goto PLL;
			break;
		case 1 : //ssc_en
			disp_mode = LGE_SSC;
			break;
		case 2://VFP
			disp_mode = LGE_VFP;
			break;
		case 3://VBP
			disp_mode = LGE_VBP;
			break;
		case 4://VSA
			disp_mode = LGE_VSA;
			break;
		case 5://HFP
			disp_mode = LGE_HFP;
			break;
		case 6://HBP
			disp_mode = LGE_HBP;
			break;
		case 7://HSA
			disp_mode = LGE_HSA;
			break;
		case 8://ESD_ON_OFF
			disp_mode = LGE_ESD_ON_OFF;
			esd_on_flag = new_value;
			primary_display_esd_check_enable(esd_on_flag);
			return count;
			break;
		default:
			break;
		}
		lge_set_dsi_value(0,disp_mode,new_value);
PLL:
		LG_ESD_recovery();
		return count;
	}
	return 0;
}
void init_mipi_total_setting_value(int pm_dsi_mode){

	dsi_total_val.hs_prpr = PanelMaster_get_dsi_timing(pm_dsi_mode,HS_PRPR);
	dsi_total_val.hs_zero = PanelMaster_get_dsi_timing(pm_dsi_mode,HS_ZERO);
	dsi_total_val.hs_trail = PanelMaster_get_dsi_timing(pm_dsi_mode,HS_TRAIL);
	dsi_total_val.ta_go = PanelMaster_get_dsi_timing(pm_dsi_mode,TA_GO);
	dsi_total_val.ta_sure = PanelMaster_get_dsi_timing(pm_dsi_mode,TA_SURE);
	dsi_total_val.ta_get = PanelMaster_get_dsi_timing(pm_dsi_mode,TA_GET);
	dsi_total_val.da_hs_exit = PanelMaster_get_dsi_timing(pm_dsi_mode,DA_HS_EXIT);
	dsi_total_val.clk_zero = PanelMaster_get_dsi_timing(pm_dsi_mode,CLK_ZERO);
	dsi_total_val.clk_trail = PanelMaster_get_dsi_timing(pm_dsi_mode,CLK_TRAIL);
	dsi_total_val.cont_det = PanelMaster_get_dsi_timing(pm_dsi_mode,CONT_DET);
	dsi_total_val.clk_hs_prpr = PanelMaster_get_dsi_timing(pm_dsi_mode,CLK_HS_PRPR);
	dsi_total_val.clk_hs_post = PanelMaster_get_dsi_timing(pm_dsi_mode,CLK_HS_POST);
	dsi_total_val.clk_hs_exit = PanelMaster_get_dsi_timing(pm_dsi_mode,CLK_HS_EXIT);
	dsi_total_val.lpx = PanelMaster_get_dsi_timing(pm_dsi_mode,LPX);

	dsi_total_val.hfp = lge_get_dsi_value(pm_dsi_mode,LGE_HFP);
	dsi_total_val.hbp = lge_get_dsi_value(pm_dsi_mode,LGE_HBP);
	dsi_total_val.hsa = lge_get_dsi_value(pm_dsi_mode,LGE_HSA);
	dsi_total_val.vfp = lge_get_dsi_value(pm_dsi_mode,LGE_VFP);
	dsi_total_val.vbp = lge_get_dsi_value(pm_dsi_mode,LGE_VBP);
	dsi_total_val.vsa = lge_get_dsi_value(pm_dsi_mode,LGE_VSA);
	dsi_total_val.ssc_en = lge_get_dsi_value(pm_dsi_mode,LGE_SSC);
}
static ssize_t lge_disp_config_add_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t ret= 0;
	init_mipi_total_setting_value(0);

	ret = sprintf(buf," 0.HS_PRPR = %d\n 1.HS_ZERO = %d\n 2.HS_TRAIL = %d\n 3.TA_GO = %d\n\
 4.TA_SURE = %d\n 5.TA_GET = %d\n 6.DA_HS_EXIT = %d\n 7.CLK_ZERO = %d\n 8.CLK_TRAIL = %d\n\
 9.CONT_DET = %d\n 10.CLK_HS_PRPR = %d\n 11.CLK_HS_POST = %d\n 12.CLK_HS_EXIT = %d\n",\
			dsi_total_val.hs_prpr, dsi_total_val.hs_zero,dsi_total_val.hs_trail,\
			dsi_total_val.ta_go,\
			dsi_total_val.ta_sure,dsi_total_val.ta_get,dsi_total_val.da_hs_exit,\
			dsi_total_val.clk_zero,dsi_total_val.clk_trail,\
			dsi_total_val.cont_det,dsi_total_val.clk_hs_prpr,dsi_total_val.clk_hs_post,\
			dsi_total_val.clk_hs_exit);
	return ret;
}
static ssize_t lge_disp_config_add_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){

	struct MIPI_TIMING timing;
	int type, value;
	sscanf(buf, "%d %d\n", &type, &value);
	if(type >= 12)
		timing.type = 12;
	else if(type < 0)
		timing.type = 0;

	timing.type = type;
	timing.value= value;
	PanelMaster_DSI_set_timing(0,timing);
	return count;
}
#endif
//CONFIG_LGE_DSI_PARAM_TUNING end

#if defined(CONFIG_LGE_INIT_CMD_TUNING)
static ssize_t mtkfb_lge_init_cmd_find_store(struct kobject *kobj, struct
		kobj_attribute *attr, const char *buf, size_t count){

	char *pvalue = NULL;

	if(!count)
		return -EINVAL;

	init_cmd = simple_strtoul(buf, &pvalue, 10);

	return count;
}

static ssize_t mtkfb_lge_init_cmd_find_show(struct kobject *kobj, struct
		kobj_attribute *attr, char *buf){

	struct LCM_setting_table_V3 * init_cmd_str = NULL;
	struct LCM_DRIVER *lcm_drv = NULL;
	int i=0,j=0;
	int str_size = 0;
	size_t ret = 0;
	char cmd[128] = {0, };

	lge_pgc = (struct display_primary_path_context*)primary_get_pgc();

	if(!lge_pgc)
		return -EINVAL;
	else{
		if(lge_pgc->plcm->drv)
			lcm_drv = lge_pgc->plcm->drv;
	}

	if (lcm_drv->get_lcm_init_cmd_str)
			init_cmd_str = lcm_drv->get_lcm_init_cmd_str();

	if (lcm_drv->get_init_cmd_str_size)
			str_size = lcm_drv->get_init_cmd_str_size();

	if(init_cmd_str == NULL || str_size == 0)
		return -EINVAL;

	for( i=0; i < str_size ; i++ ){
		printk("[LGE_TUNING] init_cmd_str[i].cmd = %d\n", init_cmd_str[i].cmd);
		if( init_cmd_str[i].cmd == init_cmd ){
			reg_num_idx = i;
			break;
		}
	}

	if( str_size == i ){
		ret = sprintf(buf,"There isn't the cmd in init command structure\n",buf);
		return 0;
	}
	else{
		printk("[LGE_TUNING] reg_num_idx = %d\n", reg_num_idx);

		for( j=0; j < init_cmd_str[reg_num_idx].count; j++ ){
			cmd[j] = init_cmd_str[reg_num_idx].para_list[j];
			sprintf(buf, "%s %02x", buf, cmd[j]);
		}

		ret = sprintf(buf,"%s \n",buf);
		return ret;
	}
	return ret;
}

static ssize_t mtkfb_lge_init_cmd_edit_store(struct kobject *kobj, struct
		kobj_attribute *attr, const char *buf, size_t count){

	struct LCM_setting_table_V3 * init_cmd_str = NULL;
	struct LCM_DRIVER *lcm_drv = NULL;

	lge_pgc = (struct display_primary_path_context*)primary_get_pgc();

	if(!lge_pgc)
		return -EINVAL;
	else{
		if(lge_pgc->plcm->drv)
			lcm_drv = lge_pgc->plcm->drv;
	}

	if (lcm_drv->get_lcm_init_cmd_str)
			init_cmd_str = lcm_drv->get_lcm_init_cmd_str();

	if(!count || reg_num_idx == -1 || init_cmd_str == NULL)
		return -EINVAL;

	sscanf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",\
		     &init_cmd_str[reg_num_idx].para_list[0], &init_cmd_str[reg_num_idx].para_list[1],\
		     &init_cmd_str[reg_num_idx].para_list[2], &init_cmd_str[reg_num_idx].para_list[3],\
		     &init_cmd_str[reg_num_idx].para_list[4], &init_cmd_str[reg_num_idx].para_list[5],\
		     &init_cmd_str[reg_num_idx].para_list[6], &init_cmd_str[reg_num_idx].para_list[7],\
		     &init_cmd_str[reg_num_idx].para_list[8], &init_cmd_str[reg_num_idx].para_list[9],\
		     &init_cmd_str[reg_num_idx].para_list[10], &init_cmd_str[reg_num_idx].para_list[11],\
		     &init_cmd_str[reg_num_idx].para_list[12], &init_cmd_str[reg_num_idx].para_list[13],\
		     &init_cmd_str[reg_num_idx].para_list[14], &init_cmd_str[reg_num_idx].para_list[15],\
		     &init_cmd_str[reg_num_idx].para_list[16], &init_cmd_str[reg_num_idx].para_list[17],\
		     &init_cmd_str[reg_num_idx].para_list[18], &init_cmd_str[reg_num_idx].para_list[19],\
		     &init_cmd_str[reg_num_idx].para_list[20], &init_cmd_str[reg_num_idx].para_list[21],\
		     &init_cmd_str[reg_num_idx].para_list[22], &init_cmd_str[reg_num_idx].para_list[23],\
		     &init_cmd_str[reg_num_idx].para_list[24], &init_cmd_str[reg_num_idx].para_list[25],\
		     &init_cmd_str[reg_num_idx].para_list[26], &init_cmd_str[reg_num_idx].para_list[27],\
		     &init_cmd_str[reg_num_idx].para_list[28], &init_cmd_str[reg_num_idx].para_list[29],\
		     &init_cmd_str[reg_num_idx].para_list[30], &init_cmd_str[reg_num_idx].para_list[31],\
		     &init_cmd_str[reg_num_idx].para_list[32], &init_cmd_str[reg_num_idx].para_list[33],\
		     &init_cmd_str[reg_num_idx].para_list[34], &init_cmd_str[reg_num_idx].para_list[35],\
		     &init_cmd_str[reg_num_idx].para_list[36], &init_cmd_str[reg_num_idx].para_list[37],\
		     &init_cmd_str[reg_num_idx].para_list[38], &init_cmd_str[reg_num_idx].para_list[39],\
		     &init_cmd_str[reg_num_idx].para_list[40], &init_cmd_str[reg_num_idx].para_list[41],\
		     &init_cmd_str[reg_num_idx].para_list[42], &init_cmd_str[reg_num_idx].para_list[43],\
		     &init_cmd_str[reg_num_idx].para_list[44], &init_cmd_str[reg_num_idx].para_list[45],\
		     &init_cmd_str[reg_num_idx].para_list[46], &init_cmd_str[reg_num_idx].para_list[47],\
		     &init_cmd_str[reg_num_idx].para_list[48], &init_cmd_str[reg_num_idx].para_list[49],\
		     &init_cmd_str[reg_num_idx].para_list[50], &init_cmd_str[reg_num_idx].para_list[51],\
		     &init_cmd_str[reg_num_idx].para_list[52], &init_cmd_str[reg_num_idx].para_list[53],\
		     &init_cmd_str[reg_num_idx].para_list[54], &init_cmd_str[reg_num_idx].para_list[55],\
		     &init_cmd_str[reg_num_idx].para_list[56], &init_cmd_str[reg_num_idx].para_list[57],\
		     &init_cmd_str[reg_num_idx].para_list[58], &init_cmd_str[reg_num_idx].para_list[59],\
		     &init_cmd_str[reg_num_idx].para_list[60], &init_cmd_str[reg_num_idx].para_list[61],\
		     &init_cmd_str[reg_num_idx].para_list[62], &init_cmd_str[reg_num_idx].para_list[63],\
		     &init_cmd_str[reg_num_idx].para_list[64], &init_cmd_str[reg_num_idx].para_list[65],\
		     &init_cmd_str[reg_num_idx].para_list[66], &init_cmd_str[reg_num_idx].para_list[67],\
		     &init_cmd_str[reg_num_idx].para_list[68], &init_cmd_str[reg_num_idx].para_list[69],\
		     &init_cmd_str[reg_num_idx].para_list[70], &init_cmd_str[reg_num_idx].para_list[71],\
		     &init_cmd_str[reg_num_idx].para_list[72], &init_cmd_str[reg_num_idx].para_list[73],\
		     &init_cmd_str[reg_num_idx].para_list[74], &init_cmd_str[reg_num_idx].para_list[75],\
		     &init_cmd_str[reg_num_idx].para_list[76], &init_cmd_str[reg_num_idx].para_list[77],\
		     &init_cmd_str[reg_num_idx].para_list[78], &init_cmd_str[reg_num_idx].para_list[79],\
		     &init_cmd_str[reg_num_idx].para_list[80], &init_cmd_str[reg_num_idx].para_list[81],\
		     &init_cmd_str[reg_num_idx].para_list[82], &init_cmd_str[reg_num_idx].para_list[83],\
		     &init_cmd_str[reg_num_idx].para_list[84], &init_cmd_str[reg_num_idx].para_list[85],\
		     &init_cmd_str[reg_num_idx].para_list[86], &init_cmd_str[reg_num_idx].para_list[87],\
		     &init_cmd_str[reg_num_idx].para_list[88], &init_cmd_str[reg_num_idx].para_list[89],\
		     &init_cmd_str[reg_num_idx].para_list[90], &init_cmd_str[reg_num_idx].para_list[91],\
		     &init_cmd_str[reg_num_idx].para_list[92], &init_cmd_str[reg_num_idx].para_list[93],\
		     &init_cmd_str[reg_num_idx].para_list[94], &init_cmd_str[reg_num_idx].para_list[95],\
		     &init_cmd_str[reg_num_idx].para_list[96], &init_cmd_str[reg_num_idx].para_list[97],\
		     &init_cmd_str[reg_num_idx].para_list[98], &init_cmd_str[reg_num_idx].para_list[99],\
		     &init_cmd_str[reg_num_idx].para_list[100], &init_cmd_str[reg_num_idx].para_list[101],\
		     &init_cmd_str[reg_num_idx].para_list[102], &init_cmd_str[reg_num_idx].para_list[103],\
		     &init_cmd_str[reg_num_idx].para_list[104], &init_cmd_str[reg_num_idx].para_list[105],\
		     &init_cmd_str[reg_num_idx].para_list[106], &init_cmd_str[reg_num_idx].para_list[107],\
		     &init_cmd_str[reg_num_idx].para_list[108], &init_cmd_str[reg_num_idx].para_list[109],\
		     &init_cmd_str[reg_num_idx].para_list[110], &init_cmd_str[reg_num_idx].para_list[111],\
		     &init_cmd_str[reg_num_idx].para_list[112], &init_cmd_str[reg_num_idx].para_list[113],\
		     &init_cmd_str[reg_num_idx].para_list[114], &init_cmd_str[reg_num_idx].para_list[115],\
		     &init_cmd_str[reg_num_idx].para_list[116], &init_cmd_str[reg_num_idx].para_list[117],\
		     &init_cmd_str[reg_num_idx].para_list[118], &init_cmd_str[reg_num_idx].para_list[119],\
		     &init_cmd_str[reg_num_idx].para_list[120], &init_cmd_str[reg_num_idx].para_list[121],\
		     &init_cmd_str[reg_num_idx].para_list[122], &init_cmd_str[reg_num_idx].para_list[123],\
		     &init_cmd_str[reg_num_idx].para_list[124], &init_cmd_str[reg_num_idx].para_list[125],\
		     &init_cmd_str[reg_num_idx].para_list[126], &init_cmd_str[reg_num_idx].para_list[127]);

	return count;
}
static ssize_t show_lcd_init_cmd_full(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
	int i, j, k;
	int str_size = 0;
	struct LCM_setting_table_V3 * init_cmd_str = NULL;
	struct LCM_DRIVER *lcm_drv = NULL;

	lge_pgc = (struct display_primary_path_context*)primary_get_pgc();

	if(!lge_pgc)
		return -EINVAL;
	else{
		if(lge_pgc->plcm->drv)
			lcm_drv = lge_pgc->plcm->drv;
	}

	if (lcm_drv->get_lcm_init_cmd_str)
			init_cmd_str = lcm_drv->get_lcm_init_cmd_str();

	if (lcm_drv->get_init_cmd_str_size)
			str_size = lcm_drv->get_init_cmd_str_size();

	if(init_cmd_str == NULL || str_size == 0)
		return -EINVAL;

	for (i = 0, j = 0; i < str_size && j < PAGE_SIZE; ++i) {
		sprintf(&buf[j], "%02x, ", init_cmd_str[i].id);
		j += 4;
		sprintf(&buf[j], "%02x, ", init_cmd_str[i].cmd);
		j += 4;
		sprintf(&buf[j], "%d, ", init_cmd_str[i].count);

		if (init_cmd_str[i].count < 10)
			j += 3;
		else if (init_cmd_str[i].count < 100)
	        	j += 4;
		else
	        	j += 5;
		buf[j++] = '{';
		for( k = 0; k <  init_cmd_str[i].count; k++ ){
			if( init_cmd_str[i].id == REGFLAG_ESCAPE_ID )
				break;
			sprintf(&buf[j], "%02x, ", init_cmd_str[i].para_list[k]);
			j += 4;
		}
		buf[j] = '}';
		buf[++j] = '\n';
		j++;
	}
	return j;
}

static ssize_t store_lcd_init_cmd_full(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i, j, k;
	int ret, str_size;
	struct LCM_setting_table_V3 * init_cmd_str = NULL;
	struct LCM_DRIVER *lcm_drv = NULL;

	lge_pgc = (struct display_primary_path_context*)primary_get_pgc();

	if(!lge_pgc)
		return -EINVAL;
	else{
		if(lge_pgc->plcm->drv)
			lcm_drv = lge_pgc->plcm->drv;
	}

	if (lcm_drv->get_lcm_init_cmd_str)
			init_cmd_str = lcm_drv->get_lcm_init_cmd_str();

	if (lcm_drv->get_init_cmd_str_size)
			str_size = lcm_drv->get_init_cmd_str_size();

	if(init_cmd_str == NULL || str_size == 0)
		return -EINVAL;

	if (count < 1)
	        return count;

	for (i = 0, j = 0; i < str_size && j < PAGE_SIZE; ++i) {
        while (!isdigit(buf[j]))
			j++;
		ret = sscanf(&buf[j], "%02x, %02x, %d", &init_cmd_str[i].id, &init_cmd_str[i].cmd, &init_cmd_str[i].count);
		printk("[LGE_TUNING] id = %02x, cmd = %02x, count = %d\n", init_cmd_str[i].id, init_cmd_str[i].cmd, init_cmd_str[i].count);

		if ( init_cmd_str[i].id == REGFLAG_ESCAPE_ID || init_cmd_str[i].count == 0){
			j += 13;
			continue;
		}

		if (init_cmd_str[i].count < 10)
			j += 12;
		else if (init_cmd_str[i].count < 100)
			j += 13;
		else
			j += 14;

		for( k = 0; k < init_cmd_str[i].count; k++){
			sscanf(&buf[j], "%02x, ", &init_cmd_str[i].para_list[k]);
			printk("[LGE_TUNING] para_list[%d] = %02x\n", k, init_cmd_str[i].para_list[k]);
			j += 4;
			if( buf[j] == '}' ){
				j++;
				break;
			}
		}
		if( buf[j] == '/' )
			break;
	}
	LG_ESD_recovery();

	return count;
}
#endif
//LGE_INIT_CMD_TUNING end

#if defined(CONFIG_LGE_BACKLIGHT_BRIGHTNESS_TUNING)

static ssize_t store_driver_brightness(struct kobject *kobj,struct kobj_attribute *attr, const char *buf,size_t size)
{
	char *pvalue = NULL;
	unsigned int sysfs_level = 0;

	sysfs_level = simple_strtoul(buf, &pvalue, 10);

#if defined(CONFIG_LGE_LCM_BACKLIGHT_PWM)
	set_pwm_backlight_brightness(sysfs_level);
#else
	set_backlight_brightness_rawdata(sysfs_level);
#endif
	return size;
}
static ssize_t show_driver_brightness(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int brightness = 0;

#if defined(CONFIG_LGE_LCM_BACKLIGHT_PWM)
	brightness = get_pwm_backlight_brightness();
#else
	brightness = get_backlight_brightness_rawdata();
#endif
	return sprintf(buf, "%d\n", brightness);
}

static ssize_t store_max_brightness(struct kobject *kobj,struct kobj_attribute *attr, const char *buf,size_t size)
{
	char *pvalue = NULL;
	unsigned int sysfs_level = 0;

	sysfs_level = simple_strtoul(buf, &pvalue, 10);

#if defined(CONFIG_LGE_LCM_BACKLIGHT_PWM)
	lge_set_bled_dim_brightness(sysfs_level);
	chargepump_set_backlight_level(1);
#else
	/* TO DO */
#endif
	return size;
}
static ssize_t show_max_brightness(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int brightness = 0;

#if defined(CONFIG_LGE_LCM_BACKLIGHT_PWM)
	brightness = lge_get_bled_dim_brightness();
#else
	/* TO DO */
#endif
	return sprintf(buf, "%d\n", brightness);
}

static ssize_t lcd_backlight_show_blmap(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
	unsigned int *table = blmap_arr;
        int i, j;

        for (i = 0, j = 0; i < blmap_size && j < PAGE_SIZE; ++i) {
				if (!(i % 15) && i != 0) {
                        buf[j] = '\n';
                        ++j;
                }
                sprintf(&buf[j], "%d ", table[i]);
                if (table[i] < 10)
                        j += 2;
                else if (table[i] < 100)
                        j += 3;
                else
                        j += 4;
        }

        return j;
}

static ssize_t lcd_backlight_store_blmap(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
        int i;
        int j;
        int value, ret;
	unsigned int *table = blmap_arr;

        if (count < 1)
                return count;

        for (i = 0, j = 0; i < count && j < blmap_size; ++i) {
                if (!isdigit(buf[i]))
                        continue;

                ret = sscanf(&buf[i], "%d", &value);
                if (ret < 1)
                        pr_err("read error\n");
                table[j] = (unsigned int)value;

                while (isdigit(buf[i]))
                        ++i;
                ++j;
        }

        return count;
}
#endif
//LGE_BACKLIGHT_BRIGHTNESS_TUNING end


#if defined(CONFIG_BACKLIGHT_DRIVER_REG_TUNING)
static ssize_t store_bl_ic_reg_addr(struct kobject *kobj,struct kobj_attribute *attr, const char *buf,size_t size)
{

	char *pvalue = NULL;
	unsigned char reg_addr = 0;

	reg_addr = simple_strtoul(buf, &pvalue, 10);

	set_bl_ic_reg_addr(reg_addr);

	return size;
}
static ssize_t show_bl_ic_reg_val(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned char read_val = 0;

	read_val = get_bl_ic_reg_val();

	return sprintf(buf, "%x\n", read_val);
}

static ssize_t store_bl_ic_reg_val(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,size_t size)
{

	char *pvalue = NULL;
	unsigned char reg_val = 0;

	reg_val = simple_strtoul(buf, &pvalue, 10);

	set_bl_ic_reg_val(reg_val);

	return size;
}
#endif

#if defined(CONFIG_LGE_DSI_PARAM_TUNING)
static struct kobj_attribute disp_config_attribute = __ATTR(disp_config, 0644 , lge_disp_config_show, lge_disp_config_store);
static struct kobj_attribute disp_config_add_attribute = __ATTR(disp_config_add, 0644 , lge_disp_config_add_show, lge_disp_config_add_store);
#endif
#if defined(CONFIG_LGE_INIT_CMD_TUNING)
static struct kobj_attribute init_cmd_attribute = __ATTR(init_cmd, 0644 , mtkfb_lge_init_cmd_find_show, mtkfb_lge_init_cmd_find_store);
static struct kobj_attribute init_cmd_edit_attribute = __ATTR(init_cmd_edit, 0644 , NULL, mtkfb_lge_init_cmd_edit_store);
static struct kobj_attribute init_cmd_full_attribute = __ATTR(init_cmd_full, 0644 , show_lcd_init_cmd_full, store_lcd_init_cmd_full);
#endif
#if defined(CONFIG_LGE_BACKLIGHT_BRIGHTNESS_TUNING)
static struct kobj_attribute driver_brightness_attribute = __ATTR(driver_brightness, 0644 , show_driver_brightness, store_driver_brightness);
static struct kobj_attribute blmap_attribute = __ATTR(blmap, 0644 , lcd_backlight_show_blmap, lcd_backlight_store_blmap);
static struct kobj_attribute max_brightness_attribute = __ATTR(max_brightness, 0644 , show_max_brightness, store_max_brightness);
#endif
#if defined(CONFIG_BACKLIGHT_DRIVER_REG_TUNING)
static struct kobj_attribute read_bl_ic_attribute = __ATTR(read_bl_ic_reg, 0644 , show_bl_ic_reg_val, store_bl_ic_reg_addr);
static struct kobj_attribute write_bl_ic_attribute = __ATTR(write_bl_ic_reg, 0644 , NULL, store_bl_ic_reg_val);
#endif

static struct attribute *attrs[] = {
#if defined(CONFIG_LGE_DSI_PARAM_TUNING)
	&disp_config_attribute.attr,
	&disp_config_add_attribute.attr,
#endif
#if defined(CONFIG_LGE_INIT_CMD_TUNING)
	&init_cmd_attribute.attr,
	&init_cmd_edit_attribute.attr,
	&init_cmd_full_attribute.attr,
#endif
#if defined(CONFIG_LGE_BACKLIGHT_BRIGHTNESS_TUNING)
	&driver_brightness_attribute.attr,
	&blmap_attribute.attr,
	&max_brightness_attribute.attr,
#endif
#if defined(CONFIG_BACKLIGHT_DRIVER_REG_TUNING)
	&read_bl_ic_attribute.attr,
	&write_bl_ic_attribute.attr,
#endif
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *lge_disp_setting_kobj;

static int __init lge_disp_setting_init(void)
{
	int retval;

	/*
	 * located under /sys/kernel/
	 *
	 * As this is a simple directory, no uevent will be sent to
	 * userspace.  That is why this function should not be used for
	 * any type of dynamic kobjects, where the name and number are
	 * not known ahead of time.
	 */
	lge_disp_setting_kobj = kobject_create_and_add("lge_disp_setting", kernel_kobj);
	if (!lge_disp_setting_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(lge_disp_setting_kobj, &attr_group);
	if (retval)
		kobject_put(lge_disp_setting_kobj);

	return retval;
}

static void __exit lge_disp_setting_exit(void)
{
	kobject_put(lge_disp_setting_kobj);
}

module_init(lge_disp_setting_init);
module_exit(lge_disp_setting_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("LGE");
