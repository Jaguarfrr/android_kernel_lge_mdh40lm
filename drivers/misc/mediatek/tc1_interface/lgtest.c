// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "lg_partition.h"

#ifdef ENABLE_TEST
#define TIMER_DELAY 30

static struct timer_list tc1_test_timer;

static void tc1_interface_test_wq(struct work_struct *unused)
{
	pr_info("start test kernel TC1 API...\n");
	LGE_API_test();
	pr_info("end test kernel TC1 API...\n");
}

static DECLARE_WORK(tc1testwq, tc1_interface_test_wq);

static void tc1_interface_test_timer(struct timer_list *unused)
{
	del_timer(&tc1_test_timer);
	schedule_work(&tc1testwq);
}
#endif

static int __init tc1_test_init(void)
{
#ifdef ENABLE_TEST
	timer_setup(&tc1_test_timer, tc1_interface_test_timer, 0);
	tc1_test_timer.expires = jiffies + TIMER_DELAY * HZ;
	add_timer(&tc1_test_timer);
#endif /* ENABLE_TEST */

	return 1;
}

/* should never be called */
static void __exit tc1_test_exit(void)
{
#ifdef ENABLE_TEST
	del_timer(&tc1_test_timer);
#endif /* ENABLE_TEST */
}
late_initcall_sync(tc1_test_init);
module_exit(tc1_test_exit);

MODULE_AUTHOR("teddy.seo@mediatek.com");
MODULE_DESCRIPTION("tc1 interface test module");
MODULE_LICENSE("GPL");
