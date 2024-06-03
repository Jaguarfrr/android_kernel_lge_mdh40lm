#define pr_fmt(fmt) "[CHGCTRL]%s: " fmt, __func__

#include <linux/device.h>
#include <linux/of.h>
#include <linux/power/charger_controller.h>
#include "charger_controller.h"

/* display */
static int chgctrl_fb_run(struct chgctrl *chip, int event)
{
	struct chgctrl_fb *fb = &chip->fb;

	if (!fb->enabled)
		return 0;

	if (chip->boot_mode != BOOT_MODE_NORMAL)
		return 0;

	if (event & ALGO_EVENT_FRAMEBUFFER) {
		chgctrl_vote(chip->fcc, FCC_VOTER_DISPLAY,
				chip->display_on ? fb->fcc : -1);
	}

	return 0;
}

static int chgctrl_fb_init(struct chgctrl *chip)
{
	struct chgctrl_fb *fb = &chip->fb;

	if (fb->fcc < 0)
		return 0;

	pr_info("fb: fcc=%dmA\n", fb->fcc);

	fb->enabled = true;

	return 0;
}

static int chgctrl_fb_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_fb *fb = &chip->fb;

	of_property_read_u32(np, "fb-fcc", &fb->fcc);

	return 0;
}

static void chgctrl_fb_init_default(struct chgctrl *chip)
{
	struct chgctrl_fb *fb = &chip->fb;

	fb->fcc = -1;
}

struct chgctrl_algo chgctrl_algo_fb = {
	.name = "fb",
	.init_default = chgctrl_fb_init_default,
	.parse_dt = chgctrl_fb_parse_dt,
	.init = chgctrl_fb_init,
	.run = chgctrl_fb_run,
};

/* restricted charging */
enum {
	RESTRICTED_VOTER_LCD,
	RESTRICTED_VOTER_CALL,
	RESTRICTED_VOTER_TDMB,
	RESTRICTED_VOTER_UHDREC,
	RESTRICTED_VOTER_WFD,
};

static const char *restricted_voters[] = {
	[RESTRICTED_VOTER_LCD] = "LCD",
	[RESTRICTED_VOTER_CALL] = "CALL",
	[RESTRICTED_VOTER_TDMB] = "TDMB",
	[RESTRICTED_VOTER_UHDREC] = "UHDREC",
	[RESTRICTED_VOTER_WFD] = "WFD",
};

static int chgctrl_restricted_get_voter(struct chgctrl *chip,
					const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(restricted_voters); i++) {
		if (!strcmp(name, restricted_voters[i]))
			break;
	}
	if (i >= ARRAY_SIZE(restricted_voters))
		return -ENODEV;

	return i;
}

static int chgctrl_restricted_get_value(struct chgctrl *chip,
					const char *name, const char *mode)
{
	struct device_node *np = chip->dev->of_node;
	char propname[30];
	int limit;
	int ret;

	if (!strcmp(mode, "OFF"))
		return -1;

	snprintf(propname, sizeof(propname), "restricted-%s-%s",
			name, mode);

	ret = of_property_read_u32(np, propname, &limit);
	if (ret)
		return -ENOTSUPP;

	return limit;
}

static ssize_t restricted_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int voter;

	voter = chgctrl_vote_active_voter(chip->restricted);
	if (voter < 0)
		return scnprintf(buf, PAGE_SIZE, "none");

	return scnprintf(buf, PAGE_SIZE, "%s", restricted_voters[voter]);
}

static ssize_t restricted_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	char name[10], mode[10];
	int voter, value;
	int ret;

	ret = sscanf(buf, "%s%s", name, mode);
	if (ret != 2)
		return -EINVAL;

	voter = chgctrl_restricted_get_voter(chip, name);
	if (voter == -ENODEV)
		return -ENODEV;

	value = chgctrl_restricted_get_value(chip, name, mode);
	if (value == -ENOTSUPP)
		return -ENOTSUPP;

	ret = chgctrl_vote(chip->restricted, voter, value);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(restricted, 0664, restricted_show, restricted_store);

static void chgctrl_restricted_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);
	int value;

	value = chgctrl_vote_active_value(vote);
	chgctrl_vote(chip->fcc, FCC_VOTER_RESTRICTED, value);
}

static const struct chgctrl_vote_desc restricted_desc = {
	.name = "restricted",
	.type = VOTER_TYPE_MIN,
	.voters = restricted_voters,
	.num_voters = ARRAY_SIZE(restricted_voters),
	.changed = chgctrl_restricted_changed,
};

static int chgctrl_restricted_init(struct chgctrl *chip)
{
	int ret;

	chip->restricted = chgctrl_vote_register(chip->dev,
			&restricted_desc, -1);
	if (IS_ERR(chip->restricted))
		return PTR_ERR(chip->restricted);

	ret = device_create_file(chip->dev, &dev_attr_restricted);
	if (ret)
		return ret;

	return 0;
}

struct chgctrl_algo chgctrl_algo_restricted = {
	.name = "restricted",
	.init = chgctrl_restricted_init,
};

/* Battery Care Charging */
#define BCC_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

static ssize_t bcc_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	int value = chgctrl_vote_get_value(chip->fcc, FCC_VOTER_BCC);

	if (value < 0)
		value = 0;

	return scnprintf(buf, PAGE_SIZE, "%d", value);
}

