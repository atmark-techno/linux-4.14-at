/*
 * awl13_usbdrv.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/usb.h>
#ifdef AWL13_HOTPLUG_FIRMWARE
#include <linux/firmware.h>
#endif /* AWL13_HOTPLUG_FIRMWARE */
#include <linux/if_arp.h>
#include <net/iw_handler.h>
#include "awl13_log.h"
#include "awl13_device.h"
#include "awl13_wid.h"
#include "awl13_ioctl.h"
#include "awl13_fw.h"
#include "awl13_sysfs.h"
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
#include "awl13_wmm.h"
#endif

/* macro & define  */
#define RX_MAX_QUEUE_MEMORY (60 * 1518)
#define	RX_QLEN(dev) (((dev)->udev->speed == USB_SPEED_HIGH) ? \
			(RX_MAX_QUEUE_MEMORY/(dev)->rx_urb_size) : 4)
#define	TX_QLEN(dev) (((dev)->udev->speed == USB_SPEED_HIGH) ? \
			(RX_MAX_QUEUE_MEMORY/(dev)->hard_mtu) : 4)


// reawaken network queue this soon after stopping; else watchdog barks
#define TX_TIMEOUT_JIFFIES	(5*HZ)

// throttle rx/tx briefly after some faults, so khubd might disconnect()
// us (it polls at HZ/4 usually) before we report too many false errors.
#define THROTTLE_JIFFIES	(HZ/8)

// between wakeups
#define UNLINK_TIMEOUT_MS	3

#define WID_TIMEOUT		(1000)

#define WID_USB_RETRY(p)	{ 					\
	struct awl13_private	*pv = (struct awl13_private*)p;	\
	if (atomic_read(&pv->modstate)==AWL13_MODSTATE_EXIST) 		\
		mod_timer (&pv->wid_retry, jiffies +msecs_to_jiffies(WID_TIMEOUT) ); \
	};
/* Power save &  Power Management */
#define SetPortFeature                  (0x2300 | USB_REQ_SET_FEATURE)
#define ClearPortFeature                (0x2300 | USB_REQ_CLEAR_FEATURE)
#define GetPortStatus                   (0xa300 | USB_REQ_GET_STATUS)
#define USB_PORT_FEAT_SUSPEND           2   /* L2 suspend */
#define USB_PORT_STAT_SUSPEND           (0x01 << USB_PORT_FEAT_SUSPEND)

#define USB_GET_STAT_CYC    			10                      /* 10 msec   */
#define USB_GET_STAT_TMO    			(200/USB_GET_STAT_CYC)  /* 200 msec */
#define USB_PM_WAIT(p)      			{							\
		struct awl13_private* pv = (struct awl13_private*)p;		\
		wait_for_completion_interruptible_timeout(					\
			&(pv->pmact.complete),									\
			msecs_to_jiffies(USB_GET_STAT_CYC) );					\
}

#ifdef AWL13_HOTPLUG_FIRMWARE
static char *firmware = AWL13_DEFAULT_FIRMWARE;
module_param (firmware, charp, S_IRUGO);
MODULE_PARM_DESC (firmware,
		  "firmware name. Default/empty:"AWL13_DEFAULT_FIRMWARE"");
#endif /* AWL13_HOTPLUG_FIRMWARE */

/* static function and variable */
unsigned int awl13_align_chunk_size(unsigned int size);

static char* awl13_wid_status(uint8_t type, uint8_t code);
static int   awl13_wid_check(struct awl13_private *priv, struct awl13_packet *res );
void awl13_wid_complete (struct urb *urb);
static void awl13_do_task(struct work_struct *work);
static void awl13_wid_retry (unsigned long param);
static void awl13_task_init(struct awl13_private *priv);
static int awl13_rx_fixup(struct awl13_usbnet *dev, struct sk_buff *skb);
static struct sk_buff *awl13_tx_fixup(struct awl13_usbnet *dev, struct sk_buff *skb, gfp_t flags);
static void awl13_unbind(struct awl13_usbnet *dev, struct usb_interface *intf);
static void awl13_bind_clean(struct awl13_private *priv);
static int awl13_bind(struct awl13_usbnet *dev, struct usb_interface *intf);
static void awl13_ctl_complete(struct urb *urb);
static int awl13_port_status( struct awl13_private *priv, 
								unsigned short type,
								unsigned short val,
                        		unsigned short* psta );
static int awl13_pm_thread(void *thr);
static int awl13_last_init(struct awl13_usbnet *dev );

static void awl13_defer_bh(struct awl13_usbnet *dev, struct sk_buff *skb, struct sk_buff_head *list);
static void awl13_rx_submit (struct awl13_usbnet *dev, struct urb *urb, gfp_t flags);
static void awl13_rx_complete (struct urb *urb);
static void awl13_intr_complete (struct urb *urb);
static int  awl13_unlink_urbs (struct awl13_usbnet *dev, struct sk_buff_head *q);
static void awl13_kevent (struct work_struct *work);
static void awl13_tx_complete (struct urb *urb);
static int  awl13_usbnet_get_endpoints(struct awl13_usbnet *dev, struct usb_interface *intf);
static int awl13_usbnet_init_status (struct awl13_usbnet *dev, struct usb_interface *intf);
static void awl13_usbnet_skb_return (struct awl13_usbnet *dev, struct sk_buff *skb);
static struct net_device_stats* awl13_usbnet_get_stats (struct net_device *net);
static void awl13_usbnet_defer_kevent (struct awl13_usbnet *dev, int work);
static int  awl13_usbnet_stop (struct net_device *net);
static int  awl13_usbnet_open (struct net_device *net);
static void awl13_usbnet_tx_timeout (struct net_device *net);
static int  awl13_usbnet_start_xmit (struct sk_buff *skb, struct net_device *net);
static void awl13_usbnet_bh (unsigned long param);
static void awl13_usbnet_disconnect (struct usb_interface *intf);
static int  awl13_usbnet_probe (struct usb_interface *udev, const struct usb_device_id *prod);
static void awl13_usbnet_clean(struct awl13_usbnet *dev,
				struct usb_interface *intf);
static int awl13_tx_process(struct sk_buff *skb, struct awl13_usbnet *dev);
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
static wlan_wmm_ac_e awl13_wmm_nextqueue(struct awl13_thread *thread);
#endif

#define AWL13_MAC_BUF_SIZE	18
#if defined(DEBUG)
static int awl13_print_mac(char dest[AWL13_MAC_BUF_SIZE],
			    const unsigned char addr[ETH_ALEN]);
#endif /* defined(DEBUG) */

struct usb_host_endpoint *tmp_int_in=NULL;
#ifdef AWL13_HOTPLUG_FIRMWARE
static int awl13_usb_download_fw(struct usb_interface *intf,
				  const char *filename);
#endif /* AWL13_HOTPLUG_FIRMWARE */


/* global function and variable */
/******************************************************************************
 * awl13_align_chunk_size -
 *
 *****************************************************************************/
/* Tx mode between  "fixed length mode" or "Variable length mode */
#define DS_VERSION (2) /* set (1) or (2) */

unsigned int
awl13_align_chunk_size(unsigned int size)
{
#if (DS_VERSION==1)
	return (((size / AWL13_BU1805GU_DMA_SIZE) * AWL13_BU1805GU_DMA_SIZE) +
		((size % AWL13_BU1805GU_DMA_SIZE) ? AWL13_BU1805GU_DMA_SIZE : 0) +
		AWL13_TERM_SIZE );
#elif (DS_VERSION==2)
	return ( 1536 );
#else
	"ERROR" /* To make a compilation error happened */
#endif
}

/******************************************************************************
 * awl13_wid_status -
 *
 *****************************************************************************/
static char*
awl13_wid_status(uint8_t type, uint8_t code)
{
	switch(code){
	case WID_STAT_WRN_FW_TYPE_MISMATCH:
		return WID_STAT_NAME_WRN_FW_TYPE_MISMATCH;
	case WID_STAT_ERR_PACKET_ERROR:
		return WID_STAT_NAME_ERR_PACKET_ERROR;
	case WID_STAT_ERR_INTERNAL_ERROR:
		return WID_STAT_NAME_ERR_INTERNAL_ERROR;
	case WID_STAT_ERR_FW_CHECKSUM_ERROR:
		return WID_STAT_NAME_ERR_FW_CHECKSUM_ERROR;
	case WID_STAT_ERR_ILLEGAL_FWID:
		return WID_STAT_NAME_ERR_ILLEGAL_FWID;
	case WID_STAT_ERR_DATA_ERROR:
		return WID_STAT_NAME_ERR_DATA_ERROR;
	case WID_STAT_ERR_FCS_ERROR:
		return WID_STAT_NAME_ERR_FCS_ERROR;
	case WID_STAT_ERR_MTYPE_INCORRECT:
		return WID_STAT_NAME_ERR_MTYPE_INCORRECT;
	case WID_STAT_ERR_WID_INCORRECT:
		return WID_STAT_NAME_ERR_WID_INCORRECT;
	case WID_STAT_ERR_MSG_LENGTH:
		return WID_STAT_NAME_ERR_MSG_LENGTH;
	case WID_STAT_ERR_ILLEGAL_SEQNO:
		return WID_STAT_NAME_ERR_ILLEGAL_SEQNO;
	case WID_STAT_ERR:
		return WID_STAT_NAME_ERR;
	case WID_STAT_NG:
		return (type == CTYPE_INFO) ?
			WID_STAT_NAME_DISCONNECTED : WID_STAT_NAME_NG;
	case WID_STAT_OK:
		return (type == CTYPE_INFO) ?
			WID_STAT_NAME_CONNECTED : WID_STAT_NAME_OK;
	default:
		break;
	}
	return WID_STAT_NAME_UNKNOWN;
}

/******************************************************************************
 * awl13_wid_check -
 *
 *****************************************************************************/
static int
awl13_wid_check(struct awl13_private *priv, struct awl13_packet *res )
{
	if (res->h.type == HTYPE_CONFIG_RES) {
		struct awl13_wid_frame *wframe = to_wid_frame(res);
		if( wframe->wid == WID_STATUS){
			if ((priv->fw_ready_check != 0) &&
			    ((signed char)wframe->val[0] == -11)) {
				priv->fw_not_ready = 1;
			}
			else {
				awl_info("WID_STATUS CODE=%d (%s)\n", 
					 (signed char)wframe->val[0],
					 awl13_wid_status(res->m.type,
							   wframe->val[0]));
			}
		}
		else {
			awl_debug("check_wps_msg(usbBuffQue) called l.%d\n",
				 __LINE__);
			check_wps_msg(priv, res->message); /* usbBuffQue in windows */
		}
		if (res->m.type == CTYPE_RESPONSE) {
			memcpy(priv->wid_response,
			       res, res->h.len + 2);
		}
		if (res->m.type == CTYPE_INFO) {
			if (wframe->wid == WID_STATUS){
    			if (res->message[7] == 0x0) {
    				awl_info("disconnected!\n");
    				//netif_stop_queue(priv->netdev);
    				//netif_carrier_off(priv->netdev);
    			} else {
				if (memcmp(priv->iwap.ap_addr.sa_data,
					   priv->bssid, ETH_ALEN)) {
					memcpy(priv->iwap.ap_addr.sa_data,
					       priv->bssid, ETH_ALEN);
					priv->iwap.ap_addr.sa_family =
						ARPHRD_ETHER;
					wireless_send_event(priv->netdev,
							    SIOCGIWAP,
							    &priv->iwap, NULL);
				}
    				awl_info("connected!\n");
    				//netif_carrier_on(priv->netdev);
    				//netif_wake_queue(priv->netdev);
    			}
   			} else if (wframe->wid == WID_DEVICE_READY){
				priv->fw_not_ready = 0;
				atomic_set(&priv->state, AWL13_STATE_READY);
				awl_info("device ready!\n");
   			} else if (wframe->wid == WID_WAKE_STATUS){
    			if (res->message[7] == 0x0) {
					atomic_set(&priv->suspend_state, AWL13_PREPARE_SUSPENDED);
					complete(&priv->pmthr.complete);
    				awl_develop2("WID_WAKE_STATUS(sleep=%u)\n", jiffies);
    			} else {
					atomic_set(&priv->suspend_state, AWL13_IDLE);
					atomic_set(&priv->resume_state, AWL13_IDLE);
					complete(&priv->pmact.complete);
    				awl_develop2("WID_WAKE_STATUS(active=%u)\n", jiffies);
    			}
			}
		}
		if (res->m.type == CTYPE_NETWORK) {
			struct awl13_wid_frame *wframe = to_wid_frame(res);
			awl_info("Network Info! [0x%04x]\n", wframe->wid);
			awl_dump( &(res->m.body[4]), (res->m.len-8) );
		}
	}
	else{
		awl_err("WID IN unknown packet recv.(type=%x)\n", res->h.type );
		return -1;
	}
	return (int)res->m.type;
}


