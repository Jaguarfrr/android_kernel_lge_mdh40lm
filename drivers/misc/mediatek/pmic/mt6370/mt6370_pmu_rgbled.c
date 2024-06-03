// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/leds.h>
#include <linux/workqueue.h>

#ifdef CONFIG_LGE_LEDS
#include <linux/leds-pattern-id.h>
#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif
#endif

#include "inc/mt6370_pmu.h"
#include "inc/mt6370_pmu_rgbled.h"

#define MT6370_PMU_RGBLED_DRV_VERSION	"1.0.1_MTK"

enum {
	MT6370_PMU_LED_PWMMODE = 0,
	MT6370_PMU_LED_BREATHMODE,
	MT6370_PMU_LED_REGMODE,
	MT6370_PMU_LED_MAXMODE,
};

struct mt6370_led_classdev {
	struct led_classdev led_dev;
	int led_index;
#ifdef CONFIG_LGE_LEDS
	unsigned long pattern_id;
	uint8_t default_isink;
	struct delayed_work pattern_dwork;
#endif
};

struct mt6370_pmu_rgbled_data {
	struct mt6370_pmu_chip *chip;
	struct device *dev;
	struct delayed_work dwork;
};

#ifdef CONFIG_LGE_LEDS
struct mt6370_pmu_breath_data {
	uint8_t tr1;
	uint8_t tr2;
	uint8_t tf1;
	uint8_t tf2;
	uint8_t ton;
	uint8_t toff;
};

struct mt6370_pmu_blink_data {
	unsigned long don;
	unsigned long doff;
};

struct mt6370_pmu_pattern {
	/* pattern information */
	unsigned long id;
	const char *name;

	/* pattern data */
	int mode;
	struct mt6370_pmu_breath_data breath;
	struct mt6370_pmu_blink_data blink;
};
#endif

#define MT_LED_ATTR(_name) {\
	.attr = {\
		.name = #_name,\
		.mode = 0644,\
	},\
	.show = mt_led_##_name##_attr_show,\
	.store = mt_led_##_name##_attr_store,\
}

#define MT_LED_ATTR2(_name) {\
	.attr = {\
		.name = #_name,\
		.mode = 0644,\
	},\
	.show = mt_led_##_name##_attr_show2,\
	.store = mt_led_##_name##_attr_store2,\
}

static const u8 rgbled_init_data[] = {
	0x60, /* MT6370_PMU_REG_RGB1DIM: 0x82 */
	0x60, /* MT6370_PMU_REG_RGB2DIM: 0x83 */
	0x60, /* MT6370_PMU_REG_RGB3DIM: 0x84 */
	0x0F, /* MT6370_PMU_REG_RGBEN: 0x85 */
	0x08, /* MT6370_PMU_REG_RGB1ISINK: 0x86 */
	0x08, /* MT6370_PMU_REG_RGB2ISINK: 0x87 */
	0x08, /* MT6370_PMU_REG_RGB3ISINK: 0x88 */
	0x52, /* MT6370_PMU_REG_RGB1TR: 0x89 */
	0x25, /* MT6370_PMU_REG_RGB1TF: 0x8A */
	0x11, /* MT6370_PMU_REG_RGB1TONTOFF: 0x8B */
	0x52, /* MT6370_PMU_REG_RGB2TR: 0x8C */
	0x25, /* MT6370_PMU_REG_RGB2TF: 0x8D */
	0x11, /* MT6370_PMU_REG_RGB2TONTOFF: 0x8E */
	0x52, /* MT6370_PMU_REG_RGB3TR: 0x8F */
	0x25, /* MT6370_PMU_REG_RGB3TF: 0x90 */
	0x11, /* MT6370_PMU_REG_RGB3TONTOFF: 0x91 */
	0x60, /* MT6370_PMU_REG_RGBCHRINDDIM: 0x92 */
	0x07, /* MT6370_PMU_REG_RGBCHRINDCTRL: 0x93 */
	0x52, /* MT6370_PMU_REG_RGBCHRINDTR: 0x94 */
	0x25, /* MT6370_PMU_REG_RGBCHRINDTF: 0x95 */
	0x11, /* MT6370_PMU_REG_RGBCHRINDTONTOFF: 0x96 */
#ifdef CONFIG_LGE_LEDS
	0x7F, /* MT6370_PMU_REG_RGBOPENSHORTEN: 0x97 */
#else /*  */
	0xFF, /* MT6370_PMU_REG_RGBOPENSHORTEN: 0x97 */
#endif
};

#ifdef CONFIG_LGE_LEDS
static struct mt6370_pmu_pattern default_pattern = {
	.id = LED_PATTERN_CALL_01,
	.name = "default",
	.mode = MT6370_PMU_LED_PWMMODE,
	.blink = {1000, 2000},
};

static struct mt6370_pmu_pattern patterns[] = {
	{
		.id = LED_PATTERN_POWER_ON,
		.name = "power on",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {7, 2, 7, 4, 3, 0},
	},
	{
		.id = LED_PATTERN_CHARGING,
		.name = "charging",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {7, 2, 7, 4, 3, 0},
	},
	{
		.id = LED_PATTERN_POWER_OFF,
		.name = "power off",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {7, 2, 7, 4, 3, 0},
	},
	{
		.id = LED_PATTERN_MISSED_NOTI,
		.name = "missed noti",
		.mode = MT6370_PMU_LED_PWMMODE,
		/* LGE Scenario is 12 seconds but MTK PMIC support max 10 seconds */
		.blink = {500, 9500},
	},
	{
		.id = LED_PATTERN_ALARM,
		.name = "alarm",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {0, 0, 0, 0, 0, 0},
	},
	{
		.id = LED_PATTERN_CALL_01,
		.name = "call_01",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {7, 2, 7, 4, 3, 0},
	},
	{
		.id = LED_PATTERN_CALL_02,
		.name = "call_02",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {0, 0, 0, 0, 4, 0},
	},
	{
		.id = LED_PATTERN_URGENT_CALL_MISSED_NOTI,
		.name = "urgent call",
		.mode = MT6370_PMU_LED_PWMMODE,
		/* LGE Scenario is 12 seconds but MTK PMIC support max 10 seconds */
		.blink = {500, 9500},
	},
	{
		.id = LED_PATTERN_INCOMING_CALL,
		.name = "incoming call",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {2, 0, 0, 0, 0, 0},
	},
	{
		.id = LED_PATTERN_MISSED_CALL,
		.name = "missed call",
		.mode = MT6370_PMU_LED_PWMMODE,
		/* LGE Scenario is 12 seconds but MTK PMIC support max 10 seconds */
		.blink = {500, 9500},
	},
	{
		.id = LED_PATTERN_URGENT_INCOMING_CALL,
		.name = "urgent incoming call",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {0, 0, 0, 0, 0, 0},
	},
	{
		.id = LED_PATTERN_KNOCK_ON,
		.name = "knock on",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {0, 0, 0, 0, 4, 0},
	},
	{
		.id = LED_PATTERN_BT_CONNECTED,
		.name = "bluetooth connected",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {0, 0, 0, 0, 4, 0},
	},
	{
		.id = LED_PATTERN_BT_DISCONNECTED,
		.name = "bluetooth disconnected",
		.mode = MT6370_PMU_LED_BREATHMODE,
		.breath = {0, 0, 0, 0, 4, 0},
	},
        {
		.id = LED_PATTERN_TMUS_MISSED_NOTI,
		.name = "tmus missed noti",
		.mode = MT6370_PMU_LED_PWMMODE,
		/* LGE Scenario is 12 seconds but MTK PMIC support max 10 seconds */
		.blink = {500, 9500},
	},
        {
		.id = LED_PATTERN_TMUS_URGENT_MISSED_NOTI,
		.name = "urgent tmus missed noti",
		.mode = MT6370_PMU_LED_PWMMODE,
		/* LGE Scenario is 12 seconds but MTK PMIC support max 10 seconds */
		.blink = {500, 9500},
	},
};
#endif

static const u8 rgbled_init_data2[] = {
	0x00, /* 0x82 */
	0xCA, /* 0x83 */
	0xCA, /* 0x84 */
	0xCA, /* 0x85 */
	0xCA, /* 0x86 */
	0x1F, /* 0x87 */
	0x1F, /* 0x88 */
	0x1F, /* 0x89 */
	0x1F, /* 0x8A */
	0x48, /* 0x8B */
	0x48, /* 0x8C */
	0x11, /* 0x8D */
	0x11, /* 0x8E */
	0x11, /* 0x8F */
	0x11, /* 0x90 */
	0x11, /* 0x91 */
	0x11, /* 0x92 */
	0x11, /* 0x93 */
	0x11, /* 0x94 */
	0x11, /* 0x95 */
	0x11, /* 0x96 */
	0x11, /* 0x97 */
	0x11, /* 0x98 */
};

static inline int mt6370_pmu_led_get_index(struct led_classdev *led_cdev)
{
	struct mt6370_led_classdev *mt_led_cdev =
				(struct mt6370_led_classdev *)led_cdev;

	return mt_led_cdev->led_index;
}

static inline int mt6370_pmu_led_update_bits(struct led_classdev *led_cdev,
	uint8_t reg_addr, uint8_t reg_mask, uint8_t reg_data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data =
				dev_get_drvdata(led_cdev->dev->parent);

	return mt6370_pmu_reg_update_bits(rgbled_data->chip,
					reg_addr, reg_mask, reg_data);
}

