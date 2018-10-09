/*
 * awl13_wid.h
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
 * 2008-06-17    Created by Nissin Systems Co.,Ltd.
 * 2009-02-26   Modified by Atmark Techno, Inc.
 * 2011-11-25   Modified by Atmark Techno, Inc.
 * 2012-06-12   Modified by Atmark Techno, Inc.
 */

#ifndef _AWL13_WID_H_
#define _AWL13_WID_H_

#include "awl13_device.h"

/********************
 * WIDs & parameters
 ********************/
#define AWL13_SSID_MAX					32
#define AWL13_BSS_INFRA				0
#define AWL13_BSS_ADHOC				1
#define AWL13_BSS_AP					2
#define AWL13_BSS_WLANOFF				0x80
#define AWL13_CHANNEL_MIN				1
#define AWL13_CHANNEL_MAX				13
#define AWL13_AUTH_OPEN				0
#define AWL13_AUTH_KEY					1
#define AWL13_WEPKEY_MIN				10
#define AWL13_WEPKEY_MAX				26
#define AWL13_PMMODE_NONE				0
#define AWL13_PMMODE_FMIN				1
#define AWL13_PMMODE_FMAX				2
#define AWL13_PMMODE_LMIN				3
#define AWL13_PMMODE_LMAX				4
#define AWL13_SCAN_TYPE_PASSIVE		0
#define AWL13_SCAN_TYPE_ACTIVE			1
#define AWL13_SITESV_CH				0
#define AWL13_SITESV_ALLCH				1
#define AWL13_SITESV_NONE				2
#define AWL13_PS_ACTIVE				0
#define AWL13_PS_SLEEP					2
#define AWL13_START_SCAN_ON			0
#define AWL13_START_SCAN_OFF			1
#define AWL13_BEACONINTVL_MIN			1
#define AWL13_BEACONINTVL_MAX			60000
#define AWL13_LISTENINTVL_MIN			1
#define AWL13_LISTENINTVL_MAX			255
#define AWL13_PSKPASS_MIN				8
#define AWL13_PSKPASS_MAX				64
#define AWL13_BCASTSSID_DISABLE		0
#define AWL13_BCASTSSID_ENABLE			1
#define AWL13_SITESV_MAX				10
#define CONFIG_ARMADILLO_WLAN_AWL13_USB_IN_XFER_VARIABLE		0
#define CONFIG_ARMADILLO_WLAN_AWL13_USB_IN_XFER_FIX			1
#define AWL13_DTIM_MIN					1
#define AWL13_DTIM_MAX					255
#define AWL13_REKEYPOLICY_NON			0
#define AWL13_REKEYPOLICY_TIME			2
#define AWL13_REKEYTIM_MIN				60
#define AWL13_REKEYTIM_MAX				86400
#define AWL13_WPS_MODE_ALONE			0
#define AWL13_WPS_MODE_HOST			1
#define CONFIG_ARMADILLO_WLAN_AWL13_USB_RMTWKUP_MIN			1
#define CONFIG_ARMADILLO_WLAN_AWL13_USB_RMTWKUP_MAX			15
#define AWL13_PIN_CODE_MIN				1
#define AWL13_PIN_CODE_MAX				8
#define AWL13_WPS_STOP					0
#define AWL13_WPS_PIN					1
#define AWL13_WPS_PBC					2
#define AWL13_WPS_CLEAR				3
#define AWL13_KEYINDEX_MAX				1

enum {
              WID_11G_PROT_MECH                  = 0x0006,
              WID_PCF_MODE                       = 0x0013,
              WID_CFP_PERIOD                     = 0x0014,
              WID_TSSI_11B                       = 0x002b,
              WID_TSSI_11G                       = 0x002c,
              WID_PM_NULL_FRAME_INTERVAL         = 0x003E,
              WID_DEVICE_READY                   = 0x003D,
              WID_ACTIVITY_TIMER_RATE            = 0x003F,
              WID_DOWNLOAD_REQUEST               = 0x00F0,/* For Firmware */
              WID_START                          = 0x00F1,/* For Firmware */
              WID_RESET                          = 0x00F2,/* For Firmware */
              WID_DOWNLOAD_DATA                  = 0x30F0,/* For Firmware */


