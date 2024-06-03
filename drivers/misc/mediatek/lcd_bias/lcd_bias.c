/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <soc/mediatek/lge/board_lge.h>

#include "lcd_bias.h"
#include "touch_core.h"
#include "touch_hwif.h"

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_client *lcd_bias_i2c_client;
static struct pinctrl *lcd_bias_pctrl; /* static pinctrl instance */
//static unsigned int touch_rst;
#if defined(CONFIG_LGE_DSV_DUALIZE)
static int dsv_id = 0;
#endif
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int lcd_bias_dts_probe(struct platform_device *pdev);
static int lcd_bias_dts_remove(struct platform_device *pdev);
static int lcd_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lcd_bias_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Extern Area
 *****************************************************************************/
static int lcd_bias_write_byte(unsigned char addr, unsigned char value)
{
    int ret = 0;
    unsigned char write_data[2] = {0};

    write_data[0] = addr;
    write_data[1] = value;

    if (NULL == lcd_bias_i2c_client) {
        LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_i2c_client is null!!\n");
        return -1;
    }
    ret = i2c_master_send(lcd_bias_i2c_client, write_data, 2);

    if (ret < 0)
        LCD_BIAS_PRINT("[LCD][BIAS] i2c write data fail !!\n");

    return ret;
}
int lcd_bias_set_vspn(unsigned int value)
{
    unsigned char level;

    if ((value <= 4000) || (value > 6500)) {
        LCD_BIAS_PRINT("[LCD][BIAS] unreasonable voltage value\n");
        return -1;
    }

    level = (value - 4000)/100;  //eg.  5.0V= 4.0V + Hex 0x0A (Bin 0 1010) * 100mV

    lcd_bias_write_byte(LCD_BIAS_VNEG_ADDR, level);
    lcd_bias_write_byte(LCD_BIAS_VPOS_ADDR, level);
    lcd_bias_write_byte(LCD_BIAS_APPS_ADDR, NEG_OUTPUT_APPS);//change max NEG output from default 40mA to 80mA

    return 0;
}

int lcd_bias_power_off_vspn(void)
{
    lcd_bias_write_byte(LCD_BIAS_APPS_ADDR, NEG_OUTPUT_APPS); //set Smartphone mode, enable active-discharge

    return 0;
}

void lcd_bias_set_gpio_ctrl(unsigned int ctrl_pin, unsigned int en)
{
    if (ctrl_pin == LCM_RST) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_LCM_RST1);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_LCM_RST0);
        }
    }
    else if (ctrl_pin == LCD_LDO_EN) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_LCD_LDO_EN1);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_LCD_LDO_EN0);
        }
    }
    else if (ctrl_pin == DSV_VPOS_EN) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_DSV_VPOS_EN1);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_DSV_VPOS_EN0);
        }
    }
    else if (ctrl_pin == DSV_VNEG_EN) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_DSV_VNEG_EN1);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_DSV_VNEG_EN0);
        }
    }
/*
	else if (ctrl_pin == TOUCH_RST) {
        if (en) {
            touch_gpio_direction_output(touch_rst, 1);
        } else {
            touch_gpio_direction_output(touch_rst, 0);
        }
    }
*/
}

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
static const char *lcd_bias_state_name[LCD_BIAS_GPIO_STATE_MAX] = {
    "lcd_bias_gpio_lcm_rst0",
    "lcd_bias_gpio_lcm_rst1",
    "lcd_bias_lcd_ldo_en0",
    "lcd_bias_lcd_ldo_en1",
    "lcd_bias_gpio_vpos_en0",
    "lcd_bias_gpio_vpos_en1",
    "lcd_bias_gpio_vneg_en0",
    "lcd_bias_gpio_vneg_en1",
};/* DTS state mapping name */

#ifdef CONFIG_OF
static const struct of_device_id gpio_of_match[] = {
    { .compatible = "mediatek,gpio_lcd_bias", },
    {},
};

static const struct of_device_id i2c_of_match[] = {
    { .compatible = "mediatek,i2c_lcd_bias", },
    {},
};
#endif

static const struct i2c_device_id lcd_bias_i2c_id[] = {
    {LCD_BIAS_I2C_ID_NAME, 0},
    {},
};

static struct platform_driver lcd_bias_platform_driver = {
    .probe = lcd_bias_dts_probe,
    .remove = lcd_bias_dts_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = LCD_BIAS_DTS_ID_NAME,
#ifdef CONFIG_OF
        .of_match_table = gpio_of_match,
#endif
    },
};

static struct i2c_driver lcd_bias_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
    .id_table = lcd_bias_i2c_id,
    .probe = lcd_bias_i2c_probe,
    .remove = lcd_bias_i2c_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = LCD_BIAS_I2C_ID_NAME,
#ifdef CONFIG_OF
        .of_match_table = i2c_of_match,
#endif
    },
};

/*****************************************************************************
 * Function
 *****************************************************************************/
static int lcd_bias_set_state(const char *name)
{
    int ret = 0;
    struct pinctrl_state *pState = 0;

    if (!lcd_bias_pctrl) {
	pr_info("this pctrl is null\n");
	return -1;
    }

    pState = pinctrl_lookup_state(lcd_bias_pctrl, name);
    if (IS_ERR(pState)) {
        pr_err("set state '%s' failed\n", name);
        ret = PTR_ERR(pState);
        goto exit;
    }

    /* select state! */
    pinctrl_select_state(lcd_bias_pctrl, pState);

exit:
    return ret; /* Good! */
}

void lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE s)
{
    BUG_ON(!((unsigned int)(s) < (unsigned int)(LCD_BIAS_GPIO_STATE_MAX)));

	if( s < LCD_BIAS_GPIO_STATE_MAX )
		lcd_bias_set_state(lcd_bias_state_name[s]);
	else
		LCD_BIAS_PRINT("[LCD][BIAS] unsupported gpio state!!\n");
}

static int lcd_bias_dts_init(struct platform_device *pdev)
{
    int ret = 0;
    struct pinctrl *pctrl;
	struct device_node *np = NULL;

    /* retrieve */
    pctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(pctrl)) {
        dev_err(&pdev->dev, "Cannot find disp pinctrl!");
        ret = PTR_ERR(pctrl);
        goto exit;
    }

    lcd_bias_pctrl = pctrl;

	np = of_find_compatible_node(NULL, NULL, LCD_BIAS_DTS_GPIO_NODE);

	if (!np) {
		dev_err(&pdev->dev, "[%s] DT node: Not found\n", __func__);
		ret = -1;
		goto exit;
	}
	else{
		//PROPERTY_GPIO(np, "touch-reset-gpio", touch_rst);
		//LCD_BIAS_PRINT("[LCD][BIAS] touch-reset-gpio = %d\n", touch_rst);
	}

exit:
    return ret;
}

#if defined(CONFIG_LGE_DSV_DUALIZE)
static void lcd_bias_set_dsv_id(void)
{
    dsv_id = lge_get_dsv_id();
    if (dsv_id != 0 && dsv_id != 1 ) {
		LCD_BIAS_PRINT("[LCD] abnormal dsv_id in cmdline! set as default\n");
		dsv_id = 0;
    }
    LCD_BIAS_PRINT("[LCD][BIAS] dsv setup complete = %s\n", (dsv_id==0) ? "SM5109" : "TPS65132");
}

#define S_IRWUGO (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
ssize_t dsv_id_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
        int r = 0;
        r = snprintf(buf, PAGE_SIZE, "%s\n", (dsv_id==0) ? "SM5109" : "TPS65132");
        return r;
}
static DEVICE_ATTR(dsv_id, S_IRWUGO, dsv_id_show, NULL);
#endif

static struct attribute* lcd_bias_attrs[] = {
#if defined(CONFIG_LGE_DSV_DUALIZE)
    &dev_attr_dsv_id.attr,
#endif
    NULL,
};

static struct attribute_group lcd_bias_attr_group = {
    .attrs = lcd_bias_attrs,
};

static int lcd_bias_dts_probe(struct platform_device *pdev)
{
    int ret = 0;

    lcd_bias_dts_init(pdev);
#if defined(CONFIG_LGE_DSV_DUALIZE)
    lcd_bias_set_dsv_id();
#endif
    ret = sysfs_create_group(&pdev->dev.kobj, &lcd_bias_attr_group);
    if(ret)
	LCD_BIAS_PRINT("[LCD][BIAS] sysfs_create_group failed : %d\n", ret);

    LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_dts_probe success\n");

    return 0;
}

static int lcd_bias_dts_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &lcd_bias_attr_group);
    platform_driver_unregister(&lcd_bias_platform_driver);

    return 0;
}

static int lcd_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    if (NULL == client) {
        LCD_BIAS_PRINT("[LCD][BIAS] i2c_client is NULL\n");
        return -1;
    }

    lcd_bias_i2c_client = client;
    LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_i2c_probe success addr = 0x%x\n", client->addr);

    return 0;
}

static int lcd_bias_i2c_remove(struct i2c_client *client)
{
    lcd_bias_i2c_client = NULL;
    i2c_unregister_device(client);

    return 0;
}

static int __init lcd_bias_init(void)
{
#if defined(CONFIG_MACH_MT6762_MH4X) || defined(CONFIG_MACH_MT6762_MH4)
	if( lge_get_board_revno() >= HW_REV_A ){
#endif
	    if (i2c_add_driver(&lcd_bias_i2c_driver)) {
	        LCD_BIAS_PRINT("[LCD][BIAS] Failed to register lcd_bias_i2c_driver!\n");
	        return -1;
	    }
#if defined(CONFIG_MACH_MT6762_MH4X) || defined(CONFIG_MACH_MT6762_MH4)
	}
#endif

    if (platform_driver_register(&lcd_bias_platform_driver)) {
        LCD_BIAS_PRINT("[LCD][BIAS] Failed to register lcd_bias_platform_driver!\n");
        return -1;
    }
    LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_init success\n");

    return 0;
}

static void __exit lcd_bias_exit(void)
{
    platform_driver_unregister(&lcd_bias_platform_driver);
	if( lge_get_board_revno() >= HW_REV_A ){
    	i2c_del_driver(&lcd_bias_i2c_driver);
	}
}

module_init(lcd_bias_init);
module_exit(lcd_bias_exit);

MODULE_AUTHOR("Oly Peng <penghoubing@huaqin.com>");
MODULE_DESCRIPTION("MTK LCD BIAS Driver");
MODULE_LICENSE("GPL");
