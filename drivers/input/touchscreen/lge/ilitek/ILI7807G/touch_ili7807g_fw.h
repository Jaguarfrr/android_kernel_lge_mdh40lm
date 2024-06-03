/* touch_ili7807g.h
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: PH1-BSP-Touch@lge.com
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

#ifndef LGE_TOUCH_ILI7807G_FW_H
#define LGE_TOUCH_ILI7807G_FW_H

#include <linux/firmware.h>
/* The size of firmware upgrade */
#define MAX_HEX_FILE_SIZE		(160*1024)

struct flash_table {
	uint16_t mid;
	uint16_t dev_id;
	int mem_size;
	int program_page;
	int sector;
};

extern int ili7807g_read_flash_info(struct device *dev);
extern int ili7807g_fw_upgrade(struct device *dev, char* fwpath);

extern int g_update_percentage;
#endif
