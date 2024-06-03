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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OV48B2Qmipi_Sensor.c
 *	   the setting is based on AM12, 2018/5/28 , Ken Cui 
 *	    - 0608: PLL change based on VIVO EMI team request
 *		- 0612: Add VC0 & VC 1 config parameter
 *		- 0619: driver update rotation orientation
 *		- 0621: set Quarter size have PDVC output
 * 	    - 0625: 1) add PD gain mappling; 2) change mipi data rate to 1464Mbps
 *		- 0626: change back sa clock to 90Mhz, and keep 1464Mbps.
 *		- 0629: 1)DPC on 2) update PDVC config by MTK Info  
 *      - 0705: 1)VC_PDAF works. 2) move stream on/off in the key setting. 3) DPC on level softness, add r5218~r521a
 *		- 0705-2:1)DPC swith to white only.
 *	    - 0706: DPC level set to 0x0c
 *      - 0709: enable PDAF in preview mode. 
 *		- 0716:	PD window change to 1152x400
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

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
//#include "../imgsensor_i2c.h"
#include "ov48b2qmipiraw_Sensor.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "OV48B2Q_camera_sensor"
#define LOG_1 LOG_INF("OV48B2Q,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 2096*1552@30fps;video 4192*3104@30fps; capture 13M@30fps\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)	pr_err(PFX "[%s] " format, __func__, ##args)
/*
static int vivo_otp_read_when_power_on;
extern int ov48b2q_otp_read(void);
extern otp_error_code_t OV48B2Q_OTP_ERROR_CODE;
extern unsigned char vivo_otp_data[0x0E28]; //[VIVO_OTP_DATA_SIZE]0x0E28;
*/
MUINT32  sn_inf_main_ov48b2q[13];  /*0 flag   1-12 data*/


static DEFINE_SPINLOCK(imgsensor_drv_lock);
static bool bIsLongExposure = KAL_FALSE;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV48B2Q_SENSOR_ID,	/* record sensor id defined in Kd_imgsensor.h */

	.checksum_value = 0xd6650427,	/*0xf86cfdf4, checksum value for Camera Auto Test */
	.pre = {
		.pclk = 115200000,	
		.linelength = 3312,	
		.framelength = 3076,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 720000000,
	},
	
	.cap = {
		.pclk = 115200000,
		.linelength = 3312,
		.framelength = 3076,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8000,
		.grabwindow_height = 6000,
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 720000000,
	},

	.normal_video = {
		.pclk = 115200000,	
		.linelength = 1104,	
		.framelength = 3478,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,	
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 720000000,
	},
	
	.hs_video = {
		.pclk = 115200000,	
		.linelength = 864,	
		.framelength = 832,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,	
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 160,
		.mipi_pixel_rate = 240000000,
	},
	.slim_video = {
		.pclk = 115200000,	
		.linelength = 1152,	
		.framelength = 3334,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,	
		.mipi_data_lp2hs_settle_dc = 120,
		.max_framerate = 300,
		.mipi_pixel_rate = 442000000,
	},

	.margin = 22,		/* sensor framelength & shutter margin */
	.min_shutter = 0x4,	/* min shutter */
	.max_frame_length = 0x7fff,	/* max framelength by sensor register's limitation */
	.ae_shut_delay_frame = 0,
	/* shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2 */
	.ae_sensor_gain_delay_frame = 0,
	/* sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2 */
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.frame_time_delay_frame = 2,	/* The delay frame of setting frame length  */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	  /*support sensor mode num*/

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,	/* enter high speed video  delay frame num */
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
//	.custom1_delay_frame = 2,
//	.custom2_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,	/* sensor_interface_type */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 1,	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,	/* sensor output first pixel color */
	.mclk = 24,		/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x6c, 0xff},
	/* record sensor support all write id addr, only supprt 4must end with 0xff */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,
	 * such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,
	/* auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker */
	.test_pattern = KAL_FALSE,
	/* test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = 0,		/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6C,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
 { 8000, 6000,    0,    0, 8000, 6000, 4000, 3000,   0, 0, 4000, 3000,   0, 0, 4000, 3000},		/* Preview */
 { 8000, 6000,    0,    0, 8000, 6000, 4000, 3000,   0, 0, 4000, 3000,   0, 0, 4000, 3000},		/* Preview */
 { 8000, 6000,    0,    0, 8000, 6000, 4000, 3000,   0, 0, 4000, 3000,   0, 0, 4000, 3000},		/* Preview */
 { 8000, 6000,    0,    0, 8000, 6000, 4000, 3000,   0, 0, 4000, 3000,   0, 0, 4000, 3000},		/* Preview */
 { 8000, 6000,    0,    0, 8000, 6000, 4000, 3000,   0, 0, 4000, 3000,   0, 0, 4000, 3000},		/* Preview */
//{ 4704, 3536, 0, 0, 4704, 3536, 4704, 3536,  48, 40, 4608, 3456, 0, 0, 4608, 3456 },//custom1
//{ 4704, 3536, 0, 0, 4704, 3536, 2352, 1768,  24, 20, 2304, 1728, 0, 0, 2304, 1728 },//custom2
};

/* add from OV48B2Q_v1_mirror_on_flip_off_mtk2.0.1.ini */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
    .i4OffsetX = 0,
    .i4OffsetY = 128,
    .i4PitchX = 32,
    .i4PitchY = 32,
    .i4PairNum = 16,
    .i4SubBlkW = 8,
    .i4SubBlkH = 8,
    .i4PosL = {{7,134},{15,134},{23,134},{31,134},{3,142},{11,142},{19,142},{27,142},{7,150},{15,150},{23,150},{31,150},{3,158},{11,158},{19,158},{27,158}},
    .i4PosR = {{6,134},{14,134},{22,134},{30,134},{2,142},{10,142},{18,142},{26,142},{6,150},{14,150},{22,150},{30,150},{2,158},{10,158},{18,158},{26,158}},    
    .i4BlockNumX = 144,
    .i4BlockNumY = 100,
  /* .i4LeFirst = 0,*/
   .i4Crop = {
        {0, 0}, {0, 0}, {0, 416}, {0, 0}, {0, 0},
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
    },
	.iMirrorFlip = 0,  /*otp and sensor identical = 0*/
};

/* add from OV48B2Q_v1_mirror_on_flip_off_mtk2.0.1.ini */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_16_9 = {
    .i4OffsetX = 0,
    .i4OffsetY = 0,
    .i4PitchX = 32,
    .i4PitchY = 32,
    .i4PairNum = 16,
    .i4SubBlkW = 8,
    .i4SubBlkH = 8,
    .i4PosL = {{7,6},{15,6},{23,6},{31,6},{3,14},{11,14},{19,14},{27,14},{7,22},{15,22},{23,22},{31,22},{3,30},{11,30},{19,30},{27,30}},
    .i4PosR = {{6,6},{14,6},{22,6},{30,6},{2,14},{10,14},{18,14},{26,14},{6,22},{14,22},{22,22},{30,22},{2,30},{10,30},{18,30},{26,30}},
    .i4BlockNumX = 144,
    .i4BlockNumY = 81,
  /* .i4LeFirst = 0,*/
   .i4Crop = {
        {0, 0}, {0, 0}, {0, 416}, {0, 0}, {0, 0},
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
    },
	.iMirrorFlip = 0,
};

/*VC1 None , VC2 for PDAF(DT=0X30), unit : 10bit*/
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x0B40, 0x0870, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x2b, 0x0168, 0x0640, 0x03, 0x00, 0x0000, 0x0000
	},
	/* Capture mode setting  288(Pixel)*1600 */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1680, 0x10E0, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x2b, 0x168, 0x0640, 0x03, 0x00, 0x0000, 0x0000
	},
	/* Video mode setting 288(pxiel)*1296 */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1680, 0x0CA8, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x2b, 0x0168, 0x0510, 0x03, 0x00, 0x0000, 0x0000
	},
};

static kal_uint16 read_cmos_sensor_16_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_16_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}
#if 0
#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif

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
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
		/* Write when remain buffer size is less than 3 bytes or reach end of data */
		if ((I2C_BUFFER_LEN - tosend) < 3 || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, 3,
					     imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}
#endif

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	/*return; //for test*/
    write_cmos_sensor_16_8(0x380c, imgsensor.line_length >> 8);
    write_cmos_sensor_16_8(0x380d, imgsensor.line_length & 0xFF);
    write_cmos_sensor_16_8(0x380e, imgsensor.frame_length >> 8);
    write_cmos_sensor_16_8(0x380f, imgsensor.frame_length & 0xFF);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;

	/* LOG_INF("framerate = %d, min_framelength_en=%d\n", framerate,min_framelength_en); */
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	/* LOG_INF("frame_length =%d\n", frame_length); */
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	/* LOG_INF("framerate = %d, min_framelength_en=%d\n", framerate,min_framelength_en); */
	set_dummy();
}



/*************************************************************************
* FUNCTION
*    set_shutter
*
* DESCRIPTION
*    This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*    iShutter : exposured lines
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void write_shutter(kal_uint32 shutter)
{
    //check
	kal_uint16 realtime_fps = 0;
    LOG_INF("write shutter :%d\n",shutter);

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;//increase current frame_length that makes shutter <= frame_length - margin.
    else
        imgsensor.frame_length = imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	if (imgsensor.autoflicker_en == KAL_TRUE)
	{
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305){
			realtime_fps = 296;
            set_max_framerate(realtime_fps,0);
		}
        else if(realtime_fps >= 147 && realtime_fps <= 150){
			realtime_fps = 146;
            set_max_framerate(realtime_fps ,0);
		}
        else
        {
        	imgsensor.frame_length = (imgsensor.frame_length  >> 1) << 1;
            write_cmos_sensor_16_8(0x380e, imgsensor.frame_length >> 8);
            write_cmos_sensor_16_8(0x380f, imgsensor.frame_length & 0xFF);
        }
    }
    else
    {
    	imgsensor.frame_length = (imgsensor.frame_length  >> 1) << 1;
        write_cmos_sensor_16_8(0x380e, imgsensor.frame_length >> 8);
        write_cmos_sensor_16_8(0x380f, imgsensor.frame_length & 0xFF);
    }

	if(shutter > (65535-8)) {
		/*enter long exposure mode */
		//kal_uint32 exposure_time;
		kal_uint32 long_shutter, normal_shutter;
		
		LOG_INF("enter long exposure mode\n");
		LOG_INF("Calc long exposure  +\n");
		//exposure_time = shutter*imgsensor_info.cap.linelength/90;//us
        //long_shutter = ((exposure_time - 33*1000);
        //LOG_INF("Calc long exposure exposure_time:%d long_shutter %d -\n",exposure_time,long_shutter);
		//long_shutter = (long_shutter >> 1) << 1;
		//LOG_INF("Calc long exposure  -\n");
		normal_shutter = (imgsensor_info.cap.framelength - imgsensor_info.margin );
		long_shutter = shutter - normal_shutter;
		
		write_cmos_sensor_16_8(0x3208, 0x03);
		write_cmos_sensor_16_8(0x3400, 0x04); //;set 0x04 after 100=1
		write_cmos_sensor_16_8(0x3410, 0x01); //;[0]long_exposure_mode_en
		write_cmos_sensor_16_8(0x3412, (long_shutter>>24)); //;long_exposure_time[31:24]
		write_cmos_sensor_16_8(0x3413, (long_shutter>>16&0xff)); //;long_exposure_time[23:16]
		write_cmos_sensor_16_8(0x3414, (long_shutter>>8)&0xff); ; //;long_exposure_time[15:8]
		write_cmos_sensor_16_8(0x3415, long_shutter&0xff); //;long_exposure_time[7:0]
		write_cmos_sensor_16_8(0x3501,(normal_shutter >> 8)); 
		write_cmos_sensor_16_8(0x3502,(normal_shutter & 0xff));		
		write_cmos_sensor_16_8(0x3508, 0x01);
		write_cmos_sensor_16_8(0x3509, 0x00);
		write_cmos_sensor_16_8(0x350a, 0x01);
		write_cmos_sensor_16_8(0x350b, 0x00);
		write_cmos_sensor_16_8(0x350c, 0x00);
		write_cmos_sensor_16_8(0x380e,(imgsensor_info.cap.framelength >> 8)); 
		write_cmos_sensor_16_8(0x380f,(imgsensor_info.cap.framelength & 0xff));			
		write_cmos_sensor_16_8(0x5221, 0x00); //;threshold is 200ms
		write_cmos_sensor_16_8(0x5222, 0x56);
		write_cmos_sensor_16_8(0x5223, 0xce);
		write_cmos_sensor_16_8(0x3208, 0x13);
		write_cmos_sensor_16_8(0x3208, 0xa3);

		bIsLongExposure = KAL_TRUE;
	} else {

	    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	
	    //frame_length and shutter should be an even number.
	    shutter = (shutter >> 1) << 1;
	    imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
		/*Warning : shutter must be even. Odd might happen Unexpected Results */
		
		write_cmos_sensor_16_8(0x3501,(shutter >> 8)); 
		write_cmos_sensor_16_8(0x3502,(shutter & 0xff));
	}

    LOG_INF("shutter =%d, framelength =%d, realtime_fps =%d\n", shutter,imgsensor.frame_length, realtime_fps);
}


static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	 spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	if (frame_length > 1)
		imgsensor.frame_length = frame_length;
/* */
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
	            write_cmos_sensor_16_8(0x380e, imgsensor.frame_length >> 8);
	            write_cmos_sensor_16_8(0x380f, imgsensor.frame_length & 0xFF);
			}
		} else {
			/* Extend frame length*/
	            write_cmos_sensor_16_8(0x380e, imgsensor.frame_length >> 8);
	            write_cmos_sensor_16_8(0x380f, imgsensor.frame_length & 0xFF);
		}
		/* Update Shutter*/
		write_cmos_sensor_16_8(0x3501,(shutter >> 8)); 
		write_cmos_sensor_16_8(0x3502,(shutter & 0xff));

		LOG_INF("Add for N3D! shutterlzl =%d, framelength =%d\n",
			shutter, imgsensor.frame_length);

}

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
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	//platform 1xgain = 64, sensor driver 1*gain = 0x100
	iReg = gain*256/BASEGAIN;

	if(iReg < 0x100)// sensor 1xGain
	{
		iReg = 0X100;
	}
	if(iReg > 0xf80)// sensor 15.5xGain
	{
		iReg = 0Xf80;
	}

	return iReg;		/* ov48b2q. sensorGlobalGain */
}

/*************************************************************************
* FUNCTION
*    set_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    iGain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor_16_8(0x03508,(reg_gain >> 8)); 
    write_cmos_sensor_16_8(0x03509,(reg_gain&0xff));

	return gain;
}				/*    set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{

/*	kal_uint8 itemp;*/

	LOG_INF("image_mirror = %d\n", image_mirror);

}

