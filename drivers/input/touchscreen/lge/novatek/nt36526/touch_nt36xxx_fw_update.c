/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 47247 $
 * $Date: 2019-07-10 10:41:36 +0800 (週三, 10 七月 2019) $
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

#include <linux/firmware.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>

/*
 *  Include to Local Header File
 */
#include "touch_nt36xxx.h"

#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define SIZE_64KB 65536
#define BLOCK_64KB_NUM 4
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)

#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)

const struct firmware *fw_entry = NULL;
static size_t fw_need_write_size = 0;

static int32_t nvt_get_fw_need_write_size(const struct firmware *fw_entry)
{
	int32_t i = 0;
	int32_t total_sectors_to_check = 0;

	total_sectors_to_check = fw_entry->size / FLASH_SECTOR_SIZE;
	/* printk("total_sectors_to_check = %d\n", total_sectors_to_check); */

	for (i = total_sectors_to_check; i > 0; i--) {
		/* printk("current end flag address checked = 0x%X\n", i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN); */
		/* check if there is end flag "NVT" at the end of this sector */
		if (strncmp(&fw_entry->data[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN], "NVT", NVT_FLASH_END_FLAG_LEN) == 0) {
			fw_need_write_size = i * FLASH_SECTOR_SIZE;
			TOUCH_I("fw_need_write_size = %zu(0x%zx)\n", fw_need_write_size, fw_need_write_size);
			return 0;
		}
	}

	TOUCH_E("end flag \"NVT\" not found!\n");
	return -1;
}

