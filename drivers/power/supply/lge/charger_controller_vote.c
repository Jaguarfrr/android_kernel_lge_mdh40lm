#define pr_fmt(fmt) "[CHGCTRL-VOTE]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power/charger_controller.h>
#include "charger_controller.h"

static inline int chgctrl_vote_adjust_value(struct chgctrl_vote *vote, int value)
{
	switch (vote->desc->type) {
	case VOTER_TYPE_TRIGGER:
		value = value > 0 ? 1 : 0;
		break;
	case VOTER_TYPE_MAX:
	case VOTER_TYPE_MIN:
	default:
		if (value < 0)
			value = -1;
		break;
	}

	return value;
}

static int chgctrl_vote_find_min(struct chgctrl_vote *vote)
{
	int voter = -1;
	int value = INT_MAX;
	int i;

	for (i = 0; i < vote->desc->num_voters; i++) {
		if (vote->values[i] < 0)
			continue;

		if (vote->values[i] < value) {
			voter = i;
			value = vote->values[i];
		}
	}

	return voter;
}

static int chgctrl_vote_find_max(struct chgctrl_vote *vote)
{
	int voter = -1;
	int value = -1;
	int i;

	for (i = 0; i < vote->desc->num_voters; i++) {
		if (vote->values[i] < 0)
			continue;

		if (vote->values[i] > value) {
			voter = i;
			value = vote->values[i];
		}
	}

	return voter;
}

static int chgctrl_vote_find_trigger(struct chgctrl_vote *vote)
{
	int i;

	for (i = 0; i < vote->desc->num_voters; i++) {
		if (vote->values[i] > 0)
			break;
	}

	if (i >= vote->desc->num_voters)
		return -1;

	return i;
}

static bool chgctrl_vote_update_vote(struct chgctrl_vote *vote)
{
	bool changed = false;
	int voter;
	int value;

	switch (vote->desc->type) {
	case VOTER_TYPE_TRIGGER:
		voter = chgctrl_vote_find_trigger(vote);
		value = ((voter < 0) ? 0 : vote->values[voter]);
		break;
	case VOTER_TYPE_MAX:
		voter = chgctrl_vote_find_max(vote);
		value = ((voter < 0) ? -1 : vote->values[voter]);
		break;
	case VOTER_TYPE_MIN:
		/* default voter type. fall through */
	default:
		voter = chgctrl_vote_find_min(vote);
		value = ((voter < 0) ? -1 : vote->values[voter]);
		break;
	}

	if (voter != vote->active_voter) {
		vote->active_voter = voter;
		changed = true;
	}
	if (value != vote->active_value) {
		vote->active_value = value;
		changed = true;
	}

	return changed;
}

static void chgctrl_vote_changed_work(struct work_struct *work)
{
	struct chgctrl_vote *vote = container_of(work,
			struct chgctrl_vote, changed_work);

	if (!vote->desc->changed)
		return;

	vote->desc->changed(vote);
}

int chgctrl_vote(struct chgctrl_vote *vote, int voter, int value)
{
	bool changed = false;

	if (IS_ERR_OR_NULL(vote))
		return -ENODEV;

	if (voter < 0 || voter >= vote->desc->num_voters)
		return -EINVAL;

	value = chgctrl_vote_adjust_value(vote, value);

	mutex_lock(&vote->lock);
	if (vote->values[voter] == value)
		goto out;

	if (vote->desc->name && vote->desc->voters[voter]) {
		pr_info("%s: %s vote %d\n", vote->desc->name,
				vote->desc->voters[voter], value);
	}

	vote->values[voter] = value;

	changed = chgctrl_vote_update_vote(vote);
	if (!changed)
		goto out;

	if (vote->active_voter < 0) {
		if (vote->desc->name)
			pr_info("%s: no active vote\n", vote->desc->name);
		goto out;
	}

	if (vote->desc->name && vote->desc->voters[vote->active_voter]) {
		pr_info("%s: active vote %d by %s\n", vote->desc->name,
				vote->active_value,
				vote->desc->voters[vote->active_voter]);
	}

out:
	mutex_unlock(&vote->lock);
	if (!changed)
		return 0;

	if (!vote->desc->changed)
		return 0;

	schedule_work(&vote->changed_work);

	return 0;
}

