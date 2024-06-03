#define pr_fmt(fmt) "[CYCLE] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/syscalls.h>
#include <linux/power/battery_cycle.h>

#define BATTERY_PERSIST_PATH "/mnt/vendor/persist-lg/battery/"
#define BATTERY_PERSIST_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define BATTERY_PERSIST_DATA_SIZE 20

static unsigned int persist_read(char *name)
{
	mm_segment_t old_fs = get_fs();
	char data[BATTERY_PERSIST_DATA_SIZE];
	int fd = 0;
	int result = 0;
	unsigned int value_read = -EINVAL;

	set_fs(KERNEL_DS);

	fd = ksys_open(name, O_RDONLY, 0);
	if (fd < 0) {
		pr_err("open error %s (%d) \n", name, fd);
		goto Error;
	}
	memset(data, 0x00, sizeof(data));
	result = ksys_read(fd, data, sizeof(data));
	if (result < 0) {
		pr_err("read error %s (%d)\n", name, result);
		goto Error;
	}
	ksys_close(fd);

	result = kstrtouint(data, 10, &value_read);
	if (result != 0) {
		pr_err("kstrtouint Error\n");
		goto Error;
	}

Error:
	set_fs(old_fs);
	return value_read;
}

static int persist_write(char *name, unsigned int value)
{
	mm_segment_t old_fs = get_fs();
	char data[BATTERY_PERSIST_DATA_SIZE];
	int fd = 0;
	int result = 0;
	int value_write = -EINVAL;
	size_t size;

	set_fs(KERNEL_DS);
	fd = ksys_open(name, O_WRONLY | O_CREAT | O_TRUNC | S_IROTH,
			BATTERY_PERSIST_PERM);
	if (fd < 0) {
		pr_err("open error %s (%d)\n", name, fd);
		goto Error;
	}

	memset(data, 0x00, sizeof(data));
	size = snprintf(data, sizeof(data), "%u\n", value);
	result = ksys_write(fd, data, size);

	if (result < 0) {
		pr_err("write error %s (%d) \n", name, result);
		goto Error;
	}
	value_write = result;
	ksys_sync();

Error:
	if (fd >= 0) {
		ksys_close(fd);
		ksys_chmod(name, BATTERY_PERSIST_PERM);
	}
	set_fs(old_fs);
	return value_write;
}

static int battery_removed = -1;

void battery_persist_set_battery_removed(int removed)
{
	battery_removed = removed;
	pr_info("battery removed = %d\n", removed);
}
EXPORT_SYMBOL(battery_persist_set_battery_removed);

/***** Battery Cycle *****/
#define BATTERY_CYCLE_COUNT BATTERY_PERSIST_PATH "cycle_count"
#define BATTERY_CYCLE_DELTA BATTERY_PERSIST_PATH "cycle_delta"
#define BATTERY_CYCLE_COUNT_BACKUP BATTERY_PERSIST_PATH "cycle_count_backup"

#define BATTERY_CYCLE_DELTA_MAX 100

//#define CYCLE_DEBUG 32

struct battery_cycle {
	bool initialized;
	struct mutex lock;

	unsigned int count;
	unsigned int delta;
	int soc;
};

static struct battery_cycle cycle = {
	.initialized = false,
	.count = 0,
	.delta = 0,
	.soc = -EINVAL,
};

static bool cycle_enabled(struct mtk_battery *gm)
{
	switch (gm->bootmode) {
		case 0: /* NORMAL_BOOT */
		case 8: /* KERNEL_POWER_OFF_CHARGING_BOOT */
		case 9: /* LOW_POWER_OFF_CHARGING_BOOT */
			return true;
	}

	return false;
}

static bool cycle_is_battery_removed(struct mtk_battery *gm)
{
	if (battery_removed >= 0)
		return battery_removed ? true : false;

	return gm->gauge->hw_status.is_bat_plugout ? true : false;
}

static void cycle_dump(void)
{
	pr_info("battery cycle = %d.%02d\n", cycle.count, cycle.delta);
}