/*************************************************************************
* FUNCTION
*    night_mode
*
* DESCRIPTION
*    This function night mode of sensor.
*
* PARAMETERS
*    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*    None
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

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
	{
//		write_cmos_sensor(0x480e, 0x06);
		write_cmos_sensor_16_8(0x0100, 0x01);
	}
	else
	{
//		write_cmos_sensor(0x480e, 0x02);
		write_cmos_sensor_16_8(0x0100, 0x00);
	}
	mdelay(100);
	return ERROR_NONE;
}


static void sensor_init(void)
{
	LOG_INF("E\n");
write_cmos_sensor_16_8(0x0103,0x01);
write_cmos_sensor_16_8(0x0102,0x01);
write_cmos_sensor_16_8(0x0301,0xc8);
write_cmos_sensor_16_8(0x0302,0x31);
write_cmos_sensor_16_8(0x0303,0x03);
write_cmos_sensor_16_8(0x0304,0x01);
write_cmos_sensor_16_8(0x0305,0x75);
write_cmos_sensor_16_8(0x0306,0x04);
write_cmos_sensor_16_8(0x0309,0x52);
write_cmos_sensor_16_8(0x0320,0x12);
write_cmos_sensor_16_8(0x0323,0x03);
write_cmos_sensor_16_8(0x0324,0x01);
write_cmos_sensor_16_8(0x0325,0xe0);
write_cmos_sensor_16_8(0x0326,0xd9);
write_cmos_sensor_16_8(0x0327,0x0a);
write_cmos_sensor_16_8(0x0329,0x01);
write_cmos_sensor_16_8(0x032a,0x07);
write_cmos_sensor_16_8(0x032b,0x02);
write_cmos_sensor_16_8(0x032f,0x0c);
write_cmos_sensor_16_8(0x0343,0x03);
write_cmos_sensor_16_8(0x0344,0x01);
write_cmos_sensor_16_8(0x0345,0x22);
write_cmos_sensor_16_8(0x0346,0xc0);
write_cmos_sensor_16_8(0x0349,0x01);
write_cmos_sensor_16_8(0x0350,0x01);
write_cmos_sensor_16_8(0x034b,0x00);
write_cmos_sensor_16_8(0x034a,0x05);
write_cmos_sensor_16_8(0x0361,0x07);
write_cmos_sensor_16_8(0x3010,0x01);
write_cmos_sensor_16_8(0x3012,0x41);
write_cmos_sensor_16_8(0x3016,0xf0);
write_cmos_sensor_16_8(0x3018,0xf0);
write_cmos_sensor_16_8(0x3019,0xd2);
write_cmos_sensor_16_8(0x301b,0x16);
write_cmos_sensor_16_8(0x3022,0xd0);
write_cmos_sensor_16_8(0x3025,0x03);
write_cmos_sensor_16_8(0x3026,0x00);
write_cmos_sensor_16_8(0x3027,0x00);
write_cmos_sensor_16_8(0x3028,0xc3);
write_cmos_sensor_16_8(0x3107,0x0a);
write_cmos_sensor_16_8(0x3200,0x00);
write_cmos_sensor_16_8(0x3201,0x16);
write_cmos_sensor_16_8(0x3202,0x2c);
write_cmos_sensor_16_8(0x3203,0x42);
write_cmos_sensor_16_8(0x3204,0x58);
write_cmos_sensor_16_8(0x3205,0x5a);
write_cmos_sensor_16_8(0x3223,0x5b);
write_cmos_sensor_16_8(0x3224,0x5c);
write_cmos_sensor_16_8(0x3225,0x5d);
write_cmos_sensor_16_8(0x3226,0x5e);
write_cmos_sensor_16_8(0x3216,0x00);
write_cmos_sensor_16_8(0x3218,0x00);
write_cmos_sensor_16_8(0x3400,0x04);
write_cmos_sensor_16_8(0x3406,0x20);
write_cmos_sensor_16_8(0x340c,0xff);
write_cmos_sensor_16_8(0x340e,0xff);
write_cmos_sensor_16_8(0x3421,0x08);
write_cmos_sensor_16_8(0x3423,0x15);
write_cmos_sensor_16_8(0x3424,0x40);
write_cmos_sensor_16_8(0x3425,0x10);
write_cmos_sensor_16_8(0x3426,0x11);
write_cmos_sensor_16_8(0x3501,0x0b);
write_cmos_sensor_16_8(0x3502,0x00);
write_cmos_sensor_16_8(0x3504,0x08);
write_cmos_sensor_16_8(0x3508,0x03);
write_cmos_sensor_16_8(0x3509,0x00);
write_cmos_sensor_16_8(0x350a,0x01);
write_cmos_sensor_16_8(0x350b,0x00);
write_cmos_sensor_16_8(0x350c,0x00);
write_cmos_sensor_16_8(0x3602,0x00);
write_cmos_sensor_16_8(0x3603,0x00);
write_cmos_sensor_16_8(0x3607,0x01);
write_cmos_sensor_16_8(0x3608,0x35);
write_cmos_sensor_16_8(0x3609,0x80);
write_cmos_sensor_16_8(0x360a,0x00);
write_cmos_sensor_16_8(0x360b,0x7f);
write_cmos_sensor_16_8(0x3618,0x2a);
write_cmos_sensor_16_8(0x3619,0x90);
write_cmos_sensor_16_8(0x361b,0x2a);
write_cmos_sensor_16_8(0x361c,0x90);
write_cmos_sensor_16_8(0x361d,0x98);
write_cmos_sensor_16_8(0x361e,0x07);
write_cmos_sensor_16_8(0x3621,0x08);
write_cmos_sensor_16_8(0x3622,0x11);
write_cmos_sensor_16_8(0x3623,0x08);
write_cmos_sensor_16_8(0x3624,0x18);
write_cmos_sensor_16_8(0x3626,0xf9);
write_cmos_sensor_16_8(0x3627,0x11);
write_cmos_sensor_16_8(0x3628,0x88);
write_cmos_sensor_16_8(0x3629,0x0d);
write_cmos_sensor_16_8(0x362c,0x77);
write_cmos_sensor_16_8(0x362e,0x00);
write_cmos_sensor_16_8(0x3631,0xf2);
write_cmos_sensor_16_8(0x3632,0xf2);
write_cmos_sensor_16_8(0x3633,0x08);
write_cmos_sensor_16_8(0x3634,0x08);
write_cmos_sensor_16_8(0x3635,0x28);
write_cmos_sensor_16_8(0x3636,0x28);
write_cmos_sensor_16_8(0x3637,0xcc);
write_cmos_sensor_16_8(0x3638,0xcc);
write_cmos_sensor_16_8(0x3639,0xc0);
write_cmos_sensor_16_8(0x363a,0xcc);
write_cmos_sensor_16_8(0x363b,0x0b);
write_cmos_sensor_16_8(0x363c,0x13);
write_cmos_sensor_16_8(0x363e,0x47);
write_cmos_sensor_16_8(0x363d,0x25);
write_cmos_sensor_16_8(0x363f,0x08);
write_cmos_sensor_16_8(0x3643,0x00);
write_cmos_sensor_16_8(0x3644,0x40);
write_cmos_sensor_16_8(0x3680,0x00);
write_cmos_sensor_16_8(0x3683,0x00);
write_cmos_sensor_16_8(0x3684,0x07);
write_cmos_sensor_16_8(0x3685,0x12);
write_cmos_sensor_16_8(0x3688,0x00);
write_cmos_sensor_16_8(0x3689,0x27);
write_cmos_sensor_16_8(0x368a,0x3a);
write_cmos_sensor_16_8(0x368b,0x04);
write_cmos_sensor_16_8(0x368e,0x0f);
write_cmos_sensor_16_8(0x3700,0x2e);
write_cmos_sensor_16_8(0x3701,0x08);
write_cmos_sensor_16_8(0x3702,0x46);
write_cmos_sensor_16_8(0x3703,0x35);
write_cmos_sensor_16_8(0x3704,0x06);
write_cmos_sensor_16_8(0x3705,0x00);
write_cmos_sensor_16_8(0x3706,0x2a);
write_cmos_sensor_16_8(0x3707,0x08);
write_cmos_sensor_16_8(0x3708,0x3a);
write_cmos_sensor_16_8(0x3709,0x7a);
write_cmos_sensor_16_8(0x370a,0x00);
write_cmos_sensor_16_8(0x370b,0x62);
write_cmos_sensor_16_8(0x370c,0x0b);
write_cmos_sensor_16_8(0x3711,0x00);
write_cmos_sensor_16_8(0x3712,0x50);
write_cmos_sensor_16_8(0x3713,0x00);
write_cmos_sensor_16_8(0x3714,0x63);
write_cmos_sensor_16_8(0x3716,0x44);
write_cmos_sensor_16_8(0x3717,0x02);
write_cmos_sensor_16_8(0x3718,0x14);
write_cmos_sensor_16_8(0x3719,0x11);
write_cmos_sensor_16_8(0x371a,0x1c);
write_cmos_sensor_16_8(0x371b,0xd0);
write_cmos_sensor_16_8(0x371c,0x04);
write_cmos_sensor_16_8(0x371d,0x22);
write_cmos_sensor_16_8(0x371e,0x13);
write_cmos_sensor_16_8(0x371f,0x0b);
write_cmos_sensor_16_8(0x3720,0x08);
write_cmos_sensor_16_8(0x3721,0x15);
write_cmos_sensor_16_8(0x3722,0x05);
write_cmos_sensor_16_8(0x3723,0x00);
write_cmos_sensor_16_8(0x3724,0x08);
write_cmos_sensor_16_8(0x3725,0x32);
write_cmos_sensor_16_8(0x3727,0x22);
write_cmos_sensor_16_8(0x3728,0x11);
write_cmos_sensor_16_8(0x372a,0x00);
write_cmos_sensor_16_8(0x372b,0x8a);
write_cmos_sensor_16_8(0x3730,0x00);
write_cmos_sensor_16_8(0x3731,0x00);
write_cmos_sensor_16_8(0x3732,0x06);
write_cmos_sensor_16_8(0x3733,0x06);
write_cmos_sensor_16_8(0x3734,0x06);
write_cmos_sensor_16_8(0x3735,0x06);
write_cmos_sensor_16_8(0x3736,0x0a);
write_cmos_sensor_16_8(0x3737,0x02);
write_cmos_sensor_16_8(0x3738,0x0a);
write_cmos_sensor_16_8(0x3739,0x02);
write_cmos_sensor_16_8(0x373a,0x0a);
write_cmos_sensor_16_8(0x373b,0x20);
write_cmos_sensor_16_8(0x373c,0x0a);
write_cmos_sensor_16_8(0x373d,0x26);
write_cmos_sensor_16_8(0x373e,0x0a);
write_cmos_sensor_16_8(0x373f,0x28);
write_cmos_sensor_16_8(0x3740,0x06);
write_cmos_sensor_16_8(0x3741,0x04);
write_cmos_sensor_16_8(0x3742,0x06);
write_cmos_sensor_16_8(0x3743,0x0a);
write_cmos_sensor_16_8(0x3744,0x16);
write_cmos_sensor_16_8(0x3745,0x0a);
write_cmos_sensor_16_8(0x3746,0x16);
write_cmos_sensor_16_8(0x3747,0x50);
write_cmos_sensor_16_8(0x3748,0x00);
write_cmos_sensor_16_8(0x3749,0xf0);
write_cmos_sensor_16_8(0x374a,0x02);
write_cmos_sensor_16_8(0x374b,0x02);
write_cmos_sensor_16_8(0x374c,0x08);
write_cmos_sensor_16_8(0x374d,0x03);
write_cmos_sensor_16_8(0x374e,0x05);
write_cmos_sensor_16_8(0x374f,0x08);
write_cmos_sensor_16_8(0x3751,0x07);
write_cmos_sensor_16_8(0x3752,0x02);
write_cmos_sensor_16_8(0x3753,0x01);
write_cmos_sensor_16_8(0x3754,0x66);
write_cmos_sensor_16_8(0x3758,0x03);
write_cmos_sensor_16_8(0x375b,0x00);
write_cmos_sensor_16_8(0x375c,0x00);
write_cmos_sensor_16_8(0x375d,0x02);
write_cmos_sensor_16_8(0x375e,0x02);
write_cmos_sensor_16_8(0x3760,0x08);
write_cmos_sensor_16_8(0x3761,0x10);
write_cmos_sensor_16_8(0x3762,0x08);
write_cmos_sensor_16_8(0x3763,0x08);
write_cmos_sensor_16_8(0x3764,0x08);
write_cmos_sensor_16_8(0x3765,0x10);
write_cmos_sensor_16_8(0x3766,0x18);
write_cmos_sensor_16_8(0x3767,0x28);
write_cmos_sensor_16_8(0x3768,0x00);
write_cmos_sensor_16_8(0x3769,0x08);
write_cmos_sensor_16_8(0x376a,0x10);
write_cmos_sensor_16_8(0x376b,0x00);
write_cmos_sensor_16_8(0x376d,0x1b);
write_cmos_sensor_16_8(0x3770,0x30);
write_cmos_sensor_16_8(0x3773,0x42);
write_cmos_sensor_16_8(0x3774,0x10);
write_cmos_sensor_16_8(0x3775,0x00);
write_cmos_sensor_16_8(0x3776,0x12);
write_cmos_sensor_16_8(0x3777,0x14);
write_cmos_sensor_16_8(0x3778,0x0e);
write_cmos_sensor_16_8(0x3779,0x22);
write_cmos_sensor_16_8(0x377a,0x24);
write_cmos_sensor_16_8(0x377b,0x46);
write_cmos_sensor_16_8(0x377c,0x14);
write_cmos_sensor_16_8(0x377d,0x04);
write_cmos_sensor_16_8(0x377e,0x16);
write_cmos_sensor_16_8(0x377f,0x04);
write_cmos_sensor_16_8(0x379b,0x10);
write_cmos_sensor_16_8(0x379c,0x10);
write_cmos_sensor_16_8(0x379d,0x10);
write_cmos_sensor_16_8(0x379e,0x30);
write_cmos_sensor_16_8(0x379f,0x30);
write_cmos_sensor_16_8(0x37a0,0x0c);
write_cmos_sensor_16_8(0x37a1,0x0c);
write_cmos_sensor_16_8(0x37a2,0x04);
write_cmos_sensor_16_8(0x37a3,0x0c);
write_cmos_sensor_16_8(0x37a4,0x04);
write_cmos_sensor_16_8(0x37a5,0x00);
write_cmos_sensor_16_8(0x37a6,0x00);
write_cmos_sensor_16_8(0x37a7,0x18);
write_cmos_sensor_16_8(0x37a8,0x0a);
write_cmos_sensor_16_8(0x37a9,0x08);
write_cmos_sensor_16_8(0x37aa,0x08);
write_cmos_sensor_16_8(0x37ab,0x0d);
write_cmos_sensor_16_8(0x37ac,0x14);
write_cmos_sensor_16_8(0x37ad,0x14);
write_cmos_sensor_16_8(0x37ae,0x14);
write_cmos_sensor_16_8(0x37b0,0x40);
write_cmos_sensor_16_8(0x37b1,0x40);
write_cmos_sensor_16_8(0x37b2,0x30);
write_cmos_sensor_16_8(0x37b3,0x46);
write_cmos_sensor_16_8(0x37b4,0x46);
write_cmos_sensor_16_8(0x37b5,0x36);
write_cmos_sensor_16_8(0x37b6,0x00);
write_cmos_sensor_16_8(0x37b7,0x00);
write_cmos_sensor_16_8(0x37b8,0x00);
write_cmos_sensor_16_8(0x37b9,0x00);
write_cmos_sensor_16_8(0x37ba,0x00);
write_cmos_sensor_16_8(0x37bc,0x00);
write_cmos_sensor_16_8(0x37bd,0x00);
write_cmos_sensor_16_8(0x37be,0x00);
write_cmos_sensor_16_8(0x37bf,0x00);
write_cmos_sensor_16_8(0x37c0,0x03);
write_cmos_sensor_16_8(0x37c1,0x02);
write_cmos_sensor_16_8(0x37c2,0x06);
write_cmos_sensor_16_8(0x37c3,0x04);
write_cmos_sensor_16_8(0x37c4,0x01);
write_cmos_sensor_16_8(0x37c5,0x06);
write_cmos_sensor_16_8(0x37c8,0x04);
write_cmos_sensor_16_8(0x37c9,0x04);
write_cmos_sensor_16_8(0x37ca,0xf9);
write_cmos_sensor_16_8(0x37cb,0x10);
write_cmos_sensor_16_8(0x37cc,0xf0);
write_cmos_sensor_16_8(0x37cd,0x02);
write_cmos_sensor_16_8(0x37ce,0x0a);
write_cmos_sensor_16_8(0x37cf,0x02);
write_cmos_sensor_16_8(0x37d0,0x0a);
write_cmos_sensor_16_8(0x37d1,0x02);
write_cmos_sensor_16_8(0x37d2,0x0a);
write_cmos_sensor_16_8(0x37d3,0x02);
write_cmos_sensor_16_8(0x37d4,0x00);
write_cmos_sensor_16_8(0x37d5,0x10);
write_cmos_sensor_16_8(0x37d6,0x01);
write_cmos_sensor_16_8(0x37d7,0x44);
write_cmos_sensor_16_8(0x37d8,0x06);
write_cmos_sensor_16_8(0x37d9,0x00);
write_cmos_sensor_16_8(0x37da,0x10);
write_cmos_sensor_16_8(0x37dc,0x06);
write_cmos_sensor_16_8(0x37de,0x02);
write_cmos_sensor_16_8(0x37df,0x80);
write_cmos_sensor_16_8(0x37e0,0x03);
write_cmos_sensor_16_8(0x37e1,0x04);
write_cmos_sensor_16_8(0x37e2,0x06);
write_cmos_sensor_16_8(0x37e3,0x18);
write_cmos_sensor_16_8(0x37e4,0x0a);
write_cmos_sensor_16_8(0x37e5,0x16);
write_cmos_sensor_16_8(0x37e6,0x0a);
write_cmos_sensor_16_8(0x37e7,0x16);
write_cmos_sensor_16_8(0x37e8,0x0a);
write_cmos_sensor_16_8(0x37e9,0x1c);
write_cmos_sensor_16_8(0x37eb,0x06);
write_cmos_sensor_16_8(0x37ec,0x06);
write_cmos_sensor_16_8(0x37ed,0x0a);
write_cmos_sensor_16_8(0x37ee,0x02);
write_cmos_sensor_16_8(0x37f0,0x0a);
write_cmos_sensor_16_8(0x37f1,0x02);
write_cmos_sensor_16_8(0x37f2,0x0a);
write_cmos_sensor_16_8(0x37f3,0x02);
write_cmos_sensor_16_8(0x37f6,0x03);
write_cmos_sensor_16_8(0x37f7,0x02);
write_cmos_sensor_16_8(0x37f8,0x3c);
write_cmos_sensor_16_8(0x37f9,0x02);
write_cmos_sensor_16_8(0x37fa,0x02);
write_cmos_sensor_16_8(0x37fb,0x02);
write_cmos_sensor_16_8(0x37fc,0x00);
write_cmos_sensor_16_8(0x37fd,0x00);
write_cmos_sensor_16_8(0x37fe,0x00);
write_cmos_sensor_16_8(0x37ff,0x00);
write_cmos_sensor_16_8(0x3800,0x00);
write_cmos_sensor_16_8(0x3801,0x00);
write_cmos_sensor_16_8(0x3802,0x00);
write_cmos_sensor_16_8(0x3803,0x00);
write_cmos_sensor_16_8(0x3804,0x1f);
write_cmos_sensor_16_8(0x3805,0x5f);
write_cmos_sensor_16_8(0x3806,0x17);
write_cmos_sensor_16_8(0x3807,0x8f);
write_cmos_sensor_16_8(0x3808,0x0f);
write_cmos_sensor_16_8(0x3809,0xa0);
write_cmos_sensor_16_8(0x380a,0x0b);
write_cmos_sensor_16_8(0x380b,0xb8);
write_cmos_sensor_16_8(0x380c,0x04);
write_cmos_sensor_16_8(0x380d,0x50);
write_cmos_sensor_16_8(0x380e,0x0d);
write_cmos_sensor_16_8(0x380f,0x96);
write_cmos_sensor_16_8(0x3810,0x00);
write_cmos_sensor_16_8(0x3811,0x09);
write_cmos_sensor_16_8(0x3812,0x00);
write_cmos_sensor_16_8(0x3813,0x08);
write_cmos_sensor_16_8(0x3814,0x11);
write_cmos_sensor_16_8(0x3815,0x11);
write_cmos_sensor_16_8(0x3816,0x01);
write_cmos_sensor_16_8(0x3817,0x10);
write_cmos_sensor_16_8(0x3818,0x01);
write_cmos_sensor_16_8(0x3819,0x00);
write_cmos_sensor_16_8(0x381a,0x1b);
write_cmos_sensor_16_8(0x381b,0x1c);
write_cmos_sensor_16_8(0x381c,0x01);
write_cmos_sensor_16_8(0x381d,0x14);
write_cmos_sensor_16_8(0x381f,0x02);
write_cmos_sensor_16_8(0x3820,0x02);
write_cmos_sensor_16_8(0x3821,0x06);
write_cmos_sensor_16_8(0x3822,0x10);
write_cmos_sensor_16_8(0x3823,0x04);
write_cmos_sensor_16_8(0x3824,0x01);
write_cmos_sensor_16_8(0x3825,0x14);
write_cmos_sensor_16_8(0x3826,0x1b);
write_cmos_sensor_16_8(0x3827,0x1c);
write_cmos_sensor_16_8(0x3837,0x18);
write_cmos_sensor_16_8(0x383a,0x00);
write_cmos_sensor_16_8(0x383b,0x00);
write_cmos_sensor_16_8(0x383c,0x00);
write_cmos_sensor_16_8(0x383d,0x09);
write_cmos_sensor_16_8(0x383e,0x0c);
write_cmos_sensor_16_8(0x383f,0x33);
write_cmos_sensor_16_8(0x3840,0x00);
write_cmos_sensor_16_8(0x3841,0x00);
write_cmos_sensor_16_8(0x3848,0x12);
write_cmos_sensor_16_8(0x3849,0x40);
write_cmos_sensor_16_8(0x384a,0x0d);
write_cmos_sensor_16_8(0x384b,0xb0);
write_cmos_sensor_16_8(0x3854,0x11);
write_cmos_sensor_16_8(0x3855,0x11);
write_cmos_sensor_16_8(0x3856,0x20);
write_cmos_sensor_16_8(0x3857,0x04);
write_cmos_sensor_16_8(0x3858,0x40);
write_cmos_sensor_16_8(0x3859,0x08);
write_cmos_sensor_16_8(0x3860,0x20);
write_cmos_sensor_16_8(0x3a01,0x7f);
write_cmos_sensor_16_8(0x3d85,0x8b);
write_cmos_sensor_16_8(0x3d8c,0x77);
write_cmos_sensor_16_8(0x3d8d,0xa0);
write_cmos_sensor_16_8(0x3dab,0x10);
write_cmos_sensor_16_8(0x3dac,0x05);
write_cmos_sensor_16_8(0x3dad,0x8b);
write_cmos_sensor_16_8(0x3dae,0x05);
write_cmos_sensor_16_8(0x3daf,0x8f);
write_cmos_sensor_16_8(0x3f01,0x12);
write_cmos_sensor_16_8(0x4002,0x13);
write_cmos_sensor_16_8(0x4009,0x02);
write_cmos_sensor_16_8(0x400e,0xc6);
write_cmos_sensor_16_8(0x4010,0xe8);
write_cmos_sensor_16_8(0x4011,0x01);
write_cmos_sensor_16_8(0x4012,0x7d);
write_cmos_sensor_16_8(0x4015,0x00);
write_cmos_sensor_16_8(0x4016,0x0f);
write_cmos_sensor_16_8(0x4017,0x00);
write_cmos_sensor_16_8(0x4018,0x03);
write_cmos_sensor_16_8(0x401a,0x40);
write_cmos_sensor_16_8(0x401e,0x01);
write_cmos_sensor_16_8(0x401f,0xaa);
write_cmos_sensor_16_8(0x4028,0x01);
write_cmos_sensor_16_8(0x4056,0x25);
write_cmos_sensor_16_8(0x4500,0x04);
write_cmos_sensor_16_8(0x4502,0x10);
write_cmos_sensor_16_8(0x4510,0x00);
write_cmos_sensor_16_8(0x4512,0x00);
write_cmos_sensor_16_8(0x4513,0x00);
write_cmos_sensor_16_8(0x4514,0x00);
write_cmos_sensor_16_8(0x4515,0x00);
write_cmos_sensor_16_8(0x4516,0x55);
write_cmos_sensor_16_8(0x4517,0x55);
write_cmos_sensor_16_8(0x4518,0x55);
write_cmos_sensor_16_8(0x4519,0x55);
write_cmos_sensor_16_8(0x451a,0x11);
write_cmos_sensor_16_8(0x451b,0xbb);
write_cmos_sensor_16_8(0x451c,0x11);
write_cmos_sensor_16_8(0x451d,0xbb);
write_cmos_sensor_16_8(0x451e,0x11);
write_cmos_sensor_16_8(0x451f,0xbb);
write_cmos_sensor_16_8(0x4520,0x11);
write_cmos_sensor_16_8(0x4521,0xbb);
write_cmos_sensor_16_8(0x4522,0x00);
write_cmos_sensor_16_8(0x4523,0x00);
write_cmos_sensor_16_8(0x4524,0x00);
write_cmos_sensor_16_8(0x4525,0x00);
write_cmos_sensor_16_8(0x4526,0x00);
write_cmos_sensor_16_8(0x4600,0x00);
write_cmos_sensor_16_8(0x4601,0xc8);
write_cmos_sensor_16_8(0x4603,0x17);
write_cmos_sensor_16_8(0x4641,0x3e);
write_cmos_sensor_16_8(0x4643,0x0c);
write_cmos_sensor_16_8(0x4800,0x64);
write_cmos_sensor_16_8(0x480e,0x04);
write_cmos_sensor_16_8(0x4810,0x00);
write_cmos_sensor_16_8(0x4811,0x02);
write_cmos_sensor_16_8(0x4815,0x2b);
write_cmos_sensor_16_8(0x4837,0x08);
write_cmos_sensor_16_8(0x4853,0x04);
write_cmos_sensor_16_8(0x484b,0x27);
write_cmos_sensor_16_8(0x4850,0x43);
write_cmos_sensor_16_8(0x4858,0x05);
write_cmos_sensor_16_8(0x4860,0x00);
write_cmos_sensor_16_8(0x4861,0xec);
write_cmos_sensor_16_8(0x4862,0x04);
write_cmos_sensor_16_8(0x4883,0x00);
write_cmos_sensor_16_8(0x4888,0x90);
write_cmos_sensor_16_8(0x4889,0x03);
write_cmos_sensor_16_8(0x4a00,0x10);
write_cmos_sensor_16_8(0x4d00,0x04);
write_cmos_sensor_16_8(0x4d01,0x49);
write_cmos_sensor_16_8(0x4d02,0xbf);
write_cmos_sensor_16_8(0x4d03,0xfb);
write_cmos_sensor_16_8(0x4d04,0x2a);
write_cmos_sensor_16_8(0x4d05,0x9b);
write_cmos_sensor_16_8(0x5000,0xcf);
write_cmos_sensor_16_8(0x5001,0x23);
write_cmos_sensor_16_8(0x5002,0x9e);
write_cmos_sensor_16_8(0x5003,0x12);
write_cmos_sensor_16_8(0x5004,0x01);
write_cmos_sensor_16_8(0x5006,0x00);
write_cmos_sensor_16_8(0x5015,0x02);
write_cmos_sensor_16_8(0x501d,0x00);
write_cmos_sensor_16_8(0x5051,0x14);
write_cmos_sensor_16_8(0x5080,0x00);
write_cmos_sensor_16_8(0x5081,0x00);
write_cmos_sensor_16_8(0x5180,0x00);
write_cmos_sensor_16_8(0x5181,0x10);
write_cmos_sensor_16_8(0x5182,0x05);
write_cmos_sensor_16_8(0x5183,0x8f);
write_cmos_sensor_16_8(0x5184,0x01);
write_cmos_sensor_16_8(0x5185,0x0b);
write_cmos_sensor_16_8(0x5187,0x00);
write_cmos_sensor_16_8(0x5198,0x08);
write_cmos_sensor_16_8(0x518c,0x02);
write_cmos_sensor_16_8(0x518d,0x06);
write_cmos_sensor_16_8(0x518e,0x02);
write_cmos_sensor_16_8(0x518f,0x06);
write_cmos_sensor_16_8(0x5200,0x6f);
write_cmos_sensor_16_8(0x52a5,0x08);
write_cmos_sensor_16_8(0x522a,0x44);
write_cmos_sensor_16_8(0x522b,0x44);
write_cmos_sensor_16_8(0x5300,0x79);
write_cmos_sensor_16_8(0x5308,0x44);
write_cmos_sensor_16_8(0x5310,0x00);
write_cmos_sensor_16_8(0x5311,0x01);
write_cmos_sensor_16_8(0x5312,0x01);
write_cmos_sensor_16_8(0x5313,0x02);
write_cmos_sensor_16_8(0x5314,0x04);
write_cmos_sensor_16_8(0x5315,0x06);
write_cmos_sensor_16_8(0x5316,0x08);
write_cmos_sensor_16_8(0x5317,0x0a);
write_cmos_sensor_16_8(0x5318,0x0c);
write_cmos_sensor_16_8(0x5319,0x0e);
write_cmos_sensor_16_8(0x531a,0x10);
write_cmos_sensor_16_8(0x531b,0x12);
write_cmos_sensor_16_8(0x531c,0x14);
write_cmos_sensor_16_8(0x531d,0x16);
write_cmos_sensor_16_8(0x531e,0x18);
write_cmos_sensor_16_8(0x533d,0x00);
write_cmos_sensor_16_8(0x5380,0x0f);
write_cmos_sensor_16_8(0x53c0,0x88);
write_cmos_sensor_16_8(0x53c1,0x8b);
write_cmos_sensor_16_8(0x53c2,0x8a);
write_cmos_sensor_16_8(0x53c3,0x88);
write_cmos_sensor_16_8(0x53c4,0x8b);
write_cmos_sensor_16_8(0x53c5,0x8b);
write_cmos_sensor_16_8(0x53c6,0x75);
write_cmos_sensor_16_8(0x53c7,0x74);
write_cmos_sensor_16_8(0x53c8,0x77);
write_cmos_sensor_16_8(0x53c9,0x8a);
write_cmos_sensor_16_8(0x53ca,0x8a);
write_cmos_sensor_16_8(0x53cb,0x8b);
write_cmos_sensor_16_8(0x53cc,0x76);
write_cmos_sensor_16_8(0x53cd,0x71);
write_cmos_sensor_16_8(0x53ce,0x71);
write_cmos_sensor_16_8(0x53cf,0x8a);
write_cmos_sensor_16_8(0x53d0,0x88);
write_cmos_sensor_16_8(0x53d1,0x8e);
write_cmos_sensor_16_8(0x53d2,0x71);
write_cmos_sensor_16_8(0x53d3,0x71);
write_cmos_sensor_16_8(0x53d4,0x70);
write_cmos_sensor_16_8(0x53d5,0x8b);
write_cmos_sensor_16_8(0x53d6,0x8e);
write_cmos_sensor_16_8(0x53d7,0x8f);
write_cmos_sensor_16_8(0x53d8,0x70);
write_cmos_sensor_16_8(0x53d9,0x72);
write_cmos_sensor_16_8(0x53da,0x7d);
write_cmos_sensor_16_8(0x53db,0x77);
write_cmos_sensor_16_8(0x53dc,0x8a);
write_cmos_sensor_16_8(0x53dd,0x88);
write_cmos_sensor_16_8(0x53de,0x72);
write_cmos_sensor_16_8(0x53df,0x7f);
write_cmos_sensor_16_8(0x53e0,0x7e);
write_cmos_sensor_16_8(0x53e1,0x72);
write_cmos_sensor_16_8(0x53e2,0x76);
write_cmos_sensor_16_8(0x53e3,0x74);
write_cmos_sensor_16_8(0x53e4,0x74);
write_cmos_sensor_16_8(0x53e5,0x74);
write_cmos_sensor_16_8(0x53e6,0x77);
write_cmos_sensor_16_8(0x53e7,0x77);
write_cmos_sensor_16_8(0x53e8,0x75);
write_cmos_sensor_16_8(0x53e9,0x8a);
write_cmos_sensor_16_8(0x53ea,0x75);
write_cmos_sensor_16_8(0x53eb,0x74);
write_cmos_sensor_16_8(0x53ec,0x77);
write_cmos_sensor_16_8(0x53ed,0x71);
write_cmos_sensor_16_8(0x53ee,0x77);
write_cmos_sensor_16_8(0x53ef,0x77);
write_cmos_sensor_16_8(0x53f0,0x88);
write_cmos_sensor_16_8(0x53f1,0x8b);
write_cmos_sensor_16_8(0x53f2,0x74);
write_cmos_sensor_16_8(0x53f3,0x70);
write_cmos_sensor_16_8(0x53f4,0x71);
write_cmos_sensor_16_8(0x53f5,0x71);
write_cmos_sensor_16_8(0x53f6,0x89);
write_cmos_sensor_16_8(0x53f7,0x89);
write_cmos_sensor_16_8(0x53f8,0x8b);
write_cmos_sensor_16_8(0x53f9,0x71);
write_cmos_sensor_16_8(0x53fa,0x76);
write_cmos_sensor_16_8(0x53fb,0x71);
write_cmos_sensor_16_8(0x53fc,0x8b);
write_cmos_sensor_16_8(0x53fd,0x8a);
write_cmos_sensor_16_8(0x53fe,0x76);
write_cmos_sensor_16_8(0x53ff,0x7d);
write_cmos_sensor_16_8(0x5400,0x73);
write_cmos_sensor_16_8(0x5401,0x73);
write_cmos_sensor_16_8(0x5402,0x74);
write_cmos_sensor_16_8(0x5403,0x76);
write_cmos_sensor_16_8(0x5404,0x7d);
write_cmos_sensor_16_8(0x5405,0x79);
write_cmos_sensor_16_8(0x5406,0x7f);
write_cmos_sensor_16_8(0x5407,0x7d);
write_cmos_sensor_16_8(0x5408,0x70);
write_cmos_sensor_16_8(0x5409,0x73);
write_cmos_sensor_16_8(0x540a,0x72);
write_cmos_sensor_16_8(0x540b,0x76);
write_cmos_sensor_16_8(0x540c,0x8a);
write_cmos_sensor_16_8(0x540d,0x88);
write_cmos_sensor_16_8(0x540e,0x77);
write_cmos_sensor_16_8(0x540f,0x71);
write_cmos_sensor_16_8(0x5410,0x76);
write_cmos_sensor_16_8(0x5411,0x8b);
write_cmos_sensor_16_8(0x5412,0x8e);
write_cmos_sensor_16_8(0x5413,0x8f);
write_cmos_sensor_16_8(0x5414,0x77);
write_cmos_sensor_16_8(0x5415,0x76);
write_cmos_sensor_16_8(0x5416,0x76);
write_cmos_sensor_16_8(0x5417,0x88);
write_cmos_sensor_16_8(0x5418,0x8f);
write_cmos_sensor_16_8(0x5419,0x8d);
write_cmos_sensor_16_8(0x541a,0x76);
write_cmos_sensor_16_8(0x541b,0x71);
write_cmos_sensor_16_8(0x541c,0x71);
write_cmos_sensor_16_8(0x541d,0x8a);
write_cmos_sensor_16_8(0x541e,0x88);
write_cmos_sensor_16_8(0x541f,0x8e);
write_cmos_sensor_16_8(0x5420,0x75);
write_cmos_sensor_16_8(0x5421,0x77);
write_cmos_sensor_16_8(0x5422,0x77);
write_cmos_sensor_16_8(0x5423,0x8a);
write_cmos_sensor_16_8(0x5424,0x75);
write_cmos_sensor_16_8(0x5425,0x8b);
write_cmos_sensor_16_8(0x5426,0x89);
write_cmos_sensor_16_8(0x5427,0x88);
write_cmos_sensor_16_8(0x5428,0x88);
write_cmos_sensor_16_8(0x5429,0x88);
write_cmos_sensor_16_8(0x542a,0x8a);
write_cmos_sensor_16_8(0x542b,0x8b);
write_cmos_sensor_16_8(0x542c,0x8a);
write_cmos_sensor_16_8(0x542d,0x77);
write_cmos_sensor_16_8(0x542e,0x72);
write_cmos_sensor_16_8(0x542f,0x7f);
write_cmos_sensor_16_8(0x5430,0x7c);
write_cmos_sensor_16_8(0x5431,0x72);
write_cmos_sensor_16_8(0x5432,0x88);
write_cmos_sensor_16_8(0x5433,0x8b);
write_cmos_sensor_16_8(0x5434,0x74);
write_cmos_sensor_16_8(0x5435,0x70);
write_cmos_sensor_16_8(0x5436,0x71);
write_cmos_sensor_16_8(0x5437,0x71);
write_cmos_sensor_16_8(0x5438,0x8e);
write_cmos_sensor_16_8(0x5439,0x89);
write_cmos_sensor_16_8(0x543a,0x8b);
write_cmos_sensor_16_8(0x543b,0x76);
write_cmos_sensor_16_8(0x543c,0x74);
write_cmos_sensor_16_8(0x543d,0x76);
write_cmos_sensor_16_8(0x543e,0x88);
write_cmos_sensor_16_8(0x543f,0x8b);
write_cmos_sensor_16_8(0x5440,0x75);
write_cmos_sensor_16_8(0x5441,0x76);
write_cmos_sensor_16_8(0x5442,0x77);
write_cmos_sensor_16_8(0x5443,0x77);
write_cmos_sensor_16_8(0x5444,0x8a);
write_cmos_sensor_16_8(0x5445,0x74);
write_cmos_sensor_16_8(0x5446,0x75);
write_cmos_sensor_16_8(0x5447,0x77);
write_cmos_sensor_16_8(0x5448,0x75);
write_cmos_sensor_16_8(0x5449,0x75);
write_cmos_sensor_16_8(0x544a,0x8b);
write_cmos_sensor_16_8(0x544b,0x8b);
write_cmos_sensor_16_8(0x544c,0x88);
write_cmos_sensor_16_8(0x544d,0x88);
write_cmos_sensor_16_8(0x544e,0x89);
write_cmos_sensor_16_8(0x544f,0x89);
write_cmos_sensor_16_8(0x5450,0x8f);
write_cmos_sensor_16_8(0x5451,0x89);
write_cmos_sensor_16_8(0x5452,0x8b);
write_cmos_sensor_16_8(0x5453,0x8b);
write_cmos_sensor_16_8(0x5454,0x77);
write_cmos_sensor_16_8(0x5455,0x72);
write_cmos_sensor_16_8(0x5456,0x89);
write_cmos_sensor_16_8(0x5457,0x8a);
write_cmos_sensor_16_8(0x5458,0x74);
write_cmos_sensor_16_8(0x5459,0x8a);
write_cmos_sensor_16_8(0x545a,0x76);
write_cmos_sensor_16_8(0x545b,0x73);
write_cmos_sensor_16_8(0x545c,0x89);
write_cmos_sensor_16_8(0x545d,0x8a);
write_cmos_sensor_16_8(0x545e,0x74);
write_cmos_sensor_16_8(0x545f,0x75);
write_cmos_sensor_16_8(0x5460,0x71);
write_cmos_sensor_16_8(0x5461,0x73);
write_cmos_sensor_16_8(0x5462,0x8e);
write_cmos_sensor_16_8(0x5463,0x88);
write_cmos_sensor_16_8(0x5464,0x88);
write_cmos_sensor_16_8(0x5465,0x8e);
write_cmos_sensor_16_8(0x5466,0x74);
write_cmos_sensor_16_8(0x5467,0x71);
write_cmos_sensor_16_8(0x5468,0x8e);
write_cmos_sensor_16_8(0x5469,0x89);
write_cmos_sensor_16_8(0x546a,0x8e);
write_cmos_sensor_16_8(0x546b,0x8c);
write_cmos_sensor_16_8(0x546c,0x88);
write_cmos_sensor_16_8(0x546d,0x74);
write_cmos_sensor_16_8(0x546e,0x8e);
write_cmos_sensor_16_8(0x546f,0x89);
write_cmos_sensor_16_8(0x5470,0x8e);
write_cmos_sensor_16_8(0x5471,0x8c);
write_cmos_sensor_16_8(0x5472,0x88);
write_cmos_sensor_16_8(0x5473,0x74);
write_cmos_sensor_16_8(0x5474,0x7f);
write_cmos_sensor_16_8(0x5475,0x73);
write_cmos_sensor_16_8(0x5476,0x77);
write_cmos_sensor_16_8(0x5477,0x76);
write_cmos_sensor_16_8(0x5478,0x75);
write_cmos_sensor_16_8(0x5479,0x8b);
write_cmos_sensor_16_8(0x547a,0x7c);
write_cmos_sensor_16_8(0x547b,0x70);
write_cmos_sensor_16_8(0x547c,0x77);
write_cmos_sensor_16_8(0x547d,0x71);
write_cmos_sensor_16_8(0x547e,0x77);
write_cmos_sensor_16_8(0x547f,0x75);
write_cmos_sensor_16_8(0x5480,0x7d);
write_cmos_sensor_16_8(0x5481,0x70);
write_cmos_sensor_16_8(0x5482,0x74);
write_cmos_sensor_16_8(0x5483,0x71);
write_cmos_sensor_16_8(0x5484,0x77);
write_cmos_sensor_16_8(0x5485,0x75);
write_cmos_sensor_16_8(0x5486,0x71);
write_cmos_sensor_16_8(0x5487,0x75);
write_cmos_sensor_16_8(0x5488,0x8e);
write_cmos_sensor_16_8(0x5489,0x8a);
write_cmos_sensor_16_8(0x548a,0x75);
write_cmos_sensor_16_8(0x548b,0x8a);
write_cmos_sensor_16_8(0x548c,0x77);
write_cmos_sensor_16_8(0x548d,0x89);
write_cmos_sensor_16_8(0x548e,0x8d);
write_cmos_sensor_16_8(0x548f,0x89);
write_cmos_sensor_16_8(0x5490,0x8b);
write_cmos_sensor_16_8(0x5491,0x88);
write_cmos_sensor_16_8(0x5492,0x77);
write_cmos_sensor_16_8(0x5493,0x89);
write_cmos_sensor_16_8(0x5494,0x8f);
write_cmos_sensor_16_8(0x5495,0x89);
write_cmos_sensor_16_8(0x5496,0x88);
write_cmos_sensor_16_8(0x5497,0x8e);
write_cmos_sensor_16_8(0x5498,0x8e);
write_cmos_sensor_16_8(0x5499,0x89);
write_cmos_sensor_16_8(0x549a,0x8c);
write_cmos_sensor_16_8(0x549b,0x83);
write_cmos_sensor_16_8(0x549c,0x8e);
write_cmos_sensor_16_8(0x549d,0x8b);
write_cmos_sensor_16_8(0x549e,0x8e);
write_cmos_sensor_16_8(0x549f,0x88);
write_cmos_sensor_16_8(0x54a0,0x8e);
write_cmos_sensor_16_8(0x54a1,0x8d);
write_cmos_sensor_16_8(0x54a2,0x89);
write_cmos_sensor_16_8(0x54a3,0x8a);
write_cmos_sensor_16_8(0x54a4,0x8e);
write_cmos_sensor_16_8(0x54a5,0x88);
write_cmos_sensor_16_8(0x54a6,0x88);
write_cmos_sensor_16_8(0x54a7,0x8f);
write_cmos_sensor_16_8(0x54a8,0x8a);
write_cmos_sensor_16_8(0x54a9,0x76);
write_cmos_sensor_16_8(0x54aa,0x8e);
write_cmos_sensor_16_8(0x54ab,0x8b);
write_cmos_sensor_16_8(0x54ac,0x74);
write_cmos_sensor_16_8(0x54ad,0x8a);
write_cmos_sensor_16_8(0x54ae,0x77);
write_cmos_sensor_16_8(0x54af,0x71);
write_cmos_sensor_16_8(0x54b0,0x8e);
write_cmos_sensor_16_8(0x54b1,0x8b);
write_cmos_sensor_16_8(0x54b2,0x75);
write_cmos_sensor_16_8(0x54b3,0x8a);
write_cmos_sensor_16_8(0x54b4,0x77);
write_cmos_sensor_16_8(0x54b5,0x70);
write_cmos_sensor_16_8(0x54b6,0x8f);
write_cmos_sensor_16_8(0x54b7,0x88);
write_cmos_sensor_16_8(0x54b8,0x8a);
write_cmos_sensor_16_8(0x54b9,0x8a);
write_cmos_sensor_16_8(0x54ba,0x76);
write_cmos_sensor_16_8(0x54bb,0x73);
write_cmos_sensor_16_8(0x54bc,0x8a);
write_cmos_sensor_16_8(0x54bd,0x8e);
write_cmos_sensor_16_8(0x54be,0x8d);
write_cmos_sensor_16_8(0x54bf,0x88);
write_cmos_sensor_16_8(0x54c0,0x8b);
write_cmos_sensor_16_8(0x54c1,0x88);
write_cmos_sensor_16_8(0x54c2,0x74);
write_cmos_sensor_16_8(0x54c3,0x8e);
write_cmos_sensor_16_8(0x54c4,0x8c);
write_cmos_sensor_16_8(0x54c5,0x8b);
write_cmos_sensor_16_8(0x54c6,0x8a);
write_cmos_sensor_16_8(0x54c7,0x88);
write_cmos_sensor_16_8(0x54c8,0x76);
write_cmos_sensor_16_8(0x54c9,0x8a);
write_cmos_sensor_16_8(0x54ca,0x8f);
write_cmos_sensor_16_8(0x54cb,0x8b);
write_cmos_sensor_16_8(0x54cc,0x8a);
write_cmos_sensor_16_8(0x54cd,0x8b);
write_cmos_sensor_16_8(0x54ce,0x73);
write_cmos_sensor_16_8(0x54cf,0x76);
write_cmos_sensor_16_8(0x54d0,0x8a);
write_cmos_sensor_16_8(0x54d1,0x77);
write_cmos_sensor_16_8(0x54d2,0x8a);
write_cmos_sensor_16_8(0x54d3,0x88);
write_cmos_sensor_16_8(0x54d4,0x73);
write_cmos_sensor_16_8(0x54d5,0x77);
write_cmos_sensor_16_8(0x54d6,0x8a);
write_cmos_sensor_16_8(0x54d7,0x74);
write_cmos_sensor_16_8(0x54d8,0x8b);
write_cmos_sensor_16_8(0x54d9,0x89);
write_cmos_sensor_16_8(0x54da,0x73);
write_cmos_sensor_16_8(0x54db,0x76);
write_cmos_sensor_16_8(0x54dc,0x8a);
write_cmos_sensor_16_8(0x54dd,0x8a);
write_cmos_sensor_16_8(0x54de,0x88);
write_cmos_sensor_16_8(0x54df,0x8f);
write_cmos_sensor_16_8(0x5500,0x57);
write_cmos_sensor_16_8(0x5501,0x6f);
write_cmos_sensor_16_8(0x5502,0x6d);
write_cmos_sensor_16_8(0x5503,0x66);
write_cmos_sensor_16_8(0x5504,0x71);
write_cmos_sensor_16_8(0x5505,0x8e);
write_cmos_sensor_16_8(0x5506,0x6b);
write_cmos_sensor_16_8(0x5507,0x62);
write_cmos_sensor_16_8(0x5508,0x64);
write_cmos_sensor_16_8(0x5509,0x79);
write_cmos_sensor_16_8(0x550a,0x8b);
write_cmos_sensor_16_8(0x550b,0x80);
write_cmos_sensor_16_8(0x550c,0x62);
write_cmos_sensor_16_8(0x550d,0x65);
write_cmos_sensor_16_8(0x550e,0x7f);
write_cmos_sensor_16_8(0x550f,0x71);
write_cmos_sensor_16_8(0x5510,0x83);
write_cmos_sensor_16_8(0x5511,0x9b);
write_cmos_sensor_16_8(0x5512,0x65);
write_cmos_sensor_16_8(0x5513,0x72);
write_cmos_sensor_16_8(0x5514,0x74);
write_cmos_sensor_16_8(0x5515,0x8f);
write_cmos_sensor_16_8(0x5516,0x9a);
write_cmos_sensor_16_8(0x5517,0x9c);
write_cmos_sensor_16_8(0x5518,0x7b);
write_cmos_sensor_16_8(0x5519,0x70);
write_cmos_sensor_16_8(0x551a,0x88);
write_cmos_sensor_16_8(0x551b,0x8c);
write_cmos_sensor_16_8(0x551c,0x9a);
write_cmos_sensor_16_8(0x551d,0x9f);
write_cmos_sensor_16_8(0x551e,0x7b);
write_cmos_sensor_16_8(0x551f,0x73);
write_cmos_sensor_16_8(0x5520,0x8e);
write_cmos_sensor_16_8(0x5521,0x82);
write_cmos_sensor_16_8(0x5522,0x98);
write_cmos_sensor_16_8(0x5523,0x9c);
write_cmos_sensor_16_8(0x5524,0xa8);
write_cmos_sensor_16_8(0x5525,0x93);
write_cmos_sensor_16_8(0x5526,0x99);
write_cmos_sensor_16_8(0x5527,0x9a);
write_cmos_sensor_16_8(0x5528,0x8c);
write_cmos_sensor_16_8(0x5529,0x74);
write_cmos_sensor_16_8(0x552a,0xab);
write_cmos_sensor_16_8(0x552b,0x92);
write_cmos_sensor_16_8(0x552c,0x86);
write_cmos_sensor_16_8(0x552d,0x80);
write_cmos_sensor_16_8(0x552e,0x88);
write_cmos_sensor_16_8(0x552f,0x71);
write_cmos_sensor_16_8(0x5530,0x96);
write_cmos_sensor_16_8(0x5531,0x9e);
write_cmos_sensor_16_8(0x5532,0x83);
write_cmos_sensor_16_8(0x5533,0x8d);
write_cmos_sensor_16_8(0x5534,0x75);
write_cmos_sensor_16_8(0x5535,0x73);
write_cmos_sensor_16_8(0x5536,0x9c);
write_cmos_sensor_16_8(0x5537,0x87);
write_cmos_sensor_16_8(0x5538,0x89);
write_cmos_sensor_16_8(0x5539,0x8a);
write_cmos_sensor_16_8(0x553a,0x70);
write_cmos_sensor_16_8(0x553b,0x7f);
write_cmos_sensor_16_8(0x553c,0x9b);
write_cmos_sensor_16_8(0x553d,0x8d);
write_cmos_sensor_16_8(0x553e,0x76);
write_cmos_sensor_16_8(0x553f,0x73);
write_cmos_sensor_16_8(0x5540,0x7f);
write_cmos_sensor_16_8(0x5541,0x79);
write_cmos_sensor_16_8(0x5542,0x81);
write_cmos_sensor_16_8(0x5543,0x8b);
write_cmos_sensor_16_8(0x5544,0x79);
write_cmos_sensor_16_8(0x5545,0x78);
write_cmos_sensor_16_8(0x5546,0x64);
write_cmos_sensor_16_8(0x5547,0x66);
write_cmos_sensor_16_8(0x5548,0x66);
write_cmos_sensor_16_8(0x5549,0x78);
write_cmos_sensor_16_8(0x554a,0x73);
write_cmos_sensor_16_8(0x554b,0x72);
write_cmos_sensor_16_8(0x554c,0x72);
write_cmos_sensor_16_8(0x554d,0x76);
write_cmos_sensor_16_8(0x554e,0x63);
write_cmos_sensor_16_8(0x554f,0x7a);
write_cmos_sensor_16_8(0x5550,0x71);
write_cmos_sensor_16_8(0x5551,0x71);
write_cmos_sensor_16_8(0x5552,0x72);
write_cmos_sensor_16_8(0x5553,0x73);
write_cmos_sensor_16_8(0x5554,0x63);
write_cmos_sensor_16_8(0x5555,0x65);
write_cmos_sensor_16_8(0x5556,0x72);
write_cmos_sensor_16_8(0x5557,0x73);
write_cmos_sensor_16_8(0x5558,0x7f);
write_cmos_sensor_16_8(0x5559,0x7d);
write_cmos_sensor_16_8(0x555a,0x61);
write_cmos_sensor_16_8(0x555b,0x7b);
write_cmos_sensor_16_8(0x555c,0x73);
write_cmos_sensor_16_8(0x555d,0x70);
write_cmos_sensor_16_8(0x555e,0x7c);
write_cmos_sensor_16_8(0x555f,0x72);
write_cmos_sensor_16_8(0x5560,0x7b);
write_cmos_sensor_16_8(0x5561,0x72);
write_cmos_sensor_16_8(0x5562,0x8b);
write_cmos_sensor_16_8(0x5563,0x8b);
write_cmos_sensor_16_8(0x5564,0x77);
write_cmos_sensor_16_8(0x5565,0x77);
write_cmos_sensor_16_8(0x5566,0x73);
write_cmos_sensor_16_8(0x5567,0x75);
write_cmos_sensor_16_8(0x5568,0x80);
write_cmos_sensor_16_8(0x5569,0x83);
write_cmos_sensor_16_8(0x556a,0x8f);
write_cmos_sensor_16_8(0x556b,0x89);
write_cmos_sensor_16_8(0x556c,0x88);
write_cmos_sensor_16_8(0x556d,0x70);
write_cmos_sensor_16_8(0x556e,0x64);
write_cmos_sensor_16_8(0x556f,0x6d);
write_cmos_sensor_16_8(0x5570,0x6b);
write_cmos_sensor_16_8(0x5571,0x51);
write_cmos_sensor_16_8(0x5572,0x83);
write_cmos_sensor_16_8(0x5573,0x8b);
write_cmos_sensor_16_8(0x5574,0x7e);
write_cmos_sensor_16_8(0x5575,0x64);
write_cmos_sensor_16_8(0x5576,0x6e);
write_cmos_sensor_16_8(0x5577,0x55);
write_cmos_sensor_16_8(0x5578,0x85);
write_cmos_sensor_16_8(0x5579,0x83);
write_cmos_sensor_16_8(0x557a,0x76);
write_cmos_sensor_16_8(0x557b,0x7e);
write_cmos_sensor_16_8(0x557c,0x66);
write_cmos_sensor_16_8(0x557d,0x6c);
write_cmos_sensor_16_8(0x557e,0x9b);
write_cmos_sensor_16_8(0x557f,0x84);
write_cmos_sensor_16_8(0x5580,0x8c);
write_cmos_sensor_16_8(0x5581,0x76);
write_cmos_sensor_16_8(0x5582,0x7e);
write_cmos_sensor_16_8(0x5583,0x64);
write_cmos_sensor_16_8(0x5584,0x98);
write_cmos_sensor_16_8(0x5585,0x85);
write_cmos_sensor_16_8(0x5586,0x8c);
write_cmos_sensor_16_8(0x5587,0x75);
write_cmos_sensor_16_8(0x5588,0x7f);
write_cmos_sensor_16_8(0x5589,0x7a);
write_cmos_sensor_16_8(0x558a,0x99);
write_cmos_sensor_16_8(0x558b,0x85);
write_cmos_sensor_16_8(0x558c,0x8d);
write_cmos_sensor_16_8(0x558d,0x8a);
write_cmos_sensor_16_8(0x558e,0x72);
write_cmos_sensor_16_8(0x558f,0x7f);
write_cmos_sensor_16_8(0x5590,0x88);
write_cmos_sensor_16_8(0x5591,0x82);
write_cmos_sensor_16_8(0x5592,0x85);
write_cmos_sensor_16_8(0x5593,0x9f);
write_cmos_sensor_16_8(0x5594,0x94);
write_cmos_sensor_16_8(0x5595,0xac);
write_cmos_sensor_16_8(0x5596,0x76);
write_cmos_sensor_16_8(0x5597,0x88);
write_cmos_sensor_16_8(0x5598,0x83);
write_cmos_sensor_16_8(0x5599,0x84);
write_cmos_sensor_16_8(0x559a,0x96);
write_cmos_sensor_16_8(0x559b,0xa8);
write_cmos_sensor_16_8(0x559c,0x70);
write_cmos_sensor_16_8(0x559d,0x75);
write_cmos_sensor_16_8(0x559e,0x8f);
write_cmos_sensor_16_8(0x559f,0x81);
write_cmos_sensor_16_8(0x55a0,0x9d);
write_cmos_sensor_16_8(0x55a1,0x97);
write_cmos_sensor_16_8(0x55a2,0x70);
write_cmos_sensor_16_8(0x55a3,0x77);
write_cmos_sensor_16_8(0x55a4,0x75);
write_cmos_sensor_16_8(0x55a5,0x8f);
write_cmos_sensor_16_8(0x55a6,0x98);
write_cmos_sensor_16_8(0x55a7,0x9c);
write_cmos_sensor_16_8(0x55a8,0x7f);
write_cmos_sensor_16_8(0x55a9,0x7d);
write_cmos_sensor_16_8(0x55aa,0x73);
write_cmos_sensor_16_8(0x55ab,0x77);
write_cmos_sensor_16_8(0x55ac,0x86);
write_cmos_sensor_16_8(0x55ad,0x9b);
write_cmos_sensor_16_8(0x55ae,0x7b);
write_cmos_sensor_16_8(0x55af,0x7b);
write_cmos_sensor_16_8(0x55b0,0x78);
write_cmos_sensor_16_8(0x55b1,0x7d);
write_cmos_sensor_16_8(0x55b2,0x89);
write_cmos_sensor_16_8(0x55b3,0x83);
write_cmos_sensor_16_8(0x55b4,0x7d);
write_cmos_sensor_16_8(0x55b5,0x7c);
write_cmos_sensor_16_8(0x55b6,0x7d);
write_cmos_sensor_16_8(0x55b7,0x7c);
write_cmos_sensor_16_8(0x55b8,0x64);
write_cmos_sensor_16_8(0x55b9,0x62);
write_cmos_sensor_16_8(0x55ba,0x7d);
write_cmos_sensor_16_8(0x55bb,0x7d);
write_cmos_sensor_16_8(0x55bc,0x70);
write_cmos_sensor_16_8(0x55bd,0x72);
write_cmos_sensor_16_8(0x55be,0x67);
write_cmos_sensor_16_8(0x55bf,0x62);
write_cmos_sensor_16_8(0x55c0,0x7f);
write_cmos_sensor_16_8(0x55c1,0x7f);
write_cmos_sensor_16_8(0x55c2,0x73);
write_cmos_sensor_16_8(0x55c3,0x7d);
write_cmos_sensor_16_8(0x55c4,0x67);
write_cmos_sensor_16_8(0x55c5,0x63);
write_cmos_sensor_16_8(0x55c6,0x7f);
write_cmos_sensor_16_8(0x55c7,0x7f);
write_cmos_sensor_16_8(0x55c8,0x70);
write_cmos_sensor_16_8(0x55c9,0x72);
write_cmos_sensor_16_8(0x55ca,0x65);
write_cmos_sensor_16_8(0x55cb,0x61);
write_cmos_sensor_16_8(0x55cc,0x71);
write_cmos_sensor_16_8(0x55cd,0x76);
write_cmos_sensor_16_8(0x55ce,0x8b);
write_cmos_sensor_16_8(0x55cf,0x8a);
write_cmos_sensor_16_8(0x55d0,0x7f);
write_cmos_sensor_16_8(0x55d1,0x7a);
write_cmos_sensor_16_8(0x55d2,0x74);
write_cmos_sensor_16_8(0x55d3,0x89);
write_cmos_sensor_16_8(0x55d4,0x82);
write_cmos_sensor_16_8(0x55d5,0x82);
write_cmos_sensor_16_8(0x55d6,0x74);
write_cmos_sensor_16_8(0x55d7,0x7d);
write_cmos_sensor_16_8(0x55d8,0x7a);
write_cmos_sensor_16_8(0x55d9,0x70);
write_cmos_sensor_16_8(0x55da,0x74);
write_cmos_sensor_16_8(0x55db,0x89);
write_cmos_sensor_16_8(0x55dc,0x81);
write_cmos_sensor_16_8(0x55dd,0x9b);
write_cmos_sensor_16_8(0x55de,0x79);
write_cmos_sensor_16_8(0x55df,0x72);
write_cmos_sensor_16_8(0x55e0,0x8a);
write_cmos_sensor_16_8(0x55e1,0x8f);
write_cmos_sensor_16_8(0x55e2,0x87);
write_cmos_sensor_16_8(0x55e3,0x98);
write_cmos_sensor_16_8(0x55e4,0x7b);
write_cmos_sensor_16_8(0x55e5,0x72);
write_cmos_sensor_16_8(0x55e6,0x88);
write_cmos_sensor_16_8(0x55e7,0x8d);
write_cmos_sensor_16_8(0x55e8,0x98);
write_cmos_sensor_16_8(0x55e9,0x92);
write_cmos_sensor_16_8(0x55ea,0x65);
write_cmos_sensor_16_8(0x55eb,0x7e);
write_cmos_sensor_16_8(0x55ec,0x71);
write_cmos_sensor_16_8(0x55ed,0x8a);
write_cmos_sensor_16_8(0x55ee,0x85);
write_cmos_sensor_16_8(0x55ef,0x9f);
write_cmos_sensor_16_8(0x55f0,0x6d);
write_cmos_sensor_16_8(0x55f1,0x66);
write_cmos_sensor_16_8(0x55f2,0x7f);
write_cmos_sensor_16_8(0x55f3,0x71);
write_cmos_sensor_16_8(0x55f4,0x83);
write_cmos_sensor_16_8(0x55f5,0x99);
write_cmos_sensor_16_8(0x55f6,0x55);
write_cmos_sensor_16_8(0x55f7,0x6e);
write_cmos_sensor_16_8(0x55f8,0x61);
write_cmos_sensor_16_8(0x55f9,0x78);
write_cmos_sensor_16_8(0x55fa,0x88);
write_cmos_sensor_16_8(0x55fb,0x87);
write_cmos_sensor_16_8(0x55fc,0x81);
write_cmos_sensor_16_8(0x55fd,0x74);
write_cmos_sensor_16_8(0x55fe,0x7c);
write_cmos_sensor_16_8(0x55ff,0x79);
write_cmos_sensor_16_8(0x5600,0x79);
write_cmos_sensor_16_8(0x5601,0x7a);
write_cmos_sensor_16_8(0x5602,0x84);
write_cmos_sensor_16_8(0x5603,0x8d);
write_cmos_sensor_16_8(0x5604,0x71);
write_cmos_sensor_16_8(0x5605,0x72);
write_cmos_sensor_16_8(0x5606,0x7c);
write_cmos_sensor_16_8(0x5607,0x79);
write_cmos_sensor_16_8(0x5608,0x99);
write_cmos_sensor_16_8(0x5609,0x87);
write_cmos_sensor_16_8(0x560a,0x8a);
write_cmos_sensor_16_8(0x560b,0x77);
write_cmos_sensor_16_8(0x560c,0x72);
write_cmos_sensor_16_8(0x560d,0x78);
write_cmos_sensor_16_8(0x560e,0x9c);
write_cmos_sensor_16_8(0x560f,0x9a);
write_cmos_sensor_16_8(0x5610,0x8f);
write_cmos_sensor_16_8(0x5611,0x8b);
write_cmos_sensor_16_8(0x5612,0x70);
write_cmos_sensor_16_8(0x5613,0x7f);
write_cmos_sensor_16_8(0x5614,0x96);
write_cmos_sensor_16_8(0x5615,0x9f);
write_cmos_sensor_16_8(0x5616,0x82);
write_cmos_sensor_16_8(0x5617,0x89);
write_cmos_sensor_16_8(0x5618,0x76);
write_cmos_sensor_16_8(0x5619,0x7e);
write_cmos_sensor_16_8(0x561a,0xab);
write_cmos_sensor_16_8(0x561b,0x96);
write_cmos_sensor_16_8(0x561c,0x98);
write_cmos_sensor_16_8(0x561d,0x86);
write_cmos_sensor_16_8(0x561e,0x8b);
write_cmos_sensor_16_8(0x561f,0x72);
write_cmos_sensor_16_8(0x5620,0x70);
write_cmos_sensor_16_8(0x5621,0x8e);
write_cmos_sensor_16_8(0x5622,0x87);
write_cmos_sensor_16_8(0x5623,0x87);
write_cmos_sensor_16_8(0x5624,0x8f);
write_cmos_sensor_16_8(0x5625,0x8e);
write_cmos_sensor_16_8(0x5626,0x7e);
write_cmos_sensor_16_8(0x5627,0x70);
write_cmos_sensor_16_8(0x5628,0x8f);
write_cmos_sensor_16_8(0x5629,0x8f);
write_cmos_sensor_16_8(0x562a,0x75);
write_cmos_sensor_16_8(0x562b,0x74);
write_cmos_sensor_16_8(0x562c,0x67);
write_cmos_sensor_16_8(0x562d,0x7a);
write_cmos_sensor_16_8(0x562e,0x71);
write_cmos_sensor_16_8(0x562f,0x76);
write_cmos_sensor_16_8(0x5630,0x72);
write_cmos_sensor_16_8(0x5631,0x70);
write_cmos_sensor_16_8(0x5632,0x61);
write_cmos_sensor_16_8(0x5633,0x65);
write_cmos_sensor_16_8(0x5634,0x73);
write_cmos_sensor_16_8(0x5635,0x70);
write_cmos_sensor_16_8(0x5636,0x7c);
write_cmos_sensor_16_8(0x5637,0x7d);
write_cmos_sensor_16_8(0x5638,0x60);
write_cmos_sensor_16_8(0x5639,0x7a);
write_cmos_sensor_16_8(0x563a,0x71);
write_cmos_sensor_16_8(0x563b,0x76);
write_cmos_sensor_16_8(0x563c,0x72);
write_cmos_sensor_16_8(0x563d,0x71);
write_cmos_sensor_16_8(0x563e,0x60);
write_cmos_sensor_16_8(0x563f,0x65);
write_cmos_sensor_16_8(0x5640,0x72);
write_cmos_sensor_16_8(0x5641,0x70);
write_cmos_sensor_16_8(0x5642,0x73);
write_cmos_sensor_16_8(0x5643,0x71);
write_cmos_sensor_16_8(0x5644,0x9a);
write_cmos_sensor_16_8(0x5645,0x80);
write_cmos_sensor_16_8(0x5646,0x88);
write_cmos_sensor_16_8(0x5647,0x77);
write_cmos_sensor_16_8(0x5648,0x7c);
write_cmos_sensor_16_8(0x5649,0x78);
write_cmos_sensor_16_8(0x564a,0x85);
write_cmos_sensor_16_8(0x564b,0x86);
write_cmos_sensor_16_8(0x564c,0x8e);
write_cmos_sensor_16_8(0x564d,0x77);
write_cmos_sensor_16_8(0x564e,0x7f);
write_cmos_sensor_16_8(0x564f,0x64);
write_cmos_sensor_16_8(0x5650,0x9e);
write_cmos_sensor_16_8(0x5651,0x9b);
write_cmos_sensor_16_8(0x5652,0x8c);
write_cmos_sensor_16_8(0x5653,0x77);
write_cmos_sensor_16_8(0x5654,0x78);
write_cmos_sensor_16_8(0x5655,0x67);
write_cmos_sensor_16_8(0x5656,0x9a);
write_cmos_sensor_16_8(0x5657,0x86);
write_cmos_sensor_16_8(0x5658,0x75);
write_cmos_sensor_16_8(0x5659,0x7e);
write_cmos_sensor_16_8(0x565a,0x66);
write_cmos_sensor_16_8(0x565b,0x6c);
write_cmos_sensor_16_8(0x565c,0x9a);
write_cmos_sensor_16_8(0x565d,0x8c);
write_cmos_sensor_16_8(0x565e,0x73);
write_cmos_sensor_16_8(0x565f,0x7b);
write_cmos_sensor_16_8(0x5660,0x6d);
write_cmos_sensor_16_8(0x5661,0x68);
write_cmos_sensor_16_8(0x5662,0x83);
write_cmos_sensor_16_8(0x5663,0x77);
write_cmos_sensor_16_8(0x5664,0x7b);
write_cmos_sensor_16_8(0x5665,0x62);
write_cmos_sensor_16_8(0x5666,0x6a);
write_cmos_sensor_16_8(0x5667,0x51);
write_cmos_sensor_16_8(0x5668,0x78);
write_cmos_sensor_16_8(0x5669,0x78);
write_cmos_sensor_16_8(0x566a,0x7f);
write_cmos_sensor_16_8(0x566b,0x7c);
write_cmos_sensor_16_8(0x566c,0x88);
write_cmos_sensor_16_8(0x566d,0x80);
write_cmos_sensor_16_8(0x566e,0x7c);
write_cmos_sensor_16_8(0x566f,0x7d);
write_cmos_sensor_16_8(0x5670,0x73);
write_cmos_sensor_16_8(0x5671,0x77);
write_cmos_sensor_16_8(0x5672,0x80);
write_cmos_sensor_16_8(0x5673,0x98);
write_cmos_sensor_16_8(0x5674,0x7f);
write_cmos_sensor_16_8(0x5675,0x73);
write_cmos_sensor_16_8(0x5676,0x74);
write_cmos_sensor_16_8(0x5677,0x8f);
write_cmos_sensor_16_8(0x5678,0x99);
write_cmos_sensor_16_8(0x5679,0x9d);
write_cmos_sensor_16_8(0x567a,0x70);
write_cmos_sensor_16_8(0x567b,0x77);
write_cmos_sensor_16_8(0x567c,0x89);
write_cmos_sensor_16_8(0x567d,0x81);
write_cmos_sensor_16_8(0x567e,0x92);
write_cmos_sensor_16_8(0x567f,0x94);
write_cmos_sensor_16_8(0x5680,0x7d);
write_cmos_sensor_16_8(0x5681,0x75);
write_cmos_sensor_16_8(0x5682,0x8f);
write_cmos_sensor_16_8(0x5683,0x81);
write_cmos_sensor_16_8(0x5684,0x90);
write_cmos_sensor_16_8(0x5685,0xaa);
write_cmos_sensor_16_8(0x5686,0x77);
write_cmos_sensor_16_8(0x5687,0x8d);
write_cmos_sensor_16_8(0x5688,0x87);
write_cmos_sensor_16_8(0x5689,0x9f);
write_cmos_sensor_16_8(0x568a,0x95);
write_cmos_sensor_16_8(0x568b,0xae);
write_cmos_sensor_16_8(0x568c,0x88);
write_cmos_sensor_16_8(0x568d,0x8d);
write_cmos_sensor_16_8(0x568e,0x80);
write_cmos_sensor_16_8(0x568f,0x86);
write_cmos_sensor_16_8(0x5690,0x88);
write_cmos_sensor_16_8(0x5691,0x70);
write_cmos_sensor_16_8(0x5692,0x76);
write_cmos_sensor_16_8(0x5693,0x75);
write_cmos_sensor_16_8(0x5694,0x8e);
write_cmos_sensor_16_8(0x5695,0x89);
write_cmos_sensor_16_8(0x5696,0x73);
write_cmos_sensor_16_8(0x5697,0x79);
write_cmos_sensor_16_8(0x5698,0x72);
write_cmos_sensor_16_8(0x5699,0x72);
write_cmos_sensor_16_8(0x569a,0x71);
write_cmos_sensor_16_8(0x569b,0x73);
write_cmos_sensor_16_8(0x569c,0x65);
write_cmos_sensor_16_8(0x569d,0x66);
write_cmos_sensor_16_8(0x569e,0x7f);
write_cmos_sensor_16_8(0x569f,0x7c);
write_cmos_sensor_16_8(0x56a0,0x73);
write_cmos_sensor_16_8(0x56a1,0x7c);
write_cmos_sensor_16_8(0x56a2,0x66);
write_cmos_sensor_16_8(0x56a3,0x62);
write_cmos_sensor_16_8(0x56a4,0x73);
write_cmos_sensor_16_8(0x56a5,0x7d);
write_cmos_sensor_16_8(0x56a6,0x71);
write_cmos_sensor_16_8(0x56a7,0x70);
write_cmos_sensor_16_8(0x56a8,0x65);
write_cmos_sensor_16_8(0x56a9,0x62);
write_cmos_sensor_16_8(0x56aa,0x72);
write_cmos_sensor_16_8(0x56ab,0x7f);
write_cmos_sensor_16_8(0x56ac,0x72);
write_cmos_sensor_16_8(0x56ad,0x7c);
write_cmos_sensor_16_8(0x56ae,0x64);
write_cmos_sensor_16_8(0x56af,0x60);
write_cmos_sensor_16_8(0x56b0,0x63);
write_cmos_sensor_16_8(0x56b1,0x66);
write_cmos_sensor_16_8(0x56b2,0x79);
write_cmos_sensor_16_8(0x56b3,0x76);
write_cmos_sensor_16_8(0x56b4,0x8f);
write_cmos_sensor_16_8(0x56b5,0x87);
write_cmos_sensor_16_8(0x56b6,0x66);
write_cmos_sensor_16_8(0x56b7,0x67);
write_cmos_sensor_16_8(0x56b8,0x7d);
write_cmos_sensor_16_8(0x56b9,0x75);
write_cmos_sensor_16_8(0x56ba,0x82);
write_cmos_sensor_16_8(0x56bb,0x9b);
write_cmos_sensor_16_8(0x56bc,0x61);
write_cmos_sensor_16_8(0x56bd,0x65);
write_cmos_sensor_16_8(0x56be,0x72);
write_cmos_sensor_16_8(0x56bf,0x8e);
write_cmos_sensor_16_8(0x56c0,0x83);
write_cmos_sensor_16_8(0x56c1,0x9a);
write_cmos_sensor_16_8(0x56c2,0x65);
write_cmos_sensor_16_8(0x56c3,0x7b);
write_cmos_sensor_16_8(0x56c4,0x71);
write_cmos_sensor_16_8(0x56c5,0x8d);
write_cmos_sensor_16_8(0x56c6,0x83);
write_cmos_sensor_16_8(0x56c7,0x85);
write_cmos_sensor_16_8(0x56c8,0x7f);
write_cmos_sensor_16_8(0x56c9,0x73);
write_cmos_sensor_16_8(0x56ca,0x89);
write_cmos_sensor_16_8(0x56cb,0x87);
write_cmos_sensor_16_8(0x56cc,0x9a);
write_cmos_sensor_16_8(0x56cd,0x9d);
write_cmos_sensor_16_8(0x56ce,0x70);
write_cmos_sensor_16_8(0x56cf,0x8e);
write_cmos_sensor_16_8(0x56d0,0x9a);
write_cmos_sensor_16_8(0x56d1,0x93);
write_cmos_sensor_16_8(0x56d2,0x90);
write_cmos_sensor_16_8(0x56d3,0x91);
write_cmos_sensor_16_8(0x56d4,0x9d);
write_cmos_sensor_16_8(0x56d5,0x9e);
write_cmos_sensor_16_8(0x56d6,0x9a);
write_cmos_sensor_16_8(0x56d7,0x8f);
write_cmos_sensor_16_8(0x56d8,0x7f);
write_cmos_sensor_16_8(0x56d9,0x6d);
write_cmos_sensor_16_8(0x56da,0x99);
write_cmos_sensor_16_8(0x56db,0x9a);
write_cmos_sensor_16_8(0x56dc,0x8d);
write_cmos_sensor_16_8(0x56dd,0x75);
write_cmos_sensor_16_8(0x56de,0x64);
write_cmos_sensor_16_8(0x56df,0x6b);
write_cmos_sensor_16_8(0x56e0,0x9a);
write_cmos_sensor_16_8(0x56e1,0x83);
write_cmos_sensor_16_8(0x56e2,0x88);
write_cmos_sensor_16_8(0x56e3,0x7d);
write_cmos_sensor_16_8(0x56e4,0x60);
write_cmos_sensor_16_8(0x56e5,0x6a);
write_cmos_sensor_16_8(0x56e6,0x86);
write_cmos_sensor_16_8(0x56e7,0x8d);
write_cmos_sensor_16_8(0x56e8,0x75);
write_cmos_sensor_16_8(0x56e9,0x7f);
write_cmos_sensor_16_8(0x56ea,0x60);
write_cmos_sensor_16_8(0x56eb,0x68);
write_cmos_sensor_16_8(0x56ec,0x80);
write_cmos_sensor_16_8(0x56ed,0x8e);
write_cmos_sensor_16_8(0x56ee,0x77);
write_cmos_sensor_16_8(0x56ef,0x79);
write_cmos_sensor_16_8(0x56f0,0x63);
write_cmos_sensor_16_8(0x56f1,0x54);
write_cmos_sensor_16_8(0x56f2,0x8d);
write_cmos_sensor_16_8(0x56f3,0x77);
write_cmos_sensor_16_8(0x56f4,0x7e);
write_cmos_sensor_16_8(0x56f5,0x61);
write_cmos_sensor_16_8(0x56f6,0x6e);
write_cmos_sensor_16_8(0x56f7,0x54);
write_cmos_sensor_16_8(0x56f8,0x7a);
write_cmos_sensor_16_8(0x56f9,0x7b);
write_cmos_sensor_16_8(0x56fa,0x78);
write_cmos_sensor_16_8(0x56fb,0x73);
write_cmos_sensor_16_8(0x56fc,0x83);
write_cmos_sensor_16_8(0x56fd,0x9d);
write_cmos_sensor_16_8(0x56fe,0x7f);
write_cmos_sensor_16_8(0x56ff,0x7d);
write_cmos_sensor_16_8(0x5700,0x73);
write_cmos_sensor_16_8(0x5701,0x75);
write_cmos_sensor_16_8(0x5702,0x9b);
write_cmos_sensor_16_8(0x5703,0x96);
write_cmos_sensor_16_8(0x5704,0x70);
write_cmos_sensor_16_8(0x5705,0x75);
write_cmos_sensor_16_8(0x5706,0x8a);
write_cmos_sensor_16_8(0x5707,0x8c);
write_cmos_sensor_16_8(0x5708,0x9f);
write_cmos_sensor_16_8(0x5709,0x94);
write_cmos_sensor_16_8(0x570a,0x76);
write_cmos_sensor_16_8(0x570b,0x8a);
write_cmos_sensor_16_8(0x570c,0x89);
write_cmos_sensor_16_8(0x570d,0x82);
write_cmos_sensor_16_8(0x570e,0x9e);
write_cmos_sensor_16_8(0x570f,0x96);
write_cmos_sensor_16_8(0x5710,0x73);
write_cmos_sensor_16_8(0x5711,0x76);
write_cmos_sensor_16_8(0x5712,0x75);
write_cmos_sensor_16_8(0x5713,0x8f);
write_cmos_sensor_16_8(0x5714,0x9b);
write_cmos_sensor_16_8(0x5715,0x96);
write_cmos_sensor_16_8(0x5716,0x7d);
write_cmos_sensor_16_8(0x5717,0x77);
write_cmos_sensor_16_8(0x5718,0x75);
write_cmos_sensor_16_8(0x5719,0x8c);
write_cmos_sensor_16_8(0x571a,0x85);
write_cmos_sensor_16_8(0x571b,0x9d);
write_cmos_sensor_16_8(0x571c,0x9a);
write_cmos_sensor_16_8(0x571d,0x75);
write_cmos_sensor_16_8(0x571e,0x72);
write_cmos_sensor_16_8(0x571f,0x65);
write_cmos_sensor_16_8(0x5720,0x61);
write_cmos_sensor_16_8(0x5721,0x6f);
write_cmos_sensor_16_8(0x5722,0x86);
write_cmos_sensor_16_8(0x5723,0x8e);
write_cmos_sensor_16_8(0x5724,0x76);
write_cmos_sensor_16_8(0x5725,0x7c);
write_cmos_sensor_16_8(0x5726,0x64);
write_cmos_sensor_16_8(0x5727,0x62);
write_cmos_sensor_16_8(0x5728,0x83);
write_cmos_sensor_16_8(0x5729,0x89);
write_cmos_sensor_16_8(0x572a,0x74);
write_cmos_sensor_16_8(0x572b,0x7f);
write_cmos_sensor_16_8(0x572c,0x64);
write_cmos_sensor_16_8(0x572d,0x6d);
write_cmos_sensor_16_8(0x572e,0x83);
write_cmos_sensor_16_8(0x572f,0x88);
write_cmos_sensor_16_8(0x5730,0x8a);
write_cmos_sensor_16_8(0x5731,0x72);
write_cmos_sensor_16_8(0x5732,0x64);
write_cmos_sensor_16_8(0x5733,0x60);
write_cmos_sensor_16_8(0x5734,0x84);
write_cmos_sensor_16_8(0x5735,0x80);
write_cmos_sensor_16_8(0x5736,0x8d);
write_cmos_sensor_16_8(0x5737,0x74);
write_cmos_sensor_16_8(0x5738,0x7f);
write_cmos_sensor_16_8(0x5739,0x61);
write_cmos_sensor_16_8(0x573a,0x92);
write_cmos_sensor_16_8(0x573b,0x9d);
write_cmos_sensor_16_8(0x573c,0x98);
write_cmos_sensor_16_8(0x573d,0x87);
write_cmos_sensor_16_8(0x573e,0x75);
write_cmos_sensor_16_8(0x573f,0x79);
write_cmos_sensor_16_8(0x5740,0x6e);
write_cmos_sensor_16_8(0x5741,0x70);
write_cmos_sensor_16_8(0x5742,0x82);
write_cmos_sensor_16_8(0x5743,0x99);
write_cmos_sensor_16_8(0x5744,0x9f);
write_cmos_sensor_16_8(0x5745,0x90);
write_cmos_sensor_16_8(0x5746,0x69);
write_cmos_sensor_16_8(0x5747,0x7b);
write_cmos_sensor_16_8(0x5748,0x88);
write_cmos_sensor_16_8(0x5749,0x82);
write_cmos_sensor_16_8(0x574a,0x87);
write_cmos_sensor_16_8(0x574b,0x9f);
write_cmos_sensor_16_8(0x574c,0x6c);
write_cmos_sensor_16_8(0x574d,0x65);
write_cmos_sensor_16_8(0x574e,0x76);
write_cmos_sensor_16_8(0x574f,0x89);
write_cmos_sensor_16_8(0x5750,0x83);
write_cmos_sensor_16_8(0x5751,0x9e);
write_cmos_sensor_16_8(0x5752,0x6d);
write_cmos_sensor_16_8(0x5753,0x7b);
write_cmos_sensor_16_8(0x5754,0x76);
write_cmos_sensor_16_8(0x5755,0x8b);
write_cmos_sensor_16_8(0x5756,0x83);
write_cmos_sensor_16_8(0x5757,0x9a);
write_cmos_sensor_16_8(0x5758,0x6c);
write_cmos_sensor_16_8(0x5759,0x65);
write_cmos_sensor_16_8(0x575a,0x73);
write_cmos_sensor_16_8(0x575b,0x8b);
write_cmos_sensor_16_8(0x575c,0x8d);
write_cmos_sensor_16_8(0x575d,0x98);
write_cmos_sensor_16_8(0x575e,0x6a);
write_cmos_sensor_16_8(0x575f,0x6c);
write_cmos_sensor_16_8(0x5760,0x7b);
write_cmos_sensor_16_8(0x5761,0x7c);
write_cmos_sensor_16_8(0x5762,0x8a);
write_cmos_sensor_16_8(0x5763,0x84);
write_cmos_sensor_16_8(0x5764,0x93);
write_cmos_sensor_16_8(0x5765,0x8d);
write_cmos_sensor_16_8(0x5766,0x7d);
write_cmos_sensor_16_8(0x5767,0x7b);
write_cmos_sensor_16_8(0x5768,0x78);
write_cmos_sensor_16_8(0x5769,0x64);
write_cmos_sensor_16_8(0x576a,0x96);
write_cmos_sensor_16_8(0x576b,0x9a);
write_cmos_sensor_16_8(0x576c,0x77);
write_cmos_sensor_16_8(0x576d,0x72);
write_cmos_sensor_16_8(0x576e,0x73);
write_cmos_sensor_16_8(0x576f,0x7e);
write_cmos_sensor_16_8(0x5770,0x96);
write_cmos_sensor_16_8(0x5771,0x99);
write_cmos_sensor_16_8(0x5772,0x8e);
write_cmos_sensor_16_8(0x5773,0x8a);
write_cmos_sensor_16_8(0x5774,0x75);
write_cmos_sensor_16_8(0x5775,0x73);
write_cmos_sensor_16_8(0x5776,0x91);
write_cmos_sensor_16_8(0x5777,0x99);
write_cmos_sensor_16_8(0x5778,0x8c);
write_cmos_sensor_16_8(0x5779,0x88);
write_cmos_sensor_16_8(0x577a,0x8a);
write_cmos_sensor_16_8(0x577b,0x71);
write_cmos_sensor_16_8(0x577c,0x9d);
write_cmos_sensor_16_8(0x577d,0x85);
write_cmos_sensor_16_8(0x577e,0x88);
write_cmos_sensor_16_8(0x577f,0x74);
write_cmos_sensor_16_8(0x5780,0x76);
write_cmos_sensor_16_8(0x5781,0x7f);
write_cmos_sensor_16_8(0x5782,0x9d);
write_cmos_sensor_16_8(0x5783,0x85);
write_cmos_sensor_16_8(0x5784,0x89);
write_cmos_sensor_16_8(0x5785,0x75);
write_cmos_sensor_16_8(0x5786,0x76);
write_cmos_sensor_16_8(0x5787,0x7e);
write_cmos_sensor_16_8(0x5788,0x8b);
write_cmos_sensor_16_8(0x5789,0x86);
write_cmos_sensor_16_8(0x578a,0x93);
write_cmos_sensor_16_8(0x578b,0xaa);
write_cmos_sensor_16_8(0x578c,0x94);
write_cmos_sensor_16_8(0x578d,0xae);
write_cmos_sensor_16_8(0x578e,0x77);
write_cmos_sensor_16_8(0x578f,0x89);
write_cmos_sensor_16_8(0x5790,0x86);
write_cmos_sensor_16_8(0x5791,0x9f);
write_cmos_sensor_16_8(0x5792,0x92);
write_cmos_sensor_16_8(0x5793,0x96);
write_cmos_sensor_16_8(0x5794,0x7c);
write_cmos_sensor_16_8(0x5795,0x72);
write_cmos_sensor_16_8(0x5796,0x8b);
write_cmos_sensor_16_8(0x5797,0x85);
write_cmos_sensor_16_8(0x5798,0x99);
write_cmos_sensor_16_8(0x5799,0x92);
write_cmos_sensor_16_8(0x579a,0x7b);
write_cmos_sensor_16_8(0x579b,0x7e);
write_cmos_sensor_16_8(0x579c,0x77);
write_cmos_sensor_16_8(0x579d,0x80);
write_cmos_sensor_16_8(0x579e,0x9b);
write_cmos_sensor_16_8(0x579f,0x9d);
write_cmos_sensor_16_8(0x57a0,0x64);
write_cmos_sensor_16_8(0x57a1,0x7b);
write_cmos_sensor_16_8(0x57a2,0x70);
write_cmos_sensor_16_8(0x57a3,0x8c);
write_cmos_sensor_16_8(0x57a4,0x85);
write_cmos_sensor_16_8(0x57a5,0x9c);
write_cmos_sensor_16_8(0x57a6,0x62);
write_cmos_sensor_16_8(0x57a7,0x67);
write_cmos_sensor_16_8(0x57a8,0x7e);
write_cmos_sensor_16_8(0x57a9,0x8a);
write_cmos_sensor_16_8(0x57aa,0x83);
write_cmos_sensor_16_8(0x57ab,0x9b);
write_cmos_sensor_16_8(0x57ac,0x88);
write_cmos_sensor_16_8(0x57ad,0x7c);
write_cmos_sensor_16_8(0x57ae,0x66);
write_cmos_sensor_16_8(0x57af,0x6f);
write_cmos_sensor_16_8(0x57b0,0x6a);
write_cmos_sensor_16_8(0x57b1,0x5d);
write_cmos_sensor_16_8(0x57b2,0x88);
write_cmos_sensor_16_8(0x57b3,0x76);
write_cmos_sensor_16_8(0x57b4,0x7c);
write_cmos_sensor_16_8(0x57b5,0x67);
write_cmos_sensor_16_8(0x57b6,0x68);
write_cmos_sensor_16_8(0x57b7,0x51);
write_cmos_sensor_16_8(0x57b8,0x8c);
write_cmos_sensor_16_8(0x57b9,0x8b);
write_cmos_sensor_16_8(0x57ba,0x70);
write_cmos_sensor_16_8(0x57bb,0x64);
write_cmos_sensor_16_8(0x57bc,0x6b);
write_cmos_sensor_16_8(0x57bd,0x52);
write_cmos_sensor_16_8(0x57be,0x83);
write_cmos_sensor_16_8(0x57bf,0x89);
write_cmos_sensor_16_8(0x57c0,0x76);
write_cmos_sensor_16_8(0x57c1,0x7a);
write_cmos_sensor_16_8(0x57c2,0x6b);
write_cmos_sensor_16_8(0x57c3,0x52);
write_cmos_sensor_16_8(0x57c4,0x85);
write_cmos_sensor_16_8(0x57c5,0x80);
write_cmos_sensor_16_8(0x57c6,0x89);
write_cmos_sensor_16_8(0x57c7,0x7d);
write_cmos_sensor_16_8(0x57c8,0x6c);
write_cmos_sensor_16_8(0x57c9,0x50);
write_cmos_sensor_16_8(0x57ca,0x9f);
write_cmos_sensor_16_8(0x57cb,0x9b);
write_cmos_sensor_16_8(0x57cc,0x81);
write_cmos_sensor_16_8(0x57cd,0x75);
write_cmos_sensor_16_8(0x57ce,0x65);
write_cmos_sensor_16_8(0x57cf,0x6b);
write_cmos_sensor_16_8(0x57d0,0x73);
write_cmos_sensor_16_8(0x57d1,0x75);
write_cmos_sensor_16_8(0x57d2,0x88);
write_cmos_sensor_16_8(0x57d3,0x8d);
write_cmos_sensor_16_8(0x57d4,0x9a);
write_cmos_sensor_16_8(0x57d5,0x93);
write_cmos_sensor_16_8(0x57d6,0x76);
write_cmos_sensor_16_8(0x57d7,0x74);
write_cmos_sensor_16_8(0x57d8,0x8a);
write_cmos_sensor_16_8(0x57d9,0x8c);
write_cmos_sensor_16_8(0x57da,0x99);
write_cmos_sensor_16_8(0x57db,0x91);
write_cmos_sensor_16_8(0x57dc,0x74);
write_cmos_sensor_16_8(0x57dd,0x8b);
write_cmos_sensor_16_8(0x57de,0x8e);
write_cmos_sensor_16_8(0x57df,0x80);
write_cmos_sensor_16_8(0x57e0,0x92);
write_cmos_sensor_16_8(0x57e1,0xab);
write_cmos_sensor_16_8(0x57e2,0x74);
write_cmos_sensor_16_8(0x57e3,0x88);
write_cmos_sensor_16_8(0x57e4,0x89);
write_cmos_sensor_16_8(0x57e5,0x80);
write_cmos_sensor_16_8(0x57e6,0x90);
write_cmos_sensor_16_8(0x57e7,0xae);
write_cmos_sensor_16_8(0x57e8,0x70);
write_cmos_sensor_16_8(0x57e9,0x76);
write_cmos_sensor_16_8(0x57ea,0x77);
write_cmos_sensor_16_8(0x57eb,0x8f);
write_cmos_sensor_16_8(0x57ec,0x9c);
write_cmos_sensor_16_8(0x57ed,0xa8);
write_cmos_sensor_16_8(0x57ee,0x7e);
write_cmos_sensor_16_8(0x57ef,0x7c);
write_cmos_sensor_16_8(0x57f0,0x7c);
write_cmos_sensor_16_8(0x57f1,0x75);
write_cmos_sensor_16_8(0x57f2,0x84);
write_cmos_sensor_16_8(0x57f3,0x96);
write_cmos_sensor_16_8(0x57f4,0x95);
write_cmos_sensor_16_8(0x57f5,0x96);
write_cmos_sensor_16_8(0x57f6,0x95);
write_cmos_sensor_16_8(0x57f7,0x90);
write_cmos_sensor_16_8(0x57f8,0x84);
write_cmos_sensor_16_8(0x57f9,0x8b);
write_cmos_sensor_16_8(0x57fa,0x9c);
write_cmos_sensor_16_8(0x57fb,0x9d);
write_cmos_sensor_16_8(0x57fc,0x99);
write_cmos_sensor_16_8(0x57fd,0x80);
write_cmos_sensor_16_8(0x57fe,0x88);
write_cmos_sensor_16_8(0x57ff,0x70);
write_cmos_sensor_16_8(0x5800,0x98);
write_cmos_sensor_16_8(0x5801,0x87);
write_cmos_sensor_16_8(0x5802,0x82);
write_cmos_sensor_16_8(0x5803,0x77);
write_cmos_sensor_16_8(0x5804,0x7c);
write_cmos_sensor_16_8(0x5805,0x67);
write_cmos_sensor_16_8(0x5806,0x9b);
write_cmos_sensor_16_8(0x5807,0x80);
write_cmos_sensor_16_8(0x5808,0x89);
write_cmos_sensor_16_8(0x5809,0x72);
write_cmos_sensor_16_8(0x580a,0x7b);
write_cmos_sensor_16_8(0x580b,0x60);
write_cmos_sensor_16_8(0x580c,0x85);
write_cmos_sensor_16_8(0x580d,0x83);
write_cmos_sensor_16_8(0x580e,0x74);
write_cmos_sensor_16_8(0x580f,0x7f);
write_cmos_sensor_16_8(0x5810,0x65);
write_cmos_sensor_16_8(0x5811,0x6c);
write_cmos_sensor_16_8(0x5812,0x9b);
write_cmos_sensor_16_8(0x5813,0x82);
write_cmos_sensor_16_8(0x5814,0x76);
write_cmos_sensor_16_8(0x5815,0x7b);
write_cmos_sensor_16_8(0x5816,0x61);
write_cmos_sensor_16_8(0x5817,0x6d);
write_cmos_sensor_16_8(0x5818,0x51);
write_cmos_sensor_16_8(0x5819,0x69);
write_cmos_sensor_16_8(0x581a,0x6d);
write_cmos_sensor_16_8(0x581b,0x66);
write_cmos_sensor_16_8(0x581c,0x7e);
write_cmos_sensor_16_8(0x581d,0x88);
write_cmos_sensor_16_8(0x581e,0x54);
write_cmos_sensor_16_8(0x581f,0x69);
write_cmos_sensor_16_8(0x5820,0x7a);
write_cmos_sensor_16_8(0x5821,0x72);
write_cmos_sensor_16_8(0x5822,0x77);
write_cmos_sensor_16_8(0x5823,0x8f);
write_cmos_sensor_16_8(0x5824,0x56);
write_cmos_sensor_16_8(0x5825,0x6c);
write_cmos_sensor_16_8(0x5826,0x7e);
write_cmos_sensor_16_8(0x5827,0x77);
write_cmos_sensor_16_8(0x5828,0x89);
write_cmos_sensor_16_8(0x5829,0x87);
write_cmos_sensor_16_8(0x582a,0x57);
write_cmos_sensor_16_8(0x582b,0x6d);
write_cmos_sensor_16_8(0x582c,0x7d);
write_cmos_sensor_16_8(0x582d,0x8b);
write_cmos_sensor_16_8(0x582e,0x8f);
write_cmos_sensor_16_8(0x582f,0x9a);
write_cmos_sensor_16_8(0x5830,0x6a);
write_cmos_sensor_16_8(0x5831,0x60);
write_cmos_sensor_16_8(0x5832,0x74);
write_cmos_sensor_16_8(0x5833,0x8d);
write_cmos_sensor_16_8(0x5834,0x81);
write_cmos_sensor_16_8(0x5835,0x9c);
write_cmos_sensor_16_8(0x5836,0x68);
write_cmos_sensor_16_8(0x5837,0x65);
write_cmos_sensor_16_8(0x5838,0x89);
write_cmos_sensor_16_8(0x5839,0x85);
write_cmos_sensor_16_8(0x583a,0x99);
write_cmos_sensor_16_8(0x583b,0x92);
write_cmos_sensor_16_8(0x583c,0x9d);
write_cmos_sensor_16_8(0x583d,0x85);
write_cmos_sensor_16_8(0x583e,0x8f);
write_cmos_sensor_16_8(0x583f,0x8b);
write_cmos_sensor_16_8(0x5840,0x8a);
write_cmos_sensor_16_8(0x5841,0x73);
write_cmos_sensor_16_8(0x5842,0x91);
write_cmos_sensor_16_8(0x5843,0x99);
write_cmos_sensor_16_8(0x5844,0x8e);
write_cmos_sensor_16_8(0x5845,0x75);
write_cmos_sensor_16_8(0x5846,0x75);
write_cmos_sensor_16_8(0x5847,0x70);
write_cmos_sensor_16_8(0x5848,0xab);
write_cmos_sensor_16_8(0x5849,0x92);
write_cmos_sensor_16_8(0x584a,0x82);
write_cmos_sensor_16_8(0x584b,0x89);
write_cmos_sensor_16_8(0x584c,0x88);
write_cmos_sensor_16_8(0x584d,0x71);
write_cmos_sensor_16_8(0x584e,0xa9);
write_cmos_sensor_16_8(0x584f,0x93);
write_cmos_sensor_16_8(0x5850,0x8d);
write_cmos_sensor_16_8(0x5851,0x88);
write_cmos_sensor_16_8(0x5852,0x88);
write_cmos_sensor_16_8(0x5853,0x71);
write_cmos_sensor_16_8(0x5854,0xaa);
write_cmos_sensor_16_8(0x5855,0x9c);
write_cmos_sensor_16_8(0x5856,0x88);
write_cmos_sensor_16_8(0x5857,0x71);
write_cmos_sensor_16_8(0x5858,0x71);
write_cmos_sensor_16_8(0x5859,0x7f);
write_cmos_sensor_16_8(0x585a,0x96);
write_cmos_sensor_16_8(0x585b,0x9a);
write_cmos_sensor_16_8(0x585c,0x77);
write_cmos_sensor_16_8(0x585d,0x7e);
write_cmos_sensor_16_8(0x585e,0x7f);
write_cmos_sensor_16_8(0x585f,0x78);
write_cmos_sensor_16_8(0x5860,0x00);
write_cmos_sensor_16_8(0x5880,0xc1);
write_cmos_sensor_16_8(0x588a,0x00);
write_cmos_sensor_16_8(0x58cb,0x03);
write_cmos_sensor_16_8(0x5900,0x40);
write_cmos_sensor_16_8(0x5901,0x40);
write_cmos_sensor_16_8(0x5902,0x40);
write_cmos_sensor_16_8(0x5903,0x40);
write_cmos_sensor_16_8(0x5904,0x40);
write_cmos_sensor_16_8(0x5905,0x40);
write_cmos_sensor_16_8(0x5906,0x40);
write_cmos_sensor_16_8(0x5907,0x40);
write_cmos_sensor_16_8(0x5908,0x40);
write_cmos_sensor_16_8(0x5909,0x40);
write_cmos_sensor_16_8(0x590a,0x40);
write_cmos_sensor_16_8(0x590b,0x40);
write_cmos_sensor_16_8(0x590c,0x40);
write_cmos_sensor_16_8(0x590d,0x40);
write_cmos_sensor_16_8(0x590e,0x40);
write_cmos_sensor_16_8(0x590f,0x40);
write_cmos_sensor_16_8(0x5910,0x40);
write_cmos_sensor_16_8(0x5911,0x40);
write_cmos_sensor_16_8(0x5912,0x40);
write_cmos_sensor_16_8(0x5913,0x40);
write_cmos_sensor_16_8(0x5914,0x40);
write_cmos_sensor_16_8(0x5915,0x40);
write_cmos_sensor_16_8(0x5916,0x40);
write_cmos_sensor_16_8(0x5917,0x40);
write_cmos_sensor_16_8(0x5918,0x40);
write_cmos_sensor_16_8(0x5919,0x40);
write_cmos_sensor_16_8(0x591a,0x40);
write_cmos_sensor_16_8(0x591b,0x40);
write_cmos_sensor_16_8(0x591c,0x40);
write_cmos_sensor_16_8(0x591d,0x40);
write_cmos_sensor_16_8(0x591e,0x40);
write_cmos_sensor_16_8(0x591f,0x40);
write_cmos_sensor_16_8(0x5920,0x40);
write_cmos_sensor_16_8(0x5921,0x40);
write_cmos_sensor_16_8(0x5922,0x40);
write_cmos_sensor_16_8(0x5923,0x40);
write_cmos_sensor_16_8(0x5924,0x40);
write_cmos_sensor_16_8(0x5925,0x40);
write_cmos_sensor_16_8(0x5926,0x40);
write_cmos_sensor_16_8(0x5927,0x40);
write_cmos_sensor_16_8(0x5928,0x40);
write_cmos_sensor_16_8(0x5929,0x40);
write_cmos_sensor_16_8(0x592a,0x40);
write_cmos_sensor_16_8(0x592b,0x40);
write_cmos_sensor_16_8(0x592c,0x40);
write_cmos_sensor_16_8(0x592d,0x40);
write_cmos_sensor_16_8(0x592e,0x40);
write_cmos_sensor_16_8(0x592f,0x40);
write_cmos_sensor_16_8(0x5930,0x40);
write_cmos_sensor_16_8(0x5931,0x40);
write_cmos_sensor_16_8(0x5932,0x40);
write_cmos_sensor_16_8(0x5933,0x40);
write_cmos_sensor_16_8(0x5934,0x40);
write_cmos_sensor_16_8(0x5935,0x40);
write_cmos_sensor_16_8(0x5936,0x40);
write_cmos_sensor_16_8(0x5937,0x40);
write_cmos_sensor_16_8(0x5938,0x40);
write_cmos_sensor_16_8(0x5939,0x40);
write_cmos_sensor_16_8(0x593a,0x40);
write_cmos_sensor_16_8(0x593b,0x40);
write_cmos_sensor_16_8(0x593c,0x40);
write_cmos_sensor_16_8(0x593d,0x40);
write_cmos_sensor_16_8(0x593e,0x40);
write_cmos_sensor_16_8(0x593f,0x40);
write_cmos_sensor_16_8(0x5940,0x40);
write_cmos_sensor_16_8(0x5941,0x40);
write_cmos_sensor_16_8(0x5942,0x40);
write_cmos_sensor_16_8(0x5943,0x40);
write_cmos_sensor_16_8(0x5944,0x40);
write_cmos_sensor_16_8(0x5945,0x40);
write_cmos_sensor_16_8(0x5946,0x40);
write_cmos_sensor_16_8(0x5947,0x40);
write_cmos_sensor_16_8(0x5948,0x40);
write_cmos_sensor_16_8(0x5949,0x40);
write_cmos_sensor_16_8(0x594a,0x40);
write_cmos_sensor_16_8(0x594b,0x40);
write_cmos_sensor_16_8(0x594c,0x40);
write_cmos_sensor_16_8(0x594d,0x40);
write_cmos_sensor_16_8(0x594e,0x40);
write_cmos_sensor_16_8(0x594f,0x40);
write_cmos_sensor_16_8(0x5950,0x40);
write_cmos_sensor_16_8(0x5951,0x40);
write_cmos_sensor_16_8(0x5952,0x40);
write_cmos_sensor_16_8(0x5953,0x40);
write_cmos_sensor_16_8(0x5954,0x40);
write_cmos_sensor_16_8(0x5955,0x40);
write_cmos_sensor_16_8(0x5956,0x40);
write_cmos_sensor_16_8(0x5957,0x40);
write_cmos_sensor_16_8(0x5958,0x40);
write_cmos_sensor_16_8(0x5959,0x40);
write_cmos_sensor_16_8(0x595a,0x40);
write_cmos_sensor_16_8(0x595b,0x40);
write_cmos_sensor_16_8(0x595c,0x40);
write_cmos_sensor_16_8(0x595d,0x40);
write_cmos_sensor_16_8(0x595e,0x40);
write_cmos_sensor_16_8(0x595f,0x40);
write_cmos_sensor_16_8(0x5960,0x40);
write_cmos_sensor_16_8(0x5961,0x40);
write_cmos_sensor_16_8(0x5962,0x40);
write_cmos_sensor_16_8(0x5963,0x40);
write_cmos_sensor_16_8(0x5964,0x40);
write_cmos_sensor_16_8(0x5965,0x40);
write_cmos_sensor_16_8(0x5966,0x40);
write_cmos_sensor_16_8(0x5967,0x40);
write_cmos_sensor_16_8(0x5968,0x40);
write_cmos_sensor_16_8(0x5969,0x40);
write_cmos_sensor_16_8(0x596a,0x40);
write_cmos_sensor_16_8(0x596b,0x40);
write_cmos_sensor_16_8(0x596c,0x40);
write_cmos_sensor_16_8(0x596d,0x40);
write_cmos_sensor_16_8(0x596e,0x40);
write_cmos_sensor_16_8(0x596f,0x40);
write_cmos_sensor_16_8(0x5970,0x40);
write_cmos_sensor_16_8(0x5971,0x40);
write_cmos_sensor_16_8(0x5972,0x40);
write_cmos_sensor_16_8(0x5973,0x40);
write_cmos_sensor_16_8(0x5974,0x40);
write_cmos_sensor_16_8(0x5975,0x40);
write_cmos_sensor_16_8(0x5976,0x40);
write_cmos_sensor_16_8(0x5977,0x40);
write_cmos_sensor_16_8(0x5978,0x40);
write_cmos_sensor_16_8(0x5979,0x40);
write_cmos_sensor_16_8(0x597a,0x40);
write_cmos_sensor_16_8(0x597b,0x40);
write_cmos_sensor_16_8(0x597c,0x40);
write_cmos_sensor_16_8(0x597d,0x40);
write_cmos_sensor_16_8(0x597e,0x40);
write_cmos_sensor_16_8(0x597f,0x40);
write_cmos_sensor_16_8(0x5980,0x40);
write_cmos_sensor_16_8(0x5981,0x40);
write_cmos_sensor_16_8(0x5982,0x40);
write_cmos_sensor_16_8(0x5983,0x40);
write_cmos_sensor_16_8(0x5984,0x40);
write_cmos_sensor_16_8(0x5985,0x40);
write_cmos_sensor_16_8(0x5986,0x40);
write_cmos_sensor_16_8(0x5987,0x40);
write_cmos_sensor_16_8(0x5988,0x40);
write_cmos_sensor_16_8(0x5989,0x40);
write_cmos_sensor_16_8(0x598a,0x40);
write_cmos_sensor_16_8(0x598b,0x40);
write_cmos_sensor_16_8(0x598c,0x40);
write_cmos_sensor_16_8(0x598d,0x40);
write_cmos_sensor_16_8(0x598e,0x40);
write_cmos_sensor_16_8(0x598f,0x40);
write_cmos_sensor_16_8(0x5990,0x40);
write_cmos_sensor_16_8(0x5991,0x40);
write_cmos_sensor_16_8(0x5992,0x40);
write_cmos_sensor_16_8(0x5993,0x40);
write_cmos_sensor_16_8(0x5994,0x40);
write_cmos_sensor_16_8(0x5995,0x40);
write_cmos_sensor_16_8(0x5996,0x40);
write_cmos_sensor_16_8(0x5997,0x40);
write_cmos_sensor_16_8(0x5998,0x40);
write_cmos_sensor_16_8(0x5999,0x40);
write_cmos_sensor_16_8(0x599a,0x40);
write_cmos_sensor_16_8(0x599b,0x40);
write_cmos_sensor_16_8(0x599c,0x40);
write_cmos_sensor_16_8(0x599d,0x40);
write_cmos_sensor_16_8(0x599e,0x40);
write_cmos_sensor_16_8(0x599f,0x40);
write_cmos_sensor_16_8(0x59a0,0x40);
write_cmos_sensor_16_8(0x59a1,0x40);
write_cmos_sensor_16_8(0x59a2,0x40);
write_cmos_sensor_16_8(0x59a3,0x40);
write_cmos_sensor_16_8(0x59a4,0x40);
write_cmos_sensor_16_8(0x59a5,0x40);
write_cmos_sensor_16_8(0x59a6,0x40);
write_cmos_sensor_16_8(0x59a7,0x40);
write_cmos_sensor_16_8(0x59a8,0x40);
write_cmos_sensor_16_8(0x59a9,0x40);
write_cmos_sensor_16_8(0x59aa,0x40);
write_cmos_sensor_16_8(0x59ab,0x40);
write_cmos_sensor_16_8(0x59ac,0x40);
write_cmos_sensor_16_8(0x59ad,0x40);
write_cmos_sensor_16_8(0x59ae,0x40);
write_cmos_sensor_16_8(0x59af,0x40);
write_cmos_sensor_16_8(0x59b0,0x40);
write_cmos_sensor_16_8(0x59b1,0x40);
write_cmos_sensor_16_8(0x59b2,0x40);
write_cmos_sensor_16_8(0x59b3,0x40);
write_cmos_sensor_16_8(0x59b4,0xcd);
write_cmos_sensor_16_8(0x59b5,0xcd);
write_cmos_sensor_16_8(0x59b6,0xcd);
write_cmos_sensor_16_8(0x59b7,0xcd);
write_cmos_sensor_16_8(0x59b8,0xcd);
write_cmos_sensor_16_8(0x59b9,0xcd);
write_cmos_sensor_16_8(0x59ba,0xcd);
write_cmos_sensor_16_8(0x59bb,0xcd);
write_cmos_sensor_16_8(0x59bc,0xcd);
write_cmos_sensor_16_8(0x59bd,0xcd);
write_cmos_sensor_16_8(0x59be,0xcd);
write_cmos_sensor_16_8(0x59bf,0xcd);
write_cmos_sensor_16_8(0x59c0,0xcd);
write_cmos_sensor_16_8(0x59c1,0xcd);
write_cmos_sensor_16_8(0x59c2,0xcd);
write_cmos_sensor_16_8(0x59c3,0xcd);
write_cmos_sensor_16_8(0x59c4,0xcd);
write_cmos_sensor_16_8(0x59c5,0xcd);
write_cmos_sensor_16_8(0x59c6,0xcd);
write_cmos_sensor_16_8(0x59c7,0xcd);
write_cmos_sensor_16_8(0x59c8,0xcd);
write_cmos_sensor_16_8(0x59c9,0xcd);
write_cmos_sensor_16_8(0x59ca,0xcd);
write_cmos_sensor_16_8(0x59cb,0xcd);
write_cmos_sensor_16_8(0x59cc,0xcd);
write_cmos_sensor_16_8(0x59cd,0xcd);
write_cmos_sensor_16_8(0x59ce,0xcd);
write_cmos_sensor_16_8(0x59cf,0xcd);
write_cmos_sensor_16_8(0x59d0,0xcd);
write_cmos_sensor_16_8(0x59d1,0xcd);
write_cmos_sensor_16_8(0x59d2,0xcd);
write_cmos_sensor_16_8(0x59d3,0xcd);
write_cmos_sensor_16_8(0x59d4,0xcd);
write_cmos_sensor_16_8(0x59d5,0xcd);
write_cmos_sensor_16_8(0x59d6,0xcd);
write_cmos_sensor_16_8(0x59d7,0xcd);
write_cmos_sensor_16_8(0x59d8,0xcd);
write_cmos_sensor_16_8(0x59d9,0xcd);
write_cmos_sensor_16_8(0x59da,0xcd);
write_cmos_sensor_16_8(0x59db,0xcd);
write_cmos_sensor_16_8(0x59dc,0xcd);
write_cmos_sensor_16_8(0x59dd,0xcd);
write_cmos_sensor_16_8(0x59de,0xcd);
write_cmos_sensor_16_8(0x59df,0xcd);
write_cmos_sensor_16_8(0x59e0,0xcd);
write_cmos_sensor_16_8(0x59e1,0xcd);
write_cmos_sensor_16_8(0x59e2,0xcd);
write_cmos_sensor_16_8(0x59e3,0xcd);
write_cmos_sensor_16_8(0x59e4,0xcd);
write_cmos_sensor_16_8(0x59e5,0xcd);
write_cmos_sensor_16_8(0x59e6,0xcd);
write_cmos_sensor_16_8(0x59e7,0xcd);
write_cmos_sensor_16_8(0x59e8,0xcd);
write_cmos_sensor_16_8(0x59e9,0xcd);
write_cmos_sensor_16_8(0x59ea,0xcd);
write_cmos_sensor_16_8(0x59eb,0xcd);
write_cmos_sensor_16_8(0x59ec,0xcd);
write_cmos_sensor_16_8(0x59ed,0xcd);
write_cmos_sensor_16_8(0x59ee,0xcd);
write_cmos_sensor_16_8(0x59ef,0xcd);
write_cmos_sensor_16_8(0x59f0,0xcd);
write_cmos_sensor_16_8(0x59f1,0xcd);
write_cmos_sensor_16_8(0x59f2,0xcd);
write_cmos_sensor_16_8(0x59f3,0xcd);
write_cmos_sensor_16_8(0x59f4,0xcd);
write_cmos_sensor_16_8(0x59f5,0xcd);
write_cmos_sensor_16_8(0x59f6,0xcd);
write_cmos_sensor_16_8(0x59f7,0xcd);
write_cmos_sensor_16_8(0x59f8,0xcd);
write_cmos_sensor_16_8(0x59f9,0xcd);
write_cmos_sensor_16_8(0x59fa,0xcd);
write_cmos_sensor_16_8(0x59fb,0xcd);
write_cmos_sensor_16_8(0x59fc,0xcd);
write_cmos_sensor_16_8(0x59fd,0xcd);
write_cmos_sensor_16_8(0x59fe,0xcd);
write_cmos_sensor_16_8(0x59ff,0xcd);
write_cmos_sensor_16_8(0x5a00,0xcd);
write_cmos_sensor_16_8(0x5a01,0xcd);
write_cmos_sensor_16_8(0x5a02,0xcd);
write_cmos_sensor_16_8(0x5a03,0xcd);
write_cmos_sensor_16_8(0x5a04,0xcd);
write_cmos_sensor_16_8(0x5a05,0xcd);
write_cmos_sensor_16_8(0x5a06,0xcd);
write_cmos_sensor_16_8(0x5a07,0xcd);
write_cmos_sensor_16_8(0x5a08,0xcd);
write_cmos_sensor_16_8(0x5a09,0xcd);
write_cmos_sensor_16_8(0x5a0a,0xcd);
write_cmos_sensor_16_8(0x5a0b,0xcd);
write_cmos_sensor_16_8(0x5a0c,0xcd);
write_cmos_sensor_16_8(0x5a0d,0xcd);
write_cmos_sensor_16_8(0x5a0e,0xcd);
write_cmos_sensor_16_8(0x5a0f,0xcd);
write_cmos_sensor_16_8(0x5a10,0xcd);
write_cmos_sensor_16_8(0x5a11,0xcd);
write_cmos_sensor_16_8(0x5a12,0xcd);
write_cmos_sensor_16_8(0x5a13,0xcd);
write_cmos_sensor_16_8(0x5a14,0xcd);
write_cmos_sensor_16_8(0x5a15,0xcd);
write_cmos_sensor_16_8(0x5a16,0xcd);
write_cmos_sensor_16_8(0x5a17,0xcd);
write_cmos_sensor_16_8(0x5a18,0xcd);
write_cmos_sensor_16_8(0x5a19,0xcd);
write_cmos_sensor_16_8(0x5a1a,0xcd);
write_cmos_sensor_16_8(0x5a1b,0xcd);
write_cmos_sensor_16_8(0x5a1c,0xcd);
write_cmos_sensor_16_8(0x5a1d,0xcd);
write_cmos_sensor_16_8(0x5a1e,0xcd);
write_cmos_sensor_16_8(0x5a1f,0xcd);
write_cmos_sensor_16_8(0x5a20,0xcd);
write_cmos_sensor_16_8(0x5a21,0xcd);
write_cmos_sensor_16_8(0x5a22,0xcd);
write_cmos_sensor_16_8(0x5a23,0xcd);
write_cmos_sensor_16_8(0x5a24,0xcd);
write_cmos_sensor_16_8(0x5a25,0xcd);
write_cmos_sensor_16_8(0x5a26,0xcd);
write_cmos_sensor_16_8(0x5a27,0xcd);
write_cmos_sensor_16_8(0x5a28,0xcd);
write_cmos_sensor_16_8(0x5a29,0xcd);
write_cmos_sensor_16_8(0x5a2a,0xcd);
write_cmos_sensor_16_8(0x5a2b,0xcd);
write_cmos_sensor_16_8(0x5a2c,0xcd);
write_cmos_sensor_16_8(0x5a2d,0xcd);
write_cmos_sensor_16_8(0x5a2e,0xcd);
write_cmos_sensor_16_8(0x5a2f,0xcd);
write_cmos_sensor_16_8(0x5a30,0xcd);
write_cmos_sensor_16_8(0x5a31,0xcd);
write_cmos_sensor_16_8(0x5a32,0xcd);
write_cmos_sensor_16_8(0x5a33,0xcd);
write_cmos_sensor_16_8(0x5a34,0xcd);
write_cmos_sensor_16_8(0x5a35,0xcd);
write_cmos_sensor_16_8(0x5a36,0xcd);
write_cmos_sensor_16_8(0x5a37,0xcd);
write_cmos_sensor_16_8(0x5a38,0xcd);
write_cmos_sensor_16_8(0x5a39,0xcd);
write_cmos_sensor_16_8(0x5a3a,0xcd);
write_cmos_sensor_16_8(0x5a3b,0xcd);
write_cmos_sensor_16_8(0x5a3c,0xcd);
write_cmos_sensor_16_8(0x5a3d,0xcd);
write_cmos_sensor_16_8(0x5a3e,0xcd);
write_cmos_sensor_16_8(0x5a3f,0xcd);
write_cmos_sensor_16_8(0x5a40,0xcd);
write_cmos_sensor_16_8(0x5a41,0xcd);
write_cmos_sensor_16_8(0x5a42,0xcd);
write_cmos_sensor_16_8(0x5a43,0xcd);
write_cmos_sensor_16_8(0x5a44,0xcd);
write_cmos_sensor_16_8(0x5a45,0xcd);
write_cmos_sensor_16_8(0x5a46,0xcd);
write_cmos_sensor_16_8(0x5a47,0xcd);
write_cmos_sensor_16_8(0x5a48,0xcd);
write_cmos_sensor_16_8(0x5a49,0xcd);
write_cmos_sensor_16_8(0x5a4a,0xcd);
write_cmos_sensor_16_8(0x5a4b,0xcd);
write_cmos_sensor_16_8(0x5a4c,0xcd);
write_cmos_sensor_16_8(0x5a4d,0xcd);
write_cmos_sensor_16_8(0x5a4e,0xcd);
write_cmos_sensor_16_8(0x5a4f,0xcd);
write_cmos_sensor_16_8(0x5a50,0xcd);
write_cmos_sensor_16_8(0x5a51,0xcd);
write_cmos_sensor_16_8(0x5a52,0xcd);
write_cmos_sensor_16_8(0x5a53,0xcd);
write_cmos_sensor_16_8(0x5a54,0xcd);
write_cmos_sensor_16_8(0x5a55,0xcd);
write_cmos_sensor_16_8(0x5a56,0xcd);
write_cmos_sensor_16_8(0x5a57,0xcd);
write_cmos_sensor_16_8(0x5a58,0xcd);
write_cmos_sensor_16_8(0x5a59,0xcd);
write_cmos_sensor_16_8(0x5a5a,0xcd);
write_cmos_sensor_16_8(0x5a5b,0xcd);
write_cmos_sensor_16_8(0x5a5c,0xcd);
write_cmos_sensor_16_8(0x5a5d,0xcd);
write_cmos_sensor_16_8(0x5a5e,0xcd);
write_cmos_sensor_16_8(0x5a5f,0xcd);
write_cmos_sensor_16_8(0x5a60,0xcd);
write_cmos_sensor_16_8(0x5a61,0xcd);
write_cmos_sensor_16_8(0x5a62,0xcd);
write_cmos_sensor_16_8(0x5a63,0xcd);
write_cmos_sensor_16_8(0x5a64,0xcd);
write_cmos_sensor_16_8(0x5a65,0xcd);
write_cmos_sensor_16_8(0x5a66,0xcd);
write_cmos_sensor_16_8(0x5a67,0xcd);
write_cmos_sensor_16_8(0x5a68,0xcd);
write_cmos_sensor_16_8(0x5a69,0xcd);
write_cmos_sensor_16_8(0x5a6a,0xcd);
write_cmos_sensor_16_8(0x5a6b,0xcd);
write_cmos_sensor_16_8(0x5a6c,0xcd);
write_cmos_sensor_16_8(0x5a6d,0xcd);
write_cmos_sensor_16_8(0x5a6e,0xcd);
write_cmos_sensor_16_8(0x5a6f,0xcd);
write_cmos_sensor_16_8(0x5a70,0xcd);
write_cmos_sensor_16_8(0x5a71,0xcd);
write_cmos_sensor_16_8(0x5a72,0xcd);
write_cmos_sensor_16_8(0x5a73,0xcd);
write_cmos_sensor_16_8(0x5a74,0xcd);
write_cmos_sensor_16_8(0x5a75,0xcd);
write_cmos_sensor_16_8(0x5a76,0xcd);
write_cmos_sensor_16_8(0x5a77,0xcd);
write_cmos_sensor_16_8(0x5a78,0xcd);
write_cmos_sensor_16_8(0x5a79,0xcd);
write_cmos_sensor_16_8(0x5a7a,0xcd);
write_cmos_sensor_16_8(0x5a7b,0xcd);
write_cmos_sensor_16_8(0x5a7c,0xcd);
write_cmos_sensor_16_8(0x5a7d,0xcd);
write_cmos_sensor_16_8(0x5a7e,0xcd);
write_cmos_sensor_16_8(0x5a7f,0xcd);
write_cmos_sensor_16_8(0x5a80,0xcd);
write_cmos_sensor_16_8(0x5a81,0xcd);
write_cmos_sensor_16_8(0x5a82,0xcd);
write_cmos_sensor_16_8(0x5a83,0xcd);
write_cmos_sensor_16_8(0x5a84,0xcd);
write_cmos_sensor_16_8(0x5a85,0xcd);
write_cmos_sensor_16_8(0x5a86,0xcd);
write_cmos_sensor_16_8(0x5a87,0xcd);
write_cmos_sensor_16_8(0x5a88,0xcd);
write_cmos_sensor_16_8(0x5a89,0xcd);
write_cmos_sensor_16_8(0x5a8a,0xcd);
write_cmos_sensor_16_8(0x5a8b,0xcd);
write_cmos_sensor_16_8(0x5a8c,0xcd);
write_cmos_sensor_16_8(0x5a8d,0xcd);
write_cmos_sensor_16_8(0x5a8e,0xcd);
write_cmos_sensor_16_8(0x5a8f,0xcd);
write_cmos_sensor_16_8(0x5a90,0xcd);
write_cmos_sensor_16_8(0x5a91,0xcd);
write_cmos_sensor_16_8(0x5a92,0xcd);
write_cmos_sensor_16_8(0x5a93,0xcd);
write_cmos_sensor_16_8(0x5a94,0xcd);
write_cmos_sensor_16_8(0x5a95,0xcd);
write_cmos_sensor_16_8(0x5a96,0xcd);
write_cmos_sensor_16_8(0x5a97,0xcd);
write_cmos_sensor_16_8(0x5a98,0xcd);
write_cmos_sensor_16_8(0x5a99,0xcd);
write_cmos_sensor_16_8(0x5a9a,0xcd);
write_cmos_sensor_16_8(0x5a9b,0xcd);
write_cmos_sensor_16_8(0x5a9c,0xcd);
write_cmos_sensor_16_8(0x5a9d,0xcd);
write_cmos_sensor_16_8(0x5a9e,0xcd);
write_cmos_sensor_16_8(0x5a9f,0xcd);
write_cmos_sensor_16_8(0x5aa0,0xcd);
write_cmos_sensor_16_8(0x5aa1,0xcd);
write_cmos_sensor_16_8(0x5aa2,0xcd);
write_cmos_sensor_16_8(0x5aa3,0xcd);
write_cmos_sensor_16_8(0x5aa4,0xcd);
write_cmos_sensor_16_8(0x5aa5,0xcd);
write_cmos_sensor_16_8(0x5aa6,0xcd);
write_cmos_sensor_16_8(0x5aa7,0xcd);
write_cmos_sensor_16_8(0x5aa8,0xcd);
write_cmos_sensor_16_8(0x5aa9,0xcd);
write_cmos_sensor_16_8(0x5aaa,0xcd);
write_cmos_sensor_16_8(0x5aab,0xcd);
write_cmos_sensor_16_8(0x5aac,0xcd);
write_cmos_sensor_16_8(0x5aad,0xcd);
write_cmos_sensor_16_8(0x5aae,0xcd);
write_cmos_sensor_16_8(0x5aaf,0xcd);
write_cmos_sensor_16_8(0x5ab0,0xcd);
write_cmos_sensor_16_8(0x5ab1,0xcd);
write_cmos_sensor_16_8(0x5ab2,0xcd);
write_cmos_sensor_16_8(0x5ab3,0xcd);
write_cmos_sensor_16_8(0x5ab4,0xcd);
write_cmos_sensor_16_8(0x5ab5,0xcd);
write_cmos_sensor_16_8(0x5ab6,0xcd);
write_cmos_sensor_16_8(0x5ab7,0xcd);
write_cmos_sensor_16_8(0x5ab8,0xcd);
write_cmos_sensor_16_8(0x5ab9,0xcd);
write_cmos_sensor_16_8(0x5aba,0xcd);
write_cmos_sensor_16_8(0x5abb,0xcd);
write_cmos_sensor_16_8(0x5abc,0xcd);
write_cmos_sensor_16_8(0x5abd,0xcd);
write_cmos_sensor_16_8(0x5abe,0xcd);
write_cmos_sensor_16_8(0x5abf,0xcd);
write_cmos_sensor_16_8(0x5ac0,0xcd);
write_cmos_sensor_16_8(0x5ac1,0xcd);
write_cmos_sensor_16_8(0x5ac2,0xcd);
write_cmos_sensor_16_8(0x5ac3,0xcd);
write_cmos_sensor_16_8(0x5ac4,0xcd);
write_cmos_sensor_16_8(0x5ac5,0xcd);
write_cmos_sensor_16_8(0x5ac6,0xcd);
write_cmos_sensor_16_8(0x5ac7,0xcd);
write_cmos_sensor_16_8(0x5ac8,0xcd);
write_cmos_sensor_16_8(0x5ac9,0xcd);
write_cmos_sensor_16_8(0x5aca,0xcd);
write_cmos_sensor_16_8(0x5acb,0xcd);
write_cmos_sensor_16_8(0x5acc,0xcd);
write_cmos_sensor_16_8(0x5acd,0xcd);
write_cmos_sensor_16_8(0x5ace,0xcd);
write_cmos_sensor_16_8(0x5acf,0xcd);
write_cmos_sensor_16_8(0x5ad0,0xcd);
write_cmos_sensor_16_8(0x5ad1,0xcd);
write_cmos_sensor_16_8(0x5ad2,0xcd);
write_cmos_sensor_16_8(0x5ad3,0xcd);
write_cmos_sensor_16_8(0x5ad4,0xcd);
write_cmos_sensor_16_8(0x5ad5,0xcd);
write_cmos_sensor_16_8(0x5ad6,0xcd);
write_cmos_sensor_16_8(0x5ad7,0xcd);
write_cmos_sensor_16_8(0x5ad8,0xcd);
write_cmos_sensor_16_8(0x5ad9,0xcd);
write_cmos_sensor_16_8(0x5ada,0xcd);
write_cmos_sensor_16_8(0x5adb,0xcd);
write_cmos_sensor_16_8(0x5adc,0xcd);
write_cmos_sensor_16_8(0x5add,0xcd);
write_cmos_sensor_16_8(0x5ade,0xcd);
write_cmos_sensor_16_8(0x5adf,0xcd);
write_cmos_sensor_16_8(0x5ae0,0xcd);
write_cmos_sensor_16_8(0x5ae1,0xcd);
write_cmos_sensor_16_8(0x5ae2,0xcd);
write_cmos_sensor_16_8(0x5ae3,0xcd);
write_cmos_sensor_16_8(0x5ae4,0xcd);
write_cmos_sensor_16_8(0x5ae5,0xcd);
write_cmos_sensor_16_8(0x5ae6,0xcd);
write_cmos_sensor_16_8(0x5ae7,0xcd);
write_cmos_sensor_16_8(0x5ae8,0xcd);
write_cmos_sensor_16_8(0x5ae9,0xcd);
write_cmos_sensor_16_8(0x5aea,0xcd);
write_cmos_sensor_16_8(0x5aeb,0xcd);
write_cmos_sensor_16_8(0x5aec,0xcd);
write_cmos_sensor_16_8(0x5aed,0xcd);
write_cmos_sensor_16_8(0x5aee,0xcd);
write_cmos_sensor_16_8(0x5aef,0xcd);
write_cmos_sensor_16_8(0x5af0,0xcd);
write_cmos_sensor_16_8(0x5af1,0xcd);
write_cmos_sensor_16_8(0x5af2,0xcd);
write_cmos_sensor_16_8(0x5af3,0xcd);
write_cmos_sensor_16_8(0x5af4,0xcd);
write_cmos_sensor_16_8(0x5af5,0xcd);
write_cmos_sensor_16_8(0x5af6,0xcd);
write_cmos_sensor_16_8(0x5af7,0xcd);
write_cmos_sensor_16_8(0x5af8,0xcd);
write_cmos_sensor_16_8(0x5af9,0xcd);
write_cmos_sensor_16_8(0x5afa,0xcd);
write_cmos_sensor_16_8(0x5afb,0xcd);
write_cmos_sensor_16_8(0x5afc,0xcd);
write_cmos_sensor_16_8(0x5afd,0xcd);
write_cmos_sensor_16_8(0x5afe,0xcd);
write_cmos_sensor_16_8(0x5aff,0xcd);
write_cmos_sensor_16_8(0x5b00,0xcd);
write_cmos_sensor_16_8(0x5b01,0xcd);
write_cmos_sensor_16_8(0x5b02,0xcd);
write_cmos_sensor_16_8(0x5b03,0xcd);
write_cmos_sensor_16_8(0x5b04,0xcd);
write_cmos_sensor_16_8(0x5b05,0xcd);
write_cmos_sensor_16_8(0x5b06,0xcd);
write_cmos_sensor_16_8(0x5b07,0xcd);
write_cmos_sensor_16_8(0x5b08,0xcd);
write_cmos_sensor_16_8(0x5b09,0xcd);
write_cmos_sensor_16_8(0x5b0a,0xcd);
write_cmos_sensor_16_8(0x5b0b,0xcd);
write_cmos_sensor_16_8(0x5b0c,0xcd);
write_cmos_sensor_16_8(0x5b0d,0xcd);
write_cmos_sensor_16_8(0x5b0e,0xcd);
write_cmos_sensor_16_8(0x5b0f,0xcd);
write_cmos_sensor_16_8(0x5b10,0xcd);
write_cmos_sensor_16_8(0x5b11,0xcd);
write_cmos_sensor_16_8(0x5b12,0xcd);
write_cmos_sensor_16_8(0x5b13,0xcd);
write_cmos_sensor_16_8(0x5b14,0xcd);
write_cmos_sensor_16_8(0x5b15,0xcd);
write_cmos_sensor_16_8(0x5b16,0xcd);
write_cmos_sensor_16_8(0x5b17,0xcd);
write_cmos_sensor_16_8(0x5b18,0xcd);
write_cmos_sensor_16_8(0x5b19,0xcd);
write_cmos_sensor_16_8(0x5b1a,0xcd);
write_cmos_sensor_16_8(0x5b1b,0xcd);
write_cmos_sensor_16_8(0x5b1c,0xcd);
write_cmos_sensor_16_8(0x5b1d,0xcd);
write_cmos_sensor_16_8(0x5b1e,0xcd);
write_cmos_sensor_16_8(0x5b1f,0xcd);
write_cmos_sensor_16_8(0x5b20,0xcd);
write_cmos_sensor_16_8(0x5b21,0xcd);
write_cmos_sensor_16_8(0x5b22,0xcd);
write_cmos_sensor_16_8(0x5b23,0xcd);
write_cmos_sensor_16_8(0x5b24,0xcd);
write_cmos_sensor_16_8(0x5b25,0xcd);
write_cmos_sensor_16_8(0x5b26,0xcd);
write_cmos_sensor_16_8(0x5b27,0xcd);
write_cmos_sensor_16_8(0x5b28,0xcd);
write_cmos_sensor_16_8(0x5b29,0xcd);
write_cmos_sensor_16_8(0x5b2a,0xcd);
write_cmos_sensor_16_8(0x5b2b,0xcd);
write_cmos_sensor_16_8(0x5b2c,0xcd);
write_cmos_sensor_16_8(0x5b2d,0xcd);
write_cmos_sensor_16_8(0x5b2e,0xcd);
write_cmos_sensor_16_8(0x5b2f,0xcd);
write_cmos_sensor_16_8(0x5b30,0xcd);
write_cmos_sensor_16_8(0x5b31,0xcd);
write_cmos_sensor_16_8(0x5b32,0xcd);
write_cmos_sensor_16_8(0x5b33,0xcd);
write_cmos_sensor_16_8(0x5b34,0xcd);
write_cmos_sensor_16_8(0x5b35,0xcd);
write_cmos_sensor_16_8(0x5b36,0xcd);
write_cmos_sensor_16_8(0x5b37,0xcd);
write_cmos_sensor_16_8(0x5b38,0xcd);
write_cmos_sensor_16_8(0x5b39,0xcd);
write_cmos_sensor_16_8(0x5b3a,0xcd);
write_cmos_sensor_16_8(0x5b3b,0xcd);
write_cmos_sensor_16_8(0x5b3c,0xcd);
write_cmos_sensor_16_8(0x5b3d,0xcd);
write_cmos_sensor_16_8(0x5b3e,0xcd);
write_cmos_sensor_16_8(0x5b3f,0xcd);
write_cmos_sensor_16_8(0x5b40,0xcd);
write_cmos_sensor_16_8(0x5b41,0xcd);
write_cmos_sensor_16_8(0x5b42,0xcd);
write_cmos_sensor_16_8(0x5b43,0xcd);
write_cmos_sensor_16_8(0x5b44,0xcd);
write_cmos_sensor_16_8(0x5b45,0xcd);
write_cmos_sensor_16_8(0x5b46,0xcd);
write_cmos_sensor_16_8(0x5b47,0xcd);
write_cmos_sensor_16_8(0x5b48,0xcd);
write_cmos_sensor_16_8(0x5b49,0xcd);
write_cmos_sensor_16_8(0x5b4a,0xcd);
write_cmos_sensor_16_8(0x5b4b,0xcd);
write_cmos_sensor_16_8(0x5b4c,0xcd);
write_cmos_sensor_16_8(0x5b4d,0xcd);
write_cmos_sensor_16_8(0x5b4e,0xcd);
write_cmos_sensor_16_8(0x5b4f,0xcd);
write_cmos_sensor_16_8(0x5b50,0xcd);
write_cmos_sensor_16_8(0x5b51,0xcd);
write_cmos_sensor_16_8(0x5b52,0xcd);
write_cmos_sensor_16_8(0x5b53,0xcd);
write_cmos_sensor_16_8(0x5b54,0xcd);
write_cmos_sensor_16_8(0x5b55,0xcd);
write_cmos_sensor_16_8(0x5b56,0xcd);
write_cmos_sensor_16_8(0x5b57,0xcd);
write_cmos_sensor_16_8(0x5b58,0xcd);
write_cmos_sensor_16_8(0x5b59,0xcd);
write_cmos_sensor_16_8(0x5b5a,0xcd);
write_cmos_sensor_16_8(0x5b5b,0xcd);
write_cmos_sensor_16_8(0x5b5c,0xcd);
write_cmos_sensor_16_8(0x5b5d,0xcd);
write_cmos_sensor_16_8(0x5b5e,0xcd);
write_cmos_sensor_16_8(0x5b5f,0xcd);
write_cmos_sensor_16_8(0x5b60,0xcd);
write_cmos_sensor_16_8(0x5b61,0xcd);
write_cmos_sensor_16_8(0x5b62,0xcd);
write_cmos_sensor_16_8(0x5b63,0xcd);
write_cmos_sensor_16_8(0x5b64,0xcd);
write_cmos_sensor_16_8(0x5b65,0xcd);
write_cmos_sensor_16_8(0x5b66,0xcd);
write_cmos_sensor_16_8(0x5b67,0xcd);
write_cmos_sensor_16_8(0x5b68,0xcd);
write_cmos_sensor_16_8(0x5b69,0xcd);
write_cmos_sensor_16_8(0x5b6a,0xcd);
write_cmos_sensor_16_8(0x5b6b,0xcd);
write_cmos_sensor_16_8(0x5b6c,0xcd);
write_cmos_sensor_16_8(0x5b6d,0xcd);
write_cmos_sensor_16_8(0x5b6e,0xcd);
write_cmos_sensor_16_8(0x5b6f,0xcd);
write_cmos_sensor_16_8(0x5b70,0xcd);
write_cmos_sensor_16_8(0x5b71,0xcd);
write_cmos_sensor_16_8(0x5b72,0xcd);
write_cmos_sensor_16_8(0x5b73,0xcd);
write_cmos_sensor_16_8(0x5b74,0xcd);
write_cmos_sensor_16_8(0x5b75,0xcd);
write_cmos_sensor_16_8(0x5b76,0xcd);
write_cmos_sensor_16_8(0x5b77,0xcd);
write_cmos_sensor_16_8(0x5b78,0xcd);
write_cmos_sensor_16_8(0x5b79,0xcd);
write_cmos_sensor_16_8(0x5b7a,0xcd);
write_cmos_sensor_16_8(0x5b7b,0xcd);
write_cmos_sensor_16_8(0x5b7c,0xcd);
write_cmos_sensor_16_8(0x5b7d,0xcd);
write_cmos_sensor_16_8(0x5b7e,0xcd);
write_cmos_sensor_16_8(0x5b7f,0xcd);
write_cmos_sensor_16_8(0x5b80,0xcd);
write_cmos_sensor_16_8(0x5b81,0xcd);
write_cmos_sensor_16_8(0x5b82,0xcd);
write_cmos_sensor_16_8(0x5b83,0xcd);
write_cmos_sensor_16_8(0x5b84,0xcd);
write_cmos_sensor_16_8(0x5b85,0xcd);
write_cmos_sensor_16_8(0x5b86,0xcd);
write_cmos_sensor_16_8(0x5b87,0xcd);
write_cmos_sensor_16_8(0x5b88,0xcd);
write_cmos_sensor_16_8(0x5b89,0xcd);
write_cmos_sensor_16_8(0x5b8a,0xcd);
write_cmos_sensor_16_8(0x5b8b,0xcd);
write_cmos_sensor_16_8(0x5b8c,0xcd);
write_cmos_sensor_16_8(0x5b8d,0xcd);
write_cmos_sensor_16_8(0x5b8e,0xcd);
write_cmos_sensor_16_8(0x5b8f,0xcd);
write_cmos_sensor_16_8(0x5b90,0xcd);
write_cmos_sensor_16_8(0x5b91,0xcd);
write_cmos_sensor_16_8(0x5b92,0xcd);
write_cmos_sensor_16_8(0x5b93,0xcd);
write_cmos_sensor_16_8(0x5b94,0xcd);
write_cmos_sensor_16_8(0x5b95,0xcd);
write_cmos_sensor_16_8(0x5b96,0xcd);
write_cmos_sensor_16_8(0x5b97,0xcd);
write_cmos_sensor_16_8(0x5b98,0xcd);
write_cmos_sensor_16_8(0x5b99,0xcd);
write_cmos_sensor_16_8(0x5b9a,0xcd);
write_cmos_sensor_16_8(0x5b9b,0xcd);
write_cmos_sensor_16_8(0x5b9c,0xcd);
write_cmos_sensor_16_8(0x5b9d,0xcd);
write_cmos_sensor_16_8(0x5b9e,0xcd);
write_cmos_sensor_16_8(0x5b9f,0xcd);
write_cmos_sensor_16_8(0x5ba0,0xcd);
write_cmos_sensor_16_8(0x5ba1,0xcd);
write_cmos_sensor_16_8(0x5ba2,0xcd);
write_cmos_sensor_16_8(0x5ba3,0xcd);
write_cmos_sensor_16_8(0x5ba4,0xcd);
write_cmos_sensor_16_8(0x5ba5,0xcd);
write_cmos_sensor_16_8(0x5ba6,0xcd);
write_cmos_sensor_16_8(0x5ba7,0xcd);
write_cmos_sensor_16_8(0x5ba8,0xcd);
write_cmos_sensor_16_8(0x5ba9,0xcd);
write_cmos_sensor_16_8(0x5baa,0xcd);
write_cmos_sensor_16_8(0x5bab,0xcd);
write_cmos_sensor_16_8(0x5bac,0xcd);
write_cmos_sensor_16_8(0x5bad,0xcd);
write_cmos_sensor_16_8(0x5bae,0xcd);
write_cmos_sensor_16_8(0x5baf,0xcd);
write_cmos_sensor_16_8(0x5bb0,0xcd);
write_cmos_sensor_16_8(0x5bb1,0xcd);
write_cmos_sensor_16_8(0x5bb2,0xcd);
write_cmos_sensor_16_8(0x5bb3,0xcd);
write_cmos_sensor_16_8(0x5bb4,0xcd);
write_cmos_sensor_16_8(0x5bb5,0xcd);
write_cmos_sensor_16_8(0x5bb6,0xcd);
write_cmos_sensor_16_8(0x5bb7,0xcd);
write_cmos_sensor_16_8(0x5bb8,0xcd);
write_cmos_sensor_16_8(0x5bb9,0xcd);
write_cmos_sensor_16_8(0x5bba,0xcd);
write_cmos_sensor_16_8(0x5bbb,0xcd);
write_cmos_sensor_16_8(0x5bbc,0xcd);
write_cmos_sensor_16_8(0x5bbd,0xcd);
write_cmos_sensor_16_8(0x5bbe,0xcd);
write_cmos_sensor_16_8(0x5bbf,0xcd);
write_cmos_sensor_16_8(0x5bc0,0xcd);
write_cmos_sensor_16_8(0x5bc1,0xcd);
write_cmos_sensor_16_8(0x5bc2,0xcd);
write_cmos_sensor_16_8(0x5bc3,0xcd);
write_cmos_sensor_16_8(0x5bc4,0xcd);
write_cmos_sensor_16_8(0x5bc5,0xcd);
write_cmos_sensor_16_8(0x5bc6,0xcd);
write_cmos_sensor_16_8(0x5bc7,0xcd);
write_cmos_sensor_16_8(0x5bc8,0xcd);
write_cmos_sensor_16_8(0x5bc9,0xcd);
write_cmos_sensor_16_8(0x5bca,0xcd);
write_cmos_sensor_16_8(0x5bcb,0xcd);
write_cmos_sensor_16_8(0x5bcc,0xcd);
write_cmos_sensor_16_8(0x5bcd,0xcd);
write_cmos_sensor_16_8(0x5bce,0xcd);
write_cmos_sensor_16_8(0x5bcf,0xcd);
write_cmos_sensor_16_8(0x5c00,0x49);
write_cmos_sensor_16_8(0x5c01,0x00);
write_cmos_sensor_16_8(0x5c02,0x00);
write_cmos_sensor_16_8(0x5c03,0x82);
write_cmos_sensor_16_8(0x5c16,0x00);
write_cmos_sensor_16_8(0x5c17,0x50);
write_cmos_sensor_16_8(0x5c18,0x00);
write_cmos_sensor_16_8(0x5c19,0x50);
write_cmos_sensor_16_8(0x5c1a,0x00);
write_cmos_sensor_16_8(0x5c1b,0x50);
write_cmos_sensor_16_8(0x5c1c,0x00);
write_cmos_sensor_16_8(0x5c1d,0x50);
write_cmos_sensor_16_8(0x5c1e,0x00);
write_cmos_sensor_16_8(0x5c1f,0x50);
write_cmos_sensor_16_8(0x5c20,0x00);
write_cmos_sensor_16_8(0x5c21,0x50);
write_cmos_sensor_16_8(0x5c22,0x00);
write_cmos_sensor_16_8(0x5c23,0x50);
write_cmos_sensor_16_8(0x5c24,0x00);
write_cmos_sensor_16_8(0x5c25,0x50);
write_cmos_sensor_16_8(0x5c26,0x00);
write_cmos_sensor_16_8(0x5c27,0x50);
write_cmos_sensor_16_8(0x5c28,0x00);
write_cmos_sensor_16_8(0x5c29,0x50);
write_cmos_sensor_16_8(0x5c2a,0x00);
write_cmos_sensor_16_8(0x5c2b,0x50);
write_cmos_sensor_16_8(0x5c2c,0x00);
write_cmos_sensor_16_8(0x5c2d,0x50);
write_cmos_sensor_16_8(0x5c2e,0x02);
write_cmos_sensor_16_8(0x5c2f,0x03);
write_cmos_sensor_16_8(0x5c36,0x0a);
write_cmos_sensor_16_8(0x5c37,0x0b);
write_cmos_sensor_16_8(0x5c44,0x02);
write_cmos_sensor_16_8(0x5c45,0x0a);
write_cmos_sensor_16_8(0x5c46,0x00);
write_cmos_sensor_16_8(0x5c47,0x07);
write_cmos_sensor_16_8(0x5c48,0x0f);
write_cmos_sensor_16_8(0x5c49,0x01);
write_cmos_sensor_16_8(0x5c4a,0x00);
write_cmos_sensor_16_8(0x5c4b,0x09);
write_cmos_sensor_16_8(0x5c4c,0x06);
write_cmos_sensor_16_8(0x5c4d,0x0e);
write_cmos_sensor_16_8(0x5c4e,0x00);
write_cmos_sensor_16_8(0x5c4f,0x00);
write_cmos_sensor_16_8(0x5c50,0x00);
write_cmos_sensor_16_8(0x5c51,0x00);
write_cmos_sensor_16_8(0x5c52,0x00);
write_cmos_sensor_16_8(0x5c53,0x00);
write_cmos_sensor_16_8(0x5c54,0x08);
write_cmos_sensor_16_8(0x5c55,0x08);
write_cmos_sensor_16_8(0x5c56,0x02);
write_cmos_sensor_16_8(0x5c57,0x06);
write_cmos_sensor_16_8(0x5c58,0x02);
write_cmos_sensor_16_8(0x5c59,0x06);
write_cmos_sensor_16_8(0x5c5a,0x02);
write_cmos_sensor_16_8(0x5c5b,0x06);
write_cmos_sensor_16_8(0x5c5c,0x02);
write_cmos_sensor_16_8(0x5c5d,0x06);
write_cmos_sensor_16_8(0x5c5e,0x00);
write_cmos_sensor_16_8(0x5c5f,0x00);
write_cmos_sensor_16_8(0x5c68,0x02);
write_cmos_sensor_16_8(0x5c69,0x03);
write_cmos_sensor_16_8(0x5c6a,0x03);
write_cmos_sensor_16_8(0x5c6b,0x02);
write_cmos_sensor_16_8(0x5c6c,0x40);
write_cmos_sensor_16_8(0x5c6d,0x01);
write_cmos_sensor_16_8(0x5d00,0x66);
write_cmos_sensor_16_8(0x5d01,0x08);
write_cmos_sensor_16_8(0x5d02,0x84);
write_cmos_sensor_16_8(0x5d03,0x04);
write_cmos_sensor_16_8(0x5d05,0x02);
write_cmos_sensor_16_8(0x5d08,0x08);
write_cmos_sensor_16_8(0x5d09,0x08);
write_cmos_sensor_16_8(0x5d0a,0x02);
write_cmos_sensor_16_8(0x5d0b,0x06);
write_cmos_sensor_16_8(0x5d0c,0x02);
write_cmos_sensor_16_8(0x5d0d,0x06);
write_cmos_sensor_16_8(0x5d0e,0x02);
write_cmos_sensor_16_8(0x5d0f,0x06);
write_cmos_sensor_16_8(0x5d10,0x02);
write_cmos_sensor_16_8(0x5d11,0x06);
write_cmos_sensor_16_8(0x5d12,0x00);
write_cmos_sensor_16_8(0x5d13,0x00);
write_cmos_sensor_16_8(0x5d14,0xff);
write_cmos_sensor_16_8(0x5d15,0x10);
write_cmos_sensor_16_8(0x5d16,0x10);
write_cmos_sensor_16_8(0x5d17,0x10);
write_cmos_sensor_16_8(0x5d18,0x10);
write_cmos_sensor_16_8(0x5d19,0xff);
write_cmos_sensor_16_8(0x5d1a,0x10);
write_cmos_sensor_16_8(0x5d1b,0x10);
write_cmos_sensor_16_8(0x5d1c,0x10);
write_cmos_sensor_16_8(0x5d1d,0x10);
write_cmos_sensor_16_8(0x5d1e,0x04);
write_cmos_sensor_16_8(0x5d1f,0x04);
write_cmos_sensor_16_8(0x5d20,0x04);
write_cmos_sensor_16_8(0x5d21,0xff);
write_cmos_sensor_16_8(0x5d27,0x64);
write_cmos_sensor_16_8(0x5d28,0xc8);
write_cmos_sensor_16_8(0x5d29,0x96);
write_cmos_sensor_16_8(0x5d2a,0xff);
write_cmos_sensor_16_8(0x5d2b,0xc8);
write_cmos_sensor_16_8(0x5d2c,0xff);
write_cmos_sensor_16_8(0x5d2d,0x04);
write_cmos_sensor_16_8(0x5d37,0x08);
write_cmos_sensor_16_8(0x5d38,0x1f);
write_cmos_sensor_16_8(0x5d39,0x60);
write_cmos_sensor_16_8(0x5d3a,0x17);
write_cmos_sensor_16_8(0x5d3b,0x80);
write_cmos_sensor_16_8(0x5d80,0x21);
write_cmos_sensor_16_8(0x5d85,0x19);
write_cmos_sensor_16_8(0x5d88,0x04);
write_cmos_sensor_16_8(0x5d89,0x0c);
write_cmos_sensor_16_8(0x5d8a,0x04);
write_cmos_sensor_16_8(0x5d8b,0x0c);
write_cmos_sensor_16_8(0x5d8c,0x04);
write_cmos_sensor_16_8(0x5d8d,0x0c);
write_cmos_sensor_16_8(0x5d8e,0x04);
write_cmos_sensor_16_8(0x5d8f,0x0c);
write_cmos_sensor_16_8(0x5d90,0x01);
write_cmos_sensor_16_8(0x5d91,0x01);
write_cmos_sensor_16_8(0x5db0,0x0f);
write_cmos_sensor_16_8(0x5ec1,0x08);
write_cmos_sensor_16_8(0x5ec2,0x04);
write_cmos_sensor_16_8(0x5ec3,0x0c);
write_cmos_sensor_16_8(0x5ec4,0x04);
write_cmos_sensor_16_8(0x5ec5,0x0c);
write_cmos_sensor_16_8(0x5ec6,0x02);
write_cmos_sensor_16_8(0x5ec7,0x06);
write_cmos_sensor_16_8(0x5ec8,0x02);
write_cmos_sensor_16_8(0x5ec9,0x06);
write_cmos_sensor_16_8(0x5eca,0x01);
write_cmos_sensor_16_8(0x5ecb,0x00);
write_cmos_sensor_16_8(0x5ece,0x00);
write_cmos_sensor_16_8(0x5ecf,0x04);
write_cmos_sensor_16_8(0x5e40,0x03);
write_cmos_sensor_16_8(0x5ea0,0x11);
write_cmos_sensor_16_8(0x0100,0x01);
write_cmos_sensor_16_8(0x0100,0x00);
write_cmos_sensor_16_8(0x0345,0x22);
write_cmos_sensor_16_8(0x0350,0x01);
write_cmos_sensor_16_8(0x3501,0x0b);
write_cmos_sensor_16_8(0x3502,0x68);
write_cmos_sensor_16_8(0x3608,0x35);
write_cmos_sensor_16_8(0x3622,0x11);
write_cmos_sensor_16_8(0x3624,0x18);
write_cmos_sensor_16_8(0x3639,0xc0);
write_cmos_sensor_16_8(0x363b,0x0b);
write_cmos_sensor_16_8(0x363e,0x47);
write_cmos_sensor_16_8(0x363d,0x25);
write_cmos_sensor_16_8(0x3684,0x07);
write_cmos_sensor_16_8(0x3689,0x27);
write_cmos_sensor_16_8(0x368e,0x0f);
write_cmos_sensor_16_8(0x3703,0x35);
write_cmos_sensor_16_8(0x3706,0x2a);
write_cmos_sensor_16_8(0x3709,0x7a);
write_cmos_sensor_16_8(0x370b,0x62);
write_cmos_sensor_16_8(0x3712,0x50);
write_cmos_sensor_16_8(0x3714,0x63);
write_cmos_sensor_16_8(0x3729,0x01);
write_cmos_sensor_16_8(0x373b,0x20);
write_cmos_sensor_16_8(0x373d,0x26);
write_cmos_sensor_16_8(0x375d,0x02);
write_cmos_sensor_16_8(0x375e,0x02);
write_cmos_sensor_16_8(0x37b2,0x30);
write_cmos_sensor_16_8(0x37b5,0x36);
write_cmos_sensor_16_8(0x37b7,0x00);
write_cmos_sensor_16_8(0x37b8,0x00);
write_cmos_sensor_16_8(0x37ba,0x00);
write_cmos_sensor_16_8(0x37bc,0x00);
write_cmos_sensor_16_8(0x3800,0x00);
write_cmos_sensor_16_8(0x3801,0x00);
write_cmos_sensor_16_8(0x3802,0x00);
write_cmos_sensor_16_8(0x3803,0x00);
write_cmos_sensor_16_8(0x3804,0x1f);
write_cmos_sensor_16_8(0x3805,0x5f);
write_cmos_sensor_16_8(0x3806,0x17);
write_cmos_sensor_16_8(0x3807,0x8f);
write_cmos_sensor_16_8(0x3808,0x0f);
write_cmos_sensor_16_8(0x3809,0xa0);
write_cmos_sensor_16_8(0x380a,0x0b);
write_cmos_sensor_16_8(0x380b,0xb8);
write_cmos_sensor_16_8(0x380c,0x04);
write_cmos_sensor_16_8(0x380d,0x50);
write_cmos_sensor_16_8(0x380e,0x0d);
write_cmos_sensor_16_8(0x380f,0x96);
write_cmos_sensor_16_8(0x3811,0x09);
write_cmos_sensor_16_8(0x3813,0x08);
write_cmos_sensor_16_8(0x3814,0x11);
write_cmos_sensor_16_8(0x3815,0x11);
write_cmos_sensor_16_8(0x381a,0x1b);
write_cmos_sensor_16_8(0x381b,0x1c);
write_cmos_sensor_16_8(0x381c,0x01);
write_cmos_sensor_16_8(0x381d,0x14);
write_cmos_sensor_16_8(0x3820,0x02);
write_cmos_sensor_16_8(0x3821,0x06);
write_cmos_sensor_16_8(0x3822,0x10);
write_cmos_sensor_16_8(0x3824,0x01);
write_cmos_sensor_16_8(0x3825,0x14);
write_cmos_sensor_16_8(0x3826,0x1b);
write_cmos_sensor_16_8(0x3827,0x1c);
write_cmos_sensor_16_8(0x3837,0x18);
write_cmos_sensor_16_8(0x383d,0x09);
write_cmos_sensor_16_8(0x3857,0x04);
write_cmos_sensor_16_8(0x3859,0x08);
write_cmos_sensor_16_8(0x4002,0x13);
write_cmos_sensor_16_8(0x4016,0x0f);
write_cmos_sensor_16_8(0x4018,0x03);
write_cmos_sensor_16_8(0x4510,0x00);
write_cmos_sensor_16_8(0x4600,0x00);
write_cmos_sensor_16_8(0x4601,0xc8);
write_cmos_sensor_16_8(0x4643,0x0c);
write_cmos_sensor_16_8(0x480e,0x04);
write_cmos_sensor_16_8(0x484b,0x27);
write_cmos_sensor_16_8(0x5000,0x8f);
write_cmos_sensor_16_8(0x5001,0x23);
write_cmos_sensor_16_8(0x5002,0x9e);
write_cmos_sensor_16_8(0x5185,0x0b);
write_cmos_sensor_16_8(0x5187,0x00);
write_cmos_sensor_16_8(0x5198,0x08);
write_cmos_sensor_16_8(0x5300,0x79);
write_cmos_sensor_16_8(0x533d,0x00);
write_cmos_sensor_16_8(0x5c00,0x4b);
write_cmos_sensor_16_8(0x5c03,0x82);
write_cmos_sensor_16_8(0x5c44,0x02);
write_cmos_sensor_16_8(0x5c45,0x0a);
write_cmos_sensor_16_8(0x5c46,0x00);
write_cmos_sensor_16_8(0x5c47,0x07);
write_cmos_sensor_16_8(0x5c48,0x0f);
write_cmos_sensor_16_8(0x5c49,0x01);
write_cmos_sensor_16_8(0x5c4a,0x00);
write_cmos_sensor_16_8(0x5c4c,0x06);
write_cmos_sensor_16_8(0x5c4d,0x0e);
write_cmos_sensor_16_8(0x5c4e,0x00);
write_cmos_sensor_16_8(0x5c4f,0x00);
write_cmos_sensor_16_8(0x5c50,0x00);
write_cmos_sensor_16_8(0x5c51,0x00);
write_cmos_sensor_16_8(0x5c52,0x00);
write_cmos_sensor_16_8(0x5c53,0x00);
write_cmos_sensor_16_8(0x5c54,0x08);
write_cmos_sensor_16_8(0x5c55,0x08);
write_cmos_sensor_16_8(0x5c56,0x02);
write_cmos_sensor_16_8(0x5c57,0x06);
write_cmos_sensor_16_8(0x5c58,0x02);
write_cmos_sensor_16_8(0x5c59,0x06);
write_cmos_sensor_16_8(0x5c5a,0x02);
write_cmos_sensor_16_8(0x5c5b,0x06);
write_cmos_sensor_16_8(0x5c5c,0x02);
write_cmos_sensor_16_8(0x5c5d,0x06);
write_cmos_sensor_16_8(0x5c5e,0x00);
write_cmos_sensor_16_8(0x5c5f,0x00);
write_cmos_sensor_16_8(0x5c6d,0x01);
write_cmos_sensor_16_8(0x5d00,0x66);
write_cmos_sensor_16_8(0x3046,0x01);
write_cmos_sensor_16_8(0x0100,0x01);
write_cmos_sensor_16_8(0x0100,0x00);
/*	
write_cmos_sensor_16_8(0x0304,0x00);
write_cmos_sensor_16_8(0x0305,0x7c);
write_cmos_sensor_16_8(0x380c,0x0c);
write_cmos_sensor_16_8(0x380d,0xf0);
*/
write_cmos_sensor_16_8(0x4837,0x08);
write_cmos_sensor_16_8(0x4850,0x40);
write_cmos_sensor_16_8(0x3684,0x02);
write_cmos_sensor_16_8(0x3689,0x07);
write_cmos_sensor_16_8(0x480e,0x00);
write_cmos_sensor_16_8(0x484b,0x07);
write_cmos_sensor_16_8(0x0100,0x01);
}				/*    sensor_init  */