/******************************************************************************
 * awl13_wid_complete -
 *
 *****************************************************************************/
void 
awl13_wid_complete (struct urb *urb)
{
	struct awl13_private 	*priv = (struct awl13_private*)urb->context;
	struct awl13_packet    *res  = priv->rxbuf;
	unsigned long		flags;

	spin_lock_irqsave (&priv->wid_submit_lock, flags);

	if( (urb->pipe&USB_ENDPOINT_DIR_MASK)==USB_DIR_IN ){
		awl_develop("WID BULK-IN completion.\n");

		if(!priv->wid_urb){
			spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
			return;
		}
		priv->wid_urb = NULL;
		if (urb->status != 0) {
			awl_debug("It fails in WID BULK IN.(status=%d)\n", urb->status );

			awl_develop2("awl13_wid_complete (%d)(r=%x, s=%x, f=%x, t=%d)\n", 
						urb->status,
						atomic_read(&priv->resume_state), 
						atomic_read(&priv->suspend_state),
						atomic_read(&priv->force_active),
						priv->probe_flags.do_task_quit );

			if (urb) {
				usb_free_urb (urb);
				urb = NULL;
			}
			
			if (priv->probe_flags.do_task_quit) {
				spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
				goto do_complete;
			}
			WID_USB_RETRY(priv);

			spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
		}
		else{
			awl_debug("WID BULK-IN success! herdlen=%d, urblen=%d\n", res->h.len+2, urb->actual_length );
			awl_dump( (void*)res,  ((res->h.len+2)>16) ? 16 : (res->h.len+2) );

			if( awl13_wid_check( priv, res )==CTYPE_RESPONSE ){
				complete(&priv->wid_complete);
			}
			if (urb) {
				usb_free_urb (urb);
				urb = NULL;
			}
			spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
			awl13_do_task((struct work_struct*)&priv->wid_queue);

		}
	} else {
		awl_debug("WID BULK-OUT completion.\n");
		complete(&priv->wid_out_complete);
		spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
	}
	return;

do_complete:

	awl13_do_task((struct work_struct*)&priv->wid_queue);

	return;
}

/******************************************************************************
 * awl13_do_task -
 *
 *****************************************************************************/
static void
awl13_do_task(struct work_struct *work)
{
	struct awl13_usbnet		*dev;
	struct awl13_data    	*data;
	struct awl13_private 	*priv;
	unsigned long		flags;
	struct urb		*urb = NULL;
	int			ret = 0;

	priv = container_of((struct delayed_work*)work, struct awl13_private, wid_queue );

	/* task of WID BULK-IN stop */
	if (priv->probe_flags.do_task_quit) {
	    if (atomic_read(&priv->power_state) == AWL13_PWR_STATE_ACTIVE) {
		    /* completion for terminatrion of bind */
		    complete(&priv->wid_do_task_complete);
	    } 
	    return;
	}

	dev  = priv->usbnetDev;
	data = (struct awl13_data *)dev->data;

	awl_develop("function start.(awl13_do_task)\n");

	while(1){
	        /* WID receive */
		if (!(urb = usb_alloc_urb (0, GFP_ATOMIC))) {
			awl_err("It fails in allocation. (URB-IN)\n");
			break;
		}
		usb_fill_bulk_urb (urb, dev->udev, usb_rcvbulkpipe(dev->udev, data->wid_bulk_in),
				   priv->rxbuf, sizeof(struct awl13_packet),
				   awl13_wid_complete, priv);

		spin_lock_irqsave (&priv->wid_submit_lock, flags);
		ret = usb_submit_urb (urb, GFP_ATOMIC);
		if(ret!=0){
			awl_err("It fails in submit.(URB-IN)(ret=%d)\n", ret );
			if (urb) {
				usb_free_urb (urb);
				urb = NULL;
			}
			spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
			break;
		}
		priv->wid_urb = urb;
		spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
		return;
	}
	WID_USB_RETRY(priv);
	return;
}

/******************************************************************************
 * awl13_wid_retry -
 *
 *****************************************************************************/
static void 
awl13_wid_retry (unsigned long param)
{
	struct awl13_private *priv;
	priv = (struct awl13_private *)param;

	if (schedule_delayed_work(&priv->wid_queue, 
				  msecs_to_jiffies(0)) != 1) {
		awl_err("%s: schedule_delayed_work failed\n", __func__);
	}
	return;
}

/******************************************************************************
 * awl13_task_init -
 *
 *****************************************************************************/
static void
awl13_task_init(struct awl13_private *priv)
{
	INIT_DELAYED_WORK(&priv->wid_queue, awl13_do_task);
	priv->wid_retry.function = awl13_wid_retry;
	priv->wid_retry.data = (unsigned long) priv;
	init_timer (&priv->wid_retry);
	init_completion(&priv->wid_do_task_complete);
	return;
}

/******************************************************************************
 * awl13_rx_fixup -
 *
 *****************************************************************************/
static int 
awl13_rx_fixup(struct awl13_usbnet *dev, struct sk_buff *skb)
{
	struct awl13_packet_header  	packet_head;

#if 1
	awl_develop("function start.(awl13_rx_fixup)(len=%d)\n", skb->len );
	awl_devdump( (void*)skb->data, skb->len );
#else
	awl_develop2("function start.(awl13_rx_fixup)(len=%d)\n", skb->len );
	awl_devdump2( (void*)skb->data, (skb->len<32)?skb->len:32 );
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	skb_copy_from_linear_data( skb, &packet_head, sizeof(packet_head) );
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	skb_copy_bits(skb, 0, &packet_head, sizeof(packet_head));
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

	skb_pull(skb, sizeof(packet_head) );

	// data only
	if (unlikely(packet_head.type != HTYPE_DATA_IN)) {
		awl_err("receive type is abnormal.(0x%x)\n", packet_head.type);
		return 0;
	}
	if (unlikely(packet_head.len > ETH_FRAME_LEN)) {
		awl_err("receive size is abnormal.(%d)\n", packet_head.len);
		return 0;
	}
	skb_trim(skb, packet_head.len);
	return 1;
}

/******************************************************************************
 * awl13_tx_fixup -
 *
 *****************************************************************************/
static struct sk_buff* 
awl13_tx_fixup(struct awl13_usbnet *dev, struct sk_buff *skb,
					gfp_t flags)
{
	struct awl13_packet_header  	packet_head;
	unsigned int			size;
	struct awl13_data    		*data = (struct awl13_data *)dev->data;
	struct awl13_private 		*priv = data->priv;

#if 1
	awl_develop("function start.(awl13_tx_fixup)(len=%d)\n", skb->len );
	awl_devdump( (void*)skb->data, skb->len );
#else
	awl_develop2("function start.(awl13_tx_fixup)(len=%d)\n", skb->len );
	awl_devdump2( (void*)skb->data, (skb->len<32)?skb->len:32 );
#endif

	if (unlikely(atomic_read(&priv->power_state) ==
		     AWL13_PWR_STATE_SLEEP ||	
		     atomic_read(&priv->state) == AWL13_STATE_NO_FW)){
		dev_kfree_skb_any(skb);
		return NULL;
	}

	size = awl13_align_chunk_size(sizeof(packet_head) + skb->len);
	if (skb_headroom(skb) < sizeof(packet_head) ||
	    skb_tailroom(skb) < size - (sizeof(packet_head) + skb->len)) {
		if (unlikely(pskb_expand_head
			     (skb, SKB_DATA_ALIGN(sizeof(packet_head)),
			      size - (sizeof(packet_head) + skb->len),
			      GFP_ATOMIC))) {
			awl_err("It fails in allocation. (skb)\n");
			dev_kfree_skb_any(skb);
			return NULL;
		}
	}

	packet_head.len  = skb->len;
	packet_head.type = HTYPE_DATA_OUT;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	skb_push(skb, sizeof(packet_head));
	skb_copy_to_linear_data(skb, &packet_head, sizeof(packet_head));
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	memcpy(skb_push(skb, sizeof(packet_head)), &packet_head,
	       sizeof(packet_head));
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

	skb_put(skb, size - skb->len);

	return skb;
}


/******************************************************************************
 * awl13_tx_thread -
 *
 *****************************************************************************/
static int
awl13_tx_thread(void *thr)
{
	wait_queue_t wait;
	struct awl13_private 	*priv = NULL;
	struct sk_buff *skb;
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
	wlan_wmm_ac_e ac; 
#endif
	unsigned long		flags;
	
	
	struct awl13_thread *thread = thr;
	if (thr)
		priv = thread->priv;
	
	else {
		awl_err("%s(thr=NULL) failed(l.%d)\n", __func__, __LINE__);
		return -1;
	}
		
	if (unlikely(priv == NULL)) {
		awl_err("priv=NULL failed(l.%d)\n", __LINE__);
		return -1;
	}
	
	awl13_activate_thread(thread);
	
	init_waitqueue_entry(&wait, current);
	
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
	if (awl13_wmm_init(priv) < 0) {
		awl_info("awl13_wmm_init(priv=%p) failed\n", priv);
		return -1;
	}
	srandom32((u32)jiffies);
#else
	spin_lock_init (&priv->skbq_lock);
	INIT_LIST_HEAD((struct list_head *) &priv->skbq);
#endif
	
	thread->flags = 0;
	
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&thread->waitQ, &wait);
		schedule();
		remove_wait_queue(&thread->waitQ, &wait);
		set_current_state(TASK_RUNNING);
		
		while (!kthread_should_stop()) {
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
			if ((ac = awl13_wmm_nextqueue(thread)) < 0) {
				awl_err("awl13_wmm_nextqueue(thread=%p) on %d\n", thread, ac);
				break;
			}
			
			if (ac == PND_Q) {
				awl_debug("%s():: Queues are empty!", __func__);
				break;
			}
			spin_lock_irqsave (&priv->wmm.wmm_skbq_lock, flags);
			skb = priv->wmm.txSkbQ[ac].skbuf.next;
			list_del((struct list_head *) priv->wmm.txSkbQ[ac].skbuf.next);
			spin_unlock_irqrestore(&priv->wmm.wmm_skbq_lock, flags);
#else
			if (list_empty((struct list_head *) &priv->skbq)) {
				awl_debug("%s():: Queues are empty!", __func__);
				break;
			}
			spin_lock_irqsave (&priv->skbq_lock, flags);
			skb = priv->skbq.next;
			list_del((struct list_head *) priv->skbq.next);
			spin_unlock_irqrestore(&priv->skbq_lock, flags);
#endif
			awl13_to_idle( priv );
			if( awl13_tx_process(skb, priv->usbnetDev) ){
				atomic_dec(&priv->force_active);
			}
		}
	}
	awl_debug("main-thread: break from main thread\n");
	awl13_deactivate_thread(thread);
	return 0;
}

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
/******************************************************************************
 * awl13_wmm_compute_txprob -
 *
 *****************************************************************************/
