/*
 * Copyright 2013 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <linux/nvmem-consumer.h>

#define REG_SET		0x4
#define REG_CLR		0x8
#define REG_TOG		0xc

#define MISC0				0x0150
#define MISC0_REFTOP_SELBIASOFF		(1 << 3)
#define MISC1				0x0160
#define MISC1_IRQ_TEMPHIGH		(1 << 29)
/* Below LOW and PANIC bits are only for TEMPMON_IMX6SX */
#define MISC1_IRQ_TEMPLOW		(1 << 28)
#define MISC1_IRQ_TEMPPANIC		(1 << 27)

#define TEMPSENSE0			0x0180
#define TEMPSENSE0_ALARM_VALUE_SHIFT	20
#define TEMPSENSE0_ALARM_VALUE_MASK	(0xfff << TEMPSENSE0_ALARM_VALUE_SHIFT)
#define TEMPSENSE0_TEMP_CNT_SHIFT	8
#define TEMPSENSE0_TEMP_CNT_MASK	(0xfff << TEMPSENSE0_TEMP_CNT_SHIFT)
#define TEMPSENSE0_FINISHED		(1 << 2)
#define TEMPSENSE0_MEASURE_TEMP		(1 << 1)
#define TEMPSENSE0_POWER_DOWN		(1 << 0)

#define TEMPSENSE1			0x0190
#define TEMPSENSE1_MEASURE_FREQ		0xffff
/* Below TEMPSENSE2 is only for TEMPMON_IMX6SX */
#define TEMPSENSE2			0x0290
#define TEMPSENSE2_LOW_VALUE_SHIFT	0
#define TEMPSENSE2_LOW_VALUE_MASK	0xfff
#define TEMPSENSE2_PANIC_VALUE_SHIFT	16
#define TEMPSENSE2_PANIC_VALUE_MASK	0xfff0000

#define OCOTP_MEM0			0x0480
#define OCOTP_ANA1			0x04e0

/* The driver supports 1 passive trip point and 1 critical trip point */
enum imx_thermal_trip {
	IMX_TRIP_PASSIVE,
	IMX_TRIP_CRITICAL,
	IMX_TRIP_NUM,
};

#define IMX_POLLING_DELAY		2000 /* millisecond */
#define IMX_PASSIVE_DELAY		1000

#define TEMPMON_IMX6Q			1
#define TEMPMON_IMX6SX			2

struct thermal_soc_data {
	u32 version;
};

static struct thermal_soc_data thermal_imx6q_data = {
	.version = TEMPMON_IMX6Q,
};

static struct thermal_soc_data thermal_imx6sx_data = {
	.version = TEMPMON_IMX6SX,
};

struct imx_thermal_data {
	struct cpufreq_policy *policy;
	struct thermal_zone_device *tz;
	struct thermal_cooling_device *cdev;
	enum thermal_device_mode mode;
	struct regmap *tempmon;
	u32 c1, c2; /* See formula in imx_init_calib() */
	int temp_passive;
	int temp_critical;
	int temp_max;
	int alarm_temp;
	int last_temp;
	bool irq_enabled;
	int irq;
	struct clk *thermal_clk;
	const struct thermal_soc_data *socdata;
	const char *temp_grade;
};

static void imx_set_panic_temp(struct imx_thermal_data *data,
			       int panic_temp)
{
	struct regmap *map = data->tempmon;
	int critical_value;

	critical_value = (data->c2 - panic_temp) / data->c1;
	regmap_write(map, TEMPSENSE2 + REG_CLR, TEMPSENSE2_PANIC_VALUE_MASK);
	regmap_write(map, TEMPSENSE2 + REG_SET, critical_value <<
			TEMPSENSE2_PANIC_VALUE_SHIFT);
}

static void imx_set_alarm_temp(struct imx_thermal_data *data,
			       int alarm_temp)
{
	struct regmap *map = data->tempmon;
	int alarm_value;

	data->alarm_temp = alarm_temp;
	alarm_value = (data->c2 - alarm_temp) / data->c1;
	regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_ALARM_VALUE_MASK);
	regmap_write(map, TEMPSENSE0 + REG_SET, alarm_value <<
			TEMPSENSE0_ALARM_VALUE_SHIFT);
}