static inline int mt6370_pmu_led_reg_read(struct led_classdev *led_cdev,
	uint8_t reg_addr)
{
	struct mt6370_pmu_rgbled_data *rgbled_data =
				dev_get_drvdata(led_cdev->dev->parent);

	return mt6370_pmu_reg_read(rgbled_data->chip, reg_addr);
}

static inline void mt6370_pmu_led_enable_dwork(struct led_classdev *led_cdev)
{
	struct mt6370_pmu_rgbled_data *rgbled_data =
				dev_get_drvdata(led_cdev->dev->parent);

	cancel_delayed_work_sync(&rgbled_data->dwork);
	schedule_delayed_work(&rgbled_data->dwork, msecs_to_jiffies(100));
}

/* 6372 start */
static void mt6370_pmu_led_bright_set2(struct led_classdev *led_cdev,
	enum led_brightness bright)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0xF, reg_shift = 0, en_mask = 0;
	bool need_enable_timer = true;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x83;
		en_mask = 0x80;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x84;
		en_mask = 0x40;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x85;
		en_mask = 0x20;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x86;
		en_mask = 0x10;
		need_enable_timer = false;
		break;
	default:
		dev_err(led_cdev->dev, "invalid mt led index\n");
		return;
	}
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, reg_mask,
					 (bright & reg_mask) << reg_shift);
	if (ret < 0) {
		dev_err(led_cdev->dev, "update brightness fail\n");
		return;
	}
	if (!bright)
		need_enable_timer = false;
	if (need_enable_timer) {
		mt6370_pmu_led_enable_dwork(led_cdev);
		return;
	}
	ret = mt6370_pmu_led_update_bits(led_cdev, 0x82, en_mask,
					 (bright > 0) ? en_mask : ~en_mask);
	if (ret < 0)
		dev_err(led_cdev->dev, "update enable bit fail\n");
}

static enum led_brightness mt6370_pmu_led_bright_get2(
	struct led_classdev *led_cdev)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0xF, reg_shift = 0, en_mask = 0;
	bool need_enable_timer = true;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x83;
		en_mask = 0x80;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x84;
		en_mask = 0x40;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x85;
		en_mask = 0x20;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x86;
		en_mask = 0x10;
		need_enable_timer = false;
		break;
	default:
		dev_err(led_cdev->dev, "invalid mt led index\n");
		return -EINVAL;
	}
	ret = mt6370_pmu_led_reg_read(led_cdev, 0x82);
	if (ret < 0)
		return ret;
	if (!(ret & en_mask))
		return LED_OFF;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	return (ret & reg_mask) >> reg_shift;
}

static inline int mt6370_pmu_led_config_pwm2(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	const ulong dim_time[] = { 8000, 4000, 2000, 1000, 500, 250, 8, 4};
	const unsigned long ton = *delay_on, toff = *delay_off;
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	int reg_addr, reg_mask, reg_shift;
	int i, j, ret = 0;

	dev_dbg(led_cdev->dev, "%s, on %lu, off %lu\n", __func__, ton, toff);
	/* find the close dim freq */
	for (i = ARRAY_SIZE(dim_time) - 1; i >= 0; i--) {
		if (dim_time[i] >= (ton + toff))
			break;
	}
	if (i < 0) {
		dev_warn(led_cdev->dev, "no match, sum %lu\n", ton + toff);
		i = 0;
	}
	/* write pwm dim freq selection */
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x8B;
		reg_mask = 0xE0;
		reg_shift = 5;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x8B;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x8C;
		reg_mask = 0xE0;
		reg_shift = 5;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x8C;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	default:
		return -EINVAL;
	}
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 reg_mask, i << reg_shift);
	if (ret < 0)
		return ret;
	/* find the closest pwm duty */
	j = 256 * ton / (ton + toff);
	if (j == 0)
		j = 1;
	j--;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x87;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x88;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x89;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x8A;
		break;
	default:
		return -EINVAL;
	}
	reg_mask = 0xFF;
	reg_shift = 0;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 reg_mask, j << reg_shift);
	if (ret < 0)
		return ret;
	return 0;
}

static int mt6370_pmu_led_change_mode2(struct led_classdev *led_cdev, int mode);
static int mt6370_pmu_led_blink_set2(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	int mode_sel = MT6370_PMU_LED_PWMMODE;
	int ret = 0;

	if (!*delay_on && !*delay_off)
		*delay_on = *delay_off = 500;
	if (!*delay_off)
		mode_sel = MT6370_PMU_LED_REGMODE;
	if (mode_sel == MT6370_PMU_LED_PWMMODE) {
		ret = mt6370_pmu_led_config_pwm2(led_cdev, delay_on, delay_off);
		if (ret < 0)
			dev_err(led_cdev->dev, "%s: cfg pwm fail\n", __func__);
	}
	ret = mt6370_pmu_led_change_mode2(led_cdev, mode_sel);
	if (ret < 0)
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
	return 0;
}

static struct mt6370_led_classdev mt6370_led_classdev2[MT6370_PMU_MAXLED] = {
	{
		.led_dev =  {
			.max_brightness = 12,
			.brightness_set = mt6370_pmu_led_bright_set2,
			.brightness_get = mt6370_pmu_led_bright_get2,
			.blink_set = mt6370_pmu_led_blink_set2,
		},
		.led_index = MT6370_PMU_LED1,
	},
	{
		.led_dev =  {
			.max_brightness = 12,
			.brightness_set = mt6370_pmu_led_bright_set2,
			.brightness_get = mt6370_pmu_led_bright_get2,
			.blink_set = mt6370_pmu_led_blink_set2,
		},
		.led_index = MT6370_PMU_LED2,
	},
	{
		.led_dev =  {
			.max_brightness = 12,
			.brightness_set = mt6370_pmu_led_bright_set2,
			.brightness_get = mt6370_pmu_led_bright_get2,
			.blink_set = mt6370_pmu_led_blink_set2,
		},
		.led_index = MT6370_PMU_LED3,
	},
	{
		.led_dev =  {
			.max_brightness = 12,
			.brightness_set = mt6370_pmu_led_bright_set2,
			.brightness_get = mt6370_pmu_led_bright_get2,
			.blink_set = mt6370_pmu_led_blink_set2,
		},
		.led_index = MT6370_PMU_LED4,
	},
};

static int mt6370_pmu_led_change_mode2(struct led_classdev *led_cdev, int mode)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0;
	int ret = 0;

	if (mode >= MT6370_PMU_LED_MAXMODE)
		return -EINVAL;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x83;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x84;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x85;
		break;
	case MT6370_PMU_LED4:
		/* disable auto mode */
		ret = mt6370_pmu_led_update_bits(led_cdev, 0x82, 0x08, 0xFF);
		if (ret < 0)
			return ret;
		reg_addr = 0x86;
		break;
	default:
		return -EINVAL;
	}
	return mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0xC0, mode << 6);
}

static const struct device_attribute mt_led_cc_mode_attrs2[] = {
};

static int mt6370_pmu_led_cc_activate2(struct led_classdev *led_cdev)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_cc_mode_attrs2); i++) {
		ret = device_create_file(led_cdev->dev,
					 mt_led_cc_mode_attrs2 + i);
		if (ret < 0) {
			dev_err(led_cdev->dev,
				"%s: create file fail %d\n", __func__, i);
			goto out_create_file;
		}
	}
	ret = mt6370_pmu_led_change_mode2(led_cdev, MT6370_PMU_LED_REGMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
		goto out_change_mode;
	}
	return 0;
out_change_mode:
	i = ARRAY_SIZE(mt_led_cc_mode_attrs2);
out_create_file:
	while (--i >= 0)
		device_remove_file(led_cdev->dev, mt_led_cc_mode_attrs2 + i);
	return ret;
}

static void mt6370_pmu_led_cc_deactivate2(struct led_classdev *led_cdev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_cc_mode_attrs2); i++)
		device_remove_file(led_cdev->dev, mt_led_cc_mode_attrs2 + i);
}

static ssize_t mt_led_pwm_duty_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x87;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x88;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x89;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x8A;
		break;
	default:
		return -EINVAL;
	}
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_addr = ret & 0xFF;
	return snprintf(buf, PAGE_SIZE, "%d (max: %d)\n", reg_addr, 255);
}

static ssize_t mt_led_pwm_duty_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > 255)
		return -EINVAL;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x87;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x88;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x89;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x8A;
		break;
	default:
		return -EINVAL;
	}
	reg_data = store << 0;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0xFF, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static const char * const led_dim_freq2[] = {
	"0.125Hz",
	"0.25Hz",
	"0.5Hz",
	"1Hz",
	"2Hz",
	"4Hz",
	"128Hz",
	"256Hz",
};

static ssize_t mt_led_pwm_dim_freq_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0,  reg_mask = 0, reg_shift = 0, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int i = 0, ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x8B;
		reg_mask = 0xE0;
		reg_shift = 5;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x8B;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x8C;
		reg_mask = 0xE0;
		reg_shift = 5;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x8C;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	default:
		return -EINVAL;
	}
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;
	ret = 0;
	for (i = 0; i < ARRAY_SIZE(led_dim_freq2); i++) {
		if (reg_data == i)
			ret += snprintf(buf + ret, cnt - ret, ">");
		ret += snprintf(buf + ret, cnt - ret, "%s ", led_dim_freq2[i]);
	}
	ret += snprintf(buf + ret, cnt - ret, "\n");
	return ret;
}

