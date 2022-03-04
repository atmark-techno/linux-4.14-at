/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#ifndef __MUX_CALLBACK_H
#define __MUX_CALLBACK_H


void Mux_StartResult_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwResult
);

void Mux_MsgAvailable_cb (
    pMUX_INSTANCE_t  pMuxInst
);

void Mux_Reset_cb (
    pMUX_INSTANCE_t  pMuxInst
);

DWORD Mux_DevSendData_cb (
    pMUX_INSTANCE_t  pMuxInst,
    PBYTE            pData,
    DWORD            dwDataLen
);

void Mux_DevActivate_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwLen
);

DWORD Mux_GetFreeTxBytes_cb (
    pMUX_INSTANCE_t  pMuxInst
);

void Mux_DevSetQueueSizes_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwRxSize,
    DWORD            dwTxSize
);

BOOL Mux_DevGetV24Lines_cb (
    pMUX_INSTANCE_t  pMux,
    BOOL            *pRing,
    BOOL            *pDCD
);

BOOL Mux_DevSetV24Lines_cb (
    pMUX_INSTANCE_t  pMux,
    BOOL             bRing,
    BOOL             bDCD
);

BOOL Mux_DevSetPowerState_cb (
    pMUX_INSTANCE_t  pMux,
    POWER_CMD_e      PwrState
);

void Mux_DLCIEstablishResult_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwDLCI,
    DWORD            dwResult
);

void Mux_DLCIReleaseResult_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwDLCI,
    DWORD            dwResult
);

void Mux_DLCISendDataReady_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwDLCI,
    DWORD            dwLen
);

DWORD Mux_DLCIReceiveData_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwDLCI,
    PBYTE            pData,
    DWORD            dwDataLen,
    DWORD            dwBytesInBuffer
);

void Mux_DLCIReceiceV24Status_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwDLCI,
    V24STATUS        V24Status
);

void Mux_DLCIEscReceived_cb (
    pMUX_INSTANCE_t  pMuxInst,
    DWORD            dwDLCI
);

#endif // __MUX_CALLBACK_H__