/* This function computes Tx prob and Tx priority for each queue depending on*/
/* the EDCA parameters                                                       */
void awl13_wmm_compute_txprob(struct awl13_private *priv)
{
	struct awl13_wmm *wmm = &priv->wmm;
	edca_param_t *edca_params = wmm->edca_params;
	ACCESS_CATEGORIES_T *priority = wmm->edca_priority;
	unsigned int *txprob = wmm->edca_txprob;
	edca_state_t edca_cs[NUM_AC] = {{0}};
	unsigned char slot_assigned = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int ac = 0;
	unsigned int total_numtx = 0;
	unsigned int total_numtxop = 0;
	unsigned int txop_prob[NUM_AC] = {0};

	awl_develop("ComputeTxProb In");

	/* Initialize the Back-off counter */
	for(ac = 0; ac < NUM_AC; ac++)
	{
		edca_cs[ac].cw   = (1 << edca_params[ac].ecwmin);
		edca_cs[ac].bcnt = (random32() & (edca_cs[ac].cw - 1)) +
				edca_params[ac].aifsn;

/* This is the number of frames that can be txd in the TXOP. This is */
/* actually a complex problem depending upon the packet length & TX  */
/* Rate. A highly simplified approach is adopted here by using a     */
/* striaght forward Time to Pkts conversion factor. Some  Pkt  */
/* Length & TX-Rates are used for determining this Conv-Factor.      */
		edca_cs[ac].nftxp = max((unsigned int)1,
			edca_params[ac].txop/TXOP_TO_PACKET_CONV_FACTOR);
	}

	for(i = 0; i < NUM_SLOTS; i++)
	{
		slot_assigned = 0;
		for(ac = 0; ac < NUM_AC; ac++)
		{
			if(edca_cs[ac].bcnt == 0)
			{
				if(slot_assigned == 0)
				{
					slot_assigned = 1;
				/* Congratulations!! You have won a TXOP */
					edca_cs[ac].numtx += edca_cs[ac].nftxp;
					edca_cs[ac].ntxopwon++;
					edca_cs[ac].cw = (1 << edca_params[ac].ecwmin);
				}
				else
				{
/* Internal Collision. A higher priority queue has already won the TXOP  . */
/* The CW parameters of the LP are updated as if its an external collision */
/* Note that the relative priorities of the queues is fixed here to:       */
/* AC_VO > AC_VI > AC_BE > AC_BK                                           */
					edca_cs[ac].numcol++;

					edca_cs[ac].cw = 2 * edca_cs[ac].cw;
					if(edca_cs[ac].cw > 
					   (1 << edca_params[ac].ecwmax))
						edca_cs[ac].cw =
						 (1 << edca_params[ac].ecwmax);
				}
				edca_cs[ac].bcnt =
					(random32() & (edca_cs[ac].cw - 1)) +
					edca_params[ac].aifsn;
			}
			else
			{
				edca_cs[ac].bcnt--;
			}
		}
	}

	for(ac = 0; ac < NUM_AC; ac++)
	{
		total_numtx += edca_cs[ac].numtx;
		total_numtxop += edca_cs[ac].ntxopwon;
	}

	if(total_numtx == 0)
	{
		/* Exception1 */
		awl_err("Error::ComputeTxProb Exc2");
		return;
	}

	/* The TX-Probabilities for each of the Queues is computed. This can */
	/* be used for making the scheduling decisions.                      */
	for(ac = 0; ac < NUM_AC; ac++)
	{
		txprob[ac]  = (100 * edca_cs[ac].numtx) / total_numtx;
	}

/* The Priority of the queue is decided based on probability of winning a TXOP*/
	for(ac = 0; ac < NUM_AC; ac++)
	{
		txop_prob[ac]  = (100 * edca_cs[ac].ntxopwon) / total_numtxop;
		priority[ac] = ac;
		for(i = 0; i < ac; i++)
		{
			if(txop_prob[ac] > txop_prob[i])
				break;
		}
		if(i < ac)
		{
			for(j = ac; j > i; j--)
				priority[j] = priority[j-1];
				priority[i] = ac;
			}
		}

	awl_develop("ComputeTxProb Out");
	return;
}



/* This function updates the Tx Prob and Tx Priority calculated in the driver */
/* context structure.                                                         */
void awl13_wmm_update_txparams(struct awl13_private *priv)
{
	struct awl13_wmm *wmm = &priv->wmm;
	ACCESS_CATEGORIES_T *priority  = wmm->edca_priority;
	unsigned int             *txprob    = wmm->edca_txprob;
	wlan_wmm_ac_e             QueueIndex = (AWL13_MAX_AC_QUEUES - 2);
	unsigned int             ac         = 0;
	unsigned long		flags;

	spin_lock_irqsave (&priv->wmm.wmm_skbq_lock, flags);

	wmm->QScheduleProb[AC_BK_Q] = txprob[AC_BK];
	wmm->QScheduleProb[AC_BE_Q] = txprob[AC_BE];
	wmm->QScheduleProb[AC_VI_Q] = txprob[AC_VI];
	wmm->QScheduleProb[AC_VO_Q] = txprob[AC_VO];

	for(ac = 0; ac < NUM_AC; ac++)
	{
		wlan_wmm_ac_e QueueType;

		if(priority[ac] == AC_VO)
			QueueType = AC_VO_Q;
		else if(priority[ac] == AC_VI)
			QueueType = AC_VI_Q;
		else if(priority[ac] == AC_BE)
			QueueType = AC_BE_Q;
		else if(priority[ac] == AC_BK)
			QueueType = AC_BK_Q;
        else {
		awl_err("%s : Invalid queue type.(%d)\n", priority[ac]);
		QueueType = AC_BK_Q;
        }

		wmm->TxQPriority[QueueIndex] = QueueType;
		QueueIndex--;
	}

	spin_unlock_irqrestore (&priv->wmm.wmm_skbq_lock, flags);
}


/******************************************************************************
 * awl13_wmm_nextqueue -
 *
 *****************************************************************************/
/* This function selects the NextQ for Tx in Round Robin manner starting from */
/* highest priority AC. Till a high priority queue is becomes empty lower     */
/* priority queues are not scheduled.                                         */
static wlan_wmm_ac_e awl13_wmm_nextqueue(struct awl13_thread *thread)
{


	struct awl13_private 	*priv = NULL;
	struct awl13_wmm *wmm = NULL;
	wlan_wmm_ac_e QueueType    = PND_Q;
	unsigned int QueueIndex   = 0;
	wlan_wmm_ac_e *TxQPriority = NULL;
	unsigned long		flags;
	
	if (thread)
		priv = thread->priv;
	else {
		awl_err("%s(thread=NULL) failed(l.%d)\n", __func__, __LINE__);
		return -1;
	}
	if (unlikely(priv == NULL)) {
		awl_err("priv=NULL failed(l.%d)\n", __LINE__);
		return -1;
	}
	
	wmm = &priv->wmm;
	TxQPriority = wmm->TxQPriority;

	/* Currently the Queues are scheduled in a Round Robin manner */
	for(QueueIndex =
		    (AWL13_MAX_AC_QUEUES - 2); QueueIndex > 0 ; QueueIndex--)
	{
		spin_lock_irqsave (&priv->wmm.wmm_skbq_lock, flags);
		QueueType = TxQPriority[QueueIndex];
		if(!list_empty((struct list_head *)
			       &priv->wmm.txSkbQ[QueueType].skbuf)) {
			spin_unlock_irqrestore (&priv->wmm.wmm_skbq_lock,flags);
			return QueueType;
		}
		spin_unlock_irqrestore (&priv->wmm.wmm_skbq_lock, flags);
	}
	return PND_Q;
}
#endif

/******************************************************************************
 * awl13_unbind -
 *
 *****************************************************************************/
static void 
awl13_unbind(struct awl13_usbnet *dev, struct usb_interface *intf)
{
	unsigned long		flags;
	struct awl13_data    	*data = (struct awl13_data *)dev->data;
	struct awl13_private 	*priv = data->priv;
	int			i;
	int ret;
	awl_develop("function start.(awl13_unbind)\n");
	
	/* set flag for information of quit */
	priv->probe_flags.do_task_quit = 1;
	atomic_set(&priv->modstate, AWL13_MODSTATE_NONE);


	/* WID BULK_IN is running */
	for (i = 0; i < AWL13_WID_BUILK_CANCEL_REQ_RETRY_N; i++) {
		spin_lock_irqsave (&priv->wid_submit_lock, flags);
		if (priv->wid_urb == NULL) {
			spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
			break;
		}
		ret = usb_unlink_urb(priv->wid_urb);
		spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
		/* usb_unlink_urb() is success */
		if (ret == -EINPROGRESS)
			break;
		
		/* failure */
		awl_info("WID BULK_IN : usb_unlink_urb failed on %d\n", ret);
		msleep(100);
	}
	if (i >= AWL13_WID_BUILK_CANCEL_REQ_RETRY_N) {
		awl_err("WID BULK_IN : usb_unlink_urb failed %d times\n",
			AWL13_WID_BUILK_CANCEL_REQ_RETRY_N);
	}

	/* wake up tx_thread */
	wake_up_interruptible(&priv->txthr.waitQ);
	awl13_terminate_thread(&priv->txthr);

	awl13_terminate_thread(&priv->pmthr);
	awl13_terminate_thread(&priv->pmact);
	
	if (atomic_read(&priv->power_state) == AWL13_PWR_STATE_ACTIVE) {
		/*  wait completion */
		if ((ret =
		     wait_for_completion_interruptible(
			     &priv->wid_do_task_complete))) {
			awl_err("%s: "
				"wait_for_completion_interruptible"
				"(task_complete) failed on %d\n",
				__func__, ret);
		}
	}

	if (priv->txthr.pid) {
		if ((ret =
		     wait_for_completion_interruptible(&priv->txthr.complete)))
			{
				awl_err("%s: wait_for_completion_interruptible"
					"(tx_thr complete) failed on %d\n",
					__func__, ret);
			}
	}

	/* terminate */
	/* timer */
	del_timer (&priv->wid_retry);

	/* wid_queue */
	(void)cancel_delayed_work(&priv->wid_queue);
	/* we don't hold rtnl here ... */
	flush_scheduled_work();

	awl13_bind_clean(priv);
	return;
}

static void
awl13_bind_clean(struct awl13_private *priv)
{

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
	int	i;
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */

	if (priv == NULL)
		return;

	/* free fw_imagge */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	if(priv->fw_image) {
		kfree(priv->fw_image);
		priv->fw_image = NULL;
	}
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	for(i = 0; i < FIRMWARE_N_BLOCKS; i++) {
		if(priv->fw_image[i]) {
			kfree(priv->fw_image[i]);
			priv->fw_image[i] = NULL;
		}
	}
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

	/* free priv->rxbuf */
	if (priv->rxbuf) {
		kfree(priv->rxbuf);
		priv->rxbuf = NULL;
	}

	/* free priv->wid_request */
	if (priv->wid_request) {
		kfree(priv->wid_request);
		priv->wid_request = NULL;
	}

	/* free priv->wid_response */
	if (priv->wid_response) {
		kfree(priv->wid_response);
		priv->wid_response = NULL;
	}

	/* free priv */
	if(priv) {
		kfree(priv);
		priv = NULL;
	}
	return;
}

/******************************************************************************
 * awl13_bind -
 *
 *****************************************************************************/