static ssize_t mt_led_pwm_dim_freq_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift = 0, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store >= ARRAY_SIZE(led_dim_freq2))
		return -EINVAL;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = 0x8B;
		reg_mask = 0xE0;
		reg_shift = 5;
		break;
	case MT6370_PMU_LED2:
		reg_addr = 0x8B;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	case MT6370_PMU_LED3:
		reg_addr = 0x8C;
		reg_mask = 0xE0;
		reg_shift = 5;
		break;
	case MT6370_PMU_LED4:
		reg_addr = 0x8C;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	default:
		return -EINVAL;
	}
	reg_data = store << reg_shift;
	ret = mt6370_pmu_led_update_bits(led_cdev,
					 reg_addr, reg_mask, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static const struct device_attribute mt_led_pwm_mode_attrs2[] = {
	MT_LED_ATTR2(pwm_duty),
	MT_LED_ATTR2(pwm_dim_freq),
};

static int mt6370_pmu_led_pwm_activate2(struct led_classdev *led_cdev)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_pwm_mode_attrs2); i++) {
		ret = device_create_file(led_cdev->dev,
					 mt_led_pwm_mode_attrs2 + i);
		if (ret < 0) {
			dev_err(led_cdev->dev,
				"%s: create file fail %d\n", __func__, i);
			goto out_create_file;
		}
	}
	ret = mt6370_pmu_led_change_mode2(led_cdev, MT6370_PMU_LED_PWMMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
		goto out_change_mode;
	}
	return 0;
out_change_mode:
	i = ARRAY_SIZE(mt_led_pwm_mode_attrs2);
out_create_file:
	while (--i >= 0)
		device_remove_file(led_cdev->dev, mt_led_pwm_mode_attrs2 + i);
	return ret;
}

static void mt6370_pmu_led_pwm_deactivate2(struct led_classdev *led_cdev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_pwm_mode_attrs2); i++)
		device_remove_file(led_cdev->dev, mt_led_pwm_mode_attrs2 + i);
}

static int mt6370_pmu_led_get_breath_regbase2(struct led_classdev *led_cdev)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		ret = 0x8D;
		break;
	case MT6370_PMU_LED2:
		ret = 0x90;
		break;
	case MT6370_PMU_LED3:
		ret = 0x93;
		break;
	case MT6370_PMU_LED4:
		ret = 0x96;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static ssize_t mt_led_tr1_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & 0xF0) >> 4;
	return snprintf(buf, cnt,
			"%d (max 15, 0.125s, step 0.25s)\n", reg_data);
}

static ssize_t mt_led_tr1_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > 15)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << 4;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0xF0, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_tr2_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & 0x0F) >> 0;
	return snprintf(buf, cnt,
			"%d (max 15, 0.125s, step 0.25s)\n", reg_data);
}

static ssize_t mt_led_tr2_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > 15)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << 0;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0x0F, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_tf1_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & 0xF0) >> 4;
	return snprintf(buf, cnt,
			"%d (max 15, 0.125s, step 0.25s)\n", reg_data);
}

static ssize_t mt_led_tf1_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > 15)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << 4;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0xF0, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_tf2_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & 0x0F) >> 0;
	return snprintf(buf, cnt,
			"%d (max 15, 0.125s, step 0.25s)\n", reg_data);
}

static ssize_t mt_led_tf2_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > 15)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << 0;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0x0F, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_ton_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & 0xF0) >> 4;
	return snprintf(buf, cnt,
			"%d (max 15, 0.125s, step 0.25s)\n", reg_data);
}

static ssize_t mt_led_ton_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > 15)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << 4;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0xF0, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_toff_attr_show2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & 0x0F) >> 0;
	return snprintf(buf, cnt,
			"%d (max 15, 0.125s, step 0.25s)\n", reg_data);
}

static ssize_t mt_led_toff_attr_store2(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > 15)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase2(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << 0;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, 0x0F, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static const struct device_attribute mt_led_breath_mode_attrs2[] = {
	MT_LED_ATTR2(tr1),
	MT_LED_ATTR2(tr2),
	MT_LED_ATTR2(tf1),
	MT_LED_ATTR2(tf2),
	MT_LED_ATTR2(ton),
	MT_LED_ATTR2(toff),
};

static int mt6370_pmu_led_breath_activate2(struct led_classdev *led_cdev)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_breath_mode_attrs2); i++) {
		ret = device_create_file(led_cdev->dev,
				mt_led_breath_mode_attrs2 + i);
		if (ret < 0) {
			dev_err(led_cdev->dev,
				"%s: create file fail %d\n", __func__, i);
			goto out_create_file;
		}
	}

	ret = mt6370_pmu_led_change_mode2(led_cdev, MT6370_PMU_LED_BREATHMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
		goto out_change_mode;
	}
	return 0;
out_change_mode:
	i = ARRAY_SIZE(mt_led_breath_mode_attrs2);
out_create_file:
	while (--i >= 0)
		device_remove_file(led_cdev->dev,
				   mt_led_breath_mode_attrs2 + i);
	return ret;
}

static void mt6370_pmu_led_breath_deactivate2(struct led_classdev *led_cdev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_breath_mode_attrs2); i++)
		device_remove_file(led_cdev->dev,
				   mt_led_breath_mode_attrs2 + i);
}

static struct led_trigger mt6370_pmu_led_trigger2[] = {
	{
		.name = "cc_mode",
		.activate = mt6370_pmu_led_cc_activate2,
		.deactivate = mt6370_pmu_led_cc_deactivate2,
	},
	{
		.name = "pwm_mode",
		.activate = mt6370_pmu_led_pwm_activate2,
		.deactivate = mt6370_pmu_led_pwm_deactivate2,
	},
	{
		.name = "breath_mode",
		.activate = mt6370_pmu_led_breath_activate2,
		.deactivate = mt6370_pmu_led_breath_deactivate2,
	},
};
/* 6372 end */
static void mt6370_pmu_led_bright_set(struct led_classdev *led_cdev,
	enum led_brightness bright)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0x7, reg_shift = 0, en_mask = 0;
	bool need_enable_timer = true;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		en_mask = 0x80;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		en_mask = 0x40;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		en_mask = 0x20;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x3;
		en_mask = 0x10;
		need_enable_timer = false;
		break;
	default:
		dev_err(led_cdev->dev, "invalid mt led index\n");
		return;
	}
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, reg_mask,
					 (bright & reg_mask) << reg_shift);
	if (ret < 0) {
		dev_err(led_cdev->dev, "update brightness fail\n");
		return;
	}
	if (!bright)
		need_enable_timer = false;
	if (need_enable_timer) {
		mt6370_pmu_led_enable_dwork(led_cdev);
		return;
	}
	ret = mt6370_pmu_led_update_bits(led_cdev, MT6370_PMU_REG_RGBEN,
					 en_mask,
					 (bright > 0) ? en_mask : ~en_mask);
	if (ret < 0)
		dev_err(led_cdev->dev, "update enable bit fail\n");
}

static enum led_brightness mt6370_pmu_led_bright_get(
	struct led_classdev *led_cdev)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0x7, reg_shift = 0, en_mask = 0;
	bool need_enable_timer = true;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		en_mask = 0x80;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		en_mask = 0x40;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		en_mask = 0x20;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x3;
		en_mask = 0x10;
		need_enable_timer = false;
		break;
	default:
		dev_err(led_cdev->dev, "invalid mt led index\n");
		return -EINVAL;
	}
	ret = mt6370_pmu_led_reg_read(led_cdev, MT6370_PMU_REG_RGBEN);
	if (ret < 0)
		return ret;
	if (!(ret & en_mask))
		return LED_OFF;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	return (ret & reg_mask) >> reg_shift;
}

static inline int mt6370_pmu_led_config_pwm(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	const ulong dim_time[] = { 10000, 5000, 2000, 1000, 500, 200, 5, 1};
	const unsigned long ton = *delay_on, toff = *delay_off;
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	int reg_addr, reg_mask, reg_shift;
	int i, j, ret = 0;

	dev_dbg(led_cdev->dev, "%s, on %lu, off %lu\n", __func__, ton, toff);
	/* find the close dim freq */
	for (i = ARRAY_SIZE(dim_time) - 1; i >= 0; i--) {
		if (dim_time[i] >= (ton + toff))
			break;
	}
	if (i < 0) {
		dev_warn(led_cdev->dev, "no match, sum %lu\n", ton + toff);
		i = 0;
	}
	/* write pwm dim freq selection */
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		reg_mask = 0x38;
		reg_shift = 3;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		reg_mask = 0x38;
		reg_shift = 3;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		reg_mask = 0x38;
		reg_shift = 3;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	default:
		return -EINVAL;
	}
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 reg_mask, i << reg_shift);
	if (ret < 0)
		return ret;
	/* find the closest pwm duty */
	j = 32 * ton / (ton + toff);
	if (j == 0)
		j = 1;
	j--;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1DIM;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2DIM;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3DIM;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDDIM;
		break;
	default:
		return -EINVAL;
	}
	reg_mask = MT6370_LED_PWMDUTYMASK;
	reg_shift = MT6370_LED_PWMDUTYSHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 reg_mask, j << reg_shift);
	if (ret < 0)
		return ret;
	return 0;
}