              WID_BSS_TYPE                       = 0x0000,
              WID_CURRENT_TX_RATE                = 0x0001,
              WID_CURRENT_CHANNEL                = 0x0002,
              WID_PREAMBLE                       = 0x0003,
              WID_11G_OPERATING_MODE             = 0x0004,
              WID_STATUS                         = 0x0005,
              WID_SCAN_TYPE                      = 0x0007,
              WID_PRIVACY_INVOKED                = 0x0008,
              WID_KEY_ID                         = 0x0009,
              WID_QOS_ENABLE                     = 0x000A,
              WID_POWER_MANAGEMENT               = 0x000B,
              WID_11I_MODE                       = 0x000C,
              WID_AUTH_TYPE                      = 0x000D,
              WID_SITE_SURVEY                    = 0x000E,
              WID_LISTEN_INTERVAL                = 0x000F,
              WID_DTIM_PERIOD                    = 0x0010,
              WID_ACK_POLICY                     = 0x0011,
              WID_F_RESET                        = 0x0012,
              WID_BCAST_SSID                     = 0x0015,
              WID_DISCONNECT                     = 0x0016,
              WID_READ_ADDR_SDRAM                = 0x0017,
              WID_TX_POWER_LEVEL_11A             = 0x0018,
              WID_REKEY_POLICY                   = 0x0019,
              WID_SHORT_SLOT_ALLOWED             = 0x001A,
              WID_PHY_ACTIVE_REG                 = 0x001B,
              WID_TX_POWER_LEVEL_11B             = 0x001D,
              WID_START_SCAN_REQ                 = 0x001E,
              WID_RSSI                           = 0x001F,
              WID_JOIN_REQ                       = 0x0020,
              WID_ANTENNA_SELECTION              = 0x0021,
              WID_USER_CONTROL_ON_TX_POWER       = 0x0027,
              WID_MEMORY_ACCESS_8BIT             = 0x0029,
              WID_UAPSD_SUPPORT_AP               = 0x002A,
              WID_CURRENT_MAC_STATUS             = 0x0031,
              WID_AUTO_RX_SENSITIVITY            = 0x0032,
              WID_DATAFLOW_CONTROL               = 0x0033,
              WID_SCAN_FILTER                    = 0x0036,
              WID_LINK_LOSS_THRESHOLD            = 0x0037,
              WID_AUTORATE_TYPE                  = 0x0038,
              WID_CCA_THRESHOLD                  = 0x0039,
              WID_802_11H_DFS_MODE               = 0x003B,
              WID_802_11H_TPC_MODE               = 0x003C,
              WID_WSC_IE_EN                      = 0x0042,
              WID_WPS_START                      = 0x0043,
              WID_WPS_DEV_MODE                   = 0x0044,

              WID_USB_RMTWKUP_TIME               = 0x0070,