static void cycle_clear(void)
{
	pr_info("clear battery cycle\n");

	persist_write(BATTERY_CYCLE_COUNT, 0);
	persist_write(BATTERY_CYCLE_DELTA, 0);

	cycle.count = 0;
	cycle.delta = 0;
}

static int cycle_init(struct mtk_battery *gm)
{
	bool clear = false;
	int count, delta;

	pr_info("Start\n");

	if (!cycle_enabled(gm))
		return -ENODEV;

	mutex_init(&cycle.lock);

	count = persist_read(BATTERY_CYCLE_COUNT);
	delta = persist_read(BATTERY_CYCLE_DELTA);

	if (count < 0) {
		pr_err("failed to read count\n");
		count = 0;
		clear = true;
	}
	if (delta < 0) {
		pr_err("failed to read delta\n");
		clear = true;
	}

	if (cycle_is_battery_removed(gm)) {
		pr_info("battery removed\n");
		clear = true;
	}

	if (clear) {
		/* create backup before clear */
		persist_write(BATTERY_CYCLE_COUNT_BACKUP, count);

		cycle_clear();
		count = 0;
		delta = 0;
	}

	cycle.count = count;
	cycle.delta = delta;

	cycle_dump();

	return 0;
}

static void cycle_update(struct mtk_battery *gm)
{
	int soc = gm->soc;
	unsigned int count = 0;
	unsigned int delta = 0;

	if (!cycle.initialized)
		return;

	if (cycle.soc == -EINVAL)
		cycle.soc = soc;

	if (soc > cycle.soc)
		delta = soc - cycle.soc;
	cycle.soc = soc;

#ifdef CYCLE_DEBUG
	delta += CYCLE_DEBUG;
#endif

	/* soc not increased */
	if (!delta)
		return;

	mutex_lock(&cycle.lock);

	delta += cycle.delta;
	count = (delta / BATTERY_CYCLE_DELTA_MAX);
	delta %= BATTERY_CYCLE_DELTA_MAX;
	if (count) {
		cycle.count += count;
		persist_write(BATTERY_CYCLE_COUNT, cycle.count);
	}

	cycle.delta = delta;
	persist_write(BATTERY_CYCLE_DELTA, cycle.delta);

	mutex_unlock(&cycle.lock);

	cycle_dump();
}

void battery_cycle_update(struct mtk_battery *gm)
{
	int ret;

	if (!cycle_enabled(gm))
		return;

	if (!cycle.initialized) {
		ret = cycle_init(gm);
		if (ret)
			return;
		cycle.initialized = true;
	}

	cycle_update(gm);
}
EXPORT_SYMBOL(battery_cycle_update);

int battery_cycle_get_count(void)
{
	return cycle.count;
}
EXPORT_SYMBOL(battery_cycle_get_count);

int battery_cycle_set_count(unsigned int count)
{
	if (!cycle.initialized)
		return 0;

	/* cycle should be increased */
	if (count <= cycle.count)
		return 0;

	mutex_lock(&cycle.lock);

	cycle.count = count;

	persist_write(BATTERY_CYCLE_COUNT, cycle.count);

	mutex_unlock(&cycle.lock);

	pr_debug("cycle changed to %d\n", count);

	cycle_dump();

	return 0;
}
EXPORT_SYMBOL(battery_cycle_set_count);

#ifdef CONFIG_LGE_PM_BATTERY_AGING_FACTOR
/***** Battery Aging *****/
#define BATTERY_CHARGE_FULL BATTERY_PERSIST_PATH "charge_full"
#define BATTERY_CHARGE_FULL_BACKUP BATTERY_PERSIST_PATH "charge_full_backup"
#define BATTERY_AGING_FACTOR BATTERY_PERSIST_PATH "aging_factor"
#define BATTERY_REMOVE_CHECK BATTERY_PERSIST_PATH "cbc3_action"

#define AGING_AVG_SIZE			(2)
#define AGING_CYCLE_CHECK		(0)
#define MIN_DROP_PERCENTAGE		(10)
#define LEARNED_CAPACITY_COUNT_CHECK	(2)

