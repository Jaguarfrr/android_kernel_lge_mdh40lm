/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k3l6_mipi_raw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/


#define PFX "S5K3l6_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k3l6_mipi_raw.h"

#define MULTI_WRITE 1

//#define S5K3L6_EEPROM_CALI
//#undef S5K3L6_EEPROM_CALI
#define EEPROM_GT24C64_ID 0xA0



#define S5K3L6_OTP  1

#if S5K3L6_OTP
#define S5K3L6_OTP_SIZE 0x1458 + 1
unsigned char s5k3l6_otp_data[S5K3L6_OTP_SIZE];
#define MODULE_INFO_FLAG 0
#define MODULE_INFO_START 1
#define MODULE_INFO_END 7
#define MODULE_INFO_CHECKSUM 8
#define MODULE_INFO_DATA_SIZE 7
unsigned char s5k3l6_module_info_data[MODULE_INFO_DATA_SIZE] = {0};

#define AWB_51K_INFO_FLAG 0x09
#define AWB_51K_INFO_START 0x0a
#define AWB_51K_INFO_END 0x19
#define AWB_51K_INFO_CHECKSUM 0x1a
#define AWB_51K_DATA_SIZE 0x10
unsigned char s5k3l6_awb_51k_data_valid = 0;

#define AWB_30K_INFO_FLAG 0x1b
#define AWB_30K_INFO_START 0x1c
#define AWB_30K_INFO_END 0x2b
#define AWB_30K_INFO_CHECKSUM 0x2c
#define AWB_30K_DATA_SIZE 0x10
unsigned char s5k3l6_awb_30k_data_valid = 0;

#define AF_INFO_FLAG 0x2d
#define AF_INFO_START 0x2f
#define AF_INFO_END 0x32
#define AF_INFO_CHECKSUM 0x33
#define AF_DATA_SIZE 0x04
unsigned char s5k3l6_af_data_valid = 0;

#define LSC_51K_INFO_FLAG 0x34
#define LSC_51K_INFO_START 0x35
#define LSC_51K_INFO_END 0x780
#define LSC_51K_INFO_CHECKSUM 0x781
#define LSC_51K_DATA_SIZE 1868
unsigned char s5k3l6_lsc_51k_data_valid = 0;

#define LSC_40K_INFO_FLAG 0x782
#define LSC_40K_INFO_START 0x783
#define LSC_40K_INFO_END 0xece
#define LSC_40K_INFO_CHECKSUM 0xecf
#define LSC_40K_DATA_SIZE 1868
unsigned char s5k3l6_lsc_40k_data_valid = 0;

#define PDAF_INFO_FLAG 0xed0
#define PDAF_INFO_START 0xed1
#define PDAF_INFO_END 0x1456
#define PDAF_INFO_CHECKSUM 0x1457
#define PDAF_DATA_SIZE 1414
unsigned char s5k3l6_pdaf_data_valid = 0;
#endif
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020; /*trans# max is 255, each 4 bytes*/
#else
static const int I2C_BUFFER_LEN = 4;
#endif

#define LOG_INF(format, args...) pr_debug(PFX "info [%s] " format, __func__, ##args)
#define LOG_DBG(format, args...) pr_err(PFX "debg [%s] " format, __func__, ##args)
extern char rear_sensor_name[20];/*LGE_CHANGE, 2019-07-04, add the camera identifying logic , kyunghun.oh@lge.com*/

/*
 * #define LOG_INF(format, args...) pr_debug(
 * PFX "[%s] " format, __func__, ##args)
 */

/* Camera Hardwareinfo */
//extern struct global_otp_struct hw_info_main_otp;

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K3L6_SENSOR_ID,
#if defined(TRAN_ID5A)
    .checksum_value = 0x143d0c73,
#else
	.checksum_value = 0x44724ea1,
#endif
	.pre = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,				//record different mode's linelength
		.framelength = 3260,			//record different mode's framelength
		.startx= 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 2104,		//record different mode's width of grabwindow
		.grabwindow_height = 1560,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 227200000,
	},
	.cap = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 3260,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.cap1 = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 3260,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.cap2 = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 3260,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.normal_video = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 3260,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.hs_video = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 816,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 640,		//record different mode's width of grabwindow
		.grabwindow_height = 480,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 1200,
		.mipi_pixel_rate = 74400000,

	},
	.slim_video = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 3260,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 1920,		//record different mode's width of grabwindow
		.grabwindow_height = 1080,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 208000000,

	},
	.custom1 = {	//Copy from cap
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 6075,//5808,				//record different mode's linelength
		.framelength = 3292,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.mipi_pixel_rate = 429600000,
		.max_framerate = 240,
	},
	.custom2 = {	//Copy from pre
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,				//record different mode's linelength
		.framelength = 4075,			//record different mode's framelength
		.startx= 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 2104,		//record different mode's width of grabwindow
		.grabwindow_height = 1560,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 240,
		.mipi_pixel_rate = 227200000,
	},

	.margin = 5,			//sensor framelength & shutter margin
	.min_shutter = 4,               /*min shutter*/

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0xFFFF-5,//REG0x0202 <=REG0x0340-5//max framelength by sensor register's limitation

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.frame_time_delay_frame = 1,//The delay frame of setting frame length
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 6,	/* support sensor mode num */

	.cap_delay_frame = 1,
	.pre_delay_frame = 1,
	.video_delay_frame = 1,

	/* enter high speed video  delay frame num */
	.hs_video_delay_frame = 2,      //enter high speed video  delay frame num
	.slim_video_delay_frame = 2,//enter slim video delay frame num
	.custom1_delay_frame = 2,	/* Dual camera frame sync control */
	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */
	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 1,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,         /*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 400, /*support 1MHz write*/
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x20, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x0200,					//current shutter
	.gain = 0x0100,						//current gain
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

		/* test pattern mode or not.
		 * KAL_FALSE for in test pattern mode,
		 * KAL_TRUE for normal output
		 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	/* sensor need support LE, SE with HDR feature */
	.ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,	/* record current sensor's i2c write id */

};


//int chip_id;
/* VC_Num, VC_PixelNum, ModeSelect, EXPO_Ratio, ODValue, RG_STATSMODE */
/* VC0_ID, VC0_DataType, VC0_SIZEH, VC0_SIZE,
 * VC1_ID, VC1_DataType, VC1_SIZEH, VC1_SIZEV
 */