              WID_11N_PROT_MECH                  = 0x0080,
              WID_11N_ERP_PROT_TYPE              = 0x0081,
              WID_11N_ENABLE                     = 0x0082,
              WID_11N_OPERATING_MODE             = 0x0083,
              WID_11N_OBSS_NONHT_DETECTION       = 0x0084,
              WID_11N_HT_PROT_TYPE               = 0x0085,
              WID_11N_RIFS_PROT_ENABLE           = 0x0086,
              WID_11N_SMPS_MODE                  = 0x0087,
              WID_11N_CURRENT_TX_MCS             = 0x0088,
              WID_11N_PRINT_STATS                = 0x0089,
              WID_HUT_FCS_CORRUPT_MODE           = 0x008A,
              WID_HUT_RESTART                    = 0x008B,
              WID_HUT_TX_FORMAT                  = 0x008C,
              WID_11N_SHORT_GI_20MHZ_ENABLE      = 0x008D,
              WID_HUT_BANDWIDTH                  = 0x008E,
              WID_HUT_OP_BAND                    = 0x008F,
              WID_HUT_STBC                       = 0x0090,
              WID_HUT_ESS                        = 0x0091,
              WID_HUT_ANTSET                     = 0x0092,
              WID_HUT_HT_OP_MODE                 = 0x0093,
              WID_HUT_RIFS_MODE                  = 0x0094,
              WID_HUT_SMOOTHING_REC              = 0x0095,
              WID_HUT_SOUNDING_PKT               = 0x0096,
              WID_HUT_HT_CODING                  = 0x0097,
              WID_HUT_TEST_DIR                   = 0x0098,
              WID_HUT_PHY_TEST_MODE              = 0x009A,
              WID_HUT_PHY_TEST_RATE_HI           = 0x009B,
              WID_HUT_PHY_TEST_RATE_LO           = 0x009C,
              WID_HUT_DISABLE_RXQ_REPLENISH      = 0x009D,
              WID_HUT_KEY_ORIGIN                 = 0x009E,
              WID_HUT_BCST_PERCENT               = 0x009F,
              WID_HUT_GROUP_CIPHER_TYPE          = 0x00A0,
              WID_TX_ABORT_CONFIG                = 0x00A1,
              WID_HOST_DATA_IF_TYPE              = 0x00A2,
              WID_HOST_CONFIG_IF_TYPE            = 0x00A3,
              WID_HUT_TSF_TEST_MODE              = 0x00A4,
              WID_HUT_PKT_TSSI_VALUE             = 0x00A5,
              WID_REG_TSSI_11B_VALUE             = 0x00A6,
              WID_REG_TSSI_11G_VALUE             = 0x00A7,
              WID_REG_TSSI_11N_VALUE             = 0x00A8,
              WID_TX_CALIBRATION                 = 0x00A9,
              WID_DSCR_TSSI_11B_VALUE            = 0x00AA,
              WID_DSCR_TSSI_11G_VALUE            = 0x00AB,
              WID_DSCR_TSSI_11N_VALUE            = 0x00AC,
              WID_HUT_RSSI_EX                    = 0x00AD,
              WID_HUT_ADJ_RSSI_EX                = 0x00AE,
              WID_11N_IMMEDIATE_BA_ENABLED       = 0x00AF,
              WID_11N_TXOP_PROT_DISABLE          = 0x00B0,
              WID_TX_POWER_LEVEL_11N             = 0x00B1,
              WID_CURRENT_SELECTING_TX_RATE      = 0x00B2,
              WID_POWER_SAVE                     = 0x0100,
              WID_WAKE_STATUS                    = 0x0101,
              WID_WAKE_CONTROL                   = 0x0102,
              WID_CCA_BUSY_START                 = 0x0103,
              WID_ANTENNA_CONTROL_TYPE           = 0x0104,
	      WID_TX_POWER_RATE                  = 0x0106,
              WID_USB_IN_XFER_MODE               = 0x0E00,

              WID_RTS_THRESHOLD                  = 0x1000,
              WID_FRAG_THRESHOLD                 = 0x1001,
              WID_SHORT_RETRY_LIMIT              = 0x1002,
              WID_LONG_RETRY_LIMIT               = 0x1003,
              WID_BEACON_INTERVAL                = 0x1006,
              WID_MEMORY_ACCESS_16BIT            = 0x1008,
              WID_RX_SENSE                       = 0x100B,
              WID_ACTIVE_SCAN_TIME               = 0x100C,
              WID_PASSIVE_SCAN_TIME              = 0x100D,
              WID_SITE_SURVEY_SCAN_TIME          = 0x100E,
              WID_JOIN_START_TIMEOUT             = 0x100F,
              WID_AUTH_TIMEOUT                   = 0x1010,
              WID_ASOC_TIMEOUT                   = 0x1011,
              WID_11I_PROTOCOL_TIMEOUT           = 0x1012,
              WID_EAPOL_RESPONSE_TIMEOUT         = 0x1013,
              WID_11N_RF_REG_VAL                 = 0x1080,
              WID_HUT_FRAME_LEN                  = 0x1081,
              WID_HUT_TXOP_LIMIT                 = 0x1082,
              WID_HUT_SIG_QUAL_AVG               = 0x1083,
              WID_HUT_SIG_QUAL_AVG_CNT           = 0x1084,
              WID_11N_SIG_QUAL_VAL               = 0x1085,
              WID_HUT_RSSI_EX_COUNT              = 0x1086,
              WID_CCA_BUSY_STATUS                = 0x1100,
              WID_DVT_RSSI_STATE_TIME            = 0x1101,

              WID_HOST_GPFLAG                    = 0x1200,

