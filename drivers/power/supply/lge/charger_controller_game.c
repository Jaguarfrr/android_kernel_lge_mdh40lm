#define pr_fmt(fmt) "[CHGCTRL-GAME]%s: " fmt, __func__

#include <linux/of.h>
#include <linux/power/charger_controller.h>
#include "charger_controller.h"

#define GAME_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

enum {
	LOAD_NONE,
	LOAD_LIGHT,
	LOAD_HEAVY,
};

extern bool mtk_get_gpu_loading(unsigned int *pLoading);
static void chgctrl_game_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct chgctrl_game *game = container_of(dwork, struct chgctrl_game,
			dwork);
	struct chgctrl *chip = container_of(game, struct chgctrl, game);

	unsigned int load = game->light_load;
	int fcc = game->fcc;
	int icl = game->icl;
	struct timespec now, diff;

	if (!game->game_mode) {
		chgctrl_vote(chip->fcc, FCC_VOTER_GAME, -1);
		chgctrl_vote(chip->icl, ICL_VOTER_GAME, -1);
		game->gpu_load = LOAD_NONE;
		return;
	}

	get_monotonic_boottime(&now);

	/* assume heavy-load in start */
	if (game->gpu_load == LOAD_NONE) {
		game->start = now;
		game->gpu_load = LOAD_HEAVY;
		goto out_vote;
	}

	/* gpu loading not available. assume heavy-load */
	if (!mtk_get_gpu_loading(&load)) {
		game->start = now;
		game->gpu_load = LOAD_HEAVY;
		goto out_reschedule;
	}

	if (load < game->light_load) {
		/* already in low. ignore */
		if (game->gpu_load == LOAD_LIGHT)
			goto out_reschedule;

		/* not enough time passed to judge */
		diff = timespec_sub(now, game->start);
		if (diff.tv_sec <= game->light_sec)
			goto out_reschedule;

		game->gpu_load = LOAD_LIGHT;
		fcc = game->light_fcc;
		icl = game->light_icl;
		goto out_vote;
	}

	/* mark current time as start */
	game->start = now;
	if (game->gpu_load != LOAD_HEAVY)
		game->gpu_load = LOAD_HEAVY;

out_vote:
	if (chgctrl_get_battery_capacity(chip) < game->lowbatt_soc) {
		fcc = game->lowbatt_fcc;
		icl = game->lowbatt_icl;
	}

	chgctrl_vote(chip->fcc, FCC_VOTER_GAME, fcc);
	chgctrl_vote(chip->icl, ICL_VOTER_GAME, icl);

out_reschedule:
	schedule_delayed_work(dwork, msecs_to_jiffies(10000));
}

static ssize_t game_mode_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	struct chgctrl_game *game = &chip->game;

	return scnprintf(buf, PAGE_SIZE, "%d", game->game_mode);
}

static ssize_t game_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct chgctrl *chip = dev_get_drvdata(dev);
	struct chgctrl_game *game = &chip->game;
	int game_mode;
	int ret;

	ret = sscanf(buf, "%d", &game_mode);
	if (ret <= 0)
		return -EINVAL;

	game_mode = (game_mode > 0) ? 1 : 0;

	if (!game->enabled)
		return count;

	if (game->game_mode == game_mode)
		return count;
	game->game_mode = game_mode;

	if (!game_mode) {
		if (game->gpu_load == LOAD_NONE)
			return count;

		/* to deactive immediatly, cancel scheduled work */
		cancel_delayed_work(&game->dwork);

		goto game_start;
	}

	if (game->gpu_load != LOAD_NONE)
		return count;

game_start:
	schedule_delayed_work(&game->dwork, 0);

	return count;
}

static DEVICE_ATTR(game_mode, GAME_PERM, game_mode_show, game_mode_store);

static int chgctrl_game_init(struct chgctrl *chip)
{
	struct chgctrl_game *game = &chip->game;
	int ret;

	if (game->light_icl < 0)
		game->light_icl = game->icl;
	if (game->light_fcc < 0)
		game->light_fcc = game->fcc;

	if (game->lowbatt_icl < 0)
		game->lowbatt_icl = game->icl;
	if (game->lowbatt_fcc < 0)
		game->lowbatt_fcc = game->fcc;

	game->game_mode = 0;
	game->gpu_load = LOAD_NONE;
	INIT_DELAYED_WORK(&game->dwork, chgctrl_game_work);

	ret = device_create_file(chip->dev, &dev_attr_game_mode);
	if (ret)
		return ret;

	if (game->icl < 0 && game->fcc < 0)
		return -EINVAL;

	game->enabled = true;

	return 0;
}

static int chgctrl_game_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_game *game = &chip->game;

	of_property_read_u32(np, "game-icl", &game->icl);
	of_property_read_u32(np, "game-fcc", &game->fcc);

	of_property_read_u32(np, "game-light-icl", &game->light_icl);
	of_property_read_u32(np, "game-light-fcc", &game->light_fcc);
	of_property_read_u32(np, "game-light-load", &game->light_load);
	of_property_read_u32(np, "game-light-sec", &game->light_sec);

	of_property_read_u32(np, "game-lowbatt-icl", &game->lowbatt_icl);
	of_property_read_u32(np, "game-lowbatt-fcc", &game->lowbatt_fcc);
	of_property_read_u32(np, "game-lowbatt-soc", &game->lowbatt_soc);

	return 0;
}

static void chgctrl_game_init_default(struct chgctrl *chip)
{
	struct chgctrl_game *game = &chip->game;

	game->icl = -1;
	game->fcc = -1;

	game->light_icl = -1;
	game->light_fcc = -1;
	game->light_load = 80;
	game->light_sec = 100;

	game->lowbatt_icl = -1;
	game->lowbatt_fcc = -1;
	game->lowbatt_soc = 15;
}

struct chgctrl_algo chgctrl_algo_game = {
	.name = "game",
	.init_default = chgctrl_game_init_default,
	.parse_dt = chgctrl_game_parse_dt,
	.init = chgctrl_game_init,
};