static void preview_setting(void)
{
	LOG_INF("2304x1728_30fps E\n");
/*		write_cmos_sensor_16_8(0x0345, 0x04);
		write_cmos_sensor_16_8(0x0350, 0x00);
		write_cmos_sensor_16_8(0x3501, 0x0b);
		write_cmos_sensor_16_8(0x3502, 0x68);
		write_cmos_sensor_16_8(0x3608, 0x35);
		write_cmos_sensor_16_8(0x3622, 0x10);
		write_cmos_sensor_16_8(0x3624, 0x18);
		write_cmos_sensor_16_8(0x3639, 0xC0);
		write_cmos_sensor_16_8(0x363b, 0x0B);
		write_cmos_sensor_16_8(0x363e, 0x47);
		write_cmos_sensor_16_8(0x363d, 0x25);
		write_cmos_sensor_16_8(0x3684, 0x02);
		write_cmos_sensor_16_8(0x3689, 0x07);
		write_cmos_sensor_16_8(0x368e, 0x00);
		write_cmos_sensor_16_8(0x3703, 0x3a);
		write_cmos_sensor_16_8(0x3706, 0x2A);
		write_cmos_sensor_16_8(0x3709, 0x9a);
		write_cmos_sensor_16_8(0x370b, 0x6A);
		write_cmos_sensor_16_8(0x3712, 0x50);
		write_cmos_sensor_16_8(0x3714, 0x61);
		write_cmos_sensor_16_8(0x3729, 0x00);
		write_cmos_sensor_16_8(0x373b, 0x20);
		write_cmos_sensor_16_8(0x373d, 0x26);
		write_cmos_sensor_16_8(0x375d, 0x02);
		write_cmos_sensor_16_8(0x375e, 0x02);
		write_cmos_sensor_16_8(0x37b2, 0x30);
		write_cmos_sensor_16_8(0x37b5, 0x36);
		write_cmos_sensor_16_8(0x37b7, 0x00);
		write_cmos_sensor_16_8(0x37b8, 0x00);
		write_cmos_sensor_16_8(0x37ba, 0x00);
		write_cmos_sensor_16_8(0x37bc, 0x00);
		write_cmos_sensor_16_8(0x3800, 0x00);
		write_cmos_sensor_16_8(0x3801, 0x00);
		write_cmos_sensor_16_8(0x3802, 0x00);
		write_cmos_sensor_16_8(0x3803, 0x00);
		write_cmos_sensor_16_8(0x3804, 0x1F);
		write_cmos_sensor_16_8(0x3805, 0x5F);
		write_cmos_sensor_16_8(0x3806, 0x17);
		write_cmos_sensor_16_8(0x3807, 0x8f);
		write_cmos_sensor_16_8(0x3808, 0x0F);
		write_cmos_sensor_16_8(0x3809, 0xA0);
		write_cmos_sensor_16_8(0x380a, 0x0B);
		write_cmos_sensor_16_8(0x380b, 0xB8);
		write_cmos_sensor_16_8(0x380c, 0x02);
		write_cmos_sensor_16_8(0x380d, 0x40);
		write_cmos_sensor_16_8(0x380e, 0x0c);
		write_cmos_sensor_16_8(0x380f, 0x1c);
		write_cmos_sensor_16_8(0x3811, 0x09);
		write_cmos_sensor_16_8(0x3813, 0x08);
		write_cmos_sensor_16_8(0x3814, 0x11);
		write_cmos_sensor_16_8(0x3815, 0x11);
		write_cmos_sensor_16_8(0x381a, 0x0c);
		write_cmos_sensor_16_8(0x381b, 0x14);
		write_cmos_sensor_16_8(0x381c, 0x01);
		write_cmos_sensor_16_8(0x381d, 0x80);
		write_cmos_sensor_16_8(0x3820, 0x02);
		write_cmos_sensor_16_8(0x3821, 0x14);
		write_cmos_sensor_16_8(0x3822, 0x00);
		write_cmos_sensor_16_8(0x3824, 0x01);
		write_cmos_sensor_16_8(0x3825, 0x80);
		write_cmos_sensor_16_8(0x3826, 0x0c);
		write_cmos_sensor_16_8(0x3827, 0x14);
		write_cmos_sensor_16_8(0x3837, 0x0b);
		write_cmos_sensor_16_8(0x383d, 0x09);
		write_cmos_sensor_16_8(0x3857, 0x04);
		write_cmos_sensor_16_8(0x3859, 0x08);
		write_cmos_sensor_16_8(0x4002, 0xd3);
		write_cmos_sensor_16_8(0x4016, 0x0F);
		write_cmos_sensor_16_8(0x4018, 0x03);
		write_cmos_sensor_16_8(0x4510, 0x00);
		write_cmos_sensor_16_8(0x4600, 0x00);
		write_cmos_sensor_16_8(0x4601, 0xC8);
		write_cmos_sensor_16_8(0x4643, 0x08);
		write_cmos_sensor_16_8(0x480e, 0x00);
		write_cmos_sensor_16_8(0x484b, 0x07);
		write_cmos_sensor_16_8(0x5000, 0x89);
		write_cmos_sensor_16_8(0x5001, 0x03);
		write_cmos_sensor_16_8(0x5002, 0x9e);
		write_cmos_sensor_16_8(0x5185, 0x0B);
		write_cmos_sensor_16_8(0x5187, 0x00);
		write_cmos_sensor_16_8(0x5198, 0x08);
		write_cmos_sensor_16_8(0x5300, 0x79);
		write_cmos_sensor_16_8(0x533d, 0x00);
		write_cmos_sensor_16_8(0x5c00, 0x49);
		write_cmos_sensor_16_8(0x5c03, 0x82);
		write_cmos_sensor_16_8(0x5c44, 0x02);
		write_cmos_sensor_16_8(0x5c45, 0x0A);
		write_cmos_sensor_16_8(0x5c46, 0x00);
		write_cmos_sensor_16_8(0x5c47, 0x07);
		write_cmos_sensor_16_8(0x5c48, 0x0F);
		write_cmos_sensor_16_8(0x5c49, 0x01);
		write_cmos_sensor_16_8(0x5c4a, 0x00);
		write_cmos_sensor_16_8(0x5c4c, 0x06);
		write_cmos_sensor_16_8(0x5c4d, 0x0E);
		write_cmos_sensor_16_8(0x5c4e, 0x00);
		write_cmos_sensor_16_8(0x5c4f, 0x00);
		write_cmos_sensor_16_8(0x5c50, 0x00);
		write_cmos_sensor_16_8(0x5c51, 0x00);
		write_cmos_sensor_16_8(0x5c52, 0x00);
		write_cmos_sensor_16_8(0x5c53, 0x00);
		write_cmos_sensor_16_8(0x5c54, 0x08);
		write_cmos_sensor_16_8(0x5c55, 0x08);
		write_cmos_sensor_16_8(0x5c56, 0x02);
		write_cmos_sensor_16_8(0x5c57, 0x06);
		write_cmos_sensor_16_8(0x5c58, 0x02);
		write_cmos_sensor_16_8(0x5c59, 0x06);
		write_cmos_sensor_16_8(0x5c5a, 0x02);
		write_cmos_sensor_16_8(0x5c5b, 0x06);
		write_cmos_sensor_16_8(0x5c5c, 0x02);
		write_cmos_sensor_16_8(0x5c5d, 0x06);
		write_cmos_sensor_16_8(0x5c5e, 0x00);
		write_cmos_sensor_16_8(0x5c5f, 0x00);
		write_cmos_sensor_16_8(0x5c6d, 0x01);
		write_cmos_sensor_16_8(0x5d00, 0x66);
		write_cmos_sensor_16_8(0x0304, 0x00);
		write_cmos_sensor_16_8(0x0305, 0xf6);
		write_cmos_sensor_16_8(0x034b, 0x02);
		write_cmos_sensor_16_8(0x380c, 0x04);
		write_cmos_sensor_16_8(0x380d, 0xe0);
		write_cmos_sensor_16_8(0x380e, 0x0c);
		write_cmos_sensor_16_8(0x380f, 0x04);
		write_cmos_sensor_16_8(0x4837, 0x0d);
		write_cmos_sensor_16_8(0x4850, 0x40);*/
		
}				/*    preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! 4608x3456_30fps currefps:%d\n", currefps);
/*		write_cmos_sensor_16_8(0x0345, 0x04);
		write_cmos_sensor_16_8(0x0350, 0x00);
		write_cmos_sensor_16_8(0x3501, 0x0b);
		write_cmos_sensor_16_8(0x3502, 0x68);
		write_cmos_sensor_16_8(0x3608, 0x35);
		write_cmos_sensor_16_8(0x3622, 0x10);
		write_cmos_sensor_16_8(0x3624, 0x18);
		write_cmos_sensor_16_8(0x3639, 0xC0);
		write_cmos_sensor_16_8(0x363b, 0x0B);
		write_cmos_sensor_16_8(0x363e, 0x47);
		write_cmos_sensor_16_8(0x363d, 0x25);
		write_cmos_sensor_16_8(0x3684, 0x02);
		write_cmos_sensor_16_8(0x3689, 0x07);
		write_cmos_sensor_16_8(0x368e, 0x00);
		write_cmos_sensor_16_8(0x3703, 0x3a);
		write_cmos_sensor_16_8(0x3706, 0x2A);
		write_cmos_sensor_16_8(0x3709, 0x9a);
		write_cmos_sensor_16_8(0x370b, 0x6A);
		write_cmos_sensor_16_8(0x3712, 0x50);
		write_cmos_sensor_16_8(0x3714, 0x61);
		write_cmos_sensor_16_8(0x3729, 0x00);
		write_cmos_sensor_16_8(0x373b, 0x20);
		write_cmos_sensor_16_8(0x373d, 0x26);
		write_cmos_sensor_16_8(0x375d, 0x02);
		write_cmos_sensor_16_8(0x375e, 0x02);
		write_cmos_sensor_16_8(0x37b2, 0x30);
		write_cmos_sensor_16_8(0x37b5, 0x36);
		write_cmos_sensor_16_8(0x37b7, 0x00);
		write_cmos_sensor_16_8(0x37b8, 0x00);
		write_cmos_sensor_16_8(0x37ba, 0x00);
		write_cmos_sensor_16_8(0x37bc, 0x00);
		write_cmos_sensor_16_8(0x3800, 0x00);
		write_cmos_sensor_16_8(0x3801, 0x00);
		write_cmos_sensor_16_8(0x3802, 0x00);
		write_cmos_sensor_16_8(0x3803, 0x00);
		write_cmos_sensor_16_8(0x3804, 0x1F);
		write_cmos_sensor_16_8(0x3805, 0x5F);
		write_cmos_sensor_16_8(0x3806, 0x17);
		write_cmos_sensor_16_8(0x3807, 0x8f);
		write_cmos_sensor_16_8(0x3808, 0x0F);
		write_cmos_sensor_16_8(0x3809, 0xA0);
		write_cmos_sensor_16_8(0x380a, 0x0B);
		write_cmos_sensor_16_8(0x380b, 0xB8);
		write_cmos_sensor_16_8(0x380c, 0x02);
		write_cmos_sensor_16_8(0x380d, 0x40);
		write_cmos_sensor_16_8(0x380e, 0x0c);
		write_cmos_sensor_16_8(0x380f, 0x1c);
		write_cmos_sensor_16_8(0x3811, 0x09);
		write_cmos_sensor_16_8(0x3813, 0x08);
		write_cmos_sensor_16_8(0x3814, 0x11);
		write_cmos_sensor_16_8(0x3815, 0x11);
		write_cmos_sensor_16_8(0x381a, 0x0c);
		write_cmos_sensor_16_8(0x381b, 0x14);
		write_cmos_sensor_16_8(0x381c, 0x01);
		write_cmos_sensor_16_8(0x381d, 0x80);
		write_cmos_sensor_16_8(0x3820, 0x02);
		write_cmos_sensor_16_8(0x3821, 0x14);
		write_cmos_sensor_16_8(0x3822, 0x00);
		write_cmos_sensor_16_8(0x3824, 0x01);
		write_cmos_sensor_16_8(0x3825, 0x80);
		write_cmos_sensor_16_8(0x3826, 0x0c);
		write_cmos_sensor_16_8(0x3827, 0x14);
		write_cmos_sensor_16_8(0x3837, 0x0b);
		write_cmos_sensor_16_8(0x383d, 0x09);
		write_cmos_sensor_16_8(0x3857, 0x04);
		write_cmos_sensor_16_8(0x3859, 0x08);
		write_cmos_sensor_16_8(0x4002, 0xd3);
		write_cmos_sensor_16_8(0x4016, 0x0F);
		write_cmos_sensor_16_8(0x4018, 0x03);
		write_cmos_sensor_16_8(0x4510, 0x00);
		write_cmos_sensor_16_8(0x4600, 0x00);
		write_cmos_sensor_16_8(0x4601, 0xC8);
		write_cmos_sensor_16_8(0x4643, 0x08);
		write_cmos_sensor_16_8(0x480e, 0x00);
		write_cmos_sensor_16_8(0x484b, 0x07);
		write_cmos_sensor_16_8(0x5000, 0x89);
		write_cmos_sensor_16_8(0x5001, 0x03);
		write_cmos_sensor_16_8(0x5002, 0x9e);
		write_cmos_sensor_16_8(0x5185, 0x0B);
		write_cmos_sensor_16_8(0x5187, 0x00);
		write_cmos_sensor_16_8(0x5198, 0x08);
		write_cmos_sensor_16_8(0x5300, 0x79);
		write_cmos_sensor_16_8(0x533d, 0x00);
		write_cmos_sensor_16_8(0x5c00, 0x49);
		write_cmos_sensor_16_8(0x5c03, 0x82);
		write_cmos_sensor_16_8(0x5c44, 0x02);
		write_cmos_sensor_16_8(0x5c45, 0x0A);
		write_cmos_sensor_16_8(0x5c46, 0x00);
		write_cmos_sensor_16_8(0x5c47, 0x07);
		write_cmos_sensor_16_8(0x5c48, 0x0F);
		write_cmos_sensor_16_8(0x5c49, 0x01);
		write_cmos_sensor_16_8(0x5c4a, 0x00);
		write_cmos_sensor_16_8(0x5c4c, 0x06);
		write_cmos_sensor_16_8(0x5c4d, 0x0E);
		write_cmos_sensor_16_8(0x5c4e, 0x00);
		write_cmos_sensor_16_8(0x5c4f, 0x00);
		write_cmos_sensor_16_8(0x5c50, 0x00);
		write_cmos_sensor_16_8(0x5c51, 0x00);
		write_cmos_sensor_16_8(0x5c52, 0x00);
		write_cmos_sensor_16_8(0x5c53, 0x00);
		write_cmos_sensor_16_8(0x5c54, 0x08);
		write_cmos_sensor_16_8(0x5c55, 0x08);
		write_cmos_sensor_16_8(0x5c56, 0x02);
		write_cmos_sensor_16_8(0x5c57, 0x06);
		write_cmos_sensor_16_8(0x5c58, 0x02);
		write_cmos_sensor_16_8(0x5c59, 0x06);
		write_cmos_sensor_16_8(0x5c5a, 0x02);
		write_cmos_sensor_16_8(0x5c5b, 0x06);
		write_cmos_sensor_16_8(0x5c5c, 0x02);
		write_cmos_sensor_16_8(0x5c5d, 0x06);
		write_cmos_sensor_16_8(0x5c5e, 0x00);
		write_cmos_sensor_16_8(0x5c5f, 0x00);
		write_cmos_sensor_16_8(0x5c6d, 0x01);
		write_cmos_sensor_16_8(0x5d00, 0x66);
		write_cmos_sensor_16_8(0x0304, 0x00);
		write_cmos_sensor_16_8(0x0305, 0xf6);
		write_cmos_sensor_16_8(0x034b, 0x02);
		write_cmos_sensor_16_8(0x380c, 0x04);
		write_cmos_sensor_16_8(0x380d, 0xe0);
		write_cmos_sensor_16_8(0x380e, 0x0c);
		write_cmos_sensor_16_8(0x380f, 0x04);
		write_cmos_sensor_16_8(0x4837, 0x0d);
		write_cmos_sensor_16_8(0x4850, 0x40);*/

}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! 4608x2592_30fps  currefps:%d\n", currefps);

}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
}

