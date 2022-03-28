/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#include "global.h"
#include "ddmpglob.h"
#include "ddmpdlch.h"
#include "mux_callback.h"
#include "mux_interface.h"
#include "mux_msgqueue.h"


void MP_vInitAdaptation // setup some frameworks vars
(
    MUX_INSTANCE_t  *pMux
)
{
    pMux->MP_uiInternalVersion = (UINT16)pMux->dwMpRevision;
}


UINT32 MP_uiDevDataSend
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *Data,
    UINT32           len
)
{
    return Mux_DevSendData_cb(pMux, (PBYTE)Data, len);
}
//---------------------------------------------------------------------------




UINT32 MP_DevGetFreeBytesInPhysTXBuf
(
     MUX_INSTANCE_t  *pMux
)
{
    return Mux_GetFreeTxBytes_cb(pMux);
}
//---------------------------------------------------------------------------



void MP_DevActivateCallback
(
    MUX_INSTANCE_t     *pMux,
    UINT32              Len,
    CallbackTXBufFun    CallbackFun
)
{
    Mux_DevActivate_cb (pMux, Len);
}
//---------------------------------------------------------------------------




/*
    ==========================================================
    access the connected applications: read/write demuxed data
    ==========================================================
*/

void MP_vAppSendV24Status
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci,
    V24STATUS        v24Status
)
{
    Mux_DLCIReceiceV24Status_cb(pMux, (DWORD)ucDlci, v24Status);
}
//---------------------------------------------------------------------------

void MP_vAppSendEsc
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    Mux_DLCIEscReceived_cb(pMux, (DWORD)ucDlci);
}
//---------------------------------------------------------------------------

void MP_vAppReceiveDataCallback
(
    MUX_INSTANCE_t  *pMux,
    UINT32           len,
    UINT8            ucDlci
)
{
    Mux_DLCISendDataReady_cb(pMux, (DWORD)ucDlci, len);
}
//---------------------------------------------------------------------------


UINT32 MP_vSendDataMP_to_App
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci,
    UINT8           *pucBuffer,
    UINT32           uiLen,
    UINT32           uiBytesInBuffer
)
{
    return Mux_DLCIReceiveData_cb(pMux, (DWORD)ucDlci, pucBuffer, uiLen, uiBytesInBuffer);
}
//---------------------------------------------------------------------------



void MP_PostUserMessage
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MuxMsg_Put (pMux, psPrimitive);
}
//---------------------------------------------------------------------------


UINT8 MP_ucGetMessage
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *pstMpPrimitive,
    UINT8            ucMsgSize
)
{
    return (MuxMsg_Get(pMux, pstMpPrimitive) == MUXMSG_OK) ? TRUE : FALSE;
}
//---------------------------------------------------------------------------

