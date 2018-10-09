/*
 * awl13_ioctl.c
 *
 *  Copyright (C) 2008 Nissin Systems Co.,Ltd.
 *  Copyright (C) 2008-2012 Atmark Techno, Inc.
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
 * 2008-03-05    Created by Nissin Systems Co.,Ltd.
 * 2009-06-19   Modified by Atmark Techno, Inc.
 * 2011-11-25   Modified by Atmark Techno, Inc.
 * 2012-06-12   Modified by Atmark Techno, Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
#include <linux/mmc/core.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <net/iw_handler.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
#include <linux/ctype.h>
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */


#include "awl13_log.h"
#include "awl13_device.h"
#include "awl13_ioctl.h"
#include "awl13_fw.h"
#include "awl13_wid.h"


#define CHARCODE_TO_NUMBIN(cc, ret)		\
({						\
	unsigned char num;			\
	ret = 0;				\
	switch ((cc)) {				\
	case '0':	/* FALL THROUGH */	\
	case '1':	/* FALL THROUGH */	\
	case '2':	/* FALL THROUGH */	\
	case '3':	/* FALL THROUGH */	\
	case '4':	/* FALL THROUGH */	\
	case '5':	/* FALL THROUGH */	\
	case '6':	/* FALL THROUGH */	\
	case '7':	/* FALL THROUGH */	\
	case '8':	/* FALL THROUGH */	\
	case '9':				\
		num = (cc) - '0'; break;	\
	case 'a':	/* FALL THROUGH */	\
	case 'b':	/* FALL THROUGH */	\
	case 'c':	/* FALL THROUGH */	\
	case 'd':	/* FALL THROUGH */	\
	case 'e':	/* FALL THROUGH */	\
	case 'f':	/* FALL THROUGH */	\
		num = (cc) - 'a' + 10; break;	\
	case 'A':	/* FALL THROUGH */	\
	case 'B':	/* FALL THROUGH */	\
	case 'C':	/* FALL THROUGH */	\
	case 'D':	/* FALL THROUGH */	\
	case 'E':	/* FALL THROUGH */	\
	case 'F':				\
		num = (cc) - 'A' + 10; break;	\
	default:				\
		num = 0; ret = -1; break;	\
	}					\
	num;					\
})

#define NUMBIN_TO_CHARCODE(num)			\
({						\
	char cc;				\
	switch ((num)) {			\
	case 0:		/* FALL THROUGH */	\
	case 1:		/* FALL THROUGH */	\
	case 2:		/* FALL THROUGH */	\
	case 3:		/* FALL THROUGH */	\
	case 4:		/* FALL THROUGH */	\
	case 5:		/* FALL THROUGH */	\
	case 6:		/* FALL THROUGH */	\
	case 7:		/* FALL THROUGH */	\
	case 8:		/* FALL THROUGH */	\
	case 9:					\
		cc = (num) + '0'; break;	\
	case 10:	/* FALL THROUGH */	\
	case 11:	/* FALL THROUGH */	\
	case 12:	/* FALL THROUGH */	\
	case 13:	/* FALL THROUGH */	\
	case 14:	/* FALL THROUGH */	\
	case 15:				\
		cc = (num) - 10 + 'A'; break;	\
	default:				\
		cc = 0;				\
	}					\
	cc;					\
})

static const int awl13_support_freq[] = {
	/* 0: no support */
	[ 0] =    0, [ 1] = 2412, [ 2] = 2417, [ 3] = 2422,
	[ 4] = 2427, [ 5] = 2432, [ 6] = 2437, [ 7] = 2442,
	[ 8] = 2447, [ 9] = 2452, [10] = 2457, [11] = 2462,
	[12] = 2467, [13] = 2472,
};

static unsigned char scan_ch_mode = 0x01;
static unsigned char scan_filter  = 0x00;
static unsigned short cur_wait_time= 50;

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
#define netdev_usbpriv(dev)  ({						\
	struct awl13_usbnet *usbnetDev = netdev_priv(dev);			\
	struct awl13_data    *awl13Data = (struct awl13_data *)usbnetDev->data; \
	awl13Data->priv;})
#define devToPriv  netdev_usbpriv
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
#define devToPriv  netdev_priv
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

static inline long
_pow(long m, short e)
{
	long r = m;
	int i;
	if (e == 0)
		return 1;
	if (e < 0)
		/* not impremented */
		return -1;
	for (i=1; i<e; i++)
		r *= m;

	return r;
}

static int
support_channel(unsigned char ch)
{
	if ((ch < ARRAY_SIZE(awl13_support_freq))
	    && (awl13_support_freq[ch]))
		return 1; /* supported */

	return 0; /* not supported */
}

static unsigned char
freq_to_channel(struct iw_freq *freq)
{
	int f;
	int i;

	/* check parameter */
	if (!freq || (freq->m == 0) ||
	    (freq->e > 1)) {		/* f is more than 10G: error */
		return 0xff; /* not support */
	}

	/* calculate frequency(f)  */
	f = freq->m;
	if (freq->e < 6)
		f /= _pow(10, 6 - freq->e);
	else if (freq->e > 6)
		f *= _pow(10, freq->e - 6);

	/* seek */
	for (i=0; i<ARRAY_SIZE(awl13_support_freq); i++)
		if (awl13_support_freq[i] == f)
			return i; /* support */
	return 0xff; /* not support */
}

static int
awl13_iw_not_support(struct net_device *dev, struct iw_request_info *info,
		      void *arg, char *extra)
{
	return -EOPNOTSUPP;
}

static int
awl13_iw_giwname(struct net_device *dev, struct iw_request_info *info,
		  char *name, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	if (atomic_read(&priv->power_state) == AWL13_PWR_STATE_SLEEP)
		strncpy(name, "sleeping", IFNAMSIZ);
	else
		strncpy(name, "IEEE 802.11bgn", IFNAMSIZ);

	return 0;
}

static int
awl13_iw_siwfreq(struct net_device *dev, struct iw_request_info *info,
		  struct iw_freq *freq, char *extra)
{

	struct awl13_private *priv = devToPriv(dev);

	unsigned char ch;
	int ret;

	awl_debug("l.%d: iw_freq of m=%d,e=%d,i=%u,flags=%u\n", 
		 __LINE__, freq->m, freq->e, freq->i, freq->flags);

	/* convert frequency to channel */
	if ((freq->e == 0) && (freq->m >= 0) && (freq->m <= 1000)) {
		/* f is less than or equal to 1000 : f is treated as channel */
		ch = freq->m;
	}
	else { /* f is more than 1000 : f is treated as freq */
		ch = freq_to_channel(freq);
		
	}

	/* check whether "ch" is supported channel */
	if (!support_channel(ch))
		return -EINVAL;

	/* set channel */
	ret = awl13_set_channel(priv, ch);
	if (ret)
		return ret; /* error */

	return 0; /* success */
}

static int
awl13_iw_giwfreq(struct net_device *dev, struct iw_request_info *info,
		  struct iw_freq *freq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char ch;
	int ret;

	awl_debug("l.%d: iw_freq of m=%d,e=%d,i=%u,flags=%u\n", 
		 __LINE__, freq->m, freq->e, freq->i, freq->flags);

	ret = awl13_get_channel(priv, &ch);
	if (ret)
		return ret;

	if (!support_channel(ch))
		return -EINVAL;

	freq->e = 0;
	freq->m = ch;

	return 0;
}

static int
awl13_iw_siwmode(struct net_device *dev, struct iw_request_info *info,
		  __u32 *mode, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char type;
	int ret;

	switch (*mode) {
	case IW_MODE_INFRA:
		type = AWL13_BSS_INFRA; break;
	case IW_MODE_ADHOC:
		type = AWL13_BSS_ADHOC; break;
	case IW_MODE_AUTO: /* WLAN OFF is treated as auto. */
		type = AWL13_BSS_WLANOFF; break; 
	case IW_MODE_MASTER:
		type = AWL13_BSS_AP; break; 
	default:
		return -EINVAL;
	}

	ret = awl13_set_bsstype(priv, type);
	if (ret)
		return ret;

	return 0;
}

static int
awl13_iw_giwmode(struct net_device *dev, struct iw_request_info *info,
		  __u32 *mode, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	unsigned char type;
	int ret;

	ret = awl13_get_bsstype(priv, &type);
	if (ret)
		return ret;

	switch (type) {
	case AWL13_BSS_INFRA:
		*mode = IW_MODE_INFRA; break;
	case AWL13_BSS_ADHOC:
		*mode = IW_MODE_ADHOC; break;
	case AWL13_BSS_WLANOFF: /* WLAN OFF is treated as auto. */
		*mode = IW_MODE_AUTO; break;
	case AWL13_BSS_AP:
		*mode = IW_MODE_MASTER; break;
	default:
		return -EINVAL;
	}

	return 0;
}

struct awl13_rate {
	__s32 rate;
	unsigned char val;
};
static const struct awl13_rate rates[] = {
	{  1000000, 0x01, },
	{  2000000, 0x02, },
	{  5500000, 0x05, },
	{  6000000, 0x06, },
	{  9000000, 0x09, },
	{ 11000000, 0x0b, },
	{ 12000000, 0x0c, },
	{ 18000000, 0x12, },
	{ 24000000, 0x18, },
	{ 36000000, 0x24, },
	{ 48000000, 0x30, },
	{ 54000000, 0x36, },
};
#define SGI_NUM 2
static const struct awl13_rate rates_mcs[SGI_NUM][8] = {
	{ {  6500000, 0x80, },
	  { 13000000, 0x81, },
	  { 19500000, 0x82, },
	  { 26000000, 0x83, },
	  { 39000000, 0x84, },
	  { 52000000, 0x85, },
	  { 58500000, 0x86, },
	  { 65000000, 0x87, }, },
	{ {  7200000, 0x80, },
	  { 14400000, 0x81, },
	  { 21700000, 0x82, },
	  { 28900000, 0x83, },
	  { 43300000, 0x84, },
	  { 57800000, 0x85, },
	  { 65000000, 0x86, },
	  { 72200000, 0x87, }, },
};
#define IS_MCS(rate) ((rate) & 0x80)
#define MCS(rate)    ((uint8_t)((rate) & ~0x80))

