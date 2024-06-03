/* touch_ft5446.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: hyokmin.kwon@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define TS_MODULE "[ft5446]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/async.h>

//#include <mach/board_lge.h>
//#include <linux/i2c.h>
//#include <touch_i2c.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>

/*
 *  Include to Local Header File
 */
#include "touch_ft5446.h"
#include "touch_ft5446_prd.h"

#if defined(CONFIG_MACH_MT6762_MH55LM)
#include <soc/mediatek/lge/board_lge.h>
#endif
// Definitions for Debugging Failure Reason in LPWG
enum {
	LPWG_DEBUG_DISABLE = 0,
	LPWG_DEBUG_ALWAYS,
	LPWG_DEBUG_BUFFER,
	LPWG_DEBUG_BUFFER_ALWAYS,
};

static const char *lpwg_debug_type_str[] = {
	"Disable Type",
	"Always Report Type",
	"Buffer Type",
	"Buffer and Always Report Type"
};

#define TCI_FAILREASON_BUF_LEN	10
#define TCI_FAILREASON_NUM		7
static const char *tci_debug_str[TCI_FAILREASON_NUM + 1] = {
	[0] = "NONE",
	[1] = "DISTANCE_INTER_TAP",
	[2] = "DISTANCE_TOUCHSLOP",
	[3] = "TIMEOUT_INTER_TAP",
	[4] = "MULTI_FINGER",
	[5] = "DELAY_TIME", /* It means Over Tap */
	[6] = "PALM_STATE",
	[7] = "Reserved" // Invalid data
};
#define SWIPE_FAILREASON_BUF_LEN	10
#define SWIPE_FAILREASON_DIR_NUM	4
static const char *swipe_debug_direction_str[SWIPE_FAILREASON_DIR_NUM + 1] = {
	[0] = "SWIPE_UP",
	[1] = "SWIPE_DOWN",
	[2] = "SWIPE_LEFT",
	[3] = "SWIPE_RIGHT",
	[4] = "Reserved" // Invalid data
};
#define SWIPE_FAILREASON_NUM		12
static const char *swipe_debug_str[SWIPE_FAILREASON_NUM + 1] = {
	[0] = "Reserved",
	[1] = "NORMAL_ERROR",
	[2] = "FINGER_FAST_RELEASE",
	[3] = "MULTI_FINGER",
	[4] = "FAST_SWIPE",
	[5] = "SLOW_SWIPE",
	[6] = "WRONG_DIRECTION",
	[7] = "RATIO_FAIL",
	[8] = "OUT_OF_START_AREA",
	[9] = "OUT_OF_ACTIVE_AREA",
	[10] = "INITAIL_RATIO_FAIL",
	[11] = "PALM_STATE",
	[12] = "Reserved" // Invalid data
};

int apk_allocate_flag = 1;
int fw_upgrade_flag = 0;

#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT
u8 *tpd_i2c_dma_va = NULL;
dma_addr_t tpd_i2c_dma_pa = 0;
#endif

#ifdef CONFIG_MTK_I2C_EXTENSION
u8 *g_dma_buff_va = NULL;
dma_addr_t g_dma_buff_pa = 0;
#endif

#ifdef CONFIG_MTK_I2C_EXTENSION
	static void msg_dma_alloct(struct device *dev)
	{
		struct touch_core_data *ts = to_touch_core(dev);

	    if (NULL == g_dma_buff_va)
    		{
       		 ts->input->dev.coherent_dma_mask = DMA_BIT_MASK(32);
       		 g_dma_buff_va = (u8 *)dma_alloc_coherent(&ts->input->dev, 128, &g_dma_buff_pa, GFP_KERNEL);
    		}

	    	if(!g_dma_buff_va)
		{
	        	TOUCH_E("[DMA][Error] Allocate DMA I2C Buffer failed!\n");
	    	}
	}
/*
	static void msg_dma_release(void){
		if(g_dma_buff_va)
		{
	     		dma_free_coherent(NULL, 128, g_dma_buff_va, g_dma_buff_pa);
	        	g_dma_buff_va = NULL;
	        	g_dma_buff_pa = 0;
			TOUCH_E("[DMA][release] Allocate DMA I2C Buffer release!\n");
	    	}
	}
	*/
#endif

#ifdef FT5446_ESD_SKIP_WHILE_TOUCH_ON
static int finger_cnt = 0;

bool ft5446_check_finger(void)
{
	return finger_cnt==0? false:true;
}
EXPORT_SYMBOL(ft5446_check_finger);
#endif

int ft5446_reg_read(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	struct touch_bus_msg msg = {0, };
	int ret = 0;

	mutex_lock(&d->rw_lock);

	ts->tx_buf[0] = addr;

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = 1;

	msg.rx_buf = ts->rx_buf;
	msg.rx_size = size;

	ret = touch_bus_read(dev, &msg);

	if (ret < 0) {
		TOUCH_E("touch bus read error : %d\n", ret);
		mutex_unlock(&d->rw_lock);
		return ret;
	}

	memcpy(data, &ts->rx_buf[0], size);
	mutex_unlock(&d->rw_lock);
	return 0;

}

int ft5446_reg_write(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	struct touch_bus_msg msg = {0, };
	int ret = 0;

	mutex_lock(&d->rw_lock);

	ts->tx_buf[0] = addr;
	memcpy(&ts->tx_buf[1], data, size);

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = size + 1;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	ret = touch_bus_write(dev, &msg);

	if (ret < 0) {
		TOUCH_E("touch bus write error : %d\n", ret);
		mutex_unlock(&d->rw_lock);
		return ret;
	}

	mutex_unlock(&d->rw_lock);

	return 0;
}

int ft5446_cmd_read(struct device *dev, void *cmd_data, int cmd_len, void *read_buf, int read_len)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	struct touch_bus_msg msg = {0, };
	int ret = 0;

	mutex_lock(&d->rw_lock);

	memcpy(&ts->tx_buf[0], cmd_data, cmd_len);

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = cmd_len;

	msg.rx_buf = ts->rx_buf;
	msg.rx_size = read_len;

	ret = touch_bus_read(dev, &msg);

	if (ret < 0) {
		TOUCH_E("touch bus read error : %d\n", ret);
		mutex_unlock(&d->rw_lock);
		return ret;
	}

	memcpy(read_buf, &ts->rx_buf[0], read_len);
	mutex_unlock(&d->rw_lock);
	return 0;

}

int fts_i2c_read_cmd(struct device *dev, char *writebuf, int writelen, char *readbuf, int readlen)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	struct i2c_client *client = to_i2c_client(dev);
	int ret = 0;

	mutex_lock(&d->rw_lock);

	if(readlen > 0)
	{
		if (writelen > 0) {
			ret = i2c_master_send(client, writebuf, writelen);
			if (ret < 0)
				TOUCH_E("touch bus read error : %d\n", ret);

			touch_msleep(1);
			ret = i2c_master_recv(client, readbuf, readlen);
			if (ret < 0)
				TOUCH_E("touch bus read error : %d\n", ret);
		} else {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0)
				TOUCH_E("touch bus read error : %d\n", ret);
		}
	}

	mutex_unlock(&d->rw_lock);

	return ret;
}

static void ft5446_get_tci_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->tci.info[TCI_1].tap_count = 2;
	ts->tci.info[TCI_1].min_intertap = 0;
	ts->tci.info[TCI_1].max_intertap = 70;
	ts->tci.info[TCI_1].touch_slop = 100;
	ts->tci.info[TCI_1].tap_distance = 10;
	ts->tci.info[TCI_1].intr_delay = 0;

	ts->tci.info[TCI_2].min_intertap = 0;
	ts->tci.info[TCI_2].max_intertap = 70;
	ts->tci.info[TCI_2].touch_slop = 100;
	ts->tci.info[TCI_2].tap_distance = 255;
	ts->tci.info[TCI_2].intr_delay = 20;
}

static void ft5446_get_swipe_info(struct device *dev)
{

	struct touch_core_data *ts = to_touch_core(dev);
	float mm_to_point = 10.55; // 1 mm -> about X point

	TOUCH_TRACE();

	ts->swipe[SWIPE_L].enable = false;
	ts->swipe[SWIPE_L].distance = 12;
	ts->swipe[SWIPE_L].ratio_thres = 150;
	ts->swipe[SWIPE_L].min_time = 4;
	ts->swipe[SWIPE_L].max_time = 150;
	ts->swipe[SWIPE_L].wrong_dir_thres = 5;
	ts->swipe[SWIPE_L].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_L].init_ratio_thres = 100;
	ts->swipe[SWIPE_L].area.x1 = 0;									// LG Pay UI width/height
	ts->swipe[SWIPE_L].area.y1 = 204;
	ts->swipe[SWIPE_L].area.x2 = (ts->caps.max_x);
	ts->swipe[SWIPE_L].area.y2 = 1105;
	ts->swipe[SWIPE_L].start_area.x1 = ((ts->caps.max_x) - (int)(mm_to_point * 10)); 		// spec 10mm
	ts->swipe[SWIPE_L].start_area.y1 = 204;
	ts->swipe[SWIPE_L].start_area.x2 = (ts->caps.max_x);
	ts->swipe[SWIPE_L].start_area.y2 = 1105;

	ts->swipe[SWIPE_R].enable = false;
	ts->swipe[SWIPE_R].distance = 12;
	ts->swipe[SWIPE_R].ratio_thres = 150;
	ts->swipe[SWIPE_R].min_time = 4;
	ts->swipe[SWIPE_R].max_time = 150;
	ts->swipe[SWIPE_R].wrong_dir_thres = 5;
	ts->swipe[SWIPE_R].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_R].init_ratio_thres = 100;
	ts->swipe[SWIPE_R].area.x1 = 0;									// LG Pay UI width/height
	ts->swipe[SWIPE_R].area.y1 = 204;
	ts->swipe[SWIPE_R].area.x2 = (ts->caps.max_x);
	ts->swipe[SWIPE_R].area.y2 = 1105;
	ts->swipe[SWIPE_R].start_area.x1 = 0;
	ts->swipe[SWIPE_R].start_area.y1 = 204;
	ts->swipe[SWIPE_R].start_area.x2 = (int)(mm_to_point * 10);					// spec 10mm
	ts->swipe[SWIPE_R].start_area.y2 = 1105;

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
}

int ft5446_ic_info(struct device *dev)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	u8 chip_id = 0;
	u8 chip_id_low = 0;
	u8 fw_vendor_id = 0;
	u8 fw_version = 0;
	int i;

	TOUCH_TRACE();

	// If it is failed to get info without error, just return error
	for (i = 0; i < 2; i++) {
		ret |= ft5446_reg_read(dev, FTS_REG_ID, (u8 *)&chip_id, 1);
		ret |= ft5446_reg_read(dev, FTS_REG_ID_LOW, (u8 *)&chip_id_low, 1);
		ret |= ft5446_reg_read(dev, FTS_REG_FW_VER, (u8 *)&fw_version, 1);
		ret |= ft5446_reg_read(dev, FTS_REG_FW_VENDOR_ID, (u8 *)&fw_vendor_id, 1);
		ret |= ft5446_reg_read(dev, FTS_REG_PRODUCT_ID_H, &(d->ic_info.product_id[0]), 1);
		ret |= ft5446_reg_read(dev, FTS_REG_PRODUCT_ID_L, &(d->ic_info.product_id[1]), 1);

		if (ret == 0) {
			TOUCH_I("Success to get ic info data\n");
			break;
		}
	}

	if (i >= 2) {
		TOUCH_E("Failed to get ic info data\n");
		return -EPERM; // Do nothing in caller
	}

	d->ic_info.version.major = (fw_version & 0x80) >> 7;
	d->ic_info.version.minor = fw_version & 0x7F;
	d->ic_info.chip_id = chip_id; // Device ID
	d->ic_info.chip_id_low = chip_id_low;
	d->ic_info.fw_vendor_id = fw_vendor_id; // Vendor ID
	d->ic_info.info_valid = 1;

	TOUCH_I("==================== Version Info ====================\n");
	TOUCH_I("Version: v%d.%02d\n", d->ic_info.version.major, d->ic_info.version.minor);
	TOUCH_I("Chip_id: %x / Chip_id_low : %x\n", d->ic_info.chip_id, d->ic_info.chip_id_low);
	TOUCH_I("Vendor_id: %x\n", d->ic_info.fw_vendor_id);
	TOUCH_I("Product_id: FT%x%x\n", d->ic_info.product_id[0], d->ic_info.product_id[1]);
	TOUCH_I("======================================================\n");

	return ret;
}

