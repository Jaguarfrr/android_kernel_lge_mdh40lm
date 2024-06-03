/* touch_ili9881h.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: BSP-TOUCH@lge.com
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <asm/uaccess.h>

#include <touch_core.h>
#include <touch_hwif.h>
#include "touch_ili9881h.h"
#include "touch_ili9881h_fw.h"

#define K			(1024)
#define M			(K * K)
#define UPDATE_PASS		0
#define UPDATE_FAIL		-1
#define TIMEOUT_SECTOR		500
#define TIMEOUT_PAGE		3500
#define TIMEOUT_PROGRAM		10

/* Firmware upgrade */
#define DDI_BK_ST_ADDR			0x1E000
#define DDI_BK_END_ADDR			0x1FFFF
#define DDI_BK_SIZE				1*K
#define RSV_BK_ST_ADDR				0x1E000
#define RSV_BK_END_ADDR				0x1E3FF
#define FW_VER_ADDR				0xFFE0
#define FW_BLOCK_INFO_NUM			17
#define INFO_HEX_ST_ADDR			0x4F
#define HW_CRC_MAX_LEN 				0x1FFFF
#define DUMP_FLASH_PATH				"/sdcard/flash_dump"
#define DUMP_IRAM_PATH				"/sdcard/iram_dump"

static struct touch_fw_data {
	u8 block_number;
	u8  fw_info[75];
	u32 start_addr;
	u32 end_addr;
	u32 bin_fw_ver;
	u32 bin_fw_build_ver;
	bool is80k;
	int hex_tag;
} tfd;

static struct flash_block_info {
	char *name;
	u32 start;
	u32 end;
	u32 len;
	u32 mem_start;
	u32 fix_mem_start;
	u8 mode;
} fbi[FW_BLOCK_INFO_NUM];


struct flash_table flash_t[] = {
    {0x00, 0x0000, (256 * K), 256, (4 * K)}, /* Default */
    {0xEF, 0x6011, (128 * K), 256, (4 * K)}, /* W25Q10EW    */
    {0xEF, 0x6012, (256 * K), 256, (4 * K)}, /* W25Q20EW    */
    {0xC8, 0x6012, (256 * K), 256, (4 * K)}, /* GD25LQ20B */
    {0xC8, 0x6013, (512 * K), 256, (4 * K)}, /* GD25LQ40 */
    {0x85, 0x6013, (4 * M), 256, (4 * K)},
    {0xC2, 0x2812, (256 * K), 256, (4 * K)},
    {0x1C, 0x3812, (256 * K), 256, (4 * K)},
};

struct flash_table *flashtab = NULL;

static u8 fw_flash_tmp[K + 7];
static u8 *pfw = NULL;

u32 ic_fw_ver;
u8 ic_fw_build_ver;
u8 bin_fw_build_ver;
u32 fw_end_addr;
u32 fw_start_addr;

int g_update_percentage = 0;

static int ili9881h_iram_read(struct device *dev, u8 *buf, u32 start, int len)
{
	int i, limit = 4*K;
	int addr = 0, end = len - 1;
//	u8 cmd[4] = {0};

	TOUCH_TRACE();

	if (!buf) {
		TOUCH_E("buf is null\n");
		return -ENOMEM;
	}

	for (i = 0, addr = start; addr < end; i += limit, addr += limit) {
		if ((addr + limit) > len)
			limit = end % limit;

		if (ili9881h_ice_reg_read(dev, addr, buf + i, limit) < 0) {
			TOUCH_E("Failed to Read iram data\n");
			return -ENODEV;
		}

		g_update_percentage = (i * 100) / end;
		TOUCH_D(FW_UPGRADE, "Reading iram data .... %d%c", g_update_percentage, '%');
	}
	return 0;
}

