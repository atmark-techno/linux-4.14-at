/*
 * awl13_wid.c
 *
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
 * 2008-12-11    Created by Atmark Techno, Inc.
 * 2009-02-26   Modified by Atmark Techno, Inc.
 * 2011-11-25   Modified by Atmark Techno, Inc.
 */

#include <linux/netdevice.h>
#include <linux/delay.h>
#ifndef  CONFIG_ARMADILLO_WLAN_AWL13_USB
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
#include <net/iw_handler.h>

#include "awl13_log.h"
#include "awl13_device.h"
#include "awl13_wid.h"

/******************************************************************************
 * awl13_set_common_value -
 *
 *****************************************************************************/
int
awl13_set_common_value(struct awl13_private *priv, unsigned short wid,
			void *buf, int size, unsigned int timeout_ms)
{
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	struct sdio_func *func = priv->func;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

	struct awl13_packet *req;
	struct awl13_packet *res;
	struct awl13_wid_frame *wframe = to_wid_frame(priv->wid_response);

	int ret;

	if (atomic_read(&priv->wid_disposal) != AWL13_WID_NO_DISP) {
		return -EBUSY;
	}
	atomic_set(&priv->wid_disposal, AWL13_WID_DISP);

	req = priv->wid_request;
	res = priv->wid_response;

	memset(req, 0, sizeof(struct awl13_packet));
	memset(res, 0, sizeof(struct awl13_packet));

	if (timeout_ms == -1) {
		res = NULL;
		timeout_ms = 1;
	}

	if( (wid & 0xf000) == 0x4000 ){
		set_wid_write_request_long(req, wid, buf, size);
	}else{
		set_wid_write_request(req, wid, buf, size);
	}

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	ret = awl13_wid_request(func, timeout_ms);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	ret = awl13_usb_wid_request(priv, timeout_ms);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	if (ret){
		atomic_set(&priv->wid_disposal, AWL13_WID_NO_DISP);
		return ret;
	}
	if (res) {
		if (!response_ok(wframe)){
			atomic_set(&priv->wid_disposal, AWL13_WID_NO_DISP);
			return -1;
		}
	}
	atomic_set(&priv->wid_disposal, AWL13_WID_NO_DISP);

	return 0;
}

/******************************************************************************
 * awl13_get_common_value -
 *
 *****************************************************************************/
int
awl13_get_common_value(struct awl13_private *priv, unsigned short wid,
			void *buf, int size, unsigned int timeout_ms)
{
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	struct sdio_func *func = priv->func;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	struct awl13_wid_frame *wframe = to_wid_frame(priv->wid_response);
	struct awl13_wid_longframe *wframe_l = to_wid_longframe(priv->wid_response);

	int ret;

	if (atomic_read(&priv->wid_disposal) != AWL13_WID_NO_DISP) {
		return -EBUSY;
	}
	atomic_set(&priv->wid_disposal, AWL13_WID_DISP);

	memset(priv->wid_request,  0, sizeof(struct awl13_packet));
	memset(priv->wid_response, 0, sizeof(struct awl13_packet));

	set_wid_query_request(priv->wid_request, wid);

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	ret = awl13_wid_request(func, timeout_ms);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	ret = awl13_usb_wid_request(priv, timeout_ms);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	if (ret){
		atomic_set(&priv->wid_disposal, AWL13_WID_NO_DISP);
		return ret;
	}
	if (size < wframe->len){
		atomic_set(&priv->wid_disposal, AWL13_WID_NO_DISP);
		return -EINVAL;
	}

	memset(buf, 0x0, size);
    if( (wframe->wid & 0xF000) == 0x4000 ){
		if (wframe_l->len)
	    	memcpy(buf, wframe_l->val, wframe_l->len);
     	atomic_set(&priv->wid_disposal, AWL13_WID_NO_DISP);
        return wframe_l->len;
    }else{
		if (wframe->len)
			memcpy(buf, wframe->val, wframe->len);
    }
	atomic_set(&priv->wid_disposal, AWL13_WID_NO_DISP);

	return 0;
}


#define MESSAGE_OFFSET          4
#define MAX_SSID_LEN            33
#define MAX_NET_KEY_LEN		64
#define MAC_ADDRESS_LEN		6