static int mt6370_pmu_led_change_mode(struct led_classdev *led_cdev, int mode);
static int mt6370_pmu_led_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	int mode_sel = MT6370_PMU_LED_PWMMODE;
	int ret = 0;

	if (!*delay_on && !*delay_off)
		*delay_on = *delay_off = 500;
	if (!*delay_off)
		mode_sel = MT6370_PMU_LED_REGMODE;
	if (mode_sel == MT6370_PMU_LED_PWMMODE) {
		/* workaround, fix cc to pwm */
		ret = mt6370_pmu_led_change_mode(led_cdev,
						 MT6370_PMU_LED_BREATHMODE);
		if (ret < 0)
			dev_err(led_cdev->dev, "%s: mode fix fail\n", __func__);
		ret = mt6370_pmu_led_config_pwm(led_cdev, delay_on, delay_off);
		if (ret < 0)
			dev_err(led_cdev->dev, "%s: cfg pwm fail\n", __func__);
	}
	ret = mt6370_pmu_led_change_mode(led_cdev, mode_sel);
	if (ret < 0)
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
	return 0;
}

static struct mt6370_led_classdev mt6370_led_classdev[MT6370_PMU_MAXLED] = {
	{
		.led_dev =  {
			.max_brightness = 6,
			.brightness_set = mt6370_pmu_led_bright_set,
			.brightness_get = mt6370_pmu_led_bright_get,
			.blink_set = mt6370_pmu_led_blink_set,
		},
		.led_index = MT6370_PMU_LED1,
#ifdef CONFIG_LGE_LEDS
		.pattern_id = 0,
		.default_isink = 0,
#endif
	},
	{
		.led_dev =  {
			.max_brightness = 6,
			.brightness_set = mt6370_pmu_led_bright_set,
			.brightness_get = mt6370_pmu_led_bright_get,
			.blink_set = mt6370_pmu_led_blink_set,
		},
		.led_index = MT6370_PMU_LED2,
#ifdef CONFIG_LGE_LEDS
		.pattern_id = 0,
		.default_isink = 0,
#endif
	},
	{
		.led_dev =  {
			.max_brightness = 6,
			.brightness_set = mt6370_pmu_led_bright_set,
			.brightness_get = mt6370_pmu_led_bright_get,
			.blink_set = mt6370_pmu_led_blink_set,
		},
		.led_index = MT6370_PMU_LED3,
#ifdef CONFIG_LGE_LEDS
		.pattern_id = 0,
		.default_isink = 0,
#endif
	},
	{
		.led_dev =  {
			.max_brightness = 3,
			.brightness_set = mt6370_pmu_led_bright_set,
			.brightness_get = mt6370_pmu_led_bright_get,
			.blink_set = mt6370_pmu_led_blink_set,
		},
		.led_index = MT6370_PMU_LED4,
#ifdef CONFIG_LGE_LEDS
		.pattern_id = 0,
		.default_isink = 0,
#endif
	},
};

static int mt6370_pmu_led_change_mode(struct led_classdev *led_cdev, int mode)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0;
	int ret = 0;

	if (mode >= MT6370_PMU_LED_MAXMODE)
		return -EINVAL;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1DIM;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2DIM;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3DIM;
		break;
	case MT6370_PMU_LED4:
		/* disable auto mode */
		ret = mt6370_pmu_led_update_bits(led_cdev,
						 MT6370_PMU_REG_RGBCHRINDDIM,
						 0x80, 0x80);
		if (ret < 0)
			return ret;
		reg_addr = MT6370_PMU_REG_RGBCHRINDDIM;
		break;
	default:
		return -EINVAL;
	}
	return mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					  MT6370_LED_MODEMASK,
					  mode << MT6370_LED_MODESHFT);
}

static const char * const soft_start_str[] = {
	"0.5us",
	"1us",
	"1.5us",
	"2us",
};

static ssize_t mt_led_soft_start_step_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0xC0, reg_shift = 6, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int i = 0, ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x60;
		reg_shift = 5;
		break;
	default:
		return -EINVAL;
	}

	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;
	ret = 0;
	for (i = 0; i < ARRAY_SIZE(soft_start_str); i++) {
		if (reg_data == i)
			ret += snprintf(buf + ret, cnt - ret, ">");
		ret += snprintf(buf + ret, cnt - ret, "%s ", soft_start_str[i]);
	}
	ret += snprintf(buf + ret, cnt - ret, "\n");
	return ret;
}

static ssize_t mt_led_soft_start_step_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0xC0, reg_shift = 6, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store >= ARRAY_SIZE(soft_start_str))
		return -EINVAL;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x60;
		reg_shift = 5;
		break;
	default:
		return -EINVAL;
	}
	reg_data = store << reg_shift;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 reg_mask, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static const struct device_attribute mt_led_cc_mode_attrs[] = {
	MT_LED_ATTR(soft_start_step),
};

static int mt6370_pmu_led_cc_activate(struct led_classdev *led_cdev)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_cc_mode_attrs); i++) {
		ret = device_create_file(led_cdev->dev,
					 mt_led_cc_mode_attrs + i);
		if (ret < 0) {
			dev_err(led_cdev->dev,
				"%s: create file fail %d\n", __func__, i);
			goto out_create_file;
		}
	}
	ret = mt6370_pmu_led_change_mode(led_cdev, MT6370_PMU_LED_REGMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
		goto out_change_mode;
	}
	return 0;
out_change_mode:
	i = ARRAY_SIZE(mt_led_cc_mode_attrs);
out_create_file:
	while (--i >= 0)
		device_remove_file(led_cdev->dev, mt_led_cc_mode_attrs + i);
	return ret;
}

static void mt6370_pmu_led_cc_deactivate(struct led_classdev *led_cdev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_cc_mode_attrs); i++)
		device_remove_file(led_cdev->dev, mt_led_cc_mode_attrs + i);
}

static ssize_t mt_led_pwm_duty_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1DIM;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2DIM;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3DIM;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDDIM;
		break;
	default:
		return -EINVAL;
	}
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_addr = ret & MT6370_LED_PWMDUTYMASK;
	return snprintf(buf, PAGE_SIZE, "%d (max: %d)\n", reg_addr,
			MT6370_LED_PWMDUTYMAX);
}

static ssize_t mt_led_pwm_duty_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > MT6370_LED_PWMDUTYMAX)
		return -EINVAL;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1DIM;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2DIM;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3DIM;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDDIM;
		break;
	default:
		return -EINVAL;
	}
	reg_data = store << MT6370_LED_PWMDUTYSHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LED_PWMDUTYMASK, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static const char * const led_dim_freq[] = {
	"0.1Hz",
	"0.2Hz",
	"0.5Hz",
	"1Hz",
	"2Hz",
	"5Hz",
	"200Hz",
	"1KHz",
};

static ssize_t mt_led_pwm_dim_freq_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0,  reg_mask = 0x38, reg_shift = 3, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int i = 0, ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	default:
		return -EINVAL;
	}
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;
	ret = 0;
	for (i = 0; i < ARRAY_SIZE(led_dim_freq); i++) {
		if (reg_data == i)
			ret += snprintf(buf + ret, cnt - ret, ">");
		ret += snprintf(buf + ret, cnt - ret, "%s ", led_dim_freq[i]);
	}
	ret += snprintf(buf + ret, cnt - ret, "\n");
	return ret;
}

static ssize_t mt_led_pwm_dim_freq_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0x38, reg_shift = 3, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store >= ARRAY_SIZE(led_dim_freq))
		return -EINVAL;
	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	default:
		return -EINVAL;
	}
	reg_data = store << reg_shift;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 reg_mask, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static const struct device_attribute mt_led_pwm_mode_attrs[] = {
	MT_LED_ATTR(pwm_duty),
	MT_LED_ATTR(pwm_dim_freq),
};

static int mt6370_pmu_led_pwm_activate(struct led_classdev *led_cdev)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_pwm_mode_attrs); i++) {
		ret = device_create_file(led_cdev->dev,
					 mt_led_pwm_mode_attrs + i);
		if (ret < 0) {
			dev_err(led_cdev->dev,
				"%s: create file fail %d\n", __func__, i);
			goto out_create_file;
		}
	}

	/* workaround, fix cc to pwm */
	ret = mt6370_pmu_led_change_mode(led_cdev, MT6370_PMU_LED_BREATHMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: mode fix fail\n", __func__);
		goto out_change_mode;
	}
	ret = mt6370_pmu_led_change_mode(led_cdev, MT6370_PMU_LED_PWMMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
		goto out_change_mode;
	}
	return 0;
out_change_mode:
	i = ARRAY_SIZE(mt_led_pwm_mode_attrs);
out_create_file:
	while (--i >= 0)
		device_remove_file(led_cdev->dev, mt_led_pwm_mode_attrs + i);
	return ret;
}

static void mt6370_pmu_led_pwm_deactivate(struct led_classdev *led_cdev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_pwm_mode_attrs); i++)
		device_remove_file(led_cdev->dev, mt_led_pwm_mode_attrs + i);
}