#if 0
static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		//write_cmos_sensor(0x5000, 0xdb);	/* disable lenc and otp_dpc */
		write_cmos_sensor_16_8(0x5081, 0x81);
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		//write_cmos_sensor(0x5000, 0xff);	/* enable otp_dpc */
		write_cmos_sensor_16_8(0x5081, 0x80);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
#endif


static kal_uint32 return_sensor_id(void)
{
	
    return ( (read_cmos_sensor_16_8(0x300b) << 12) | ((read_cmos_sensor_16_8(0x300c)-55) << 8) | read_cmos_sensor_16_8(0x302d));
}
#if 0
static void ov16b_PDGainMap_cali(void)
{

	unsigned char i, read_buf[98];
	kal_uint16 addr;
	/*[VIVO_OTP_DATA_SIZE]0x0E28; PD gain mapping from 0xDB4(3508)*/
/*	
	if (vivo_otp_data[0x0DB4] == 0x01){
		memcpy(buf,&vivo_otp_data[0x0DB5],sizeof(buf)); // start addre =0xDB4,(3508)
		
		for( addr = 0x5ad0, i = 0; i < 4; i++)
			write_cmos_sensor(addr + i, buf[i + 4]); //PDC gain ratio
		for( addr = 0x592c, i =0; i < 45; i++)	//total 90 byte for PD gain curve
			write_cmos_sensor((addr -i),buf[i + 8]); //for mirror/flip_enable only!
		for( addr = 0x5959, i = 0; i < 45; i++)
			write_cmos_sensor((addr -i),buf[i + 53]); //for mirror/flip_enable only!
		LOG_INF("ov16b pd gain maping write success \n");
	}else{
		LOG_INF("ov16b pd gain invalid \n");
	}
*/
	/*read gain */
	for( addr = 0x5ad0, i = 0; i < 4; i++)
		read_buf[i + 4] = read_cmos_sensor_16_8(addr + i);
	for( addr = 0x592c, i =0; i < 45; i++)
		read_buf[i + 8] = read_cmos_sensor_16_8(addr - i);
	for( addr = 0x5959, i = 0; i < 45; i++)
		read_buf[i + 53] = read_cmos_sensor_16_8(addr - i);
	for(i = 0 ; i < 98; i++)
		LOG_INF("read_buf[%d] = 0x%x\n", i, read_buf[i]);
}
#endif
/*************************************************************************
* FUNCTION
*    get_imgsensor_id
*
* DESCRIPTION
*    This function get the sensor ID
*
* PARAMETERS
*    *sensorID : return the sensor ID
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
/*	int I2C_BUS = -1 ;
	I2C_BUS = i2c_adapter_id(pgi2c_cfg_legacy->pinst->pi2c_client->adapter);
	LOG_INF(" I2C_BUS = %d\n",I2C_BUS);
	if(I2C_BUS != 2){	
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}*/
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address*/
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			printk("xinj: %s %d id=0x%x\n",__func__,__LINE__,*sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				
				/*vivo lxd add for CameraEM otp errorcode*/
/*				LOG_INF("lxd_add:start read eeprom ---vivo_otp_read_when_power_on = %d\n", vivo_otp_read_when_power_on);
				vivo_otp_read_when_power_on = ov48b2q_otp_read();
				LOG_INF("lxd_add:end read eeprom ---vivo_otp_read_when_power_on = %d,OV48B2Q_OTP_ERROR_CODE=%d\n", vivo_otp_read_when_power_on, OV48B2Q_OTP_ERROR_CODE);*/
				/*vivo lxd add end*/
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail:0x%x, id: 0x%x\n", imgsensor.i2c_write_id,
				*sensor_id);
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
}


