/*
 * RTC driver for the NDK NR3225SA
 *
 * Copyright (C) 2018 Atmark Techno, Inc.
 *
 * Hiroaki OHSAWA <ohsawa@atmark-techno.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/bcd.h>
#include <linux/bitops.h>
#include <linux/log2.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/rtc.h>

#define NR3225SA_I2C_TRY_COUNT		4

#define NR3225SA_SEC			0x00
#define NR3225SA_MIN			0x01
#define NR3225SA_HOUR			0x02
#define NR3225SA_WEEK			0x03
#define NR3225SA_DAY			0x04
#define NR3225SA_MONTH			0x05
#define NR3225SA_YEAR			0x06
#define NR3225SA_ALARM_MIN		0x07
#define NR3225SA_ALARM_HOUR		0x08
#define NR3225SA_ALARM_WEEK_OR_DAY	0x09
#define NR3225SA_TIMER_COUNTER		0x0A
#define NR3225SA_SELECT			0x0B
#define NR3225SA_FLAG			0x0C
#define NR3225SA_CTRL			0x0D

#define NR3225SA_SELECT_AS		BIT(1)

#define NR3225SA_FLAG_VDHF			BIT(5)
#define NR3225SA_FLAG_VDLF			BIT(4)
#define NR3225SA_FLAG_AF			BIT(1)
#define NR3225SA_FLAG_TF			BIT(2)
#define NR3225SA_FLAG_UTF			BIT(0)
#define NR3225SA_FLAG_MASK			\
	(NR3225SA_FLAG_VDHF | NR3225SA_FLAG_VDLF | NR3225SA_FLAG_AF | \
	 NR3225SA_FLAG_TF | NR3225SA_FLAG_UTF)

#define NR3225SA_CTRL_RESET		BIT(7)

#define NR3225SA_CTRL_AIE			BIT(1)
#define NR3225SA_CTRL_TIE			BIT(2)
#define NR3225SA_CTRL_UTIE			BIT(0)

struct nr3225sa_data {
	struct i2c_client *client;
	struct rtc_device *rtc;
	struct mutex flags_lock;
	u8 ctrl;
};

static int nr3225sa_read_reg(const struct i2c_client *client, u8 reg)
{
	int try = NR3225SA_I2C_TRY_COUNT;
	s32 ret;

	/*
	 * There is a 61µs window during which the RTC does not acknowledge I2C
	 * transfers. In that case, ensure that there are multiple attempts.
	 */
	do
		ret = i2c_smbus_read_byte_data(client, reg);
	while ((ret == -ENXIO || ret == -EIO) && --try);
	if (ret < 0)
		dev_err(&client->dev, "Unable to read register 0x%02x\n", reg);

	return ret;
}

