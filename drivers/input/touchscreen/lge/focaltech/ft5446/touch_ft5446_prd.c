/* production_test.c
 *
 * Copyright (C) 2015 LGE.
 *
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
#define TS_MODULE "[prd]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>

//#include <mach/board_lge.h>

/*
 *  Include to touch core Header File
 */
#include <touch_hwif.h>
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_ft5446.h"
#include "touch_ft5446_prd.h"

enum {
	RAW_DATA_TEST = 0,
	JITTER_TEST,
	DELTA_SHOW,
	SCAP_RAW_DATA_TEST,
	CB_TEST,
	LPWG_RAW_DATA_TEST,
	LPWG_JITTER_TEST,
};

enum {
	TEST_FAIL = 0,
	TEST_PASS,
};

#define LOG_BUF_SIZE		(4096 * 4)
#define ADC_BUF_SIZE		(1+1+MAX_ROW+MAX_COL+1+MAX_ROW+MAX_COL)
#define SCAP_BUF_SIZE       (MAX_ROW+MAX_COL)
#define SCAP_BUF_RX_SIZE       	(MAX_ROW)

#define FTS_WORK_MODE		0x00
#define FTS_FACTORY_MODE	0x40

#define FTS_MODE_CHANGE_LOOP	20

#define TEST_PACKET_LENGTH		342 // 255

static char line[50000];
static u16 LowerImage[MAX_ROW][MAX_COL];
static u16 UpperImage[MAX_ROW][MAX_COL];

static s16 fts_data[MAX_ROW][MAX_COL];
static int fGShortResistance[MAX_ROW+MAX_COL];
static int fMShortResistance[MAX_ROW+MAX_COL];
static int fts_data_adc_buff[ADC_BUF_SIZE*2];
static int fts_scap_data[SCAP_BUF_SIZE];

static u8 i2c_data[MAX_COL*MAX_ROW*2];
static u8 log_buf[LOG_BUF_SIZE]; /* !!!!!!!!!Should not exceed the log size !!!!!!!! */

int max_data = 0;
int min_data = 0;
u8 ic_type = 0x00;


static void log_file_size_check(struct device *dev)
{
	char *fname = NULL;
	struct file *file;
	loff_t file_size = 0;
	int i = 0;
	char buf1[128] = {0};
	char buf2[128] = {0};
	mm_segment_t old_fs = get_fs();
	int ret = 0;
	int boot_mode = 0;

	set_fs(KERNEL_DS);

	boot_mode = touch_check_boot_mode(dev);

	TOUCH_TRACE();

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
		fname = "/data/vendor/touch/touch_self_test.txt";
		break;
	case TOUCH_MINIOS_AAT:
		fname = "/data/touch/touch_self_test.txt";
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		fname = "/data/touch/touch_self_mfts.txt";
		break;
	default:
		TOUCH_I("%s : not support mode\n", __func__);
		break;
	}

	if (fname) {
		file = filp_open(fname, O_RDONLY, 0666);
		sys_chmod(fname, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n",
				__func__);
		goto error;
	}

	if (IS_ERR(file)) {
		TOUCH_E("%s : ERR(%ld) Open file error [%s]\n",
				__func__, PTR_ERR(file), fname);
		goto error;
	}

	file_size = vfs_llseek(file, 0, SEEK_END);
	TOUCH_I("%s : [%s] file_size = %lld\n",
			__func__, fname, file_size);

	filp_close(file, 0);

	if (file_size > MAX_LOG_FILE_SIZE) {
		TOUCH_I("%s : [%s] file_size(%lld) > MAX_LOG_FILE_SIZE(%d)\n",
				__func__, fname, file_size, MAX_LOG_FILE_SIZE);

		for (i = MAX_LOG_FILE_COUNT - 1; i >= 0; i--) {
			if (i == 0)
				sprintf(buf1, "%s", fname);
			else
				sprintf(buf1, "%s.%d", fname, i);

			ret = sys_access(buf1, 0);

			if (ret == 0) {
				TOUCH_I("%s : file [%s] exist\n",
						__func__, buf1);

				if (i == (MAX_LOG_FILE_COUNT - 1)) {
					if (sys_unlink(buf1) < 0) {
						TOUCH_E("%s : failed to remove file [%s]\n",
								__func__, buf1);
						goto error;
					}

					TOUCH_I("%s : remove file [%s]\n",
							__func__, buf1);
				} else {
					sprintf(buf2, "%s.%d",
							fname,
							(i + 1));

					if (sys_rename(buf1, buf2) < 0) {
						TOUCH_E("%s : failed to rename file [%s] -> [%s]\n",
								__func__, buf1, buf2);
						goto error;
					}

					TOUCH_I("%s : rename file [%s] -> [%s]\n",
							__func__, buf1, buf2);
				}
			} else {
				TOUCH_E("%s : file [%s] does not exist (ret = %d)\n",
						__func__, buf1, ret);
			}
		}
	}

error:
	set_fs(old_fs);
	return;
}
static void write_file(struct device *dev, char *data, int write_time)
{
	int fd = 0;
	char *fname = NULL;
	char time_string[64] = {0};
	struct timespec my_time;
	struct tm my_date;
	mm_segment_t old_fs = get_fs();
	int boot_mode = 0;

	set_fs(KERNEL_DS);

	boot_mode = touch_check_boot_mode(dev);

	TOUCH_TRACE();

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
		fname = "/data/vendor/touch/touch_self_test.txt";
		break;
	case TOUCH_MINIOS_AAT:
		fname = "/data/touch/touch_self_test.txt";
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		fname = "/data/touch/touch_self_mfts.txt";
		break;
	default:
		TOUCH_I("%s : not support mode\n", __func__);
		break;
	}

	if (fname) {
		fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);
		sys_chmod(fname, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n", __func__);
		set_fs(old_fs);
		return;
	}

	if (fd >= 0) {
		if (write_time == TIME_INFO_WRITE) {
			my_time = __current_kernel_time();
			time_to_tm(my_time.tv_sec,
					sys_tz.tz_minuteswest * 60 * (-1),
					&my_date);
			snprintf(time_string, 64,
				"\n[%02d-%02d %02d:%02d:%02d.%03lu]\n",
				my_date.tm_mon + 1,
				my_date.tm_mday, my_date.tm_hour,
				my_date.tm_min, my_date.tm_sec,
				(unsigned long) my_time.tv_nsec / 1000000);
			sys_write(fd, time_string, strlen(time_string));
		}
		sys_write(fd, data, strlen(data));
		sys_close(fd);
	} else {
		TOUCH_E("File open failed\n");
	}
	set_fs(old_fs);
}

static int spec_file_read(struct device *dev)
{
	int ret = 0;
	struct touch_core_data *ts = to_touch_core(dev);
	const struct firmware *fwlimit = NULL;
	const char *path[2] = {ts->dual_panel_spec[0], ts->dual_panel_spec_mfts[0]};
	int boot_mode = 0;
	int path_idx = 0;

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);
	if(boot_mode >= TOUCH_MINIOS_MFTS_FOLDER && boot_mode <= TOUCH_MINIOS_MFTS_CURVED)
		path_idx = 1;
	else
		path_idx = 0;

	if (ts->dual_panel_spec[0] == NULL || ts->dual_panel_spec_mfts[0] == NULL) {
		TOUCH_E("dual_panel_spec file name is null\n");
		ret = -ENOENT;
		goto error;
	}

	TOUCH_I("touch_panel_spec file path[%d] = %s\n", path_idx, path[path_idx]);

	ret = request_firmware(&fwlimit, path[path_idx], dev);
	if (ret) {
		TOUCH_E("request ihex is failed in normal mode\n");
		goto error;
	}

	if (fwlimit->data == NULL) {
		TOUCH_E("fwlimit->data is NULL\n");
		ret = -EINVAL;
		goto error;
	}

	if (fwlimit->size == 0) {
		TOUCH_E("fwlimit->size is 0\n");
		ret = -EINVAL;
		goto error;
	}
	strlcpy(line, fwlimit->data, fwlimit->size);

	TOUCH_I("spec_file_read success\n");

error:
	if (fwlimit)
		release_firmware(fwlimit);

	return ret;
}

