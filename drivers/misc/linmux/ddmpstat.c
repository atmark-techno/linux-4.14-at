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

// ERROR CODES
#define  MP_STAT_INVALID_VERSION    0


//***** Defines **************************************************************

static void DLC_statefun_CLOSEDDOWN(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);
static void DLC_statefun_WAIT4STARTUP(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);
static void DLC_statefun_DISCONNECTED(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);
static void DLC_statefun_DISCONNECTEDNEGOTIATION(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);
static void DLC_statefun_DISCONNECTEDWAIT4UAFRAME(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);
static void DLC_statefun_CONNECTED(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);
static void DLC_statefun_CONNECTEDWAIT4RESPONSE(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);
static void DLC_statefun_WAIT4CLOSEDOWN(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);


// ***** global vars **********************************************************

// Table of States
static void ( *const MP_state_tab[] ) (MUX_INSTANCE_t *pMux, MP_PRIMITIVE* ) =
                                        { DLC_statefun_CLOSEDDOWN
                                        , DLC_statefun_WAIT4STARTUP
                                        , DLC_statefun_DISCONNECTED
                                        , DLC_statefun_DISCONNECTEDNEGOTIATION
                                        , DLC_statefun_DISCONNECTEDWAIT4UAFRAME
                                        , DLC_statefun_CONNECTED
                                        , DLC_statefun_CONNECTEDWAIT4RESPONSE
                                        , DLC_statefun_WAIT4CLOSEDOWN };

// Tables of Transitions
static const aMPEventTransition DLC_EventTransT_CLOSEDDOWN[] =
   { { MP_IndStartUp, MP_vCLOSEDDOWN_IndStartUp}
   , { MP_ReqStartUp, MP_vCLOSEDDOWN_RequStartUp}
   };

static const aMPEventTransition DLC_EventTransT_WAIT4STARTUP[] =
   { { MP_RespStartUpOkay, MP_vWAIT4STARTUP_RespStartUpOkay}
   , { MP_RespStartUpError, MP_vWAIT4STARTUP_RespStartUpError}
   , { MP_ConfStartUpOkay, MP_vWAIT4STARTUP_ConfStartUpOkay}
   , { MP_ConfStartUpError, MP_vWAIT4STARTUP_ConfStartUpError}
   };

static const aMPEventTransition DLC_EventTransT_DISCONNECTED[] =
   { { MP_IndSABM_DLCI0, MP_vDISCONNECTED_IndSABM_DLCI0}
   , { MP_ReqSABM_DLCI0, MP_vDISCONNECTED_ReqSABM_DLCI0}
   , { MP_IndSABM, MP_vDISCONNECTED_IndSABM}
   , { MP_ReqSABM, MP_vDISCONNECTED_ReqSABM}
   , { MP_IndDISC, MP_vDISCONNECTED_IndDISC}
   , { MP_ReqDISC, MP_vDISCONNECTED_ReqDISC}
   , { MP_IndCloseDown, MP_vAllEvents_IndCloseDown}
   , { MP_ReqCloseDown, MP_vAllEvents_ReqCloseDown}
   };

static const aMPEventTransition DLC_EventTransT_DISCONNECTEDNEGOTIATION[] =
   { { MP_IndDISC, MP_vDISCONNECTEDNEGOTIATION_IndDISC}
   , { MP_ReqDISC, MP_vDISCONNECTEDNEGOTIATION_ReqDISC}
   , { MP_IndCloseDown, MP_vAllEvents_IndCloseDown}
   , { MP_ReqCloseDown, MP_vAllEvents_ReqCloseDown}
   };

static const aMPEventTransition DLC_EventTransT_DISCONNECTEDWAIT4UAFRAME[] =
   { { MP_RespDM_SABM, MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_SABM}
   , { MP_RespUA_SABM, MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_SABM}
   , { MP_ConfDM_SABM, MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_SABM}
   , { MP_ConfUA_SABM, MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_SABM}
   , { MP_EndeT1_SABM, MP_vDISCONNECTEDWAIT4UAFRAME_EndeT1}
   , { MP_RespUA_DISC, MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_DISC}
   , { MP_RespDM_DISC, MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_DISC}
   , { MP_ConfUA_DISC, MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_DISC}
   , { MP_ConfDM_DISC, MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_DISC}
   , { MP_IndDISC, MP_vDISCONNECTEDWAIT4UAFRAME_IndDISC}
   , { MP_ReqDISC, MP_vDISCONNECTEDWAIT4UAFRAME_ReqDISC}
   , { MP_IndCloseDown, MP_vAllEvents_IndCloseDown}
   , { MP_ReqCloseDown, MP_vAllEvents_ReqCloseDown}
   , { MP_ChnEscape, MP_vAllEvents_EscapeDetected}
   };