              WID_FAILED_COUNT                   = 0x2000,
              WID_RETRY_COUNT                    = 0x2001,
              WID_MULTIPLE_RETRY_COUNT           = 0x2002,
              WID_FRAME_DUPLICATE_COUNT          = 0x2003,
              WID_ACK_FAILURE_COUNT              = 0x2004,
              WID_RECEIVED_FRAGMENT_COUNT        = 0x2005,
              WID_MCAST_RECEIVED_FRAME_COUNT     = 0x2006,
              WID_FCS_ERROR_COUNT                = 0x2007,
              WID_SUCCESS_FRAME_COUNT            = 0x2008,
              WID_HUT_TX_COUNT                   = 0x200A,
              WID_TX_FRAGMENT_COUNT              = 0x200B,
              WID_TX_MULTICAST_FRAME_COUNT       = 0x200C,
              WID_RTS_SUCCESS_COUNT              = 0x200D,
              WID_RTS_FAILURE_COUNT              = 0x200E,
              WID_WEP_UNDECRYPTABLE_COUNT        = 0x200F,
              WID_REKEY_PERIOD                   = 0x2010,
              WID_REKEY_PACKET_COUNT             = 0x2011,
              WID_1X_SERV_ADDR                   = 0x2012,
              WID_STACK_IP_ADDR                  = 0x2013,
              WID_STACK_NETMASK_ADDR             = 0x2014,
              WID_HW_RX_COUNT                    = 0x2015,
              WID_MEMORY_ADDRESS                 = 0x201E,
              WID_MEMORY_ACCESS_32BIT            = 0x201F,
              WID_RF_REG_VAL                     = 0x2021,
              WID_FIRMWARE_INFO                  = 0x2023,

	      WID_DEV_OS_VERSION                 = 0x2025,

              WID_11N_PHY_ACTIVE_REG_VAL         = 0x2080,
              WID_HUT_NUM_TX_PKTS                = 0x2081,
              WID_HUT_TX_TIME_TAKEN              = 0x2082,
              WID_HUT_TX_TEST_TIME               = 0x2083,
              WID_FLASH_ADDRESS                  = 0x2100,
              WID_EEPROM_ADDRESS                 = 0x2101,
              WID_BT_COEX_PARAM                  = 0x2105,

              WID_SSID                           = 0x3000,
              WID_FIRMWARE_VERSION               = 0x3001,
              WID_OPERATIONAL_RATE_SET           = 0x3002,
              WID_BSSID                          = 0x3003,
              WID_WEP_KEY_VALUE                  = 0x3004,
              WID_11I_PSK                        = 0x3008,
              WID_11E_P_ACTION_REQ               = 0x3009,
              WID_1X_KEY                         = 0x300A,
              WID_HARDWARE_VERSION               = 0x300B,
              WID_MAC_ADDR                       = 0x300C,
              WID_HUT_DEST_ADDR                  = 0x300D,
              WID_PHY_VERSION                    = 0x300F,
              WID_SUPP_USERNAME                  = 0x3010,
              WID_SUPP_PASSWORD                  = 0x3011,
              WID_SITE_SURVEY_RESULTS            = 0x3012,
              WID_RX_POWER_LEVEL                 = 0x3013,
              WID_ADD_WEP_KEY                    = 0x3019,
              WID_REMOVE_WEP_KEY                 = 0x301A,
              WID_ADD_PTK                        = 0x301B,
              WID_ADD_RX_GTK                     = 0x301C,
              WID_ADD_TX_GTK                     = 0x301D,
              WID_REMOVE_KEY                     = 0x301E,
              WID_ASSOC_REQ_INFO                 = 0x301F,
              WID_ASSOC_RES_INFO                 = 0x3020,
              WID_UPDATE_RF_SUPPORTED_INFO       = 0x3021,
	      WID_CURRENT_SYSTEM_STATUS		 = 0x3023,
              WID_WPS_STATUS                     = 0x3024,
              WID_WPS_PIN                        = 0x3025,
              WID_11N_P_ACTION_REQ               = 0x3080,
              WID_HUT_TEST_ID                    = 0x3081,
              WID_FLASH_DATA                     = 0x3100,
              WID_EEPROM_DATA                    = 0x3101,
              WID_SERIAL_NUMBER                  = 0x3102,

              WID_UAPSD_CONFIG                   = 0x4001,
              WID_UAPSD_STATUS                   = 0x4002,
              WID_WMM_AP_AC_PARAMS               = 0x4003,
              WID_WMM_STA_AC_PARAMS              = 0x4004,
              WID_WPS_CRED_LIST                  = 0x4006,
              WID_PRIM_DEV_TYPE                  = 0x4007,
              WID_11N_AUTORATE_TABLE             = 0x4080,
              WID_HUT_TX_PATTERN                 = 0x4081,
              WID_HUT_STATS                      = 0x4082,
              WID_HUT_LOG_STATS                  = 0x4083,
	      WID_BEACON_VSIE			 = 0x4100,
              WID_ALL                            = 0x7FFE,
};