static int 
awl13_bind(struct awl13_usbnet *dev, struct usb_interface *intf)
{
	struct awl13_data    	*data;
	struct awl13_private 	*priv = NULL;
	int status;
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
	int			i;
#endif

	awl_develop("function start.(awl13_bind)\n");

	data = (struct awl13_data *)dev->data;
	memset(data, 0x00, sizeof(struct awl13_data));

	if ((data->priv = kzalloc(sizeof(struct awl13_private),
				  GFP_KERNEL)) == NULL) {
		awl_err("It fails in allocation. (private)\n");
		return -ENOMEM;
	}

	if ((data->priv->wid_response = 
	     kzalloc(sizeof(struct awl13_packet), GFP_KERNEL)) == NULL) {
		awl_err("not allocate buffer wid_response\n");
		status = -ENOMEM;
		goto out;
	}

	if ((data->priv->wid_request = 
	     kzalloc(sizeof(struct awl13_packet), GFP_KERNEL)) == NULL) {
		awl_err("not allocate buffer wid_request\n");
		status = -ENOMEM;
		goto out1;
	}

	if ((data->priv->rxbuf =
	     kzalloc(sizeof(struct awl13_packet), GFP_KERNEL)) == NULL) {
		awl_err("not allocate buffer rxbuf\n");
		status = -ENOMEM;
		goto out2;
	}

	dev->status = tmp_int_in;

	priv =data->priv;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	if ((priv->fw_image = kzalloc(FIRMWARE_SIZE_MAX, GFP_KERNEL)) == NULL) {
		awl_err("It fails in allocation. (firmware)\n");
		status = -ENOMEM;
		goto out3;

	}
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	for(i = 0; i < FIRMWARE_N_BLOCKS; i++) {
		if ((priv->fw_image[i] = kzalloc(FIRMWARE_BLOCK_MAX_SIZE,
						 GFP_KERNEL)) == NULL) {
			awl_err("It fails in allocation. (firmware %d/%d)\n",
				i, FIRMWARE_N_BLOCKS);
			status = -ENOMEM;
			if (i > 0) {
				for (; i >= 0; i--) {
					kfree(priv->fw_image[i]);
				}
			}
			goto out3;
		}
	}
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

	priv->netdev    = dev->net;
	priv->usbnetDev = dev;
	priv->fw_size   = 0;

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
    /* Initialize Default WMM params */
	priv->wmm.QScheduleProb[PND_Q]   = TXOP_FOR_PND_Q   ;
	priv->wmm.QScheduleProb[AC_BK_Q] = TXOP_FOR_AC_BK_Q ;
	priv->wmm.QScheduleProb[AC_BE_Q] = TXOP_FOR_AC_BE_Q ;
	priv->wmm.QScheduleProb[AC_VI_Q] = TXOP_FOR_AC_VI_Q ;
	priv->wmm.QScheduleProb[AC_VO_Q] = TXOP_FOR_AC_VO_Q ;
	priv->wmm.QScheduleProb[HIP_Q]   = TXOP_FOR_HIP_Q   ;

	for(i = 0; i < AWL13_MAX_AC_QUEUES; i++) {
		/* Assign the Tx Priority to each queue */
		priv->wmm.TxQPriority[i]  = i;
	}

	/* Initialize Default EDCA Parameters */
	priv->wmm.edca_params[AC_VO].aifsn  = 2;
	priv->wmm.edca_params[AC_VO].ecwmin = 2;
	priv->wmm.edca_params[AC_VO].ecwmax = 3;
	priv->wmm.edca_params[AC_VO].txop   = 1504;

	priv->wmm.edca_params[AC_VI].aifsn  = 2;
	priv->wmm.edca_params[AC_VI].ecwmin = 3;
	priv->wmm.edca_params[AC_VI].ecwmax = 4;
	priv->wmm.edca_params[AC_VI].txop   = 3008;

	priv->wmm.edca_params[AC_BE].aifsn  = 3;
	priv->wmm.edca_params[AC_BE].ecwmin = 4;
	priv->wmm.edca_params[AC_BE].ecwmax = 10;
	priv->wmm.edca_params[AC_BE].txop   = 0;

	priv->wmm.edca_params[AC_BK].aifsn  = 7;
	priv->wmm.edca_params[AC_BK].ecwmin = 4;
	priv->wmm.edca_params[AC_BK].ecwmax = 10;
	priv->wmm.edca_params[AC_BK].txop   = 0;
#endif

	atomic_set(&priv->state, AWL13_STATE_NO_FW);
	atomic_set(&priv->power_state, AWL13_PWR_STATE_ACTIVE);
	atomic_set(&priv->power_mngt,  AWL13_PWR_MNGT_ACTIVE);
	atomic_set(&priv->suspend_state, AWL13_IDLE);
	atomic_set(&priv->resume_state, AWL13_IDLE);
	atomic_set(&priv->net_suspend_state, AWL13_IDLE);
	atomic_set(&priv->force_active, 0);
	spin_lock_init (&priv->pm_lock);
	atomic_set(&priv->modstate, AWL13_MODSTATE_NONE);
	sema_init(&priv->wid_req_lock, 1);
	init_completion(&priv->wid_complete);
	init_completion(&priv->wid_out_complete);
	awl13_task_init(priv);

	spin_lock_init (&priv->wid_submit_lock);

#ifdef AWL13_HOTPLUG_FIRMWARE
	sema_init(&priv->fw_lock, 1);

	if ((firmware == NULL) || (firmware[0] == 0))
		priv->firmware_file = 0;
	else 
		priv->firmware_file = firmware;

#endif /* AWL13_HOTPLUG_FIRMWARE */

        awl13_wireless_attach(priv->netdev);
	if((status = awl13_usbnet_get_endpoints(dev,intf)) < 0 ){
		awl_err("unknown endpoint.\n");
		goto out4;
	}

	if( data->wid_bulk_in != AWL13_EP_WIDIN    ||
	    data->wid_bulk_out != AWL13_EP_WIDOUT  ||
	    dev->in != AWL13_EP_DATIN              ||
	    dev->out != AWL13_EP_DATOUT ){
		awl_err("unknown endpoint number.\n");
		status = -1;
		goto out4;
	}

	priv->txthr.priv = priv;
	init_waitqueue_head(&priv->txthr.waitQ);
	init_completion(&priv->txthr.complete);
	awl13_create_thread(awl13_tx_thread, &priv->txthr, "awl13_txthr");
	if (schedule_delayed_work(&priv->wid_queue,
				  msecs_to_jiffies(0)) != 1) {
		awl_err("%s: schedule_delayed_work failed\n", __func__);
		return -ESRCH;
	}
	priv->probe_flags.do_task_quit = 0;

	priv->pmthr.priv = priv;
	priv->pmact.priv = priv;
	init_completion(&priv->pmthr.complete);
	init_completion(&priv->pmact.complete);
	awl13_create_thread(awl13_pm_thread, &priv->pmthr, "awl13_pmthr");
	return 0;


out4:
out3:
out2:
out1:
out:
	awl13_bind_clean(priv);

	return status;
}

/******************************************************************************
 * awl13_get_port_status  -
 *
 *****************************************************************************/
static void 
awl13_ctl_complete(struct urb *urb)
{
	struct awl13_private 	*priv = (struct awl13_private*)urb->context;
	urb->dev->parent = priv->parent;
	usb_free_urb(urb);
	return;
}

/******************************************************************************
 * awl13_get_port_status  -
 *
 *****************************************************************************/
static int
awl13_port_status( struct awl13_private *priv, 
						unsigned short type,
						unsigned short val,
                        unsigned short* psta )
{
	int							ret;
    struct usb_ctrlrequest* 	ctrReq;
//	unsigned long				flags;
	struct urb					*urb = NULL;
	struct awl13_usbnet* 		dev = priv->usbnetDev;

	ctrReq = (struct usb_ctrlrequest*)&priv->port_status;

	if (!(urb = usb_alloc_urb (0, GFP_ATOMIC))) {
		awl_err("It fails in allocation. (URB-CONTROL)\n");
		return -1;
	}
	
//	spin_lock_irqsave (&priv->pm_lock, flags);
	ctrReq->bRequestType	= (type >> 8) & 0xFF;
	ctrReq->bRequest		= type & 0xFF;
 	ctrReq->wValue			= val;
	ctrReq->wIndex			= 1;
	ctrReq->wLength			= 2;
	
	usb_fill_control_urb(urb, dev->udev,
			 			 usb_sndctrlpipe(dev->udev, 0), 
						(char *) (void*)ctrReq,
						(char *) (void*)ctrReq,
						sizeof(unsigned short), 
						awl13_ctl_complete, priv );
	
	priv->parent = urb->dev->parent;
	urb->dev->parent = 0;

	ret = usb_submit_urb (urb, GFP_ATOMIC);
//	spin_unlock_irqrestore (&priv->pm_lock, flags);
	if( ret!=0 ) {
		awl_err("usb_submit_urb(URB-CONTROL) error. (0x%x,%d)\n", type, ret );
		return -1;
	}
	if(psta)
		*psta = priv->port_status;
	return 0;
}

/******************************************************************************
 * awl13_to_idle  -
 *
 *****************************************************************************/
void
awl13_to_idle( struct awl13_private *priv )
{
	int		loop;

	atomic_inc(&priv->force_active);

	if( atomic_read(&priv->power_mngt)==AWL13_PWR_MNGT_ACTIVE )
    	return;

RESUME_RETRY:

	for(loop=0; loop<USB_GET_STAT_TMO; loop++){
		if( atomic_read(&priv->resume_state)==AWL13_IDLE ) 
			break;
		USB_PM_WAIT(priv);
	}
	if( atomic_read(&priv->resume_state)!=AWL13_IDLE ) {
		awl_develop2("idle wait1.(r=%x, s=%x, f=%x)\n", 
					atomic_read(&priv->resume_state), 
					atomic_read(&priv->suspend_state),
					atomic_read(&priv->force_active) );
		goto WID_IN_RESTART;
	}


	switch( atomic_read(&priv->suspend_state) ) {
		case AWL13_IDLE:
		case AWL13_PREPARE_SUSPENDED:
			break;

		case AWL13_EXECUTE_SUSPENDED:
			while( atomic_read(&priv->suspend_state)!=AWL13_SUSPENDED ){
				awl_debug("idle wait2.(r=%x, s=%x, f=%x)\n", 
							atomic_read(&priv->resume_state), 
							atomic_read(&priv->suspend_state),
							atomic_read(&priv->force_active) );
				USB_PM_WAIT(priv);
			}
			break;

		case AWL13_SUSPENDED:
			atomic_set(&priv->resume_state, AWL13_PREPARE_RESUME );
			msleep(1);
			awl13_resume(priv, AWL13_ON );
			break;
	}

	for(loop=0; loop<USB_GET_STAT_TMO; loop++){
		if( atomic_read(&priv->suspend_state)==AWL13_IDLE || 
			atomic_read(&priv->suspend_state)==AWL13_PREPARE_SUSPENDED )
			break;
		USB_PM_WAIT(priv);
	}

	if( atomic_read(&priv->suspend_state)!=AWL13_IDLE && 
		atomic_read(&priv->suspend_state)!=AWL13_PREPARE_SUSPENDED ){
		awl_develop2("idle retry.(r=%x, s=%x, f=%x)\n", 
					atomic_read(&priv->resume_state), 
					atomic_read(&priv->suspend_state),
					atomic_read(&priv->force_active) );
		goto WID_IN_RESTART;
	}
	return;

WID_IN_RESTART:
	mod_timer (&priv->wid_retry, jiffies +msecs_to_jiffies(0) );
	goto RESUME_RETRY;
}


/******************************************************************************
 * awl13_suspend  -
 *
 *****************************************************************************/
int
awl13_suspend( struct awl13_private *priv )
{
	int						i;
	unsigned long			flags;
	int						wait;
	unsigned short 			status = 0;
	struct awl13_usbnet* 	dev = priv->usbnetDev;
	int						ret = 0;

	if (netif_running (dev->net)) {
		atomic_set(&priv->net_suspend_state, AWL13_SUSPENDED);
		ret = awl13_usbnet_stop(dev->net);
		if( ret != 0 ){
			awl_err("AWL13 suspend :USB/Network stop error\n");
			return -1;
		}
	}

	priv->probe_flags.do_task_quit = 1;

	/* WID BULK_IN is running */
	for (i = 0; i < AWL13_WID_BUILK_CANCEL_REQ_RETRY_N; i++) {
		spin_lock_irqsave (&priv->wid_submit_lock, flags);
		if (priv->wid_urb == NULL) {
			spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
			break;
		}
		ret = usb_unlink_urb(priv->wid_urb);
		spin_unlock_irqrestore (&priv->wid_submit_lock, flags);
		/* usb_unlink_urb() is success */
		if (ret == -EINPROGRESS)
			break;
		
		/* failure */
		awl_info("AWL13 suspend :usb_unlink_urb failed on %d\n", ret);
		msleep(100);
	}
	if (i >= AWL13_WID_BUILK_CANCEL_REQ_RETRY_N) {
		awl_err("AWL13 suspend :usb_unlink_urb failed %d times\n",
		AWL13_WID_BUILK_CANCEL_REQ_RETRY_N);
		return -1;
	}

SUSPEND_SET:

	if( awl13_port_status( priv, 
							SetPortFeature,
							USB_PORT_FEAT_SUSPEND,
                        	NULL ) != 0 ){
		awl_err("AWL13 suspend : setting port stats error.\n");
		return -1;
	}

	for( wait=0; wait<USB_GET_STAT_TMO; wait++ ){
		if( awl13_port_status( priv, 
								GetPortStatus,
								0,
	                        	&status ) != 0 ){
			awl_err("AWL13 suspend : get port stats error.\n");
			return -1;
		}

		if( (status & USB_PORT_STAT_SUSPEND) 	&&
			dev->rxq.qlen==0					&&
			dev->txq.qlen==0					)
			break;
		USB_PM_WAIT(priv);
	}
	if( wait >= USB_GET_STAT_TMO ) {
		awl_err("AWL13 suspend : port stats error.(stat=0x%x)\n", status );
		USB_PM_WAIT(priv);
		goto SUSPEND_SET;
	}

	atomic_set(&priv->suspend_state, AWL13_SUSPENDED);
	return 0;
}

/******************************************************************************
 * awl13_resume  -
 *
 *****************************************************************************/
int
awl13_resume( struct awl13_private *priv, int flag )
{
	int						wait;
	struct awl13_usbnet* 	dev = priv->usbnetDev;
	int						ret = 0;
	unsigned short 			status = 0;

	if( flag == AWL13_ON ){

		if( awl13_port_status( priv, 
								ClearPortFeature,
								USB_PORT_FEAT_SUSPEND,
	                        	NULL ) != 0 ){
			awl_err("AWL13 resume : clear port stats error.\n");
			return -1;
		}

		for( wait=0; wait<USB_GET_STAT_TMO; wait++ ){
 			USB_PM_WAIT(priv);
			if( awl13_port_status( priv, 
									GetPortStatus,
									0,
		                        	&status ) != 0 ){
				awl_err("AWL13 resume : get port stats error.\n");
				return -1;
			}
			if( (status & USB_PORT_STAT_SUSPEND) == 0 )
				break;
		}
		if( wait > USB_GET_STAT_TMO ) {
			awl_err("AWL13 resume : port stats error.(stat=0x%x)\n", status );
			return -1;
		}
	}

	priv->probe_flags.do_task_quit = 0;
	mod_timer (&priv->wid_retry, jiffies +msecs_to_jiffies(0) );
	if( atomic_read(&priv->net_suspend_state) ) {
		ret = awl13_usbnet_open(dev->net);
		if( ret != 0 ){
			awl_err("AWL13 resume :USB/Network start error\n");
		}
		atomic_set(&priv->net_suspend_state, AWL13_IDLE);
	}
	return ret;
}