/* VC2_ID, VC2_DataType, VC2_SIZEH, VC2_SIZE,
 * VC3_ID, VC3_DataType, VC3_SIZEH, VC3_SIZEV
 */
#if 0
/* Preview mode setting */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x0910, 0x06D0, 0x01, 0x00, 0x0000, 0x0000,
	0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
	0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
	0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000}
	};
#endif
/* Sensor output window information */

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
 { 4208, 3120,	  0,  	0, 4208, 3120, 2104, 1560,   0,	0, 2104, 1560, 	 0, 0, 2104, 1560}, // Preview
 { 4208, 3120,	  0,  	0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // capture
 { 4208, 3120,	  0,  	0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // video
 { 4208, 3120,	824,  600, 2560, 1920,  640,  480,   0,	0,  640,  480, 	 0, 0,  640,  480}, //hight speed video
 { 4208, 3120,	184,  480, 3840, 2160, 1920, 1080,   0,	0, 1920, 1080, 	 0, 0, 1920, 1080}, // slim video
 { 4208, 3120,	  0,  	0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // custom1
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
	.i4OffsetX = 24,
	.i4OffsetY = 24,
	.i4PitchX = 64,
	.i4PitchY = 64,
	.i4PairNum =16,
	.i4SubBlkW =16,
	.i4SubBlkH =16,
	.i4BlockNumX = 65,
	.i4BlockNumY = 48,
	.iMirrorFlip = 0,
	.i4PosL = {{28,31},{44,35},{64,35},{80,31},{32,51},{48,55},{60,55},{76,51},{32,67},{48,63},{60,63},{76,67},{28,87},{44,83},{64,83},{80,87}},
	.i4PosR = {{28,35},{44,39},{64,39},{80,35},{32,47},{48,51},{60,51},{76,47},{32,71},{48,67},{60,67},{76,71},{28,83},{44,79},{64,79},{80,83}},
};


#if 0
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	iReadReg((u16) addr, (u8 *) &get_byte, imgsensor.i2c_write_id);
	return get_byte;
}

#define write_cmos_sensor(addr, para) iWriteReg(\
	(u16) addr, (u32) para, 1,  imgsensor.i2c_write_id)
#endif
#define RWB_ID_OFFSET 0x0F73
#define EEPROM_READ_ID  0xA4
#define EEPROM_WRITE_ID   0xA5

#if 0
static kal_uint16 is_RWB_sensor(void)
{
	kal_uint16 get_byte = 0;

	char pusendcmd[2] = {
		(char)(RWB_ID_OFFSET >> 8), (char)(RWB_ID_OFFSET & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, EEPROM_READ_ID);
	return get_byte;
}
#endif
#if 0
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte << 8) & 0xff00) | ((get_byte >> 8) & 0x00ff);
}
#endif

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}
//#ifdef S5K3L6_EEPROM_CALI
#if 0
static kal_uint16 read_eeprom(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_GT24C64_ID);

	return get_byte;
}
#endif
static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* return; //for test */
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}				/*      set_dummy  */

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE

	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(
		puSendCmd, tosend, imgsensor.i2c_write_id, 4,
				     imgsensor_info.i2c_speed);
		tosend = 0;
	}
#else
		iWriteRegI2CTiming(puSendCmd, 4,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

		tosend = 0;

#endif
	}
	return 0;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */

static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	LOG_DBG("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		//write_cmos_sensor(0x6214, 0x7970);
		write_cmos_sensor(0x0100, 0x0100);
	} else {
		//write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0100, 0x0000);
		check_streamoff();
	}
	return ERROR_NONE;
}

static void write_shutter(kal_uint16 shutter)
{

	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length);

		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length);
		pr_debug("(else)imgsensor.frame_length = %d\n",
			imgsensor.frame_length);

	}
	/* Update Shutter*/
	write_cmos_sensor(0x0202, (shutter) & 0xFFFF);

	pr_debug("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}				/*      write_shutter  */
#if 1
static void set_shutter_frame_length(
	kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	 spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);

	if (frame_length > 1)
		imgsensor.frame_length = frame_length;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;

	if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		shutter =
		(imgsensor_info.max_frame_length - imgsensor_info.margin);

	if (imgsensor.autoflicker_en) {

		realtime_fps =
	   imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length*/
			write_cmos_sensor(0x0340, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length*/
		 write_cmos_sensor(0x0340, imgsensor.frame_length);
	}
	/* Update Shutter*/
	write_cmos_sensor(0x0202, (shutter) & 0xFFFF);

	pr_debug("Add for N3D! shutterlzl =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}
#endif

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}				/*      set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain / 2;
	return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		LOG_DBG("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

	write_cmos_sensor(0x0204, reg_gain);
	write_cmos_sensor_8(0x0104, 0x00);
    /*write_cmos_sensor_8(0x0204,(reg_gain>>8));*/
    /*write_cmos_sensor_8(0x0205,(reg_gain&0xff));*/

	return gain;
}				/*      set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{

	kal_uint8 itemp;

	LOG_DBG("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, itemp);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x02);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x01);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x03);
		break;
	}
}

/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif

static kal_uint16 addr_data_pair_init_3l6[] = {
	0x3084,0x1314,
	0x3266,0x0001,
	0x3242,0x2020,
	0x306A,0x2F4C,
	0x306C,0xCA01,
	0x307A,0x0D20,
	0x309E,0x002D,
	0x3072,0x0013,
	0x3074,0x0977,
	0x3076,0x9411,
	0x3024,0x0016,
	0x3070,0x3D00,
	0x3002,0x0E00,
	0x3006,0x1000,
	0x300A,0x0C00,
	0x3010,0x0400,
	0x3018,0xC500,
	0x303A,0x0204,
	0x3452,0x0001,
	0x3454,0x0001,
	0x3456,0x0001,
	0x3458,0x0001,
	0x345a,0x0002,
	0x345C,0x0014,
	0x345E,0x0002,
	0x3460,0x0014,
	0x3464,0x0006,
	0x3466,0x0012,
	0x3468,0x0012,
	0x346A,0x0012,
	0x346C,0x0012,
	0x346E,0x0012,
	0x3470,0x0012,
	0x3472,0x0008,
	0x3474,0x0004,
	0x3476,0x0044,
	0x3478,0x0004,
	0x347A,0x0044,
	0x347E,0x0006,
	0x3480,0x0010,
	0x3482,0x0010,
	0x3484,0x0010,
	0x3486,0x0010,
	0x3488,0x0010,
	0x348A,0x0010,
	0x348E,0x000C,
	0x3490,0x004C,
	0x3492,0x000C,
	0x3494,0x004C,
	0x3496,0x0020,
	0x3498,0x0006,
	0x349A,0x0008,
	0x349C,0x0008,
	0x349E,0x0008,
	0x34A0,0x0008,
	0x34A2,0x0008,
	0x34A4,0x0008,
	0x34A8,0x001A,
	0x34AA,0x002A,
	0x34AC,0x001A,
	0x34AE,0x002A,
	0x34B0,0x0080,
	0x34B2,0x0006,
	0x32A2,0x0000,
	0x32A4,0x0000,
	0x32A6,0x0000,
	0x32A8,0x0000,
	0x3066,0x7E00,
	0x3004,0x0800,
	0X3934,0x1C00,
};

static void sensor_init(void)
{
	LOG_DBG("sensor_init() E\n");
	/* initial sequence */
	// Convert from : "InitGlobal.sset"

	write_cmos_sensor(0x0100,0x0000);
	write_cmos_sensor(0x0000,0x0040);
	write_cmos_sensor(0x0000,0x30C6);
	mdelay(3);

	table_write_cmos_sensor(addr_data_pair_init_3l6,
		sizeof(addr_data_pair_init_3l6) / sizeof(kal_uint16));
}/* sensor_init */

