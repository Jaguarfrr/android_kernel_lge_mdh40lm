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

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <soc/mediatek/lge/board_lge.h>

#include "sm5109.h"

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/

static struct i2c_client *sm5109_i2c_client;
static unsigned char dsv_data[3] = {0x20,0x00,0x00};
static unsigned char default_dsv_data[3] = {0x2A,0xA,0x43};//+,- 5.5v, target for tablet, active_discharge (actually for dh10)

unsigned char dsv_addr[5] = {
  SM5109_VPOS_ADDR,
  SM5109_VNEG_ADDR,
  SM5109_APPS_ADDR,
};

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/

static int sm5109_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int sm5109_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Extern Area
 *****************************************************************************/

static int sm5109_write_byte(unsigned char addr, unsigned char value)
{
  int ret = 0;
  unsigned char write_data[2] = {0};

  write_data[0] = addr;
  write_data[1] = value;

  if (NULL == sm5109_i2c_client) {
    SM5109_PRINT("[LCD][SM5109] sm5109_i2c_client is null!!\n");
    return -1;
  }
  ret = i2c_master_send(sm5109_i2c_client, write_data, 2);

  if (ret < 0)
    SM5109_PRINT("[LCD][SM5109] i2c write data fail !!\n");

  return ret;
}

static int sm5109_read_byte(unsigned char addr, unsigned char *value)
{
  int ret = 0;

  if (NULL == sm5109_i2c_client) {
    SM5109_PRINT("[LCD][SM5109] tps65132_i2c_client is null!!\n");
    return -1;
  }
  ret = i2c_smbus_read_i2c_block_data(sm5109_i2c_client, addr, 1, value);

  if (ret < 0)
    SM5109_PRINT("[LCD][SM5109] i2c write data fail !!\n");

  return ret;
}

int sm5109_set_vspn(void)
{
  int i = 0;

  for(i = POSCTRL; i < REGCNT; i++) {
    sm5109_write_byte(dsv_addr[i],dsv_data[i]);
  }

  SM5109_PRINT("[LCD][SM5109] POSCTRL = 0x%x, NEGCTRL = 0x%x, CONTROL = 0x%x\n", dsv_data[POSCTRL], dsv_data[NEGCTRL], dsv_data[CONTROL]);

  return 0;
}

int sm5109_set_vspn_value(unsigned int value)
{
  int ret = 0;

  if ((value <= MIN_VOLTAGE) || (value > MAX_VOLTAGE)) {
    SM5109_PRINT("[LCD][BIAS] unreasonable voltage value\n");
    ret = -1;
    goto out;
  }

  dsv_data[POSCTRL] = 0;
  dsv_data[NEGCTRL] = 0;

  dsv_data[POSCTRL] |= (value - 4000) / 100; //eg.  5.0V= 4.0V + Hex 0x0A (Bin 0 1010) * 100mV
  dsv_data[NEGCTRL] |= (value - 4000) / 100;

  sm5109_set_vspn();

  SM5109_PRINT("[LCD][SM5109] SET VALUE POSCTRL = 0x%x, NEGCTRL = 0x%x, CONTROL = 0x%x\n", dsv_data[POSCTRL], dsv_data[NEGCTRL], dsv_data[CONTROL]);

out:
  return ret;
}

int sm5109_power_off_vspn(void)
{
  sm5109_write_byte(SM5109_APPS_ADDR, NEG_OUTPUT_APPS); //set Smartphone mode, enable active-discharge

  return 0;
}

static const struct of_device_id i2c_of_match[] = {
    { .compatible = "mediatek,i2c_sm5109", },
    {},
};

static const struct i2c_device_id sm5109_i2c_id[] = {
    {SM5109_I2C_ID_NAME, 0},
    {},
};

static struct i2c_driver sm5109_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
  .id_table = sm5109_i2c_id,
  .probe = sm5109_i2c_probe,
  .remove = sm5109_i2c_remove,
  .driver = {
    .owner = THIS_MODULE,
    .name = SM5109_I2C_ID_NAME,
#ifdef CONFIG_OF
    .of_match_table = i2c_of_match,
#endif
  },
};

static int sm5109_parse_dt(void)
{
  int ret = 0;
  u32 tmp = 0;

  struct device_node *np = NULL;

  np = of_find_compatible_node(NULL, NULL, SM5109_DTS_NODE);

  if (!np) {
    SM5109_PRINT("[%s] DT node: Not found & default dsv data is used\n", __func__);
    memcpy(dsv_data, default_dsv_data, 3);
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

    SM5109_PRINT("[LCD][SM5109] POSCTRL = 0x%x, NEGCTRL = 0x%x, CONTROL = 0x%x\n", dsv_data[POSCTRL], dsv_data[NEGCTRL], dsv_data[CONTROL]);
  }

exit:
  return ret;
}

static ssize_t sysfs_show_dsv(struct device *dev, struct device_attribute *attr, char *buf)
{
  int n = 0;
  int err = 0;
  u8 data = 0;

  err = sm5109_read_byte(SM5109_VPOS_ADDR, &data);
  n +=  sprintf(buf+n, "VPOS = 0x%02x\n",data);

  err = sm5109_read_byte(SM5109_VNEG_ADDR, &data);
  n +=  sprintf(buf+n, "VNEG = 0x%02x\n",data);

  err = sm5109_read_byte(SM5109_APPS_ADDR, &data);
  n +=  sprintf(buf+n, "APPS = 0x%02x\n",data);

  return n;
}

DEVICE_ATTR(sm5109_reg, 0444, sysfs_show_dsv, NULL);

static int sm5109_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  int ret = 0;

  if (client == NULL) {
    SM5109_PRINT("[LCD][SM5109] i2c_client is NULL\n");
    return -1;
  }

  sm5109_i2c_client = client;

  ret = sm5109_parse_dt();

  device_create_file(&client->dev, &dev_attr_sm5109_reg);

  if(!ret)
    SM5109_PRINT("[LCD][SM5109] sm5109 DTS parsing success addr = 0x%x\n", client->addr);
  else
    SM5109_PRINT("[LCD][SM5109] sm5109 DTS parsing failed, default DSV data will be used. failed addr = 0x%x\n", client->addr);

  return ret;
}

static int sm5109_i2c_remove(struct i2c_client *client)
{
  sm5109_i2c_client = NULL;
  i2c_unregister_device(client);

  return 0;
}

static int __init sm5109_init(void)
{
  if (i2c_add_driver(&sm5109_i2c_driver)) {
    SM5109_PRINT("[LCD][SM5109] Failed to register sm5109_i2c_driver!\n");
    return -1;
  }

  SM5109_PRINT("[LCD][SM5109] sm5109_init success\n");

  return 0;
}

static void __exit sm5109_exit(void)
{
  i2c_del_driver(&sm5109_i2c_driver);
  SM5109_PRINT("[LCD][SM5109] unregister sm5109_i2c_driver!\n");
}

module_init(sm5109_init);
module_exit(sm5109_exit);

MODULE_AUTHOR("Woong Hwan Lee <woonghwan.lee@lge.com>");
MODULE_DESCRIPTION("LGE DSV Driver");
MODULE_LICENSE("GPL");
