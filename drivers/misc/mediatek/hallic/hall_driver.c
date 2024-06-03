#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/stylus.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

#include <linux/pinctrl/consumer.h>

/*
* If any MTK platform does not support both edge trigger, use this feature.
* c.f MT6755M support both edge, do not need it.
* falling or rising, not both falling + rising
* #define HALLIC_IRQ_SUPPORT_ONLY_ONE_EDGE
*
*/

#ifdef CONFIG_STYLUS_PEN_DETECTION
typedef enum {
	STYLUS_PEN_OUT  = 0,				/* STYLUS_DETECT High */
	STYLUS_PEN_IN   = 1				/* STYLUS_DETECT Low */
} pen_state_t;
#endif

#define STR_PEN_STATE(p)	((p) == STYLUS_PEN_OUT ? "out" : "in")

#define HALL_IC_DEV_NAME "hallic"
#define HALL_IC_COMPATIBLE "lge,hallic"

struct hallic_platform_data {
#ifdef CONFIG_STYLUS_PEN_DETECTION
	int pen_irq_gpio;
	int pen_irq;
	int check_irq;
	int pen_debounce;
#endif
};

struct hallic_cradle {
	spinlock_t lock;
	struct hallic_platform_data *pdata;
	struct wake_lock wake_lock;
	int wake_lock_timeout_ms;
#ifdef CONFIG_STYLUS_PEN_DETECTION
	struct stylus_dev pen_sdev;
	pen_state_t pen_state;
#endif
};

#ifdef CONFIG_STYLUS_PEN_DETECTION
static struct delayed_work pen_work;
#endif

static struct workqueue_struct *cradle_wq;
static struct hallic_cradle *cradle;
static struct hallic_platform_data hallic_platform_data;

#ifdef CONFIG_STYLUS_PEN_DETECTION
static void hallic_get_pen_state(int gpio, pen_state_t *pen_state)
{
	unsigned long flags;

	if (pen_state) {
		spin_lock_irqsave(&cradle->lock, flags);
		if (gpio_get_value(gpio)) {
			/* if the gpio is high, it means that pen is out. */
			*pen_state = STYLUS_PEN_OUT;
		} else {
			/* if the gpio is low, it means that pen is in. */
			*pen_state = STYLUS_PEN_IN;
		}
		spin_unlock_irqrestore(&cradle->lock, flags);

		pr_info("%s::pen state = %d(%s)\n", __func__,
				*pen_state, STR_PEN_STATE(*pen_state));
	}
}
#endif

static void boot_cradle_det_func(void)
{
	if (!cradle) {
		pr_err("%s::error, cradle is null", __func__);
		return;
	}

#ifdef CONFIG_STYLUS_PEN_DETECTION
	if (cradle->pdata->pen_irq_gpio >= 0) {
		hallic_get_pen_state(cradle->pdata->pen_irq_gpio,
						&cradle->pen_state);

		wake_lock_timeout(&cradle->wake_lock,
			msecs_to_jiffies(cradle->wake_lock_timeout_ms));
		stylus_set_state(&cradle->pen_sdev, cradle->pen_state);
	}
#endif
}

#ifdef CONFIG_STYLUS_PEN_DETECTION
static void hallic_pen_work_func(struct work_struct *work)
{
	pen_state_t pen_state;
#ifdef HALLIC_IRQ_SUPPORT_ONLY_ONE_EDGE
	unsigned long flags;
#endif

	if (!cradle) {
		pr_err("%s::error, cradle is null", __func__);
		return;
	}

	hallic_get_pen_state(cradle->pdata->pen_irq_gpio, &pen_state);
	if (cradle->pen_state != pen_state) {
		cradle->pen_state = pen_state;

#ifdef HALLIC_IRQ_SUPPORT_ONLY_ONE_EDGE
		spin_lock_irqsave(&cradle->lock, flags);
		if (pen_state == STYLUS_PEN_OUT) {
			/* if pen is out, set gpio to detect in */
			irq_set_irq_type(cradle->pdata->pen_irq,
					IRQF_TRIGGER_FALLING);
		} else {
			/* if pen is in, set gpio to detect out */
			irq_set_irq_type(cradle->pdata->pen_irq,
					IRQF_TRIGGER_RISING);
		}
		spin_unlock_irqrestore(&cradle->lock, flags);
#endif

		wake_lock_timeout(&cradle->wake_lock,
			msecs_to_jiffies(cradle->wake_lock_timeout_ms));
		stylus_set_state(&cradle->pen_sdev, pen_state);

		pr_info("%s::pen state is changed to %d(%s)\n", __func__ ,
				pen_state, STR_PEN_STATE(pen_state));
	} else {
		pr_info("%s::pen state is not changed from %d(%s)\n", __func__ ,
				pen_state, STR_PEN_STATE(pen_state));
	}
}