static int
awl13_iw_giwrange(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *data, char *extra)
{
	struct iw_range *range = (struct iw_range *)extra;
	int i, j;

	memset(range, 0, sizeof(struct iw_range));
	data->length = sizeof(struct iw_range);

	range->num_channels = 0;
	for (i=0; i<ARRAY_SIZE(awl13_support_freq); i++) {
		if (awl13_support_freq[i]) {
			range->freq[range->num_channels].i = i;
			range->freq[range->num_channels].m =
				awl13_support_freq[i];
			range->freq[range->num_channels].e = 6;
			range->num_channels++;
		}
	}
	range->num_frequency = range->num_channels;
	range->max_encoding_tokens = AWL13_KEYINDEX_MAX;
	range->num_encoding_sizes = 2;
	range->encoding_size[0] = 5;  /* 64bits WEP */
	range->encoding_size[1] = 13; /* 128bits WEP */
	range->encoding_size[2] = 32; /* 256bits WPA-PSK */

	range->max_qual.qual = 0;
	range->max_qual.level = 0x100 - 100;
	range->max_qual.noise = 0;

	range->num_bitrates = 0;
	for (i = 0; i < ARRAY_SIZE(rates); i++)
		if (range->num_bitrates <= 0 ||
		    range->bitrate[range->num_bitrates - 1] < rates[i].rate)
			range->bitrate[range->num_bitrates++] = rates[i].rate;
	for (j = 0; j < SGI_NUM; j++)
		for (i = 0; i < ARRAY_SIZE(rates_mcs[j]); i++)
			if (range->num_bitrates <= 0 ||
			    range->bitrate[range->num_bitrates - 1] <
			    rates_mcs[j][i].rate)
				range->bitrate[range->num_bitrates++] =
					rates_mcs[j][i].rate;

	range->txpower_capa = IW_TXPOW_DBM;

	range->enc_capa = (IW_ENC_CAPA_WPA |
			   IW_ENC_CAPA_WPA2 |
			   IW_ENC_CAPA_CIPHER_TKIP |
			   IW_ENC_CAPA_CIPHER_CCMP);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)
	range->scan_capa = IW_SCAN_CAPA_ESSID;
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24) */
	range->event_capa[0] = (IW_EVENT_CAPA_K_0 |
				IW_EVENT_CAPA_MASK(SIOCGIWAP));
	range->event_capa[1] = IW_EVENT_CAPA_K_1;

	range->we_version_source = SUPPORTED_WIRELESS_EXT;
	range->we_version_compiled = WIRELESS_EXT;

	return 0;
}

static int
awl13_iw_siwap(struct net_device *dev, struct iw_request_info *info,
		struct sockaddr *awrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	memcpy(priv->bssid, awrq->sa_data, ETH_ALEN);

	return 0;
}

static int
awl13_iw_giwap(struct net_device *dev, struct iw_request_info *info,
		struct sockaddr *awrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char bssid[ETH_ALEN];
	int ret;

	ret = awl13_get_bssid(priv, bssid, ETH_ALEN);
	if (ret)
		return ret;

	memcpy(priv->iwap.ap_addr.sa_data, bssid, ETH_ALEN);
	priv->iwap.ap_addr.sa_family = ARPHRD_ETHER;
	memcpy(awrq, &priv->iwap.ap_addr, sizeof(*awrq));

	return 0;
}

static int
awl13_iw_siwscan(struct net_device *dev, struct iw_request_info *info,
		  struct iw_point *srq, char *extra)
{
	return 0;
}

static int
awl13_iw_giwscan(struct net_device *dev, struct iw_request_info *info,
		  struct iw_point *srq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	struct awl13_survey_res survey[2];
	struct iw_event iwe;

	char *cev = extra, *cev_end = extra + IW_SCAN_MAX_DATA;
	int ret;
	int i, j;
	char *pret;
	uint8_t ie[28], *len, *cnt;

	/* scan AP */
	if ((ret = awl13_start_ap_scan(priv, survey)) != 0)
		return ret;

	for (i=0; i<2; i++) {
		int nr_info = (survey[i].size /
			       sizeof(struct awl13_survey_info));
		for (j=0; j<nr_info; j++) {
			struct awl13_survey_info *sv_info;
			sv_info = &survey[i].info[j];

			/* check data */
			if (sv_info->channel==0)
				continue;
			
			/* First entry *MUST* be the BSSID */
			iwe.cmd = SIOCGIWAP;
			iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
			memcpy(iwe.u.ap_addr.sa_data, sv_info->bssid, ETH_ALEN);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
			pret = iwe_stream_add_event(cev, cev_end, &iwe,
						   IW_EV_ADDR_LEN);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			pret = iwe_stream_add_event(info, cev, cev_end, &iwe,
						   IW_EV_ADDR_LEN);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			/* check returned value */
			if ((cev + IW_EV_ADDR_LEN) != pret) {
				awl_debug("l.%d: "
					  "iwe_stream_add_event() failed\n",
					  __LINE__);
				return -EIO;
			}
			cev = pret;

			/* SSID */
			iwe.cmd = SIOCGIWESSID;
			iwe.u.data.length = strlen(sv_info->ssid);
			iwe.u.data.flags = 1;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
			pret = iwe_stream_add_point(cev, cev_end, &iwe,
						   sv_info->ssid);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			pret = iwe_stream_add_point(info, cev, cev_end, &iwe,
						   sv_info->ssid);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			/* check returned value */
			if ((cev + IW_EV_POINT_LEN + iwe.u.data.length)
			    != pret) { 
				awl_debug("l.%d: "
					  "iwe_stream_add_point() failed\n",
					  __LINE__);
				return -EIO;
			}
			cev = pret;

			/* Mode */
			iwe.cmd = SIOCGIWMODE;
			switch (sv_info->bsstype) {
			case 0:
				iwe.u.mode = IW_MODE_MASTER;
				break;
			case 1:
				iwe.u.mode = IW_MODE_ADHOC;
				break;
			case 2:
				iwe.u.mode = IW_MODE_AUTO;
				break;
			default:
				awl_err ("invalid bsstype "
					 "WID_SITE_SURVEY_RESULTS: %d\n",
					 sv_info->bsstype);
				return -EPROTONOSUPPORT;
			}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
			pret = iwe_stream_add_event(cev, cev_end, &iwe,
						   IW_EV_UINT_LEN);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			pret = iwe_stream_add_event(info, cev, cev_end, &iwe,
						   IW_EV_UINT_LEN);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			/* check returned value */
			if ((cev + IW_EV_UINT_LEN) != pret) {
				awl_debug("l.%d: "
					  "iwe_stream_addevent() failed\n",
					  __LINE__);
				return -EIO;
			}
			cev = pret;

			/* Frequency */
			iwe.cmd = SIOCGIWFREQ;
			iwe.u.freq.m = awl13_support_freq[sv_info->channel];
			iwe.u.freq.e = 6;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
			pret = iwe_stream_add_event(cev, cev_end, &iwe,
						   IW_EV_FREQ_LEN);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			pret = iwe_stream_add_event(info, cev, cev_end, &iwe,
						   IW_EV_FREQ_LEN);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			/* check returned value */
			if ((cev + IW_EV_FREQ_LEN) != pret) {
				awl_debug("l.%d: "
					  "iwe_stream_add_event() failed\n",
					  __LINE__);
				return -EIO;
			}
			cev = pret;

			/* Encryption capability */
			iwe.cmd = SIOCGIWENCODE;
			if (sv_info->security & AWL13_CRYPT_ENABLE)
				iwe.u.data.flags = (IW_ENCODE_ENABLED |
						    IW_ENCODE_NOKEY);
			else
				iwe.u.data.flags = IW_ENCODE_DISABLED;

			iwe.u.data.length = 0;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
			pret = iwe_stream_add_point(cev, cev_end, &iwe, NULL);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			pret = iwe_stream_add_point(info, cev, cev_end,
						    &iwe, NULL);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			/* check returned value */
			if ((cev + IW_EV_POINT_LEN + iwe.u.data.length)
			    != pret) {
				awl_debug("l.%d: "
					  "iwe_stream_add_point() failed\n",
					  __LINE__);
				return -EIO;
			}
			cev = pret;

			/* set link quality */
			iwe.cmd = IWEVQUAL;
			/* signeal level (rxpower) */
			if (sv_info->rxpower > -100) {
				iwe.u.qual.level = 0x100 + sv_info->rxpower;
				iwe.u.qual.updated = IW_QUAL_DBM |
					IW_QUAL_LEVEL_UPDATED |
					IW_QUAL_QUAL_INVALID |
					IW_QUAL_NOISE_INVALID;
			}
			else
				iwe.u.qual.updated = IW_QUAL_ALL_INVALID;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
			pret = iwe_stream_add_event(cev, cev_end, &iwe,
						    IW_EV_QUAL_LEN);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			pret = iwe_stream_add_event(info, cev, cev_end,
						    &iwe, IW_EV_QUAL_LEN);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
			/* check returned value */
			if ((cev + IW_EV_QUAL_LEN) != pret) {
				awl_debug("l.%d: "
					  "iwe_stream_add_event() failed\n",
					  __LINE__);
				return -EIO;
			}
			cev = pret;

			/* Information element */
			if (sv_info->security & AWL13_CRYPT_WPA2) {
				ie[0] = 0x30;
				len = &ie[1];
				*len = 0;
				ie[2 + (*len)++] = 0x01;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x0f;
				ie[2 + (*len)++] = 0xac;
				if (sv_info->security & AWL13_CRYPT_WEPSZ)
					ie[2 + (*len)++] = 0x05;
				else if (sv_info->security & AWL13_CRYPT_WEP)
					ie[2 + (*len)++] = 0x01;
				else if (sv_info->security & AWL13_CRYPT_TKIP)
					ie[2 + (*len)++] = 0x02;
				else if (sv_info->security & AWL13_CRYPT_CCMP)
					ie[2 + (*len)++] = 0x04;
				else
					ie[2 + (*len)++] = 0x00;
				cnt = &ie[2 + *len];
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x00;
				if (sv_info->security & AWL13_CRYPT_CCMP) {
					ie[2 + (*len)++] = 0x00;
					ie[2 + (*len)++] = 0x0f;
					ie[2 + (*len)++] = 0xac;
					ie[2 + (*len)++] = 0x04;
					(*cnt)++;
				}
				if (sv_info->security & AWL13_CRYPT_TKIP) {
					ie[2 + (*len)++] = 0x00;
					ie[2 + (*len)++] = 0x0f;
					ie[2 + (*len)++] = 0xac;
					ie[2 + (*len)++] = 0x02;
					(*cnt)++;
				}
				ie[2 + (*len)++] = 0x01;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x0f;
				ie[2 + (*len)++] = 0xac;
				ie[2 + (*len)++] = 0x02;

				iwe.cmd = IWEVGENIE;
				iwe.u.data.length = 2 + *len;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
				pret = iwe_stream_add_point(cev, cev_end, &iwe,
							    ie);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
				pret = iwe_stream_add_point(info, cev, cev_end,
							    &iwe, ie);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
				/* check returned value */
				if ((cev + IW_EV_POINT_LEN + iwe.u.data.length)
				    != pret) { 
					awl_debug("l.%d: "
						  "iwe_stream_add_point() failed\n",
						  __LINE__);
					return -EIO;
				}
				cev = pret;
			}
			if (sv_info->security & AWL13_CRYPT_WPA) {
				ie[0] = 0xdd;
				len = &ie[1];
				*len = 0;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x50;
				ie[2 + (*len)++] = 0xf2;
				ie[2 + (*len)++] = 0x01;
				ie[2 + (*len)++] = 0x01;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x50;
				ie[2 + (*len)++] = 0xf2;
				if (sv_info->security & AWL13_CRYPT_WEPSZ)
					ie[2 + (*len)++] = 0x05;
				else if (sv_info->security & AWL13_CRYPT_WEP)
					ie[2 + (*len)++] = 0x01;
				else if (sv_info->security & AWL13_CRYPT_TKIP)
					ie[2 + (*len)++] = 0x02;
				else if (sv_info->security & AWL13_CRYPT_CCMP)
					ie[2 + (*len)++] = 0x04;
				else
					ie[2 + (*len)++] = 0x00;
				cnt = &ie[2 + *len];
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x00;
				if (sv_info->security & AWL13_CRYPT_CCMP) {
					ie[2 + (*len)++] = 0x00;
					ie[2 + (*len)++] = 0x50;
					ie[2 + (*len)++] = 0xf2;
					ie[2 + (*len)++] = 0x04;
					(*cnt)++;
				}
				if (sv_info->security & AWL13_CRYPT_TKIP) {
					ie[2 + (*len)++] = 0x00;
					ie[2 + (*len)++] = 0x50;
					ie[2 + (*len)++] = 0xf2;
					ie[2 + (*len)++] = 0x02;
					(*cnt)++;
				}
				ie[2 + (*len)++] = 0x01;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x00;
				ie[2 + (*len)++] = 0x50;
				ie[2 + (*len)++] = 0xf2;
				ie[2 + (*len)++] = 0x02;

				iwe.cmd = IWEVGENIE;
				iwe.u.data.length = 2 + *len;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
				pret = iwe_stream_add_point(cev, cev_end, &iwe,
							   ie);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
				pret = iwe_stream_add_point(info, cev, cev_end,
							   &iwe, ie);
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26) */
				/* check returned value */
				if ((cev + IW_EV_POINT_LEN + iwe.u.data.length)
				    != pret) { 
					awl_debug("l.%d: "
						  "iwe_stream_add_point() failed\n",
						  __LINE__);
					return -EIO;
				}
				cev = pret;
			}
		}
	}

	srq->length = cev - extra;
	srq->flags = 0;

	return 0;
}

