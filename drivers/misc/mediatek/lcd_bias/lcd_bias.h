#ifndef _LCD_BIAS_H
#define _LCD_BIAS_H

#define LCD_BIAS_VPOS_ADDR    	0x00
#define LCD_BIAS_VNEG_ADDR    	0x01
#define LCD_BIAS_APPS_ADDR    	0x03
#define NEG_OUTPUT_APPS    			0x03

#define LCD_BIAS_PRINT					printk
#define LCD_BIAS_DTS_ID_NAME 		"DTS_LCD_BIAS"
#define LCD_BIAS_I2C_ID_NAME		"I2C_LCD_BIAS"
#define LCD_BIAS_DTS_ID_NAME		"DTS_LCD_BIAS"
#define LCD_BIAS_DTS_GPIO_NODE	"mediatek,gpio_lcd_bias"

/* DTS state */
typedef enum tagLCD_BIAS_GPIO_STATE {
	LCD_BIAS_GPIO_STATE_LCM_RST0,
	LCD_BIAS_GPIO_STATE_LCM_RST1,
	LCD_BIAS_GPIO_STATE_LCD_LDO_EN0,
	LCD_BIAS_GPIO_STATE_LCD_LDO_EN1,
	LCD_BIAS_GPIO_STATE_DSV_VPOS_EN0,
	LCD_BIAS_GPIO_STATE_DSV_VPOS_EN1,
	LCD_BIAS_GPIO_STATE_DSV_VNEG_EN0,
	LCD_BIAS_GPIO_STATE_DSV_VNEG_EN1,
	LCD_BIAS_GPIO_STATE_MAX,	/* for array size */
} LCD_BIAS_GPIO_STATE;

typedef enum {
	LCM_RST = 0,
	LCD_LDO_EN = 1,
	DSV_VPOS_EN = 2,
	DSV_VNEG_EN = 3,
	TOUCH_RST = 4,
} LCD_BIAS_GPIO_CTRL;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
int lcd_bias_set_vspn(unsigned int value);
int lcd_bias_power_off_vspn(void);
void lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE s);
void lcd_bias_set_gpio_ctrl(unsigned int en, unsigned int seq);

#endif /* _LCD_BIAS_H */
