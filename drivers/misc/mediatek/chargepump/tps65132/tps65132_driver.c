/*
 * Copyright (C) 2019 LG Electronics corp.
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
#include "tps65132.h"

enum TPS65132 {
	POSCTRL = 0,
	NEGCTRL,
	CONTROL,
};

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_client *tps65132_i2c_client;
static unsigned char dsv_data[3] = {0x0F,0x0F,0x03};

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int tps65132_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tps65132_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Extern Area
 *****************************************************************************/
static int tps65132_write_byte(unsigned char addr, unsigned char value)
{
    int ret = 0;
    unsigned char write_data[2] = {0};

    write_data[0] = addr;
    write_data[1] = value;

    if (NULL == tps65132_i2c_client) {
        TPS65132_PRINT("[LCD][TPS65132] tps65132_i2c_client is null!!\n");
        return -1;
    }
    ret = i2c_master_send(tps65132_i2c_client, write_data, 2);

    if (ret < 0)
        TPS65132_PRINT("[LCD][TPS65132] i2c write data fail !!\n");

    return ret;
}

static int tps65132_read_byte(unsigned char addr, unsigned char *value)
{
    int ret = 0;

    if (NULL == tps65132_i2c_client) {
        TPS65132_PRINT("[LCD][TPS] tps65132_i2c_client is null!!\n");
        return -1;
    }
    ret = i2c_smbus_read_i2c_block_data(tps65132_i2c_client, addr, 1, value);

    if (ret < 0)
        TPS65132_PRINT("[LCD][TPS] i2c write data fail !!\n");

    return ret;
}

int tps65132_set_vspn(void)
{
    tps65132_write_byte(TPS65132_VPOS_ADDR, dsv_data[POSCTRL]);
    mdelay(2);
    tps65132_write_byte(TPS65132_VNEG_ADDR, dsv_data[NEGCTRL]);
    mdelay(2);
    tps65132_write_byte(TPS65132_APPS_ADDR, dsv_data[CONTROL]);

    TPS65132_PRINT("[LCD][TPS] POSCTRL = 0x%x, NEGCTRL = 0x%x, APPS = 0x%x\n", dsv_data[POSCTRL], dsv_data[NEGCTRL], dsv_data[CONTROL]);

    return 0;
}

int tps65132_power_off_vspn(void)
{
  //tps65132_write_byte(SM5109_APPS_ADDR, NEG_OUTPUT_APPS); //set Smartphone mode, enable active-discharge

    return 0;
}

static const struct of_device_id i2c_of_match[] = {
    { .compatible = "mediatek,i2c_tps65132", },
    {},
};

static const struct i2c_device_id tps65132_i2c_id[] = {
    {TPS65132_I2C_ID_NAME, 0},
    {},
};

static struct i2c_driver tps65132_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
    .id_table = tps65132_i2c_id,
    .probe = tps65132_i2c_probe,
    .remove = tps65132_i2c_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = TPS65132_I2C_ID_NAME,
#ifdef CONFIG_OF
        .of_match_table = i2c_of_match,
#endif
    },
};

static int tps65132_parse_dt(void)
{
    struct device_node *np = NULL;
    int ret = 0;
    u32 tmp = 0;

    np = of_find_compatible_node(NULL, NULL, TPS65132_DTS_NODE);

    if (!np) {
	TPS65132_PRINT("[%s] DT node: Not found\n", __func__);
	ret = -1;
	goto exit;
    } else {
	if (of_property_read_u32(np, "vpos", &tmp) < 0)
		dsv_data[POSCTRL] |= 10; //default 5V for VPOS
	else
		dsv_data[POSCTRL] |= (tmp - 4000) / 100;

    if (of_property_read_u32(np, "vneg", &tmp) < 0)
		dsv_data[NEGCTRL] |= 10; //default -5V for VNEG
	else
		dsv_data[NEGCTRL] |= (tmp - 4000) / 100;

    if (of_property_read_u32(np, "apps", &tmp) < 0)
		dsv_data[CONTROL] |= 0 << 6; //default target for smartphone
	else
		dsv_data[CONTROL] |= tmp << 6;

    if (of_property_read_u32(np, "active_discharge", &tmp) < 0)
		dsv_data[CONTROL] |= 3; //default active discharge enable
	else
		dsv_data[CONTROL] |= tmp;

	TPS65132_PRINT("[LCD][TPS] POSCTRL = 0x%x, NEGCTRL = 0x%x, CONTROL = 0x%x\n", dsv_data[POSCTRL], dsv_data[NEGCTRL], dsv_data[CONTROL]);
    }
    return 0;
exit:
    return ret;
}
static ssize_t sysfs_show_dsv(struct device *dev, struct device_attribute *attr, char *buf)
{
    int n = 0;
    int err = 0;
    u8 data = 0;

    err = tps65132_read_byte(TPS65132_VPOS_ADDR, &data);
    n +=  sprintf(buf+n, "VPOS = 0x%02x\n",data);
    err = tps65132_read_byte(TPS65132_VNEG_ADDR, &data);
    n +=  sprintf(buf+n, "VNEG = 0x%02x\n",data);
    err = tps65132_read_byte(TPS65132_APPS_ADDR, &data);
    n +=  sprintf(buf+n, "APPS = 0x%02x\n",data);
    return n;
}

DEVICE_ATTR(tps65132_reg, 0444, sysfs_show_dsv, NULL);

static int tps65132_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;

    if (NULL == client) {
        TPS65132_PRINT("[LCD][TPS] i2c_client is NULL\n");
        return -1;
    }

    tps65132_i2c_client = client;

    ret = tps65132_parse_dt();

    if(!ret)
        TPS65132_PRINT("[LCD][TPS] TPS65132_i2c_probe success addr = 0x%x\n", client->addr);
    else
        TPS65132_PRINT("[LCD][TPS] TPS65132_i2c_probe failed addr = 0x%x\n", client->addr);

    device_create_file(&client->dev, &dev_attr_tps65132_reg);

    return 0;
}

static int tps65132_i2c_remove(struct i2c_client *client)
{
    tps65132_i2c_client = NULL;
    i2c_unregister_device(client);

    return 0;
}

static int __init tps65132_init(void)
{
    if (i2c_add_driver(&tps65132_i2c_driver)) {
        TPS65132_PRINT("[LCD][TPS] Failed to register tps65132_i2c_driver!\n");
        return -1;
    }

    TPS65132_PRINT("[LCD][TPS] tps65132_init success\n");

    return 0;
}

static void __exit tps65132_exit(void)
{
	i2c_del_driver(&tps65132_i2c_driver);
	TPS65132_PRINT("[LCD][TPS] unregister tps65132_i2c_driver!\n");
}

module_init(tps65132_init);
module_exit(tps65132_exit);

MODULE_AUTHOR("Woong Hwan Lee <woonghwan.lee@lge.com>");
MODULE_DESCRIPTION("LGE DSV Driver");
MODULE_LICENSE("GPL");