static kal_uint16 addr_data_pair_pre_3l6[] = {
	0x0344,0x0008,
	0x0346,0x0008,
	0x0348,0x1077,
	0x034A,0x0C37,
	0x034C,0x0838,
	0x034E,0x0618,
	0x0900,0x0122,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0001,
	0x0386,0x0003,
	0x0114,0x0330,
	0x0110,0x0002,
	0x0136,0x1800,
	0x0304,0x0004,
	0x0306,0x0078,
	0x3C1E,0x0000,
	0x030C,0x0003,
	0x030E,0x0047,
	0x3C16,0x0001,
	0x0300,0x0006,
	0x0342,0x1320,
	0x0340,0x0CBC,
	0x38C4,0x0004,
	0x38D8,0x0011,
	0x38DA,0x0005,
	0x38DC,0x0005,
	0x38C2,0x0005,
	0x38C0,0x0004,
	0x38D6,0x0004,
	0x38D4,0x0004,
	0x38B0,0x0007,
	0x3932,0x1000,
	0x3938,0x000C,
	0x0820,0x0238,
	0x380C,0x0049,
	0x3064,0xFFCF,
	0x309C,0x0640,
	0x3090,0x8000,
	0x3238,0x000B,
	0x314A,0x5F02,
	0x3300,0x0000,
	0x3400,0x0000,
	0x3402,0x4E46,
	0x32B2,0x0008,
	0x32B4,0x0008,
	0x32B6,0x0008,
	0x32B8,0x0008,
	0x3C34,0x0048,
	0x3C36,0x3000,
	0x3C38,0x0020,
	0x393E,0x4000,
	0x3C1E,0x0100,
	0x0100,0x0100,
	0x3C1E,0x0000,
};

static void preview_setting(void)
{
	LOG_DBG("preview_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_pre_3l6,
			sizeof(addr_data_pair_pre_3l6) / sizeof(kal_uint16));
}/* preview_setting */

static kal_uint16 addr_data_pair_cap_3l6[] = {
	0x0100,0x0000,
	0x0344,0x0008,
	0x0346,0x0008,
	0x0348,0x1077,
	0x034A,0x0C37,
	0x034C,0x1070,
	0x034E,0x0C30,
	0x0900,0x0000,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0001,
	0x0386,0x0001,
	0x0114,0x0330,
	0x0110,0x0002,
	0x0136,0x1800,
	0x0304,0x0004,
	0x0306,0x0078,
	0x3C1E,0x0000,
	0x030C,0x0004,
	0x030E,0x0064,
	0x3C16,0x0000,
	0x0300,0x0006,
	0x0342,0x1320,
	0x0340,0x0CBC,
	0x38C4,0x0009,
	0x38D8,0x002A,
	0x38DA,0x000A,
	0x38DC,0x000B,
	0x38C2,0x000A,
	0x38C0,0x000F,
	0x38D6,0x000A,
	0x38D4,0x0009,
	0x38B0,0x000F,
	0x3932,0x1800,
	0x3938,0x000C,
	0x0820,0x04B0,
	0x380C,0x0090,
	0x3064,0xEFCF,
	0x309C,0x0640,
	0x3090,0x8800,
	0x3238,0x000C,
	0x314A,0x5F00,
	0x3300,0x0000,
	0x3400,0x0000,
	0x3402,0x4E42,
	0x32B2,0x0006,
	0x32B4,0x0006,
	0x32B6,0x0006,
	0x32B8,0x0006,
	0x3C34,0x0008,
	0x3C36,0x0000,
	0x3C38,0x0000,
	0x393E,0x4000,
	0x3C1E,0x0100,
	0x0100,0x0100,
	0x3C1E,0x0000,
};

static void capture_setting(kal_uint16 currefps)
{
	LOG_DBG("capture_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_cap_3l6,
			sizeof(addr_data_pair_cap_3l6) / sizeof(kal_uint16));
}

#if 0 //unused code, soojong.jin@lge.com
static kal_uint16 addr_data_pair_video_3l6[] = {
	0x0100,0x0000,
	0x0344,0x0008,
	0x0346,0x0008,
	0x0348,0x1077,
	0x034A,0x0C37,
	0x034C,0x1070,
	0x034E,0x0C30,
	0x0900,0x0000,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0001,
	0x0386,0x0001,
	0x0114,0x0330,
	0x0110,0x0002,
	0x0136,0x1800,
	0x0304,0x0004,
	0x0306,0x0078,
	0x3C1E,0x0000,
	0x030C,0x0003,
	0x030E,0x004B,
	0x3C16,0x0000,
	0x0300,0x0006,
	0x0342,0x1320,
	0x0340,0x0CBC,
	0x38C4,0x0009,
	0x38D8,0x002A,
	0x38DA,0x000A,
	0x38DC,0x000B,
	0x38C2,0x000A,
	0x38C0,0x000F,
	0x38D6,0x000A,
	0x38D4,0x0009,
	0x38B0,0x000F,
	0x3932,0x1800,
	0x3938,0x000C,
	0x0820,0x04B0,
	0x380C,0x0090,
	0x3064,0xFFCF,
	0x309C,0x0640,
	0x3090,0x8800,
	0x3238,0x000C,
	0x314A,0x5F00,
	0x3300,0x0000,
	0x3400,0x0000,
	0x3402,0x4E42,
	0x32B2,0x0006,
	0x32B4,0x0006,
	0x32B6,0x0006,
	0x32B8,0x0006,
	0x3C34,0x0048,
	0x3C36,0x3000,
	0x3C38,0x0020,
	0x393E,0x4000,
	0x3C1E,0x0100,
	0x0100,0x0100,
	0x3C1E,0x0000,
};

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_DBG("normal_video_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_video_3l6,
		sizeof(addr_data_pair_video_3l6) / sizeof(kal_uint16));
}
#endif

