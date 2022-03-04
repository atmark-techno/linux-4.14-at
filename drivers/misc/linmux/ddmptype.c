/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#include "global.h"
#include "mux_callback.h"
#include "ddmpglob.h"
#include "ddmpdlch.h"


static const char TEMUXLabel[] = TEMUX_Version;     //TEMUX Version Label
static const char MSMUXLabel[] = MSMUX_Version;     //TEMUX Version Label

#define MUX_VERS_Range      0x05  // Range of characters + 1 (for SZT String Zero Termination) which are evaluated
#define Invalid_MUXVersion  0x00  // is used if a operation results an invalid MUXVersion
#define SZT_Size            0x01  // size of SZT sign (String Zero Termination)


void MP_WriteRequest_SendFrame
(
    MUX_INSTANCE_t  *pMux,
    MPFRAME         *pFrame,
    UINT8            ucDLCI
)
{
    if (pMux->MP_IndTXQueueLoopActive == FALSE)
    {
        pMux->MP_IndTXQueueLoopActive = TRUE;
        MP_PostSimpleUserMessage(pMux, MP_IndTXQueue, ucDLCI);
    }

    /* don't queue control frames when link is not up */
    if (pFrame)
    {
        if(pMux->MP_LinkState == eLINK_ACTIVE)
        {
            pFrame->ucDlci = ucDLCI;
            MP_vFifoPut(&pMux->MP_TxFifo, pFrame);
        }
        else
        {
            /* discard */
            MP_vFifoPut(&pMux->MP_freelist, pFrame);
        }
    }
}

void MP_WriteRequest_V24Ind
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
  if ((pMux->MP_V24IndLoopActive & (((UINT16)1) << ucDlci)) == 0)
  {
    pMux->MP_V24IndLoopActive |= ((UINT16)1) << ucDlci;
    MP_PostSimpleUserMessage(pMux, MP_IndV24Status, ucDlci);
  }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteRequest_SABM_DLCI0() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteRequest_SABM_DLCI0
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_SABM;
    SetTxFrameControlPFBit(psFrame);             // P - Bit = 1
    psFrame->Len = 0x00;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteRequest_SABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteRequest_SABM