/*************************
 * site survey structures
 *************************/
struct awl13_survey_info {
	uint8_t ssid[33];
	uint8_t bsstype;
	uint8_t channel;
	uint8_t security;
#define AWL13_CRYPT_ENABLE	(1<<0)
#define AWL13_CRYPT_WEP	(1<<1)
#define AWL13_CRYPT_WEPSZ	(1<<2)
#define AWL13_CRYPT_WPA	(1<<3)
#define AWL13_CRYPT_WPA2	(1<<4)
#define AWL13_CRYPT_CCMP	(1<<5)
#define AWL13_CRYPT_TKIP	(1<<6)
#define AWL13_CRYPT_RESERVED	(1<<7)

#define AWL13_CRYPT_WEP_MASK	(AWL13_CRYPT_WEP | AWL13_CRYPT_WEPSZ)
#define AWL13_CRYPT_WPA_MASK	(AWL13_CRYPT_WPA | AWL13_CRYPT_WPA2 | \
				 AWL13_CRYPT_CCMP | AWL13_CRYPT_TKIP)

#define AWL13_CRYPT_DISABLE	(0)
#define AWL13_CRYPT_UNKNOWN	(AWL13_CRYPT_ENABLE)
#define AWL13_CRYPT_WEP64	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WEP)
#define AWL13_CRYPT_WEP128	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WEP | \
				 AWL13_CRYPT_WEPSZ)
#define AWL13_CRYPT_WPA_CCMP	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WPA | \
				 AWL13_CRYPT_CCMP)
#define AWL13_CRYPT_WPA_TKIP	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WPA | \
				 AWL13_CRYPT_TKIP)
#define AWL13_CRYPT_WPA_MIX	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WPA | \
				 AWL13_CRYPT_CCMP | AWL13_CRYPT_TKIP)
#define AWL13_CRYPT_WPA2_CCMP	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WPA2 | \
				 AWL13_CRYPT_CCMP)
#define AWL13_CRYPT_WPA2_TKIP	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WPA2 | \
				 AWL13_CRYPT_TKIP)
#define AWL13_CRYPT_WPA2_MIX	(AWL13_CRYPT_ENABLE | AWL13_CRYPT_WPA2 | \
				 AWL13_CRYPT_CCMP | AWL13_CRYPT_TKIP)
#define AWL13_CRYPT_WPA_2_CCMP	(AWL13_CRYPT_ENABLE | \
				 AWL13_CRYPT_WPA | AWL13_CRYPT_WPA2 | \
				 AWL13_CRYPT_CCMP)
#define AWL13_CRYPT_WPA_2_TKIP	(AWL13_CRYPT_ENABLE | \
				 AWL13_CRYPT_WPA | AWL13_CRYPT_WPA2 | \
				 AWL13_CRYPT_TKIP)
#define AWL13_CRYPT_WPA_2_MIX	(AWL13_CRYPT_ENABLE | \
				 AWL13_CRYPT_WPA | AWL13_CRYPT_WPA2 | \
				 AWL13_CRYPT_CCMP | AWL13_CRYPT_TKIP)

	uint8_t bssid[6];
	int8_t rxpower;
	uint8_t reserved;
} __attribute__((packed));

struct awl13_survey_res {
	uint8_t size;
	uint8_t index;
	struct awl13_survey_info info[5];
} __attribute__((packed));

/**********
 * externs
 **********/
extern int awl13_set_common_value(struct awl13_private *priv, unsigned short wid,
                                   void *buf, int size, unsigned int timeout_ms);
extern int awl13_get_common_value(struct awl13_private *priv, unsigned short wid,
                                   void *buf, int size, unsigned int timeout_ms);