/*************************************************************************
* FUNCTION
*    open
*
* DESCRIPTION
*    This function initialize the registers of CMOS sensor
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;
	LOG_2;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			printk("xinj: %s %d id=0x%x\n",__func__,__LINE__,sensor_id);
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,
				sensor_id);
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
	
	/* for PD gain curve */
//	ov16b_PDGainMap_cali(); 

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = imgsensor_info.ihdr_support;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*    open  */



/*************************************************************************
* FUNCTION
*    close
*
* DESCRIPTION
*
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*    close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*    This function start the sensor preview.
*
* PARAMETERS
*    *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	/* set_mirror_flip(imgsensor.mirror); */
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    preview   */

/*************************************************************************
* FUNCTION
*    capture
*
* DESCRIPTION
*    This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
	LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	 capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	/*preview_setting();*/
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */
#if 0
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);/*using caputre_setting*/
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	custom1   */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();/*using preview setting*/
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	custom2   */
#endif

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
/*	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;
*/
	return ERROR_NONE;
}				/*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	/* sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; not use */
	/* sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; not use */
	/* imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
/*	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
*/
	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 0; /* PDAF_SUPPORT_CAMSV*/
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

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

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
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
/*	    case MSDK_SCENARIO_ID_CUSTOM1:
	        sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx; 
	        sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

	        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc; 

	        break;
	    case MSDK_SCENARIO_ID_CUSTOM2:
	        sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx; 
	        sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

	        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom2.mipi_data_lp2hs_settle_dc; 
*/
	        break;

	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
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
/*	    case MSDK_SCENARIO_ID_CUSTOM1:
	        Custom1(image_window, sensor_config_data);
	        break;
	    case MSDK_SCENARIO_ID_CUSTOM2:
	        Custom2(image_window, sensor_config_data);
	        break;*/
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{				/* This Function not used after ROME */
	LOG_INF("framerate = %d\n ", framerate);
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

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
						MUINT32 framerate)
{
	/* kal_int16 dummyLine; */
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	if (framerate == 0)
		return ERROR_NONE;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.pre.framelength) ? (frame_length -
							imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.normal_video.framelength) ? (frame_length -
								 imgsensor_info.normal_video.
								 framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy(); 
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length =
			    imgsensor_info.cap1.pclk / framerate * 10 /
			    imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length >
			     imgsensor_info.cap1.framelength) ? (frame_length -
								 imgsensor_info.cap1.
								 framelength) : 0;
			imgsensor.frame_length =
			    imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_INF
				    ("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				     framerate, imgsensor_info.cap.max_framerate / 10);
			frame_length =
			    imgsensor_info.cap.pclk / framerate * 10 /
			    imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length >
			     imgsensor_info.cap.framelength) ? (frame_length -
								imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
			    imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		 set_dummy(); 
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		 set_dummy(); 
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
		    imgsensor_info.slim_video.pclk / framerate * 10 /
		    imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.slim_video.framelength) ? (frame_length -
							       imgsensor_info.slim_video.
							       framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		 set_dummy(); 
		break;
/*	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;*/
	default:		/* coding with  preview scenario by default */
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.pre.framelength) ? (frame_length -
							imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
						    MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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
/*	case MSDK_SCENARIO_ID_CUSTOM1:
	    *framerate = imgsensor_info.custom1.max_framerate;
	    break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	    *framerate = imgsensor_info.custom2.max_framerate;
	    break;*/
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	/* check,power+volum+ */
	LOG_INF("enable: %d\n", enable);

	if (enable) {
        write_cmos_sensor_16_8(0x5081, 0x81);
	} else {
        write_cmos_sensor_16_8(0x5081, 0x80);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	/* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
        struct SENSOR_VC_INFO_STRUCT *pvcinfo;


	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	             /*night_mode((BOOL) *feature_data);*/
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_16_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor_16_8(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
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
		set_auto_flicker_mode((BOOL) * feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM) *feature_data,*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM) *(feature_data),(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	/*case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		read_ov48b2q_eeprom((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
		break;*/

	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *feature_data_32);
	    wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
/*	        case MSDK_SCENARIO_ID_CUSTOM1:
	            memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[5],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
	            break;
	        case MSDK_SCENARIO_ID_CUSTOM2:
	            memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[6],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
	            break;*/
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%lld\n", *feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
/*		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM1:*/
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info_16_9,sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		/* PDAF capacity enable or not, ov48b2q only full size support PDAF */
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; /* video & capture use same setting*/
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
/*		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;

		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
*/
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
		break;
/*
    case SENSOR_FEATURE_SET_PDAF:
        LOG_INF("PDAF mode :%d\n", *feature_data_16);
        imgsensor.pdaf_mode= *feature_data_16;
        break;
    */
    case SENSOR_FEATURE_GET_VC_INFO:
        LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
        pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
//		case MSDK_SCENARIO_ID_CUSTOM1:
		    memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1], sizeof(struct SENSOR_VC_INFO_STRUCT));
		    break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		    memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2], sizeof(struct SENSOR_VC_INFO_STRUCT));
		    break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