static int
awl13_iw_siwessid(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *data, char *essid)
{
	int ret;
	struct awl13_private *priv = devToPriv(dev);

	awl_debug("l.%d: data->flags=0x%x, data->length=%d\n",
		 __LINE__, data->flags, data->length);

	if (data->flags == 0) { /* in case of ANY or OFF */
		data->length = 1; /* set 1 because iwconfig set 0 */
	}

	/* set ESSID string */
	if ((ret = awl13_set_ssid(priv, essid, data->length)) != 0) {
		return ret; /* error */
	}

	return 0;
}

static int
awl13_iw_giwessid(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *data, char *essid)
{
	struct awl13_private *priv = devToPriv(dev);
	char buf[AWL13_SSID_MAX + 1];
	int size = min(AWL13_SSID_MAX, (int)data->length);

	memset(buf, 0, sizeof(buf));
	if (awl13_get_ssid(priv, buf, size) != 0)
		return -EINVAL;
	memcpy(essid, buf, size);

	data->flags = 1;
	data->length = strlen(buf);

	return 0;
}

static int
awl13_iw_siwrate(struct net_device *dev, struct iw_request_info *info,
		  struct iw_param *vwrq, char *extra)
{
	struct awl13_private	*priv = devToPriv(dev);
	uint8_t			rate = 0, sslot = 1, n = 1;
	uint8_t			mcs = MCS(rates_mcs[0]
					  [ARRAY_SIZE(rates_mcs[0]) - 1].val);
	uint8_t			sgi = 0;
	int			ret, i, j;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (vwrq->fixed) {
		for (i = 0; i < ARRAY_SIZE(rates); i++) {
			rate = rates[i].val;
			if (vwrq->value <= rates[i].rate) {
				if (vwrq->value <= 2000000)
					sslot = 0;
				n = 0;
				break;
			}
		}
		if (n) {
			for (j = 0; j < SGI_NUM; j++) {
				for (i = 0; i < ARRAY_SIZE(rates_mcs[j]); i++) {
					mcs = MCS(rates_mcs[j][i].val);
					sgi = j;
					if (vwrq->value <= rates_mcs[j][i].rate)
						break;
				}
				if (i < ARRAY_SIZE(rates_mcs[j]))
					break;
			}
		}
	}

	ret = awl13_set_common_value(priv, WID_CURRENT_TX_RATE,
				      &rate, 1, WID_DEFAULT_TIMEOUT);
	if (ret < 0)
		return ret;
	ret = awl13_set_common_value(priv, WID_SHORT_SLOT_ALLOWED,
				      &sslot, 1, WID_DEFAULT_TIMEOUT);
	if (ret < 0)
		return ret;
	ret = awl13_set_common_value(priv, WID_11N_ENABLE,
				      &n, 1, WID_DEFAULT_TIMEOUT);
	if (ret < 0)
		return ret;
	ret = awl13_set_common_value(priv, WID_11N_CURRENT_TX_MCS,
				      &mcs, 1, WID_DEFAULT_TIMEOUT);
	if (ret < 0)
		return ret;
	ret = awl13_set_common_value(priv, WID_11N_SHORT_GI_20MHZ_ENABLE,
				      &sgi, 1, WID_DEFAULT_TIMEOUT);
	if (ret < 0)
		return ret;

	return 0;
}

static int
awl13_iw_giwrate(struct net_device *dev, struct iw_request_info *info,
		  struct iw_param *vwrq, char *extra)
{
	struct awl13_private	*priv = devToPriv(dev);
	__u32			mode;
	uint8_t			rate, n, mcs, sgi;
	int			ret, i;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_iw_giwmode(dev, NULL, &mode, NULL);
	if (ret < 0)
		return ret;

	if (mode == IW_MODE_AUTO) {
		vwrq->fixed = 0;
		vwrq->disabled = 1;
		vwrq->value = 0;
		return 0;
	}

	ret = awl13_get_common_value(priv, WID_CURRENT_TX_RATE,
				      &rate, 1, WID_DEFAULT_TIMEOUT);
	if (ret < 0)
		return ret;
	vwrq->fixed = (rate != 0);
	if (mode == IW_MODE_MASTER) {
		ret = awl13_get_common_value(priv, WID_11N_ENABLE,
					     &n, 1, WID_DEFAULT_TIMEOUT);
		if (ret < 0)
			return ret;
		if (n) {
			ret = awl13_get_common_value(priv,
						     WID_11N_CURRENT_TX_MCS,
						     &mcs, 1,
						     WID_DEFAULT_TIMEOUT);
			if (ret < 0)
				return ret;
			rate = 0x80 | mcs;
		}
		else if (!rate)
			rate = 0x36;
	}
	else {
		ret = awl13_get_common_value(priv,
					     WID_CURRENT_SELECTING_TX_RATE,
					     &rate, 1, WID_DEFAULT_TIMEOUT);
		if (ret < 0)
			return ret;
	}
	vwrq->disabled = !rate;
	if (!rate) {
		vwrq->value = 0;
		return 0;
	}
	if (!IS_MCS(rate)) {
		for (i = 0; i < ARRAY_SIZE(rates); i++) {
			if (rate == rates[i].val) {
				vwrq->value = rates[i].rate;
				break;
			}
		}
		if (i >= ARRAY_SIZE(rates))
			return -EINVAL;
	}
	else {
		ret = awl13_get_common_value
			(priv, WID_11N_SHORT_GI_20MHZ_ENABLE,
			 &sgi, 1, WID_DEFAULT_TIMEOUT);
		if (ret < 0)
			return ret;
		if (sgi >= SGI_NUM)
			return -EINVAL;
		for (i = 0; i < ARRAY_SIZE(rates_mcs[sgi]); i++)
			if (rate == rates_mcs[sgi][i].val) {
				vwrq->value = rates_mcs[sgi][i].rate;
				break;
			}
		if (i >= ARRAY_SIZE(rates_mcs[sgi]))
			return -EINVAL;
	}

	return 0;
}

#define POWER_NUM 5
static const int power11b[POWER_NUM] = { 15, /*13.5*/14, 12, /*10.5*/11,  9 };
static const int power11g[POWER_NUM] = { 13, /*11.5*/12, 10, /* 8.5*/ 9,  7 };
static const int power11n[POWER_NUM] = { 12, /*10.5*/11,  9, /* 7.5*/ 8,  6 };

static int
awl13_iw_siwtxpow(struct net_device *dev, struct iw_request_info *info,
		  struct iw_param *vwrq, char *extra)
{
	struct awl13_private	*priv = devToPriv(dev);
	struct iw_param		raterq;
	uint8_t			powerrate;
	const int		*power;
	int			ret;

	if (vwrq->disabled || (vwrq->flags & IW_TXPOW_TYPE) != IW_TXPOW_DBM)
		return -EOPNOTSUPP;

	powerrate = 0;
	if (vwrq->fixed) {
		ret = awl13_iw_giwrate(dev, NULL, &raterq, NULL);
		if (ret < 0)
			return ret;

		if (raterq.value == 1000000 || raterq.value == 2000000 ||
		    raterq.value == 5500000 || raterq.value == 11000000)
			power = power11b;
		else if (raterq.value <= 54000000)
			power = power11g;
		else
			power = power11n;
		while (power[powerrate + 1] >= vwrq->value)
			if (++powerrate + 1 >= POWER_NUM)
				break;
	}

	return awl13_set_common_value(priv, WID_TX_POWER_RATE,
				      &powerrate, 1, WID_DEFAULT_TIMEOUT);
}

