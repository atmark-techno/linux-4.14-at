/*
 * awl13_sdiodrv.h
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
 * 2012-06-11   Modified by Atmark Techno, Inc.
 */

#ifndef _AWL13_SDIODRV_H_
#define _AWL13_SDIODRV_H_

#include <linux/version.h>
#include <linux/semaphore.h>
#include <linux/netdevice.h>
#include <linux/leds.h>
#include <net/iw_handler.h>

/**************
 * Driver Info
 **************/
/* BU1806GU SDIO IDs */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
#undef CONFIG_LEDS_TRIGGERS
#endif

#ifndef SDIO_VENDOR_ID_ROHM
#define SDIO_VENDOR_ID_ROHM			(0x032c)
#endif

#ifndef SDIO_DEVICE_ID_ROHM_BU1806GU
#define SDIO_DEVICE_ID_ROHM_BU1806GU		(0x0120)
#endif


/* BU1806GU SDIO Function #1 Registers */

#define AWL13_F1REG_SD2AHB_BLOCKLEN_LSB	(0x00000)
#define AWL13_F1REG_SD2AHB_BLOCKLEN_MSB	(0x00001)
#define AWL13_F1REG_AHB2SD_BLOCKLEN_LSB	(0x00002)
#define AWL13_F1REG_AHB2SD_BLOCKLEN_MSB	(0x00003)
#define AWL13_F1REG_INT_MASK_REG		(0x00004)
#define AWL13_F1REG_FUNC1_INT_PENDING		(0x00005)
#define AWL13_F1REG_FUNC1_INT_STATUS		(0x00006)
#define AWL13_F1WRITEBUFFER_ADDR		(0x00007)
#define AWL13_F1READBUFFER_ADDR		(0x00008)

#define AWL13_TX_CISINFO_STATUS_ADDR		(0x10F6)
#define AWL13_RX_CISINFO_STATUS_ADDR		(0x10F7)
#define AWL13_CISSTAT_TXSLEEP			(0x01)
#define AWL13_CISSTAT_TXWAIT			(0x02)
#define AWL13_CISSTAT_TXACTIVE			(0x03)
#define AWL13_CISSTAT_RXNODATA			(0x01)
#define AWL13_CISSTAT_RXREADY			(0x02)
#define AWL13_CISSTAT_ACTIVE			(0x03)
#define AWL13_CISSTAT_MASK			(0x03)
#define AWL13_TOGGLE_SHIFT			(4)

#define AWL13_INT_RXRCV			(0x01)


/* Value */

#define AWL13_CHUNK_SIZE			(512)
#define AWL13_WRITE_SIZE			(1536)
#define AWL13_READ_SIZE			(1536)
#define AWL13_WRITE_WORD_SIZE			(384)
#define AWL13_VERSION_SIZE			(128)

#define AWL13_TX_CISPOLL_INTVAL		(1)          // usec
#define AWL13_RX_CISPOLL_INTVAL		(100)        // usec
#define AWL13_TX_TOGGLE_TIMEOUT		(0)          // msec
#define AWL13_RX_TOGGLE_TIMEOUT		(500)        // msec
#define AWL13_DEVRDY_TIMEOUT			(5)          // sec

#ifdef AWL13_SDIO_HANG_FIX
#define AWL13_SDIO_TX_WAIT_DELAY		(13)          // usec
#endif


/*******************************
 * WID config packet structures
 *******************************/
#define MAX_APACKET_MSG_LEN (1660)

struct tx_args {
	struct net_device *dev;
	struct sk_buff *skb;
	struct list_head entry;
};

struct awl13_task_info {
	struct delayed_work queue;
	struct list_head tx_args_list;
	spinlock_t lock;
};

struct awl13_wid_frame {
	uint16_t wid;
	uint8_t len;
	uint8_t val[MAX_APACKET_MSG_LEN - 7];
}  __attribute__((packed));

struct awl13_wid_longframe {
	uint16_t wid;
	uint16_t len;
	uint8_t val[MAX_APACKET_MSG_LEN - 7];
}  __attribute__((packed));

struct awl13_packet_message {
	uint8_t type;
#define CTYPE_QUERY		0x51	/* Q: query */
#define CTYPE_WRITE		0x57	/* W: write */
#define CTYPE_RESPONSE		0x52	/* R: response */
#define CTYPE_INFO		0x49	/* I: info */
#define CTYPE_NETWORK		0x4E	/* N: network */
	uint8_t seqno;
	uint16_t len;
	uint8_t body[MAX_APACKET_MSG_LEN - 4];
} __attribute__((packed));
#define awl13_packet_message_fixed_len (4)

struct awl13_packet_header {
	uint16_t len:12;
	uint16_t type:4;
#define HTYPE_DATA_OUT		0x1
#define HTYPE_DATA_IN		0x2
#define HTYPE_CONFIG_RES	0x3
#define HTYPE_CONFIG_REQ	0x4
} __attribute__((packed));

struct awl13_packet {
	/* header part */
	union {
		uint16_t header;
		struct awl13_packet_header h;
	} __attribute__((packed));

	/* message part */
	union {
		uint8_t message[MAX_APACKET_MSG_LEN];
		struct awl13_packet_message m;
	} __attribute__((packed));
} __attribute__((packed, aligned(sizeof(int32_t))));

