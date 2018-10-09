/*
 * awl13_sysfs.c
 *
 *  Copyright (C) 2008 Nissin Systems Co.,Ltd.
 *  Copyright (C) 2008-2011 Atmark Techno, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 2008-07-18    Created by Nissin Systems Co.,Ltd.
 * 2009-02-26   Modified by Atmark Techno, Inc.
 * 2011-11-25   Modified by Atmark Techno, Inc.
 */

#include <linux/module.h>
#include <linux/sysfs.h>

#include "awl13_log.h"
#include "awl13_device.h"
#include "awl13_wid.h"
#include "awl13_fw.h"
#include "awl13_ioctl.h"

struct awl13_attribute {
	struct device_attribute dattr;
	struct awl13_private *priv;

	int flags;
#define AFLAG_NONE	(0)
#define AFLAG_SIGNED	(1)

	int (*get8)(struct awl13_private *, unsigned char *);
	int (*set8)(struct awl13_private *, unsigned char);
	int (*get16)(struct awl13_private *, unsigned short *);
	int (*set16)(struct awl13_private *, unsigned short);
	int (*get32)(struct awl13_private *, unsigned int *);
	int (*set32)(struct awl13_private *, unsigned int);

	int (*getstr)(struct awl13_private *, char *, int);
};

#define to_awl13_attr(x) container_of(x, struct awl13_attribute, dattr)

#define AWL13_ATTR(_name, _mode, _show, _store)	\
struct awl13_attribute awl_attr_##_name = {		\
	.dattr = __ATTR(_name, _mode, _show, _store),	\
};

#define AWL13_ATTR_COMMON8(_name, _mode, _get, _set)			\
struct awl13_attribute awl_attr_##_name = {				\
	.dattr = __ATTR(_name, _mode,					\
		       awl13_sysfs_common_show,				\
		       awl13_sysfs_common_store),			\
	.get8 = (_get),							\
	.set8 = (_set),							\
};
#define AWL13_ATTR_COMMON8_SIGNED(_name, _mode, _get, _set)		\
struct awl13_attribute awl_attr_##_name = {				\
	.dattr = __ATTR(_name, _mode,					\
		       awl13_sysfs_common_show,				\
		       awl13_sysfs_common_store),			\
	.get8 = (_get),							\
	.set8 = (_set),							\
	.flags = AFLAG_SIGNED,						\
};
#define AWL13_ATTR_COMMON16(_name, _mode, _get, _set)			\
struct awl13_attribute awl_attr_##_name = {				\
	.dattr = __ATTR(_name, _mode,					\
		       awl13_sysfs_common_show,				\
		       awl13_sysfs_common_store),			\
	.get16 = (_get),						\
	.set16 = (_set),						\
};
#define AWL13_ATTR_COMMON32(_name, _mode, _get, _set)			\
struct awl13_attribute awl_attr_##_name = {				\
	.dattr = __ATTR(_name, _mode,					\
		       awl13_sysfs_common_show,				\
		       awl13_sysfs_common_store),			\
	.get32 = (_get),						\
	.set32 = (_set),						\
};
#define AWL13_ATTR_COMMON_STR(_name, _mode, _get)			\
struct awl13_attribute awl_attr_##_name = {				\
	.dattr = __ATTR(_name, _mode,					\
		       awl13_sysfs_common_string_show,			\
		       NULL),						\
	.getstr = (_get),						\
};

