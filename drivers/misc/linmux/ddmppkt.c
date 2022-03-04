/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

/*****                       Header - Files                              *****/
#include "global.h"
#include "ddmpglob.h"
#include "ddmpdlch.h"
#include "ddmpiofunc.h"
#include "ddmphdlc.h"


/*
    =====================================================================
    =====================================================================

    ATTENTION  ATTENTION  ATTENTION  ATTENTION ATTENTION  ATTENTION

    The TxBuffer should never filled to its last byte:

    ReadIndex == WriteIndex MUST ALWYS MEAN: BUFFER EMPTY

    =====================================================================
    =====================================================================
*/


/*****                           Macros                                  *****/


/*****                          Typedefs                                 *****/
/*****       (enums, structs and unions shall always be typedef'd)       *****/



//***** Global variables ******************************************************



/*!****************************************************************************
\brief      check for new data from connected application

\attention
            The TxBuffer must not filled to its last byte!
            ReadIndex == WriteIndex always means: empty

\param      [in] ucDlci     used channel

\return     data available or not
\retval     FALSE no further data available
\retval     != FALSE  new data available
******************************************************************************/
BOOL MP_bTxAppDataAvail
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    if ((ucDlci > 0) && (ucDlci < pMux->dwMaxNumberOfDLCI))
    {
        if (pMux->pDLCIArray[ucDlci].MP_TxRingBuf.WriteIndex != pMux->pDLCIArray[ucDlci].MP_TxRingBuf.Packets.MaxWrOffset)
        {
            return TRUE;
        }
        return FALSE;
    }
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("MP_bTxAppDataAvail: no packet handling available for dlci=%d"), ucDlci);
    return FALSE;
}


/*!****************************************************************************
\brief      check if data still to be send



\param      [in] ucDlci     used channel

\return     data available or not
\retval     FALSE no further data available
\retval     != FALSE  new data available
******************************************************************************/
BOOL MP_bTxDataAvail
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    MP_TTxRingBuf *Ptr = &pMux->pDLCIArray[ucDlci].MP_TxRingBuf;

    if ((ucDlci > 0) && (ucDlci < pMux->dwMaxNumberOfDLCI))
    {
        if (Ptr->Packets.TxPacket[pMux->pDLCIArray[ucDlci].sHDLC.V_S].Len)
        {
            return TRUE;
        }
        return MP_bTxAppDataAvail(pMux, ucDlci);
    }
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("MP_bTxDataAvail: no packet handling available for dlci=%d"), ucDlci);
    return FALSE;
}


/*!****************************************************************************
\brief      create packet according to channels rules

This functions will create a new packet within the tx buffer (shared memory with
SAPI or other buffer) according to current channel rules, as packet length


\param      [in] ucDlci     used channel

\return     data available or not
\retval     FALSE no further data available
\retval     != FALSE  new data available
******************************************************************************/
UINT8 MP_uiTxCreatePacket
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    MP_TTxRingBuf *Ptr = &pMux->pDLCIArray[ucDlci].MP_TxRingBuf;

    if ((ucDlci > 0) && (ucDlci < pMux->dwMaxNumberOfDLCI))
    {
        UINT8   i = pMux->pDLCIArray[ucDlci].sHDLC.V_S;
        UINT32  l;

        if (Ptr->WriteIndex >= Ptr->Packets.MaxWrOffset)
        {
            l = Ptr->WriteIndex - Ptr->Packets.MaxWrOffset;
        }
        else
        {
            l = Ptr->BufSize + Ptr->WriteIndex - Ptr->Packets.MaxWrOffset;
        }

        if (l > pMux->pDLCIArray[ucDlci].sMP_DLCI.uiFrameSize)
        {
            l = pMux->pDLCIArray[ucDlci].sMP_DLCI.uiFrameSize;
        }

        if (Ptr->Packets.TxPacket[i].Len)
        {
            MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("MP_uiTxCreatePacket: pkt create fail"));
        }

        Ptr->Packets.TxPacket[i].Len = l;
        Ptr->Packets.TxPacket[i].StartOffset = Ptr->Packets.MaxWrOffset;

        Ptr->Packets.MaxWrOffset += l;
        if (Ptr->Packets.MaxWrOffset >= Ptr->BufSize)
        {
            Ptr->Packets.MaxWrOffset -= Ptr->BufSize;
        }

        MUXDBG(ZONE_MUX_PROT_HDLC, TEXT("MP_uiTxCreatePacket: pkt=%d len=%d dlci=%d wrindex=%d startoffset=%d maxwroffset=%d"), i, Ptr->Packets.TxPacket[i].Len, ucDlci, Ptr->WriteIndex, Ptr->Packets.TxPacket[i].StartOffset, Ptr->Packets.MaxWrOffset);

        return (i);
    }
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("MP_bTxCreatePacket: no packet handling available for dlci=%d"), ucDlci);
    return FALSE;
}


