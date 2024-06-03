#ifndef _CHARGER_CONTROLLER_H_
#define _CHARGER_CONTROLLER_H_

#include <linux/power_supply.h>

struct chgctrl;

struct chgctrl_helper {
	void (*charger_hook)(struct chgctrl_helper *helper,
			     enum power_supply_property psp,
			     union power_supply_propval *val);
	void (*battery_hook)(struct chgctrl_helper *helper,
			     enum power_supply_property psp,
			     union power_supply_propval *val);

	int (*get_icl)(struct chgctrl_helper *helper);
	int (*get_fcc)(struct chgctrl_helper *helper);
	int (*get_vfloat)(struct chgctrl_helper *helper);
	int (*get_icl_boost)(struct chgctrl_helper *helper);
	bool (*get_fastchg)(struct chgctrl_helper *helper);
	int (*get_wless_pwr)(struct chgctrl_helper *helper);

	void (*set_bc12_type)(struct chgctrl_helper *helper,
			      enum power_supply_type type,
			      enum power_supply_usb_type usb_type);
	void (*set_typec_usb_type)(struct chgctrl_helper *helper,
				   enum power_supply_usb_type usb_type);
	void (*set_fastchg_type)(struct chgctrl_helper *helper,
				 const char *type);

	void (*set_overvoltage)(struct chgctrl_helper *helper,
				bool overvoltage);

	void (*set_input_current_limit) (struct chgctrl_helper *helper,
				int input_current_limit);

	bool (*is_floatchg)(struct chgctrl_helper *helper);
	bool (*is_fastchg)(struct chgctrl_helper *helper);

#ifdef CONFIG_LGE_PM_QNOVO_QNS
	void (*set_qns_fcc)(struct chgctrl_helper *helper, int val);
	void (*set_qns_vfloat)(struct chgctrl_helper *helper, int val);
#endif

	struct chgctrl *chip;
};

#endif