static irqreturn_t hallic_pen_irq_handler(int irq, void *handle)
{
	cancel_delayed_work(&pen_work);
	queue_delayed_work(cradle_wq, &pen_work, msecs_to_jiffies(200));

	pr_debug("%s::pen irq done!!!!\n", __func__);

	return IRQ_HANDLED;
}

static void stylus_pen_gpio_init(struct device *dev)
{
	struct pinctrl *pinctrl = NULL;
	struct pinctrl_state *stylus_detect_cfg = NULL;
	unsigned long irq_flag;
	int ret;

	if (!cradle) {
		pr_err("%s::error, cradle is null", __func__);
		return;
	}

	/* set up GPIO */
	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s::error, fail to get pinctrl\n", __func__);
		return;
	}

	stylus_detect_cfg = pinctrl_lookup_state(pinctrl, "stylus_detect_cfg");
	if (IS_ERR(stylus_detect_cfg)) {
		pr_err("%s::error, fail to get stylus_detect_cfg\n", __func__);
		return;
	}

	pinctrl_select_state(pinctrl, stylus_detect_cfg);

	/* set up IRQ */
#ifdef HALLIC_IRQ_SUPPORT_ONLY_ONE_EDGE
	hallic_get_pen_state(cradle->pdata->pen_irq_gpio, &cradle->pen_state);

	if (cradle->pen_state == STYLUS_PEN_OUT) {
		/* if pen is out, set gpio to detect in */
		irq_flag = IRQF_TRIGGER_FALLING;
	} else {
		/* if pen is in, set gpio to detect out */
		irq_flag = IRQF_TRIGGER_RISING;
	}
#else
		irq_flag = IRQF_TRIGGER_NONE;
#endif

	/* initialize irq STYLUS_DETECT */
	if (cradle->pdata->pen_debounce > 0) {
		gpio_set_debounce(cradle->pdata->pen_irq_gpio,
				cradle->pdata->pen_debounce);
	}
	ret = request_irq(cradle->pdata->pen_irq, hallic_pen_irq_handler,
					irq_flag, "hallic_pen-eint", NULL);
	if (ret) {
		pr_err("%s::error, fail to register irq handler, ret=%d\n",
							 __func__, ret);
	}

	if (enable_irq_wake(cradle->pdata->pen_irq))
		pr_err("%s::error, fail to enable_irq_wake\n", __func__);
}

static ssize_t cradle_pen_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (cradle) {
		len = snprintf(buf, PAGE_SIZE, "pen state : %d -> %s\n",
			cradle->pen_state, STR_PEN_STATE(cradle->pen_state));
	}

	return len;
}

static struct device_attribute cradle_pen_attr = __ATTR(pen, S_IRUGO,
					cradle_pen_state_show, NULL);
#endif /* CONFIG_STYLUS_PEN_DETECTION */

static void hallic_parse_dt(struct device *dev,
		struct hallic_platform_data *pdata)
{
	int debounce[2] = {0, 0};
	int debounce_cnt = 0;

	struct device_node *node = dev->of_node;

#ifdef CONFIG_STYLUS_PEN_DETECTION
	pdata->pen_irq_gpio = -1;
	pdata->pen_debounce = 0;
	pdata->pen_irq = -1;
#endif

	if (node) {
		debounce_cnt = of_property_count_u32_elems(node, "debounce");
		pr_info("%s::debounce_cnt : %d\n", __func__, debounce_cnt);
		if (debounce_cnt >= 1 && debounce_cnt <= ARRAY_SIZE(debounce)) {
			of_property_read_u32_array(node, "debounce", debounce,
								debounce_cnt);
#ifdef CONFIG_STYLUS_PEN_DETECTION
			//pdata->pen_irq_gpio = debounce[0];
			pdata->pen_irq_gpio = of_get_named_gpio_flags(node, "pen-irq-gpio", 0, NULL);
			pdata->pen_debounce = debounce[1];

			/* First */
			pdata->pen_irq = irq_of_parse_and_map(node, 0);

			pr_info("%s::pen_irq_gpio=%d, pen_irq=%d\n",
			 __func__, pdata->pen_irq_gpio, pdata->pen_irq);
#endif
		}
	} else {
		pr_err("%s::not found for %s\n", __func__,
						HALL_IC_COMPATIBLE);
	}
}

