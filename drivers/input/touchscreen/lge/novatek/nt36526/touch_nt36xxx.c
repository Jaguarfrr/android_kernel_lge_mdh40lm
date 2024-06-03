/*
 * Copyright (C) 2010 - 2019 Novatek, Inc.
 *
 * $Revision$
 * $Date$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/input/mt.h>
//#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>

/*
 *  Include to Local Header File
 */
#include "touch_nt36xxx.h"

//---Fail Reason Type String---
#define LPWG_FAIL_NUM	3

#define MATCH_NAME	"novatek,nt36526"

static const char *debug_str[LPWG_FAIL_NUM] = {
	[0] = "TCI_1",
	[1] = "TCI_2",
	[2] = "SWIPE",
};

enum {
	LPWG_DEBUG_TCI_1 = 0,
	LPWG_DEBUG_TCI_2 = 1,
	LPWG_DEBUG_SWIPE = 2,
};

//---TCI Fail Reason Strings---
#define TCI_FAIL_NUM	9
static const char *tci_debug_str[TCI_FAIL_NUM] = {
	"NONE",
	"DISTANCE_TOUCHSLOP",
	"TIMEOUT_CONTINUOUS_TAP",
	"DISTANCE_GAP_TAP",
	"TIMEOUT_GAP_TAP",
	"TOTAL_TAP_NUM",
	"MULTI_FINGER",
	"ACTIVE_AREA",
	"INT_DELAY_TIME",
};

//---SWIPE Fail Reason Strings---
#define SWIPE_FAIL_NUM	8
static const char *swipe_debug_str[SWIPE_FAIL_NUM] = {
	"NONE",
	"FINGER_FAST_RELEASE",
	"MULTI_FINGER",
	"FAST_SWIPE",
	"SLOW_SWIPE",
	"WRONG_DIRECTION",
	"RATIO_FAIL",
	"OUT_OF_START_AREA",
};

static int nt36xxx_swipe_enable(struct device *dev, bool enable);
static int nt36xxx_lpwg_control(struct device *dev, int mode);

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(struct device *dev);
#endif

#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(struct device *dev);
extern int32_t nt36xxx_prd_register_sysfs(struct device *dev);
#endif

extern int32_t nt36xxx_upgrade(struct device *dev);
extern int32_t Read_CheckSum(struct device *dev, uint16_t *WR_Filechksum, uint16_t *RD_Filechksum);

int nt36xxx_init(struct device *dev);

static struct touch_core_data *gts;

#if TOUCH_KEY_NUM > 0
const uint16_t touch_key_array[TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
#include <linux/msm_lcd_power_mode.h>
#endif
#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
#include <linux/lcd_power_mode.h>
#endif

#define LCD_POWER_MODE_NUM 2
/*
static const char const *lcd_power_mode_str[LCD_POWER_MODE_NUM] = {
	[0] = "DEEP_SLEEP_ENTER",
	[1] = "DEEP_SLEEP_EXIT",
};
*/
/*
int nt36xxx_display_power_ctrl(struct device *dev, int ctrl)
{
	TOUCH_TRACE();

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	lge_panel_set_power_mode(ctrl);
#elif defined(CONFIG_LGE_TOUCH_CORE_MTK)
	primary_display_set_deep_sleep(ctrl);
#endif

	if (ctrl >= DEEP_SLEEP_ENTER && ctrl <= DSV_ALWAYS_ON) {
		TOUCH_I("%s, mode : %s\n", __func__, lcd_power_mode_str[ctrl]);
	}

	return 0;
}
*/
//extern void lge_panel_enter_deep_sleep(void);
//extern void lge_panel_exit_deep_sleep(void);
/*******************************************************
Description:
	Novatek touchscreen i2c read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t CTP_I2C_READ(struct device *dev, uint16_t i2c_addr, uint8_t *buf, uint16_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	int ret = -1;
	int retries = 0;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = i2c_addr;
	msgs[0].len   = 1;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = i2c_addr;
	msgs[1].len   = len-1;
	msgs[1].buf   = &buf[1];

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		TOUCH_E("error, ret=%d\n", ret);
		ret = -EIO;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen i2c write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t CTP_I2C_WRITE(struct device *dev, uint16_t i2c_addr, uint8_t *buf, uint16_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msg;
	int ret = -1;
	int retries = 0;

	msg.flags = !I2C_M_RD;
	msg.addr  = i2c_addr;
	msg.len   = len;
	msg.buf   = buf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		TOUCH_E("error, ret=%d\n", ret);
		ret = -EIO;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(struct device *dev, uint16_t i2c_addr, uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 16) & 0xFF;
	buf[2] = (addr >> 8) & 0xFF;

	return CTP_I2C_WRITE(dev, i2c_addr, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU then into idle mode
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset_idle(struct device *dev)
{
	uint8_t buf[4]={0};

	TOUCH_TRACE();

	//---write i2c cmds to reset idle---
	buf[0]=0x00;
	buf[1]=0xA5;
	CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);

	msleep(15);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU (boot) function.

return:
	n.a.
*******************************************************/
void nvt_bootloader_reset(struct device *dev)
{
	uint8_t buf[8] = {0};

	TOUCH_TRACE();

	//---write i2c cmds to reset---
	buf[0] = 0x00;
	buf[1] = 0x69;
	CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);

	// need 35ms delay after bootloader reset
	msleep(35);
}

/*******************************************************
Description:
	Novatek touchscreen clear FW status function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_clear_fw_status(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	TOUCH_TRACE();

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (d->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (d->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 3);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_I2C_READ(dev, I2C_FW_Address, buf, 2);

		if (buf[1] == 0x00)
			break;

		msleep(10);
	}

	if (i >= retry) {
		TOUCH_E("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW status function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_status(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	TOUCH_TRACE();

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (d->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (d->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 3);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_READ(dev, I2C_FW_Address, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		msleep(10);
	}

	if (i >= retry) {
		TOUCH_E("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW reset state function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_reset_state(struct device *dev, RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	TOUCH_TRACE();

	while (1) {
		msleep(10);

		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_I2C_READ(dev, I2C_FW_Address, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] < 0xFF)) {
			ret = 0;
			break;
		}

		retry++;
		if (unlikely(retry > 50)) {
			TOUCH_E("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get novatek project id information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_read_pid(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[3] = {0};
	int32_t ret = 0;

	TOUCH_TRACE();

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (d->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (d->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 3);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	CTP_I2C_READ(dev, I2C_FW_Address, buf, 3);

	d->nvt_pid = (buf[2] << 8) + buf[1];
	//snprintf(d->nvt_pid, sizeof(ts->nvt_pid), "%02X%02X", buf[2], buf[1]);

	TOUCH_I("PID=%04X\n", d->nvt_pid);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

	TOUCH_TRACE();

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (d->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (d->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 3);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_I2C_READ(dev, I2C_FW_Address, buf, 20);
	d->fw_ver = buf[1];
	d->fw_ver_bar = buf[2];
	d->x_num = buf[3];
	d->y_num = buf[4];
	d->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	d->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	d->max_button_num = buf[11];
	d->fw_sub_ver = buf[18];	// 0: Test FW, 1: Official Released FW

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		TOUCH_E("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		d->fw_ver = 0;
		d->x_num = 18;
		d->y_num = 32;
		d->abs_x_max = 1080;
		d->abs_y_max = 1920;
		d->max_button_num = 0;
		d->fw_sub_ver = 0;

		if (retry_count < 3) {
			retry_count++;
			TOUCH_E("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			TOUCH_E("Set default fw_ver=0, x_num=18, y_num=32, abs_x_max=1080, abs_y_max=1920, max_button_num=0!\n");
			ret = -1;
		}
	} else {
		ret = 0;
	}

	//---Get Novatek PID---
	nvt_read_pid(dev);

	return ret;
}

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTflash"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTflash read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t str[68] = {0};
	int32_t ret = -1;
	int32_t retries = 0;
	int8_t i2c_wr = 0;

	TOUCH_TRACE();

	if (count > sizeof(str)) {
		TOUCH_E("error count=%zu\n", count);
		return -EFAULT;
	}

	if (copy_from_user(str, buff, count)) {
		TOUCH_E("copy from user error\n");
		return -EFAULT;
	}

	i2c_wr = str[0] >> 7;

	if (i2c_wr == 0) {	//I2C write
		while (retries < 20) {
			ret = CTP_I2C_WRITE(gts->dev, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 1)
				break;
			else
				TOUCH_E("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			TOUCH_E("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else if (i2c_wr == 1) {	//I2C read
		while (retries < 20) {
			ret = CTP_I2C_READ(gts->dev, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 2)
				break;
			else
				TOUCH_E("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		// copy buff to user if i2c transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count))
				return -EFAULT;
		}

		if (unlikely(retries == 20)) {
			TOUCH_E("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else {
		TOUCH_E("Call error, str[0]=%d\n", str[0]);
		return -EFAULT;
	}
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTflash open function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	TOUCH_TRACE();

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		TOUCH_E("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTflash close function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	TOUCH_TRACE();

	if (dev)
		kfree(dev);

	return 0;
}

static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTflash initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int nvt_flash_proc_init(void)
{
	TOUCH_TRACE();

	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		TOUCH_E("Failed!\n");
		return -ENOMEM;
	} else {
		TOUCH_I("Succeeded!\n");
	}

	TOUCH_I("============================================================\n");
	TOUCH_I("Create /proc/NVTflash\n");
	TOUCH_I("============================================================\n");

	return 0;
}
#endif

static int nt36xxx_usb_status(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int charger_state = atomic_read(&ts->state.connect);
	int ret = -1;
	u8 buf[4] = {0x00, };
	u8 usb_none_notify_cmd = 0x51;
	u8 usb_dc_notify_cmd = 0x52;
	u8 usb_ac_notify_cmd = 0x53;

	TOUCH_TRACE();

	TOUCH_I("TA Type: %d\n", charger_state);

	if (atomic_read(&ts->state.debug_option_mask)
			& DEBUG_OPTION_0) {
		TOUCH_I("TA Simulator mode, Set CONNECT_TA\n");
		charger_state = 1;
	}

	if (atomic_read(&ts->state.pm) > DEV_PM_RESUME) {
		TOUCH_I("DEV_PM_SUSPEND - Don't try\n");
		return 0;
	}

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("IC_DEEP_SLEEP - Don't try\n");
		return 0;
	}

	/* write chatger_state */
	buf[0] = EVENT_MAP_HOST_CMD;
	switch (charger_state) {
		case USB_CONNECT_NONE:
			buf[1] = usb_none_notify_cmd;
			break;
		case USB_CONNECT_DC:
			buf[1] = usb_dc_notify_cmd;
			break;
		case USB_CONNECT_AC:
			buf[1] = usb_ac_notify_cmd;
			break;
		default:
			TOUCH_I("TA type not support\n");
			return 0;
	}
	ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		TOUCH_E("Write USB Notify Command Fail\n");
	}

	return 0;
}