/******************************************************************************
 * awl13_pm_thread -
 *
 *****************************************************************************/
static int
awl13_pm_thread(void *thr)
{
	int 					timeout;
	struct awl13_private 	*priv   = NULL;
	struct awl13_thread 	*thread = thr;
	unsigned short 			status = 0;
	struct awl13_usbnet* 	dev;

	if (thr)
		priv = thread->priv;
	else {
		awl_err("%s(thr=NULL) failed(l.%d)\n", __func__, __LINE__);
		return -1;
	}
	dev = priv->usbnetDev;
		
	if (unlikely(priv == NULL)) {
		awl_err("priv=NULL failed(l.%d)\n", __LINE__);
		return -1;
	}
	
	awl13_activate_thread(thread);

	do {
		timeout = wait_for_completion_interruptible_timeout(
				&priv->pmthr.complete,
				msecs_to_jiffies(100));

		awl_debug("awl13_pm_thread.(r=%x, s=%x, f=%x)\n", 
					atomic_read(&priv->resume_state), 
					atomic_read(&priv->suspend_state),
					atomic_read(&priv->force_active) );

		set_current_state(TASK_RUNNING);
		switch( atomic_read(&priv->suspend_state) ){
			case AWL13_IDLE:
				break;
			case AWL13_PREPARE_SUSPENDED:
				if( atomic_read(&priv->force_active)==AWL13_OFF ){
					atomic_set(&priv->suspend_state, AWL13_EXECUTE_SUSPENDED);
					if( awl13_suspend( priv ) != 0 ){
						atomic_set(&priv->suspend_state, AWL13_PREPARE_SUSPENDED);
						awl_err("AWL13 power management : suspend error\n");
						continue;
					}
					awl_develop2("sleep!(%u) \n", jiffies );
				}
				break;

			case AWL13_SUSPENDED:
				if( atomic_read(&priv->resume_state) == AWL13_IDLE) {
					if( awl13_port_status( priv, 
											GetPortStatus,
											0,
				                        	&status ) == 0 ){
						if( (status & USB_PORT_STAT_SUSPEND)== 0 ){
							atomic_set(&priv->resume_state, AWL13_PREPARE_WAKEUP );
							awl13_resume(priv, AWL13_OFF);
			   				awl_develop2("active!(%u)\n", jiffies);
						}
					}
					else
						awl_err("AWL13 power management : get port stats error.\n");
				}
				break;
		}
		set_current_state(TASK_INTERRUPTIBLE);
	} while (!kthread_should_stop());

	awl13_deactivate_thread(thread);

	return 0;
}

/******************************************************************************
 * awl13_last_init -
 *
 *****************************************************************************/
static int 
awl13_last_init(struct awl13_usbnet *dev )
{
	struct awl13_data    	*data;
	struct awl13_private 	*priv;
	int 			ret;
	struct usb_device	*xdev;
	char			*speed[5]={"UNKNOWN", "LOW", "FULL", "HIGH", "VARIABLE" };
	data = (struct awl13_data *)dev->data;
	priv =data->priv;

	awl_develop("function start.(awl13_last_init)\n");

	atomic_set(&priv->modstate, AWL13_MODSTATE_EXIST);
	xdev = interface_to_usbdev (dev->intf);
	priv->speed     = xdev->speed;

	awl_info("Driver Version %s %s\n", AWL13_VERSION,
#ifdef AWL13_HOTPLUG_FIRMWARE
		 "hotplug"
#else /* AWL13_HOTPLUG_FIRMWARE */
		 "normal"
#endif /* AWL13_HOTPLUG_FIRMWARE */
		 );

	awl_info("    Endpoint   : DATA-BULK-OUT=%d, WID-BULK-OUT=%d, DATA-BULK-IN=%d, WID-BULK-IN=%d\n",
		 dev->out, data->wid_bulk_out, dev->in, data->wid_bulk_in);
	if(dev->status){
		unsigned	epnum;
		epnum = dev->status->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
		awl_info("                 INT-IN=%d\n", epnum);
	}
	else{
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33)
	awl_info("    Bus Speed  : %s\n", (priv->speed>USB_SPEED_WIRELESS) ? 
                                         speed[USB_SPEED_UNKNOWN] : speed[priv->speed] ); 
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33) */
	awl_info("    Bus Speed  : %s\n", (priv->speed>USB_SPEED_VARIABLE) ? 
                                         speed[USB_SPEED_UNKNOWN] : speed[priv->speed] ); 
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33) */

	ret = awl13_sysfs_init(priv);
	if (ret) {
		awl_err("failed register sysfs-entry\n");
		return -1;
	}
	return 0;
}


/******************************************************************************
 * awl13_defer_bh -
 *
 *****************************************************************************/
static void 
awl13_defer_bh(struct awl13_usbnet *dev, struct sk_buff *skb, struct sk_buff_head *list)
{
	unsigned long		flags;

	awl_develop("function start.(awl13_defer_bh)\n");

	spin_lock_irqsave(&list->lock, flags);
	__skb_unlink(skb, list);

	spin_unlock_irqrestore(&list->lock, flags);
	spin_lock_irqsave(&dev->done.lock, flags);

	__skb_queue_tail(&dev->done, skb);
	if (dev->done.qlen > 0)
		tasklet_schedule(&dev->bh);
	spin_unlock_irqrestore(&dev->done.lock, flags);
}


/******************************************************************************
 * awl13_rx_submit -
 *
 *****************************************************************************/
static void 
awl13_rx_submit (struct awl13_usbnet *dev, struct urb *urb, gfp_t flags)
{
	struct sk_buff		*skb;
	struct awl13_skb_data		*entry;
	int			retval = 0;
	unsigned long		lockflags;
	size_t			size = dev->rx_urb_size;

	awl_develop("function start.(awl13_rx_submit)\n");

	if ((skb = alloc_skb (size + NET_IP_ALIGN, flags)) == NULL) {
		awl_err ("no rx skb\n");
		awl13_usbnet_defer_kevent (dev, AWL13_EVENT_RX_MEMORY);
		if (urb) {
			usb_free_urb (urb);
			urb = NULL;
		}
		return;
	}
	skb_reserve (skb, NET_IP_ALIGN);

	entry = (struct awl13_skb_data *) skb->cb;
	entry->urb = urb;
	entry->dev = dev;
	entry->state = awl13_rx_start;
	entry->length = 0;

	usb_fill_bulk_urb (urb, dev->udev, usb_rcvbulkpipe(dev->udev, dev->in),
		skb->data, size, awl13_rx_complete, skb);

	spin_lock_irqsave (&dev->rxq.lock, lockflags);

	if (netif_running (dev->net)
			&& netif_device_present (dev->net)
			&& !test_bit (AWL13_EVENT_RX_HALT, &dev->flags)) {
		switch (retval = usb_submit_urb (urb, GFP_ATOMIC)) {
		case -EPIPE:
			awl13_usbnet_defer_kevent (dev, AWL13_EVENT_RX_HALT);
			break;
		case -ENOMEM:
			awl13_usbnet_defer_kevent (dev, AWL13_EVENT_RX_MEMORY);
			break;
		case -ENODEV:
			awl_debug ("device gone\n");
			netif_device_detach (dev->net);
			break;
		default:
			awl_debug ("rx submit, %d\n", retval);
			tasklet_schedule (&dev->bh);
			break;
		case 0:
			__skb_queue_tail (&dev->rxq, skb);
		}
	} else {
		awl_debug ("rx: stopped\n");
		retval = -ENOLINK;
	}
	spin_unlock_irqrestore (&dev->rxq.lock, lockflags);
	if (retval) {
		if (skb) {
			dev_kfree_skb_any (skb);
			skb = NULL;
		}
		if (urb) {
			usb_free_urb (urb);
			urb = NULL;
		}
	}
}


/******************************************************************************
 * awl13_rx_process  -
 *
 *****************************************************************************/
static inline void 
awl13_rx_process (struct awl13_usbnet *dev, struct sk_buff *skb)
{

	awl_develop("function start.(awl13_rx_process )\n");

	if (!awl13_rx_fixup (dev, skb))
		goto error;
	// else network stack removes extra byte if we forced a short packet

	if (skb->len){
		awl13_usbnet_skb_return (dev, skb);
	}
	else {
		awl_err ("drop\n");
error:
		dev->stats.rx_errors++;
		skb_queue_tail (&dev->done, skb);
	}
}

/******************************************************************************
 * awl13_rx_complete  -
 *
 *****************************************************************************/
static void 
awl13_rx_complete (struct urb *urb)
{
	struct sk_buff		*skb = (struct sk_buff *) urb->context;
	struct awl13_skb_data		*entry = (struct awl13_skb_data *) skb->cb;
	struct awl13_usbnet		*dev = entry->dev;
	int			urb_status = urb->status;

	awl_develop("function start.(awl13_rx_complete)\n");

	skb_put (skb, urb->actual_length);
	entry->state = awl13_rx_done;
	entry->urb = NULL;

	if(urb_status)
		awl_develop2 ("awl13_rx_complete error (%d)\n", urb_status );

	switch (urb_status) {
	/* success */
	case 0:
		if (skb->len < dev->net->hard_header_len) {
			awl_err ("rx length error skb=%d, head=%d\n", skb->len, dev->net->hard_header_len );
			entry->state = awl13_rx_cleanup;
			dev->stats.rx_errors++;
			dev->stats.rx_length_errors++;
		}
		break;

	/* stalls need manual reset. this is rare ... except that
	 * when going through USB 2.0 TTs, unplug appears this way.
	 * we avoid the highspeed version of the ETIMEOUT/EILSEQ
	 * storm, recovering as needed.
	 */
	case -EPIPE:
		awl_debug ("awl13_rx_complete urb status error(%d)\n", urb_status );
		dev->stats.rx_errors++;
		awl13_usbnet_defer_kevent (dev, AWL13_EVENT_RX_HALT);
		// FALLTHROUGH

	/* software-driven interface shutdown */
	case -ECONNRESET:		/* async unlink */
	case -ESHUTDOWN:		/* hardware gone */
	case -EPROTO:
		awl_debug ("awl13_rx_complete urb status error(%d)\n", urb_status );
		goto block;

	/* we get controller i/o faults during khubd disconnect() delays.
	 * throttle down resubmits, to avoid log floods; just temporarily,
	 * so we still recover when the fault isn't a khubd delay.
	 */
	case -ETIME:
	case -EILSEQ:
		awl_debug ("awl13_rx_complete urb status error(%d)\n", urb_status );
		dev->stats.rx_errors++;
		if (!timer_pending (&dev->delay)) {
			mod_timer (&dev->delay, jiffies + THROTTLE_JIFFIES);
			awl_debug ("rx throttle %d\n", urb_status);
		}
block:
		entry->state = awl13_rx_cleanup;
		entry->urb = urb;
		urb = NULL;
		break;

	/* data overrun ... flush fifo? */
	case -EOVERFLOW:
		awl_debug ("awl13_rx_complete urb status error(%d)\n", urb_status );
		dev->stats.rx_over_errors++;
		// FALLTHROUGH

	default:
		awl_debug ("awl13_rx_complete urb status error(%d)\n", urb_status );
		entry->state = awl13_rx_cleanup;
		dev->stats.rx_errors++;
		break;
	}

	awl13_defer_bh(dev, skb, &dev->rxq);

	if (urb) {
		if (netif_running (dev->net)
				&& !test_bit (AWL13_EVENT_RX_HALT, &dev->flags)) {
			awl13_rx_submit (dev, urb, GFP_ATOMIC);
			return;
		}
		if (urb) {
			usb_free_urb (urb);
			urb = NULL;
		}
	}
}



/******************************************************************************
 * awl13_intr_complete  -
 *
 *****************************************************************************/

static void awl13_intr_complete (struct urb *urb)
{
	struct awl13_private 	*priv = (struct awl13_private*)urb->context;
	struct awl13_usbnet		*dev  = priv->usbnetDev;
	int			status = urb->status;

	awl_develop("function start.(awl13_intr_complete)\n");

	switch (status) {

	/* success */
	case 0:
		;
		;
		; /* Judgment of status */
		;
		;
		break;

	/* software-driven interface shutdown */
	case -ENOENT:		/* urb killed */
	case -ESHUTDOWN:	/* hardware gone */
		awl_debug ("awl13_intr_complete urb status error(%d)\n", status );
		return;

	/* NOTE:  not throttling like RX/TX, since this endpoint
	 * already polls infrequently
	 */
	default:
		awl_err ("awl13_intr_complete urb status error(%d)\n", status );
		break;
	}

	if (!netif_running (dev->net))
		return;

	memset(urb->transfer_buffer, 0, urb->transfer_buffer_length);
	status = usb_submit_urb (urb, GFP_ATOMIC);
	awl_debug ("intr resubmit --> %d", status);
}

