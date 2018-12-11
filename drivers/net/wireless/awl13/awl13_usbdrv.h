/*
 * awl13_usbdrv.h
 *
 *  Copyright (C) 2011-2012 Atmark Techno, Inc.
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
 * 2011-11-25   Modified by Atmark Techno, Inc.
 * 2012-06-11   Modified by Atmark Techno, Inc.
 */

#ifndef _AWL13_USBDRV_H_
#define _AWL13_USBDRV_H_

#include <linux/version.h>
#include <linux/semaphore.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/leds.h>
#include <net/iw_handler.h>

/* macro & define  */
#ifdef AWL13_HOTPLUG_FIRMWARE
#define AWL13_DEFAULT_FIRMWARE			"fwimage340d_STA_USB.bin"
#endif /* AWL13_HOTPLUG_FIRMWARE */

/********
 * Value
 ********/
#define AWL13_CMD_TIMEOUT			(HZ*1)
#define AWL13_VERSION_SIZE			(128)

#define AWL13_TERM_SIZE			(256)

#define AWL13_BU1805GU_DMA_SIZE	(512)


#define AWL13_EP_CTL				0
#define AWL13_EP_DATOUT			1
#define AWL13_EP_WIDOUT			2
#define AWL13_EP_DATIN				3
#define AWL13_EP_WIDIN				4
#define AWL13_EP_INTIN				5

/* request cancel urb pf WID BULK_IN */
#define AWL13_WID_BUILK_CANCEL_REQ_RETRY_N 5

/*******************************
 * WID config packet structures
 *******************************/
#define MAX_APACKET_MSG_LEN (1660)

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
 * WID_STATUS CODE
 ***************************/
typedef enum {
    WID_STAT_WRN_FW_TYPE_MISMATCH  = 0xCF,
    WID_STAT_ERR_PACKET_ERROR      = 0xF5,
    WID_STAT_ERR_INTERNAL_ERROR    = 0xF6,
    WID_STAT_ERR_FW_CHECKSUM_ERROR = 0xF7,
    WID_STAT_ERR_ILLEGAL_FWID      = 0xF8,
    WID_STAT_ERR_DATA_ERROR        = 0xF9,
    WID_STAT_ERR_FCS_ERROR         = 0xFA,
    WID_STAT_ERR_MTYPE_INCORRECT   = 0xFB,
    WID_STAT_ERR_WID_INCORRECT     = 0xFC,
    WID_STAT_ERR_MSG_LENGTH        = 0xFD,
    WID_STAT_ERR_ILLEGAL_SEQNO     = 0xFE,
    WID_STAT_ERR                   = 0xFF,
    WID_STAT_NG                    = 0x00,
    WID_STAT_OK                    = 0x01,
} WID_STATUS_ERR_T;
#define  WID_STAT_NAME_WRN_FW_TYPE_MISMATCH 	"WRN_FW_TYPE_MISMATCH"
#define  WID_STAT_NAME_ERR_PACKET_ERROR     	"ERR_PACKET_ERROR"
#define  WID_STAT_NAME_ERR_INTERNAL_ERROR   	"ERR_INTERNAL_ERROR"
#define  WID_STAT_NAME_ERR_FW_CHECKSUM_ERROR	"ERR_FW_CHECKSUM_ERROR"
#define  WID_STAT_NAME_ERR_ILLEGAL_FWID     	"ERR_ILLEGAL_FWID"
#define  WID_STAT_NAME_ERR_DATA_ERROR       	"ERR_DATA_ERROR"
#define  WID_STAT_NAME_ERR_FCS_ERROR        	"ERR_FCS_ERROR"
#define  WID_STAT_NAME_ERR_MTYPE_INCORRECT  	"ERR_MTYPE_INCORRECT"
#define  WID_STAT_NAME_ERR_WID_INCORRECT    	"ERR_WID_INCORRECT"
#define  WID_STAT_NAME_ERR_MSG_LENGTH       	"ERR_MSG_LENGTH"
#define  WID_STAT_NAME_ERR_ILLEGAL_SEQNO    	"ERR_ILLEGAL_SEQNO"
#define  WID_STAT_NAME_ERR                  	"ERR"
#define  WID_STAT_NAME_NG                   	"NG"
#define  WID_STAT_NAME_OK                   	"OK"
#define  WID_STAT_NAME_DISCONNECTED           	"DISCONNECTED"
#define  WID_STAT_NAME_CONNECTED              	"CONNECTED"
#define  WID_STAT_NAME_UNKNOWN              	"unknown"

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
/***************************
 * AWL13 WMM 
 ***************************/