extern void check_wps_msg(struct awl13_private *priv, unsigned char *usbBuffQue);
extern int awl13_set_bsstype(struct awl13_private *priv, unsigned char type);
extern int awl13_get_bsstype(struct awl13_private *priv, unsigned char *type);
extern int awl13_set_ssid(struct awl13_private *priv, char *ssid, int size);
extern int awl13_get_ssid(struct awl13_private *priv, char *ssid, int size);
extern int awl13_get_bssid(struct awl13_private *priv, unsigned char *bssid, int size);
extern int awl13_set_channel(struct awl13_private *priv, unsigned char ch);
extern int awl13_get_channel(struct awl13_private *priv, unsigned char *ch);
extern int awl13_get_macaddr(struct awl13_private *priv, unsigned char *mac, int size);
extern int awl13_set_authtype(struct awl13_private *priv, unsigned char type);
extern int awl13_get_authtype(struct awl13_private *priv, unsigned char *type);
extern int awl13_set_cryptmode(struct awl13_private *priv, unsigned char mode);
extern int awl13_get_cryptmode(struct awl13_private *priv, unsigned char *mode);
extern int awl13_set_wepkey(struct awl13_private *priv, int index, char *key, int len);
extern int awl13_get_wepkey(struct awl13_private *priv, int index, char *key, int len);
extern int awl13_set_powerman(struct awl13_private *priv, unsigned char mode);
extern int awl13_get_powerman(struct awl13_private *priv, unsigned char *mode);
extern int awl13_set_scan_type(struct awl13_private *priv, unsigned char type);
extern int awl13_get_scan_type(struct awl13_private *priv, unsigned char *type);
extern int awl13_set_sitesv(struct awl13_private *priv, unsigned char mode);
extern int awl13_get_sitesv(struct awl13_private *priv, unsigned char *mode);
extern int awl13_set_psctl(struct awl13_private *priv, unsigned char mode);
extern int awl13_set_startscan(struct awl13_private *priv, unsigned char mode);
extern int awl13_get_startscan(struct awl13_private *priv, unsigned char *mode);
extern int awl13_set_beaconint(struct awl13_private *priv, unsigned short val);
extern int awl13_get_beaconint(struct awl13_private *priv, unsigned short *val);
extern int awl13_set_lisnintvl(struct awl13_private *priv, unsigned char val);
extern int awl13_get_lisnintvl(struct awl13_private *priv, unsigned char *val);
extern int awl13_set_passps(struct awl13_private *priv, char *passps, int size);
extern int awl13_get_passps(struct awl13_private *priv, char *passps, int size);
extern int awl13_set_bcastssid(struct awl13_private *priv, unsigned char mode);
extern int awl13_get_bcastssid(struct awl13_private *priv, unsigned char *mode);
extern int awl13_set_join(struct awl13_private *priv, unsigned char index);
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
extern int awl13_set_usb_in_xfer_mode(struct awl13_private *priv, unsigned char type);
extern int awl13_get_usb_in_xfer_mode(struct awl13_private *priv, unsigned char *type);
extern int awl13_fw_is_not_ready(struct awl13_private *priv);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
extern int awl13_get_firmware_version(struct awl13_private *priv, char *buf, int size);
extern int awl13_get_site_survey_results(struct awl13_private *priv,
                                          struct awl13_survey_res *buf, int size);
extern int awl13_get_rssi(struct awl13_private *priv, signed char *rssi);
extern int awl13_get_rssi_u8(struct awl13_private *priv, unsigned char *rssi);
extern int awl13_get_current_mac_status(struct awl13_private *priv, unsigned char *val);
extern int awl13_get_serial_number(struct awl13_private *priv, char *buf, int size);
extern int awl13_get_key_id(struct awl13_private *priv, unsigned char *val);
extern int awl13_set_dtim_period(struct awl13_private *priv, unsigned char val);
extern int awl13_get_dtim_period(struct awl13_private *priv, unsigned char *val);
extern int awl13_set_rekey_policy(struct awl13_private *priv, unsigned char val);
extern int awl13_get_rekey_policy(struct awl13_private *priv, unsigned char *val);
extern int awl13_set_rekey_period(struct awl13_private *priv, unsigned int val);
extern int awl13_get_rekey_period(struct awl13_private *priv, unsigned int *val);
extern int awl13_set_scan_filter(struct awl13_private *priv, unsigned char val);
extern int awl13_get_scan_filter(struct awl13_private *priv, unsigned char *val);
extern int awl13_set_wps_dev_mode(struct awl13_private *priv, unsigned char val);
extern int awl13_get_wps_dev_mode(struct awl13_private *priv, unsigned char *val);
#ifdef CONFIG_ARMADILLO_WLAN_AWL13_USB
extern int awl13_set_usb_rmtwkup_time(struct awl13_private *priv, unsigned char val);
extern int awl13_get_usb_rmtwkup_time(struct awl13_private *priv, unsigned char *val);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
extern int awl13_set_wps_pin(struct awl13_private *priv, char *pin, int size);
extern int awl13_get_wps_pin(struct awl13_private *priv, char *pin, int size);
extern int awl13_set_wps_start(struct awl13_private *priv, unsigned char val);
extern int awl13_get_wps_start(struct awl13_private *priv, unsigned char *val);
extern int awl13_set_wps_cred_list(struct awl13_private *priv );
extern int awl13_get_wps_cred_list(struct awl13_private *priv );
extern void set_wid_query_request(struct awl13_packet *req,
                                  unsigned short wid);