static int imx_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct imx_thermal_data *data = tz->devdata;
	struct regmap *map = data->tempmon;
	unsigned int n_meas;
	bool wait;
	u32 val;

	if (data->mode == THERMAL_DEVICE_ENABLED) {
		/* Check if a measurement is currently in progress */
		regmap_read(map, TEMPSENSE0, &val);
		wait = !(val & TEMPSENSE0_FINISHED);
	} else {
		/*
		 * Every time we measure the temperature, we will power on the
		 * temperature sensor, enable measurements, take a reading,
		 * disable measurements, power off the temperature sensor.
		 */
		regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_POWER_DOWN);
		regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_MEASURE_TEMP);

		wait = true;
	}

	/*
	 * According to the temp sensor designers, it may require up to ~17us
	 * to complete a measurement.
	 */
	if (wait)
		usleep_range(20, 50);

	regmap_read(map, TEMPSENSE0, &val);

	if (data->mode != THERMAL_DEVICE_ENABLED) {
		regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_MEASURE_TEMP);
		regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_POWER_DOWN);
	}

	if ((val & TEMPSENSE0_FINISHED) == 0) {
		dev_dbg(&tz->device, "temp measurement never finished\n");
		return -EAGAIN;
	}

	n_meas = (val & TEMPSENSE0_TEMP_CNT_MASK) >> TEMPSENSE0_TEMP_CNT_SHIFT;

	/* See imx_init_calib() for formula derivation */
	*temp = data->c2 - n_meas * data->c1;

	/* Update alarm value to next higher trip point for TEMPMON_IMX6Q */
	if (data->socdata->version == TEMPMON_IMX6Q) {
		if (data->alarm_temp == data->temp_passive &&
			*temp >= data->temp_passive)
			imx_set_alarm_temp(data, data->temp_critical);
		if (data->alarm_temp == data->temp_critical &&
			*temp < data->temp_passive) {
			imx_set_alarm_temp(data, data->temp_passive);
			dev_dbg(&tz->device, "thermal alarm off: T < %d\n",
				data->alarm_temp / 1000);
		}
	}

	if (*temp != data->last_temp) {
		dev_dbg(&tz->device, "millicelsius: %d\n", *temp);
		data->last_temp = *temp;
	}

	/* Reenable alarm IRQ if temperature below alarm temperature */
	if (!data->irq_enabled && *temp < data->alarm_temp) {
		data->irq_enabled = true;
		enable_irq(data->irq);
	}

	return 0;
}

static int imx_get_mode(struct thermal_zone_device *tz,
			enum thermal_device_mode *mode)
{
	struct imx_thermal_data *data = tz->devdata;

	*mode = data->mode;

	return 0;
}

static int imx_set_mode(struct thermal_zone_device *tz,
			enum thermal_device_mode mode)
{
	struct imx_thermal_data *data = tz->devdata;
	struct regmap *map = data->tempmon;

	if (mode == THERMAL_DEVICE_ENABLED) {
		tz->polling_delay = IMX_POLLING_DELAY;
		tz->passive_delay = IMX_PASSIVE_DELAY;

		regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_POWER_DOWN);
		regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_MEASURE_TEMP);

		if (!data->irq_enabled) {
			data->irq_enabled = true;
			enable_irq(data->irq);
		}
	} else {
		regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_MEASURE_TEMP);
		regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_POWER_DOWN);

		tz->polling_delay = 0;
		tz->passive_delay = 0;

		if (data->irq_enabled) {
			disable_irq(data->irq);
			data->irq_enabled = false;
		}
	}

	data->mode = mode;
	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);

	return 0;
}