typedef enum
{
/* default queue priorities: VO->VI->BE->BK */
    PND_Q,		/* Pending Queue */
    AC_BK_Q,		/* AC-BK Queue */
    AC_BE_Q,		/* AC-BE Queue */
    AC_VI_Q,		/* AC-VI Queue */
    AC_VO_Q,		/* AC-VO Queue */
    HIP_Q,		/* High-Priority Queue */
    AWL13_MAX_AC_QUEUES, 
} wlan_wmm_ac_e;



typedef enum {
              AC_BE = 0,
              AC_BK = 1,
              AC_VI = 2,
              AC_VO = 3,
              NUM_AC = 4
} ACCESS_CATEGORIES_T;

/* Number of slots over which the simulation is run */
#define NUM_SLOTS 100000
/* This number is the nominal TX time for each MPDU. It is used to conver */
/* the TXOP Limit expressed in units of time to number of packets.        */
#define TXOP_TO_PACKET_CONV_FACTOR 400 /* microsecs/pkt */

#define TXOP_FOR_PND_Q            0
#define TXOP_FOR_AC_BK_Q          1
#define TXOP_FOR_AC_BE_Q          2
#define TXOP_FOR_AC_VI_Q          50
#define TXOP_FOR_AC_VO_Q          45
#define TXOP_FOR_HIP_Q            100

typedef struct
{
	unsigned int ecwmin;	/* Exponential CW-Max. Actual CWmax = 2^ecmax - 1 */
	unsigned int ecwmax;	/* Exponential CW-Min. Actual CWmin = 2^ecmin - 1 */
	unsigned int txop;		/* TXOP-Limit in microsecs */
	unsigned int aifsn; 	/* AIFSN */
} edca_param_t;

typedef struct
{
	unsigned int bcnt;		/* Current Back-off count */
	int cw; 				/* Current Back-off count */
	unsigned int nftxp; 	/* Number of frames in TXOP */
	unsigned int ntxopwon;	/* Number of TXOPs won */
	unsigned int numtx; 	/* Number of frames transmitted */
	unsigned int numcol;	/* Number of Collisions */
} edca_state_t;


struct awl13_tx_queue {
	struct sk_buff		skbuf;
	unsigned int		txop;
};

struct awl13_wmm {
	struct awl13_tx_queue	txSkbQ[AWL13_MAX_AC_QUEUES];
    unsigned int  QScheduleProb[AWL13_MAX_AC_QUEUES]; /* TXOP Probability */
	wlan_wmm_ac_e TxQPriority[AWL13_MAX_AC_QUEUES];   /* Arranged in increasing order of TxQPriority */
	unsigned int edca_txprob[NUM_AC];
	edca_param_t edca_params[NUM_AC];
	ACCESS_CATEGORIES_T     edca_priority[NUM_AC];
	unsigned int		window_size;
	spinlock_t		wmm_skbq_lock;
};
#endif

#include	<linux/kthread.h>

struct awl13_thread
{
    struct task_struct *task;
    wait_queue_head_t waitQ;
    pid_t pid;
    void *priv;
    int flags;
    struct completion 	complete;
};

static inline void
awl13_activate_thread(struct awl13_thread *thr)
{
    thr->pid = current->pid;
}

static inline void
awl13_deactivate_thread(struct awl13_thread *thr)
{
	complete(&thr->complete);
	thr->pid = 0;
}

static inline void
awl13_create_thread(int (*func) (void *), struct awl13_thread *thr, char *name)
{
    if (thr->task) {
        awl_debug ("%s: thread already running\n", name);
        return;
    }

    thr->task = kthread_run(func, thr, "%s", name);
}

static inline int
awl13_terminate_thread(struct awl13_thread *thr)
{
    if (!thr->pid) {
        return -1;
    }
    kthread_stop(thr->task);
    thr->task = NULL;

    return 0;
}


/***************************
 * AWL13 private structure
 ***************************/
/* This structure cannot exceed sizeof(unsigned long [5]) AKA 20 bytes */
struct awl13_data {
	struct awl13_private* 	priv;
	unsigned int 	wid_bulk_in;
	unsigned int 	wid_bulk_out;
	unsigned long	reserve [2];
};

struct awl13_private {
	struct net_device *netdev;
	struct awl13_usbnet     *usbnetDev;

