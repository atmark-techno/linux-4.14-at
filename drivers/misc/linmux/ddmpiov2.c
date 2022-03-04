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
#include "ddmpiofunc.h"


//***** Prototypes ***********************************************************
void MP_RxFraming(MUX_INSTANCE_t *pMux, UINT8 *pStart, UINT8 *pStop);
void MP_RxFramingHdlc(MUX_INSTANCE_t *pMux, UINT8 *pStart, UINT8 *pStop);


//***** EXIT CODES ************************************************************
#define MP_IO_WRONG_INSTANCE        0


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   Pointer and Len  DEV -> MP
 * returns: data written
 * ---------------------------------------------------------------------------
 * descr:   read framed data from connected (serial) driver
 *
 * ---------------------------------------------------------------------------
 */
UINT32 MP_uiDevDataReceive
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *Data,
    UINT32           len
)
{
    UINT8 count;

    if (pMux->MP_uiUsedProtVersion != MP_REVISION_04)
    {
        MP_RxFraming(pMux, Data, Data + len);

        for (count = DLCI1; count < pMux->dwMaxNumberOfDLCI; count++)
        {
          if (pMux->pDLCIArray[count].MP_RxRingBuf.Count)
          {
              MP_vSetRXLoop(pMux, (UINT8)(count | MP_RXLOOP_HIGHPRIO));
          }
        }
    }
    else
    {
        MP_RxFramingHdlc(pMux, Data, Data + len);
    }

    return len;
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   WriteDataCallback                            DEV -> MP
 *          Wakes up Mux to write framed data to device
 * ---------------------------------------------------------------------------
 */
void MP_vDevDataSendCallback
(
  MUX_INSTANCE_t  *pMux,
  UINT32           len
)
{
    MP_vSetTXLoop(pMux); // TX-Loop not loger needs any instance, so we send DLCI0 as a dummy
}



UINT32 MP_uiAppDataReceive
(
    MUX_INSTANCE_t  *pMux,
    UINT8           *Data,
    UINT32           len,
    UINT8            ucDlci
)
{
    UINT8   state;

    if ((ucDlci == DLCI0) || (ucDlci >= pMux->dwMaxNumberOfDLCI))
    {
        MUX_EXIT(TEXT("Mux-Exit: %d"), MP_IO_WRONG_INSTANCE);
    }
    state = MP_eGetState(pMux, ucDlci);

    // check if channel is connected
    if ((DLC_CONNECTED != state) && (DLC_CONNECTEDWAIT4RESPONSE != state))
    {
        MUXDBG(ZONE_MUX_PROT2, TEXT("Channel not connected dlci=%d len=%d state=%d"), ucDlci, len, state);
        /* discard data */
        return len;
    }

    return MP_uiToTxBufTransparent(pMux, Data, len, ucDlci);
}

void MP_vAppDataSendCallback
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    MP_vSetRXLoop(pMux, ucDlci  | MP_RXLOOP_HIGHPRIO);
}