static int spec_get_limit(struct device *dev, char *breakpoint, u16 limit_data[MAX_ROW][MAX_COL])
{
	int p = 0;
	int q = 0;
	int r = 0;
	int cipher = 1;
	int ret = 0;
	int retval = 0;
	char *found;
	int tx_num = 0;
	int rx_num = 0;

	TOUCH_TRACE();

	if (breakpoint == NULL) {
		ret = -1;
		goto error;
	}

	retval = spec_file_read(dev);
	if (retval) {
		ret = retval;
		goto error;
	}

	if (line == NULL) {
		ret =  -1;
		goto error;
	}

	found = strnstr(line, breakpoint, sizeof(line));
	if (found != NULL) {
		q = found - line;
	} else {
		TOUCH_E(
			"failed to find breakpoint. The panel_spec_file is wrong\n");
		ret = -1;
		goto error;
	}

	memset(limit_data, 0, MAX_ROW * MAX_COL * 2);

	while (1) {
		if (line[q] == ',') {
			cipher = 1;
			for (p = 1; (line[q - p] >= '0') &&
					(line[q - p] <= '9'); p++) {
				limit_data[tx_num][rx_num] += ((line[q - p] - '0') * cipher);
				cipher *= 10;
			}
			r++;
			if (r % (int)MAX_COL == 0) {
				rx_num = 0;
				tx_num++;
			} else {
				rx_num++;
			}
		}
		q++;
		if (r == (int)MAX_ROW * (int)MAX_COL) {
			TOUCH_I("[%s] panel_spec_file scanning is success\n", breakpoint);
			break;
		}
	}

	if (ret == 0) {
		ret = -1;
		goto error;

	} else {
		TOUCH_I("panel_spec_file scanning is success\n");
		return ret;
	}

error:
	return ret;
}

int ft5446_change_op_mode(struct device *dev, u8 op_mode)
{

	int i = 0;
	u8 ret;
	u8 data;

	TOUCH_I("%s : op_mode = 0x%02x\n", __func__, op_mode);

	data = 0x00;
	ret = ft5446_reg_read(dev, 0x00, &data, 1);
	if(ret < 0) {
		TOUCH_E("0x00 register read error\n");
		return ret;
	}

	if (data == op_mode) {
		TOUCH_I("Already mode changed\n");
		return 0;
	}

	data = op_mode;
	ret = ft5446_reg_write(dev, 0x00, &data, 1);
	if(ret < 0) {
		TOUCH_E("0x00 register op_mode write error\n");
		return ret;
	}

	mdelay(10);

	for ( i = 0; i < FTS_MODE_CHANGE_LOOP; i++) {
		data = 0x00;
		ret = ft5446_reg_read(dev, 0x00, &data, 1);
		if(ret < 0) {
			TOUCH_E("op_mode change check error\n");
			return ret;
		}

		if(data == op_mode)
			break;
		mdelay(50);
	}

	if (i >= FTS_MODE_CHANGE_LOOP) {
		TOUCH_E("Timeout to change op mode\n");
		return -EPERM;
	}
	TOUCH_I("Operation mode changed\n");
	mdelay(200);

	return 0;
}


int ft5446_switch_cal(struct device *dev, u8 cal_en)
{
	u8 ret;
	u8 data;

	TOUCH_I("%s : cal_en = 0x%02x\n", __func__, cal_en);

	ret = ft5446_reg_read(dev, 0xEE, &data, 1);
	if(ret < 0) {
		TOUCH_E("0xC2 register(calibration) read error\n");
		return ret;
	}

	if (data == cal_en) {
		TOUCH_I("Already switch_cal changed\n");
		return 0;
	}

	data = cal_en;
	ret = ft5446_reg_write(dev, 0xEE, &data, 1);
	if(ret < 0) {
		TOUCH_E("0xC2 register(calibration) write error\n");
		return ret;
	}
	mdelay(10);
	return 0;
}


int ft5446_prd_check_ch_num(struct device *dev)
{
	u8 ret;
	u8 data;

	TOUCH_I("%s\n", __func__);

	/* Channel number check */
	ret = ft5446_reg_read(dev, 0x02, &data, 1);
	if(ret < 0) {
		TOUCH_E("tx number register read error\n");
		return ret;
	}
	TOUCH_I("TX Channel : %d\n", data);

	mdelay(3);

	if (data != MAX_COL) {
		TOUCH_E("Invalid TX Channel Num.\n");
		return -EPERM;
	}

	ret = ft5446_reg_read(dev, 0x03, &data, 1);
	if(ret < 0) {
		TOUCH_E("rx number register read error\n");
		return ret;
	}

	TOUCH_I("RX Channel : %d\n", data);

	mdelay(3);

	if (data != MAX_ROW) {
		TOUCH_E("Invalid RX Channel Num.\n");
		return -EPERM;
	}

	return 0;
}

int ft5446_prd_get_raw_data(struct device *dev)
{
	int i, j, k;
	int ret = 0;
	u8 data = 0x00;
	u8 rawdata = 0x00;
	bool bscan = false;

	memset(i2c_data, 0, sizeof(i2c_data));

	// Data Select to raw data
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x06, &data, 1);
	if (ret < 0) {
		TOUCH_E("data select to rawdata error\n");
		return ret;
	}
	mdelay(100);

	/* Start SCAN */
	for (k = 0; k < 3; k++)
	{
		TOUCH_I("Start SCAN (%d/3)\n", k+1);
		memset(i2c_data, 0, sizeof(i2c_data));
		ret = ft5446_reg_read(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("start scan read error\n");
			return ret;
		}
		data |= 0x80; // 0x40|0x80
		ret = ft5446_reg_write(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("0x00 register 0x80 write error\n");
			return ret;
		}

		mdelay(20);

		for (i = 0; i < 5; i++) {
			ret = ft5446_reg_read(dev, 0x00, &data, 1);
			if (ret < 0) {
				TOUCH_E("scan success check error\n");
				return ret;
			}
			if (data == 0x40) {
				TOUCH_I("SCAN Success : 0x%X, %d ms \n", data, i*20);
				bscan = true;
				break;
			} else {
				bscan = false;
				mdelay(20);
			}
		}

		if(bscan==true){
			/* Read Raw data */
			rawdata = 0xAA;    // FT5446 RAWDATA ADDRESS
			ret = ft5446_reg_write(dev, 0x01, &rawdata, 1);
			if(ret < 0) {
				TOUCH_E("read raw data error\n");
				return ret;
			}
			mdelay(10);

			TOUCH_I("Read Raw data at once\n");

			ret = ft5446_reg_read(dev, 0x36, &i2c_data[0], MAX_COL*MAX_ROW*2);
			if(ret < 0) {
				TOUCH_E("0x36 register read error\n");
				return ret;
			}

		}else {
			TOUCH_E("SCAN Fail (%d/3)\n", k+1);
		}
	}

	// Data Select to raw data
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x06, &data, 1);  // 0: RAWDATA
	if(ret < 0) {
		TOUCH_E("default data select to rawdata error\n");
	}
	mdelay(100);

	for (i = 0; i < MAX_ROW; i++) {
		for(j = 0; j < MAX_COL; j++) {
			k = ((j * MAX_ROW) + i) << 1;
			fts_data[i][j] = (i2c_data[k] << 8) + i2c_data[k+1];
		}
	}

	return 0;
}

int ft5446_prd_get_raw_data_sd(struct device *dev)
{
	int i, j, k;
	int ret = 0;
	u8 data = 0x00;
	u8 rawdata = 0x00;
	bool bscan = false;

	memset(i2c_data, 0, sizeof(i2c_data));

	// Data Select to raw data
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x06, &data, 1); // 0: RAWDATA
	if (ret < 0) {
		TOUCH_E("data select to rawdata error\n");
		return ret;
	}
	mdelay(100);

	data = 0x81;  // highest frequency point in the frequency hopping table
	ret = ft5446_reg_write(dev, 0x0A, &data, 1);
	if (ret < 0) {
		TOUCH_E("low frequency setting error\n");
		return ret;
	}
	mdelay(10);

	data = 0x00; //FIR filter off
	ret = ft5446_reg_write(dev, 0xFB, &data, 1);
	if (ret < 0) {
		TOUCH_E("FIR disable error\n");
		return ret;
	}
	mdelay(150);

	/* Start SCAN */
	for (k = 0; k < 3; k++)
	{
		TOUCH_I("Start SCAN (%d/3)(low freq/FIR off)\n", k+1);
		memset(i2c_data, 0, sizeof(i2c_data));
		ret = ft5446_reg_read(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("start scan read error\n");
			return ret;
		}
		data |= 0x80; // 0x40|0x80
		ret = ft5446_reg_write(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("0x00 register 0x80 write error\n");
			return ret;
		}

		mdelay(20);

		for (i = 0; i < 5; i++) {
			ret = ft5446_reg_read(dev, 0x00, &data, 1);
			if (ret < 0) {
				TOUCH_E("scan success(low freq/FIR off)\n");
				return ret;
			}
			if (data == 0x40) {
				TOUCH_I("SCAN Success : 0x%X, %d ms \n", data, i*20);
				bscan = true;
				break;
			} else {
				bscan = false;
				mdelay(20);
			}
		}

		if(bscan==true) {
			/* Read Raw data */
			rawdata = 0xAA;
			ret = ft5446_reg_write(dev, 0x01, &rawdata, 1);
			if(ret < 0) {
				TOUCH_E("read raw data error\n");
				return ret;
			}
			mdelay(10);

			TOUCH_I("Read Raw data_sd at once\n");

			ret = ft5446_reg_read(dev, 0x36, &i2c_data[0], MAX_COL*MAX_ROW*2);
			if(ret < 0) {
				TOUCH_E("0x36 register read error\n");
				return ret;
			}
		} else {
			TOUCH_E("SCAN Fail(low freq/FIR off) (%d/3)\n", k+1);
		}
	}

	//FIR filter on(restore default value)
	data = 0x01;
	ret = ft5446_reg_write(dev, 0xFB, &data, 1);
	if (ret < 0) {
		TOUCH_E("FIR enable error\n");
		return ret;
	}
	mdelay(150);

       // Frequency hopping table default 0
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x0A, &data, 1);
	if (ret < 0) {
		TOUCH_E("frequency hopping table default 0 error\n");
		return ret;
	}
	mdelay(10);

	// Data Select to raw data
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x06, &data, 1);  // 0: RAWDATA
	if(ret < 0) {
		TOUCH_E("default data select to rawdata error\n");
	}
	mdelay(100);

	for (i = 0; i < MAX_ROW; i++) {
		for(j = 0; j < MAX_COL; j++) {
			k = ((j * MAX_ROW) + i) << 1;
			fts_data[i][j] = (i2c_data[k] << 8) + i2c_data[k+1];
		}
	}

	return 0;
}

