/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#include "global.h"
#include "mux_interface.h"
#include "mux_callback.h"
#include "mux_msgqueue.h"
#include "ddmpdlch.h"
#include "ddmphdlc.h"


//============================================================================
// private functions
//============================================================================

//////////////////////////////////////////////////////////////////////////////
static pDLCI_INSTANCE_t DLCI_GetProp (
    pMUX_INSTANCE_t      pMux,
    DWORD                dwDLCI
)
{
    // check: is DLCI valid
    if ((dwDLCI < DLCI1) || (dwDLCI >= pMux->dwMaxNumberOfDLCI)) {
        return NULL;
    }
    return &pMux->pDLCIArray[dwDLCI];
}

//////////////////////////////////////////////////////////////////////////////
static DWORD DLCI_SaveProp (
    pMUX_INSTANCE_t      pMux,
    DWORD                dwDLCI,
    DWORD                dwUserData,
    pDLCI_INTERFACE_t    pDLCIInterface
)
{
    pDLCI_INSTANCE_t pDLCI = DLCI_GetProp(pMux, dwDLCI);

    // check: is DLCI valid
    if (pDLCI == NULL) {
        return MUX_ERR_WRONG_DLCI;
    }

    // check DLCI interface
    if (    (NULL == pDLCIInterface->pDlciEstablishResult)
         || (NULL == pDLCIInterface->pDlciReleaseResult)
         || (NULL == pDLCIInterface->pDlciSendDataContinue)
         || (NULL == pDLCIInterface->pDlciReceiveData)
         || (NULL == pDLCIInterface->pDlciReceiveV24Status)
         || (NULL == pDLCIInterface->pDlciShutdown) )
    {
        return MUX_ERR_INVALID_PARAMETER;
    }

    // set DLCI information
    pDLCI->dwUserData     =   dwUserData;
    pDLCI->DLCIInterface  =  *pDLCIInterface;

    // init DLCI
    MEMCLR(&pDLCI->sMP_DLCI, sizeof(MPVAR));
    MEMCLR(&pDLCI->sMP_Info, sizeof(MP_INFOBLOCK));

    pDLCI->sMP_DLCI.State               = DLC_DISCONNECTED;
    pDLCI->sMP_DLCI.sV24Status.DTR      = 1;
    pDLCI->sMP_DLCI.sV24Status.RTS      = 1;
    pDLCI->sMP_DLCI.sV24Status.CTS      = 1;
    pDLCI->sMP_DLCI.sV24Status.DSR      = 1;
    pDLCI->sMP_DLCI.sV24Status.TxEmpty  = 1;
    pDLCI->sMP_DLCI.sV24Status.TxFull   = 0;

    pDLCI->sMP_DLCI.uiFrameSize         = DEF_MAXFRAMESIZE;

    pDLCI->MP_RxRingBuf.Count           = 0;
    pDLCI->MP_RxRingBuf.ReadIndex       = 0;
    pDLCI->MP_RxRingBuf.WriteIndex      = 0;

    pDLCI->MP_TxRingBuf.Count           = 0;
    pDLCI->MP_TxRingBuf.ReadIndex       = 0;
    pDLCI->MP_TxRingBuf.WriteIndex      = 0;

    return MUX_OK;
}

//////////////////////////////////////////////////////////////////////////////
static DWORD DLCI_ClearProp (
    pMUX_INSTANCE_t      pMux,
    DWORD                dwDLCI
)
{
    pDLCI_INSTANCE_t pDLCI = DLCI_GetProp(pMux, dwDLCI);

    // check: is DLCI valid
    if (pDLCI == NULL) {
        return MUX_ERR_WRONG_DLCI;
    }

    // clear DLCI
    MEMCLR(&pDLCI->DLCIInterface, sizeof(DLCI_INTERFACE_t));
    pDLCI->dwUserData = 0;

    return MUX_OK;
}

