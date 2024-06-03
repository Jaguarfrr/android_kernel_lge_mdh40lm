// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
//#include <mach/upmu_sw.h>
//#include <mach/upmu_hw.h>
#include <mt-plat/mtk_otp.h>
#include <mt-plat/upmu_common.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/mt6357/core.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include "otp_pmic_config.h"

#define kal_uint32 unsigned int

static struct regmap *g_regmap;
static struct regulator *reg_vefuse;

/**************************************************************************
 *EXTERN FUNCTION
 **************************************************************************/
u32 otp_pmic_fsource_init(struct platform_device *pdev)
{
	reg_vefuse = devm_regulator_get_optional(&pdev->dev, "vefuse");
	if (IS_ERR(reg_vefuse)) {
		pr_info("[OTP] fail to get vefuse regulator");
		return PTR_ERR(reg_vefuse);
	}

	g_regmap = regulator_get_regmap(reg_vefuse);
	if (IS_ERR_VALUE(g_regmap)) {
		g_regmap = NULL;
		pr_info("[OTP] get regmap fail\n");
		return -ENOENT;
	}

	return 0;
}

u32 otp_pmic_fsource_set(void)
{
	u32 ret_val = 0;

	// 1.8V
	ret_val |= regmap_update_bits(
			g_regmap,
			(kal_uint32)(MT6357_RG_VEFUSE_VOSEL_ADDR),
			(kal_uint32)(MT6357_RG_VEFUSE_VOSEL_MASK) <<
				MT6357_RG_VEFUSE_VOSEL_SHIFT,
			(kal_uint32)(0x4) <<
				MT6357_RG_VEFUSE_VOSEL_SHIFT
			);

	// +40mV
	ret_val |= regmap_update_bits(
			g_regmap,
			(kal_uint32)(MT6357_RG_VEFUSE_VOCAL_ADDR),
			(kal_uint32)(MT6357_RG_VEFUSE_VOCAL_MASK) <<
				MT6357_RG_VEFUSE_VOCAL_SHIFT,
			(kal_uint32)(0x4) <<
				MT6357_RG_VEFUSE_VOCAL_SHIFT
			);

	// Fsource(or VEFUSE) enabled
	ret_val |= regmap_update_bits(
			g_regmap,
			(kal_uint32)(MT6357_RG_LDO_VEFUSE_EN_ADDR),
			(kal_uint32)(MT6357_RG_LDO_VEFUSE_EN_MASK) <<
				MT6357_RG_LDO_VEFUSE_EN_SHIFT,
			(kal_uint32)(0x1) <<
				MT6357_RG_LDO_VEFUSE_EN_SHIFT
			);

	mdelay(10);

	return ret_val;
}

u32 otp_pmic_fsource_release(void)
{
	u32 ret_val = 0;

	// Fsource(VEFUSE or VMIPI) disabled
	ret_val |= regmap_update_bits(
			g_regmap,
			(kal_uint32)(MT6357_RG_LDO_VEFUSE_EN_ADDR),
			(kal_uint32)(MT6357_RG_LDO_VEFUSE_EN_MASK) <<
				MT6357_RG_LDO_VEFUSE_EN_SHIFT,
			(kal_uint32)(0x0) <<
				MT6357_RG_LDO_VEFUSE_EN_SHIFT
			);

	mdelay(10);

	return ret_val;
}

u32 otp_pmic_is_fsource_enabled(void)
{
	u32 regVal = 0;

	/*  Check Fsource(VEFUSE or VMIPI) Status */
	regmap_read(
		g_regmap,
		(kal_uint32)(MT6357_RG_LDO_VEFUSE_EN_ADDR),
		&regVal);

	regVal = (regVal >> MT6357_RG_LDO_VEFUSE_EN_SHIFT) &
		MT6357_RG_LDO_VEFUSE_EN_MASK;

	/* return 1 : fsource enabled
	 * return 0 : fsource disabled
	 */

	return regVal == 1;
}

static struct mtk_pm_qos_request dvfsrc_vcore_opp_req;

u32 otp_pmic_high_vcore_init(void)
{
	mtk_pm_qos_add_request(&dvfsrc_vcore_opp_req, MTK_PM_QOS_VCORE_OPP,
			MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);

	return 0;
}

u32 otp_pmic_high_vcore_set(void)
{
	//OPP table :
	//
	//0	0.8 - Min(AP, GPU, MD1 VB)
	//1	0.7 - Min(AP, GPU VB)
	//2	0.7 - MD1 VB
	//3	0.65
	//4	VCORE_OPP_UNREQ

	mtk_pm_qos_update_request(&dvfsrc_vcore_opp_req, 0);

	return STATUS_DONE;
}

u32 otp_pmic_high_vcore_release(void)
{
	mtk_pm_qos_update_request(&dvfsrc_vcore_opp_req,
		MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);

	return STATUS_DONE;
}