static int
awl13_iw_giwtxpow(struct net_device *dev, struct iw_request_info *info,
		  struct iw_param *vwrq, char *extra)
{
	struct awl13_private	*priv = devToPriv(dev);
	struct iw_param		raterq;
	uint8_t			powerrate;
	const int		*power;
	int			ret;

	ret = awl13_iw_giwrate(dev, NULL, &raterq, NULL);
	if (ret < 0)
		return ret;

	if (raterq.value == 1000000 || raterq.value == 2000000 ||
	    raterq.value == 5500000 || raterq.value == 11000000)
		power = power11b;
	else if (raterq.value <= 54000000)
		power = power11g;
	else
		power = power11n;

	ret = awl13_get_common_value(priv, WID_TX_POWER_RATE,
				     &powerrate, 1, WID_DEFAULT_TIMEOUT);
	if (ret < 0)
		return ret;

	if (raterq.value == 1000000 || raterq.value == 2000000 ||
	    raterq.value == 5500000 || raterq.value == 11000000)
		power = power11b;
	else if (raterq.value <= 54000000)
		power = power11g;
	else
		power = power11n;
	if (powerrate >= POWER_NUM)
		powerrate = POWER_NUM - 1;

	vwrq->fixed = 1;
	vwrq->flags = IW_TXPOW_DBM;
	if (raterq.disabled)
		vwrq->value = 0;
	else
		vwrq->value = power[powerrate];
	vwrq->disabled = raterq.disabled;

	return 0;
}

static int
awl13_checkcrypt(unsigned char crypt);

