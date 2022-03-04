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
#include "mux_callback.h"
#include "ddmphdlc.h"


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCLOSEDDOWN_IndStartUp() ...
 * ---------------------------------------------------------------------------
 */
void MP_vCLOSEDDOWN_IndStartUp
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    if (psPrimitive->ucDLCI == DLCI0)
    {   // Enable Multiplexer Control Channel
        MP_vSetState(pMux, DLC_WAIT4STARTUP, DLCI0);


        // CLOSEDDOWN -> DLC_WAIT4STARTUP -> DISCONNECTED
        // Statemachine Response senden
        psPrimitive->Event  =  MP_RespStartUpOkay;
        psPrimitive->ucDLCI =  DLCI0;
        MP_PostUserMessage (pMux, psPrimitive);

    }
    else
    {   // CLOSEDDOWN -> DLC_WAIT4STARTUP -> CLOSEDDOWN
        // Statemachine Response senden
        psPrimitive->Event  =  MP_RespStartUpError;
        psPrimitive->ucDLCI =  DLCI0;
        MP_PostUserMessage (pMux, psPrimitive);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCLOSEDDOWN_RequStartUp() ...
 * ---------------------------------------------------------------------------
 */
void MP_vCLOSEDDOWN_RequStartUp
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    if (psPrimitive->ucDLCI == DLCI0)
    {
        // wait for ConfirmStartUpOkay
        MP_vSetState(pMux, DLC_WAIT4STARTUP, DLCI0);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vWAIT4STARTUP_RespStartUpOkay() ...
 * ---------------------------------------------------------------------------
 */
void MP_vWAIT4STARTUP_RespStartUpOkay
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8   i;

    if (psPrimitive->ucDLCI != DLCI0)
    {
        // Error
    }
    else
    {
        MP_vSetState(pMux, DLC_DISCONNECTED, DLCI0);
        // set all available channels to disconnected
        // CLOSEDDOWN -> DISCONNECTED
        for (i = 1; i < pMux->dwMaxNumberOfDLCI; i++)
        {
            MP_vSetState(pMux, DLC_DISCONNECTED, i);
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vWAIT4STARTUP_RespStartUpError() ...
 * ---------------------------------------------------------------------------
 */
void MP_vWAIT4STARTUP_RespStartUpError
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_CLOSEDDOWN, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vWAIT4STARTUP_ConfStartUpOkay() ...
 * ---------------------------------------------------------------------------
 */
void MP_vWAIT4STARTUP_ConfStartUpOkay
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8  i;

    if (psPrimitive->ucDLCI == DLCI0)
    {
        // Enable all logic channels
        // CLOSEDDOWN -> DISCONNECTED
        for (i = 0; i < pMux->dwMaxNumberOfDLCI; i++)
        {
            MP_vSetState(pMux, DLC_DISCONNECTED, i);
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vWAIT4STARTUP_ConfStartUpError() ...
 * ---------------------------------------------------------------------------
 */
void MP_vWAIT4STARTUP_ConfStartUpError
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_CLOSEDDOWN, psPrimitive->ucDLCI);
}



/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_IndSABM_DLCI0() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTED_IndSABM_DLCI0
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8   DLCI_ready = 1;

    if (psPrimitive->ucDLCI != DLCI0)
    {
        //ERROR
        DLCI_ready = 0;
    }
    else
    {
        pMux->ucInitiator = 0;

        MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, DLCI0);

        // prepare the response
        // Send statemachine response
        psPrimitive->ucDLCI = DLCI0;
        if(DLCI_ready == 1)  // DLCI will be activated
        {
            psPrimitive->Event =  MP_RespUA_SABM;
            pMux->DLCI0_Established = 1;
        }
        else                  // DLCI will not be activated
        {
            psPrimitive->Event =  MP_RespDM_SABM;
            pMux->DLCI0_Established = 0;
        }
        MP_PostUserMessage(pMux, psPrimitive);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_ReqSABM_DLCI0() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTED_ReqSABM_DLCI0
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    if (psPrimitive->ucDLCI != DLCI0)
    {
        //ERROR
    }
    else
    {
        pMux->ucInitiator = 1; // bInitiator = 1, when initiating station

        MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, DLCI0);

        // send request !!!
        psFrame = MP_pstFifoGet(&pMux->MP_freelist);
        if (psFrame)
        {
            MP_WriteRequest_SABM_DLCI0(psFrame);
            MP_WriteRequest_SendFrame(pMux, psFrame, DLCI0);
            pMux->DLCI0_Established = 1;
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_IndSABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTED_IndSABM
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);

    // Send statemachine response
    if (pMux->DLCI0_Established)
    {
        // DLCI will be activated
        psPrimitive->Event =  MP_RespUA_SABM;
    }
    else
    {
        // DLCI will not be activated
        psPrimitive->Event =  MP_RespDM_SABM;
    }
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_ReqSABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTED_ReqSABM
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME  *psFrame;
    UINT8     ucDLCI = psPrimitive->ucDLCI;

    if ((ucDLCI == DLCI0) || (pMux->DLCI0_Established == 0))
    {
        // request not possible
    }
    else
    {
        MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, ucDLCI);

        // send request
        psFrame = MP_pstFifoGet(&pMux->MP_freelist);
        if (psFrame)
        {
            MP_WriteRequest_SABM(psFrame);
            MP_WriteRequest_SendFrame(pMux, psFrame, ucDLCI);
            MP_vInitPacketHandler(pMux, ucDLCI);
            Hdlc_Init(pMux, ucDLCI, pMux->pDLCIArray[ucDLCI].sHDLC.WindowSize);
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_IndDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTED_IndDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_ReqDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTED_ReqDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME  *psFrame;

    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);

    // send request
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteRequest_DISC(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDNEGOTIATION_IndDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDNEGOTIATION_IndDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDNEGOTIATION_ReqDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDNEGOTIATION_ReqDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);

    // send Request
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteRequest_DISC(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_SABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_SABM
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    MP_vSetState(pMux, DLC_DISCONNECTED, psPrimitive->ucDLCI);

    // send Response
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteResponse_DM_SABM(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_SABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_SABM
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;
    UINT8    ucDLCI = psPrimitive->ucDLCI;

    MP_vSetState(pMux, DLC_CONNECTED, ucDLCI);

    MP_PostSimpleUserMessage(pMux, MP_IndMemTest, DLCI0);

    // send response
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteResponse_UA_SABM(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, ucDLCI);
    }

    // unblock application
    if (ucDLCI != DLCI0)
    {
        MP_vAppCallback(pMux, ucDLCI, 0);
    }

    if ((ucDLCI != DLCI0) && (pMux->MP_uiUsedProtVersion >= MP_REVISION_03))
    {
        // inform about the current states of virtual V24 lines
        MP_vSendMSCInitialValues(pMux, ucDLCI);
        // init HDLC; do not care about mux version; init is always save
        MP_vInitPacketHandler(pMux, ucDLCI);
        Hdlc_Init(pMux, ucDLCI, pMux->pDLCIArray[ucDLCI].sHDLC.WindowSize);
    }

    // Inform channels
    if (ucDLCI == DLCI0) {
        Mux_StartResult_cb(pMux, MUX_OK);
    } else {
        Mux_DLCIEstablishResult_cb(pMux, (DWORD)ucDLCI, MUX_OK);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_SABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_SABM
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTED, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_SABM() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_SABM
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_CONNECTED, psPrimitive->ucDLCI);

    if (psPrimitive->ucDLCI == 0)           //executing only once after sending SABM for DLCI_0
    {
        psPrimitive->Event = MP_ReqTest;    // 1st trigger executing the Version check
        MP_PostUserMessage(pMux, psPrimitive);
        // Inform channel
        Mux_StartResult_cb(pMux, MUX_OK);
    }
    else
    {
        MP_vMSC_FlowControlOn(pMux, psPrimitive->ucDLCI);
        MP_vAllEvents_ReqNegotiation(pMux, psPrimitive);
        // Inform channel
        Mux_DLCIEstablishResult_cb(pMux, (DWORD)psPrimitive->ucDLCI, MUX_OK);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_EndeT1() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_EndeT1
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTED, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_DISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_DISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    MP_vSetState(pMux, DLC_DISCONNECTED, psPrimitive->ucDLCI);

    pMux->pDLCIArray[psPrimitive->ucDLCI].sMP_DLCI.ucFrameType = 0;

    // send Response
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteResponse_UA_DISC(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }

    // Inform channel
    if (psPrimitive->ucDLCI != DLCI0)
    {
        Mux_DLCIReleaseResult_cb(pMux, (DWORD)psPrimitive->ucDLCI, MUX_OK);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_DISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_DISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    MP_vSetState(pMux, DLC_DISCONNECTED, psPrimitive->ucDLCI);

    // send Response
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteResponse_DM_DISC(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_DISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_DISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTED, psPrimitive->ucDLCI);
    // Inform channel
    if (psPrimitive->ucDLCI != DLCI0)
    {
        Mux_DLCIReleaseResult_cb(pMux, (DWORD)psPrimitive->ucDLCI, MUX_OK);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_DISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_DISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTED, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_IndDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_IndDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);
    // send statemachine response
    psPrimitive->Event = MP_RespUA_DISC;
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDWAIT4UAFRAME_ReqDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vDISCONNECTEDWAIT4UAFRAME_ReqDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);

    // Send request !!!
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteRequest_DISC(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_IndNegotiation() ...
 *          Indication Negotiation was recognized, prepare the negotiatiation
 *          and prepare the response
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_IndNegotiation
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    // send Statemachine Response; the frame info will read when getting this frame
    // as a response, we use the same frame :-)
    psPrimitive->Event = MP_RespNegotiation;
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDNEGOTIATION_IndNegotiation() ...
 *          negotiate the parameters and respond the command
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_RespNegotiation
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME  *psFrame = (MPFRAME *)psPrimitive->pucPtr;
    UINT8     ucSetDlci = psFrame->Buf[0];

    // read frame / set own parameters depending on the frame
    MP_PnCompare(pMux, psFrame, ucSetDlci);

    // write own infos into the frame
    MP_Write_Negotiation(pMux, psFrame, ucSetDlci, TRUE);

    // send frame back as the response
    MP_WriteRequest_SendFrame(pMux, psFrame, DLCI0);

    // send status lines
    if (pMux->MP_uiUsedProtVersion >= MP_REVISION_04)
    {
        if (MP_bDlciConnected(pMux, ucSetDlci))
        {
            // Only if channel is connected
            MP_WriteRequest_V24Ind(pMux, ucSetDlci);
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTED_ReqNegotiation() ...
 *          Negotiate the parameters, send a request
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ReqNegotiation
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    // send request
    MUXDBG(ZONE_MUX_PROT1, TEXT("*** PN Request dlci=%02x req= %s ***"), psPrimitive->ucDLCI, pMux->pDLCIArray[psPrimitive->ucDLCI].sMP_DLCI.ucFrameType ? TEXT("Cinterion Type"): TEXT("Standard 0"));
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_Write_Negotiation(pMux, psFrame, psPrimitive->ucDLCI, FALSE);
        MP_WriteRequest_SendFrame(pMux, psFrame, DLCI0);
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vDISCONNECTEDNEGOTIATION_ReqNegotiation() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ConfNegotiation
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME  *psFrame = (MPFRAME *)psPrimitive->pucPtr;
    UINT8     ucSetDlci = psFrame->Buf[0];

    // setup new framesize value
    MP_PnCompare(pMux, psFrame, ucSetDlci);

    MP_vFifoPut(&pMux->MP_freelist, psFrame);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_IndDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_IndDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);

    // send state machine response
    psPrimitive->Event = MP_RespUA_DISC;
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_ReqDISC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ReqDISC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    MP_vSetState(pMux, DLC_DISCONNECTEDWAIT4UAFRAME, psPrimitive->ucDLCI);

    // send request
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteRequest_DISC(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_IndUIH() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_IndUIH
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    // no answer, this event should never happen!
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_ReqUIH() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ReqUIH
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    // only trigger the queue, frame is already there
    MP_WriteRequest_SendFrame(pMux, NULL, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_IndTest() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_IndTest
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    // send state machine Response
    psPrimitive->Event = MP_RespTest;
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_ReqTest() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ReqTest
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME*  psFrame;

    // send request
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteRequest_Test(pMux, psFrame, psPrimitive->ucDLCI);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTEDWAIT4RESPONSE_RespTest() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_RespTest
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;
    MPFRAME *psmuxFrame;

    // send Response
    psmuxFrame = (MPFRAME *)psPrimitive->pucPtr;

    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteResponse_Test(psFrame, psmuxFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }

    if ((psPrimitive->ucDLCI == 0) && (psmuxFrame->Buf[0] == TEMUX_Version_IEI))
    {
        //triggering MS Mux Version TestMessage
        psPrimitive->Event = MP_ReqTest;
        MP_PostUserMessage(pMux, psPrimitive);
    }

    if ((psmuxFrame->Buf[0] == MSMUX_Version_IEI) ||(psmuxFrame->Buf[0] == TEMUX_Version_IEI))
    {
        //triggering the version comparison
        MP_VersCompare(pMux, psmuxFrame);
    }

    MP_vFifoPut(&pMux->MP_freelist, psmuxFrame);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTEDWAIT4RESPONSE_ConfTest() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ConfTest
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psmuxFrame = (MPFRAME *)psPrimitive->pucPtr;

    if (psmuxFrame)
    {
        MP_vFifoPut(&pMux->MP_freelist, psmuxFrame);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_IndPSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_IndPSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8   ucMode;
    UINT8   ucResult = PSC_RES_SUCCESS;

    //ucMode = psPrimitive->uMsg.stIndPSC.ucMode;
    ucMode = PRIM_READ_PSC_MODE(psPrimitive);

    MUXDBG(ZONE_MUX_PROT1, TEXT("PSC Indication, Mode: %d"), ucMode);

    // send State machine response
    psPrimitive->Event = MP_RespPSC;

    /* in default mode there is no result code */
    if(ucMode == PSC_MODE_DEFAULT)
    {
        PRIM_WRITE_PSC_MODE(psPrimitive, PSC_RES_DEFAULT);
    }
    else
    {
        PRIM_WRITE_PSC_MODE(psPrimitive, ucResult);
    }
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_ConfPSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ConfPSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8   ucResult;

    ucResult = PRIM_READ_PSC_MODE(psPrimitive);

    MUXDBG(ZONE_MUX_PROT1, TEXT("PSC Confirm, Result: %d"), ucResult);

    if(pMux->MP_LinkState == eLINK_SHUTTING_DOWN)
    {
        pMux->MP_LinkState = eLINK_DOWN;
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_ReqPSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ReqPSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME  *psFrame;
    UINT8     ucMode;

    ucMode = PRIM_READ_PSC_MODE(psPrimitive);

    MUXDBG(ZONE_MUX_PROT1, TEXT("PSC Request, Mode: %d"), ucMode);

    if(pMux->MP_LinkState == eLINK_ACTIVE)
    {
        // send request
        psFrame = MP_pstFifoGet(&pMux->MP_freelist);
        if (psFrame)
        {
            MP_WriteRequest_PSC(psFrame, ucMode);
            MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
        }
        pMux->MP_LinkState = eLINK_SHUTTING_DOWN;
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_ReqWake() ...
 * ---------------------------------------------------------------------------
 */

#define REQWAKE_SEQ_LEN             20          // length of wakeup sequence

void MP_vAllEvents_ReqWake
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT32  i = REQWAKE_SEQ_LEN;
    UINT8   Wake[] = { 0xf9 };

    MUXDBG(ZONE_MUX_PROT1, TEXT("Wake Request"));

    if((pMux->MP_LinkState == eLINK_DOWN) || (pMux->MP_LinkState == eLINK_WAKING_UP))
    {
        pMux->MP_LinkState = eLINK_WAKING_UP;

        while(i)
        {
            (void) MP_uiDevDataSend(pMux, Wake, sizeof(Wake));
            i--;
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_RespWake() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_RespWake
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8 Wake[] = { 0xf9 };

    MUXDBG(ZONE_MUX_PROT1, TEXT("Wake Respone"));

    if(pMux->MP_LinkState == eLINK_DOWN)
    {
        pMux->MP_LinkState = eLINK_ACTIVE;

        (void) MP_uiDevDataSend(pMux, Wake, 1);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_RespPSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_RespPSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME  *psFrame;
    UINT8     ucResult;

    ucResult = PRIM_READ_PSC_MODE(psPrimitive);

    MUXDBG(ZONE_MUX_PROT1, TEXT("PSC Response, Result: %d"), ucResult);

    // send response
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteResponse_PSC(psFrame, ucResult);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_IndMSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_IndMSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8  ucSetDlci;
    UINT8  ucV24Sign;
    UINT8  ucEscSign;

    ucSetDlci = PRIM_READ_MSC_DLCI(psPrimitive);
    ucV24Sign = PRIM_READ_MSC_V24(psPrimitive);
    ucEscSign = PRIM_READ_MSC_ESC(psPrimitive);
    // dlci is transmitted is that way ...
    ucSetDlci = ucSetDlci >> 2;

    MUXDBG(ZONE_MUX_PROT2, TEXT("* Found remote flowcontrol dlci=0x%02x"), ucSetDlci);
    MUXDBG(ZONE_MUX_PROT2, TEXT("* local flow control on dlci=0x%02x is %d"), ucSetDlci, (pMux->pDLCIArray[ucSetDlci].sMP_DLCI.bFlowControl & LOCAL_FLOWCONTROL) ? 1 : 0);
    MUXDBG(ZONE_MUX_PROT2, TEXT("* remote flow control on dlci=0x%02x is %d"), psPrimitive->ucDLCI, (pMux->pDLCIArray[ucSetDlci].sMP_DLCI.bFlowControl & REMOTE_FLOWCONTROL) ? 1 : 0);

    if ((ucEscSign) && (ucSetDlci < pMux->dwMaxNumberOfDLCI))
    {
        MP_vAppSendEsc(pMux, ucSetDlci);
    }

    // decode flow control
    if (ucSetDlci < pMux->dwMaxNumberOfDLCI)
    {
        MP_vMSC_ReadMSC(pMux, ucSetDlci, ucV24Sign);
    }
    else
    {
        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Wrong dlci=%d"), ucSetDlci);
    }

    // send statemachine Response, all parameter have to send back for an msc response!
    psPrimitive->Event = MP_RespMSC;
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTED_ReqMSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ReqMSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        psFrame->ucDlci = DLCI0;
        MP_WriteRequest_MSC(psFrame, psPrimitive);

        // do not use state machine for speedup purposes; direct call to
        // the fifo mechanism
        if(pMux->MP_LinkState==eLINK_ACTIVE)
        {
            MP_vFifoPut(&pMux->MP_TxFifo, psFrame);
            MP_vUpdateTxQE(pMux);
        }
        else
        {
            // discard
            MP_vFifoPut(&pMux->MP_freelist, psFrame);
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTEDWAIT4RESPONSE_RespMSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_RespMSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        // send back the received MSC as in spec
        MP_WriteResponse_MSC(psFrame, psPrimitive);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTEDWAIT4RESPONSE_ConfMSC() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ConfMSC
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    // Not used
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vCONNECTEDWAIT4RESPONSE_EndeT2() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_EndeT2
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_CONNECTED, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vWAIT4CLOSEDOWN_IndCloseDown() ...
 * ---------------------------------------------------------------------------
 */
void MP_vWAIT4CLOSEDOWN_RespCloseDown
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8     i;
    MPFRAME  *psFrame;

    psFrame = MP_pstFifoGet(&pMux->MP_freelist);

    for (i = 0; i < pMux->dwMaxNumberOfDLCI; i++)
    {
       MP_vSetState(pMux, DLC_CLOSEDDOWN, i);
    }

    if (psFrame)
    {
        // send response
        MP_WriteResponse_CloseDown(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);

        // to prevent from switching too fast to non framed mode, trigger
        // queue system direct without state machine
        MP_vUpdateTxQE(pMux);
    }

    MUXDBG(ZONE_MUX_PROT1, TEXT("*** RespCloseDown: CloseDown Muxer on dlci=%d ***"), psPrimitive->ucDLCI);
    // reset mux, go to unframed mode
    Mux_Reset_cb(pMux);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vWAIT4CLOSEDOWN_ReqCloseDown() ...
 * ---------------------------------------------------------------------------
 */
void MP_vWAIT4CLOSEDOWN_ConfCloseDown
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8   i;

    for (i = 0; i < pMux->dwMaxNumberOfDLCI; i++)
    {
        MP_vSetState(pMux, DLC_CLOSEDDOWN, i);
    }

    MUXDBG(ZONE_MUX_PROT1, TEXT("*** ConfCloseDown: CloseDown Muxer on dlci=%d ***"), psPrimitive->ucDLCI);
    // reset mux, go to unframed mode; release all memory
    Mux_Reset_cb(pMux);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vWAIT4CLOSEDOWN_EndeT3() ...
 * ---------------------------------------------------------------------------
 */
void MP_vWAIT4CLOSEDOWN_EndeT3
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSetState(pMux, DLC_CLOSEDDOWN, psPrimitive->ucDLCI);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vAllEvents_IndCloseDown() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_IndCloseDown
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    if (psPrimitive->ucDLCI != 0)
    {
        // ERROR
    }

    MP_vSetState(pMux, DLC_WAIT4CLOSEDOWN, psPrimitive->ucDLCI);

    // CLOSEDDOWN -> DLC_WAIT4STARTUP -> DISCONNECTED
    // send Statemachine Response
    psPrimitive->Event = MP_RespCloseDown;
    MP_PostUserMessage(pMux, psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vAllEvents_ReqCloseDown() ...
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_ReqCloseDown
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MPFRAME *psFrame;

    MP_vSetState(pMux, DLC_WAIT4CLOSEDOWN, psPrimitive->ucDLCI);

    // send Request
    psFrame = MP_pstFifoGet(&pMux->MP_freelist);
    if (psFrame)
    {
        MP_WriteRequest_CloseDown(psFrame);
        MP_WriteRequest_SendFrame(pMux, psFrame, psPrimitive->ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   Sends an Indication MSC with ESC-Signal
 * ---------------------------------------------------------------------------
 */
void MP_vAllEvents_EscapeDetected
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    MP_vSendESC(pMux, psPrimitive->ucDLCI);
}

/* EOF */
