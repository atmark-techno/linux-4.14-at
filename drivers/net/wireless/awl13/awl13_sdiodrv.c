/*
 * awl13_sdiodrv.c
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

#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/etherdevice.h>
#include <linux/leds.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/if_arp.h>
#include <net/iw_handler.h>

#include "awl13_device.h"
#include "awl13_log.h"
#include "awl13_wid.h"
#include "awl13_ioctl.h"
#include "awl13_fw.h"
#include "awl13_sysfs.h"

#ifdef AD500_SDIO_GPIO
#include <asm/arch/gpio.h>
#define  AWL13_IO_TX_CISSTAT    MX31_PIN_GPIO3_0		/* CON16 : PIN13 (GPIO0) */
#define  AWL13_IO_TX_TOGGLE     MX31_PIN_GPIO3_1		/* CON16 : PIN14 (GPIO1) */
#define  AWL13_IO_RX_CISSTAT    MX31_PIN_ATA_CS0		/* CON16 : PIN17 (GPIO6) */
#define  AWL13_IO_RX_TOGGLE     MX31_PIN_ATA_DIOR		/* CON16 : PIN19 (GPIO2) */

//#define  DISABLE_GPIO_RX_STATUS

#define  IS_IO_TXWAIT()		(mxc_get_gpio_datain(AWL13_IO_TX_CISSTAT))
#define  IS_IO_TXTOGGLE(t)	((t==TOGGLE_NONE) ? 1 : ((mxc_get_gpio_datain(AWL13_IO_TX_TOGGLE))==t) )
#define  SET_IO_TOGGLE(t)	{ t ^= 0x01; }

#define  IS_IO_RXRDY()		(mxc_get_gpio_datain(AWL13_IO_RX_CISSTAT))
#define  IS_IO_RXTOGGLE(t)	((t==TOGGLE_NONE) ? 1 : ((mxc_get_gpio_datain(AWL13_IO_RX_TOGGLE))==t) )
#endif /* AD500_SDIO_GPIO */

#define  IS_TXWAIT(v)		((v&AWL13_CISSTAT_MASK)==AWL13_CISSTAT_TXWAIT)
#define  IS_TXBUSY(v)		(((v&AWL13_CISSTAT_MASK)==AWL13_CISSTAT_TXSLEEP) || \
				 ((v&AWL13_CISSTAT_MASK)==AWL13_CISSTAT_TXACTIVE) )
#define  IS_RXRDY(v)		((v&AWL13_CISSTAT_MASK)==AWL13_CISSTAT_RXREADY)
#define  IS_RXNODATA(v)		((v&AWL13_CISSTAT_MASK)==AWL13_CISSTAT_RXNODATA)
#define  IS_TOGGLE(v,t)		((t==TOGGLE_NONE) ? 1 : (((v>>AWL13_TOGGLE_SHIFT)&0x01)==t) )
#define  SET_TOGGLE(v,t)	{ t = ((v>>AWL13_TOGGLE_SHIFT)&0x01)^0x01; }

#ifndef	CONFIG_ARMADILLO_WLAN_AWL13
static int awl13_sdio_enable_wide(struct sdio_func *func);
static int awl13_sdio_disable_wide(struct sdio_func *func);
#endif	/* CONFIG_ARMADILLO_WLAN_AWL13 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
static int awl13_open(struct net_device *dev);
static int awl13_close(struct net_device *dev);
static int awl13_hardstart(struct sk_buff *skb, struct net_device *dev);
struct net_device_stats * awl13_getstats(struct net_device *dev);
static void awl13_do_task(struct work_struct *work);
static int awl13_tx_thread(void *_priv);

static struct net_device_ops awl13_netdev_ops = {
         .ndo_open = awl13_open,
         .ndo_stop = awl13_close,
         .ndo_start_xmit = awl13_hardstart,
         .ndo_get_stats = awl13_getstats,
	    };
#endif

static int
awl13_sdio_f0_readb(struct sdio_func *func, unsigned int addr,
		     int *err_ret)
{
	int __num = func->num;
	unsigned char val;
	func->num = 0;
	val = sdio_readb(func, addr, err_ret);
	func->num = __num;
	return val;
}

static void
awl13_sdio_f0_writeb(struct sdio_func *func, unsigned char b,
		      unsigned int addr, int *err_ret)
{
	int __num = func->num;
	func->num = 0;
	sdio_writeb(func, b, addr, err_ret);
	func->num = __num;
}

int
awl13_enable_wide(struct awl13_private *priv)
{
	struct sdio_func *func = priv->func;

	if (atomic_read(&priv->power_mngt) == AWL13_PWR_MNGT_ACTIVE) {
		return 0;
	}

	if (atomic_read(&priv->bus_type) != SDIO_FORCE_4BIT) {

#ifdef CONFIG_ARMADILLO_WLAN_AWL13
		sdio_enable_wide(func->card);
#else /* CONFIG_ARMADILLO_WLAN_AWL13 */
		awl13_sdio_enable_wide(func);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */

		atomic_set(&priv->bus_type, SDIO_FORCE_4BIT);
	}
	return 0;
}

