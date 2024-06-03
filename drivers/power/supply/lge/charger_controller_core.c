#define pr_fmt(fmt) "[CHGCTRL]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/power/charger_controller.h>
#include "charger_controller.h"

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#include <linux/power/lge_pseudo_batt.h>
#endif

#ifdef CONFIG_LGE_PM_PSEUDO_HVDCP
#include <linux/power/lge_pseudo_hvdcp.h>
#endif

static inline int mtou(int val)
{
	/* change unit : milli to micro */
	return val > 0 ? val * 1000 : val;
}

static inline int utom(int val)
{
	/* change unit : micro to milli */
	return val > 0 ? val / 1000 : val;
}

/* helper */
static void chgctrl_helper_charger_hook(struct chgctrl_helper *helper,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct chgctrl *chip = helper->chip;

	/* hook value */
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		chip->hook.online = val->intval;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		chip->hook.usb_type = val->intval;
		break;
	default:
		break;
	}

	/* override value */
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (chgctrl_vote_active_value(chip->input_suspend) > 0)
			val->intval = 0;
		break;
	default:
		break;
	}
}

static int chgctrl_override_status(struct chgctrl *chip, int status)
{
	int capacity;

	if (status != POWER_SUPPLY_STATUS_CHARGING &&
			status != POWER_SUPPLY_STATUS_FULL)
		return status;

	if (chip->ccd_status != POWER_SUPPLY_STATUS_UNKNOWN)
		return chip->ccd_status;

	capacity = chgctrl_get_battery_capacity(chip);
	if (capacity >= 100)
		status = POWER_SUPPLY_STATUS_FULL;
	else
		status = POWER_SUPPLY_STATUS_CHARGING;

	return status;
}