static int nt36xxx_ime_status(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ime_state = atomic_read(&ts->state.ime);
	int ret = -1;
	u8 buf[4] = {0x00, };
	u8 ime_notify_cmd = 0x74;

	TOUCH_TRACE();

	TOUCH_I("IME Type: %d\n", ime_state);

	if (atomic_read(&ts->state.fb) == FB_SUSPEND) {
		TOUCH_I("Skip!!! ime setting in suspend\n");
		return 0;
	}

	/* write ime_state */
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = ime_notify_cmd;
	buf[2] = (u8)ime_state;
	ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 3);
	if (ret < 0) {
		TOUCH_E("Write IME Notify Command Fail\n");
	}

	return 0;
}

static int nt36xxx_call_status(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int call_state = atomic_read(&ts->state.incoming_call);

	TOUCH_TRACE();

	TOUCH_I("CALL Type: %d\n", call_state);

	/* write call_state */

	return 0;
}

static void nt36xxx_lcd_mode(struct device *dev, u32 mode)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);

	TOUCH_TRACE();

	TOUCH_I("lcd_mode: %d (prev: %d)\n", mode, d->lcd_mode);

	if (mode == LCD_MODE_U2_UNBLANK)
		mode = LCD_MODE_U2;

	d->lcd_mode = mode;
}

static void nt36xxx_enter_wkg_mode(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	int ret = -1;
	u8 buf[4] = {0x00, };
	u8 enable_wkg_mode = 0x13;	//wakeup gesture mode command

	TOUCH_TRACE();

	d->report_type = REPORT_WKG;

	//---Enable Wakeup Gesture Mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = enable_wkg_mode;
	ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		TOUCH_E("Enable LPWG Write Fail\n");
	}
}

static int nt36xxx_sleep_control(struct device *dev, int status)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[2] = {0};
	int ret = 0;

	TOUCH_TRACE();

	if (status == IC_NORMAL) {
		TOUCH_I("%s: IC_NORMAL\n", __func__);
		d->mode_state = FW_NORMAL_MODE;
		atomic_set(&ts->state.sleep, IC_NORMAL);
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
		nvt_bootloader_reset(dev);
		nvt_check_fw_reset_state(dev, RESET_STATE_INIT);
	} else if (status == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP\n", __func__);
		touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
		if (!atomic_read(&ts->state.incoming_call)) {	/* Idle status */
			//---write i2c cmds to enter deep sleep---
			buf[0] = EVENT_MAP_HOST_CMD;
			buf[1] = 0x11;
			//ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
			nt36xxx_lpwg_control(dev,LPWG_DOUBLE_TAP); //set lpwg forcely for saving power
			nt36xxx_enter_wkg_mode(dev);
			if (ret < 0) {
				TOUCH_E("Deep Sleep Write Fail\n");
			}
		} else {					/* Ringing or Offhook status*/
			TOUCH_I("%s: Avoid deep sleep during Call\n", __func__);
		}

		atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
		d->mode_state = DEEP_SLEEP_MODE;
	} else {
		TOUCH_E("invalid status:%d\n", status);
		ret = -EINVAL;
	}

	return ret;
}

static int nt36xxx_swipe_get_position(struct device *dev, u8 *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 1;
	int start_x = 0;
	int start_y = 0;
	int end_x = 0;
	int end_y = 0;
	int time = 0;

	TOUCH_TRACE();

	ts->lpwg.code_num = 1;
	memset(ts->lpwg.code, 0, sizeof(struct point) * MAX_LPWG_CODE);

	start_x = (buf[2 + 1] << 8) | buf[2 + 0];
	start_y = (buf[2 + 3] << 8) | buf[2 + 2];

	end_x = (buf[2 + 5] << 8) | buf[2 + 4];
	end_y = (buf[2 + 7] << 8) | buf[2 + 6];

	time = (buf[2 + 9] << 8) | buf[2 + 8];

	TOUCH_I("Swipe Gesture: start(%4d,%4d) end(%4d,%4d) swipe_time(%dms)\n",
			start_x, start_y, end_x, end_y, time);

	ts->lpwg.code[0].x = end_x;
	ts->lpwg.code[0].y = end_y;

	ts->lpwg.code[1].x = -1;
	ts->lpwg.code[1].y = -1;

	return ret;
}

static int novatek_swipe_read_fail_reason(struct device *dev, int mode)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	int ret = 0;
	u8 buf[16] = {0x00, };
	int i = 0;
	int fail_reason_cnt = 0;

	TOUCH_TRACE();

	if (d->report_type != REPORT_WKG) {
		return ret;
	}

	//---Read SWIPE_FAIL_REASON---
	buf[0] = EVENT_MAP_SWIPE_FAIL_REASON;
	ret = CTP_I2C_READ(dev, I2C_FW_Address, buf, 4);
	if (ret < 0) {
		TOUCH_E("Fail Reason Read Fail\n");
		goto swipe_read_fail_reason_i2c_fail;
	}

	//---print SWIPE fail reason---
	for (i = 0; i < 6; i++) {
		fail_reason_cnt = i/2 + 1;
		if (i % 2) {
			if ((buf[fail_reason_cnt] >> 4) < SWIPE_FAIL_NUM) {
				TOUCH_I("[%s][%d] %s\n", debug_str[LPWG_DEBUG_SWIPE], i, swipe_debug_str[buf[fail_reason_cnt] >> 4]);
			} else {
				TOUCH_E("[%s][%d] unsupported fail_num:%d\n", debug_str[LPWG_DEBUG_SWIPE], i, buf[fail_reason_cnt] >> 4);
			}
		} else {
			if ((buf[fail_reason_cnt] & 0x0F) < SWIPE_FAIL_NUM) {
				TOUCH_I("[%s][%d] %s\n", debug_str[LPWG_DEBUG_SWIPE], i, swipe_debug_str[buf[fail_reason_cnt] & 0x0F]);
			} else {
				TOUCH_E("[%s][%d] unsupported fail_num:%d\n", debug_str[LPWG_DEBUG_SWIPE], i, buf[fail_reason_cnt] & 0x0F);
			}
		}
	}

swipe_read_fail_reason_i2c_fail:
	return ret;
}

static int nt36xxx_lpwg_get_position(struct device *dev, int count, u8 *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 1;
	int i = 0;
	bool hide_lockscreen_coord =
		((atomic_read(&ts->state.lockscreen) == LOCKSCREEN_LOCK) &&
		 (ts->role.hide_coordinate));

	TOUCH_TRACE();

	ts->lpwg.code_num = count;
	memset(ts->lpwg.code, 0, sizeof(struct point) * MAX_LPWG_CODE);
	for (i = 2; i < count * 4 + 2; i = i + 4) {
		ts->lpwg.code[i/4].x =  (buf[i + 1] << 8) | buf[i];
		ts->lpwg.code[i/4].y =  (buf[i + 3] << 8) | buf[i + 2];
		if (hide_lockscreen_coord) {
			TOUCH_I("%s() idx:%d, x:xxxx, y:xxxx\n", __func__, i/4);
		} else {
			TOUCH_I("%s() idx:%d, x:%4d, y:%4d\n", __func__, i/4,
					ts->lpwg.code[i/4].x, ts->lpwg.code[i/4].y);
		}
	}

	ts->lpwg.code[i/4].x = -1;
	ts->lpwg.code[i/4].y = -1;

	return ret;
}