extern void set_wid_write_request(struct awl13_packet *req,
                                  unsigned short wid,
                                  void *val,
                                  unsigned char size);
extern void set_wid_write_request_long(struct awl13_packet *req,
                                  unsigned short wid,
                                  void *val,
                                  unsigned short size);
#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
extern int awl13_wid_request(struct sdio_func *func,unsigned int timeout_ms);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
extern int awl13_usb_wid_request( struct awl13_private *priv, unsigned int timeout_ms);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */


#define to_wid_frame(p) ((struct awl13_wid_frame *)(p)->m.body)
#define to_wid_longframe(p) ((struct awl13_wid_longframe *)(p)->m.body)

/*****************************************************************************/
/* WPS status for display                                                    */
/*****************************************************************************/
typedef enum
{
    IN_PROGRESS                 = (0x00),
    ASSOC_PASS                  = (IN_PROGRESS+1),
    REG_PROT_SUCC_COMP          = (ASSOC_PASS+1),
    /*************************************************************************/
    /* Error status                                                          */
    /*************************************************************************/
    ERR_ASSOC_FAIL              = (0x80),
    ERR_SYSTEM                  = (ERR_ASSOC_FAIL+1),
    ERR_WALK_TIMEOUT            = (ERR_SYSTEM+1),
    SESSION_OVERLAP_DETECTED    = (ERR_WALK_TIMEOUT+1),
    ERR_PBC_REC_FAIL            = (SESSION_OVERLAP_DETECTED+1),
    ERR_REC_FAIL                = (ERR_PBC_REC_FAIL+1),
    ERR_REC_NACK                = (ERR_REC_FAIL+1),
    ERR_DIFF_PASS_ID_M2         = (ERR_REC_NACK+1),
    ERR_REC_WRONG_M2            = (ERR_DIFF_PASS_ID_M2+1),
    REC_M2D                     = (ERR_REC_WRONG_M2+1),
    ERR_REC_WRONG_M4            = (REC_M2D+1),
    ERR_REC_WRONG_M6            = (ERR_REC_WRONG_M4+1),
    ERR_REC_WRONG_M8            = (ERR_REC_WRONG_M6+1),
    ERR_REG_MSG_TIMEOUT         = (ERR_REC_WRONG_M8+1),
    ERR_PBC_REG_MSG_TIMEOUT     = (ERR_REG_MSG_TIMEOUT+1),
    ERR_REG_PROT_TIMEOUT        = (ERR_PBC_REG_MSG_TIMEOUT+1),
    ERR_STA_DISCONNECT          = (ERR_REG_PROT_TIMEOUT+1),

    /*************************************************************************/
    /* Configuration message status                                          */
    /*************************************************************************/
    RCV_CRED_VALUE              = (0x40),
    CRED_JOIN_FAILURE           = (RCV_CRED_VALUE+1),
    CRED_JOIN_SUCCESS           = (CRED_JOIN_FAILURE+1),
    CRED_JOIN_LIST_NULL         = (CRED_JOIN_SUCCESS+1),

    WLAN_DIS_WPS_PROT           = (0xC0)
} WPS_STATUS_T;

#define WID_DEFAULT_TIMEOUT	(1000)
#define WID_GENKEY_TIMEOUT	(20000)

#define NOFW_EXIT(p)							\
({									\
	if (atomic_read(&((p)->state)) == AWL13_STATE_NO_FW)		\
		return -EINVAL;						\
})
#define SLEEP_EXIT(p)							\
({									\
	if (atomic_read(&((p)->power_state)) == AWL13_PWR_STATE_SLEEP)	\
		return -EBUSY;						\
})

#endif /* _AWL13_WID_H_ */