static int mt6370_pmu_led_get_breath_regbase(struct led_classdev *led_cdev)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		ret = MT6370_PMU_REG_RGB1TR;
		break;
	case MT6370_PMU_LED2:
		ret = MT6370_PMU_REG_RGB2TR;
		break;
	case MT6370_PMU_LED3:
		ret = MT6370_PMU_REG_RGB3TR;
		break;
	case MT6370_PMU_LED4:
		ret = MT6370_PMU_REG_RGBCHRINDTR;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static ssize_t mt_led_tr1_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & MT6370_LEDTR1_MASK) >> MT6370_LEDTR1_SHFT;
	return snprintf(buf, cnt, "%d (max 15, 0.125s, step 0.2s)\n", reg_data);
}

static ssize_t mt_led_tr1_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > MT6370_LEDBREATH_MAX)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << MT6370_LEDTR1_SHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LEDTR1_MASK, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_tr2_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & MT6370_LEDTR2_MASK) >> MT6370_LEDTR2_SHFT;
	return snprintf(buf, cnt, "%d (max 15, 0.125s, step 0.2s)\n", reg_data);
}

static ssize_t mt_led_tr2_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 0, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > MT6370_LEDBREATH_MAX)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << MT6370_LEDTR2_SHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LEDTR2_MASK, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_tf1_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & MT6370_LEDTF1_MASK) >> MT6370_LEDTF1_SHFT;
	return snprintf(buf, cnt, "%d (max 15, 0.125s, step 0.2s)\n", reg_data);
}

static ssize_t mt_led_tf1_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > MT6370_LEDBREATH_MAX)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << MT6370_LEDTF1_SHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LEDTF1_MASK, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_tf2_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & MT6370_LEDTF2_MASK) >> MT6370_LEDTF2_SHFT;
	return snprintf(buf, cnt, "%d (max 15, 0.125s, step 0.2s)\n", reg_data);
}

static ssize_t mt_led_tf2_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 1, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > MT6370_LEDBREATH_MAX)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << MT6370_LEDTF2_SHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LEDTF2_MASK, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_ton_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & MT6370_LEDTON_MASK) >> MT6370_LEDTON_SHFT;
	return snprintf(buf, cnt, "%d (max 15, 0.125s, step 0.2s)\n", reg_data);
}

static ssize_t mt_led_ton_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > MT6370_LEDBREATH_MAX)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << MT6370_LEDTON_SHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LEDTON_MASK, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static ssize_t mt_led_toff_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & MT6370_LEDTOFF_MASK) >> MT6370_LEDTOFF_SHFT;
	return snprintf(buf, cnt, "%d (max 15, 0.125s, step 0.2s)\n", reg_data);
}

static ssize_t mt_led_toff_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	uint8_t reg_addr = 2, reg_data = 0;
	unsigned long store = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &store);
	if (ret < 0)
		return ret;
	if (store > MT6370_LEDBREATH_MAX)
		return -EINVAL;
	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;
	reg_addr += ret;
	reg_data = store << MT6370_LEDTOFF_SHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LEDTOFF_MASK, reg_data);
	if (ret < 0)
		return ret;
	return cnt;
}

static const struct device_attribute mt_led_breath_mode_attrs[] = {
	MT_LED_ATTR(tr1),
	MT_LED_ATTR(tr2),
	MT_LED_ATTR(tf1),
	MT_LED_ATTR(tf2),
	MT_LED_ATTR(ton),
	MT_LED_ATTR(toff),
};

static int mt6370_pmu_led_breath_activate(struct led_classdev *led_cdev)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_breath_mode_attrs); i++) {
		ret = device_create_file(led_cdev->dev,
				mt_led_breath_mode_attrs + i);
		if (ret < 0) {
			dev_err(led_cdev->dev,
				"%s: create file fail %d\n", __func__, i);
			goto out_create_file;
		}
	}

	ret = mt6370_pmu_led_change_mode(led_cdev, MT6370_PMU_LED_BREATHMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
		goto out_change_mode;
	}
	return 0;
out_change_mode:
	i = ARRAY_SIZE(mt_led_breath_mode_attrs);
out_create_file:
	while (--i >= 0)
		device_remove_file(led_cdev->dev, mt_led_breath_mode_attrs + i);
	return ret;
}

static void mt6370_pmu_led_breath_deactivate(struct led_classdev *led_cdev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_breath_mode_attrs); i++)
		device_remove_file(led_cdev->dev, mt_led_breath_mode_attrs + i);
}

#ifdef CONFIG_LGE_LEDS
static inline void mt6370_pmu_led_set_pattern_id(struct led_classdev *led_cdev,
	unsigned long id)
{
	struct mt6370_led_classdev *mt_led_cdev =
				(struct mt6370_led_classdev *)led_cdev;

	mt_led_cdev->pattern_id = id;
}

static inline unsigned long mt6370_pmu_led_get_pattern_id(
	struct led_classdev *led_cdev)
{
	struct mt6370_led_classdev *mt_led_cdev =
				(struct mt6370_led_classdev *)led_cdev;

	return mt_led_cdev->pattern_id;
}

static int mt6370_pmu_led_set_enable(struct led_classdev *led_cdev,
	bool enable)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t en_mask = 0;
	bool need_enable_timer = true;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		en_mask = 0x80;
		break;
	case MT6370_PMU_LED2:
		en_mask = 0x40;
		break;
	case MT6370_PMU_LED3:
		en_mask = 0x20;
		break;
	case MT6370_PMU_LED4:
		en_mask = 0x10;
		need_enable_timer = false;
		break;
	default:
		return -EINVAL;
	}

	if (!enable) {
		need_enable_timer = false;
		led_cdev->brightness = LED_OFF;
	}

	if (need_enable_timer) {
		mt6370_pmu_led_enable_dwork(led_cdev);
		return 0;
	}

	ret = mt6370_pmu_led_update_bits(led_cdev, MT6370_PMU_REG_RGBEN,
					 en_mask, enable ? en_mask : ~en_mask);
	if (ret < 0) {
		dev_err(led_cdev->dev, "update enable bit fail\n");
		return ret;
	}

	return 0;
}

static bool mt6370_pmu_led_get_enable(struct led_classdev *led_cdev)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t en_mask = 0;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		en_mask = 0x80;
		break;
	case MT6370_PMU_LED2:
		en_mask = 0x40;
		break;
	case MT6370_PMU_LED3:
		en_mask = 0x20;
		break;
	case MT6370_PMU_LED4:
		en_mask = 0x10;
		break;
	default:
		return false;
	}

	ret = mt6370_pmu_led_reg_read(led_cdev, MT6370_PMU_REG_RGBEN);
	if (ret < 0) {
		dev_err(led_cdev->dev, "read enable fail\n");
		return false;
	}
	if (ret & en_mask)
		return true;

	return false;
}

static int mt6370_pmu_led_set_pwm_dim_freq(struct led_classdev *led_cdev,
	uint8_t freq)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0x38, reg_shift = 3;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = 0x1C;
		reg_shift = 2;
		break;
	default:
		return -EINVAL;
	}

	if (freq > 0x7)
		return -EINVAL;

	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, reg_mask,
					 freq << reg_shift);
	if (ret < 0)
		dev_err(led_cdev->dev, "update pwm freq fail\n");

	return ret;
}

static int mt6370_pmu_led_set_pwm_dim_duty(struct led_classdev *led_cdev,
	uint8_t duty)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_data = 0;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1DIM;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2DIM;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3DIM;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDDIM;
		break;
	default:
		return -EINVAL;
	}

	if (duty > MT6370_LED_PWMDUTYMAX)
		duty = MT6370_LED_PWMDUTYMAX;

	reg_data = duty << MT6370_LED_PWMDUTYSHFT;
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
					 MT6370_LED_PWMDUTYMASK, reg_data);
	if (ret < 0)
		dev_err(led_cdev->dev, "update pwm dim duty fail\n");

	return ret;
}

static int mt6370_pmu_led_get_pwm_dim_duty(struct led_classdev *led_cdev)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1DIM;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2DIM;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3DIM;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDDIM;
		break;
	default:
		return -EINVAL;
	}

	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;

	return (ret & MT6370_LED_PWMDUTYMASK) >> MT6370_LED_PWMDUTYSHFT;
}

static int mt6370_pmu_led_set_isink(struct led_classdev *led_cdev,
	uint8_t isink)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0x7, reg_shift = 0;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = (MT6370_CHRIND_HCUR_EN | 0x3);
		/* set HCUR_EN if needed */
		if (isink & 0x04)
			isink ^= (MT6370_CHRIND_HCUR_EN | 0x04);
		break;
	default:
		return -EINVAL;
	}

	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr, reg_mask,
					 (isink & reg_mask) << reg_shift);
	if (ret < 0)
		dev_err(led_cdev->dev, "update isink fail\n");

	return ret;
}

static int mt6370_pmu_led_get_isink(struct led_classdev *led_cdev)
{
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	uint8_t reg_addr = 0, reg_mask = 0x7, reg_shift = 0;
	int ret = 0;

	switch (led_index) {
	case MT6370_PMU_LED1:
		reg_addr = MT6370_PMU_REG_RGB1ISINK;
		break;
	case MT6370_PMU_LED2:
		reg_addr = MT6370_PMU_REG_RGB2ISINK;
		break;
	case MT6370_PMU_LED3:
		reg_addr = MT6370_PMU_REG_RGB3ISINK;
		break;
	case MT6370_PMU_LED4:
		reg_addr = MT6370_PMU_REG_RGBCHRINDCTRL;
		reg_mask = (MT6370_CHRIND_HCUR_EN | 0x3);
		break;
	default:
		return -EINVAL;
	}

	ret = mt6370_pmu_led_reg_read(led_cdev, reg_addr);
	if (ret < 0)
		return ret;

	if (led_index == MT6370_PMU_LED4) {
		ret &= reg_mask;
		if (ret & MT6370_CHRIND_HCUR_EN)
			ret ^= (MT6370_CHRIND_HCUR_EN | 0x04);
		return ret >> reg_shift;
	}

	return (ret & reg_mask) >> reg_shift;
}