static int ft5446_tci_control(struct device *dev, int type)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	struct tci_info *info1 = &ts->tci.info[TCI_1];
	struct tci_info *info2 = &ts->tci.info[TCI_2];
	u8 data;
	int ret = 0;

	TOUCH_TRACE();

	switch (type) {
		case TCI_CTRL_CONFIG_COMMON:
			data = (((d->tci_debug_type) & 0x02) << 3) | ((d->tci_debug_type) & 0x01);
			ret = ft5446_reg_write(dev, 0xE5, &data, 1);	// Fail Reason Debug Function Enable

			// Resolution 720x1520 MH4+ HD+

			data = 0x28;
			ret |= ft5446_reg_write(dev, 0x92, &data, 1);	// Active Area LSB of X1
			data = 0x00;
			ret |= ft5446_reg_write(dev, 0xB5, &data, 1);	// Active Area MSB of X1 (40)

			data = 0xA8;
			ret |= ft5446_reg_write(dev, 0xB6, &data, 1);	// Active Area LSB of X2
			data = 0x02;
			ret |= ft5446_reg_write(dev, 0xB7, &data, 1);	// Active Area MSB of X2 (680)
			if (is_lcm_name("KD-JD9365Z")) {
				data = 0x28;
			} else {
				data = 0x22;
			}
			ret |= ft5446_reg_write(dev, 0xB8, &data, 1);	// Active Area LSB of Y1
			data = 0x00;
			ret |= ft5446_reg_write(dev, 0xB9, &data, 1);	// Active Area MSB of Y1 (34)
			if (is_lcm_name("KD-JD9365Z")) {
				data = 0x78;
			} else {
				data = 0xCE;
			}
			ret |= ft5446_reg_write(dev, 0xBA, &data, 1);	// Active Area LSB of Y2
			data = 0x05;
			ret |= ft5446_reg_write(dev, 0xBB, &data, 1);	// Active Area MSB of Y2 (1486)
			break;

		case TCI_CTRL_CONFIG_TCI_1:
			data = 0x0A;
			ret = ft5446_reg_write(dev, 0xBC, &data, 1);	// Touch Slop (10mm)
			data = 0x0A;
			ret |= ft5446_reg_write(dev, 0xC4, &data, 1);	// Touch Distance (10mm)
			data = 0x46; //0x32;
			ret |= ft5446_reg_write(dev, 0xC6, &data, 1);	// Time Gap Max (700ms)
			data = 0x02;
			ret |= ft5446_reg_write(dev, 0xCA, &data, 1);	// Total Tap Count (2)
			data = info1->intr_delay; //0x32;
			ret |= ft5446_reg_write(dev, 0xCC, &data, 1);	// Interrupt Delay (700ms or 0ms)
			break;

		case TCI_CTRL_CONFIG_TCI_2:
			data = 0x0A;
			ret = ft5446_reg_write(dev, 0xBD, &data, 1);	// Touch Slop (10mm)
			data = 0xFF; //0xC8;
			ret |= ft5446_reg_write(dev, 0xC5, &data, 1);	// Touch Distance (255mm)
			data = 0x46;
			ret |= ft5446_reg_write(dev, 0xC7, &data, 1);	// Time Gap Max (700ms)
			data = info2->tap_count;
			ret |= ft5446_reg_write(dev, 0xCB, &data, 1);	// Total Tap Count (6~10)
			data = 0x32;
			ret |= ft5446_reg_write(dev, 0xCD, &data, 1);	// Interrupt Delay (500ms)
			break;

		default:
			break;
	}

	return ret;
}

static int ft5446_swipe_control(struct device *dev, u8 idx, int *swipe_mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	u8 data;
	int ret = 0;

	TOUCH_TRACE();

	switch (idx) {
		case SWIPE_D:
			*swipe_mode |= 0x1 << 3;
			data = 0x1;
			ret |= ft5446_reg_write(dev, 0x60, &data, 1);	// Select BankSel (which swipe config)
			break;
		case SWIPE_U:
			*swipe_mode |= 0x1 << 2;
			data = 0x0;
			ret |= ft5446_reg_write(dev, 0x60, &data, 1);
			break;
		case SWIPE_L:
			*swipe_mode |= 0x1 << 4;
			data = 0x2;
			ret |= ft5446_reg_write(dev, 0x60, &data, 1);
			break;
		case SWIPE_R:
			*swipe_mode |= 0x1 << 5;
			data = 0x3;
			ret |= ft5446_reg_write(dev, 0x60, &data, 1);
			break;
		default:
			TOUCH_E("Not supported swipe index : %d\n", idx);
			return -1;
	}

	data = (((d->swipe_debug_type) & 0x02) << 3) | ((d->swipe_debug_type) & 0x01);
	ret = ft5446_reg_write(dev, 0x95, &data, 1);	// Fail Reason Debug Function Enable

	data = ts->swipe[idx].distance;
	ret |= ft5446_reg_write(dev, 0x61, &data, 1);	// Distance
	data = ts->swipe[idx].ratio_thres;
	ret |= ft5446_reg_write(dev, 0x62, &data, 1);	// Ratio
	data = (ts->swipe[idx].min_time & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x63, &data, 1);	// Min time HighBit
	data = ts->swipe[idx].min_time & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x64, &data, 1);	// Min time LowBit
	data = (ts->swipe[idx].max_time & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x65, &data, 1);	// Max time HighBit
	data = ts->swipe[idx].max_time & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x66, &data, 1);	// Max time LowBit
	data = ts->swipe[idx].wrong_dir_thres;
	ret |= ft5446_reg_write(dev, 0x67, &data, 1);	// Wrong Direction

	data = (ts->swipe[idx].area.x1 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x68, &data, 1);	// Active Area x1 HighBit
	data = ts->swipe[idx].area.x1 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x69, &data, 1);	// Active Area x1 LowBit
	data = (ts->swipe[idx].area.y1 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x6A, &data, 1);	// Active Area y1 HighBit
	data = ts->swipe[idx].area.y1 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x6B, &data, 1);	// Active Area y1 LowBit

	data = (ts->swipe[idx].area.x2 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x6C, &data, 1);	// Active Area x2 HighBit
	data = ts->swipe[idx].area.x2 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x6D, &data, 1);	// Active Area x2 LowBit
	data = (ts->swipe[idx].area.y2 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x6E, &data, 1);	// Active Area y2 HighBit
	data = ts->swipe[idx].area.y2 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x6F, &data, 1);	// Active Area y2 LowBit

	data = (ts->swipe[idx].start_area.x1 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x70, &data, 1);	// Active Start Area x1 HighBit
	data = ts->swipe[idx].start_area.x1 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x71, &data, 1);	// Active Start Area x1 LowBit
	data = (ts->swipe[idx].start_area.y1 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x72, &data, 1);	// Active Start Area y1 HighBit
	data = ts->swipe[idx].start_area.y1 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x73, &data, 1);	// Active Start Area y1 LowBit

	data = (ts->swipe[idx].start_area.x2 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x74, &data, 1);	// Active Start Area x2 HighBit
	data = ts->swipe[idx].start_area.x2 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x75, &data, 1);	// Active Start Area x2 LowBit
	data = (ts->swipe[idx].start_area.y2 & 0xFF00) >> 8;
	ret |= ft5446_reg_write(dev, 0x76, &data, 1);	// Active Start Area y2 HighBit
	data = ts->swipe[idx].start_area.y2 & 0x00FF;
	ret |= ft5446_reg_write(dev, 0x77, &data, 1);	// Active Start Area y2 LowBit

	data = ts->swipe[idx].init_ratio_chk_dist;
	ret |= ft5446_reg_write(dev, 0x78, &data, 1);	// Initial Ratio Check Distance
	data = ts->swipe[idx].init_ratio_thres;
	ret |= ft5446_reg_write(dev, 0x79, &data, 1);	// Initial Ratio Threshold

	return ret;
}


static int ft5446_lpwg_control(struct device *dev, u8 tci_mode, bool swipe_enable)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct tci_info *info1 = &ts->tci.info[TCI_1];
	//struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	int swipe_mode = 0;
	u8 data = 0;
	int i = 0;

	TOUCH_TRACE();
	switch (tci_mode) {
		case LPWG_DOUBLE_TAP:
			ts->tci.mode = 0x01;
			info1->intr_delay = 0;

			ret = ft5446_tci_control(dev, TCI_CTRL_CONFIG_TCI_1);
			ret |= ft5446_tci_control(dev, TCI_CTRL_CONFIG_COMMON);
			break;
#ifdef KNOCK_CODE
		case LPWG_PASSWORD:
			ts->tci.mode = 0x03;
			info1->intr_delay = ts->tci.double_tap_check ? 70 : 0;

			ret = ft5446_tci_control(dev, TCI_CTRL_CONFIG_TCI_1);
			ret |= ft5446_tci_control(dev, TCI_CTRL_CONFIG_TCI_2);
			ret |= ft5446_tci_control(dev, TCI_CTRL_CONFIG_COMMON);
			break;
		case LPWG_PASSWORD_ONLY:
			ts->tci.mode = 0x02;
			info1->intr_delay = 0;

			ret |= ft5446_tci_control(dev, TCI_CTRL_CONFIG_TCI_2);
			ret |= ft5446_tci_control(dev, TCI_CTRL_CONFIG_COMMON);
			break;
#endif
		default:
			ts->tci.mode = 0;

			break;
	}

	if (swipe_enable) {
		for (i = 0; i < sizeof(ts->swipe) / sizeof(struct swipe_ctrl); i++) {
			if (ts->swipe[i].enable) {
				ret |= ft5446_swipe_control(dev, i, &swipe_mode);
			}
		}
	} else { //swipe_disable
		swipe_mode = 0;
	}


	// Setting Enable Register
	data = ts->tci.mode | swipe_mode;
	ret |= ft5446_reg_write(dev, 0xD0, &data, 1);

	TOUCH_I("ft5446_lpwg_control tci = %d, swipe = %d\n", ts->tci.mode, swipe_mode);

	return ret;
}

static int ft5446_deep_sleep(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u8 data = 0;

	TOUCH_TRACE();

	TOUCH_I("ft5446_deep_sleep = %d\n", mode);

	if (!atomic_read(&ts->state.incoming_call)) { /* IDLE status */
		if(mode) {
			touch_interrupt_control(dev, INTERRUPT_DISABLE);
			atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
			data = 0x03;
			ret = ft5446_reg_write(dev, 0xA5, &data, 1);
		}
		else {
			ft5446_reset_ctrl(dev, HW_RESET);
			atomic_set(&ts->state.sleep, IC_NORMAL);
			touch_interrupt_control(dev, INTERRUPT_ENABLE);
		}
	} else { /* RINGING or OFFHOOK status */
		TOUCH_I("Incoming call status (Not deep sleep)\n");
		if(mode) {
			touch_interrupt_control(dev, INTERRUPT_DISABLE);
			data = 0x01;
			ret = ft5446_reg_write(dev, 0xF9, &data, 1);
		}
		else {
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
				ft5446_reset_ctrl(dev, HW_RESET);
				atomic_set(&ts->state.sleep, IC_NORMAL);
				touch_interrupt_control(dev, INTERRUPT_ENABLE);
			} else {
				data = 0x00;
				ret = ft5446_reg_write(dev, 0xF9, &data, 1);
				touch_interrupt_control(dev, INTERRUPT_ENABLE);
			}
		}
	}

	return ret;
}

