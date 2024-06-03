#ifndef _CHARGER_CONTROLLER_H
#define _CHARGER_CONTROLLER_H

#include <linux/device.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

enum {
	ICL_VOTER_DEFAULT,
	ICL_VOTER_USER,
	ICL_VOTER_CCD,
	ICL_VOTER_RESTRICTED,
	ICL_VOTER_GAME,
	ICL_VOTER_INPUT_SUSPEND,
	ICL_VOTER_PSEUDO_HVDCP,
};

enum {
	FCC_VOTER_DEFAULT,
	FCC_VOTER_USER,
	FCC_VOTER_CCD,
	FCC_VOTER_OTP,
	FCC_VOTER_SPEC,
	FCC_VOTER_THERMAL,
	FCC_VOTER_DISPLAY,
	FCC_VOTER_RESTRICTED,
	FCC_VOTER_GAME,
	FCC_VOTER_BATTERY_ID,
	FCC_VOTER_ATCMD,
	FCC_VOTER_FACTORY,
	FCC_VOTER_BCC,
	FCC_VOTER_QNS,
};

enum {
	VFLOAT_VOTER_DEFAULT,
	VFLOAT_VOTER_USER,
	VFLOAT_VOTER_CCD,
	VFLOAT_VOTER_OTP,
	VFLOAT_VOTER_BATTERY_ID,
	VFLOAT_VOTER_QNS,
};

enum {
	ICL_BOOST_VOTER_USER,
	ICL_BOOST_VOTER_PSEUDO_BATTERY,
	ICL_BOOST_VOTER_USB_CURRENT_MAX,
	ICL_BOOST_VOTER_ATCMD,
	ICL_BOOST_VOTER_FACTORY,
};

enum {
	INPUT_SUSPEND_VOTER_USER,
	INPUT_SUSPEND_VOTER_WATER_DETECT,
	INPUT_SUSPEND_VOTER_OVER_VOLTAGE,
};

enum {
	FASTCHG_VOTER_DEFAULT,
	FASTCHG_VOTER_USER,
	FASTCHG_VOTER_CCD,
	FASTCHG_VOTER_NETWORK,
	FASTCHG_VOTER_FACTORY,
};

enum {
	WLESS_PWR_VOTER_DEFAULT,
	WLESS_PWR_VOTER_USER,
	WLESS_PWR_VOTER_CCD,
};

enum voter_type {
	VOTER_TYPE_MIN,
	VOTER_TYPE_MAX,
	VOTER_TYPE_TRIGGER,
};

struct chgctrl;
struct chgctrl_vote;

struct chgctrl_vote_desc {
	const char *name;
	int type;

	const char **voters;
	int num_voters;

	void (*changed)(struct chgctrl_vote *);
};

struct chgctrl_vote {
	const struct chgctrl_vote_desc *desc;
	struct mutex lock;

	/* vote information */
	int *values;

	/* active */
	int active_voter;
	int active_value;

	/* handler */
	struct work_struct changed_work;
	void *driver_data;
};

extern int chgctrl_vote(struct chgctrl_vote *vote, int voter, int value);
extern void chgctrl_vote_dump(struct chgctrl_vote *vote);
extern int chgctrl_vote_active_value(struct chgctrl_vote *vote);
extern int chgctrl_vote_active_voter(struct chgctrl_vote *vote);
extern const char *chgctrl_vote_active_voter_name(struct chgctrl_vote *vote);
extern const char *chgctrl_vote_get_voter_name(struct chgctrl_vote *vote, int voter);
extern int chgctrl_vote_get_value(struct chgctrl_vote *vote, int voter);
extern struct chgctrl_vote *chgctrl_vote_register(struct device *dev,
						  const struct chgctrl_vote_desc *desc,
						  int init_value);
static inline void *chgctrl_vote_get_drvdata(struct chgctrl_vote *vote)
{
	return vote->driver_data;
}
extern int chgctrl_init_vote(struct chgctrl *chip);

enum {
	ALGO_EVENT_START	= BIT(0),
	ALGO_EVENT_CHARGER	= BIT(1),
	ALGO_EVENT_BATTERY	= BIT(2),
	ALGO_EVENT_FRAMEBUFFER	= BIT(3),
	ALGO_EVENT_BATTERY_ID	= BIT(4),
};

struct chgctrl_algo {
	const char *name;
	void (*init_default)(struct chgctrl *chip);
	int (*parse_dt)(struct chgctrl *chip, struct device_node *np);
	int (*init)(struct chgctrl *chip);
	int (*exit)(struct chgctrl *chip);
	int (*run)(struct chgctrl *chip, int event);
};

extern void chgctrl_algo_event(struct chgctrl *chip, int event);
extern int chgctrl_init_algo(struct chgctrl *chip);

extern int chgctrl_init_cmd(struct chgctrl *chip);
extern int chgctrl_init_info(struct chgctrl *chip);

extern int chgctrl_init_ccd(struct chgctrl *chip);
extern int chgctrl_init_atcmd(struct chgctrl *chip);

/* boot mode */
enum chgctrl_boot_mode {
	BOOT_MODE_NORMAL,
	BOOT_MODE_CHARGER,
	BOOT_MODE_FACTORY,
};

/* otp */
struct chgctrl_otp {
	bool enabled;
	int version;
	struct work_struct work;
	int temp_state;
	int volt_state;
	int health;
	int fcc;
	int vfloat;
};

/* frame buffer */
struct chgctrl_fb {
	bool enabled;
	int fcc;
};

