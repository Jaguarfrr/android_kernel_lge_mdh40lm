#define pr_fmt(fmt) "[CHGCTRL-ATCMD]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/power/charger_controller.h>
#include "charger_controller.h"

/*
 * AT%BATMP
 * - output : xx.x (ex 25.0)
 * - /proc/lge_power/testmode/temp
 */
static ssize_t atcmd_batmp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);

	/* update battery info */
	chgctrl_get_battery_temp(chip);

	return scnprintf(buf, PAGE_SIZE, "%d.%d",
			chip->hook.temp / 10, chip->hook.temp % 10);
}
static DEVICE_ATTR_RO(atcmd_batmp);

/*
 * AT%CHARGE
 * - input : 1
 * - output : 0 or 1
 * - /proc/lge_power/testmode/charge
 */
static ssize_t atcmd_charge_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int charge = 0;

	/* update battery info */
	chgctrl_get_battery_status(chip);

	if (chip->hook.status == POWER_SUPPLY_STATUS_CHARGING
			|| chip->hook.status == POWER_SUPPLY_STATUS_FULL)
		charge = 1;

	return scnprintf(buf, PAGE_SIZE, "%d", charge);
}

static ssize_t atcmd_charge_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int charge = 0;
	int ret;

	ret = sscanf(buf, "%d", &charge);
	if (ret <= 0)
		return -EINVAL;

	ret = chgctrl_vote(chip->icl_boost, ICL_BOOST_VOTER_ATCMD,
			(charge == 1) ? 1500 : -1);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(atcmd_charge, 0664, atcmd_charge_show, atcmd_charge_store);

/*
 * AT%CHCOMP
 * - output : 0 or 1
 * - /proc/lge_power/testmode/chcomp
 */
static ssize_t atcmd_chcomp_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int chcomp = 0;

	/* update battery info */
	/* update battery info */
	chgctrl_get_battery_status(chip);
	chgctrl_get_battery_voltage_now(chip);

	if (chip->hook.status == POWER_SUPPLY_STATUS_FULL)
		chcomp = 1;

	if (chip->hook.status == POWER_SUPPLY_STATUS_CHARGING
			&& chip->hook.voltage_now >= 4250000)
		chcomp = 1;

	return scnprintf(buf, PAGE_SIZE, "%d", chcomp);
}
static DEVICE_ATTR_RO(atcmd_chcomp);

/*
 * AT%CHARGINGMODEOFF
 * - input : 1
 * - /proc/lge_power/testmode/chgmodeoff
 */
static ssize_t atcmd_chgmodeoff_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int chgmodeoff = 0;
	int ret;

	ret = sscanf(buf, "%d", &chgmodeoff);
	if (ret <= 0)
		return -EINVAL;

	ret = chgctrl_vote(chip->fcc, FCC_VOTER_ATCMD,
			(chgmodeoff == 1) ? 0 : -1);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(atcmd_chgmodeoff, 0220, NULL, atcmd_chgmodeoff_store);

static struct attribute *chgctrl_atcmd_attrs[] = {
	&dev_attr_atcmd_batmp.attr,
	&dev_attr_atcmd_charge.attr,
	&dev_attr_atcmd_chcomp.attr,
	&dev_attr_atcmd_chgmodeoff.attr,
	NULL,
};
ATTRIBUTE_GROUPS(chgctrl_atcmd);

int chgctrl_init_atcmd(struct chgctrl *chip)
{
	return devm_device_add_groups(chip->dev, chgctrl_atcmd_groups);
}