/*******************************************************
Description:
	Novatek touchscreen request update firmware function.

return:
	Executive outcomes. 0---succeed. -1,-22---failed.
*******************************************************/
int32_t update_firmware_request(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	char fwpath[256];
	int32_t ret = 0;

	//---get fw path---
	if (ts->test_fwpath[0]) {
		memcpy(fwpath, &ts->test_fwpath[0], sizeof(fwpath));
		TOUCH_I("get fwpath from test_fwpath:%s\n",
			&ts->test_fwpath[0]);
	} else if (ts->def_fwcnt) {
		if(is_lcm_name("TXD-NT36526")) {
			memcpy(fwpath, ts->def_fwpath[0], sizeof(fwpath));
			TOUCH_I("get fwpath from def_fwpath[0] : %s\n",
					ts->def_fwpath[0]);
		} else if (is_lcm_name("SKI-NT36526")) {
			memcpy(fwpath, ts->def_fwpath[1], sizeof(fwpath));
			TOUCH_I("get fwpath from def_fwpath[1] : %s\n",
					ts->def_fwpath[1]);
		}

	} else {
		TOUCH_E("no firmware file\n");
		return -EPERM;
	}

	//---check fw path---
	if (fwpath == NULL) {
		TOUCH_E("error get fw path\n");
		return -EPERM;
	}

	TOUCH_I("fwpath[%s]\n", fwpath);

	// request fw bin file
	ret = request_firmware(&fw_entry, fwpath, dev);
	if (ret < 0) {
		TOUCH_E("fail to request_firmware fwpath: %s (ret:%d)\n", fwpath, ret);
		return ret;
	}

	// check FW need to write size
	if (nvt_get_fw_need_write_size(fw_entry)) {
		TOUCH_E("get fw need to write size fail!\n");
		return -EINVAL;
	}

	// check if FW version add FW version bar equals 0xFF
	if (*(fw_entry->data + FW_BIN_VER_OFFSET) + *(fw_entry->data + FW_BIN_VER_BAR_OFFSET) != 0xFF) {
		TOUCH_E("bin file FW_VER + FW_VER_BAR should be 0xFF!\n");
		TOUCH_E("FW_VER=0x%02X, FW_VER_BAR=0x%02X\n", *(fw_entry->data+FW_BIN_VER_OFFSET), *(fw_entry->data+FW_BIN_VER_BAR_OFFSET));
		return -EINVAL;
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen release update firmware function.

return:
	n.a.
*******************************************************/
void update_firmware_release(void)
{
	if (fw_entry) {
		release_firmware(fw_entry);
	}
	fw_entry=NULL;
}

/*******************************************************
Description:
	Novatek touchscreen check firmware version function.

return:
	Executive outcomes. 0---need update. 1---need not
	update.
*******************************************************/
int32_t Check_FW_Ver(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[16] = {0};
	int32_t ret = 0;

	//write i2c index to EVENT BUF ADDR
	ret = nvt_set_page(dev, I2C_BLDR_Address, d->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);
	if (ret < 0) {
		TOUCH_E("i2c write error!(%d)\n", ret);
		return ret;
	}

	//read Firmware Version
	buf[0] = EVENT_MAP_FWINFO;
	buf[1] = 0x00;
	buf[2] = 0x00;
	ret = CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 3);
	if (ret < 0) {
		TOUCH_E("i2c read error!(%d)\n", ret);
		return ret;
	}

	TOUCH_I("IC FW Ver = 0x%02X, FW Ver Bar = 0x%02X\n", buf[1], buf[2]);
	TOUCH_I("Bin FW Ver = 0x%02X, FW ver Bar = 0x%02X\n",
			fw_entry->data[FW_BIN_VER_OFFSET], fw_entry->data[FW_BIN_VER_BAR_OFFSET]);

	// check IC FW_VER + FW_VER_BAR equals 0xFF or not, need to update if not
	if ((buf[1] + buf[2]) != 0xFF) {
		TOUCH_E("IC FW_VER + FW_VER_BAR not equals to 0xFF!\n");
		return 0;
	}

	// compare IC and binary FW version
	if (buf[1] > fw_entry->data[FW_BIN_VER_OFFSET])
		return 1;
	else
		return 0;
}

/*******************************************************
Description:
	Novatek touchscreen resume from deep power down function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Resume_PD(struct device *dev)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// Resume Command
	buf[0] = 0x00;
	buf[1] = 0xAB;
	ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		TOUCH_E("Write Enable error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Resume Command)
	retry = 0;
	while(1) {
		msleep(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Check 0xAA (Resume Command) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			TOUCH_E("Check 0xAA (Resume Command) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}
	msleep(10);

	TOUCH_I("Resume PD OK\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen check firmware checksum function.

return:
	Executive outcomes. 0---checksum not match.
	1---checksum match. -1--- checksum read failed.
*******************************************************/
int32_t Check_CheckSum(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);

	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = d->mmap->READ_FLASH_CHECKSUM_ADDR;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t k = 0;
	uint16_t WR_Filechksum[BLOCK_64KB_NUM] = {0};
	uint16_t RD_Filechksum[BLOCK_64KB_NUM] = {0};
	size_t len_in_blk = 0;
	int32_t retry = 0;

	if (Resume_PD(dev)) {
		TOUCH_E("Resume PD error!!\n");
		return -1;
	}

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_need_write_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_need_write_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += fw_entry->data[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[0] = 0x00;
			buf[1] = 0x07;
			buf[2] = i;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[6] = (len_in_blk - 1) & 0xFF;
			ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				TOUCH_E("Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);
				buf[0] = 0x00;
				buf[1] = 0x00;
				ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					TOUCH_E("Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}
				if (buf[1] == 0xAA) {
					break;
				}
				retry++;
				if (unlikely(retry > 5)) {
					TOUCH_E("Check 0xAA (Fast Read Command) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			ret = nvt_set_page(dev, I2C_BLDR_Address, XDATA_Addr);
			if (ret < 0) {
				TOUCH_E("Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[0] = (XDATA_Addr) & 0xFF;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				TOUCH_E("Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2] << 8) | buf[1]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				TOUCH_E("RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
				TOUCH_E("firmware checksum not match!!\n");
				return 0;
			}
		}
	}

	TOUCH_I("firmware checksum match\n");
	return 1;
}

/*******************************************************
Description:
	Novatek touchscreen check firmware checksum function.

return:
	WR_Filechksum, RD_Filechksum
*******************************************************/
int32_t Read_CheckSum(struct device *dev, uint16_t *WR_Filechksum, uint16_t *RD_Filechksum)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);

	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = d->mmap->READ_FLASH_CHECKSUM_ADDR;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t k = 0;
	size_t fw_bin_size = 0;
	size_t len_in_blk = 0;
	int32_t retry = 0;

	// request bin file in "/etc/firmware"
	ret = update_firmware_request(dev);
	if (ret) {
		TOUCH_E("update_firmware_request failed. (%d)\n", ret);
		goto out_Read_CheckSum;
	}

	nvt_sw_reset_idle(dev);

	if (Resume_PD(dev)) {
		TOUCH_E("Resume PD error!!\n");
		goto out_Read_CheckSum;
	}

	fw_bin_size = fw_entry->size;

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_need_write_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_need_write_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += fw_entry->data[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[0] = 0x00;
			buf[1] = 0x07;
			buf[2] = i;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[6] = (len_in_blk - 1) & 0xFF;
			ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				TOUCH_E("Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);
				buf[0] = 0x00;
				buf[1] = 0x00;
				ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					TOUCH_E("Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}
				if (buf[1] == 0xAA) {
					break;
				}
				retry++;
				if (unlikely(retry > 5)) {
					TOUCH_E("Check 0xAA (Fast Read Command) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			ret = nvt_set_page(dev, I2C_BLDR_Address, XDATA_Addr);
			if (ret < 0) {
				TOUCH_E("Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[0] = (XDATA_Addr) & 0xFF;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				TOUCH_E("Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2] << 8) | buf[1]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				TOUCH_E("RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
				TOUCH_E("firmware checksum not match!!\n");
				return 0;
			}
		}
	}

out_Read_CheckSum:
	// Bootloader Reset
	nvt_bootloader_reset(dev);
	nvt_check_fw_reset_state(dev, RESET_STATE_INIT);
	
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen initial bootloader and flash
	block function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Init_BootLoader(struct device *dev)
{
	uint8_t buf[64] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// SW Reset & Idle
	nvt_sw_reset_idle(dev);

	// Initiate Flash Block
	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = I2C_FW_Address;
	ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		TOUCH_E("Inittial Flash Block error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Initiate Flash Block)
	retry = 0;
	while(1) {
		msleep(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Check 0xAA (Inittial Flash Block) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			TOUCH_E("Check 0xAA (Inittial Flash Block) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	TOUCH_I("Init OK \n");
	msleep(20);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen erase flash sectors function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Erase_Flash(struct device *dev)
{
	uint8_t buf[64] = {0};
	int32_t ret = 0;
	int32_t count = 0;
	int32_t i = 0;
	int32_t Flash_Address = 0;
	int32_t retry = 0;

	// Write Enable
	buf[0] = 0x00;
	buf[1] = 0x06;
	ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		TOUCH_E("Write Enable (for Write Status Register) error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Enable)
	retry = 0;
	while (1) {
		mdelay(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Check 0xAA (Write Enable for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			TOUCH_E("Check 0xAA (Write Enable for Write Status Register) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	// Write Status Register
	buf[0] = 0x00;
	buf[1] = 0x01;
	buf[2] = 0x00;
	ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		TOUCH_E("Write Status Register error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Status Register)
	retry = 0;
	while (1) {
		mdelay(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Check 0xAA (Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			TOUCH_E("Check 0xAA (Write Status Register) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	// Read Status
	retry = 0;
	while (1) {
		mdelay(5);
		buf[0] = 0x00;
		buf[1] = 0x05;
		ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Read Status (for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}

		// Check 0xAA (Read Status)
		buf[0] = 0x00;
		buf[1] = 0x00;
		buf[2] = 0x00;
		ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 3);
		if (ret < 0) {
			TOUCH_E("Check 0xAA (Read Status for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if ((buf[1] == 0xAA) && (buf[2] == 0x00)) {
			break;
		}
		retry++;
		if (unlikely(retry > 100)) {
			TOUCH_E("Check 0xAA (Read Status for Write Status Register) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
			return -1;
		}
	}

	if (fw_need_write_size % FLASH_SECTOR_SIZE)
		count = fw_need_write_size / FLASH_SECTOR_SIZE + 1;
	else
		count = fw_need_write_size / FLASH_SECTOR_SIZE;

	for(i = 0; i < count; i++) {
		// Write Enable
		buf[0] = 0x00;
		buf[1] = 0x06;
		ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Write Enable error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			mdelay(1);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Check 0xAA (Write Enable) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				TOUCH_E("Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[1]);
				return -1;
			}
		}

		Flash_Address = i * FLASH_SECTOR_SIZE;

		// Sector Erase
		buf[0] = 0x00;
		buf[1] = 0x20;    // Command : Sector Erase
		buf[2] = ((Flash_Address >> 16) & 0xFF);
		buf[3] = ((Flash_Address >> 8) & 0xFF);
		buf[4] = (Flash_Address & 0xFF);
		ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 5);
		if (ret < 0) {
			TOUCH_E("Sector Erase error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Sector Erase)
		retry = 0;
		while (1) {
			mdelay(1);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Check 0xAA (Sector Erase) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				TOUCH_E("Check 0xAA (Sector Erase) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
				return -1;
			}
		}

		// Read Status
		retry = 0;
		while (1) {
			mdelay(5);
			buf[0] = 0x00;
			buf[1] = 0x05;
			ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Read Status error!!(%d,%d)\n", ret, i);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				TOUCH_E("Check 0xAA (Read Status) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if ((buf[1] == 0xAA) && (buf[2] == 0x00)) {
				break;
			}
			retry++;
			if (unlikely(retry > 100)) {
				TOUCH_E("Check 0xAA (Read Status) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
				return -1;
			}
		}
	}

	TOUCH_I("Erase OK \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen write flash sectors function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Write_Flash(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = d->mmap->RW_FLASH_DATA_ADDR;
	uint32_t Flash_Address = 0;
	int32_t i = 0, j = 0, k = 0;
	uint8_t tmpvalue = 0;
	int32_t count = 0;
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t percent = 0;
	int32_t previous_percent = -1;

	// change I2C buffer index
	ret = nvt_set_page(dev, I2C_BLDR_Address, XDATA_Addr);
	if (ret < 0) {
		TOUCH_E("change I2C buffer index error!!(%d)\n", ret);
		return ret;
	}

	if (fw_need_write_size % 256)
		count = fw_need_write_size / 256 + 1;
	else
		count = fw_need_write_size / 256;

	for (i = 0; i < count; i++) {
		Flash_Address = i * 256;

		// Write Enable
		buf[0] = 0x00;
		buf[1] = 0x06;
		ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			TOUCH_E("Write Enable error!!(%d)\n", ret);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			udelay(100);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Check 0xAA (Write Enable) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				TOUCH_E("Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[1]);
				return -1;
			}
		}

		// Write Page : 256 bytes
		for (j = 0; j < min(fw_need_write_size - i * 256, (size_t)256); j += 32) {
			buf[0] = (XDATA_Addr + j) & 0xFF;
			for (k = 0; k < 32; k++) {
				buf[1 + k] = fw_entry->data[Flash_Address + j + k];
			}
			ret = CTP_I2C_WRITE(dev, I2C_BLDR_Address, buf, 33);
			if (ret < 0) {
				TOUCH_E("Write Page error!!(%d), j=%d\n", ret, j);
				return ret;
			}
		}
		if (fw_need_write_size - Flash_Address >= 256)
			tmpvalue=(Flash_Address >> 16) + ((Flash_Address >> 8) & 0xFF) + (Flash_Address & 0xFF) + 0x00 + (255);
		else
			tmpvalue=(Flash_Address >> 16) + ((Flash_Address >> 8) & 0xFF) + (Flash_Address & 0xFF) + 0x00 + (fw_need_write_size - Flash_Address - 1);

		for (k = 0; k < min(fw_need_write_size - Flash_Address, (size_t)256); k++)
			tmpvalue += fw_entry->data[Flash_Address + k];

		tmpvalue = 255 - tmpvalue + 1;

		// Page Program
		buf[0] = 0x00;
		buf[1] = 0x02;
		buf[2] = ((Flash_Address >> 16) & 0xFF);
		buf[3] = ((Flash_Address >> 8) & 0xFF);
		buf[4] = (Flash_Address & 0xFF);
		buf[5] = 0x00;
		buf[6] = min(fw_need_write_size - Flash_Address, (size_t)256) - 1;
		buf[7] = tmpvalue;
		ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 8);
		if (ret < 0) {
			TOUCH_E("Page Program error!!(%d), i=%d\n", ret, i);
			return ret;
		}
		// Check 0xAA (Page Program)
		retry = 0;
		while (1) {
			mdelay(1);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Page Program error!!(%d)\n", ret);
				return ret;
			}
			if (buf[1] == 0xAA || buf[1] == 0xEA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				TOUCH_E("Check 0xAA (Page Program) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
				return -1;
			}
		}
		if (buf[1] == 0xEA) {
			TOUCH_E("Page Program error!! i=%d\n", i);
			return -3;
		}

		// Read Status
		retry = 0;
		while (1) {
			mdelay(5);
			buf[0] = 0x00;
			buf[1] = 0x05;
			ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				TOUCH_E("Read Status error!!(%d)\n", ret);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				TOUCH_E("Check 0xAA (Read Status) error!!(%d)\n", ret);
				return ret;
			}
			if (((buf[1] == 0xAA) && (buf[2] == 0x00)) || (buf[1] == 0xEA)) {
				break;
			}
			retry++;
			if (unlikely(retry > 100)) {
				TOUCH_E("Check 0xAA (Read Status) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
				return -1;
			}
		}
		if (buf[1] == 0xEA) {
			TOUCH_E("Page Program error!! i=%d\n", i);
			return -4;
		}

		percent = ((i + 1) * 100) / count;
		if (((percent % 10) == 0) && (percent != previous_percent)) {
			TOUCH_I("Programming...%2d%%\n", percent);
			previous_percent = percent;
		}
	}

	TOUCH_I("Program OK         \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen verify checksum of written
	flash function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Verify_Flash(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = d->mmap->READ_FLASH_CHECKSUM_ADDR;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t k = 0;
	uint16_t WR_Filechksum[BLOCK_64KB_NUM] = {0};
	uint16_t RD_Filechksum[BLOCK_64KB_NUM] = {0};
	size_t len_in_blk = 0;
	int32_t retry = 0;

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_need_write_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_need_write_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += fw_entry->data[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[0] = 0x00;
			buf[1] = 0x07;
			buf[2] = i;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[6] = (len_in_blk - 1) & 0xFF;
			ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				TOUCH_E("Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);
				buf[0] = 0x00;
				buf[1] = 0x00;
				ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					TOUCH_E("Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}
				if (buf[1] == 0xAA) {
					break;
				}
				retry++;
				if (unlikely(retry > 5)) {
					TOUCH_E("Check 0xAA (Fast Read Command) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			ret = nvt_set_page(dev, I2C_BLDR_Address, XDATA_Addr);
			if (ret < 0) {
				TOUCH_E("Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[0] = (XDATA_Addr) & 0xFF;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				TOUCH_E("Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2] << 8) | buf[1]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				TOUCH_E("Verify Fail%d!!\n", i);
				TOUCH_E("RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
				return -1;
			}
		}
	}

	TOUCH_I("Verify OK \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen update firmware function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Update_Firmware(struct device *dev)
{
	int32_t ret = 0;

	TOUCH_TRACE();

	//---Stop CRC check to prevent IC auto reboot---
	nvt_stop_crc_reboot(dev);

	// Step 1 : initial bootloader
	ret = Init_BootLoader(dev);
	if (ret) {
		return ret;
	}

	// Step 2 : Resume PD
	ret = Resume_PD(dev);
	if (ret) {
		return ret;
	}

	// Step 3 : Erase
	ret = Erase_Flash(dev);
	if (ret) {
		return ret;
	}

	// Step 4 : Program
	ret = Write_Flash(dev);
	if (ret) {
		return ret;
	}

	// Step 5 : Verify
	ret = Verify_Flash(dev);
	if (ret) {
		return ret;
	}

	//Step 6 : Bootloader Reset
	nvt_bootloader_reset(dev);
	nvt_check_fw_reset_state(dev, RESET_STATE_INIT);
	nvt_get_fw_info(dev);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen check flash end flag function.

return:
	Executive outcomes. 0---succeed. 1,negative---failed.
*******************************************************/
int32_t nvt_check_flash_end_flag(struct device *dev)
{
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	uint8_t buf[8] = {0};
	uint8_t nvt_end_flag[NVT_FLASH_END_FLAG_LEN + 1] = {0};
	int32_t ret = 0;

	// Step 1 : initial bootloader
	ret = Init_BootLoader(dev);
	if (ret) {
		return ret;
	}

	// Step 2 : Resume PD
	ret = Resume_PD(dev);
	if (ret) {
		return ret;
	}

	// Step 3 : unlock
	buf[0] = 0x00;
	buf[1] = 0x35;
	ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		TOUCH_E("write unlock error!!(%d)\n", ret);
		return ret;
	}
	msleep(10);

	//Step 4 : Flash Read Command
	buf[0] = 0x00;
	buf[1] = 0x03;
	buf[2] = (NVT_FLASH_END_FLAG_ADDR >> 16) & 0xFF; //Addr_H
	buf[3] = (NVT_FLASH_END_FLAG_ADDR >> 8) & 0xFF; //Addr_M
	buf[4] = NVT_FLASH_END_FLAG_ADDR & 0xFF; //Addr_L
	buf[5] = (NVT_FLASH_END_FLAG_LEN >> 8) & 0xFF; //Len_H
	buf[6] = NVT_FLASH_END_FLAG_LEN & 0xFF; //Len_L
	ret = CTP_I2C_WRITE(dev, I2C_HW_Address, buf, 7);
	if (ret < 0) {
		TOUCH_E("write Read Command error!!(%d)\n", ret);
		return ret;
	}
	msleep(10);

	// Check 0xAA (Read Command)
	buf[0] = 0x00;
	buf[1] = 0x00;
	ret = CTP_I2C_READ(dev, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		TOUCH_E("Check 0xAA (Read Command) error!!(%d)\n", ret);
		return ret;
	}
	if (buf[1] != 0xAA) {
		TOUCH_E("Check 0xAA (Read Command) error!! status=0x%02X\n", buf[1]);
		return -1;
	}

	msleep(10);

	//Step 5 : Read Flash Data
	ret = nvt_set_page(dev, I2C_BLDR_Address, d->mmap->READ_FLASH_CHECKSUM_ADDR);
	if (ret < 0) {
		TOUCH_E("change index error!! (%d)\n", ret);
		return ret;
	}
	msleep(10);

	// Read Back
	buf[0] = d->mmap->READ_FLASH_CHECKSUM_ADDR & 0xFF;
	ret = CTP_I2C_READ(dev, I2C_BLDR_Address, buf, 6);
	if (ret < 0) {
		TOUCH_E("Read Back error!! (%d)\n", ret);
		return ret;
	}

	//buf[3:5] => NVT End Flag
	strncpy(nvt_end_flag, &buf[3], NVT_FLASH_END_FLAG_LEN);
	TOUCH_I("nvt_end_flag=%s (%02X %02X %02X)\n", nvt_end_flag, buf[3], buf[4], buf[5]);

	if (strncmp(nvt_end_flag, "NVT", NVT_FLASH_END_FLAG_LEN) == 0) {
		return 0;
	} else {
		TOUCH_E("\"NVT\" end flag not found!\n");
		return 1;
	}
}

int32_t nt36xxx_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct nt36xxx_data *d = to_nt36xxx_data(dev);
	int update = 0;
	int32_t ret = 0;

	d->mode_state = FW_UPGRADE_MODE;

	// request bin file in "/etc/firmware"
	ret = update_firmware_request(dev);
	if (ret) {
		TOUCH_E("update_firmware_request failed. (%d)\n", ret);
		return ret;
	}

	mutex_lock(&d->lock);

	//---update check---
	if (ts->force_fwup) {
		update = 1;
	} else {
		nvt_sw_reset_idle(dev);
		
		ret = Check_CheckSum(dev);
		if (ret < 0) {	//read firmware checksum failed
			TOUCH_E("read firmware checksum failed\n");
			update = 1;
		} else if (ret == 0) {	//(fw checksum not match) && (bin fw version >= ic fw version)
			TOUCH_I("check sum is not match but firmware version is match\n");
			update = 1;
		} else {	
			 if (Check_FW_Ver(dev) == 1) {	//(fw checksum match) && (bin fw version != ic fw version)
				 TOUCH_I("firmware checksum match, but version not match\n");
				 update = 1;
			 }
		}

		//check firmware info page 
		if (nvt_check_flash_end_flag(dev)) {
			TOUCH_I("check flash end flag failed\n");
			update = 1;
		}
	}
	
	//---do update---
	if(update == 1)
	{
		ret = Update_Firmware(dev);
	}

	// Bootloader Reset
	nvt_bootloader_reset(dev);
	nvt_check_fw_reset_state(dev, RESET_STATE_INIT);
	
	mutex_unlock(&d->lock);

	update_firmware_release();
	d->mode_state = FW_NORMAL_MODE;
	
	return ret;
}