int
awl13_disable_wide(struct awl13_private *priv)
{
	struct sdio_func *func = priv->func;

	if (atomic_read(&priv->power_mngt) == AWL13_PWR_MNGT_ACTIVE) {
		return 0;
	}

	if (atomic_read(&priv->bus_type) != SDIO_FORCE_1BIT) {
#ifdef CONFIG_ARMADILLO_WLAN_AWL13
		sdio_disable_wide(func->card);
#else /* CONFIG_ARMADILLO_WLAN_AWL13 */
		awl13_sdio_disable_wide(func);
#endif  /* CONFIG_ARMADILLO_WLAN_AWL13 */
		atomic_set(&priv->bus_type, SDIO_FORCE_1BIT);
	}
	return 0;
}

int
awl13_set_txcis_polling_intvl(struct awl13_private *priv, unsigned int val)
{
	if(!val){
		return -1;
	}
	priv->tx_cis_intval = val;
	return 0;
}

int
awl13_get_txcis_polling_intvl(struct awl13_private *priv, unsigned int *val)
{
	*val = priv->tx_cis_intval;
	return 0;
}

int
awl13_set_rxcis_polling_intvl(struct awl13_private *priv, unsigned int val)
{
	if(!val){
		return -1;
	}
	priv->rx_cis_intval = val;
	return 0;
}

int
awl13_get_rxcis_polling_intvl(struct awl13_private *priv, unsigned int *val)
{
	*val = priv->rx_cis_intval;
	return 0;
}

int
awl13_set_txcis_toggle_timeout(struct awl13_private *priv, unsigned int val)
{
	priv->tx_cis_toggle_timeout = val/10;
	return 0;
}

int
awl13_get_txcis_toggle_timeout(struct awl13_private *priv, unsigned int *val)
{
	*val = priv->tx_cis_toggle_timeout*10;
	return 0;
}

int
awl13_set_rxcis_toggle_timeout(struct awl13_private *priv, unsigned int val)
{
	priv->rx_cis_toggle_timeout = val/10;
	return 0;
}

int
awl13_get_rxcis_toggle_timeout(struct awl13_private *priv, unsigned int *val)
{
	*val = priv->rx_cis_toggle_timeout*10;
	return 0;
}

extern void mmc_detect_change(struct mmc_host *host, unsigned long delay);

int
awl13_restart(struct awl13_private *priv)
{
	unsigned char 	val;
	int 		ret;
	int 		timeout;
	struct sdio_func *func = priv->func;
	struct mmc_card  *card = func->card;
	struct mmc_host  *host = card->host;

	if (atomic_read(&priv->state) == AWL13_STATE_NO_FW)
		return 0;

	sdio_claim_host(func);

	ret = sdio_release_irq(func);
	if(ret){
		sdio_release_host(func);
		printk( KERN_ERR "failed release irq\n" );
		return -EINVAL;
	}

	val = awl13_sdio_f0_readb(func, SDIO_CCCR_ABORT, &ret);
	if (ret){
		sdio_release_host(func);
		awl_err("failed read : CIS CCCR(ABORT)\n");
		return -EIO;
	}
	
	/* reset */
	awl13_sdio_f0_writeb(func, val|0x08, SDIO_CCCR_ABORT, &ret);
	if (ret!=-ETIMEDOUT) {
		sdio_release_host(func);
		awl_err("failed reset (ret=%d)\n", ret );
		return -EIO;
	}

	sdio_release_host(func);

	mmc_detect_change(host, msecs_to_jiffies(100));
	timeout = wait_for_completion_timeout(&priv->reset_ready,
				    msecs_to_jiffies(500));
	if(!timeout){
		printk( KERN_ERR "remove timeout\n" );
		return -ETIMEDOUT;
	}

	mmc_detect_change(host, msecs_to_jiffies(100));
	return 0;
}


/* sdio-lock of this function must be taken. */
int
awl13_send_prepare(struct sdio_func *func, int size)
{
	unsigned char val;
	int retry = 100;
	int ret = -EIO;
	int flag = 0x80;

        if(size==0){
		flag=0;
	}
	/* Set block length as word(32bit) to SD2AHB register (LSB) */
	retry = 10;
	while (retry--) {
		sdio_writeb(func, size & 0xff,
			    AWL13_F1REG_SD2AHB_BLOCKLEN_LSB, &ret);
		if (!ret) {
			val = sdio_readb(func,
					 AWL13_F1REG_SD2AHB_BLOCKLEN_LSB,
					 &ret);
			if (val == (size & 0xff))
				break;
		}
	}
	if (ret) {
		awl_err("failed set SD2AHB register(LSB)\n");
		return -EIO;
	}

	/* Set block length as word(32bit) to SD2AHB register (MSB) */
	retry = 10;
	while (retry--) {
		sdio_writeb(func, (size >> 8) | flag,
			    AWL13_F1REG_SD2AHB_BLOCKLEN_MSB, &ret);
		if (!ret) {
			val = sdio_readb(func,
					 AWL13_F1REG_SD2AHB_BLOCKLEN_MSB,
					 &ret);
			if (val == ((size >> 8) | flag))
				break;
		}
	}
	if (ret) {
		awl_err("failed set SD2AHB register(MSB)\n");
		return -EIO;
	}

	return 0;
}