static int hallic_cradle_probe(struct platform_device *pdev)
{
	int ret = -1;

	struct hallic_platform_data *pdata = &hallic_platform_data;

	if (pdev->dev.of_node) {
		pdev->dev.platform_data = pdata;
		hallic_parse_dt(&pdev->dev, pdata);
	} else {
		return -ENODEV;
	}

	cradle = kzalloc(sizeof(*cradle), GFP_KERNEL);
	if (!cradle)
		return -ENOMEM;

	cradle->pdata = pdata;
	spin_lock_init(&cradle->lock);
	wake_lock_init(&cradle->wake_lock, WAKE_LOCK_SUSPEND,
					"hall_ic_wakeups");
	cradle->wake_lock_timeout_ms = 3000;

#ifdef CONFIG_STYLUS_PEN_DETECTION
	if (pdata->pen_irq_gpio >= 0) {
		cradle->pen_sdev.name = "pen_state";
		/* not use - it is not necessary */
		/* cradle->pen_sdev.print_name = cradle_print_name; */

		ret = stylus_dev_register(&cradle->pen_sdev);
		if (ret < 0)
			goto err_stylus_dev_register;

#if 0 /* Check Pen IRQ */
		if (gpio_is_valid(pdata->pen_irq_gpio)) {
			ret =gpio_request(pdata->pen_irq_gpio, pdev->name);
			if (ret < 0)
				pr_info("%s:: failed to gpio_request : %d\n", __func__, ret);
		}
		ret = gpio_direction_input(pdata->pen_irq_gpio);
		if (ret < 0)
			pr_info("%s:: failed to gpio_direction_input : %d\n", __func__, ret);

		pdata->check_irq = gpio_to_irq(pdata->pen_irq_gpio);
		pr_info("%s:: Check Pen_IRQ info : %d\n", __func__, pdata->check_irq);
#endif
		INIT_DELAYED_WORK(&pen_work, hallic_pen_work_func);

		stylus_pen_gpio_init(&pdev->dev);
	}
#endif

	pr_info("%s::init cradle\n", __func__);

	boot_cradle_det_func();

#ifdef CONFIG_STYLUS_PEN_DETECTION
	if (pdata->pen_irq_gpio >= 0) {
		ret = device_create_file(&pdev->dev, &cradle_pen_attr);
		if (ret)
			goto err_request_irq;
	}
#endif

	pr_info("%s::hall_ic, probe done\n", __func__);

	return 0;

#if defined(CONFIG_STYLUS_PEN_DETECTION)
err_request_irq:
err_stylus_dev_register:
#endif
#ifdef CONFIG_STYLUS_PEN_DETECTION
	stylus_dev_unregister(&cradle->pen_sdev);
#endif

	kfree(cradle);
	cradle = NULL;

	return ret;
}

static int hallic_cradle_remove(struct platform_device *pdev)
{
#ifdef CONFIG_STYLUS_PEN_DETECTION
	cancel_delayed_work_sync(&pen_work);
	stylus_dev_unregister(&cradle->pen_sdev);
#endif

	platform_set_drvdata(pdev, NULL);

	kfree(cradle);
	cradle = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hallic_of_match[] = {
	{.compatible = HALL_IC_COMPATIBLE, },
	{},
};
#endif

static struct platform_driver hallic_cradle_driver = {
	.probe  = hallic_cradle_probe,
	.remove = hallic_cradle_remove,
	.driver	= {
		.name	= HALL_IC_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = hallic_of_match,
#endif
	},
};

static int __init hallic_cradle_init(void)
{
	cradle_wq = create_singlethread_workqueue("cradle_wq");

	if (!cradle_wq) {
		pr_err("fail to create workqueue\n");
		return -ENOMEM;
	}

	return platform_driver_register(&hallic_cradle_driver);
}

static void __exit hallic_cradle_exit(void)
{
	if (cradle_wq) {
		destroy_workqueue(cradle_wq);
		cradle_wq = NULL;
	}

	platform_driver_unregister(&hallic_cradle_driver);
}

module_init(hallic_cradle_init);
module_exit(hallic_cradle_exit);

MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("Generic HALL IC Driver for MTK platform");
MODULE_LICENSE("GPL");