void ili9881h_dump_iram_data(struct device *dev, u32 start, u32 end)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int i, ret, len;
	u8 *fw_buf = NULL;

	TOUCH_TRACE();

	g_update_percentage = FW_STAT_INIT;

	ret = ili9881h_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Failed to enter ICE mode, ret = %d\n", ret);
		goto out;
	}

	len = end - start + 1;

	if (len > MAX_HEX_FILE_SIZE) {
		TOUCH_E("len is larger than buffer, abort\n");
		ret = -EINVAL;
		goto out;
	}

	fw_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL);
	if (ERR_ALLOC_MEM(fw_buf)) {
		TOUCH_E("Failed to allocate update_buf\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
		fw_buf[i] = 0xFF;

	ret = ili9881h_iram_read(dev, fw_buf, start, len);
	if (ret < 0)
		goto out;

	f = filp_open(DUMP_IRAM_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (ERR_ALLOC_MEM(f)) {
		TOUCH_E("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -1;
		goto out;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(f, fw_buf, len, &pos);
	set_fs(old_fs);
	filp_close(f, NULL);
	TOUCH_I("Save iram data to %s\n", DUMP_IRAM_PATH);

out:
	ret = ili9881h_ice_mode_disable(dev);
	TOUCH_I("Dump IRAM %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	g_update_percentage = (ret < 0) ? FW_UPDATE_FAIL : FW_UPDATE_PASS;
	ipio_kfree((void **)&fw_buf);
}

static void ili9881h_flash_write_enable(struct device *dev)
{
	TOUCH_TRACE();

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1) < 0)
		TOUCH_E("Pull CS low failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3) < 0)
		TOUCH_E("Write key failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x6, 1) < 0)
		TOUCH_E("Write 0x6 failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
		TOUCH_E("Pull CS high failed\n");
}

static u32 HexToDec(char *phex, s32 len)
{
	u32 ret = 0, temp = 0, i;
	s32 shift = (len - 1) * 4;

	for (i = 0; i < len; shift -= 4, i++) {
		if ((phex[i] >= '0') && (phex[i] <= '9'))
			temp = phex[i] - '0';
		else if ((phex[i] >= 'a') && (phex[i] <= 'f'))
			temp = (phex[i] - 'a') + 10;
		else if ((phex[i] >= 'A') && (phex[i] <= 'F'))
			temp = (phex[i] - 'A') + 10;
		else
			return -1;

		ret |= (temp << shift);
	}
	return ret;
}

static int CalculateCRC32(u32 start_addr, u32 len, u8 *pfw)
{
	int i = 0, j = 0;
	int crc_poly = 0x04C11DB7;
	int tmp_crc = 0xFFFFFFFF;

	for (i = start_addr; i < start_addr + len; i++) {
		tmp_crc ^= (pfw[i] << 24);

		for (j = 0; j < 8; j++) {
			if ((tmp_crc & 0x80000000) != 0)
				tmp_crc = tmp_crc << 1 ^ crc_poly;
			else
				tmp_crc = tmp_crc << 1;
		}
	}
	return tmp_crc;
}

static int ili7808g_flash_poll_busy(struct device *dev, int timer)
{
	int ret = UPDATE_PASS, retry = timer;
	u8 cmd = 0x5;
	u32 temp = 0;

	TOUCH_TRACE();

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1) < 0)
		TOUCH_E("Pull cs low failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3) < 0)
		TOUCH_E("Write key failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, cmd, 1) < 0)
		TOUCH_E("Write 0x5 cmd failed\n");

	do {
		if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0xFF, 1) < 0)
			TOUCH_E("Write dummy failed\n");

		touch_msleep(1);

		if (ili9881h_ice_reg_read(dev, FLASH4_ADDR, &temp, sizeof(u8)) < 0)
			TOUCH_E("Read flash busy error\n");

		if ((temp & 0x3) == 0)
			break;
	} while (--retry >= 0);

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
		TOUCH_E("Pull cs high failed\n");

	if (retry <= 0) {
		TOUCH_E("Flash polling busy timeout ! tmp = %x\n", temp);
		ret = UPDATE_FAIL;
	}

	return ret;
}

static int ili9881h_read_flash_data(struct device *dev, u32 start, u32 end, u8 *data, int len)
{
	u32 i, index = 0;
	u32 tmp;

	TOUCH_TRACE();

	if (end - start > len) {
		TOUCH_E("the length (%d) reading crc is over than len(%d)\n", end - start, len);
		return -1;
	}

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1) < 0)
		TOUCH_E("Write cs low failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3) < 0)
		TOUCH_E("Write key failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x03, 1) < 0)
		TOUCH_E("Write 0x3 failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, (start & 0xFF0000) >> 16, 1) < 0)
		TOUCH_E("Write address failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, (start & 0x00FF00) >> 8, 1) < 0)
		TOUCH_E("Write address failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, (start & 0x0000FF), 1) < 0)
		TOUCH_E("Write address failed\n");

	for (i = start; i <= end; i++) {
		if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0xFF, 1) < 0)
			TOUCH_E("Write dummy failed\n");

		if (ili9881h_ice_reg_read(dev, FLASH4_ADDR, &tmp, sizeof(u8)) < 0)
			TOUCH_E("Read flash data error!\n");

		data[index] = tmp;
		index++;
		g_update_percentage = (i * 100) / end;
		TOUCH_D(FW_UPGRADE, "Reading flash data .... %d%c", g_update_percentage, '%');
	}

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
		TOUCH_E("Write cs high failed\n");

	return 0;
}

int ili9881h_dump_flash_data(struct device *dev, u32 start, u32 end)
{
	struct file *f = NULL;
	u8 *buf = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u32 start_addr, end_addr;
	int ret, length;

	TOUCH_TRACE();

	g_update_percentage = FW_STAT_INIT;

	f = filp_open(DUMP_FLASH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (ERR_ALLOC_MEM(f)) {
		TOUCH_E("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -1;
		goto out;
	}

	ret = ili9881h_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Failed to enter ICE mode, ret = %d\n", ret);
		goto out;
	}

	start_addr = start;
	end_addr = end;

	length = end_addr - start_addr + 1;
	TOUCH_I("len = %d\n", length);

	buf = vmalloc(length * sizeof(u8));
	if (ERR_ALLOC_MEM(buf)) {
		TOUCH_E("Failed to allocate buf memory, %ld\n", PTR_ERR(buf));
		filp_close(f, NULL);
		ret = -1;
		goto out;
	}

	ret = ili9881h_read_flash_data(dev, start_addr, end_addr, buf, length);
	if (ret < 0)
		goto out;

	old_fs = get_fs();
	set_fs(get_ds());
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(f, buf, length, &pos);
	set_fs(old_fs);
	filp_close(f, NULL);
	ipio_vfree((void **)&buf);

out:
	ret = ili9881h_ice_mode_disable(dev);
	TOUCH_I("Dump flash %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	g_update_percentage = (ret < 0) ? FW_UPDATE_FAIL : FW_UPDATE_PASS;
	return ret;
}

static void ili9881h_flash_protect(struct device *dev, bool enable)
{
	TOUCH_TRACE();

	TOUCH_I("%s flash protection\n", enable ? "Enable" : "Disable");

	ili9881h_flash_write_enable(dev);

	ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1);

	ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3);

	ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x1, 1);

	ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x0, 1);


	switch (flashtab->mid ) {
	case 0xEF:
		if (flashtab->dev_id == 0x6012 || flashtab->dev_id == 0x6011) {
			if (enable) {
				if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x7E, 1) < 0)
					TOUCH_E("Write 0x7E at %x failed\n", FLASH2_ADDR);
			} else {
				if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x0, 1) < 0)
					TOUCH_E("Write 0x0 at %x failed\n", FLASH2_ADDR);
			}
		}
		break;
	case 0xC8:
		if (flashtab->dev_id == 0x6012 || flashtab->dev_id == 0x6013) {
			if (enable) {
				if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x7A, 1) < 0)
					TOUCH_E("Write 0x7A at %x failed\n", FLASH2_ADDR);
			} else {
				if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x0, 1) < 0)
					TOUCH_E("Write 0x0 at %x failed\n", FLASH2_ADDR);
			}

		}
		break;
	default:
		TOUCH_E("Can't find flash id(0x%x), ignore protection\n", flashtab->mid );
		break;
	}

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
		TOUCH_E("Write cs high failed\n");
}