int ft5446_prd_get_scap_raw_data(struct device *dev, bool IsWaterProof)
{
	int i, k;
	int ret = 0;
	u8 data = 0x00;
	u8 scap_rawdata = 0x00;

	memset(i2c_data, 0, sizeof(i2c_data));

	// Data Select to raw data
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x06, &data, 1);
	if (ret < 0) {
		TOUCH_E("data select to rawdata error\n");
		return ret;
	}
	mdelay(100);

	/* Start SCAN */
	for (k = 0; k < 3; k++)
	{
		TOUCH_I("Start SCAN (%d/3)\n", k+1);
		ret = ft5446_reg_read(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("start scan read error\n");
			return ret;
		}
		data |= 0x80; // 0x40|0x80
		ret = ft5446_reg_write(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("0x00 register 0xC0 write error\n");
			return ret;
		}

		mdelay(20);

		for (i = 0; i < 5; i++) {
			ret = ft5446_reg_read(dev, 0x00, &data, 1);
			if (ret < 0) {
				TOUCH_E("scan success check error\n");
				return ret;
			}
			if (data == 0x40) {
				TOUCH_I("SCAN Success : 0x%X, %d ms \n", data, i*20);
				break;
			}
			mdelay(20);
		}

		if (i < 5) {
			break;
		}
			TOUCH_E("SCAN Fail (%d/3)\n", k+1);
	}

	if (k >= 3) {
		TOUCH_E("SCAN failed\n");
		return -EPERM;
	}

	/* Sc Water, RawData Address register points to self-contained water proof */
	if(IsWaterProof == true)
		scap_rawdata = 0xAC;//Water proof on
	else
		scap_rawdata = 0xAB;//Water proof off

	TOUCH_I("Scap Rawdata type(%x)switch\n", scap_rawdata);
	ret = ft5446_reg_write(dev, 0x01, &scap_rawdata, 1);
	if(ret < 0) {
		TOUCH_E("Scap Rawdata type(%x)switch error\n", scap_rawdata);
		return ret;
	}
	mdelay(10);

	TOUCH_I("Read Self-Raw data at once\n");

	ret = ft5446_reg_read(dev, 0x36, &i2c_data[0], SCAP_BUF_SIZE * 2);
	if(ret < 0) {
		TOUCH_E("Read Self-Raw Data error\n");
		return ret;
	}

	data = 0x00;
	ret = ft5446_reg_write(dev, 0x06, &data, 1); // 0: RAWDATA
	if (ret < 0) {
		TOUCH_E("default data select to rawdata error\n");
		return ret;
	}
	mdelay(100);

	/* Combine */
	for (i = 0; i < SCAP_BUF_SIZE; i++) {
		fts_scap_data[i] = (i2c_data[(i * 2)] << 8) + i2c_data[(i * 2)+ 1];
	}

	return 0;
}

int ft5446_prd_get_cb_data(struct device *dev, bool IsWaterProof)
{
	int i, k;
	int ret = 0;
	u8 data = 0x00;
	u8 cb_data = 0x00;

	memset(i2c_data, 0, sizeof(i2c_data));

	if(IsWaterProof == true)
		cb_data = 0x01;//Water proof on
	else
		cb_data = 0x00;//Water proof off

	TOUCH_I("Scap Water proof mode selection(%x)\n", cb_data);
	ret = ft5446_reg_write(dev, 0x44, &cb_data, 1);
	if(ret < 0) {
		TOUCH_E("Scap Water proof mode selection(%x) error\n", cb_data);
		return ret;
	}

	mdelay(20);

	data = 0x00;
	/* Start SCAN */
	for (k = 0; k < 3; k++)
	{
		TOUCH_I("Start Second SCAN (%d/3)\n", k+1);
		ret = ft5446_reg_read(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("start Second scan read error\n");
			return ret;
		}
		data |= 0x80; // 0x40|0x80
		ret = ft5446_reg_write(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("0x00 register 0x80 write error\n");
			return ret;
		}

		mdelay(20);

		for (i = 0; i < 5; i++) {
			ret = ft5446_reg_read(dev, 0x00, &data, 1);
			if (ret < 0) {
				TOUCH_E("scan success check error\n");
				return ret;
			}
			if (data == 0x40) {
				TOUCH_I("Second SCAN Success : 0x%X, %d ms \n", data, i*20);
				break;
			}
			mdelay(20);
		}

		if(i < 5)
			break;

		TOUCH_E("Second SCAN Fail (%d/3)\n", k+1);
	}

	if (k >= 3) {
		TOUCH_E("Second SCAN failed\n");
		return -EPERM;
	}

	/* Read CB data */
	cb_data = 0x00;
	ret = ft5446_reg_write(dev, 0x45, &cb_data, 1);
	if(ret < 0) {
		TOUCH_E("Self cap CB Read Address error\n");
		return ret;
	}
	mdelay(20);

	TOUCH_I("Read CB data at once\n");

	ret = ft5446_reg_read(dev, 0x4E, &i2c_data[0], SCAP_BUF_SIZE);
	if(ret < 0) {
		TOUCH_E("0x4E register read error\n");
		return ret;
	}

	/* Combine */
	for (i = 0; i < SCAP_BUF_SIZE; i++) {
			fts_scap_data[i] = i2c_data[i];
	}

	return 0;
}

int ft5446_prd_get_jitter_data(struct device *dev)
{
	int i, j, k;
	int ret = 0;
	u8 data = 0x00;

	memset(i2c_data, 0, sizeof(i2c_data));

	// Set jitter Test Frame Count
	//low  byte
	data = 0x32;
	ret = ft5446_reg_write(dev, 0x12, &data, 1);
	if (ret < 0) {
		TOUCH_E("set jitter test frame count(low byte) error\n");
		return ret;
	}
	mdelay(10);

	//high  byte
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x13, &data, 1);
	if (ret < 0) {
		TOUCH_E("set jitter test frame count(high byte) error\n");
		return ret;
	}
	mdelay(10);

	// Start Noise Test
	data = 0x01;
	ret = ft5446_reg_write(dev, 0x11, &data, 1);
	if (ret < 0) {
		TOUCH_E("start jitter test error\n");
		return ret;
	}
	mdelay(100);

	// Check Scan is finished
	for (i = 0; i < 100; i++)
	{
		ret = ft5446_reg_read(dev, 0x11, &data, 1);
		if (ret < 0) {
			TOUCH_E("check scan error\n");
			return ret;
		}
		if ((data & 0xff) == 0x00){
			TOUCH_I("Scan finished : %d ms, data = %x\n", i*50 ,data);
			break;
		}
		mdelay(50); //mdelay(20);
	}

	if (i >= 100) {
		TOUCH_E("Scan failed\n");
		return -EPERM;
	}

	// Get Noise data
	TOUCH_I("Read jitter data at once\n");

	// (Get RMS data)->(Get MaxNoise Data)
	ret = ft5446_reg_read(dev, 0x8D, &i2c_data[0], MAX_COL*MAX_ROW*2);
	if(ret < 0) {
		TOUCH_E("read jitter data error\n");
		return ret;
	}
	mdelay(100);

	for (i = 0; i < MAX_ROW; i++) {
		for(j = 0; j < MAX_COL; j++) {
			k = ((j * MAX_ROW) + i) << 1;
			fts_data[i][j] = abs((i2c_data[k] << 8) + i2c_data[k+1]);
		}
	}

	return 0;
}