static int imx_get_trip_type(struct thermal_zone_device *tz, int trip,
			     enum thermal_trip_type *type)
{
	*type = (trip == IMX_TRIP_PASSIVE) ? THERMAL_TRIP_PASSIVE :
					     THERMAL_TRIP_CRITICAL;
	return 0;
}

static int imx_get_crit_temp(struct thermal_zone_device *tz, int *temp)
{
	struct imx_thermal_data *data = tz->devdata;

	*temp = data->temp_critical;
	return 0;
}

static int imx_get_trip_temp(struct thermal_zone_device *tz, int trip,
			     int *temp)
{
	struct imx_thermal_data *data = tz->devdata;

	*temp = (trip == IMX_TRIP_PASSIVE) ? data->temp_passive :
					     data->temp_critical;
	return 0;
}

static int imx_set_trip_temp(struct thermal_zone_device *tz, int trip,
			     int temp)
{
	struct imx_thermal_data *data = tz->devdata;

	/* do not allow changing critical threshold */
	if (trip == IMX_TRIP_CRITICAL)
		return -EPERM;

	/* do not allow passive to be set higher than critical */
	if (temp < 0 || temp > data->temp_critical)
		return -EINVAL;

	data->temp_passive = temp;

	imx_set_alarm_temp(data, temp);

	return 0;
}

static int imx_bind(struct thermal_zone_device *tz,
		    struct thermal_cooling_device *cdev)
{
	int ret;

	ret = thermal_zone_bind_cooling_device(tz, IMX_TRIP_PASSIVE, cdev,
					       THERMAL_NO_LIMIT,
					       THERMAL_NO_LIMIT,
					       THERMAL_WEIGHT_DEFAULT);
	if (ret) {
		dev_err(&tz->device,
			"binding zone %s with cdev %s failed:%d\n",
			tz->type, cdev->type, ret);
		return ret;
	}

	return 0;
}

static int imx_unbind(struct thermal_zone_device *tz,
		      struct thermal_cooling_device *cdev)
{
	int ret;

	ret = thermal_zone_unbind_cooling_device(tz, IMX_TRIP_PASSIVE, cdev);
	if (ret) {
		dev_err(&tz->device,
			"unbinding zone %s with cdev %s failed:%d\n",
			tz->type, cdev->type, ret);
		return ret;
	}

	return 0;
}

static struct thermal_zone_device_ops imx_tz_ops = {
	.bind = imx_bind,
	.unbind = imx_unbind,
	.get_temp = imx_get_temp,
	.get_mode = imx_get_mode,
	.set_mode = imx_set_mode,
	.get_trip_type = imx_get_trip_type,
	.get_trip_temp = imx_get_trip_temp,
	.get_crit_temp = imx_get_crit_temp,
	.set_trip_temp = imx_set_trip_temp,
};

static int imx_init_calib(struct platform_device *pdev, u32 ocotp_ana1)
{
	struct imx_thermal_data *data = platform_get_drvdata(pdev);
	int n1;
	u64 temp64;

	if (ocotp_ana1 == 0 || ocotp_ana1 == ~0) {
		dev_err(&pdev->dev, "invalid sensor calibration data\n");
		return -EINVAL;
	}

	/*
	 * The sensor is calibrated at 25 °C (aka T1) and the value measured
	 * (aka N1) at this temperature is provided in bits [31:20] in the
	 * i.MX's OCOTP value ANA1.
	 * To find the actual temperature T, the following formula has to be used
	 * when reading value n from the sensor:
	 *
	 * T = T1 + (N - N1) / (0.4148468 - 0.0015423 * N1) °C + 3.580661 °C
	 *   = [T1' - N1 / (0.4148468 - 0.0015423 * N1) °C] + N / (0.4148468 - 0.0015423 * N1) °C
	 *   = [T1' + N1 / (0.0015423 * N1 - 0.4148468) °C] - N / (0.0015423 * N1 - 0.4148468) °C
	 *   = c2 - c1 * N
	 *
	 * with
	 *
	 *  T1' = 28.580661 °C
	 *   c1 = 1 / (0.0015423 * N1 - 0.4297157) °C
	 *   c2 = T1' + N1 / (0.0015423 * N1 - 0.4148468) °C
	 *      = T1' + N1 * c1
	 */
	n1 = ocotp_ana1 >> 20;

	temp64 = 10000000; /* use 10^7 as fixed point constant for values in formula */
	temp64 *= 1000; /* to get result in °mC */
	do_div(temp64, 15423 * n1 - 4148468);
	data->c1 = temp64;
	data->c2 = n1 * data->c1 + 28581;

	return 0;
}

