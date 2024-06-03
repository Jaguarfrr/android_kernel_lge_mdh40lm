/*
 *  lge_battery_id.c
 *
 *  LGE Battery Charger Interface Driver
 *
 *  Copyright (C) 2011 LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#include <linux/power/lge_pseudo_batt.h>
#endif

#ifdef CONFIG_LGE_USB_ID
#include <soc/mediatek/lge/lge_usb_id.h>
#endif
#define BATT_NOT_PRESENT 200

enum {
	BATT_ID_MISSING         = -1,
	BATT_ID_UNKNOWN         = 0,
	/* Embedded ADC Gen 1 */
	BATT_ID_LGC             = 5,
	BATT_ID_TOCAD           = 75,
	BATT_ID_ATL             = 200,
	BATT_ID_BYD             = 225,
	BATT_ID_LISHEN          = 230,
	/* Embedded ADC Gen 2 */
	BATT_LGC_GEN2           = 7,
	BATT_TOCAD_GEN2         = 77,
	BATT_ATL_GEN2           = 202,
	BATT_BYD_GEN2           = 227,
	BATT_LISHEN_GEN2        = 232,
	/* Authentication IC */
	BATT_ID_DS2704_N        = 17,
	BATT_ID_DS2704_L        = 32,
	BATT_ID_DS2704_C        = 48,
	BATT_ID_ISL6296_N       = 73,
	BATT_ID_ISL6296_L       = 94,
	BATT_ID_ISL6296_C       = 105,
	BATT_ID_ISL6296A_N      = 110,
	BATT_ID_ISL6296A_L      = 115,
	BATT_ID_ISL6296A_C      = 120,
	BATT_ID_RA4301_VC0      = 130,
	BATT_ID_RA4301_VC1      = 147,
	BATT_ID_RA4301_VC2      = 162,
	BATT_ID_SW3800_VC0      = 187,
	BATT_ID_SW3800_VC1      = 204,
	BATT_ID_SW3800_VC2      = 219,
};

struct battery_id_t {
	int id;
	char *name;
};

struct lge_battery_id_info {
	struct device *dev;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;

	/* battery id information */
	const struct battery_id_t *id;
	int valid;

	/* battery information */
	int voltage_max;
	int charge_full_design;
	const char *model;
	const char *manufacturer;
	const char *type;
};

static const struct battery_id_t id_unknown = {
	.id = BATT_ID_UNKNOWN,
	.name = "UNKNOWN",
};

static const struct battery_id_t id_missing = {
	.id = BATT_ID_MISSING,
	.name = "MISSING",
};

static const struct battery_id_t ids[] = {
	/* Embedded ADC Gen 1 */
	{ BATT_ID_LGC, "LGC" },
	{ BATT_ID_TOCAD, "TOCAD" },
	{ BATT_ID_ATL, "ATL" },
	{ BATT_ID_BYD, "BYD" },
	{ BATT_ID_LISHEN, "LISHEN" },
	/* Embedded ADC Gen 2 */
	{ BATT_LGC_GEN2, "LGC_GEN2" },
	{ BATT_TOCAD_GEN2, "TOCAD_GEN2" },
	{ BATT_ATL_GEN2, "ATL_GEN2" },
	{ BATT_BYD_GEN2, "BYD_GEN2" },
	{ BATT_LISHEN_GEN2, "LISHEN_GEN2" },
	/* Authentication IC */
	{ BATT_ID_DS2704_N, "DS2704_N" },
	{ BATT_ID_DS2704_L, "DS2704_L" },
	{ BATT_ID_DS2704_C, "DS2704_C" },
	{ BATT_ID_ISL6296_N, "ISL6296_N" },
	{ BATT_ID_ISL6296_L, "ISL6296_L" },
	{ BATT_ID_ISL6296_C, "ISL6296_C" },
	{ BATT_ID_ISL6296A_N, "ISL6296A_N" },
	{ BATT_ID_ISL6296A_L, "ISL6296A_L" },
	{ BATT_ID_ISL6296A_C, "ISL6296A_C" },
	{ BATT_ID_RA4301_VC0, "RA4301_VC0" },
	{ BATT_ID_RA4301_VC1, "RA4301_VC1" },
	{ BATT_ID_RA4301_VC2, "RA4301_VC2" },
	{ BATT_ID_SW3800_VC0, "SW3800_VC0" },
	{ BATT_ID_SW3800_VC1, "SW3800_VC1" },
	{ BATT_ID_SW3800_VC2, "SW3800_VC2" },
};

static ssize_t id_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	struct lge_battery_id_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", info->id->id);
}
static DEVICE_ATTR_RO(id);

static ssize_t id_valid_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct lge_battery_id_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", info->valid);
}
static DEVICE_ATTR_RO(id_valid);

static ssize_t type_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct lge_battery_id_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s", info->type ?: "Unknown");
}
static DEVICE_ATTR_RO(type);

static ssize_t model_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct lge_battery_id_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s", info->model ?: "Unknown");
}
static DEVICE_ATTR_RO(model);

