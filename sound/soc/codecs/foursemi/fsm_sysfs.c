/**
 * Copyright (c) 2018 WinTech Technologies Co., Ltd. 2018-2019. All rights reserved.
 *Description: Core Defination For Foursemi Device .
 *Author: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */

#include "fsm_public.h"
// 190819 LGE add get/set registers S
#include "fs1603.h"
// 190819 LGE add get/set registers E
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/slab.h>

#define MAX_LEN 800
static int g_f0_test_status = 0;

static ssize_t fsm_start_f0_test(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t len)
{
    fsm_config_t *cfg = fsm_get_config();
    int ret = -EINVAL;
    int argv;

    sscanf(buf, "%d", &argv);
    pr_info("argv: %d", argv);

    switch (argv) {
        case 0:
            if (g_f0_test_status) {
                ret = fsm_i2c_cmd(FSM_CMD_INIT, 1);
                g_f0_test_status = 0;
            }
            break;
        case 1:
            if (g_f0_test_status == 0) {
                ret = fsm_i2c_cmd(FSM_CMD_F0_STEP1, 0);
                g_f0_test_status = 1;
            }
            break;
        default:
            if (g_f0_test_status) {
                pr_info("freq: %d\n", argv);
                cfg->test_freq = argv;
                ret = fsm_i2c_cmd(FSM_CMD_F0_STEP2, 0);
            }
            break;
    }
    return ret;
}

static ssize_t fsm_zmdata_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    int dev_count = fsm_check_device();
    char str_buffer[MAX_LEN];
    int len;

    len = snprintf(str_buffer, 10, "count:%d ", dev_count);
    len += fsm_get_zmdata_str(&str_buffer[len], (MAX_LEN - len));
    str_buffer[len] = '\0';
    //pr_info("buf: %s\n", str_buffer);
    return scnprintf(buf, PAGE_SIZE, "%s\n", str_buffer);
}

void fsm_hal_clear_calib_data(fsm_dev_t *fsm_dev)
{
    fsm_dev->calib_data.count = 0;
    fsm_dev->calib_data.calib_count = 0;
    fsm_dev->calib_data.calib_re25 = 0;
    fsm_dev->calib_data.minval = 0;
    fsm_dev->calib_data.preval = 0;
}

static ssize_t fsm_show_calib(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int ret;
    char str_buffer[MAX_LEN];
    int len;

    pr_info("%s force calibration enter", __func__);
// 191029 LGE add force calibration S
    pr_info("%s add delay 500ms before force calibration", __func__);
    fsm_delay_ms(500);
// 191029 LGE add force calibration E
    ret = fsm_calibrate(1);

    len = snprintf(str_buffer, MAX_LEN, "count:%d ", fsm_check_device());
    len += fsm_get_r25_str(&str_buffer[len], (MAX_LEN - len));
    str_buffer[len] = '\0';

    pr_info("buf: %s\n", str_buffer);
    return scnprintf(buf, PAGE_SIZE, "%s\n", str_buffer);
}

static ssize_t fsm_dsp_bypass_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t len)
{
    /*int ret;
    int bypass;

    if (!sscanf(buf, "%d", &bypass)) {
        return -EINVAL;
    }

    ret = fsm_i2c_cmd(FSM_CMD_SET_BYPASS, bypass);

    if(ret == 0) {
        pr_info("%s %s sys DSP Success, ret: %d\n", __func__, ((bypass == 1) ? "Bypass":"Unbypass"), ret);
    } else {
        pr_info("%s %s sys DSP Failed, ret: %d\n", __func__, ((bypass == 1) ? "Bypass":"Unbypass"), ret);
    }

    return ret;*/
    return 0;
}

// 190819 add LGE get/set registers S
struct fs16xx_regmap {
    const char *name;
    uint8_t reg;
    int writeable;
} fs16xx_regs[] = {
    { "00_FSM_STATUS",            FS1603_STATUS,     0 },
    { "05_ANA_STATUS",            FS1603_ANASTAT,    0 },
    { "04_I2S_CTRL",              FS1603_I2SCTRL,    1 },
    { "06_AUD_CTRL",              FS1603_AUDIOCTRL,  1 },
    { "09_SYS_CTRL",              FS1603_SYSCTRL,    1 },
    { "89_ACS_CTRL",              FS1603_ACSCTRL,    1 },
    { "A1_DSP_CTRL",              FS1603_DSPCTRL,    1 },
    { "AE_DAC_CTRL",              FS1603_DACCTRL,    1 },
    { "C0_BST_CTRL",              FS1603_BSTCTRL,    1 },
    { "D0_ANA_CTRL",              FS1603_ANACTRL,    1 },
};

static ssize_t fsm_reg_show(struct device *dev,
                  struct device_attribute *attr, char *buf)
{
    unsigned i, n, reg_count;
    int read_reg = 0;
    struct i2c_client *client = to_i2c_client(dev);
    uint8_t i2c_addr = client->addr;