void chgctrl_vote_dump(struct chgctrl_vote *vote)
{
	int unvote;
	int i;

	if (IS_ERR_OR_NULL(vote))
		return;

	if (!vote->desc->name)
		return;

	unvote = chgctrl_vote_adjust_value(vote, -1);
	mutex_lock(&vote->lock);

	for (i = 0; i < vote->desc->num_voters; i++) {
		if (vote->values[i] <= unvote)
			continue;

		pr_info("%s: %s is voting %d\n", vote->desc->name,
				vote->desc->voters[i], vote->values[i]);
	}

	mutex_unlock(&vote->lock);
}

int chgctrl_vote_active_value(struct chgctrl_vote *vote)
{
	if (IS_ERR_OR_NULL(vote))
		return -ENODEV;

	return vote->active_value;
}

int chgctrl_vote_active_voter(struct chgctrl_vote *vote)
{
	if (IS_ERR_OR_NULL(vote))
		return -ENODEV;

	return vote->active_voter;
}

const char *chgctrl_vote_active_voter_name(struct chgctrl_vote *vote)
{
	int voter;

	if (IS_ERR_OR_NULL(vote))
		return NULL;

	voter = vote->active_voter;
	if (voter < 0)
		return NULL;

	return vote->desc->voters[voter];
}

const char *chgctrl_vote_get_voter_name(struct chgctrl_vote *vote, int voter)
{
	if (IS_ERR_OR_NULL(vote))
		return NULL;

	if (voter < 0 || voter >= vote->desc->num_voters)
		return NULL;

	return vote->desc->voters[voter];
}

int chgctrl_vote_get_value(struct chgctrl_vote *vote, int voter)
{
	if (IS_ERR_OR_NULL(vote))
		return -ENODEV;

	if (voter < 0 || voter >= vote->desc->num_voters)
		return -EINVAL;

	return vote->values[voter];
}

struct chgctrl_vote *chgctrl_vote_register(struct device *dev,
					   const struct chgctrl_vote_desc *desc,
					   int init_value)
{
	struct chgctrl_vote *vote;
	int default_value;
	int i;

	if (!desc || !desc->voters || !desc->num_voters)
		return ERR_PTR(-EINVAL);

	vote = devm_kzalloc(dev, sizeof(*vote), GFP_KERNEL);
	if (!vote)
		return ERR_PTR(-ENOMEM);

	vote->values = devm_kmalloc_array(dev, desc->num_voters,
			sizeof(int), GFP_KERNEL);
	if (!vote->values)
		return ERR_PTR(-ENOMEM);

	vote->desc = desc;
	vote->driver_data = dev_get_drvdata(dev);
	mutex_init(&vote->lock);
	INIT_WORK(&vote->changed_work, chgctrl_vote_changed_work);

	default_value = chgctrl_vote_adjust_value(vote, -1);
	for (i = 0; i < desc->num_voters; i++)
		vote->values[i] = default_value;

	vote->active_voter = -1;
	vote->active_value = default_value;

	init_value = chgctrl_vote_adjust_value(vote, init_value);
	if (init_value != default_value)
		vote->values[0] = init_value;

	chgctrl_vote_update_vote(vote);

	return vote;
}

static void chgctrl_icl_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);

	chgctrl_changed(chip);
}

static const char *icl_voters[] = {
	[ICL_VOTER_DEFAULT] = "default",
	[ICL_VOTER_USER] = "user",
	[ICL_VOTER_CCD] = "ccd",
	[ICL_VOTER_RESTRICTED] = "restricted",
	[ICL_VOTER_GAME] = "game",
	[ICL_VOTER_INPUT_SUSPEND] = "input_suspend",
	[ICL_VOTER_PSEUDO_HVDCP] = "pseudo_hvdcp",
};