	atomic_t state;
#define AWL13_STATE_NO_FW			0
#define AWL13_STATE_READY			1

	atomic_t power_state;
#define AWL13_PWR_STATE_ACTIVE			0x00
#define AWL13_PWR_STATE_SLEEP			0x02

	atomic_t power_mngt;
#define AWL13_PWR_MNGT_ACTIVE			0x00
#define AWL13_PWR_MNGT_MIN_FAST		0x01
#define AWL13_PWR_MNGT_MAX_FAST		0x02
#define AWL13_PWR_MNGT_MIN_PSPOLL		0x03
#define AWL13_PWR_MNGT_MAX_PSPOLL		0x04

	struct awl13_stat {
		int	awl13_binded		: 1;
		int	net_registered		: 1;
		int	prev_firmware_found	: 1;
		int	do_task_quit		: 1;
	} probe_flags;

	struct awl13_packet *wid_response;
	struct awl13_packet *wid_request;
	struct awl13_packet *rxbuf; /*deprecated*/
	
	atomic_t wid_disposal;
#define AWL13_WID_NO_DISP			0x00
#define AWL13_WID_DISP				0x01

	atomic_t modstate;
#define AWL13_MODSTATE_NONE			0
#define AWL13_MODSTATE_EXIST			1

	struct delayed_work 	wid_queue;
	struct timer_list	wid_retry;
	struct semaphore 	wid_req_lock;
	struct completion 	wid_complete;
	struct completion 	wid_out_complete;
	struct urb		*wid_urb;
	spinlock_t 		wid_submit_lock;
	struct completion 	wid_do_task_complete;
	enum usb_device_speed	speed;


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	unsigned char	    *fw_image;
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	unsigned char	    *fw_image[FIRMWARE_N_BLOCKS];
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
#ifdef AWL13_HOTPLUG_FIRMWARE
	struct semaphore	fw_lock;
	char			*firmware_file;
#endif /* AWL13_HOTPLUG_FIRMWARE */
	int			fw_ready_check;
	int			fw_not_ready;
	unsigned int		fw_size;
	int fw_type;
#define AWL13_FWTYPE_STA			0x01
#define AWL13_FWTYPE_TEST			0x02

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
	struct awl13_wmm wmm;
#else
	struct sk_buff		skbq;
	spinlock_t		skbq_lock;
#endif
	struct awl13_thread txthr;
	struct awl13_thread pmthr;
	struct awl13_thread pmact;

	unsigned char prev_enckey[32];
	unsigned char prev_crypt;

	struct attribute_group attr_group;

	atomic_t 	suspend_state;
	atomic_t 	resume_state;
#define AWL13_IDLE					0x00
#define AWL13_PREPARE_SUSPENDED	0x01
#define AWL13_EXECUTE_SUSPENDED	0x02
#define AWL13_SUSPENDED			0x03
#define AWL13_PREPARE_RESUME		0x10
#define AWL13_PREPARE_WAKEUP		0x20
#define AWL13_OFF					0x00
#define AWL13_ON					0x01

	atomic_t 	net_suspend_state;
	spinlock_t 	pm_lock;
	atomic_t 	force_active;

	void*    	parent;
	int		port_status;

#define WPS_CRED_LIST_SZ	1024
	unsigned char	wps_cred_list[WPS_CRED_LIST_SZ];
	unsigned long 	wps_cred_list_size;

	char			bssid[ETH_ALEN];
	union iwreq_data	iwap;
	struct iw_statistics	iwstats;

#define AWL13_PRIV_TMP_BUF_SZ 1024
	unsigned char tmp_buf[AWL13_PRIV_TMP_BUF_SZ];
};

extern unsigned int awl13_align_word_size(unsigned int size);
extern unsigned int awl13_align_chunk_size(unsigned int size);
extern void         awl13_wid_complete (struct urb *urb);
extern int awl13_read_cmd(struct awl13_private *priv, int size, u8 cmd, u16 value, u16 index );
extern int asix_write_cmd( struct awl13_private *priv, int size, u8 cmd, u16 value, u16 index );
extern void awl13_to_idle( struct awl13_private *priv );
extern int awl13_suspend( struct awl13_private *priv );
extern int awl13_resume( struct awl13_private *priv, int flag );

/* interface from usbnet core to each USB networking link we handle */
struct awl13_usbnet {
	/* housekeeping */
	struct usb_device	*udev;
	struct usb_interface	*intf;
	struct awl13_driver_info	*driver_info;
	const char		*driver_name;
	void			*driver_priv;
	wait_queue_head_t	*wait;
	//struct mutex		phy_mutex;
	unsigned char		suspend_count;