/******************************************************************************
 * awl13_sysfs_show_firmware -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_show_firmware(struct device *dev, struct device_attribute *dattr,
			  char *buf)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	int ret;
		ret = awl13_firmware_setup(priv);
		if (ret)
			awl_err("failed firmware load\n");
	return 0;
}

/******************************************************************************
 * awl13_sysfs_store_firmware -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_store_firmware(struct device *dev, struct device_attribute *dattr,
			   const char *buf, size_t count)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	struct awl13_firmware_info *info =
		(struct awl13_firmware_info *)buf;
	static struct awl13_firmware_info header;

	static loff_t pos = 0;

	if (info->id == AWL13_FW_ID) {
		memcpy(&header, buf, sizeof(struct awl13_firmware_info));
		pos = 0;
	}
	
	if (pos + count > FIRMWARE_SIZE_MAX)
		return -EFBIG;

	memcpy(priv->fw_image + pos, buf, count);

	pos += count;
	priv->fw_size = pos;

	return count;
}

/******************************************************************************
 * awl13_sysfs_show_scanning -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_show_scanning(struct device *dev, struct device_attribute *dattr,
			  char *buf)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	struct awl13_survey_res survey[2];
	char bar[] = {"---------------------------------------"
		      "---------------------------------------"};
	char fmt[] = {"%-33s|%-5s|%-3d|%-11s|"
		      "%02x.%02x.%02x.%02x.%02x.%02x |%d\n"};
	int len = 0;
	int ret;
	int i, j;


	/* scan AP */
	if ((ret = awl13_start_ap_scan(priv, survey)) != 0)
		return ret;

	len += sprintf(buf + len, "%-33s|%-5s|%-3s|%-11s|%-18s|%-4s\n",
		       "SSID", "Type", "Ch", "Security", "BSSID", "dB");
	len += sprintf(buf + len, "%s\n", bar);

	for (i=0; i<2; i++) {
		int nr_info = (survey[i].size /
			       sizeof(struct awl13_survey_info));

		for (j=0; j<nr_info; j++) {
			struct awl13_survey_info *info;
			char bsstype[][5] = {"BSS", "IBSS"};
			char *security;
			char tmp[16];

			info = &survey[i].info[j];

			/* check data */
			if (info->channel==0)
				continue;

			switch (info->security) {
			case AWL13_CRYPT_DISABLE:
				security = "NONE"; break;
			case AWL13_CRYPT_UNKNOWN:
				security = "UNKNOWN"; break;
			case AWL13_CRYPT_WEP64:
				security = "WEP64"; break;
			case AWL13_CRYPT_WEP128:
				security = "WEP128"; break;
			case AWL13_CRYPT_WPA_CCMP:
				security = "WPA-AES"; break;
			case AWL13_CRYPT_WPA_TKIP:
				security = "WPA-TKIP"; break;
			case AWL13_CRYPT_WPA_MIX:
				security = "WPA-MIX"; break;
			case AWL13_CRYPT_WPA2_CCMP:
				security = "WPA2-AES"; break;
			case AWL13_CRYPT_WPA2_TKIP:
				security = "WPA2-TKIP"; break;
			case AWL13_CRYPT_WPA2_MIX:
				security = "WPA2-MIX"; break;
			case AWL13_CRYPT_WPA_2_TKIP:
				security = "WPA/2-TKIP"; break;
			case AWL13_CRYPT_WPA_2_CCMP:
				security = "WPA/2-AES"; break;
			case AWL13_CRYPT_WPA_2_MIX:
				security = "WPA/2-MIX"; break;
			default:
				sprintf(tmp, "ID:%02x", info->security);
				security = tmp; break;
			}

			len += sprintf(buf + len, fmt,
				       info->ssid,
				       bsstype[(info->bsstype==0)?0:1],
				       info->channel,
				       security,
				       info->bssid[0],
				       info->bssid[1],
				       info->bssid[2],
				       info->bssid[3],
				       info->bssid[4],
				       info->bssid[5],
				       info->rxpower
				       );
		}
	}

	return len;
}