static const aMPEventTransition DLC_EventTransT_CONNECTED[] =
   {
     { MP_IndMSC, MP_vAllEvents_IndMSC}
   , { MP_ReqMSC, MP_vAllEvents_ReqMSC}
   , { MP_RespMSC, MP_vAllEvents_RespMSC}
   , { MP_ConfMSC, MP_vAllEvents_ConfMSC}

   , { MP_IndPSC, MP_vAllEvents_IndPSC}
   , { MP_ReqPSC, MP_vAllEvents_ReqPSC}
   , { MP_RespPSC, MP_vAllEvents_RespPSC}
   , { MP_ConfPSC, MP_vAllEvents_ConfPSC}
   , { MP_ReqWake, MP_vAllEvents_ReqWake}
   , { MP_RespWake, MP_vAllEvents_RespWake}

   , { MP_IndTest, MP_vAllEvents_IndTest}
   , { MP_ReqTest, MP_vAllEvents_ReqTest}
   , { MP_RespTest, MP_vAllEvents_RespTest}
   , { MP_ConfTest, MP_vAllEvents_ConfTest}

   , { MP_IndCloseDown, MP_vAllEvents_IndCloseDown}
   , { MP_ReqCloseDown, MP_vAllEvents_ReqCloseDown}

   , { MP_ChnEscape, MP_vAllEvents_EscapeDetected}

   , { MP_IndDISC, MP_vAllEvents_IndDISC}
   , { MP_ReqDISC, MP_vAllEvents_ReqDISC}

   , { MP_IndUIH, MP_vAllEvents_IndUIH}
   , { MP_ReqUIH, MP_vAllEvents_ReqUIH}

   , { MP_IndNegotiation, MP_vAllEvents_IndNegotiation}
   , { MP_ReqNegotiation, MP_vAllEvents_ReqNegotiation}

   , { MP_RespNegotiation, MP_vAllEvents_RespNegotiation}
   , { MP_ConfNegotiation, MP_vAllEvents_ConfNegotiation}

   , { MP_IndCloseDown, MP_vAllEvents_IndCloseDown}
   , { MP_ReqCloseDown, MP_vAllEvents_ReqCloseDown}

   , { MP_EndeT2, MP_vAllEvents_EndeT2}
   };



static const aMPEventTransition DLC_EventTransT_CONNECTEDWAIT4RESPONSE[] =
   {
     { MP_IndMSC, MP_vAllEvents_IndMSC}
   , { MP_ReqMSC, MP_vAllEvents_ReqMSC}
   , { MP_RespMSC, MP_vAllEvents_RespMSC}
   , { MP_ConfMSC, MP_vAllEvents_ConfMSC}

   , { MP_IndTest, MP_vAllEvents_IndTest}
   , { MP_ReqTest, MP_vAllEvents_ReqTest}
   , { MP_RespTest, MP_vAllEvents_RespTest}
   , { MP_ConfTest, MP_vAllEvents_ConfTest}

   , { MP_IndCloseDown, MP_vAllEvents_IndCloseDown}
   , { MP_ReqCloseDown, MP_vAllEvents_ReqCloseDown}

   , { MP_ChnEscape, MP_vAllEvents_EscapeDetected}

   , { MP_IndDISC, MP_vAllEvents_IndDISC}
   , { MP_ReqDISC, MP_vAllEvents_ReqDISC}

   , { MP_IndUIH, MP_vAllEvents_IndUIH}
   , { MP_ReqUIH, MP_vAllEvents_ReqUIH}

   , { MP_IndCloseDown, MP_vAllEvents_IndCloseDown}
   , { MP_ReqCloseDown, MP_vAllEvents_ReqCloseDown}

   , { MP_EndeT2, MP_vAllEvents_EndeT2}
   };