static int
awl13_iw_siwenc(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *dwrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	unsigned char crypt = AWL13_CRYPT_ENABLE | AWL13_CRYPT_WEP;
	unsigned char buf[sizeof(priv->prev_enckey)];
	int index;
	int ret;
	int i;

	awl_debug("l.%d: dwrq->flags=0x%x, dwrq->length=%d\n",
		 __LINE__, dwrq->flags, dwrq->length);

	if ((dwrq->flags & IW_ENCODE_DISABLED) ||
	    (dwrq->flags & IW_ENCODE_OPEN)) {
		return awl13_set_cryptmode(priv, AWL13_CRYPT_DISABLE);
	}

	if (dwrq->flags & IW_ENCODE_NOKEY)
		return -EINVAL;

	index = dwrq->flags & IW_ENCODE_INDEX;
	if ((index < 0) || (AWL13_KEYINDEX_MAX < index))
		return -EINVAL;
	if (index != 0)
		index--;

	if (!extra)
		return -EINVAL;
	switch (dwrq->length) {
	case (AWL13_WEPKEY_MAX / 2):
		crypt |= AWL13_CRYPT_WEPSZ;
		/* FALL THROUGH */
	case (AWL13_WEPKEY_MIN / 2):
		memset(buf, 0, sizeof(buf));
		/* convert to ascii code */
		for (i=0; i<dwrq->length; i++) {
			buf[i*2  ] = NUMBIN_TO_CHARCODE((extra[i] >> 4) & 0x0f);
			buf[i*2+1] = NUMBIN_TO_CHARCODE(extra[i] & 0x0f);
		}
		/* preserve */
		memcpy(priv->prev_enckey, buf, sizeof(priv->prev_enckey));
		priv->prev_crypt = crypt;
		break;
	case 0:
		if ((priv->prev_enckey[0] != '\0') && crypt != 0) {
			crypt = priv->prev_crypt;
			memcpy(buf, priv->prev_enckey,
			       sizeof(priv->prev_enckey));
		}
		else {
			awl_err("There is no code key before\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	/* set value to module */
	if (awl13_checkcrypt(crypt)) {
		ret = awl13_set_cryptmode(priv, crypt);
		if (ret)
			return ret;
	}
	ret = awl13_set_wepkey(priv, index, buf, strlen(buf));
	if (ret)
		return ret;

	return 0;
}

static int
awl13_iw_giwenc(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *dwrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	unsigned char crypt;
	unsigned char enckey[sizeof(priv->prev_enckey)];
	int encsize, index;
	int ret, i;

	ret = awl13_get_cryptmode(priv, &crypt);
	if (ret)
		return ret;

	index = dwrq->flags & IW_ENCODE_INDEX;
	if ((index < 0) || (AWL13_KEYINDEX_MAX < index))
		return -EINVAL;
	if (index != 0)
		index--;

	switch (crypt) {
	case AWL13_CRYPT_WEP64:
		encsize = AWL13_WEPKEY_MIN;
		break;
	case AWL13_CRYPT_WEP128:
		encsize = AWL13_WEPKEY_MAX;
		break;
	default:
		dwrq->length = 0;
		dwrq->flags = IW_ENCODE_DISABLED;
		return 0;
	}

	memset(enckey, 0, sizeof(enckey));
	ret = awl13_get_wepkey(priv, index, enckey, sizeof(enckey));
	if (ret)
		return ret;

	dwrq->length = encsize / 2;
	dwrq->flags = (index + 1) | IW_ENCODE_ENABLED;
	for (i=0; i<dwrq->length; i++) {
		extra[i] = (char)CHARCODE_TO_NUMBIN(enckey[i*2+1], ret);
		extra[i] |= (char)(CHARCODE_TO_NUMBIN(enckey[i*2], ret) << 4);
	}

	return 0;
}

static int
awl13_iw_siwpower(struct net_device *dev, struct iw_request_info *info,
		   struct iw_param *wrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	unsigned char mode;

	if (wrq->disabled) {
		mode = AWL13_PMMODE_NONE;
	} else {
		switch (wrq->flags & IW_POWER_MODE) {
		case IW_POWER_ON:
			mode = AWL13_PMMODE_FMIN;
			break;
		default:
			return -EINVAL;
		}
	}

	return awl13_set_powerman(priv, mode);
}

static int
awl13_iw_giwpower(struct net_device *dev, struct iw_request_info *info,
		   struct iw_param *rrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	unsigned char val;
	int ret;

	ret = awl13_get_powerman(priv, &val);
	if (ret)
		return ret;

	if (val == AWL13_PMMODE_NONE)
		rrq->disabled = 1;
	else
		rrq->disabled = 0;

	return 0;
}

static int
awl13_iw_siwgenie(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *data, char *extra)
{
	return 0;
}

static int
awl13_iw_siwauth(struct net_device *dev, struct iw_request_info *info,
		  struct iw_param *dwrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char crypt = priv->prev_crypt, old;
	int ret;

	switch (dwrq->flags & IW_AUTH_INDEX) {
    	case IW_AUTH_WPA_VERSION:
		if (dwrq->value & IW_AUTH_WPA_VERSION_DISABLED)
			crypt &= ~(AWL13_CRYPT_WPA | AWL13_CRYPT_WPA2);
		else {
			if (dwrq->value & IW_AUTH_WPA_VERSION_WPA)
				crypt |= AWL13_CRYPT_WPA;
			else
				crypt &= ~AWL13_CRYPT_WPA;
			if (dwrq->value & IW_AUTH_WPA_VERSION_WPA2)
				crypt |= AWL13_CRYPT_WPA2;
			else
				crypt &= ~AWL13_CRYPT_WPA2;
		}
		break;
    	case IW_AUTH_CIPHER_PAIRWISE:
		switch (dwrq->value) {
		case IW_AUTH_CIPHER_NONE:
			crypt = AWL13_CRYPT_DISABLE;
			break;
		case IW_AUTH_CIPHER_WEP40:
			crypt = AWL13_CRYPT_WEP64;
			break;
		case IW_AUTH_CIPHER_WEP104:
			crypt = AWL13_CRYPT_WEP128;
			break;
		case IW_AUTH_CIPHER_TKIP:
			crypt = AWL13_CRYPT_ENABLE |
			  (crypt & ~AWL13_CRYPT_WEP_MASK) | AWL13_CRYPT_TKIP;
			break;
		case IW_AUTH_CIPHER_CCMP:
			crypt = AWL13_CRYPT_ENABLE |
			  (crypt & ~AWL13_CRYPT_WEP_MASK) | AWL13_CRYPT_CCMP;
			break;
		default:
			return 0;
		}
		break;
	case IW_AUTH_CIPHER_GROUP:
	case IW_AUTH_KEY_MGMT:
	case IW_AUTH_TKIP_COUNTERMEASURES:
	case IW_AUTH_DROP_UNENCRYPTED:
	case IW_AUTH_80211_AUTH_ALG:
	case IW_AUTH_WPA_ENABLED:
	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
	case IW_AUTH_ROAMING_CONTROL:
	case IW_AUTH_PRIVACY_INVOKED:
		return 0;
	default:
		return -EINVAL;
	}

	priv->prev_crypt = crypt;
	if (awl13_checkcrypt(crypt)) {
		ret = awl13_get_cryptmode(priv, &old);
		if (ret)
			return ret;
		if (crypt != old)
			return awl13_set_cryptmode(priv, crypt);
	}

	return 0;
}

static int
awl13_iw_siwencodeext(struct net_device *dev, struct iw_request_info *info,
		       struct iw_point *dwrq, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	unsigned char buf[max((int)sizeof(priv->prev_enckey), 64)];
	int index, ret, i;

	index = dwrq->flags & IW_ENCODE_INDEX;
	if ((index < 0) || (AWL13_KEYINDEX_MAX < index))
		return -EINVAL;
	if (index != 0)
		index--;

	if (dwrq->flags & IW_ENCODE_DISABLED) {
		memset(buf, 0, sizeof(buf));
		memset(buf, '0', AWL13_WEPKEY_MAX);
		memcpy(priv->prev_enckey, buf, sizeof(priv->prev_enckey));
		return awl13_set_wepkey(priv, index, buf, AWL13_WEPKEY_MAX);
	}

	if (!extra)
		return -EINVAL;

	switch (ext->alg) {
	case IW_ENCODE_ALG_NONE:
		break;
	case IW_ENCODE_ALG_WEP:
		if (ext->key_len != (AWL13_WEPKEY_MIN / 2) &&
		    ext->key_len != (AWL13_WEPKEY_MAX / 2))
			return -EINVAL;
		memset(buf, 0, sizeof(buf));
		for (i=0; i<ext->key_len; i++) {
			buf[i*2  ] = NUMBIN_TO_CHARCODE((ext->key[i] >> 4) & 0x0f);
			buf[i*2+1] = NUMBIN_TO_CHARCODE(ext->key[i] & 0x0f);
		}
		memcpy(priv->prev_enckey, buf, sizeof(priv->prev_enckey));
		ret = awl13_set_wepkey(priv, index, buf, strlen(buf));
		if (ret)
			return ret;
		break;
	case IW_ENCODE_ALG_TKIP:
	case IW_ENCODE_ALG_CCMP:
		if (ext->key_len != 32)
			return -EINVAL;
		for (i=0; i<32; i++) {
			buf[i*2  ] = NUMBIN_TO_CHARCODE((ext->key[i] >> 4) & 0x0f);
			buf[i*2+1] = NUMBIN_TO_CHARCODE(ext->key[i] & 0x0f);
		}
		ret = awl13_set_passps(priv, buf, 64);
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
awl13_ip_fwload(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	return awl13_firmware_setup(priv);
}

static int
awl13_ip_fwsetup(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	return awl13_firmware_setup_without_load(priv);
}

static int
awl13_ip_getmac(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *data, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	unsigned char mac[6+1];
	int ret;

	ret = awl13_get_macaddr(priv, mac, 6);
	if (ret)
		return ret;

	sprintf(extra, "%02x%02x%02x%02x%02x%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	data->length = strlen(extra);

	memcpy(priv->netdev->dev_addr, mac, 6);

	return 0;
}

struct awl13_crypt_mode {
	char *label;
	unsigned char val;
};
static const struct awl13_crypt_mode crypts[] = {
	{ "NONE", AWL13_CRYPT_DISABLE, },
	{ "WEP64", AWL13_CRYPT_WEP64, },
	{ "WEP128", AWL13_CRYPT_WEP128, },
	{ "WPA-AES", AWL13_CRYPT_WPA_CCMP },
	{ "WPA-CCMP", AWL13_CRYPT_WPA_CCMP },
	{ "WPA-TKIP", AWL13_CRYPT_WPA_TKIP, },
	{ "WPA2-AES", AWL13_CRYPT_WPA2_CCMP, },
	{ "WPA2-CCMP", AWL13_CRYPT_WPA2_CCMP, },
	{ "WPA2-TKIP", AWL13_CRYPT_WPA2_TKIP, },

	{ "WPA-MIX", AWL13_CRYPT_WPA_MIX, },
	{ "WPA2-MIX", AWL13_CRYPT_WPA2_MIX, },
	{ "WPA/2-AES", AWL13_CRYPT_WPA_2_CCMP, },
	{ "WPA/2-CCMP", AWL13_CRYPT_WPA_2_CCMP, },
	{ "WPA/2-TKIP", AWL13_CRYPT_WPA_2_TKIP, },
	{ "WPA/2-MIX", AWL13_CRYPT_WPA_2_MIX, },
};

static int
awl13_checkcrypt(unsigned char crypt)
{
	int i;

	for (i=0; i<ARRAY_SIZE(crypts); i++)
		if (crypt == crypts[i].val)
			return 1;

	return 0;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
static int awl13_strcasecmp(const char *str1, const char *str2)
{
	int chr1=0, chr2=0;
	
	while (chr1 != 0) {
		chr1 = tolower(*str1++);
		chr2 = tolower(*str2++);
		if (chr1 != chr2)
			break;
	}
	return chr1 - chr2;
}
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */

static int
awl13_ip_setcrypt(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	int i;

	for (i=0; i<ARRAY_SIZE(crypts); i++)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
		if (strcasecmp(crypts[i].label, extra) == 0)
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
		if (awl13_strcasecmp(crypts[i].label, extra) == 0)
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
			return awl13_set_cryptmode(priv, crypts[i].val);

	return -EINVAL;
}

static int
awl13_ip_getcrypt(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char val;
	int ret, i;

	ret = awl13_get_cryptmode(priv, &val);
	if (ret)
		return ret;

	for (i=0; i<ARRAY_SIZE(crypts); i++) {
		if (crypts[i].val == val) {
			sprintf(extra, "%s\n", crypts[i].label);
			wri->length = strlen(extra);
			return 0;
		}
	}

	sprintf(extra, "unknown(%02x)\n", val);
	wri->length = strlen(extra);
	return 0;
}

static int
awl13_ip_setpsk(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	return awl13_set_passps(priv, extra, wri->length - 1);
}

static int
awl13_ip_getpsk(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);

	unsigned char pskpass[AWL13_PSKPASS_MAX + 1];
	int ret;

	ret = awl13_get_passps(priv, pskpass, sizeof(pskpass));
	if (ret)
		return ret;

	strcpy(extra, pskpass);
	wri->length = strlen(extra);

	return 0;
}

static int
awl13_ip_setwid8(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	int 			*param = (int *)extra;
	unsigned short 		wid    = param[0]&0x0000FFFF;
	unsigned char 		val    = param[1]&0x000000FF;
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	int			ret;
	
	if(wid==WID_11I_MODE){
		timeout = WID_GENKEY_TIMEOUT;
	}

	ret = awl13_set_common_value(priv, wid, &val, 1, timeout);
	if (ret){
		awl_err("The setting(0x%x) of WID(0x%x) faild. \n", val, wid );
		return -EINVAL;
	}
	awl_info("The setting(0x%x) of WID(0x%x) succeeded. \n", val, wid );
	return 0;
}

static int
awl13_ip_getwid8(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	int 			*param = (int *)extra;
	unsigned short 		wid    = param[0]&0x0000FFFF;
	unsigned char 		val    = 0;
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	int			ret;

	ret = awl13_get_common_value(priv, wid, &val, 1, timeout );
	if (ret<0){
		awl_err("The getting(0x%x) of WID(0x%x) faild. \n", val, wid );
		return -EINVAL;
	}
	awl_info("The getting(0x%x) of WID(0x%x) succeeded. \n", val, wid );
	param[0] = (int)val;
	return 0;
}


static int
awl13_ip_setwid16(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	int 			*param = (int *)extra;
	unsigned short 		wid    = param[0]&0x0000FFFF;
	unsigned short		val    = param[1]&0x0000FFFF;
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	int			ret;

	ret = awl13_set_common_value(priv, wid, &val, 2, timeout);
	if (ret){
		awl_err("The setting(0x%x) of WID(0x%x) faild. \n", val, wid );
		return -EINVAL;
	}
	awl_info("The setting(0x%x) of WID(0x%x) succeeded. \n", val, wid );
	return 0;
}

static int
awl13_ip_getwid16(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	int 			*param = (int *)extra;
	unsigned short 		wid    = param[0]&0x0000FFFF;
	unsigned short 		val    = 0;
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	int			ret;

	ret = awl13_get_common_value(priv, wid, &val, 2, timeout );
	if (ret<0){
		awl_err("The getting(0x%x) of WID(0x%x) faild. \n", val, wid );
		return -EINVAL;
	}
	awl_info("The getting(0x%x) of WID(0x%x) succeeded. \n", val, wid );
	param[0] = (int)val;
	return 0;
}


static int
awl13_ip_setwid32(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char 		*param = (unsigned char *)extra;
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	unsigned char*		p1=0;
	unsigned char*		p2=0;
	unsigned short 		wid;
	unsigned int 		val;
	int					ret;
	int					loop,flag;

    param[wri->length]=0x00;
	for( flag=0, loop=0 ; 
	     loop<wri->length && flag<3; 
	     loop++, param++  ){
		switch(flag){
		case 0:
			if(*param!=' ' && *param){
				p1 = param;
				flag++;
			}
			break;
		case 1:
			if(*param==' ' && *param){
				*param = 0x00;
				flag++;
			}
			break;
		case 2:
			if(*param!=' ' && *param){
				p2 = param;
				flag++;
			}
			break;
		}
	}
	if( !p1 || !p2 || flag!=3  ){
		awl_err("Please tie up wid and value by a double quotation. \n" );
		return -EINVAL;
	}

	wid = simple_strtol(p1, (char **)NULL, 16);
	val = simple_strtol(p2, (char **)NULL, 16);

	ret = awl13_set_common_value(priv, wid, &val, 4, timeout);
	if (ret){
		awl_err("The setting(0x%x) of WID(0x%x) faild. \n", val, wid );
		return -EINVAL;
	}
	awl_info("The setting(0x%x) of WID(0x%x) succeeded. \n", val, wid );
	return 0;
}

static int
awl13_ip_getwid32(struct net_device *dev, struct iw_request_info *info,
		   struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	int 			*param = (int *)extra;
	unsigned short 		wid    = param[0]&0x0000FFFF;
	unsigned int 		val    = 0;
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	int			ret;

	ret = awl13_get_common_value(priv, wid, &val, 4, timeout );
	if (ret<0){
		awl_err("The getting(0x%x) of WID(0x%x) faild. \n", val, wid );
		return -EINVAL;
	}
	awl_info("The getting(0x%x) of WID(0x%x) succeeded. \n", val, wid );
	param[0] = (int)val;
	return 0;
}

static int
awl13_ip_setwidstr(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char 		*param = (unsigned char *)extra;
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	unsigned char*		p1=0;
	unsigned char*		p2=0;
	unsigned short 		wid;
	int			size;
	int			ret;
	int			loop,flag;
	unsigned char		action_req[1024];
	
        param[wri->length]=0x00;
	for( flag=0, loop=0 ; 
	     loop<wri->length && flag<3; 
	     loop++, param++  ){
		switch(flag){
		case 0:
			if(*param!=' ' && *param){
				p1 = param;
				flag++;
			}
			break;
		case 1:
			if(*param==' ' && *param){
				*param = 0x00;
				flag++;
			}
			break;
		case 2:
			if(*param!=' ' && *param){
				p2 = param;
				flag++;
			}
			break;
		}
	}
	if( !p1 || !p2 || flag!=3  ){
		awl_err("Please tie up wid and value by a double quotation. \n" );
		return -EINVAL;
	}

	wid = simple_strtol(p1, (char **)NULL, 16);
	size= strlen(p2);
	
	if(wid==WID_11N_P_ACTION_REQ || wid==WID_11E_P_ACTION_REQ || 
       ((wid & 0xF000)== 0x4000) ){
	    unsigned char sum = 0;
		char  temp[2] = {0,0};
		memset( action_req, 0x00, sizeof(action_req) );
		size = size/2;
		for(loop=0; loop<size; loop++){
			temp[0] = *(p2+(loop*2));
			temp[1] = *(p2+1+(loop*2));
			action_req[loop] = simple_strtol(temp, (char **)NULL, 16);
		}
		
		if( (wid & 0xF000)== 0x4000 ){
		    for( loop=0; loop<size; loop++ ){
		        sum += action_req[loop];
		    }
			action_req[size] = sum;
		}
		p2 = action_req;
		awl_devdump( p2, size );
	}
	else {
		if( wid==WID_SSID || wid==WID_11I_PSK ){
			timeout = WID_GENKEY_TIMEOUT;
		}
	}

	if( (!size) && (size>256) ){
		awl_err("The size is up to 1-256 bytes. (%d)\n", size );
		return -EINVAL;
	}
	ret = awl13_set_common_value(priv, wid, p2, size, timeout);
	if (ret){
		awl_err("The setting of WID(0x%x) faild. \n", wid  );
		return -EINVAL;
	}
	awl_info("The setting of WID(0x%x) succeeded. \n", wid );
	return 0;
}

static int
awl13_ip_getwidstr(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char		param[16];
	unsigned int 		timeout= WID_DEFAULT_TIMEOUT;
	int			size;
	unsigned short 		wid;
	unsigned char 		val[1024];
	int			ret;

	memset(val, 0x00, sizeof(val));
	copy_from_user(param, wri->pointer, wri->length);
        param[wri->length]=0x00;
	wid = simple_strtol( param, (char **)NULL, 16);
	size= sizeof(val);
	ret = awl13_get_common_value(priv, wid, &val, size, timeout );
	if (ret<0){
		awl_err("The getting of WID(0x%x) faild. \n", wid );
		return -EINVAL;
	}
	size = strlen(val);
	if( (wid == WID_BSSID) || (wid == WID_MAC_ADDR) ) size = 6;
    if( (wid & 0xF000) == 0x4000 ) size = ret;
	awl_info("The getting of WID(0x%x) succeeded. (size=%d)\n", wid, size );
    awl13_dump(LOGLVL_1, &val, size );
//	strcpy(extra, val);
//	wri->length = size;
	wri->length = 0;
	return 0;
}

#define AWL13_WPS_PIN_LEN 8
#define AWL13_STRANGE_SIGN		(unsigned char)'?'

static int
awl13_ip_setwpspin(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *data, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	char str[AWL13_WPS_PIN_LEN];
	unsigned char pin[AWL13_WPS_PIN_LEN];
	int i;
	
	/* check length */
	if (data->length < AWL13_WPS_PIN_LEN)
		return -EINVAL;

	copy_from_user(str, data->pointer, data->length);
	for (i = 0; i < AWL13_WPS_PIN_LEN; i++) {
		/* check range */
		if (str[i] < '0' || str[i] > '9')
			return -EINVAL;
		pin[i] = str[i];
	}

	return  awl13_set_wps_pin(priv, pin, AWL13_WPS_PIN_LEN);
}

static int
awl13_ip_getwpspin(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *data, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	unsigned char pin[8];
	int ret, i;
	
	/* get pincode from device */
	ret = awl13_get_wps_pin(priv, pin, AWL13_WPS_PIN_LEN);
	if (ret) {
		awl_err("awl13_get_wps_pin() failed on %d\n", ret);
		return ret; /* failure */
	}

	for (i = 0; i < AWL13_WPS_PIN_LEN; i++) {
		/* check range */
		if (pin[i] <= '9' && pin[i] >= '0')
			extra[i] = pin[i];
		else {
			extra[i] = AWL13_STRANGE_SIGN;
		}
	}

	/* set length of extra */
	data->length = AWL13_WPS_PIN_LEN;
	
	return  0; /* success */
}

#define AWL13_WID_SET_FUNC(_type, _name)				\
do {									\
	{								\
		_type val;						\
									\
		val = param[1];						\
									\
		/* set value to module */				\
		ret = awl13_set_##_name(priv, val);			\
		awl_debug("l.%d: val=%u, ret=%d\n", __LINE__, val, ret);\
		if (ret)						\
			return ret;					\
	}								\
} while(0)


static int
awl13_ioctl_setparam_int(struct net_device *dev, struct iw_request_info *info,
		  void *w, char *extra)
{

	struct awl13_private *priv = devToPriv(dev);
	int *param = (int *)extra;
	int ret;

	switch (param[0])
	{
	case AWL13_PARAMS_INT_WID_AUTH_TYPE:
		AWL13_WID_SET_FUNC(unsigned char, authtype);
		break;
	case AWL13_PARAMS_INT_WID_SITE_SURVEY:
     	awl_info("It doesn't return to Disable(2) after it scans when bit4 is turned on.\n");
        scan_ch_mode = param[1];
		break;
	case AWL13_PARAMS_INT_WID_JOIN_REQ:
		AWL13_WID_SET_FUNC(unsigned char, join);
		break;
	case AWL13_PARAMS_INT_WID_11I_MODE:
		AWL13_WID_SET_FUNC(unsigned char, cryptmode);
		break;
	case AWL13_PARAMS_INT_WID_POWER_MANAGEMENT:
		AWL13_WID_SET_FUNC(unsigned char, powerman);
		break;
	case AWL13_PARAMS_INT_WID_SCAN_TYPE:
		AWL13_WID_SET_FUNC(unsigned char, scan_type);
		break;
	case AWL13_PARAMS_INT_WID_POWER_SAVE:
		AWL13_WID_SET_FUNC(unsigned char, psctl);
		break;
	case AWL13_PARAMS_INT_WID_BEACON_INTERVAL:
		AWL13_WID_SET_FUNC(unsigned short, beaconint);
		break;
	case AWL13_PARAMS_INT_WID_LISTEN_INTERVAL:
		AWL13_WID_SET_FUNC(unsigned char, lisnintvl);
		break;
	case AWL13_PARAMS_INT_WID_BCAST_SSID:
		AWL13_WID_SET_FUNC(unsigned char, bcastssid);
		break;
	case AWL13_PARAMS_INT_WID_DTIM_PERIOD:
		AWL13_WID_SET_FUNC(unsigned char, dtim_period);
		break;
	case AWL13_PARAMS_INT_WID_REKEY_POLICY:
		AWL13_WID_SET_FUNC(unsigned char, rekey_policy);
		break;
	case AWL13_PARAMS_INT_WID_REKEY_PERIOD:
		AWL13_WID_SET_FUNC(unsigned int, rekey_period);
		break;
	case AWL13_PARAMS_INT_WID_SCAN_FILTER:
		scan_filter = param[1];
		break;
	case AWL13_PARAMS_INT_WPSDMD:
		AWL13_WID_SET_FUNC(unsigned char, wps_dev_mode);
		break;
	case AWL13_PARAMS_INT_WPSST:
		AWL13_WID_SET_FUNC(unsigned char, wps_start);
		break;
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
	case AWL13_PARAMS_INT_WID_USB_IN_XFER_MODE:
		AWL13_WID_SET_FUNC(unsigned char, usb_in_xfer_mode);
		break;
	case AWL13_PARAMS_INT_WID_USB_RMTWKUP_TIME:
		AWL13_WID_SET_FUNC(unsigned char, usb_rmtwkup_time);
		break;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}


#define AWL13_WID_GET_FUNC(_type, _name)				\
do {									\
		{							\
		_type val;						\
		/* get value to module */				\
		ret = awl13_get_##_name(priv, &val);			\
		awl_debug("l.%d: val=%u, ret=%d\n", __LINE__, val, ret);\
		if (ret)						\
			return ret;					\
									\
		/* teruen value */					\
		param[0] = val;						\
		return 0;						\
		}							\
} while(0)

static int
awl13_ioctl_getparam_int(struct net_device *dev, struct iw_request_info *info,
			  void *w, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	int *param = (int *)extra; 
	int ret;
	
	switch (param[0])
	{
	case AWL13_PARAMS_INT_WID_AUTH_TYPE:
		AWL13_WID_GET_FUNC(unsigned char, authtype);
		break;
	case AWL13_PARAMS_INT_WID_SITE_SURVEY:
		param[0] = scan_ch_mode;
		break;
	case AWL13_PARAMS_INT_WID_11I_MODE:
		AWL13_WID_GET_FUNC(unsigned char, cryptmode);
		break;
	case AWL13_PARAMS_INT_WID_POWER_MANAGEMENT:
		AWL13_WID_GET_FUNC(unsigned char, powerman);
		break;
	case AWL13_PARAMS_INT_WID_SCAN_TYPE:
		AWL13_WID_GET_FUNC(unsigned char, scan_type);
		break;
	case AWL13_PARAMS_INT_WID_POWER_SAVE:
        param[0] = (int)atomic_read(&priv->power_state);
		break;
	case AWL13_PARAMS_INT_WID_BEACON_INTERVAL:
		AWL13_WID_GET_FUNC(unsigned short, beaconint);
		break;
	case AWL13_PARAMS_INT_WID_LISTEN_INTERVAL:
		AWL13_WID_GET_FUNC(unsigned char, lisnintvl);
		break;
	case AWL13_PARAMS_INT_WID_BCAST_SSID:
		AWL13_WID_GET_FUNC(unsigned char, bcastssid);
		break;
	case AWL13_PARAMS_INT_WID_DTIM_PERIOD:
		AWL13_WID_GET_FUNC(unsigned char, dtim_period);
		break;
	case AWL13_PARAMS_INT_WID_REKEY_POLICY:
		AWL13_WID_GET_FUNC(unsigned char, rekey_policy);
		break;
	case AWL13_PARAMS_INT_WID_REKEY_PERIOD:
		AWL13_WID_GET_FUNC(unsigned int, rekey_period);
		break;
	case AWL13_PARAMS_INT_WID_SCAN_FILTER:
		param[0] = scan_filter;
		break;
	case AWL13_PARAMS_INT_WPSDMD:
		AWL13_WID_GET_FUNC(unsigned char, wps_dev_mode);
		break;
	case AWL13_PARAMS_INT_WPSST:
		AWL13_WID_GET_FUNC(unsigned char, wps_start);
		break;
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
	case AWL13_PARAMS_INT_WID_USB_IN_XFER_MODE:
		AWL13_WID_GET_FUNC(unsigned char, usb_in_xfer_mode);
		break;
	case AWL13_PARAMS_INT_WID_USB_RMTWKUP_TIME:
		AWL13_WID_GET_FUNC(unsigned char, usb_rmtwkup_time);
		break;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int
awl13_ioctl_setparam_str(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
		return -EOPNOTSUPP;
}

#define AWL13_WID_GET_FUNC_STR(_name)					\
do {									\
	{								\
		memset(str_val, 0x00, sizeof(str_val));			\
									\
		/* get data from module */				\
		ret = awl13_get_##_name(priv,				\
					 str_val, AWL13_VERSION_SIZE);	\
		if (ret)						\
			return ret;					\
		awl_dump(str_val, sizeof(str_val));			\
									\
		/* return data */					\
		snprintf(extra, sizeof(str_val), "%s" , str_val);	\
		data->length = strlen(extra);				\
	}								\
} while(0)

static int
awl13_ioctl_getparam_str(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	struct iw_point *data = (struct iw_point *)w;
	int ret;
	char str_val[AWL13_VERSION_SIZE + 1];
	

	switch (data->flags)
	{
	case AWL13_PARAMS_STR_WID_FIRMWARE_VERSION:
		AWL13_WID_GET_FUNC_STR(firmware_version);
		break;
	case AWL13_PARAMS_STR_DRIVER_VERSION:
		memset(str_val, 0x00, sizeof(str_val));
		/* return data */
		snprintf(extra, AWL13_VERSION_SIZE, "%s" , AWL13_VERSION);
		data->length = strlen(extra);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}


static int
awl13_ip_setlog(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	int *param = (int *)extra;
	int val = param[0];

	awl13_log_flag = val;
	return 0;
}

static int
awl13_ip_getlog(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *wri, char *extra)
{
	int *param = (int *)extra;

	param[0] = awl13_log_flag;

	return 0;
}

static struct iw_statistics *
awl13_iw_getstats(struct net_device *dev)
{
	struct awl13_private	*priv = devToPriv(dev);
	signed char		rssi;
	int			ret = -1;

#ifdef in_atomic
	if (!in_atomic())
#endif
		ret = awl13_get_rssi(priv, &rssi);
	if (!ret && rssi > -100) {
		priv->iwstats.qual.level = 0x100 + rssi;
		priv->iwstats.qual.updated = IW_QUAL_DBM |
			IW_QUAL_LEVEL_UPDATED |
			IW_QUAL_QUAL_INVALID | IW_QUAL_NOISE_INVALID;
	}
	else
		priv->iwstats.qual.updated = IW_QUAL_ALL_INVALID;

	return &priv->iwstats;
}


/******************************************************************************
 * awl13_ip_setwps_cred_list -
 *
 *****************************************************************************/
static int
awl13_ip_setwps_cred_list(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *data, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	char buf[8];

	copy_from_user(buf, data->pointer, data->length);
	if(strcmp(buf, "permit")==0){

		if(awl13_set_wps_cred_list(priv) != 0){
			awl_err("failed set WPS Credentials\n");
			return -EINVAL;
		}
		awl_info("Set WPS Credentials success\n");
		return 0;
	}
	awl_info("Please input to make sure as \"permit\".\n");
	return -EINVAL;
}

/******************************************************************************
 * awl13_ip_getwps_cred_list -
 *
 *****************************************************************************/

static int
awl13_ip_getwps_cred_list(struct net_device *dev, struct iw_request_info *info,
		 struct iw_point *data, char *extra)
{
	struct awl13_private *priv = devToPriv(dev);
	char buf[8];

	copy_from_user(buf, data->pointer, data->length);
	data->length = 0;
	if(strcmp(buf, "permit")==0){

		if(awl13_get_wps_cred_list(priv) < 0){
			awl_err("failed get WPS Credentials\n");
			return -EINVAL;
		}
		awl_info("Get WPS Credentials success\n");
		return 0;
	}
	awl_info("Please input to make sure as \"permit\".\n");
	return -EINVAL;
}

#define IW_IOCTL(x, func) [(x) - SIOCSIWCOMMIT] = (iw_handler)func
static const iw_handler awl13_iw_handlers[] = {
	/* Wireless Identification */
	IW_IOCTL(SIOCSIWCOMMIT,		awl13_iw_not_support),
	IW_IOCTL(SIOCGIWNAME,		awl13_iw_giwname),
	/* Basic operations */
	IW_IOCTL(SIOCSIWNWID,		NULL),
	IW_IOCTL(SIOCGIWNWID,		awl13_iw_not_support),
	IW_IOCTL(SIOCSIWFREQ,		awl13_iw_siwfreq),
	IW_IOCTL(SIOCGIWFREQ,		awl13_iw_giwfreq),
	IW_IOCTL(SIOCSIWMODE,		awl13_iw_siwmode),
	IW_IOCTL(SIOCGIWMODE,		awl13_iw_giwmode),
	IW_IOCTL(SIOCSIWSENS,		NULL),
	IW_IOCTL(SIOCGIWSENS,		NULL),
	/* Informative stuff */
	IW_IOCTL(SIOCSIWRANGE,		NULL),
	IW_IOCTL(SIOCGIWRANGE,		awl13_iw_giwrange),
	IW_IOCTL(SIOCSIWPRIV,		NULL),
	IW_IOCTL(SIOCGIWPRIV,		NULL),
	IW_IOCTL(SIOCSIWSTATS,		NULL),
	IW_IOCTL(SIOCGIWSTATS,		NULL),
	/* Spy support */
	IW_IOCTL(SIOCSIWSPY,		NULL),
	IW_IOCTL(SIOCGIWSPY,		NULL),
	IW_IOCTL(SIOCSIWTHRSPY,		NULL),
	IW_IOCTL(SIOCGIWTHRSPY,		NULL),
	/* Access Point manipulation */
	IW_IOCTL(SIOCSIWAP,		awl13_iw_siwap),
	IW_IOCTL(SIOCGIWAP,		awl13_iw_giwap),
	IW_IOCTL(SIOCGIWAPLIST,		NULL),
	IW_IOCTL(SIOCSIWSCAN,		awl13_iw_siwscan),
	IW_IOCTL(SIOCGIWSCAN,		awl13_iw_giwscan),
	/* 802.11 specific support */
	IW_IOCTL(SIOCSIWESSID,		awl13_iw_siwessid),
	IW_IOCTL(SIOCGIWESSID,		awl13_iw_giwessid),
	IW_IOCTL(SIOCSIWNICKN,		NULL),
	IW_IOCTL(SIOCGIWNICKN,		NULL),
	/* Other parameters useful in 802.11 and some other devices */
	IW_IOCTL(SIOCSIWRATE,		awl13_iw_siwrate),
	IW_IOCTL(SIOCGIWRATE,		awl13_iw_giwrate),
	IW_IOCTL(SIOCSIWRTS,		NULL),
	IW_IOCTL(SIOCGIWRTS,		NULL),
	IW_IOCTL(SIOCSIWFRAG,		NULL),
	IW_IOCTL(SIOCGIWFRAG,		NULL),
	IW_IOCTL(SIOCSIWTXPOW,		awl13_iw_siwtxpow),
	IW_IOCTL(SIOCGIWTXPOW,		awl13_iw_giwtxpow),
	IW_IOCTL(SIOCSIWRETRY,		NULL),
	IW_IOCTL(SIOCGIWRETRY,		NULL),
	/* Encoding stuff (scrambling, hardware security, WEP...) */
	IW_IOCTL(SIOCSIWENCODE,		awl13_iw_siwenc),
	IW_IOCTL(SIOCGIWENCODE,		awl13_iw_giwenc),
	/* Power saving stuff (power management, unicast and multicast) */
	IW_IOCTL(SIOCSIWPOWER,		awl13_iw_siwpower),
	IW_IOCTL(SIOCGIWPOWER,		awl13_iw_giwpower),
	/* WPA : Generic IEEE 802.11 informatiom element */
	IW_IOCTL(SIOCSIWGENIE,		awl13_iw_siwgenie),
	IW_IOCTL(SIOCGIWGENIE,		NULL),
	/* WPA : IEEE 802.11 MLME requests */
	IW_IOCTL(SIOCSIWMLME,		NULL),
	/* WPA : Authentication mode parameters */
	IW_IOCTL(SIOCSIWAUTH,		awl13_iw_siwauth),
	IW_IOCTL(SIOCGIWAUTH,		NULL),
	/* WPA : Extended version of encoding configuration */
	IW_IOCTL(SIOCSIWENCODEEXT,	awl13_iw_siwencodeext),
	IW_IOCTL(SIOCGIWENCODEEXT,	NULL),
	/* WPA2 : PMKSA cache management */
	IW_IOCTL(SIOCSIWPMKSA,		NULL),
};

#define IW_PRIV(x, func) [(x) - SIOCIWFIRSTPRIV] = (iw_handler)func
static const iw_handler awl13_iw_priv_handlers[] = {
	IW_PRIV(AWL13_PRIV_FWLOAD,		awl13_ip_fwload),
	IW_PRIV(AWL13_PRIV_FWSETUP,	awl13_ip_fwsetup),
	IW_PRIV(AWL13_PRIV_GETMAC,		awl13_ip_getmac),
	IW_PRIV(AWL13_PRIV_SETCRYPT,	awl13_ip_setcrypt),
	IW_PRIV(AWL13_PRIV_GETCRYPT,	awl13_ip_getcrypt),
	IW_PRIV(AWL13_PRIV_SETPSK,	awl13_ip_setpsk),
	IW_PRIV(AWL13_PRIV_GETPSK,	awl13_ip_getpsk),

	IW_PRIV(AWL13_PRIV_SETWID8,	awl13_ip_setwid8),
	IW_PRIV(AWL13_PRIV_GETWID8,	awl13_ip_getwid8),
	IW_PRIV(AWL13_PRIV_SETWID16,	awl13_ip_setwid16),
	IW_PRIV(AWL13_PRIV_GETWID16,	awl13_ip_getwid16),
	IW_PRIV(AWL13_PRIV_SETWID32,	awl13_ip_setwid32),
	IW_PRIV(AWL13_PRIV_GETWID32,	awl13_ip_getwid32),
	IW_PRIV(AWL13_PRIV_SETWIDSTR,	awl13_ip_setwidstr),
	IW_PRIV(AWL13_PRIV_GETWIDSTR,	awl13_ip_getwidstr),

	IW_PRIV(AWL13_PRIV_SETWPSPIN,	awl13_ip_setwpspin),
	IW_PRIV(AWL13_PRIV_GETWPSPIN,	awl13_ip_getwpspin),
	IW_PRIV(AWL13_IOCTL_SETPARAM_INT,	awl13_ioctl_setparam_int),
	IW_PRIV(AWL13_IOCTL_GETPARAM_INT,	awl13_ioctl_getparam_int),
	IW_PRIV(AWL13_IOCTL_SETPARAM_STR,	awl13_ioctl_setparam_str),
	IW_PRIV(AWL13_IOCTL_GETPARAM_STR,	awl13_ioctl_getparam_str),
	IW_PRIV(AWL13_PRIV_SETWPSCREDLIST,	awl13_ip_setwps_cred_list),
	IW_PRIV(AWL13_PRIV_GETWPSCREDLIST,	awl13_ip_getwps_cred_list),
	IW_PRIV(AWL13_PRIV_SETLOG,	awl13_ip_setlog),
	IW_PRIV(AWL13_PRIV_GETLOG,	awl13_ip_getlog),
};

#define IW_PRIV_ARGS(_cmd, _name, _set, _get) \
{                                             \
	.cmd = _cmd,                          \
	.name = _name,                        \
	.set_args = _set,                     \
	.get_args = _get,                     \
}
#define IW_ARG_CHAR_T(x) (IW_PRIV_TYPE_CHAR | (x))
#define IW_ARG_INT_FIXED_T(x) (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | (x))
#define IW_ARG_CHAR_FIXED_T(x) (IW_PRIV_TYPE_CHAR  | IW_PRIV_SIZE_FIXED | (x))
#define IW_ARG_BYTE_T(x) (IW_PRIV_TYPE_BYTE | (x))
static const struct iw_priv_args awl13_iw_priv_args[] = {
	IW_PRIV_ARGS(AWL13_PRIV_FWLOAD, "fwload",
		     0,
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_FWSETUP, "fwsetup",
		     0,
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETMAC, "get_macaddr",
		     0,
		     IW_ARG_CHAR_T(12)),
	IW_PRIV_ARGS(AWL13_PRIV_SETCRYPT, "set_cryptmode",
		     IW_ARG_CHAR_T(32),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETCRYPT, "get_cryptmode",
		     0,
		     IW_ARG_CHAR_T(32)),
	IW_PRIV_ARGS(AWL13_PRIV_SETPSK, "set_psk",
		     IW_ARG_CHAR_T(128),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETPSK, "get_psk",
		     0,
		     IW_ARG_CHAR_T(128)),

	IW_PRIV_ARGS(AWL13_PRIV_SETWID8, "set_wid_8",
		     IW_ARG_INT_FIXED_T(2),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETWID8, "get_wid_8",
		     IW_ARG_INT_FIXED_T(1),
		     IW_ARG_INT_FIXED_T(1)),

	IW_PRIV_ARGS(AWL13_PRIV_SETWID16, "set_wid_16",
		     IW_ARG_INT_FIXED_T(2),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETWID16, "get_wid_16",
		     IW_ARG_INT_FIXED_T(1),
		     IW_ARG_INT_FIXED_T(1)),

	IW_PRIV_ARGS(AWL13_PRIV_SETWID32, "set_wid_32",
		     IW_ARG_CHAR_T(32),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETWID32, "get_wid_32",
		     IW_ARG_INT_FIXED_T(1),
		     IW_ARG_INT_FIXED_T(1)),

	IW_PRIV_ARGS(AWL13_PRIV_SETWIDSTR, "set_wid_str",
		     IW_ARG_CHAR_T(2000+16),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETWIDSTR, "get_wid_str",
		     IW_ARG_CHAR_T(16),
		     IW_ARG_CHAR_T(2000)),

	IW_PRIV_ARGS(AWL13_PRIV_SETWPSPIN, "set_wpspin",
		     IW_ARG_CHAR_T(8),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETWPSPIN, "get_wpspin",
		     0,
		     IW_ARG_CHAR_T(8)),

    /*
     * These depends on sub-ioctl support which added in version 12.
     */
    /* sub-ioctl definitions */
	IW_PRIV_ARGS(AWL13_IOCTL_SETPARAM_INT, "",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_IOCTL_GETPARAM_INT, "",
		     0,
		     IW_ARG_INT_FIXED_T(1)),

	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_AUTH_TYPE,
		     "set_auth_type",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_AUTH_TYPE,
		     "auth_type",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_SITE_SURVEY,
		     "set_scan_chmode",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_SITE_SURVEY,
		     "scan_chmode",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_JOIN_REQ,
		     "set_join_req",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_JOIN_REQ,
		     "",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_11I_MODE,
		     "set_11i_mode",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_11I_MODE,
		     "11i_mode",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_POWER_MANAGEMENT,
		     "set_power_man",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_POWER_MANAGEMENT,
		     "power_man",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_SCAN_TYPE,
		     "set_scan_type",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_SCAN_TYPE,
		     "scan_type",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_POWER_SAVE,
		     "set_pow_save",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_POWER_SAVE,
		     "pow_save",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_BEACON_INTERVAL,
		     "set_beacon_int",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_BEACON_INTERVAL,
		     "beacon_int",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_LISTEN_INTERVAL,
		     "set_listen_int",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_LISTEN_INTERVAL,
		     "listen_int",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_BCAST_SSID,
		     "set_bcast_ssid",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_BCAST_SSID,
		     "bcast_ssid",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_DTIM_PERIOD,
		     "set_dtim_period",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_DTIM_PERIOD,
		     "dtim_period",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_REKEY_POLICY,
		     "set_rkey_policy",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_REKEY_POLICY,
		     "rkey_policy",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_REKEY_PERIOD,
		     "set_rkey_period",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_REKEY_PERIOD,
		     "rkey_period",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_SCAN_FILTER,
		     "set_scan_filter",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_SCAN_FILTER,
		     "scan_filter",
		     0,
		     IW_ARG_INT_FIXED_T(1)),

	IW_PRIV_ARGS(AWL13_PARAMS_INT_WPSDMD,
		     "set_wpsdmd",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WPSDMD,
		     "get_wpsdmd",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WPSST,
		     "set_wpsst",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WPSST,
		     "get_wpsst",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_USB_RMTWKUP_TIME,
		     "set_rmt_wkup",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_USB_RMTWKUP_TIME,
		     "rmt_wkup",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_USB_IN_XFER_MODE,
		     "set_usb_in_mode",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_INT_WID_USB_IN_XFER_MODE,
		     "usb_in_mode",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

    /*
     * These depends on sub-ioctl support which added in version 12.
     */
    /* sub-ioctl handlers (WID command)*/
	IW_PRIV_ARGS(AWL13_IOCTL_SETPARAM_STR, "",
		     IW_ARG_CHAR_T(AWL13_VERSION_SIZE),
		     0),
	IW_PRIV_ARGS(AWL13_IOCTL_GETPARAM_STR, "",
		     0,
		     IW_ARG_CHAR_T(AWL13_VERSION_SIZE)),

	IW_PRIV_ARGS(AWL13_PARAMS_STR_WID_FIRMWARE_VERSION,
		     "",
		     IW_ARG_CHAR_T(AWL13_VERSION_SIZE),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_STR_WID_FIRMWARE_VERSION,
		     "fw_ver",
		     0,
		     IW_ARG_CHAR_T(AWL13_VERSION_SIZE)),
	IW_PRIV_ARGS(AWL13_PARAMS_STR_DRIVER_VERSION,
		     "",
		     IW_ARG_CHAR_T(AWL13_VERSION_SIZE),
		     0),
	IW_PRIV_ARGS(AWL13_PARAMS_STR_DRIVER_VERSION,
		     "driver_ver",
		     0,
		     IW_ARG_CHAR_T(AWL13_VERSION_SIZE)),


	IW_PRIV_ARGS(AWL13_PRIV_SETWPSCREDLIST, "set_wpslist",
		     IW_ARG_CHAR_T(8),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETWPSCREDLIST, "get_wpslist",
		     IW_ARG_CHAR_T(8),
		     IW_ARG_CHAR_T(8)),

	IW_PRIV_ARGS(AWL13_PRIV_SETLOG,
		     "set_log_level",
		     IW_ARG_INT_FIXED_T(1),
		     0),
	IW_PRIV_ARGS(AWL13_PRIV_GETLOG,
		     "get_log_level",
		     0,
		     IW_ARG_INT_FIXED_T(1)),
};

static struct iw_handler_def awl13_iw_handler_def = {
	.standard = awl13_iw_handlers,
	.num_standard = ARRAY_SIZE(awl13_iw_handlers),
	.private = awl13_iw_priv_handlers,
	.num_private = ARRAY_SIZE(awl13_iw_priv_handlers),
	.private_args = (struct iw_priv_args *)awl13_iw_priv_args,
	.num_private_args = ARRAY_SIZE(awl13_iw_priv_args),
	.get_wireless_stats = awl13_iw_getstats,
};

void
awl13_wireless_attach(struct net_device *dev)
{
	dev->wireless_handlers = &awl13_iw_handler_def;

	return;
}


extern int
awl13_start_ap_scan(struct awl13_private *priv,
		     struct awl13_survey_res *survey)
{
	unsigned char old_scantype, old_scanfilter;
	int ret;
	int i;
	
	/* backup current parameters for scan */
	if ((ret = awl13_get_scan_type(priv, &old_scantype)) != 0)
		return ret;
	if ((ret = awl13_get_scan_filter(priv, &old_scanfilter)) != 0)
		return ret;

	/* set parameters for scan */
	if ((ret = awl13_set_scan_type(priv, 0x0)) != 0)
		return ret;
	if ((ret = awl13_set_scan_filter(priv, scan_filter)) != 0)
		goto restore_san_type;

	/* start scan for all channels */
	if ((ret = awl13_set_sitesv(priv, scan_ch_mode&0x3 )) != 0)
		goto restore_parameters;
	if ((awl13_set_startscan(priv, 0x01)) != 0)
		goto restore_parameters;

	/* wait for firmware to finish scan */
	for (i=0; i < ARRAY_SIZE(awl13_support_freq); i++) {

		msleep(cur_wait_time);
	}

	/* get result of scan for first half and second half */
	for (i=0; i<2; i++) {
		int size = sizeof(struct awl13_survey_res);
		memset(&survey[i], 0, size);
		ret = awl13_get_site_survey_results(priv, &survey[i], size);
		if (ret){
			ret = -EIO;
			goto restore_parameters;
		}
	}

	if( !(scan_ch_mode & 0x10) ) {
		if ((ret = awl13_set_sitesv(priv, 0x02)) != 0)
			goto restore_parameters;
	}

	/* restore parameters */
	if ((ret = awl13_set_scan_filter(priv, old_scanfilter)) != 0)
		goto restore_parameters;
	if ((ret = awl13_set_scan_type(priv, old_scantype)) != 0)
		goto restore_san_type;
	return 0;
	
restore_parameters:
	awl13_set_scan_filter(priv, old_scanfilter);
restore_san_type:
	awl13_set_scan_type(priv, old_scantype);
	
	return ret;
}