static int nt36xxx_lpwg_enable(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = -1;
	u8 buf[32] = {0x00, };
	u8 ctrl_flag = 0x00;

	TOUCH_TRACE();

	switch (mode) {
	case LPWG_NONE:
		ctrl_flag = 0x00;	//disable TCI
		break;
	case LPWG_DOUBLE_TAP:
		//ctrl_flag = 0x81 | 0x40;
		ctrl_flag = 0x81;      //disable TCI fail reason
		break;
	case LPWG_PASSWORD:
		if (!ts->tci.info[TCI_2].intr_delay) {
			ctrl_flag = 0x83;	//disable TCI fail reason
			//ctrl_flag = 0x83 | 0x40;	//enable TCI fail reason
		} else {
			ctrl_flag = 0x83 | 0x20;	//disable TCI fail reason
			//ctrl_flag = 0x83 | 0x20 | 0x40;	//enable TCI fail reason
		}
		break;
	default:
		TOUCH_I("%s() mode is not defined, mode=%d\n", __func__, mode);
		return ret;
	}

	//---Write LPWG_CONTROL_FLAG---
	buf[0] = EVENT_MAP_LPWG_CONTROL_FLAG;
	buf[1] = ctrl_flag;
	ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		TOUCH_E("%s() LPWG CTRL data I2C write Fail\n", __func__);
		goto set_lpwg_i2c_fail;
	}

	if (mode != LPWG_NONE) {
		TOUCH_I("LPWG Set Parameter START, mode=%d\n", mode);

		//---Set LPWG parameters---
		buf[0] = EVENT_MAP_LPWG_PARM;
		buf[1] = 0x37;	//XMIN : low byte
		buf[2] = 0x00;	//XMIN : high byte
		buf[3] = 0x99;	//XMAX : low byte
		buf[4] = 0x02;	//XMAX : high byte
		buf[5] = 0x37;	//YMIN : low byte
		buf[6] = 0x00;	//YMIN : high byte
		buf[7] = 0x09;	//YMAX : low byte
		buf[8] = 0x06;	//YMAX : high byte
		buf[9] = ts->tci.info[TCI_2].touch_slop; //TCHMOVMAX : low byte
		buf[10] = 0x00;	//TCHMOVMAX : high byte
		buf[11] = 0xFF;	//NOTCHMOVMAX : low byte
		buf[12] = 0xFF;	//NOTCHMOVMAX : high byte
		buf[13] = 0x00;	//TCHIMMIN
		buf[14] = ts->tci.info[TCI_2].min_intertap;	//TCHIMMAX
		buf[15] = 0x00;	//NOTCHIMMIN
		buf[16] = ts->tci.info[TCI_2].max_intertap;	//NOTCHIMMAX
		//TAP CNT
		if (mode == LPWG_DOUBLE_TAP) {
			buf[17] = ts->tci.info[TCI_1].tap_count;
		} else {
			buf[17] = ts->tci.info[TCI_2].tap_count;
		}
		//DT WINTIM
		if (mode == LPWG_DOUBLE_TAP) {
			buf[18] = 0x00;	//DTPREWINTIM : low byte
			buf[19] = 0x00;	//DTPREWINTIM : high byte
			buf[20] = 0x32;	//DTPOSTWINTIM : low byte
			buf[21] = 0x00;	//DTPOSTWINTIM : high byte
		} else if (mode == LPWG_PASSWORD) {
			buf[18] = 0x32;	//DTPREWINTIM : low byte
			buf[19] = 0x00;	//DTPREWINTIM : high byte
			buf[20] = 0x00;	//DTPOSTWINTIM : low byte
			buf[21] = 0x00;	//DTPOSTWINTIM : high byte
		}
		buf[22] = ts->tci.info[TCI_1].touch_slop;	//DTTCHMOVMAX : low byte
		buf[23] = 0x00;	//DTTCHMOVMAX : high byte
		buf[24] = ts->tci.info[TCI_1].tap_distance; //DTNOTCHMOVMAX : low byte
		buf[25] = 0x00; //DTNOTCHMOVMAX : high byte
		buf[26] = 0;	//DTTCHTIMMIN
		buf[27] = ts->tci.info[TCI_1].min_intertap;	//DTTCHTIMMAX
		buf[28] = 0;	//DTNOTCHTIMMIN
		buf[29] = ts->tci.info[TCI_1].max_intertap;	//DTNOTCHTIMMAX : 750ms
		ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 30);
		if (ret < 0) {
			TOUCH_E("Write LPWG_DT_TCH_PARM Fail, mode=%d\n", mode);
			goto set_lpwg_i2c_fail;
		}

		TOUCH_I("LPWG Set Parameter END\n");
	}
set_lpwg_i2c_fail:
	return ret;
}

static int nt36xxx_lpwg_clear_fail_reseon(struct device *dev)
{
	uint8_t buf[11] = {0x00, };

	TOUCH_TRACE();

	//---Write 0x00 to clear LPWG fail reason---
	buf[0] = EVENT_MAP_LPWG_FAIL_REASON;
	CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 11);

	return 0;
}

static int novatek_tci_read_fail_reason(struct device *dev, int mode)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	int ret = 0;
	u8 buf[16] = {0x00, };
	int i = 0;
	int fail_reason_cnt = 0;

	TOUCH_TRACE();

	if (d->report_type != REPORT_WKG) {
		return ret;
	}

	//---Read LPWG_FAIL_REASON---
	buf[0] = EVENT_MAP_LPWG_FAIL_REASON;
	ret = CTP_I2C_READ(dev, I2C_FW_Address, buf, 8);
	if (ret < 0) {
		TOUCH_E("Fail Reason Read Fail\n");
		goto lpwg_read_fail_reason_i2c_fail;
	}

	//---print TCI fail reason---
	for (i = 0; i < 7; i++) {
		fail_reason_cnt = i + 1;
		if (mode == LPWG_DOUBLE_TAP) {
			if ((buf[fail_reason_cnt] & 0x0F) < TCI_FAIL_NUM) {
				TOUCH_I("[%s][%d] %s\n", debug_str[LPWG_DEBUG_TCI_1], i, tci_debug_str[buf[fail_reason_cnt] & 0x0F]);
			} else {
				TOUCH_E("[%s][%d] unsupported fail_num:%d\n", debug_str[LPWG_DEBUG_TCI_1], i, buf[fail_reason_cnt] & 0x0F);
			}
		} else if (mode == LPWG_PASSWORD) {
			if (((buf[i] & 0xF0) >> 4) < TCI_FAIL_NUM) {
				TOUCH_I("[%s][%d] %s\n", debug_str[LPWG_DEBUG_TCI_2], i,tci_debug_str[(buf[fail_reason_cnt] & 0xF0) >> 4]);
			} else {
				TOUCH_E("[%s][%d] unsupported fail_num:%d\n", debug_str[LPWG_DEBUG_TCI_2], i, (buf[fail_reason_cnt] & 0xF0) >> 4);
			}
		} else {
			return 0;
		}
	}

lpwg_read_fail_reason_i2c_fail:
	return ret;
}

static void nt36xxx_get_tci_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->tci.info[TCI_1].tap_count = 2;

	//---TCI_1 for Knock On---
	//DTTCHTIMMAX
	ts->tci.info[TCI_1].min_intertap = 64; //time between press and release, 640ms, 1->10ms
	//DTNOTCHTIMMAX
	ts->tci.info[TCI_1].max_intertap = 70; //time between release and next press, 700ms, 1->10ms
	//DTTCHMOVMAX
	ts->tci.info[TCI_1].touch_slop = 10; //10mm
	//DTNOTCHMOVMAX
	ts->tci.info[TCI_1].tap_distance = 10;  //10mm
	ts->tci.info[TCI_1].intr_delay = 0;

	//---TCI_2 for Knock Code---
	//TCHIMMAX
	ts->tci.info[TCI_2].min_intertap = 64; //time between press and release, 640ms, 1 -> 10ms
	//NOTCHIMMAX
	ts->tci.info[TCI_2].max_intertap = 70; //time between release and next press, 700ms, 1->10ms
	//TCHMOVMAX
	ts->tci.info[TCI_2].touch_slop = 10; //10mm
	//NOTCHMOVMAX
	ts->tci.info[TCI_2].tap_distance = 255; // Full Screen set value??
	ts->tci.info[TCI_2].intr_delay = 0; //atmel T93 18, 19th pre, post
}