void check_wps_msg(struct awl13_private *priv, unsigned char *usbBuffQue)
{
	int wid    = 0; /* PARAMETER_TYPE_T in windows */
	wid = (usbBuffQue[MESSAGE_OFFSET] & 0xFF) |
		((unsigned short)(usbBuffQue[MESSAGE_OFFSET + 1] & 0x00FF)<< 8);
	if(WID_WPS_STATUS == wid)
	{
        unsigned char wps_info_len = usbBuffQue[MESSAGE_OFFSET + 2] - 1;
        unsigned char wps_status   = usbBuffQue[MESSAGE_OFFSET + 3];
#if defined(DEBUG)
        unsigned char  *wps_info   = &(usbBuffQue[MESSAGE_OFFSET + 4]);
#endif /* defined(DEBUG) */
        switch(wps_status)
        {
        case IN_PROGRESS:
            {
                awl_err("WPS Status: IN_PROGRESS\n");
                break;
            }
        case ASSOC_PASS:
            {
                awl_err("WPS Status: ASSOC_PASS\n");
                break;
            }
        case REG_PROT_SUCC_COMP:
            {
                awl_err("WPS Status: REG_PROT_SUCC_COMP\n");
                break;
            }
        case ERR_ASSOC_FAIL:
            {
                awl_err("WPS Status: ERR_ASSOC_FAIL\n");
                break;
            }
        case ERR_SYSTEM:
            {
                awl_err("WPS Status: ERR_SYSTEM\n");
                break;
            }
        case ERR_WALK_TIMEOUT:
            {
                awl_err("WPS Status: ERR_WALK_TIMEOUT\n");
                break;
            }
        case SESSION_OVERLAP_DETECTED:
            {
                awl_err("WPS Status: SESSION_OVERLAP_DETECTED\n");
                break;
            }
        case ERR_PBC_REC_FAIL:
            {
                awl_err("WPS Status: ERR_PBC_REC_FAIL\n");
                break;
            }
        case ERR_REC_FAIL:
            {
                awl_err("WPS Status: ERR_REC_FAIL\n");
                break;
            }
        case ERR_REC_NACK:
            {
                awl_err("WPS Status: ERR_REC_NACK\n");
                break;
            }
        case ERR_DIFF_PASS_ID_M2:
            {
                awl_err("WPS Status: ERR_DIFF_PASS_ID_M2\n");
                break;
            }
        case ERR_REC_WRONG_M2:
            {
                awl_err("WPS Status: ERR_REC_WRONG_M2\n");
                break;
            }
        case REC_M2D:
            {
                awl_err("WPS Status: REC_M2D\n");
                break;
            }
        case ERR_REC_WRONG_M4:
            {
                awl_err("WPS Status: ERR_REC_WRONG_M4\n");
                break;
            }
        case ERR_REC_WRONG_M6:
            {
                awl_err("WPS Status: ERR_REC_WRONG_M6\n");
                break;
            }
        case ERR_REC_WRONG_M8:
            {
                awl_err("WPS Status: ERR_REC_WRONG_M8\n");
                break;
            }
        case ERR_REG_MSG_TIMEOUT:
            {
                awl_err("WPS Status: ERR_REG_MSG_TIMEOUT\n");
                break;
            }
        case ERR_PBC_REG_MSG_TIMEOUT:
            {
                awl_err("WPS Status: ERR_PBC_REG_MSG_TIMEOUT\n");
                break;
            }
        case ERR_REG_PROT_TIMEOUT:
            {
                awl_err("WPS Status: ERR_REG_PROT_TIMEOUT\n");
                break;
            }
        case ERR_STA_DISCONNECT:
            {
                awl_err("WPS Status: ERR_STA_DISCONNECT\n");
                break;
            }
        case RCV_CRED_VALUE:
            {
                awl_err("WPS Status: RCV_CRED_VALUE\n");
                break;
            }
        case CRED_JOIN_FAILURE:
            {
                awl_err("WPS Status: CRED_JOIN_FAILURE\n");
                break;
            }
        case CRED_JOIN_SUCCESS:
            {
                awl_err("WPS Status: CRED_JOIN_SUCCESS\n");
                break;
            }
        case CRED_JOIN_LIST_NULL:
            {
                awl_err("WPS Status: CRED_JOIN_LIST_NULL\n");
                break;
            }
        case WLAN_DIS_WPS_PROT:
            {
                awl_err("WPS Status: ERR_STA_DISCONNECT\n");
                break;
            }
        default:
            {
                awl_err("WPS Status: 0x%x\n",wps_status);
                break;
            }
        } /* switch(wps_status) */
        if(wps_info_len)
        {
            unsigned char count = 0;
            awl_debug("WPS Info len:    0x%x\n", wps_info_len);

            for(count=0; count < wps_info_len; count++)
            {
                awl_debug(" 0x%x\n",wps_info[count]);
            }
        }
    } /* else if (WID_WPS_STATUS == wid) */
    else if (WID_WPS_CRED_LIST == wid)
    {
        unsigned short wps_info_len = usbBuffQue[MESSAGE_OFFSET + 2] +
            ((usbBuffQue[MESSAGE_OFFSET + 3] << 8) & 0xFF00);
        unsigned char  *wps_cred_info = &(usbBuffQue[MESSAGE_OFFSET + 4]);
        if(wps_info_len)
        {
            unsigned char  *msg_cur_ptr, *msg_end_ptr, *next_cred_ptr;
            unsigned char  cred_len = 0;
            unsigned short attr_type;
            unsigned char  attr_len;

			awl_devdump( wps_cred_info, wps_info_len );
			priv->wps_cred_list_size = wps_info_len;
			memcpy( priv->wps_cred_list, wps_cred_info, priv->wps_cred_list_size );
            awl_debug("WPS Cred Info len:   0x%x\n",wps_info_len);
            awl_debug("WPS No of credentials:   0x%x\n", *wps_cred_info++);
            cred_len = *wps_cred_info++;
            msg_cur_ptr   = wps_cred_info;
            msg_end_ptr   = wps_cred_info + wps_info_len;
            next_cred_ptr = msg_cur_ptr + cred_len;
            awl_debug("WPS Credential length:   0x%x\n",cred_len);
            if(5 > wps_info_len)
            {
                msg_cur_ptr =  msg_end_ptr;
            }
            /*****************************************************************/
            /* Parse the received message                                    */
            /*****************************************************************/
            while (msg_cur_ptr < msg_end_ptr)
            {
                if ((msg_end_ptr - msg_cur_ptr) < 4)
                {
                    break;
                }
                attr_type     = msg_cur_ptr[0] + (msg_cur_ptr[1] << 8);
                msg_cur_ptr += 2;
                attr_len      = *msg_cur_ptr;
                msg_cur_ptr += 1;
                if(attr_len > msg_end_ptr - msg_cur_ptr)
                {
                    break;
                }
                /*************************************************************/
                /* Check the parse attribute, and print                      */
                /* required list the check                                   */
                /*************************************************************/
                switch (attr_type)
                {
                case WID_SSID:
                {
                    unsigned char ssid[MAX_SSID_LEN];
                    memset(ssid,0, sizeof(ssid));
                    awl_debug("SSID len:    0x%x\n",attr_len);
                    if(MAX_SSID_LEN <= attr_len)
                    {
                        awl_err("WPS Cred: Wrong SSID len\n");
                        break;
                    }
                    memcpy(ssid,msg_cur_ptr, attr_len);
                    awl_debug("SSID:    %s", ssid);
                    break;
                }
                case WID_AUTH_TYPE:
                    {
                        if(1 != attr_len)
                        {
                            awl_err("WPS Cred: Wrong AUTH_TYPE len");
                            break;
                        }
                            awl_debug("AUTH_TYPE:    0x%x",*msg_cur_ptr);
                        break;
                    }
                case WID_11I_MODE: /* WID_802_11I_MODE in Windows */
                    {
                        if(1 != attr_len)
                        {
                                awl_err("WPS Cred: Wrong 802_11I_MODE len\n");
                            break;
                        }
                        awl_debug("802_11I_MODE:    0x%x", *msg_cur_ptr);
                        break;
                    }
                case WID_KEY_ID:
                    {
                        if(1 != attr_len)
                        {
                                awl_err("WPS Cred: Wrong KEY_ID len\n");
                            break;
                        }
                        awl_debug("KEY_ID:  0x%x",*msg_cur_ptr);
                        break;
                    }
                case WID_WEP_KEY_VALUE: /* WID_WEP_KEY_VALUE0 in Windows */
                    {
                        unsigned char count =0;
                            awl_debug("WEP_KEY_VALUE0 len:   0x%x",attr_len);
                        if(MAX_NET_KEY_LEN < attr_len)
                        {
                            awl_err("WPS Cred: Wrong WEP_KEY_VALUE len\n");
                            break;
                        }
                        awl_debug("WEP_KEY_VALUE Value:\n");
                        for(count=0; count < attr_len; count++)
                        {
                            awl_debug(" 0x%x",msg_cur_ptr[count]);
                        }
                        break;
                    }
                case WID_11I_PSK:
                    {
			/* "unsigned char count =0;" existed in Windows */
                        unsigned char psk[MAX_NET_KEY_LEN + 1];
                        memset(psk,0, sizeof(psk));
                        awl_debug("11I_PSK len:  0x%x\n",attr_len);
                        if(MAX_NET_KEY_LEN < attr_len)
                        {
				awl_err("WPS Cred: Wrong 11I_PSK len\n");
				break;
                        }
                        memcpy(psk,msg_cur_ptr, attr_len);
                        awl_debug("11I_PSK: %s\n", psk);
                        break;
                    }
                case WID_BSSID:
                    {
                        if(MAC_ADDRESS_LEN != attr_len)
                        {
                            awl_err("WPS Cred: Wrong BSSID len\n");
                            break;
                        }
                        awl_debug("BSSID:    0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
                            msg_cur_ptr[0],msg_cur_ptr[1],msg_cur_ptr[2],
                            msg_cur_ptr[3],msg_cur_ptr[4],msg_cur_ptr[5]);
                        break;
                    }
                default:
                    {
                        awl_info("Unsupported WID: 0x%0x\n", attr_type);
                        break;
                    }
                } /* switch (attr_type) */
                msg_cur_ptr += attr_len;
                if((next_cred_ptr == msg_cur_ptr) &&
                    ((msg_cur_ptr + 5) < msg_end_ptr))
                {
                    cred_len = *msg_cur_ptr++;
                    next_cred_ptr = msg_cur_ptr + cred_len;
                    awl_debug("WPS Credential length:   0x%x\n", cred_len);
                }
            } /* while (msg_cur_ptr < msg_end_ptr) */
        } /* if(wps_info_len) */
    } /* else if (WID_WPS_CRED_LIST == wid) */
} /* check_wps_msg */