int ft5446_lpwg_reset(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	TOUCH_I("ft5446_lpwg_reset\n");
	touch_gpio_direction_output(ts->reset_pin, 0);
	touch_msleep(5);
	touch_gpio_direction_output(ts->reset_pin, 1);

	touch_msleep(1);
	LCD_RESET_H;
	touch_msleep(1);
	LCD_RESET_L;
	touch_msleep(1);
	LCD_RESET_H;

	touch_msleep(200);
	return 0;
}

static int ft5446_lpwg_mode(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	u8 next_state;
	int ret = 0;

	TOUCH_TRACE();

	if (atomic_read(&d->init) == IC_INIT_NEED) {
		TOUCH_I("Not Ready, Need IC init\n");
		return 0;
	}

	// NORMAL Case
	if (d->state == TC_STATE_ACTIVE) {
		if ((ts->mfts_lpwg) && (atomic_read(&ts->state.fb) == FB_SUSPEND)) {
			next_state = TC_STATE_LPWG;
			TOUCH_I("STATE_ACTIVE to STATE_LPWG (MiniOS/MFTS)\n");

			ft5446_lpwg_control(dev, LPWG_DOUBLE_TAP, false);
			goto out;
		}

		if (ts->lpwg.screen == 0) {
			if (ts->lpwg.sensor == PROX_NEAR) {
				next_state = TC_STATE_DEEP_SLEEP;
				TOUCH_I("STATE_ACTIVE to STATE_DEEP_SLEEP (Proxi Near)\n");

				ret = ft5446_deep_sleep(dev, 1);
			} else if (ts->lpwg.mode == LPWG_NONE
					&& !ts->swipe[SWIPE_U].enable
					&& !ts->swipe[SWIPE_L].enable
					&& !ts->swipe[SWIPE_R].enable) {
				next_state = TC_STATE_DEEP_SLEEP;
				TOUCH_I("STATE_ACTIVE to STATE_DEEP_SLEEP (LPWG_NONE & SWIPE_NONE)\n");

				ret = ft5446_deep_sleep(dev, 1);
			} else {
				next_state = TC_STATE_LPWG; // Do nothing
				TOUCH_I("STATE_ACTIVE to STATE_LPWG");

//				ret = ft5446_lpwg_control(dev, ts->lpwg.mode,
//							(ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable));
			}
		} else {
			next_state = TC_STATE_ACTIVE; // Do nothing
			TOUCH_I("STATE_ACTIVE to STATE_ACTIVE\n");
		}
	} else if (d->state == TC_STATE_LPWG) {
		if (ts->lpwg.screen == 0) {
			if (ts->lpwg.sensor == PROX_NEAR) {
				next_state = TC_STATE_DEEP_SLEEP;
				TOUCH_I("STATE_LPWG to STATE_DEEP_SLEEP (Proxi Near)\n");

				ret = ft5446_deep_sleep(dev, 1);
			} else {
				next_state = TC_STATE_LPWG;
				TOUCH_I("STATE_LPWG to STATE_LPWG\n");
				ret = ft5446_lpwg_control(dev, ts->lpwg.mode,
							(ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable));
			}
		} else {
			next_state = TC_STATE_ACTIVE;
			TOUCH_I("STATE_LPWG to STATE_ACTIVE\n");

			// Report fr before touch IC reset
			if(d->tci_debug_type & LPWG_DEBUG_BUFFER)
				ft5446_report_tci_fr_buffer(dev);
			if(d->swipe_debug_type & LPWG_DEBUG_BUFFER)
				ft5446_report_swipe_fr_buffer(dev);

			touch_report_all_event(ts);
//			ft5446_reset_ctrl(dev, HW_RESET);
		}
	} else if (d->state == TC_STATE_DEEP_SLEEP) {
		if (ts->lpwg.screen == 0) {
			if (ts->lpwg.mode == LPWG_NONE
					&& !ts->swipe[SWIPE_U].enable
					&& !ts->swipe[SWIPE_L].enable
					&& !ts->swipe[SWIPE_R].enable) {
				next_state = TC_STATE_DEEP_SLEEP; // Do nothing
				TOUCH_I("DEEP_SLEEP to DEEP_SLEEP (LPWG_NONE & SWIPE_NONE)\n");
			} else if (ts->lpwg.sensor == PROX_FAR) {
				next_state = TC_STATE_LPWG;
				TOUCH_I("DEEP_SLEEP to STATE_LPWG\n");

				ret = ft5446_deep_sleep(dev, 0);
				ret = ft5446_lpwg_control(dev, ts->lpwg.mode,
							(ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable));
			} else {
				next_state = TC_STATE_DEEP_SLEEP; // Do nothing
				TOUCH_I("DEEP_SLEEP to DEEP_SLEEP\n");
			}
		} else {
			next_state = TC_STATE_ACTIVE;
			TOUCH_I("DEEP_SLEEP to STATE_ACTIVE\n");

			ret = ft5446_deep_sleep(dev, 0);
		}
	} else {
		next_state = d->state;
	}

out:
	TOUCH_I("State changed from [%d] to [%d]\n", d->state, next_state);
	d->state = next_state;

	return ret;

}

static int ft5446_lpwg(struct device *dev, u32 code, void *param)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int *value = (int *)param;

	TOUCH_TRACE();

	switch (code) {
	case LPWG_ACTIVE_AREA:
		ts->tci.area.x1 = value[0];
		ts->tci.area.x2 = value[1];
		ts->tci.area.y1 = value[2];
		ts->tci.area.y2 = value[3];
		TOUCH_I("LPWG_ACTIVE_AREA: x0[%d], x1[%d], x2[%d], x3[%d]\n",
			value[0], value[1], value[2], value[3]);
		break;

	case LPWG_TAP_COUNT:
		ts->tci.info[TCI_2].tap_count = value[0];
		TOUCH_I("LPWG_TAP_COUNT: [%d]\n", value[0]);
		break;

	case LPWG_DOUBLE_TAP_CHECK:
		ts->tci.double_tap_check = value[0];
		TOUCH_I("LPWG_DOUBLE_TAP_CHECK: [%d]\n", value[0]);
		break;

	case LPWG_UPDATE_ALL:
		ts->lpwg.mode = value[0];
		ts->lpwg.screen = value[1];
		ts->lpwg.sensor = value[2];
		ts->lpwg.qcover = value[3];

		TOUCH_I(
			"LPWG_UPDATE_ALL: mode[%d], screen[%s], sensor[%s], qcover[%s]\n",
			value[0],
			value[1] ? "ON" : "OFF",
			value[2] ? "FAR" : "NEAR",
			value[3] ? "CLOSE" : "OPEN");

			ft5446_lpwg_mode(dev);
		break;

	case LPWG_REPLY:
		break;

	}

	return 0;
}

int ft5446_lpwg_set(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);

	mutex_lock(&d->fb_lock);



	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
		ft5446_deep_sleep(dev, 0);

	ft5446_lpwg_mode(dev);

	mutex_unlock(&d->fb_lock);

	return 0;
}

static void ft5446_connect(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int charger_state = atomic_read(&ts->state.connect);
//	int wireless_state = atomic_read(&ts->state.wireless);

	TOUCH_TRACE();

	d->charger = 0;

	/* wire */
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	if (charger_state == CONNECT_INVALID)
		d->charger = CONNECT_NONE;
	else if ((charger_state == CONNECT_DCP)
			|| (charger_state == CONNECT_PROPRIETARY))
		d->charger = CONNECT_TA;
	else if (charger_state == CONNECT_HUB)
		d->charger = CONNECT_OTG;
	else
		d->charger = CONNECT_USB;

	/* Distinguish just TA state or not. */
	if (d->charger == CONNECT_TA || d->charger == CONNECT_OTG || d->charger == CONNECT_USB)
		d->charger = 1;
	else
		d->charger = 0;
#elif defined(CONFIG_LGE_TOUCH_CORE_MTK)
	if (charger_state) {
		d->charger = 1;
	} else {
		d->charger = 0;
	}
#endif

	/* wireless */
	/*
	if (wireless_state)
		d->charger = d->charger | CONNECT_WIRELESS;
	*/

	/* code for TA simulator */
	if (atomic_read(&ts->state.debug_option_mask)
			& DEBUG_OPTION_0) {
		TOUCH_I("TA Simulator mode, Set CONNECT_TA\n");
		d->charger = 1;
	}

	if (ts->lpwg.screen != 0 ) {

		TOUCH_I("%s: write charger_state = 0x%02X\n", __func__, d->charger);
		if (atomic_read(&ts->state.pm) > DEV_PM_RESUME) {
			TOUCH_I("DEV_PM_SUSPEND - Don't try SPI\n");
			return;
		}

		ft5446_reg_write(dev, SPR_CHARGER_STS, &d->charger, sizeof(u8));
	}

}

static int ft5446_usb_status(struct device *dev, u32 mode)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();
	TOUCH_I("TA Type: %d\n", atomic_read(&ts->state.connect));
	ft5446_connect(dev);
	return 0;
}

static int ft5446_debug_option(struct device *dev, u32 *data)
{
	u32 chg_mask = data[0];
	u32 enable = data[1];

	switch (chg_mask) {
	case DEBUG_OPTION_0:
		TOUCH_I("Debug Option 0 - TA Simulator mode %s\n", enable ? "Enable" : "Disable");
		ft5446_connect(dev);
		break;
	case DEBUG_OPTION_1:
		TOUCH_I("Debug Option 1 %s\n", enable ? "Enable" : "Disable");
		break;
	case DEBUG_OPTION_2:
		TOUCH_I("Debug Option 2 %s\n", enable ? "Enable" : "Disable");
		break;
	case DEBUG_OPTION_3:
		TOUCH_I("Debug Option 3 %s\n", enable ? "Enable" : "Disable");
		break;
	case DEBUG_OPTION_4:
		TOUCH_I("Debug Option 4 %s\n", enable ? "Enable" : "Disable");
		break;
	default:
		TOUCH_E("Not supported debug option\n");
		break;
	}

	return 0;
}

static int ft5446_notify(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u8 status = 0x00;
	int ret = 0;

	TOUCH_TRACE();
	TOUCH_I("%s event=0x%x\n", __func__, (unsigned int)event);
	switch (event) {
	case NOTIFY_TOUCH_RESET:
		TOUCH_I("NOTIFY_TOUCH_RESET! - DO NOTHING (Add-on)\n");
		break;
	case NOTIFY_CONNECTION:
		TOUCH_I("NOTIFY_CONNECTION!\n");
		ret = ft5446_usb_status(dev, *(u32 *)data);
		break;
	case NOTIFY_IME_STATE:
		TOUCH_I("NOTIFY_IME_STATE!\n");
		status = atomic_read(&ts->state.ime);
		ret = ft5446_reg_write(dev, REG_IME_STATE, &status, sizeof(status));
		if (ret)
			TOUCH_E("failed to write reg_ime_state, ret : %d\n", ret);
		break;
	case NOTIFY_CALL_STATE:
		TOUCH_I("NOTIFY_CALL_STATE!\n");
		break;
	case NOTIFY_DEBUG_OPTION:
		TOUCH_I("NOTIFY_DEBUG_OPTION!\n");
		ret = ft5446_debug_option(dev, (u32 *)data);
		break;
	case LCD_EVENT_LCD_BLANK:
		TOUCH_I("LCD_EVENT_LCD_BLANK! - DO NOTHING (Add-on)\n");
		break;
	case LCD_EVENT_LCD_UNBLANK:
		TOUCH_I("LCD_EVENT_LCD_UNBLANK! - DO NOTHING (Add-on)\n");
		break;
	case LCD_EVENT_LCD_MODE:
		TOUCH_I("LCD_EVENT_LCD_MODE! - DO NOTHING (Add-on)\n");
		break;
	case LCD_EVENT_READ_REG:
		TOUCH_I("LCD_EVENT_READ_REG - DO NOTHING (Add-on)\n");
		break;
	case LCD_EVENT_LCD_VDDIO_ON:
		touch_power_3_3_vcl(dev, 1); //2.8V vdd power on
		TOUCH_I("LCD_EVENT_LCD_VDDIO_ON - In MFTS, LCD VDDIO and TOUCH VDD is enabled both\n");
		break;
#if 0
	case NOTIFY_WIRELESS:
		TOUCH_I("NOTIFY_WIRELESS!\n");
		ret = ft5446_wireless_status(dev, *(u32 *)data);
		break;
	case NOTIFY_EARJACK:
		TOUCH_I("NOTIFY_EARJACK!\n");
		ret = ft5446_earjack_status(dev, *(u32 *)data);
		break;
	case NOTIFY_ONHAND_STATE:
		TOUCH_I("NOTIFY_ONHAND_STATE!\n");
		break;
	case NOTIFY_QUICKCOVER_STATE:
		TOUCH_I("NOTIFY_QUICKCOVER_STATE!: %d\n", *(u8 *)data);
		/*ret = ft5446_reg_write(dev, SPR_QUICKCOVER_STS, (u8 *)data, sizeof(u8));*/
		break;
#endif
	default:
		TOUCH_E("%lx is not supported\n", event);
		break;
	}

	return ret;
}