/******************************************************************************
 * awl13_unlink_urbs  -
 *
 *****************************************************************************/
static int 
awl13_unlink_urbs (struct awl13_usbnet *dev, struct sk_buff_head *q)
{
	unsigned long		flags;
	struct sk_buff		*skb, *skbnext;
	int			count = 0;

	awl_develop("function start.(awl13_unlink_urbs)\n");

	spin_lock_irqsave (&q->lock, flags);
	for (skb = q->next; skb != (struct sk_buff *) q; skb = skbnext) {
		struct awl13_skb_data		*entry;
		struct urb		*urb;
		int			retval;

		entry = (struct awl13_skb_data *) skb->cb;
		urb = entry->urb;
		skbnext = skb->next;

		// during some PM-driven resume scenarios,
		// these (async) unlinks complete immediately
		retval = usb_unlink_urb (urb);
		if (retval != -EINPROGRESS && retval != 0)
			awl_err ("unlink urb err, %d\n", retval);
		else
			count++;
	}
	spin_unlock_irqrestore (&q->lock, flags);
	return count;
}

/******************************************************************************
 * awl13_kevent  -
 *
 *****************************************************************************/
static void 
awl13_kevent (struct work_struct *work)
{
	struct awl13_usbnet		*dev =
	container_of(work, struct awl13_usbnet, kevent);
	int			status;

	awl_develop("function start.(awl13_kevent)\n");

	/* usb_clear_halt() needs a thread context */
	if (test_bit (AWL13_EVENT_TX_HALT, &dev->flags)) {
		awl13_unlink_urbs (dev, &dev->txq);
		status = usb_clear_halt (dev->udev, usb_sndbulkpipe(dev->udev, dev->out));
		if (status < 0
				&& status != -EPIPE
				&& status != -ESHUTDOWN) {
			awl_err ( "can't clear tx halt, status %d\n",
					status);
		} else {
			clear_bit (AWL13_EVENT_TX_HALT, &dev->flags);
			if (status != -ESHUTDOWN)
				netif_wake_queue (dev->net);
		}
	}
	if (test_bit (AWL13_EVENT_RX_HALT, &dev->flags)) {
		awl13_unlink_urbs (dev, &dev->rxq);
		status = usb_clear_halt (dev->udev, usb_rcvbulkpipe(dev->udev, dev->in));
		if (status < 0
				&& status != -EPIPE
				&& status != -ESHUTDOWN) {
			awl_err ( "can't clear rx halt, status %d\n",
					status);
		} else {
			clear_bit (AWL13_EVENT_RX_HALT, &dev->flags);
			tasklet_schedule (&dev->bh);
		}
	}

	/* tasklet could resubmit itself forever if memory is tight */
	if (test_bit (AWL13_EVENT_RX_MEMORY, &dev->flags)) {
		struct urb	*urb = NULL;

		if (netif_running (dev->net))
			urb = usb_alloc_urb (0, GFP_KERNEL);
		else
			clear_bit (AWL13_EVENT_RX_MEMORY, &dev->flags);
		if (urb != NULL) {
			clear_bit (AWL13_EVENT_RX_MEMORY, &dev->flags);
			awl13_rx_submit (dev, urb, GFP_KERNEL);
			tasklet_schedule (&dev->bh);
		}
	}

	if (dev->flags)
		awl_debug ("awl13_kevent done, flags = 0x%lx\n", dev->flags);
}

/******************************************************************************
 * awl13_tx_complete  -
 *
 *****************************************************************************/
static void 
awl13_tx_complete (struct urb *urb)
{
	struct awl13_data    		*data;
	struct awl13_private 		*priv;
	struct sk_buff				*skb = (struct sk_buff *) urb->context;
	struct awl13_skb_data		*entry = (struct awl13_skb_data *) skb->cb;
	struct awl13_usbnet		*dev = entry->dev;
	data = (struct awl13_data *)dev->data;
	priv =data->priv;

	awl_develop("function start.(awl13_tx_complete)\n");

	if (urb->status == 0) {
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += entry->length;
	} else {
		dev->stats.tx_errors++;

		switch (urb->status) {
		case -EPIPE:
			awl_debug ("awl13_tx_complete urb status error(%d)\n", urb->status );
			awl13_usbnet_defer_kevent (dev, AWL13_EVENT_TX_HALT);
			break;

		/* software-driven interface shutdown */
		case -ECONNRESET:		// async unlink
		case -ESHUTDOWN:		// hardware gone
			awl_debug ("awl13_tx_complete urb status error(%d)\n", urb->status );
			break;

		// like rx, tx gets controller i/o faults during khubd delays
		// and so it uses the same throttling mechanism.
		case -EPROTO:
		case -ETIME:
		case -EILSEQ:
			awl_debug ("awl13_tx_complete urb status error(%d)\n", urb->status );
			if (!timer_pending (&dev->delay)) {
				mod_timer (&dev->delay,
					jiffies + THROTTLE_JIFFIES);
				awl_err ("tx throttle %d\n",
							urb->status);
			}
			netif_stop_queue (dev->net);
			break;
		default:
			awl_debug ("awl13_tx_complete urb status error(%d)\n", urb->status );
			awl_err ("tx err %d\n", entry->urb->status);
			break;
		}
	}

	urb->dev = NULL;
	entry->state = awl13_tx_done;
	awl13_defer_bh(dev, skb, &dev->txq);
	atomic_dec(&priv->force_active);
}

/******************************************************************************
 * awl13_usbnet_get_endpoints -
 *
 *****************************************************************************/
static int 
awl13_usbnet_get_endpoints(struct awl13_usbnet *dev, struct usb_interface *intf)
{
	int				tmp;
	struct usb_host_interface	*alt = NULL;
	struct usb_host_endpoint	*in = NULL, *out = NULL;
	struct usb_host_endpoint	*status = NULL;
	struct awl13_data    		*data;

	awl_develop("function start.(awl13_usbnet_get_endpoints)\n");

	data = (struct awl13_data *)dev->data;
	for (tmp = 0; tmp < intf->num_altsetting; tmp++) {
		unsigned	ep;
		unsigned	epnum;

		in = out = status = NULL;
		alt = intf->altsetting + tmp;

		awl_debug("awl13_usbnet_get_endpoints tmp=%d, al=%d, epnum=%d\n", tmp, intf->num_altsetting, alt->desc.bNumEndpoints );
		/* take the first altsetting with in-bulk + out-bulk;
		 * remember any status endpoint, just in case;
		 * ignore other endpoints and altsetttings.
		 */
		for (ep = 0; ep < alt->desc.bNumEndpoints; ep++) {
			struct usb_host_endpoint	*e;

			e = alt->endpoint + ep;
			awl_debug("awl13_usbnet_get_endpoints ep=%d, attr=%d\n", ep, e->desc.bmAttributes );
			epnum = e->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;

			switch (e->desc.bmAttributes) {
			case USB_ENDPOINT_XFER_INT:
				if (!usb_endpoint_dir_in(&e->desc)){
					awl_err("Interrupt endpoints error(OUT?)\n");
					return -EINVAL;
				}
				tmp_int_in  = e;
				awl_develop("EP_INTIN ep=%d\n", epnum);
				break;

			case USB_ENDPOINT_XFER_BULK:
				if (usb_endpoint_dir_in(&e->desc)) {
					switch (epnum) {
					case AWL13_EP_DATIN:
						awl_develop("EP_DATIN ep=%d\n", epnum);
						dev->in = epnum;
						break;
					case AWL13_EP_WIDIN:
						awl_develop("EP_WIDIN ep=%d\n", epnum);
						data->wid_bulk_in = epnum;
						break;
					default:
						awl_err("Bulk-IN endpoints error(EP number=%d)\n", epnum);
						break;
					}
				} else {
					switch (epnum) {
					case AWL13_EP_DATOUT:
						awl_develop("EP_DATOUT ep=%d\n", epnum);
						dev->out = epnum;
						break;
					case AWL13_EP_WIDOUT:
						awl_develop("EP_WIDOUT ep=%d\n", epnum);
						data->wid_bulk_out = epnum;
						break;
					default:
						awl_err("Bulk-OUT endpoints error(EP number=%d)\n", epnum);
						break;
					}
				}
				break;
			default:
				continue;
			}
		}
	}

	if (alt->desc.bAlternateSetting != 0
			|| !(dev->driver_info->flags & AWL13_FLAG_NO_SETINT)) {
		tmp = usb_set_interface (dev->udev, alt->desc.bInterfaceNumber,
				alt->desc.bAlternateSetting);
		if (tmp < 0)
			return tmp;
	}
	return 0;
}



/******************************************************************************
 * awl13_usbnet_init_status  -
 *
 *****************************************************************************/
static int
awl13_usbnet_init_status (struct awl13_usbnet *dev, struct usb_interface *intf)
{
	char			*buf = NULL;
	unsigned		pipe = 0;
	unsigned		maxp;
	unsigned		period;
	struct awl13_data    	*data;
	struct awl13_private 	*priv;
	data = (struct awl13_data *)dev->data;
	priv =data->priv;

	awl_develop("function start.(awl13_usbnet_init_status)\n");

	pipe = usb_rcvintpipe (dev->udev,
			dev->status->desc.bEndpointAddress
				& USB_ENDPOINT_NUMBER_MASK);

	maxp = usb_maxpacket (dev->udev, pipe, 0);

        /* BUG: It changes in v0.3.3 */
	/* avoid 1 msec chatter:  min 8 msec poll rate */
	period = max ((int) dev->status->desc.bInterval,
		(dev->udev->speed == USB_SPEED_HIGH) ? 7 : 3);

	awl_develop("Interrupt IN bInterval=%d\n", period );
	buf = kmalloc (maxp, GFP_KERNEL);
	if (buf) {
		dev->interrupt = usb_alloc_urb (0, GFP_KERNEL);
		if (!dev->interrupt) {
			if (buf) {
				kfree (buf);
				buf = NULL;
			}
			return -ENOMEM;
		} else {
			usb_fill_int_urb(dev->interrupt, dev->udev, pipe,
				buf, maxp, awl13_intr_complete, priv, period);
			awl_debug( "status ep%din, %d bytes period %d\n",
				usb_pipeendpoint(pipe), maxp, period);
		}
	}
	return 0;
}


/******************************************************************************
 * awl13_usbnet_skb_return -
 *
 *****************************************************************************/
static void 
awl13_usbnet_skb_return (struct awl13_usbnet *dev, struct sk_buff *skb)
{
	int	status;

	awl_develop("function start.(awl13_usbnet_skb_return)\n");

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
	skb->dev = dev->net;
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */
	skb->protocol = eth_type_trans (skb, dev->net);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;

	awl_debug ("skb_return: len %zu, type 0x%x\n",
			skb->len + sizeof (struct ethhdr), skb->protocol);
	memset (skb->cb, 0, sizeof (struct awl13_skb_data));
	status = netif_rx (skb);

}



/******************************************************************************
 * awl13_usbnet_get_stats -
 *
 *****************************************************************************/
static struct net_device_stats* 
awl13_usbnet_get_stats (struct net_device *net)
{
	struct awl13_usbnet	*dev = netdev_priv(net);
	awl_develop("function start.(awl13_usbnet_get_stats)\n");
	return &dev->stats;
}

/******************************************************************************
 * awl13_usbnet_defer_kevent -
 *
 *****************************************************************************/
static void 
awl13_usbnet_defer_kevent (struct awl13_usbnet *dev, int work)
{

	awl_develop("function start.(awl13_usbnet_defer_kevent)\n");

	set_bit (work, &dev->flags);
	if (!schedule_work (&dev->kevent))
		awl_err ("kevent %d may have been dropped\n", work);
	else
		awl_develop ("kevent %d scheduled\n", work);
}

/******************************************************************************
 * awl13_usbnet_stop  -
 *
 *****************************************************************************/
