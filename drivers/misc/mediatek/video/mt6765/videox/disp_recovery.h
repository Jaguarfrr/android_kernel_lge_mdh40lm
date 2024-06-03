/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _DISP_RECOVERY_H_
#define _DISP_RECOVERY_H_
#include "ddp_info.h"
#include "ddp_dsi.h"

#define GPIO_EINT_MODE	0
#define GPIO_DSI_MODE	1



/* defined in mtkfb.c should move to mtkfb.h*/
extern unsigned int islcmconnected;
#if defined(CONFIG_LGE_DISPLAY_COMMON)
void LG_ESD_recovery(void);
int primary_display_esd_recovery_suspend(void);
int primary_display_esd_recovery_resume(void);
int primary_display_esd_report_touchintpin_keep_low(void);
bool get_esd_recovery_state(void);
void set_esd_recovery_state(unsigned int enable);
int primary_display_esd_recovery_suspend(void);
int primary_display_esd_recovery_resume(void);

int get_ap_lcd_state(void);
void set_ap_lcd_state(unsigned int state);
int get_display_state(void);
void set_display_state(unsigned int state);
int disp_esd_check(void);
bool get_disp_esd_check_lcm(void);
void set_disp_esd_check_lcm(bool enable);
void set_disp_check_delay(unsigned int delay);
#endif

void primary_display_check_recovery_init(void);
void primary_display_esd_check_enable(int enable);
unsigned int need_wait_esd_eof(void);

void external_display_check_recovery_init(void);
void external_display_esd_check_enable(int enable);

void set_esd_check_mode(unsigned int mode);
int do_lcm_vdo_lp_read(struct ddp_lcm_read_cmd_table *read_table);
int do_lcm_vdo_lp_write(struct ddp_lcm_write_cmd_table *write_table,
			unsigned int count);



#endif