static const aMPEventTransition DLC_EventTransT_WAIT4CLOSEDOWN[] =
   { { MP_RespCloseDown, MP_vWAIT4CLOSEDOWN_RespCloseDown}
   , { MP_ConfCloseDown, MP_vWAIT4CLOSEDOWN_ConfCloseDown}
   , { MP_EndeT3, MP_vWAIT4CLOSEDOWN_EndeT3}
   };



//***** Prototypes ***********************************************************
static void MP_vTickStatemachine (MUX_INSTANCE_t *pMux, MP_PRIMITIVE *sPrimitive);



/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   global
 * input:   void
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vStatemachineInit () ...
 * ---------------------------------------------------------------------------
 */
void MP_vStatemachineInit
(
    MUX_INSTANCE_t  *pMux
)
{
    UINT8 i;

    MPVAR *pPtr;

    pMux->DLCI0_Established = 0;

    for (i=0; i < pMux->dwMaxNumberOfDLCI; i++)
    {
        MEMCLR(&pMux->pDLCIArray[i].sMP_DLCI, sizeof(MPVAR));
        MEMCLR(&pMux->pDLCIArray[i].sMP_Info, sizeof(MP_INFOBLOCK));
    }

    MEMCLR(&pMux->sMP_ParserInfo, sizeof(MP_PARSERINFO));

    pMux->MP_uiUsedProtVersion = MP_PROTOCOL_VERSION_MIN;
    pMux->MP_uiInternalVersion = MP_PROTOCOL_VERSION_MAX;

    pMux->MP_IndRXQueueLoopActive = 0;
    pMux->MP_IndTXQueueLoopActive = FALSE;
    pMux->MP_V24IndLoopActive     = 0;
    pMux->ucInitiator             = 0;

    for (i=0; i < pMux->dwMaxNumberOfDLCI; i++)
    {
        pPtr = &pMux->pDLCIArray[i].sMP_DLCI;
        pPtr->State               = DLC_CLOSEDDOWN;
        pPtr->sV24Status.DTR      = 1;
        pPtr->sV24Status.RTS      = 1;
        pPtr->sV24Status.CTS      = 1;
        pPtr->sV24Status.DSR      = 1;
        pPtr->sV24Status.TxEmpty  = 1;
        pPtr->sV24Status.TxFull   = 0;

        pPtr->uiFrameSize         = DEF_MAXFRAMESIZE;

        Hdlc_Init(pMux, i, (UINT8)pMux->dwMpWindowSize);
        MP_vInitPacketHandler(pMux, i);
    }

    pMux->MP_LinkState = eLINK_ACTIVE;
}


