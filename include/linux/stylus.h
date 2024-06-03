/*
 *  Stylus class driver
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

#ifndef __LINUX_STYLUS_H__
#define __LINUX_STYLUS_H__

struct stylus_dev {
	const char	*name;
	struct device	*dev;
	int		index;
	int		state;

	ssize_t	(*print_name)(struct stylus_dev *sdev, char *buf);
	ssize_t	(*print_state)(struct stylus_dev *sdev, char *buf);
};

struct gpio_stylus_platform_data {
	const char *name;
	unsigned 	gpio;

	/* if NULL, stylus_dev.name will be printed */
	const char *name_on;
	const char *name_off;
	/* if NULL, "0" or "1" will be printed */
	const char *state_on;
	const char *state_off;
};

extern int stylus_dev_register(struct stylus_dev *sdev);
extern void stylus_dev_unregister(struct stylus_dev *sdev);

static inline int stylus_get_state(struct stylus_dev *sdev)
{
	return sdev->state;
}

extern void stylus_set_state(struct stylus_dev *sdev, int state);

#endif /* __LINUX_STYLUS_H__ */