static void nt36xxx_get_swipe_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	float mm_to_point = 10.9; // 1 mm -> about X point

	TOUCH_TRACE();

	ts->swipe[SWIPE_U].enable = false;
	ts->swipe[SWIPE_U].distance = 20;
	ts->swipe[SWIPE_U].ratio_thres = 150;
	ts->swipe[SWIPE_U].min_time = 4;
	ts->swipe[SWIPE_U].max_time = 150;
	ts->swipe[SWIPE_U].wrong_dir_thres = 5;
	ts->swipe[SWIPE_U].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_U].init_ratio_thres = 100;
	ts->swipe[SWIPE_U].area.x1 = 0 + (int)(mm_to_point * 4);					// spec 4mm
	ts->swipe[SWIPE_U].area.y1 = 0;									// spec 0mm
	ts->swipe[SWIPE_U].area.x2 = ts->caps.max_x - (int)(mm_to_point * 4);				// spec 4mm
	ts->swipe[SWIPE_U].area.y2 = ts->caps.max_y;							// spec 0mm
	ts->swipe[SWIPE_U].start_area.x1 = (ts->caps.max_x / 2) - (int)(mm_to_point * 12.5);		// spec start_area_width 25mm
	ts->swipe[SWIPE_U].start_area.y1 = ts->swipe[SWIPE_U].area.y2 - (int)(mm_to_point * 14.5);	// spec start_area_height 14.5mm
	ts->swipe[SWIPE_U].start_area.x2 = (ts->caps.max_x / 2) + (int)(mm_to_point * 12.5);		// spec start_area_width 25mm
	ts->swipe[SWIPE_U].start_area.y2 = ts->swipe[SWIPE_U].area.y2;

	ts->swipe[SWIPE_D].enable = false;
	ts->swipe[SWIPE_D].distance = 15;
	ts->swipe[SWIPE_D].ratio_thres = 150;
	ts->swipe[SWIPE_D].min_time = 0;
	ts->swipe[SWIPE_D].max_time = 150;
	ts->swipe[SWIPE_D].wrong_dir_thres = 5;
	ts->swipe[SWIPE_D].init_ratio_chk_dist = 5;
	ts->swipe[SWIPE_D].init_ratio_thres = 100;
	ts->swipe[SWIPE_D].area.x1 = 0 + (int)(mm_to_point * 4);					// spec 4mm
	ts->swipe[SWIPE_D].area.y1 = 0;									// spec 0mm
	ts->swipe[SWIPE_D].area.x2 = ts->caps.max_x - (int)(mm_to_point * 4);				// spec 4mm
	ts->swipe[SWIPE_D].area.y2 = ts->caps.max_y;							// spec 0mm
	ts->swipe[SWIPE_D].start_area.x1 = 0 + (int)(mm_to_point * 4);					// spec 4mm
	ts->swipe[SWIPE_D].start_area.y1 = 0;								// spec 0mm
	ts->swipe[SWIPE_D].start_area.x2 = ts->caps.max_x - (int)(mm_to_point * 4);			// spec 4mm
	ts->swipe[SWIPE_D].start_area.y2 = 200;

	ts->swipe[SWIPE_L2].enable = false; // true modify for test
	ts->swipe[SWIPE_L2].distance = 12;
	ts->swipe[SWIPE_L2].ratio_thres = 150;
	ts->swipe[SWIPE_L2].min_time = 4;
	ts->swipe[SWIPE_L2].max_time = 150;
	ts->swipe[SWIPE_L2].wrong_dir_thres = 5;
	ts->swipe[SWIPE_L2].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_L2].init_ratio_thres = 100;
	ts->swipe[SWIPE_L2].area.x1 = 0;									// LG Pay UI width/height
	ts->swipe[SWIPE_L2].area.y1 = 204;
	ts->swipe[SWIPE_L2].area.x2 = (ts->caps.max_x);
	ts->swipe[SWIPE_L2].area.y2 = 1105;
	ts->swipe[SWIPE_L2].start_area.x1 = ((ts->caps.max_x) - (int)(mm_to_point * 10));		// spec 10mm
	ts->swipe[SWIPE_L2].start_area.y1 = 204;
	ts->swipe[SWIPE_L2].start_area.x2 = (ts->caps.max_x);
	ts->swipe[SWIPE_L2].start_area.y2 = 1105;

	ts->swipe[SWIPE_R2].enable = false;
	ts->swipe[SWIPE_R2].distance = 12;
	ts->swipe[SWIPE_R2].ratio_thres = 150;
	ts->swipe[SWIPE_R2].min_time = 4;
	ts->swipe[SWIPE_R2].max_time = 150;
	ts->swipe[SWIPE_R2].wrong_dir_thres = 5;
	ts->swipe[SWIPE_R2].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_R2].init_ratio_thres = 100;
	ts->swipe[SWIPE_R2].area.x1 = 0;									// LG Pay UI width/height
	ts->swipe[SWIPE_R2].area.y1 = 204;
	ts->swipe[SWIPE_R2].area.x2 = (ts->caps.max_x);
	ts->swipe[SWIPE_R2].area.y2 = 1105;
	ts->swipe[SWIPE_R2].start_area.x1 = 0;
	ts->swipe[SWIPE_R2].start_area.y1 = 204;
	ts->swipe[SWIPE_R2].start_area.x2 = (int)(mm_to_point * 10);					// spec 10mm
	ts->swipe[SWIPE_R2].start_area.y2 = 1105;
}

static int nt36xxx_lpwg_control(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	//---check if FW is after initial state---
	nvt_check_fw_reset_state(dev, RESET_STATE_INIT);

	//---clear LPWG fail reason before FW going to LPWG---
	ret = nt36xxx_lpwg_clear_fail_reseon(dev);
	if (ret < 0) {
		TOUCH_E("Fail to clear fail reason\n");
		goto clear_fail_reseon_fail;
	}

	d->mode_state = LPWG_MODE;

	ts->tci.mode = mode;
	ret = nt36xxx_lpwg_enable(dev, ts->tci.mode);

clear_fail_reseon_fail:
	return ret;
}

static int nt36xxx_lpwg_mode(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.fb) == FB_SUSPEND) {
		if (ts->mfts_lpwg) {
			TOUCH_I("%s(%d) - mfts_lpwg -> skip\n", __func__, __LINE__);
			return 0;
		}

		if (ts->lpwg.screen) {
			TOUCH_I("%s(%d) - FB_SUSPEND & screen on -> skip\n",
				__func__, __LINE__);
		} else if (ts->lpwg.sensor == PROX_NEAR) {
			TOUCH_I("%s(%d) - deep sleep by prox\n",
				__func__, __LINE__);
			if (atomic_read(&ts->state.sleep) == IC_NORMAL)
				ret = nt36xxx_sleep_control(dev, IC_DEEP_SLEEP);
		} else if (ts->lpwg.qcover == HALL_NEAR) {
			TOUCH_I("%s(%d) - deep sleep by hall\n",
				__func__, __LINE__);
			if (atomic_read(&ts->state.sleep) == IC_NORMAL)
				ret = nt36xxx_sleep_control(dev, IC_DEEP_SLEEP);
		} else {
			if (ts->lpwg.mode == LPWG_NONE
					&& !ts->swipe[SWIPE_L2].enable
					&& !ts->swipe[SWIPE_R2].enable
					&& !ts->swipe[SWIPE_U].enable
					&& !ts->swipe[SWIPE_D].enable) {
				TOUCH_I("%s(%d) - deep sleep (LPWG_NONE & swipe disable)\n",
						__func__, __LINE__);
				if (atomic_read(&ts->state.sleep) == IC_NORMAL)
					ret = nt36xxx_sleep_control(dev, IC_DEEP_SLEEP);
			} else {
				TOUCH_I("%s(%d) - knock %d\n",
						__func__, __LINE__, ts->lpwg.mode);
				if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
					ret = nt36xxx_sleep_control(dev, IC_NORMAL);
				// nt36xxx_swipe_enable() needs to be called BEFORE nt36xxx_lpwg_control()
				nt36xxx_swipe_enable(dev, true);
				nt36xxx_lpwg_control(dev, ts->lpwg.mode);
				nt36xxx_enter_wkg_mode(dev);
			}
		}

		return 0;
	}

	/* resume */
	touch_report_all_event(ts);

	if (ts->lpwg.screen) {
		TOUCH_I("%s(%d) - normal\n",
				__func__, __LINE__);
		//if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
			ret = nt36xxx_sleep_control(dev, IC_NORMAL);
		TOUCH_I("%s(%d) - set REPORT_ABS\n", __func__, __LINE__);
		d->report_type = REPORT_ABS;
		nt36xxx_swipe_enable(dev, false);
		nt36xxx_lpwg_control(dev, LPWG_NONE);
	} else if (ts->lpwg.sensor == PROX_NEAR) {
		TOUCH_I("%s(%d) - deep sleep by prox\n",
				__func__, __LINE__);
		if (atomic_read(&ts->state.sleep) == IC_NORMAL)
			ret = nt36xxx_sleep_control(dev, IC_DEEP_SLEEP);
	} else if (ts->lpwg.qcover == HALL_NEAR) {
		TOUCH_I("%s(%d) - deep sleep by hall\n",
				__func__, __LINE__);
		if (atomic_read(&ts->state.sleep) == IC_NORMAL)
			ret = nt36xxx_sleep_control(dev, IC_DEEP_SLEEP);
	} else {
		/* partial */
		TOUCH_I("%s(%d) - partial mode\n",
				__func__, __LINE__);
		if (ts->lpwg.mode == LPWG_NONE
				&& !ts->swipe[SWIPE_L2].enable
				&& !ts->swipe[SWIPE_R2].enable
				&& !ts->swipe[SWIPE_U].enable
				&& !ts->swipe[SWIPE_D].enable) {
			TOUCH_I("%s(%d) - deep sleep (LPWG_NONE & swipe disable)\n",
					__func__, __LINE__);
			ret = nt36xxx_sleep_control(dev, IC_DEEP_SLEEP);
		} else {
			TOUCH_I("%s(%d) - knock %d\n",
					__func__, __LINE__, ts->lpwg.mode);
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
				ret = nt36xxx_sleep_control(dev, IC_NORMAL);
			// nt36xxx_swipe_enable() needs to be called BEFORE nt36xxx_lpwg_control()
			nt36xxx_swipe_enable(dev, true);
			nt36xxx_lpwg_control(dev, ts->lpwg.mode);
			nt36xxx_enter_wkg_mode(dev);
		}
	}

	return 0;
}