static int ili9881h_check_ddi_chunk(struct device *dev, int from_dma, u8 *pfw)
{
	/* ili9881h_check_ddi_chunk funcion purpose is for DH15 project(DH15 f/w have more 1 block(DDI block))*/
	struct ili9881h_data *d = to_ili9881h_data(dev);
	struct ili9881h_fw_info *fw_info = &d->ic_info.fw_info;

	int ret = 0;
	u8 cmd[7] = {0x0};

	u32 start, end, len;

	if (!fbi[DDI].start || !fbi[DDI].end)
		return UPDATE_PASS;

	/* Get current fw version before comparing. */
	cmd[0] = CMD_GET_FIRMWARE_VERSION;
	ret = ili7807_get_fw_data_wrapper(dev, cmd, 0, fw_info, sizeof(*fw_info));
	if (ret < 0) {
		TOUCH_E("There is no fw in flash\n");
		goto dma;
	}
	cmd[0] = CMD_GET_FLASH_DATA;			//cmd
	cmd[1] = DDI_BK_SIZE & 0xFF;			//len L byte
	cmd[2] = (DDI_BK_SIZE >> 8) & 0xFF;		//len H byte

	start = DDI_BK_ST_ADDR;
	cmd[3] = (start & 0xFF);       		//addr L byte
	cmd[4] = (start >> 8) & 0xFF ;  	//addr M byte
	cmd[5] = (start >> 16) & 0xFF; 		//addr H byte
	cmd[6] = ili9881h_calc_data_checksum(cmd, sizeof(cmd) - 1);
	TOUCH_I("read Customer Reserved block start addr = 0x%X, len = 0x%X ", start, DDI_BK_SIZE);

	ret = ili7807_get_fw_data_wrapper(dev, cmd, sizeof(cmd) - 1, fw_flash_tmp, sizeof(fw_flash_tmp));
	memcpy(&pfw[start], fw_flash_tmp + 6, DDI_BK_SIZE);

	start = DDI_BK_END_ADDR - DDI_BK_SIZE + 1;
	cmd[3] = (start & 0xFF);       		//addr L byte
	cmd[4] = (start >> 8) & 0xFF ;  	//addr M byte
	cmd[5] = (start >> 16) & 0xFF; 		//addr H byte	//addr H byte
	cmd[6] = ili9881h_calc_data_checksum(cmd, sizeof(cmd) - 1);
	TOUCH_I("read Customer Reserved block start addr = 0x%X, len = 0x%X ", start, DDI_BK_SIZE);

	ret = ili7807_get_fw_data_wrapper(dev, cmd, sizeof(cmd) - 1, fw_flash_tmp, sizeof(fw_flash_tmp));
	memcpy(&pfw[start], fw_flash_tmp + 6, DDI_BK_SIZE);

	goto out;

dma:
	TOUCH_I("Obtain block data from dma\n");

	ret = ili9881h_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Failed to enter ICE mode, ret = %d\n", ret);
		goto out;
	}
	start = DDI_BK_ST_ADDR;
	end = DDI_BK_ST_ADDR + DDI_BK_SIZE - 1;
	len = DDI_BK_SIZE;
	if (ili9881h_read_flash_data(dev, start, end, fw_flash_tmp, len) < 0) {
		TOUCH_E("Read flash data from dma error\n");
		ret = UPDATE_FAIL;
		goto out;
	}
	memcpy(&pfw[start], fw_flash_tmp + 6, DDI_BK_SIZE);

	start = DDI_BK_END_ADDR - DDI_BK_SIZE + 1;
	end = DDI_BK_END_ADDR;
	len = DDI_BK_SIZE;
	if (ili9881h_read_flash_data(dev, start, end, fw_flash_tmp, len) < 0) {
		TOUCH_E("Read flash data from dma error\n");
		ret = UPDATE_FAIL;
		goto out;
	}
	memcpy(&pfw[start], fw_flash_tmp + 6, DDI_BK_SIZE);

	ret = ili9881h_ice_mode_disable(dev);
