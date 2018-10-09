#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/if_vlan.h>


#include "awl13_log.h"
#include "awl13_device.h"
#include "awl13_wmm.h"

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
/* static function and variable */
static wlan_wmm_ac_e classify_priority(struct sk_buff *skb);

/******************************************************************************
 * classify_priority -
 *
 *****************************************************************************/
static wlan_wmm_ac_e 
classify_priority(struct sk_buff *skb)
{
	wlan_wmm_ac_e ac = AC_BE_Q;
	struct ethhdr *eth;
	struct udphdr *udp;
	struct iphdr *ip;
	struct vlan_ethhdr *vlan;
	int ac_assigned = 0;

	eth = (struct ethhdr *) skb->data;

	switch (eth->h_proto) {
	case __constant_htons(ETH_P_IP): /* IP */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
		udp = udp_hdr(skb);
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
		udp = skb->h.uh;
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */

		if (!udp) {
			ac = AC_BE_Q; /* Best Effort */
			break;
		}

		ac_assigned = 1;

		switch (__constant_htons(udp->dest)) {
		case 2340:
			ac = AC_BK_Q; /* background */
			break;
			
		case 2341:
			ac = AC_BE_Q; /* Best Effort */
			break;

		case 2342:
			ac = AC_VI_Q; /* video */
			break;

		case 2343:
			ac = AC_VO_Q;
			break;
			
		default:
			ac_assigned = 0;
			break;
		}

		if (!ac_assigned) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
			ip = ip_hdr(skb);
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
			ip = skb->nh.iph;
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21) */
			switch(IPTOS_PREC(ip->tos)) {
			case 0x20:
			case 0x40:
				ac = AC_BK_Q; /* background */
				break;

			case 0x80:
			case 0xA0:
				ac = AC_VI_Q; /* video */
				break;

			case 0xC0:           /* voice */
			case 0xE0:
			case 0x88:           /* XXX UPSD */
			case 0xB8:
				ac = AC_VO_Q;
				break;

			default:
				ac = AC_BE_Q; /* Best Effort */
				break;
			}
		}
		break;
    
    case __constant_htons(ETH_P_8021Q): /* VLAN */
        vlan = vlan_eth_hdr(skb);
		switch(__constant_htons(vlan->h_vlan_TCI) & 0x07) {
		case 0x01:
		case 0x02:
			ac = AC_BK_Q;
			break;

		case 0x00:
		case 0x03:
			ac = AC_BE_Q;
			break;

		case 0x04:
		case 0x05:
			ac = AC_VI_Q;
			break;

		case 0x06:
		case 0x07:
			ac = AC_VO_Q;
			break;

		default:
			ac =  AC_BE_Q;
			break;
		}
		break;
        
    default:
        ac = AC_BE_Q;
        break;
    }

	return ac;
}

/******************************************************************************
 * awl13_wmm_map_and_add_skb -
 *
 *****************************************************************************/
void
awl13_wmm_map_and_add_skb(struct awl13_private *priv, struct sk_buff *skb)
{
	wlan_wmm_ac_e		ac;
	unsigned long		flags;
	
	ac = classify_priority(skb);
	
	spin_lock_irqsave (&priv->wmm.wmm_skbq_lock, flags);
	list_add_tail((struct list_head *) skb, 
		      (struct list_head *) &priv->wmm.txSkbQ[ac].skbuf);
	spin_unlock_irqrestore (&priv->wmm.wmm_skbq_lock, flags);
	
	__net_timestamp(skb);
}



/******************************************************************************
 * awl13_wmm_init -
 *
 *****************************************************************************/
int
awl13_wmm_init(struct awl13_private *priv)
{
	wlan_wmm_ac_e		ac;
	unsigned long		flags;
	
	if (unlikely(priv == NULL)) {
		awl_err("priv=NULL failed(l.%d)\n", __LINE__);
		return -1;
	}
	
	spin_lock_init (&priv->wmm.wmm_skbq_lock);
	
	spin_lock_irqsave (&priv->wmm.wmm_skbq_lock, flags);
	
	for (ac = 0; ac < AWL13_MAX_AC_QUEUES; ac++) {
		/* initialize  list of txSkbQ[] */
		INIT_LIST_HEAD((struct list_head *) &priv->wmm.txSkbQ[ac].skbuf);
		
		/* set priority information */
		switch (ac) {
		case PND_Q:
			priv->wmm.txSkbQ[ac].txop = AWL13_TXOP_PND_Q;
			break;
		case AC_BK_Q:
			priv->wmm.txSkbQ[ac].txop = AWL13_TXOP_AC_BK_Q;
			break;
		case AC_BE_Q:
			priv->wmm.txSkbQ[ac].txop = AWL13_TXOP_AC_BE_Q;
			break;
		case AC_VI_Q:
			priv->wmm.txSkbQ[ac].txop = AWL13_TXOP_AC_VI_Q;
			break;
		case AC_VO_Q:
			priv->wmm.txSkbQ[ac].txop = AWL13_TXOP_AC_VO_Q;
			break;
		case HIP_Q:
			priv->wmm.txSkbQ[ac].txop = AWL13_TXOP_HIP_Q;
			break;
		default:
			spin_unlock_irqrestore (&priv->wmm.wmm_skbq_lock, flags);
			awl_err("unknown ac_queue(%d) (l.%d)\n", ac, __LINE__);
			return -1;
		}
	}
	
	/* set window size */
	priv->wmm.window_size	 = priv->wmm.txSkbQ[AC_BK_Q].txop;
	priv->wmm.window_size	+= priv->wmm.txSkbQ[AC_BE_Q].txop;
	priv->wmm.window_size	+= priv->wmm.txSkbQ[AC_VI_Q].txop;
	priv->wmm.window_size	+= priv->wmm.txSkbQ[AC_VO_Q].txop;
	
	spin_unlock_irqrestore (&priv->wmm.wmm_skbq_lock, flags);
	return 0;

}

#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