/******************************************************************************
 * awl13_sysfs_show_versions -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_show_versions(struct device *dev, struct device_attribute *dattr,
			  char *buf)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	char *print_fmt = "%25s: %s\n";
	char version[AWL13_VERSION_SIZE + 1];
	int len = 0;
	int ret;
	int i;

	struct info_table {
		int (*fn)(struct awl13_private *, char *, int);
		char *label;
	} table[] = {
		{ awl13_get_firmware_version, "Firmware" },
	};

	len += sprintf(buf + len, print_fmt, "Driver", AWL13_VERSION);

	for (i=0; i<ARRAY_SIZE(table); i++) {
		memset( version, 0x00, sizeof(version) );
		ret = table[i].fn(priv, version, AWL13_VERSION_SIZE);
		if (ret)
			return ret;

		len += sprintf(buf + len, print_fmt,
			       table[i].label, version);
	}

	return len;
}

/******************************************************************************
 * awl13_sysfs_show_driver_version -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_show_driver_version(struct device *dev, struct device_attribute *dattr,
				char *buf)
{
	int len = 0;

	len += sprintf(buf + len, "%s\n", AWL13_VERSION);

	return len;
}

/******************************************************************************
 * awl13_sysfs_show_wps_cred_list -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_show_wps_cred_list(struct device *dev, struct device_attribute *dattr,
			       char *buf)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	memcpy(buf, priv->wps_cred_list, priv->wps_cred_list_size);
	awl_devdump( buf, priv->wps_cred_list_size );
	return priv->wps_cred_list_size;
}

/******************************************************************************
 * awl13_sysfs_store_firmware -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_store_wps_cred_list(struct device *dev, struct device_attribute *dattr,
				const char *buf, size_t count)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	loff_t pos = 0;
	
	if (pos + count > WPS_CRED_LIST_SZ)
		return -EFBIG;

	memcpy(&priv->wps_cred_list[pos], buf, count);
	pos += count;
	priv->wps_cred_list_size = pos;
	awl_devdump( priv->wps_cred_list, priv->wps_cred_list_size );
	return count;
}


/******************************************************************************
 * awl13_sysfs_common_show -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_common_show(struct device *dev, struct device_attribute *dattr,
			char *buf)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	int len = 0;
	int ret;

	if (attr->get8) {
		unsigned char val;
		ret = attr->get8(priv, &val);
		if (attr->flags & AFLAG_SIGNED)
			len += sprintf(buf + len, "%d\n", (signed char)val);
		else
			len += sprintf(buf + len, "%d\n", val);
	} else if (attr->get16) {
		unsigned short val;
		ret = attr->get16(priv, &val);
		if (attr->flags & AFLAG_SIGNED)
			len += sprintf(buf + len, "%d\n", (signed short)val);
		else
			len += sprintf(buf + len, "%d\n", val);
	} else if (attr->get32) {
		unsigned int val;
		ret = attr->get32(priv, &val);
		if (attr->flags & AFLAG_SIGNED)
			len += sprintf(buf + len, "%d\n", (signed int)val);
		else
			len += sprintf(buf + len, "%d\n", val);
	} else
		return -EOPNOTSUPP;

	if (ret)
		return ret;

	return len;
}

/******************************************************************************
 * awl13_sysfs_common_store -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_common_store(struct device *dev, struct device_attribute *dattr,
			 const char *buf, size_t count)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (attr->set8) {
		if (val < 0 || 255 < val)
			return -EINVAL;
		ret = attr->set8(priv, (unsigned char)val);
	} else if (attr->set16) {
		if (val < 0 || 65535 < val)
			return -EINVAL;
		ret = attr->set16(priv, (unsigned short)val);
	} else if (attr->set32)
		ret = attr->set32(priv, (unsigned int)val);
	else
		return -EOPNOTSUPP;

	if (ret)
		return ret;

	return count;
}

/******************************************************************************
 * awl13_sysfs_common_string_show -
 *
 *****************************************************************************/
static ssize_t
awl13_sysfs_common_string_show(struct device *dev, struct device_attribute *dattr,
			       char *buf)
{
	struct awl13_attribute *attr = to_awl13_attr(dattr);
	struct awl13_private *priv = attr->priv;
	static char val[2048];

	int len = 0;
	int ret;

	if (attr->getstr)
		ret = attr->getstr(priv, val, 2048);
	else
		return -EOPNOTSUPP;

	if (ret)
		return ret;

	len += sprintf(buf + len, "%s\n", val);

	return len;
}

/******************************************************************************
 * awl13_attributes
 *****************************************************************************/
static AWL13_ATTR(firmware, 0644,
		   awl13_sysfs_show_firmware,
		   awl13_sysfs_store_firmware);
static AWL13_ATTR(scanning, 0444,
		   awl13_sysfs_show_scanning,
		   NULL);
static AWL13_ATTR(versions, 0444,
		   awl13_sysfs_show_versions,
		   NULL);
static AWL13_ATTR_COMMON8(bss_type, 0644,
			   awl13_get_bsstype,
			   awl13_set_bsstype);
static AWL13_ATTR_COMMON8(current_channel, 0644,
			   awl13_get_channel,
			   awl13_set_channel);
static AWL13_ATTR_COMMON8(auth_type, 0644,
			   awl13_get_authtype,
			   awl13_set_authtype);
static AWL13_ATTR_COMMON8(crypt_mode, 0644,
			   awl13_get_cryptmode,
			   awl13_set_cryptmode);
static AWL13_ATTR_COMMON8(power_management, 0644,
			   awl13_get_powerman,
			   awl13_set_powerman);
static AWL13_ATTR_COMMON8(scan_type, 0644,
			   awl13_get_scan_type,
			   awl13_set_scan_type);
static AWL13_ATTR_COMMON8(power_save, 0200,
			   NULL,
			   awl13_set_psctl);
static AWL13_ATTR_COMMON16(beacon_interval, 0644,
			    awl13_get_beaconint,
			    awl13_set_beaconint);
static AWL13_ATTR_COMMON8(listen_interval, 0644,
			   awl13_get_lisnintvl,
			   awl13_set_lisnintvl);
static AWL13_ATTR_COMMON8(stealth, 0644,
			   awl13_get_bcastssid,
			   awl13_set_bcastssid);
static AWL13_ATTR_COMMON8_SIGNED(rssi, 0444,
				  awl13_get_rssi_u8,
				  NULL);
static AWL13_ATTR_COMMON8(current_mac_status, 0444,
			   awl13_get_current_mac_status,
			   NULL);
static AWL13_ATTR_COMMON_STR(firmware_version, 0444,
			      awl13_get_firmware_version);
