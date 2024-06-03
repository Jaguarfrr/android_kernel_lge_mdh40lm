#ifndef __DW8768L_H__
#define __DW8768L_H__

#define DW8768L_DEV_NAME        "I2C_DW8768L"
#define DW8768L_DTS_NODE        "mediatek,i2c_dw8768l"

#define ENABLE                  1
#define DISABLE                 0

#define LCD_BIAS_VPOS_ADDR      0x00
#define LCD_BIAS_VNEG_ADDR      0x01
#define LCD_BIAS_DISC_ADDR      0x03
#define LCD_BIAS_ENA_ADDR       0x05
#define LCD_BIAS_KNOCK_ADDR     0x07

typedef enum {
	VPOS = 0,
	VNEG,
	DISC,
	ENAR,
	KNOK
} DW8768L_REG;

unsigned char dsv_addr[5] = {
  LCD_BIAS_VPOS_ADDR,
  LCD_BIAS_VNEG_ADDR,
  LCD_BIAS_DISC_ADDR,
  LCD_BIAS_ENA_ADDR,
  LCD_BIAS_KNOCK_ADDR
};

unsigned char dsv_data[5] = {
  0x0F,
  0x0F,
  0x83,
  0x0F,
  0x00
};

#endif