int
awl13_set_interrupt(struct sdio_func *func, int setint)
{
	unsigned char 	val;
	int 		ret;

	sdio_writeb(func, setint, AWL13_F1REG_INT_MASK_REG, &ret);
	if (ret) {
		awl_err("failed set f1:int-mask\n");
		return -1;
	}
	val = sdio_readb(func, AWL13_F1REG_INT_MASK_REG, &ret);
	if (ret || val!=setint) {
		awl_err("failed read f1:int-mask (0x%x)\n", val );
		return -1;
	}
	awl_info("RX Transmission mode %s\n", 
		(val&AWL13_INT_RXRCV) ? "SDINT HT" : "CIS POLLING");

	return 0;
}

/* New Code with Double Read */
int
awl13_flow_start(struct awl13_private *priv)
{
	unsigned char 	val;
	int 			ret;
	struct sdio_func *func = priv->func;

	if (atomic_read(&priv->power_mngt) != AWL13_PWR_MNGT_ACTIVE) {
		if (awl13_send_prepare(func,AWL13_WRITE_WORD_SIZE)) {
			awl_err("failed send prepare(TX data waiting)\n");
			return -EINVAL;
		}
	}

	while(1){
#ifdef AD500_SDIO_GPIO
		if (atomic_read(&priv->state) != AWL13_STATE_NO_FW) {
			goto GPIO_TXCIS_CHECK;
		}
#endif  /* AD500_SDIO_GPIO */

		val = awl13_sdio_f0_readb(func, AWL13_TX_CISINFO_STATUS_ADDR, &ret);
		if (ret) {
			awl_err("failed read : CIS INFO STATUS(TX)\n");
			return -EIO;
		}

		if(!IS_TXWAIT(val))
		{
#ifdef AWL13_SDIO_HANG_FIX
			/* In case device is not ready then release the SDIO so that RX  */
			/* interrupts are processed, if there are any                    */
			sdio_release_host(func);
			udelay(AWL13_SDIO_TX_WAIT_DELAY);
			sdio_claim_host(func);
#endif /* AWL13_SDIO_HANG_FIX */
			continue;
		}

#if 0
		/* Read TX status register second time in order to avoid error due to*/
		/* metastable state read                                             */
		val = awl13_sdio_f0_readb(func, AWL13_TX_CISINFO_STATUS_ADDR, &ret);
		if (ret) {
			awl_err("failed read : CIS INFO STATUS(TX)\n");
			return -EIO;
		}
#endif

		if (IS_TOGGLE(val,priv->tx_toggle)){
			SET_TOGGLE(val,priv->tx_toggle);
			break;
		}

		udelay(priv->tx_cis_intval);
		continue;

#ifdef AD500_SDIO_GPIO
GPIO_TXCIS_CHECK:

		if(!IS_IO_TXWAIT())
		{
			continue;
		}

		if (IS_IO_TXTOGGLE(priv->tx_toggle)){
			SET_IO_TOGGLE(priv->tx_toggle);
			break;
		}
		udelay(priv->tx_cis_intval);
#endif  /* AD500_SDIO_GPIO */
	}

	return 0;
}

int
awl13_flow_end(struct awl13_private *priv)
{
	struct sdio_func *func = priv->func;

	if (atomic_read(&priv->power_mngt) != AWL13_PWR_MNGT_ACTIVE) {
		if (awl13_send_prepare(func,0x00)) {
			awl_err("failed send prepare(TX data complete)\n");
			return -EINVAL;
		}
	}
	return 0;
}