	/* i/o info: pipes etc */
	unsigned int		in, out;
	struct usb_host_endpoint *status;
	struct timer_list	delay;

	/* protocol/interface state */
	struct net_device	*net;
	struct net_device_stats	stats;
	int			msg_enable;
	unsigned long		data [5];
	u32			xid;
	u32			hard_mtu;	/* count any extra framing */
	size_t			rx_urb_size;	/* size for rx urbs */
	struct mii_if_info	mii;

	/* various kinds of pending driver work */
	struct sk_buff_head	rxq;
	struct sk_buff_head	txq;
	struct sk_buff_head	done;
	struct urb		*interrupt;
	struct tasklet_struct	bh;

	struct work_struct	kevent;
	unsigned long		flags;
#define AWL13_EVENT_TX_HALT	0
#define AWL13_EVENT_RX_HALT	1
#define AWL13_EVENT_RX_MEMORY	2
#define AWL13_EVENT_STS_SPLIT	3
#define AWL13_EVENT_LINK_RESET	4
};

static inline struct usb_driver *driver_of(struct usb_interface *intf)
{
	return to_usb_driver(intf->dev.driver);
}

/* interface from the device/framing level "minidriver" to core */
struct awl13_driver_info {
	char		*description;

	int		flags;

/* framing is CDC Ethernet, not writing ZLPs (hw issues), or optionally: */
#define AWL13_FLAG_FRAMING_NC	0x0001	/* guard against device dropouts */
#define AWL13_FLAG_FRAMING_GL	0x0002	/* genelink batches packets */
#define AWL13_FLAG_FRAMING_Z	0x0004	/* zaurus adds a trailer */
#define AWL13_FLAG_FRAMING_RN	0x0008	/* RNDIS batches, plus huge header */

#define AWL13_FLAG_NO_SETINT	0x0010	/* device can't set_interface() */
#define AWL13_FLAG_ETHER	0x0020	/* maybe use "eth%d" names */

#define AWL13_FLAG_FRAMING_AX	0x0040	/* AX88772/178 packets */
#define AWL13_FLAG_WLAN	0x0080	/* use "awlan%d" names */


	/* init device ... can sleep, or cause probe() failure */
	int	(*bind)(struct awl13_usbnet *, struct usb_interface *);

	/* cleanup device ... can sleep, but can't fail */
	void	(*unbind)(struct awl13_usbnet *, struct usb_interface *);

	/* reset device ... can sleep */
	int	(*reset)(struct awl13_usbnet *);

	/* see if peer is connected ... can sleep */
	int	(*check_connect)(struct awl13_usbnet *);

	/* for status polling */
	void	(*status)(struct awl13_usbnet *, struct urb *);

	/* link reset handling, called from defer_kevent */
	int	(*link_reset)(struct awl13_usbnet *);

	/* fixup rx packet (strip framing) */
	int	(*rx_fixup)(struct awl13_usbnet *dev, struct sk_buff *skb);

	/* fixup tx packet (add framing) */
	struct sk_buff	*(*tx_fixup)(struct awl13_usbnet *dev,
				struct sk_buff *skb, gfp_t flags);

	/* early initialization code, can sleep. This is for minidrivers
	 * having 'subminidrivers' that need to do extra initialization
	 * right after minidriver have initialized hardware. */
	int	(*early_init)(struct awl13_usbnet *dev);

	/* called by minidriver when link state changes, state: 0=disconnect,
	 * 1=connect */
	void	(*link_change)(struct awl13_usbnet *dev, int state);

	/* for new devices, use the descriptor-reading code instead */
	int		in;		/* rx endpoint */
	int		out;		/* tx endpoint */

	unsigned long	data;		/* Misc driver specific data */
};


/* we record the state for each of our queued skbs */
enum awl13_skb_state {
	awl13_illegal = 0,
	awl13_tx_start, awl13_tx_done,
	awl13_rx_start, awl13_rx_done, awl13_rx_cleanup
};

struct awl13_skb_data {	/* skb->cb is one of these */
	struct urb		*urb;
	struct awl13_usbnet	*dev;
	enum awl13_skb_state	state;
	size_t			length;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
typedef struct wait_queue_entry wait_queue_t;
#endif

#endif  /* _AWL13_USBDRV_H_ */