static int chgctrl_override_health(struct chgctrl *chip, int health)
{
	if (health != POWER_SUPPLY_HEALTH_GOOD)
		return health;

	if (chip->ccd_health != POWER_SUPPLY_HEALTH_UNKNOWN)
		return chip->ccd_health;

	if (chip->otp.health == POWER_SUPPLY_HEALTH_OVERHEAT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (chip->otp.health == POWER_SUPPLY_HEALTH_COLD)
		return POWER_SUPPLY_HEALTH_COLD;

	return health;
}

static int chgctrl_override_temp(struct chgctrl *chip, int temp)
{
	unsigned int temp_comp = 0;
	int batt_therm_comp = 0;
	int ibat_now = 0;

	if (!chip->info.online)
		return temp;

	if (chip->display_on)
		return temp;

	ibat_now = chgctrl_get_battery_current_now(chip);
	ibat_now /= 1000;
	ibat_now *= -1;

	if (ibat_now < 0)
		return temp;

	temp_comp = ibat_now * ibat_now * chip->batt_therm_comp / 1000000;
	batt_therm_comp = temp - temp_comp;

//	pr_info("report_therm: %d batt_therm ori: %d, temp_comp: %d (%d), ibat: %d\n",
//			batt_therm_comp, temp, temp_comp, chip->batt_therm_comp, ibat_now);

	return batt_therm_comp;

}

static void chgctrl_helper_battery_hook(struct chgctrl_helper *helper,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct chgctrl *chip = helper->chip;

	/* hook value */
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		chip->hook.status = val->intval;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		chip->hook.health = val->intval;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		chip->hook.present = val->intval;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		chip->hook.technology = val->intval;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		chip->hook.voltage_now = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		chip->hook.current_now = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->hook.capacity = val->intval;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		chip->hook.temp = val->intval;
		break;
	default:
		break;
	}

	/* override value */
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chgctrl_override_status(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chgctrl_override_health(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		/* override only if value already exist */
		if (val->intval == 0)
			break;

		val->intval = chip->ccd_ttf > 0 ? chip->ccd_ttf : 0;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chgctrl_override_temp(chip, val->intval);
		break;
	default:
		break;
	}
}

static int chgctrl_helper_get_icl(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return mtou(chgctrl_vote_active_value(chip->icl));
}

static int chgctrl_helper_get_fcc(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return mtou(chgctrl_vote_active_value(chip->fcc));
}

static int chgctrl_helper_get_vfloat(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return mtou(chgctrl_vote_active_value(chip->vfloat));
}

static int chgctrl_helper_get_icl_boost(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return mtou(chgctrl_vote_active_value(chip->icl_boost));
}

static bool chgctrl_helper_get_fastchg(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return chgctrl_vote_active_value(chip->fastchg) ? true : false;
}

static int chgctrl_helper_get_wless_pwr(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return mtou(chgctrl_vote_active_value(chip->wless_pwr));
}

static void chgctrl_helper_set_bc12_type(struct chgctrl_helper *helper,
					 enum power_supply_type type,
					 enum power_supply_usb_type usb_type)
{
	struct chgctrl *chip = helper->chip;

	chip->bc12_type = type;
	chip->bc12_usb_type = usb_type;
}

static void chgctrl_helper_set_typec_usb_type(struct chgctrl_helper *helper,
					      enum power_supply_usb_type usb_type)
{
	struct chgctrl *chip = helper->chip;

	chip->typec_usb_type = usb_type;
}

static void chgctrl_helper_set_fastchg_type(struct chgctrl_helper *helper,
					    const char *type)
{
	struct chgctrl *chip = helper->chip;

	if (!type && !chip->fastchg_name)
		return;

	if (type && chip->fastchg_name) {
		if (!strcmp(type, chip->fastchg_name))
			return;
	}

	chip->fastchg_name = type;

	power_supply_changed(chip->psy);
}

static void chgctrl_helper_set_overvoltage(struct chgctrl_helper *helper,
					   bool overvoltage)
{
	struct chgctrl *chip = helper->chip;

	chgctrl_vote(chip->input_suspend, INPUT_SUSPEND_VOTER_OVER_VOLTAGE,
			overvoltage ? 1 : 0);
}

static void chgctrl_helper_set_input_current_limit(struct chgctrl_helper *helper,
					   int input_current_limit)
{
	struct chgctrl *chip = helper->chip;

        if (!chip->vzw_chg_mode)
            return;

	if (chip->input_current_limit != input_current_limit) {
		chip->input_current_limit = input_current_limit;
		if (chip->charger_psy)
			schedule_work(&chip->psy_charger_work);
	}
}

static bool chgctrl_helper_is_floatchg(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return chip->floated_charger;
}

static bool chgctrl_helper_is_fastchg(struct chgctrl_helper *helper)
{
	struct chgctrl *chip = helper->chip;

	return chip->fastchg_name ? true : false;
}

#ifdef CONFIG_LGE_PM_QNOVO_QNS
static void chgctrl_helper_set_qns_fcc(struct chgctrl_helper *helper,
					   int val)
{
	struct chgctrl *chip = helper->chip;

	chgctrl_vote(chip->fcc, FCC_VOTER_QNS, val);
}
static void chgctrl_helper_set_qns_vfloat(struct chgctrl_helper *helper,
					   int val)
{
	struct chgctrl *chip = helper->chip;

	chgctrl_vote(chip->vfloat, VFLOAT_VOTER_QNS, val);
}
#endif

/* power supply notifier */
static bool chgctrl_bc12_need_retry(struct chgctrl *chip)
{
	if (!chip->bc12_retry_ms)
		return false;
	if (!chip->info.online)
		return false;

	if (chip->bc12_type == POWER_SUPPLY_TYPE_USB_DCP &&
			chip->bc12_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
		return true;

	return false;
}

static void chgctrl_bc12_retry_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct chgctrl *chip = container_of(dwork, struct chgctrl,
			bc12_retry_work);
	union power_supply_propval val = { .intval = 1 };

	if (!chgctrl_bc12_need_retry(chip))
		return;

	pr_info("floated charger. retry bc12 now\n");
	chip->bc12_retry = true;
	power_supply_set_property(chip->charger_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &val);
}

static void chgctrl_update_floated_charger(struct chgctrl *chip)
{
	if (!chgctrl_bc12_need_retry(chip))
		goto out_clear;

	/* floated charger already detected */
	if (chip->floated_charger)
		goto out;

	/* bc12 not retried yet */
	if (chip->bc12_retry) {
		pr_info("floated charger detected\n");
		chip->floated_charger = true;
		chgctrl_changed(chip);
		goto out;
	}

	__pm_stay_awake(chip->bc12_retry_ws);

	pr_info("floated charger. retry bc12 after %ums\n",
			chip->bc12_retry_ms);
	schedule_delayed_work(&chip->bc12_retry_work,
			msecs_to_jiffies(chip->bc12_retry_ms));

	return;

out_clear:
	cancel_delayed_work_sync(&chip->bc12_retry_work);
	chip->bc12_retry = false;
	chip->floated_charger = false;
out:
	__pm_relax(chip->bc12_retry_ws);
}

/* verizon carrier */
enum {
    VZW_NO_CHARGER,
    VZW_NORMAL_CHARGING,
    VZW_INCOMPATIBLE_CHARGING,
    VZW_UNDER_CURRENT_CHARGING,
    VZW_USB_DRIVER_UNINSTALLED,
    VZW_CHARGER_STATUS_MAX,
};
#define SLOW_CHARGING_CURRENT 450
static void chgctrl_update_vzw_charger(struct chgctrl *chip)
{
	int aicl_current;

	if (!chip->info.online) {
		chip->vzw_chg_state = VZW_NO_CHARGER;
		chip->input_current_limit = 3000000;
		pr_info("vzw_chg_state = %d\n", chip->vzw_chg_state);
		return;
	}
	chip->vzw_chg_state = VZW_NORMAL_CHARGING;

	chgctrl_update_floated_charger(chip);
	if (chip->floated_charger) {
		if (chip->typec_usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			chip->vzw_chg_state = VZW_NORMAL_CHARGING;
			pr_info("vzw_chg_state typec_usb_type\n");
		} else {
			chip->vzw_chg_state = VZW_INCOMPATIBLE_CHARGING;
			pr_info("vzw_chg_state = %d\n", chip->vzw_chg_state);
			return;
		}
	}

	/* check slow charger */
	if(chip->info.usb_type == POWER_SUPPLY_USB_TYPE_DCP && chip->info.type == POWER_SUPPLY_TYPE_USB_DCP) {
		aicl_current = utom(chip->input_current_limit);
		pr_info("vzw_chg_state aicl_current = %d\n", aicl_current);
		if (aicl_current < SLOW_CHARGING_CURRENT) {
			chip->vzw_chg_state = VZW_UNDER_CURRENT_CHARGING;
			chgctrl_changed(chip);
		}
	}
	pr_info("vzw_chg_state = %d\n", chip->vzw_chg_state);

}

static void chgctrl_psy_charger_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl,
			psy_charger_work);

	__pm_stay_awake(chip->psy_charger_ws);

	chip->info.online = chgctrl_get_charger_online(chip);
	chip->info.type = chgctrl_get_charger_type(chip);
	chip->info.usb_type = chgctrl_get_charger_usb_type(chip);

	if (chip->vzw_chg_mode)
		chgctrl_update_vzw_charger(chip);
	else
		chgctrl_update_floated_charger(chip);

