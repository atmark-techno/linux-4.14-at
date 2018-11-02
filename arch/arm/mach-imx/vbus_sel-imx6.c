#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include "common.h"

static struct notifier_block nb_otg1, nb_otg2;

static int imx6_vbus_sel_regulator_notify(struct notifier_block *nb,
					  unsigned long event,
					  void *ignored)
{
	if (event == REGULATOR_EVENT_PRE_DISABLE) {
		/*1:USB_OTG1_VBUS, 0:USB_OTG2_VBUS*/
		imx_anatop_3p0_vbus_sel(&nb_otg1 != nb);
	}

	return NOTIFY_OK;
}


static int imx6_vbus_sel_probe(struct platform_device *pdev)
{
	struct regulator *otg1_vbus_reg, *otg2_vbus_reg;
	unsigned long flags;
	int ret;

	otg1_vbus_reg = devm_regulator_get_optional(&pdev->dev, "otg1-vbus-reg");
	if (IS_ERR(otg1_vbus_reg)) {
		if (PTR_ERR(otg1_vbus_reg) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
	} else {
		nb_otg1.notifier_call = &imx6_vbus_sel_regulator_notify;
		ret = devm_regulator_register_notifier(otg1_vbus_reg, &nb_otg1);
		if (ret) {
			dev_err(&pdev->dev,
				"OTG1 VBUS regulator notifier request failed\n");
			return ret;
		}

		local_irq_save(flags);
		imx_anatop_3p0_vbus_sel(regulator_is_enabled(otg1_vbus_reg));
		local_irq_restore(flags);
	}

	otg2_vbus_reg = devm_regulator_get_optional(&pdev->dev, "otg2-vbus-reg");
	if (IS_ERR(otg2_vbus_reg)) {
		if (PTR_ERR(otg2_vbus_reg) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
	} else {
		nb_otg2.notifier_call = &imx6_vbus_sel_regulator_notify;
		ret = devm_regulator_register_notifier(otg2_vbus_reg, &nb_otg2);
		if (ret) {
			dev_err(&pdev->dev,
				"OTG2 VBUS regulator notifier request failed\n");
			return ret;
		}

		local_irq_save(flags);
		imx_anatop_3p0_vbus_sel(!regulator_is_enabled(otg2_vbus_reg));
		local_irq_restore(flags);
	}

	return 0;
}

static struct of_device_id imx6_vbus_sel_dt_ids[] = {
	{ .compatible = "imx6-vbus-sel" },
	{ }
};

static struct platform_driver imx6_vbus_sel_driver = {
	.driver = {
		.name = "imx6-vbus-sel",
		.of_match_table = imx6_vbus_sel_dt_ids,
	},
	.probe = imx6_vbus_sel_probe,
};

static int __init imx6_vbus_sel_init(void)
{
	return platform_driver_register(&imx6_vbus_sel_driver);
}
subsys_initcall_sync(imx6_vbus_sel_init);