static int __mt6370_pmu_led_set_pattern_blink(struct led_classdev *led_cdev,
	struct mt6370_pmu_blink_data *data)
{
	unsigned long delay_on, delay_off;
	int ret;

	delay_on = data->don;
	delay_off = data->doff;
	ret = mt6370_pmu_led_blink_set(led_cdev, &delay_on, &delay_off);
	if (ret < 0)
		return ret;

	return 0;
}

static int __mt6370_pmu_led_set_pattern_breath(struct led_classdev *led_cdev,
	struct mt6370_pmu_breath_data *data)
{
	uint8_t reg_addr = 0, reg_mask = 0, reg_data = 0;
	int ret = 0;

	ret = mt6370_pmu_led_change_mode(led_cdev, MT6370_PMU_LED_BREATHMODE);
	if (ret < 0)
		return ret;

	ret = mt6370_pmu_led_get_breath_regbase(led_cdev);
	if (ret < 0)
		return ret;

	reg_addr = ret;

	reg_mask = MT6370_LEDTR1_MASK | MT6370_LEDTR2_MASK;
	reg_data = (data->tr1 << MT6370_LEDTR1_SHFT) |
			(data->tr2 << MT6370_LEDTR2_SHFT);
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr,
			reg_mask, reg_data);
	if (ret < 0)
		return ret;

	reg_mask = MT6370_LEDTF1_MASK | MT6370_LEDTF2_MASK;
	reg_data = (data->tf1 << MT6370_LEDTF1_SHFT) |
			(data->tf2 << MT6370_LEDTF2_SHFT);
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr + 1,
			reg_mask, reg_data);
	if (ret < 0)
		return ret;

	reg_mask = MT6370_LEDTON_MASK | MT6370_LEDTOFF_MASK;
	reg_data = (data->ton << MT6370_LEDTON_SHFT) |
			(data->toff << MT6370_LEDTOFF_SHFT);
	ret = mt6370_pmu_led_update_bits(led_cdev, reg_addr + 2,
			reg_mask, reg_data);
	if (ret < 0)
		return ret;

	return 0;
}

static int mt6370_pmu_led_set_pattern(struct led_classdev *led_cdev,
	struct mt6370_pmu_pattern *pattern)
{
	int ret = 0;

	switch (pattern->mode) {
	case MT6370_PMU_LED_PWMMODE:
		ret = __mt6370_pmu_led_set_pattern_blink(led_cdev,
				&pattern->blink);
		break;
	case MT6370_PMU_LED_BREATHMODE:
		ret = __mt6370_pmu_led_set_pattern_breath(led_cdev,
				&pattern->breath);
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set pattern fail\n", __func__);
		return ret;
	}

	mt6370_pmu_led_set_pattern_id(led_cdev, pattern->id);

	return 0;
}

static struct mt6370_pmu_pattern *mt6370_pmu_led_get_pattern(
	struct led_classdev *led_cdev, unsigned long id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(patterns); i++) {
		if (patterns[i].id == id)
			return &patterns[i];
	}

	return &default_pattern;
}

static void mt6370_pmu_led_brightness_set(struct led_classdev *led_cdev,
	enum led_brightness bright)
{
	struct mt6370_led_classdev *mt_led_cdev = container_of(led_cdev,
			struct mt6370_led_classdev, led_dev);
	unsigned long pattern_id = mt6370_pmu_led_get_pattern_id(led_cdev);
	int ret = 0;

	if (pattern_id != LED_PATTERN_STOP && pattern_id != ULONG_MAX) {
		cancel_delayed_work_sync(&mt_led_cdev->pattern_dwork);
		pm_relax(led_cdev->dev);
	}

	/* PWM Frequecy : 1kHz */
	ret = mt6370_pmu_led_set_pwm_dim_freq(led_cdev, 0x07);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set pwm dim freq fail\n",
				__func__);
		return;
	}

	/* Rescale LED_FULL -> MT6370_LED_PWMDUTYMAX */
	if (bright) {
		bright = bright / 8 + 1;
		if (bright > MT6370_LED_PWMDUTYMAX)
			bright = MT6370_LED_PWMDUTYMAX;
	}

	ret = mt6370_pmu_led_set_pwm_dim_duty(led_cdev, bright);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set pwm dim duty fail\n",
				__func__);
		return;
	}

	ret = mt6370_pmu_led_change_mode(led_cdev, MT6370_PMU_LED_PWMMODE);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);
		return;
	}

	ret = mt6370_pmu_led_set_enable(led_cdev, (bright > 0) ? true : false);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set enable fail\n", __func__);
		return;
	}

	mt6370_pmu_led_set_pattern_id(led_cdev,
			(bright > 0) ? ULONG_MAX : LED_PATTERN_STOP);

	return;
}

static enum led_brightness mt6370_pmu_led_brightness_get(
	struct led_classdev *led_cdev)
{
	int ret = 0;

	if (!mt6370_pmu_led_get_enable(led_cdev))
		return LED_OFF;

	/* pattern running, return as FULL */
	if (mt6370_pmu_led_get_pattern_id(led_cdev) != LED_PATTERN_STOP)
		return LED_FULL;

	ret = mt6370_pmu_led_get_pwm_dim_duty(led_cdev);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: get pwm dim duty fail\n",
				__func__);
		return ret;
	}

	if (!ret)
		return ret;

	/* Rescale LED_FULL -> MT6370_LED_PWMDUTYMAX */
	return (ret - 1) * 8;
}

static void mt6370_led_pattern_dwork_func(struct work_struct *work)
{
	struct mt6370_led_classdev *mt_led_cdev = container_of(work,
			struct mt6370_led_classdev, pattern_dwork.work);
	struct led_classdev *led_cdev = &mt_led_cdev->led_dev;
	int ret = 0;

	if (mt6370_pmu_led_get_pattern_id(led_cdev) == LED_PATTERN_STOP)
		return;

	ret = mt6370_pmu_led_set_enable(led_cdev, false);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set enable fail\n",
				__func__);
		return;
	}

	mt6370_pmu_led_set_pattern_id(led_cdev, LED_PATTERN_STOP);
	led_cdev->brightness = LED_OFF;

	dev_info(led_cdev->dev, "%s: pattern stop\n", __func__);

	pm_relax(led_cdev->dev);
}

static ssize_t mt_led_isink_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	unsigned long cnt = PAGE_SIZE;
	int ret = 0;

	ret = mt6370_pmu_led_get_isink(led_cdev);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: get isink fail\n", __func__);
		return ret;
	}

	return snprintf(buf, cnt, "%d\n", ret);
}

static ssize_t mt_led_isink_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int led_index = mt6370_pmu_led_get_index(led_cdev);
	unsigned long bright, bright_max = 6;
	int ret = 0;

	ret = kstrtoul(buf, 10, &bright);
	if (ret < 0)
		return ret;

	if (led_index == MT6370_PMU_LED4)
		bright_max = 7;

	if (bright > bright_max)
		return -EINVAL;

	ret = mt6370_pmu_led_set_isink(led_cdev, bright);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set isink fail\n", __func__);
		return ret;
	}

	return cnt;
}

static ssize_t mt_led_pattern_id_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	unsigned long pattern_id = mt6370_pmu_led_get_pattern_id(led_cdev);
	struct mt6370_pmu_pattern *pattern = NULL;
	unsigned long cnt = PAGE_SIZE;

	if (pattern_id == LED_PATTERN_STOP)
		return snprintf(buf, cnt, "%lu (%s)\n", pattern_id, "stop");

	if (pattern_id == ULONG_MAX)
		return snprintf(buf, cnt, "N/A (%s)\n", "custom");

	pattern = mt6370_pmu_led_get_pattern(led_cdev, pattern_id);
	if (!pattern)
		return snprintf(buf, cnt, "%lu (%s)\n", pattern_id, "none");

	return snprintf(buf, cnt, "%lu (%s)\n", pattern->id, pattern->name);
}

static ssize_t mt_led_pattern_id_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt6370_led_classdev *mt_led_cdev = container_of(led_cdev,
			struct mt6370_led_classdev, led_dev);
	struct mt6370_pmu_pattern *pattern = NULL;
	unsigned long pattern_id = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &pattern_id);
	if (ret < 0)
		return ret;

	if (mt6370_pmu_led_get_pattern_id(led_cdev) == pattern_id) {
		if (pattern_id == LED_PATTERN_STOP)
			return cnt;

		cancel_delayed_work_sync(&mt_led_cdev->pattern_dwork);
		pm_relax(led_cdev->dev);

		if (mt6370_pmu_led_get_pattern_id(led_cdev) == LED_PATTERN_STOP)
			goto start_pattern;

		return cnt;
	}

	if (pattern_id == LED_PATTERN_STOP) {
		pm_stay_awake(led_cdev->dev);
		schedule_delayed_work(&mt_led_cdev->pattern_dwork,
				0);
		return cnt;
	}

	cancel_delayed_work_sync(&mt_led_cdev->pattern_dwork);
	pm_relax(led_cdev->dev);