/******************************************************************************
 * is_wpa_mode :
 *
 *****************************************************************************/
static int
is_wpa_mode(struct awl13_private *priv)
{
	unsigned char mode;
	int ret;

	ret = awl13_get_cryptmode(priv, &mode);
	if (ret)
		return -1;

	switch (mode) {
	case AWL13_CRYPT_WPA_CCMP:
	case AWL13_CRYPT_WPA_TKIP:
	case AWL13_CRYPT_WPA2_CCMP:
	case AWL13_CRYPT_WPA2_TKIP:
		return 1;
	default:
		return 0;
	}
}

/******************************************************************************
 * awl13_set_bsstype - set the bss type
 * @priv:	awl13 private device
 * @type:	bss type
 *****************************************************************************/
int
awl13_set_bsstype(struct awl13_private *priv, unsigned char type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (type!=AWL13_BSS_INFRA &&
		type!=AWL13_BSS_ADHOC &&
		type!=AWL13_BSS_AP    &&
		type!=AWL13_BSS_WLANOFF )
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_BSS_TYPE, &type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set BSS type\n");

	return ret;
}

/******************************************************************************
 * awl13_get_bsstype - get the bss type
 * @priv:	awl13 private device
 * @type:	bss type
 *****************************************************************************/
int
awl13_get_bsstype(struct awl13_private *priv, unsigned char *type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_BSS_TYPE, type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get BSS type\n");

	return ret;
}

/******************************************************************************
 * awl13_set_ssid - set the SSID
 *
 *****************************************************************************/
int
awl13_set_ssid(struct awl13_private *priv, char *ssid, int size)
{
	int timeout = -1;
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (!size || AWL13_SSID_MAX < size)
		return -EINVAL;

	switch (is_wpa_mode(priv)) {
	case 1:
		timeout = WID_GENKEY_TIMEOUT;
		break;
	case 0:
		timeout = WID_DEFAULT_TIMEOUT;
		break;
	case -1:
	default:
		return -1;
	}

	ret = awl13_set_common_value(priv, WID_SSID, ssid, size,
				      timeout);
	if (ret)
		awl_err("failed set SSID\n");

	return ret;
}

/******************************************************************************
 * awl13_get_ssid -
 *
 *****************************************************************************/
int
awl13_get_ssid(struct awl13_private *priv, char *ssid, int size)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (size < AWL13_SSID_MAX)
		return -EINVAL;

	ret = awl13_get_common_value(priv, WID_SSID, ssid, size,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get SSID\n");

	return ret;
}

/******************************************************************************
 * awl13_get_bssid -
 *
 *****************************************************************************/
int
awl13_get_bssid(struct awl13_private *priv, unsigned char *bssid, int size)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (size < 6)
		return -EINVAL;

	ret = awl13_get_common_value(priv, WID_BSSID, bssid, size,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get BSSID\n");

	return ret;
}

/******************************************************************************
 * awl13_set_channel -
 *
 *****************************************************************************/
int
awl13_set_channel(struct awl13_private *priv, unsigned char ch)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (ch < AWL13_CHANNEL_MIN || AWL13_CHANNEL_MAX < ch)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_CURRENT_CHANNEL, &ch, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set channel\n");

	return ret;
}

/******************************************************************************
 * awl13_get_channel -
 *
 *****************************************************************************/
int
awl13_get_channel(struct awl13_private *priv, unsigned char *ch)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_CURRENT_CHANNEL, ch, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get channel\n");

	return ret;
}

/******************************************************************************
 * awl13_get_macaddr - get mac address
 * @func:	awl13 private device
 * @buf:	pointer to returned data buffer
 * @size:	buffer size
 *
 *****************************************************************************/
int
awl13_get_macaddr(struct awl13_private *priv, unsigned char *mac, int size)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (size < 6)
		return -EINVAL;

	ret = awl13_get_common_value(priv, WID_MAC_ADDR, mac, 6,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get MAC address\n");

	return ret;
}

/******************************************************************************
 * awl13_set_authtype -
 *
 *****************************************************************************/
int
awl13_set_authtype(struct awl13_private *priv, unsigned char type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (type != AWL13_AUTH_OPEN && type != AWL13_AUTH_KEY)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_AUTH_TYPE, &type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set Auth type\n");

	return ret;
}

/******************************************************************************
 * awl13_get_authtype -
 *
 *****************************************************************************/
int
awl13_get_authtype(struct awl13_private *priv, unsigned char *type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_AUTH_TYPE, type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get Auth type\n");

	return ret;
}

/******************************************************************************
 * awl13_set_cryptmode -
 *
 *****************************************************************************/
int
awl13_set_cryptmode(struct awl13_private *priv, unsigned char mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_set_common_value(priv, WID_11I_MODE, &mode, 1,
				      WID_GENKEY_TIMEOUT);
	if (ret)
		awl_err("failed set crypt mode\n");

	return ret;
}

/******************************************************************************
 * awl13_get_cryptmode -
 *
 *****************************************************************************/
int
awl13_get_cryptmode(struct awl13_private *priv, unsigned char *mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_11I_MODE, mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get crypt mode\n");

	return ret;
}

/******************************************************************************
 * awl13_set_wepkey -
 *
 *****************************************************************************/
