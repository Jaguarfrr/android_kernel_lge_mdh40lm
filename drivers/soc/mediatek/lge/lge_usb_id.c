#define pr_fmt(fmt) "[USB_ID]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/reboot.h>

/* PD */
#include <tcpm.h>

#ifdef CONFIG_MACH_LGE
#include <soc/mediatek/lge/board_lge.h>
#endif
#ifdef CONFIG_LGE_HANDLE_PANIC
#include <soc/mediatek/lge/lge_handle_panic.h>
#endif
#include <soc/mediatek/lge/lge_usb_id.h>

struct usb_id_table {
	int adc_min;
	int adc_max;
	int type;
};

struct lge_usb_id {
	struct device *dev;
	struct iio_channel *channel;

	struct work_struct update_work;
	struct work_struct reboot_work;
	struct mutex lock;

	int bootmode;

	int type;
	int voltage;

	struct power_supply *psy;
	struct notifier_block psy_nb;
	int online;

	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	bool typec_debug;

	struct timespec last_offline_time;
	bool usb_configured;

	/* device configuration */
	unsigned int transition_delay;
	unsigned int delay;

	/* options */
	bool embedded_battery;
	int debounce;

	struct usb_id_table *table;
	int table_size;
};

static struct lge_usb_id *g_chip = NULL;