static const struct chgctrl_vote_desc icl_desc = {
	.name = "icl",
	.type = VOTER_TYPE_MIN,
	.voters = icl_voters,
	.num_voters = ARRAY_SIZE(icl_voters),
	.changed = chgctrl_icl_changed,
};

static void chgctrl_fcc_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);

	chgctrl_changed(chip);
}

static const char *fcc_voters[] = {
	[FCC_VOTER_DEFAULT] = "default",
	[FCC_VOTER_USER] = "user",
	[FCC_VOTER_CCD] = "ccd",
	[FCC_VOTER_OTP] = "otp",
	[FCC_VOTER_SPEC] = "spec",
	[FCC_VOTER_THERMAL] = "thermal",
	[FCC_VOTER_DISPLAY] = "display",
	[FCC_VOTER_RESTRICTED] = "restricted",
	[FCC_VOTER_GAME] = "game",
	[FCC_VOTER_BATTERY_ID] = "battery_id",
	[FCC_VOTER_ATCMD] = "atcmd",
	[FCC_VOTER_FACTORY] = "factory",
	[FCC_VOTER_BCC] = "bcc",
	[FCC_VOTER_QNS] = "qns",
};

static const struct chgctrl_vote_desc fcc_desc = {
	.name = "fcc",
	.type = VOTER_TYPE_MIN,
	.voters = fcc_voters,
	.num_voters = ARRAY_SIZE(fcc_voters),
	.changed = chgctrl_fcc_changed,
};

static void chgctrl_vfloat_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);

	chgctrl_changed(chip);
}

static const char *vfloat_voters[] = {
	[VFLOAT_VOTER_DEFAULT] = "default",
	[VFLOAT_VOTER_USER] = "user",
	[VFLOAT_VOTER_CCD] = "ccd",
	[VFLOAT_VOTER_OTP] = "otp",
	[VFLOAT_VOTER_BATTERY_ID] = "battery_id",
	[VFLOAT_VOTER_QNS] = "qns",
};

static const struct chgctrl_vote_desc vfloat_desc = {
	.name = "vfloat",
	.type = VOTER_TYPE_MIN,
	.voters = vfloat_voters,
	.num_voters = ARRAY_SIZE(vfloat_voters),
	.changed = chgctrl_vfloat_changed,
};

static void chgctrl_icl_boost_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);

	chgctrl_changed(chip);
}

static const char *icl_boost_voters[] = {
	[ICL_BOOST_VOTER_USER] = "user",
	[ICL_BOOST_VOTER_PSEUDO_BATTERY] = "pseudo_battery",
	[ICL_BOOST_VOTER_USB_CURRENT_MAX] = "usb_current_max",
	[ICL_BOOST_VOTER_ATCMD] = "atcmd",
	[ICL_BOOST_VOTER_FACTORY] = "factory",
};

static const struct chgctrl_vote_desc icl_boost_desc = {
	.name = "icl_boost",
	.type = VOTER_TYPE_MAX,
	.voters = icl_boost_voters,
	.num_voters = ARRAY_SIZE(icl_boost_voters),
	.changed = chgctrl_icl_boost_changed,
};

static void chgctrl_input_suspend_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);

	chgctrl_vote(chip->icl, ICL_VOTER_INPUT_SUSPEND,
			chgctrl_vote_active_value(vote) ? 0 : -1);
}

static const char *input_suspend_voters[] = {
	[INPUT_SUSPEND_VOTER_USER] = "user",
	[INPUT_SUSPEND_VOTER_WATER_DETECT] = "water_detect",
	[INPUT_SUSPEND_VOTER_OVER_VOLTAGE] = "over_voltage",
};