int
awl13_set_wepkey(struct awl13_private *priv, int index, char *key, int len)
{
	unsigned short wid_wep_key[] = { WID_WEP_KEY_VALUE };

	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (len != AWL13_WEPKEY_MIN && len != AWL13_WEPKEY_MAX)
		return -EINVAL;
	if (index != 0)
		return -EINVAL;

	ret = awl13_set_common_value(priv, wid_wep_key[index], key, len,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set WEP key\n");

	return ret;
}

/******************************************************************************
 * awl13_get_wepkey -
 *
 *****************************************************************************/
int
awl13_get_wepkey(struct awl13_private *priv, int index, char *key, int len)
{
	unsigned short wid_wep_key[] = { WID_WEP_KEY_VALUE };

	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (len < 26)
		return -EINVAL;
	if (index != 0)
		return -EINVAL;

	ret = awl13_get_common_value(priv, wid_wep_key[index], key, len,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get WEP key\n");

	return ret;
}

/******************************************************************************
 * awl13_set_powerman -
 *
 *****************************************************************************/
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
int
awl13_set_powerman(struct awl13_private *priv, unsigned char mode)
{
	int ret = 0;
	unsigned char temp;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (AWL13_PMMODE_LMAX < mode)
		return -EINVAL;

    temp = atomic_read(&priv->power_mngt);
	if( mode!=AWL13_PMMODE_NONE ){
		if( !netif_running (priv->usbnetDev->net) ) {
	    	awl_err("The network doesn't start.\n");
			return -EINVAL;
		}
		atomic_set(&priv->power_mngt, mode);
	}

	ret = awl13_set_common_value(priv, WID_POWER_MANAGEMENT, &mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set Power management\n");

	if (ret){
		atomic_set(&priv->power_mngt, temp);
	}else if( mode==AWL13_PMMODE_NONE ){
		atomic_set(&priv->suspend_state, AWL13_IDLE);
		atomic_set(&priv->resume_state, AWL13_IDLE);
		atomic_set(&priv->net_suspend_state, AWL13_IDLE);
		atomic_set(&priv->force_active, 0);
		awl13_to_idle(priv);
		atomic_set(&priv->power_mngt, mode);
	}
	return ret;
}

#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

int
awl13_set_powerman(struct awl13_private *priv, unsigned char mode)
{
	int ret = 0;
	struct sdio_func *func = priv->func;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (AWL13_PMMODE_LMAX < mode)
		return -EINVAL;

	if( mode != AWL13_PMMODE_NONE){
		sdio_claim_host(func);
		if (awl13_send_prepare(func,
			(mode!=AWL13_PMMODE_NONE) ? 0x00 : AWL13_WRITE_WORD_SIZE )){
			awl_err("failed send prepare(power management)\n");
			return -EINVAL;
		}

		atomic_set(&priv->power_mngt, mode);
#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
		func->card->ext_caps &= ~MMC_CARD_CAPS_FORCE_CLK_KEEP;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */
		awl13_disable_wide(priv);
		sdio_release_host(func);

		ret = awl13_set_common_value(priv, WID_POWER_MANAGEMENT, &mode, 1,
					      WID_DEFAULT_TIMEOUT);
		if (ret)
			awl_err("failed set Power management\n");
	}
	else{
		ret = awl13_set_common_value(priv, WID_POWER_MANAGEMENT, &mode, 1,
					      WID_DEFAULT_TIMEOUT);
		if (ret)
			awl_err("failed set Power management\n");

		sdio_claim_host(func);
#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
		func->card->ext_caps |= MMC_CARD_CAPS_FORCE_CLK_KEEP;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */
		awl13_enable_wide(priv);
		atomic_set(&priv->power_mngt, AWL13_PWR_MNGT_ACTIVE);

		if (awl13_send_prepare(func,
			(mode!=AWL13_PMMODE_NONE) ? 0x00 : AWL13_WRITE_WORD_SIZE )){
			awl_err("failed send prepare(power management)\n");
			return -EINVAL;
		}
		sdio_release_host(func);
	}

	return ret;
}

#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

/******************************************************************************
 * awl13_get_powerman -
 *
 *****************************************************************************/
int
awl13_get_powerman(struct awl13_private *priv, unsigned char *mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_POWER_MANAGEMENT, mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get Power management\n");

	return ret;
}

/******************************************************************************
 * awl13_set_scan_type -
 *
 *****************************************************************************/
int
awl13_set_scan_type(struct awl13_private *priv, unsigned char type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (type != AWL13_SCAN_TYPE_PASSIVE && type != AWL13_SCAN_TYPE_ACTIVE)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_SCAN_TYPE, &type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set scan type\n");

	return ret;
}

/******************************************************************************
 * awl13_get_scan_type -
 *
 *****************************************************************************/
int
awl13_get_scan_type(struct awl13_private *priv, unsigned char *type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_SCAN_TYPE, type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get scan type\n");

	return ret;
}

/******************************************************************************
 * awl13_set_sitesv -
 *
 *****************************************************************************/
int
awl13_set_sitesv(struct awl13_private *priv, unsigned char mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (mode != AWL13_SITESV_CH    && 
        mode != AWL13_SITESV_ALLCH &&
        mode != AWL13_SITESV_NONE )
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_SITE_SURVEY, &mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set site survey mode\n");

	return ret;
}

/******************************************************************************
 * awl13_get_sitesv -
 *
 *****************************************************************************/
int
awl13_get_sitesv(struct awl13_private *priv, unsigned char *mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_SITE_SURVEY, mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get site survey mode\n");

	return ret;
}

/******************************************************************************
 * awl13_set_psctl -
 *
 *****************************************************************************/
int
awl13_set_psctl(struct awl13_private *priv, unsigned char mode)
{
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	struct sdio_func *func = priv->func;
	struct mmc_card *card = func->card;
	struct mmc_host *host = card->host;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	int ret = 0;
	unsigned int 			timeout = WID_DEFAULT_TIMEOUT;
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
	static unsigned int clock = 0;
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
	struct awl13_usbnet 	*dev 	= priv->usbnetDev;
	if (atomic_read(&priv->power_mngt) ) {
		awl_err("It is necessary to invalidate the power management.\n");
		return -EINVAL;
	}
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */

	NOFW_EXIT(priv);

	switch (mode) {
	case AWL13_PS_SLEEP:
		if (atomic_read(&priv->power_state) == AWL13_PWR_STATE_SLEEP)
			return 0;

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
		sdio_claim_host(func);
		atomic_set(&priv->power_state, AWL13_PWR_STATE_SLEEP);
		sdio_release_host(func);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		if (netif_running (dev->net)) {
			netif_carrier_off(dev->net);
		}
		if( awl13_suspend(priv) != 0 )
			return -EINVAL;
		atomic_set(&priv->power_state, AWL13_PWR_STATE_SLEEP);
		return 0;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		break;
	case AWL13_PS_ACTIVE:
		if (atomic_read(&priv->power_state) == AWL13_PWR_STATE_ACTIVE)
			return 0;
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
		host->ops->clk_start(host);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */
		msleep(100);
		sdio_release_host(func);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		atomic_set(&priv->resume_state, AWL13_PREPARE_RESUME );
		if( atomic_read(&priv->net_suspend_state) ) {
			netif_carrier_on(dev->net);
		}
		if( awl13_resume(priv, AWL13_ON) != 0 )
			return -EINVAL;
		atomic_set(&priv->power_state, AWL13_PWR_STATE_ACTIVE);
		return 0;
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		break;
	default:
		return -EINVAL;
	}

	ret = awl13_set_common_value(priv, WID_POWER_SAVE, &mode, 1,
				      timeout);
	if (ret)
		awl_err("failed set power save\n");

	switch (mode) {
	case AWL13_PS_SLEEP:
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
		sdio_claim_host(func);
		clock = host->ios.clock;
		host->ios.clock = 100000;
		host->ops->set_ios(host, &host->ios);
#ifdef	CONFIG_ARMADILLO_WLAN_AWL13
		host->ops->clk_stop(host);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13 */
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		break;
	default:
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
		sdio_claim_host(func);
		host->ios.clock = clock;
		host->ops->set_ios(host, &host->ios);
		atomic_set(&priv->power_state, AWL13_PWR_STATE_ACTIVE);
		sdio_release_host(func);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
		break;
	}
	return ret;
}

/******************************************************************************
 * awl13_set_startscan -
 *
 *****************************************************************************/
int
awl13_set_startscan(struct awl13_private *priv, unsigned char mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (mode != AWL13_START_SCAN_ON && mode != AWL13_START_SCAN_OFF)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_START_SCAN_REQ, &mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed site survey start request\n");

	return ret;
}

/******************************************************************************
 * awl13_get_startscan -
 *
 *****************************************************************************/
int
awl13_get_startscan(struct awl13_private *priv, unsigned char *mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_START_SCAN_REQ, mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get scan status\n");

	return ret;
}

/******************************************************************************
 * awl13_set_beaconint -
 *
 *****************************************************************************/
int
awl13_set_beaconint(struct awl13_private *priv, unsigned short val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (val < AWL13_BEACONINTVL_MIN || AWL13_BEACONINTVL_MAX < val)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_BEACON_INTERVAL, &val, 2,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set beacon interval\n");

	return ret;
}

/******************************************************************************
 * awl13_get_beaconint -
 *
 *****************************************************************************/
int
awl13_get_beaconint(struct awl13_private *priv, unsigned short *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_BEACON_INTERVAL, val, 2,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get beacon interval\n");

	return ret;
}


/******************************************************************************
 * awl13_set_lisnintvl - set the listen interval
 *
 *****************************************************************************/
int
awl13_set_lisnintvl(struct awl13_private *priv, unsigned char val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (val < AWL13_LISTENINTVL_MIN )
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_LISTEN_INTERVAL, &val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set listen interval\n");
	return ret;
}


/******************************************************************************
 * awl13_get_lisnintvl -
 *
 *****************************************************************************/
int
awl13_get_lisnintvl(struct awl13_private *priv, unsigned char *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_LISTEN_INTERVAL, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get listen interval\n");

	return ret;
}

/******************************************************************************
 * awl13_set_passps -
 *
 *****************************************************************************/
int
awl13_set_passps(struct awl13_private *priv, char *passps, int size)
{
	int timeout;
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (size < AWL13_PSKPASS_MIN || AWL13_PSKPASS_MAX < size)
		return -EINVAL;

	if( size == AWL13_PSKPASS_MAX ){
		int		loop;
		for(loop=0; loop<size; loop++){
			if( !((passps[loop] >= '0' && passps[loop] <= '9') ||
				  (passps[loop] >= 'A' && passps[loop] <= 'F') ||
				  (passps[loop] >= 'a' && passps[loop] <= 'f') )){
				awl_err("It should be a hexadecimal number character string for 64 bytes.\n");
				return -EINVAL;
			}
		}
	}

	switch (is_wpa_mode(priv)) {
	case 1:
		timeout = WID_GENKEY_TIMEOUT;
		break;
	case 0:
		timeout = WID_DEFAULT_TIMEOUT;
		break;
	case -1:
	default:
		return -1;
	}

	ret = awl13_set_common_value(priv, WID_11I_PSK, passps, size,
				      timeout);
	if (ret)
		awl_err("failed set Pre-Shared Key passphrase\n");

	return ret;
}

/******************************************************************************
 * awl13_get_passps -
 *
 *****************************************************************************/
int
awl13_get_passps(struct awl13_private *priv, char *passps, int size)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (size < 63)
		return -EINVAL;

	ret = awl13_get_common_value(priv, WID_11I_PSK, passps, size,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get Pre-Shared Key passphrase\n");

	return ret;
}


/******************************************************************************
 * awl13_set_bcastssid -
 *
 *****************************************************************************/
int
awl13_set_bcastssid(struct awl13_private *priv, unsigned char mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (mode != AWL13_BCASTSSID_DISABLE && mode != AWL13_BCASTSSID_ENABLE)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_BCAST_SSID, &mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set Broadcast SSID option\n");

	return ret;
}

/******************************************************************************
 * awl13_get_bcastssid -
 *
 *****************************************************************************/
int
awl13_get_bcastssid(struct awl13_private *priv, unsigned char *mode)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_BCAST_SSID, mode, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get Broadcast SSID option\n");

	return ret;
}

/******************************************************************************
 * awl13_set_join
 *
 *****************************************************************************/
int
awl13_set_join(struct awl13_private *priv, unsigned char index)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (AWL13_SITESV_MAX <= index)
		return -EINVAL;

	awl_info("It is necessary to make it to 0x10 or 0x11 with WID_SITE_SURVEY(set_scan_chmode).\n");

	ret = awl13_set_common_value(priv, WID_JOIN_REQ, &index, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed network join\n");

	return ret;
}

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
/******************************************************************************
 * awl13_set_usb_in_xfer_mode -
 *
 *****************************************************************************/
int
awl13_set_usb_in_xfer_mode(struct awl13_private *priv, unsigned char type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (type != CONFIG_ARMADILLO_WLAN_AWL13_USB_IN_XFER_VARIABLE && type != CONFIG_ARMADILLO_WLAN_AWL13_USB_IN_XFER_FIX)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_USB_IN_XFER_MODE, &type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set usb_in_xfer_mode\n");

	return ret;
}

/******************************************************************************
 * awl13_get_usb_in_xfer_mode -
 *
 *****************************************************************************/
int
awl13_get_usb_in_xfer_mode(struct awl13_private *priv, unsigned char *type)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_USB_IN_XFER_MODE, type, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get usb_in_xfer_mode\n");

	return ret;
}

/******************************************************************************
 * awl13_fw_is_not_ready -
 *
 *****************************************************************************/
int
awl13_fw_is_not_ready(struct awl13_private *priv)
{
	int ret;
	char str[128];
	memset(str, 0x00, sizeof(str));

	ret = awl13_get_common_value(priv, WID_FIRMWARE_VERSION, str,
				      sizeof(str), WID_DEFAULT_TIMEOUT);

	if (ret != 0 || priv->fw_not_ready == 1) {
		/* Boot ROM run */
		return 1;
	}
#if 0   /* 2010/11/12 9:22 K.Okada */
	else {
#else   /* 2010/11/12 9:22 K.Okada */
	else if ( priv->fw_not_ready == 0 ) {
#endif  /* 2010/11/12 9:22 K.Okada */
		/* Firmware run */
		return 0;
	}
	return 0;
}
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */


/******************************************************************************
 * awl13_get_firmware_version -
 *
 *****************************************************************************/
int
awl13_get_firmware_version(struct awl13_private *priv, char *buf, int size)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (size < 128)
		return -EINVAL;

	ret = awl13_get_common_value(priv, WID_FIRMWARE_VERSION, buf, size,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get firmware version\n");

	return ret;
}



/******************************************************************************
 * awl13_get_site_survey_results -
 *
 *****************************************************************************/
int
awl13_get_site_survey_results(struct awl13_private *priv,
			       struct awl13_survey_res *buf, int size)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_SITE_SURVEY_RESULTS,
				      buf, size, WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get site survey results[2]\n");

	return ret;
}

/******************************************************************************
 * awl13_get_rssi -
 *
 *****************************************************************************/
int
awl13_get_rssi(struct awl13_private *priv, signed char *rssi)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_RSSI, rssi, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get rssi value\n");

	return ret;
}
int
awl13_get_rssi_u8(struct awl13_private *priv, unsigned char *rssi)
{
	return awl13_get_rssi(priv, (signed char *)rssi);
}