static int nr3225sa_read_regs(const struct i2c_client *client,
			    u8 reg, u8 count, u8 *values)
{
	int try = NR3225SA_I2C_TRY_COUNT;
	s32 ret;

	do
		ret = i2c_smbus_read_i2c_block_data(client, reg, count, values);
	while ((ret == -ENXIO || ret == -EIO) && --try);
	if (ret != count) {
		dev_err(&client->dev,
			"Unable to read registers 0x%02x..0x%02x\n",
			reg, reg + count - 1);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static int nr3225sa_write_reg(const struct i2c_client *client, u8 reg, u8 value)
{
	int try = NR3225SA_I2C_TRY_COUNT;
	s32 ret;

	do
		ret = i2c_smbus_write_byte_data(client, reg, value);
	while ((ret == -ENXIO || ret == -EIO) && --try);
	if (ret)
		dev_err(&client->dev, "Unable to write register 0x%02x\n", reg);

	return ret;
}

static int nr3225sa_write_regs(const struct i2c_client *client,
			     u8 reg, u8 count, const u8 *values)
{
	int try = NR3225SA_I2C_TRY_COUNT;
	s32 ret;

	do
		ret = i2c_smbus_write_i2c_block_data(client, reg, count,
						     values);
	while ((ret == -ENXIO || ret == -EIO) && --try);
	if (ret)
		dev_err(&client->dev,
			"Unable to write registers 0x%02x..0x%02x\n",
			reg, reg + count - 1);

	return ret;
}

static irqreturn_t nr3225sa_handle_irq(int irq, void *dev_id)
{
	struct i2c_client *client = dev_id;
	struct nr3225sa_data *nr3225sa = i2c_get_clientdata(client);
	unsigned long events = 0;
	int flags;
	u8 new_flags = NR3225SA_FLAG_MASK;

	mutex_lock(&nr3225sa->flags_lock);

	flags = nr3225sa_read_reg(client, NR3225SA_FLAG);
	if (flags <= 0) {
		mutex_unlock(&nr3225sa->flags_lock);
		return IRQ_NONE;
	}

	if (flags & NR3225SA_FLAG_VDHF)
		dev_warn(&client->dev, "Voltage low, temperature compensation stopped.\n");

	if (flags & NR3225SA_FLAG_VDLF)
		dev_warn(&client->dev, "Voltage low, data loss detected.\n");

	if (flags & NR3225SA_FLAG_TF) {
		new_flags &= ~NR3225SA_FLAG_TF;
		nr3225sa->ctrl &= ~NR3225SA_CTRL_TIE;
		events |= RTC_PF;
	}

	if (flags & NR3225SA_FLAG_AF) {
		new_flags &= ~NR3225SA_FLAG_AF;
		nr3225sa->ctrl &= ~NR3225SA_CTRL_AIE;
		events |= RTC_AF;
	}

	if (flags & NR3225SA_FLAG_UTF) {
		new_flags &= ~NR3225SA_FLAG_UTF;
		nr3225sa->ctrl &= ~NR3225SA_CTRL_UTIE;
		events |= RTC_UF;
	}

	if (events) {
		rtc_update_irq(nr3225sa->rtc, 1, events);
		nr3225sa_write_reg(client, NR3225SA_FLAG, new_flags);
		nr3225sa_write_reg(nr3225sa->client, NR3225SA_CTRL, nr3225sa->ctrl);
	}

	mutex_unlock(&nr3225sa->flags_lock);

	return IRQ_HANDLED;
}

static int nr3225sa_get_time(struct device *dev, struct rtc_time *tm)
{
	struct nr3225sa_data *nr3225sa = dev_get_drvdata(dev);
	u8 date1[7];
	u8 date2[7];
	u8 *date = date1;
	int ret, flags;

	flags = nr3225sa_read_reg(nr3225sa->client, NR3225SA_FLAG);
	if (flags < 0)
		return flags;

	if (flags & NR3225SA_FLAG_VDLF) {
		dev_warn(dev, "Voltage low, data is invalid.\n");
		return -EINVAL;
	}

	ret = nr3225sa_read_regs(nr3225sa->client, NR3225SA_SEC, 7, date);
	if (ret)
		return ret;

	if ((date1[NR3225SA_SEC] & 0x7f) == bin2bcd(59)) {
		ret = nr3225sa_read_regs(nr3225sa->client, NR3225SA_SEC, 7, date2);
		if (ret)
			return ret;

		if ((date2[NR3225SA_SEC] & 0x7f) != bin2bcd(59))
			date = date2;
	}

	tm->tm_sec  = bcd2bin(date[NR3225SA_SEC] & 0x7f);
	tm->tm_min  = bcd2bin(date[NR3225SA_MIN] & 0x7f);
	tm->tm_hour = bcd2bin(date[NR3225SA_HOUR] & 0x3f);
	tm->tm_wday = date[NR3225SA_WEEK] & 0x07;
	tm->tm_mday = bcd2bin(date[NR3225SA_DAY] & 0x3f);
	tm->tm_mon  = bcd2bin(date[NR3225SA_MONTH] & 0x1f) - 1;
	tm->tm_year = bcd2bin(date[NR3225SA_YEAR]) + 100;

	return 0;
}

static int nr3225sa_set_time(struct device *dev, struct rtc_time *tm)
{
	struct nr3225sa_data *nr3225sa = dev_get_drvdata(dev);
	u8 date[7];
	int ctrl, ret;

	if ((tm->tm_year < 100) || (tm->tm_year > 199))
		return -EINVAL;

	ctrl = nr3225sa_read_reg(nr3225sa->client, NR3225SA_CTRL);
	if (ctrl < 0)
		return ctrl;

	/* Stop the clock */
	ret = nr3225sa_write_reg(nr3225sa->client, NR3225SA_CTRL,
			       ctrl | NR3225SA_CTRL_RESET);
	if (ret)
		return ret;

	date[NR3225SA_SEC]   = bin2bcd(tm->tm_sec);
	date[NR3225SA_MIN]   = bin2bcd(tm->tm_min);
	date[NR3225SA_HOUR]  = bin2bcd(tm->tm_hour);
	date[NR3225SA_WEEK]  = tm->tm_wday;
	date[NR3225SA_DAY]   = bin2bcd(tm->tm_mday);
	date[NR3225SA_MONTH] = bin2bcd(tm->tm_mon + 1);
	date[NR3225SA_YEAR]  = bin2bcd(tm->tm_year - 100);

	ret = nr3225sa_write_regs(nr3225sa->client, NR3225SA_SEC, 7, date);
	if (ret)
		return ret;

	/* Restart the clock */
	ret = nr3225sa_write_reg(nr3225sa->client, NR3225SA_CTRL,
			       ctrl & ~NR3225SA_CTRL_RESET);
	if (ret)
		return ret;

	mutex_lock(&nr3225sa->flags_lock);

	ret = nr3225sa_write_reg(nr3225sa->client, NR3225SA_FLAG,
			       NR3225SA_FLAG_MASK & ~(NR3225SA_FLAG_VDHF | NR3225SA_FLAG_VDLF));

	mutex_unlock(&nr3225sa->flags_lock);

	return ret;
}

static int nr3225sa_get_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct nr3225sa_data *nr3225sa = dev_get_drvdata(dev);
	struct i2c_client *client = nr3225sa->client;
	u8 alarmvals[3];
	int flags, ret;

	ret = nr3225sa_read_regs(client, NR3225SA_ALARM_MIN, 3, alarmvals);
	if (ret)
		return ret;

	flags = nr3225sa_read_reg(client, NR3225SA_FLAG);
	if (flags < 0)
		return flags;

	alrm->time.tm_sec  = 0;
	alrm->time.tm_min  = bcd2bin(alarmvals[0] & 0x7f);
	alrm->time.tm_hour = bcd2bin(alarmvals[1] & 0x3f);
	alrm->time.tm_mday = bcd2bin(alarmvals[2] & 0x3f);

	alrm->enabled = !!(nr3225sa->ctrl & NR3225SA_CTRL_AIE);
	alrm->pending = (flags & NR3225SA_FLAG_AF) && alrm->enabled;

	return 0;
}

static int nr3225sa_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct nr3225sa_data *nr3225sa = dev_get_drvdata(dev);
	u8 alarmvals[3];
	u8 flags;
	int err;

	/* The alarm has no seconds, round up to nearest minute */
	if (alrm->time.tm_sec) {
		time64_t alarm_time = rtc_tm_to_time64(&alrm->time);

		alarm_time += 60 - alrm->time.tm_sec;
		rtc_time64_to_tm(alarm_time, &alrm->time);
	}

	mutex_lock(&nr3225sa->flags_lock);

	alarmvals[0] = bin2bcd(alrm->time.tm_min);
	alarmvals[1] = bin2bcd(alrm->time.tm_hour);
	alarmvals[2] = bin2bcd(alrm->time.tm_mday);

	if (nr3225sa->ctrl & (NR3225SA_CTRL_AIE | NR3225SA_CTRL_UTIE)) {
		nr3225sa->ctrl &= ~(NR3225SA_CTRL_AIE | NR3225SA_CTRL_UTIE);
		err = nr3225sa_write_reg(nr3225sa->client, NR3225SA_CTRL,
				       nr3225sa->ctrl);
		if (err) {
			mutex_unlock(&nr3225sa->flags_lock);
			return err;
		}
	}

	flags = NR3225SA_FLAG_MASK & ~NR3225SA_FLAG_AF;
	err = nr3225sa_write_reg(nr3225sa->client, NR3225SA_FLAG, flags);
	mutex_unlock(&nr3225sa->flags_lock);
	if (err)
		return err;

	err = nr3225sa_write_regs(nr3225sa->client, NR3225SA_ALARM_MIN, 3, alarmvals);
	if (err)
		return err;

	if (alrm->enabled) {
		if (nr3225sa->rtc->uie_rtctimer.enabled)
			nr3225sa->ctrl |= NR3225SA_CTRL_UTIE;
		if (nr3225sa->rtc->aie_timer.enabled)
			nr3225sa->ctrl |= NR3225SA_CTRL_AIE;

		err = nr3225sa_write_reg(nr3225sa->client, NR3225SA_CTRL,
				       nr3225sa->ctrl);
		if (err)
			return err;
	}

	return 0;
}