static void imx_init_temp_grade(struct platform_device *pdev, u32 ocotp_mem0)
{
	struct imx_thermal_data *data = platform_get_drvdata(pdev);

	/* The maximum die temp is specified by the Temperature Grade */
	switch ((ocotp_mem0 >> 6) & 0x3) {
	case 0: /* Commercial (0 to 95 °C) */
		data->temp_grade = "Commercial";
		data->temp_max = 95000;
		break;
	case 1: /* Extended Commercial (-20 °C to 105 °C) */
		data->temp_grade = "Extended Commercial";
		data->temp_max = 105000;
		break;
	case 2: /* Industrial (-40 °C to 105 °C) */
		data->temp_grade = "Industrial";
		data->temp_max = 105000;
		break;
	case 3: /* Automotive (-40 °C to 125 °C) */
		data->temp_grade = "Automotive";
		data->temp_max = 125000;
		break;
	}

	/*
	 * Set the critical trip point at 5 °C under max
	 * Set the passive trip point at 10 °C under max (changeable via sysfs)
	 */
	data->temp_critical = data->temp_max - (1000 * 5);
	data->temp_passive = data->temp_max - (1000 * 10);
}

static int imx_init_from_tempmon_data(struct platform_device *pdev)
{
	struct device_node *np;
	struct nvmem_device *nvdev;
	int ret;
	u32 val;

	np = of_parse_phandle(pdev->dev.of_node, "fsl,tempmon-data", 0);
	if (!np) {
		dev_err(&pdev->dev, "failed to get sensor device node\n");
		return -ENOENT;
	}

	nvdev = of_nvmem_find(np);
	of_node_put(np);
	if (!nvdev) {
		dev_err(&pdev->dev, "failed to get nvmem device\n");
		return -ENOENT;
	}

	ret = nvmem_device_read(nvdev, OCOTP_ANA1, 4, &val);
	if (ret < 0) {
		nvmem_device_put(nvdev);
		dev_err(&pdev->dev, "failed to read sensor data: %d\n", ret);
		return ret;
	}
	ret = imx_init_calib(pdev, val);
	if (ret) {
		nvmem_device_put(nvdev);
		return ret;
	}

	ret = nvmem_device_read(nvdev, OCOTP_MEM0, 4, &val);
	nvmem_device_put(nvdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to read temp grade: %d\n", ret);
		return ret;
	}
	imx_init_temp_grade(pdev, val);

	return 0;
}

static int imx_init_from_nvmem_cells(struct platform_device *pdev)
{
	int ret;
	u32 val;

	ret = nvmem_cell_read_u32(&pdev->dev, "calib", &val);
	if (ret)
		return ret;
	imx_init_calib(pdev, val);

	ret = nvmem_cell_read_u32(&pdev->dev, "temp_grade", &val);
	if (ret)
		return ret;
	imx_init_temp_grade(pdev, val);

	return 0;
}