/******************************************************************************
 * awl13_get_current_mac_status -
 *
 *****************************************************************************/
int
awl13_get_current_mac_status(struct awl13_private *priv, unsigned char *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_CURRENT_MAC_STATUS, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get current mac status\n");

	return ret;
}


/******************************************************************************
 * awl13_get_serial_number -
 *
 *****************************************************************************/
int
awl13_get_serial_number(struct awl13_private *priv, char *buf, int size)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (size < 32)
		return -EINVAL;

	ret = awl13_get_common_value(priv, WID_SERIAL_NUMBER, buf, size,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get serial number\n");

	return ret;
}


/******************************************************************************
 * awl13_get_key_id -
 *
 *****************************************************************************/
int
awl13_get_key_id(struct awl13_private *priv, unsigned char *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_KEY_ID, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get key id\n");

	return ret;
}

/******************************************************************************
 * awl13_set_dtim_period -
 *
 *****************************************************************************/
int
awl13_set_dtim_period(struct awl13_private *priv, unsigned char val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (val < AWL13_DTIM_MIN )
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_DTIM_PERIOD, &val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set DTIM period\n");

	return ret;
}

/******************************************************************************
 * awl13_get_dtim_period -
 *
 *****************************************************************************/
int
awl13_get_dtim_period(struct awl13_private *priv, unsigned char *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_DTIM_PERIOD, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get DTIM period\n");

	return ret;
}