int ft5446_prd_get_delta_data(struct device *dev)
{
	int i, j, k;
	int ret = 0;
	u8 data = 0x00;
	//int total, offset = 0, read_len;

	//total = MAX_COL*MAX_ROW;

	memset(i2c_data, 0, sizeof(i2c_data));

	// Data Select to diff data
	data = 0x01;
	ret = ft5446_reg_write(dev, 0x06, &data, 1);
	if (ret < 0) {
		TOUCH_E("data select to diff data write error\n");
		return ret;
	}
	mdelay(100);

	/* Start SCAN */
	for (k = 0; k < 3; k++)
	{
		TOUCH_I("Start SCAN (%d/3)\n", k+1);
		ret = ft5446_reg_read(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("start scan read error\n");
			return ret;
		}
		data |= 0x80; // 0x40|0x80
		ret = ft5446_reg_write(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("0x00 register 0x80 write error\n");
			return ret;
		}

		mdelay(10);

		for (i = 0; i < 5; i++) {
			ret = ft5446_reg_read(dev, 0x00, &data, 1);
			if (ret < 0) {
				TOUCH_E("scan success check error\n");
				return ret;
			}
			if (data == 0x40) {
				TOUCH_I("SCAN Success : 0x%X, %d ms \n", data, i*20);
				break;
			}
			mdelay(20);
		}

		if (i < 5) {
			break;
		}

		TOUCH_E("SCAN Fail (%d/3)\n", k+1);
	}

	if (k >= 3) {
		TOUCH_E("SCAN failed\n");
		return -EPERM;
	}

	/* Read Raw data */
	data = 0xAA;    // FT5446 RAWDATA ADDRESS
	ret = ft5446_reg_write(dev, 0x01, &data, 1);
	if(ret < 0) {
		TOUCH_E("read rawdata error\n");
		return ret;
	}
	mdelay(10);

	TOUCH_I("Read Delta at once\n");

	ret = ft5446_reg_read(dev, 0x36, &i2c_data[0], MAX_COL*MAX_ROW*2);   // TH8 Column:34 Row:21
	if(ret < 0) {
		TOUCH_E("0x36 register read error\n");
		return ret;
	}

	// Data Select to raw data
	data = 0x00;
	ret = ft5446_reg_write(dev, 0x06, &data, 1);
	if(ret < 0) {
		TOUCH_E("data select to rawdata error\n");
	}
	mdelay(100);

	for (i = 0; i < MAX_ROW; i++) {
		for(j = 0; j < MAX_COL; j++) {
			k = ((j * MAX_ROW) + i) << 1;
			fts_data[i][j] = (i2c_data[k] << 8) + i2c_data[k+1];
		}
	}

	return 0;
}

int ft5446_prd_get_adc_data(struct device *dev)
{
	int i;
	int ret = 0;
	u8 data = 0x00;
	bool bscan = false;

	memset(i2c_data, 0, sizeof(i2c_data));

	/* Start RAWDATA SCAN */
	TOUCH_I("Start RAWDATA SCAN\n");
	ret = ft5446_reg_read(dev, 0x00, &data, 1);
	if (ret < 0) {
		TOUCH_E("adc rawdata scan read error\n");
		return ret;
	}
	data |= 0x80; // 0x40|0x80
	ret = ft5446_reg_write(dev, 0x00, &data, 1);
	if (ret < 0) {
		TOUCH_E("0x00 register 0x80 write error\n");
		return ret;
	}

	mdelay(20);

	for (i = 0; i < 5; i++) {
		ret = ft5446_reg_read(dev, 0x00, &data, 1);
		if (ret < 0) {
			TOUCH_E("RAWDATA SCAN Success read error\n");
			return ret;
		}
		if (data == 0x40) {
			TOUCH_I("RAWDATA SCAN Success : 0x%X, %d ms \n", data, i*20);
			bscan = true;
			break;
		} else {
			bscan = false;
			mdelay(20);
		}
	}
	if (bscan == false)
		TOUCH_E("RAWDATA SCAN Fail.\n");

	/* ADC sampling*/
	data = 0x01; // 0x01
	ret = ft5446_reg_write(dev, 0x07, &data, 1);
	if (ret < 0) {
		TOUCH_E("0x07 register 0x01 write error\n");
		return ret;
	}

	mdelay(300); // wait to do Scan.

	bscan = false; // need to initialize
	for (i = 0; i < 5; i++) {
		ret = ft5446_reg_read(dev, 0x07, &data, 1);
		if (ret < 0) {
			TOUCH_E("ADC SCAN Success read error\n");
			return ret;
		}
		if (data == 0x00) {
			TOUCH_I("ADC SCAN Success : 0x%X, %d ms \n", data, i*20);
			bscan = true;
			break;
		} else {
			bscan = false;
			mdelay(20);
		}
	}
	if(bscan==false)
		TOUCH_E("ADC SCAN Fail.\n");


	if(bscan==true){
		/* Read ADC data */
		ret = ft5446_reg_read(dev, 0xF4, &i2c_data[0], ADC_BUF_SIZE*2);	  // MH4+ Column:14 Row:28
		//ADC Value [1 + 1 + TX_NUM_MAX + RX_NUM_MAX + 1 + TX_NUM_MAX + RX_NUM_MAX]

		if(ret < 0) {
			TOUCH_E("read ADC data error\n");
			return ret;
		}
	}


	/* ADC data get */
	i=0;
	for (i = 0; i< ADC_BUF_SIZE; i++) {
		fts_data_adc_buff[i] = (i2c_data[i*2] << 8) + i2c_data[i*2+1];
	}

	return 0;
}