static int 
awl13_usbnet_stop (struct net_device *net)
{
	struct awl13_private 	*priv;
	struct awl13_data    	*data;
	struct awl13_usbnet	*dev = netdev_priv(net);
	int						temp;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK (unlink_wakeup);
	DECLARE_WAITQUEUE (wait, current);

	data = (struct awl13_data *)dev->data;
	priv = data->priv;

	awl_develop("function start.(awl13_usbnet_stop)\n");

	if( atomic_read(&priv->power_mngt)==AWL13_PWR_MNGT_ACTIVE ){
		netif_stop_queue (net);
	}

	awl_debug ( "stop stats: rx/tx %ld/%ld, errs %ld/%ld\n",
		dev->stats.rx_packets, dev->stats.tx_packets,
		dev->stats.rx_errors, dev->stats.tx_errors
		);

	// ensure there are no more active urbs
	add_wait_queue (&unlink_wakeup, &wait);
	dev->wait = &unlink_wakeup;
	temp = awl13_unlink_urbs (dev, &dev->txq) + awl13_unlink_urbs (dev, &dev->rxq);

	// maybe wait for deletions to finish.
	while (!skb_queue_empty(&dev->rxq)
			&& !skb_queue_empty(&dev->txq)
			&& !skb_queue_empty(&dev->done)) {
		msleep(UNLINK_TIMEOUT_MS);
		awl_debug ("waited for %d urb completions\n", temp);
	}
	dev->wait = NULL;
	remove_wait_queue (&unlink_wakeup, &wait);

	usb_kill_urb(dev->interrupt);

	/* deferred work (task, timer, softirq) must also stop.
	 * can't flush_scheduled_work() until we drop rtnl (later),
	 * else workers could deadlock; so make workers a NOP.
	 */
	dev->flags = 0;
	del_timer_sync (&dev->delay);
	tasklet_kill (&dev->bh);
	//usb_autopm_put_interface(dev->intf);
	return 0;
}

/******************************************************************************
 * awl13_usbnet_open  -
 *
 *****************************************************************************/
static int 
awl13_usbnet_open (struct net_device *net)
{
	struct awl13_usbnet		*dev = netdev_priv(net);
	int				retval = 0;
	//struct awl13_driver_info	*info = dev->driver_info;

	awl_develop("function start.(awl13_usbnet_open)\n");

	//if ((retval = usb_autopm_get_interface(dev->intf)) < 0) {
	//	awl_info ("resumption fail (%d) usbnet usb-%s-%s, %s\n",
	//		retval,
	//		dev->udev->bus->bus_name, dev->udev->devpath,
	//	info->description);
	//	goto done_nopm;
	//}
	netif_start_queue (net);
	{
		awl_debug ("open: enable queueing "
				"(rx %d, tx %d) mtu %d %s framing\n",
			(int)RX_QLEN (dev), (int)TX_QLEN (dev), dev->net->mtu,
			AWL13_DRVINFO);
	}

	// delay posting reads until we're fully open
	tasklet_schedule (&dev->bh);
	return retval;

//done_nopm:
//	return retval;
}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
/******************************************************************************
 * awl13_usb_suspend  -
 *
 *****************************************************************************/
static int
awl13_usbnet_suspend(struct usb_interface *intf, pm_message_t message)
{
	awl_info("%s() called\n", __func__);
	return 0;
}

/******************************************************************************
 * awl13_usb_resume  -
 *
 *****************************************************************************/
static int
awl13_usbnet_resume(struct usb_interface *intf)
{
	awl_info("%s() called\n", __func__);
	return 0;
}
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */
#ifdef CONFIG_PM
/******************************************************************************
 * awl13_usb_suspend  -
 *
 *****************************************************************************/
static int
awl13_usbnet_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct awl13_usbnet	*dev = usb_get_intfdata(intf);
	struct awl13_data	*data = (struct awl13_data *)dev->data;
	struct awl13_private	*priv = data->priv;
	int ret;

	awl_debug("%s() called\n", __func__);

	if (priv->txthr.task)
		awl13_terminate_thread(&priv->txthr);

	if (priv->txthr.pid) {
		wake_up_interruptible(&priv->txthr.waitQ);
		if ((ret =
		     wait_for_completion_interruptible(&priv->txthr.complete)))
			{
				awl_err("%s: wait_for_completion_interruptible"
					"(tx_thr complete) failed on %d\n",
					__func__, ret);
			}
	}

	priv->probe_flags.do_task_quit = 1;
	atomic_set(&priv->modstate, AWL13_MODSTATE_NONE);

	return 0;
}

/******************************************************************************
 * awl13_usb_resume  -
 *
 *****************************************************************************/
static int
awl13_usbnet_resume(struct usb_interface *intf)
{
	struct awl13_usbnet	*dev = usb_get_intfdata(intf);
	struct awl13_data	*data = (struct awl13_data *)dev->data;
	struct awl13_private	*priv = data->priv;

	awl_debug("%s() called\n", __func__);

	atomic_set(&priv->modstate, AWL13_MODSTATE_EXIST);
	priv->probe_flags.do_task_quit = 0;

	awl13_create_thread(awl13_tx_thread, &priv->txthr, "awl13_txthr");

	return 0;
}
#endif /* CONFIG_PM */
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */
/******************************************************************************
 * awl13_usbnet_tx_timeout  -
 *
 *****************************************************************************/
static void 
awl13_usbnet_tx_timeout (struct net_device *net)
{
	struct awl13_usbnet		*dev = netdev_priv(net);

	awl_develop("function start.(awl13_usbnet_tx_timeout)\n");

	awl13_unlink_urbs (dev, &dev->txq);
	tasklet_schedule (&dev->bh);

	// FIXME: device recovery -- reset?
}

/******************************************************************************
 * awl13_usbnet_start_xmit  -
 *
 *****************************************************************************/
static int 
awl13_usbnet_start_xmit (struct sk_buff *skb, struct net_device *net)
{
	struct awl13_usbnet		*dev = netdev_priv(net);
	struct awl13_data    	*data;
	struct awl13_private 	*priv;
	int			retval = NET_XMIT_SUCCESS;
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_WMM
	unsigned long		flags;
#endif

	data = (struct awl13_data *)dev->data;
	priv = data->priv;

	awl_develop("function start.(awl13_usbnet_start_xmit)\n");
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_WMM
	awl13_wmm_map_and_add_skb(priv, skb);
#else
	spin_lock_irqsave (&priv->skbq_lock, flags);
	list_add_tail((struct list_head *) skb,
		      (struct list_head *) &priv->skbq);
	spin_unlock_irqrestore (&priv->skbq_lock, flags);

	__net_timestamp(skb);
#endif
	
	wake_up_interruptible(&priv->txthr.waitQ);

	return retval;
}

/******************************************************************************
 * awl13_tx_process  -
 *
 *****************************************************************************/