#ifdef CONFIG_LGE_PM_PSEUDO_HVDCP
	chgctrl_vote(chip->icl, ICL_VOTER_PSEUDO_HVDCP,
			pseudo_hvdcp_is_enabled() ? 1000 : -1);
#endif

	chgctrl_algo_event(chip, ALGO_EVENT_CHARGER);

	__pm_relax(chip->psy_charger_ws);
}

static void chgctrl_psy_battery_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl,
			psy_battery_work);

	__pm_stay_awake(chip->psy_battery_ws);

	chip->info.present = chgctrl_get_battery_present(chip);
	chip->info.status = chgctrl_get_battery_status(chip);
	chip->info.health = chgctrl_get_battery_health(chip);
	chip->info.voltage_now = chgctrl_get_battery_voltage_now(chip);
	chip->info.current_now = chgctrl_get_battery_current_now(chip);
	chip->info.capacity = chgctrl_get_battery_capacity(chip);
	chip->info.temp = chgctrl_get_battery_temp(chip);
	chip->info.technology = chgctrl_get_battery_technology(chip);

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
	chgctrl_vote(chip->icl_boost, ICL_BOOST_VOTER_PSEUDO_BATTERY,
			get_pseudo_batt_info(PSEUDO_BATT_MODE) ? 900 : -1);
#endif

	chgctrl_algo_event(chip, ALGO_EVENT_BATTERY);

	__pm_relax(chip->psy_battery_ws);
}

static int chgctrl_psy_notifier_call(struct notifier_block *nb,
				     unsigned long event, void *v)
{
	struct chgctrl *chip = container_of(nb, struct chgctrl, psy_nb);
	struct power_supply *psy = (struct power_supply *)v;

	if (psy == chip->charger_psy)
		schedule_work(&chip->psy_charger_work);

	if (psy == chip->battery_psy)
		schedule_work(&chip->psy_battery_work);

	return NOTIFY_DONE;
}