static void
awl13_rx_task(struct sdio_func *func)
{
	int		ret;
	struct sk_buff *skb;
	struct awl13_private	*priv = sdio_get_drvdata(func);
	struct awl13_packet	*pr  = priv->rxbuf;
	int			size;

	ret = sdio_readsb(func, (char*)priv->rxbuf,
			  AWL13_F1READBUFFER_ADDR, AWL13_WRITE_SIZE);
	if (ret) {
		awl_err("failed first block read (stat=0x%x)\n", 
			sdio_readb(func, AWL13_F1REG_FUNC1_INT_STATUS, &ret) );
		priv->net_stats.rx_errors++;
		goto RX_END;
	}

	size = pr->h.len + 2;
	if(size>AWL13_READ_SIZE){
		awl_err("unknown size [%d]\n", size );
		priv->net_stats.rx_errors++;
		goto RX_END;
	}

	awl13_dump(LOGLVL_3, (char*)priv->rxbuf, 16);

	if (pr->h.type == HTYPE_CONFIG_RES) {
		struct awl13_wid_frame *rf = to_wid_frame(priv->rxbuf);


		if( rf->wid == WID_STATUS){
			awl_info("WID=0x%x, STATUS CODE=0x%x\n",
				rf->wid,
				 (signed char)rf->val[0] );
		}
		else
		{
			awl_debug("check_wps_msg(usbBuffQue) called l.%d\n",
				 __LINE__);
			check_wps_msg(priv, pr->message); /* usbBuffQue in windows */
        }

		if (pr->m.type == CTYPE_RESPONSE) {
			if(priv->wid_response->header)
				msleep(1);
			if(priv->wid_response->header)
				awl_err("duplicate WID response!\n");
			memcpy(priv->wid_response, priv->rxbuf, size);
			complete(&priv->wid_complete);
			return;
		}
		if (pr->m.type == CTYPE_INFO) {
			struct awl13_wid_frame *rf = to_wid_frame(priv->rxbuf);
			if (rf->wid == WID_STATUS){
				if (pr->message[7] == 0x0) {
					awl_info("disconnected!\n");
				} else {
					if (memcmp(priv->iwap.ap_addr.sa_data,
						   priv->bssid, ETH_ALEN)) {
						memcpy(priv->
						       iwap.ap_addr.sa_data,
						       priv->bssid, ETH_ALEN);
						priv->iwap.ap_addr.sa_family =
							ARPHRD_ETHER;
						wireless_send_event
							(priv->netdev,
							 SIOCGIWAP,
							 &priv->iwap, NULL);
					}
					awl_info("connected!\n");
				}
			}
			else if (rf->wid == WID_DEVICE_READY){
				atomic_set(&priv->state, AWL13_STATE_READY);
				awl_info("device ready!\n");
			}
			return;
		}
		if (pr->m.type == CTYPE_NETWORK) {
			struct awl13_wid_frame *rf = to_wid_frame(priv->rxbuf);
			awl_info("Network Info! [0x%04x]\n", rf->wid);
			awl_dump( &(pr->m.body[4]), (pr->m.len-8) );
		}
		return;
	}

	/* Data IN packet */
	skb = dev_alloc_skb(size + NET_IP_ALIGN);
	if (!skb) {
		priv->net_stats.rx_dropped++;
		awl_err("failed skb memory allocate error\n");
		goto RX_END;
	}
	skb->dev = priv->netdev;
	skb_reserve(skb, NET_IP_ALIGN);
	skb_put(skb, size);
	size = size - sizeof(struct awl13_packet_header);
	skb_copy_to_linear_data(skb, pr->message, size );
	skb->protocol = eth_type_trans(skb, skb->dev);

	ret = netif_rx_ni(skb);
	if (ret == NET_RX_DROP) {
		priv->net_stats.rx_dropped++;
		awl_info("packet dropped\n");
		dev_kfree_skb_any(skb);
	}
	priv->net_stats.rx_packets++;
	priv->net_stats.rx_bytes += size;

RX_END:
	return;
}


/******************************************************************************
 * awl13_tx_task -
 *
 *****************************************************************************/
static void
awl13_tx_task(struct sk_buff *skb, struct net_device *dev)
{
	struct awl13_private *priv = netdev_priv(dev);
	struct awl13_packet_header packet_head;
	struct awl13_packet *txbuf;
	struct sdio_func *func = priv->func;
#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
	unsigned long flags;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */
	int ret;
	
	packet_head.len = skb->len;
	packet_head.type = HTYPE_DATA_OUT;
	if (skb_headroom(skb) < sizeof(packet_head) ||
	    skb_tailroom(skb) <
	    sizeof(*txbuf) - (sizeof(packet_head) + skb->len)) {
		if (unlikely(pskb_expand_head
			     (skb, SKB_DATA_ALIGN(sizeof(packet_head)),
			      sizeof(*txbuf) - (sizeof(packet_head) + skb->len),
			      GFP_ATOMIC))) {
			awl_err("It fails in allocation. (skb)\n");
			dev_kfree_skb_any(skb);
			return;
		}
	}
	txbuf = (struct awl13_packet *)skb_push(skb, sizeof(packet_head));
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
	skb_copy_to_linear_data(skb, &packet_head, sizeof(packet_head));
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	memcpy(txbuf, &packet_head, sizeof(packet_head));
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
	dev->trans_start = jiffies;

#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
	local_irq_save(flags);
	if (atomic_read(&priv->power_state) == AWL13_PWR_STATE_SLEEP) {
		local_irq_restore(flags);
		dev_kfree_skb_any(skb);
		return;
	}
	sdio_claim_host(func);
	local_irq_restore(flags);
#else /* CONFIG_ARMADILLO_WLAN_AWL13 */
	sdio_claim_host(func);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */

	awl13_led_trigger_event(priv, LED_FULL);

	if( awl13_flow_start(priv) != 0){
		goto tx_end;
	}

	ret = sdio_writesb(func, AWL13_F1WRITEBUFFER_ADDR, txbuf,
			   AWL13_WRITE_SIZE);
	if (ret) {
		awl_err("failed send data\n");
		priv->net_stats.tx_errors++;
		goto tx_end;
	}
	awl13_dump(LOGLVL_3, &txbuf, 16);

	if( awl13_flow_end(priv) != 0){
		goto tx_end;
	}

	priv->net_stats.tx_packets++;
	priv->net_stats.tx_bytes += packet_head.len;

tx_end:
	dev_kfree_skb_any(skb);
	sdio_release_host(func);

	awl13_led_trigger_event(priv, LED_OFF);
	return;
}

