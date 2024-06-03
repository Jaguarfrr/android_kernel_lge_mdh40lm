#ifndef __BATTERY_CYCLE_H__
#define __BATTERY_CYCLE_H__

#include "mtk_battery.h"

void battery_persist_set_battery_removed(int removed);

void battery_cycle_update(struct mtk_battery *gm);
int battery_cycle_get_count(void);
int battery_cycle_set_count(unsigned int cycle);

#ifdef CONFIG_LGE_PM_BATTERY_AGING_FACTOR

void battery_aging_update(struct mtk_battery *gm);
int battery_aging_get_capacity(void);
int battery_aging_get_factor(void);
int battery_aging_get_factor_ten_multiple(void);
int battery_aging_get_factor_level(void);
int battery_aging_set_factor_level(int factor);
#endif

#endif
