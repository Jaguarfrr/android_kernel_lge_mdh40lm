#define pr_fmt(fmt) "[CHGCTRL-CMD]%s: " fmt, __func__

#include <linux/power/charger_controller.h>
#include "charger_controller.h"

/* #define SHOW_OFFLINE_ON_CHARGING_DISABLED */
/* #define SHOW_OFFLINE_ON_INPUT_SUSPEND */

static ssize_t cmd_battery_charging_enabled_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val = chgctrl_vote_get_value(chip->fcc, FCC_VOTER_USER);

	return scnprintf(buf, PAGE_SIZE, "%d", val ? 1 : 0);
}

static ssize_t cmd_battery_charging_enabled_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <= 0)
		return -EINVAL;

	val = val ? -1 : 0;
	ret = chgctrl_vote(chip->fcc, FCC_VOTER_USER, val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(cmd_battery_charging_enabled, 0664,
		cmd_battery_charging_enabled_show,
		cmd_battery_charging_enabled_store);

static ssize_t cmd_charging_enabled_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;

#ifdef SHOW_OFFLINE_ON_CHARGING_DISABLED
	val = chgctrl_vote_get_value(chip->input_suspend,
			INPUT_SUSPEND_VOTER_USER);
	val = val ? 0 : 1;
#else
	val = chgctrl_vote_get_value(chip->icl, ICL_VOTER_USER);
	val = val ? 1 : 0;
#endif

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t cmd_charging_enabled_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <= 0)
		return -EINVAL;

#ifdef SHOW_OFFLINE_ON_CHARGING_DISABLED
	val = val ? 0 : 1;
	ret = chgctrl_vote(chip->input_suspend, INPUT_SUSPEND_VOTER_USER, val);
	if (ret)
		return ret;
#else
	val = val ? -1 : 0;
	ret = chgctrl_vote(chip->icl, ICL_VOTER_USER, val);
	if (ret)
		return ret;
#endif

	return count;
}
static DEVICE_ATTR(cmd_charging_enabled, 0664,
		cmd_charging_enabled_show, cmd_charging_enabled_store);

static ssize_t cmd_input_suspend_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;

#ifdef SHOW_OFFLINE_ON_INPUT_SUSPEND
	val = chgctrl_vote_get_value(chip->input_suspend,
			INPUT_SUSPEND_VOTER_USER);
	val = val ? 1 : 0;
#else
	val = chgctrl_vote_get_value(chip->icl, ICL_VOTER_USER);
	val = val ? 0 : 1;
#endif

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t cmd_input_suspend_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <=0)
		return -EINVAL;

#ifdef SHOW_OFFLINE_ON_INPUT_SUSPEND
	val = val ? 1 : 0;
	ret = chgctrl_vote(chip->input_suspend, INPUT_SUSPEND_VOTER_USER, val);
	if (ret)
		return ret;
#else
	val = val ? 0 : -1;
	ret = chgctrl_vote(chip->icl, ICL_VOTER_USER, val);
	if (ret)
		return ret;
#endif

	return count;
}
static DEVICE_ATTR(cmd_input_suspend, 0664,
		cmd_input_suspend_show, cmd_input_suspend_store);

static ssize_t cmd_usb_current_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val = chgctrl_vote_get_value(chip->icl_boost,
			ICL_BOOST_VOTER_USB_CURRENT_MAX);

	return scnprintf(buf, PAGE_SIZE, "%d", val > 0 ? 1 : 0);
}

static ssize_t cmd_usb_current_max_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <=0)
		return -EINVAL;

	val = val ? 900 : -1;
	ret = chgctrl_vote(chip->icl_boost, ICL_BOOST_VOTER_USB_CURRENT_MAX,
			val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(cmd_usb_current_max, 0664,
		cmd_usb_current_max_show, cmd_usb_current_max_store);

static struct attribute *chgctrl_cmd_attrs[] = {
	&dev_attr_cmd_battery_charging_enabled.attr,
	&dev_attr_cmd_charging_enabled.attr,
	&dev_attr_cmd_input_suspend.attr,
	&dev_attr_cmd_usb_current_max.attr,
	NULL,
};
ATTRIBUTE_GROUPS(chgctrl_cmd);

int chgctrl_init_cmd(struct chgctrl *chip)
{
	return devm_device_add_groups(chip->dev, chgctrl_cmd_groups);
}