/*****************************************************************************/
/*!\brief Mux High Prio MainProc
*
* This function is called from the framework when the multiplex main task will
* run a received message. The message is posted with MP_PostUserMessage(). The
* MainProc will receive the message with MP_ucGetMessage(). The format in the
* post- and the receive-function is always the same structure.
*
* \see MP_vProcLoop()
* \eee MP_PostUserMessage()
* \see MP_ucGetMessage()
******************************************************************************/
void MP_vProc
(
    MUX_INSTANCE_t  *pMux
)
{
    MP_PRIMITIVE sPrimitive;
    if (MP_ucGetMessage(pMux, &sPrimitive,  sizeof(sPrimitive)))
    {
        MP_vTickStatemachine(pMux, &sPrimitive );
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   global
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vTickStatemachine () ...
 * ---------------------------------------------------------------------------
 */
static void MP_vTickStatemachine
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
    UINT8       ucDLCI;
    MP_eState   State_old;

    ucDLCI = psPrimitive->ucDLCI;


    switch (psPrimitive->Event)
    {

      case MP_IndTXQueue:
        pMux->MP_IndTXQueueLoopActive = FALSE;
        MP_vUpdateTxQE(pMux);
        return;

      case MP_IndRXQueue:
        pMux->MP_IndRXQueueLoopActive &= ~(((UINT16)1) << ucDLCI);
        MP_vUpdateRxQE(pMux, ucDLCI);
        return;

      case MP_IndV24Status:
        pMux->MP_V24IndLoopActive &= ~(((UINT16)1) << ucDLCI);
        MP_vMSC_SendMSC(pMux, ucDLCI, 0);
        return;

      case MP_IndMemTest:
        return;

      case MP_IndPN_TimeOut:
        // PN msg is optional, so a timeout is necessary to continue
        for (ucDLCI = 1; ucDLCI < pMux->dwMaxNumberOfDLCI; ucDLCI++)
        {
            if (pMux->pDLCIArray[ucDLCI].sMP_DLCI.ucFrameType == 0xfe)
            {
                pMux->pDLCIArray[ucDLCI].sMP_DLCI.ucFrameType = 0x00;
                psPrimitive->Event = MP_ReqSABM;
                psPrimitive->ucDLCI  = ucDLCI;
                MP_PostUserMessage(pMux, psPrimitive);
                MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("*** PN Request TIMEOUT dlci=%02x, will continue with ReqSABM ***"), ucDLCI);
            }
        }
        return;

      default:  // all other messages will processed in the normal state machine below
        break;
    }

   // change state while valid
   if ( pMux->pDLCIArray[ucDLCI].sMP_DLCI.State < NUMBER_OF_MP_STATES )
   {
      State_old = pMux->pDLCIArray[ucDLCI].sMP_DLCI.State;  // save old state
      (*MP_state_tab[State_old])(pMux, psPrimitive);       // call State function
   }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   global
 * input:   const aMPEventTransition *pEvTransTable
 *          pointer to the actual event table
 * input:   UINT16 uiEntries
 *          Number of entries in the actual event table
 * input:   MP_PRIMITIVE *pstPrimitive
 *          pointer to the sent message
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vProcessEvent () ...
 *          searches the event function in a given event table
 * ---------------------------------------------------------------------------
 */
void MP_vProcessEvent
(
    MUX_INSTANCE_t            *pMux,
    const aMPEventTransition  *pEvTransTable,
    UINT16                     uiEntries,
    MP_PRIMITIVE              *pstPrimitive
)
{
   UINT32   ucI;

   // scan through all events in the event table (except DEFAULT_EVENT)
   for (ucI = 0 ; ucI < (uiEntries) ; ucI++)
   {
      // compare current table event with sent event
      if (pEvTransTable[ucI].uiEvent == pstPrimitive->Event)
      {
         // event matches, call event function
         (*pEvTransTable[ucI].sTransition)(pMux, pstPrimitive);
         return;
      }
   }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_CLOSEDDOWN() ...
 *          statefunction for CLOSEDDOWN state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_CLOSEDDOWN
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_CLOSEDDOWN,
                      N_EVTR_ENTRIES(DLC_EventTransT_CLOSEDDOWN),
                      psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_WAIT4STARTUP() ...
 *          statefunction for WAIT4STARTUP state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_WAIT4STARTUP
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_WAIT4STARTUP,
                      N_EVTR_ENTRIES(DLC_EventTransT_WAIT4STARTUP),
                      psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_DISCONNECTED() ...
 *          statefunction for DISCONNECTED state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_DISCONNECTED
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_DISCONNECTED,
                      N_EVTR_ENTRIES(DLC_EventTransT_DISCONNECTED),
                      psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_DISCONNECTEDNEGOTIATION() ...
 *          statefunction for DISCONNECTEDNEGOTIATION state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_DISCONNECTEDNEGOTIATION
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_DISCONNECTEDNEGOTIATION,
                      N_EVTR_ENTRIES(DLC_EventTransT_DISCONNECTEDNEGOTIATION),
                      psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_DISCONNECTEDWAIT4UAFRAME() ...
 *          statefunction for DISCONNECTEDWAIT4UAFRAME state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_DISCONNECTEDWAIT4UAFRAME
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_DISCONNECTEDWAIT4UAFRAME,
                      N_EVTR_ENTRIES(DLC_EventTransT_DISCONNECTEDWAIT4UAFRAME),
                      psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_CONNECTED() ...
 *          statefunction for CONNECTED state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_CONNECTED
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_CONNECTED,
                      N_EVTR_ENTRIES(DLC_EventTransT_CONNECTED),
                      psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_CONNECTEDWAIT4RESPONSE() ...
 *          statefunction for CONNECTEDWAIT4RESPONSE state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_CONNECTEDWAIT4RESPONSE
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_CONNECTEDWAIT4RESPONSE,
                      N_EVTR_ENTRIES(DLC_EventTransT_CONNECTEDWAIT4RESPONSE),
                      psPrimitive);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:   local
 * input:   MP_PRIMITIVE *psPrimitive
 * returns: static void
 * ---------------------------------------------------------------------------
 * descr:   The function DLC_statefun_WAIT4CLOSEDOWN() ...
 *          statefunction for WAIT4CLOSEDOWN state
 *          all events of the statetable will be processed,
 *          all other events will bi discarded
 *          @parm   pointer to the received MP_PRIMITIVE
 * ---------------------------------------------------------------------------
 */
static void DLC_statefun_WAIT4CLOSEDOWN
(
    MUX_INSTANCE_t  *pMux,
    MP_PRIMITIVE    *psPrimitive
)
{
   MP_vProcessEvent ( pMux,
                      DLC_EventTransT_WAIT4CLOSEDOWN,
                      N_EVTR_ENTRIES(DLC_EventTransT_WAIT4CLOSEDOWN),
                      psPrimitive);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   creates an MSC message
 *          send all V24signals per selected version
 * ---------------------------------------------------------------------------
 */
void MP_vMSC_SendMSC
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci,
    UINT8            Escape
)
{
    MP_PRIMITIVE    sPrimitive;
    UINT8           v24sign = 0;

    if (pMux->ucInitiator)
    {
        if (pMux->pDLCIArray[ucDlci].sMP_DLCI.bFlowControl & LOCAL_FLOWCONTROL)
        {
            v24sign = V24_FlowControlOff;
        }
        else
        {
            v24sign = V24_FlowControlOn;
        }
        // costumer application will send always a valid DTR and RTS line
        // !! Attention !! depending from the used mux version this may
        // result in unattended actions from the module, see MP_vMSC_ReadMSC()
        // what will happen !!
        if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DTR)
            v24sign |= MP_HS_DTR;
        if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RTS)
            v24sign |= MP_HS_RFR;
    }
    else
    {
        // module will send status lines depending from the mux version. Only in
        // mux V3 a full set of v24 status lines will send, a static 1 else
        switch (pMux->MP_uiUsedProtVersion)
        {
          case MP_REVISION_03:
            if (pMux->pDLCIArray[ucDlci].sMP_DLCI.bFlowControl & LOCAL_FLOWCONTROL)
                v24sign |= V24_FlowControlOff;
            // FALLTHROUGH IS WHAT WE WANT
            // Falls through
          default:
            if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DSR)
                v24sign |= MP_HS_DSR;
            if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.CTS)
                v24sign |= MP_HS_CTS;
            if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RI)
                v24sign |= MP_HS_RI;
            if (pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DCD)
                v24sign |= MP_HS_DCD;
            break;

          case MP_REVISION_02:
          case MP_REVISION_01:
            if (pMux->pDLCIArray[ucDlci].sMP_DLCI.bFlowControl & LOCAL_FLOWCONTROL)
                v24sign = V24_FlowControlOff;
            v24sign |= (MP_HS_DSR | MP_HS_CTS | MP_HS_DCD);
            break;
        }
    }

    if (Escape)
    {
        Escape = 0x01;
    }
    if (pMux->ucInitiator)
    {
        MUXDBG(ZONE_MUX_PROT2, TEXT("SendMSC dlci=%d, DTR=%d RTS=%d flctrl=%d esc=%d"),
                               ucDlci,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DTR,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RTS,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.bFlowControl,
                               Escape);
    }
    else
    {
        MUXDBG(ZONE_MUX_PROT2, TEXT("MuxDrv: SendMSC dlci=%d, DSR=%d CTS=%d DCD=%d RI=%d flctrl=%d esc=%d"),
                               ucDlci,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DSR,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.CTS,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DCD,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RI,
                               pMux->pDLCIArray[ucDlci].sMP_DLCI.bFlowControl,
                               Escape);
    }

    // MSC dlci is transmitted in that way ...
    ucDlci = ((ucDlci << 2) | 0x03);

    sPrimitive.Event    = MP_ReqMSC;
    sPrimitive.ucDLCI   = DLCI0;
    sPrimitive.ulParam  = 0;
    PRIM_WRITE_MSC_DLCI(&sPrimitive, ucDlci);
    PRIM_WRITE_MSC_V24(&sPrimitive, v24sign);
    PRIM_WRITE_MSC_ESC(&sPrimitive, Escape);

    MP_vAllEvents_ReqMSC(pMux, &sPrimitive); // do not use state machine; call direct (performance!)
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   reads an MSC message
 *          send all V24signals per selected version
 * ---------------------------------------------------------------------------
 */