static irqreturn_t imx_thermal_alarm_irq(int irq, void *dev)
{
	struct imx_thermal_data *data = dev;

	disable_irq_nosync(irq);
	data->irq_enabled = false;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t imx_thermal_alarm_irq_thread(int irq, void *dev)
{
	struct imx_thermal_data *data = dev;

	dev_dbg(&data->tz->device, "THERMAL ALARM: T > %d\n",
		data->alarm_temp / 1000);

	thermal_zone_device_update(data->tz, THERMAL_EVENT_UNSPECIFIED);

	return IRQ_HANDLED;
}

static const struct of_device_id of_imx_thermal_match[] = {
	{ .compatible = "fsl,imx6q-tempmon", .data = &thermal_imx6q_data, },
	{ .compatible = "fsl,imx6sx-tempmon", .data = &thermal_imx6sx_data, },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_imx_thermal_match);

static int imx_thermal_probe(struct platform_device *pdev)
{
	struct imx_thermal_data *data;
	struct regmap *map;
	int measure_freq;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	map = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "fsl,tempmon");
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		dev_err(&pdev->dev, "failed to get tempmon regmap: %d\n", ret);
		return ret;
	}
	data->tempmon = map;

	data->socdata = of_device_get_match_data(&pdev->dev);
	if (!data->socdata) {
		dev_err(&pdev->dev, "no device match found\n");
		return -ENODEV;
	}

	/* make sure the IRQ flag is clear before enabling irq on i.MX6SX */
	if (data->socdata->version == TEMPMON_IMX6SX) {
		regmap_write(map, MISC1 + REG_CLR, MISC1_IRQ_TEMPHIGH |
			MISC1_IRQ_TEMPLOW | MISC1_IRQ_TEMPPANIC);
		/*
		 * reset value of LOW ALARM is incorrect, set it to lowest
		 * value to avoid false trigger of low alarm.
		 */
		regmap_write(map, TEMPSENSE2 + REG_SET,
			TEMPSENSE2_LOW_VALUE_MASK);
	}

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0)
		return data->irq;

	platform_set_drvdata(pdev, data);

	if (of_find_property(pdev->dev.of_node, "nvmem-cells", NULL)) {
		ret = imx_init_from_nvmem_cells(pdev);
		if (ret == -EPROBE_DEFER)
			return ret;
		if (ret) {
			dev_err(&pdev->dev, "failed to init from nvmem: %d\n",
				ret);
			return ret;
		}
	} else {
		ret = imx_init_from_tempmon_data(pdev);
		if (ret) {
			dev_err(&pdev->dev, "failed to init from from fsl,tempmon-data\n");
			return ret;
		}
	}

	/* Make sure sensor is in known good state for measurements */
	regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_POWER_DOWN);
	regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_MEASURE_TEMP);
	regmap_write(map, TEMPSENSE1 + REG_CLR, TEMPSENSE1_MEASURE_FREQ);
	regmap_write(map, MISC0 + REG_SET, MISC0_REFTOP_SELBIASOFF);
	regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_POWER_DOWN);

	data->policy = cpufreq_cpu_get(0);
	if (!data->policy) {
		pr_debug("%s: CPUFreq policy not found\n", __func__);
		return -EPROBE_DEFER;
	}

	data->cdev = cpufreq_cooling_register(data->policy);
	if (IS_ERR(data->cdev)) {
		ret = PTR_ERR(data->cdev);
		dev_err(&pdev->dev,
			"failed to register cpufreq cooling device: %d\n", ret);
		cpufreq_cpu_put(data->policy);
		return ret;
	}

	data->thermal_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(data->thermal_clk)) {
		ret = PTR_ERR(data->thermal_clk);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"failed to get thermal clk: %d\n", ret);
		cpufreq_cooling_unregister(data->cdev);
		cpufreq_cpu_put(data->policy);
		return ret;
	}

	/*
	 * Thermal sensor needs clk on to get correct value, normally
	 * we should enable its clk before taking measurement and disable
	 * clk after measurement is done, but if alarm function is enabled,
	 * hardware will auto measure the temperature periodically, so we
	 * need to keep the clk always on for alarm function.
	 */
	ret = clk_prepare_enable(data->thermal_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable thermal clk: %d\n", ret);
		cpufreq_cooling_unregister(data->cdev);
		cpufreq_cpu_put(data->policy);
		return ret;
	}

	data->tz = thermal_zone_device_register("imx_thermal_zone",
						IMX_TRIP_NUM,
						BIT(IMX_TRIP_PASSIVE), data,
						&imx_tz_ops, NULL,
						IMX_PASSIVE_DELAY,
						IMX_POLLING_DELAY);
	if (IS_ERR(data->tz)) {
		ret = PTR_ERR(data->tz);
		dev_err(&pdev->dev,
			"failed to register thermal zone device %d\n", ret);
		clk_disable_unprepare(data->thermal_clk);
		cpufreq_cooling_unregister(data->cdev);
		cpufreq_cpu_put(data->policy);
		return ret;
	}

	dev_info(&pdev->dev, "%s CPU temperature grade - max:%dC"
		 " critical:%dC passive:%dC\n", data->temp_grade,
		 data->temp_max / 1000, data->temp_critical / 1000,
		 data->temp_passive / 1000);

	/* Enable measurements at ~ 10 Hz */
	regmap_write(map, TEMPSENSE1 + REG_CLR, TEMPSENSE1_MEASURE_FREQ);
	measure_freq = DIV_ROUND_UP(32768, 10); /* 10 Hz */
	regmap_write(map, TEMPSENSE1 + REG_SET, measure_freq);
	imx_set_alarm_temp(data, data->temp_passive);

	if (data->socdata->version == TEMPMON_IMX6SX)
		imx_set_panic_temp(data, data->temp_critical);

	regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_POWER_DOWN);
	regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_MEASURE_TEMP);

	data->irq_enabled = true;
	data->mode = THERMAL_DEVICE_ENABLED;

	ret = devm_request_threaded_irq(&pdev->dev, data->irq,
			imx_thermal_alarm_irq, imx_thermal_alarm_irq_thread,
			0, "imx_thermal", data);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request alarm irq: %d\n", ret);
		clk_disable_unprepare(data->thermal_clk);
		thermal_zone_device_unregister(data->tz);
		cpufreq_cooling_unregister(data->cdev);
		cpufreq_cpu_put(data->policy);
		return ret;
	}

	return 0;
}