/*!****************************************************************************
\brief      free all packets not within range v_a -> v_s



\param      [in] ucDlci     used channel

\return     void
******************************************************************************/
void MP_uiTxFreeQueue
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    UINT8           i, k;
    BOOL            RxIndexSet = FALSE;
    MP_TTxRingBuf  *Ptr = &pMux->pDLCIArray[ucDlci].MP_TxRingBuf;
    BOOL            fTxStateChanged = FALSE;

    if ((ucDlci > 0) && (ucDlci < pMux->dwMaxNumberOfDLCI))
    {
        UINT8 va = pMux->pDLCIArray[ucDlci].sHDLC.V_A;
        UINT8 vs = pMux->pDLCIArray[ucDlci].sHDLC.V_S;

        // Step downwards, beginning from va-1, free all packets until
        // the zero len is found. At least one packet MUST HAVE zero len,
        // because we have 8 possible packets, but a window size of 7 max.
        // k is unsigned! 0 - 1 is 0xff, modulo 8 is 7
        for (k = (va - 1); k != vs; k--)
        {
            i = k & 7; // modulo 8

            // no packet; from now; all packets will be destroyed
            if (Ptr->Packets.TxPacket[i].Len == 0)
            {
                // a zero packet is found; all freed now
                break;
            }

            MUXDBG(ZONE_MUX_PROT_HDLC, TEXT("MP_uiTxFreeQueue: pkt=%d len=%d dlci=%d wrindex=%d rdindex=%d"), i, Ptr->Packets.TxPacket[i].Len, ucDlci, Ptr->WriteIndex, Ptr->ReadIndex);

            if (!RxIndexSet)
            {
                RxIndexSet = TRUE;
                Ptr->ReadIndex = Ptr->Packets.TxPacket[i].StartOffset + Ptr->Packets.TxPacket[i].Len;
                if (Ptr->ReadIndex >= Ptr->BufSize)
                {
                    Ptr->ReadIndex -= Ptr->BufSize;
                }

                // adjust len
                if (Ptr->WriteIndex >= Ptr->ReadIndex)
                {
                    Ptr->Count = Ptr->WriteIndex - Ptr->ReadIndex;
                }
                else
                {
                    Ptr->Count = Ptr->BufSize + Ptr->WriteIndex - Ptr->ReadIndex;
                }
            }

            // update statistic values
            pMux->pDLCIArray[ucDlci].sMP_Info.BytesWritten += Ptr->Packets.TxPacket[i].Len;

            // reset packet: Len is free-indicator
            Ptr->Packets.TxPacket[i].Len = 0;
        }

        // Update TxEmpty status
        if (!Ptr->Count )
        {
            pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxEmpty = 1;
            fTxStateChanged = TRUE;
        }
        // Update TxFull status
        if ((Ptr->Count < (Ptr->BufSize - 1)) && pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxFull)
        {
            pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxFull = 0;
            fTxStateChanged = TRUE;
        }
        if (fTxStateChanged)
        {
            MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
        }

        MP_vAppCallback (pMux, ucDlci, Ptr->Count);
    }
    else
    {
        MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("MP_uiTxFreeQueue: no packet handling available for dlci=%d"), ucDlci);
    }
}


