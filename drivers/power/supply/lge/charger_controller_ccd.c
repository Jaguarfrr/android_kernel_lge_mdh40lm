#define pr_fmt(fmt) "[CHGCTRL-CCD]%s: " fmt, __func__

#include <linux/power/charger_controller.h>
#include "charger_controller.h"

struct ccd_desc {
	const char *text;
	int value;
};

static ssize_t ccd_icl_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val = chgctrl_vote_get_value(chip->icl, ICL_VOTER_CCD);

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t ccd_icl_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <= 0)
		return -EINVAL;

	ret = chgctrl_vote(chip->icl, ICL_VOTER_CCD, val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(ccd_icl, 0664, ccd_icl_show, ccd_icl_store);

static ssize_t ccd_fcc_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val = chgctrl_vote_get_value(chip->fcc, FCC_VOTER_CCD);

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t ccd_fcc_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <= 0)
		return -EINVAL;

	ret = chgctrl_vote(chip->fcc, FCC_VOTER_CCD, val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(ccd_fcc, 0664, ccd_fcc_show, ccd_fcc_store);

static ssize_t ccd_vfloat_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val = chgctrl_vote_get_value(chip->vfloat, VFLOAT_VOTER_CCD);

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t ccd_vfloat_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <= 0)
		return -EINVAL;

	ret = chgctrl_vote(chip->vfloat, VFLOAT_VOTER_CCD, val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(ccd_vfloat, 0664, ccd_vfloat_show, ccd_vfloat_store);

static ssize_t ccd_fastchg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val = chgctrl_vote_get_value(chip->fastchg, FASTCHG_VOTER_CCD);

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t ccd_fastchg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <= 0)
		return -EINVAL;

	ret = chgctrl_vote(chip->fastchg, FASTCHG_VOTER_CCD, val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(ccd_fastchg, 0664, ccd_fastchg_show, ccd_fastchg_store);

static ssize_t ccd_wless_pwr_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val = chgctrl_vote_get_value(chip->wless_pwr, WLESS_PWR_VOTER_CCD);

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t ccd_wless_pwr_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret <= 0)
		return -EINVAL;

	ret = chgctrl_vote(chip->wless_pwr, WLESS_PWR_VOTER_CCD, val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(ccd_wless_pwr, 0664,
		ccd_wless_pwr_show, ccd_wless_pwr_store);

static const struct ccd_desc health_desc[] = {
	{ .text = "Unknown", .value = POWER_SUPPLY_HEALTH_UNKNOWN },
	{ .text = "Good", .value = POWER_SUPPLY_HEALTH_GOOD },
	{ .text = "Overheat", .value = POWER_SUPPLY_HEALTH_OVERHEAT },
	{ .text = "Cold", .value = POWER_SUPPLY_HEALTH_COLD },
};

static ssize_t ccd_health_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int health = chip->ccd_health;
	int i;

	for (i = 0; i < ARRAY_SIZE(health_desc); i++) {
		if (health != health_desc[i].value)
			continue;

		return scnprintf(buf, PAGE_SIZE, "%s", health_desc[i].text);
	}

	return scnprintf(buf, PAGE_SIZE, "%s", health_desc[0].text);
}

static ssize_t ccd_health_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(health_desc); i++) {
		if (strcmp(buf, health_desc[i].text))
			continue;

		chip->ccd_health = health_desc[i].value;
		return count;
	}

	chip->ccd_health = health_desc[0].value;

	return count;
}
static DEVICE_ATTR(ccd_health, 0664, ccd_health_show, ccd_health_store);

static const struct ccd_desc status_desc[] = {
	{ .text = "Unknown", .value = POWER_SUPPLY_STATUS_UNKNOWN },
	{ .text = "Full", .value = POWER_SUPPLY_STATUS_FULL },
};

