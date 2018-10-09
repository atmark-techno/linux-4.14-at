#ifndef _AWL13_WMM_H_
#define _AWL13_WMM_H_


#define AWL13_TXOP_PND_Q			0
#define AWL13_TXOP_AC_BK_Q			1
#define AWL13_TXOP_AC_BE_Q			2
#define AWL13_TXOP_AC_VI_Q			50
#define AWL13_TXOP_AC_VO_Q			45
#define AWL13_TXOP_HIP_Q			100


extern int awl13_wmm_init(struct awl13_private * priv);
extern void awl13_wmm_map_and_add_skb(struct awl13_private *priv,
				       struct sk_buff *skb);

#endif /* _AWL13_WMM_H_ */