/******************************************************************************
 * awl13_set_rekey_policy -
 *
 *****************************************************************************/
int
awl13_set_rekey_policy(struct awl13_private *priv, unsigned char val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if (val != AWL13_REKEYPOLICY_NON && val != AWL13_REKEYPOLICY_TIME)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_REKEY_POLICY, &val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set rekey policy\n");

	return ret;
}

/******************************************************************************
 * awl13_get_rekey_policy -
 *
 *****************************************************************************/
int
awl13_get_rekey_policy(struct awl13_private *priv, unsigned char *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_REKEY_POLICY, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get rekey policy\n");

	return ret;
}

/******************************************************************************
 * awl13_set_rekey_period -
 *
 *****************************************************************************/
int
awl13_set_rekey_period(struct awl13_private *priv, unsigned int val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	if ( val && (val < AWL13_REKEYTIM_MIN || AWL13_REKEYTIM_MAX < val))
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_REKEY_PERIOD, &val, 4,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set rekey period\n");

	return ret;
}

/******************************************************************************
 * awl13_get_rekey_period -
 *
 *****************************************************************************/
int
awl13_get_rekey_period(struct awl13_private *priv, unsigned int *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_REKEY_PERIOD, val, 4,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get rekey period\n");

	return ret;
}

/******************************************************************************
 * awl13_set_scan_filter -
 *
 *****************************************************************************/
