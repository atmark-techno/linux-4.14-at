/*
 * GPIO Reset Controller driver
 *
 * Copyright 2013 Philipp Zabel, Pengutronix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset-controller.h>

/*
 * operations in suspend mode
 * DO_NOTHING_IN_SUSPEND - the default value
 * ASSERT_IN_SUSPEND	 - assert reset in suspend states
 * DEASSERT_IN_SUSPEND	 - de-assert reset in suspend states
 */

#define DO_NOTHING_IN_SUSPEND	0
#define ASSERT_IN_SUSPEND	1
#define DEASSERT_IN_SUSPEND	2

struct gpio_reset_data {
	struct reset_controller_dev rcdev;
	unsigned int gpio;
	bool active_low;
	s32 delay_us;
	s32 wait_delay_us;

	/* suspend states of reset */
	int suspend_action;
	int pre_suspend_state;
};

static void gpio_reset_set(struct reset_controller_dev *rcdev, int asserted)
{
	struct gpio_reset_data *drvdata = container_of(rcdev,
			struct gpio_reset_data, rcdev);
	int value = asserted;

	if (drvdata->active_low)
		value = !value;

	gpio_set_value_cansleep(drvdata->gpio, value);
}

static int gpio_reset_get(struct reset_controller_dev *rcdev)
{
	struct gpio_reset_data *drvdata = container_of(rcdev,
			struct gpio_reset_data, rcdev);
	int value;

	value = gpio_get_value_cansleep(drvdata->gpio);

	if (drvdata->active_low)
		value = !value;

	return value;
}

static int gpio_reset(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct gpio_reset_data *drvdata = container_of(rcdev,
			struct gpio_reset_data, rcdev);

	if (drvdata->delay_us < 0)
		return -ENOSYS;

	if ((drvdata->wait_delay_us / 1000) >  MAX_UDELAY_MS)
		mdelay(drvdata->wait_delay_us / 1000);
	else
		udelay(drvdata->wait_delay_us);
	gpio_reset_set(rcdev, 1);
	if ((drvdata->delay_us / 1000) >  MAX_UDELAY_MS)
		mdelay(drvdata->delay_us / 1000);
	else
		udelay(drvdata->delay_us);
	gpio_reset_set(rcdev, 0);

	return 0;
}

static int gpio_reset_assert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	gpio_reset_set(rcdev, 1);

	return 0;
}

static int gpio_reset_deassert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	gpio_reset_set(rcdev, 0);

	return 0;
}

static int gpio_reset_status(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return gpio_reset_get(rcdev);
}

static struct reset_control_ops gpio_reset_ops = {
	.reset = gpio_reset,
	.assert = gpio_reset_assert,
	.deassert = gpio_reset_deassert,
	.status = gpio_reset_status,
};

static int of_gpio_reset_xlate(struct reset_controller_dev *rcdev,
			       const struct of_phandle_args *reset_spec)
{
	if (WARN_ON(reset_spec->args_count != 0))
		return -EINVAL;

	return 0;
}