static const struct chgctrl_vote_desc input_suspend_desc = {
	.name = "input_suspend",
	.type = VOTER_TYPE_TRIGGER,
	.voters = input_suspend_voters,
	.num_voters = ARRAY_SIZE(input_suspend_voters),
	.changed = chgctrl_input_suspend_changed,
};

static void chgctrl_fastchg_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);

	chgctrl_changed(chip);
}

static const char *fastchg_voters[] = {
	[FASTCHG_VOTER_DEFAULT] = "default",
	[FASTCHG_VOTER_USER] = "user",
	[FASTCHG_VOTER_CCD] = "ccd",
	[FASTCHG_VOTER_NETWORK] = "network",
	[FASTCHG_VOTER_FACTORY] = "factory",
};

static const struct chgctrl_vote_desc fastchg_desc = {
	.name = "fastchg",
	.type = VOTER_TYPE_MIN,
	.voters = fastchg_voters,
	.num_voters = ARRAY_SIZE(fastchg_voters),
	.changed = chgctrl_fastchg_changed,
};

static void chgctrl_wless_pwr_changed(struct chgctrl_vote *vote)
{
	struct chgctrl *chip = chgctrl_vote_get_drvdata(vote);

	chgctrl_changed(chip);
}

static const char *wless_pwr_voters[] = {
	[WLESS_PWR_VOTER_DEFAULT] = "default",
	[WLESS_PWR_VOTER_USER] = "user",
	[WLESS_PWR_VOTER_CCD] = "ccd",
};

static const struct chgctrl_vote_desc wless_pwr_desc = {
	.name = "wless_pwr",
	.type = VOTER_TYPE_MIN,
	.voters = wless_pwr_voters,
	.num_voters = ARRAY_SIZE(wless_pwr_voters),
	.changed = chgctrl_wless_pwr_changed,
};

static int chgctrl_vote_parse_default(struct chgctrl *chip,
				      const struct chgctrl_vote_desc *desc)
{
	const char *propname = desc->name;
	int val = -1;

	of_property_read_u32(chip->dev->of_node, propname, &val);
	if (chip->model_of_node)
		of_property_read_u32(chip->model_of_node, propname, &val);

	return val;
}

int chgctrl_init_vote(struct chgctrl *chip)
{
	chip->icl = chgctrl_vote_register(chip->dev, &icl_desc,
			chgctrl_vote_parse_default(chip, &icl_desc));
	if (IS_ERR(chip->icl))
		return PTR_ERR(chip->icl);

	chip->fcc = chgctrl_vote_register(chip->dev, &fcc_desc,
			chgctrl_vote_parse_default(chip, &fcc_desc));
	if (IS_ERR(chip->fcc))
		return PTR_ERR(chip->fcc);

	chip->vfloat = chgctrl_vote_register(chip->dev, &vfloat_desc,
			chgctrl_vote_parse_default(chip, &vfloat_desc));
	if (IS_ERR(chip->vfloat))
		return PTR_ERR(chip->vfloat);

	chip->icl_boost = chgctrl_vote_register(chip->dev,
			&icl_boost_desc, -1);
	if (IS_ERR(chip->icl_boost))
		return PTR_ERR(chip->icl_boost);

	chip->input_suspend = chgctrl_vote_register(chip->dev,
			&input_suspend_desc, -1);
	if (IS_ERR(chip->input_suspend))
		return PTR_ERR(chip->input_suspend);

	chip->fastchg = chgctrl_vote_register(chip->dev, &fastchg_desc,
			chgctrl_vote_parse_default(chip, &fastchg_desc));
	if (IS_ERR(chip->fastchg))
		return PTR_ERR(chip->fastchg);

	chip->wless_pwr = chgctrl_vote_register(chip->dev, &wless_pwr_desc,
			chgctrl_vote_parse_default(chip, &wless_pwr_desc));
	if (IS_ERR(chip->wless_pwr))
		return PTR_ERR(chip->wless_pwr);

	return 0;
}