int ft5446_prd_test_data_adc(struct device *dev, int* test_result)
{
	struct ft5446_data *d = to_ft5446_data(dev);

	int i;
	int fail_count = 0;
	int ret = 0;
	int index = 0;

	int iGDrefn   = 0;
	int iDoffset  = 0;
	int iMDrefn   = 0;
	int iDsen = 0;
	int fKcal = 1;
	int iRefsen = 57;
	int iDCal = 0;
	int iMa = 0;


	iGDrefn   = fts_data_adc_buff[1];
	iDoffset  = fts_data_adc_buff[0] - 1024;  //3
	iMDrefn   = fts_data_adc_buff[2 + MAX_ROW + MAX_COL];

	*test_result = TEST_PASS;

	/* print ADC DATA */
	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret,
			"Version: v%d.%02d, Bin_version: v%d.%02d\n",
			d->ic_info.version.major, d->ic_info.version.minor,
			d->ic_info.version.bin_major, d->ic_info.version.bin_minor);
	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "[Information] iGDrefn : %5d, iDoffset : %5d, iMDrefn : %5d\n", iGDrefn, iDoffset, iMDrefn);
	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============================= ADC DATA ===============================\n");

	/* print ADC for CG */
	index = 2; // 1 + 1 -> TX_NUM + RX_NUM + 1 + (TX_NUM+RX_NUM) : index change

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "< ADC : Channel to Ground >\n");

	for (i = 0; i < MAX_ROW + MAX_COL; i++) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "[%2d] %5d\n", i+1, fts_data_adc_buff[i + index]);
	}

	/* print ADC for CC */
	index += i; // 1 + 1 + TX_NUM + RX_NUM -> 1 + (TX_NUM+RX_NUM) : index change
	index += 1; // 1 + 1 + TX_NUM + RX_NUM + 1 -> (TX_NUM+RX_NUM) : index change

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "\n< ADC : Channel to Channel >\n");

	for (i = 0; i < MAX_ROW + MAX_COL; i++) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "[%2d] %5d\n", i+1, fts_data_adc_buff[i+ index]);
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "==================================================\n\n");



	/* channel to channel Test : CG Test*/
	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= ADC Open Short Test Result : Channel to Ground =============\n");

	for (i=0; i< MAX_ROW + MAX_COL; i++){
		iDsen = fts_data_adc_buff[i+2];
		if(( 2047 + iDoffset ) - iDsen <= 0 ) {
			continue;
		}

		if(ic_type <= 0x05 || ic_type == 0xff) {
			//fGShortResistance[i] = (float)( iDsen - iDoffset + 410 ) * 25.1 * fKcal / ( 2047 + iDoffset - iDsen ) - 3;
			fGShortResistance[i] = ( iDsen - iDoffset + 410 ) * 25 * fKcal / ( 2047 + iDoffset - iDsen ) - 3;
		}
		else
		{
			if ( iGDrefn - iDsen <= 0 )
			{
				fGShortResistance[i] = CG_MIN;    // WeakShortTest_CG = 1200
				continue;
			}
			//fGShortResistance[i] = (float)(((float)(iDsen - iDoffset + 384) / (float)(iGDrefn - iDsen) * 57) - 1.2);//( ( iDsen - iDoffset + 384 ) * iRsen / (/*temp*/iDrefn - iDsen) ) * fKcal - 1.2;
			fGShortResistance[i] = (((iDsen - iDoffset + 384) / (iGDrefn - iDsen) * 57) - 1);
		}

		if( fGShortResistance[i] < 0 )
			fGShortResistance[i] = 0;

		if( ( fGShortResistance[i] < CG_MIN ) || ( iDsen - iDoffset < 0 ) ) {
			++fail_count;
			*test_result = TEST_FAIL;
		}
	}


	for (i = 0; i < MAX_ROW + MAX_COL; i++) {
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "[%2d] %6d\n", i+1, fGShortResistance[i]);
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "==================================================\n");

	if( fail_count > 0 ) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test FAIL : %d Errors\n\n", fail_count);
	}
	else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test PASS : No Errors\n\n");
	}



	/* channel to channel Test : CC Test*/
	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= ADC Open Short Test Result : Channel to Channel=============\n");

	fail_count = 0;
	fKcal = 1.0;

	if ( iMDrefn > 116 + iDoffset )
		iDCal = iMDrefn;
	else
		iDCal = 116 + iDoffset ;

	for(i = 0; i < MAX_ROW + MAX_COL; i++)
	{
		iDsen = fts_data_adc_buff[i + MAX_ROW + MAX_COL + 3]; // 1 + 1 + TX_NUM + RX_NUM + 1 + (TX_NUM+RX_NUM)
		if(ic_type <= 0x05 || ic_type == 0xff)
		{
			if(iDsen - iMDrefn < 0)  {
				continue;
			}
		}

		if(ic_type <= 0x05 || ic_type == 0xff)
		{
			iMa = iDsen - iDCal;
			iMa = iMa ? iMa : 1;
			fMShortResistance[i] = ( ( 2047 + iDoffset - iDCal ) * 24 / iMa - 27 ) * fKcal - 6;
		}
		else
		{
			if ( iMDrefn - iDsen <= 0 )
			{
				fMShortResistance[i] = CC_MIN;  // WeakShortTest_CC 1200
				continue;
			}
			//fMShortResistance[i] = (float)( iDsen - iDoffset - 123 ) * iRefsen * fKcal / (iMDrefn - iDsen ) - 2;
			fMShortResistance[i] = ( iDsen - iDoffset - 123 ) * iRefsen * fKcal / (iMDrefn - iDsen ) - 2;
		}

		if( fMShortResistance[i] >= -240 && fMShortResistance[i] < 0 )  fMShortResistance[i] = 0;
		else if( fMShortResistance[i] < -240 )  continue;

		if( fMShortResistance[i] <= 0  || fMShortResistance[i] < CC_MIN )
		{
			++fail_count;
			*test_result = TEST_FAIL;
		}
	}

	for (i = 0; i < MAX_ROW + MAX_COL; i++) {
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "[%2d] %6d\n", i+1, fMShortResistance[i]);
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "==================================================\n");

	if( fail_count > 0 ) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test FAIL : %d Errors\n", fail_count);
	}
	else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test PASS : No Errors\n");
	}

	return ret;
}

int ft5446_prd_test_data(struct device *dev, int test_type, int* test_result)
{
	//struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);

	int i, j;
	int ret = 0;

	int limit_upper = 0, limit_lower = 0;
	int min, max/*, aver, stdev*/;
	int fail_count = 0;
	int check_limit = 1;
	u32 notch_bit = 0;
	*test_result = TEST_FAIL;

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret,
			"Version: v%d.%02d, Bin_version: v%d.%02d\n",
			d->ic_info.version.major, d->ic_info.version.minor,
			d->ic_info.version.bin_major, d->ic_info.version.bin_minor);

	if (is_lcm_name("KD-ILI9881C")) {
		/* MH4P */
		notch_bit = 1 << 6 | 1 << 7;
	} else if (is_lcm_name("HLT-JD9365D")) {
		/* MH55 */
		notch_bit = 1 << 5 | 1 << 6 | 1 << 7;
	} else if (is_lcm_name("CTC-ILI9881D")) {
		/* DH5 */    //bringup temp
		notch_bit = 1 << 6;
	} else {
		/* MH5 */
		notch_bit = 0;
	}
	
	TOUCH_I("ignore notch bit: %x", notch_bit);

	switch (test_type) {
		case RAW_DATA_TEST:
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= Raw Data Test Result =============\n");
			limit_upper = RAW_DATA_MAX + RAW_DATA_MARGIN;
			limit_lower = RAW_DATA_MIN - RAW_DATA_MARGIN;
			spec_get_limit(dev, "LowerImageLimit", LowerImage);
			spec_get_limit(dev, "UpperImageLimit", UpperImage);
			break;
		case JITTER_TEST:
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= jitter Test Result =============\n");
			limit_upper = JITTER_MAX;
			limit_lower = JITTER_MIN;
			break;
		case DELTA_SHOW:
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= Delta Result =============\n");
			check_limit = 0;
			break;
		case LPWG_RAW_DATA_TEST:
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= LPWG Raw Data Test Result =============\n");
			limit_upper = RAW_DATA_MAX + RAW_DATA_MARGIN;
			limit_lower = RAW_DATA_MIN - RAW_DATA_MARGIN;
			spec_get_limit(dev, "LowerImageLimit", LowerImage);
			spec_get_limit(dev, "UpperImageLimit", UpperImage);
			break;
		case LPWG_JITTER_TEST:
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= LPWG Jitter Test Result =============\n");
			limit_upper = LPWG_JITTER_MAX;
			limit_lower = LPWG_JITTER_MIN;
			break;
		default:
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test Failed (Invalid test type)\n");
			return ret;
	}

	max = min = fts_data[0][0];

	for (i = 0; i < MAX_ROW; i++) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "[%2d] ", i+1);
		for (j = 0; j < MAX_COL; j++) {

			if (test_type == RAW_DATA_TEST || test_type == LPWG_RAW_DATA_TEST) {
				ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "%5d ", fts_data[i][j]);
			}
			else {
				ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "%4d ", fts_data[i][j]);
			}

			if (test_type == RAW_DATA_TEST || test_type == LPWG_RAW_DATA_TEST) {
				if( i == 0) {
					if (notch_bit & (1 << j)) {
						TOUCH_I("ignore test in notch area[%d][%d] = %d / LowerImage[%d][%d] = %d / UpperImage[%d][%d] = %d\n"
								, i, j, fts_data[i][j], i, j, LowerImage[i][j], i, j, UpperImage[i][j]);
						continue;
					}
				}

				if (check_limit && (fts_data[i][j] < LowerImage[i][j] || fts_data[i][j] > UpperImage[i][j])) {
					TOUCH_I("Failed [%d][%d] = %d / LowerImage[%d][%d] = %d / UpperImage[%d][%d] = %d\n"
							, i, j, fts_data[i][j], i, j, LowerImage[i][j], i, j, UpperImage[i][j]);
					fail_count++;
				}
			} else {
				if (check_limit && (fts_data[i][j] < limit_lower || fts_data[i][j] > limit_upper)) {
					fail_count++;
				}
			}
			if (fts_data[i][j] < min)
				min = fts_data[i][j];
			if (fts_data[i][j] > max)
				max = fts_data[i][j];
		}
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "\n");
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "==================================================\n");

	if(fail_count && check_limit) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test FAIL : %d Errors\n", fail_count);
	}
	else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test PASS : No Errors\n");
		*test_result = TEST_PASS;
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "MAX = %d, MIN = %d, Upper = %d, Lower = %d\n\n", max, min, limit_upper, limit_lower);

	return ret;
}