static void ft5446_init_locks(struct ft5446_data *d)
{
	mutex_init(&d->rw_lock);
	mutex_init(&d->fb_lock);
}

static int ft5446_probe(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = NULL;

	TOUCH_TRACE();

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);

	if (!d) {
		TOUCH_E("failed to allocate ft5446 data\n");
		return -ENOMEM;
	}

	d->dev = dev;
	touch_set_device(ts, d);

	touch_gpio_init(ts->reset_pin, "touch_reset");
	touch_gpio_direction_output(ts->reset_pin, 0);

	touch_gpio_init(ts->int_pin, "touch_int");
	touch_gpio_direction_input(ts->int_pin);

	touch_power_init(dev);

	touch_bus_init(dev, MAX_BUF_SIZE);

	ft5446_init_locks(d);

	ft5446_get_tci_info(dev);
	ft5446_get_swipe_info(dev);

	d->tci_debug_type = LPWG_DEBUG_BUFFER;
	d->swipe_debug_type = LPWG_DEBUG_BUFFER;

	// To be implemented.....
#ifdef FTS_CTL_IIC
	fts_rw_iic_drv_init(to_i2c_client(dev));
#endif
#ifdef FTS_SYSFS_DEBUG
	fts_create_sysfs(to_i2c_client(dev));
#endif
#ifdef FTS_APK_DEBUG
	fts_create_apk_debug_channel(to_i2c_client(dev));
#endif

	return 0;
}

static int ft5446_remove(struct device *dev)
{
	TOUCH_TRACE();
#ifdef FTS_APK_DEBUG
		fts_release_apk_debug_channel();
#endif

#ifdef FTS_SYSFS_DEBUG
		fts_remove_sysfs(to_i2c_client(dev));
#endif

#ifdef FTS_CTL_IIC
		fts_rw_iic_drv_exit();
#endif
	return 0;
}

static int ft5446_fw_compare(struct device *dev, const struct firmware *fw)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	u8 major = d->ic_info.version.major;
	u8 minor = d->ic_info.version.minor;
	u8 *bin_major = &(d->ic_info.version.bin_major);
	u8 *bin_minor = &(d->ic_info.version.bin_minor);
	u8 fw_version = 0;
	int update = 0;

	TOUCH_TRACE();

	if(d->ic_info.info_valid == 0) { // Failed to get ic info
		TOUCH_I("invalid ic info, skip fw upgrade\n");
		return 0;
	}

	if (is_lcm_name("KD-JD9365Z")) {
		fw_version = fw->data[(u32)(fw->size) - 2];
	} else {
		fw_version = fw->data[0x2000+0x0014];
	}

	if(*bin_major == 0 && *bin_minor == 0) {
		// IF fw ver of bin is not initialized
		*bin_major = (fw_version & 0x80) >> 7;
		*bin_minor = fw_version & 0x7F;
	}

	if (ts->force_fwup) {
		update = 1;
	} else if ((major != d->ic_info.version.bin_major)
			|| (minor != d->ic_info.version.bin_minor)) {
		update = 1;
	}

	TOUCH_I("%s : binary[v%d.%d] device[v%d.%d]" \
		" -> update: %d, force: %d\n", __func__,
		*bin_major, *bin_minor, major, minor,
		update, ts->force_fwup);

	return update;
}

static int ft5446_fwboot_upgrade(struct device *dev, const struct firmware *fw_boot)
{
	struct touch_core_data *ts = to_touch_core(dev);
	const u8 *fw_data = fw_boot->data;
	u32 fw_size = (u32)(fw_boot->size);
	u8 *fw_check_buf = NULL;
	u8 i2c_buf[FTS_PACKET_LENGTH + 12] = {0,};
	int ret;
	int packet_num, i, j, packet_addr, packet_len;
	u8 pramboot_ecc;

	TOUCH_TRACE();
	TOUCH_I("%s - START\n", __func__);

	if(fw_size > 0x10000 || fw_size == 0)
		return -EIO;

	fw_check_buf = kmalloc(fw_size+1, GFP_ATOMIC);
	if(fw_check_buf == NULL)
		return -ENOMEM;

	for (i = 12; i <= 30; i++) {
		// Reset CTPM
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_msleep(50);
		touch_gpio_direction_output(ts->reset_pin, 1);
		touch_msleep(i);

		// Set Upgrade Mode
		ret = ft5446_reg_write(dev, 0x55, i2c_buf, 0);
		if(ret < 0) {
			TOUCH_E("set upgrade mode write error\n");
			goto FAIL;
		}
		touch_msleep(1);

		// Check ID
		TOUCH_I("%s - Set Upgrade Mode and Check ID : %d ms\n", __func__, i);
		ret = ft5446_reg_read(dev, 0x90, i2c_buf, 2);
		if(ret < 0) {
			TOUCH_E("check id read error\n");
			goto FAIL;
		}

		TOUCH_I("Check ID : 0x%x , 0x%x\n", i2c_buf[0], i2c_buf[1]);
		if(i2c_buf[0] == 0x54&& i2c_buf[1] == 0x22){
			touch_msleep(50);
			break;
		}
	}

	if (i > 30) {
		TOUCH_E("timeout to set upgrade mode\n");
		goto FAIL;
	}

	// Write F/W (Pramboot) Binary to CTPM
	TOUCH_I("%s - Write F/W (Pramboot)\n", __func__);
	pramboot_ecc = 0;
	packet_num = (fw_size-8 + FTS_PACKET_LENGTH - 1) / FTS_PACKET_LENGTH;   // FW SIZE -8
	for (i = 0; i < packet_num; i++) {
		packet_addr = i * FTS_PACKET_LENGTH;
		i2c_buf[0] = 0; //(u8)(((u32)(packet_addr) & 0x00FF0000) >> 16);
		i2c_buf[1] = (u8)(((u32)(packet_addr) & 0x0000FF00) >> 8);
		i2c_buf[2] = (u8)((u32)(packet_addr) & 0x000000FF);
		if(packet_addr + FTS_PACKET_LENGTH > fw_size)
			packet_len = fw_size - packet_addr;
		else
			packet_len = FTS_PACKET_LENGTH;
		i2c_buf[3] = (u8)(((u32)(packet_len) & 0x0000FF00) >> 8);
		i2c_buf[4] = (u8)((u32)(packet_len) & 0x000000FF);
		for (j = 0; j < packet_len; j++) {
			i2c_buf[5 + j] = fw_data[packet_addr + j];
			//i2c_buf[5 + (j/4)*4 + (3 - (j%4))] = fw_data[packet_addr + j];        //Kylin20170315 for LGE
			pramboot_ecc ^= i2c_buf[5 + j];
		}
		//TOUCH_I("#%d : Writing to %d , %d bytes\n", i, packet_addr, packet_len); //kjh
		ret = ft5446_reg_write(dev, 0xAE, i2c_buf, packet_len + 5);
		if(ret < 0) {
			TOUCH_E("f/w(Pramboot) binary to CTPM write error\n");
			goto FAIL;
		}
	}

	// Verify F/W
	TOUCH_I("%s - Verify\n", __func__);
	packet_addr = 0xD7F8;
	i2c_buf[0] = 0;;
	i2c_buf[1] = (u8)(((u32)(packet_addr) & 0x0000FF00) >> 8);
	i2c_buf[2] = (u8)((u32)(packet_addr) & 0x000000FF);
	packet_len = 8;
	i2c_buf[3] = (u8)(((u32)(packet_len) & 0x0000FF00) >> 8);
	i2c_buf[4] = (u8)((u32)(packet_len) & 0x000000FF);
	for (j = 0; j < packet_len; j++) {
		i2c_buf[5 + j] = fw_data[(fw_size - 8) + j];
		pramboot_ecc ^= i2c_buf[5 + j];
	}
	//TOUCH_I("#%d : Writing to %d , %d bytes\n", i, packet_addr, packet_len);
	ret = ft5446_reg_write(dev, 0xAE, i2c_buf, packet_len + 5);
	if(ret < 0) {
		TOUCH_E("verify f/w write error\n");
		goto FAIL;
	}
	touch_msleep(20);

	// Read out Checksum
#if 1  // Verify method 1: Check sum
	ret = ft5446_reg_read(dev, 0xCC, i2c_buf, 1);
	if(ret < 0) {
		TOUCH_E("0xCC register read error\n");
		goto FAIL;
	}

	TOUCH_I("Make ecc: 0x%x ,Read ecc 0x%x\n", pramboot_ecc, i2c_buf[0]);

	if(i2c_buf[0] != pramboot_ecc){
		TOUCH_I("Pramboot Verify Failed !!\n" );
		goto FAIL;
	}
	TOUCH_I("%s - Pramboot write Verify OK !!\n", __func__);
#else  // Verify method 2: All data read and compare.
	TOUCH_I("%s - Verify\n", __func__);
	for (i = 0; i < packet_num; i++) {
		i2c_buf[0] = 0x85;
		packet_addr = i * FTS_PACKET_LENGTH;
		i2c_buf[1] = 0; //(u8)(((u32)(packet_addr) & 0x00FF0000) >> 16);
		i2c_buf[2] = (u8)(((u32)(packet_addr) & 0x0000FF00) >> 8);
		i2c_buf[3] = (u8)((u32)(packet_addr) & 0x000000FF);
		if(packet_addr + FTS_PACKET_LENGTH > fw_size)
			packet_len = fw_size - packet_addr;
		else
			packet_len = FTS_PACKET_LENGTH;
		//TOUCH_I("#%d : Reading from %d , %d bytes\n", i, packet_addr, packet_len);
		ret = ft5446_cmd_read(dev, i2c_buf, 4, fw_check_buf+packet_addr, packet_len);
		if(ret < 0) {
			TOUCH_E("i2c error\n");
			goto FAIL;
		}
	}
	for (i = 0; i < fw_size; i++) {
		if(fw_check_buf[i] != fw_data[i]) {
			TOUCH_I("%s - Verify Failed !!\n", __func__);
			goto FAIL;
		}
	}
	TOUCH_I("%s - Pramboot write Verify OK !!\n", __func__);
#endif

	// Start App
	TOUCH_I("%s - Start App\n", __func__);
	ret = ft5446_reg_write(dev, 0x08, i2c_buf, 0);
	if(ret < 0) {
		TOUCH_E("Start App error\n");
		goto FAIL;
	}
	touch_msleep(10);
	if(fw_check_buf)
		kfree(fw_check_buf);

	TOUCH_I("===== Firmware (Pramboot) download Okay =====\n");

	return 0;

FAIL :

	if(fw_check_buf)
		kfree(fw_check_buf);

	TOUCH_I("===== Firmware (Pramboot) download FAIL!!! =====\n");

	return -EIO;

}