/*!****************************************************************************
\brief      Main function for scheduler



\param      [in] ucDlci     used channel

\return     further handling necessary on channel or not
\retval     FALSE no further handling necessary
\retval     != FALSE  further handling necessary
******************************************************************************/
BOOL MP_bHandleTxPackets
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    if ((ucDlci > 0) && (ucDlci < pMux->dwMaxNumberOfDLCI))
    {
        UINT8  *DataPtr = NULL;
        UINT32  DataLen = 0;

        MUXDBG(ZONE_MUX_PROT_HDLC, TEXT("MP_bHandlePackets: start; dlci=%d"), ucDlci);
        if (MP_bTxDataAvail(pMux, ucDlci))
        {
            MP_TTxRingBuf  *Ptr = &pMux->pDLCIArray[ucDlci].MP_TxRingBuf;
            DLCI_HDLC_t    *p   = &pMux->pDLCIArray[ucDlci].sHDLC;
            UINT8           np  = pMux->pDLCIArray[ucDlci].sHDLC.V_S;

            MUXDBG(ZONE_MUX_PROT_HDLC, TEXT("... data available"));

            if (!Ptr->Packets.TxPacket[np].Len)
            {
                // jump over windows, if vs < va; then we have linear conditions
                UINT8 _np = np;
                if (np < p->V_A)
                {
                    _np += MP_TX_PACKET_CNT;
                }

                if ((p->V_A + p->WindowSize) > _np)
                {
                    MUXDBG(ZONE_MUX_PROT_HDLC, TEXT("... create new packet, v_s=%d"), np);
                    (void)MP_uiTxCreatePacket(pMux, ucDlci);
                }
                else
                {
                    MUXDBG(ZONE_MUX_PROT_HDLC, TEXT("... no packet outside window: va=%d window=%d vs=%d"), p->V_A, p->WindowSize, np);
                }
            }

            DataLen = Ptr->Packets.TxPacket[np].Len;
            if (DataLen)
            {
                DataPtr = &Ptr->Buf[Ptr->Packets.TxPacket[np].StartOffset];
            }
            MUXDBG(ZONE_MUX_PROT_HDLC, TEXT("... v_s=%d, ptr=%p len=%d"), pMux->pDLCIArray[ucDlci].sHDLC.V_S, DataPtr, DataLen);
        }
        return Hdlc_SendData(pMux, ucDlci, DataPtr, DataLen);
    }
    MUXDBG(ZONE_MUX_PROT_HDLC | ZONE_MUX_PROT_HDLC_ERROR, TEXT("MP_bHandlePackets: no packet handling available for dlci=%d"), ucDlci);
    return FALSE;
}


/*!****************************************************************************
\brief      Init packet handler

\param      [in] ucDlci     used channel

\return     further handling necessary on channel or not
\retval     FALSE no further handling necessary
\retval     != FALSE  further handling necessary
******************************************************************************/
void MP_vInitPacketHandler
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    if (ucDlci < pMux->dwMaxNumberOfDLCI)
    {
        UINT8 p;
        for (p = 0; p < MP_TX_PACKET_CNT; p++)
        {
            pMux->pDLCIArray[ucDlci].MP_TxRingBuf.Packets.TxPacket[p].StartOffset = 0;
            pMux->pDLCIArray[ucDlci].MP_TxRingBuf.Packets.TxPacket[p].Len = 0;
        }

        // start packet handler from the current _last_ transmitted byte; not from
        // the beginning. This is necessary, since this function is called when a
        // channel was opened; and already data have been transmitted
        pMux->pDLCIArray[ucDlci].MP_TxRingBuf.Packets.MaxWrOffset = pMux->pDLCIArray[ucDlci].MP_TxRingBuf.WriteIndex;
    }
}

/************************  E N D   O F   F I L E  ****************************/