static AWL13_ATTR(driver_version, 0444,
		   awl13_sysfs_show_driver_version,
		   NULL);
static AWL13_ATTR_COMMON8(key_id, 0444,
			   awl13_get_key_id,
			   NULL);
static AWL13_ATTR_COMMON8(dtim_period, 0644,
			   awl13_get_dtim_period,
			   awl13_set_dtim_period);
static AWL13_ATTR_COMMON8(rekey_policy, 0644,
			   awl13_get_rekey_policy,
			   awl13_set_rekey_policy);
static AWL13_ATTR_COMMON32(rekey_period, 0644,
			   awl13_get_rekey_period,
			   awl13_set_rekey_period);
static AWL13_ATTR_COMMON8(scan_filter, 0644,
			   awl13_get_scan_filter,
			   awl13_set_scan_filter);

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
static AWL13_ATTR_COMMON32(tx_cis_polling_interval, 0644,
			    awl13_get_txcis_polling_intvl,
			    awl13_set_txcis_polling_intvl);
static AWL13_ATTR_COMMON32(rx_cis_polling_interval, 0644,
			    awl13_get_rxcis_polling_intvl,
			    awl13_set_rxcis_polling_intvl);
static AWL13_ATTR_COMMON32(tx_cis_toggle_timeout, 0644,
			    awl13_get_txcis_toggle_timeout,
			    awl13_set_txcis_toggle_timeout);
static AWL13_ATTR_COMMON32(rx_cis_toggle_timeout, 0644,
			    awl13_get_rxcis_toggle_timeout,
			    awl13_set_rxcis_toggle_timeout);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
static AWL13_ATTR_COMMON8(usb_in_xfer_mode, 0644,
			   awl13_get_usb_in_xfer_mode,
			   awl13_set_usb_in_xfer_mode);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
static AWL13_ATTR(wps_cred_list, 0644,
		   awl13_sysfs_show_wps_cred_list,
		   awl13_sysfs_store_wps_cred_list);

static struct awl13_attribute *awl13_attrs[] = {
	&awl_attr_firmware,
	&awl_attr_scanning,
	&awl_attr_versions,
	&awl_attr_bss_type,
	&awl_attr_current_channel,
	&awl_attr_auth_type,
	&awl_attr_crypt_mode,
	&awl_attr_power_management,
	&awl_attr_scan_type,
	&awl_attr_power_save,
	&awl_attr_beacon_interval,
	&awl_attr_listen_interval,
	&awl_attr_stealth,
	&awl_attr_rssi,
	&awl_attr_current_mac_status,
	&awl_attr_firmware_version,
	&awl_attr_driver_version,
	&awl_attr_key_id,
	&awl_attr_dtim_period,
	&awl_attr_rekey_policy,
	&awl_attr_rekey_period,
	&awl_attr_scan_filter,
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	&awl_attr_tx_cis_polling_interval,
	&awl_attr_rx_cis_polling_interval,
	&awl_attr_tx_cis_toggle_timeout,
	&awl_attr_rx_cis_toggle_timeout,
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	&awl_attr_usb_in_xfer_mode,
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	&awl_attr_wps_cred_list,
};

/******************************************************************************
 * awl13_sysfs_init -
 *
 *****************************************************************************/
int awl13_sysfs_init(struct awl13_private *priv)
{
	int ret = 0;
	int i;

	priv->attr_group.name = "awl13";
	priv->attr_group.attrs = kzalloc(sizeof(struct attribute *) *
					 (ARRAY_SIZE(awl13_attrs) + 1),
					 GFP_KERNEL);
	if (!priv->attr_group.attrs)
		return -ENOMEM;

	for (i=0; i<ARRAY_SIZE(awl13_attrs); i++) {
		awl13_attrs[i]->priv = priv;
		priv->attr_group.attrs[i] = &awl13_attrs[i]->dattr.attr;
	}

	if (get_device(&priv->netdev->dev)) {
		ret = sysfs_create_group(&priv->netdev->dev.kobj,
					 &priv->attr_group);
		if (ret) {
			if (priv->attr_group.attrs) {
				kfree(priv->attr_group.attrs);
				priv->attr_group.attrs = NULL;
			}
			return ret;
		}
		put_device(&priv->netdev->dev);
	}

	return 0;
}

/******************************************************************************
 * awl13_sysfs_remove -
 *
 *****************************************************************************/
void awl13_sysfs_remove(struct awl13_private *priv)
{
	sysfs_remove_group(&priv->netdev->dev.kobj, &priv->attr_group);

	if (priv->attr_group.attrs) {
		kfree(priv->attr_group.attrs);
		priv->attr_group.attrs = NULL;
	}
	return;
}