struct battery_aging {
	bool initialized;
	struct mutex lock;

	int ui_capacity;	/* in 0.1 mhA */
	int ui_factor;		/* in 0.01 percent */

	int capacity_avg;
	int factor_avg;
	bool valid_avg;

	int capacity[AGING_AVG_SIZE];
	int factor[AGING_AVG_SIZE];
	unsigned int idx;

	unsigned int count_check;

	int force_ui_factor;
};

static struct battery_aging aging = {
	.initialized = false,
	.ui_capacity = 0,
	.ui_factor = 0,
	.capacity_avg = 0,
	.factor_avg = 0,
	.valid_avg = false,
	.capacity = { 0, },
	.factor = { 0, },
	.idx = 0,
	.count_check = 0,
	.force_ui_factor = 0,
};

static bool aging_enabled(struct mtk_battery *gm)
{
	switch (gm->bootmode) {
		case 0: /* NORMAL_BOOT */
		case 8: /* KERNEL_POWER_OFF_CHARGING_BOOT */
		case 9: /* LOW_POWER_OFF_CHARGING_BOOT */
			return true;
	}

	return false;
}

static bool aging_is_battery_removed(struct mtk_battery *gm)
{
	if (battery_removed >= 0)
		return battery_removed ? true : false;

	return gm->gauge->hw_status.is_bat_plugout ? true : false;
}

static void aging_dump(void)
{
	int i;

	for (i = 0; i < AGING_AVG_SIZE; i++) {
		if (!aging.capacity[i] && !aging.factor[i])
			continue;
		pr_info("[AGING] capacity[%d]: %d factor[%d]: %d\n",
				i, aging.capacity[i], i, aging.factor[i]);
	}

	if (aging.valid_avg) {
		pr_info("[AGING][AVG] capacity: %d factor = %d\n",
				aging.capacity_avg, aging.factor_avg);
	}


	pr_info("[AGING] learning_capacity: %d aging_factor: %d\n",
			aging.ui_capacity, aging.ui_factor);
}

static void aging_clear(void)
{
	pr_info("[AGING] clear battery aging\n");

	persist_write(BATTERY_CHARGE_FULL, 0);
	persist_write(BATTERY_AGING_FACTOR, 0);

	aging.ui_capacity = 0;
	aging.ui_factor = 0;
}

static int aging_init(struct mtk_battery *gm)
{
	bool clear = false;
	int capacity, factor;

	if (!aging_enabled(gm))
		return -ENODEV;

	mutex_init(&aging.lock);

	capacity = persist_read(BATTERY_CHARGE_FULL);
	factor = persist_read(BATTERY_AGING_FACTOR);

	if (capacity < 0) {
		pr_err("[AGING] failed to read count\n");
		clear = true;
	}
	if (factor < 0) {
		pr_err("[AGING] failed to read delta\n");
		clear = true;
	}

	if (aging_is_battery_removed(gm)) {
		pr_info("[AGING] battery removed\n");
		persist_write(BATTERY_CHARGE_FULL_BACKUP, capacity);
		persist_write(BATTERY_REMOVE_CHECK, 0);
		clear = true;
	}

	if (clear) {
		aging_clear();
		capacity = 0;
		factor = 0;
	}

	aging.ui_capacity = capacity;
	aging.ui_factor = factor;

	aging_dump();

	return 0;
}

static bool aging_update_avg(int capacity, int factor)
{
	int i;

	if (aging.idx >= AGING_AVG_SIZE)
		aging.idx = 0;

	aging.capacity[aging.idx] = capacity;
	aging.factor[aging.idx] = factor;
	aging.idx++;

	/* accumulated average */
	if (aging.valid_avg) {
		aging.capacity_avg = (aging.capacity_avg + capacity) / 2;
		aging.factor_avg = (aging.factor_avg + factor) / 2;
		return true;
	}

	/* initial average */
	if (aging.idx >= AGING_AVG_SIZE) {
		capacity = 0;
		factor = 0;
		for (i = 0; i < AGING_AVG_SIZE; i++) {
			capacity += aging.capacity[i];
			factor += aging.factor[i];
		}

		aging.capacity_avg = capacity / AGING_AVG_SIZE;
		aging.factor_avg = factor / AGING_AVG_SIZE;
		return true;

	}

	return false;
}