static ssize_t manufacturer_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct lge_battery_id_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s", info->manufacturer ?: "Unknown");
}
static DEVICE_ATTR_RO(manufacturer);

static struct attribute *battery_id_info_attrs[] = {
	&dev_attr_id.attr,
	&dev_attr_id_valid.attr,
	&dev_attr_type.attr,
	&dev_attr_model.attr,
	&dev_attr_manufacturer.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_id_info);

static enum power_supply_property lge_battery_id_properties[] = {
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	/* Properties of type `const char *' */
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int lge_battery_id_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct lge_battery_id_info *info = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->voltage_max;
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = info->valid;
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
		if (get_pseudo_batt_info(PSEUDO_BATT_MODE))
			val->intval = 1;
#endif
#ifdef CONFIG_LGE_USB_ID
		if (lge_is_factory_cable_boot())
			val->intval = 1;
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = info->charge_full_design;
		break;
	/* Properties of type `const char *' */
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = info->model ?: "Unknown";
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = info->manufacturer ?: "Unknown";
		break;
	/* type can be read in kernel-space only */
	case POWER_SUPPLY_PROP_TYPE:
		val->strval = info->type ?: "Unknown";
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int lge_battery_id_read_info(struct lge_battery_id_info *info,
				    struct device_node *np)
{
	of_property_read_u32(np, "voltage-max", &info->voltage_max);
	of_property_read_u32(np, "charge-full-design",
			&info->charge_full_design);
	of_property_read_string(np, "model-name", &info->model);
	of_property_read_string(np, "manufacturer", &info->manufacturer);
	of_property_read_string(np, "type", &info->type);

	return 0;
}

static int lge_battery_id_get_info(struct lge_battery_id_info *info)
{
	struct device_node *np = info->dev->of_node;
	struct device_node *battery = NULL;
	const char *name = info->id->name;

	/* read base information */
	lge_battery_id_read_info(info, np);

	/* find battery information */
	for_each_child_of_node(np, battery) {
		if (of_property_match_string(battery, "id", name) >= 0) {
			lge_battery_id_read_info(info, battery);
			of_node_put(battery);
			info->valid = 1;
			break;
		}
	}

	return 0;
}

static const struct battery_id_t *lge_battery_id_find_id(const char *name)
{
	int i;

	if (!name)
		return &id_unknown;

	for (i = 0; i < ARRAY_SIZE(ids); i++) {
		if (!strcmp(name, ids[i].name))
			return &ids[i];
	}

	if (!strcmp(name, id_missing.name))
		return &id_missing;

	return &id_unknown;
}

static const char *lge_battery_id_get_name(struct lge_battery_id_info *info)
{
	struct device_node *np = info->dev->of_node;
	struct device_node *battery_node;
	const char *name;

	if (!np)
		return NULL;

	battery_node = of_parse_phandle(np, "battery-id", 0);
	if (!battery_node) {
		dev_info(info->dev, "battery-id not found\n");
		return NULL;
	}

	name = of_get_property(battery_node, "lge,battery-id", NULL);
	if (!name)
		dev_info(info->dev, "lge,battery-id not found\n");

	of_node_put(battery_node);

	return name;
}

static int lge_battery_id_probe(struct platform_device *pdev)
{
	struct lge_battery_id_info *info;
	const char *name;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(struct lge_battery_id_info),
			GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "failed: allocation memory\n");
		return -ENOMEM;
	}

	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	name = lge_battery_id_get_name(info);
	if (!name)
		return -ENODEV;

	info->id = lge_battery_id_find_id(name);
	lge_battery_id_get_info(info);

	info->psy_desc.name = "battery_id";
	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc.properties = lge_battery_id_properties;
	info->psy_desc.num_properties =
			ARRAY_SIZE(lge_battery_id_properties);
	info->psy_desc.get_property = lge_battery_id_get_property;

	info->psy_cfg.drv_data = info;

	info->psy = devm_power_supply_register_no_ws(info->dev,
			&info->psy_desc, &info->psy_cfg);
	if (IS_ERR(info->psy))
		return PTR_ERR(info->psy);

	dev_set_uevent_suppress(&info->psy->dev, 1);

	ret = devm_device_add_groups(info->dev, battery_id_info_groups);
	if (ret) {
		dev_info(info->dev, "failed to add group\n");
		return ret;
	}

	dev_info(info->dev, "Type: %s\n", info->type ?: "Unknown");
	dev_info(info->dev, "Model: %s, ID: %s, Vendor: %s\n",
			info->model ?: "Unknown",
			info->id->name,  info->manufacturer ?: "Unknown");

	return 0;
}

static struct of_device_id lge_battery_id_match_table[] = {
	{ .compatible = "lge,battery-id" },
	{}
};

static struct platform_driver lge_battery_id_driver = {
	.driver = {
		.name   = "lge_battery_id",
		.owner  = THIS_MODULE,
		.of_match_table = lge_battery_id_match_table,
	},
	.probe  = lge_battery_id_probe,
};

module_platform_driver(lge_battery_id_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cowboy");
MODULE_DESCRIPTION("LGE Battery ID Checker");