int ft5446_prd_scap_test_data(struct device *dev, int test_type, bool IsWaterProof, int* test_result)
{
	struct ft5446_data *d = to_ft5446_data(dev);

	int i;
	int ret = 0;

	int limit_upper = 0, limit_lower = 0;
	int fail_count = 0;
	int min, max;
	int check_limit = 1;
	int loop = 0;

	*test_result = TEST_FAIL;

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret,
			"Version: v%d.%02d, Bin_version: v%d.%02d\n",
			d->ic_info.version.major, d->ic_info.version.minor,
			d->ic_info.version.bin_major, d->ic_info.version.bin_minor);

	switch (test_type) {
		case SCAP_RAW_DATA_TEST:
			if(IsWaterProof)
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= SCAP Raw Data(Water proof on) Test Result =============\n");
			else
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= SCAP Raw Data(Water proof off)Test Result =============\n");

			limit_upper = SCAP_RAW_DATA_MAX + SCAP_RAW_DATA_MARGIN;
			limit_lower = SCAP_RAW_DATA_MIN - SCAP_RAW_DATA_MARGIN;
			break;
		case CB_TEST:
			if(IsWaterProof)
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= CB Test(Water proof on) Result =============\n");
			else
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "============= CB Test(Water proof off)Result =============\n");

			limit_upper = CB_MAX;
			limit_lower = CB_MIN;
			break;
		default:
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test Failed (Invalid test type)\n");
			return ret;
	}

	max = min = fts_scap_data[0];

	if(IsWaterProof)
		loop = SCAP_BUF_SIZE;
	else
		loop = SCAP_BUF_RX_SIZE;

	for (i = 0; i < loop; i++) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "[%2d] ", i+1);

		if (test_type == SCAP_RAW_DATA_TEST ) {
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "%5d ", fts_scap_data[i]);
		}
		else {
			ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "%4d ", fts_scap_data[i]);
		}

		if (check_limit && (fts_scap_data[i] < limit_lower || fts_scap_data[i] > limit_upper)) {
			fail_count++;
		}
		if (fts_scap_data[i] < min)
			min = fts_scap_data[i];
		if (fts_scap_data[i] > max)
			max = fts_scap_data[i];

		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "\n");
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "==================================================\n");

	if( fail_count > 0 ) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test FAIL : %d Errors\n", fail_count);
	}
	else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Test PASS : No Errors\n");
		*test_result = TEST_PASS;
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "MAX = %d, MIN = %d, Upper = %d, Lower = %d\n\n", max, min, limit_upper, limit_lower);

	return ret;
}

static ssize_t show_sd(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	int ret_size = 0, ret_total_size = 0;
	int rawdata_ret = TEST_FAIL;
	int jitter_ret = TEST_FAIL;
	int scap_water_on_ret = TEST_FAIL;
	int scap_water_off_ret = TEST_FAIL;
	int cb_water_on_ret = TEST_FAIL;
	int cb_water_off_ret = TEST_FAIL;
	int short_ret = TEST_FAIL;
	u8 data = 0x00;

	TOUCH_I("%s\n", __func__);
	// Check Current State
	if(d->state != TC_STATE_ACTIVE) {
		TOUCH_E("Show_sd is called in NOT Active state\n");
		return 0;
	}

	ft5446_reset_ctrl(dev, HW_RESET);

	/* file create , time log */
	TOUCH_I("Show_sd Test Start\n");
	write_file(dev, "\nShow_sd Test Start", TIME_INFO_SKIP);
	write_file(dev, "\n", TIME_INFO_WRITE);

	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	mutex_lock(&ts->lock);

	ret = ft5446_reg_read(dev, 0xB1, &data, 1);  // IC_TYPE READ
	if (ret < 0) {
		TOUCH_E("read IC_TYPE error\n");
		goto FAIL;
	}

	ic_type= data;

	// Change clb switch
	ret = ft5446_switch_cal(dev, 1);
	if(ret < 0)
		goto FAIL;

	// Change to factory mode
	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	// Start to raw data test
	TOUCH_I("Show_sd : Raw data test\n");
	memset(log_buf, 0, LOG_BUF_SIZE);

	ret = ft5446_prd_check_ch_num(dev);
	if(ret < 0)
		goto FAIL;

#if 1
	ret = ft5446_prd_get_raw_data_sd(dev);
#else
	ret = ft5446_prd_get_raw_data(dev);
#endif

	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_test_data(dev, RAW_DATA_TEST, &rawdata_ret);

	TOUCH_I("Raw Data Test Result : %d\n", rawdata_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);

	// Start to jitter test
	TOUCH_I("Show_sd : jitter test\n");
	memset(log_buf, 0, LOG_BUF_SIZE);
#if 0
	ft5446_reset_ctrl(dev, HW_RESET);

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;
#endif
	ret = ft5446_prd_get_jitter_data(dev);
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_test_data(dev, JITTER_TEST, &jitter_ret);

	TOUCH_I("jitter Test Result : %d\n", jitter_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);

	// Start to SCAP Rawdata test(Water proof on)
	TOUCH_I("Show_sd : SCAP Raw data test(water proof on)\n");
	memset(log_buf, 0, LOG_BUF_SIZE);

	ret = ft5446_prd_get_scap_raw_data(dev, true);
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, SCAP_RAW_DATA_TEST, true, &scap_water_on_ret);

	TOUCH_I("SCAP Raw Data Test(water proof on) Result : %d\n", scap_water_on_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);

	// Start to SCAP Rawdata test(Water proof off)
	TOUCH_I("Show_sd : SCAP Raw data test(Water proof off)\n");
	memset(log_buf, 0, LOG_BUF_SIZE);

	ret = ft5446_prd_get_scap_raw_data(dev, false);
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, SCAP_RAW_DATA_TEST, false, &scap_water_off_ret);

	TOUCH_I("SCAP Raw Data Test(Water proof off) Result : %d\n", scap_water_off_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);

	// Start to SCAP CB test
	TOUCH_I("Show_sd : SCAP CB test(water proof on)\n");
	memset(log_buf, 0, LOG_BUF_SIZE);

	ret = ft5446_prd_get_cb_data(dev, true);
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, CB_TEST, true,  &cb_water_on_ret);

	TOUCH_I("SCAP CB Test(water proof on) Result : %d\n", cb_water_on_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);

	// Start to SCAP CB test(Water proof off)
	TOUCH_I("Show_sd : SCAP CB test(water proof off)\n");
	memset(log_buf, 0, LOG_BUF_SIZE);

	ret = ft5446_prd_get_cb_data(dev, false);
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, CB_TEST, false, &cb_water_off_ret);

	TOUCH_I("SCAP CB Test(water proof off) Result : %d\n", cb_water_off_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);

	/* ADC open_short test */
	TOUCH_I("Show_sd : Weak Short Circuit test(Open/Short Test)\n");
	memset(log_buf, 0, LOG_BUF_SIZE);

	ret = ft5446_prd_get_adc_data(dev);
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_test_data_adc(dev, &short_ret);

	TOUCH_I("Weak Short Circuit Test Result : %d\n", short_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);
#if 0
	// Change to working mode
	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;
#endif
	ft5446_reset_ctrl(dev, HW_RESET);//equal to change to working mode

	// Test result
	ret = snprintf(log_buf, sizeof(log_buf), "\n========RESULT=======\n");
	TOUCH_I("========RESULT=======\n");

	if ((rawdata_ret == TEST_PASS) & (jitter_ret == TEST_PASS)) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Raw Data : Pass\n");
		TOUCH_I("Raw Data : Pass\n");
	} else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Raw Data : Fail (raw:%d/jitter:%d)\n",
				(rawdata_ret == TEST_FAIL)? 0 : 1, (jitter_ret == TEST_FAIL)? 0 : 1);
		TOUCH_I("Raw Data : Fail (raw:%d/jitter:%d)\n",
				(rawdata_ret == TEST_FAIL)? 0 : 1, (jitter_ret == TEST_FAIL)? 0 : 1);
	}

	if ((scap_water_on_ret == TEST_PASS) && (scap_water_off_ret == TEST_PASS) && (cb_water_on_ret == TEST_PASS) && (cb_water_off_ret == TEST_PASS) && (short_ret == TEST_PASS)) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Channel Status : Pass\n");
		TOUCH_I("Channel Status : Pass\n");
	} else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Channel Status: Fail (scap_on:%d/scap_off:%d/cb_on:%d/cb_off:%d/short:%d)\n",
				(scap_water_on_ret == TEST_FAIL)? 0 : 1, (scap_water_off_ret == TEST_FAIL)? 0 : 1,
				(cb_water_on_ret == TEST_FAIL)? 0 : 1, (cb_water_off_ret == TEST_FAIL)? 0 : 1, (short_ret == TEST_FAIL)? 0 : 1);
		TOUCH_I("Channel Status : Fail (scap_on:%d/scap_off:%d/cb_on:%d/cb_off:%d/short:%d)\n",
				(scap_water_on_ret == TEST_FAIL)? 0 : 1, (scap_water_off_ret == TEST_FAIL)? 0 : 1,
				(cb_water_on_ret == TEST_FAIL)? 0 : 1, (cb_water_off_ret == TEST_FAIL)? 0 : 1, (short_ret == TEST_FAIL)? 0 : 1);
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "=====================\n");
	TOUCH_I("=====================\n");

	write_file(dev, log_buf, TIME_INFO_SKIP);
	write_file(dev, "Show_sd Test End\n", TIME_INFO_WRITE);

	log_file_size_check(dev);

	memcpy(buf + ret_total_size, log_buf, ret);
	ret_total_size += ret;

	mutex_unlock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);

	return ret_total_size;