void lge_usb_id_set_usb_configured(bool configured)
{
	struct lge_usb_id *chip = g_chip;

	if (!chip) {
		pr_err("not ready\n");
		return;
	}

	if (!configured)
		return;

	if (chip->usb_configured)
		return;
	chip->usb_configured = true;

	if (!chip->embedded_battery)
		return;

	switch (chip->type) {
	case 56:
	case 910:
		schedule_work(&chip->reboot_work);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(lge_usb_id_set_usb_configured);

static int usb_id_get_voltage(struct lge_usb_id *chip, int *mV)
{
	return iio_read_channel_processed(chip->channel, mV);
}

#define MAX_USB_VOLTAGE_COUNT 4
#define CABLE_VOLTAGE_DIFFERENCE 100
#define NORMAL_CABLE_VOLTAGE 1800
static int lge_read_factory_cable_voltage(struct lge_usb_id *chip, int *voltage)
{
	bool normal_case = false;
	int i = 0, cable_voltage = 0;
	int cable_voltage_data[2] = {0};

	do {
		if (i != 0) msleep(10);

		if (usb_id_get_voltage(chip, &cable_voltage) < 0)
			return -1;
		cable_voltage_data[0] = cable_voltage;

		msleep(20);

		if (usb_id_get_voltage(chip, &cable_voltage) < 0)
			return -1;

		cable_voltage_data[1] = cable_voltage;

		if (abs(cable_voltage_data[1] - cable_voltage_data[0])
				< CABLE_VOLTAGE_DIFFERENCE) {
			normal_case = true;
			break;
		}
	} while (!normal_case && (++i < MAX_USB_VOLTAGE_COUNT));

	*voltage = cable_voltage;

	return 0;
}

static int lge_read_check_cable_voltage(struct lge_usb_id *chip, int *voltage)
{
	bool abnormal_cable = false;
	int i = 0, j = 0, cable_voltage = 0;
	int cable_voltage_data[MAX_USB_VOLTAGE_COUNT] = {0};

	do {
		if (i != 0) msleep(10);

		if (usb_id_get_voltage(chip, &cable_voltage) < 0)
			return -1;

		cable_voltage_data[i] = cable_voltage;

		for (j = 1; j < i + 1; j++) {
			/* Assume that the cable is normal when the differences are over 100 mV */
			if (abs(cable_voltage_data[i] - cable_voltage_data[i-j])
					> CABLE_VOLTAGE_DIFFERENCE) {
				abnormal_cable = true;
				cable_voltage = NORMAL_CABLE_VOLTAGE;
				break;
			}
		}
	} while (!abnormal_cable && (++i < MAX_USB_VOLTAGE_COUNT));

	*voltage = cable_voltage;

	return 0;
}

static int usb_id_read_voltage(struct lge_usb_id *chip, int *voltage)
{
	if (!chip->embedded_battery)
		return usb_id_get_voltage(chip, voltage);

	/* embedded battery */
	if (lge_is_factory_cable_boot())
		return lge_read_factory_cable_voltage(chip, voltage);

	return lge_read_check_cable_voltage(chip, voltage);
}

static void usb_id_read_pre(struct lge_usb_id *chip)
{
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select(chip->dev, "transition");
	if (!IS_ERR(pinctrl) && chip->transition_delay)
		msleep(chip->transition_delay);

	devm_pinctrl_get_select(chip->dev, "adc");

	/* wait for adc voltage stabilized */
	if (chip->delay)
		msleep(chip->delay);
}

static void usb_id_read_post(struct lge_usb_id *chip)
{
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select(chip->dev, "transition");
	if (!IS_ERR(pinctrl) && chip->transition_delay)
		msleep(chip->transition_delay);

	devm_pinctrl_get_select(chip->dev, "default");
}

static int usb_id_find_type(struct lge_usb_id *chip, int adc)
{
	int i;

	/* if valid table not exist, just return as normal */
	if (!chip->table || !chip->table_size)
		return 0;

	for (i = 0; i < chip->table_size; i++) {
		/* found matched cable id */
		if (adc >= chip->table[i].adc_min
				&& adc <= chip->table[i].adc_max) {
			return chip->table[i].type;
		}
	}

	return -EINVAL;
}

static bool usb_id_is_cable_valid(struct lge_usb_id *chip)
{
	if (!chip->online)
		return false;

	/* do not read adc if cable is not for debug */
	if (chip->tcpc && !chip->typec_debug)
		return false;

	return true;
}

static void usb_id_update(struct work_struct *work)
{
	struct lge_usb_id *chip = container_of(work, struct lge_usb_id,
			update_work);
	int cnt, retry_cnt = 3;
	int voltage, type;
	int ret;

	mutex_lock(&chip->lock);

	if (!usb_id_is_cable_valid(chip)) {
		chip->type = 0;
		chip->voltage = 0;
		goto out_update;
	}

	usb_id_read_pre(chip);

	for (cnt = 0; cnt < retry_cnt; cnt++) {
		if (!usb_id_is_cable_valid(chip)) {
			chip->type = 0;
			chip->voltage = 0;
			break;
		}

		ret = usb_id_read_voltage(chip, &voltage);
		if (ret) {
			msleep(50);
			continue;
		}

		pr_info("usb id voltage = %dmV\n", voltage);

		type = usb_id_find_type(chip, voltage);
		if (type < 0) {
			msleep(50);
			continue;
		}

		/* found type. exit loop */
		if (type)
			pr_info("usb id = LT_%dK\n", type);
		chip->type = type;
		chip->voltage = voltage;

		break;
	}

	usb_id_read_post(chip);

out_update:
	mutex_unlock(&chip->lock);

	if (chip->embedded_battery)
		schedule_work(&chip->reboot_work);

	return;
}

static void usb_id_reboot_56k(struct lge_usb_id *chip)
{
	/* do not reboot except normal boot. NORMAL_BOOT = 0 */
	if (chip->bootmode != 0)
		return;

	if (lge_is_factory_cable_boot())
		return;

	pr_info("[FACTORY] PIF_56K detected in NORMAL BOOT, reboot!!\n");

	/* wait for usb configuration */
	msleep(500);
	kernel_restart("LGE Reboot by PIF 56k");
}

static void usb_id_reboot_910k(struct lge_usb_id *chip, struct timespec now)
{
	struct timespec diff = timespec_sub(now, chip->last_offline_time);

	/* do not reboot in recovery boot. RECOVERY_BOOT = 2 */
	if (chip->bootmode == 2)
		return;

#ifdef CONFIG_MACH_LGE
	/* do not reboot in laf mode */
	if (lge_get_laf_mode()) {
		pr_info("in LAF. ignore\n");
		return;
	}
#endif

	if (lge_usb_id_boot_type() == 910) {
		/* do not reboot before plug-out */
		if (!chip->last_offline_time.tv_sec)
			return;
		/* do not reboot if usb not configured yet */
		if (!chip->usb_configured)
			return;
	}

	/* do not reboot if cable re-plugged too fast */
	if (diff.tv_sec <= chip->debounce) {
		pr_info("reconnected within %ldsec. ignore\n", diff.tv_sec);
		return;
	}

	pr_info("[FACTORY] PIF_910K detected, reboot!!\n");

	/* wait for usb configuration */
	msleep(500);
#ifdef CONFIG_LGE_HANDLE_PANIC
	lge_set_reboot_reason(LGE_REBOOT_REASON_DLOAD);
#endif
	kernel_restart("LGE Reboot by PIF 910k");
}

static void usb_id_reboot(struct work_struct *work)
{
	struct lge_usb_id *chip = container_of(work, struct lge_usb_id,
			reboot_work);
	struct timespec now;

	get_monotonic_boottime(&now);

	/* mark offline time */
	if (!chip->online) {
		chip->last_offline_time = now;
		return;
	}

	if (lge_usb_id_boot_type() < 0) {
		pr_err("boot type not exist. ignore reboot\n");
		return;
	}

	switch (chip->type) {
	case 56:
		usb_id_reboot_56k(chip);
		break;
	case 910:
		usb_id_reboot_910k(chip, now);
		break;
	default:
		break;
	}
}

static int usb_id_psy_notifier_call(struct notifier_block *nb,
				      unsigned long event, void *v)
{
	struct lge_usb_id *chip = container_of(nb, struct lge_usb_id, psy_nb);
	struct power_supply *psy = (struct power_supply*)v;
	union power_supply_propval val;
	int ret;

	if (psy != chip->psy)
		return NOTIFY_DONE;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
	if (ret)
		return NOTIFY_DONE;

	/* force set offline if not usb */
	if (psy->desc->type != POWER_SUPPLY_TYPE_USB
			&& psy->desc->type != POWER_SUPPLY_TYPE_USB_CDP)
		val.intval = 0;

	if (val.intval == chip->online)
		return NOTIFY_DONE;

	chip->online = val.intval;
	pr_info("usb %s\n", chip->online ? "in" : "out");

	schedule_work(&chip->update_work);

	return NOTIFY_DONE;
}

#ifdef CONFIG_TCPC_CLASS
static int usb_id_pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct lge_usb_id *chip = container_of(nb, struct lge_usb_id, pd_nb);
	struct tcp_notify *noti = data;

	if (event != TCP_NOTIFY_TYPEC_STATE)
		return NOTIFY_OK;

	switch (noti->typec_state.new_state) {
	case TYPEC_ATTACHED_DEBUG:
	case TYPEC_ATTACHED_DBGACC_SNK:
	case TYPEC_ATTACHED_CUSTOM_SRC:
		if (!chip->typec_debug) {
			chip->typec_debug = true;
			pr_info("debug accessory attached\n");
		}

		break;
	default:
		if (chip->typec_debug) {
			pr_info("debug accessory dettached\n");
			chip->typec_debug = false;
		}
		break;
	}

	return NOTIFY_OK;
}
#endif

static ssize_t voltage_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct lge_usb_id *chip = dev_get_drvdata(dev);
	int voltage, ret;

	mutex_lock(&chip->lock);

	usb_id_read_pre(chip);

	ret = usb_id_read_voltage(chip, &voltage);
	if (ret)
		voltage = 0;

	usb_id_read_post(chip);

	mutex_unlock(&chip->lock);

	return scnprintf(buf, PAGE_SIZE, "%d", voltage * 1000);
}
static DEVICE_ATTR_RO(voltage);

static ssize_t type_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct lge_usb_id *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", chip->type);
}
static DEVICE_ATTR_RO(type);

static ssize_t boot_type_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", lge_usb_id_boot_type());
}
static DEVICE_ATTR_RO(boot_type);

/*
 * AT%USBIDADC
 * - output : adc,id
 * - /proc/lge_power/testmode/usb_id
 */
static ssize_t atcmd_usb_id_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct lge_usb_id *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d,%d", chip->voltage, chip->type);
}
static DEVICE_ATTR_RO(atcmd_usb_id);

static struct attribute *usb_id_attrs[] = {
	&dev_attr_voltage.attr,
	&dev_attr_type.attr,
	&dev_attr_boot_type.attr,
	&dev_attr_atcmd_usb_id.attr,
	NULL,
};
ATTRIBUTE_GROUPS(usb_id);

static void usb_id_dump_info(struct lge_usb_id *chip)
{
	int i;

	pr_info("delay = %d ms\n", chip->delay);
	if (chip->embedded_battery) {
		pr_info("embedded battery mode with %d sec debounce\n",
				chip->debounce);
	}

	for (i = 0; i < chip->table_size; i++) {
		if (!chip->table[i].type)
			continue;

		pr_info("LT_%dK = %dmV to %dmV\n", chip->table[i].type,
			chip->table[i].adc_min, chip->table[i].adc_max);
	}
}

static int usb_id_get_bootmode(struct lge_usb_id *chip)
{
	struct device_node *np = chip->dev->of_node;
	int bootmode = -ENODEV;
	struct tag_bootmode {
		u32 size;
		u32 tag;
		u32 bootmode;
		u32 boottype;
	} *tag;

	np = of_parse_phandle(np, "bootmode", 0);
	if (!np)
		goto out;

	tag = (struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
	if (tag)
		bootmode = tag->bootmode;

	of_node_put(np);

	pr_info("boot mode = %d\n", bootmode);

out:
	return bootmode;
}

static int usb_id_parse_dt(struct lge_usb_id *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct property *prop = NULL;
	const __be32 *data = NULL;
	int size;
	int ret;
	int i;

	ret = of_property_read_u32(node, "delay", &chip->delay);
	if (ret)
		chip->delay = 0;

	ret = of_property_read_u32(node, "transition-delay",
			&chip->transition_delay);
	if (ret)
		chip->transition_delay = 0;

	chip->embedded_battery =
			of_property_read_bool(node, "embedded-battery");
	ret = of_property_read_u32(node, "debounce", &chip->debounce);
	if (ret)
		chip->debounce = 5;	/* default : 5sec */

	prop = of_find_property(node, "range", &size);
	if (!prop)
		return -ENODATA;

	/* invalid data size */
	if (!size || size % sizeof(struct usb_id_table))
		return -EINVAL;

	chip->table_size = size / sizeof(struct usb_id_table);
	chip->table = (struct usb_id_table *)devm_kzalloc(chip->dev, size,
			GFP_KERNEL);
	if (!chip->table)
		return -ENOMEM;

	for (i = 0; i < chip->table_size; i++) {
		data = of_prop_next_u32(prop, data, &chip->table[i].adc_min);
		data = of_prop_next_u32(prop, data, &chip->table[i].adc_max);
		data = of_prop_next_u32(prop, data, &chip->table[i].type);
	}

	return 0;
}

static int usb_id_probe(struct platform_device *pdev)
{
	struct lge_usb_id *chip = NULL;
	int ret = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct lge_usb_id),
			GFP_KERNEL);
	if (!chip) {
		pr_err("failed to alloc memory\n");
		return -ENOMEM;
	}
	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	mutex_init(&chip->lock);
	INIT_WORK(&chip->update_work, usb_id_update);
	INIT_WORK(&chip->reboot_work, usb_id_reboot);

	ret = usb_id_parse_dt(chip);
	if (ret) {
		pr_err("failed to parse device-tree\n");
		return ret;
	}

	chip->bootmode = usb_id_get_bootmode(chip);
	chip->channel = devm_iio_channel_get(&pdev->dev, "usb-id");
	if (IS_ERR(chip->channel)) {
		pr_err("fail to get iio channel\n");
		return -ENODEV;
	}

#ifdef CONFIG_TCPC_CLASS
	chip->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!chip->tcpc)
		return -EPROBE_DEFER;

	chip->pd_nb.notifier_call = usb_id_pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(chip->tcpc, &chip->pd_nb,
					TCP_NOTIFY_TYPE_USB);
#endif

	chip->psy = devm_power_supply_get_by_phandle(&pdev->dev, "charger");
	if (IS_ERR_OR_NULL(chip->psy))
		return -EPROBE_DEFER;

	/* power supply notifier */
	chip->psy_nb.notifier_call = usb_id_psy_notifier_call;
	ret = power_supply_reg_notifier(&chip->psy_nb);
	if (ret) {
		pr_err("failed to register notifier for power_supply\n");
		return ret;
	}

	ret = devm_device_add_groups(chip->dev, usb_id_groups);
	if (ret) {
		pr_err("failed to add group\n");
		return ret;
	}

	usb_id_dump_info(chip);

	g_chip = chip;

	return 0;
}

static int usb_id_remove(struct platform_device *pdev)
{
	struct lge_usb_id *chip = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&chip->psy_nb);

	return 0;
}

static struct of_device_id usb_id_match_table[] = {
	{ .compatible = "lge,usb-id", },
	{ },
};

static struct platform_driver usb_id_driver = {
	.probe = usb_id_probe,
	.remove = usb_id_remove,
	.driver = {
		.name = "usb-id",
		.owner = THIS_MODULE,
		.of_match_table = usb_id_match_table,
	},
};

module_platform_driver(usb_id_driver);

/* boot cable id inforamtion */
/* CAUTION: These strings are come from LK. */
static char *boot_cable_str[] = {
	" "," "," "," "," "," ",
	"LT_56K",
	"LT_130K",
	"400MA",
	"DTC_500MA",
	"Abnormal_400MA",
	"LT_910K",
	"NO_INIT",
};

static usb_cable_type boot_cable = NO_INIT_CABLE;
static int boot_type = -ENODEV;

int lge_usb_id_boot_type(void)
{
	return boot_type;
}
EXPORT_SYMBOL(lge_usb_id_boot_type);

usb_cable_type lge_get_board_cable(void)
{
	return boot_cable;
}
EXPORT_SYMBOL(lge_get_board_cable);

bool lge_is_factory_cable_boot(void)
{
	return (boot_type > 0) ? true : false;
}
EXPORT_SYMBOL(lge_is_factory_cable_boot);

static int __init fdt_find_boot_cable(unsigned long node, const char *uname,
	int depth, void *data)
{
	usb_cable_type cable;
	char *cable_str;

	if (depth != 1)
		return 0;

	if (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0)
		return 0;

	cable_str = (char*)of_get_flat_dt_prop(node, "lge,boot-cable", NULL);
	if (!cable_str)
		return 0;

	for (cable = LT_CABLE_56K; cable <= NO_INIT_CABLE; cable++) {
		if (!strcmp(cable_str, boot_cable_str[cable])) {
			boot_cable = cable;
			break;
		}
	}

	switch (boot_cable) {
	case LT_CABLE_56K:
		boot_type = 56;
		break;
	case LT_CABLE_130K:
		boot_type = 130;
		break;
	case LT_CABLE_910K:
		boot_type = 910;
		break;
	default:
		boot_type = 0;
		break;
	}

	pr_info("boot cable = %s\n", boot_cable_str[boot_cable]);

	return 1;
}

static int __init lge_usb_id_pure_init(void)
{
	int rc;

	rc = of_scan_flat_dt(fdt_find_boot_cable, NULL);
	if (!rc)
		pr_err("boot cable not found\n");

	return 0;
}
pure_initcall(lge_usb_id_pure_init);

MODULE_DESCRIPTION("LG Electronics USB ID");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL");