static int
awl13_tx_process(struct sk_buff *skb, struct awl13_usbnet *dev)
{
	int			length;
	int			retval = NET_XMIT_SUCCESS;
	struct urb		*urb = NULL;
	struct awl13_skb_data		*entry;
	unsigned long		flags;

	skb = awl13_tx_fixup (dev, skb, GFP_ATOMIC);
	if (!skb) {
		awl_debug ("can't tx_fixup skb\n");
		goto drop;
	}
	length = skb->len;

	if (!(urb = usb_alloc_urb (0, GFP_ATOMIC))) {
		awl_err ("no urb\n");
		goto drop;
	}

	entry = (struct awl13_skb_data *) skb->cb;
	entry->urb = urb;
	entry->dev = dev;
	entry->state = awl13_tx_start;
	entry->length = length;

	usb_fill_bulk_urb (urb, dev->udev, usb_sndbulkpipe(dev->udev, dev->out),
			skb->data, skb->len, awl13_tx_complete, skb);

	spin_lock_irqsave (&dev->txq.lock, flags);

	switch ((retval = usb_submit_urb (urb, GFP_ATOMIC))) {
	case -EPIPE:
		awl_err("The transmission is abnormal.(submit status=%d)\n", retval );
		netif_stop_queue (dev->net);
		awl13_usbnet_defer_kevent (dev, AWL13_EVENT_TX_HALT);
		break;
	default:
		awl_err("The transmission is abnormal.(submit status=%d)\n", retval );
		break;
	case 0:
		awl_debug("awl13_usbnet_start_xmit success!\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
		dev->net->_tx->trans_start = jiffies;
#else
		dev->net->trans_start = jiffies;
#endif
		__skb_queue_tail (&dev->txq, skb);
		if (dev->txq.qlen >= TX_QLEN (dev))
			netif_stop_queue (dev->net);
	}
	spin_unlock_irqrestore (&dev->txq.lock, flags);

	if (retval) {
		awl_err ("drop, code %d\n", retval);
drop:
		retval = NET_XMIT_SUCCESS;
		dev->stats.tx_dropped++;
		if (skb) {
			dev_kfree_skb_any (skb);
			skb = NULL;
		}
		if (urb) {
			usb_free_urb (urb);
			urb = NULL;
		}
	}
	
	return retval;
}

/******************************************************************************
 * awl13_usbnet_bh  -
 *
 *****************************************************************************/
static void 
awl13_usbnet_bh (unsigned long param)
{
	struct awl13_usbnet		*dev = (struct awl13_usbnet *) param;
	struct sk_buff				*skb;
	struct awl13_skb_data		*entry;
	struct awl13_data			*data;
	struct awl13_private		*priv;

	awl_develop("function start.(awl13_usbnet_bh)\n");

	while ((skb = skb_dequeue (&dev->done))) {
		entry = (struct awl13_skb_data *) skb->cb;
		switch (entry->state) {
		case awl13_rx_done:
			entry->state = awl13_rx_cleanup;
			awl13_rx_process (dev, skb);
			continue;
		case awl13_tx_done:
		case awl13_rx_cleanup:
			awl_debug("awl13_usbnet_bh: %s.\n", (entry->state==awl13_tx_done) ? "tx_done" : "rx_cleanup" );
			if (entry->urb) {
				usb_free_urb (entry->urb);
				entry->urb = NULL;
			}
			if (skb) {
				dev_kfree_skb (skb);
				skb = NULL;
			}
			continue;
		default:
awl_debug ("bogus skb state %d\n", entry->state);
		}
	}


	data = (struct awl13_data *)dev->data;
	priv = data->priv;

	if( atomic_read(&priv->net_suspend_state) ) {
		return;
	}

	// waiting for all pending urbs to complete?
	if (dev->wait) {
		if ((dev->txq.qlen + dev->rxq.qlen + dev->done.qlen) == 0) {
			wake_up (dev->wait);
		}

	// or are we maybe short a few urbs?
	} else if (netif_running (dev->net)
			&& netif_device_present (dev->net)
			&& !timer_pending (&dev->delay)
			&& !test_bit (AWL13_EVENT_RX_HALT, &dev->flags)) {
		int	temp = dev->rxq.qlen;
		int	qlen = RX_QLEN (dev);

		if (temp < qlen) {
			struct urb	*urb;
			int		i;

			// don't refill the queue all at once
			for (i = 0; i < 10 && dev->rxq.qlen < qlen; i++) {
				urb = usb_alloc_urb (0, GFP_ATOMIC);
				if (urb != NULL) {
					awl13_rx_submit (dev, urb, GFP_ATOMIC);
				}
			}
			if (temp != dev->rxq.qlen )
				awl_debug ("rxqlen %d --> %d\n",
						temp, dev->rxq.qlen);

			if (dev->rxq.qlen < qlen)
				tasklet_schedule (&dev->bh);
		}
		if (dev->txq.qlen < TX_QLEN (dev))
			netif_wake_queue (dev->net);
	}
}



/******************************************************************************
 * awl13_usbnet_disconnect  -
 *
 *****************************************************************************/
static void 
awl13_usbnet_disconnect (struct usb_interface *intf)
{
	struct awl13_usbnet		*dev;
	struct usb_device	*xdev;
	struct net_device	*net;
	struct awl13_data		*data;
	struct awl13_private		*priv;

	awl_develop("function start.(awl13_usbnet_disconnect)\n");

	dev = usb_get_intfdata(intf);
	usb_set_intfdata(intf, NULL);
	if (!dev)
		return;

	xdev = interface_to_usbdev (intf);

	if(!intf->altsetting->desc.bInterfaceNumber){
		tmp_int_in = 0;
		return;
	}

	awl_debug ("unregister '%s' usb-%s-%s, %s\n",
		intf->dev.driver->name,
		xdev->bus->bus_name, xdev->devpath,
		dev->driver_info->description);

	net = dev->net;

	data = (struct awl13_data *)dev->data;
	priv = data->priv;

	awl13_usbnet_clean(dev, intf);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static const struct net_device_ops awl13_netdev_ops = {
	.ndo_open =			awl13_usbnet_open,
	.ndo_stop =			awl13_usbnet_stop,
	.ndo_start_xmit =		awl13_usbnet_start_xmit,
	.ndo_tx_timeout =		awl13_usbnet_tx_timeout,
	.ndo_get_stats =		awl13_usbnet_get_stats,
};
#endif

/******************************************************************************
 * awl13_usbnet_probe  -
 *
 *****************************************************************************/
static int
awl13_usbnet_probe (struct usb_interface *intf, const struct usb_device_id *prod)
{
	struct awl13_usbnet		*dev = NULL;
	struct net_device		*net;
	struct usb_host_interface	*interface;
	struct awl13_driver_info	*info;
	struct usb_device		*xdev;
	int				status;
	const char			*name;
	struct awl13_data		*data;
	struct awl13_private		*priv;

#if defined(DEBUG)
	char mac[AWL13_MAC_BUF_SIZE];
#endif /* defined(DEBUG) */

	awl_develop("function start.(awl13_usbnet_probe)\n");

	name = intf->dev.driver->name;
	info = (struct awl13_driver_info *) prod->driver_info;
	if (!info) {
		dev_dbg (&intf->dev, "blacklisted by %s\n", name);
		return -ENODEV;
	}

	xdev = interface_to_usbdev (intf);
	interface = intf->cur_altsetting;

	usb_get_dev (xdev);

	status = -ENOMEM;

	// set up our own records
	net = alloc_etherdev(sizeof(*dev));
	if (!net) {
		awl_err("can't kmalloc dev(=%u btyes)\n", sizeof(*dev));
		goto out;
	}

	dev = netdev_priv(net);
	dev->udev = xdev;
	dev->intf = intf;
	dev->driver_info = info;
	dev->driver_name = name;
	skb_queue_head_init (&dev->rxq);
	skb_queue_head_init (&dev->txq);
	skb_queue_head_init (&dev->done);
	dev->bh.func = awl13_usbnet_bh;
	dev->bh.data = (unsigned long) dev;
	INIT_WORK (&dev->kevent, awl13_kevent);
	dev->delay.function = awl13_usbnet_bh;
	dev->delay.data = (unsigned long) dev;
	init_timer (&dev->delay);
	//mutex_init (&dev->phy_mutex);

	dev->net = net;
	strcpy(net->name, "awlan%d");

	/* rx and tx sides can use different message sizes;
	 * bind() should set rx_urb_size in that case.
	 */
	dev->hard_mtu = (AWL13_BU1805GU_DMA_SIZE*3) + sizeof(struct awl13_packet_header);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
	net->get_stats = awl13_usbnet_get_stats;
	net->hard_start_xmit = awl13_usbnet_start_xmit;
	net->open = awl13_usbnet_open;
	net->stop = awl13_usbnet_stop;
	net->watchdog_timeo = TX_TIMEOUT_JIFFIES;
	net->tx_timeout = awl13_usbnet_tx_timeout;
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29) */
	net->netdev_ops = &awl13_netdev_ops;
	net->watchdog_timeo = TX_TIMEOUT_JIFFIES;
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29) */

	awl_develop("bInterfaceNumber=%d \n", intf->altsetting->desc.bInterfaceNumber );

	if(!intf->altsetting->desc.bInterfaceNumber){
		if((status = awl13_usbnet_get_endpoints(dev,intf)) < 0) {
			goto out1;
		}
		usb_set_intfdata (intf, dev);
		goto out1;
	}
	status = awl13_bind (dev, intf);
	if (status < 0)
		goto out1;

	data = (struct awl13_data *)dev->data;
	priv = data->priv;
	priv->probe_flags.awl13_binded = 1;

	/* maybe the remote can't receive an Ethernet MTU */
	if (net->mtu > (dev->hard_mtu - net->hard_header_len))
		net->mtu = dev->hard_mtu - net->hard_header_len;

	if (dev->status) {
		status = awl13_usbnet_init_status (dev, intf);
		if (status < 0){
			goto out3;
		}
	}

	if (!dev->rx_urb_size)
		dev->rx_urb_size = dev->hard_mtu;

	SET_NETDEV_DEV(net, &intf->dev);
	status = register_netdev (net);
	if (status)
		goto out3;
	priv->probe_flags.net_registered = 1;

	awl_debug ("register '%s' at usb-%s-%s, %s, %s\n",
		intf->dev.driver->name,
		xdev->bus->bus_name, xdev->devpath,
		dev->driver_info->description,

		awl13_print_mac(mac, net->dev_addr));

	// ok, it's ready to go.
	usb_set_intfdata (intf, dev);

	// start as if the link is up
	netif_device_attach (net);

	status = awl13_last_init (dev);
	if (status != 0)
		goto out4;

	priv->fw_ready_check = 1;
	status = awl13_fw_is_not_ready(priv);
	if (status == 1) {
		/* boot ROM run */
		awl_info("boot rom run\n");
		awl_debug("l.%d:priv->fw_not_ready=%d\n",
			 __LINE__, priv->fw_not_ready);
#ifdef AWL13_HOTPLUG_FIRMWARE
		awl_info("downloading firmware...\n");
		priv->probe_flags.prev_firmware_found = 0;
		status = awl13_usb_download_fw(intf, priv->firmware_file);
		if (status == (-ENOENT)) {
			awl_err("firmware is nothing\n");
			goto out5;
		}
		if (status == 0) {
			/* if firmware is already downloaded, we should reset
			   that firmware */
			if (priv->probe_flags.prev_firmware_found) {
				awl_info("found previous firmware -- "
					 "resetting...\n");
				/* command hardware reset to device */
				status = awl13_set_f_reset(priv, 3);
				if (status != 0) {
					awl_err("awl13_set_f_reset(3) failed "
						"on %d\n", status);
#if 0
/* free process of this function seems buggy, we prompt user to reset firmware
   manually instead */
awl_err("please execute: iwpriv <netif> set_f_reset 3\n");
#else /* 0/1 */
					goto out5;
#endif /* 0/1 */
				}
			}
			/* device will be disconnected and re-probed soon */
			goto ok;
		}
		else {

			awl_err("awl13_usb_download_fw() failed on %d\n",
				status);
			goto out5;

		}
#endif /* AWL13_HOTPLUG_FIRMWARE */
	} else if (status == 0) {
		/* firmware run */
		atomic_set(&priv->state, AWL13_STATE_READY);

		awl_info("new-session firmware run\n");
#ifdef AWL13_HOTPLUG_FIRMWARE
		/* "iwpriv awlan0 fwsetup" execute here */
		if ((status = awl13_firmware_setup_without_load(priv))
		    != 0) {
			awl_err("awl13_firmware_setup_without_load() "
				"failed on %d\n", status);
			goto out5;
		}
#endif /* AWL13_HOTPLUG_FIRMWARE */
	} else {
		/* error */
		awl_err("[fatal]:%d: invalid status=%d\n", __LINE__, status);
		goto out5;
	}
	
	awl_info("awl13 device successfully probed\n");
	return 0;

out5:
out4:
out3:
out1:
out:
	awl13_usbnet_clean(dev, intf);
	return status;
}

static void
awl13_usbnet_clean (struct awl13_usbnet *dev, struct usb_interface *intf)
{

	struct usb_device		*xdev = NULL;
	struct net_device		*net  = NULL;
	struct awl13_data		*data = NULL;
	struct awl13_private		*priv = NULL;

	if (dev) {
		xdev	= dev->udev;
		net	= dev->net;
		data	= (struct awl13_data *)dev->data;
		priv	= data->priv; 
	}
	
	if (priv) {

		if (priv->fw_ready_check) {
			priv->fw_ready_check = 0;
		}

		if (priv->attr_group.attrs) {
			awl13_sysfs_remove(priv);
		}

		if (priv->probe_flags.net_registered) {
			unregister_netdev (net);
			priv->probe_flags.net_registered = 0;
		}

		if (priv->probe_flags.awl13_binded) {
			awl13_unbind (dev, intf);
		}
	}

	if (net) {
		free_netdev(net);
		net = NULL;
	}

	usb_put_dev(xdev);
}

#ifdef AWL13_HOTPLUG_FIRMWARE
static int awl13_usb_download_fw(struct usb_interface *intf,
				 const char *filename)
{
	int	err = 0;
	const struct firmware	*fw;
	struct awl13_usbnet	*dev = usb_get_intfdata(intf);
	struct awl13_data	*a_data = (struct awl13_data *)dev->data;
	struct awl13_private	*priv = a_data->priv;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
	size_t remain_size;
	size_t copy_size;
	int i, loop_max;
	const unsigned char *data;
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */

	if (intf == NULL || filename == NULL || filename[0] == '\0')
		return -EINVAL;

	/* check length of firmware_name */
	if (strlen(priv->firmware_file) > (FIRMWARE_NAME_MAX - 1)) {

		awl_err("length of firmware exceeded : \"%s\"=%d "
			"(max length=%d)\n",
			priv->firmware_file, strlen(priv->firmware_file),
			(FIRMWARE_NAME_MAX - 1));

		return -EINVAL;
	}

	err = request_firmware(&fw, filename, &intf->dev);
	if (err) {
		if (err != (-ENOENT)) {
		awl_err("l.%d: request_firmware(\"%s\") failed on %d\n",
			__LINE__, filename, err);
		}
		awl_info("l.%d: request_firmware() failed on %d\n",
			 __LINE__, err);
		goto out;
	}
	
	/* check returned data */
	awl_debug("fw=%p\n", fw);
	if (fw == NULL) {
		err = -EINVAL;
		goto out;
	}
	

	/* copy firmware image to driver */


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	memcpy(priv->fw_image, fw->data, fw->size);
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	remain_size = fw->size;
	data = fw->data;
	/* calculate loop n */
	if (fw->size%FIRMWARE_BLOCK_MAX_SIZE) {
		loop_max = (fw->size/FIRMWARE_BLOCK_MAX_SIZE)+1;
	} else {
		/* futaction */
		loop_max = fw->size/FIRMWARE_BLOCK_MAX_SIZE;
	}

	for (i=0; i<loop_max; i++) {
		if (remain_size/FIRMWARE_BLOCK_MAX_SIZE != 0)
			copy_size = FIRMWARE_BLOCK_MAX_SIZE;
		else
			copy_size = fw->size%FIRMWARE_BLOCK_MAX_SIZE;
		memcpy(priv->fw_image[i], data, copy_size);
		remain_size -= copy_size;
		data += copy_size;
	}
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

	/* start download to AWL13 module */
	err = awl13_firmware_setup(priv);
	if (err && (priv->fw_ready_check == 0)) {
		awl_err("l.%d: awl13_firmware_setup() failed on %d\n",
			__LINE__, err);
	}
  out:
	release_firmware(fw);
	return err;
}
#endif /* AWL13_HOTPLUG_FIRMWARE */

#if defined(DEBUG)
static int awl13_print_mac(char dest[AWL13_MAC_BUF_SIZE],
			    const unsigned char addr[ETH_ALEN])
{
	int i;
	char *p = dest;
	
	for ( i = 0; i < ETH_ALEN; i++) {
		p += snprintf(p, 3, "%02x", addr[i]);
		if (i >= ETH_ALEN-1) break;
		*p++ = ':';
	}
	return p - dest;
}
#endif /* defined(DEBUG) */

static const struct awl13_driver_info awl13_info = {
	.description = "AWL13-USB",
	.flags 	= AWL13_FLAG_WLAN,
};

static const struct usb_device_id	products [] = {
{
	// BU1805  Vendor ID=0x0D4B  ProductID=0x0102
	USB_DEVICE (0x04B5, 0x0102),  
	.driver_info =	(unsigned long) &awl13_info,
}, 
{
	// BU1805(debug)  Vendor ID=0x0D4B  ProductID=0x0112
	USB_DEVICE (0x0D4B, 0x0112),  
	.driver_info =	(unsigned long) &awl13_info,
}, 

	{ },		// END
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver awl13_driver = {
	.name = 	AWL13_DRVINFO,
	.id_table =	products,
	.probe =	awl13_usbnet_probe,
	.disconnect =	awl13_usbnet_disconnect,

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	.supports_autosuspend = 0,
#ifdef CONFIG_PM
	.suspend =	awl13_usbnet_suspend,
	.resume =	awl13_usbnet_resume,
#endif /* CONFIG_PM */
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	.suspend =	awl13_usbnet_suspend,
	.resume =	awl13_usbnet_resume,
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

};

/******************************************************************************
 * awl13_usbnet_init  -
 *
 *****************************************************************************/
static int __init awl13_usbnet_init(void)
{
	awl_develop("function start.(awl13_usbnet_init)\n");
	return usb_register(&awl13_driver);
}
module_init(awl13_usbnet_init);

/******************************************************************************
 * awl13_usbnet_exit  -
 *
 *****************************************************************************/
static void __exit awl13_usbnet_exit(void)
{
	awl_develop("function start.(awl13_usbnet_exit)\n");
	usb_deregister(&awl13_driver);

}
module_exit(awl13_usbnet_exit);

MODULE_LICENSE("GPL");