static int ft5446_fw_upgrade(struct device *dev, const struct firmware *fw)
{
	struct touch_core_data *ts = to_touch_core(dev);
	const u8 *fw_data = fw->data;
	u32 fw_size = (u32)(fw->size);
	u8 i2c_buf[FTS_PACKET_LENGTH + 12] = {0,};
	int ret;
	int packet_num, retry, i, j, packet_addr, packet_len;
	u8 fw_ecc;
	u8 data = 0;

	TOUCH_TRACE();
	TOUCH_I("%s - START\n", __func__);

	// Enter Upgrade Mode and ID check
	for (i = 0; i < 30; i++) {
		// Enter Upgrade Mode
		TOUCH_I("%s - Enter Upgrade Mode and Check ID\n", __func__);

		ret = ft5446_reg_write(dev, 0x55, i2c_buf, 0);
		if(ret < 0) {
			TOUCH_E("upgrade mode enter and check ID error\n");
			return -EIO;
		}
		touch_msleep(1);

		// Check ID

		ret = ft5446_reg_read(dev, 0x90, i2c_buf, 2);

		if(ret < 0) {
			TOUCH_E("check ID error\n");
			return -EIO;
		}

		TOUCH_I("Check ID [%d] : 0x%x , 0x%x\n", i, i2c_buf[0], i2c_buf[1]);
		if (is_lcm_name("KD-JD9365Z")) {
			if(i2c_buf[0] == 0x54 && i2c_buf[1] == 0x2B)
				break;
		} else {
			if(i2c_buf[0] == 0x54 && i2c_buf[1] == 0x2D)
				break;
		}
		touch_msleep(10);
	}

	if (i == 30) {
		TOUCH_E("timeout to set upgrade mode\n");
		goto FAIL;
	}


	// Change to write flash set range
	i2c_buf[0] = 0x0A; //- 0x0A : All, 0x0B : App, 0x0C : Lcd
	ret = ft5446_reg_write(dev, 0x09, i2c_buf, 1);
	if(ret < 0) {
		TOUCH_E("change to write flash set range error\n");
		goto FAIL;
	}
	touch_msleep(50);

	// Erase start
	TOUCH_I("%s - Erase All  Area\n", __func__);

	ret = ft5446_reg_write(dev, 0x61, i2c_buf, 0);
	if(ret < 0) {
		TOUCH_E("Erase All Area error\n");
		goto FAIL;
	}

	touch_msleep(1000);

	retry = 300;
	i = 0;
	do {
		ret = ft5446_reg_read(dev, 0x6A, i2c_buf, 2);
		if(ret < 0) {
			TOUCH_E("0x6A register 2byte read error\n");
			goto FAIL;
		}

		if(i2c_buf[0] == 0xF0 && i2c_buf[1] == 0xAA)
		{
			TOUCH_I("Erase Done : %d \n", i);
			break;
		}
		i++;
		mdelay(20);
	} while (--retry);

	//  Write F/W (All or App) Binary to CTPM
	TOUCH_I("%s - Write F/W (App)\n", __func__);
	fw_ecc = 0;
	packet_num = (fw_size + FTS_PACKET_LENGTH - 1) / FTS_PACKET_LENGTH;
	for (i = 0; i < packet_num; i++) {
		packet_addr = i * FTS_PACKET_LENGTH;
		i2c_buf[0] = (u8)(((u32)(packet_addr) & 0x00FF0000) >> 16);
		i2c_buf[1] = (u8)(((u32)(packet_addr) & 0x0000FF00) >> 8);
		i2c_buf[2] = (u8)((u32)(packet_addr) & 0x000000FF);
		if(packet_addr + FTS_PACKET_LENGTH > fw_size)
			packet_len = fw_size - packet_addr;
		else
			packet_len = FTS_PACKET_LENGTH;
		i2c_buf[3] = (u8)(((u32)(packet_len) & 0x0000FF00) >> 8);
		i2c_buf[4] = (u8)((u32)(packet_len) & 0x000000FF);
		for (j = 0; j < packet_len; j++) {
			i2c_buf[5 + j] = fw_data[packet_addr + j];
			fw_ecc ^= i2c_buf[5 + j];
		}
#if 0
		TOUCH_I("#%d : Writing to %d , %d bytes..[0x%02x][0x%02x][0x%02x][0x%02x][0x%02x]\n",
				i, packet_addr, packet_len,
				i2c_buf[0], i2c_buf[1], i2c_buf[2], i2c_buf[3], i2c_buf[4]);
#endif
		ret = ft5446_reg_write(dev, 0xBF, i2c_buf, packet_len + 5);
		if(ret < 0) {
			TOUCH_E("write f/w(app) binary to CTPM error\n");
			goto FAIL;
		}
		//touch_msleep(10);

		// Waiting
		retry = 100;
		do {
			ret = ft5446_reg_read(dev, 0x6A, i2c_buf, 2);
			if(ret < 0) {
				TOUCH_E("wating error\n");
				goto FAIL;
			}

			if((u32)(i + 0x1000) == (((u32)(i2c_buf[0]) << 8) | ((u32)(i2c_buf[1]))))
			{
				if((i & 0x007F) == 0) {
					TOUCH_I("Write Done : %d / %d\n", i+1, packet_num);
				}
				break;
			}
			//touch_msleep(1);
			mdelay(1);
		} while (--retry);
		if(retry == 0) {
			TOUCH_I("Write Done, Max packet delay : %d / %d : [0x%02x] , [0x%02x]\n", i+1, packet_num, i2c_buf[0], i2c_buf[1]);
			//goto FAIL;
		}
	}
	TOUCH_I("Write Finished : Total %d\n", packet_num);

	touch_msleep(50);

	// Read out Checksum
	TOUCH_I("%s - Read out checksum (App) for %d bytes\n", __func__, fw_size);
	ret = ft5446_reg_write(dev, 0x64, i2c_buf, 0);
	if(ret < 0) {
		TOUCH_E("read out checksum error\n");
		goto FAIL;
	}

	touch_msleep(5);

	packet_len=0;

	if (is_lcm_name("KD-JD9365Z")) {
		i2c_buf[0] = (u8)(((u32)(packet_len) & 0x00FF0000) >> 16);
		i2c_buf[1] = (u8)(((u32)(packet_len) & 0x0000FF00) >> 8);
		i2c_buf[2] = (u8)((u32)(packet_len) & 0x000000FF);
		packet_len = fw_size;
		i2c_buf[3] = (u8)(((u32)(packet_len) & 0x0000FF00) >> 8);
		i2c_buf[4] = (u8)((u32)(packet_len) & 0x000000FF);
		ret = ft5446_reg_write(dev, 0x65, i2c_buf, 5);
		if(ret < 0) {
			TOUCH_E("i2c error\n");
			goto FAIL;
		}
	} else {
		i2c_buf[0] = (u8)(((u32)(packet_len) & 0x00FF0000) >> 16);
		i2c_buf[1] = (u8)(((u32)(packet_len) & 0x0000FF00) >> 8);
		i2c_buf[2] = (u8)((u32)(packet_len) & 0x000000FF);
		packet_len = fw_size;
		i2c_buf[3] = (u8)(((u32)(packet_len) & 0x00FF0000) >> 16);
		i2c_buf[4] = (u8)(((u32)(packet_len) & 0x0000FF00) >> 8);
		i2c_buf[5] = (u8)((u32)(packet_len) & 0x000000FF);
		ret = ft5446_reg_write(dev, 0x65, i2c_buf, 6);
		if(ret < 0) {
			TOUCH_E("i2c error\n");
			goto FAIL;
		}

	}

	touch_msleep(200);

	retry = 200;
	do {
		ret = ft5446_reg_read(dev, 0x6A, i2c_buf, 2);
		if(ret < 0) {
			TOUCH_E("0x6A register 2byte read error\n");
			goto FAIL;
		}

		if(i2c_buf[0] == 0xF0 && i2c_buf[1] == 0x55)
		{
			TOUCH_I("Checksum Calc. Done\n");
			break;
		}
		touch_msleep(10);
	} while (--retry);

	ret = ft5446_reg_read(dev, 0x66, i2c_buf, 1);
	if(ret < 0) {
		TOUCH_E("0x66 register read error\n");
		goto FAIL;
	}
	TOUCH_I("Reg 0x66 : 0x%x\n", i2c_buf[0]);

	if(i2c_buf[0] != fw_ecc)
	{
		TOUCH_E("Checksum ERROR : Reg 0x66 [0x%x] , fw_ecc [0x%x]\n", i2c_buf[0], fw_ecc);
		goto FAIL;
	}

	TOUCH_I("Checksum OK : Reg 0x66 [0x%x] , fw_ecc [0x%x]\n", i2c_buf[0], fw_ecc);

	TOUCH_I("===== Firmware download OK!!! =====\n");

	// Exit download mode
	ret = ft5446_reg_write(dev, 0x07, &data, 0);
	touch_msleep(ts->caps.hw_reset_delay);
	if(ret < 0) {
		TOUCH_E("Exit download mode write fail, ret = %d\n", ret);
		goto FAIL;
	}

	return 0;

FAIL:

	TOUCH_I("===== Firmware download FAIL!!! =====\n");

	// Exit download mode
	ret = ft5446_reg_write(dev, 0x07, &data, 0);
	touch_msleep(ts->caps.hw_reset_delay);
	if(ret < 0)
		TOUCH_E("Exit download mode write fail, ret = %d\n", ret);

	return -EIO;

}


static int ft5446_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	const struct firmware *fw = NULL;
	const struct firmware *fw_boot = NULL;
	char fwpath[256] = {0};
	int ret = 0, retry = 0;
	int boot_mode = TOUCH_NORMAL_BOOT;
//	int i = 0;

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);
	if((boot_mode == TOUCH_LAF_MODE) || (boot_mode == TOUCH_RECOVERY_MODE) ||
			(boot_mode == TOUCH_CHARGER_MODE)) {
		TOUCH_I("skip fw upgrade : %d (CHARGER:5/LAF:6/RECOVER_MODE:7)\n", boot_mode);
		return -EPERM;

	}

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		return -EPERM;
	}

	if (ts->test_fwpath[0] && (ts->test_fwpath[0] != 't' && ts->test_fwpath[1] != 0)) {
		memcpy(fwpath, &ts->test_fwpath[0], sizeof(fwpath));
		TOUCH_I("get fwpath from test_fwpath:%s\n", &ts->test_fwpath[0]);
	} else if (ts->def_fwcnt) {
		memcpy(fwpath, ts->def_fwpath[1], sizeof(fwpath)); // 0 : pramboot bin, 1 : FT5446 all bin
		if(is_lcm_name("HLT-JD9365D")) {
			TOUCH_I("get board_rev: %d", lge_get_board_revno());
			if (lge_get_board_revno() < HW_REV_1_0) {
				memcpy(fwpath, ts->def_fwpath[2], sizeof(fwpath));
			}
		}
		TOUCH_I("get fwpath from def_fwpath : %s\n", fwpath);
	} else {
		TOUCH_E("no firmware file\n");
		return -EPERM;
	}

	fwpath[sizeof(fwpath)-1] = '\0';

	if (strlen(fwpath) <= 0) {
		TOUCH_E("error get fw path\n");
		return -EPERM;
	}

	TOUCH_I("fwpath[%s]\n", fwpath);

	ret = request_firmware(&fw, fwpath, dev);

	if (ret) {
		TOUCH_E("fail to request_firmware fwpath: %s (ret:%d)\n", fwpath, ret);
		goto error;
	}

	TOUCH_I("fw size:%zu, data: %p\n", fw->size, fw->data);

	if (ft5446_fw_compare(dev, fw)) {

		TOUCH_I("fwpath_boot[%s]\n", ts->def_fwpath[0]);  // PARAMBOOT.BIN
		ret = request_firmware(&fw_boot, ts->def_fwpath[0], dev);
		if (ret) {
			TOUCH_E("fail to request_firmware fwpath: %s (ret:%d)\n", ts->def_fwpath[0], ret);
			goto error;
		}

		do {
			ret = ft5446_fwboot_upgrade(dev, fw_boot);
			ret += ft5446_fw_upgrade(dev, fw);

			if (ret >= 0)
				break;

			TOUCH_E("fail to upgrade f/w - ret: %d, retry: %d\n", ret, retry);
		} while (++retry < 3);

		if (retry >= 3) {
			goto error;
		}
		TOUCH_I("f/w upgrade complete\n");
	}