//		case MSDK_SCENARIO_ID_CUSTOM2:
		default:
		    memcpy((void *)pvcinfo, (void *) &SENSOR_VC_INFO[0], sizeof(struct SENSOR_VC_INFO_STRUCT));
		    break;
		}
		break;  
		
#if 0
        case SENSOR_FEATURE_GET_CUSTOM_INFO:
			
		    LOG_INF("SENSOR_FEATURE_GET_CUSTOM_INFO information type:%lld  OV48B2Q_OTP_ERROR_CODE:%d \n", *feature_data,OV48B2Q_OTP_ERROR_CODE);
			switch (*feature_data) {
				case 0:    //info type: otp state
				    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = OV48B2Q_OTP_ERROR_CODE;//otp_state
					memcpy(feature_data+2, sn_inf_main_ov48b2q, sizeof(MUINT32)*13); 
					#if 0
							for (i = 0 ; i<13 ; i++ ){
							printk("sn_inf_main_ov48b2q[%d]= 0x%x\n", i, sn_inf_main_ov48b2q[i]);
							}
						
					#endif
					break;
			}
			break;

	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16) *feature_data,
			(UINT16) *(feature_data + 1), (UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data, (UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
#endif
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        {
               kal_uint32 rate;
 
               switch (*feature_data) {
               case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                       rate = imgsensor_info.cap.mipi_pixel_rate;
                       break;
               case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                       rate = imgsensor_info.normal_video.mipi_pixel_rate;
                       break;
               case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                       rate = imgsensor_info.hs_video.mipi_pixel_rate;
                       break;
               case MSDK_SCENARIO_ID_SLIM_VIDEO:
                       rate = imgsensor_info.slim_video.mipi_pixel_rate;
                       break;
               case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                       rate = imgsensor_info.pre.mipi_pixel_rate;
                       break;
               default:
                       rate = 0;
                       break;
               }
               *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
        }
        break;

	default:
		break;
	}

	return ERROR_NONE;
}				/*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV48B2Q_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    OV48B2Q_MIPI_RAW_SensorInit    */