FAIL:
	ft5446_reset_ctrl(dev, HW_RESET);//equal to change to working mode

	ret = snprintf(log_buf, sizeof(log_buf), "\n========RESULT=======\n");
	TOUCH_I("========RESULT=======\n");

	if ((rawdata_ret == TEST_PASS) & (jitter_ret == TEST_PASS)) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Raw Data : Pass\n");
		TOUCH_I("Raw Data : Pass\n");
	} else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Raw Data : Fail\n");
		TOUCH_I("Raw Data : Fail\n");
	}

	if ((scap_water_on_ret == TEST_PASS) && (scap_water_off_ret == TEST_PASS) && (cb_water_on_ret == TEST_PASS) && (cb_water_off_ret == TEST_PASS) && (short_ret == TEST_PASS)) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Channel Status : Pass\n");
		TOUCH_I("Channel Status : Pass\n");
	} else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "Channel Status: Fail\n"),
		TOUCH_I("Channel Status : Fail\n");
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "=====================\n");
	TOUCH_I("=====================\n");

	write_file(dev, log_buf, TIME_INFO_SKIP);
	write_file(dev, "Show_sd Test End\n", TIME_INFO_WRITE);

	log_file_size_check(dev);

	memcpy(buf + ret_total_size, log_buf, ret);
	ret_total_size += ret;

	mutex_unlock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);

	return ret_total_size;
}

static ssize_t show_delta(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;

	TOUCH_I("Show Delta Data\n");

	mutex_lock(&ts->lock);

	// Change clb switch
	ret = ft5446_switch_cal(dev, 0);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_delta_data(dev);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;

	TOUCH_I("Show Delta Data OK !!!\n");

	ret = ft5446_switch_cal(dev, 1);  // 1: Calibration ON
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_test_data(dev, DELTA_SHOW, &test_result);
	memcpy(buf, log_buf, ret_size);
	TOUCH_I("Show Delta Data Result : %d\n", test_result);

	mutex_unlock(&ts->lock);

	return ret_size;

FAIL:

	TOUCH_I("Show Delta Data FAIL !!!\n");
	mutex_unlock(&ts->lock);

	return 0;
}

static ssize_t show_rawdata(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;

	TOUCH_I("Show Raw Data\n");


	mutex_lock(&ts->lock);

	// Change clb switch
	ret = ft5446_switch_cal(dev, 0);  // 0: Calibration OFF
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_raw_data(dev);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;
	TOUCH_I("Show Raw Data OK !!!\n");

	ret = ft5446_switch_cal(dev, 1);  // 1: Calibration ON
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_test_data(dev, RAW_DATA_TEST, &test_result);
	memcpy(buf, log_buf, ret_size);
	TOUCH_I("Raw Data Test Result : %d\n", test_result);

	mutex_unlock(&ts->lock);

	return ret_size;

FAIL:

	TOUCH_I("Show Raw Data FAIL !!!\n");
	mutex_unlock(&ts->lock);

	return 0;
}

static ssize_t show_scap_rawdata_water_on(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;

	TOUCH_I("Show SCAP Raw Data(Water on)\n");

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "LCD off!!! Can not scap_rawdata(water on).\n");
		return ret;
	}

	mutex_lock(&ts->lock);

	// Change clb switch
	ret = ft5446_switch_cal(dev, 0);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_scap_raw_data(dev, true);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;

	TOUCH_I("Show SCAP Raw Data(Water on) OK !!!\n");

	ret = ft5446_switch_cal(dev, 1);  // 1: Calibration ON
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, SCAP_RAW_DATA_TEST, true, &test_result);
	memcpy(buf, log_buf, ret_size);
	TOUCH_I("SCAP Raw Data(Water on) Test Result : %d\n", test_result);

	mutex_unlock(&ts->lock);

	return ret_size;

FAIL:

	TOUCH_I("Show SCAP Raw Data(Water on) FAIL !!!\n");
	mutex_unlock(&ts->lock);

	return 0;
}

static ssize_t show_scap_rawdata_water_off(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;

	TOUCH_I("Show SCAP Raw Data(Water off)\n");

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "LCD off!!! Can not scap_rawdata(water off).\n");
		return ret;
	}

	mutex_lock(&ts->lock);

	// Change clb switch
	ret = ft5446_switch_cal(dev, 0);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_scap_raw_data(dev, false);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;

	TOUCH_I("Show SCAP Raw Data(Water off) OK !!!\n");

	ret = ft5446_switch_cal(dev, 1);  // 1: Calibration ON
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, SCAP_RAW_DATA_TEST, false, &test_result);
	memcpy(buf, log_buf, ret_size);
	TOUCH_I("SCAP Raw Data(Water off) Test Result : %d\n", test_result);

	mutex_unlock(&ts->lock);

	return ret_size;

FAIL:

	TOUCH_I("Show SCAP Raw Data(Water off) FAIL !!!\n");
	mutex_unlock(&ts->lock);

	return 0;
}

static ssize_t show_cb_water_on(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;
	//u8 data = 0x00;

	TOUCH_I("Show CB Data(Water on)\n");

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "LCD off!!! Can not cb(Water on).\n");
		return ret;
	}

	mutex_lock(&ts->lock);

	// Change clb switch
	ret = ft5446_switch_cal(dev, 1);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_cb_data(dev, true);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;

	TOUCH_I("Show CB Data(Water on) OK !!!\n");

	ret = ft5446_switch_cal(dev, 1);  // 1: Calibration ON
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, CB_TEST, true, &test_result);
	memcpy(buf, log_buf, ret_size);
	TOUCH_I("CB Data(Water on) Test Result : %d\n", test_result);

	mutex_unlock(&ts->lock);

	return ret_size;

FAIL:

	TOUCH_I("Show CB Data(Water on) FAIL !!!\n");
	mutex_unlock(&ts->lock);

	return 0;
}

static ssize_t show_cb_water_off(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;
	//u8 data = 0x00;

	TOUCH_I("Show CB Data(Water off)\n");

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "LCD off!!! Can not cb(Water off).\n");
		return ret;
	}

	mutex_lock(&ts->lock);

	// Change clb switch
	ret = ft5446_switch_cal(dev, 1);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_cb_data(dev, false);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;

	TOUCH_I("Show CB Data(Water off) OK !!!\n");

	ret = ft5446_switch_cal(dev, 1);  // 1: Calibration ON
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_scap_test_data(dev, CB_TEST, false, &test_result);
	memcpy(buf, log_buf, ret_size);
	TOUCH_I("CB Data(Water off) Test Result : %d\n", test_result);

	mutex_unlock(&ts->lock);

	return ret_size;

FAIL:

	TOUCH_I("Show CB Data(Water off) FAIL !!!\n");
	mutex_unlock(&ts->lock);

	return 0;
}

static ssize_t show_jitter(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;

	TOUCH_I("Show jitter\n");

//	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	mutex_lock(&ts->lock);

	ft5446_reset_ctrl(dev, HW_RESET);

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_jitter_data(dev);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;

	TOUCH_I("Show jitter OK !!!\n");

	ret_size = ft5446_prd_test_data(dev, JITTER_TEST, &test_result);
	memcpy(buf, log_buf, ret_size);
	TOUCH_I("jitter Test Result : %d\n", test_result);

	mutex_unlock(&ts->lock);
	if(d->state != TC_STATE_ACTIVE) {
		ft5446_lpwg_set(dev);
	}

	return ret_size;

FAIL:

	TOUCH_I("Show jitter FAIL !!!\n");
	ft5446_reset_ctrl(dev, HW_RESET);
	mutex_unlock(&ts->lock);

	if(d->state != TC_STATE_ACTIVE) {
		ft5446_lpwg_set(dev);
	}

	return 0;
}

static ssize_t show_short(struct device *dev, char *buf) {

	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int ret_size = 0;
	int test_result;
	u8 data = 0x00;

	TOUCH_I("Show Short Data\n");

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "LCD off!!! Can not sd.\n");
		return ret;
	}
	mutex_lock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	// IC_TYPE READ for ADC Test
	ret = ft5446_reg_read(dev, 0xB1, &data, 1);
	if (ret < 0) {
		TOUCH_E("read IC_TYPE error\n");
		return ret;
	}

	ic_type= data;

	// Change clb switch
	ret = ft5446_switch_cal(dev, 0);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_prd_get_adc_data(dev);
	if(ret < 0)
		goto FAIL;

	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;

	// Change clb switch
	ret = ft5446_switch_cal(dev, 1);
	if(ret < 0)
		goto FAIL;

	TOUCH_I("Show Short Data OK !!!\n");

	ret_size = ft5446_prd_test_data_adc(dev, &test_result);

	memcpy(buf, log_buf, ret_size);
	TOUCH_I("Show Short Data Result : %d\n", test_result);

	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);

	return ret_size;