static ssize_t bcc_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	struct chgctrl_bcc *bcc = &chip->bcc;
	int value;
	int ret;

	ret = sscanf(buf, "%d", &value);
	if (ret != 1)
		return -EINVAL;

	ret = chgctrl_vote(chip->fcc, FCC_VOTER_BCC,
			(value > 0) ? bcc->fcc : -1);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(bcc, BCC_PERM, bcc_show, bcc_store);

static int chgctrl_bcc_init(struct chgctrl *chip)
{
	int ret;

	ret = device_create_file(chip->dev, &dev_attr_bcc);
	if (ret)
		return ret;

	return 0;
}

static int chgctrl_bcc_parse_dt(struct chgctrl *chip,
				struct device_node *np)
{
	struct chgctrl_bcc *bcc = &chip->bcc;

	of_property_read_u32(np, "bcc-current", &bcc->fcc);

	return 0;
}

static void chgctrl_bcc_init_default(struct chgctrl *chip)
{
	struct chgctrl_bcc *bcc = &chip->bcc;

	bcc->fcc = 1800;
}

struct chgctrl_algo chgctrl_algo_bcc = {
	.name = "bcc",
	.init_default = chgctrl_bcc_init_default,
	.parse_dt = chgctrl_bcc_parse_dt,
	.init = chgctrl_bcc_init,
};

/* battery id */
static int chgctrl_battery_id_run(struct chgctrl *chip, int event)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int fcc = -1, vfloat = -1;
	int ret;

	if (!(event & ALGO_EVENT_BATTERY_ID))
		return 0;

	psy = devm_power_supply_get_by_phandle(chip->dev, "battery-id");
	if (IS_ERR_OR_NULL(psy))
		return 0;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_AUTHENTIC, &val);
	if (!ret)
		fcc = val.intval ? -1 : 0;

	if (chip->boot_mode == BOOT_MODE_FACTORY)
		fcc = -1;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (!ret)
		vfloat = val.intval / 1000;

	power_supply_put(psy);

	chgctrl_vote(chip->fcc, FCC_VOTER_BATTERY_ID, fcc);
	chgctrl_vote(chip->vfloat, VFLOAT_VOTER_BATTERY_ID, vfloat);

	return 0;
}

struct chgctrl_algo chgctrl_algo_battery_id = {
	.name = "battery_id",
	.run = chgctrl_battery_id_run,
};

/* factory */
static int chgctrl_factory_run(struct chgctrl *chip, int event)
{
	struct chgctrl_factory *factory = &chip->factory;

	if (chip->boot_mode != BOOT_MODE_FACTORY)
		return 0;

	if (event & ALGO_EVENT_START) {
		chgctrl_vote(chip->icl_boost, ICL_BOOST_VOTER_FACTORY,
				factory->icl);
		chgctrl_vote(chip->fcc, FCC_VOTER_FACTORY,
				factory->fcc);
		chgctrl_vote(chip->fastchg, FASTCHG_VOTER_FACTORY,
				factory->fastchg);
	}

	return 0;
}

static int chgctrl_factory_parse_dt(struct chgctrl *chip,
				    struct device_node *np)
{
	struct chgctrl_factory *factory = &chip->factory;

	of_property_read_u32(np, "factory-icl", &factory->icl);
	of_property_read_u32(np, "factory-fcc", &factory->fcc);
	of_property_read_u32(np, "factory-fastchg", &factory->fastchg);

	return 0;
}

static void chgctrl_factory_init_default(struct chgctrl *chip)
{
	struct chgctrl_factory *factory = &chip->factory;

	factory->icl = 1500;
	factory->fcc = 500;
	factory->fastchg = 0;
}

struct chgctrl_algo chgctrl_algo_factory = {
	.name = "factory",
	.init_default = chgctrl_factory_init_default,
	.parse_dt = chgctrl_factory_parse_dt,
	.run = chgctrl_factory_run,
};

/* ccd no heartbeat W/R */
static void chgctrl_ttf_update_work(struct work_struct *work)
{
	struct chgctrl_ttf *ttf = container_of(work, struct chgctrl_ttf,
			update_work.work);
	struct chgctrl *chip = container_of(ttf, struct chgctrl, ttf);

	chgctrl_changed(chip);
	pr_info("chgctrl_changed\n");
}

static int chgctrl_ttf_run(struct chgctrl *chip, int event)
{
	struct chgctrl_ttf *ttf = &chip->ttf;

	if (event & ALGO_EVENT_CHARGER) {
		if (chip->info.online && (!ttf->pre_online)) {
			pr_info("update_work start\n");
			schedule_delayed_work(&ttf->update_work, msecs_to_jiffies(11000));
		} else if (!chip->info.online) {
			pr_info("offline update_work cancel\n");
			cancel_delayed_work(&ttf->update_work);
		}
		ttf->pre_online = chip->info.online;
	}

	return 0;
}

static int chgctrl_ttf_init(struct chgctrl *chip)
{
	struct chgctrl_ttf *ttf = &chip->ttf;

	INIT_DELAYED_WORK(&ttf->update_work, chgctrl_ttf_update_work);
	ttf->pre_online = 0;

	return 0;
}

struct chgctrl_algo chgctrl_algo_ttf = {
	.name = "ttf",
	.init = chgctrl_ttf_init,
	.run = chgctrl_ttf_run,
};