start_pattern:
	pattern = mt6370_pmu_led_get_pattern(led_cdev, pattern_id);
	if (!pattern) {
		dev_err(led_cdev->dev, "%s: find pattern fail\n", __func__);
		return -EINVAL;
	}

	ret = mt6370_pmu_led_set_pattern(led_cdev, pattern);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set pattern fail\n", __func__);
		return ret;
	}

	dev_info(led_cdev->dev, "%s: pattern %s enabled\n", __func__,
			pattern->name);

	mt6370_pmu_led_set_pattern_id(led_cdev, pattern_id);
	led_cdev->brightness = LED_FULL;

	ret = mt6370_pmu_led_set_enable(led_cdev, true);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set enable fail\n", __func__);
		return ret;
	}

	return cnt;
}

static ssize_t mt_led_blink_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mt_led_blink_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt6370_led_classdev *mt_led_cdev = container_of(led_cdev,
			struct mt6370_led_classdev, led_dev);
	unsigned long pattern_id = mt6370_pmu_led_get_pattern_id(led_cdev);
	unsigned int rgb = 0;
	unsigned long delay_on = 0, delay_off = 0;
	int ret = 0;

	ret = sscanf(buf, "0x%06x %lu %lu", &rgb, &delay_on, &delay_off);
	if (ret < 3)
		return -EINVAL;

	dev_info(led_cdev->dev, "%s: rgb = %x delay on/off = (%lu, %lu)\n",
			__func__, rgb, delay_on, delay_off);

	if (pattern_id != LED_PATTERN_STOP && pattern_id != ULONG_MAX) {
		cancel_delayed_work_sync(&mt_led_cdev->pattern_dwork);
		pm_relax(led_cdev->dev);
	}

	ret = mt6370_pmu_led_blink_set(led_cdev, &delay_on, &delay_off);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: blink set fail\n", __func__);
		return ret;
	}

	mt6370_pmu_led_set_pattern_id(led_cdev, ULONG_MAX);
	if (rgb & 0xffffff)
		led_cdev->brightness = LED_FULL;

	ret = mt6370_pmu_led_set_enable(led_cdev,
			(rgb & 0xffffff) ? true : false);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set enable fail\n", __func__);
		return ret;
	}

	return cnt;
}

static const struct device_attribute mt_led_lg_notifications_attrs[] = {
	MT_LED_ATTR(isink),
	MT_LED_ATTR(pattern_id),
	MT_LED_ATTR(blink),
};

static int mt6370_pmu_led_lg_notifications_activate(struct led_classdev *led_cdev)
{
	struct mt6370_led_classdev *mt_led_cdev = container_of(led_cdev,
			struct mt6370_led_classdev, led_dev);
	struct mt6370_pmu_pattern *pattern = NULL;
	int ret = 0;

	if (led_cdev->activated)
		return ret; //need to fix

	ret = mt6370_pmu_led_set_isink(led_cdev, mt_led_cdev->default_isink);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: set default isink fail\n", __func__);
		goto done_change_mode;
	}

	if (system_state == SYSTEM_BOOTING || system_state == SYSTEM_SCHEDULING) {
#ifdef CONFIG_MTK_BOOT
		if (get_boot_mode() != NORMAL_BOOT) {
			ret = mt6370_pmu_led_change_mode(led_cdev, MT6370_PMU_LED_BREATHMODE);
			if (ret < 0)
				dev_err(led_cdev->dev, "%s: change mode fail\n", __func__);

			ret = mt6370_pmu_led_set_enable(led_cdev, false);
			if (ret < 0)
				dev_err(led_cdev->dev, "%s: set enable fail\n", __func__);

			goto done_change_mode;
		}
#endif
		pattern = mt6370_pmu_led_get_pattern(led_cdev, LED_PATTERN_POWER_ON);
		if (!pattern)
			goto done_change_mode;

		ret = mt6370_pmu_led_set_pattern(led_cdev, pattern);
		if (ret < 0) {
			dev_err(led_cdev->dev, "%s: set pattern fail\n", __func__);
			goto done_change_mode;
		}

		mt6370_pmu_led_set_pattern_id(led_cdev, LED_PATTERN_POWER_ON);
		led_cdev->brightness = LED_FULL;

		ret = mt6370_pmu_led_set_enable(led_cdev, true);
		if (ret < 0) {
			dev_err(led_cdev->dev, "%s: set enable fail\n", __func__);
			goto done_change_mode;
		}
	}

done_change_mode:
	led_cdev->activated = true;

	return 0;
}

static void mt6370_pmu_led_lg_notifications_deactivate(struct led_classdev *led_cdev)
{
	if (!led_cdev->activated)
		return;

	led_cdev->activated = false;
}

static void led_lg_notifications_register(struct led_classdev *led_cdev)
{
	int i;
	int ret;

	if (!led_cdev->default_trigger)
		return;
	if (strcmp(led_cdev->default_trigger, "lg_notifications"))
		return;

	led_cdev->max_brightness = LED_FULL;
	led_cdev->brightness_set = mt6370_pmu_led_brightness_set;
	led_cdev->brightness_get = mt6370_pmu_led_brightness_get;

	for (i = 0; i < ARRAY_SIZE(mt_led_lg_notifications_attrs); i++) {
		ret = device_create_file(led_cdev->dev,
				mt_led_lg_notifications_attrs + i);
		if (ret < 0) {
			dev_err(led_cdev->dev,
				"%s: create file fail %d\n", __func__, i);
		}
	}
}
#endif

static struct led_trigger mt6370_pmu_led_trigger[] = {
	{
		.name = "cc_mode",
		.activate = mt6370_pmu_led_cc_activate,
		.deactivate = mt6370_pmu_led_cc_deactivate,
	},
	{
		.name = "pwm_mode",
		.activate = mt6370_pmu_led_pwm_activate,
		.deactivate = mt6370_pmu_led_pwm_deactivate,
	},
	{
		.name = "breath_mode",
		.activate = mt6370_pmu_led_breath_activate,
		.deactivate = mt6370_pmu_led_breath_deactivate,
	},
#ifdef CONFIG_LGE_LEDS
	{
		.name = "lg_notifications",
		.activate = mt6370_pmu_led_lg_notifications_activate,
		.deactivate = mt6370_pmu_led_lg_notifications_deactivate,
	},
#endif
};

static irqreturn_t mt6370_pmu_isink4_short_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_isink3_short_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_isink2_short_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_isink1_short_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

#ifndef CONFIG_LGE_LEDS
static irqreturn_t mt6370_pmu_isink4_open_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}
#endif

static irqreturn_t mt6370_pmu_isink3_open_irq_handler(int irq, void *data)

{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_isink2_open_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_isink1_open_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = data;

	dev_notice(rgbled_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static struct mt6370_pmu_irq_desc mt6370_rgbled_irq_desc[] = {
	MT6370_PMU_IRQDESC(isink4_short),
	MT6370_PMU_IRQDESC(isink3_short),
	MT6370_PMU_IRQDESC(isink2_short),
	MT6370_PMU_IRQDESC(isink1_short),
#ifndef CONFIG_LGE_LEDS
	MT6370_PMU_IRQDESC(isink4_open),
#endif
	MT6370_PMU_IRQDESC(isink3_open),
	MT6370_PMU_IRQDESC(isink2_open),
	MT6370_PMU_IRQDESC(isink1_open),
};

static void mt6370_pmu_rgbled_irq_register(struct platform_device *pdev)
{
	struct resource *res;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt6370_rgbled_irq_desc); i++) {
		if (!mt6370_rgbled_irq_desc[i].name)
			continue;
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						mt6370_rgbled_irq_desc[i].name);
		if (!res)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, res->start, NULL,
					mt6370_rgbled_irq_desc[i].irq_handler,
					IRQF_TRIGGER_FALLING,
					mt6370_rgbled_irq_desc[i].name,
					platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev, "request %s irq fail\n", res->name);
			continue;
		}
		mt6370_rgbled_irq_desc[i].irq = res->start;
	}
}

static void mt6370_led_enable_dwork_func(struct work_struct *work)
{
	struct mt6370_pmu_rgbled_data *rgbled_data =
		container_of(work, struct mt6370_pmu_rgbled_data, dwork.work);
	u8 chip_vid = rgbled_data->chip->chip_vid;
	u8 reg_data = 0, reg_mask = 0xE0, reg_addr = MT6370_PMU_REG_RGBEN;
	int ret = 0;

	dev_dbg(rgbled_data->dev, "%s\n", __func__);
	/* red */
	if (mt6370_led_classdev[0].led_dev.brightness != 0)
		reg_data |= 0x80;
	/* blue */
	if (mt6370_led_classdev[1].led_dev.brightness != 0)
		reg_data |= 0x40;
	/* green */
	if (mt6370_led_classdev[2].led_dev.brightness != 0)
		reg_data |= 0x20;
	/* if 6372 */
	if (chip_vid == MT6372_VENDOR_ID || chip_vid == MT6372C_VENDOR_ID)
		reg_addr = 0x82;
	ret =  mt6370_pmu_reg_update_bits(rgbled_data->chip,
					  reg_addr,
					  reg_mask, reg_data);
	if (ret < 0)
		dev_err(rgbled_data->dev, "timer update enable bit fail\n");
}