static ssize_t ccd_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int status = chip->ccd_status;
	int i;

	for (i = 0; i < ARRAY_SIZE(status_desc); i++) {
		if (status != status_desc[i].value)
			continue;

		return scnprintf(buf, PAGE_SIZE, "%s", status_desc[i].text);
	}

	return scnprintf(buf, PAGE_SIZE, "%s", status_desc[0].text);
}

static ssize_t ccd_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(status_desc); i++) {
		if (strcmp(buf, status_desc[i].text))
			continue;

		chip->ccd_status = status_desc[i].value;
		return count;
	}

	chip->ccd_status = status_desc[0].value;

	return count;
}
static DEVICE_ATTR(ccd_status, 0664, ccd_status_show, ccd_status_store);

static ssize_t ccd_chgtype_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	const char *chgtype = "None";

	switch (chip->bc12_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		break;
	case POWER_SUPPLY_TYPE_USB:
		chgtype = "USB";
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		chgtype = "USB_DCP";
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		chgtype = "USB_CDP";
		break;
	case POWER_SUPPLY_TYPE_APPLE_BRICK_ID:
		chgtype = "USB_DCP";
		break;
	default:
		chgtype = "Unknown";
		break;
	}

	if (chip->typec_pd) {
		pr_info("typec_pd\n");
		switch (chip->typec_usb_type) {
		case POWER_SUPPLY_USB_TYPE_C:
			chgtype = "USB_C";
			break;
		case POWER_SUPPLY_USB_TYPE_PD:
			chgtype = "USB_PD";
			break;
		case POWER_SUPPLY_USB_TYPE_PD_PPS:
			chgtype = "USB_PD_PPS";
			break;
		default:
			break;
		}
	} else {
		pr_info("No typec_pd\n");
	}

	chgtype = chip->floated_charger ? "NON_STD" : chgtype;
	chgtype = chip->fastchg_name ?: chgtype;

	return scnprintf(buf, PAGE_SIZE, "%s", chgtype);

}
static DEVICE_ATTR_RO(ccd_chgtype);

static ssize_t ccd_ttf_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", chip->ccd_ttf);
}

static ssize_t ccd_ttf_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int hour = 0, min = 0, sec = 0;
	bool notify = false;
	int ttf;

	if (sscanf(buf, "%d", &ttf) < 1)
		return -EINVAL;

	if (chip->ccd_ttf == ttf)
		goto out;

	if (ttf < 0) {
		pr_info("set ccd_ttf %d (none)\n", ttf);
		chip->ccd_ttf = ttf;
		goto out;
	}

	/* estimated first */
	if (chip->ccd_ttf < 0)
		notify = true;

	hour = ttf / (60 * 60);
	min = ttf % (60 * 60) / 60;
	sec = ttf % 60;

	pr_info("set ccd_ttf %d (%dhour %02dmin %02dsec)\n",
			ttf, hour, min, sec);
	chip->ccd_ttf = ttf;

	if (notify) {
		pr_info("notify ccd_ttf\n");
		chgctrl_changed(chip);
	}

out:
	return count;
}
static DEVICE_ATTR(ccd_ttf, 0664, ccd_ttf_show, ccd_ttf_store);

static struct attribute *chgctrl_ccd_attrs[] = {
	&dev_attr_ccd_icl.attr,
	&dev_attr_ccd_fcc.attr,
	&dev_attr_ccd_vfloat.attr,
	&dev_attr_ccd_fastchg.attr,
	&dev_attr_ccd_wless_pwr.attr,
	&dev_attr_ccd_health.attr,
	&dev_attr_ccd_status.attr,
	&dev_attr_ccd_chgtype.attr,
	&dev_attr_ccd_ttf.attr,
	NULL,
};
ATTRIBUTE_GROUPS(chgctrl_ccd);

int chgctrl_init_ccd(struct chgctrl *chip)
{
	chip->ccd_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	chip->ccd_status = POWER_SUPPLY_STATUS_UNKNOWN;
	chip->ccd_ttf = -1;

	return devm_device_add_groups(chip->dev, chgctrl_ccd_groups);
}
