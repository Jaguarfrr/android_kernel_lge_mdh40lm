#define pr_fmt(fmt) "[CHGCTRL-ALGO]%s: " fmt, __func__

#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/power/charger_controller.h>
#include "charger_controller.h"

/* algorithm */
extern struct chgctrl_algo chgctrl_algo_otp;
extern struct chgctrl_algo chgctrl_algo_fb;
extern struct chgctrl_algo chgctrl_algo_spec;
extern struct chgctrl_algo chgctrl_algo_thermal;
extern struct chgctrl_algo chgctrl_algo_restricted;
extern struct chgctrl_algo chgctrl_algo_game;
extern struct chgctrl_algo chgctrl_algo_bcc;
extern struct chgctrl_algo chgctrl_algo_battery_id;
extern struct chgctrl_algo chgctrl_algo_factory;
extern struct chgctrl_algo chgctrl_algo_info;
extern struct chgctrl_algo chgctrl_algo_ttf;

static struct chgctrl_algo *chgctrl_algos[] = {
	&chgctrl_algo_otp,
	&chgctrl_algo_fb,
	&chgctrl_algo_spec,
	&chgctrl_algo_thermal,
	&chgctrl_algo_restricted,
	&chgctrl_algo_game,
	&chgctrl_algo_bcc,
	&chgctrl_algo_battery_id,
	&chgctrl_algo_factory,
	&chgctrl_algo_ttf,
	NULL, /* sentinel */
};

static void chgctrl_algo_init_default(struct chgctrl *chip)
{
	struct chgctrl_algo **algo;

	for (algo = chgctrl_algos; *algo; algo++) {
		if ((*algo)->init_default)
			(*algo)->init_default(chip);
	}
}

static int chgctrl_algo_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_algo **algo;

	for (algo = chgctrl_algos; *algo; algo++) {
		if ((*algo)->parse_dt)
			(*algo)->parse_dt(chip, np);
	}

	return 0;
}

static int chgctrl_algo_init(struct chgctrl *chip)
{
	struct chgctrl_algo **algo;

	for (algo = chgctrl_algos; *algo; algo++) {
		if ((*algo)->init)
			(*algo)->init(chip);
	}

	return 0;
}

static void chgctrl_algo_exit(struct chgctrl *chip)
{
	struct chgctrl_algo **algo;

	for (algo = chgctrl_algos; *algo; algo++) {
		if ((*algo)->exit)
			(*algo)->exit(chip);
	}
}

static int chgctrl_algo_thread(void *arg)
{
	struct chgctrl *chip = arg;
	struct chgctrl_algo **algo;
	unsigned int event;
	int ret;

	chgctrl_algo_init_default(chip);
	chgctrl_algo_parse_dt(chip, chip->dev->of_node);
	if (chip->model_of_node)
		chgctrl_algo_parse_dt(chip, chip->model_of_node);
	chgctrl_algo_init(chip);

	while (!kthread_should_stop()) {
		ret = wait_event_freezable(chip->algo_wq, chip->algo_event);
		if (ret < 0)
			continue;

		mutex_lock(&chip->algo_lock);
		event = chip->algo_event;
		chip->algo_event ^= event;
		mutex_unlock(&chip->algo_lock);

		if (!event)
			continue;

		__pm_stay_awake(chip->algo_ws);

		for (algo = chgctrl_algos; *algo; algo++) {
			if ((*algo)->run)
				(*algo)->run(chip, event);
		}

		__pm_relax(chip->algo_ws);
	};

	chgctrl_algo_exit(chip);

	return 0;
}

void chgctrl_algo_event(struct chgctrl *chip, int event)
{
	mutex_lock(&chip->algo_lock);
	chip->algo_event |= event;
	mutex_unlock(&chip->algo_lock);

	wake_up(&chip->algo_wq);
}

int chgctrl_init_algo(struct chgctrl *chip)
{
	chip->algo_ws = wakeup_source_register(chip->dev, "chgctrl_algo");
	mutex_init(&chip->algo_lock);
	init_waitqueue_head(&chip->algo_wq);

	chip->algo_task = kthread_run(chgctrl_algo_thread, chip,
			"chgctrl_algo");
	if (IS_ERR(chip->algo_task))
		return PTR_ERR(chip->algo_task);

	return 0;
}
