#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

#include <linux/pinctrl/consumer.h>


#define PCB_INDICATOR_DEV_NAME "pcb_indicator"
#define PCB_INDICATOR_COMPATIBLE "lge,pcb_indicator"
#define PCB_INDICATOR_MAX 6

struct pcb_indicator_platform_data {
	int pcb_indicator[PCB_INDICATOR_MAX];
};

static struct pcb_indicator_platform_data pcb_indicator_platform_data;
struct pinctrl *pcb_indicator_pinctrl = NULL;

#ifdef CONFIG_LGE_SKU_GPIO_PULL_UP
void set_pcb_indicator_no_pull(void)
{
	struct pinctrl_state *pcb_indicator_no_pull = NULL;

	if (IS_ERR(pcb_indicator_pinctrl)) {
		pr_err("%s::error, fail to get pcb_indicator_pinctrl\n", __func__);
		return;
	}
	pcb_indicator_no_pull = pinctrl_lookup_state(pcb_indicator_pinctrl, "pcb_indicator_no_pull");
	if (IS_ERR(pcb_indicator_no_pull)) {
		pr_err("%s::error, fail to get pcb_indicator_no_pull\n", __func__);
		return;
	}

	pinctrl_select_state(pcb_indicator_pinctrl, pcb_indicator_no_pull);
	pr_info("%s::done", __func__);
}
EXPORT_SYMBOL(set_pcb_indicator_no_pull);

void set_pcb_indicator_pull_up(void)
{
	struct pinctrl_state *pcb_indicator_pull_up = NULL;

	if (IS_ERR(pcb_indicator_pinctrl)) {
		pr_err("%s::error, fail to get pcb_indicator_pinctrl\n", __func__);
		return;
	}
	pcb_indicator_pull_up = pinctrl_lookup_state(pcb_indicator_pinctrl, "pcb_indicator_pull_up");
	if (IS_ERR(pcb_indicator_pull_up)) {
		pr_err("%s::error, fail to get pcb_indicator_pull_up\n", __func__);
		return;
	}

	pinctrl_select_state(pcb_indicator_pinctrl, pcb_indicator_pull_up);
	pr_info("%s::done", __func__);
}
EXPORT_SYMBOL(set_pcb_indicator_pull_up);
#endif

static void pcb_indicator_parse_dt(struct device *dev, struct pcb_indicator_platform_data *pdata)
{
	int pcb_indicator[PCB_INDICATOR_MAX];
	int pcb_indicator_cnt = 0;
	int i = 0;

	struct device_node *node = dev->of_node;

	if (node) {
		pcb_indicator_cnt = of_property_count_u32_elems(node, "pcb_indicator");
		pr_info("%s::pcb_indicator_cnt : %d\n", __func__, pcb_indicator_cnt);
		if (pcb_indicator_cnt >= 1 && pcb_indicator_cnt <= ARRAY_SIZE(pcb_indicator)) {
			of_property_read_u32_array(node, "pcb_indicator", pcb_indicator, pcb_indicator_cnt);
		}
		pr_info("=== pcb_indicator list ===\n");
		for (i=0; i<pcb_indicator_cnt; i++) {
			pr_info("GPIO: %d\n", pcb_indicator[i]);
		}

		pcb_indicator_pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR(pcb_indicator_pinctrl)) {
			pr_err("%s::error, fail to get pinctrl\n", __func__);
		}
	} else {
		pr_err("%s::not found for %s\n", __func__, PCB_INDICATOR_COMPATIBLE);
	}
}

static int pcb_indicator_probe(struct platform_device *pdev)
{
	struct pcb_indicator_platform_data *pdata = &pcb_indicator_platform_data;

	if (pdev->dev.of_node) {
		pdev->dev.platform_data = pdata;
		pcb_indicator_parse_dt(&pdev->dev, pdata);
	} else {
		return -ENODEV;
	}

#ifdef CONFIG_LGE_SKU_GPIO_PULL_UP
	set_pcb_indicator_pull_up();
#endif

	pr_info("%s::pcb_indicator, probe done\n", __func__);

	return 0;
}

static int pcb_indicator_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id pcb_indicator_of_match[] = {
	{.compatible = PCB_INDICATOR_COMPATIBLE, },
	{},
};
#endif

static struct platform_driver pcb_indicator_driver = {
	.probe  = pcb_indicator_probe,
	.remove = pcb_indicator_remove,
	.driver	= {
		.name	= PCB_INDICATOR_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = pcb_indicator_of_match,
#endif
	},
};

static int __init pcb_indicator_init(void)
{
	return platform_driver_register(&pcb_indicator_driver);
}

static void __exit pcb_indicator_exit(void)
{
	platform_driver_unregister(&pcb_indicator_driver);
}

module_init(pcb_indicator_init);
module_exit(pcb_indicator_exit);

MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("PCB indicator driver");
MODULE_LICENSE("GPL");