static inline int mt6370_pmu_rgbled_init_register(
	struct mt6370_pmu_rgbled_data *rgbled_data)
{
	const u8 *init_data = rgbled_init_data;
	int init_data_size = ARRAY_SIZE(rgbled_init_data);
	u8 chip_vid = rgbled_data->chip->chip_vid;

	if (chip_vid == MT6372_VENDOR_ID || chip_vid == MT6372C_VENDOR_ID) {
		init_data = rgbled_init_data2;
		init_data_size = ARRAY_SIZE(rgbled_init_data2);
	}
	return mt6370_pmu_reg_block_write(rgbled_data->chip,
					  MT6370_PMU_REG_RGB1DIM,
					  init_data_size,
					  init_data);
}

static inline int mt6370_pmu_rgbled_parse_initdata(
	struct mt6370_pmu_rgbled_data *rgbled_data)
{
	return 0;
}

static inline int mt_parse_dt(struct device *dev)
{
	struct mt6370_pmu_rgbled_platdata *pdata = dev_get_platdata(dev);
	int name_cnt = 0, trigger_cnt = 0;
#ifdef CONFIG_LGE_LEDS
	int isink_cnt = 0;
#endif
	struct device_node *np = dev->of_node;
	int ret = 0;

	while (true) {
		const char *name = NULL;

		ret = of_property_read_string_index(np, "mt,led_name",
						    name_cnt, &name);
		if (ret < 0)
			break;
		pdata->led_name[name_cnt] = name;
		name_cnt++;
	}
	while (true) {
		const char *name = NULL;

		ret = of_property_read_string_index(np,
						    "mt,led_default_trigger",
						    trigger_cnt, &name);
		if (ret < 0)
			break;
		pdata->led_default_trigger[trigger_cnt] = name;
		trigger_cnt++;
	}
	if (name_cnt != MT6370_PMU_MAXLED || trigger_cnt != MT6370_PMU_MAXLED)
		return -EINVAL;
#ifdef CONFIG_LGE_LEDS
	while (true) {
		uint32_t isink;

		ret = of_property_read_u32_index(np,
						 "lge,led_default_isink",
						 isink_cnt, &isink);
		if (ret < 0)
			break;
		pdata->led_default_isink[isink_cnt] = isink;
		isink_cnt++;
	}
	/* set minimum if device-tree not found */
	if (isink_cnt != MT6370_PMU_MAXLED) {
		memset(pdata->led_default_isink, 0x0,
				sizeof(pdata->led_default_isink));
	}
#endif
	return 0;
}

static int mt6370_pmu_rgbled_probe(struct platform_device *pdev)
{
	struct mt6370_pmu_rgbled_platdata *pdata =
					dev_get_platdata(&pdev->dev);
	struct mt6370_pmu_rgbled_data *rgbled_data;
	bool use_dt = pdev->dev.of_node, new = false;
	int i = 0, ret = 0;

	pr_info("%s: (%s)\n", __func__, MT6370_PMU_RGBLED_DRV_VERSION);

	rgbled_data = devm_kzalloc(&pdev->dev,
				   sizeof(*rgbled_data), GFP_KERNEL);
	if (!rgbled_data)
		return -ENOMEM;
	if (use_dt) {
		/* DTS used */
		pdata = devm_kzalloc(&pdev->dev,
				     sizeof(*rgbled_data), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto out_pdata;
		}
		pdev->dev.platform_data = pdata;
		ret = mt_parse_dt(&pdev->dev);
		if (ret < 0) {
			devm_kfree(&pdev->dev, pdata);
			goto out_pdata;
		}
	} else {
		if (!pdata) {
			ret = -EINVAL;
			goto out_pdata;
		}
	}
	rgbled_data->chip = dev_get_drvdata(pdev->dev.parent);
	rgbled_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, rgbled_data);
	INIT_DELAYED_WORK(&rgbled_data->dwork, mt6370_led_enable_dwork_func);

	if (rgbled_data->chip->chip_vid == MT6372_VENDOR_ID ||
	    rgbled_data->chip->chip_vid == MT6372C_VENDOR_ID)
		new = true;

	ret = mt6370_pmu_rgbled_parse_initdata(rgbled_data);
	if (ret < 0)
		goto out_init_data;

	ret = mt6370_pmu_rgbled_init_register(rgbled_data);
	if (ret < 0)
		goto out_init_reg;

#ifdef CONFIG_LGE_LEDS
	for (i = 0; i < ARRAY_SIZE(mt6370_led_classdev); i++) {
		mt6370_led_classdev[i].default_isink =
				pdata->led_default_isink[i];
		INIT_DELAYED_WORK(&mt6370_led_classdev[i].pattern_dwork,
				mt6370_led_pattern_dwork_func);
	}
#endif

	for (i = 0; i < ARRAY_SIZE(mt6370_pmu_led_trigger) && !new; i++) {
		ret = led_trigger_register(&mt6370_pmu_led_trigger[i]);
		if (ret < 0) {
			dev_err(&pdev->dev, "register %d trigger fail\n", i);
			goto out_led_trigger;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mt6370_pmu_led_trigger2) && new; i++) {
		ret = led_trigger_register(&mt6370_pmu_led_trigger2[i]);
		if (ret < 0) {
			dev_err(&pdev->dev, "register %d trigger fail\n", i);
			goto out_led_trigger;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mt6370_led_classdev) && !new; i++) {
		mt6370_led_classdev[i].led_dev.name = pdata->led_name[i];
		mt6370_led_classdev[i].led_dev.default_trigger =
						pdata->led_default_trigger[i];
		ret = led_classdev_register(&pdev->dev,
					    &mt6370_led_classdev[i].led_dev);
		if (ret < 0) {
			dev_err(&pdev->dev, "register led %d fail\n", i);
			goto out_led_register;
		}
#ifdef CONFIG_LGE_LEDS
		led_lg_notifications_register(&mt6370_led_classdev[i].led_dev);
#endif
	}

	for (i = 0; i < ARRAY_SIZE(mt6370_led_classdev2) && new; i++) {
		mt6370_led_classdev2[i].led_dev.name = pdata->led_name[i];
		mt6370_led_classdev2[i].led_dev.default_trigger =
						pdata->led_default_trigger[i];
		ret = led_classdev_register(&pdev->dev,
					    &mt6370_led_classdev2[i].led_dev);
		if (ret < 0) {
			dev_err(&pdev->dev, "register led %d fail\n", i);
			goto out_led_register;
		}
	}
	mt6370_pmu_rgbled_irq_register(pdev);
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
out_led_register:
	while (!new && --i >= 0)
		led_classdev_unregister(&mt6370_led_classdev[i].led_dev);
	while (new && --i >= 0)
		led_classdev_unregister(&mt6370_led_classdev2[i].led_dev);
	if (!new)
		i = ARRAY_SIZE(mt6370_pmu_led_trigger);
	else
		i = ARRAY_SIZE(mt6370_pmu_led_trigger2);
out_led_trigger:
	while (!new && --i >= 0)
		led_trigger_register(&mt6370_pmu_led_trigger[i]);
	while (new && --i >= 0)
		led_trigger_register(&mt6370_pmu_led_trigger2[i]);
out_init_reg:
out_init_data:
out_pdata:
	devm_kfree(&pdev->dev, rgbled_data);
	return ret;
}

static int mt6370_pmu_rgbled_remove(struct platform_device *pdev)
{
	struct mt6370_pmu_rgbled_data *rgbled_data = platform_get_drvdata(pdev);
	u8 chip_vid = rgbled_data->chip->chip_vid;
	bool new = false;
	int i = 0;

	if (chip_vid == MT6372_VENDOR_ID || chip_vid == MT6372C_VENDOR_ID)
		new = true;
	for (i = 0; i < ARRAY_SIZE(mt6370_led_classdev) && !new; i++)
		led_classdev_unregister(&mt6370_led_classdev[i].led_dev);
	for (i = 0; i < ARRAY_SIZE(mt6370_led_classdev2) && new; i++)
		led_classdev_unregister(&mt6370_led_classdev2[i].led_dev);
	for (i = 0; i < ARRAY_SIZE(mt6370_pmu_led_trigger) && !new; i++)
		led_trigger_register(&mt6370_pmu_led_trigger[i]);
	for (i = 0; i < ARRAY_SIZE(mt6370_pmu_led_trigger2) && new; i++)
		led_trigger_register(&mt6370_pmu_led_trigger2[i]);
	dev_info(rgbled_data->dev, "%s successfully\n", __func__);
	return 0;
}

static const struct of_device_id mt_ofid_table[] = {
	{ .compatible = "mediatek,mt6370_pmu_rgbled", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt_ofid_table);

static const struct platform_device_id mt_id_table[] = {
	{ "mt6370_pmu_rgbled", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, mt_id_table);

static struct platform_driver mt6370_pmu_rgbled = {
	.driver = {
		.name = "mt6370_pmu_rgbled",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt_ofid_table),
	},
	.probe = mt6370_pmu_rgbled_probe,
	.remove = mt6370_pmu_rgbled_remove,
	.id_table = mt_id_table,
};
module_platform_driver(mt6370_pmu_rgbled);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT6370 PMU RGBled");
MODULE_VERSION(MT6370_PMU_RGBLED_DRV_VERSION);

/*
 * Release Note
 * 1.0.1_MTK
 * (1) Add support for MT6372
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */
