#define pr_fmt(fmt) "[ATD_MID] %s: " fmt, __func__

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/kthread.h>
#ifdef CONFIG_LGE_BOOT_MODE
#include <soc/mediatek/lge/lge_boot_mode.h>
#endif

static struct wakeup_source *atd_poweroff_ws = NULL;

static int atd_status = 0;

#define DISCONNECT_POLLING_MS (100)
static int atd_mid_poweroff_thread(void *data)
{
	int time_left_ms = 5000;
	int cable_con = 0;

	while (time_left_ms > 0 && atd_status) {
		cable_con = power_supply_is_system_supplied();
		if (cable_con == 0) {
			pr_info("cable out\n");
			break;
		}

		pr_info("waiting for disconnect [%d]. "
			"ATD_STATUS: %d, CABLE_CON: %d\n",
			time_left_ms, atd_status, cable_con);

		msleep(DISCONNECT_POLLING_MS);
		time_left_ms -= DISCONNECT_POLLING_MS;
	}

	if (!atd_status) {
		pr_info("cancel ATD POWER-OFF. ATD_STATUS: %d\n", atd_status);
		return 0;
	}

	if (time_left_ms <= 0 && power_supply_is_system_supplied() > 0)
		pr_info("timeout waiting for disconnect\n");

	pr_err("do ATD POWER-OFF. ATD_STATUS: %d\n", atd_status);

	kernel_power_off();

	return 0;
}

static struct task_struct *atd_tsk;

static ssize_t atd_status_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", atd_status);
}

static ssize_t atd_status_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int status = 0;
	int ret;

	ret = sscanf(buf, "%d", &status);
	if (ret < 1) {
		pr_info("ATD_STATUS invalid\n");
		return count;
	}
	status = status ? 1 : 0;

	if (atd_status == status) {
		pr_info("ATD_STATUS already set\n");
		return count;
	}
	atd_status = status;

	pr_info("ATD_STATUS set as %d\n", atd_status);

	if (atd_status == 0) {
		__pm_relax(atd_poweroff_ws);
		return count;
	}

	__pm_stay_awake(atd_poweroff_ws);

	pr_info("create ATD POWER-OFF waiting thread, "
		"ATD_STATUS : %d\n", atd_status);

	atd_tsk = kthread_run(atd_mid_poweroff_thread, NULL, "ATD_POWER-OFF");
	if (IS_ERR(atd_tsk))
		pr_info("error creating ATD POWER-OFF thread\n");

	return count;
}
static struct kobj_attribute atd_status_attr =
	__ATTR(atd_status, 0660, atd_status_show, atd_status_store);

static struct kobject *atd_kobj = NULL;

static void atd_mid_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

#ifdef CONFIG_LGE_BOOT_MODE
	if (!lge_get_factory_boot())
		return;

	pr_info("factory boot. prepare atd_mid\n");
#endif

	atd_kobj = kobject_create_and_add("atd_mid", kernel_kobj);
	if (!atd_kobj) {
		pr_err("failed to create the atd_mid kobj\n");
		return;
	}

	ret = sysfs_create_file(atd_kobj, &atd_status_attr.attr);
	if (ret) {
		pr_err("failed to create atd_status sysfs\n");
		goto failed_sysfs;
	}

	return;

failed_sysfs:
	kobject_put(atd_kobj);
	atd_kobj = NULL;

	return;
}

static int __init atd_mid_init(void)
{
	atd_poweroff_ws = wakeup_source_register(NULL, "ATD_POWER-OFF");

	async_schedule(atd_mid_init_async, NULL);

	return 0;
}

static void __exit atd_mid_exit(void)
{
	wakeup_source_unregister(atd_poweroff_ws);

	if (atd_kobj) {
		sysfs_remove_file(atd_kobj, &atd_status_attr.attr);
		kobject_put(atd_kobj);
	}
}

module_init(atd_mid_init);
module_exit(atd_mid_exit);
MODULE_DESCRIPTION("LGE ATD STATE driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