static int gpio_reset_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct gpio_reset_data *drvdata;
	enum of_gpio_flags flags;
	unsigned long gpio_flags;
	bool initially_in_reset;
	bool reset_on_init;
	int ret;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL)
		return -ENOMEM;

	if (of_gpio_named_count(np, "reset-gpios") != 1) {
		dev_err(&pdev->dev,
			"reset-gpios property missing, or not a single gpio\n");
		return -EINVAL;
	}

	drvdata->gpio = of_get_named_gpio_flags(np, "reset-gpios", 0, &flags);
	if (drvdata->gpio == -EPROBE_DEFER) {
		return drvdata->gpio;
	} else if (!gpio_is_valid(drvdata->gpio)) {
		dev_err(&pdev->dev, "invalid reset gpio: %d\n", drvdata->gpio);
		return drvdata->gpio;
	}

	drvdata->active_low = flags & OF_GPIO_ACTIVE_LOW;

	ret = of_property_read_u32(np, "reset-delay-us", &drvdata->delay_us);
	if (ret < 0)
		drvdata->delay_us = -1;
	else if (drvdata->delay_us < 0)
		dev_warn(&pdev->dev, "reset delay too high\n");

	ret = of_property_read_u32(np, "wait-delay-us", &drvdata->wait_delay_us);
	if (ret < 0)
		drvdata->wait_delay_us = 0;
	else if (drvdata->wait_delay_us < 0)
		dev_warn(&pdev->dev, "wait delay too high\n");

	initially_in_reset = of_property_read_bool(np, "initially-in-reset");
	if (drvdata->active_low ^ initially_in_reset)
		gpio_flags = GPIOF_OUT_INIT_HIGH;
	else
		gpio_flags = GPIOF_OUT_INIT_LOW;

	ret = devm_gpio_request_one(&pdev->dev, drvdata->gpio, gpio_flags, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request gpio %d: %d\n",
			drvdata->gpio, ret);
		return ret;
	}

	if (!initially_in_reset) {
		reset_on_init = of_property_read_bool(np, "reset-on-init");
		if (reset_on_init)
			gpio_reset(&drvdata->rcdev, 0);
	}

	if (of_property_read_bool(np, "reset-assert-in-suspend"))
		drvdata->suspend_action = ASSERT_IN_SUSPEND;
	if (of_property_read_bool(np, "reset-deassert-in-suspend"))
		drvdata->suspend_action = DEASSERT_IN_SUSPEND;

	platform_set_drvdata(pdev, drvdata);

	drvdata->rcdev.of_node = np;
	drvdata->rcdev.owner = THIS_MODULE;
	drvdata->rcdev.nr_resets = 1;
	drvdata->rcdev.ops = &gpio_reset_ops;
	drvdata->rcdev.of_xlate = of_gpio_reset_xlate;
	reset_controller_register(&drvdata->rcdev);

	return 0;
}

static int gpio_reset_remove(struct platform_device *pdev)
{
	struct gpio_reset_data *drvdata = platform_get_drvdata(pdev);

	reset_controller_unregister(&drvdata->rcdev);

	return 0;
}

static struct of_device_id gpio_reset_dt_ids[] = {
	{ .compatible = "gpio-reset" },
	{ }
};

#ifdef CONFIG_PM_SLEEP
static int gpio_reset_suspend(struct device *dev)
{
	struct gpio_reset_data *drvdata = dev_get_drvdata(dev);

	if (drvdata->suspend_action != DO_NOTHING_IN_SUSPEND) {
		drvdata->pre_suspend_state = gpio_reset_status(&drvdata->rcdev, 0);
		if (drvdata->suspend_action == ASSERT_IN_SUSPEND)
			gpio_reset_assert(&drvdata->rcdev, 0);
		else if (drvdata->suspend_action == DEASSERT_IN_SUSPEND)
			gpio_reset_deassert(&drvdata->rcdev, 0);
	}

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}
static int gpio_reset_resume(struct device *dev)
{
	struct gpio_reset_data *drvdata = dev_get_drvdata(dev);

	pinctrl_pm_select_default_state(dev);

	if (drvdata->suspend_action != DO_NOTHING_IN_SUSPEND) {
		if (drvdata->pre_suspend_state)
			gpio_reset_assert(&drvdata->rcdev, 0);
		else
			gpio_reset_deassert(&drvdata->rcdev, 0);
	}

	return 0;
}
#endif

static const struct dev_pm_ops gpio_reset_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(gpio_reset_suspend, gpio_reset_resume)
};

static struct platform_driver gpio_reset_driver = {
	.probe = gpio_reset_probe,
	.remove = gpio_reset_remove,
	.driver = {
		.name = "gpio-reset",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gpio_reset_dt_ids),
		.pm = &gpio_reset_pm_ops,
	},
};

static int __init gpio_reset_init(void)
{
	return platform_driver_register(&gpio_reset_driver);
}
arch_initcall(gpio_reset_init);

static void __exit gpio_reset_exit(void)
{
	platform_driver_unregister(&gpio_reset_driver);
}
module_exit(gpio_reset_exit);

MODULE_AUTHOR("Philipp Zabel <p.zabel@pengutronix.de>");
MODULE_DESCRIPTION("gpio reset controller");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio-reset");
MODULE_DEVICE_TABLE(of, gpio_reset_dt_ids);