#define response_ok(w) \
	((((w)->wid != WID_STATUS) || ((w)->val[0] != 1)) ? 0 : 1)

/***************************
 * AWL13 private structure
 ***************************/
struct awl13_private {
	struct net_device *netdev;
	struct sdio_func *func;

	struct {
		int netdev_registered	: 1;
		int sysfs_initialized	: 1;
	} flags;

	atomic_t state;
#define AWL13_STATE_NO_FW			0
#define AWL13_STATE_READY			1
#define AWL13_STATE_RUNNING			2

	atomic_t power_state;
#define AWL13_PWR_STATE_ACTIVE			0x00
#define AWL13_PWR_STATE_SLEEP			0x02

	struct awl13_packet *wid_response;
	struct awl13_packet *wid_request;
	struct awl13_packet *rxbuf;
	atomic_t wid_disposal;
#define AWL13_WID_NO_DISP			0x00
#define AWL13_WID_DISP				0x01
	struct awl13_task_info tx_task;
	struct task_struct *tx_thread;
	struct completion tx_thread_wait;

	struct net_device_stats net_stats;

	struct semaphore wid_req_lock;
	struct completion wid_complete;

	int fw_load;
#define AWL13_FWSTAND_ALONE                    0x00
#define AWL13_FWLOAD_EXEC                      0x01
#define AWL13_FWLOAD_AFTER                     0x02

	unsigned char *fw_image;
	unsigned int fw_size;
	int fw_type;
#define AWL13_FWTYPE_STA			0x01
#define AWL13_FWTYPE_TEST			0x02

	atomic_t bus_type;
#define SDIO_FORCE_INIT				0
#define SDIO_FORCE_1BIT				1
#define SDIO_FORCE_4BIT				2

	atomic_t power_mngt;
#define AWL13_PWR_MNGT_ACTIVE			0x00
#define AWL13_PWR_MNGT_SLEEP			0x01
#define AWL13_PWR_MNGT_MAXSLEEP		0x02

	struct completion reset_ready;

#ifdef CONFIG_LEDS_TRIGGERS
	struct led_trigger *led;
#endif /* CONFIG_LEDS_TRIGGERS */
	int 	bss_type;
	int	tx_cis_intval;
	int	rx_cis_intval;
	int	tx_cis_toggle_timeout;
	int	rx_cis_toggle_timeout;
	int	tx_toggle;
	int	rx_toggle;
#define TOGGLE_0			0x00
#define TOGGLE_1			0x01
#define TOGGLE_NONE			0x02


	unsigned char prev_enckey[32];
	unsigned char prev_crypt;

	struct attribute_group attr_group;
#define WPS_CRED_LIST_SZ	1024
	unsigned char	wps_cred_list[WPS_CRED_LIST_SZ];
	unsigned long 	wps_cred_list_size;

#ifdef RX_CIS_POLLING
	struct task_struct *rx_thread;
	struct completion rx_thread_wait;
#endif

	char			bssid[ETH_ALEN];
	union iwreq_data	iwap;
	struct iw_statistics	iwstats;
};

/* LEDs */

static inline void
awl13_register_led_trigger(struct awl13_private *priv)
{
#ifdef CONFIG_LEDS_TRIGGERS
	led_trigger_register_simple(priv->netdev->dev.bus_id, &priv->led);
#endif /* CONFIG_LEDS_TRIGGERS */
}

static inline void
awl13_unregister_led_trigger(struct awl13_private *priv)
{
#ifdef CONFIG_LEDS_TRIGGERS
	if (priv->led) {
		led_trigger_unregister_simple(priv->led);
		priv->led = NULL;
	}
#endif /* CONFIG_LEDS_TRIGGERS */
}

static inline void
awl13_led_trigger_event(struct awl13_private *priv,
			 enum led_brightness event)
{
#ifdef CONFIG_LEDS_TRIGGERS
	led_trigger_event(priv->led, event);
#endif /* CONFIG_LEDS_TRIGGERS */
}

extern int awl13_debug_flag;

extern int awl13_enable_wide(struct awl13_private *priv); 
extern int awl13_disable_wide(struct awl13_private *priv); 
extern int awl13_set_txcis_polling_intvl(struct awl13_private *priv, unsigned int val);
extern int awl13_get_txcis_polling_intvl(struct awl13_private *priv, unsigned int *val);
extern int awl13_set_rxcis_polling_intvl(struct awl13_private *priv, unsigned int val);
extern int awl13_get_rxcis_polling_intvl(struct awl13_private *priv, unsigned int *val);
extern int awl13_set_txcis_toggle_timeout(struct awl13_private *priv, unsigned int val);
extern int awl13_get_txcis_toggle_timeout(struct awl13_private *priv, unsigned int *val);
extern int awl13_set_rxcis_toggle_timeout(struct awl13_private *priv, unsigned int val);
extern int awl13_get_rxcis_toggle_timeout(struct awl13_private *priv, unsigned int *val);
extern int awl13_send_prepare(struct sdio_func *func, int size);
extern int awl13_set_interrupt(struct sdio_func *func, int setint);
extern int awl13_flow_start(struct awl13_private *priv);
extern int awl13_flow_end(struct awl13_private *priv);
extern int awl13_restart(struct awl13_private *priv);

#endif /* _AWL13_SDIODRV_H_ */