//////////////////////////////////////////////////////////////////////////////
DWORD  Dlci_GetConnected (
    pMUX_INSTANCE_t      pMux,
    DWORD                dwDLCI
)
{
    pDLCI_INSTANCE_t  pDLCIProp;
    DWORD             dwRet = MUX_OK;

    pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp == NULL) {
        dwRet = MUX_ERR_WRONG_DLCI;
    } else {
        // is DLCI connected
        if (pDLCIProp->sMP_DLCI.State != DLC_CONNECTED) {
            dwRet = MUX_ERR_DLCI_NOT_CONNECTED;
        }
    }
    return dwRet;
}


//============================================================================
// callback functions
//============================================================================

//////////////////////////////////////////////////////////////////////////////
void Mux_Lock_cb (
    pMUX_INSTANCE_t  pMux
)
{
    if (pMux->MuxInterface.pMuxLock) {
        pMux->MuxInterface.pMuxLock((DWORD)pMux, pMux->dwMuxUserData);
    }
}

//////////////////////////////////////////////////////////////////////////////
void Mux_Unlock_cb (
    pMUX_INSTANCE_t  pMux
)
{
    if (pMux->MuxInterface.pMuxUnlock) {
        pMux->MuxInterface.pMuxUnlock((DWORD)pMux, pMux->dwMuxUserData);
    }
}

// MUX frame and device

//////////////////////////////////////////////////////////////////////////////
void Mux_StartResult_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwResult
)
{
    if (pMux->MuxInterface.pMuxStartResult) {
        pMux->MuxInterface.pMuxStartResult((DWORD)pMux, pMux->dwMuxUserData, dwResult);
    }
}

//////////////////////////////////////////////////////////////////////////////
void Mux_Reset_cb (
    pMUX_INSTANCE_t  pMux
)
{
    if (pMux->MuxInterface.pMuxReset) {
        pMux->MuxInterface.pMuxReset((DWORD)pMux, pMux->dwMuxUserData);
    }
    MP_ResetResync(pMux);
}