/* spec */
struct chgctrl_spec_data {
	/* temperature range */
	int tmin;
	int tmax;
	/* voltage range */
	int volt;
	/* charge limit */
	int curr;
};

struct chgctrl_spec {
	bool enabled;
	struct mutex lock;
	struct work_struct work;
	struct delayed_work vote_work;
	struct chgctrl_spec_data *data;
	int data_size;
	int idx;
	int vfloat;
	int step_fcc;
	int step_ms;
	int step_idx;
};

/* thermal */
struct chgctrl_thermal_trip {
	/* trigger temperature */
	int trigger;
	int offset;

	/* limit */
	int curr;
};

struct chgctrl_thermal {
	bool enabled;
	struct work_struct work;
	struct chgctrl_thermal_trip *trip;
	int trip_size;
	int idx;
};

struct chgctrl_game {
	bool enabled;
	int game_mode;
	int gpu_load;
	struct timespec start;

	struct delayed_work dwork;

	int fcc;
	int icl;
	int light_fcc;
	int light_icl;
	int light_load;
	int light_sec;
	int lowbatt_fcc;
	int lowbatt_icl;
	int lowbatt_soc;
};
/* factory cable */
struct chgctrl_factory {
	int icl;
	int fcc;
	int fastchg;
};

/* battery care charging */
struct chgctrl_bcc {
	int fcc;
};

/* ccd no heartbeat W/R */
struct chgctrl_ttf {
	struct delayed_work update_work;
	int pre_online;
};

struct chgctrl_info {
	/* charger */
	int online;
	int type;
	int usb_type;
	/* battery */
	int present;
	int status;
	int health;
	int voltage_now;
	int current_now;
	int capacity;
	int temp;
	int technology;
};

struct chgctrl_helper;

struct chgctrl {
	struct device *dev;
	struct device_node *model_of_node;
	struct device_node *setting_of_node;

	/* vote */
	struct chgctrl_vote *icl;
	struct chgctrl_vote *fcc;
	struct chgctrl_vote *vfloat;
	struct chgctrl_vote *icl_boost;
	struct chgctrl_vote *input_suspend;
	struct chgctrl_vote *fastchg;
	struct chgctrl_vote *wless_pwr;

	/* algorithm */
	struct chgctrl_algo *algos;
	struct wakeup_source *algo_ws;
	struct mutex algo_lock;
	struct task_struct *algo_task;
	wait_queue_head_t algo_wq;
	unsigned int algo_event;

	/* power supply */
	struct chgctrl_helper *helper;
	struct power_supply_desc psy_desc;
	struct power_supply *psy;
	enum power_supply_type bc12_type;
	enum power_supply_usb_type bc12_usb_type;
	enum power_supply_usb_type typec_usb_type;
	const char *fastchg_name;

	/* power supply notifier */
	struct notifier_block psy_nb;
	struct wakeup_source *psy_charger_ws;
	struct work_struct psy_charger_work;
	struct wakeup_source *psy_battery_ws;
	struct work_struct psy_battery_work;

	struct delayed_work bc12_retry_work;
	struct wakeup_source *bc12_retry_ws;
	unsigned int bc12_retry_ms;
	bool bc12_retry;
	bool floated_charger;

	/* vzw carrier */
	int vzw_chg_state;
	bool vzw_chg_mode;
	int input_current_limit;

	/* frame buffer notifier */
	struct work_struct fb_work;
	struct notifier_block fb_nb;

	/* internal data */
	enum chgctrl_boot_mode boot_mode;
	struct power_supply *charger_psy;
	struct power_supply *battery_psy;
	struct chgctrl_info info;
	struct chgctrl_info hook;
	bool display_on;

	/* batt_therm compensation */
	int batt_therm_comp;

	/* type c & pd enable*/
	bool typec_pd;

	/* ccd */
	int ccd_health;
	int ccd_status;
	int ccd_ttf;

	/* algorithm : otp */
	struct chgctrl_otp otp;

	/* algorithm : frame buffer */
	struct chgctrl_fb fb;

	/* algorithm : spec */
	struct chgctrl_spec spec;

	/* algorithm : restrict charging */
	struct chgctrl_vote *restricted;

	/* algorithm : thermal */
	struct chgctrl_thermal thermal;

	/* algorithm : game */
	struct chgctrl_game game;

	/* algorithm : battery care charging */
	struct chgctrl_bcc bcc;

	/* algorithm : factory cable */
	struct chgctrl_factory factory;

	/* ccd no heartbeat W/R */
	struct chgctrl_ttf ttf;
};

static inline void chgctrl_changed(struct chgctrl* chip)
{
	if (chip->psy)
		power_supply_changed(chip->psy);
}

/* info */
extern int chgctrl_get_charger_online(struct chgctrl *chip);
extern int chgctrl_get_charger_type(struct chgctrl *chip);
extern int chgctrl_get_charger_usb_type(struct chgctrl *chip);

extern int chgctrl_get_battery_present(struct chgctrl *chip);
extern int chgctrl_get_battery_status(struct chgctrl *chip);
extern int chgctrl_get_battery_health(struct chgctrl *chip);
extern int chgctrl_get_battery_voltage_now(struct chgctrl *chip);
extern int chgctrl_get_battery_current_now(struct chgctrl *chip);
extern int chgctrl_get_battery_capacity(struct chgctrl *chip);
extern int chgctrl_get_battery_temp(struct chgctrl *chip);
extern int chgctrl_get_battery_technology(struct chgctrl *chip);

extern enum chgctrl_boot_mode chgctrl_get_boot_mode(struct chgctrl *chip);

#endif