static int nt36xxx_lpwg(struct device *dev, u32 code, void *param)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int *value = (int *)param;

	TOUCH_TRACE();

	TOUCH_I("%s start\n", __func__);

	switch (code) {
	case LPWG_ACTIVE_AREA:
		ts->lpwg.area[0].x = value[0];
		ts->lpwg.area[0].y = value[2];
		ts->lpwg.area[1].x = value[1];
		ts->lpwg.area[1].y = value[3];
		TOUCH_I("LPWG AREA (%d,%d)-(%d,%d)\n",
			ts->lpwg.area[0].x, ts->lpwg.area[0].y,
			ts->lpwg.area[1].x, ts->lpwg.area[1].y);
		break;
	case LPWG_TAP_COUNT:
		ts->tci.info[TCI_2].tap_count = value[0];
		break;
	case LPWG_DOUBLE_TAP_CHECK:
		TOUCH_I("%s() value:%d\n",__func__, value[0]);
		if (value[0])
			ts->tci.info[TCI_2].intr_delay = 70;
		else
			ts->tci.info[TCI_2].intr_delay = 0;
		break;

	case LPWG_UPDATE_ALL:
		ts->lpwg.mode = value[0];
		ts->lpwg.screen = value[1];
		ts->lpwg.sensor = value[2];
		ts->lpwg.qcover = value[3];
		TOUCH_I(
			"LPWG_UPDATE_ALL: mode[%d], screen[%s], sensor[%s], qcover[%s]\n",
			ts->lpwg.mode,
			ts->lpwg.screen ? "ON" : "OFF",
			ts->lpwg.sensor ? "FAR" : "NEAR",
			ts->lpwg.qcover ? "CLOSE" : "OPEN");
		nt36xxx_lpwg_mode(dev);
		break;
	case LPWG_REPLY:
		break;

	default:
		TOUCH_I("LPWG UNKNOWN CMD 0x%02x\n", code);
		break;
	}

	TOUCH_I("%s end\n", __func__);
	return 0;
}

#define NVT_LPWG_SUSCCESS 1
#define NVT_LPWG_FAIL 2
static int nt36xxx_lpwg_read(struct device *dev, u8 *result)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int tci1_result = 0;
	int tci2_result = 0;
	int swipe_result = 0;
	int swipe_type = 0;

	TOUCH_TRACE();

	//---Read EVENT_MAP_LPWG_RESULT---
	result[0] = EVENT_MAP_LPWG_RESULT;
	ret = CTP_I2C_READ(dev, I2C_FW_Address, result, 42);
	if (ret < 0) {
		TOUCH_E("LPWG Result Read Fail\n");
		goto lpwg_read_i2c_fail;
	}

	tci1_result = (int)(result[1] & 0x0F);
	tci2_result = (int)((result[1] & 0xF0) >> 4);
	swipe_result = (int)(result[2] & 0x0F);
	swipe_type = (int)((result[2] & 0xF0) >> 4);

	if (tci1_result == NVT_LPWG_SUSCCESS) {
		TOUCH_I("TCI_1\n");
		ret = nt36xxx_lpwg_get_position(dev, ts->tci.info[TCI_1].tap_count, &result[1]);
		ts->intr_status = TOUCH_IRQ_KNOCK;
	} else if (tci2_result == NVT_LPWG_SUSCCESS) {
		TOUCH_I("TCI_2\n");
		ret = nt36xxx_lpwg_get_position(dev, ts->tci.info[TCI_2].tap_count, &result[1]);
		ts->intr_status = TOUCH_IRQ_PASSWD;
	} else if ((tci2_result == NVT_LPWG_FAIL) || (tci1_result == NVT_LPWG_FAIL)){
		TOUCH_I("TCI Fail Reason\n");
		ret = novatek_tci_read_fail_reason(dev, ts->lpwg.mode);
	} else if (swipe_result == NVT_LPWG_SUSCCESS) {
		switch (swipe_type) {
		case (1 << 0):
			TOUCH_I("SWIPE_DOWN\n");
			ret = nt36xxx_swipe_get_position(dev, &result[1]);
			ts->intr_status = TOUCH_IRQ_SWIPE_DOWN;
			break;
		case (1 << 1):
			TOUCH_I("SWIPE_UP\n");
			ret = nt36xxx_swipe_get_position(dev, &result[1]);
			ts->intr_status = TOUCH_IRQ_SWIPE_UP;
			break;
		case (1 << 2):
			TOUCH_I("SWIPE_RIGHT\n");
			ret = nt36xxx_swipe_get_position(dev, &result[1]);
			ts->intr_status = TOUCH_IRQ_SWIPE_RIGHT2;
			break;
		case (1 << 3):
			TOUCH_I("SWIPE_LEFT\n");
			ret = nt36xxx_swipe_get_position(dev, &result[1]);
			ts->intr_status = TOUCH_IRQ_SWIPE_LEFT2;
			break;
		default:
			TOUCH_E("SWIPE reports more than 1 type, result[]=0x%2X, 0x%02X\n", result[1], result[2]);
			return -1;
		}
	} else if (swipe_result == NVT_LPWG_FAIL) {
		TOUCH_I("SWIPE Fail Reason\n");
		ret = novatek_swipe_read_fail_reason(dev, ts->lpwg.mode);
	} else {
		TOUCH_E("LPWG result with nothing, result[]=0x%02X, 0x%02X\n", result[1], result[2]);
		return -1;
	}

lpwg_read_i2c_fail:
	return ret;
}

static int nt36xxx_notify(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lge_panel_notifier *panel_data = data;
	int ret = 0;

	TOUCH_TRACE();

	switch (event) {
	case NOTIFY_TOUCH_RESET:
		if (panel_data->state == LGE_PANEL_RESET_LOW) {
			TOUCH_I("NOTIFY_TOUCH_RESET_LOW!\n");
			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			touch_gpio_direction_output(ts->reset_pin, 0);
		} else if (panel_data->state == LGE_PANEL_RESET_HIGH) {
			TOUCH_I("NOTIFY_TOUCH_RESET_HIGH!\n");
			touch_gpio_direction_output(ts->reset_pin, 1);
			//atomic_set(&d->init, IC_INIT_NEED);
		}
		break;
	case LCD_EVENT_LCD_BLANK:
		TOUCH_I("LCD_EVENT_LCD_BLANK!\n");
		break;
	case LCD_EVENT_LCD_UNBLANK:
		TOUCH_I("LCD_EVENT_LCD_UNBLANK!\n");
		break;
	case LCD_EVENT_LCD_MODE:
		TOUCH_I("LCD_EVENT_LCD_MODE!\n");
		TOUCH_I("lcd mode : %lu\n", (unsigned long)*(u32 *)data);
		nt36xxx_lcd_mode(dev, *(u32 *)data);
		break;
	case NOTIFY_CONNECTION:
		TOUCH_I("NOTIFY_CONNECTION!\n");
		ret = nt36xxx_usb_status(dev);
		break;
	case NOTIFY_IME_STATE:
		TOUCH_I("NOTIFY_IME_STATE!\n");
		ret = nt36xxx_ime_status(dev);
		break;
	case NOTIFY_CALL_STATE:
		TOUCH_I("NOTIFY_CALL_STATE!\n");
		ret = nt36xxx_call_status(dev);
		break;
	default:
		TOUCH_E("%lu is not supported\n", event);
		break;
	}

	return ret;
}

static int nt36xxx_set(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_TRACE();

	return 0;
}

static int nt36xxx_get_cmd_version(struct device *dev, char *buf)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint16_t BIN_Filechksum[8] = {0};
	uint16_t IC_Filechksum[8] = {0};
	int ret = 0;
	int offset = 0;

	TOUCH_TRACE();

	ret = nvt_get_fw_info(dev);
	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Read Fail Touch IC Info\n");
		goto read_fw_info_fail;
	}

	ret = Read_CheckSum(dev, BIN_Filechksum, IC_Filechksum);
	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Read Fail Touch IC or Bin Checksum\n");
		goto read_checksum_fail;
	}

	TOUCH_I("fw_ver:%d.%02d\n",
			d->fw_sub_ver, d->fw_ver);
	TOUCH_I("nvt_pid:0x%x\n",
			d->nvt_pid);
	TOUCH_I("x_axis_num:%d, y_axis_num:%d resolution_x:%d, resolution_y:%d\n",
			d->x_num, d->y_num, d->abs_x_max, d->abs_y_max);
	TOUCH_I("BIN_Filechksum[0]=0x%04X, BIN_Filechksum[1]=0x%04X\n",
			BIN_Filechksum[0], BIN_Filechksum[1]);
	TOUCH_I(" IC_Filechksum[0]=0x%04X,  IC_Filechksum[1]=0x%04X\n",
			IC_Filechksum[0], IC_Filechksum[1]);

	offset += snprintf(buf + offset, PAGE_SIZE, "fw_ver:%d.%02d\n", d->fw_sub_ver, d->fw_ver);
	offset += snprintf(buf + offset, PAGE_SIZE, "%s\n", MATCH_NAME);
	offset += snprintf(buf + offset, PAGE_SIZE, "nvt_pid: [NT%X]\n", d->nvt_pid);
	offset += snprintf(buf + offset, PAGE_SIZE, "BIN_chksum0=0x%04X, BIN_chksum1=0x%04X\n",
			BIN_Filechksum[0], BIN_Filechksum[1]);
	offset += snprintf(buf + offset, PAGE_SIZE, "IC_chksum0=0x%04X, IC_chksum1=0x%04X\n",
			IC_Filechksum[0], IC_Filechksum[1]);
	nvt_bootloader_reset(dev);
	nt36xxx_init(dev);

