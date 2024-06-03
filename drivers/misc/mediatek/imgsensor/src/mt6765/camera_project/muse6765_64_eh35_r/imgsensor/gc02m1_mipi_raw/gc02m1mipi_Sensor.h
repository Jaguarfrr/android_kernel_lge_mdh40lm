/*****************************************************************************
 *
 * Filename:
 * ---------
 *     gc02m1mipi_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef __GC02M1MIPI_SENSOR_H__
#define __GC02M1MIPI_SENSOR_H__


/* SENSOR MIRROR FLIP INFO */
#define GC02M1_MIRROR_NORMAL    1
#define GC02M1_MIRROR_H         0
#define GC02M1_MIRROR_V         0
#define GC02M1_MIRROR_HV        0

#if GC02M1_MIRROR_NORMAL
#define GC02M1_MIRROR	        0x80
#elif GC02M1_MIRROR_H
#define GC02M1_MIRROR	        0x81
#elif GC02M1_MIRROR_V
#define GC02M1_MIRROR	        0x82
#elif GC02M1_MIRROR_HV
#define GC02M1_MIRROR	        0x83
#else
#define GC02M1_MIRROR	        0x80
#endif
#define GC02M1_EEPROM_ENABLE    0
#if GC02M1_EEPROM_ENABLE
#define GC8034_EEPROM_I2C_ADDR 0xA0
#include <linux/hardware_info.h>
#endif


/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define GC02M1_SENSOR_GAIN_BASE             0x400
#define GC02M1_SENSOR_GAIN_MAX              (12 * GC02M1_SENSOR_GAIN_BASE)
#define GC02M1_SENSOR_GAIN_MAX_VALID_INDEX  16
#define GC02M1_SENSOR_GAIN_MAP_SIZE         16
#define GC02M1_SENSOR_DGAIN_BASE            0x400

enum{
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;
	kal_uint32 linelength;
	kal_uint32 framelength;
	kal_uint8 startx;
	kal_uint8 starty;
	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;
	kal_uint8 mipi_data_lp2hs_settle_dc;
	kal_uint32 mipi_pixel_rate;
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES */
struct imgsensor_struct {
	kal_uint8 mirror;
	kal_uint8 sensor_mode;
	kal_uint32 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint32 frame_length;
	kal_uint32 line_length;
	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;
	kal_uint16 dummy_line;
	kal_uint16 current_fps;
	kal_bool   autoflicker_en;
	kal_bool   test_pattern;
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_uint8  ihdr_en;
	kal_uint8 i2c_write_id;
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32 sensor_id;
	kal_uint32 checksum_value;
	struct imgsensor_mode_struct pre;
	struct imgsensor_mode_struct cap;
	struct imgsensor_mode_struct cap1;
	struct imgsensor_mode_struct normal_video;
	struct imgsensor_mode_struct hs_video;
	struct imgsensor_mode_struct slim_video;
	kal_uint8  ae_shut_delay_frame;
	kal_uint8  ae_sensor_gain_delay_frame;
	kal_uint8  ae_ispGain_delay_frame;
	kal_uint8  ihdr_support;
	kal_uint8  ihdr_le_firstline;
	kal_uint8  sensor_mode_num;
	kal_uint8  cap_delay_frame;
	kal_uint8  pre_delay_frame;
	kal_uint8  video_delay_frame;
	kal_uint8  hs_video_delay_frame;
	kal_uint8  slim_video_delay_frame;
	kal_uint8  margin;
	kal_uint32 min_shutter;
	kal_uint32 max_frame_length;
	kal_uint8  isp_driving_current;
	kal_uint8  sensor_interface_type;
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;
	kal_uint8  mipi_lane_num;
	kal_uint8  i2c_addr_table[5];
	kal_uint32 i2c_speed;
};

#if GC02M1_EEPROM_ENABLE
typedef struct eeprom_data {
	kal_uint8 vaild_flag;
	kal_uint8 supplier_code;
	kal_uint8 module_code;
	kal_uint8 module_version;
	kal_uint8 sw_ver;
	kal_uint8 year;
	kal_uint8 month;
	kal_uint8 day;
	kal_uint8 awb_lsc_station;
	kal_uint8 pdaf_station;
	kal_uint8 af_station;
	kal_uint8 vcm_id;
	kal_uint8 sensor_id;
	kal_uint8 lens_id;
	kal_uint8 driver_id;
}EEPROM_DATA;

static EEPROM_DATA pOtp_data;
extern struct global_otp_struct hw_info_sub2_otp;
#endif

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId, u16 timing);

#endif
