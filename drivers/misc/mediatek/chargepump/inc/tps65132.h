#ifndef __TPS65132_H__
#define __TPS65132_H__

#define TPS65132_VPOS_ADDR    	0x00
#define TPS65132_VNEG_ADDR    	0x01
#define TPS65132_APPS_ADDR    	0x03

#define TPS65132_PRINT			printk
#define TPS65132_I2C_ID_NAME	"I2C_TPS65132"
#define TPS65132_DTS_NODE       "mediatek,i2c_tps65132"
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
int TPS65132_set_vspn(void);
int TPS65132_power_off_vspn(void);

#endif