read_checksum_fail:
read_fw_info_fail:
	return offset;
}

static int nt36xxx_get_cmd_atcmd_version(struct device *dev, char *buf)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint16_t BIN_Filechksum[8] = {0};
	uint16_t IC_Filechksum[8] = {0};
	int ret = 0;
	int offset = 0;

	TOUCH_TRACE();

	ret = nvt_get_fw_info(dev);
	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Read Fail Touch IC Info\n");
		goto read_fw_info_fail;
	}

	ret = Read_CheckSum(dev, BIN_Filechksum, IC_Filechksum);
	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Read Fail Touch IC or Bin Checksum\n");
		goto read_checksum_fail;
	}

	offset += snprintf(buf + offset, PAGE_SIZE, "fw_ver:0x%X, fw_ver_bar:0x%X\n",
			d->fw_ver, d->fw_ver_bar);
	offset += snprintf(buf + offset, PAGE_SIZE, "BIN_Filechksum[0]=0x%04X, BIN_Filechksum[1]=0x%04X\n",
			BIN_Filechksum[0], BIN_Filechksum[1]);
	offset += snprintf(buf + offset, PAGE_SIZE, " IC_Filechksum[0]=0x%04X,  IC_Filechksum[1]=0x%04X\n",
			IC_Filechksum[0], IC_Filechksum[1]);

	nvt_bootloader_reset(dev);
	nt36xxx_init(dev);

read_checksum_fail:
read_fw_info_fail:
	return offset;
}

static int nt36xxx_get(struct device *dev, u32 cmd, void *input, void *output)
{
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_D(BASE_INFO, "%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
	case CMD_VERSION:
		ret = nt36xxx_get_cmd_version(dev, (char *)output);
		break;
	case CMD_ATCMD_VERSION:
		ret = nt36xxx_get_cmd_atcmd_version(dev, (char *)output);
		break;
	default:
		break;
	}

	return  ret;
}

static int nt36xxx_irq_abs(struct device *dev, u8 *point_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	struct touch_data *tdata;
	int ime_state = atomic_read(&ts->state.ime);
	uint32_t position = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t input_w_major = 0;
	uint32_t input_w_minor = 0;
	uint32_t input_p = 0;
	int32_t input_o = 0;
	uint8_t input_id = 0;
	int32_t i = 0;
	int32_t finger_cnt = 0;

	TOUCH_TRACE();

	ts->new_mask = 0;

	// ckeck palm id
	if (((point_data[1] >> 3) == ETD_REPORT_ID) && (point_data[2] == ETD_REPORT_ID_PALM)) {
		switch (point_data[3]) {
		case 1:
			TOUCH_I("Palm Detected\n");
			ts->is_cancel = 1;
			break;
		case 2:
			TOUCH_I("Palm Released\n");
			ts->is_cancel = 0;
			break;
		default:
			TOUCH_E("Invalid Palm Status, 0x%02X\n", point_data[3]);
			break;
		}
		ts->tcount = 0;
		ts->intr_status = TOUCH_IRQ_FINGER;
		return 0;
	}

	for (i = 0; i < d->max_touch_num; i++) {
		position = 1 + 6 * i;
		input_id = (uint8_t)(point_data[position + 0] >> 3) - 1;
		if (input_id > d->max_touch_num)
			continue;

		if (((point_data[position] & 0x07) == 0x01) || ((point_data[position] & 0x07) == 0x02)) {	//finger down (enter & moving)
			input_x = (uint32_t)(point_data[position + 1] << 4) + (uint32_t)(point_data[position + 3] >> 4);
			input_y = (uint32_t)(point_data[position + 2] << 4) + (uint32_t)(point_data[position + 3] & 0x0F);
			if ((input_x < 0) || (input_y < 0))
				continue;
			if ((input_x > d->abs_x_max)||(input_y > d->abs_y_max))
				continue;

			if(ime_state) {
				input_w_major = 0;
				input_w_minor = 0;
			} else {
				input_w_major = (uint32_t)(point_data[position + 4]);
				if (input_w_major == 0)
					input_w_major = 1;
				input_w_minor = (uint32_t)(point_data[i + 71]);
				if (input_w_minor == 0)
					input_w_minor = 1;
			}

			if (input_w_major == 0)
				input_w_major = 1;
			if (input_w_minor == 0)
				input_w_minor = 1;

			input_p = (uint32_t)(point_data[position + 5]);
			if (input_p == 0)
				input_p = 1;
			input_o = (int8_t)(point_data[i + 111]);

			//---width scaling : dimention(mm) to pixel---
			if ((d->x_dimention !=0) && (d->y_dimention != 0)) {
				input_w_major = (uint32_t)((uint32_t)(input_w_major * ts->caps.max_y) / d->y_dimention);
				input_w_minor = (uint32_t)((uint32_t)(input_w_minor * ts->caps.max_x) / d->x_dimention);
			}

			ts->new_mask |= (1 << input_id);
			tdata = ts->tdata + input_id;

			tdata->id = input_id;
			tdata->type = 0;
			tdata->x = input_x;
			tdata->y = input_y;
			tdata->pressure = input_p;
			tdata->width_major = input_w_major;
			tdata->width_minor = input_w_minor;
			tdata->orientation = input_o;

			TOUCH_D(ABS,
				"tdata [id:%d t:%d x:%4d y:%4d z:%3d-%3d,%3d,%3d]\n",
				tdata->id,
				tdata->type,
				tdata->x,
				tdata->y,
				tdata->pressure,
				tdata->width_major,
				tdata->width_minor,
				tdata->orientation);

			finger_cnt++;
		}
	}

	ts->intr_status = TOUCH_IRQ_FINGER;
	ts->tcount = finger_cnt;

	return 0;
}


#define POINT_DATA_LEN 120
static int nt36xxx_irq_handler(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	u8 buffer[POINT_DATA_LEN + 1] = {0,};
	int ret = -1;
	u8 read_id;

	TOUCH_TRACE();

	mutex_lock(&d->lock);

	if (d->report_type == REPORT_WKG) {
		//---read LPWG results---
		ret = nt36xxx_lpwg_read(dev, buffer);
	} else {
		//---read point data---
		ret = CTP_I2C_READ(dev, I2C_FW_Address, buffer, POINT_DATA_LEN + 1);
		if (ret < 0) {
			TOUCH_E("CTP_I2C_READ failed.(%d)\n", ret);
			goto XFER_ERROR;
		}

		//---check touch id---
		read_id = buffer[1] >> 3;
		if (((read_id > 0) && (read_id < 11)) || (read_id == 0x1F) || (read_id == ETD_REPORT_ID)) {
			ret = nt36xxx_irq_abs(dev, &buffer[0]);
			if (ret < 0) {
				TOUCH_E("Fail Get nt36xxx_irq_abs\n");
				goto read_point_data_fail;
			}
		} else {
			ret = -1;
			goto touch_id_illegal;
		}
	}

touch_id_illegal:
read_point_data_fail:
XFER_ERROR:
	mutex_unlock(&d->lock);
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen check and stop crc reboot loop.

return:
	n.a.
*******************************************************/
void nvt_stop_crc_reboot(struct device *dev)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;

	TOUCH_TRACE();

	//read dummy buffer to check CRC fail reboot is happening or not

	//---change I2C index to prevent geting 0xFF, but not 0xFC---
	nvt_set_page(dev, I2C_BLDR_Address, 0x1F64E);

	//---read to check if buf is 0xFC which means IC is in CRC reboot ---
	buf[0] = 0x4E;
	CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 4);

	if ((buf[1] == 0xFC) ||
		((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {

		//IC is in CRC fail reboot loop, needs to be stopped!
		for (retry = 5; retry > 0; retry--) {

			//---write i2c cmds to reset idle : 1st---
			buf[0]=0x00;
			buf[1]=0xA5;
			CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);

			//---write i2c cmds to reset idle : 2rd---
			buf[0]=0x00;
			buf[1]=0xA5;
			CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
			msleep(1);

			//---clear CRC_ERR_FLAG---
			nvt_set_page(dev, I2C_BLDR_Address, 0x3F135);

			buf[0] = 0x35;
			buf[1] = 0xA5;
			CTP_I2C_WRITE(dev, I2C_BLDR_Address, buf, 2);

			//---check CRC_ERR_FLAG---
			nvt_set_page(dev, I2C_BLDR_Address, 0x3F135);

			buf[0] = 0x35;
			buf[1] = 0x00;
			CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 2);

			if (buf[1] == 0xA5)
				break;
		}
		if (retry == 0)
			TOUCH_E("CRC auto reboot is not able to be stopped! buf[1]=0x%02X\n", buf[1]);
	}

	return;
}