void MP_vAppSetV24Status
(
    MUX_INSTANCE_t  *pMux,
    V24STATUS        status,
    UINT8            ucDlci
)
{
    BOOL bStatusChanged = FALSE;
    V24STATUS MyStatus = pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status;

    if (pMux->ucInitiator) // customer application is always master
    {
        if (MyStatus.DTR != status.DTR) {
            MyStatus.DTR = status.DTR;
            bStatusChanged = TRUE;
        }

        if (MyStatus.RTS != status.RTS)   {
            MyStatus.RTS = status.RTS;
            bStatusChanged = TRUE;
        }
    }
    else // module is always slave
    {
        if (MyStatus.CTS != status.CTS) {
            MyStatus.CTS = status.CTS;
            bStatusChanged = TRUE;
        }

        if (MyStatus.RI != status.RI)   {
            MyStatus.RI = status.RI;
            bStatusChanged = TRUE;
        }

        if (MyStatus.DSR != status.DSR) {
            MyStatus.DSR = status.DSR;
            bStatusChanged = TRUE;
        }

        if (MyStatus.DCD != status.DCD) {
            MyStatus.DCD = status.DCD;
            bStatusChanged = TRUE;
        }
    }

    // transmit the status changes using a MSC frame over the channel
    if (bStatusChanged)
    {
        // the new bits have been already written in this function
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status = MyStatus;

        if (MP_bDlciConnected(pMux, ucDlci))
        {
            // Nur, wenn der Kanal logisch verbunden ist
            MP_WriteRequest_V24Ind(pMux, ucDlci);
        }
    }
    if (!pMux->ucInitiator) // only on module side
    {
        BOOL PhysRI = FALSE, PhysDCD = FALSE;
        // from compatibility reasons the physical lines are a sum signal from
        // all logical channels for DCD and RING. If any channel has a logical
        // RING, the hardware ports RING is set, too. The same comes with DCD
        if (Mux_DevGetV24Lines_cb(pMux, &PhysRI, &PhysDCD)) {
            UINT8  i;
            BOOL   WiredRI = FALSE, WiredDCD = FALSE;

            bStatusChanged = FALSE;

            for (i = 1; i < pMux->dwMaxNumberOfDLCI; i++) {
                if (pMux->pDLCIArray[i].sMP_DLCI.sV24Status.RI)  WiredRI  = TRUE;
                if (pMux->pDLCIArray[i].sMP_DLCI.sV24Status.DCD) WiredDCD = TRUE;
            }

            if (PhysRI != WiredRI) {
                bStatusChanged = TRUE;
                PhysRI = WiredRI;
            }

            if (PhysDCD != WiredDCD) {
                bStatusChanged = TRUE;
                PhysDCD = WiredDCD;
            }

            if (bStatusChanged)
            {
                Mux_DevSetV24Lines_cb(pMux, PhysRI, PhysDCD);
            }
        }
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   Len
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   callback to connected application to write more unframed
 *          data into the mux APPi -> MP
 * ---------------------------------------------------------------------------
 */
void MP_vAppCallback
(
    MUX_INSTANCE_t  *pMux,
    UINT8           ucDlci,
    UINT32          len
)
{
    if ((ucDlci == DLCI0) || (ucDlci >= pMux->dwMaxNumberOfDLCI))
    {
        MUX_EXIT(TEXT("Mux-Exit: %d"), MP_IO_WRONG_INSTANCE);
    }
    // wake up application via callback if in stopped state
    if (pMux->MP_uiUsedProtVersion < MP_REVISION_04)
    {
        if (pMux->pDLCIArray[ucDlci].MP_TxRingBuf.Count < (pMux->pDLCIArray[ucDlci].MP_TxRingBuf.BufSize / 8))
        {
            MP_vAppReceiveDataCallback(pMux, 0, ucDlci);
        }
    }
    else
    {
        if (pMux->pDLCIArray[ucDlci].MP_TxRingBuf.Count < (pMux->pDLCIArray[ucDlci].MP_TxRingBuf.BufSize / 2))
        {
            MP_vAppReceiveDataCallback(pMux, 0, ucDlci);
        }
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   Read physical V24Values from Device, fill into MUX_DEVICE
 *          send all V24signals per selected version
 * ---------------------------------------------------------------------------
 */
void MP_vSendMSCInitialValues
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    if (ucDlci == 1)
    {
        BOOL bRing, bDCD;
        if (Mux_DevGetV24Lines_cb(pMux, &bRing, &bDCD)) {
            // fill V24structs with values from original device
            pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RI  = bRing;
            pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DCD = bDCD;
        }
    }
    MP_vMSC_SendMSC(pMux, ucDlci, 0);
}


void MP_Init
(
    MUX_INSTANCE_t  *pMux
)
{
    MP_PRIMITIVE sPrimitive;

    // standard initialization functions for the mux parts
    MP_vInitScanner(pMux);
    MP_vMemInit(pMux);
    MP_vStatemachineInit(pMux);

    // system depending initialization
    MP_vInitAdaptation(pMux);

    // sent first message to indicate startup to mux state machine
    sPrimitive.Event    = MP_IndStartUp;
    sPrimitive.ucDLCI   = DLCI0;
    MP_PostUserMessage(pMux, &sPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   EVENT, CHANNEL ID
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   sends a specified message with only one param, the channel, to the
 *          statemachine
 * ---------------------------------------------------------------------------
 */
void MP_PostSimpleUserMessage
(
    MUX_INSTANCE_t  *pMux,
    MP_EVENT_TYPE    evnt,
    UINT8            dlci
)
{
    MP_PRIMITIVE prim;
    prim.Event    = evnt;
    prim.ucDLCI   = dlci;
    prim.ulParam  = 0;
    prim.pucPtr   = NULL;
    MP_PostUserMessage(pMux, &prim);
}

/*****************************************************************************/

void MP_StartShutdownProc
(
    MUX_INSTANCE_t  *pMux
)
{
    MUXDBG(ZONE_MUX_PROT1, TEXT("MP_StartShutdownProc()"));
    MP_PostSimpleUserMessage(pMux, MP_ReqCloseDown, DLCI0);
}


/********************************************************************************/
/**
    upper layer calls here, to request a given low power state
    -> Mux sends PSC command

\return  none
*/
void MP_SleepReqEx
(
    MUX_INSTANCE_t  *pMux,
    MP_ePSCMode      newMode
)
{
    MP_PRIMITIVE   sPrimitive;

    sPrimitive.ulParam = 0;
    sPrimitive.Event   = MP_ReqPSC;
    sPrimitive.ucDLCI  = DLCI0;

    PRIM_WRITE_PSC_MODE(&sPrimitive, newMode);

    MP_PostUserMessage(pMux, &sPrimitive);
}

/* EOF */