out:
	/*
	 * Since there're two blocks combined with DDI chunk togeter,
	 * need to extend the addresses where are going to upgrade.
	 */
	fbi[DDI].start = DDI_BK_ST_ADDR;
	fbi[DDI].end = DDI_BK_END_ADDR;
	tfd.end_addr = DDI_BK_END_ADDR;


	return ret;
}

static u32 ili9881h_read_hw_crc(struct device *dev, u32 start, u32 end)
{
	int retry = 500;
	u32 busy = 0;
	u32 write_len = end;
	u32 flash_crc = 0;

	TOUCH_TRACE();

	if (write_len > HW_CRC_MAX_LEN) {
		TOUCH_E("The length (%x) written into firmware is greater than max count (%x)\n",
			write_len, HW_CRC_MAX_LEN);
		return -1;
	}

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1) < 0)
		TOUCH_E("Pull CS low failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3) < 0)
		TOUCH_E("Write key failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x3b, 1) < 0)
		TOUCH_E("Write 0x3b failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, (start & 0xFF0000) >> 16, 1) < 0)
		TOUCH_E("Write address failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, (start & 0x00FF00) >> 8, 1) < 0)
		TOUCH_E("Write address failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, (start & 0x0000FF), 1) < 0)
		TOUCH_E("Write address failed\n");

	if (ili9881h_ice_reg_write(dev, 0x041003, 0x01, 1) < 0)
		TOUCH_E("Write enable Dio_Rx_dual failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0xFF, 1) < 0)
		TOUCH_E("Write dummy failed\n");

	if (ili9881h_ice_reg_write(dev, 0x04100C, write_len, 3) < 0)
		TOUCH_E("Write Set Receive count failed\n");

	if (ili9881h_ice_reg_write(dev, 0x048007, 0x02, 1) < 0)
		TOUCH_E("Write clearing int flag failed\n");

	if (ili9881h_ice_reg_write(dev, 0x041016, 0x00, 1) < 0)
		TOUCH_E("Write 0x0 at 0x041016 failed\n");

	if (ili9881h_ice_reg_write(dev, 0x041016, 0x01, 1) < 0)
		TOUCH_E("Write Checksum_En failed\n");

	if (ili9881h_ice_reg_write(dev, FLASH4_ADDR, 0xFF, 1) < 0)
		TOUCH_E("Write start to receive failed\n");

	do {
		if (ili9881h_ice_reg_read(dev, 0x048007, &busy, sizeof(u8)) < 0)
			TOUCH_E("Read busy error\n");

		TOUCH_D(FW_UPGRADE, "busy = %x\n", busy);
		if (((busy >> 1) & 0x01) == 0x01)
			break;
	} while (--retry >= 0);

	if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
		TOUCH_E("Write CS high failed\n");

	if (retry <= 0) {
		TOUCH_E("Read HW CRC timeout !, busy = 0x%x\n", busy);
		return -1;
	}

	if (ili9881h_ice_reg_write(dev, 0x041003, 0x0, 1) < 0)
		TOUCH_E("Write disable dio_Rx_dual failed\n");

	if (ili9881h_ice_reg_read(dev, 0x04101C, &flash_crc, sizeof(u32)) < 0) {
		TOUCH_E("Read hw crc error\n");
		return -1;
	}

	return flash_crc;
}

static int ili9881h_check_fw_ver(struct device *dev, u8 *pfw)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili9881h_data *d = to_ili9881h_data(dev);
	int i, len = 0, crc_byte_len = 4;
//	u8 flash_crc[4] = {0};
//	u32 start_addr = 0, end_addr = 0;
	u32 hw_crc[FW_BLOCK_INFO_NUM];
//	u32 flash_crc_cb = 0,
	u32 hex_crc = 0;

	TOUCH_TRACE();

	if (ts->force_fwup) {
		TOUCH_I("Force upgrade, Don't do check crc\n");
		return UPDATE_FAIL;
	}

	// /* Check Flash and HW CRC with last 4 bytes in each block */
	// for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
	// 	start_addr = fbi[i].start;
	// 	end_addr = fbi[i].end;

	// 	/* Invaild end address */
	// 	if (end_addr == 0)
	// 		continue;

	// 	if (ili9881h_read_flash_data(dev, end_addr - crc_byte_len + 1, end_addr,
	// 				flash_crc, sizeof(flash_crc)) < 0) {
	// 		TOUCH_E("Read Flash failed\n");
	// 		return UPDATE_FAIL;
	// 	}

	// 	flash_crc_cb = flash_crc[0] << 24 | flash_crc[1] << 16 | flash_crc[2] << 8 | flash_crc[3];

	// 	hw_crc[i] = ili9881h_read_hw_crc(dev, start_addr, end_addr - start_addr - crc_byte_len + 1);

	// 	TOUCH_I("Block = %d, HW CRC = 0x%06x, Flash CRC = 0x%06x\n", i, hw_crc[i], flash_crc_cb);

	// 	/* Compare Flash CRC with HW CRC */
	// 	if (flash_crc_cb != hw_crc[i]) {
	// 		TOUCH_I("HW and Flash CRC not matched, do upgrade\n");
	// 		return UPDATE_FAIL;
	// 	}
	// 	memset(flash_crc, 0, sizeof(flash_crc));
	// }
	ic_fw_ver = (d->ic_info.fw_info.core << 24) | (d->ic_info.fw_info.customer_code << 16) |
		(d->ic_info.fw_info.major << 8) | d->ic_info.fw_info.minor;
	/* Check FW version */
	TOUCH_I("New FW ver = 0x%x, Current FW ver = 0x%x\n", tfd.bin_fw_ver, ic_fw_ver);
	if (tfd.bin_fw_ver != ic_fw_ver) {
		TOUCH_I("FW version is different, do upgrade\n");
		return UPDATE_FAIL;
	}

	/* Check Hex and HW CRC */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		len = fbi[i].end - fbi[i].start + 1 - crc_byte_len;
		hex_crc = CalculateCRC32(fbi[i].start, len, pfw);
		hw_crc[i] = ili9881h_read_hw_crc(dev, fbi[i].start, len);
		TOUCH_I("Block = %d, HW CRC = 0x%06x, Hex CRC = 0x%06x\n", i, hw_crc[i], hex_crc);
		if (hex_crc != hw_crc[i]) {
			TOUCH_E("Hex and HW CRC not matched, do upgrade\n");
			return UPDATE_FAIL;
		}
	}

	TOUCH_I("Firmware is the newest version, upgrade abort\n");
	return UPDATE_PASS;
}

static int ili9881h_flash_hex_hw_crc_cmp(struct device *dev, u8 *pfw)
{
	u32 i = 0, len = 0;
	u32 hex_crc = 0, hw_crc;

	TOUCH_TRACE();

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		len = fbi[i].end - fbi[i].start + 1 - 4;
		hex_crc = CalculateCRC32(fbi[i].start, len, pfw);
		hw_crc = ili9881h_read_hw_crc(dev, fbi[i].start, len);

		TOUCH_I("Block = %d, Hex CRC = %x, HW CRC = %x\n", i, hex_crc, hw_crc);

		if (hex_crc != hw_crc) {
			TOUCH_E("Hex and HW CRC NO matched !!!\n");
			return UPDATE_FAIL;
		}
	}

	TOUCH_I("Hex and HW CRC match!\n");
	return UPDATE_PASS;
}

static int ili9881h_flash_erase(struct device *dev)
{
	int ret = 0;
	u32 i = 0, addr = 0, recv_addr = 0;
	bool bk_erase = false;

	TOUCH_TRACE();

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		TOUCH_I("Block[%d]: Erasing from (0x%x) to (0x%x) \n", i, fbi[i].start, fbi[i].end);

		for (addr = fbi[i].start; addr <= fbi[i].end; addr += flashtab->sector) {
			ili9881h_flash_write_enable(dev);

			if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1) < 0)
				TOUCH_E("Write cs low failed\n");

			if (ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3) < 0)
				TOUCH_E("Write key failed\n");

			if (addr == fbi[AP].start) {
				/* block erase with 64k bytes. */
				if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0xD8, 1) < 0)
					TOUCH_E("Write 0xB at %x failed\n", FLASH2_ADDR);
				bk_erase = true;
			} else {
				/* sector erase with 4k bytes. */
				if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x20, 1) < 0)
					TOUCH_E("Write 0x20 at %x failed\n", FLASH2_ADDR);
			}

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, recv_addr, 3) < 0)
				TOUCH_E("Write address failed\n");

			if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
				TOUCH_E("Write cs high failed\n");

			/* Waitint for flash setting ready */
			touch_msleep(1);

			if (addr == fbi[AP].start)
				ret = ili7808g_flash_poll_busy(dev, TIMEOUT_PAGE);
			else
				ret = ili7808g_flash_poll_busy(dev, TIMEOUT_SECTOR);

			if (ret < 0)
				return UPDATE_FAIL;

			if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
				TOUCH_E("Write cs high failed\n");

			if (tfd.is80k) {
				if (i == AP && bk_erase) {
					addr = (64*K) - (4*K);
					bk_erase = false;
				}
			} else {
				if (i == AP)
					break;
			}
		}
	}
	return UPDATE_PASS;
}