error:
	if(fw_boot ==  NULL)
		release_firmware(fw);
	else{
		release_firmware(fw);
		release_firmware(fw_boot);
	}

	return -EPERM; // Return non-zero to not reset
}

static int ft5446_suspend(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int mfts_mode = 0;
	int ret = 0;

	TOUCH_TRACE();

	mfts_mode = touch_check_boot_mode(dev);
	if ((mfts_mode >= TOUCH_MINIOS_MFTS_FOLDER && mfts_mode <= TOUCH_MINIOS_MFTS_CURVED) && !ts->mfts_lpwg) {
		d->state = TC_STATE_POWER_OFF;
		TOUCH_I("STATE_ACTIVE to STATE_POWER_OFF (MFTS MODE)\n");
		ft5446_power(dev, POWER_OFF);
		return -1;
	} else if (mfts_mode == TOUCH_CHARGER_MODE) {
		return -1;
	}

	if (atomic_read(&d->init) == IC_INIT_DONE)
		ft5446_lpwg_mode(dev);
	else /* need init */
		ret = 1;

	return ret;
}

static int ft5446_resume(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int mfts_mode = 0;

	TOUCH_TRACE();

	mfts_mode = touch_check_boot_mode(dev);
	if ((mfts_mode >= TOUCH_MINIOS_MFTS_FOLDER && mfts_mode <= TOUCH_MINIOS_MFTS_CURVED) && !ts->mfts_lpwg) {
		d->state = TC_STATE_ACTIVE;
		TOUCH_I("STATE_POWER_OFF to STATE_ACTIVE (MFTS MODE)\n");
		ft5446_power(dev, POWER_ON);
		return 0;
	} else if (mfts_mode == TOUCH_CHARGER_MODE) {
		return -1;
	}

	ft5446_reset_ctrl(dev, HW_RESET);
	return 0;
}

static int ft5446_init(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	u8 data = 0;
	int ret = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s - when we enter deep sleep, don't init func\n", __func__);

		return -EINVAL;
	}

	ret = ft5446_ic_info(dev);
	if (ret < 0) {
		TOUCH_I("failed to get ic_info, ret:%d\n", ret);
		return ret;
	}

	TOUCH_I("%s: charger_state = 0x%02X\n", __func__, d->charger);
	ret = ft5446_reg_write(dev, SPR_CHARGER_STS, &d->charger, sizeof(u8));
	if (ret)
		TOUCH_E("failed to write \'spr_charger_sts\', ret:%d\n", ret);

	data = atomic_read(&ts->state.ime);
	TOUCH_I("%s: ime_state = %d\n", __func__, data);
	ret = ft5446_reg_write(dev, REG_IME_STATE, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'reg_ime_state\', ret:%d\n", ret);

	atomic_set(&d->init, IC_INIT_DONE);
	atomic_set(&ts->state.sleep, IC_NORMAL);
	d->state = TC_STATE_ACTIVE;

	ft5446_lpwg_mode(dev);
	if (ret)
		TOUCH_E("failed to lpwg_control, ret:%d\n", ret);

	if(apk_allocate_flag == 1) {

#ifdef CONFIG_MTK_I2C_EXTENSION
		msg_dma_alloct(dev);
#endif


#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT

		if (NULL == tpd_i2c_dma_va)
		{
		touch_msleep(3000);
			if (ts->input != NULL ) {

				dma_set_coherent_mask(&ts->input->dev, DMA_BIT_MASK(32));
				ts->input->dev.coherent_dma_mask = DMA_BIT_MASK(32);
				tpd_i2c_dma_va = (u8 *)dma_alloc_coherent(&ts->input->dev, 250, &tpd_i2c_dma_pa, GFP_KERNEL);
				//tpd_i2c_dma_va = (u8 *)dma_alloc_coherent(NULL, 250, &tpd_i2c_dma_pa, GFP_KERNEL);
			} else {
				TOUCH_E ( "ts->input is NULL\n");
			}
		}
		if (!tpd_i2c_dma_va)
			TOUCH_E("dma_alloc_coherent error!\n");
		else
			TOUCH_I("dma_alloc_coherent success!\n");

#endif


#ifdef CONFIG_MTK_I2C_EXTENSION
		//msg_dma_release();
#endif

		apk_allocate_flag = 0;

	}

	return 0;
}

static int ft5446_irq_abs_data(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	struct ft5446_touch_data *data = d->info.data;
	struct touch_data *tdata;
	u32 touch_count = 0;
	u8 finger_index = 0;
	int i = 0;
	u8 touch_id, event, palm;
	//static u8 z_toggle;

	touch_count = d->info.touch_cnt;
	ts->new_mask = 0;
	//z_toggle ^= 0x1;

	for (i = 0; i < FTS_MAX_POINTS; i++) {
		touch_id = (u8)(data[i].yh) >> 4;
		if (touch_id >= FTS_MAX_ID) {
			break; // ??
		}

		event = (u8)(data[i].xh) >> 6;
		palm = ((u8)(data[i].xh) >> 4) & 0x01;

		if (palm) {
			if (event == FTS_TOUCH_CONTACT) { // FTS_TOUCH_DOWN
				ts->is_cancel = 1;
				TOUCH_I("Palm Detected\n");
			}
			else if (event == FTS_TOUCH_UP) {
				ts->is_cancel = 0;
				TOUCH_I("Palm Released\n");
			}
			ts->tcount = 0;
			ts->intr_status = TOUCH_IRQ_FINGER;
			return 0;
		}

		if(event == FTS_TOUCH_DOWN || event == FTS_TOUCH_CONTACT) {
			ts->new_mask |= (1 << touch_id);
			tdata = ts->tdata + touch_id;

			tdata->id = touch_id;
			tdata->type = MT_TOOL_FINGER;
			tdata->x = ((u16)(data[i].xh & 0x0F))<<8 | (u16)(data[i].xl);
			tdata->y = ((u16)(data[i].yh & 0x0F))<<8 | (u16)(data[i].yl);
			tdata->pressure = (u8)(data[i].weight);// + z_toggle;
			tdata->width_major = 0;
			tdata->width_minor = 0;
			tdata->orientation = 0;

			finger_index++;

			TOUCH_D(ABS, "tdata [id:%d e:%d x:%d y:%d z:%d - %d,%d,%d]\n",\
					tdata->id,\
					event,\
					tdata->x,\
					tdata->y,\
					tdata->pressure,\
					tdata->width_major,\
					tdata->width_minor,\
					tdata->orientation);

		}
	}

#ifdef FT5446_ESD_SKIP_WHILE_TOUCH_ON
	if (finger_index != finger_cnt) {
//		TOUCH_I("finger cnt changed from %d to %d\n", finger_cnt, finger_index);
		finger_cnt = finger_index;
	}
#endif

	ts->tcount = finger_index;
	ts->intr_status = TOUCH_IRQ_FINGER;

	return 0;
}

int ft5446_irq_abs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	struct ft5446_touch_data *data = d->info.data;

	u8 point_buf[POINT_READ_BUF] = { 0, };
	int ret = -1;

	ret = ft5446_reg_read(dev, 0, point_buf, POINT_READ_BUF);

	if (ret < 0) {
		TOUCH_E("Fail to read point regs.\n");
		return ret;
	}

	/* check if touch cnt is valid */
	if (/*point_buf[FTS_TOUCH_P_NUM] == 0 || */point_buf[FTS_TOUCH_P_NUM] > ts->caps.max_id) {
		TOUCH_I("%s : touch cnt is invalid - %d\n",
			__func__, point_buf[FTS_TOUCH_P_NUM]);
		return -ERANGE;
	}

	d->info.touch_cnt = point_buf[FTS_TOUCH_P_NUM];

	memcpy(data, point_buf+FTS_TOUCH_EVENT_POS, FTS_ONE_TCH_LEN * FTS_MAX_POINTS);

	return ft5446_irq_abs_data(dev);
}

int ft5446_get_tci_data(struct device *dev, int tci) {
	struct touch_core_data *ts = to_touch_core(dev);
	//struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0, tap_count, i, j;
	u8 tci_data_buf[MAX_TAP_COUNT*4 + 2];

	TOUCH_TRACE();

	if(ts->lpwg.mode == LPWG_NONE || (ts->lpwg.mode == LPWG_DOUBLE_TAP && tci == TCI_2)) {
		TOUCH_I("lpwg irq is invalid!!\n");
		return -1;
	}

	ret = ft5446_reg_read(dev, 0xD3, tci_data_buf, 2);
	if (ret < 0) {
		TOUCH_E("Fail to read tci data\n");
		return ret;
	}

	TOUCH_I("TCI Data : TCI[%d], Result[%d], TapCount[%d]\n", tci, tci_data_buf[0], tci_data_buf[1]);

	// Validate tci data
	if (!((tci_data_buf[0] == 0x01 && tci == TCI_1) || (tci_data_buf[0] == 0x02 && tci == TCI_2))
		|| tci_data_buf[1] == 0 || tci_data_buf[1] > MAX_TAP_COUNT) {
		TOUCH_I("tci data is invalid!!\n");
		return -1;
	}

	tap_count = tci_data_buf[1];

	ret = ft5446_reg_read(dev, 0xD3, tci_data_buf, tap_count*4 + 2);
	if (ret < 0) {
		TOUCH_E("Fail to read tci data\n");
		return ret;
	}

	ts->lpwg.code_num = tap_count;
	for (i = 0; i < tap_count; i++) {
		j = i*4+2;
		ts->lpwg.code[i].x = ((int)tci_data_buf[j] << 8) | (int)tci_data_buf[j+1];
		ts->lpwg.code[i].y = ((int)tci_data_buf[j+2] << 8) | (int)tci_data_buf[j+3];

		if ( (ts->lpwg.mode >= LPWG_PASSWORD) && (ts->role.hide_coordinate) )
			TOUCH_I("LPWG data xxxx, xxxx\n");
		else
			TOUCH_I("LPWG data %d, %d\n", ts->lpwg.code[i].x, ts->lpwg.code[i].y);
	}
	ts->lpwg.code[tap_count].x = -1;
	ts->lpwg.code[tap_count].y = -1;

	return ret;
}

int ft5446_get_swipe_data(struct device *dev, int swipe) {
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u8 swipe_data_buf[10] = {0, };
	u16 start_x, start_y, end_x, end_y, time;

	TOUCH_TRACE();

	if (ts->swipe[SWIPE_D].enable == false &&
			ts->swipe[SWIPE_U].enable == false &&
			ts->swipe[SWIPE_L].enable == false &&
			ts->swipe[SWIPE_R].enable == false) {
		TOUCH_I("swipe irq is invalid!!\n");
		return -1;
	}

	ret = ft5446_reg_read(dev, 0x02, swipe_data_buf, sizeof(swipe_data_buf));
	if (ret < 0) {
		TOUCH_E("Fail to read swipe data\n");
		return ret;
	}

	start_x = swipe_data_buf[0] | ((u16)swipe_data_buf[1] << 8);
	start_y = swipe_data_buf[2] | ((u16)swipe_data_buf[3] << 8);
	end_x = swipe_data_buf[4] | ((u16)swipe_data_buf[5] << 8);
	end_y = swipe_data_buf[6] | ((u16)swipe_data_buf[7] << 8);
	time = swipe_data_buf[8] | ((u16)swipe_data_buf[9] << 8);

	TOUCH_I("SWIPE Data : SWIPE_DIR[%d], start(%d, %d), end(%d, %d), time(%dms)\n",
			swipe, start_x, start_y, end_x, end_y, time);

	return 0;
}