int
awl13_set_scan_filter(struct awl13_private *priv, unsigned char val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_set_common_value(priv, WID_SCAN_FILTER, &val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set scan filter\n");

	return ret;
}

/******************************************************************************
 * awl13_get_scan_filter -
 *
 *****************************************************************************/
int
awl13_get_scan_filter(struct awl13_private *priv, unsigned char *val)
{
	int ret = 0;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

	ret = awl13_get_common_value(priv, WID_SCAN_FILTER, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get scan filter\n");

	return ret;
}


/******************************************************************************
 * awl13_set_wps_dev_mode -
 *
 *****************************************************************************/
int
awl13_set_wps_dev_mode(struct awl13_private *priv, unsigned char val)
{
	int ret;
	
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	if (val != AWL13_WPS_MODE_ALONE && val != AWL13_WPS_MODE_HOST)
		return -EINVAL;
	
	ret = awl13_set_common_value(priv, WID_WPS_DEV_MODE, &val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set wps_dev_mode \n");
	
	return ret;

}
/******************************************************************************
 * awl13_get_wps_dev_mode -
 *
 *****************************************************************************/
int
awl13_get_wps_dev_mode(struct awl13_private *priv, unsigned char *val)
{
	int ret;
	
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	ret = awl13_get_common_value(priv, WID_WPS_DEV_MODE, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get wps_dev_mode \n");
	
	return ret;

}

#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
/******************************************************************************
 * awl13_set_usb_rmtwkup_time -
 *
 *****************************************************************************/
int
awl13_set_usb_rmtwkup_time(struct awl13_private *priv, unsigned char val)
{
	int ret;
	
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	if (val < 10 || val > 30)
		return -EINVAL;
	
	if (val < CONFIG_ARMADILLO_WLAN_AWL13_USB_RMTWKUP_MIN || CONFIG_ARMADILLO_WLAN_AWL13_USB_RMTWKUP_MAX < val)
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_USB_RMTWKUP_TIME, &val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set usb_rmtwkup_time \n");
	
	return ret;

}
/******************************************************************************
 * awl13_get_usb_rmtwkup_time -
 *
 *****************************************************************************/
int
awl13_get_usb_rmtwkup_time(struct awl13_private *priv, unsigned char *val)
{
	int ret;
	
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	ret = awl13_get_common_value(priv, WID_USB_RMTWKUP_TIME, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get usb_rmtwkup_time \n");
	
	return ret;

}
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */


/******************************************************************************
 * awl13_set_wps_pin -
 *
 *****************************************************************************/
int
awl13_set_wps_pin(struct awl13_private *priv, char *pin, int size)
{
	int ret;
	
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	if (size < AWL13_PIN_CODE_MIN || AWL13_PIN_CODE_MAX < size)
		return -EINVAL;
	
	ret = awl13_set_common_value(priv, WID_WPS_PIN, pin, size,
				      WID_DEFAULT_TIMEOUT);
	if (ret) {
		awl_err("failed set wps_pin \n");
		return ret;
	}
	return ret;
}
/******************************************************************************
 * awl13_get_wps_pin -
 *
 *****************************************************************************/
int
awl13_get_wps_pin(struct awl13_private *priv, char *pin, int size)
{
	int ret;
	
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	if (size < AWL13_PIN_CODE_MIN || AWL13_PIN_CODE_MAX < size)
		return -EINVAL;
	
	ret = awl13_get_common_value(priv, WID_WPS_PIN, pin, size,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0) {
		awl_err("failed get wps_pin \n");
		return ret;
	}
	return ret;
}
/******************************************************************************
 * awl13_set_wps_start -
 *
 *****************************************************************************/
int
awl13_set_wps_start(struct awl13_private *priv, unsigned char val)
{
	int ret;
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	if (val > 3) {
		return -EINVAL;
	}
	
	if (val != AWL13_WPS_STOP && val != AWL13_WPS_PIN && 
		val != AWL13_WPS_PBC  && val != AWL13_WPS_CLEAR  )
		return -EINVAL;

	ret = awl13_set_common_value(priv, WID_WPS_START, &val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret)
		awl_err("failed set wps_start\n");
	
	return ret;
}
/******************************************************************************
 * awl13_get_wps_start -
 *
 *****************************************************************************/
int
awl13_get_wps_start(struct awl13_private *priv, unsigned char *val)
{
	int ret;
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	ret = awl13_get_common_value(priv, WID_WPS_START, val, 1,
				      WID_DEFAULT_TIMEOUT);
	if (ret<0)
		awl_err("failed get wps_start\n");
	
	return ret;
}


/******************************************************************************
 * awl13_set_wps_cred_list -
 *
 *****************************************************************************/
int
awl13_set_wps_cred_list(struct awl13_private *priv )
{
	int 			ret;
	int			  loop;
    unsigned char sum = 0;
	int			  len = priv->wps_cred_list_size+2;

	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);

    for( loop=0; loop<len; loop++ ){
        sum += priv->wps_cred_list[loop];
    }
    priv->wps_cred_list[len] = sum;

	ret = awl13_set_common_value( priv, 
								   WID_WPS_CRED_LIST, 
								   priv->wps_cred_list, 
								   len,
								   WID_GENKEY_TIMEOUT );
	if (ret<0)
		awl_err("failed set WID_WPS_CRED_LIST\n");
	else
	    awl_devdump( priv->wps_cred_list, priv->wps_cred_list_size );
	return ret;
}


/******************************************************************************
 * awl13_get_wps_cred_list -
 *
 *****************************************************************************/
int
awl13_get_wps_cred_list(struct awl13_private *priv )
{
	int ret;
	NOFW_EXIT(priv);
	SLEEP_EXIT(priv);
	
	ret = awl13_get_common_value( priv, 
								   WID_WPS_CRED_LIST, 
								   priv->wps_cred_list, 
								   WPS_CRED_LIST_SZ,
								   WID_DEFAULT_TIMEOUT );
	if (ret<0)
		awl_err("failed get WID_WPS_CRED_LIST\n");
	else
	    awl_devdump( priv->wps_cred_list, priv->wps_cred_list_size );
	return ret;
}

/******************************************************************************
 * set_wid_query_request -
 *
 *****************************************************************************/
void
set_wid_query_request(struct awl13_packet *req,
		      unsigned short wid)
{
	struct awl13_wid_frame *wframe = to_wid_frame(req);

	/* WID part */
	wframe->wid = wid;

	/* MESSAGE part */
	req->m.type = CTYPE_QUERY;
	req->m.seqno = 0;
	req->m.len = (awl13_packet_message_fixed_len
		      + 2 /* wid:(unsinged short) */);

	/* HEADER part */
	req->h.type = HTYPE_CONFIG_REQ;
	req->h.len = req->m.len;
	req->h.len = req->m.len;
}

/******************************************************************************
 * set_wid_write_request -
 *
 *****************************************************************************/
void
set_wid_write_request(struct awl13_packet *req,
		      unsigned short wid,
		      void *val,
		      unsigned char size)
{
	struct awl13_wid_frame *wframe = to_wid_frame(req);

	/* WID part */
	wframe->wid = wid;
	wframe->len = size;
	memcpy(wframe->val, val, size);

	/* MESSAGE part */
	req->m.type = CTYPE_WRITE;
	req->m.seqno = 0;
	req->m.len = (awl13_packet_message_fixed_len
		      + 2 /* wid:(unsinged short) */
		      + 1 /* len:(unsigned char) */
		      + size);

	/* HEADER part */
	req->h.type = HTYPE_CONFIG_REQ;
	req->h.len = req->m.len;
	req->h.len = req->m.len;
}


/******************************************************************************
 * set_wid_write_request_long -
 *
 *****************************************************************************/
void
set_wid_write_request_long(struct awl13_packet *req,
		      unsigned short wid,
		      void *val,
		      unsigned short size)
{
	struct awl13_wid_longframe *wframe = to_wid_longframe(req);

	/* WID part */
	wframe->wid = wid;
	wframe->len = size;

	if( (wid & 0xf000) == 0x4000 ){
		++size;
	}

	memcpy(wframe->val, val, size);

	/* MESSAGE part */
	req->m.type = CTYPE_WRITE;
	req->m.seqno = 0;
	req->m.len = (awl13_packet_message_fixed_len
		      + 2 /* wid:(unsinged short) */
		      + 2 /* len:(unsigned short) */
//		      + 1 /* sum:(unsigned char)  */
		      + size);

	/* HEADER part */
	req->h.type = HTYPE_CONFIG_REQ;
	req->h.len = req->m.len;
	req->h.len = req->m.len;
}


#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
/******************************************************************************
 * awl13_wid_request -
 *
 *****************************************************************************/
int
awl13_wid_request(struct sdio_func *func,
		   unsigned int timeout_ms)
{
	struct awl13_private *priv = sdio_get_drvdata(func);
	static unsigned char seq = 0;
	unsigned long timeout;
	int ret;
	int retry = 3;
#if 1   /* WID_START does not wait for a response. */
	struct awl13_wid_frame *wframe = to_wid_frame(priv->wid_request);
#endif

	ret = down_interruptible(&priv->wid_req_lock);
	if (ret)
		return -ERESTARTSYS;

	while (retry--) {
		
		memset(priv->wid_response, 0x00, sizeof(struct awl13_packet) );
		
		priv->wid_request->m.seqno = ++seq;
		sdio_claim_host(func);

		if( awl13_flow_start(priv) != 0){
			sdio_release_host(func);
			goto cmd_end;
		}

		ret = sdio_writesb(func, AWL13_F1WRITEBUFFER_ADDR,
				   priv->wid_request, AWL13_WRITE_SIZE);
		if (ret) {
			awl_err("failed send wid request\n");
			sdio_release_host(func);
			goto cmd_end;
		}

		awl13_dump(LOGLVL_3, priv->wid_request, 16);

		if( awl13_flow_end(priv) != 0){
			sdio_release_host(func);
			goto cmd_end;
		}

		sdio_release_host(func);

		if (!priv->wid_response)
			goto cmd_end;

		timeout = msecs_to_jiffies(timeout_ms);
#if 1   /* WID_START does not wait for a response. */
		if (wframe->wid == WID_START)
        {
			while(atomic_read(&priv->state)==AWL13_STATE_NO_FW)
				wait_for_completion_timeout(&priv->wid_complete,timeout);
			goto cmd_end;
        }
#endif
		while (timeout) {
			timeout = wait_for_completion_timeout(&priv->wid_complete,
							      timeout);
			if (timeout > 0){
				if (atomic_read(&priv->state) != AWL13_STATE_NO_FW) {
					if (priv->wid_response->m.seqno == seq)
						break;
				} else {
					struct awl13_wid_frame *rf = to_wid_frame(priv->wid_response);
					if (rf->wid == WID_STATUS){
						timeout = msecs_to_jiffies(timeout_ms);
						ret = (signed char)rf->val[0];
						if (ret < 0) {
							awl_err("WID_STATUS Code=0x%02X(%d)\n", (unsigned char)ret, ret);
						}
						memset(priv->wid_response, 0x00, sizeof(struct awl13_packet) );
						continue;
					}
                                        break;
                                }
			}
		}
		if (timeout)
			break;
	}
	if (retry < 0) {
		awl_err("wait wid response timed out\n");
		ret = -ETIMEDOUT;
		goto cmd_end;
	}
	memcpy(priv->wid_request, priv->wid_response, priv->wid_response->h.len + 2);

cmd_end:
	up(&priv->wid_req_lock);

	return ret;
}
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
/******************************************************************************
 * awl13_usb_wid_request -
 *
 *****************************************************************************/
int
awl13_usb_wid_request( struct awl13_private *priv, 
		    unsigned int timeout_ms)
{
	unsigned long		timeout;
	struct awl13_usbnet 		*dev = priv->usbnetDev;
	struct awl13_data    	*data=(struct awl13_data *)dev->data;
	struct urb		*urb = NULL;
	int			ret = 0;
	static unsigned char 	seq = 0;

	int 			size = awl13_align_chunk_size( 
					priv->wid_request->h.len+
					sizeof(struct awl13_packet_header) );
	struct awl13_wid_frame *wframe = to_wid_frame(priv->wid_request);
	priv->wid_request->m.seqno = ++seq;

	awl13_to_idle( priv );

	awl_develop("function start.(awl13_usb_wid_request) (out=%x, in=%x)\n", 
		    data->wid_bulk_out, data->wid_bulk_in);

	ret = down_interruptible(&priv->wid_req_lock);
	if (ret){
		atomic_dec(&priv->force_active);
		return -ERESTARTSYS;
	}

        /* WID send */
	if (!(urb = usb_alloc_urb (0, GFP_ATOMIC))) {
		awl_err("It fails in allocation. (URB-OUT)\n");
		ret=-ENOMEM;
		goto cmd_end;
	}

	usb_fill_bulk_urb (urb, dev->udev, usb_sndbulkpipe(dev->udev, data->wid_bulk_out),
		(void*)priv->wid_request, size, awl13_wid_complete, priv);
	awl_debug("WID transmission.(out=%x,len=%d)\n", data->wid_bulk_out, size);
	awl_dump( (void*)priv->wid_request, ((priv->wid_request->h.len+2)>16) ? 16 : (priv->wid_request->h.len+2) );

	ret = usb_submit_urb (urb, GFP_ATOMIC);
	if(ret!=0){
		awl_err("It fails in submit.(URB-OUT)(ret=%d)\n", ret );
		if (urb) {
			usb_free_urb (urb);
			urb = NULL;
		}

		ret=-EINVAL;
		goto cmd_end;
	}

	timeout = msecs_to_jiffies(timeout_ms);
	while (timeout) {
		timeout = wait_for_completion_timeout(&priv->wid_out_complete,
						      timeout);
		if (timeout)
			break;
	}
	if( !timeout ){
		awl_err("submit timeout.(URB-OUT)\n");
		if (urb) {
			usb_free_urb (urb);
			urb = NULL;
		}
		ret=-ETIMEDOUT;
		goto cmd_end;
	}
	else{
		if (urb->status == 0) {
			awl_debug("WID BULK OUT success! \n" );
		} else {
			awl_err("It fails in WID BULK OUT.(status=%d)\n", urb->status );
			if (urb) {
				usb_free_urb (urb);
				urb = NULL;
			}

			ret=-EINVAL;
			goto cmd_end;
		}
	}
	urb->dev = NULL;

	if (urb) {
		usb_free_urb (urb);
		urb = NULL;
	}
	
	if (wframe->wid == WID_START)
		goto cmd_end;
	
	timeout = msecs_to_jiffies(timeout_ms);

        /* BUG: It changes in v0.3.4 */
	while (timeout) {
		timeout = wait_for_completion_timeout(&priv->wid_complete,
						      timeout);
		/* check sequence no. */
		if( priv->wid_response->m.seqno == priv->wid_request->m.seqno) {
			/* check supported WID command */
			if ((priv->wid_response->h.len == 4) && 
			    (priv->wid_response->m.len == 4)) {
				awl_err("Unsupported WID! (WID:0x%04x)\n",
					wframe->wid);
				ret = -EPROTONOSUPPORT;
			}
			break; /* ok */
		}
	}
	if (!timeout){
		if (priv->fw_ready_check == 0)
			awl_err("wait wid response timed out\n");
		ret=-ETIMEDOUT;
		goto cmd_end;
	}

cmd_end:
	up(&priv->wid_req_lock);

	atomic_dec(&priv->force_active);
	return ret;
}
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */



