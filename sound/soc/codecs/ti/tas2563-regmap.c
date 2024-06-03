/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2563-regmap.c
**
** Description:
**     I2C driver with regmap for Texas Instruments TAS2563 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2563_REGMAP

//#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include "tas2563.h"

#ifdef CONFIG_TAS2563_CODEC
#include "tas2563-codec.h"
#endif

#ifdef CONFIG_TAS2563_MISC
#include "tas2563-misc.h"
#endif

#define ENABLE_TILOAD

#ifdef ENABLE_TILOAD
#include "tiload.h"
#endif

#ifdef CONFIG_SND_SOC_TAS2563_LGE
struct pinctrl *pinctrl_tas2563 = NULL;
struct pinctrl_state *irq1_42 = NULL;
struct pinctrl_state *irq2_90 = NULL;
struct pinctrl_state *reset_167_high = NULL;
struct pinctrl_state *reset_167_low = NULL;
#endif

#define LOW_TEMPERATURE_GAIN 6
#define LOW_TEMPERATURE_COUNTER 12
static char pICN[] = {0x00, 0x00, 0x2f, 0x2c};
static char pICNDelay[] = {0x00, 0x00, 0x70, 0x80};

static struct tas2563_priv *mTAS2563;

static int tas2563_regmap_write(struct tas2563_priv *p_tas2563,
	unsigned int reg, unsigned int value)
{
	int nResult = 0;
	int retry_count = TAS2563_I2C_RETRY_COUNT;

	if (p_tas2563->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while (retry_count--) {
		nResult = regmap_write(p_tas2563->regmap, reg,
			value);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if (retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2563_regmap_bulk_write(struct tas2563_priv *p_tas2563,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;
	int retry_count = TAS2563_I2C_RETRY_COUNT;

	if (p_tas2563->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while (retry_count--) {
		nResult = regmap_bulk_write(p_tas2563->regmap, reg,
				pData, nLength);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if (retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2563_regmap_read(struct tas2563_priv *p_tas2563,
	unsigned int reg, unsigned int *value)
{
	int nResult = 0;
	int retry_count = TAS2563_I2C_RETRY_COUNT;

	if (p_tas2563->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while (retry_count--) {
		nResult = regmap_read(p_tas2563->regmap, reg,
			value);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if (retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2563_regmap_bulk_read(struct tas2563_priv *p_tas2563,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;
	int retry_count = TAS2563_I2C_RETRY_COUNT;

	if (p_tas2563->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while (retry_count--) {
		nResult = regmap_bulk_read(p_tas2563->regmap, reg,
			pData, nLength);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if (retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2563_regmap_update_bits(struct tas2563_priv *p_tas2563,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int nResult = 0;
	int retry_count = TAS2563_I2C_RETRY_COUNT;

	if (p_tas2563->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while (retry_count--) {
		nResult = regmap_update_bits(p_tas2563->regmap, reg,
			mask, value);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if (retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2563_change_book_page(struct tas2563_priv *p_tas2563,
	enum channel chn,
	int book, int page)
{
	int n_result = 0;

	if ((chn&channel_left) || (p_tas2563->mn_channels == 1)) {
		p_tas2563->client->addr = p_tas2563->mnLAddr;
		if (p_tas2563->mn_l_current_book != book) {
			n_result = tas2563_regmap_write(p_tas2563,
				TAS2563_BOOKCTL_PAGE, 0);
			if (n_result < 0) {
				dev_err(p_tas2563->dev,
					"%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, n_result);
				goto end;
			}
			p_tas2563->mn_l_current_page = 0;
			n_result = tas2563_regmap_write(p_tas2563,
				TAS2563_BOOKCTL_REG, book);
			if (n_result < 0) {
				dev_err(p_tas2563->dev,
					"%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, n_result);
				goto end;
			}
			p_tas2563->mn_l_current_book = book;
		}

		if (p_tas2563->mn_l_current_page != page) {
			n_result = tas2563_regmap_write(p_tas2563,
				TAS2563_BOOKCTL_PAGE, page);
			if (n_result < 0) {
				dev_err(p_tas2563->dev,
					"%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, n_result);
				goto end;
			}
			p_tas2563->mn_l_current_page = page;
		}
	}

	if ((chn&channel_right) && (p_tas2563->mn_channels == 2)) {
		p_tas2563->client->addr = p_tas2563->mnRAddr;
		if (p_tas2563->mn_r_current_book != book) {
			n_result = tas2563_regmap_write(p_tas2563,
				TAS2563_BOOKCTL_PAGE, 0);
			if (n_result < 0) {
				dev_err(p_tas2563->dev,
					"%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, n_result);
				goto end;
			}
			p_tas2563->mn_r_current_page = 0;
			n_result = tas2563_regmap_write(p_tas2563,
				TAS2563_BOOKCTL_REG, book);
			if (n_result < 0) {
				dev_err(p_tas2563->dev,
					"%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, n_result);
				goto end;
			}
			p_tas2563->mn_r_current_book = book;
		}

		if (p_tas2563->mn_r_current_page != page) {
			n_result = tas2563_regmap_write(p_tas2563,
				TAS2563_BOOKCTL_PAGE, page);
			if (n_result < 0) {
				dev_err(p_tas2563->dev,
					"%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, n_result);
				goto end;
			}
			p_tas2563->mn_r_current_page = page;
		}
	}

end:
	return n_result;
}

static int tas2563_dev_read(struct tas2563_priv *p_tas2563,
	enum channel chn,
	unsigned int reg, unsigned int *pValue)
{
	int n_result = 0;
	pr_debug( "%s: chn: %x chn_num: %d\n",
		__func__, chn, p_tas2563->mn_channels);
	mutex_lock(&p_tas2563->dev_lock);

	n_result = tas2563_change_book_page(p_tas2563, chn,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	if ((chn == channel_left) || (p_tas2563->mn_channels == 1))
		p_tas2563->client->addr = p_tas2563->mnLAddr;
	else if (chn == channel_right)
		p_tas2563->client->addr = p_tas2563->mnRAddr;
	else
		dev_err(p_tas2563->dev,
				"%s, wrong channel number\n", __func__);

	n_result = tas2563_regmap_read(p_tas2563,
		TAS2563_PAGE_REG(reg), pValue);
	if (n_result < 0)
		dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, n_result);
	else
		pr_debug("%s: chn:%x:BOOK:PAGE:REG %u:%u:%u,0x%x\n", __func__,
			p_tas2563->client->addr, TAS2563_BOOK_ID(reg),
			TAS2563_PAGE_ID(reg),
			TAS2563_PAGE_REG(reg), *pValue);

end:
	mutex_unlock(&p_tas2563->dev_lock);
	return n_result;
}

static int tas2563_dev_write(struct tas2563_priv *p_tas2563, enum channel chn,
	unsigned int reg, unsigned int value)
{
	int n_result = 0;

	mutex_lock(&p_tas2563->dev_lock);

	n_result = tas2563_change_book_page(p_tas2563, chn,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	if ((chn&channel_left) || (p_tas2563->mn_channels == 1)) {
		p_tas2563->client->addr = p_tas2563->mnLAddr;

		n_result = tas2563_regmap_write(p_tas2563,
			TAS2563_PAGE_REG(reg), value);
		if (n_result < 0)
			dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, n_result);
		else
			pr_debug("%s: chn%x:BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
				__func__, p_tas2563->client->addr,
				TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
				TAS2563_PAGE_REG(reg), value);
	}

	if ((chn&channel_right) && (p_tas2563->mn_channels == 2)) {
		p_tas2563->client->addr = p_tas2563->mnRAddr;

		n_result = tas2563_regmap_write(p_tas2563,
		TAS2563_PAGE_REG(reg),
			value);
		if (n_result < 0)
			dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, n_result);
		else
			pr_debug("%s: chn%x:BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
				__func__, p_tas2563->client->addr,
				TAS2563_BOOK_ID(reg),
				TAS2563_PAGE_ID(reg),
				TAS2563_PAGE_REG(reg), value);
	}

end:
	mutex_unlock(&p_tas2563->dev_lock);
	return n_result;
}

static int tas2563_dev_bulk_write(struct tas2563_priv *p_tas2563,
	enum channel chn,
	unsigned int reg, unsigned char *p_data, unsigned int n_length)
{
	int n_result = 0;

	mutex_lock(&p_tas2563->dev_lock);

	n_result = tas2563_change_book_page(p_tas2563, chn,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	if ((chn&channel_left) || (p_tas2563->mn_channels == 1)) {
		p_tas2563->client->addr = p_tas2563->mnLAddr;
		n_result = tas2563_regmap_bulk_write(p_tas2563,
			TAS2563_PAGE_REG(reg), p_data, n_length);
		if (n_result < 0)
			dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, n_result);
		else
			pr_debug("%s: chn%x:BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
				__func__, p_tas2563->client->addr,
				TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
				TAS2563_PAGE_REG(reg), n_length);
	}

	if ((chn&channel_right) && (p_tas2563->mn_channels == 2)) {
		p_tas2563->client->addr = p_tas2563->mnRAddr;
				n_result = tas2563_regmap_bulk_write(p_tas2563,
			TAS2563_PAGE_REG(reg), p_data, n_length);
		if (n_result < 0)
			dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, n_result);
		else
			pr_debug("%s: %x:BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
				__func__, p_tas2563->client->addr,
				TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
				TAS2563_PAGE_REG(reg), n_length);
	}

end:
	mutex_unlock(&p_tas2563->dev_lock);
	return n_result;
}

static int tas2563_dev_bulk_read(struct tas2563_priv *p_tas2563,
	enum channel chn,
	unsigned int reg, unsigned char *p_data, unsigned int n_length)
{
	int n_result = 0;

	mutex_lock(&p_tas2563->dev_lock);

	if ((chn == channel_left) || (p_tas2563->mn_channels == 1))
		p_tas2563->client->addr = p_tas2563->mnLAddr;
	else if (chn == channel_right)
		p_tas2563->client->addr = p_tas2563->mnRAddr;
	else
		dev_err(p_tas2563->dev,
				"%s, wrong channel number\n", __func__);

	n_result = tas2563_change_book_page(p_tas2563, chn,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	n_result = tas2563_regmap_bulk_read(p_tas2563,
	TAS2563_PAGE_REG(reg), p_data, n_length);
	if (n_result < 0)
		dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, n_result);
	else
		pr_debug("%s: chn%x:BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, p_tas2563->client->addr,
			TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
			TAS2563_PAGE_REG(reg), n_length);
end:
	mutex_unlock(&p_tas2563->dev_lock);
	return n_result;
}

static int tas2563_dev_update_bits(struct tas2563_priv *p_tas2563,
	enum channel chn,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int n_result = 0;

	mutex_lock(&p_tas2563->dev_lock);
	n_result = tas2563_change_book_page(p_tas2563, chn,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	if ((chn&channel_left) || (p_tas2563->mn_channels == 1)) {
		p_tas2563->client->addr = p_tas2563->mnLAddr;
		n_result = tas2563_regmap_update_bits(p_tas2563,
			TAS2563_PAGE_REG(reg), mask, value);
		if (n_result < 0)
			dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, n_result);
		else
			pr_debug("%s: chn%x:BOOK:PAGE:REG %u:%u:%u, mask: 0x%x, val: 0x%x\n",
				__func__, p_tas2563->client->addr,
				TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
				TAS2563_PAGE_REG(reg), mask, value);
	}

	if ((chn&channel_right) && (p_tas2563->mn_channels == 2)) {
		p_tas2563->client->addr = p_tas2563->mnRAddr;
		n_result = tas2563_regmap_update_bits(p_tas2563,
			TAS2563_PAGE_REG(reg), mask, value);
		if (n_result < 0)
			dev_err(p_tas2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, n_result);
		else
			pr_debug("%s:chn%x:BOOK:PAGE:REG %u:%u:%u,mask: 0x%x, val: 0x%x\n",
				__func__, p_tas2563->client->addr,
				TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
				TAS2563_PAGE_REG(reg), mask, value);
	}

end:
	mutex_unlock(&p_tas2563->dev_lock);
	return n_result;
}

static const struct reg_default tas2563_reg_defaults[] = {
	{ TAS2563_Page, 0x00 },
	{ TAS2563_SoftwareReset, 0x00 },
	{ TAS2563_PowerControl, 0x0e },
	{ TAS2563_PlaybackConfigurationReg0, 0x10 },
	{ TAS2563_MiscConfigurationReg0, 0x07 },
	{ TAS2563_TDMConfigurationReg1, 0x02 },
	{ TAS2563_TDMConfigurationReg2, 0x0a },
	{ TAS2563_TDMConfigurationReg3, 0x10 },
	{ TAS2563_InterruptMaskReg0, 0xfc },
	{ TAS2563_InterruptMaskReg1, 0xb1 },
	{ TAS2563_InterruptConfiguration, 0x1d },
	{ TAS2563_MiscIRQ, 0x81 },
	{ TAS2563_ClockConfiguration, 0x0c },

};

static bool tas2563_volatile(struct device *dev, unsigned int reg)
{
	return true;
}

static bool tas2563_writeable(struct device *dev, unsigned int reg)
{
	return true;
}
static const struct regmap_config tas2563_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2563_writeable,
	.volatile_reg = tas2563_volatile,
//	.reg_defaults = tas2563_reg_defaults,
//	.num_reg_defaults = ARRAY_SIZE(tas2563_reg_defaults),
	.cache_type = REGCACHE_NONE,
	.max_register = 1 * 128,
};


static void tas2563_hw_reset(struct tas2563_priv *pTAS2563)
{
#ifdef CONFIG_SND_SOC_TAS2563_LGE
	int ret = 0;
	ret = pinctrl_select_state(pinctrl_tas2563, reset_167_low);
	if (ret) dev_err(pTAS2563->dev,"%s could not set reset_167_low pin\n",__func__);
	msleep(5);
	ret = pinctrl_select_state(pinctrl_tas2563, reset_167_high);
	if (ret) dev_err(pTAS2563->dev,"%s could not set reset_167_high pin\n",__func__);
	msleep(2);
#else
	if (gpio_is_valid(pTAS2563->mnResetGPIO)) {
		gpio_direction_output(pTAS2563->mnResetGPIO, 0);
#ifdef TAS2563_DOUBLE_RESET_PIN
		if (gpio_is_valid(pTAS2563->mnResetGPIO2))
			gpio_direction_output(pTAS2563->mnResetGPIO2, 0);
#endif

		msleep(5);

		gpio_direction_output(pTAS2563->mnResetGPIO, 1);
#ifdef TAS2563_DOUBLE_RESET_PIN
		if (gpio_is_valid(pTAS2563->mnResetGPIO2))
			gpio_direction_output(pTAS2563->mnResetGPIO2, 1);
#endif

		msleep(2);
	}
#endif

	pTAS2563->mn_l_current_book = -1;
	pTAS2563->mn_l_current_page = -1;
	pTAS2563->mn_r_current_book = -1;
	pTAS2563->mn_r_current_page = -1;
}

void tas2563_enableIRQ(struct tas2563_priv *ptas2563,
			enum channel chl, bool enable)
{
	static bool bLeftChlEnable;
	static bool bRightChlEnable;

	if (enable) {
		if (!ptas2563->mbIRQEnable) {
			if (chl & channel_left) {
				if (gpio_is_valid(ptas2563->mnIRQGPIO)) {
					enable_irq(ptas2563->mnIRQ);
					bLeftChlEnable = true;
				} else
					bLeftChlEnable = false;
			}

			if (chl & channel_right) {
				if (gpio_is_valid(ptas2563->mnIRQGPIO2)) {
					if (ptas2563->mnIRQ2 != ptas2563->mnIRQ) {
						enable_irq(ptas2563->mnIRQ2);
						bRightChlEnable = true;
					} else if (!bLeftChlEnable) {
						enable_irq(ptas2563->mnIRQ2);
						bRightChlEnable = true;
					} else
						bRightChlEnable = false;
				} else
					bRightChlEnable = false;
			}
			if (bLeftChlEnable || bRightChlEnable) {
				/* check after 10 ms */
				schedule_delayed_work(&ptas2563->irq_work,
					msecs_to_jiffies(10));
			}
			ptas2563->mbIRQEnable = true;
		}
	} else {
		if (ptas2563->mbIRQEnable) {
			if (gpio_is_valid(ptas2563->mnIRQGPIO)) {
				if (bLeftChlEnable) {
					disable_irq_nosync(ptas2563->mnIRQ);
					bLeftChlEnable = false;
				}
			}
			if (gpio_is_valid(ptas2563->mnIRQGPIO2)) {
				if (bRightChlEnable) {
					disable_irq_nosync(ptas2563->mnIRQ2);
					bRightChlEnable = false;
				}
			}
			ptas2563->mbIRQEnable = false;
		}
	}
}


static void irq_work_routine(struct work_struct *work)
{
	struct tas2563_priv *pTAS2563 =
		container_of(work, struct tas2563_priv, irq_work.work);
	unsigned int nDevInt1Status = 0, nDevInt2Status = 0, nDevInt3Status = 0;
	int nCounter = 2;
	int nResult = 0;
	int irqreg = 0;

	dev_info(pTAS2563->dev, "%s\n", __func__);
#ifdef CONFIG_TAS2563_CODEC
	mutex_lock(&pTAS2563->codec_lock);
#endif

	if (pTAS2563->mbRuntimeSuspend) {
		dev_info(pTAS2563->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}

	if (pTAS2563->mnPowerState == TAS2563_POWER_SHUTDOWN) {
		dev_info(pTAS2563->dev, "%s, device not powered\n", __func__);
		goto end;
	}

	//if (pTAS2563->mn_channels & channel_left && pTAS2563->spk_l_control) {
	if (pTAS2563->spk_l_control) {
		nResult = tas2563_dev_write(pTAS2563,
				channel_left,
				TAS2563_InterruptMaskReg0,
				TAS2563_InterruptMaskReg0_Disable);
		nResult = tas2563_dev_write(pTAS2563,
				channel_left,
				TAS2563_InterruptMaskReg1,
				TAS2563_InterruptMaskReg1_Disable);
		nResult = tas2563_dev_write(pTAS2563,
				channel_left,
				TAS2563_InterruptMaskReg2,
				TAS2563_InterruptMaskReg2_Disable);

		if (nResult < 0)
			goto reload;

		nResult = tas2563_dev_read(pTAS2563,
				channel_left,
				TAS2563_LatchedInterruptReg0, &nDevInt1Status);

		if (nResult >= 0)
			nResult = tas2563_dev_read(pTAS2563,
				channel_left,
				TAS2563_LatchedInterruptReg1, &nDevInt2Status);
		else
			goto reload;

		if (nResult >= 0)
			nResult = tas2563_dev_read(pTAS2563,
				channel_left,
				TAS2563_LatchedInterruptReg2, &nDevInt3Status);
		else
			goto reload;

		dev_info(pTAS2563->dev, "IRQ status : 0x%x, 0x%x, 0x%x \n",
			nDevInt1Status, nDevInt2Status, nDevInt3Status);

		if (((nDevInt1Status & 0x3) != 0) || ((nDevInt2Status & 0x04) != 0) || ((nDevInt3Status & 0x20) != 0)) {
			/* in case of INT_OC, INT_OT, TDM Clock Error, INT_UVLT, Brown Out shutdown */

			// Over Current Error
			if (nDevInt1Status & TAS2563_LatchedInterruptReg0_OCEFlagSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_OVER_CURRENT;
				dev_err(pTAS2563->dev, "SPK over current!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_OVER_CURRENT;

			 // Over Temp Error
			if (nDevInt1Status & TAS2563_LatchedInterruptReg0_OTEFlagSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_DIE_OVERTEMP;
				dev_err(pTAS2563->dev, "die over temperature!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_DIE_OVERTEMP;

			// Brown Out Shutdown Error
			if (nDevInt2Status & TAS2563_LatchedInterruptReg1_BrownOutShutdownSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_BROWNOUT_SHUTDOWN;
				dev_err(pTAS2563->dev, "brownout Shutdown Error!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_BROWNOUT_SHUTDOWN;

			  // Under Voltage error
			if (nDevInt3Status & TAS2563_LatchedInterruptReg2_VBATUVLOSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_UNDER_VOLTAGE;
				dev_err(pTAS2563->dev, "SPK under voltage!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

			goto reload;
		} else {
			nCounter = 2;

			while (nCounter > 0) {
				nResult = tas2563_dev_read(pTAS2563, channel_left,
					TAS2563_PowerControl, &nDevInt1Status);
				if (nResult < 0)
					goto reload;

				if ((nDevInt1Status & TAS2563_PowerControl_OperationalMode10_Mask)
					!= TAS2563_PowerControl_OperationalMode10_Shutdown)
					break;

				pTAS2563->read(pTAS2563, channel_left,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev, "IRQ reg is: %s %d, %d\n",
					__func__, irqreg, __LINE__);

				nResult = pTAS2563->update_bits(pTAS2563,
					channel_left,
					TAS2563_PowerControl,
					TAS2563_PowerControl_OperationalMode10_Mask |
					TAS2563_PowerControl_ISNSPower_Mask |
					TAS2563_PowerControl_VSNSPower_Mask,
					TAS2563_PowerControl_OperationalMode10_Active |
					TAS2563_PowerControl_VSNSPower_Active |
					TAS2563_PowerControl_ISNSPower_Active);
				if (nResult < 0)
					goto reload;

				pTAS2563->read(pTAS2563, channel_left,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev,
					"IRQ reg is: %s, %d, %d\n",
					__func__, irqreg, __LINE__);

				dev_info(pTAS2563->dev, "set ICN to -90dB\n");
				nResult = pTAS2563->bulk_write(pTAS2563,
					channel_left, TAS2563_ICN_REG, pICN, 4);
				if (nResult < 0)
					goto reload;

				pTAS2563->read(pTAS2563, channel_left,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev,
					"IRQ reg is: %d, %d\n",
					irqreg, __LINE__);

				dev_info(pTAS2563->dev, "set ICN delay\n");
				nResult = pTAS2563->bulk_write(pTAS2563,
					channel_left, TAS2563_ICN_DELAY, pICNDelay, 4);

				pTAS2563->read(pTAS2563, channel_left,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev,
					"IRQ reg is: %d, %d\n", irqreg, __LINE__);

				nCounter--;
				if (nCounter > 0) {
					/* in case check power status just after power on TAS2563 */
					dev_dbg(pTAS2563->dev,
					"PowSts B: 0x%x, check again after 10ms\n",
						nDevInt1Status);
					msleep(20);
				}
			}

			if ((nDevInt1Status & TAS2563_PowerControl_OperationalMode10_Mask)
				== TAS2563_PowerControl_OperationalMode10_Shutdown) {
				dev_err(pTAS2563->dev,
					"%s, Critical ERROR REG[0x%x] = 0x%x\n",
					__func__,
					TAS2563_PowerControl,
					nDevInt1Status);
				pTAS2563->mnErrCode |= ERROR_CLASSD_PWR;
				goto reload;
			}
			pTAS2563->mnErrCode &= ~ERROR_CLASSD_PWR;
		}
	}

	if (pTAS2563->mn_channels & channel_right && pTAS2563->spk_r_control) {
		nResult = tas2563_dev_write(pTAS2563,
				channel_right,
				TAS2563_InterruptMaskReg0,
				TAS2563_InterruptMaskReg0_Disable);
		nResult = tas2563_dev_write(pTAS2563,
				channel_right,
				TAS2563_InterruptMaskReg1,
				TAS2563_InterruptMaskReg1_Disable);
		nResult = tas2563_dev_write(pTAS2563,
				channel_right,
				TAS2563_InterruptMaskReg2,
				TAS2563_InterruptMaskReg2_Disable);

		if (nResult < 0)
			goto reload;

		nResult = tas2563_dev_read(pTAS2563,
				channel_right,
				TAS2563_LatchedInterruptReg0, &nDevInt1Status);

		if (nResult >= 0)
			nResult = tas2563_dev_read(pTAS2563,
				channel_right,
				TAS2563_LatchedInterruptReg1, &nDevInt2Status);
		else
			goto reload;

		if (nResult >= 0)
			nResult = tas2563_dev_read(pTAS2563,
				channel_right,
				TAS2563_LatchedInterruptReg2, &nDevInt2Status);
		else
			goto reload;

		dev_info(pTAS2563->dev, "IRQ2 status : 0x%x, 0x%x, 0x%x \n",
				nDevInt1Status, nDevInt2Status, nDevInt3Status);

		if (((nDevInt1Status & 0x3) != 0) || ((nDevInt2Status & 0x04) != 0) || ((nDevInt3Status & 0x20) != 0)) {
			/* in case of INT_OC, INT_OT, TDM Clock Error, INT_UVLT, Brown Out shutdown */

			// Over Current Error
			if (nDevInt1Status & TAS2563_LatchedInterruptReg0_OCEFlagSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_OVER_CURRENT;
				dev_err(pTAS2563->dev, "SPK over current!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_OVER_CURRENT;

			 // Over Temp Error
			if (nDevInt1Status & TAS2563_LatchedInterruptReg0_OTEFlagSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_DIE_OVERTEMP;
				dev_err(pTAS2563->dev, "die over temperature!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_DIE_OVERTEMP;

			// Brown Out Shutdown Error
			if (nDevInt2Status & TAS2563_LatchedInterruptReg1_BrownOutShutdownSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_BROWNOUT_SHUTDOWN;
				dev_err(pTAS2563->dev, "brownout shutdown Error!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_BROWNOUT_SHUTDOWN;

			  // Under Voltage error
			if (nDevInt3Status & TAS2563_LatchedInterruptReg2_VBATUVLOSticky_Interrupt) {
				pTAS2563->mnErrCode |= ERROR_UNDER_VOLTAGE;
				dev_err(pTAS2563->dev, "SPK under voltage!\n");
			} else
				pTAS2563->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

			goto reload;
		} else {
			nCounter = 2;

			while (nCounter > 0) {
				nResult = tas2563_dev_read(pTAS2563, channel_right,
					TAS2563_PowerControl, &nDevInt1Status);
				if (nResult < 0)
					goto reload;

				if ((nDevInt1Status & TAS2563_PowerControl_OperationalMode10_Mask)
					!= TAS2563_PowerControl_OperationalMode10_Shutdown)
					break;

				pTAS2563->read(pTAS2563, channel_right,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev,
					"IRQ reg is: %s %d, %d\n", __func__,
					irqreg, __LINE__);

				nResult = pTAS2563->update_bits(pTAS2563,
					channel_right,
					TAS2563_PowerControl,
					TAS2563_PowerControl_OperationalMode10_Mask |
					TAS2563_PowerControl_ISNSPower_Mask |
					TAS2563_PowerControl_VSNSPower_Mask,
					TAS2563_PowerControl_OperationalMode10_Active |
					TAS2563_PowerControl_VSNSPower_Active |
					TAS2563_PowerControl_ISNSPower_Active);
				if (nResult < 0)
					goto reload;

				pTAS2563->read(pTAS2563, channel_right,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev,
					"IRQ reg is: %s, %d, %d\n", __func__,
					irqreg, __LINE__);

				dev_info(pTAS2563->dev, "set ICN to -90dB\n");
				nResult = pTAS2563->bulk_write(pTAS2563,
					channel_right, TAS2563_ICN_REG, pICN, 4);
				if (nResult < 0)
					goto reload;

				pTAS2563->read(pTAS2563, channel_right,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev, "IRQ reg is: %d, %d\n",
					irqreg, __LINE__);

				dev_info(pTAS2563->dev, "set ICN delay\n");
				nResult = pTAS2563->bulk_write(pTAS2563,
					channel_right, TAS2563_ICN_DELAY,
					pICNDelay, 4);

				pTAS2563->read(pTAS2563, channel_right,
					TAS2563_LatchedInterruptReg0, &irqreg);
				dev_info(pTAS2563->dev, "IRQ reg is: %d, %d\n",
					irqreg, __LINE__);

				nCounter--;
				if (nCounter > 0) {
					/* in case check power status just after power on TAS2563 */
					dev_dbg(pTAS2563->dev,
					"PowSts B: 0x%x, check again after 10ms\n",
						nDevInt1Status);
					msleep(20);
				}
			}

			if ((nDevInt1Status & TAS2563_PowerControl_OperationalMode10_Mask)
				== TAS2563_PowerControl_OperationalMode10_Shutdown) {
				dev_err(pTAS2563->dev,
					"%s, Critical ERROR REG[0x%x] = 0x%x\n",
					__func__,
					TAS2563_PowerControl, nDevInt1Status);
				pTAS2563->mnErrCode |= ERROR_CLASSD_PWR;
				goto reload;
			}
			pTAS2563->mnErrCode &= ~ERROR_CLASSD_PWR;
		}
	}

	nResult = tas2563_dev_write(pTAS2563, channel_both,
				TAS2563_InterruptMaskReg0, 0xfC);
	if (nResult < 0)
		goto reload;

	nResult = tas2563_dev_write(pTAS2563, channel_both,
				TAS2563_InterruptMaskReg1, 0xfB);
	if (nResult < 0)
		goto reload;

	nResult = tas2563_dev_write(pTAS2563, channel_both,
				TAS2563_InterruptMaskReg2, 0xDF);
	if (nResult < 0)
		goto reload;

	goto end;

reload:
	/* hardware reset and reload */
	nResult = -1;
	//tas2563_LoadConfig(pTAS2563);
	//tas2563_set_program(pTAS2563, pTAS2563->mnCurrentProgram, pTAS2563->mnCurrentConfiguration);

	if (pTAS2563->spk_l_control && pTAS2563->spk_r_control) {
		tas2563_set_program(pTAS2563, channel_both,
			pTAS2563->mnCurrentProgram,
			pTAS2563->mnLeftCurrentConfiguration);
	} else if (pTAS2563->spk_l_control) {
		tas2563_set_program(pTAS2563, channel_left,
			pTAS2563->mnCurrentProgram,
			pTAS2563->mnLeftCurrentConfiguration);
	} else if (pTAS2563->mn_channels & channel_right && pTAS2563->spk_r_control) {
		tas2563_set_program(pTAS2563, channel_right,
		pTAS2563->mnCurrentProgram, pTAS2563->mnRightCurrentConfiguration);
	}

	if(pTAS2563->spk_l_control || pTAS2563->spk_r_control) {
		pTAS2563->mnPowerState =  TAS2563_POWER_ACTIVE;
		pTAS2563->mbPowerUp = true;
	}

end:
	if (nResult >= 0)
		tas2563_enableIRQ(pTAS2563, channel_both, true);

#ifdef CONFIG_TAS2563_CODEC
	mutex_unlock(&pTAS2563->codec_lock);
#endif
}

static enum hrtimer_restart timer_func(struct hrtimer *timer)
{
	struct tas2563_priv *pTAS2563 = container_of(timer,
		struct tas2563_priv, mtimerwork);

	if (pTAS2563->mnPowerState != TAS2563_POWER_SHUTDOWN) {
		if (!delayed_work_pending(&pTAS2563->irq_work))
			schedule_delayed_work(&pTAS2563->irq_work,
				msecs_to_jiffies(20));
	}

	return HRTIMER_NORESTART;
}

static irqreturn_t tas2563_irq_handler(int irq, void *dev_id)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)dev_id;

	tas2563_enableIRQ(pTAS2563, channel_both, false);
	/* get IRQ status after 100 ms */
	schedule_delayed_work(&pTAS2563->irq_work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}


static int tas2563_parse_dt(struct device *dev, struct tas2563_priv *pTAS2563)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;

	rc = of_property_read_u32(np, "ti,channels", &pTAS2563->mn_channels);
	if (rc) {
		dev_err(pTAS2563->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,channels", np->full_name, rc);
	} else {
		pr_debug("ti,channels=%d", pTAS2563->mn_channels);
	}

	rc = of_property_read_u32(np, "ti,left-channel", &pTAS2563->mnLAddr);
	if (rc) {
		dev_err(pTAS2563->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,left-channel", np->full_name, rc);
	} else {
		pr_debug("ti,left-channel=0x%x", pTAS2563->mnLAddr);
	}

	pTAS2563->mnResetGPIO = of_get_named_gpio(np, "ti,reset-gpio", 0);
	if (!gpio_is_valid(pTAS2563->mnResetGPIO)) {
		dev_err(pTAS2563->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio", np->full_name, pTAS2563->mnResetGPIO);
	} else {
		pr_debug("ti,reset-gpio=%d", pTAS2563->mnResetGPIO);
	}

	pTAS2563->mnIRQGPIO = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(pTAS2563->mnIRQGPIO)) {
		dev_err(pTAS2563->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name, pTAS2563->mnIRQGPIO);
	} else
		pr_debug("ti,irq-gpio=%d", pTAS2563->mnIRQGPIO);

	if (pTAS2563->mn_channels == 1) {
		dev_info(pTAS2563->dev,
			"%s: mono config, don't parse stereo params!\n",
			__func__);
		goto END;
	}

	rc = of_property_read_u32(np, "ti,right-channel", &pTAS2563->mnRAddr);
	if (rc) {
		dev_err(pTAS2563->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,right-channel", np->full_name, rc);
	} else {
		pr_debug("ti,right-channel=0x%x", pTAS2563->mnRAddr);
	}

#ifdef TAS2563_DOUBLE_RESET_PIN
	pTAS2563->mnResetGPIO2 = of_get_named_gpio(np, "ti,reset-gpio2", 0);
	if (!gpio_is_valid(pTAS2563->mnResetGPIO2)) {
		dev_err(pTAS2563->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio2", np->full_name, pTAS2563->mnResetGPIO2);
	} else {
		pr_debug("ti,reset-gpio2=%d", pTAS2563->mnResetGPIO2);
	}
#endif

	pTAS2563->mnIRQGPIO2 = of_get_named_gpio(np, "ti,irq-gpio2", 0);
	if (!gpio_is_valid(pTAS2563->mnIRQGPIO2)) {
		dev_err(pTAS2563->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio2", np->full_name, pTAS2563->mnIRQGPIO2);
	} else {
		pr_debug("ti,irq-gpio2=%d", pTAS2563->mnIRQGPIO2);
	}
END:
	return ret;
}


static int tas2563_runtime_suspend(struct tas2563_priv *pTAS2563)
{
	dev_dbg(pTAS2563->dev, "%s\n", __func__);

	pTAS2563->mbRuntimeSuspend = true;

	if (gpio_is_valid(pTAS2563->mnIRQGPIO)) {
		if (delayed_work_pending(&pTAS2563->irq_work)) {
			dev_dbg(pTAS2563->dev, "cancel IRQ work\n");
			cancel_delayed_work_sync(&pTAS2563->irq_work);
		}
	}

	return 0;
}

static int tas2563_runtime_resume(struct tas2563_priv *pTAS2563)
{
	struct TProgram *pProgram;

	dev_dbg(pTAS2563->dev, "%s\n", __func__);
	if (!pTAS2563->mpFirmware->mpPrograms) {
		dev_dbg(pTAS2563->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	if (pTAS2563->mnCurrentProgram >= pTAS2563->mpFirmware->mnPrograms) {
		dev_err(pTAS2563->dev, "%s, firmware corrupted\n", __func__);
		goto end;
	}

	pProgram = &(pTAS2563->mpFirmware->mpPrograms[pTAS2563->mnCurrentProgram]);

	pTAS2563->mbRuntimeSuspend = false;
end:

	return 0;
}

/*
struct tas2563_regmap {
	const char *name;
	unsigned int reg;
	int writeable;
} tas2563_regs[] = {
	{ "00_Device_Page",                       TAS2563_Page, 1 },
	{ "01_SW_Reset",                        TAS2563_SoftwareReset, 1 },
	{ "02_Power_Control",        TAS2563_PowerControl, 1 },
	{ "03_Playback_Config",                  TAS2563_PlaybackConfigurationReg0, 1 },
	{ "04_MISC_Config_1",                          TAS2563_MiscConfigurationReg0, 1 },
	{ "05_MISC_Config_2",                         TAS2563_TDMConfigurationReg0, 1 },
	{ "06_TDM_Config_1",                 TAS2563_TDMConfigurationReg0, 1 },
	{ "07_TDM2",         TAS2563_TDMConfigurationReg1, 1 },
	{ "08_TDM3",                  TAS2563_TDMConfigurationReg2, 1 },
	{ "09_TDM4",                            TAS2563_TDMConfigurationReg3, 1 },
	{ "0a_TDM5",       TAS2563_TDMConfigurationReg4, 1 },
	{ "0b_TDM6",                 TAS2563_TDMConfigurationReg5, 1 },
	{ "0c_TDM7",                    TAS2563_TDMConfigurationReg6, 1 },
	{ "0d_TDM8",                TAS2563_TDMConfigurationReg7, 1 },
	{ "0e_TDM9",               TAS2563_TDMConfigurationReg8, 1 },
	{ "0f_TDM10",                       TAS2563_TDMConfigurationReg9, 1 },
	{ "10_TDM_Colok_Detection_Monitor",                       TAS2563_TDMConfigurationReg10, 1 },
	{ "3e_NoiseGate_Control",                          TAS2563_NoiseGate, 1 },
	{ "5c__page2_NoiseGate_CH_Threshold_1",                          TAS2563_IDCT_92, 1 },
	{ "5d_page2_NoiseGate_CH_Threshold_2",                          TAS2563_IDCT_93, 1 },
	{ "5e_page2_NoiseGate_CH_Threshold_3",                          TAS2563_IDCT_94, 1 },
	{ "5f_page2_NoiseGate_CH_Threshold_4",                   TAS2563_IDCT_95, 1 },
	{ "64_page2_NoiseGate_Timer_1",                  TAS2563_IDCH_100, 1 },
	{ "65_page2_NoiseGate_Timer_2",                  TAS2563_IDCH_101, 1 },
	{ "66_page2_NoiseGate_Timer_3",                  TAS2563_IDCH_102, 1 },
	{ "67_page2_NoiseGate_Timer_4",                  TAS2563_IDCH_103, 1 },
	{ "0c_page2_DVC_PCM_1",                  TAS2563_DCV_PCM_12, 1 },
	{ "0d_page2_DVC_PCM_2",                  TAS2563_DCV_PCM_13, 1 },
	{ "0e_page2_DVC_PCM_3",                  TAS2563_DCV_PCM_14, 1 },
	{ "0f_page2_DVC_PCM_4",                  TAS2563_DCV_PCM_15, 1 },
};
*/

static ssize_t tas2563_tas_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int i = 0, n = 0, reg_count = 0x81;
	unsigned int read_buf = 0;

	//reg_count = sizeof(tas2563_regs) / sizeof(tas2563_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		tas2563_dev_read(mTAS2563, channel_left, TAS2563_REG(0x0, 0x0, i), &read_buf);
		n += scnprintf(buf + n, PAGE_SIZE - n, "(4c)%02X: %02X\n", i, read_buf);
	}

	for (i = 0; i < reg_count; i++) {
		tas2563_dev_read(mTAS2563, channel_right, TAS2563_REG(0x0, 0x0, i), &read_buf);
		n += scnprintf(buf + n, PAGE_SIZE - n, "(4d)%02X: %02X\n", i, read_buf);
	}

	return n;
}

static ssize_t tas2563_tas_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	unsigned int channel = 0, book = 0, page = 0, reg = 0, value = 0;

	if (count >= 45) {
		pr_err("%s:input too long\n", __func__);
		return -EFAULT;
	}

	if (sscanf(buf, "%x %x %x %x %x", &channel, &book, &page, &reg, &value) != 5) {
		pr_err("%s: unable to parse input\n", __func__);
		return -EFAULT;
	}

	dev_err(mTAS2563->dev,
		"%s, channel=0x%x, book=0x%x, page=0x%x, reg=0x%x, val=0x%x \n",
		__func__, channel, book, page, reg, value);

	ret = tas2563_dev_write(mTAS2563, channel,
		TAS2563_REG(book, page, reg), value);
	if (ret < 0) {
		dev_err(mTAS2563->dev, "%s, ERROR, reg=0x%x, E=%d\n",
			__func__, reg, ret);
	ret = -EFAULT;
	}

	return -1;
}

static DEVICE_ATTR(tas_register, S_IWUSR | S_IRUGO,
	tas2563_tas_register_show, tas2563_tas_register_store);

static struct attribute *tas2563_attrs[] = {
	&dev_attr_tas_register.attr,
	NULL
};

static const struct attribute_group tas2563_attr_group = {
	.attrs = tas2563_attrs,
};

/* tas2563_i2c_probe :
 * platform dependent
 * should implement hardware reset functionality
 */
static int tas2563_i2c_probe(struct i2c_client *pClient,
	const struct i2c_device_id *pID)
{
	struct tas2563_priv *pTAS2563 = NULL;
	int nResult = 0;
	unsigned int nValue = 0;
	const char *pFWName = NULL;

	dev_info(&pClient->dev, "%s enter\n", __func__);

	pTAS2563 = devm_kzalloc(&pClient->dev,
			sizeof(struct tas2563_priv), GFP_KERNEL);
	if (!pTAS2563) {
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2563->client = pClient;
	pTAS2563->dev = &pClient->dev;
	i2c_set_clientdata(pClient, pTAS2563);
	dev_set_drvdata(&pClient->dev, pTAS2563);

	pTAS2563->regmap = devm_regmap_init_i2c(pClient, &tas2563_i2c_regmap);
	if (IS_ERR(pTAS2563->regmap)) {
		nResult = PTR_ERR(pTAS2563->regmap);
		dev_err(&pClient->dev, "Failed to allocate register map: %d\n",
			nResult);
		goto err;
	}

	if (pClient->dev.of_node)
		tas2563_parse_dt(&pClient->dev, pTAS2563);
#ifdef CONFIG_SND_SOC_TAS2563_LGE
	pinctrl_tas2563= devm_pinctrl_get(&pClient->dev);
	if (IS_ERR(pinctrl_tas2563)) {
		dev_err(&pClient->dev,"%s Cannot find pinctrl_tas2563!\n",__func__);
		pinctrl_tas2563 = NULL;
	} else {
		reset_167_high = pinctrl_lookup_state(pinctrl_tas2563, "reset_high");
		if (IS_ERR(reset_167_high)) {
			nResult = PTR_ERR(reset_167_high);
			dev_err(&pClient->dev,"%s pinctrl_lookup_state reset_167_high fail %d\n", __func__, nResult);
			goto err;
		}
		reset_167_low = pinctrl_lookup_state(pinctrl_tas2563, "reset_low");
		if (IS_ERR(reset_167_low)) {
			nResult = PTR_ERR(reset_167_low);
			dev_err(&pClient->dev,"%s pinctrl_lookup_state reset_167_low fail %d\n", __func__, nResult);
			goto err;
		}
		tas2563_hw_reset(pTAS2563);
	}
#else
	if (gpio_is_valid(pTAS2563->mnResetGPIO)) {
		nResult = gpio_request(pTAS2563->mnResetGPIO, "TAS2563-RESET");
		if (nResult < 0) {
			dev_err(pTAS2563->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2563->mnResetGPIO);
			goto err;
		}
		tas2563_hw_reset(pTAS2563);
	}
#endif

#ifdef TAS2563_DOUBLE_RESET_PIN
	if (gpio_is_valid(pTAS2563->mnResetGPIO2)) {
		nResult = gpio_request(pTAS2563->mnResetGPIO2, "TAS2563-RESET2");
		if (nResult < 0) {
			dev_err(pTAS2563->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2563->mnResetGPIO2);
			goto err;
		}
		tas2563_hw_reset(pTAS2563);
	}
#endif

	pTAS2563->read = tas2563_dev_read;
	pTAS2563->write = tas2563_dev_write;
	pTAS2563->bulk_read = tas2563_dev_bulk_read;
	pTAS2563->bulk_write = tas2563_dev_bulk_write;
	pTAS2563->update_bits = tas2563_dev_update_bits;
	pTAS2563->enableIRQ = tas2563_enableIRQ;
//	pTAS2563->clearIRQ = tas2563_clearIRQ;
	pTAS2563->hw_reset = tas2563_hw_reset;
	pTAS2563->runtime_suspend = tas2563_runtime_suspend;
	pTAS2563->runtime_resume = tas2563_runtime_resume;
	pTAS2563->mnRestart = 0;
	pTAS2563->mnPowerState = TAS2563_POWER_SHUTDOWN;

	mutex_init(&pTAS2563->dev_lock);

	/* Reset the chip */
	nResult = tas2563_dev_write(pTAS2563, channel_both,
				TAS2563_SoftwareReset, 0x01);
	if (nResult < 0) {
		dev_err(&pClient->dev, "I2c fail, %d\n", nResult);
//		goto err;
	}

	msleep(1);
	nResult = tas2563_dev_read(pTAS2563, channel_left,
				TAS2563_RevisionandPGID, &nValue);
	pTAS2563->mnLPGID = nValue;
	nResult = tas2563_dev_read(pTAS2563, channel_right,
			TAS2563_RevisionandPGID, &nValue);
	pTAS2563->mnRPGID = nValue;
	dev_info(pTAS2563->dev, "LPGID: %d RPGID:%d\n",
			pTAS2563->mnLPGID, pTAS2563->mnRPGID);

	pFWName = TAS2563_FW_NAME;

	nResult = tas2563_dev_read(pTAS2563, channel_left,
			TAS2563_PlaybackConfigurationReg0, &nValue);
	pTAS2563->mnLeftTlv =
		nValue & TAS2563_PlaybackConfigurationReg0_AmplifierLevel40_Mask;
	nResult = tas2563_dev_read(pTAS2563, channel_right,
			TAS2563_PlaybackConfigurationReg0, &nValue);
	pTAS2563->mnRightTlv =
		nValue & TAS2563_PlaybackConfigurationReg0_AmplifierLevel40_Mask;

#ifdef CONFIG_SND_SOC_TAS2563_LGE
	if (pinctrl_tas2563 != NULL) {
		irq1_42 = pinctrl_lookup_state(pinctrl_tas2563, "default_l");
		if (IS_ERR(irq1_42)) {
			nResult = PTR_ERR(irq1_42);
			dev_err(&pClient->dev,"%s pinctrl_lookup_state irq1_42 fail %d\n", __func__, nResult);
		} else {
			nResult = pinctrl_select_state(pinctrl_tas2563, irq1_42);
			if (nResult) dev_err(&pClient->dev,"%s could not set irq1_42 pin\n",__func__);
		}
		irq2_90 = pinctrl_lookup_state(pinctrl_tas2563, "default_r");
		if (IS_ERR(irq2_90)) {
			nResult = PTR_ERR(irq2_90);
			dev_err(&pClient->dev,"%s pinctrl_lookup_state irq2_90 fail %d\n", __func__, nResult);
		} else {
			nResult = pinctrl_select_state(pinctrl_tas2563, irq2_90);
			if (nResult) dev_err(&pClient->dev,"%s could not set irq2_90 pin\n",__func__);
		}
	}
#endif

	if (gpio_is_valid(pTAS2563->mnIRQGPIO)) {
		nResult = gpio_request(pTAS2563->mnIRQGPIO, "TAS2563-IRQ");
		if (nResult < 0) {
			dev_err(pTAS2563->dev,
				"%s: GPIO %d request INT error\n",
				__func__, pTAS2563->mnIRQGPIO);
			goto err;
		}

		gpio_direction_input(pTAS2563->mnIRQGPIO);
		pTAS2563->mnIRQ = gpio_to_irq(pTAS2563->mnIRQGPIO);
		dev_dbg(pTAS2563->dev, "irq = %d\n", pTAS2563->mnIRQ);
		nResult = request_threaded_irq(pTAS2563->mnIRQ, tas2563_irq_handler,
					NULL, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					pClient->name, pTAS2563);
		if (nResult < 0) {
			dev_err(pTAS2563->dev,
				"request_irq failed, %d\n", nResult);
			goto err;
		}
		disable_irq_nosync(pTAS2563->mnIRQ);
	}

	if (gpio_is_valid(pTAS2563->mnIRQGPIO2)) {
		nResult = gpio_request(pTAS2563->mnIRQGPIO2, "TAS2563-IRQ2");
		if (nResult < 0) {
			dev_err(pTAS2563->dev,
				"%s: GPIO %d request INT error\n",
				__func__, pTAS2563->mnIRQGPIO2);
			goto err;
		}

		gpio_direction_input(pTAS2563->mnIRQGPIO2);
		pTAS2563->mnIRQ2 = gpio_to_irq(pTAS2563->mnIRQGPIO2);
		dev_dbg(pTAS2563->dev, "irq2 = %d\n", pTAS2563->mnIRQ2);
		nResult = request_threaded_irq(pTAS2563->mnIRQ2, tas2563_irq_handler,
					NULL, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					pClient->name, pTAS2563);
		if (nResult < 0) {
			dev_err(pTAS2563->dev,
				"request_irq failed, %d\n", nResult);
			goto err;
		}
		disable_irq_nosync(pTAS2563->mnIRQ2);
	}

	if (gpio_is_valid(pTAS2563->mnIRQGPIO) ||
		gpio_is_valid(pTAS2563->mnIRQGPIO2)) {
		INIT_DELAYED_WORK(&pTAS2563->irq_work, irq_work_routine);
	}

	pTAS2563->mpCalFirmware = devm_kzalloc(&pClient->dev,
					sizeof(struct TFirmware), GFP_KERNEL);
	if (!pTAS2563->mpCalFirmware) {
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2563->mpFirmware = devm_kzalloc(&pClient->dev,
					sizeof(struct TFirmware), GFP_KERNEL);
	if (!pTAS2563->mpFirmware) {
		nResult = -ENOMEM;
		goto err;
	}

#ifdef CONFIG_TAS2563_CODEC
	mutex_init(&pTAS2563->codec_lock);
	nResult = tas2563_register_codec(pTAS2563);
	if (nResult < 0) {
		dev_err(pTAS2563->dev,
			"register codec failed, %d\n", nResult);
		goto err;
	}
#endif

#ifdef CONFIG_TAS2563_MISC
	mutex_init(&pTAS2563->file_lock);
	nResult = tas2563_register_misc(pTAS2563);
	if (nResult < 0) {
		dev_err(pTAS2563->dev,
			"register misc failed, %d\n", nResult);
		goto err;
	}
#endif

#ifdef ENABLE_TILOAD
	tiload_driver_init(pTAS2563);
#endif

	hrtimer_init(&pTAS2563->mtimerwork, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pTAS2563->mtimerwork.function = timer_func;
	//INIT_WORK(&pTAS2563->mtimerwork, timer_func);

	request_firmware_nowait(THIS_MODULE, 1, pFWName,
		pTAS2563->dev, GFP_KERNEL, pTAS2563, tas2563_fw_ready);

	nResult = sysfs_create_group(&pClient->dev.kobj, &tas2563_attr_group);
	if (nResult < 0) {
		dev_err(pTAS2563->dev, "sysfs_create_group failed, %d\n", nResult);
		nResult = 0;
	}
	mTAS2563 = pTAS2563;

err:

	return nResult;
}

static int tas2563_i2c_remove(struct i2c_client *pClient)
{
	struct tas2563_priv *pTAS2563 = i2c_get_clientdata(pClient);

	dev_info(pTAS2563->dev, "%s\n", __func__);

#ifdef CONFIG_TAS2563_CODEC
	tas2563_deregister_codec(pTAS2563);
	mutex_destroy(&pTAS2563->codec_lock);
#endif

#ifdef CONFIG_TAS2563_MISC
	tas2563_deregister_misc(pTAS2563);
	mutex_destroy(&pTAS2563->file_lock);
#endif

	mutex_destroy(&pTAS2563->dev_lock);
	return 0;
}

static const struct i2c_device_id tas2563_i2c_id[] = {
	{"tas2563", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tas2563_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas2563_of_match[] = {
	{.compatible = "ti,tas2563"},
	{},
};

MODULE_DEVICE_TABLE(of, tas2563_of_match);
#endif

static struct i2c_driver tas2563_i2c_driver = {
	.driver = {
			.name = "tas2563",
			.owner = THIS_MODULE,
#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(tas2563_of_match),
#endif
		},
	.probe = tas2563_i2c_probe,
	.remove = tas2563_i2c_remove,
	.id_table = tas2563_i2c_id,
};

module_i2c_driver(tas2563_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2563 I2C Smart Amplifier driver");
MODULE_LICENSE("GPL v2");

#endif
