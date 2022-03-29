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
#include "ddmpiofunc.h"

#include "ddmphdlc.h"


#define MP_vStartHdlcTimer()
#define MP_vActivateDataCallbackMP_to_App(a)

#define GET_HDLC()     &pMux->pDLCIArray[ucDLCI].sHDLC


//****************************************************************************
// procedures for hdlc-protocol
//****************************************************************************
//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_StartT1
(
    DLCI_HDLC_t     *pHDLC
)
{
    pHDLC->T1_Timer = 1 + HDLC_TO_T1;
    MP_vStartHdlcTimer();
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_StartClearRejTimer
(
    DLCI_HDLC_t     *pHDLC
)
{
    pHDLC->T_ClearRej = 1 + HDLC_TO_CLEAR_REJ;
    MP_vStartHdlcTimer();
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_StopClearRejTimer
(
    DLCI_HDLC_t     *pHDLC
)
{
    pHDLC->T_ClearRej = 0;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_StopT1
(
    DLCI_HDLC_t     *pHDLC
)
{
    pHDLC->T1_Timer = 0;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
eMP_HdlcEvent Hdlc_DecodeEvent
(
    UINT8           ucControlField,
    UINT8           *pN_S,
    UINT8           *pN_R,
    UINT8           *pP_F
)
{
    *pN_S = HDLC_GET_N_S(ucControlField);
    *pN_R = HDLC_GET_N_R(ucControlField);
    *pP_F = HDLC_GET_P_F(ucControlField);


    switch (HDLC_GET_FRAMETYPE(ucControlField)) {
        case TYPE_I:
            return eHDLC_I;
        case TYPE_RR:
            *pN_S = 0;
            return eHDLC_RR;
        case TYPE_RNR:
            *pN_S = 0;
            return eHDLC_RNR;
        case TYPE_REJ:
            *pN_S = 0;
            return eHDLC_REJ;
        default:
            break;
    }

    *pN_S = 0;
    *pN_R = 0;
    *pP_F = 0;
    return eHDLC_Invalid;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_IsValid_N_R
(
    DLCI_HDLC_t     *pHDLC
)
{
    BOOL                bResult = FALSE;
    UINT8               tmp_V_A, tmp_V_S, tmp_N_R;
    UINT8               move;

    tmp_V_A = pHDLC->V_A;
    tmp_V_S = pHDLC->V_S;
    tmp_N_R = pHDLC->N_R;

    // move the values (V_A, N_R, V_S) to the begin of the number range
    move = 8 - tmp_V_A;

    // tmp = (8 - tmp_V_A) % 8
    tmp_V_A += move;
    tmp_V_A -= (tmp_V_A >> 3) << 3;

    tmp_V_S += move;
    tmp_V_S -= (tmp_V_S >> 3) << 3;

    tmp_N_R += move;
    tmp_N_R -= (tmp_N_R >> 3) << 3;

    // a valid N(R) -> V(A) <= N(R) <= V(S)
    if ((tmp_N_R >= tmp_V_A) && (tmp_N_R <= tmp_V_S))
    {
        bResult = TRUE;
    }

    // if bResult == FALSE -> N(R) ERROR
    return bResult;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_CanBeSent
(
    DLCI_HDLC_t     *pHDLC
)
{
    BOOL                bResult = FALSE;
    UINT8               tmp_V_A, tmp_V_S;
    UINT8               move;

    tmp_V_A = pHDLC->V_A;
    tmp_V_S = pHDLC->V_S;

    // move the values (V_A, N_R, V_S) to the begin of the number range
    move = 8 - tmp_V_A;

    // tmp = (8 - tmp_V_A) % 8
    tmp_V_A += move;
    tmp_V_A -= (tmp_V_A >> 3) << 3;

    tmp_V_S += move;
    tmp_V_S -= (tmp_V_S >> 3) << 3;

    if (tmp_V_S < (tmp_V_A + pHDLC->WindowSize))
    {
        bResult = TRUE;
    }

    return bResult;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_Inc_V_R
(
    DLCI_HDLC_t     *pHDLC
)
{
    pHDLC->V_R++;

    // modulo 8
    pHDLC->V_R -=  (pHDLC->V_R >> 3) << 3;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_Inc_V_S
(
    DLCI_HDLC_t     *pHDLC
)
{
    pHDLC->V_S++;

    // modulo 8
    pHDLC->V_S -=  (pHDLC->V_S >> 3) << 3;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_Retransmission
(
    DLCI_HDLC_t     *pHDLC
)
{
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("RETRANSMITR: V(S)=%d, V(A)=%d"), pHDLC->V_S, pHDLC->V_A);

    pHDLC->fRetransActive = TRUE;
    pHDLC->V_S = pHDLC->V_A;

    // restart T1
    // Hdlc_StartT1 (pHDLC);
    // realized in function Hdlc_SendData()
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
UINT32 Hdlc_SendFrame
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    eMP_HdlcEvent    eEvent,
    UINT8           *pIFrameData,
    UINT32           uiIFrameDataLen
)
{
    DLCI_HDLC_t        *pHDLC;
    UINT8              *pCurrent = pMux->SendBuf;
    UINT8               uiHeadLen = 0;

    pHDLC = &pMux->pDLCIArray[ucDLCI].sHDLC;

    // 1 Byte StartFlag + 1 Byte Address + 1 Byte Control [+ 1 or 2 Byte length + data] + 1 Byte CRC + 1 Byte EndFlag

    // check buffersize
    if (sizeof(pMux->SendBuf) < (4 + 2 + uiIFrameDataLen + 1))
    {
        // internal error -> exit
        return FALSE;
    }

    // set StartFlag
    *pCurrent++ = DLC_FRAME_FLAG_BASIC_MODE;

    // set Address
    *pCurrent++ = (UINT8)(( ucDLCI << 2) | ((0x01 << 1) | DLC_FRAME_EA_BIT_SET));

    // set Control-Field
    switch (eEvent)
    {
        case eHDLC_RR:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Snd RR N(R)=%d"), pHDLC->V_R);
            *pCurrent++ = (UINT8)(TYPE_RR | (pHDLC->P_F << 4) | (pHDLC->V_R << 5));
            break;
        case eHDLC_RNR:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Snd RNR N(R)=%d"), pHDLC->V_R);
            *pCurrent++ = (UINT8)(TYPE_RNR | (pHDLC->P_F << 4) | (pHDLC->V_R << 5));
            pMux->pDLCIArray[ucDLCI].sMP_Info.SentRNR++;
            break;
        case eHDLC_REJ:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Snd REJ N(R)=%d"), pHDLC->V_R);
            *pCurrent++ = (UINT8)(TYPE_REJ | (pHDLC->P_F << 4) | (pHDLC->V_R << 5));
            pMux->pDLCIArray[ucDLCI].sMP_Info.SentREJ++;
            break;
        case eHDLC_I:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Snd I N(S)=%d, N(R)=%d, len=%d"), pHDLC->V_S, pHDLC->V_R, uiIFrameDataLen);
            *pCurrent++ = (UINT8)(TYPE_I | (pHDLC->P_F << 4) | (pHDLC->V_S << 1) | (pHDLC->V_R << 5));
            // statistics
            pMux->pDLCIArray[ucDLCI].sMP_Info.BytesWritten += uiIFrameDataLen;
            break;
        default:
            break;
    }

    uiHeadLen = 2;

    // set length
    if (eEvent == eHDLC_I)
    {
        // - Length, normally less than 0x7F, only one byte needed --------------
        if (uiIFrameDataLen <= 0x7F)
        {
            *pCurrent++ = (UINT8)((uiIFrameDataLen & 0x7F) << 1 | DLC_FRAME_EA_BIT_SET);
            uiHeadLen++;
        }
        // - Length, will need 2 bytes ------------------------------------------
        else
        {
            *pCurrent++ = (UINT8)((uiIFrameDataLen & 0x7F) << 1);  // No E/A bit set
            *pCurrent++ = (UINT8)(uiIFrameDataLen >> 7);           // 1. bit transmits 7 Bytes only !!!
            uiHeadLen += 2;
        }

        // - Append Frame Data ----------------------------------------------------
        {
            MP_pTxRingBuf   Ptr = &pMux->pDLCIArray[ucDLCI].MP_TxRingBuf;
            UINT32          localReadIndex = pIFrameData - Ptr->Buf;

            if ((localReadIndex + uiIFrameDataLen) <= Ptr->BufSize)
            {
                MEMCPY(pCurrent, &(Ptr->Buf[localReadIndex]), uiIFrameDataLen);
                pCurrent += uiIFrameDataLen;
            }
            else
            {
                UINT32 nCopy = Ptr->BufSize - localReadIndex;

                MEMCPY(pCurrent, &(Ptr->Buf[localReadIndex]), nCopy);
                pCurrent += nCopy;
                MEMCPY(pCurrent, &(Ptr->Buf[0]), uiIFrameDataLen - nCopy);
                pCurrent += uiIFrameDataLen - nCopy;
            }
        }
    }

    // set checksum
    *pCurrent++ = ucMP_TxFCS(pMux, &(pMux->SendBuf[1]), uiHeadLen);

    // set EndFlag
    *pCurrent++ = DLC_FRAME_FLAG_BASIC_MODE;

    MP_uiDevDataSend(pMux, pMux->SendBuf, pCurrent - pMux->SendBuf);

    return (pCurrent - pMux->SendBuf);
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_IsOwnReceiverBusy
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    DLCI_HDLC_t        *pHDLC = &pMux->pDLCIArray[ucDLCI].sHDLC;
    MP_pRxRingBuf       Ptr = &pMux->pDLCIArray[pMux->ucMP_RxDLCI].MP_RxRingBuf;
    UINT32              toCopy = pMux->RxFrame.Len;

    if ((Ptr->Count + toCopy) >= Ptr->BufSize)
    {
        pHDLC->eReceiver = eReceiver_Busy;
        pMux->pDLCIArray[ucDLCI].sMP_Info.OwnBusy++;
        MP_vActivateDataCallbackMP_to_App(ucDLCI);
        return TRUE;
    }
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_Send_RR
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    (void)Hdlc_SendFrame(pMux, ucDLCI, eHDLC_RR, NULL, 0);
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_Send_RNR
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    (void)Hdlc_SendFrame(pMux, ucDLCI, eHDLC_RNR, NULL, 0);
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_Send_REJ
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    (void)Hdlc_SendFrame (pMux, ucDLCI, eHDLC_REJ, NULL, 0);
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_Handle_N_R
(
    DLCI_HDLC_t     *pHDLC
)
{
    // process N(R)
    // test for valid N(R)
    if (Hdlc_IsValid_N_R (pHDLC))
    {
        if (pHDLC->V_A != pHDLC->N_R)
        {
            if (pHDLC->N_RNR > 0)
            {
                pHDLC->N_RNR--;
            }

            pHDLC->V_A = pHDLC->N_R;

            if (pHDLC->V_A == pHDLC->V_S)
            {
                // STOP T1
                Hdlc_StopT1 (pHDLC);
            }
            else
            {
                // Restart T1
                Hdlc_StartT1 (pHDLC);
            }
        }
        return TRUE;
    }
    // N(R) Error
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("ERROR N(R): V(A)=%d, N(R)=%d, V(S)=%d"), pHDLC->V_A, pHDLC->N_R, pHDLC->V_S);
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_Handle_I_Frame
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    DLCI_HDLC_t  *pHDLC = GET_HDLC();
    BOOL          bResult = FALSE;

    if (pHDLC->eReceiver == eReceiver_Busy)
    {
        // discard data
        bResult = FALSE;

        // send RNR
        Hdlc_Send_RNR(pMux, ucDLCI);

        // clear acknowledge pending
        pHDLC->eAck     = eAck_NotPending;
    }
    else
    {
        if (pHDLC->N_S == pHDLC->V_R)
        {
            Hdlc_Inc_V_R (pHDLC);

            pHDLC->eReject  = eReject_Reset;

            Hdlc_StopClearRejTimer (pHDLC);

            pHDLC->eAck     = eAck_Pending;

            // data valid
            bResult = TRUE;
        }
        else
        {
            if (pHDLC->eReject == eReject_Reset)
            {
                // sequence error
                MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("SEQ-ERROR: N(S)=%d != V(R)=%d"), pHDLC->N_S, pHDLC->V_R);

                // discard data
                bResult = FALSE;

                // send REJ
                Hdlc_Send_REJ (pMux, ucDLCI);

                pHDLC->eReject  = eReject_Set;

                // start timeout for clear reject state
                Hdlc_StartClearRejTimer (pHDLC);

                // clear acknowledge pending
                pHDLC->eAck     = eAck_NotPending;
            }
        }
    }

    (void)Hdlc_Handle_N_R (pHDLC);

    return bResult;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_Handle_RR_Frame
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    DLCI_HDLC_t *pHDLC = GET_HDLC();

    // test for valid N(R)
    if (Hdlc_IsValid_N_R (pHDLC))
    {
        if (pHDLC->ePeerReceiver == eReceiver_Busy)
        {
            pHDLC->V_S = pHDLC->N_R;
        }

        // peer receiver ready
        pHDLC->ePeerReceiver = eReceiver_Ready;

        (void)Hdlc_Handle_N_R (pHDLC);

        return TRUE;
    }

    // peer receiver ready
    pHDLC->ePeerReceiver = eReceiver_Ready;

    // N(R) Error
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("ERROR N(R): V(A)=%d, N(R)=%d, V(S)=%d"), pHDLC->V_A, pHDLC->N_R, pHDLC->V_S);
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_Handle_RNR_Frame
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    DLCI_HDLC_t *pHDLC = GET_HDLC();

    // peer receiver busy
    pHDLC->ePeerReceiver  = eReceiver_Busy;
    pHDLC->N_RNR          = pHDLC->WindowSize;

    (void)Hdlc_Handle_N_R(pHDLC);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_Handle_REJ_Frame
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    DLCI_HDLC_t *pHDLC = GET_HDLC();

    // peer receiver ready
    pHDLC->ePeerReceiver = eReceiver_Ready;

    // test for valid N(R)
    if (Hdlc_IsValid_N_R(pHDLC))
    {
        // Handle REJ only if we've received enough correct frames after last
        // RNR to avoid endless retransmissions caused by unhandled frames
        // still in transit.
        if (pHDLC->N_RNR == 0)
        {
            // V(A) = N(R)
            pHDLC->V_A = pHDLC->N_R;

            // invoke retransmission
            Hdlc_Retransmission(pHDLC);
        }
        else
        {
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("Ignoring REJ after RNR: V(A)=%d, N(R)=%d, V(S)=%d, N(RNR)=%d"), pHDLC->V_A, pHDLC->N_R, pHDLC->V_S, pHDLC->N_RNR);
        }

        return TRUE;
    }
    // N(R) Error
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("ERROR N(R): V(A)=%d, N(R)=%d, V(S)=%d"), pHDLC->V_A, pHDLC->N_R, pHDLC->V_S);
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_HandleT1
(
    DLCI_HDLC_t     *pHDLC
)
{
    // restart T1
    Hdlc_StartT1 (pHDLC);
    // invoke retransmission
    Hdlc_Retransmission (pHDLC);
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_HandleTClearRej
(
    DLCI_HDLC_t     *pHDLC
)
{
    // clear reject state
    pHDLC->eReject = eReject_Reset;
}


//****************************************************************************
// interface mux-protocol
//****************************************************************************

//////////////////////////////////////////////////////////////////////////////
//
//
//
// Parameters:
//
//
// Return:
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_Init
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    UINT8            ucWindowSize
)
{
    DLCI_HDLC_t         *pHDLC;

    pHDLC = &pMux->pDLCIArray[ucDLCI].sHDLC;

    MEMCLR(pHDLC, sizeof(DLCI_HDLC_t));

    pHDLC->eEvent      = eHDLC_Invalid;
    pHDLC->WindowSize  = ucWindowSize;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_Process
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    UINT8            ucControlField
)
{
    DLCI_HDLC_t         *pHDLC = GET_HDLC();

    MP_vSetTXLoop(pMux);

    // read HDLC data
    pHDLC->eEvent = Hdlc_DecodeEvent(ucControlField, &pHDLC->N_S, &pHDLC->N_R, &pHDLC->P_F);
    if (eHDLC_Invalid == pHDLC->eEvent)
    {
        return FALSE;
    }

    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("HDLC V(A)=%d, V(S)=%d, V(R)=%d"), pHDLC->V_A, pHDLC->V_S, pHDLC->V_R);
    switch (pHDLC->eEvent)
    {
        case eHDLC_I:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Rec I N(S)=%d, N(R)=%d"), pHDLC->N_S, pHDLC->N_R);
            (void)Hdlc_IsOwnReceiverBusy (pMux, ucDLCI);
            return Hdlc_Handle_I_Frame(pMux, ucDLCI);
        case eHDLC_RR:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Rec RR N(R)=%d"), pHDLC->N_R);
            return Hdlc_Handle_RR_Frame(pMux, ucDLCI);
        case eHDLC_RNR:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Rec RNR N(R)=%d"), pHDLC->N_R);
            pMux->pDLCIArray[ucDLCI].sMP_Info.RecRNR++;
            return Hdlc_Handle_RNR_Frame(pMux, ucDLCI);
        case eHDLC_REJ:
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_FRM, TEXT("Rec REJ N(R)=%d"), pHDLC->N_R);
            pMux->pDLCIArray[ucDLCI].sMP_Info.RecREJ++;
            return Hdlc_Handle_REJ_Frame(pMux, ucDLCI);
        default:
            return FALSE;
    }

    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_OwnReceiverReady
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    DLCI_HDLC_t         *pHDLC;

    pHDLC = &pMux->pDLCIArray[ucDLCI].sHDLC;

    if (pHDLC->eReceiver == eReceiver_Busy)
    {
        pHDLC->eReceiver = eReceiver_Ready;
        Hdlc_Send_RR(pMux, ucDLCI);

        MP_vSetTXLoop(pMux);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
void Hdlc_TimerTick
(
    MUX_INSTANCE_t  *pMux
)
{
    UINT8 i;
    UINT8 restart = 0;

    for (i = 0; i < pMux->dwMaxNumberOfDLCI; i++)
    {
        if (MP_bDlciConnected(pMux, i))
        {
            DLCI_HDLC_t *pHDLC;

            pHDLC = &pMux->pDLCIArray[i].sHDLC;

            if (pHDLC->T1_Timer > 0)
            {
                if (pHDLC->T1_Timer-- == 1)
                {
                    // TimeOut
                    pHDLC->eEvent = eHDLC_T1;
                    Hdlc_HandleT1 (pHDLC);
                    pMux->pDLCIArray[i].sMP_Info.ReTrans++;
                    MP_vUpdateTxQE_hdlc (pMux);
                }
                else
                {
                    restart = 1;
                }
            }

            if (pHDLC->T_ClearRej > 0)
            {
                if (pHDLC->T_ClearRej-- == 1)
                {
                    // TimeOut
                    Hdlc_HandleTClearRej (pHDLC);
                }
                else
                {
                    restart = 1;
                }
            }
        }
    }

    if (restart)
    {
        MP_vStartHdlcTimer();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
BOOL Hdlc_SendData
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    UINT8           *pData,
    UINT32           uiLen
)
{
    DLCI_HDLC_t        *pHDLC = GET_HDLC();
    BOOL                fResult = FALSE;
    BOOL                fSendRR = FALSE;

    // data available ?
    if (pData)
    {
        if ((pHDLC->ePeerReceiver != eReceiver_Busy) || pHDLC->fRetransActive)
        {
            pHDLC->fRetransActive = FALSE;

            // check send window
            if (Hdlc_CanBeSent(pHDLC))
            {
                // send I-Frame[N(S)=V_S, N(R)=V_R]
                (void)Hdlc_SendFrame(pMux, ucDLCI, eHDLC_I, pData, uiLen);

                Hdlc_StartT1 (pHDLC);

                Hdlc_Inc_V_S (pHDLC);

                pHDLC->eAck = eAck_NotPending;

                fResult = TRUE;
            }
            else
            {
                // window is full
                fSendRR = TRUE;
            }
        }
        else
        {
            // peer receiver busy
            fSendRR = TRUE;
        }
    }
    else
    {
        // no data avaiable
        fSendRR = TRUE;
    }

    if (fSendRR)
    {
        if (pHDLC->eAck == eAck_Pending)
        {
            // send RR[N(R) = V_R]
            Hdlc_Send_RR (pMux, ucDLCI);

            pHDLC->eAck = eAck_NotPending;

            fResult = TRUE;
        }
    }

    return fResult;
}