    reg_count = sizeof(fs16xx_regs) / sizeof(fs16xx_regs[0]);
    for (i = 0, n = 0; i < reg_count; i++) {
        read_reg = fsm_get_registers(i2c_addr, fs16xx_regs[i].reg);
        if (read_reg == -1) {
            pr_err("%s:Failed to read %s, print FFFF\n", __func__, fs16xx_regs[i].name);
            read_reg = 0xFFFF;
        }
        n += scnprintf(buf + n, PAGE_SIZE - n,
                "%-20s = 0x%04x  writeable : %d\n",
                fs16xx_regs[i].name, read_reg, fs16xx_regs[i].writeable);
    }

    return n;
}

static ssize_t fsm_reg_store(struct device *dev,
                   struct device_attribute *attr,
                   const char *buf, size_t count)
{
    unsigned i, reg_count, value;
    int error = 0;
    char name[40];
    struct i2c_client *client = to_i2c_client(dev);
    uint8_t i2c_addr = client->addr;

    if (count >= 40) {
        pr_err("%s:input too long\n", __func__);
        return -1;
    }

    if (sscanf(buf, "%35s %x", name, &value) != 2) {
        pr_err("%s:unable to parse input\n", __func__);
        return -1;
    }

    pr_info("%s: %s %0xx", __func__, name, value);
    reg_count = sizeof(fs16xx_regs) / sizeof(fs16xx_regs[0]);
    for (i = 0; i < reg_count; i++) {
        if (!strcmp(name, fs16xx_regs[i].name)) {
            if (fs16xx_regs[i].writeable) {
                error = fsm_set_registers(i2c_addr, fs16xx_regs[i].reg, value);
                if (error) {
                    pr_err("%s:Failed to write %s\n", __func__, name);
                    return -1;
                }
            } else {
                pr_err("%s:Register %s is not writeable\n", __func__, name);
                return -1;
            }
            return count;
        }
    }

    pr_err("%s:no such register %s\n", __func__, name);
    return -1;
}
// 190819 LGE add get/set registers E

// 191029 LGE add force calibration S
static ssize_t fsm_calib_status(struct device *dev,
                  struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    uint8_t i2c_addr = client->addr;
    char str_buffer[MAX_LEN];
    int len = 0;
    int calib_impedance = 0;
    int calib_done = 0;

    calib_impedance = fsm_get_calib_impedance(i2c_addr);
    calib_done = fsm_get_calib_status(i2c_addr);

    len = snprintf(str_buffer, 100, "impedance : %d\n", calib_impedance);
    if ((i2c_addr == FSM_DEV_LEFT && calib_done && (6144 <= calib_impedance) && (calib_impedance <= 8704)) ||
        (i2c_addr == FSM_DEV_RIGHT && calib_done && (6144 <= calib_impedance) && (calib_impedance <= 7680)))
        len += snprintf(str_buffer + len, 100 - len, "status : %s", "OK");
    else
        len += snprintf(str_buffer + len, 100 - len, "status : %s", "NOK");
    str_buffer[len] = '\0';

    return scnprintf(buf, PAGE_SIZE, "%s\n", str_buffer);
}
// 191029 LGE add force calibration E

static DEVICE_ATTR(f0_test_fsm, 0664,
           fsm_zmdata_show, fsm_start_f0_test);

//S_IRUGO|S_IWUGO,
static DEVICE_ATTR(dsp_bypass_fsm, 0664,
           NULL, fsm_dsp_bypass_store);

static DEVICE_ATTR(force_calib_fsm, 0664,
           fsm_show_calib, NULL);

// 190819 LGE add get/set registers S
static DEVICE_ATTR(registers, 0664,
           fsm_reg_show, fsm_reg_store);
// 190819 LGE add get/set registers E

// 191029 LGE add force calibration S
static DEVICE_ATTR(calibration_status, 0664,
           fsm_calib_status, NULL);
// 191029 LGE add force calibration E

static struct attribute *fsm_attributes[] = {
    &dev_attr_f0_test_fsm.attr,
    &dev_attr_dsp_bypass_fsm.attr,
    &dev_attr_force_calib_fsm.attr,
// 190819 LGE add get/set registers S
    &dev_attr_registers.attr,
// 190819 LGE add get/set registers E
// 191029 LGE add force calibration S
    &dev_attr_calibration_status.attr,
// 191029 LGE add force calibration E
    NULL
};

static const struct attribute_group fsm_attr_group = {
    .attrs = fsm_attributes,
};

int fsm_sysfs_init(struct i2c_client *client)
{
    int ret;

    ret = sysfs_create_group(&client->dev.kobj, &fsm_attr_group);
    pr_info("%s exit, ret = %d\n", __func__, ret);
    return ret;
}

void fsm_sysfs_deinit(struct i2c_client *client)
{
    sysfs_remove_group(&client->dev.kobj, &fsm_attr_group);
    pr_info("%s exit\n", __func__);
}


