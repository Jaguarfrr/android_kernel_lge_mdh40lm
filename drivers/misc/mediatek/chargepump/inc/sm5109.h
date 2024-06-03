#ifndef __SM5109_H__
#define __SM5109_H__

#define SM5109_VPOS_ADDR      0x00
#define SM5109_VNEG_ADDR      0x01
#define SM5109_APPS_ADDR      0x03
#define NEG_OUTPUT_APPS       0x03

#define SM5109_PRINT          printk
#define SM5109_I2C_ID_NAME    "I2C_SM5109"
#define SM5109_DTS_NODE       "mediatek,i2c_sm5109"

#define MIN_VOLTAGE           4000
#define MAX_VOLTAGE           6500

enum SM5109 {
  POSCTRL = 0,
  NEGCTRL,
  CONTROL,
  REGCNT
};

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/

int sm5109_set_vspn(void);
int sm5109_power_off_vspn(void);
int sm5109_set_vspn_value(unsigned int value);

#endif