(
    MPFRAME         *psFrame
)
{
   psFrame->Control = DLC_FRAME_CONTROL_TYPE_SABM;
   SetTxFrameControlPFBit(psFrame);              // P - Bit = 1
   psFrame->Len = 0x00;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteRequest_DISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteRequest_DISC
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_DISC;
    SetTxFrameControlPFBit(psFrame);             // P - Bit = 1
    psFrame->Len = 0x00;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteRequest_UIH() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteRequest_UIH
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;
    ResetTxFrameControlPFBit(psFrame);               // P - Bit = 0
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteRequest_Test() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteRequest_Test
(
    MUX_INSTANCE_t  *pMux,
    MPFRAME         *psFrame,
    UINT8            ucDLCI
)
{
    UINT8       *DataBuffer;
    UINT8        ucType;
    UINT8        ucTypeLen;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;                //Control Field UIH
    ResetTxFrameControlPFBit(psFrame);                            // P-Bit = 0

    ucType = (DLCI0_TYPE_TEST | DLC_FRAME_ADDRESS_CR_SET);        //Type Field (C/R = 1, EA = 1)
    DataBuffer = &psFrame->Buf[4];
    *DataBuffer++ = ucType;

    if (pMux->ucInitiator == 1)                                   // bInitiator = 1 initiator
    {
        ucTypeLen = sizeof(UINT8) + sizeof(TEMUXLabel) + 2;
        *DataBuffer++ = (ucTypeLen << 1 | DLC_FRAME_EA_BIT_SET);  // Length of Testmessage (EA=1)
        *DataBuffer++ = TEMUX_Version_IEI;                        // Information Element Identifier
        MEMCPY(DataBuffer, &TEMUXLabel[0] , sizeof(TEMUXLabel));
        DataBuffer += sizeof(TEMUXLabel) - SZT_Size;
    }
    else
    {
        ucTypeLen = sizeof(UINT8) + sizeof(MSMUXLabel) + 2;

        *DataBuffer++ = ((ucTypeLen << 1) | DLC_FRAME_EA_BIT_SET);  // Length of Testmessage (EA=1)
        *DataBuffer++ = MSMUX_Version_IEI;                          // Information Element Identifier
        MEMCPY(DataBuffer, &MSMUXLabel[0] , sizeof(MSMUXLabel));
        DataBuffer += sizeof(MSMUXLabel) - SZT_Size;
    }

    if (pMux->MP_uiInternalVersion >= 10)
    {
        *DataBuffer++ = 0x30 + (UINT8)(pMux->MP_uiInternalVersion / 10);
    }
    *DataBuffer++ = 0x30 + (UINT8)(pMux->MP_uiInternalVersion % 10);
    psFrame->Len = sizeof(ucType) + sizeof(ucTypeLen) + ucTypeLen;      //length of Control Frame
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
* scope:
* input:   UINT8 ucDLCI
* returns: void
* ---------------------------------------------------------------------------
* descr:   The function MP_WriteRequest_MSC() ...
* ---------------------------------------------------------------------------*/
void MP_WriteRequest_MSC
(
    MPFRAME         *psFrame,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8       *DataBuffer;
    UINT8        ucLength;
    UINT8        ucType;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;
    ResetTxFrameControlPFBit(psFrame);               // P - Bit = 0
    // Fill control channel structure
    ucType = DLCI0_TYPE_MODEM_STATUS | DLC_FRAME_ADDRESS_CR_SET;

    ucLength =  2;  // 2 Bytes Header
    if (PRIM_READ_MSC_ESC(psPrimitive))
    {
      ucLength++; // 3 Bytes with Esc-Info
    }
    // Overall length of frame == Length of header + length of segment
    psFrame->Len = 2 + ucLength;
    // Load data: The 1st 4 bytes are header, after that is start of data
    DataBuffer = &psFrame->Buf[4];
    *DataBuffer++ = ucType;
    *DataBuffer++ = (ucLength << 1) | 0x01;
    *DataBuffer++ = PRIM_READ_MSC_DLCI(psPrimitive);
    *DataBuffer++ = PRIM_READ_MSC_V24(psPrimitive);
    *DataBuffer++ = PRIM_READ_MSC_ESC(psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
* scope:
* input:   UINT8 ucDLCI
* returns: void
* ---------------------------------------------------------------------------
* descr:   The function MP_WriteResponse_MSC() ...
* ---------------------------------------------------------------------------*/
void MP_WriteResponse_MSC
(
    MPFRAME         *psFrame,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8       *DataBuffer;
    UINT8        ucLength;
    UINT8        ucType;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;
    ResetTxFrameControlPFBit(psFrame);            // P - Bit = 0
    // Fill structure control channel, delete response bit 1 / don't set
    ucType = DLCI0_TYPE_MODEM_STATUS;

    ucLength = 2; // 2 Bytes Header
    if (PRIM_READ_MSC_ESC(psPrimitive))
    {
        ucLength++; // 3 Bytes with Esc-Info
    }
    // Overall length of frame == Length of header + length of segment
    psFrame->Len = 2 + ucLength;
    // Load data: The 1st 4 bytes are header, after that is start of data
     DataBuffer   = &psFrame->Buf[4];
    *DataBuffer++ = ucType;
    *DataBuffer++ = (ucLength << 1) | 0x01;
    *DataBuffer++ = PRIM_READ_MSC_DLCI(psPrimitive);
    *DataBuffer++ = PRIM_READ_MSC_V24(psPrimitive);
    *DataBuffer++ = PRIM_READ_MSC_ESC(psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 *          UINT8 ucMode
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteRequest_PSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteRequest_PSC
(
    MPFRAME         *psFrame,
    UINT8            ucMode
)
{
    UINT8       *DataBuffer;
    UINT8        ucLength;
    UINT8        ucType;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;
    ResetTxFrameControlPFBit(psFrame);                  // P - Bit = 0
    ucLength = 0;

    // Type:
    ucType = DLCI0_TYPE_POWER_SAVING | DLC_FRAME_ADDRESS_CR_SET; // C/R-Bit = 1

    // prepare packet
    DataBuffer = &psFrame->Buf[4];
    DataBuffer[0] = ucType;

    // don't add parameter in default mode
    if(ucMode != PSC_MODE_DEFAULT)
    {
        DataBuffer[2] = ucMode;                             // parameter
        ucLength += 1;                                      // 1 byte parameter length
    }

    DataBuffer[1] = ((ucLength << 1) | DLC_FRAME_EA_BIT_SET) ;

    psFrame->Len = 2 + ucLength;
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteRequest_CloseDown() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteRequest_CloseDown
(
    MPFRAME         *psFrame
)
{
    UINT8       *DataBuffer;
    UINT8        ucLength;
    UINT8        ucType;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;
    ResetTxFrameControlPFBit(psFrame);               // P - Bit = 0
    psFrame->Len = 2;
    // Fill structue control channel
    // Type:
    ucType = DLCI0_TYPE_CLOSE_DOWN | DLC_FRAME_ADDRESS_CR_SET; // C/R-Bit = 1
    // Length and Value-Field
    ucLength = 0;
    // Load Data
    DataBuffer = &psFrame->Buf[4];
    *DataBuffer++ = ucType;
    // Reload length
    *DataBuffer++ = ((ucLength << 1) | DLC_FRAME_EA_BIT_SET);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_SABM_DLCI0() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_DM_SABM
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_DM;
    SetTxFrameControlPFBit(psFrame);             // F - Bit = 1
    psFrame->Len = 0x00;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_SABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_UA_SABM
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UA;
    SetTxFrameControlPFBit(psFrame);             // F - Bit = 1
    psFrame->Len = 0x00;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_DISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_DM_DISC
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_DM;
    SetTxFrameControlPFBit(psFrame);             // F - Bit = 1
    psFrame->Len = 0x00;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_UA_DISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_UA_DISC
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UA;
    SetTxFrameControlPFBit(psFrame);             // F - Bit = 1
    psFrame->Len = 0x00;
}



/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_Test() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_Test
(
    MPFRAME         *psFrame,
    MPFRAME         *psmuxFrame
)
{
    UINT8       *DataBuffer;
    UINT8        ucType;
    UINT16       uiTypeLen;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;                      // Cotrol Field UIH
    ResetTxFrameControlPFBit(psFrame);                                  // P-Bit = 0
    ucType        = DLCI0_TYPE_TEST;                                    // Type Field (C/R = 0, EA = 1)
    DataBuffer    = &psFrame->Buf[4];
    *DataBuffer++ = ucType;
    uiTypeLen     = psmuxFrame->Len;

    if (uiTypeLen < 0x7F)
    {
        // only one Length byte is needed (EA = 1)
        *DataBuffer++ = (UINT8)(uiTypeLen << 1) | DLC_FRAME_EA_BIT_SET;
        psFrame->Len  = sizeof(ucType) + sizeof(UINT8) + uiTypeLen;     //length of Control Frame
    }
    else
    {
        // two bytes are used for Length (EA = 0)
        *DataBuffer++ = (UINT8)(uiTypeLen << 1);
        *DataBuffer++ = (UINT8)((((uiTypeLen << 1) & 0xFF00) >> 8) | DLC_FRAME_EA_BIT_RESET);
        psFrame->Len  = sizeof(ucType) + sizeof(UINT16) + uiTypeLen;    // length of Control Frame
    }

    // copy frame contents as received before avoiding buffer overflow
    MEMCPY(DataBuffer, psmuxFrame->Buf , min(psmuxFrame->Len, (UINT16)sizeof(psFrame->Buf)));
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_Write_Negotiation() ...
 * ---------------------------------------------------------------------------
 */
void MP_Write_Negotiation
(
    MUX_INSTANCE_t  *pMux,
    MPFRAME         *psFrame,
    UINT8            ucDlci,
    BOOL             Resp
)
{
    UINT8       *DataBuffer;
    UINT16       uiFrameSize;

    psFrame->ucDlci  = DLCI0;                        // always use DLCI0 for param neg.
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;   //Control Field UIH

    if (Resp)
    {
        ResetTxFrameControlPFBit(psFrame);           // P-Bit = 0
    }
    else
    {
        SetTxFrameControlPFBit(psFrame);             // P-Bit = 1
    }

    psFrame->Len = 10;                               // length of Control Frame
    DataBuffer   = &psFrame->Buf[4];
    *DataBuffer  = DLCI0_TYPE_PARAMETER_NEGOTIATION;
    if (!Resp)
    {
        *DataBuffer |= DLC_FRAME_ADDRESS_CR_SET;
    }
    DataBuffer++;

    *DataBuffer++ = (8 << 1) | DLC_FRAME_EA_BIT_SET;

    // now set the correct values for out data: see ddmpdlch.h for details! 8 Bytes

    // (1)
    *DataBuffer++ = ucDlci;

    // (2)
    if (pMux->MP_uiUsedProtVersion >= MP_REVISION_04)
    {
        // I-Frame + special Cinterion WM convergence layer (8)
        *DataBuffer++ = WM_SPECIAL_UIH_WITH_WND;
    }
    else
    {
        if (pMux->pDLCIArray[ucDlci].sMP_DLCI.ucFrameType == 0)
        {
            *DataBuffer++ = 0;
        }
        else
        {
            // I-Frame + special Cinterion WM convergence layer (8)
            *DataBuffer++ = WM_SPECIAL_CONVERGENCE_TYPE;
        }
    }


    // (3)
    *DataBuffer++ = ucDlci; // priority is the same as dlci

    // (4)
    if ((pMux->MP_uiUsedProtVersion >= MP_REVISION_04) && (( HDLC_TO_T1 * 100) < 0xff))
    {
        /* HDLC_TO_T1 is in seconds, PN: The units are hundredths of a second*/
        *DataBuffer++ = (UINT8)(HDLC_TO_T1 * 100);
    }
    else
    {
        *DataBuffer++ = 0xff; // T1 acknowledge timer to the max
    }


    // (5)
    // (6)
    uiFrameSize = Resp ? pMux->pDLCIArray[ucDlci].sMP_DLCI.uiFrameSize : (UINT16)pMux->dwMpFrameSize;
    *DataBuffer++ = (UINT8)(uiFrameSize     ); // LO
    *DataBuffer++ = (UINT8)(uiFrameSize >> 8); // HI

    // (7)
    *DataBuffer++ = 0;  // number of retransmissions

    // (8) // window size for error recovery mode
    *DataBuffer++ = Resp ? pMux->pDLCIArray[ucDlci].sHDLC.WindowSize : (UINT16)pMux->dwMpWindowSize;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_PnCompare() ...
 * ---------------------------------------------------------------------------
 */
void MP_AdjustQueueSize
(
  MUX_INSTANCE_t  *pMux
)
{
  UINT32   uiQueueSize;
  UINT16   uiMaxFrameSize = DEF_MAXFRAMESIZE;
  UINT8    uiMaxWindowSize = 1;
  UINT8    i;

  for (i = 1; i < pMux->dwMaxNumberOfDLCI; i++) {
    if (pMux->pDLCIArray[i].sMP_DLCI.uiFrameSize > uiMaxFrameSize) {
      uiMaxFrameSize = pMux->pDLCIArray[i].sMP_DLCI.uiFrameSize;
    }
    if (pMux->pDLCIArray[i].sHDLC.WindowSize > uiMaxWindowSize) {
      uiMaxWindowSize = pMux->pDLCIArray[i].sHDLC.WindowSize;
    }
  }

  if (pMux->MP_uiUsedProtVersion >= MP_REVISION_04) {
    uiQueueSize = uiMaxWindowSize * (uiMaxFrameSize + 8);
    // Do not allow to pile up more bytes in the queues than can be sent within 3/4 of a second
    // to prevent unnecessary frame resends
    if (pMux->dwMpBaudRate) {
      uiQueueSize = min(uiQueueSize, (pMux->dwMpBaudRate * 3) / (4 * 10));
    }
    // Ensure that we have enough room for at least one frame + RESTART_SEND_LEVEL
    uiQueueSize = max((UINT32)uiQueueSize, (UINT32)(uiMaxFrameSize + 8 + RESTART_SEND_LEVEL));
  } else {
    uiQueueSize = 5 * (uiMaxFrameSize + 8);
  }

  Mux_DevSetQueueSizes_cb(pMux, uiQueueSize, uiQueueSize);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_PnCompare() ...
 * ---------------------------------------------------------------------------
 */
void MP_PnCompare
(
    MUX_INSTANCE_t  *pMux,
    MPFRAME         *psFrame,
    UINT8            ucDlci
)
{
    UINT8       *Data = &psFrame->Buf[0];
    UINT32       fLen, BufSize;
/*
    Data[0] = DLCI
    Data[1] = FrameType
    Data[2] = Prio
    Data[3] = T1 ack timer
    Data[4] = Max Framesize Lo Byte
    Data[5] = Max Framesize Hi Byte
    Data[6] = number retransmissions
    Data[7] = windows size for recovery mode
*/

    if (Data[0] != ucDlci) // consistence check
    {
        MUX_EXIT(TEXT("Mux-Exit: %d"), 0x0f);
    }

    // Frame type handling: WM special types available?
    if (Data[1] != 0x00)
    {
      if (pMux->MP_uiUsedProtVersion >= MP_REVISION_04)
      {
        if ( (Data[1] == WM_SPECIAL_UIH_WITH_WND_FULL_CHKSUM) ||
             (Data[1] == WM_SPECIAL_UIH_WITH_WND) )
        {
          pMux->pDLCIArray[ucDlci].sMP_DLCI.ucFrameType = Data[1];
        }
      }
    }

    // Window Size for HDLC mode
    if ((Data[7] > 1) && (Data[7] < 8))
    {
        pMux->pDLCIArray[ucDlci].sHDLC.WindowSize = Data[7];
        MUXDBG(ZONE_GENERAL_INFO | ZONE_MUX_PROT1, TEXT("Dlci %d - Hdlc window size: Requested: %d, Received %d, Used: %d"), ucDlci, pMux->dwMpWindowSize, Data[7], pMux->pDLCIArray[ucDlci].sHDLC.WindowSize);
    }
    else
    {
        if (pMux->MP_uiUsedProtVersion >= MP_REVISION_04)
        {
            MUX_EXIT(TEXT("Dlci %d - Received invalid hdlc window size %d"), ucDlci, Data[7]);
        }
    }

    fLen = (UINT32)Data[4] | ((UINT32)Data[5] << 8);

    BufSize = min(pMux->pDLCIArray[ucDlci].MP_RxRingBuf.BufSize, pMux->pDLCIArray[ucDlci].MP_TxRingBuf.BufSize);
    if (pMux->MP_uiUsedProtVersion >= MP_REVISION_04)
    {
        // max packet size = Min Buffer / Windows Size
        BufSize /= pMux->pDLCIArray[ucDlci].sHDLC.WindowSize + 1;
    }
    // adapt to lower bound: 98
    fLen = max(fLen, (UINT32)DEF_MAXFRAMESIZE);
    // adapt to upper bound: ringbuffer
    fLen = min(fLen, BufSize);
    // adapt to upper bound: receive buffer
    fLen = min(fLen, (UINT32)MAXFRAMESIZE);
    // adapt to requested frame size
    fLen = min(fLen, pMux->dwMpFrameSize);

    pMux->pDLCIArray[ucDlci].sMP_DLCI.uiFrameSize = fLen;

    MP_AdjustQueueSize(pMux);

    MUXDBG(ZONE_GENERAL_INFO | ZONE_MUX_PROT1, TEXT("Dlci %d - Mux frame size: Requested: %d, Received %d, Used: %d"), ucDlci, pMux->dwMpFrameSize, (UINT32)Data[4] | ((UINT32)Data[5] << 8), fLen);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_VersCompare() ...
 * ---------------------------------------------------------------------------
 */
void MP_VersCompare
(
    MUX_INSTANCE_t  *pMux,
    MPFRAME         *psmuxFrame
)
{
    UINT8       *DataBuffer;

    DataBuffer = psmuxFrame->Buf;

    if (sizeof(TEMUXLabel) <= psmuxFrame->Len)
    {
        DataBuffer += sizeof(TEMUXLabel);   //set pointer to Version Character
        pMux->MP_uiExternalVersion = (UINT16)atoi((char *)DataBuffer);
    }
    else
    {
       pMux->MP_uiExternalVersion = MP_REVISION_01;
    }


    pMux->MP_uiUsedProtVersion = (pMux->MP_uiExternalVersion < pMux->MP_uiInternalVersion) ? pMux->MP_uiExternalVersion : pMux->MP_uiInternalVersion;

    if (pMux->MP_uiUsedProtVersion >= MP_REVISION_LAST)
    {
        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("VersionControl Syntax Error Ve=%d Vi=%d use=%d"), pMux->MP_uiExternalVersion, pMux->MP_uiInternalVersion, pMux->MP_uiUsedProtVersion);
        pMux->MP_uiUsedProtVersion = MP_REVISION_01;
    }
    else
    {
        if ((pMux->MP_uiUsedProtVersion >= MP_PROTOCOL_VERSION_MIN) && (pMux->MP_uiUsedProtVersion <= MP_PROTOCOL_VERSION_MAX))
        {
            MUXDBG(ZONE_GENERAL_INFO | ZONE_MUX_PROT1, TEXT("Protocol-Version: Requested: %d, Supported: %d, Used: %d"), pMux->MP_uiInternalVersion, pMux->MP_uiExternalVersion, pMux->MP_uiUsedProtVersion);
            MP_AdjustQueueSize(pMux);
        }
        else
        {
            MUXDBG(ZONE_MUX_PROT1, TEXT("VersionControl Error Ve=%d Vi=%d use=%d"), pMux->MP_uiExternalVersion, pMux->MP_uiInternalVersion, pMux->MP_uiUsedProtVersion);
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_PSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_PSC
(
    MPFRAME         *psFrame,
    UINT8            ucResult
)
{
    UINT8       *DataBuffer;
    UINT8        ucLength;
    UINT8        ucType;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;
    ResetTxFrameControlPFBit(psFrame);               // P - Bit = 0

    /* type */
    ucType = DLCI0_TYPE_POWER_SAVING;
    ucLength = 0;

    /* prepare packet */
    DataBuffer = &psFrame->Buf[4];
    DataBuffer[0] = ucType;

    /* don't add parameter in default mode */
    if(ucResult != PSC_RES_DEFAULT)
    {
        DataBuffer[2] = ucResult;
        ucLength += 1;                              // 1 parameter byte length
    }

    DataBuffer[1] = (ucLength << 1) | DLC_FRAME_EA_BIT_SET;

    psFrame->Len = 2 + ucLength;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_CloseDown() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_CloseDown
(
    MPFRAME         *psFrame
)
{
    UINT8       *DataBuffer;
    UINT8        ucLength;
    UINT8        ucType;

    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;

    ucType = DLCI0_TYPE_CLOSE_DOWN;
    ResetTxFrameControlPFBit(psFrame);               // P - Bit = 0
    psFrame->Len = 2;
    ucLength = 0;
    DataBuffer = &psFrame->Buf[4];
    *DataBuffer++ = ucType;
    *DataBuffer++ = (ucLength << 1) | DLC_FRAME_EA_BIT_SET;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_WriteResponse_RespNSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_WriteResponse_RespNSC
(
    MPFRAME         *psFrame
)
{
    psFrame->Control = DLC_FRAME_CONTROL_TYPE_UIH;
    ResetTxFrameControlPFBit(psFrame);               // P - Bit = 0

/* ---------------------------------------------------------------------------*/
    psFrame->Len = 0x00;
}

/* EOF */