static void
awl13_irq(struct sdio_func *func)
{
#ifndef  RX_CIS_POLLING
	unsigned char   val;
	unsigned char   check;
	int		ret;

	/* Check Interrupt status */
	val = sdio_readb(func, AWL13_F1REG_FUNC1_INT_STATUS, &ret);
	if (ret) {
		awl_err("failed get interrupt status\n");
		return;
	}

	if(val&AWL13_INT_RXRCV){
		sdio_writeb(func, val, AWL13_F1REG_FUNC1_INT_PENDING, &ret);
		if (ret) {
			awl_err("failed clear interrupt status\n");
			return;
		}
		for (;;) {
			check = sdio_readb(func, AWL13_F1REG_FUNC1_INT_STATUS, &ret);
			if (ret) {
				awl_err("failed get interrupt status[2]\n");
			}
			if ((check&val)==0)
				break;
			awl_err("interrupt pending? (0x%x)\n", val );
		}
	}

	if(val&AWL13_INT_RXRCV){
		awl13_rx_task(func);
	}
#endif
	return;
}

static int
awl13_open(struct net_device *dev)
{
	struct awl13_private *priv = netdev_priv(dev);

	if (atomic_read(&priv->state) == AWL13_STATE_NO_FW) {
		awl_err("firmware was not loadding.\n");
		return -EIO;
	}

	if (priv->bss_type!=AWL13_BSS_WLANOFF){
		if (awl13_set_bsstype( priv, priv->bss_type)!=0){
			awl_err("failed bss type setting\n");
			return -EINVAL;
		}
	}

	netif_start_queue(dev);
	return 0;
}