static int chgctrl_init_psy_notifier(struct chgctrl *chip)
{
	struct device_node *np = chip->dev->of_node;

	chip->psy_charger_ws = wakeup_source_register(chip->dev,
			"chgctrl_psy_charger");
	INIT_WORK(&chip->psy_charger_work, chgctrl_psy_charger_work);

	chip->psy_battery_ws = wakeup_source_register(chip->dev,
			"chgctrl_psy_battery");
	INIT_WORK(&chip->psy_battery_work, chgctrl_psy_battery_work);

	of_property_read_u32(np, "bc12-retry-ms", &chip->bc12_retry_ms);
	chip->bc12_retry_ws = wakeup_source_register(chip->dev,
			"chgctrl_bc12_retry");
	INIT_DELAYED_WORK(&chip->bc12_retry_work, chgctrl_bc12_retry_work);

	chip->psy_nb.notifier_call = chgctrl_psy_notifier_call;
	power_supply_reg_notifier(&chip->psy_nb);

	return 0;
}

/* frame buffer notifier */
static void chgctrl_fb_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl, fb_work);

	chgctrl_algo_event(chip, ALGO_EVENT_FRAMEBUFFER);
}

static int chgctrl_fb_notifier_call(struct notifier_block *nb,
				    unsigned long event, void *v)
{
	struct chgctrl *chip = container_of(nb, struct chgctrl, fb_nb);
	struct fb_event *ev = (struct fb_event *)v;
	bool display_on = false;

	if (event != FB_EVENT_BLANK)
		return NOTIFY_DONE;

	if (!ev || !ev->data)
		return NOTIFY_DONE;

	if (*(int*)ev->data == FB_BLANK_UNBLANK)
		display_on = true;

	if (chip->display_on == display_on)
		return NOTIFY_DONE;

	chip->display_on = display_on;
	pr_info("display %s\n", (display_on ? "on" : "off"));

	schedule_work(&chip->fb_work);

	return NOTIFY_DONE;
}

static int chgctrl_init_fb_notifier(struct chgctrl *chip)
{
	INIT_WORK(&chip->fb_work, chgctrl_fb_work);
	chip->fb_nb.notifier_call = chgctrl_fb_notifier_call;
	fb_register_client(&chip->fb_nb);

	return 0;
}