FAIL:

	TOUCH_I("Show Delta Data FAIL !!!\n");
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);

	return 0;
}

static ssize_t show_limit(struct device *dev, char *buf)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	int i, j;

	TOUCH_TRACE();
	TOUCH_I("get_limit\n");

	if (d->limit_type == 1) {

		spec_get_limit(dev, "UpperImageLimit", UpperImage);

		ret = snprintf(buf + ret, PAGE_SIZE - ret, "======UpperImage====\n");
		for (i = 0; i < MAX_ROW; i++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", i+1);
			for (j = 0; j < MAX_COL; j++) {

				ret += snprintf(buf + ret, PAGE_SIZE - ret, "%5d ", UpperImage[i][j]);
			}
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		}
	} else {

		spec_get_limit(dev, "LowerImageLimit", LowerImage);

		ret = snprintf(buf + ret, PAGE_SIZE - ret, "======LowerImage====\n");
		for (i = 0; i < MAX_ROW; i++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", i+1);
			for (j = 0; j < MAX_COL; j++) {

				ret += snprintf(buf + ret, PAGE_SIZE - ret, "%5d ", LowerImage[i][j]);
			}
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		}
	}


	return ret;

}

static ssize_t store_limit(struct device *dev, const char *buf, size_t count)
{
	struct ft5446_data *d = to_ft5446_data(dev);
	int value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%d", &value) <= 0 || value < 0 || value > 1) {
		TOUCH_I("Invalid limit Type, please input 0~1\n");
		return count;
	}

	d->limit_type = value;

	TOUCH_I("Set Limit Type = %s\n", (d->limit_type > 0) ? "Upper Limit" : "Lower Limit");

	return count;
}
static ssize_t show_lpwg_sd(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ft5446_data *d = to_ft5446_data(dev);
	int ret = 0;
	int ret_size = 0, ret_total_size = 0;
	int rawdata_ret = TEST_FAIL;
	int jitter_ret = TEST_FAIL;
	u8 data = 0x00;


	// Check Current State
	if(d->state == TC_STATE_ACTIVE) {
		TOUCH_E("Show_lpwg_sd called in Active state\n");
		return 0;
	}

	ft5446_reset_ctrl(dev, HW_RESET);

	/* file create , time log */
	TOUCH_I("Show_lpwg_sd Test Start\n");
	write_file(dev, "\nShow_lpwg_sd Test Start", TIME_INFO_SKIP);
	write_file(dev, "\n", TIME_INFO_WRITE);

	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	mutex_lock(&ts->lock);

	// IC_TYPE READ for ADC Test
	ret = ft5446_reg_read(dev, 0xB1, &data, 1);
	if (ret < 0) {
		TOUCH_E("read IC_TYPE error\n");
		goto FAIL;
	}

	ic_type= data;

	// Change clb switch
	ret = ft5446_switch_cal(dev, 1);
	if(ret < 0)
		goto FAIL;

	// Change to factory mode
	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;

	// Start to lpwg raw data test
	TOUCH_I("Show_lpwg_sd : LPWG Raw data test\n");
	memset(log_buf, 0, LOG_BUF_SIZE);

	ret = ft5446_prd_check_ch_num(dev);
	if(ret < 0)
		goto FAIL;

#if 1
	ret = ft5446_prd_get_raw_data_sd(dev);
#else
	ret = ft5446_prd_get_raw_data(dev);
#endif

	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_test_data(dev, LPWG_RAW_DATA_TEST, &rawdata_ret);

	TOUCH_I("LPWG RawData Test Result : %d\n", rawdata_ret);
	////ret_total_size += ret_size;
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);

	// Start to lpwg jitter test
	TOUCH_I("Show_lpwg_sd : LPWG jitter test\n");
	memset(log_buf, 0, LOG_BUF_SIZE);
#if 0
	ft5446_reset_ctrl(dev, HW_RESET);

	ret = ft5446_change_op_mode(dev, FTS_FACTORY_MODE);
	if(ret < 0)
		goto FAIL;
#endif
	ret = ft5446_prd_get_jitter_data(dev);
	if(ret < 0)
		goto FAIL;

	ret_size = ft5446_prd_test_data(dev, LPWG_JITTER_TEST, &jitter_ret);

	TOUCH_I("LPWG jitter Test Result : %d\n", jitter_ret);
	write_file(dev, log_buf, TIME_INFO_SKIP);

	msleep(30);
#if 0
	// Change to working mode
	ret = ft5446_change_op_mode(dev, FTS_WORK_MODE);
	if(ret < 0)
		goto FAIL;
#endif
	ft5446_reset_ctrl(dev, HW_RESET);//equal to change to working mode

	// Test result
	ret = snprintf(log_buf, sizeof(log_buf), "\n========RESULT=======\n");
	TOUCH_I("========RESULT=======\n");

	if ((rawdata_ret == TEST_PASS) & (jitter_ret == TEST_PASS)) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "LPWG RawData : Pass\n");
		TOUCH_I("LPWG RawData : Pass\n");
	} else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "LPWG RawData : Fail (raw:%d/jitter:%d)\n",
				(rawdata_ret == TEST_FAIL)? 0 : 1, (jitter_ret == TEST_FAIL)? 0 : 1);
		TOUCH_I("LPWG RawData : Fail (raw:%d/jitter:%d)\n",
				(rawdata_ret == TEST_FAIL)? 0 : 1, (jitter_ret == TEST_FAIL)? 0 : 1);
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "=====================\n");
	TOUCH_I("=====================\n");
	write_file(dev, log_buf, TIME_INFO_SKIP);
	write_file(dev, "Show_lpwg_sd Test End\n", TIME_INFO_WRITE);

	log_file_size_check(dev);

	memcpy(buf + ret_total_size, log_buf, ret);
	ret_total_size += ret;

	mutex_unlock(&ts->lock);
	if(d->state != TC_STATE_ACTIVE) {
		ft5446_lpwg_set(dev);
	}
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);

	return ret_total_size;
FAIL:
	// Test result
	ret = snprintf(log_buf, sizeof(log_buf), "\n========RESULT=======\n");
	TOUCH_I("========RESULT=======\n");

	if ((rawdata_ret == TEST_PASS) & (jitter_ret == TEST_PASS)) {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "LPWG RawData : Pass\n");
		TOUCH_I("LPWG RawData : Pass\n");
	} else {
		ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "LPWG RawData : Fail\n");
		TOUCH_I("LPWG RawData : Fail\n");
	}

	ret += snprintf(log_buf + ret, sizeof(log_buf) - ret, "=====================\n");
	TOUCH_I("=====================\n");
	write_file(dev, log_buf, TIME_INFO_SKIP);
	write_file(dev, "Show_lpwg_sd Test End\n", TIME_INFO_WRITE);

	log_file_size_check(dev);

	memcpy(buf + ret_total_size, log_buf, ret);
	ret_total_size += ret;

	mutex_unlock(&ts->lock);
	if(d->state != TC_STATE_ACTIVE) {
		ft5446_lpwg_set(dev);
	}
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);

	return ret_total_size;
}

static TOUCH_ATTR(sd, show_sd, NULL);
static TOUCH_ATTR(lpwg_sd, show_lpwg_sd, NULL);
static TOUCH_ATTR(delta, show_delta, NULL);
static TOUCH_ATTR(rawdata, show_rawdata, NULL);
static TOUCH_ATTR(jitter, show_jitter, NULL);
static TOUCH_ATTR(short_test, show_short, NULL);
static TOUCH_ATTR(scap_rawdata_water_on, show_scap_rawdata_water_on, NULL);
static TOUCH_ATTR(scap_rawdata_water_off, show_scap_rawdata_water_off, NULL);
static TOUCH_ATTR(cb_water_on, show_cb_water_on, NULL);
static TOUCH_ATTR(cb_water_off, show_cb_water_off, NULL);
static TOUCH_ATTR(limit, show_limit, store_limit);

static struct attribute *prd_attribute_list[] = {
	&touch_attr_sd.attr,
	&touch_attr_lpwg_sd.attr,
	&touch_attr_delta.attr,
	&touch_attr_rawdata.attr,
	&touch_attr_jitter.attr,
	&touch_attr_short_test.attr,
	&touch_attr_scap_rawdata_water_on.attr,
	&touch_attr_scap_rawdata_water_off.attr,
	&touch_attr_cb_water_on.attr,
	&touch_attr_cb_water_off.attr,
	&touch_attr_limit.attr,
	NULL,
};

static const struct attribute_group prd_attribute_group = {
	.attrs = prd_attribute_list,
};

int ft5446_prd_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &prd_attribute_group);

	if (ret < 0) {
		TOUCH_E("failed to create sysfs\n");
		return ret;
	}

	return ret;
}