static kal_uint16 addr_data_pair_hs_3l6[] = {
	0x0100,0x0000,
	0x0344,0x0340,
	0x0346,0x0260,
	0x0348,0x0D3F,
	0x034A,0x09DF,
	0x034C,0x0280,
	0x034E,0x01E0,
	0x0900,0x0144,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0001,
	0x0386,0x0007,
	0x0114,0x0330,
	0x0110,0x0002,
	0x0136,0x1800,
	0x0304,0x0004,
	0x0306,0x0078,
	0x3C1E,0x0000,
	0x030C,0x0003,
	0x030E,0x005D,
	0x3C16,0x0003,
	0x0300,0x0006,
	0x0342,0x1320,
	0x0340,0x0330,
	0x38C4,0x0006,
	0x38D8,0x0003,
	0x38DA,0x0003,
	0x38DC,0x0017,
	0x38C2,0x0008,
	0x38C0,0x0000,
	0x38D6,0x0013,
	0x38D4,0x0005,
	0x38B0,0x0002,
	0x3932,0x1800,
	0x3938,0x200C,
	0x0820,0x00BA,
	0x380C,0x0023,
	0x3064,0xFFCF,
	0x309C,0x0640,
	0x3090,0x8000,
	0x3238,0x000A,
	0x314A,0x5F00,
	0x3300,0x0000,
	0x3400,0x0000,
	0x3402,0x4E46,
	0x32B2,0x000A,
	0x32B4,0x000A,
	0x32B6,0x000A,
	0x32B8,0x000A,
	0x3C34,0x0048,
	0x3C36,0x4000,
	0x3C38,0x0020,
	0x393E,0x4000,
	0x3C1E,0x0100,
	0x0100,0x0100,
	0x3C1E,0x0000,
};