static int nr3225sa_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nr3225sa_data *nr3225sa = dev_get_drvdata(dev);
	int ctrl, flags, err;

	ctrl = nr3225sa->ctrl;

	if (enabled) {
		if (nr3225sa->rtc->uie_rtctimer.enabled)
			ctrl |= NR3225SA_CTRL_UTIE;
		if (nr3225sa->rtc->aie_timer.enabled)
			ctrl |= NR3225SA_CTRL_AIE;
	} else {
		if (!nr3225sa->rtc->uie_rtctimer.enabled)
			ctrl &= ~NR3225SA_CTRL_UTIE;
		if (!nr3225sa->rtc->aie_timer.enabled)
			ctrl &= ~NR3225SA_CTRL_AIE;
	}

	mutex_lock(&nr3225sa->flags_lock);
	flags = NR3225SA_FLAG_MASK & ~(NR3225SA_FLAG_AF | NR3225SA_FLAG_UTF);
	err = nr3225sa_write_reg(client, NR3225SA_FLAG, flags);
	mutex_unlock(&nr3225sa->flags_lock);
	if (err)
		return err;

	if (ctrl != nr3225sa->ctrl) {
		nr3225sa->ctrl = ctrl;
		err = nr3225sa_write_reg(client, NR3225SA_CTRL, nr3225sa->ctrl);
		if (err)
			return err;
	}

	return 0;
}