static int
awl13_close(struct net_device *dev)
{
	unsigned char 		type;
	struct awl13_private 	*priv = netdev_priv(dev);

	atomic_set(&priv->state, AWL13_STATE_READY);
	netif_stop_queue(dev);

	if (awl13_get_bsstype(priv, &type)!=0){
		awl_err("failed bss type setting\n");
		return -EINVAL;
	}
	priv->bss_type = type;
	if (priv->bss_type!=AWL13_BSS_WLANOFF){
		if (awl13_set_bsstype( priv, AWL13_BSS_WLANOFF)!=0){
			awl_err("failed bss type setting\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int
awl13_hardstart(struct sk_buff *skb, struct net_device *dev)
{
	struct awl13_private *priv = netdev_priv(dev);
	struct awl13_task_info *task = &priv->tx_task;
	struct tx_args *args;
	unsigned long flags;

#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
	args = kzalloc(sizeof(struct tx_args), GFP_KERNEL);
#else /* CONFIG_ARMADILLO_WLAN_AWL13 */
	args = kzalloc(sizeof(struct tx_args), GFP_NOWAIT);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */
	if (args == NULL) {
		dev_kfree_skb_any(skb);
		awl_err("failed tx_args memory allocation\n");
		return -ENOMEM;
	}

	args->dev = dev;
	args->skb = skb;

	spin_lock_irqsave(&task->lock, flags);
	list_add_tail(&args->entry, &task->tx_args_list);
	spin_unlock_irqrestore(&task->lock, flags);

	schedule_delayed_work(&task->queue, usecs_to_jiffies(0));

	return 0;
}

struct net_device_stats *
awl13_getstats(struct net_device *dev)
{
	struct awl13_private *priv = netdev_priv(dev);

	return &priv->net_stats;
}

static void
awl13_do_task(struct work_struct *work)
{
	struct awl13_task_info *task = container_of(work,
						     struct awl13_task_info,
						     queue.work);
	struct awl13_private *priv = container_of(task,
						   struct awl13_private,
						   tx_task);

	complete(&priv->tx_thread_wait);
}

static int
awl13_tx_thread(void *_priv)
{
	struct awl13_private *priv = _priv;
	struct awl13_task_info *task = &priv->tx_task;
	struct tx_args *args;
	unsigned long flags;

	init_completion(&priv->tx_thread_wait);

	do {
		int timeout;

		timeout = wait_for_completion_interruptible_timeout(
						&priv->tx_thread_wait,
						msecs_to_jiffies(100));
		set_current_state(TASK_RUNNING);
		if (timeout > 0) {
			spin_lock_irqsave(&task->lock, flags);
			while (!list_empty(&task->tx_args_list)) {
				args = list_entry(task->tx_args_list.next,
						  struct tx_args, entry);
				list_del(&args->entry);
				spin_unlock_irqrestore(&task->lock, flags);

				awl13_tx_task(args->skb, args->dev);
				kfree(args);

				spin_lock_irqsave(&task->lock, flags);
			}
			spin_unlock_irqrestore(&task->lock, flags);
		}
	} while (!kthread_should_stop());

	return 0;
}

#ifdef RX_CIS_POLLING
static int
awl13_rx_thread(void *_priv)
{
	unsigned char 	val;
	int 			ret;
	unsigned long   start;
	unsigned long	flags;
	struct awl13_private 	*priv = _priv;
	struct sdio_func 		*func = priv->func;

	init_completion(&priv->rx_thread_wait);

	do {
#if 0
		wait_for_completion_interruptible_timeout(
						&priv->rx_thread_wait,
						usecs_to_jiffies(priv->rx_cis_intval));
#else
		udelay(priv->rx_cis_intval);
#endif
		set_current_state(TASK_RUNNING);

		start = jiffies;
		local_irq_save(flags);
		sdio_claim_host(func);

		while(1){
#ifdef AD500_SDIO_GPIO
			if (atomic_read(&priv->state) != AWL13_STATE_NO_FW) {
				goto GPIO_RXCIS_CHECK;
			}
#endif  /* AD500_SDIO_GPIO */

			val = awl13_sdio_f0_readb(func, AWL13_RX_CISINFO_STATUS_ADDR, &ret);
			if (ret) {
				awl_err("failed read : CIS INFO STATUS(RX)\n");
				break;
			}

			
			if (!IS_RXRDY(val)) {
//				priv->rx_toggle = TOGGLE_NONE;
				break;
			}

			val = awl13_sdio_f0_readb(func, AWL13_RX_CISINFO_STATUS_ADDR, &ret);
			if (ret) {
				awl_err("failed read : CIS INFO STATUS(RX)\n");
				break;
			}

			if (IS_TOGGLE(val,priv->rx_toggle)){
				awl13_rx_task(func);
				SET_TOGGLE(val,priv->rx_toggle);
				break;
			}
            break;

#ifdef AD500_SDIO_GPIO
GPIO_RXCIS_CHECK:

#ifndef DISABLE_GPIO_RX_STATUS
			if(!IS_IO_RXRDY())	{
//				priv->rx_toggle = TOGGLE_NONE;
				break;
			}
#endif
			if (IS_IO_RXTOGGLE(priv->rx_toggle)){
				SET_IO_TOGGLE(priv->rx_toggle);
				awl13_rx_task(func);
			}
			break;
#endif  /* AD500_SDIO_GPIO */
		}
		sdio_release_host(func);
		local_irq_restore(flags);

	} while (!kthread_should_stop() );

	return 0;
}
#endif

static int
awl13_task_init(struct awl13_private *priv)
{
	struct awl13_task_info *txtask = &priv->tx_task;
	int	ret;

	memset(txtask, 0, sizeof(struct awl13_task_info));

	spin_lock_init(&txtask->lock);
	INIT_LIST_HEAD(&txtask->tx_args_list);
	INIT_DELAYED_WORK(&txtask->queue, awl13_do_task);

	priv->tx_thread = kthread_run(awl13_tx_thread, priv, "%s.tx/%s",
				      AWL13_DRVINFO, priv->netdev->name);
	if (IS_ERR(priv->tx_thread)) {
		ret = PTR_ERR(priv->tx_thread);
		awl_err("Could not create tx thread. (ret=%d)\n", ret);
		priv->tx_thread = NULL;
		return ret;
	}
#ifdef RX_CIS_POLLING
	priv->rx_thread = kthread_run(awl13_rx_thread, priv, "%s.rx/%s",
				      AWL13_DRVINFO, priv->netdev->name);
	if (IS_ERR(priv->rx_thread)) {
		awl_err("Could not create rx thread.\n");
		return PTR_ERR(priv->rx_thread);
	}
#endif
	return 0;
}

static void
awl13_task_exit(struct awl13_private *priv)
{
	if (priv->tx_thread != NULL) {
		kthread_stop(priv->tx_thread);
		priv->tx_thread = NULL;
	}
#ifdef RX_CIS_POLLING
	if (priv->rx_thread) {
		kthread_stop(priv->rx_thread);
		priv->rx_thread = NULL;
	}
#endif
}

static int
awl13_sdio_if_init(struct sdio_func *func)
{
	unsigned char val;
	int retry;
	int ret;
	u8  setint;

	sdio_claim_host(func);

	/* set enable to Function #1 I/O */
	awl13_sdio_f0_writeb(func, 0x02, SDIO_CCCR_IOEx, &ret);
	if (ret) {
		awl_err("failed set func1 enable\n");
		goto init_end;
	}

	/* check if the I/O of Function #1 is ready */
	retry = 10;
	do {
		val = awl13_sdio_f0_readb(func, SDIO_CCCR_IORx, &ret);
		if (ret)
			break;
		if (val & 0x02)
			break;
		msleep(10);
	}
	while (--retry);
	if (!(val & 0x02)) {
		awl_err("func1 isnot ready\n");
		goto init_end;
	}

	/* set enable Function #1 Interrupt */
	awl13_sdio_f0_writeb(func, 0x03, SDIO_CCCR_IENx, &ret);
	if (ret) {
		awl_err("failed enable interrupt\n");
		goto init_end;
	}

	sdio_enable_func(func);

	setint = 0;
#ifndef  RX_CIS_POLLING
	setint |= AWL13_INT_RXRCV;
#endif

	if( awl13_set_interrupt(func, setint) != 0 ){
		awl_err("failed interrupt setting\n");
		goto init_end;
	}

	sdio_claim_irq(func, awl13_irq);

	/* Set block length */
	ret = sdio_set_block_size(func, AWL13_CHUNK_SIZE);
	if (ret) {
		awl_err("failed set block size\n");
		goto init_end;
	}
#ifdef CONFIG_ARMADILLO_WLAN_AWL13
	sdio_enable_wide(func->card);
#else /* CONFIG_ARMADILLO_WLAN_AWL13 */
	awl13_sdio_enable_wide(func);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */

	ret = awl13_send_prepare(func, AWL13_WRITE_WORD_SIZE);
	if (ret) {
		awl_err("failed send prepare\n");
	}

init_end:
	sdio_release_host(func);

	return ret;
}

#ifndef CONFIG_ARMADILLO_WLAN_AWL13
static int awl13_sdio_enable_wide(struct sdio_func *func)
{
	int ret;
	u8 ctrl;
	struct mmc_card *card = func->card;
	struct mmc_host  *host = card->host;

	if (!(card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (card->cccr.low_speed && !card->cccr.wide_bus)
		return 0;

	ctrl = awl13_sdio_f0_readb(func ,SDIO_CCCR_IF, &ret);
	if (ret)
		return ret;

	ctrl |= SDIO_BUS_WIDTH_4BIT;

	awl13_sdio_f0_writeb(func, ctrl, SDIO_CCCR_IF, &ret);
	if (ret)
		return ret;
	host->ios.bus_width = MMC_BUS_WIDTH_4;
	host->ops->set_ios(host, &host->ios);
	return 0;
}

static int awl13_sdio_disable_wide(struct sdio_func *func)
{
	int ret;
	u8 ctrl;
	struct mmc_card *card = func->card;
	struct mmc_host  *host = card->host;
	
	if (!(card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (card->cccr.low_speed && !card->cccr.wide_bus)
		return 0;
	ctrl = awl13_sdio_f0_readb(func ,SDIO_CCCR_IF, &ret);
	if (ret)
		return ret;

	if (!(ctrl & SDIO_BUS_WIDTH_4BIT))
		return 0;
	ctrl &= ~SDIO_BUS_WIDTH_4BIT;

	awl13_sdio_f0_writeb(func, ctrl, SDIO_CCCR_IF, &ret);
	if (ret)
		return ret;

	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ops->set_ios(host, &host->ios);
	return 0;
}
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */


static void
sdio_awl13_clean(struct awl13_private *priv)
{
	struct sdio_func *func;

	if (priv == NULL)
		return;

	/* stop threads */
	awl13_task_exit(priv);

	/* stop SDIO interrupt and function */
	func = priv->func;
	sdio_claim_host(func);
	sdio_release_irq(func);
	sdio_disable_func(func);
	sdio_release_host(func);

	if (priv->flags.sysfs_initialized)
		awl13_sysfs_remove(priv);

	if (priv->flags.netdev_registered)
		unregister_netdev(priv->netdev);

	/* free buffers */
	if (priv->fw_image	!= NULL)
		kfree(priv->fw_image);
	if (priv->rxbuf		!= NULL)
		kfree(priv->rxbuf);
	if (priv->wid_request	!= NULL)
		kfree(priv->wid_request);
	if (priv->wid_response	!= NULL)
		kfree(priv->wid_response);

	/* free netdev */
	free_netdev(priv->netdev);
}

static int
sdio_awl13_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	struct awl13_private *priv = NULL;
	struct net_device *dev;
	int ret;

#ifdef AD500_SDIO_GPIO
	mxc_set_gpio_direction(AWL13_IO_TX_CISSTAT, 1);  /* Input  */
	mxc_set_gpio_direction(AWL13_IO_TX_TOGGLE, 1);   /* Input  */
	mxc_set_gpio_direction(AWL13_IO_RX_CISSTAT, 1);  /* Input  */
	mxc_set_gpio_direction(AWL13_IO_RX_TOGGLE, 1);   /* Input  */
	
	mxc_set_gpio_dataout(AWL13_IO_TX_CISSTAT, 0 );   /* Low	*/
	mxc_set_gpio_dataout(AWL13_IO_TX_TOGGLE, 0 );    /* Low	*/
	mxc_set_gpio_dataout(AWL13_IO_RX_CISSTAT, 0 );   /* Low	*/
	mxc_set_gpio_dataout(AWL13_IO_RX_TOGGLE, 0 );    /* Low	*/
#endif /* AD500_SDIO_GPIO */

	/* net device initialize */
	dev = alloc_netdev(sizeof(struct awl13_private),
			   "awlan%d", ether_setup);
	if (!dev) {
		awl_err("could not allocate device\n");
		return -ENOMEM;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	dev->netdev_ops = &awl13_netdev_ops;
#else
	dev->open		= awl13_open;
	dev->stop		= awl13_close;
	dev->hard_start_xmit	= awl13_hardstart;
	dev->get_stats		= awl13_getstats;
#endif

	awl13_wireless_attach(dev);

#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
	/* card specific capability */
	func->card->ext_caps |= MMC_CARD_CAPS_FORCE_CLK_KEEP | MMC_CARD_CAPS_FORCE_BLKXMIT;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */

	/* AWL13 initialize */
#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
	priv = dev->priv;
#else /* CONFIG_ARMADILLO_WLAN_AWL13 */
	priv = netdev_priv(dev);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */
	memset(priv, 0, sizeof(*priv));
	priv->netdev = dev;
	priv->func = func;
	priv->bss_type      = AWL13_BSS_WLANOFF;
	priv->tx_cis_intval = AWL13_TX_CISPOLL_INTVAL;
	priv->rx_cis_intval = AWL13_RX_CISPOLL_INTVAL;
	priv->tx_cis_toggle_timeout = AWL13_TX_TOGGLE_TIMEOUT/10;
	priv->rx_cis_toggle_timeout = AWL13_RX_TOGGLE_TIMEOUT/10;
	priv->tx_toggle = TOGGLE_NONE;
	priv->rx_toggle = TOGGLE_NONE;
	
	/* allocate buffers */
	if ((priv->wid_response =
		 kzalloc(sizeof(struct awl13_packet), GFP_KERNEL)) == NULL) {
		awl_err("not allocate buffer wid_response\n");
		ret = -ENOMEM;
		goto error;
	}
	if ((priv->wid_request =
	     kzalloc(sizeof(struct awl13_packet), GFP_KERNEL)) == NULL) {
		awl_err("not allocate buffer wid_request\n");
		ret = -ENOMEM;
		goto error;
	}
	if ((priv->rxbuf =
	     kzalloc(sizeof(struct awl13_packet), GFP_KERNEL)) == NULL) {
		awl_err("not allocate buffer rxbuf\n");
		ret = -ENOMEM;
		goto error;
	}
	if ((priv->fw_image = 
	     kzalloc(FIRMWARE_SIZE_MAX, GFP_KERNEL)) == NULL) {
		awl_err("not allocate buffer fw_image\n");
		ret = -ENOMEM;
		goto error;
	}
	priv->fw_size = 0;
	
	sdio_set_drvdata(func, priv);

	atomic_set(&priv->state,	AWL13_STATE_NO_FW);
	atomic_set(&priv->power_state,	AWL13_PWR_STATE_ACTIVE);
	atomic_set(&priv->bus_type,	SDIO_FORCE_4BIT);
	atomic_set(&priv->power_mngt,	AWL13_PWR_MNGT_ACTIVE);

	sema_init(&priv->wid_req_lock, 1);

	init_completion(&priv->wid_complete);
	init_completion(&priv->reset_ready);

	ret = awl13_sdio_if_init(func);
	if (ret)
		goto error;

	SET_NETDEV_DEV(dev, &func->dev);
	ret = register_netdev(dev);
	if (ret) {
		awl_err("failed register netdev\n");
		goto error;
	}
	priv->flags.netdev_registered = 1;

	ret = awl13_sysfs_init(priv);
	if (ret) {
		awl_err("failed register sysfs-entry\n");
		goto error;
	}
	priv->flags.sysfs_initialized = 1;

	ret = awl13_task_init(priv);
	if (ret)
		goto error;

	awl13_register_led_trigger(priv);

	printk(KERN_INFO "%s: registerd \"%s\" device as %s\n",
	       mmc_hostname(func->card->host), AWL13_DRVINFO, dev->name);

	return 0;

error:
	sdio_awl13_clean(priv);
	
	return ret;
}

/******************************************************************************
 * spi_awl13_remove -
 *
 *****************************************************************************/
static void
sdio_awl13_remove(struct sdio_func *func)
{
	struct awl13_private *priv = sdio_get_drvdata(func);

	complete(&priv->reset_ready);

	awl13_unregister_led_trigger(priv);

	sdio_awl13_clean(priv);

	return;
}

static const struct sdio_device_id sdio_awl13_ids[] = {
	{ SDIO_CLASS_WLAN, SDIO_VENDOR_ID_ROHM, SDIO_DEVICE_ID_ROHM_BU1806GU },
	{ /* end: all zeros */ },
};

MODULE_DEVICE_TABLE(sdio, sdio_awl13_ids);

static struct sdio_driver sdio_awl13_driver = {
	.probe		= sdio_awl13_probe,
	.remove		= sdio_awl13_remove,
	.name		= "sdio_awl13",
	.id_table	= sdio_awl13_ids,
};

/******************************************************************************
 * sdio_awl13_init -
 *
 *****************************************************************************/
static int __init
sdio_awl13_init(void)
{
	int ret;

	ret = sdio_register_driver(&sdio_awl13_driver);
	if (ret)
		awl_err("Registration failure.(%d)\n", ret);
	else
		awl_info("Version %s Load.\n", AWL13_VERSION);

	return ret;
}

/******************************************************************************
 * sdio_awl13_exit -
 *
 *****************************************************************************/
static void __exit
sdio_awl13_exit(void)
{
	sdio_unregister_driver(&sdio_awl13_driver);
	awl_info("Version %s unloaded.\n", AWL13_VERSION);
}

module_init(sdio_awl13_init);
module_exit(sdio_awl13_exit);

MODULE_AUTHOR("Nissin Systems Co.,Ltd.");
MODULE_LICENSE("GPL");