/*******************************************************
Description:
	Novatek touchscreen check chip version trim function.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
static int8_t nvt_ts_check_chip_ver_trim(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;

	TOUCH_TRACE();

	nvt_bootloader_reset(dev); // NOT in retry loop

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {
		nvt_sw_reset_idle(dev);

		buf[0] = 0x00;
		buf[1] = 0x35;
		CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
		msleep(10);

		nvt_set_page(dev, I2C_BLDR_Address, 0x1F64E);

		buf[0] = 0x4E;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 7);
		TOUCH_I("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		//---Stop CRC check to prevent IC auto reboot---
		if ((buf[1] == 0xFC) ||
			((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {
			nvt_stop_crc_reboot(dev);
			continue;
		}

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				TOUCH_I("This is NVT touch IC\n");
				d->mmap = trim_id_table[list].mmap;
				d->carrier_system = trim_id_table[list].hwinfo->carrier_system;
				ret = 0;
				goto out;
			} else {
				d->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	return ret;
}

static ssize_t show_grip_suppression(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u8 grip_status = 0;
	uint8_t data[4] = {0};

	TOUCH_TRACE();

	mutex_lock(&ts->lock);
	/* read grip_status */
	//---read fw function status---
	data[0] = EVENT_MAP_FUNC_STATUS;
	CTP_I2C_READ(dev, I2C_FW_Address, data, 2);

	if (data[1] & (1 << FUNC_STATUS_EDGE_REJECT)) {
		grip_status = 1;
	} else {
		grip_status = 0;
	}

	mutex_unlock(&ts->lock);

	ret = snprintf(buf, PAGE_SIZE, "%hhu\n", grip_status);
	TOUCH_I("%s: grip_status = %hhu\n", __func__, grip_status);

	return ret;
}

#define NVT_EDGE_REJECT_DISABLE		0xB9
#define NVT_EDGE_REJECT_VERTICAL	0xBA
static ssize_t store_grip_suppression(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u8 grip_status = 0;
	uint8_t data[4] = {0};

	TOUCH_TRACE();

	if (kstrtou8(buf, 10, &grip_status) < 0)
		return count;

	if ((grip_status != 0) && (grip_status != 1)) {
		TOUCH_E("invalid grip_status (%hhu)\n", grip_status);
		return count;
	}

	TOUCH_I("%s: grip_status = %hhu\n", __func__, grip_status);

	mutex_lock(&ts->lock);
	/* write grip_status */
	switch (grip_status) {
	case 0:
		data[0] = EVENT_MAP_HOST_CMD;
		data[1] = NVT_EDGE_REJECT_DISABLE;
		CTP_I2C_WRITE(dev, I2C_FW_Address, data, 2);
		TOUCH_I("Disable Edge Reject.\n");
		break;
	case 1:
		data[0] = EVENT_MAP_HOST_CMD;
		data[1] = NVT_EDGE_REJECT_VERTICAL;
		CTP_I2C_WRITE(dev, I2C_FW_Address, data, 2);
		TOUCH_I("Enable Edge Reject to Vertical Mode.\n");
		break;
	default:
		TOUCH_E("%s: Not sopport value, grip_status=%d.\n", __func__, grip_status);
		break;
	}

	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_gpio_pin(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = snprintf(buf, PAGE_SIZE, "RST:%d, INT:%d\n",
			gpio_get_value(ts->reset_pin),
			gpio_get_value(ts->int_pin));

	TOUCH_I("%s :%s", __func__, buf);

	return ret;
}

static TOUCH_ATTR(grip_suppression, show_grip_suppression, store_grip_suppression);
static TOUCH_ATTR(gpio_pin, show_gpio_pin, NULL);

static struct attribute *nt36xxx_attribute_list[] = {
	&touch_attr_grip_suppression.attr,
	&touch_attr_gpio_pin.attr,
	NULL,
};

static const struct attribute_group nt36xxx_attribute_group = {
	.attrs = nt36xxx_attribute_list,
};

static int nt36xxx_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &nt36xxx_attribute_group);
	if (ret < 0) {
		TOUCH_E("sysfs register failed\n");
		kobject_del(&ts->kobj);
	}

	//---set device node---