static int nr3225sa_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nr3225sa_data *nr3225sa = dev_get_drvdata(dev);
	int flags, ret = 0;

	switch (cmd) {
	case RTC_VL_READ:
		flags = nr3225sa_read_reg(client, NR3225SA_FLAG);
		if (flags < 0)
			return flags;

		if (flags & NR3225SA_FLAG_VDHF)
			dev_warn(&client->dev, "Voltage low, temperature compensation stopped.\n");

		if (flags & NR3225SA_FLAG_VDLF)
			dev_warn(&client->dev, "Voltage low, data loss detected.\n");

		flags &= NR3225SA_FLAG_VDHF | NR3225SA_FLAG_VDLF;

		if (copy_to_user((void __user *)arg, &flags, sizeof(int)))
			return -EFAULT;

		return 0;

	case RTC_VL_CLR:
		mutex_lock(&nr3225sa->flags_lock);
		flags = NR3225SA_FLAG_MASK & ~(NR3225SA_FLAG_VDHF | NR3225SA_FLAG_VDLF);
		ret = nr3225sa_write_reg(client, NR3225SA_FLAG, flags);
		mutex_unlock(&nr3225sa->flags_lock);
		if (ret)
			return ret;

		return 0;

	default:
		return -ENOIOCTLCMD;
	}
}

