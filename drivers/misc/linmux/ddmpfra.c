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

//***** Global variables **************************************************************

const UINT8  fcstab8[256] = {   //reversed, 8-bit, poly=0x07
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75,  0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69,  0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D,  0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51,  0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,

    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05,  0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19,  0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D,  0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21,  0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,

    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95,  0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89,  0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD,  0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1,  0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,

    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5,  0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9,  0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD,  0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1,  0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
};
#define FCS8INIT      0xFF    // Init FCS Wert
#define FCS8OKAY      0xCF    // Korrektes Empfangs-FCS


enum
{
    MP_FRA_LIST_OF_TX_OVERFLOW,             // 0
    MP_FRA_LIST_OF_TX_DATA_OVERFLOW,        // 1
    MP_FRA_NO_FIFO_RX,                      // 2
    MP_FRA_NULLPTR,                         // 3
    MP_FRA_SYS_ERROR,                       // 4
    MP_FRA_DOWNWRITEFAIL,                   // 5
    MP_FRA_WRONG_INSTANCE                   // 6
};


//***** Prototypes ***********************************************************

static void MP_vFramedDataSend(MUX_INSTANCE_t *pMux, UINT8 ucDlci, MPFRAME *psFrame);
static void MP_vFramedDataSend_2(MUX_INSTANCE_t *pMux, UINT8 ucDlci, UINT32 fLen);
static void MP_ScanForMessage(MUX_INSTANCE_t *pMux, REC_MPFRAME *pFrame);
static MP_EVENTS MP_Scan4ControlChannel(MUX_INSTANCE_t *pMux, REC_MPFRAME *pFrame);
static UINT32 MP_TxFraming(MUX_INSTANCE_t *pMux, UINT8 ucDLCI, MPFRAME *psFrame);


//***** Constants ************************************************************
#define MP_DevGetFreeBytesInPhysTXBuf_hdlc   MP_DevGetFreeBytesInPhysTXBuf

//***** Data *****************************************************************


//***** Process **************************************************************


void MP_vInitScanner
(
    MUX_INSTANCE_t  *pMux
)
{
    pMux->ucLastSendDLCI    = DLCI1;
    pMux->MP_RxStateScanner = SCAN4STARTFLAG;
    pMux->ucMP_RxDLCI       = DLCI0;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_TxFraming() ...
 *          Framing: Set flags, obtain address, set control field,
 *          obtain length of data field, calculate FCS
 * ---------------------------------------------------------------------------
 */
static UINT32 MP_TxFraming
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI,
    MPFRAME         *psFrame
)
{
    UINT32          Buffersize;
    UINT16          uiFrameLen;
    UINT8           ucResponse;

    // The length is as follows:
    //  - 2 Byte Start+EndFlag
    //  - 1 Byte Address
    //  - 1 Byte Control
    //  - 1 or 2 Bytes length
    //  - n Byte Data
    //  - 1 Byte CRC

    uiFrameLen = psFrame->Len;
    Buffersize = uiFrameLen + 6;

    if(uiFrameLen > MAXFRAMESIZE)
    {
        MUX_EXIT(TEXT("Mux-Exit: %d"), MP_FRA_LIST_OF_TX_OVERFLOW);
    }


    psFrame->Buf[0] = DLC_FRAME_FLAG_BASIC_MODE;               // Flag Sequence
    switch(psFrame->Control & 0xEF)
    {
       case DLC_FRAME_CONTROL_TYPE_UIH:
       case DLC_FRAME_CONTROL_TYPE_SABM:
       case DLC_FRAME_CONTROL_TYPE_DISC:
         ucResponse = pMux->ucInitiator;
         break;
       case DLC_FRAME_CONTROL_TYPE_UA:
       case DLC_FRAME_CONTROL_TYPE_DM:
         ucResponse = !pMux->ucInitiator;
         break;
       default:
         ucResponse = 0;
         break;
    }
    psFrame->Buf[1] = (UINT8)((ucDLCI<<2)|((ucResponse<<1)|DLC_FRAME_EA_BIT_SET));
    psFrame->Buf[2] = psFrame->Control;                                     // Control-Field
    psFrame->Buf[3] = (UINT8)((uiFrameLen << 1) | DLC_FRAME_EA_BIT_SET);    // len
    psFrame->Buf[Buffersize - 2] = ucMP_TxFCS(pMux, &psFrame->Buf[1], 3);   // FCS
    psFrame->Buf[Buffersize - 1] = DLC_FRAME_FLAG_BASIC_MODE;               // Flag

    pMux->pDLCIArray[ucDLCI].sMP_DLCI.ucLastControl = psFrame->Control;
    return(Buffersize);
}


void MP_SendSyncFlag
(
    MUX_INSTANCE_t  *pMux
)
{
    UINT8   FlagBuf[MAX_SYNC_FLAGS];

    MEMSET(FlagBuf, 0xF9, MAX_SYNC_FLAGS);
    MP_uiDevDataSend(pMux, FlagBuf, MAX_SYNC_FLAGS);
}

void MP_ResetResync
(
    MUX_INSTANCE_t  *pMux
)
{
    pMux->u32ResyncTickCount = 0;
}

void MP_SetResync
(
    MUX_INSTANCE_t  *pMux
)
{
    pMux->u32ResyncTickCount = 1;
}