static int ili9881h_flash_program(struct device *dev, u8 *pfw)
{
	u8 buf[512] = {0};
	u32 i = 0, addr = 0, k = 0, recv_addr = 0;
	bool skip = true;

	TOUCH_TRACE();

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		TOUCH_I("Block[%d]: Programing from (0x%x) to (0x%x), tfd.end_addr = 0x%x\n",
				i, fbi[i].start, fbi[i].end, tfd.end_addr);

		for (addr = fbi[i].start; addr < fbi[i].end; addr += flashtab->program_page) {
			buf[0] = (char)((FLASH2_ADDR & 0x000000FF) >> 0);
			buf[1] = (char)((FLASH2_ADDR & 0x0000FF00) >> 8);
			buf[2] = (char)((FLASH2_ADDR & 0x00FF0000) >> 16);

			for (k = 0; k < flashtab->program_page; k++) {
				if (addr + k <= tfd.end_addr)
					buf[3 + k] = pfw[addr + k];
				else
					buf[3 + k] = 0xFF;

				if (buf[3 + k] != 0xFF)
					skip = false;
			}

			if (skip) {
				if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
					TOUCH_E("Write cs high failed\n");
				return UPDATE_FAIL;
			}

			ili9881h_flash_write_enable(dev);

			if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1) < 0)
				TOUCH_E("Write cs low failed\n");

			if (ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3) < 0)
				TOUCH_E("Write key failed\n");

			if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0x2, 1) < 0)
				TOUCH_E("Write 0x2 failed\n");

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili9881h_ice_reg_write(dev, FLASH2_ADDR, recv_addr, 3) < 0)
				TOUCH_E("Write address failed\n");

			if (ili9881h_reg_write(dev, CMD_GET_MCU_STOP_INTERNAL_DATA, buf, flashtab->program_page + 3) < 0) {
				TOUCH_E("Failed to program data at start_addr = 0x%X, k = 0x%X, addr = 0x%x\n",
				addr, k, addr + k);
				if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
					TOUCH_E("Write cs high failed\n");
				return UPDATE_FAIL;
			}

			if (ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1) < 0)
				TOUCH_E("Write cs high failed\n");

			if (flashtab->mid  == 0xEF) {
				touch_msleep(1);
			} else {
				if (ili7808g_flash_poll_busy(dev, TIMEOUT_PROGRAM) < 0)
					return UPDATE_FAIL;
			}

			/* holding the status until finish this upgrade. */
			g_update_percentage = (addr * 101) / tfd.end_addr;
			if (g_update_percentage > FW_UPDATING)
				g_update_percentage = FW_UPDATING;
		}
	}
	return UPDATE_PASS;
}