int ft5446_irq_report_tci_fr(struct device *dev)
{
	//struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	u8 data, tci_1_fr;
#ifdef KNOCK_CODE
	u8 tci_2_fr;
#endif

	TOUCH_TRACE();

	if (d->tci_debug_type != LPWG_DEBUG_ALWAYS && d->tci_debug_type != LPWG_DEBUG_BUFFER_ALWAYS) {
		TOUCH_I("tci debug in real time is disabled!!\n");
		return 0;
	}

	ret = ft5446_reg_read(dev, 0xE6, &data, 1);
	if (ret < 0) {
		TOUCH_E("i2c error\n");
		return ret;
	}

	tci_1_fr = data & 0x0F; // TCI_1

	if (tci_1_fr < TCI_FAILREASON_NUM) {
		TOUCH_I("Knock-on Failure Reason Reported : [%s]\n", tci_debug_str[tci_1_fr]);
	}
	else {
		TOUCH_I("Knock-on Failure Reason Reported : [%s]\n", tci_debug_str[TCI_FAILREASON_NUM]);
	}

#ifdef KNOCK_CODE
	tci_2_fr = (data & 0xF0) >> 4; // TCI_2

	if (tci_2_fr < TCI_FAILREASON_NUM) {
		TOUCH_I("Knock-code Failure Reason Reported : [%s]\n", tci_debug_str[tci_2_fr]);
	}
	else {
		TOUCH_I("Knock-code Failure Reason Reported : [%s]\n", tci_debug_str[TCI_FAILREASON_NUM]);
	}
#endif

	return ret;
}

int ft5446_irq_report_swipe_fr(struct device *dev)
{
	//struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	u8 data, swipe_dir, swipe_fr;

	TOUCH_TRACE();

	if (d->swipe_debug_type != LPWG_DEBUG_ALWAYS && d->swipe_debug_type != LPWG_DEBUG_BUFFER_ALWAYS) {
		TOUCH_I("swipe debug in real time is disabled!!\n");
		return 0;
	}

	ret = ft5446_reg_read(dev, 0x96, &data, 1);
	if (ret < 0) {
		TOUCH_E("i2c error\n");
		return ret;
	}

	swipe_dir = (data & 0xF0) >> 4; // Direction
	swipe_fr = data & 0x0F; // Error code

	if (swipe_dir < SWIPE_FAILREASON_DIR_NUM && swipe_fr < SWIPE_FAILREASON_NUM) {
		TOUCH_I("Swipe Failure Reason Reported : [%s][%s]\n",
				swipe_debug_direction_str[swipe_dir],
				swipe_debug_str[swipe_fr]);
	}
	else {
		TOUCH_I("Swipe Failure Reason Reported : [%s][%s]\n",
				swipe_debug_direction_str[SWIPE_FAILREASON_DIR_NUM],
				swipe_debug_str[SWIPE_FAILREASON_NUM]);
	}

	return ret;
}

int ft5446_report_tci_fr_buffer(struct device *dev)
{
	//struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0, i;
	u8 tci_fr_buffer[1 + TCI_FAILREASON_BUF_LEN], tci_fr_cnt, tci_fr;

	TOUCH_TRACE();

	if (d->tci_debug_type != LPWG_DEBUG_BUFFER && d->tci_debug_type != LPWG_DEBUG_BUFFER_ALWAYS) {
		TOUCH_I("tci debug in buffer is disabled!!\n");
		return 0;
	}

	// Knock-on
	for (i = 0; i < 25; i++) {
		ret = ft5446_reg_read(dev, 0xE7, &tci_fr_buffer, sizeof(tci_fr_buffer));
		if (!ret) {
			break;
		}
		msleep(2);
	}
	if (i == 25) {
		TOUCH_E("i2c error\n");
		return ret;
	}

	tci_fr_cnt = tci_fr_buffer[0];
	if (tci_fr_cnt > TCI_FAILREASON_BUF_LEN) {
		TOUCH_I("Knock-on Failure Reason Buffer Count Invalid\n");
	}
	else if (tci_fr_cnt == 0) {
		TOUCH_I("Knock-on Failure Reason Buffer NONE\n");
	}
	else {
		for (i = 0; i < tci_fr_cnt; i++) {
			tci_fr = tci_fr_buffer[1 + i];
			if (tci_fr < TCI_FAILREASON_NUM) {
				TOUCH_I("Knock-on Failure Reason Buffer [%02d] : [%s]\n", i+1, tci_debug_str[tci_fr]);
			}
			else {
				TOUCH_I("Knock-on Failure Reason Buffer [%02d] : [%s]\n", i+1, tci_debug_str[TCI_FAILREASON_NUM]);
			}
		}
	}
#ifdef KNOCK_CODE
	// Knock-code (Same as knock-on case except for reg addr)
	for (i = 0; i < 25; i++) {
		ret = ft5446_reg_read(dev, 0xE9, &tci_fr_buffer, sizeof(tci_fr_buffer));
		if (!ret) {
			break;
		}
		msleep(2);
	}
	if (i == 25) {
		TOUCH_E("i2c error\n");
		return ret;
	}

	tci_fr_cnt = tci_fr_buffer[0];
	if (tci_fr_cnt > TCI_FAILREASON_BUF_LEN) {
		TOUCH_I("Knock-code Failure Reason Buffer Count Invalid\n");
	}
	else if (tci_fr_cnt == 0) {
		TOUCH_I("Knock-code Failure Reason Buffer NONE\n");
	}
	else {
		for (i = 0; i < tci_fr_cnt; i++) {
			tci_fr = tci_fr_buffer[1 + i];
			if (tci_fr < TCI_FAILREASON_NUM) {
				TOUCH_I("Knock-code Failure Reason Buffer [%02d] : [%s]\n", i+1, tci_debug_str[tci_fr]);
			}
			else {
				TOUCH_I("Knock-code Failure Reason Buffer [%02d] : [%s]\n", i+1, tci_debug_str[TCI_FAILREASON_NUM]);
			}
		}
	}
#endif

	return ret;
}

int ft5446_report_swipe_fr_buffer(struct device *dev)
{
	//struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0, i;
	u8 swipe_fr_buffer[1 + SWIPE_FAILREASON_BUF_LEN];
	u8 swipe_fr_cnt;
	u8 data, swipe_dir, swipe_fr;

	TOUCH_TRACE();

	if (d->swipe_debug_type != LPWG_DEBUG_BUFFER && d->swipe_debug_type != LPWG_DEBUG_BUFFER_ALWAYS) {
		TOUCH_I("swipe debug in buffer is disabled!!\n");
		return 0;
	}

	// Knock-on
	for (i = 0; i < 25; i++) {
		ret = ft5446_reg_read(dev, 0x93, &swipe_fr_buffer, sizeof(swipe_fr_buffer));
		if (!ret) {
			break;
		}
		msleep(2);
	}
	if (i == 25) {
		TOUCH_E("i2c error\n");
		return ret;
	}

	swipe_fr_cnt = swipe_fr_buffer[0];
	if (swipe_fr_cnt > SWIPE_FAILREASON_BUF_LEN) {
		TOUCH_I("Swipe Failure Reason Buffer Count Invalid\n");
	}
	else if (swipe_fr_cnt == 0) {
		TOUCH_I("Swipe Failure Reason Buffer NONE\n");
	}
	else {
		for (i = 0; i < swipe_fr_cnt; i++) {
			data = swipe_fr_buffer[1 + i];
			swipe_dir = (data & 0xF0) >> 4; // Direction
			swipe_fr = data & 0x0F; // Error code

			if (swipe_dir < SWIPE_FAILREASON_DIR_NUM && swipe_fr < SWIPE_FAILREASON_NUM) {
				TOUCH_I("Swipe Failure Reason Buffer [%02d] : [%s][%s]\n",
						i+1,
						swipe_debug_direction_str[swipe_dir],
						swipe_debug_str[swipe_fr]);
			}
			else {
				TOUCH_I("Swipe Failure Reason Buffer [%02d] : [%s][%s]\n",
						i+1,
						swipe_debug_direction_str[SWIPE_FAILREASON_DIR_NUM],
						swipe_debug_str[SWIPE_FAILREASON_NUM]);
			}
		}
	}

	return ret;
}

int ft5446_irq_handler(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u8 int_status = 0;

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_E("read status error but touch ic sleep. Do nothing.\n");
		return -EPERM;
	}

	ret = ft5446_reg_read(dev, 0x01, &int_status, 1);
	if (ret < 0)
		return ret;

	switch (int_status) {
		case 0x01: // Finger
			ret = ft5446_irq_abs(dev);
			break;
		case 0x02: // TCI_1
			ts->intr_status = TOUCH_IRQ_KNOCK;
			ret = ft5446_get_tci_data(dev, TCI_1);
			break;
#ifdef KNOCK_CODE
		case 0x03: // TCI_2
			ts->intr_status = TOUCH_IRQ_PASSWD;
			ret = ft5446_get_tci_data(dev, TCI_2);
			break;
#endif
		case 0x04: // LPWG Fail Reason Report (RealTime)
			ret = ft5446_irq_report_tci_fr(dev);
			break;
		case 0x05: // ESD
			TOUCH_I("ESD interrupt !!\n");
			ret = -EHWRESET_ASYNC;
			break;
		case 0x06: // Swipe Up
			ts->intr_status = TOUCH_IRQ_SWIPE_UP; 
			ret = ft5446_get_swipe_data(dev, SWIPE_U);
			break;
		case 0x07: // Swipe Down
			ts->intr_status = TOUCH_IRQ_SWIPE_DOWN;
			ret = ft5446_get_swipe_data(dev, SWIPE_D);
			break;
		case 0x08: // Swipe Left
			ts->intr_status = TOUCH_IRQ_SWIPE_UP;
			ret = ft5446_get_swipe_data(dev, SWIPE_L);
			break;
		case 0x09: // Swipe Right
			ts->intr_status = TOUCH_IRQ_SWIPE_UP;
			ret = ft5446_get_swipe_data(dev, SWIPE_R);
			break;
		case 0x0A: // Swipe Fail Reason Report (RealTime)
			ret = ft5446_irq_report_swipe_fr(dev);
			break;
		case 0x00:
			TOUCH_I("No interrupt status\n");
			break;
		default:
			TOUCH_E("Invalid interrupt status : %d\n", int_status);
			ret = -EHWRESET_ASYNC;
			break;
	}

	return ret;

}

int ft5446_reset_ctrl(struct device *dev, int ctrl)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u8 data = 0;

	TOUCH_TRACE();

	switch (ctrl) {
	case SW_RESET:
		TOUCH_I("%s : SW Reset\n", __func__);
		data = 0x55;
		ft5446_reg_write(dev, 0xFC, &data, 1);
		touch_msleep(10);
		data = 0x66;
		ft5446_reg_write(dev, 0xFC, &data, 1);
		touch_msleep(ts->caps.sw_reset_delay);
		break;
	case HW_RESET:
		TOUCH_I("%s : HW Reset\n", __func__);
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_msleep(5);
		touch_gpio_direction_output(ts->reset_pin, 1);
		touch_msleep(ts->caps.hw_reset_delay);
		break;
	default:
		TOUCH_I("%s, Unknown reset ctrl!!!!\n", __func__);
		break;
	}

	return 0;
}