void MP_CheckResyncTimeOut
(
    MUX_INSTANCE_t  *pMux
)
{
    if (pMux->u32ResyncTickCount > 0)
    {
        pMux->u32ResyncTickCount++;

        if (pMux->u32ResyncTickCount > 2)
        {

            if (pMux->MP_RxStateScanner == SCANRESYNC)
            {
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Repeating resync %d after incomplete mux frame!"), pMux->u32ResyncTickCount - 2);
                MP_SendSyncFlag(pMux);
            }
            else
            {
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Incomplete mux frame detected!"));
                pMux->MP_RxStateScanner = SCANRESYNC;
                MP_SendSyncFlag(pMux);
            }
        }
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_ReceiveFrame() ...
 *          this es the main decoder for the multiplex protocol
 * ---------------------------------------------------------------------------
 */
void MP_RxFraming
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *pStart,
    UINT8           *pStop
)
{
    UINT32          length1 = 0;
    UINT32          FCSlen = 0;
    UINT8          *pLastStartFlag;

    // update statistics
    pMux->sMP_ParserInfo.BytesRead += pStop - pStart;

    pLastStartFlag = pStart;
    if (pMux->MP_RxStateScanner != SCAN4STARTFLAG)
    {
        pLastStartFlag--;
    }
    while (pStart < pStop)
    {
        switch (pMux->MP_RxStateScanner)
        {
          case SCAN4ENDFLAG:
            MP_ResetResync(pMux);

            pMux->MP_RxStateScanner = SCAN4STARTFLAG;
            if (*pStart != 0xF9)                // no end 0x09 flag received
            {
                pStart = pLastStartFlag + 1;
                pMux->sMP_ParserInfo.FlagEndError++;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Frame End Mark Error"));
                break;
            }
            MUXDBG(ZONE_MUX_PROT2, TEXT("Rx pkt read from UART dlci=%d datalen=%d"), pMux->ucMP_RxDLCI, pMux->RxFrame.Len);

            //update statistics
            pMux->sMP_ParserInfo.OverallPacketsRead++;

            if (pMux->ucMP_RxDLCI >= pMux->dwMaxNumberOfDLCI)
            {
                pStart = pLastStartFlag;
                pMux->sMP_ParserInfo.DLCItoobig++;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("DLCI > MAX dlci=%d"), pMux->ucMP_RxDLCI);
            }
            else if ((pMux->ucMP_RxDLCI == DLCI0) || ((pMux->RxFrame.Control & 0xEF) != DLC_FRAME_CONTROL_TYPE_UIH))
            {
                pMux->RxFrame.ucDlci = pMux->ucMP_RxDLCI;
                pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.BytesRead += pMux->RxFrame.Len;
                MP_ScanForMessage(pMux, &pMux->RxFrame);
            }
            else if (pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_DLCI.State == DLC_DISCONNECTED)
            {
                pStart = pLastStartFlag;
                pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.ChanNotActivePackets++;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("CHANNEL NOT CONNECTED dlci=%d"), pMux->ucMP_RxDLCI);
            }
            else
            {
                MP_pRxRingBuf  Ptr = &pMux->pDLCIArray[pMux->ucMP_RxDLCI].MP_RxRingBuf;
                UINT32         toCopy = pMux->RxFrame.Len;
                if ((Ptr->Count + toCopy) > Ptr->BufSize)
                {
                    MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Data RX lost dlci=%d cnt=%d"), pMux->ucMP_RxDLCI, ((Ptr->Count + toCopy) - Ptr->BufSize));
                    pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.DevUpLostBytes += Ptr->Count + toCopy - Ptr->BufSize;
                    toCopy = Ptr->BufSize - Ptr->Count;
                }
                pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.BytesRead += toCopy;

                if ((Ptr->WriteIndex + toCopy) <= Ptr->BufSize)
                {
                    MEMCPY(&Ptr->Buf[Ptr->WriteIndex], pMux->RxFrame.Buf, toCopy);
                    Ptr->WriteIndex += toCopy;
                }
                else
                {
                    UINT32 nCopy = Ptr->BufSize - Ptr->WriteIndex;
                    MEMCPY(&Ptr->Buf[Ptr->WriteIndex], pMux->RxFrame.Buf, nCopy);
                    MEMCPY(&Ptr->Buf[0], pMux->RxFrame.Buf + nCopy, toCopy - nCopy);
                    Ptr->WriteIndex = toCopy - nCopy;
                }
                if (Ptr->WriteIndex >= Ptr->BufSize)
                {
                    Ptr->WriteIndex -= Ptr->BufSize;
                }
                Ptr->Count += toCopy;

                // update statistics
                if (pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.RxBufMax < Ptr->Count)
                {
                    pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.RxBufMax = Ptr->Count;
                }

                // Update error restart pointer to first byte after last correct frame
                pLastStartFlag = pStart + 1;
            }
            pStart++;
            break;

          case SCAN4STARTFLAG:
            if (*pStart == 0xF9) // start flag 0xf9 received
            {
                pLastStartFlag = pStart;
                pMux->MP_RxStateScanner = SCAN4ADDRESS;
            }
            pStart++;
            break;

          case SCAN4ADDRESS:
            if (*pStart==0xF9)
            {
                pLastStartFlag = pStart;
                pMux->F9Count++;

                // only respond to multiple flags, so we can be sure the sender is serious
                if (pMux->F9Count > 3)
                {
                    switch(pMux->MP_LinkState)
                    {
                        case eLINK_ACTIVE:
                            // just echo
                            (void)MP_uiDevDataSend(pMux, pStart, 1);
                            break;

                        case eLINK_DOWN:
                            break;

                        case eLINK_WAKING_UP:
                            // this is the reponse to flags send by us
                            pMux->MP_LinkState = eLINK_ACTIVE;

                            // trigger TX, there might be data in buffer
                            MP_vSetTXLoop(pMux);
                            break;

                        default:
                            // don't respond, don't tell, wait!
                            break;
                    }

                    pMux->F9Count = 0;
                }

                pStart++;
                break;
            }
            pMux->F9Count = 0;

            // Looking for Address, control frame-queue
            pMux->ucMP_RxDLCI = (MP_eDLCI)(*pStart >> 2);
            pMux->RxFrame.Address = pMux->RxFrame.BufHeader[0] = *pStart;

            // flow control only on valid frames
            if (pMux->ucMP_RxDLCI < pMux->dwMaxNumberOfDLCI)
            {
                MP_vSetFlowControl(pMux, pMux->ucMP_RxDLCI);
            }

            pMux->MP_RxStateScanner = SCAN4CONTROL;

            MP_SetResync(pMux);

            pStart++;
            break;

          case SCAN4CONTROL:
            pMux->RxFrame.Len = 0xFFFF;
            pMux->RxFrame.Control = pMux->RxFrame.BufHeader[1] = *pStart;
            pMux->MP_RxStateScanner = SCAN4LENGTH;
            pStart++;
            break;

          case SCAN4LENGTH:
            {
                BOOL FirstRun = (pMux->RxFrame.Len == 0xFFFF) ? TRUE : FALSE;

                if (FirstRun)
                {
                    pMux->RxFrame.Len = *pStart >> 1;
                    pMux->RxFrame.BufHeader[2] = *pStart;
                    pMux->Datalen = 0;                    // until now 0 bytes copied
                }
                else
                {
                    pMux->RxFrame.Len = pMux->RxFrame.Len | (((UINT16)(*pStart)) << 7);
                    pMux->RxFrame.BufHeader[3] = *pStart;
                }

                if ((*pStart & 1) || (!FirstRun))
                {

                    if (pMux->ucMP_RxDLCI >= pMux->dwMaxNumberOfDLCI)
                    {
                        pStart = pLastStartFlag;
                        pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Wrong dlci ERROR dlci=%d"), pMux->ucMP_RxDLCI);
                    }
                    else if (pMux->RxFrame.Len > pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_DLCI.uiFrameSize)
                    {
                        pMux->sMP_ParserInfo.Lentoolong++;
                        pStart = pLastStartFlag;
                        pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Length ERROR len=%d dlci=%d"), pMux->RxFrame.Len, pMux->ucMP_RxDLCI);
                    }
                    else if (pMux->RxFrame.Len == 0)
                    {
                        pMux->MP_RxStateScanner = SCAN4FCS;
                    }
                    else
                    {
                        pMux->MP_RxStateScanner = SCAN4DATA;
                    }
                }
            }
            pStart++;
            break;

          case SCAN4DATA:
            length1 = (pStop - pStart) + pMux->Datalen;
            if (pMux->RxFrame.Len <= length1)
            {
                MEMCPY(pMux->RxFrame.Buf + pMux->Datalen, pStart, pMux->RxFrame.Len - pMux->Datalen);
                pStart =  pStart + (pMux->RxFrame.Len - pMux->Datalen);
                pMux->Datalen = 0;
            }
            else
            {
                length1 = pStop - pStart;
                MEMCPY( pMux->RxFrame.Buf + pMux->Datalen, pStart, length1 );
                pMux->Datalen += length1;
                return;
            }
            pMux->MP_RxStateScanner = SCAN4FCS;
            break;

          case SCAN4FCS:
            FCSlen = 3; // UIH packet format

            // If we have a two byte length identifier, one more byte has to be checked
            if (pMux->RxFrame.Len > 0x7f)
            {
                FCSlen++;
            }

            MP_ucRxFCS(pMux, &pMux->RxFrame.BufHeader[0], FCSlen, *pStart);
            pStart++;

            if (pMux->fcs8_RX == FCS8OKAY)
            {
                pMux->MP_RxStateScanner = SCAN4ENDFLAG;
            }
            else
            {
                pStart = pLastStartFlag + 1;
                pMux->sMP_ParserInfo.CRCErrors++;
                pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Checksum ERROR dlci=%d"), pMux->ucMP_RxDLCI);
            }
            break;

          case SCANRESYNC:
            if (*pStart == 0xF9)
            {
                pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                MP_ResetResync(pMux);
            }
            pStart++;
            break;

          default:
            pMux->sMP_ParserInfo.ParserStateError++;
            pMux->MP_RxStateScanner = SCAN4STARTFLAG;
            MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Unknown scanner state dlci=%d"), pMux->ucMP_RxDLCI);
            break;
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_RxFramingHdlc() ...
 *          this es the main decoder for the multiplex protocol for HDLC-Framing
 * ---------------------------------------------------------------------------
 */
void MP_RxFramingHdlc
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *pStart,
    UINT8           *pStop
)
{
    UINT32          length1 = 0;
    UINT32          FCSlen = 0;
    UINT8          *pLastStartFlag;

    // update statistics
    pMux->sMP_ParserInfo.BytesRead += pStop - pStart;

    pLastStartFlag = pStart;
    if (pMux->MP_RxStateScanner != SCAN4STARTFLAG)
    {
        pLastStartFlag--;
    }
    while (pStart < pStop)
    {
        switch (pMux->MP_RxStateScanner)
        {
          case SCAN4ENDFLAG:
            MP_ResetResync(pMux);

            pMux->MP_RxStateScanner = SCAN4STARTFLAG;
            if (*pStart != 0xF9)                // no end 0x09 flag received
            {
                pStart = pLastStartFlag + 1;
                pMux->sMP_ParserInfo.FlagEndError++;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Frame End Mark Error"));
                break;
            }
            MUXDBG(ZONE_MUX_PROT2, TEXT("Rx hdlc-pkt read from UART dlci=%d datalen=%d"), pMux->ucMP_RxDLCI, pMux->RxFrame.Len);

            //update statistics
            pMux->sMP_ParserInfo.OverallPacketsRead++;

            if (pMux->ucMP_RxDLCI >= pMux->dwMaxNumberOfDLCI)
            {
                pStart = pLastStartFlag;
                pMux->sMP_ParserInfo.DLCItoobig++;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("DLCI > MAX dlci=%d"), pMux->ucMP_RxDLCI);
            }
            else if ((pMux->ucMP_RxDLCI == DLCI0) || (!HDLC_IS_I_FRAME_TYPE(pMux->RxFrame.Control)))
            {
                if (!(pMux->ucMP_RxDLCI == DLCI0) && (HDLC_IS_R_FRAME_TYPE(pMux->RxFrame.Control)))
                {
                    MP_vSetFlowControl(pMux, pMux->ucMP_RxDLCI);

                    // handle HDLC command
                    (void)Hdlc_Process(pMux, pMux->ucMP_RxDLCI, pMux->RxFrame.Control);

                    MP_uiTxFreeQueue(pMux, pMux->ucMP_RxDLCI);
                    if (MP_bTxDataAvail(pMux, pMux->ucMP_RxDLCI))
                    {
                        if (MP_DevGetFreeBytesInPhysTXBuf(pMux) >= ((UINT32)pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_DLCI.uiFrameSize * 2))
                        {
                            MP_bHandleTxPackets(pMux, pMux->ucMP_RxDLCI);
                        }
                    }

                    MP_uiTxFreeQueue(pMux, pMux->ucMP_RxDLCI);
                }
                else
                {
                    pMux->RxFrame.ucDlci = pMux->ucMP_RxDLCI;
                    pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.BytesRead += pMux->RxFrame.Len;
                    MP_ScanForMessage(pMux, &pMux->RxFrame);
                }
            }
            else if (pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_DLCI.State == DLC_DISCONNECTED)
            {
                pStart = pLastStartFlag;
                pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.ChanNotActivePackets++;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("CHANNEL NOT CONNECTED dlci=%d"), pMux->ucMP_RxDLCI);
            }
            else
            {
                if (HDLC_IS_I_FRAME_TYPE(pMux->RxFrame.Control))
                {
                  MP_vSetFlowControl(pMux, pMux->ucMP_RxDLCI);

                  // handle HDLC command
                  if (FALSE == Hdlc_Process(pMux, pMux->ucMP_RxDLCI, pMux->RxFrame.Control))
                  {
                      // discard data
                  }
                  else
                  {

                      MP_pRxRingBuf  Ptr = &pMux->pDLCIArray[pMux->ucMP_RxDLCI].MP_RxRingBuf;
                      UINT32         toCopy = pMux->RxFrame.Len;

                      pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.BytesRead += toCopy;

                      if ((Ptr->WriteIndex + toCopy) <= Ptr->BufSize)
                      {
                          MEMCPY(&Ptr->Buf[Ptr->WriteIndex], pMux->RxFrame.Buf, toCopy);
                          Ptr->WriteIndex += toCopy;
                      }
                      else
                      {
                          UINT32 nCopy = Ptr->BufSize - Ptr->WriteIndex;
                          MEMCPY(&Ptr->Buf[Ptr->WriteIndex], pMux->RxFrame.Buf, nCopy);
                          MEMCPY(&Ptr->Buf[0], pMux->RxFrame.Buf+nCopy, toCopy - nCopy);
                          Ptr->WriteIndex = toCopy - nCopy;
                      }
                      if (Ptr->WriteIndex >= Ptr->BufSize)
                      {
                          Ptr->WriteIndex -= Ptr->BufSize;
                      }
                      Ptr->Count += toCopy;

                      // update statistics
                      if (pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.RxBufMax < Ptr->Count)
                      {
                          pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_Info.RxBufMax = Ptr->Count;
                      }
                      // startup the data transmitter to app
                      MP_vUpdateRxQE(pMux, pMux->ucMP_RxDLCI);
                  }

                  MP_uiTxFreeQueue(pMux, pMux->ucMP_RxDLCI);
                }
                else
                {
                    // no HDLC I-Frame -> discard data
                }
                // Update error restart pointer to first byte after last correct frame
                pLastStartFlag = pStart + 1;
            }
            pStart++;
            break;

          case SCAN4STARTFLAG:
            if (*pStart == 0xF9) // start flag 0xf9 received
            {
                pLastStartFlag = pStart;
                pMux->MP_RxStateScanner = SCAN4ADDRESS;
            }
            pStart++;
            break;

          case SCAN4ADDRESS:
            if (*pStart==0xF9)
            {
                pLastStartFlag = pStart;
                pMux->F9Count++;

                // only respond to multiple flags, so we can be sure the sender is serious
                if (pMux->F9Count > 3)
                {
                    switch(pMux->MP_LinkState)
                    {
                        case eLINK_ACTIVE:
                            // just echo
                            (void)MP_uiDevDataSend(pMux, pStart, 1);
                            break;

                        case eLINK_DOWN:
                            break;

                        case eLINK_WAKING_UP:
                            // this is the reponse to flags send by us
                            pMux->MP_LinkState = eLINK_ACTIVE;

                            // trigger TX, there might be data in buffer
                            MP_vSetTXLoop(pMux);
                            break;

                        default:
                            // don't respond, don't tell, wait!
                            break;
                    }

                    pMux->F9Count = 0;
                }

                pStart++;
                break;
            }
            pMux->F9Count = 0;

            // Looking for Address, control frame-queue
            pMux->ucMP_RxDLCI = (MP_eDLCI)(*pStart >> 2);
            pMux->RxFrame.Address = pMux->RxFrame.BufHeader[0] = *pStart;


            pMux->MP_RxStateScanner = SCAN4CONTROL;

            MP_SetResync(pMux);

            pStart++;
            break;

          case SCAN4CONTROL:
            // if this package RR,RNR,REJ frame is, then no information about the length follow
            pMux->RxFrame.Control = pMux->RxFrame.BufHeader[1] = *pStart;
            if(HDLC_IS_R_FRAME_TYPE(*pStart))
            {
                pMux->RxFrame.Len = 0;
                pMux->MP_RxStateScanner = SCAN4FCS;
            }
            else
            {
                pMux->RxFrame.Len = 0xFFFF;
                pMux->MP_RxStateScanner = SCAN4LENGTH;
            }
            pStart++;

            break;

          case SCAN4LENGTH:
            {
                BOOL FirstRun = (pMux->RxFrame.Len == 0xFFFF) ? TRUE : FALSE;

                if (FirstRun)
                {
                    pMux->RxFrame.Len = *pStart >> 1;
                    pMux->RxFrame.BufHeader[2] = *pStart;
                    pMux->Datalen = 0;                    // until now 0 bytes copied
                }
                else
                {
                    pMux->RxFrame.Len = pMux->RxFrame.Len | (((UINT16)(*pStart)) << 7);
                    pMux->RxFrame.BufHeader[3] = *pStart;
                }

                if ((*pStart & 1) || (!FirstRun))
                {

                    if (pMux->ucMP_RxDLCI >= pMux->dwMaxNumberOfDLCI)
                    {
                        pStart = pLastStartFlag;
                        pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Wrong dlci ERROR dlci=%d"), pMux->ucMP_RxDLCI);
                    }
                    else if (pMux->RxFrame.Len > pMux->pDLCIArray[pMux->ucMP_RxDLCI].sMP_DLCI.uiFrameSize)
                    {
                        pMux->sMP_ParserInfo.Lentoolong++;
                        pStart = pLastStartFlag;
                        pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Length ERROR len=%d dlci=%d"), pMux->RxFrame.Len, pMux->ucMP_RxDLCI);
                    }
                    else if (pMux->RxFrame.Len == 0)
                    {
                        pMux->MP_RxStateScanner = SCAN4FCS;
                    }
                    else
                    {
                        pMux->MP_RxStateScanner = SCAN4DATA;
                    }
                }
            }
            pStart++;
            break;

          case SCAN4DATA:
            length1 = (pStop - pStart) + pMux->Datalen;
            if (pMux->RxFrame.Len <= length1)
            {
                MEMCPY(pMux->RxFrame.Buf + pMux->Datalen, pStart, pMux->RxFrame.Len - pMux->Datalen);
                pStart =  pStart + (pMux->RxFrame.Len - pMux->Datalen);
                pMux->Datalen = 0;
            }
            else
            {
                length1 = pStop - pStart;
                MEMCPY( pMux->RxFrame.Buf + pMux->Datalen, pStart, length1 );
                pMux->Datalen += length1;
                return;
            }
            pMux->MP_RxStateScanner = SCAN4FCS;
            break;

          case SCAN4FCS:
            if(HDLC_IS_I_FRAME_TYPE(pMux->RxFrame.BufHeader[1]) || !HDLC_IS_R_FRAME_TYPE(pMux->RxFrame.BufHeader[1]))
            {
                FCSlen = 3; // UIH packet format or HDLC I-Frame
                // if a two byte length identifier, one more bytes has to check
                if (pMux->RxFrame.Len > 0x7f)
                {
                    FCSlen++;
                }
            }
            else
            {
                FCSlen = 2; // HDLC command (RR, RNR, REJ)
            }

            (void)MP_ucRxFCS(pMux, &pMux->RxFrame.BufHeader[0], FCSlen, *pStart);

            pStart++;

            if (pMux->fcs8_RX == FCS8OKAY)
            {
                pMux->MP_RxStateScanner = SCAN4ENDFLAG;
            }
            else
            {
                pStart = pLastStartFlag + 1;
                pMux->sMP_ParserInfo.CRCErrors++;
                pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Checksum ERROR dlci=%d"), pMux->ucMP_RxDLCI);
            }

            break;

          case SCANRESYNC:
            if (*pStart == 0xF9)
            {
                pMux->MP_RxStateScanner = SCAN4STARTFLAG;
                MP_ResetResync(pMux);
            }
            pStart++;
            break;

          default:
            pMux->sMP_ParserInfo.ParserStateError++;
            pMux->MP_RxStateScanner = SCAN4STARTFLAG;
            MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Unknown scanner state dlci=%d"), pMux->ucMP_RxDLCI);
            break;
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_ScanForMessage() ...
 * ---------------------------------------------------------------------------
 */
static void MP_ScanForMessage
(
    MUX_INSTANCE_t  *pMux,
    REC_MPFRAME     *pFrame
)
{
    MP_PRIMITIVE    Prim;
    MP_EVENTS       event;
    MPVAR          *psDLC = &pMux->pDLCIArray[pFrame->ucDlci].sMP_DLCI;
    UINT8           ucCommandResponse;

    event = MP_IndNSC;

    ucCommandResponse = (pFrame->Address & 0x2) >> 1;

    switch(pFrame->Control & 0xEF)
    {
      case DLC_FRAME_CONTROL_TYPE_SABM:
        if (pFrame->ucDlci == DLCI0)
        {
            if (((ucCommandResponse == 1) && (pMux->ucInitiator == 0)) ||
                ((ucCommandResponse == 0) && (pMux->ucInitiator == 1)))
            {
                event = MP_IndSABM_DLCI0;
                MUXDBG(ZONE_MUX_PROT1, TEXT("Received IndSABM_DLCI0 dlci=%d"), pFrame->ucDlci);
            }
        }
        else
        {
            if (((ucCommandResponse == 1) && (pMux->ucInitiator == 0)) ||
                ((ucCommandResponse == 0) && (pMux->ucInitiator == 1)))
            {
                event = MP_IndSABM;
                MUXDBG(ZONE_MUX_PROT1, TEXT("Received IndSABM dlci=%d"), pFrame->ucDlci);
            }
        }
        break;

    case DLC_FRAME_CONTROL_TYPE_DISC:
        if (((ucCommandResponse == 1) && (pMux->ucInitiator == 0)) ||
            ((ucCommandResponse == 0) && (pMux->ucInitiator == 1)))
        {
            event = MP_IndDISC;
            MUXDBG(ZONE_MUX_PROT1, TEXT("Received IndDISC dlci=%d"), pFrame->ucDlci);
        }
        break;

    case DLC_FRAME_CONTROL_TYPE_UA:                  // Response to SABM or DISC
        if ((psDLC->ucLastControl & 0xEF) == DLC_FRAME_CONTROL_TYPE_SABM)
        {
            if (((ucCommandResponse == 1) && (pMux->ucInitiator == 1)) ||
                ((ucCommandResponse == 0) && (pMux->ucInitiator == 0)))
            {
                event = MP_ConfUA_SABM;
                MUXDBG(ZONE_MUX_PROT1, TEXT("Received ConfUA_SABM dlci=%d"), pFrame->ucDlci);
            }
        }
        else
        {
            event = MP_ConfUA_DISC;
            MUXDBG(ZONE_MUX_PROT1, TEXT("Received ConfUA_DISC dlci=%d"), pFrame->ucDlci);
        }
        break;

    case DLC_FRAME_CONTROL_TYPE_DM:                  // Response to SABM or DISC
       if (((ucCommandResponse == 1) && (pMux->ucInitiator == 1)) ||
           ((ucCommandResponse == 0) && (pMux->ucInitiator == 0)))
       {
            if ((psDLC->ucLastControl & 0xEF) == DLC_FRAME_CONTROL_TYPE_SABM)
            {
                event = MP_ConfDM_SABM;
                MUXDBG(ZONE_MUX_PROT1, TEXT("Received ConfDM_SABM dlci=%d"), pFrame->ucDlci);
            }
            else
            {
                event = MP_ConfDM_DISC;
                MUXDBG(ZONE_MUX_PROT1, TEXT("Received ConfDM_DISC dlci=%d"), pFrame->ucDlci);
            }
        }
        break;

    case DLC_FRAME_CONTROL_TYPE_UIH:
        if (pFrame->ucDlci != DLCI0)          // UIH-Frames are not allowed here
        {
            event = MP_IndNSC;
            MUXDBG(ZONE_MUX_PROT1, TEXT("Received unspecified UIH for dlci=%d"), pFrame->ucDlci);
        }
        else
        {
            event = MP_Scan4ControlChannel(pMux, pFrame);
        }
       break;

    default:
        event = MP_IndNSC;
        break;
    }

    if (event != MP_Ind_already_sent)
    {
        Prim.Event = event;
        Prim.ucDLCI = (MP_eDLCI)pFrame->ucDlci; // Control frames only on DLCI0 !!!
        MP_PostUserMessage(pMux, &Prim);
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_Scan4ControlChannel() ...
 * ---------------------------------------------------------------------------
 */
static MP_EVENTS MP_Scan4ControlChannel
(
    MUX_INSTANCE_t  *pMux,
    REC_MPFRAME     *pFrame
)
{
    MPFRAME         *psmuxFrame;
    UINT8            length;
    UINT16           ucSegmentLen;
    UINT8            ucType;
    UINT8           *DataBuffer;
    MP_EVENTS        event;
    UINT8           *ucValue;
    MP_PRIMITIVE     sPrimitive;

    event = MP_IndNSC;
    length = 0;
    DataBuffer = pFrame->Buf;

    while (DataBuffer < (pFrame->Buf + pFrame->Len))
    {
        // read type field , save C/R-Bit
        ucType = *DataBuffer++;

        // read length field
        length = *DataBuffer++;
        if (length & 0x01)
        {
            // EA-Bit = 1 -> 1 Byte lengh
            ucSegmentLen = length >> 1;
        }
        else
        {
            // EA-Bit = 0 -> 2 Byte lengh
            ucSegmentLen  = length >> 1;
            length        = *DataBuffer++;
            ucSegmentLen += ((UINT16)length) << 7;
            // shift only 7 Bits, because the first bit is the E/A-Bit
        }
        // set local pointer for the data bytes
        ucValue = DataBuffer;
        // set read ptr to next info element segment
        DataBuffer += ucSegmentLen;

        // to detect type, no C/R-Bit but E/A-Bit is used now
        switch (ucType & ~(0x02))
        {
          case DLCI0_TYPE_PARAMETER_NEGOTIATION:
            if (ucSegmentLen < 8) // neg. needs exactly 8 parameters
            {
                event = MP_IndNSC;
            }
            else
            {
                psmuxFrame = MP_pstFifoGet(&pMux->MP_freelist);       // Fifo area is allocated for psmuxFrame
                if (psmuxFrame)
                {
                    psmuxFrame->Len = ucSegmentLen;                   // length of Control Frame
                    sPrimitive.pucPtr = (UINT8*)psmuxFrame;           // copy the address of psmuxFrame
                    MEMCPY(psmuxFrame->Buf, ucValue, ucSegmentLen);   // copy the rcvd negParams
                    event = (MP_EVENTS)((ucType & 0x02) ? MP_IndNegotiation : MP_ConfNegotiation);
                    MUXDBG(ZONE_MUX_PROT1, TEXT("Received PN"));
                }
                else
                {
                    event = MP_IndNSC;
                }

            }
            break;

          case DLCI0_TYPE_CLOSE_DOWN:
            event = (MP_EVENTS)((ucType & 0x02) ? MP_IndCloseDown : MP_ConfCloseDown);
            MUXDBG(ZONE_MUX_PROT1, TEXT("Received CLOSEDOWN"));
            break;

          case DLCI0_TYPE_TEST:
            psmuxFrame = MP_pstFifoGet(&pMux->MP_freelist);           // Fifo area is allocated for psmuxFrame
            if (psmuxFrame)
            {
                psmuxFrame->Len = ucSegmentLen;                       // length of Control Frame
                sPrimitive.pucPtr = (UINT8*)psmuxFrame;               // copy the address of psmuxFrame
                MEMCPY(psmuxFrame->Buf, ucValue, ucSegmentLen);       // copy the rcvd TestFrame mirroring the contents in RespTest
                event = (MP_EVENTS)((ucType & 0x02) ? MP_IndTest : MP_ConfTest);
                MUXDBG(ZONE_MUX_PROT1, TEXT("Received TEST"));
            }
            else
            {
                event = MP_IndNSC;
            }

            break;

          case DLCI0_TYPE_MODEM_STATUS:
            sPrimitive.ulParam = 0;
            if (ucSegmentLen>2)
            {
                PRIM_WRITE_MSC_DLCI(&sPrimitive, ucValue[0]);
                PRIM_WRITE_MSC_V24(&sPrimitive, ucValue[1]);
                PRIM_WRITE_MSC_ESC(&sPrimitive, ucValue[2]);
            }
            else
            {
                PRIM_WRITE_MSC_DLCI(&sPrimitive, ucValue[0]);
                PRIM_WRITE_MSC_V24(&sPrimitive, ucValue[1]);
            }
            event = (MP_EVENTS)((ucType & 0x02) ? MP_IndMSC : MP_ConfMSC);
            MUXDBG(ZONE_MUX_PROT1, TEXT("Received MSC params=%d,%d,%d"), ucValue[0], ucValue[1], ucValue[2]);
            break;

          case DLCI0_TYPE_NON_SUPPORTED_COMMAND_RESPONSE:
            event = (MP_EVENTS)((ucType & 0x02) ? MP_IndNSC : MP_ConfNSC);
            MUXDBG(ZONE_MUX_PROT1, TEXT("Received NSC"));
            break;

          case DLCI0_TYPE_POWER_SAVING:
            {
                UINT8 ucPara;

                event = (MP_EVENTS)((ucType & 0x02) ? MP_IndPSC : MP_ConfPSC);

                // any parameter given ?
                if (ucSegmentLen > 0)
                {
                    ucPara = ucValue[0];             // get parameter
                }
                else
                {
                    ucPara = PSC_MODE_DEFAULT;       // no parameter given so use default
                }

                sPrimitive.ulParam = 0;
                PRIM_WRITE_PSC_MODE(&sPrimitive, ucPara);
                MUXDBG(ZONE_MUX_PROT1, TEXT("Received PWRSAVE param=%d"), ucPara);
                break;
            }

          default:                                   // unknown Command
            event = MP_IndNSC;
            break;
        }

        sPrimitive.Event   = event;
        sPrimitive.ucDLCI  =  DLCI0; // Control frames on dlci0 only !!!
        MP_PostUserMessage(pMux, &sPrimitive);
    }
    // all messages found are already sent to the state machine. We indicate it
    // to the calling sw not to send any message again.
    return (MP_Ind_already_sent);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   UINT8 *
            UINT32
            UINT8
 * returns: UINT8
 * ---------------------------------------------------------------------------
 * descr:   The function MP_ucRxFCS() ...
 * FCS check for data block:
 * ---------------------------------------------------------------------------
 */
UINT8 MP_ucRxFCS
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *pFCS,
    UINT32           FCSlen,
    UINT8            FCSCheckByte
)
{
    // -----------------------------------------------------------------
    // FCS in Ordnung?:
    // Anzahl Bytes , über die FCS berechnet wird:
    if (0 == FCSlen)
    {
        return (FALSE);
    }

    pMux->fcs8_RX = FCS8INIT;

    while (FCSlen--)
    {
        pMux->fcs8_RX = fcstab8[pMux->fcs8_RX ^ (*pFCS)];
        pFCS++;
    }
    pMux->fcs8_RX = fcstab8[pMux->fcs8_RX ^ FCSCheckByte];
    return ((UINT8)(pMux->fcs8_RX == FCS8OKAY));
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   UINT8 ucDLCI
 * returns: UINT8
 * ---------------------------------------------------------------------------
 * descr:   The function ucMP_TxFCS() ...
 * FCS über Address-, Control- und Length-Field berechnen:
 * ---------------------------------------------------------------------------
 */
UINT8 ucMP_TxFCS
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *p,
    UINT32           len)
{
    pMux->fcs8_TX = FCS8INIT;
    while (len--)
    {
        pMux->fcs8_TX = fcstab8[pMux->fcs8_TX ^ (*p++)];
    }
    pMux->fcs8_TX = (UINT8)(FCS8INIT - pMux->fcs8_TX);            // FCS8INIT = 0xFF

    return(pMux->fcs8_TX);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   Set global flow control for channel ucDLCI
 *          POSTCONDITION: send Flow Control (modem status) on DLCI0
 * ---------------------------------------------------------------------------
 */
void MP_vSetFlowControl
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
  if (ucDLCI == DLCI0)
  {
      return; // No FlowControl on internal channel
  }


  if (ucDLCI >= pMux->dwMaxNumberOfDLCI)
  {
      MUX_EXIT(TEXT("Mux-Exit: %d"), MP_FRA_WRONG_INSTANCE);
      return; // Wrong instance, maybe from parser
  }

  if (pMux->MP_uiUsedProtVersion == MP_REVISION_04)
  {
      if (pMux->pDLCIArray[ucDLCI].MP_RxRingBuf.Count <= (pMux->pDLCIArray[ucDLCI].MP_RxRingBuf.BufSize / 2))
      {
          Hdlc_OwnReceiverReady(pMux, ucDLCI);
      }
      return; // NO FlowControl (this type) with v4
  }


  if ((pMux->pDLCIArray[ucDLCI].MP_RxRingBuf.Count >= RX_HWM(pMux, ucDLCI))
          &&
     ((pMux->pDLCIArray[ucDLCI].sMP_DLCI.bFlowControl & LOCAL_FLOWCONTROL) == 0))
  {
      pMux->pDLCIArray[ucDLCI].sMP_Info.LFCActive++;
      pMux->pDLCIArray[ucDLCI].sMP_DLCI.sV24Status.InFlowActive = 1;
      MP_vMSC_FlowControlOff(pMux, ucDLCI);

      // send internal flow control state to application
      MP_vAppSendV24Status(pMux, ucDLCI, pMux->pDLCIArray[ucDLCI].sMP_DLCI.sV24Status);
  }
  else if ((pMux->pDLCIArray[ucDLCI].MP_RxRingBuf.Count <= RX_LWM(pMux, ucDLCI))
               &&
          ((pMux->pDLCIArray[ucDLCI].sMP_DLCI.bFlowControl & LOCAL_FLOWCONTROL)))
  {
      pMux->pDLCIArray[ucDLCI].sMP_DLCI.sV24Status.InFlowActive = 0;
      MP_vMSC_FlowControlOn(pMux, ucDLCI);

      // send internal flow control state to application
      MP_vAppSendV24Status(pMux, ucDLCI, pMux->pDLCIArray[ucDLCI].sMP_DLCI.sV24Status);
  }
  else
  {
      return;
  }

  MUXDBG(ZONE_MUX_PROT2, TEXT("fc dlci=%d lfc=%d rfc=%d"), ucDLCI,
                         (pMux->pDLCIArray[ucDLCI].sMP_DLCI.bFlowControl & LOCAL_FLOWCONTROL)  ? 1 : 0,
                         (pMux->pDLCIArray[ucDLCI].sMP_DLCI.bFlowControl & REMOTE_FLOWCONTROL) ? 1 : 0);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   send all Packet in RX-Queue to Application on selected DLCI
 *
 * ---------------------------------------------------------------------------
 */
void MP_vUpdateRxQE
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    MP_pRxRingBuf  Tmp = &pMux->pDLCIArray[ucDLCI].MP_RxRingBuf;

    if ((ucDLCI==DLCI0) || (ucDLCI >= pMux->dwMaxNumberOfDLCI))
    {
        MUX_EXIT(TEXT("Mux-Exit: %d"), MP_FRA_WRONG_INSTANCE);
        return; // Wrong instance, maybe from parser
    }

    // error detection, this should _never_ happen except the flow control does not work
    // or the sender does not react to flow control frames (MSC)
    if (Tmp->Count == Tmp->BufSize)
    {
        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Buffer full (Dlci %d, Count %d"), ucDLCI, Tmp->Count);
    }

    while (Tmp->Count > 0)
    {
        UINT32 len;
        UINT32 wr;
        if (Tmp->WriteIndex <= Tmp->ReadIndex)
        {
            len = Tmp->BufSize - Tmp->ReadIndex;
            if (Tmp->WriteIndex == Tmp->ReadIndex)
            {
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Pointer mismach, dlci=%d count=%d; Rx=Tx=%d"), ucDLCI, Tmp->Count, Tmp->ReadIndex);
            }
        }
        else
        {
            len = Tmp->WriteIndex - Tmp->ReadIndex;
        }

        wr = MP_vSendDataMP_to_App(pMux, ucDLCI, &Tmp->Buf[Tmp->ReadIndex], len, Tmp->Count);

        Tmp->ReadIndex += wr;
        Tmp->Count -= wr;
        if (Tmp->ReadIndex >= Tmp->BufSize)
        {
            Tmp->ReadIndex -= Tmp->BufSize;
        }

        // not all data could written out. wait for next MP_MyDownlink_ReadDataCallback
        if (wr != len)
        {
            break;
        }

    }
    MP_vSetFlowControl(pMux, ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   send all Packet in TX-Queue to UART
 *
 * ---------------------------------------------------------------------------
 */
void MP_vUpdateTxQE_std
(
    MUX_INSTANCE_t  *pMux
)
{
    MPFRAME      *pFrame;
    UINT8         StartDlci,CurDlci;
    UINT32        DataMaxToSendThisStep;
    BOOL          DataAvail = FALSE;

    while (pMux->MP_TxFifo.count > 0)   // DLCI0 has the highest prio, first use
    {
        pFrame = MP_pstFifoGet(&pMux->MP_TxFifo);
        MP_vFramedDataSend(pMux, pFrame->ucDlci, pFrame);
        MP_vFifoPut(&pMux->MP_freelist, pFrame);
    }


    DataMaxToSendThisStep = MP_DevGetFreeBytesInPhysTXBuf(pMux);

    if (!DataMaxToSendThisStep)
    {
        return;
    }

    // data only send when link is active
    if (pMux->MP_LinkState == eLINK_ACTIVE)
    {
        // start with "next" dlci
        StartDlci = pMux->ucLastSendDLCI + 1;
        if (StartDlci >= pMux->dwMaxNumberOfDLCI)
        {
            StartDlci = DLCI1;
        }

        do
        {
            // reset inner loop
            DataAvail = FALSE;
            CurDlci = StartDlci;

            do
            {
                // RoundRobin Implementation: send a packet one each other from every queue
                if (pMux->pDLCIArray[CurDlci].MP_TxRingBuf.Count && !(pMux->pDLCIArray[CurDlci].sMP_DLCI.bFlowControl & REMOTE_FLOWCONTROL))
                {
                    UINT32 uiDataLen = pMux->pDLCIArray[CurDlci].MP_TxRingBuf.Count;

                    if (uiDataLen > pMux->pDLCIArray[CurDlci].sMP_DLCI.uiFrameSize)
                    {
                        uiDataLen = pMux->pDLCIArray[CurDlci].sMP_DLCI.uiFrameSize;
                    }

                    if (DataMaxToSendThisStep <= (uiDataLen + 7))
                    {
                        // device reached the max for mux; break loop
                        MP_DevActivateCallback(pMux, 8, MP_vSetTXLoop);
                        return;
                    }

                    DataMaxToSendThisStep -= (uiDataLen + 7);
                    MP_vFramedDataSend_2(pMux, CurDlci, uiDataLen);
                    pMux->ucLastSendDLCI = CurDlci;


                    // update statistic values
                    pMux->pDLCIArray[CurDlci].sMP_Info.BytesWritten += uiDataLen;

                    // still data in tx ringbuffer ?
                    if (!DataAvail)
                    {
                        DataAvail = (pMux->pDLCIArray[CurDlci].MP_TxRingBuf.Count > 0);
                    }
                }
                // compute the next channel to send
                CurDlci++;
                if (CurDlci >= pMux->dwMaxNumberOfDLCI)
                {
                    CurDlci = DLCI1;
                }
            }
            while ((CurDlci != StartDlci));
        }
        while(DataAvail);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   send all Packet in TX-Queue to UART; HDLC MODE
 *
 * ---------------------------------------------------------------------------
 */
void MP_vUpdateTxQE_hdlc
(
    MUX_INSTANCE_t *pMux
)
{
    MPFRAME      *pFrame;
    UINT8         StartDlci, CurDlci;
    BOOL          DataAvail = FALSE;

    do
    {
        // MP_DevGetFreeBytesInPhysTXBuf returns the more or less _true_ free bytes in buffer in v4
        if ( MP_DevGetFreeBytesInPhysTXBuf_hdlc(pMux) <= 64)
        {   // device reached the max for mux; break loop
            MP_DevActivateCallback(pMux, RESTART_SEND_LEVEL, MP_vSetTXLoop);
            MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("TX free=%d; cannot send from TxFifo"), MP_DevGetFreeBytesInPhysTXBuf_hdlc(pMux));
            return;
        }

        if (pMux->MP_TxFifo.count)   // DLCI0 has the highest prio, first use
        {
            pFrame = MP_pstFifoGet(&pMux->MP_TxFifo);
            MP_vFramedDataSend(pMux, pFrame->ucDlci, pFrame);
            MP_vFifoPut(&pMux->MP_freelist, pFrame);
        }
    }
    while (pMux->MP_TxFifo.count);

    // MP_DevGetFreeBytesInPhysTXBuf returns the more or less _true_ free bytes in buffer in v4
    if (MP_DevGetFreeBytesInPhysTXBuf_hdlc(pMux) <= 64)
    {
        MP_DevActivateCallback(pMux, RESTART_SEND_LEVEL, MP_vSetTXLoop);
        return;
    }

    // data only send when link is active
    if (pMux->MP_LinkState==eLINK_ACTIVE)
    {
        // start with "next" dlci
        StartDlci = pMux->ucLastSendDLCI+1;
        if (StartDlci >= pMux->dwMaxNumberOfDLCI)
        {
            StartDlci = DLCI1;
        }

        do
        {
            // reset inner loop
            DataAvail = FALSE;
            CurDlci = StartDlci;

            do
            {
                if ((MP_DevGetFreeBytesInPhysTXBuf_hdlc(pMux) < ((UINT32)pMux->pDLCIArray[CurDlci].sMP_DLCI.uiFrameSize + 64)) )
                {
                    // device reached the max for mux; break loop
                    MP_DevActivateCallback(pMux, RESTART_SEND_LEVEL, MP_vSetTXLoop);
                    return;
                }

                if (pMux->pDLCIArray[CurDlci].sMP_DLCI.State == DLC_CONNECTED)
                {
                    if (MP_bHandleTxPackets(pMux, CurDlci))
                    {
                        pMux->ucLastSendDLCI = CurDlci;
                        DataAvail = TRUE;
                    }
                }

                // compute the next channel to send
                CurDlci++;
                if (CurDlci >= pMux->dwMaxNumberOfDLCI)
                {
                    CurDlci = DLCI1;
                }
            }
            while ((CurDlci != StartDlci));
        }
        while (DataAvail);
    }
}

void MP_vUpdateTxQE
(
    MUX_INSTANCE_t  *pMux
)
{
    if (pMux->MP_uiUsedProtVersion != MP_REVISION_04)
    {
        MP_vUpdateTxQE_std(pMux);
    }
    else
    {
        MP_vUpdateTxQE_hdlc(pMux);
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   ucDlci: channel number
 *          psFrame: frame to send
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   write (framed) data to connected driver / all frame types
 *
 * ---------------------------------------------------------------------------
 */
static void MP_vFramedDataSend
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci,
    MPFRAME         *psFrame
)
{
    UINT32  len, n;

    len = MP_TxFraming(pMux, ucDlci, psFrame);

    n = MP_uiDevDataSend(pMux, psFrame->Buf, len);

    if (n > len)
    {
        MUX_EXIT(TEXT("Mux-Exit: %d"), MP_FRA_SYS_ERROR);
    }
    else if (n < len)
    {
        pMux->pDLCIArray[ucDlci].sMP_Info.DevDownLostBytes += len - n;
        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Tx pkt send DATA LOST dlci=0x%02x lostbytes=%d"), ucDlci, len - n);
    }

    if (ucDlci != DLCI0)
    {
        MP_vAppCallback(pMux, ucDlci, 0);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   ucDlci: channel number
 * returns: number of copied bytes
 * ---------------------------------------------------------------------------
 * descr:   write (framed) data to connected driver / UIH frames only
 *
 * ---------------------------------------------------------------------------
 */
static void MP_vFramedDataSend_2
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci,
    UINT32           fLen
)
{
    UINT32         n, uiFrameLen;
    UINT8          ucResponse;
    UINT8          uiHeadLen = 0;
    BOOL           fTxStateChanged = FALSE;

    MP_pTxRingBuf  Ptr = &pMux->pDLCIArray[ucDlci].MP_TxRingBuf;

    if ((ucDlci==DLCI0) || (ucDlci >= pMux->dwMaxNumberOfDLCI))
    {
        MUX_EXIT(TEXT("Mux-Exit: %d"), MP_FRA_WRONG_INSTANCE);
    }

    // header length is:
    // 1 Byte StartFlag + 1 Byte Address + 1 Byte Control + 1 oder 2 Byte length

    uiFrameLen = Ptr->Count;

    if (fLen < uiFrameLen)
    {
        uiFrameLen = fLen;
    }


    // - Flag Sequence ------------------------------------------------------
    pMux->SendBuf[uiHeadLen++] = DLC_FRAME_FLAG_BASIC_MODE;

    // - Address-Field; we always have UIH on a data channel ----------------
    ucResponse = pMux->ucInitiator;

    pMux->SendBuf[uiHeadLen++] = (UINT8)(( ucDlci << 2) | ((ucResponse << 1) | DLC_FRAME_EA_BIT_SET));

    // - Control-Field: always UIH ------------------------------------------
    pMux->SendBuf[uiHeadLen++] = pMux->pDLCIArray[ucDlci].sMP_DLCI.ucLastControl = DLC_FRAME_CONTROL_TYPE_UIH;

    // - Length, normally less than 0x7F, only one byte needed --------------
    if (uiFrameLen <= 0x7F)
    {
        pMux->SendBuf[uiHeadLen++] = (UINT8)(((uiFrameLen & 0x7F) << 1) | DLC_FRAME_EA_BIT_SET);
    }
    // - Length, will need 2 bytes ------------------------------------------
    else
    {
        pMux->SendBuf[uiHeadLen++] = (UINT8)((uiFrameLen & 0x7F) << 1);  // No E/A bit set
        pMux->SendBuf[uiHeadLen++] = (UINT8)(uiFrameLen >> 7);           // 1. bit transmits 7 Bytes only !!!
    }

    // - Append Frame Data ----------------------------------------------------
    if ((Ptr->ReadIndex + uiFrameLen) <= Ptr->BufSize)
    {
        MEMCPY(&(pMux->SendBuf[uiHeadLen]), &(Ptr->Buf[Ptr->ReadIndex]), uiFrameLen);
        Ptr->ReadIndex += uiFrameLen;
    }
    else
    {
        UINT32 nCopy = Ptr->BufSize - Ptr->ReadIndex;

        MEMCPY(&(pMux->SendBuf[uiHeadLen]), &(Ptr->Buf[Ptr->ReadIndex]), nCopy);
        MEMCPY(&(pMux->SendBuf[uiHeadLen + nCopy]), &(Ptr->Buf[0]), uiFrameLen - nCopy);
        Ptr->ReadIndex = uiFrameLen - nCopy;
    }
    if (Ptr->ReadIndex >= Ptr->BufSize)
    {
        Ptr->ReadIndex -= Ptr->BufSize;
    }
    Ptr->Count -= uiFrameLen;

    // - send FCS and Frame end flag, always 2 bytes ------------------------
    pMux->SendBuf[uiHeadLen + uiFrameLen] = ucMP_TxFCS(pMux, &(pMux->SendBuf[1]), (UINT8)(uiHeadLen - 1));
    pMux->SendBuf[uiHeadLen + 1 + uiFrameLen] = DLC_FRAME_FLAG_BASIC_MODE;

    // len = Framelen + Headlen(Flag + Header) + Checksum + Flag
    n = MP_uiDevDataSend(pMux, pMux->SendBuf, uiFrameLen + uiHeadLen + 2);

    MP_vAppCallback(pMux, ucDlci, 0);

    // Update TxEmpty status
    if (!Ptr->Count)
    {
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxEmpty = 1;
        fTxStateChanged = TRUE;
    }
    // Update TxFull status
    if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxFull == 1)
    {
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxFull = 0;
        fTxStateChanged = TRUE;
    }
    if (fTxStateChanged)
    {
        MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
    }

    if (n < (uiFrameLen + 6))
    {
        MUX_EXIT(TEXT("DataSendFail dlci=%d wr=%d should=%d"), ucDlci, n, uiFrameLen+6);
        pMux->pDLCIArray[ucDlci].sMP_Info.DevDownLostBytes += (uiFrameLen + 6 - n);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   transmit application data to tx buffer, transparent
 *
 * ---------------------------------------------------------------------------
 */
UINT32 MP_uiToTxBufTransparent
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *Data,
    UINT32           len,
    UINT8            ucDlci
)
{
    MP_pTxRingBuf  Ptr = &pMux->pDLCIArray[ucDlci].MP_TxRingBuf;
    UINT32         toCopy = len;
    BOOL           fTxStateChanged = FALSE;

    if ((Ptr->Count + len) > (Ptr->BufSize - 1))
    {
        toCopy = Ptr->BufSize - Ptr->Count - 1;
    }

    if ((Ptr->WriteIndex + toCopy) <= Ptr->BufSize)
    {
        MEMCPY(&Ptr->Buf[Ptr->WriteIndex], Data, toCopy);
        Ptr->WriteIndex += toCopy;
    }
    else
    {
        UINT32 nCopy = Ptr->BufSize - Ptr->WriteIndex;
        MEMCPY(&Ptr->Buf[Ptr->WriteIndex], Data, nCopy);
        MEMCPY(&Ptr->Buf[0], Data + nCopy, toCopy - nCopy);
        Ptr->WriteIndex = toCopy - nCopy;
    }
    if (Ptr->WriteIndex >= Ptr->BufSize)
    {
        Ptr->WriteIndex -= Ptr->BufSize;
    }

    Ptr->Count += toCopy;

    if (Ptr->Count >= (Ptr->BufSize - 1))
    {
        if (!pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxFull)
        {
            fTxStateChanged = TRUE;
            pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxFull = 1;
        }
    }

    if (toCopy)
    {
        if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxEmpty)
        {
            fTxStateChanged = TRUE;
        }
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.TxEmpty = 0;
        if (fTxStateChanged)
        {
            MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
        }
        MP_WriteRequest_SendFrame(pMux, NULL, ucDlci);
    }
    else
    {
        if (fTxStateChanged)
        {
            MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
        }
    }

    return toCopy;
}

/* EOF */