static int ili9881h_flash_upgrade(struct device *dev, u8 *pfw)
{
	int ret = UPDATE_PASS;

	TOUCH_TRACE();

	ili9881h_reset_ctrl(dev, HW_RESET_ONLY);

	ili9881h_check_ddi_chunk(dev, ret, pfw);

	ili9881h_ice_mode_enable(dev, MCU_STOP);

	ret = ili9881h_check_fw_ver(dev, pfw);
	if (ret == UPDATE_PASS)
		goto out;

	ret = ili9881h_flash_erase(dev);
	if (ret == UPDATE_FAIL)
		goto out;

	ret = ili9881h_flash_program(dev, pfw);
	if (ret == UPDATE_FAIL)
		goto out;

	ret = ili9881h_flash_hex_hw_crc_cmp(dev, pfw);

out:
	ili9881h_ice_mode_disable(dev);
	return ret;
}

static int calc_file_crc(u8 *pfw)
{
	int i;
	u32 ex_addr, data_crc, file_crc;

	TOUCH_TRACE();

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;
		ex_addr = fbi[i].end;

		data_crc = CalculateCRC32(fbi[i].start, fbi[i].len - 4, pfw);
		TOUCH_I("fbi[%d].start = 0x%x, end = 0x%x, len = 0x%x\n", i, fbi[i].start, fbi[i].end, fbi[i].len);
		file_crc = pfw[ex_addr - 3] << 24 | pfw[ex_addr - 2] << 16 | pfw[ex_addr - 1] << 8 | pfw[ex_addr];
		TOUCH_I("data crc = %x, file crc = %x\n", data_crc, file_crc);
		if (data_crc != file_crc) {
			TOUCH_E("Content of fw file is broken. (%d, %x, %x)\n",
				i, data_crc, file_crc);
			return -1;
		}
	}

	TOUCH_I("Content of fw file is correct\n");
	return 0;
}