int ft5446_power(struct device *dev, int ctrl)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);

	TOUCH_TRACE();

	switch (ctrl) {
	case POWER_OFF:
		TOUCH_I("%s, off\n", __func__);
		//touch_interrupt_control(dev, INTERRUPT_DISABLE);
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_msleep(5);
		touch_power_3_3_vcl(dev, 0); //2.8V vdd power off
		touch_msleep(5);
		break;
	case POWER_ON:
		TOUCH_I("%s, on\n", __func__);
		atomic_set(&d->init, IC_INIT_NEED);
		touch_power_3_3_vcl(dev, 1); //2.8V vdd power on
		touch_msleep(5);
		touch_gpio_direction_output(ts->reset_pin, 1);
		touch_msleep(200);
		//touch_interrupt_control(dev, INTERRUPT_ENABLE);
		break;
	case POWER_SLEEP:
		TOUCH_I("%s, sleep\n", __func__);
		break;
	case POWER_WAKE:
		TOUCH_I("%s, wake\n", __func__);
		break;
	case POWER_HW_RESET_ASYNC:
		TOUCH_I("%s, HW Reset(%d)\n", __func__, ctrl);
		touch_interrupt_control(dev, INTERRUPT_DISABLE);
		atomic_set(&d->init, IC_INIT_NEED);
		touch_report_all_event(ts);
		ft5446_reset_ctrl(dev, HW_RESET);
		queue_delayed_work(ts->wq, &ts->init_work, 0);
		touch_interrupt_control(dev, INTERRUPT_ENABLE);
		break;
	case POWER_HW_RESET_SYNC:
		TOUCH_I("%s, HW Reset(%d)\n", __func__, ctrl);
		touch_interrupt_control(dev, INTERRUPT_DISABLE);
		atomic_set(&d->init, IC_INIT_NEED);
		touch_report_all_event(ts);
		ft5446_reset_ctrl(dev, HW_RESET);
		ft5446_init(dev);
		touch_interrupt_control(dev, INTERRUPT_ENABLE);
		break;
	case POWER_SW_RESET:
	    TOUCH_I("%s, SW Reset\n", __func__);
		touch_interrupt_control(dev, INTERRUPT_DISABLE);
		atomic_set(&d->init, IC_INIT_NEED);
		ft5446_reset_ctrl(dev, SW_RESET);
		ft5446_init(dev);
		touch_interrupt_control(dev, INTERRUPT_ENABLE);
		break;
	default:
		TOUCH_I("%s, Unknown Power Ctrl!!!!\n", __func__);
		break;
	}

	return 0;
}
static ssize_t store_reg_ctrl(struct device *dev,
				const char *buf, size_t count)
{
	char command[6] = {0};
	u32 reg = 0;
	u32 value = 0;
	u8 data = 0;
	u8 reg_addr;

	TOUCH_TRACE();

	if (sscanf(buf, "%5s %x %x", command, &reg, &value) <= 0)
		return count;

	reg_addr = (u8)reg;
	if (!strcmp(command, "write")) {
		data = (u8)value;
		if (ft5446_reg_write(dev, reg_addr, &data, sizeof(u8)) < 0)
			TOUCH_E("reg addr 0x%x write fail\n", reg_addr);
		else
			TOUCH_I("reg[%x] = 0x%x\n", reg_addr, data);
	} else if (!strcmp(command, "read")) {
		if (ft5446_reg_read(dev, reg_addr, &data, sizeof(u8)) < 0)
			TOUCH_E("reg addr 0x%x read fail\n", reg_addr);
		else
			TOUCH_I("reg[%x] = 0x%x\n", reg_addr, data);
	} else {
		TOUCH_D(BASE_INFO, "Usage\n");
		TOUCH_D(BASE_INFO, "Write reg value\n");
		TOUCH_D(BASE_INFO, "Read reg\n");
	}
	return count;
}

static ssize_t show_tci_debug(struct device *dev, char *buf)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = snprintf(buf + ret, PAGE_SIZE,
			"Current TCI Debug Type = %s\n",
			(d->tci_debug_type < 4) ? lpwg_debug_type_str[d->tci_debug_type] : "Invalid");

	TOUCH_I("Current TCI Debug Type = %s\n",
			(d->tci_debug_type < 4) ? lpwg_debug_type_str[d->tci_debug_type] : "Invalid");

	return ret;
}

static ssize_t store_tci_debug(struct device *dev, const char *buf, size_t count)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int value = 0;

	TOUCH_TRACE();
	if (sscanf(buf, "%d", &value) <= 0 || value < 0 || value > 3) {
		TOUCH_I("Invalid TCI Debug Type, please input 0~3\n");
		return count;
	}

	d->tci_debug_type = (u8)value;

	TOUCH_I("Set TCI Debug Type = %s\n", (d->tci_debug_type < 4) ? lpwg_debug_type_str[d->tci_debug_type] : "Invalid");

	return count;
}

static ssize_t show_swipe_debug(struct device *dev, char *buf)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = snprintf(buf + ret, PAGE_SIZE,
			"Current SWIPE Debug Type = %s\n",
			(d->swipe_debug_type < 4) ? lpwg_debug_type_str[d->swipe_debug_type] : "Invalid");

	TOUCH_I("Current Swipe Debug Type = %s\n",
			(d->swipe_debug_type < 4) ? lpwg_debug_type_str[d->swipe_debug_type] : "Invalid");

	return ret;
}

static ssize_t store_swipe_debug(struct device *dev, const char *buf, size_t count)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%d", &value) <= 0 || value < 0 || value > 3) {
		TOUCH_I("Invalid SWIPE Debug Type, please input 0~3\n");
		return count;
	}

	d->swipe_debug_type = (u8)value;

	TOUCH_I("Set SWIPE Debug Type = %s\n", (d->swipe_debug_type < 4) ? lpwg_debug_type_str[d->swipe_debug_type] : "Invalid");

	return count;
}

static ssize_t store_reset_ctrl(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value == 0) {
		ft5446_power(dev, POWER_SW_RESET);
	} else if (value == 1) {
		ft5446_power(dev, POWER_HW_RESET_ASYNC);
	} else if (value == 2) {
		ft5446_power(dev, POWER_HW_RESET_SYNC);
	} else if (value == 3) {
		ft5446_power(dev, POWER_OFF);
		ft5446_power(dev, POWER_ON);
		queue_delayed_work(ts->wq, &ts->init_work, 0);
	} else {
		TOUCH_I("Unsupported command %d\n", value);
	}

	return count;
}

static ssize_t show_pinstate(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = snprintf(buf, PAGE_SIZE, "RST:%d, INT:%d\n",
			gpio_get_value(ts->reset_pin), gpio_get_value(ts->int_pin));
	TOUCH_I("%s() buf:%s",__func__, buf);
	return ret;
}


static TOUCH_ATTR(reg_ctrl, NULL, store_reg_ctrl);
static TOUCH_ATTR(tci_debug, show_tci_debug, store_tci_debug);
static TOUCH_ATTR(swipe_debug, show_swipe_debug, store_swipe_debug);
static TOUCH_ATTR(reset_ctrl, NULL, store_reset_ctrl);
static TOUCH_ATTR(pinstate, show_pinstate, NULL);

static struct attribute *ft5446_attribute_list[] = {
	&touch_attr_reg_ctrl.attr,
	&touch_attr_tci_debug.attr,
	&touch_attr_swipe_debug.attr,
	&touch_attr_reset_ctrl.attr,
	&touch_attr_pinstate.attr,
	NULL,
};

static const struct attribute_group ft5446_attribute_group = {
	.attrs = ft5446_attribute_list,
};

static int ft5446_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &ft5446_attribute_group);
	if (ret < 0)
		TOUCH_E("ft5446 sysfs register failed\n");

//	ft5446_watch_register_sysfs(dev);
	ft5446_prd_register_sysfs(dev);
//	ft5446_asc_register_sysfs(dev);	/* ASC */
//	ft5446_sic_abt_register_sysfs(&ts->kobj);

	return 0;
}

static int ft5446_get_cmd_version(struct device *dev, char *buf)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int offset = 0;

	TOUCH_TRACE();

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"==================== Version Info ====================\n");
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Version: v%d.%02d\n", d->ic_info.version.major, d->ic_info.version.minor);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Chip_id: %x / Chip_id_low : %x\n", d->ic_info.chip_id, d->ic_info.chip_id_low);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Vendor_id: %x\n", d->ic_info.fw_vendor_id);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Product_id: [FT%x%x]\n", d->ic_info.product_id[0], d->ic_info.product_id[1]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"======================================================\n");

	return offset;
}

static int ft5446_get_cmd_atcmd_version(struct device *dev, char *buf)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int offset = 0;

	TOUCH_TRACE();

	offset = snprintf(buf, PAGE_SIZE, "V%d.%02d\n",
		d->ic_info.version.major, d->ic_info.version.minor);

	return offset;
}

static int ft5446_esd_recovery(struct device *dev)
{
#if 0
	//Not used in add-on type (Only use in-cell model)

	TOUCH_TRACE();

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	lge_mdss_report_panel_dead();
#endif

#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
	mtkfb_esd_recovery();
#endif
#endif
	return 0;
}

static int ft5446_swipe_enable(struct device *dev, bool enable)
{
	//struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	// TODO swipe function
	//ili9881h_lpwg_control(dev, ts->lpwg.mode, enable);

	return 0;
}

static int ft5446_init_pm(struct device *dev)
{
#if 0
	//Not used in add-on type (Only use in-cell model)
	 
#if defined(CONFIG_FB)
	struct touch_core_data *ts = to_touch_core(dev);
#endif

	TOUCH_TRACE();

#if defined(CONFIG_DRM_MSM) && defined(CONFIG_FB) && defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
	TOUCH_I("%s: drm_notif change\n", __func__);
	ts->drm_notif.notifier_call = ili9881h_drm_notifier_callback;
#elif defined(CONFIG_FB)
	TOUCH_I("%s: fb_notif change\n", __func__);
	fb_unregister_client(&ts->fb_notif);
	ts->fb_notif.notifier_call = ili9881h_fb_notifier_callback;
	*/
#endif
#endif
	return 0;
}

static int ft5446_set(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_TRACE();

	return 0;
}

static int ft5446_get(struct device *dev, u32 cmd, void *input, void *output)
{
	int ret = 0;

	TOUCH_D(BASE_INFO, "%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
	case CMD_VERSION:
		ret = ft5446_get_cmd_version(dev, (char *)output);
		break;

	case CMD_ATCMD_VERSION:
		ret = ft5446_get_cmd_atcmd_version(dev, (char *)output);
		break;

	default:
		break;
	}

	return ret;
}

static int ft5446_shutdown(struct device *dev){
	TOUCH_TRACE();
	return 0;
}

static struct touch_driver touch_driver = {
	.probe = ft5446_probe,
	.remove = ft5446_remove,
	.suspend = ft5446_suspend,
	.shutdown = ft5446_shutdown,
	.resume = ft5446_resume,
	.init = ft5446_init,
	.irq_handler = ft5446_irq_handler,
	.power = ft5446_power,
	.upgrade = ft5446_upgrade,
	.esd_recovery = ft5446_esd_recovery,
	.lpwg = ft5446_lpwg,
	.swipe_enable = ft5446_swipe_enable,
	.notify = ft5446_notify,
	.init_pm = ft5446_init_pm,
	.register_sysfs = ft5446_register_sysfs,
	.set = ft5446_set,
	.get = ft5446_get,
};

#define MATCH_NAME			"focaltech,ft5446"

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
/*
static bool ft5446_get_device_type(void)
{
	bool bdevice = false;
	enum lge_panel_type panel_type = touch_get_device_type();

	TOUCH_I("%s [lge_get_panel_type] = [%d]\n", __func__, panel_type);

	switch(panel_type) {
		case LV3_TIANMA:
			bdevice = true;
			break;
		default:
			break;
	}

	return bdevice;
}
*/

/*
static void __init touch_device_async_init(void *data, async_cookie_t cookie)
{
       touch_bus_device_init(&hwif, &touch_driver);
       TOUCH_TRACE();
}
*/

static int __init touch_device_init(void)
{
	TOUCH_TRACE();
	if (is_lcm_name("TXD-ILI9881C") || is_lcm_name("CTC-ILI9881D") ||
	    is_lcm_name("KD-ILI9881C") || is_lcm_name("HLT-JD9365D") || is_lcm_name("KD-JD9365Z")) {
		TOUCH_I("%s, ft5446 found!! lcm_name = %s\n",__func__,lge_get_lcm_name());
		return touch_bus_device_init(&hwif, &touch_driver);
	}
	
	TOUCH_I("%s, ft5446 not found.\n", __func__);
	return 0;
}

static void __exit touch_device_exit(void)
{
	TOUCH_TRACE();
	touch_bus_device_exit(&hwif);
}

//module_init(touch_device_init);
late_initcall(touch_device_init);
module_exit(touch_device_exit);

MODULE_AUTHOR("BSP-TOUCH@lge.com");
MODULE_DESCRIPTION("LGE touch driver v3");
MODULE_LICENSE("GPL");