static void aging_update_by_full(struct mtk_battery *gm)
{
	static int pre_capacity = 0;
	int capacity = gm->bs_data.bat_charge_full;
	int factor = gm->bs_data.bat_aging_factor;
	int temp = gm->bs_data.bat_batt_temp;
	int cycle;

	if (!aging.initialized)
		return;

	if (temp < 15 || temp >= 45) {
		pr_info("[AGING] Not Update in this temperature (%d)\n", temp);
		return;
	}

	/* Not use for AGING_CYCKE_CHECK */
	cycle = battery_cycle_get_count();
	if (cycle < AGING_CYCLE_CHECK) {
		pr_info("[AGING] aging_init_check %d\n", cycle);
		return;
	}

	if (capacity == pre_capacity)
		return;
	pre_capacity = capacity;

	aging.count_check++;
	if (aging.count_check < LEARNED_CAPACITY_COUNT_CHECK) {
		pr_info("[AGING] capacity_count_init_check %d\n",
				aging.count_check);
		return;
	}
	aging.count_check = LEARNED_CAPACITY_COUNT_CHECK;

	/* update average */
	aging.valid_avg = aging_update_avg(capacity, factor);
	if (!aging.valid_avg) {
		pr_info("[AGING] EMPTY \n");
		return;
	}

	mutex_lock(&aging.lock);

	capacity = aging.ui_capacity * (100 - MIN_DROP_PERCENTAGE) / 100;
	aging.ui_capacity = max(aging.capacity_avg, capacity);
	persist_write(BATTERY_CHARGE_FULL, aging.ui_capacity);

	factor = aging.ui_factor ?: 10000;
	factor = factor * (100 - MIN_DROP_PERCENTAGE) / 100;
	aging.ui_factor = max(aging.factor_avg, factor);
	persist_write(BATTERY_AGING_FACTOR, aging.ui_factor);

	mutex_unlock(&aging.lock);

	aging_dump();
}

void battery_aging_update(struct mtk_battery *gm)
{
	int ret;

	if (!aging.initialized) {
		ret = aging_init(gm);
		if (ret)
			return;
		aging.initialized = true;
	}

	if (gm->soc >= 100)
		aging_update_by_full(gm);

}
EXPORT_SYMBOL(battery_aging_update);

int battery_aging_get_capacity(void)
{
	return aging.ui_capacity * 100;
}
EXPORT_SYMBOL(battery_aging_get_capacity);

int battery_aging_get_factor(void)
{
	if (!aging.ui_factor)
		return 100;

	return aging.ui_factor / 100;
}
EXPORT_SYMBOL(battery_aging_get_factor);

int battery_aging_get_factor_ten_multiple(void)
{
	int value;

	if (!aging.ui_factor)
		return 100;

	value = ((aging.ui_factor / 1000) + 1) * 10;
	if (value >= 100)
		value = 100;

	return value;
}
EXPORT_SYMBOL(battery_aging_get_factor_ten_multiple);

int battery_aging_get_factor_level(void)
{
	int level;

	if (aging.force_ui_factor)
		return aging.force_ui_factor / 100;

	if (!aging.ui_factor)
		return 100;

	level = ((aging.ui_factor / 1000) + 1) * 10;
	if (level >= 100)
		level = 100;

	if (level >= 90)
		level += 1;
	else if (level >= 60)
		level += 2;
	else
		level += 3;

	return level;
}
EXPORT_SYMBOL(battery_aging_get_factor_level);

int battery_aging_set_factor_level(int level)
{
	int factor = level * 100;

	if (aging.force_ui_factor == factor)
		return 0;

	aging.force_ui_factor = factor;
	pr_info("[AGING] set force level %d\n", level);

	return 1;
}
EXPORT_SYMBOL(battery_aging_set_factor_level);
#endif