static int fw_file_convert(u8 *phex, int size, u8 *pfw)
{
	int block = 0;
	u32 i = 0, j = 0, k = 0, num = 0;
	u32 len = 0, addr = 0, type = 0;
	u32 start_addr = 0x0, end_addr = 0x0, ex_addr = 0;
	u32 offset;

	TOUCH_TRACE();

	memset(fbi, 0x0, sizeof(fbi));

	/* Parsing HEX file */
	for (; i < size;) {
		len = HexToDec(&phex[i + 1], 2);
		addr = HexToDec(&phex[i + 3], 4);
		type = HexToDec(&phex[i + 7], 2);

		if (type == 0x04) {
			ex_addr = HexToDec(&phex[i + 9], 4);
		} else if (type == 0x02) {
			ex_addr = HexToDec(&phex[i + 9], 4);
			ex_addr = ex_addr >> 12;
		} else if (type == BLOCK_TAG_AF) {
			/* insert block info extracted from hex */
			tfd.hex_tag = type;
			if (tfd.hex_tag == BLOCK_TAG_AF)
				num = HexToDec(&phex[i + 9 + 6 + 6], 2);
			else
				num = 0xFF;

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				TOUCH_E("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].start = HexToDec(&phex[i + 9], 6);
			fbi[num].end = HexToDec(&phex[i + 9 + 6], 6);
			fbi[num].fix_mem_start = INT_MAX;
			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			TOUCH_I("Block[%d]: start_addr = %x, end = %x", num, fbi[num].start, fbi[num].end);

			block++;
		} else if (type == BLOCK_TAG_B0 && tfd.hex_tag == BLOCK_TAG_AF) {
			num = HexToDec(&phex[i + 9 + 6], 2);

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				TOUCH_E("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].fix_mem_start = HexToDec(&phex[i + 9], 6);
			TOUCH_I("Tag 0xB0: change Block[%d] to addr = 0x%x\n", num, fbi[num].fix_mem_start);
		}

		addr = addr + (ex_addr << 16);

		if (phex[i + 1 + 2 + 4 + 2 + (len * 2) + 2] == 0x0D)
			offset = 2;
		else
			offset = 1;

		if (addr > MAX_HEX_FILE_SIZE) {
			TOUCH_E("Invalid hex format %d\n", addr);
			return -1;
		}

		if (type == 0x00) {
			end_addr = addr + len;
			if (addr < start_addr)
				start_addr = addr;
			/* fill data */
			for (j = 0, k = 0; j < (len * 2); j += 2, k++)
				pfw[addr + k] = HexToDec(&phex[i + 9 + j], 2);
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}

	if (calc_file_crc(pfw) < 0)
		return -1;

	tfd.start_addr = start_addr;
	tfd.end_addr = end_addr;
	tfd.block_number = block;
	return 0;
}

static inline void *ipio_memcpy(void *dest, const void *src, int n, int dest_size)

{
        if (n > dest_size)
            n = dest_size;

        return memcpy(dest, src, n);
}


static void update_block_info(u8 *pfw)
{
	u32 fw_info_addr = 0;

	fbi[AP].name = "AP";
	fbi[DATA].name = "DATA";
	fbi[TUNING].name = "TUNING";
	fbi[MP].name = "MP";
	fbi[GESTURE].name = "GESTURE";

	/* upgrade mode define */
	fbi[DATA].mode = fbi[AP].mode = fbi[TUNING].mode = AP;
	fbi[MP].mode = MP;
	fbi[GESTURE].mode = GESTURE;

	TOUCH_TRACE();

	/* Copy fw info */
	fw_info_addr = fbi[AP].end - INFO_HEX_ST_ADDR;
	TOUCH_I("Parsing hex info start addr = 0x%x\n", fw_info_addr);
	ipio_memcpy(tfd.fw_info, (pfw + fw_info_addr), sizeof(tfd.fw_info), sizeof(tfd.fw_info));

	if (fbi[AP].end > (64*K))
		tfd.is80k = true;

	/* Get hex fw vers */
	tfd.bin_fw_ver = (tfd.fw_info[48] << 24) | (tfd.fw_info[49] << 16) |
			(tfd.fw_info[50] << 8) | tfd.fw_info[51];

	/* Calculate update address */
	TOUCH_I("New FW ver = 0x%x\n", tfd.bin_fw_ver);
	TOUCH_I("star_addr = 0x%06X, end_addr = 0x%06X, Block Num = %d\n", tfd.start_addr, tfd.end_addr, tfd.block_number);
}

static int open_fw_file(struct device *dev, char* fwpath, u8 *pfw)
{
	int ret =0, fsize = 0;
	const struct firmware *fw = NULL;
	u8 *temp = NULL;

	TOUCH_TRACE();

//	mm_segment_t old_fs;
//	loff_t pos = 0;

	if (request_firmware(&fw, fwpath, dev) < 0) {
		TOUCH_E("Request firmware failed, try again\n");
		if (request_firmware(&fw, fwpath, dev) < 0) {
			TOUCH_E("Request firmware failed after retry\n");
			ret = -1;
			return ret;
		}
	}

	fsize = fw->size;
	TOUCH_I("fsize = %d\n", fsize);
	if (fsize <= 0) {
		TOUCH_E("The size of file is zero\n");
		ret = -1;
		goto out;
	}

	temp = vmalloc(fsize);
	if (ERR_ALLOC_MEM(temp)) {
		TOUCH_E("Failed to allocate tp_fw by vmalloc, try again\n");
		temp = vmalloc(fsize);
		if (ERR_ALLOC_MEM(temp)) {
			TOUCH_E("Failed to allocate tp_fw after retry\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	/* Copy fw data got from request_firmware to global */
	memcpy(temp, fw->data, fsize);

	/* Convert hex and copy data from temp to pfw */
	if (fw_file_convert(temp, fsize, pfw) < 0) {
		TOUCH_E("Convert hex file failed\n");
		ret = -1;
		goto out;
	}

	update_block_info(pfw);

out:
	release_firmware(fw);
	ipio_vfree((void **)&(temp));
	return ret;
}

int ili9881h_fw_upgrade(struct device *dev, char* fwpath)
{
	int ret = 0, retry = 3;

	TOUCH_TRACE();

	pfw = vmalloc(MAX_HEX_FILE_SIZE * sizeof(u8));
	if (ERR_ALLOC_MEM(pfw)) {
		TOUCH_E("Failed to allocate pfw memory, %ld\n", PTR_ERR(pfw));
		ipio_vfree((void **)&pfw);
		ret = -ENOMEM;
		goto out;
	}

	//temp
	memset(pfw, 0xff, MAX_HEX_FILE_SIZE * sizeof(u8));

	if (open_fw_file(dev, fwpath, pfw) < 0) {
		TOUCH_E("Open FW file fail\n");
		goto out;
	}

	do {
		ret = ili9881h_flash_upgrade(dev, pfw);
		if (ret == UPDATE_PASS)
			break;

		TOUCH_E("Upgrade failed, do retry!\n");
	} while (--retry > 0);

	if (ret != UPDATE_PASS) {
		TOUCH_E("Failed to upgrade fw %d times, erasing flash\n", retry);
		ili9881h_ice_mode_enable(dev, MCU_STOP);
		ili9881h_flash_erase(dev);
		ili9881h_ice_mode_disable(dev);
	}

	ili9881h_reset_ctrl(dev, HW_RESET_ONLY);
out:
	ipio_vfree((void **)&pfw);
	return ret;
}

int ili9881h_read_flash_info(struct device *dev)
{
	int i, ret = 0;
	u8 buf[4] = {0};
	u8 cmd = 0x9F;
	u32 tmp = 0;
	u16 flash_id = 0, mid  = 0;

	TOUCH_TRACE();

	flashtab = devm_kzalloc(dev, sizeof(flash_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(flashtab)) {
		TOUCH_E("Failed to allocate flashtab memory, %ld\n", PTR_ERR(flashtab));
		ret = -ENOMEM;
		return ret;
	}

	ret = ili9881h_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Failed to enter ICE mode, ret = %d\n", ret);
		goto out;
	}
	ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1);
	ili9881h_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3);

	ili9881h_ice_reg_write(dev, FLASH2_ADDR, cmd, 1);
	for (i = 0; i < ARRAY_SIZE(buf); i++) {
		ili9881h_ice_reg_write(dev, FLASH2_ADDR, 0xFF, 1);
		ili9881h_ice_reg_read(dev, FLASH4_ADDR, &tmp, sizeof(u8));

		buf[i] = tmp;
	}

	ili9881h_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1);

	mid  = buf[0];
	flash_id = buf[1] << 8 | buf[2];

	for (i = 0; i < ARRAY_SIZE(flash_t); i++) {
		if (mid  == flash_t[i].mid && flash_id == flash_t[i].dev_id) {
			flashtab->mid = flash_t[i].mid;
			flashtab->dev_id = flash_t[i].dev_id;
			flashtab->program_page = flash_t[i].program_page;
			flashtab->sector = flash_t[i].sector;
			break;
		}
	}

	if (i >= ARRAY_SIZE(flash_t)) {
		TOUCH_I("Not found flash id in tab, use default\n");
		flashtab->mid = flash_t[0].mid;
		flashtab->dev_id = flash_t[0].dev_id;
		flashtab->program_page = flash_t[0].program_page;
		flashtab->sector = flash_t[0].sector;
	}

	TOUCH_I("Flash MID = %x, Flash DEV_ID = %x\n", flashtab->mid , flashtab->dev_id);
	TOUCH_I("Flash program page = %d\n", flashtab->program_page);
	TOUCH_I("Flash sector = %d\n", flashtab->sector);

	ili9881h_flash_protect(dev, false);

out:
	ret = ili9881h_ice_mode_disable(dev);
	return ret;
}