#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		TOUCH_E("nvt flash proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init(dev);
	if (ret != 0) {
		TOUCH_E("nvt extra proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init(dev);
	if (ret != 0) {
		TOUCH_E("nvt mp proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}

	ret = nt36xxx_prd_register_sysfs(dev);
	if (ret != 0) {
		TOUCH_E("nt36xxx_prd_register_sysfs failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

	return 0;

#if (NVT_TOUCH_PROC || NVT_TOUCH_EXT_PROC || NVT_TOUCH_MP)
err_init_NVT_ts:
#endif
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver probe function.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
static int nt36xxx_probe(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct nt36xxx_data *d;
	struct device_node *np = ts->dev->of_node;
	int32_t ret = 0;

	TOUCH_TRACE();

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);

	if (!d) {
		TOUCH_E("failed to allocate nt36xxx data\n");
		return -ENOMEM;
	}

	touch_set_device(ts, d);

	touch_gpio_init(ts->reset_pin, "touch_reset");
	touch_gpio_direction_output(ts->reset_pin, 1);

	touch_gpio_init(ts->int_pin, "touch_int");
	touch_gpio_direction_input(ts->int_pin);

	//touch_power_init(dev);		// TDDI power : VDDI/AVDD/AVEE are from LCD driver
	touch_bus_init(dev, 256);	// nt36xxx max. i2c transfer size is 256 bytes

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim(dev);
	if (ret) {
		TOUCH_E("chip is not identified\n");
		ret = -EINVAL;
		goto err_chipvertrim_failed;
	}

	//---init mutex lock---
	mutex_init(&d->lock);

	//---reset touch and get the fw info.
	nvt_bootloader_reset(dev);
	nvt_check_fw_reset_state(dev, RESET_STATE_INIT);
	nvt_get_fw_info(dev);
	nt36xxx_get_tci_info(dev);
	nt36xxx_get_swipe_info(dev);

	//---get x&y dimention for width event scaling---
	PROPERTY_U32(np, "panel_x_dimention", d->x_dimention);
	PROPERTY_U32(np, "panel_y_dimention", d->y_dimention);
	TOUCH_I("panel_x_dimention:%d mm, panel_y_dimention:%d mm", d->x_dimention, d->y_dimention);

	d->max_touch_num = TOUCH_MAX_FINGER_NUM;
	d->mode_state = FW_NORMAL_MODE;
	gts = ts;

	d->lcd_mode = LCD_MODE_U3;

err_chipvertrim_failed:
	return ret;
}


/*******************************************************
Description:
	Novatek touchscreen driver release function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nt36xxx_remove(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);

	TOUCH_TRACE();

	mutex_destroy(&d->lock);

	TOUCH_I("Removing driver...\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen driver suspend function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nt36xxx_suspend(struct device *dev)
{
	TOUCH_TRACE();

	TOUCH_I("%s start\n", __func__);

	//nt36xxx_lpwg_mode(dev);

	TOUCH_I("%s end\n", __func__);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen driver resume function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nt36xxx_resume(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);

	TOUCH_TRACE();

	TOUCH_I("%s start\n", __func__);

	d->report_type = REPORT_ABS;

	TOUCH_I("%s end\n", __func__);
	return 0;
}

static int nt36xxx_power(struct device *dev, int ctrl)
{
	//TODO

	TOUCH_TRACE();

	return 0;
}

int nt36xxx_init(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);

	TOUCH_TRACE();

	TOUCH_I("%s start\n", __func__);

	nt36xxx_sleep_control(dev, IC_NORMAL);

	TOUCH_I("fw_ver:%d.%02d, nvt_pid:0x%x\n",
			d->fw_sub_ver, d->fw_ver, d->nvt_pid);

	nt36xxx_usb_status(dev);
	nt36xxx_ime_status(dev);
	nt36xxx_call_status(dev);

	d->report_type = REPORT_ABS;

	nt36xxx_lpwg_mode(dev);
	TOUCH_I("%s end\n", __func__);
	return 0;
}

static int nt36xxx_esd_recovery(struct device *dev)
{
	TOUCH_TRACE();

	return 0;
}

static int nt36xxx_swipe_enable(struct device *dev, bool enable)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct swipe_ctrl *ctrl[NT36XXX_SWIPE_NUM] = {  /* D, U, R, L */
		&ts->swipe[SWIPE_D],
		&ts->swipe[SWIPE_U],
		&ts->swipe[SWIPE_R2],
		&ts->swipe[SWIPE_L2],
	};
	int i = 0;
	int j = 0;
	u8 buf[32] = {0x00, };
	u8 enable_swipe_cmd = 0x73;
	const int32_t retry = 20;
	int ret  = 0;
	bool all_swipe_disabled = true;

	TOUCH_TRACE();

	TOUCH_I("%s: SWIPE D(%d),U(%d),R(%d),L(%d)\n", __func__,
			ctrl[NT36XXX_SWIPE_D]->enable,
			ctrl[NT36XXX_SWIPE_U]->enable,
			ctrl[NT36XXX_SWIPE_R]->enable,
			ctrl[NT36XXX_SWIPE_L]->enable);

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s : not set(deep sleep)\n", __func__);
		return 0;
	}

	if (enable) {
		for (i = 0; i < NT36XXX_SWIPE_NUM; i++) { /* D, U, R, L */
			ctrl[i]->debug_enable = ctrl[i]->enable;

			if (!ctrl[i]->enable) {
				TOUCH_I("skip (ctrl[%d]->enable = %d)\n", i, ctrl[i]->enable);
				continue;
			}
			all_swipe_disabled = false;

			TOUCH_I("enable[%d] distance[%d] ratio_thres[%d] min_time[%d] max_time[%d] wrong_dir_thres[%d] init_ratio_chk_dist[%d] init_ratio_thres[%d]\n",
					ctrl[i]->enable,
					ctrl[i]->distance,
					ctrl[i]->ratio_thres,
					ctrl[i]->min_time,
					ctrl[i]->max_time,
					ctrl[i]->wrong_dir_thres,
					ctrl[i]->init_ratio_chk_dist,
					ctrl[i]->init_ratio_thres
			       );

			TOUCH_I("area[(%4d,%4d)~(%4d,%4d)] start_area[(%4d,%4d)~(%4d,%4d)]\n",
					ctrl[i]->area.x1,
					ctrl[i]->area.y1,
					ctrl[i]->area.x2,
					ctrl[i]->area.y2,
					ctrl[i]->start_area.x1,
					ctrl[i]->start_area.y1,
					ctrl[i]->start_area.x2,
					ctrl[i]->start_area.y2
			       );

			//---Write LPWG_CONTROL_FLAG---
			buf[0] = EVENT_MAP_SWIPE_CONTROL_FLAG;
			buf[1] = (1 << i);		// disable SWIPE fail reason
			//buf[1] = (0x40 | (1 << i));	// enable SWIPE fail reason
			ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Fail to set swipe enable register (ret = %d)\n", ret);
			}

			//---Set SWIPE parameters---
			buf[0] = EVENT_MAP_LPWG_PARM;
			buf[1] = ctrl[i]->distance;				//distance (unit : mm)
			buf[2] = ctrl[i]->ratio_thres;				//ratio (unit : %)
			buf[3] = (u8)(ctrl[i]->min_time & 0xFF);		//min_time_L (unit : 10ms)
			buf[4] = (u8)((ctrl[i]->min_time >> 8) & 0xFF);		//min_time_H (unit : 10ms)
			buf[5] = (u8)(ctrl[i]->max_time & 0xFF);		//max_time_L (unit : 10ms)
			buf[6] = (u8)((ctrl[i]->max_time >> 8) & 0xFF);		//max_time_H (unit : 10ms)
			buf[7] = (u8)(ctrl[i]->area.x1 & 0xFF);			//active_area_x1_L
			buf[8] = (u8)((ctrl[i]->area.x1 >> 8) & 0xFF);		//active_area_x1_H
			buf[9] = (u8)(ctrl[i]->area.y1 & 0xFF);			//active_area_y1_L
			buf[10] = (u8)((ctrl[i]->area.y1 >> 8) & 0xFF);		//active_area_y1_H
			buf[11] = (u8)(ctrl[i]->area.x2 & 0xFF);		//active_area_x2_L
			buf[12] = (u8)((ctrl[i]->area.x2 >> 8) & 0xFF);		//active_area_x2_H
			buf[13] = (u8)(ctrl[i]->area.y2 & 0xFF);		//active_area_y2_L
			buf[14] = (u8)((ctrl[i]->area.y2 >> 8) & 0xFF);		//active_area_y2_H
			buf[15] = (u8)(ctrl[i]->start_area.x1 & 0xFF);		//start_area_x1_L
			buf[16] = (u8)((ctrl[i]->start_area.x1 >> 8) & 0xFF);	//start_area_x1_H
			buf[17] = (u8)(ctrl[i]->start_area.y1 & 0xFF);		//start_area_y1_L
			buf[18] = (u8)((ctrl[i]->start_area.y1 >> 8) & 0xFF);	//start_area_y1_H
			buf[19] = (u8)(ctrl[i]->start_area.x2 & 0xFF);		//start_area_x2_L
			buf[20] = (u8)((ctrl[i]->start_area.x2 >> 8) & 0xFF);	//start_area_x2_H
			buf[21] = (u8)(ctrl[i]->start_area.y2 & 0xFF);		//start_area_y2_L
			buf[22] = (u8)((ctrl[i]->start_area.y2 >> 8) & 0xFF);	//start_area_y2_H
			buf[23] = ctrl[i]->wrong_dir_thres;			//swipe_wrong_direction_threshold (unit : 1 mm)
			buf[24] = ctrl[i]->init_ratio_chk_dist;			//swipe_initial_ratio_check_distance (unit : 1mm)
			buf[25] = ctrl[i]->init_ratio_thres;			//swipe_initial_ratio_threshold (unit : %)

			ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 26);
			if (ret < 0) {
				TOUCH_E("Fail to set swipe control registers (ret = %d)\n", ret);
			}

			//---Trigger firmware backup SWIPE parameters---
			buf[0] = EVENT_MAP_HOST_CMD;
			buf[1] = enable_swipe_cmd;
			ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Fail to write enable_swipe_cmd (ret = %d)\n", ret);
			}

			for (j = 0; j < retry; j++) {
				//---read fw command buffer---
				buf[0] = EVENT_MAP_HOST_CMD;
				buf[1] = 0xFF;
				CTP_I2C_READ(dev, I2C_FW_Address, buf, 2);

				if (buf[1] == 0x00)
					break;

				msleep(5);
			}

			if (j >= retry) {
				TOUCH_E("Trigger firmware backup SWIPE parameters failed, i=%d, j=%d, buf[1]=0x%02X\n", i, j, buf[1]);
			}
		}
	}

	//---check and write swipe disable---
	if (enable == false || all_swipe_disabled == true) {
		TOUCH_I("all swipe gestures are disabled.\n");

		for (i = 0; i < NT36XXX_SWIPE_NUM; i++) { /* D, U, R, L */
			ctrl[i]->debug_enable = false;
		}

		buf[0] = EVENT_MAP_SWIPE_CONTROL_FLAG;
		buf[1] = 0x00;
		ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Fail to clear swipe enable register (ret = %d)\n", ret);
		}

		//---Trigger firmware backup SWIPE parameters---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = enable_swipe_cmd;
		ret = CTP_I2C_WRITE(dev, I2C_FW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Fail to write enable_swipe_cmd (ret = %d)\n", ret);
		}

		for (j = 0; j < retry; j++) {
			//---read fw command buffer---
			buf[0] = EVENT_MAP_HOST_CMD;
			buf[1] = 0xFF;
			CTP_I2C_READ(dev, I2C_FW_Address, buf, 2);

			if (buf[1] == 0x00)
				break;

			msleep(5);
		}

		if (j >= retry) {
			TOUCH_E("Trigger firmware backup SWIPE parameters failed, i=%d, j=%d, buf[1]=0x%02X\n", i, j, buf[1]);
		}
	}

	return 0;
}

static int nt36xxx_init_pm(struct device *dev)
{

	TOUCH_TRACE();

	return 0;
}

static int nt36xxx_shutdown(struct device *dev)
{
	TOUCH_TRACE();

	return 0;
}

static struct touch_driver touch_driver = {
	.probe = nt36xxx_probe,
	.remove = nt36xxx_remove,
	.suspend = nt36xxx_suspend,
	.shutdown = nt36xxx_shutdown,
	.resume = nt36xxx_resume,
	.init = nt36xxx_init,
	.upgrade = nt36xxx_upgrade,
	.esd_recovery = nt36xxx_esd_recovery,
	.irq_handler = nt36xxx_irq_handler,
	.power = nt36xxx_power,
	.lpwg = nt36xxx_lpwg,
	.swipe_enable = nt36xxx_swipe_enable,
	.notify = nt36xxx_notify,
	.init_pm = nt36xxx_init_pm,
	.register_sysfs = nt36xxx_register_sysfs,
	.set = nt36xxx_set,
	.get = nt36xxx_get,
};

static struct of_device_id touch_match_ids[] = {
	{ .compatible = MATCH_NAME, },
    {},
};

static struct touch_hwif hwif = {
	.bus_type = HWIF_I2C,
	.name = LGE_TOUCH_NAME,
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(touch_match_ids),
};

/*******************************************************
Description:
	Driver Install function.

return:
	Executive Outcomes. 0---succeed. not 0---failed.
********************************************************/
static int32_t __init nt36xxx_driver_init(void)
{
	TOUCH_TRACE();
	if (is_lcm_name("TXD-NT36526") || is_lcm_name("SKI-NT36526")
		|| is_lcm_name("TIANMA-NT36526")) {
		TOUCH_I("%s, nt36xxx detected lcm_name = %s\n", __func__, lge_get_lcm_name());
		return touch_bus_device_init(&hwif, &touch_driver);
	}
	TOUCH_I("%s, nt36xxx not found\n", __func__);
	return 0;
}

/*******************************************************
Description:
	Driver uninstall function.

return:
	n.a.
********************************************************/
static void __exit nt36xxx_driver_exit(void)
{
	TOUCH_TRACE();

	touch_bus_device_exit(&hwif);
}

//late_initcall(nvt_driver_init);
late_initcall(nt36xxx_driver_init);
module_exit(nt36xxx_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
