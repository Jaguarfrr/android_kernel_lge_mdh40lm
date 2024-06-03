#define pr_fmt(fmt) "[CHGCTRL-INFO]" fmt

#include <linux/power/charger_controller.h>
#include "charger_controller.h"

#ifdef CONFIG_LGE_USB_ID
#include <soc/mediatek/lge/lge_usb_id.h>
#endif

int chgctrl_get_charger_online(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->charger_psy,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (!ret)
		return val.intval;

	return 0;
}

int chgctrl_get_charger_type(struct chgctrl *chip)
{
	return chip->charger_psy->desc->type;
}

int chgctrl_get_charger_usb_type(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->charger_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &val);
	if (!ret)
		return val.intval;

	return POWER_SUPPLY_USB_TYPE_UNKNOWN;
}

int chgctrl_get_battery_present(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (!ret)
		return val.intval;

	return 1;
}

int chgctrl_get_battery_status(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_STATUS, &val);
	if (!ret)
		return val.intval;

	return POWER_SUPPLY_STATUS_UNKNOWN;
}

int chgctrl_get_battery_health(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_HEALTH, &val);
	if (!ret)
		return val.intval;

	return POWER_SUPPLY_HEALTH_UNKNOWN;
}

int chgctrl_get_battery_voltage_now(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (!ret)
		return val.intval;

	return 4000000;
}

int chgctrl_get_battery_current_now(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (!ret)
		return val.intval;

	return 0;
}

int chgctrl_get_battery_capacity(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
	if (!ret)
		return val.intval;

	return 50;
}

int chgctrl_get_battery_temp(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_TEMP, &val);
	if (!ret)
		return val.intval;

	return 200;
}

int chgctrl_get_battery_technology(struct chgctrl *chip)
{
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(chip->battery_psy,
			POWER_SUPPLY_PROP_TECHNOLOGY, &val);
	if (!ret)
		return val.intval;

	return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
}

static bool chgctrl_is_charger_boot(struct chgctrl *chip)
{
	struct device_node *np = chip->dev->of_node;
	bool charger_boot = false;
	struct tag_bootmode {
		u32 size;
		u32 tag;
		u32 bootmode;
		u32 boottype;
	} *tag;

	np = of_parse_phandle(np, "bootmode", 0);
	if (!np) {
		pr_err("failed to get boot mode phandle\n");
		return false;
	}

	tag = (struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
	if (!tag) {
		pr_err("failed to get atag,boot\n");
		goto out;
	}

	pr_info("boot mode = %u\n", tag->bootmode);
	if (tag->bootmode == 8 || tag->bootmode == 9) {
		pr_info("charger boot\n");
		charger_boot = true;
	}

out:
	of_node_put(np);

	return charger_boot;
}

enum chgctrl_boot_mode chgctrl_get_boot_mode(struct chgctrl *chip)
{
#ifdef CONFIG_LGE_USB_ID
	if (lge_is_factory_cable_boot())
		return BOOT_MODE_FACTORY;
#endif

	if (chgctrl_is_charger_boot(chip))
		return BOOT_MODE_CHARGER;

	return BOOT_MODE_NORMAL;
}

enum chgstep {
	CHGSTEP_DISCHG,
	CHGSTEP_TRICLKE,
	CHGSTEP_PRECHARGING,
	CHGSTEP_FAST,
	CHGSTEP_FULLON,
	CHGSTEP_TAPER,
	CHGSTEP_EOC,
	CHGSTEP_INHIBIT,
};

static const char *chgstep_str[] = {
	[CHGSTEP_DISCHG] = "0 DISCHG",
	[CHGSTEP_TRICLKE] = "1 TRICKLE",
	[CHGSTEP_PRECHARGING] = "2 PRECHARGING",
	[CHGSTEP_FAST] = "3 FAST",
	[CHGSTEP_FULLON] = "4 FULLON",
	[CHGSTEP_TAPER] = "5 TAPER",
	[CHGSTEP_EOC] = "6 EOC",
	[CHGSTEP_INHIBIT] = "7 INHIBIT",
};

static ssize_t chgstep_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	enum chgstep chgstep;

	switch (chip->hook.status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		chgstep = CHGSTEP_FAST;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		chgstep = CHGSTEP_EOC;
		break;
	default:
		chgstep = CHGSTEP_DISCHG;
		break;
	}

	return scnprintf(buf, PAGE_SIZE, "%s", chgstep_str[chgstep]);
}
static DEVICE_ATTR_RO(chgstep);

static ssize_t fastchg_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", chip->fastchg_name ? 1 : 0);
}
static DEVICE_ATTR_RO(fastchg);

static ssize_t fastchg_support_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int support = chgctrl_vote_get_value(chip->fastchg,
			FASTCHG_VOTER_DEFAULT) ? 1 : 0;

	return scnprintf(buf, PAGE_SIZE, "%d", support);
}
static DEVICE_ATTR_RO(fastchg_support);

static ssize_t fastchg_type_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s", chip->fastchg_name ?: "None");
}
static DEVICE_ATTR_RO(fastchg_type);

static ssize_t floatchg_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int floated_charger = chip->floated_charger;

	if (!chip->info.online)
		floated_charger = 0;
	if (chip->typec_usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
		floated_charger = 0;
	if (chip->fastchg_name)
		floated_charger = 0;

	return scnprintf(buf, PAGE_SIZE, "%d", floated_charger);
}
static DEVICE_ATTR_RO(floatchg);

static ssize_t vzwchg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int vzw_chg = chip->vzw_chg_state;

	if (chip->fastchg_name)
		vzw_chg = 1;
	if (!chip->info.online)
		vzw_chg = 0;

	return scnprintf(buf, PAGE_SIZE, "%d", vzw_chg);
}
static DEVICE_ATTR_RO(vzwchg);

static struct attribute *chgctrl_info_attrs[] = {
	&dev_attr_chgstep.attr,
	&dev_attr_fastchg.attr,
	&dev_attr_fastchg_support.attr,
	&dev_attr_fastchg_type.attr,
	&dev_attr_floatchg.attr,
	&dev_attr_vzwchg.attr,
	NULL,
};
ATTRIBUTE_GROUPS(chgctrl_info);

int chgctrl_init_info(struct chgctrl *chip)
{
	return devm_device_add_groups(chip->dev, chgctrl_info_groups);
}