/* power supply */
static enum power_supply_property chgctrl_properties[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static int chgctrl_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chgctrl_helper *helper = power_supply_get_drvdata(psy);
	struct chgctrl *chip = helper->chip;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = -1;
		if (!chgctrl_vote_get_value(chip->fastchg,
				FASTCHG_VOTER_DEFAULT))
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = -1;
		if (!chgctrl_vote_active_value(chip->fastchg))
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chgctrl_vote_get_value(chip->icl,
				ICL_VOTER_DEFAULT);
		val->intval = mtou(val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chgctrl_vote_active_value(chip->icl);
		val->intval = mtou(val->intval);
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		val->intval = chgctrl_vote_active_value(chip->wless_pwr);
		val->intval = mtou(val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = chgctrl_vote_active_value(chip->fcc);
		val->intval = mtou(val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = chgctrl_vote_get_value(chip->fcc,
				FCC_VOTER_DEFAULT);
		val->intval = mtou(val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = chgctrl_vote_active_value(chip->vfloat);
		val->intval = mtou(val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = chgctrl_vote_get_value(chip->vfloat,
				VFLOAT_VOTER_DEFAULT);
		val->intval = mtou(val->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = chip->ccd_ttf;
		break;
	default:
		return -ENODATA;
	}

	return 0;
}

static int chgctrl_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct chgctrl_helper *helper = power_supply_get_drvdata(psy);
	struct chgctrl *chip = helper->chip;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = chgctrl_vote(chip->fastchg, FASTCHG_VOTER_DEFAULT,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = chgctrl_vote(chip->fastchg, FASTCHG_VOTER_USER,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = chgctrl_vote(chip->icl, ICL_VOTER_DEFAULT,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = chgctrl_vote(chip->icl, ICL_VOTER_USER,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		ret = chgctrl_vote(chip->wless_pwr, WLESS_PWR_VOTER_USER,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = chgctrl_vote(chip->fcc, FCC_VOTER_USER,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		ret = chgctrl_vote(chip->fcc, FCC_VOTER_DEFAULT,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = chgctrl_vote(chip->vfloat, VFLOAT_VOTER_USER,
				utom(val->intval));
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = chgctrl_vote(chip->vfloat, VFLOAT_VOTER_DEFAULT,
				utom(val->intval));
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int chgctrl_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property psp)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = 1;
		break;
	default:
		break;
	}

	return ret;
}

static char *chgctrl_supplied_to[] = {
	"mtk-master-charger",
};

static int chgctrl_init_power_supply(struct chgctrl *chip)
{
	struct power_supply_config psy_cfg = {
		.drv_data = chip->helper,
		.of_node = chip->dev->of_node,
		.supplied_to = chgctrl_supplied_to,
		.num_supplicants = ARRAY_SIZE(chgctrl_supplied_to),
	};
	struct device_node *np = chip->dev->of_node;

	chip->psy_desc.name = of_get_property(np, "power-supply-name", NULL);
	chip->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	chip->psy_desc.properties = chgctrl_properties;
	chip->psy_desc.num_properties = ARRAY_SIZE(chgctrl_properties);
	chip->psy_desc.get_property = chgctrl_get_property;
	chip->psy_desc.set_property = chgctrl_set_property;
	chip->psy_desc.property_is_writeable = chgctrl_property_is_writeable;

	chip->psy = devm_power_supply_register(chip->dev, &chip->psy_desc,
				&psy_cfg);
	if (IS_ERR(chip->psy)) {
		pr_err("failed to init power_supply\n");
		return PTR_ERR(chip->psy);
	}

	return 0;
}

static int chgctrl_init_helper(struct chgctrl *chip)
{
	struct chgctrl_helper *helper;

	helper = devm_kzalloc(chip->dev, sizeof(struct chgctrl_helper),
			GFP_KERNEL);
	if (!helper) {
		pr_err("failed to alloc memory for helper\n");
		return -ENOMEM;
	}

	helper->chip = chip;

	helper->charger_hook = chgctrl_helper_charger_hook;
	helper->battery_hook = chgctrl_helper_battery_hook;

	helper->get_icl = chgctrl_helper_get_icl;
	helper->get_fcc = chgctrl_helper_get_fcc;
	helper->get_vfloat = chgctrl_helper_get_vfloat;
	helper->get_icl_boost = chgctrl_helper_get_icl_boost;
	helper->get_fastchg = chgctrl_helper_get_fastchg;
	helper->get_wless_pwr = chgctrl_helper_get_wless_pwr;

	helper->set_bc12_type = chgctrl_helper_set_bc12_type;
	helper->set_typec_usb_type = chgctrl_helper_set_typec_usb_type;
	helper->set_fastchg_type = chgctrl_helper_set_fastchg_type;

	helper->set_overvoltage = chgctrl_helper_set_overvoltage;
	helper->set_input_current_limit = chgctrl_helper_set_input_current_limit;

	helper->is_floatchg = chgctrl_helper_is_floatchg;
	helper->is_fastchg = chgctrl_helper_is_fastchg;

#ifdef CONFIG_LGE_PM_QNOVO_QNS
	helper->set_qns_fcc = chgctrl_helper_set_qns_fcc;
	helper->set_qns_vfloat = chgctrl_helper_set_qns_vfloat;
#endif

	chip->helper = helper;

	return 0;
}

extern char *lge_get_model_name(void);	/* from device_lge.h */
static struct device_node *chgctrl_get_model_of_node(struct chgctrl *chip)
{
	const char *name = lge_get_model_name();
	struct device_node *np = chip->dev->of_node;
	struct device_node *model_np = NULL;

	pr_info("[MODEL] init %s Default Setting \n", name);
	if (!name)
		return NULL;
	model_np = of_get_child_by_name(np, "model");
	if (!model_np) {
		pr_info("model Node is NULL\n");
		return NULL;
	}

	pr_info("[MODEL] %s Default Setting \n", name);

	return model_np;
}

static struct device_node *chgctrl_model_find_setting(struct device_node *node)
{
	struct device_node *setting = NULL;
	const char *name;
	const char *index;
	int arr_cnt, arr_num;

	/* parse from group */
	arr_cnt = of_property_count_strings(node, "group");
	if (arr_cnt > 0) {
		pr_info("[MODEL] Total model is %d\n", arr_cnt/2);
	} else {
		pr_info("[MODEL] ERROR group isn't exist\n");
		return NULL;
	}

	/* compare model name */
	name = lge_get_model_name();
	if (!name)
		return NULL;

	for (arr_num = 0; arr_num < arr_cnt; arr_num++) {
		of_property_read_string_index(node, "group", arr_num++, &index);
		if (!strcmp(name, index)) {
			of_property_read_string_index(node, "group", arr_num, &index);
			pr_info("[MODEL] %s %s \n", name, index);
			setting = of_get_child_by_name(node, index);
			return setting;
		}
	}

	pr_info("[MODEL] %s Default Setting \n", name);

	return NULL;
}

static int chgctrl_parse_dt(struct chgctrl *chip)
{
	struct device_node *np = chip->dev->of_node;

	of_property_read_u32(np, "batt_therm_comp", &chip->batt_therm_comp);

	chip->typec_pd = of_property_read_bool(np, "typec_pd");

	return 0;
}

static int chgctrl_vzw_chg_parse_dt(struct chgctrl *chip, struct device_node *node)
{
	chip->vzw_chg_mode = of_property_read_bool(node, "vzw_chg_mode");
	if (chip->vzw_chg_mode)
		pr_info("vzw_chg_mode\n");
	if (chip->vzw_chg_mode)
		return 0;

	chip->vzw_chg_mode = of_property_read_bool(node, "slow_chg_mode");
	if (chip->vzw_chg_mode)
		pr_info("slow_chg_mode\n");

	return 0;
}

static int chgctrl_probe(struct platform_device *pdev)
{
	struct chgctrl *chip;
	struct power_supply *charger_psy, *battery_psy;
	int ret;

	charger_psy = devm_power_supply_get_by_phandle(&pdev->dev, "charger");
	if (IS_ERR_OR_NULL(charger_psy))
		return -EPROBE_DEFER;

	battery_psy = devm_power_supply_get_by_phandle(&pdev->dev, "battery");
	if (IS_ERR_OR_NULL(battery_psy))
		return -EPROBE_DEFER;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct chgctrl), GFP_KERNEL);
	if (!chip) {
		pr_err("failed to alloc memory for chip\n");
		return -ENOMEM;
	}
	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	chip->charger_psy = charger_psy;
	chip->battery_psy = battery_psy;
	chip->model_of_node = chgctrl_get_model_of_node(chip);
	if (chip->model_of_node)
		chip->setting_of_node = chgctrl_model_find_setting(chip->model_of_node);
	chip->boot_mode = chgctrl_get_boot_mode(chip);

	ret = chgctrl_parse_dt(chip);
	if (ret) {
		pr_err("failed to parse dt\n");
		return ret;
	}

	if (chip->setting_of_node) {
		ret = chgctrl_vzw_chg_parse_dt(chip, chip->setting_of_node);
		if (ret) {
			pr_err("failed to vzw_chg parse dt\n");
			return ret;
		}
	}

	ret = chgctrl_init_vote(chip);
	if (ret) {
		pr_err("failed to init vote\n");
		return ret;
	}

	ret = chgctrl_init_helper(chip);
	if (ret) {
		pr_err("failed to init helper\n");
		return ret;
	}

	ret = chgctrl_init_power_supply(chip);
	if (ret) {
		pr_err("failed to init power_supply\n");
		return ret;
	}

	ret = chgctrl_init_algo(chip);
	if (ret) {
		pr_err("failed to init algo\n");
		return ret;
	}

	ret = chgctrl_init_psy_notifier(chip);
	if (ret)
		pr_err("failed to init psy_notifier\n");

	ret = chgctrl_init_fb_notifier(chip);
	if (ret)
		pr_err("failed to init fb_notifier\n");

	ret = chgctrl_init_info(chip);
	if (ret)
		pr_err("failed to init info\n");

	ret = chgctrl_init_cmd(chip);
	if (ret)
		pr_err("failed to init cmd\n");

	ret = chgctrl_init_ccd(chip);
	if (ret)
		pr_err("failed to init ccd\n");

	ret = chgctrl_init_atcmd(chip);
	if (ret)
		pr_err("failed to init atcmd\n");

	chgctrl_algo_event(chip, ALGO_EVENT_START);

	return 0;
}

static struct of_device_id chgctrl_match_table[] = {
	{
		.compatible = "lge,charger-controller",
	},
	{ },
};

static struct platform_driver chgctrl_driver = {
	.probe = chgctrl_probe,
	.driver = {
		.name = "charger-controller",
		.owner = THIS_MODULE,
		.of_match_table = chgctrl_match_table,
	},
};

module_platform_driver(chgctrl_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Charger IC Current Controller");
MODULE_VERSION("1.2");
