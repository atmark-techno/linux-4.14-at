/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#define MASK_I_FRAME        0x01
#define MASK_R_FRAME        0x03

#define MASK_N_S            0x0E
#define MASK_N_R            0xE0
#define MASK_P_F            0x10


#define TYPE_I              0x00
#define TYPE_RR             0x01
#define TYPE_RNR            0x05
#define TYPE_REJ            0x09


// Macros
#define HDLC_IS_I_FRAME_TYPE(c) ((c & MASK_I_FRAME) ? FALSE : TRUE)
#define HDLC_IS_R_FRAME_TYPE(c) (((c & MASK_R_FRAME) == 0x01) ? TRUE : FALSE)

#define HDLC_GET_FRAMETYPE(c)   ((c & MASK_I_FRAME) ? (c & 0x0F): TYPE_I)
#define HDLC_GET_N_S(c)         ((c & MASK_N_S) >> 1)
#define HDLC_GET_N_R(c)         ((c & MASK_N_R) >> 5)
#define HDLC_GET_P_F(c)         ((c & MASK_P_F) >> 4)



void Hdlc_Init
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    UINT8            ucWindowSize
);

BOOL Hdlc_Process
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    UINT8            ucControlField
);

void Hdlc_OwnReceiverReady
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
);

BOOL Hdlc_SendData
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    UINT8           *pData,
    UINT32           uiLen
);

void Hdlc_TimerTick
(
    MUX_INSTANCE_t  *pMux
);
