/*
 *  drivers/stylus/stylus_class.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Kiwoo Han <kiwoo.han@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/stylus.h>

struct class *stylus_class;
static atomic_t device_count;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct stylus_dev *sdev = (struct stylus_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		int ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->state);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct stylus_dev *sdev = (struct stylus_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		int ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%s\n", sdev->name);
}

static DEVICE_ATTR(state, S_IRUGO, state_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, name_show, NULL);

void stylus_set_state(struct stylus_dev *sdev, int state)
{
	char name_buf[120];
	char state_buf[120];
	char *prop_buf;
	char *envp[3];
	int env_offset = 0;
	int length;

	if (sdev->state != state) {
		sdev->state = state;

		prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
		if (prop_buf) {
			length = name_show(sdev->dev, NULL, prop_buf);
			if (length > 0) {
				if (prop_buf[length - 1] == '\n')
					prop_buf[length - 1] = 0;
				snprintf(name_buf, sizeof(name_buf),
					"SWITCH_NAME=%s", prop_buf);
				envp[env_offset++] = name_buf;
			}
			length = state_show(sdev->dev, NULL, prop_buf);
			if (length > 0) {
				if (prop_buf[length - 1] == '\n')
					prop_buf[length - 1] = 0;
				snprintf(state_buf, sizeof(state_buf),
					"SWITCH_STATE=%s", prop_buf);
				envp[env_offset++] = state_buf;
			}
			envp[env_offset] = NULL;
			kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);
			free_page((unsigned long)prop_buf);
		} else {
			printk(KERN_ERR "out of memory in stylus_set_state\n");
			kobject_uevent(&sdev->dev->kobj, KOBJ_CHANGE);
		}
	}
}
EXPORT_SYMBOL_GPL(stylus_set_state);

static int create_stylus_class(void)
{
	if (!stylus_class) {
		stylus_class = class_create(THIS_MODULE, "stylus");
		if (IS_ERR(stylus_class))
			return PTR_ERR(stylus_class);
		atomic_set(&device_count, 0);
	}
	return 0;
}

int stylus_dev_register(struct stylus_dev *sdev)
{
	int ret;

	if (!stylus_class) {
		ret = create_stylus_class();
		if (ret < 0)
			return ret;
	}

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(stylus_class, NULL,
		MKDEV(0, sdev->index), NULL, sdev->name);
	if (IS_ERR(sdev->dev))
		return PTR_ERR(sdev->dev);

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0)
		goto err_create_file_1;
	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0)
		goto err_create_file_2;

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;
	return 0;

err_create_file_2:
	device_remove_file(sdev->dev, &dev_attr_state);
err_create_file_1:
	device_destroy(stylus_class, MKDEV(0, sdev->index));
	printk(KERN_ERR "stylus: Failed to register driver %s\n", sdev->name);

	return ret;
}
EXPORT_SYMBOL_GPL(stylus_dev_register);

void stylus_dev_unregister(struct stylus_dev *sdev)
{
	device_remove_file(sdev->dev, &dev_attr_name);
	device_remove_file(sdev->dev, &dev_attr_state);
	dev_set_drvdata(sdev->dev, NULL);
	device_destroy(stylus_class, MKDEV(0, sdev->index));
}
EXPORT_SYMBOL_GPL(stylus_dev_unregister);

static int __init stylus_class_init(void)
{
	return create_stylus_class();
}

static void __exit stylus_class_exit(void)
{
	class_destroy(stylus_class);
}

module_init(stylus_class_init);
module_exit(stylus_class_exit);

MODULE_AUTHOR("Kiwoo Han <kiwoo.han@lge.com>");
MODULE_DESCRIPTION("Stylus class driver");
MODULE_LICENSE("GPL");