static struct rtc_class_ops nr3225sa_rtc_ops = {
	.read_time = nr3225sa_get_time,
	.set_time = nr3225sa_set_time,
	.ioctl = nr3225sa_ioctl,
};

static int nr3225sa_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct nr3225sa_data *nr3225sa;
	int err, flags;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&adapter->dev, "doesn't support I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_I2C_BLOCK\n");
		return -EIO;
	}

	nr3225sa = devm_kzalloc(&client->dev, sizeof(struct nr3225sa_data),
			      GFP_KERNEL);
	if (!nr3225sa)
		return -ENOMEM;

	mutex_init(&nr3225sa->flags_lock);
	nr3225sa->client = client;
	i2c_set_clientdata(client, nr3225sa);

	flags = NR3225SA_SELECT_AS;
	err = nr3225sa_write_reg(client, NR3225SA_SELECT, flags);
	if (err)
		return err;

	flags = nr3225sa_read_reg(client, NR3225SA_FLAG);
	if (flags < 0)
		return flags;

	if (flags & NR3225SA_FLAG_VDHF)
		dev_warn(&client->dev, "Voltage low, temperature compensation stopped.\n");

	if (flags & NR3225SA_FLAG_VDLF)
		dev_warn(&client->dev, "Voltage low, data loss detected.\n");

	if (flags & NR3225SA_FLAG_AF)
		dev_warn(&client->dev, "An alarm maybe have been missed.\n");

	nr3225sa->rtc = devm_rtc_allocate_device(&client->dev);
	if (IS_ERR(nr3225sa->rtc)) {
		return PTR_ERR(nr3225sa->rtc);
	}

	device_init_wakeup(&client->dev, true);

	/* the nr3225sa alarm only supports a minute accuracy */
	nr3225sa->rtc->uie_unsupported = 1;

	if (client->irq > 0) {
		err = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, nr3225sa_handle_irq,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						"nr3225sa", client);
		if (err) {
			dev_warn(&client->dev, "unable to request IRQ, alarms disabled\n");
			client->irq = 0;
		} else {
			nr3225sa_rtc_ops.read_alarm = nr3225sa_get_alarm;
			nr3225sa_rtc_ops.set_alarm = nr3225sa_set_alarm;
			nr3225sa_rtc_ops.alarm_irq_enable = nr3225sa_alarm_irq_enable;
		}
	}

	nr3225sa->rtc->ops = &nr3225sa_rtc_ops;
	err = rtc_register_device(nr3225sa->rtc);
	if (err) {
		device_init_wakeup(&client->dev, false);
		return err;
	}

	nr3225sa->rtc->max_user_freq = 1;

	return 0;
}

#ifdef CONFIG_PM
static int nr3225sa_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(client->irq);

	return 0;
}

static int nr3225sa_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(client->irq);

	return 0;
}
#else
#define nr3225sa_suspend	NULL
#define nr3225sa_resume		NULL
#endif  /* CONFIG_PM */

static const struct dev_pm_ops nr3225sa_pm_ops = {
	.suspend = nr3225sa_suspend,
	.resume  = nr3225sa_resume,
};

static const struct i2c_device_id nr3225sa_id[] = {
	{ "nr3225sa", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, nr3225sa_id);

static const struct of_device_id nr3225sa_of_match[] = {
	{
		.compatible = "ndk,nr3225sa",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, nr3225sa_of_match);

static struct i2c_driver nr3225sa_driver = {
	.driver = {
		.name = "rtc-nr3225sa",
		.pm = &nr3225sa_pm_ops,
		.of_match_table = nr3225sa_of_match,
	},
	.probe		= nr3225sa_probe,
	.id_table	= nr3225sa_id,
};
module_i2c_driver(nr3225sa_driver);

MODULE_AUTHOR("Hiroaki OHSAWA <ohsawa@atmark-techno.com>");
MODULE_DESCRIPTION("NDK NR3225SA RTC driver");
MODULE_LICENSE("GPL v2");