static int imx_thermal_remove(struct platform_device *pdev)
{
	struct imx_thermal_data *data = platform_get_drvdata(pdev);
	struct regmap *map = data->tempmon;

	/* Disable measurements */
	regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_POWER_DOWN);
	if (!IS_ERR(data->thermal_clk))
		clk_disable_unprepare(data->thermal_clk);

	thermal_zone_device_unregister(data->tz);
	cpufreq_cooling_unregister(data->cdev);
	cpufreq_cpu_put(data->policy);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int imx_thermal_suspend(struct device *dev)
{
	struct imx_thermal_data *data = dev_get_drvdata(dev);
	struct regmap *map = data->tempmon;

	/*
	 * Need to disable thermal sensor, otherwise, when thermal core
	 * try to get temperature before thermal sensor resume, a wrong
	 * temperature will be read as the thermal sensor is powered
	 * down.
	 */
	regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_MEASURE_TEMP);
	regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_POWER_DOWN);
	data->mode = THERMAL_DEVICE_DISABLED;
	clk_disable_unprepare(data->thermal_clk);

	return 0;
}

static int imx_thermal_resume(struct device *dev)
{
	struct imx_thermal_data *data = dev_get_drvdata(dev);
	struct regmap *map = data->tempmon;
	int ret;

	ret = clk_prepare_enable(data->thermal_clk);
	if (ret)
		return ret;
	/* Enabled thermal sensor after resume */
	regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_POWER_DOWN);
	regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_MEASURE_TEMP);
	data->mode = THERMAL_DEVICE_ENABLED;

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(imx_thermal_pm_ops,
			 imx_thermal_suspend, imx_thermal_resume);

static struct platform_driver imx_thermal = {
	.driver = {
		.name	= "imx_thermal",
		.pm	= &imx_thermal_pm_ops,
		.of_match_table = of_imx_thermal_match,
	},
	.probe		= imx_thermal_probe,
	.remove		= imx_thermal_remove,
};
module_platform_driver(imx_thermal);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Thermal driver for Freescale i.MX SoCs");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-thermal");