static void hs_video_setting(void)
{
	LOG_DBG("hs_video_setting() E\n");
	/*//VGA 120fps*/

	/*// Convert from : "Init.txt"*/
	/*check_streamoff();*/
	table_write_cmos_sensor(addr_data_pair_hs_3l6,
			sizeof(addr_data_pair_hs_3l6) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_slim_3l6[] = {
	0x0100,0x0000,
	0x0344,0x00C0,
	0x0346,0x01E8,
	0x0348,0x0FBF,
	0x034A,0x0A57,
	0x034C,0x0780,
	0x034E,0x0438,
	0x0900,0x0122,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0001,
	0x0386,0x0003,
	0x0114,0x0330,
	0x0110,0x0002,
	0x0136,0x1800,
	0x0304,0x0004,
	0x0306,0x0078,
	0x3C1E,0x0000,
	0x030C,0x0003,
	0x030E,0x0082,
	0x3C16,0x0002,
	0x0300,0x0006,
	0x0342,0x1320,
	0x0340,0x0CBC,
	0x38C4,0x0004,
	0x38D8,0x000F,
	0x38DA,0x0005,
	0x38DC,0x0005,
	0x38C2,0x0004,
	0x38C0,0x0003,
	0x38D6,0x0004,
	0x38D4,0x0003,
	0x38B0,0x0006,
	0x3932,0x2000,
	0x3938,0x000C,
	0x0820,0x0208,
	0x380C,0x0049,
	0x3064,0xFFCF,
	0x309C,0x0640,
	0x3090,0x8000,
	0x3238,0x000B,
	0x314A,0x5F02,
	0x3300,0x0000,
	0x3400,0x0000,
	0x3402,0x4E46,
	0x32B2,0x0008,
	0x32B4,0x0008,
	0x32B6,0x0008,
	0x32B8,0x0008,
	0x3C34,0x0048,
	0x3C36,0x5000,
	0x3C38,0x0020,
	0x393E,0x4000,
	0x3C1E,0x0100,
	0x0100,0x0100,
	0x3C1E,0x0000,
};

static void slim_video_setting(void)
{
	LOG_DBG("slim_video_setting() E\n");
	/* 1080p 60fps */
	/* Convert from : "Init.txt"*/
	table_write_cmos_sensor(addr_data_pair_slim_3l6,
		sizeof(addr_data_pair_slim_3l6) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom1_3l6[] = {
		0x0344, 0x0008,
		0x0346, 0x0008,
		0x0348, 0x1077,
		0x034A, 0x0C37,
		0x034C, 0x1070,
		0x034E, 0x0C30,
		0x0900, 0x0000,
		0x0380, 0x0001,
		0x0382, 0x0001,
		0x0384, 0x0001,
		0x0386, 0x0001,
		0x0114, 0x0330,
		0x0110, 0x0002,
		0x0136, 0x1800,
		0x0304, 0x0004,
		0x0306, 0x0078,
		0x3C1E, 0x0000,
		0x030C, 0x0004,
		0x030E, 0x00B3,
		0x3C16, 0x0001,
		0x0300, 0x0006,
		0x0342, 0x17BB,
		0x0340, 0x0CDC,
		0x38C4, 0x0008,
		0x38D8, 0x0025,
		0x38DA, 0x0009,
		0x38DC, 0x000A,
		0x38C2, 0x0009,
		0x38C0, 0x000D,
		0x38D6, 0x0009,
		0x38D4, 0x0008,
		0x38B0, 0x000D,
		0x3932, 0x2000,
		0x3938, 0x000C,
		0x0820, 0x0432,
		0x380C, 0x0090,
		0x3064, 0xFFCF,
		0x309C, 0x0640,
		0x3090, 0x8800,
		0x3238, 0x000C,
		0x314A, 0x5F00,
		0x3300, 0x0000,
		0x3400, 0x0000,
		0x3402, 0x4E42,
		0x32B2, 0x0006,
		0x32B4, 0x0006,
		0x32B6, 0x0006,
		0x32B8, 0x0006,
		0x3C34, 0x0048,
		0x3C36, 0x3000,
		0x3C38, 0x0020,
		0x393E, 0x4000,
		0x303A, 0x0204,
		0x3034, 0x4B01,
		0x3036, 0x0029,
		0x3032, 0x4800,
		0x320E, 0x049E,
		0x3C67, 0x10,
};

static void custom1_setting(kal_uint16 currefps)
{
	pr_debug("custom1_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_custom1_3l6,
			sizeof(addr_data_pair_custom1_3l6) / sizeof(kal_uint16));
}/* custom1_setting */

static kal_uint16 addr_data_pair_custom2_3l6[] = {
	0x0344,0x0008,
	0x0346,0x0008,
	0x0348,0x1077,
	0x034A,0x0C37,
	0x034C,0x0838,
	0x034E,0x0618,
	0x0900,0x0122,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0001,
	0x0386,0x0003,
	0x0114,0x0330,
	0x0110,0x0002,
	0x0136,0x1800,
	0x0304,0x0004,
	0x0306,0x0078,
	0x3C1E,0x0000,
	0x030C,0x0003,
	0x030E,0x0047,
	0x3C16,0x0001,
	0x0300,0x0006,
	0x0342,0x1320,
	0x0340,0x0FEB,
	0x38C4,0x0004,
	0x38D8,0x0011,
	0x38DA,0x0005,
	0x38DC,0x0005,
	0x38C2,0x0005,
	0x38C0,0x0004,
	0x38D6,0x0004,
	0x38D4,0x0004,
	0x38B0,0x0007,
	0x3932,0x1000,
	0x3938,0x000C,
	0x0820,0x0238,
	0x380C,0x0049,
	0x3064,0xEBCF,
	0x309C,0x0600,
	0x3090,0x8000,
	0x3238,0x000B,
	0x314A,0x5F02,
	0x32B2,0x0003,
	0x32B4,0x0003,
	0x32B6,0x0003,
	0x32B8,0x0003,
	0x3300,0x0000,
	0x3400,0x0000,
	0x3402,0x4E46,
	0x32B2,0x0008,
	0x32B4,0x0008,
	0x32B6,0x0008,
	0x32B8,0x0008,
	0x3C34,0x0008,
	0x3C36,0x0000,
	0x3C38,0x0000,
	0x393E,0x4000,
	0x3C1E,0x0100,
	//0x0100,0x0100,//For Dual Camera streaming control
	0x3C1E,0x0000,
};

static void custom2_setting(void)
{
	pr_debug("custom2_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_custom2_3l6,
			sizeof(addr_data_pair_custom2_3l6) / sizeof(kal_uint16));
}/* custom2_setting */

//#ifdef S5K3L6_EEPROM_CALI
#if 0
static struct n8_s5k3l6_hlt_otp_data_str pOTP_Data = {0};

static kal_uint32 read_eeprom_info_data(
		struct n8_s5k3l6_hlt_otp_data_str *pData, kal_uint16 addr)
{
	kal_uint32 i = 0, ret = 0, checksum = 0;
	u8 *p = (u8 *)(&(pData->supplier_code));

	for (i = 0; i < 10; i++) {
		*p = read_eeprom(addr + i);
		//LOG_INF("reg[0x%x]: 0x%x\n", addr + i, *p);
		checksum += *p;
		p++;
	}
	pData->info_checksum_calc = (checksum % 255 +1) & 0xFF;
	pData->info_checksum_readout = read_eeprom(0x0B) & 0xFF;//0x0B: Info data checksum
	LOG_INF("info_checksum_calc = 0x%x, info_checksum_readout = 0x%x\n",
			pData->info_checksum_calc, pData->info_checksum_readout);

	return ret;
}

static kal_uint32 read_eeprom_awb_data(
		struct n8_s5k3l6_hlt_otp_data_str *pData, kal_uint16 addr)
{
	kal_uint32 i = 0, ret = 0, checksum = 0;
	u8 *p = (u8 *)(&(pData->R_over_Gr_unit_h));

	for (i = 0; i < 12; i++) {
		*p = read_eeprom(addr + i);
		//LOG_INF("reg[0x%x]: 0x%x\n", addr + i, *p);
		checksum += *p;
		p++;
	}
	pData->awb_checksum_calc = (checksum % 255 + 1) & 0xFF;
	pData->awb_checksum_readout = read_eeprom(0x1D) & 0xFF;//0x1D: AWB data checksum
	LOG_INF("awb_checksum_calc = 0x%x, awb_checksum_readout = 0x%x\n",
			pData->awb_checksum_calc, pData->awb_checksum_readout);

	return ret;
}

#define GAIN_DEFAULT 0x0100
static kal_uint32 n8_s5k3l6_hlt_otp_apply(struct n8_s5k3l6_hlt_otp_data_str *pData)
{
	/* Apply AWB */
	kal_uint32 R_gain, B_gain, Gb_gain, Gr_gain, Base_gain;
	kal_uint16 RGr_ratio, BGr_ratio, GbGr_ratio, RGr_ratio_Typical, BGr_ratio_Typical, GbGr_ratio_Typical;

	write_cmos_sensor(0x6028, 0x4000);
    write_cmos_sensor(0x3c90, 0x0000);
    mdelay(5);

	RGr_ratio = ((pData->R_over_Gr_unit_h << 8) | (pData->R_over_Gr_unit_l & 0xFF));
	BGr_ratio = ((pData->B_over_Gr_unit_h << 8) | (pData->B_over_Gr_unit_l & 0xFF));
	GbGr_ratio = ((pData->Gb_over_Gr_unit_h << 8) | (pData->Gb_over_Gr_unit_l & 0xFF));
	//LOG_INF("RGr_ratio = 0x%x, BGr_ratio = 0x%x, GbGr_ratio = 0x%x", RGr_ratio, BGr_ratio, GbGr_ratio);

	RGr_ratio_Typical = (pData->R_over_Gr_golden_h << 8) | (pData->R_over_Gr_golden_l & 0xFF);
	BGr_ratio_Typical = (pData->B_over_Gr_golden_h << 8) | (pData->B_over_Gr_golden_l & 0xFF);
	GbGr_ratio_Typical = (pData->Gb_over_Gr_golden_h << 8) | (pData->Gb_over_Gr_golden_l & 0xFF);
	//LOG_INF("RGr_ratio_Typical = 0x%x, BGr_ratio_Typical = 0x%x, GbGr_ratio_Typical = 0x%x", RGr_ratio_Typical, BGr_ratio_Typical, GbGr_ratio_Typical);

	R_gain = (RGr_ratio_Typical * 1000) / RGr_ratio;
	B_gain = (BGr_ratio_Typical * 1000) / BGr_ratio;
	Gb_gain = (GbGr_ratio_Typical * 1000) / GbGr_ratio;
	Gr_gain = 1000;
	Base_gain = R_gain;
	//LOG_INF("R_gain = 0x%x, B_gain = 0x%x, Gb_gain = 0x%x, Gr_gain = 0x%x, Base_gain = 0x%x", R_gain, B_gain, Gb_gain, Gr_gain, Base_gain);

	if (Base_gain > B_gain)
		Base_gain = B_gain;
	if (Base_gain > Gb_gain)
		Base_gain = Gb_gain;
	if (Base_gain > Gr_gain)
		Base_gain = Gr_gain;

	R_gain = 0x100 * R_gain / Base_gain;
	B_gain = 0x100 * B_gain / Base_gain;
	Gb_gain = 0x100 * Gb_gain / Base_gain;
	Gr_gain = 0x100 * Gr_gain / Base_gain;

	LOG_INF("R_gain = 0x%x, B_gain = 0x%x, Gb_gain = 0x%x, Gr_gain = 0x%x", R_gain, B_gain, Gb_gain, Gr_gain);
	//LOG_DBG("Before Apply: [0x020e] = 0x%x, [0x0210] = 0x%x, [0x0212] = 0x%x, [0x0214] = %x",
	//		read_cmos_sensor(0x020E), read_cmos_sensor(0x0210), read_cmos_sensor(0x0212), read_cmos_sensor(0x0214));
	if(Gr_gain > 0x100)
		write_cmos_sensor(0x020E, Gr_gain);
	if(R_gain > 0x100)
		write_cmos_sensor(0x0210, R_gain);
	if(B_gain > 0x100)
		write_cmos_sensor(0x0212, B_gain);
	if(Gb_gain > 0x100)
		write_cmos_sensor(0x0214, Gb_gain);
	//LOG_DBG("After Apply: [0x020e] = 0x%x, [0x0210] = 0x%x, [0x0212] = 0x%x, [0x0214] = %x",
	//		read_cmos_sensor(0x020E), read_cmos_sensor(0x0210), read_cmos_sensor(0x0212), read_cmos_sensor(0x0214));
	return 1;
}

static void n8_s5k3l6_hlt_get_eeprom_data(void)
{
	pOTP_Data.info_flag = read_eeprom(0x00);//0x00: info flag
	if (pOTP_Data.info_flag == 0x01)
		read_eeprom_info_data(&pOTP_Data, 0x01);//0x01: info start addr

	pOTP_Data.awb_flag = read_eeprom(0x10);//0x10: awb flag
	if (pOTP_Data.awb_flag == 0x01)
		read_eeprom_awb_data(&pOTP_Data, 0x11);//0x11: awb data start addr
}
#endif
/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
#if defined(CONFIG_TRAN_SYSTEM_DEVINFO)
extern void app_get_back_sensor_name(char *back_name);
#endif



#if S5K3L6_OTP 
int read_s5k3l6_otp(void){
		int addr = imgsensor.i2c_write_id;
		int i, sum = 0;
		imgsensor.i2c_write_id = 0xA2;
		for(i = 0; i < S5K3L6_OTP_SIZE; i++){
				s5k3l6_otp_data[i]	= read_cmos_sensor_8(i);
				//pr_err("=== s5k3l6_otp_data[%x]: %x ===\n", i, s5k3l6_otp_data[i]);
		}
		imgsensor.i2c_write_id = addr;
		//check module info
		for(i = MODULE_INFO_FLAG; i < MODULE_INFO_CHECKSUM; i++){
					sum += s5k3l6_otp_data[i];
		}
		if((s5k3l6_otp_data[MODULE_INFO_FLAG] = 0x55) && (sum % 0xff + 1) == s5k3l6_otp_data[MODULE_INFO_CHECKSUM]){
			LOG_DBG("Mouule   ID 0x%x ", s5k3l6_otp_data[1]);
			LOG_DBG("Position ID 0x%x ", s5k3l6_otp_data[2]);
			LOG_DBG("Lens     ID 0x%x ", s5k3l6_otp_data[3]);
			LOG_DBG("VCM      ID 0x%x ", s5k3l6_otp_data[4]);
			LOG_DBG("Year: %d Month: %d Data: %d", s5k3l6_otp_data[5], s5k3l6_otp_data[6], s5k3l6_otp_data[7]);
		}else{
				pr_err("=== s5k3l6_read module info failed ===\n");
				return 0;
		}
		sum = 0;
		//check module awb 51k info
		for(i = AWB_51K_INFO_FLAG; i < AWB_51K_INFO_CHECKSUM; i++){
					sum += s5k3l6_otp_data[i];
		}
		if((s5k3l6_otp_data[AWB_51K_INFO_FLAG] = 0x55) && (sum % 0xff + 1) == s5k3l6_otp_data[AWB_51K_INFO_CHECKSUM]){
			s5k3l6_awb_51k_data_valid = 1;
			pr_err("=== s5k3l6_read module info AWB 51k OK!\n");
		}else{
				pr_err("=== s5k3l6_read module info awb 51k failed ===\n");
				return 0;
		}
		sum = 0;
		//check module awb 30k info
		for(i = AWB_30K_INFO_FLAG; i < AWB_30K_INFO_CHECKSUM; i++){
					sum += s5k3l6_otp_data[i];
		}
		if((s5k3l6_otp_data[AWB_30K_INFO_FLAG] = 0x55) && (sum % 0xff + 1) == s5k3l6_otp_data[AWB_30K_INFO_CHECKSUM]){
			s5k3l6_awb_30k_data_valid = 1;
			pr_err("=== s5k3l6_read module info AWB 30k OK!\n");
		}else{
				pr_err("=== s5k3l6_read module info awb 30k failed ===\n");
				return 0;
		}
		sum = 0;
		//check module af info
		for(i = AF_INFO_FLAG; i < AF_INFO_CHECKSUM; i++){
					sum += s5k3l6_otp_data[i];
		}
		if((s5k3l6_otp_data[AF_INFO_FLAG] = 0x55) && (sum % 0xff + 1) == s5k3l6_otp_data[AF_INFO_CHECKSUM]){
			s5k3l6_af_data_valid = 1;
			pr_err("=== s5k3l6_read module info AF OK!\n");
		}else{
				pr_err("=== s5k3l6_read module info af failed ===\n");
				return 0;
		}
		sum = 0;
		//check module lsc 51k info
		for(i = LSC_51K_INFO_FLAG; i < LSC_51K_INFO_CHECKSUM; i++){
					sum += s5k3l6_otp_data[i];
		}
		if((s5k3l6_otp_data[LSC_51K_INFO_FLAG] = 0x55) && (sum % 0xff + 1) == s5k3l6_otp_data[LSC_51K_INFO_CHECKSUM]){
			s5k3l6_lsc_51k_data_valid = 1;
			pr_err("=== s5k3l6_read module info LSC 51k OK!\n");
		}else{
				pr_err("=== s5k3l6_read module info lsc 51k failed ===\n");
				return 0;
		}
		sum = 0;
		//check module lsc 40k info
		for(i = LSC_40K_INFO_FLAG; i < LSC_40K_INFO_CHECKSUM; i++){
					sum += s5k3l6_otp_data[i];
		}
		if((s5k3l6_otp_data[LSC_40K_INFO_FLAG] = 0x55) && (sum % 0xff + 1) == s5k3l6_otp_data[LSC_40K_INFO_CHECKSUM]){
			s5k3l6_lsc_40k_data_valid = 1;
			pr_err("=== s5k3l6_read module info LSC 40k OK!\n");
		}else{
				return 0;
		}
		sum = 0;
		//check module pdaf info
		for(i = PDAF_INFO_FLAG; i < PDAF_INFO_CHECKSUM; i++){
					sum += s5k3l6_otp_data[i];
		}
		if((s5k3l6_otp_data[PDAF_INFO_FLAG] = 0x55) && (sum % 0xff + 1) == s5k3l6_otp_data[PDAF_INFO_CHECKSUM]){
			s5k3l6_pdaf_data_valid = 1;
			pr_err("=== s5k3l6_read module info pdaf OK!\n");
		}else{
				pr_err("=== s5k3l6_read module info pdaf failed ===\n");
				//return 0;
		}
		return 1;
}
#endif

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	#if S5K3L6_OTP 
	int ret;
	#endif
		while (imgsensor_info.i2c_addr_table[i] != 0xff) {
			spin_lock(&imgsensor_drv_lock);
			imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
			spin_unlock(&imgsensor_drv_lock);
			do {
				*sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
				
				LOG_DBG("qhq gc2375 i2c write sensor id : 0x%x\n", *sensor_id);
				if (*sensor_id == imgsensor_info.sensor_id) {
				strcpy(rear_sensor_name, "s5k3l6_hlt");/*LGE_CHANGE, 2019-07-04, add the camera identifying logic , kyunghun.oh@lge.com*/
#if S5K3L6_OTP 
					ret = read_s5k3l6_otp();
					if(!ret){
					LOG_INF("get eeprom data failed\n");
					*sensor_id = 0xFFFFFFFF;
					return ERROR_SENSOR_CONNECT_FAIL;
					}
#endif
					LOG_DBG("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
					return ERROR_NONE;
				}
				LOG_DBG("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
				retry--;
			} while (retry > 0);
			i++;
			retry = 2;
		}
		if (*sensor_id != imgsensor_info.sensor_id) {
			/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
			*sensor_id = 0xFFFFFFFF;
			return ERROR_SENSOR_CONNECT_FAIL;
		}
		return ERROR_NONE;

//#ifdef S5K3L6_EEPROM_CALI
#if 0
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 vendorId = 0;
//	kal_uint16 sp8spFlag = 0;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
			if (*sensor_id == imgsensor_info.sensor_id) {
				vendorId = read_eeprom(0x0001);
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x vendorID=0x%x\n",
						imgsensor.i2c_write_id, *sensor_id,vendorId);
				if (vendorId == 0x55)//HLT Code : 0x55
				{
				if ((pOTP_Data.info_flag != 0x01) || (pOTP_Data.awb_flag != 0x01))
					n8_s5k3l6_hlt_get_eeprom_data();


					hw_info_main_otp.sensor_name = SENSOR_DRVNAME_S5K3L6MIPI_RAW;

					hw_info_main_otp.otp_valid = pOTP_Data.info_flag ? 1 : 0;
					hw_info_main_otp.vendor_id = pOTP_Data.supplier_code;
					hw_info_main_otp.module_code = pOTP_Data.module_code;
					hw_info_main_otp.module_ver = pOTP_Data.module_version;
					hw_info_main_otp.sw_ver = pOTP_Data.software_version;
					hw_info_main_otp.year = pOTP_Data.year;
					hw_info_main_otp.month = pOTP_Data.month;
					hw_info_main_otp.day = pOTP_Data.day;
					hw_info_main_otp.vcm_vendorid = 0;
					hw_info_main_otp.vcm_moduleid = 0;
					return ERROR_NONE;
				}
				else
				{
					*sensor_id = 0xFFFFFFFF;
				}
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",	imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
	
#endif
}


/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	LOG_DBG("%s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));

			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_DBG("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			LOG_DBG("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

//#ifdef S5K3L6_EEPROM_CALI
#if 0
	/* Retry read EEPROM data if get nothing in sensor search */
	if ((pOTP_Data.info_flag != 0x01) || (pOTP_Data.awb_flag != 0x01))
		n8_s5k3l6_hlt_get_eeprom_data();
	if ((pOTP_Data.awb_flag == 0x01) &&
			(pOTP_Data.awb_checksum_calc == pOTP_Data.awb_checksum_readout))
		n8_s5k3l6_hlt_otp_apply(&pOTP_Data);
#endif

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*      open  */



/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	LOG_DBG("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*      close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("preview E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      preview   */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("capture E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {

		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			LOG_DBG("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap.max_framerate / 10);
		}

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("normal_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);//[LGE_UPDATE] [kyunghun.oh@lge.com] [2019-01-28] make the fullsize recording works
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("hs_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("slim_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      slim_video       */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("custom1 E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);

	custom1_setting(imgsensor.current_fps);
set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}/* custom1 */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("custom2 E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom2_setting();
set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}/* custom1 */


static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	pr_debug("get_resolution E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("get_info -> scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;

	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain*/
	sensor_info->AESensorGainDelayFrame =
				imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
				imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;	//Add For Dual Camera Frame Length
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	/* change pdaf support mode to pdaf VC mode */
	sensor_info->PDAF_Support = 1;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*      get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* //pr_debug("framerate = %d\n ", framerate); */
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(
	kal_bool enable, UINT16 framerate)
{
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
	    (frame_length > imgsensor_info.normal_video.framelength)
	  ? (frame_length - imgsensor_info.normal_video.  framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

		frame_length = imgsensor_info.cap1.pclk
			/ framerate * 10 / imgsensor_info.cap1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frame_length > imgsensor_info.cap1.framelength)
		    ? (frame_length - imgsensor_info.cap1.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		frame_length = imgsensor_info.cap2.pclk
			/ framerate * 10 / imgsensor_info.cap2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frame_length > imgsensor_info.cap2.framelength)
		    ? (frame_length - imgsensor_info.cap2.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap2.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_DBG("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

		frame_length = imgsensor_info.cap.pclk
			/ framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.  framelength) : 0;

		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength):0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength):0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_DBG("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	if (enable) {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
         write_cmos_sensor(0x3202, 0x0080);
         write_cmos_sensor(0x3204, 0x0080);
         write_cmos_sensor(0x3206, 0x0080);
         write_cmos_sensor(0x3208, 0x0080);
         write_cmos_sensor(0x3232, 0x0000);
         write_cmos_sensor(0x3234, 0x0000);
         write_cmos_sensor(0x32a0, 0x0100);
         write_cmos_sensor(0x3300, 0x0001);
         write_cmos_sensor(0x3400, 0x0001);
         write_cmos_sensor(0x3402, 0x4e00);
         write_cmos_sensor(0x3268, 0x0000);
         write_cmos_sensor(0x0600, 0x0002);
	} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
         write_cmos_sensor(0x3202, 0x0000);
         write_cmos_sensor(0x3204, 0x0000);
         write_cmos_sensor(0x3206, 0x0000);
         write_cmos_sensor(0x3208, 0x0000);
         write_cmos_sensor(0x3232, 0x0000);
         write_cmos_sensor(0x3234, 0x0000);
         write_cmos_sensor(0x32a0, 0x0000);
         write_cmos_sensor(0x3300, 0x0000);
         write_cmos_sensor(0x3400, 0x0000);
         write_cmos_sensor(0x3402, 0x0000);
         write_cmos_sensor(0x3268, 0x0000);
         write_cmos_sensor(0x0600, 0x0000);
	}
	//write_cmos_sensor_byte(0x3268, 0x00);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature >= 0x0 && temperature <= 0x78)
		temperature_convert = temperature;
	else
		temperature_convert = -1;

	/*pr_info("temp_c(%d), read_reg(%d), enable %d\n",
	 *	temperature_convert, temperature, read_cmos_sensor_8(0x0138));
	 */

	return temperature_convert;
}

#define FOUR_CELL_SIZE 3072
static void read_4cell_from_eeprom(char *data)
{
	int i = 0;
	int addr = 0x763;/*Start of 4 cell data*/
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	/*size = 3072 = 0xc00*/
	data[0] = (FOUR_CELL_SIZE & 0xff);/*Low*/
	data[1] = ((FOUR_CELL_SIZE >> 8) & 0xff);/*High*/

	for (i = 2; i < (FOUR_CELL_SIZE + 2); i++) {
		pu_send_cmd[0] = (char)(addr >> 8);
		pu_send_cmd[1] = (char)(addr & 0xFF);
		iReadRegI2C(pu_send_cmd, 2, &data[i], 1, EEPROM_READ_ID);
		addr++;
	}
}



static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;

	/* SET_PD_BLOCK_INFO_T *PDAFinfo; */
	/* SENSOR_VC_INFO_STRUCT *pvcinfo; */
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_debug("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
#if 0
		pr_debug(
			"feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
#endif
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	/* night_mode((BOOL) *feature_data); no need to implement this mode */
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;

	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL) (*feature_data_16),
					*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
	    (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) (*feature_data));
		break;

	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
					(void *)&imgsensor_winsize_info[5],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
					(void *)&imgsensor_winsize_info[6],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),
 * (UINT16)*(feature_data+2));
 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
/* ihdr_write_shutter((UINT16)*feature_data,(UINT16)*(feature_data+1)); */
		break;

	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(uintptr_t)(*(feature_data+1));

		if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
			read_4cell_from_eeprom(data);
			pr_debug("read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);
		}
		break;
	}


		/******************** PDAF START >>> *********/
		
		case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n", (UINT16)*feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: //full
		    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		    case MSDK_SCENARIO_ID_CAMERA_PREVIEW: //2x2 binning
		       memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T)); //need to check
		    break;
		    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		    case MSDK_SCENARIO_ID_SLIM_VIDEO:
		    default:
		    break;
		}
		break;
		/*
		case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
		(UINT16)*feature_data);
		pvcinfo =
		(SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],
		sizeof(SENSOR_VC_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],
		sizeof(SENSOR_VC_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
		memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],
		sizeof(SENSOR_VC_INFO_STRUCT));
		break;
		}
		break;
		*/
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		    pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",(UINT16)*feature_data);
		//PDAF capacity enable or not
		switch (*feature_data) {
		   case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		      *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
		   break;
		   case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		      *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		   // video & capture use same setting
		   break;
		   case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		      *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		   break;
		   case MSDK_SCENARIO_ID_SLIM_VIDEO:
		   //need to check
		      *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		   break;
		   case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		      *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		   break;
		   default:
		      *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		   break;
		}
		break;
		case SENSOR_FEATURE_GET_PDAF_DATA: //get cal data from eeprom
		   pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		   //read_2T7_eeprom((kal_uint16 )(*feature_data), (char*)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
		   pr_debug("SENSOR_FEATURE_GET_PDAF_DATA success\n");
		break;
		case SENSOR_FEATURE_SET_PDAF:
		   pr_debug("PDAF mode :%d\n", *feature_data_16);
		   imgsensor.pdaf_mode= *feature_data_16;
		break;
		
		/******************** PDAF END   <<< *********/
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_DBG("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_DBG("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom1.pclk /
			(imgsensor_info.custom1.linelength - 80))*
			imgsensor_info.custom1.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom2.pclk /
			(imgsensor_info.custom2.linelength - 80))*
			imgsensor_info.custom2.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K3L6_MIPI_RAW_SensorInit(
	struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
