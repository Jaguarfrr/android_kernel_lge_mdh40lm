/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MT_OTP_PMIC_CONFIG_H__
#define __MT_OTP_PMIC_CONFIG_H__
#include <linux/types.h>

//EXTERN FUNCTION
extern u32 otp_pmic_fsource_init(struct platform_device *pdev);
extern u32 otp_pmic_fsource_set(void);
extern u32 otp_pmic_fsource_release(void);
extern u32 otp_pmic_is_fsource_enabled(void);
extern u32 otp_pmic_high_vcore_init(void);
extern u32 otp_pmic_high_vcore_set(void);
extern u32 otp_pmic_high_vcore_release(void);
#endif /* __MT_OTP_PMIC_CONFIG_H__ */
