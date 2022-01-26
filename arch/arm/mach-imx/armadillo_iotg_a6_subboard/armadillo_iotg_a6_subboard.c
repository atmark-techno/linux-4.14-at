// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Atmark Techno, Inc. All Rights Reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#define dtb_begin(f)    __dtb_##f##_begin
#define dtb_end(f)      __dtb_##f##_end
#define dtb_size(f)     (dtb_end(f) - dtb_begin(f))

#define extern_dtb(f)           \
extern uint8_t dtb_begin(f)[];  \
extern uint8_t dtb_end(f)[];

extern_dtb(armadillo_iotg_a6_rtc_nr3225sa);
extern_dtb(armadillo_iotg_a6_rtc_rv8803);

/* Atmark Techno Subboard A6 Revison Number */
#define SUBBOARD_REVISION_ATMARK_TECHNO_A6_5	(0x0005)

enum a6_subboard_rtc {
	RTC_UNKNOWN,
	RTC_NR3225SA,
	RTC_RV8803
};

static enum a6_subboard_rtc select_rtc(struct device *dev)
{
	struct device_node *root;
	unsigned long revision;
	const char *system_revision;
	int ret;

	revision = ULONG_MAX;
	root = of_find_node_by_path("/");
	if (root) {
		ret = of_property_read_string(root, "revision-number",
					      &system_revision);
		if (ret == 0) {
			ret = kstrtoul(system_revision, 16, &revision);
			if (ret)
				revision = ULONG_MAX;
		}
	}

	dev_info(dev, "Armadillo-IoT Gateway A6 subboard\n");
	if (revision == ULONG_MAX) {
		dev_info(dev, "revision undefined.");
		return RTC_NR3225SA;
	}

	dev_info(dev, "revision:%lu", revision);

	if (revision < SUBBOARD_REVISION_ATMARK_TECHNO_A6_5)
		return RTC_NR3225SA;

	return RTC_RV8803;
}

static int rtc_dt_overlay(const enum a6_subboard_rtc rtc, struct device *dev)
{
	static void *overlay_data;
	struct device_node *overlay;
	void *begin;
	size_t size;
	int ret;

	switch (rtc) {
	case RTC_NR3225SA:
		begin = dtb_begin(armadillo_iotg_a6_rtc_nr3225sa);
		size = dtb_size(armadillo_iotg_a6_rtc_nr3225sa);
		break;
	case RTC_RV8803:
		begin = dtb_begin(armadillo_iotg_a6_rtc_rv8803);
		size = dtb_size(armadillo_iotg_a6_rtc_rv8803);
		break;
	default:
		dev_warn(dev, "unknown rtc:%d\n", rtc);
		ret = -EINVAL;
		goto err;
	}

	/*
	 * Must create permanent copy of FDT because of_fdt_unflatten_tree()
	 * will create pointers to the passed in FDT in the EDT.
	 */
	overlay_data = kmemdup(begin, size, GFP_KERNEL);
	if (overlay_data == NULL) {
		dev_err(dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err;
	}

	of_fdt_unflatten_tree(overlay_data, NULL, &overlay);
	if (overlay == NULL) {
		dev_err(dev, "No tree to attach\n");
		ret = -EINVAL;
		goto err_free_overlay_data;
	}

	of_node_set_flag(overlay, OF_DETACHED);
	ret = of_resolve_phandles(overlay);
	if (ret != 0) {
		dev_err(dev, "Failed to resolve phandles\n");
		goto err_free_overlay_data;
	}

	ret = of_overlay_create(overlay);
	if (ret < 0) {
		dev_err(dev, "Failed to creating overlay\n");
		goto err_free_overlay_data;
	}

err_free_overlay_data:
	kfree(overlay_data);

err:
	return ret;
}

static int armadillo_iotg_a6_subboard_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "probe\n");

	return rtc_dt_overlay(select_rtc(&pdev->dev), &pdev->dev);
}

static int armadillo_iotg_a6_subboard_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "remove\n");

	return 0;
}

static struct of_device_id armadillo_iotg_a6_subboard_dt_ids[] = {
	{ .compatible = "armadillo_iotg_a6_subboard" },
	{ }
};

static struct platform_driver armadillo_iotg_a6_subboard_driver = {
	.driver		= {
		.name	= "armadillo_iotg_a6_subboard",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(armadillo_iotg_a6_subboard_dt_ids),
	},
	.probe		= armadillo_iotg_a6_subboard_probe,
	.remove		= armadillo_iotg_a6_subboard_remove,
};

static int __init armadillo_iotg_a6_subboard_init(void)
{
	int ret;

	ret = platform_driver_register(&armadillo_iotg_a6_subboard_driver);
	if (ret)
		printk(KERN_ERR "armadillo_iotg_a6_subboard: probe failed: %d\n", ret);

	return 0;
}
subsys_initcall_sync(armadillo_iotg_a6_subboard_init);

static void __exit armadillo_iotg_a6_subboard_exit(void)
{
	platform_driver_unregister(&armadillo_iotg_a6_subboard_driver);
}
module_exit(armadillo_iotg_a6_subboard_exit);

MODULE_AUTHOR("Atmark Techno, Inc.");
MODULE_DESCRIPTION("Armadillo-IoT Gateway A6 Subboard");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:armadillo_iotg_a6_subboard");
