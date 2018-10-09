/*
 * awl13_fw.c
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
 * 2008-04-02    Created by Nissin Systems Co.,Ltd.
 * 2009-06-19   Modified by Atmark Techno, Inc.
 * 2011-11-25   Modified by Atmark Techno, Inc.
 */

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

#include "awl13_log.h"
#include "awl13_device.h"
#include "awl13_wid.h"
#include "awl13_fw.h"

#define to_download_addr(w) ((w)->val[3] << 24 | (w)->val[2] << 16 | \
			     (w)->val[1] <<  8 | (w)->val[0] <<  0)
#define to_download_size(w) ((w)->val[5] <<  8 | (w)->val[4] <<  0)

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
#define IS_ERROR(r)	    ( ((r&0xFF)>0xCF) ? 1 : 0 ) /* -49 */
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

int
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
awl13_firmware_load(struct sdio_func *func)
{
	struct awl13_private *priv = sdio_get_drvdata(func);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
awl13_firmware_load(struct awl13_private *priv)
{
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	struct awl13_wid_frame *wframe;
	int ret = 0;

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	int retry = 3;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

	struct awl13_firmware_info *info =

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
		(struct awl13_firmware_info *)priv->fw_image;
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
		(struct awl13_firmware_info *)priv->fw_image[0];
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

	unsigned char val;

	if (info->id != AWL13_FW_ID) {
	      awl_err("l.%d: info->id=%lu and AWL13_FW_ID=%d are different\n" ,
		__LINE__, info->id, AWL13_FW_ID);
		return -EINVAL;
	}

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	priv->tx_toggle = TOGGLE_NONE;
	priv->rx_toggle = TOGGLE_NONE;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

	memset(priv->wid_request, 0, sizeof(struct awl13_packet));
	memset(priv->wid_response, 0, sizeof(struct awl13_packet));
	val = 1;
	set_wid_write_request(priv->wid_request, WID_RESET, &val, 1);

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	ret = awl13_wid_request(func, WID_DEFAULT_TIMEOUT);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	ret = awl13_usb_wid_request(priv, WID_DEFAULT_TIMEOUT);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

	if (ret) {
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
		if (priv->fw_ready_check == 0)
			awl_err("failed WID_RESET\n");
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		awl_err("failed WID_RESET\n");
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		return -EIO;
	}

	wframe = to_wid_frame(priv->wid_response);
	if (wframe->wid == WID_STATUS) {
		awl_info("Firmware is already loaded\n");
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
		priv->probe_flags.prev_firmware_found = 1;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		return 0;
	}
	while (wframe->wid == WID_DOWNLOAD_REQUEST) {
		struct awl13_wid_longframe *lframe = to_wid_longframe(priv->wid_request);
		unsigned char sum;
		unsigned char *src;
		unsigned char *dst;
		unsigned int request_addr;
		unsigned short request_size;
		unsigned int loop;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
		int block_count;
		int remain_size;
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */

		request_addr = to_download_addr(wframe);
		request_size = to_download_size(wframe);

		memset(priv->wid_request, 0, sizeof(struct awl13_packet));
		/* wid */
		lframe->wid = WID_DOWNLOAD_DATA;
		lframe->len = request_size;

		sum = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
		src = ((unsigned char*)info) + request_addr;
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
		block_count = request_addr / FIRMWARE_BLOCK_MAX_SIZE;
		src = priv->fw_image[block_count] +
			request_addr%FIRMWARE_BLOCK_MAX_SIZE;
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

		dst = lframe->val;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
		remain_size = FIRMWARE_BLOCK_MAX_SIZE -
			request_addr%FIRMWARE_BLOCK_MAX_SIZE;
		loop=0;
		if (remain_size < request_size) {
			for (; loop < remain_size; loop++, src++, dst++) {
				*dst = *src; /* copy par byte */
				sum += *dst; /* add sum*/
			}
			block_count++;
			src = priv->fw_image[block_count];
		}
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
		for (loop = 0; loop < request_size; loop++, src++, dst++) {
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
		for (; loop < request_size; loop++, src++, dst++) {
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

			*dst = *src; /* copy par byte */
			sum += *dst; /* add sum*/
		}
		*dst = sum;
		/* message */
		priv->wid_request->m.len = (awl13_packet_message_fixed_len
			     + 2 /* wid:(unsinged short) */
			     + 2 /* len:(unsigned short) */
			     + 1 /* sum:(unsigned char) */
			     + request_size);

		priv->wid_request->m.type = CTYPE_WRITE;

		/* header */
		priv->wid_request->h.len = priv->wid_request->m.len;
		priv->wid_request->h.type = HTYPE_CONFIG_REQ;

		memset(priv->wid_response, 0, sizeof(struct awl13_packet));

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
		ret = awl13_wid_request(func, WID_DEFAULT_TIMEOUT);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		ret = awl13_usb_wid_request(priv, WID_DEFAULT_TIMEOUT);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
		if (ret) {
			if(IS_ERROR(ret)){
				awl_err("firmware load(error code=%02X(%d))\n",
					ret, ret );
				if( retry-- == 0 ) {
					awl_err("failed WID_DOWNLOAD_DATA (code=%d))\n", ret );
					return -EIO;
				}
			}
			else{
				awl_err("firmware load(warning code=%d)\n",
					ret );
			}
		}
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		if (ret) {
			awl_err("failed WID_DOWNLOAD_DATA\n");
			return -EIO;
		}
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	}
	if (wframe->wid != WID_START) {
		awl_err("unknown WID received\n");
		return -EINVAL;
	}

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	sdio_claim_host(func);
	ret = awl13_send_prepare(func, AWL13_WRITE_WORD_SIZE);
	sdio_release_host(func);
	if (ret) {
		awl_err("failed send prepare\n");
		return -EINVAL;
	}
	priv->tx_toggle = TOGGLE_NONE;
	priv->rx_toggle = TOGGLE_NONE;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	memset(priv->wid_request, 0, sizeof(struct awl13_packet));
	val = 1;

	set_wid_write_request(priv->wid_request, WID_START, &val, 1);

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	ret = awl13_wid_request(func, WID_DEFAULT_TIMEOUT);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	ret = awl13_usb_wid_request(priv, WID_DEFAULT_TIMEOUT);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

	if (ret)
		awl_err("failed WID_START\n");

	return ret;
}

int
awl13_firmware_setup(struct awl13_private *priv)
{
	int ret;
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
	int retry_count;
#define AWL13_FIRMWARE_LOAD_RETRY_MAX		10
#define AWL13_FIRMWARE_LOAD_RETRY_INTERVAL	500
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

	if (atomic_read(&priv->state) != AWL13_STATE_NO_FW) {
		awl_debug("Firmware is already loaded\n");
		return 0;
	}

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
#ifdef AWL13_HOTPLUG_FIRMWARE
	ret =  down_interruptible(&priv->fw_lock);
	if (ret)
		awl_err("l.%d: down_interruptible(&fw_download_lock) failed\n",
			 __LINE__);
#endif /* AWL13_HOTPLUG_FIRMWARE */
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	ret = awl13_firmware_load(priv->func);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	ret = -1;
	for (retry_count=0 ; 
	     retry_count < AWL13_FIRMWARE_LOAD_RETRY_MAX && ret != 0;
	     retry_count++ ) {
		ret = awl13_firmware_load(priv);
		awl_debug("awl13_firmware_load executed(ret=%d) count=%d\n",
			  __LINE__, ret, retry_count);
		if (!retry_count)
			continue;
		msleep(AWL13_FIRMWARE_LOAD_RETRY_INTERVAL);
	}
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	if (ret) {
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
		if (priv->fw_ready_check == 0) {
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
			awl_err("l.%d: awl13_firmware_load() failed on %d\n",
			__LINE__, ret);
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
		}
#ifdef AWL13_HOTPLUG_FIRMWARE
		up(&priv->fw_lock);
#endif /* AWL13_HOTPLUG_FIRMWARE */
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

		return ret;
	}
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
#ifdef AWL13_HOTPLUG_FIRMWARE
	up(&priv->fw_lock);
#endif /* AWL13_HOTPLUG_FIRMWARE */
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

//	atomic_set(&priv->state, AWL13_STATE_READY);

	return 0;
}

int
awl13_firmware_setup_without_load(struct awl13_private *priv)
{
	unsigned char mac[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	char version[128];
	int ret;
	uint8_t preamble = 2;

	awl13_get_macaddr(priv, mac, 6);
	memcpy(priv->netdev->dev_addr, mac, 6);

	awl_info("MAC is %02x:%02x:%02x:%02x:%02x:%02x\n",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	awl13_get_firmware_version(priv, version, 128);
	if (version[0] == 'T')
		priv->fw_type = AWL13_FWTYPE_TEST;
	else
		priv->fw_type = AWL13_FWTYPE_STA;

	ret = awl13_set_scan_type(priv, 1);
	if (ret)
		return ret;
	ret = awl13_set_common_value(priv, WID_PREAMBLE,
				&preamble, 1, WID_DEFAULT_TIMEOUT);
	if (ret)
		return ret;

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
	priv->fw_ready_check = 0;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	return 0;
}