//////////////////////////////////////////////////////////////////////////////
DWORD Mux_DevSendData_cb (
    pMUX_INSTANCE_t  pMux,
    PBYTE            pData,
    DWORD            dwDataLen
)
{
    if (pMux->MuxInterface.pMuxDevSendData) {
        return pMux->MuxInterface.pMuxDevSendData((DWORD)pMux, pMux->dwMuxUserData, pData, dwDataLen);
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
void Mux_DevActivate_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwLen
)
{
    if (pMux->MuxInterface.pMuxDevActivate) {
        pMux->MuxInterface.pMuxDevActivate((DWORD)pMux, pMux->dwMuxUserData, dwLen);
    }
}

//////////////////////////////////////////////////////////////////////////////
DWORD Mux_GetFreeTxBytes_cb (
    pMUX_INSTANCE_t  pMux
)
{
    if (pMux->MuxInterface.pMuxDevGetFreeTxBytes) {
        return pMux->MuxInterface.pMuxDevGetFreeTxBytes((DWORD)pMux, pMux->dwMuxUserData);
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
void Mux_DevSetQueueSizes_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwRxSize,
    DWORD            dwTxSize
)
{
    if (pMux->MuxInterface.pMuxDevSetQueueSizes) {
        pMux->MuxInterface.pMuxDevSetQueueSizes((DWORD)pMux, pMux->dwMuxUserData, dwRxSize, dwTxSize);
    }
}

//////////////////////////////////////////////////////////////////////////////
BOOL Mux_DevGetV24Lines_cb (
    pMUX_INSTANCE_t  pMux,
    BOOL            *pRing,
    BOOL            *pDCD
)
{
    if (pMux->MuxInterface.pMuxDevGetV24Lines) {
        return pMux->MuxInterface.pMuxDevGetV24Lines((DWORD)pMux, pMux->dwMuxUserData, pRing, pDCD);
    }
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
BOOL Mux_DevSetV24Lines_cb (
    pMUX_INSTANCE_t  pMux,
    BOOL             bRing,
    BOOL             bDCD
)
{
    if (pMux->MuxInterface.pMuxDevSetV24Lines) {
        return pMux->MuxInterface.pMuxDevSetV24Lines((DWORD)pMux, pMux->dwMuxUserData, bRing, bDCD);
    }
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
BOOL Mux_DevSetPowerState_cb (
    pMUX_INSTANCE_t  pMux,
    POWER_CMD_e      PwrState
)
{
    if (pMux->MuxInterface.pMuxDevSetPowerState) {
        return pMux->MuxInterface.pMuxDevSetPowerState((DWORD)pMux, pMux->dwMuxUserData, PwrState);
    }
    return FALSE;
}

// DLCI

//////////////////////////////////////////////////////////////////////////////
void Mux_DLCIEstablishResult_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwDLCI,
    DWORD            dwResult
)
{
    pDLCI_INSTANCE_t pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp) {
        if (pDLCIProp->DLCIInterface.pDlciEstablishResult) {
            pDLCIProp->DLCIInterface.pDlciEstablishResult((DWORD)pMux, pDLCIProp->dwUserData, dwDLCI, dwResult);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
void Mux_DLCIReleaseResult_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwDLCI,
    DWORD            dwResult
)
{
    pDLCI_INSTANCE_t pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp) {
        if (pDLCIProp->DLCIInterface.pDlciReleaseResult) {
            pDLCIProp->DLCIInterface.pDlciReleaseResult((DWORD)pMux, pDLCIProp->dwUserData, dwDLCI, dwResult);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
void Mux_DLCISendDataReady_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwDLCI,
    DWORD            dwLen
)
{
    pDLCI_INSTANCE_t pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp) {
        if (pDLCIProp->DLCIInterface.pDlciSendDataContinue) {
            pDLCIProp->DLCIInterface.pDlciSendDataContinue((DWORD)pMux, pDLCIProp->dwUserData, dwDLCI, dwLen);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
DWORD Mux_DLCIReceiveData_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwDLCI,
    PBYTE            pData,
    DWORD            dwDataLen,
    DWORD            dwBytesInBuffer
)
{
    pDLCI_INSTANCE_t pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp) {
        if (pDLCIProp->DLCIInterface.pDlciReceiveData) {
            return pDLCIProp->DLCIInterface.pDlciReceiveData((DWORD)pMux, pDLCIProp->dwUserData, dwDLCI, pData, dwDataLen, dwBytesInBuffer);
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
void Mux_DLCIReceiceV24Status_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwDLCI,
    V24STATUS        V24Status
)
{
    pDLCI_INSTANCE_t pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp) {
        if (pDLCIProp->DLCIInterface.pDlciReceiveV24Status) {
            MUX_V24STATUS_t MuxV24;

            MuxV24.BreakInd       = V24Status.BreakInd;
            MuxV24.BreakReq       = V24Status.BreakReq;
            MuxV24.CTS            = V24Status.CTS;
            MuxV24.DCD            = V24Status.DCD;
            MuxV24.DSR            = V24Status.DSR;
            MuxV24.DTR            = V24Status.DTR;
            MuxV24.InFlowActive   = V24Status.InFlowActive;
            MuxV24.OutFlowActive  = V24Status.OutFlowActive;
            MuxV24.RI             = V24Status.RI;
            MuxV24.RTS            = V24Status.RTS;
            MuxV24.TxEmpty        = V24Status.TxEmpty;
            MuxV24.TxFull         = V24Status.TxFull;

            pDLCIProp->DLCIInterface.pDlciReceiveV24Status((DWORD)pMux, pDLCIProp->dwUserData, dwDLCI, MuxV24);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
void Mux_DLCIShutdown_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwDLCI
)
{
    pDLCI_INSTANCE_t pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp) {
        if (pDLCIProp->DLCIInterface.pDlciShutdown) {
            pDLCIProp->DLCIInterface.pDlciShutdown((DWORD)pMux, pDLCIProp->dwUserData, dwDLCI);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
void Mux_DLCIEscReceived_cb (
    pMUX_INSTANCE_t  pMux,
    DWORD            dwDLCI
)
{
    pDLCI_INSTANCE_t pDLCIProp = DLCI_GetProp(pMux, dwDLCI);
    if (pDLCIProp) {
        if (pDLCIProp->DLCIInterface.pDlciEscReceived) {
          pDLCIProp->DLCIInterface.pDlciEscReceived((DWORD)pMux, pDLCIProp->dwUserData, dwDLCI);
        }
    }
}


//============================================================================
// public interface
//============================================================================

//////////////////////////////////////////////////////////////////////////////
DWORD Mux_Register (
    DWORD            dwUserData,
    pMUX_INTERFACE_t pMuxInterface,
    DWORD            dwMaxDlci,
    DWORD            dwMpRevision,
    PDWORD           phMuxInstance
)
{
    DWORD            dwMemSize = 0;
    pMUX_INSTANCE_t  pMux = NULL;

    // check interface
    if (     (NULL == pMuxInterface->pMuxDevActivate)
         ||  (NULL == pMuxInterface->pMuxDevGetFreeTxBytes)
         ||  (NULL == pMuxInterface->pMuxDevSetQueueSizes)
         ||  (NULL == pMuxInterface->pMuxDevSendData)
         ||  (NULL == pMuxInterface->pMuxMsgAvailable)
         ||  (NULL == pMuxInterface->pMuxReset)
         ||  (NULL == pMuxInterface->pMuxLock)
         ||  (NULL == pMuxInterface->pMuxUnlock)
         ||  (NULL == pMuxInterface->pMuxStartResult)
         ||  (NULL == pMuxInterface->pMuxGetMem)
         ||  (NULL == pMuxInterface->pMuxFreeMem)
    ) {
        return MUX_ERR_INVALID_PARAMETER;
    }

    // memory for mux instance
    dwMemSize = sizeof(MUX_INSTANCE_t);
    pMux = pMuxInterface->pMuxGetMem(dwUserData, dwMemSize);
    if (pMux == NULL) {
        return MUX_ERR_GET_MEMORY_FAILED;
    }
    MEMCLR(pMux, dwMemSize);
    pMux->dwMuxUserData  = dwUserData;
    pMux->MuxInterface = *pMuxInterface;

    // memory for dlci instances
    dwMemSize = sizeof(DLCI_INSTANCE_t) * (dwMaxDlci + 1); // + 1, for DLCI0 (control channel)
    pMux->pDLCIArray = pMuxInterface->pMuxGetMem(dwUserData, dwMemSize);
    if (pMux->pDLCIArray == NULL) {
        Mux_Deregister((DWORD)pMux);
        return MUX_ERR_GET_MEMORY_FAILED;
    }
    MEMCLR(pMux->pDLCIArray, dwMemSize);

    // memory for ringbuffers
    pMux->RingBufferStatic.RxBuf = pMuxInterface->pMuxGetMem(dwUserData, __MP_RX_RINGBUF_SIZE * dwMaxDlci);
    if (pMux->RingBufferStatic.RxBuf == NULL) {
        Mux_Deregister((DWORD)pMux);
        return MUX_ERR_GET_MEMORY_FAILED;
    }

    pMux->RingBufferStatic.TxBuf = pMuxInterface->pMuxGetMem(dwUserData, __MP_TX_RINGBUF_SIZE * dwMaxDlci);
    if (pMux->RingBufferStatic.TxBuf == NULL) {
        Mux_Deregister((DWORD)pMux);
        return MUX_ERR_GET_MEMORY_FAILED;
    }

    // Frame and window size
    pMux->dwMpWindowSize = HDLC_DEF_WINDOW_SIZE;
    pMux->dwMpFrameSize = MAXFRAMESIZE;

    // mux protocol revision
    pMux->dwMpRevision = MINMAX(dwMpRevision, (DWORD)MP_PROTOCOL_VERSION_MIN, (DWORD)MP_PROTOCOL_VERSION_MAX);

    pMux->dwMaxNumberOfDLCI = dwMaxDlci + 1;

    MuxMsg_Init(pMux);

    *phMuxInstance = (DWORD)pMux;

    return MUX_OK;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Mux_Deregister (
    DWORD           hMuxInstance
)
{
    pMUX_INSTANCE_t pMux = (pMUX_INSTANCE_t)hMuxInstance;

    if (pMux == NULL) {
        return MUX_ERR_INVALID_PARAMETER;
    }

    if (pMux->pDLCIArray)              pMux->MuxInterface.pMuxFreeMem(pMux->dwMuxUserData, pMux->pDLCIArray);
    if (pMux->RingBufferStatic.RxBuf)  pMux->MuxInterface.pMuxFreeMem(pMux->dwMuxUserData, pMux->RingBufferStatic.RxBuf);
    if (pMux->RingBufferStatic.TxBuf)  pMux->MuxInterface.pMuxFreeMem(pMux->dwMuxUserData, pMux->RingBufferStatic.TxBuf);

    pMux->MuxInterface.pMuxFreeMem(pMux->dwMuxUserData, pMux);

    return MUX_OK;
}

//////////////////////////////////////////////////////////////////////////////
void Mux_Init
(
    DWORD           hMuxInstance,
    DWORD           dwWindowSize,
    DWORD           dwFrameSize,
    DWORD           dwBaudRate
)
{
  pMUX_INSTANCE_t pMux = (pMUX_INSTANCE_t)hMuxInstance;

  Mux_Lock_cb(pMux);

  pMux->dwMpWindowSize = MINMAX(dwWindowSize, (DWORD)1, (DWORD)7);
  pMux->dwMpFrameSize = MINMAX(dwFrameSize, (DWORD)DEF_MAXFRAMESIZE, (DWORD)MAXFRAMESIZE);
  pMux->dwMpBaudRate = dwBaudRate;

  MP_Init(pMux);

  MP_AdjustQueueSize(pMux);

  Mux_Unlock_cb(pMux);
}

//////////////////////////////////////////////////////////////////////////////
DWORD Mux_Start (
    DWORD           hMuxInstance
)
{
    pMUX_INSTANCE_t pMux = (pMUX_INSTANCE_t)hMuxInstance;

    Mux_Lock_cb(pMux);

    // process first mux message
    MP_vProc(pMux);
    // establishment of the control channel DLCI0
    MP_PostSimpleUserMessage(pMux, MP_ReqSABM_DLCI0, DLCI0);

    Mux_Unlock_cb(pMux);

    return MUX_OK;
}

//////////////////////////////////////////////////////////////////////////////
void Mux_Stop (
    DWORD           hMuxInstance
)
{
    pMUX_INSTANCE_t pMux = (pMUX_INSTANCE_t)hMuxInstance;

    Mux_Lock_cb(pMux);

    if (pMux->ucInitiator) {
        MP_StartShutdownProc(pMux);
    }

    Mux_Unlock_cb(pMux);
}

//////////////////////////////////////////////////////////////////////////////
void Mux_Shutdown (
    DWORD           hMuxInstance
)
{
    if (hMuxInstance) {
        pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
        DWORD            i;

        Mux_Lock_cb (pMux);

        // shutdown callback for all DLCI
        for (i = 1; i < pMux->dwMaxNumberOfDLCI; i++) {
            if (Dlci_GetConnected(pMux, i) == MUX_OK) {
                Mux_DLCIShutdown_cb(pMux, i);
            }
        }

        MuxMsg_Init(pMux);
        // standard initialization functions for the mux parts
        MP_vInitScanner(pMux);
        MP_vMemInit(pMux);
        MP_vStatemachineInit(pMux);
        MP_ResetResync(pMux);

        Mux_Unlock_cb(pMux);
    }
}

//////////////////////////////////////////////////////////////////////////////
DWORD Mux_SendPowerCmd (
    DWORD           hMuxInstance,
    POWER_CMD_e     PwrCmd
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet = MUX_OK;

    Mux_Lock_cb(pMux);

    if (pMux->ucInitiator) {
        switch (PwrCmd)
        {
            case MUX_POWER_MODE_DEFAULT:
                MP_SleepReqEx(pMux, PSC_MODE_DEFAULT);
                break;
            case MUX_POWER_MODE_FULL:
                MP_SleepReqEx(pMux, PSC_MODE_FULL);
                break;
            case MUX_POWER_MODE_SLEEP:
                MP_SleepReqEx(pMux, PSC_MODE_SLEEP);
                break;
            case MUX_POWER_MODE_CYCLIC_SLEEP_SHORT:
                MP_SleepReqEx(pMux, PSC_MODE_CYCLIC_SLEEP_SHORT);
                break;
            case MUX_POWER_MODE_CYCLIC_SLEEP_LONG:
                MP_SleepReqEx(pMux, PSC_MODE_CYCLIC_SLEEP_LONG);
                break;
            case MUX_POWER_MODE_SWITCH_OFF:
                MP_SleepReqEx(pMux, PSC_MODE_SWITCH_OFF);
                break;
            case MUX_POWER_MODE_RESET:
                MP_SleepReqEx(pMux, PSC_MODE_RESET);
                break;
            case MUX_POWER_MODE_CYCLIC_SLEEP_SHORT_CONT:
                MP_SleepReqEx(pMux, PSC_MODE_CYCLIC_SLEEP_SHORT_CONT);
                break;
            case MUX_POWER_MODE_CYCLIC_SLEEP_LONG_CONT:
                MP_SleepReqEx(pMux, PSC_MODE_CYCLIC_SLEEP_LONG_CONT);
                break;
            case MUX_POWER_MODE_CYCLIC_SLEEP_LONG_CONT2:
                MP_SleepReqEx(pMux, PSC_MODE_CYCLIC_SLEEP_LONG_CONT2);
                break;
            default:
                dwRet = MUX_ERR_WRONG_POWER_CMD;
                break;
        }
    } else {
        dwRet = MUX_ERR_CLIENT_NOT_ALLOWED;
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
BOOL Mux_MsgTick (
    DWORD           hMuxInstance
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    BOOL             fRet;

    Mux_Lock_cb(pMux);

    MP_vProc(pMux);
    fRet = MuxMsg_Exist(pMux);

    Mux_Unlock_cb(pMux);

    return fRet;
}

//////////////////////////////////////////////////////////////////////////////
void Mux_TimerTick (
    DWORD           hMuxInstance
)
{
    pMUX_INSTANCE_t pMux = (pMUX_INSTANCE_t)hMuxInstance;

    Mux_Lock_cb(pMux);

    MP_CheckResyncTimeOut(pMux);
    Hdlc_TimerTick(pMux);

    Mux_Unlock_cb(pMux);
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_Register (
    DWORD                hMuxInstance,
    DWORD                dwDLCI,
    DWORD                dwUserData,
    pDLCI_INTERFACE_t    pDLCIInterface
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet;

    Mux_Lock_cb(pMux);

    dwRet = DLCI_SaveProp(pMux, dwDLCI, dwUserData, pDLCIInterface);

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_Deregister
(
  DWORD                hMuxInstance,
  DWORD                dwDLCI
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet;

    Mux_Lock_cb(pMux);

    dwRet = DLCI_ClearProp(pMux, dwDLCI);

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_Establish (
    DWORD                hMuxInstance,
    DWORD                dwDLCI
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet = MUX_ERR_WRONG_DLCI;

    Mux_Lock_cb(pMux);

    // check: is DLCI valid
    if (DLCI_GetProp(pMux, dwDLCI)) {
        Hdlc_Init(pMux, (UINT8)dwDLCI, (UINT8)pMux->dwMpWindowSize);
        MP_vInitPacketHandler(pMux, (UINT8)dwDLCI);
        if (pMux->ucInitiator) {
            MP_PostSimpleUserMessage(pMux, MP_ReqSABM, (UINT8)dwDLCI);
        }
        dwRet = MUX_OK;
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_Release (
    DWORD           hMuxInstance,
    DWORD           dwDLCI
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet = MUX_ERR_CLIENT_NOT_ALLOWED;

    Mux_Lock_cb(pMux);

    if (pMux->ucInitiator) {
        // check: is DLCI valid
        dwRet = MUX_ERR_WRONG_DLCI;
        if (DLCI_GetProp(pMux, dwDLCI)) {
            // Initiate disconnection
            MP_PostSimpleUserMessage(pMux, MP_ReqDISC, (UINT8)dwDLCI);
            dwRet = MUX_OK;
        }
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD  Dlci_IsConnected (
    DWORD           hMuxInstance,
    DWORD           dwDLCI
)
{
    pMUX_INSTANCE_t   pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD             dwRet;

    Mux_Lock_cb(pMux);

    dwRet = Dlci_GetConnected(pMux, dwDLCI);

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_SendData (
    DWORD           hMuxInstance,
    DWORD           dwDLCI,
    PBYTE           pData,
    PDWORD          pdwDataLen
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwTransfer = *pdwDataLen;
    DWORD            dwRet;

    *pdwDataLen = 0;

    Mux_Lock_cb(pMux);

    dwRet = Dlci_GetConnected(pMux, dwDLCI);
    if (dwRet == MUX_OK) {
        *pdwDataLen = MP_uiAppDataReceive(pMux, pData, dwTransfer, (UINT8)dwDLCI);
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_ReceiveDataContinue (
    DWORD           hMuxInstance,
    DWORD           dwDLCI
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet;

    Mux_Lock_cb(pMux);

    dwRet = Dlci_GetConnected(pMux, dwDLCI);
    if (dwRet == MUX_OK) {
        MP_vAppDataSendCallback(pMux, (UINT8)dwDLCI);
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_SendEsc (
    DWORD           hMuxInstance,
    DWORD           dwDLCI
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet = MUX_ERR_CLIENT_NOT_ALLOWED;

    Mux_Lock_cb(pMux);

    if (pMux->ucInitiator) {
        dwRet = Dlci_GetConnected(pMux, dwDLCI);
        if (dwRet == MUX_OK) {
            MP_vSendESC(pMux, (UINT8)dwDLCI);
        }
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_SetV24Status (
    DWORD            hMuxInstance,
    DWORD            dwDLCI,
    MUX_V24STATUS_t  MuxV24Status
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet;

    Mux_Lock_cb(pMux);

    dwRet = Dlci_GetConnected(pMux, dwDLCI);
    if (dwRet == MUX_OK) {
        V24STATUS V24Status;
        MEMCLR(&V24Status, sizeof(V24STATUS));
        V24Status.BreakInd       = MuxV24Status.BreakInd;
        V24Status.BreakReq       = MuxV24Status.BreakReq;
        V24Status.CTS            = MuxV24Status.CTS;
        V24Status.DCD            = MuxV24Status.DCD;
        V24Status.DSR            = MuxV24Status.DSR;
        V24Status.DTR            = MuxV24Status.DTR;
        V24Status.InFlowActive   = MuxV24Status.InFlowActive;
        V24Status.OutFlowActive  = MuxV24Status.OutFlowActive;
        V24Status.RI             = MuxV24Status.RI;
        V24Status.RTS            = MuxV24Status.RTS;
        MP_vAppSetV24Status(pMux, V24Status, (UINT8)dwDLCI);
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_GetQueueInformation (
    DWORD           hMuxInstance,
    DWORD           dwDLCI,
    PDWORD          pdwBytesInQueueRx,
    PDWORD          pdwBytesInQueueTx,
    PDWORD          pdwSizeOfQueueRx,
    PDWORD          pdwSizeOfQueueTx
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet;

    Mux_Lock_cb(pMux);

    dwRet = Dlci_GetConnected(pMux, dwDLCI);
    if (dwRet == MUX_OK) {
        // tx buffer
        if (pdwBytesInQueueTx)  *pdwBytesInQueueTx = pMux->pDLCIArray[dwDLCI].MP_TxRingBuf.Count;
        if (pdwSizeOfQueueTx)   *pdwSizeOfQueueTx  = pMux->pDLCIArray[dwDLCI].MP_TxRingBuf.BufSize;
        // rx buffer
        if (pdwBytesInQueueRx)  *pdwBytesInQueueRx = pMux->pDLCIArray[dwDLCI].MP_RxRingBuf.Count;
        if (pdwSizeOfQueueRx)   *pdwSizeOfQueueRx  = pMux->pDLCIArray[dwDLCI].MP_RxRingBuf.BufSize;
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_EmptyQueues (
    DWORD           hMuxInstance,
    DWORD           dwDLCI,
    BOOL            fEmptyRx,
    BOOL            fEmptyTx
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwRet;

    Mux_Lock_cb(pMux);

    dwRet = Dlci_GetConnected(pMux, dwDLCI);
    if (dwRet == MUX_OK) {
        if (fEmptyRx) {
            MP_pRxRingBuf pRxBuf = &pMux->pDLCIArray[dwDLCI].MP_RxRingBuf;

            pRxBuf->Count       = 0;
            pRxBuf->ReadIndex   = 0;
            pRxBuf->WriteIndex  = 0;

            // Restart possibly blocked data flow
            MP_vSetFlowControl(pMux, (UINT8)dwDLCI);
        }

        if (fEmptyTx) {
            MP_pTxRingBuf pTxBuf = &pMux->pDLCIArray[dwDLCI].MP_TxRingBuf;

            if (pMux->MP_uiUsedProtVersion < MP_REVISION_04) {
                pTxBuf->Count       = 0;
                pTxBuf->ReadIndex   = 0;
                pTxBuf->WriteIndex  = 0;
            } else {
                pTxBuf->Count       = 0;
                pTxBuf->WriteIndex  = pTxBuf->Packets.MaxWrOffset;
            }
        }
    }

    Mux_Unlock_cb(pMux);

    return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
DWORD Dev_ReceiveData (
    DWORD           hMuxInstance,
    PBYTE           pData,
    PDWORD          pdwDataLen
)
{
    pMUX_INSTANCE_t  pMux = (pMUX_INSTANCE_t)hMuxInstance;
    DWORD            dwTransfer = *pdwDataLen;

    Mux_Lock_cb(pMux);

    *pdwDataLen = MP_uiDevDataReceive(pMux, pData, dwTransfer);

    Mux_Unlock_cb(pMux);

    return MUX_OK;
}


//////////////////////////////////////////////////////////////////////////////
void Dev_SendDataContinue (
    DWORD           hMuxInstance,
    DWORD           dwLen
)
{
    pMUX_INSTANCE_t pMux = (pMUX_INSTANCE_t)hMuxInstance;

    Mux_Lock_cb(pMux);

    MP_vDevDataSendCallback(pMux, dwLen);

    Mux_Unlock_cb(pMux);
}