void MP_vMSC_ReadMSC
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci,
    UINT8            ucV24Signals
)
{
    V24STATUS OldV24Status;

    switch (pMux->MP_uiUsedProtVersion)
    {
        case MP_REVISION_04:
            // in V4, do nothing
            MUXDBG(ZONE_MUX_PROT2, TEXT("MP_vMSC_ReadMSC: no flow control handling in v4"));
            break;
        default:
            if ((ucV24Signals & V24_FlowControlOff) == V24_FlowControlOff)
            {
                pMux->pDLCIArray[ucDlci].sMP_DLCI.bFlowControl |= REMOTE_FLOWCONTROL;
                MUXDBG(ZONE_MUX_PROT2, TEXT("RFC=ON dlci=%d v24=%d"), ucDlci, ucV24Signals);
                pMux->pDLCIArray[ucDlci].sMP_Info.RFCActive++;
            }
            else
            {
                pMux->pDLCIArray[ucDlci].sMP_DLCI.bFlowControl &= ~(REMOTE_FLOWCONTROL);
                MUXDBG(ZONE_MUX_PROT2, TEXT("RFC=OFF dlci=%d v24=%d"), ucDlci, ucV24Signals);
                // if flow control is switched off, the selected instance is able to send
                // data again --> trigger on
                MP_PostSimpleUserMessage(pMux, MP_IndTXQueue, ucDlci);
            }
            break;
    }

    OldV24Status = pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status;
    if (pMux->ucInitiator)
    {
        // costumer aplication: will always read input from the
        // module and interpret the lines as dsr/cts/ring/dcd. Pleas
        // note that these bits are valid in mux veriosn 3 only !!
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DSR = (ucV24Signals & MP_HS_DSR)?1:0;
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.CTS = (ucV24Signals & MP_HS_CTS)?1:0;
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RI  = (ucV24Signals & MP_HS_RI)?1:0;
        pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DCD = (ucV24Signals & MP_HS_DCD)?1:0;
    }
    else
    {
        // module: is always slave, will always read input from the
        // costumer app and interpret the lines as dtr/rts or depending from
        // the uses mux version
        switch (pMux->MP_uiUsedProtVersion)
        {
            // v24 status lines are transmitted via MSC. We decode the lines
            // and save them into the SMP_DLCI-structure on each channel. Not
            // all channels may use v24 status lines
            case MP_REVISION_03:
            case MP_REVISION_02:
            default:
               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DTR = (ucV24Signals & MP_HS_DTR)?1:0;
               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RTS = (ucV24Signals & MP_HS_RFR)?1:0;
               if (MEMCMP(&OldV24Status, &pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status, sizeof(V24STATUS)))
               MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
           break;

            // This is the default procedure from TC35; the old one
            case MP_REVISION_01:
               // to hangup a connection the following method is available
               //  when a DTR==0 in the transmitted MSC frame is detected, we create a
               //  1/0/1 signal with DTR in gipsy. For that we have to make shure that
               //  gipsy can read this 1/0/1 signal, we first send the 1 signal to gipsy
               //  from here, and our second process MP_PROTOKOLL_LOOP sends the 0 on DTR
               //  and some times later the last 1 on DTR
               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DTR = 1;
               pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RTS = 1;

               // special Mux-V1 handling: send a 1/0/1 sequence to application
               if ((ucV24Signals & MP_HS_DTR) == 0)
               {
                   MUXDBG(ZONE_MUX_PROT2, (TEXT("MuxDrv: v1 special handling P1: DTR to APP1=ON")));
                   MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
                   if (ucDlci == DLCI1) /* only for dlci1 --> gipsy */
                   {
                       pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DTR = 0;
                       pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RTS = 0;
                       MUXDBG(ZONE_MUX_PROT2, (TEXT("MuxDrv: v1 special handling P2: DTR to APP1=OFF")));
                       MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
                       pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.DTR = 1;
                       pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status.RTS = 1;
                       MUXDBG(ZONE_MUX_PROT2, (TEXT("MuxDrv: v1 special handling P3: DTR to APP1=ON")));
                       MP_vAppSendV24Status(pMux, ucDlci, pMux->pDLCIArray[ucDlci].sMP_DLCI.sV24Status);
                   }
                   else
                   {
                       MUXDBG(ZONE_MUX_PROT2, (TEXT("MuxDrv: Special DTR handling mux v1 only on data channel")));
                   }
                }
                break;
        }
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   creates an MSC message with Local Flow control disabled
 *
 * ---------------------------------------------------------------------------
 */
void MP_vMSC_FlowControlOff
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    pMux->pDLCIArray[ucDLCI].sMP_DLCI.bFlowControl |= LOCAL_FLOWCONTROL;
    MP_vMSC_SendMSC(pMux, ucDLCI, 0);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   creates an MSC message with Local Flow control enabled
 *
 * ---------------------------------------------------------------------------
 */
void MP_vMSC_FlowControlOn
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    pMux->pDLCIArray[ucDLCI].sMP_DLCI.bFlowControl &= ~(LOCAL_FLOWCONTROL);
    MP_vMSC_SendMSC(pMux, ucDLCI, 0);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   creates an MSC message with ESC enabled
 *
 * ---------------------------------------------------------------------------
 */
void MP_vSendESC
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDlci
)
{
    MUXDBG(ZONE_MUX_PROT1, TEXT("Send ESC on dlci=%d"), ucDlci);
    MP_vMSC_SendMSC(pMux, ucDlci, 1);
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   LOOP Management: Send Msg to Loop Process, when not pending
 *
 * high prio set: send message to own process
 * low prio set : Send message to loopback process. The loopback process
 *                has a very low priority; all other messages in the system
 *                has a chance now to run before the multiplexer. The mp
 *                decreases its priority for this messags when using that way
 * ---------------------------------------------------------------------------
 */
void MP_vSetRXLoop
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    ucDLCI &= 0x7f;

    // lock bit to have only one message pending
    if ((pMux->MP_IndRXQueueLoopActive & (((UINT16)1) << ucDLCI)) == 0)
    {
        pMux->MP_IndRXQueueLoopActive |= ((UINT16)1) << ucDLCI;
        MP_PostSimpleUserMessage(pMux, MP_IndRXQueue, ucDLCI);
    }
}

/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   LOOP Management: Send Msg to Loop Process, when not pending
 *
 * ---------------------------------------------------------------------------
 */
void MP_vSetTXLoop
(
    MUX_INSTANCE_t  *pMux
)
{
    if (pMux->MP_IndTXQueueLoopActive == FALSE)
    {
        pMux->MP_IndTXQueueLoopActive = TRUE;
        MP_PostSimpleUserMessage(pMux, MP_IndTXQueue, DLCI0);
    }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   MP_eState eNewState
 * input:   UINT8 ucDLCI
 * returns: void
 * ---------------------------------------------------------------------------
 * descr:   The function MP_vSetState() ...
 *          Sets the state of a DLCI
 * ---------------------------------------------------------------------------
 */
void MP_vSetState
(
    MUX_INSTANCE_t  *pMux,
    MP_eState        eNewState,
    UINT8            ucDLCI
)
{
   pMux->pDLCIArray[ucDLCI].sMP_DLCI.State= eNewState;
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:   UINT8 ucDLCI
 * returns: MP_eState State of DLCI
 * ---------------------------------------------------------------------------
 * descr:   The function MP_eGetState() ...
 *          returns the state of a DLCI
 * ---------------------------------------------------------------------------
 */
MP_eState MP_eGetState
(
    MUX_INSTANCE_t  *pMux,
    UINT8            ucDLCI
)
{
    return (pMux->pDLCIArray[ucDLCI].sMP_DLCI.State);
}


BOOL MP_bDlciConnected(MUX_INSTANCE_t *pMux, UINT8 ucDlci)
{
  if (
       (pMux->pDLCIArray[ucDlci].sMP_DLCI.State == DLC_CONNECTED)
       ||
       (pMux->pDLCIArray[ucDlci].sMP_DLCI.State == DLC_CONNECTEDWAIT4RESPONSE)
     )
  {
    return TRUE;
  }
  return FALSE;
}
/* EOF */
