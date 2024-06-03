/* Copyright (c) 2013-2015, LGE Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *   * Neither the name of The Linux Foundation, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

 #include <linux/fb.h>
 #include <linux/slab.h>
 #include <linux/kernel.h>
 #include <linux/module.h>
 #include <linux/console.h>

static struct class *display = NULL;
static struct device *sysfs_dev_display = NULL;

static int auto_touch_test_mode;

ssize_t get_auto_touch_test_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", auto_touch_test_mode);
}

ssize_t set_auto_touch_test_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	auto_touch_test_mode  = simple_strtoul(buf, NULL, 10);
	pr_info("%s: Auto touch test mode is [%s] \n", __func__, auto_touch_test_mode? "ON" : "OFF");

	return count;
}

/* 0 : enable, 1 : disable */
int get_auto_touch_test(void)
{
	return auto_touch_test_mode;
}

static struct device_attribute auto_touch_test_mode_status_attrs[] = {
	 __ATTR(auto_touh_test_mode_status, 0644, get_auto_touch_test_mode, set_auto_touch_test_mode),
};

void lge_init_mfts(void)
{
	if(!display) {
		display = class_create(THIS_MODULE, "display");

		if (IS_ERR(display))
			pr_err("%s: Failed to create display class\n", __func__);
	}

	if (!sysfs_dev_display ) {
		sysfs_dev_display = device_create(display, NULL, 0, NULL, "auto_touch_test_mode");

		if (IS_ERR(sysfs_dev_display)) {
			pr_err("%s: Failed to create dev(sysfs_dev_display>auto_touch_test_mode)!", __func__);
		} else {
			if (device_create_file(sysfs_dev_display, &auto_touch_test_mode_status_attrs[0]) < 0)
				pr_err("%s: Fail!", __func__);
		}
	}

	pr_info("%s done\n", __func__);
}
