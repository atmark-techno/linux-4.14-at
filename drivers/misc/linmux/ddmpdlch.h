/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//***** Defines **************************************************************
#ifndef __DDMPDLCH_H_
#define __DDMPDLCH_H_


#define DLCI0_TYPE_PARAMETER_NEGOTIATION                0x81
#define DLCI0_TYPE_CLOSE_DOWN                           0xC1
#define DLCI0_TYPE_TEST                                 0x21
#define DLCI0_TYPE_MODEM_STATUS                         0xE1
#define DLCI0_TYPE_NON_SUPPORTED_COMMAND_RESPONSE       0x11
#define DLCI0_TYPE_POWER_SAVING                         0x41

#define V24_FlowControlOn                               0x01            // frames accepted
#define V24_FlowControlOff                              0x03            // frames not accepted
#define LOCAL_FLOWCONTROL                               0x01
#define REMOTE_FLOWCONTROL                              0x10

#define MSC_ESC_SIGNAL                                  0x01



/*-Enhancements for the TEST-Command version control-------------------------*/
#define TEMUX_Version             "TEMUXVERSION"  //costumer application string
#define MSMUX_Version             "MSMUXVERSION"  //module stringt
#define TEMUX_Version_IEI         0x04            //Information Element Identifier
#define MSMUX_Version_IEI         0x08            //Information Element Identifier


// *** MP MSC V.24 Signal Codings ***

// Mapping DTE
#define MP_HS_FC    0x02
#define MP_HS_DSR   0x04
#define MP_HS_CTS   0x08
#define MP_HS_RI    0x40
#define MP_HS_DCD   0x80

// Mapping DCE
#define MP_HS_DTR   0x04
#define MP_HS_RFR   0x08
#define MP_HS_BIT7  0x40
#define MP_HS_BIT8  0x80


/*---------------------------------------------------------------------------
  Frame
--------------------------------------------------------------------------- */

#define DLC_FRAME_FLAG_BASIC_MODE         0xF9

#define DLC_FRAME_CONTROL_PF_BIT_SET      0x10
#define DLC_FRAME_CONTROL_PF_BIT_RESET    0x00
#define SetTxFrameControlPFBit(x)         x->Control = x->Control | DLC_FRAME_CONTROL_PF_BIT_SET
#define ResetTxFrameControlPFBit(x)       x->Control = x->Control & 0xEF

#define DLC_FRAME_CONTROL_TYPE_SABM       0x2F
#define DLC_FRAME_CONTROL_TYPE_UA         0x63
#define DLC_FRAME_CONTROL_TYPE_DM         0x0F
#define DLC_FRAME_CONTROL_TYPE_DISC       0x43
#define DLC_FRAME_CONTROL_TYPE_UIH        0xEF

#define DLC_FRAME_ADDRESS_CR_SET          0x02
#define DLC_FRAME_ADDRESS_CR_RESET        0x00

#define DLC_FRAME_EA_BIT_SET              0x01
#define DLC_FRAME_EA_BIT_RESET            0x00

/*---------------------------------------------------------------------------
  @enum     MP_EVENTS | transitions of Multiplexer statemachine
--------------------------------------------------------------------------- */
typedef enum
{
   MP_IndStartUp,                   // 0
   MP_ReqStartUp,                   // 1

   MP_RespStartUpOkay,              // 2
   MP_RespStartUpError,             // 3
   MP_ConfStartUpOkay,              // 4
   MP_ConfStartUpError,             // 5

   MP_IndNegotiation,               // 6
   MP_ReqNegotiation,               // 7
   MP_RespNegotiation,              // 8
   MP_ConfNegotiation,              // 9

   MP_IndSABM_DLCI0,                // 10
   MP_ReqSABM_DLCI0,                // 11
   MP_IndSABM,                      // 12
   MP_ReqSABM,                      // 13

   MP_IndDISC,                      // 14
   MP_ReqDISC,                      // 15
   MP_RespUA_DISC,                  // 16
   MP_RespDM_DISC,                  // 17
   MP_ConfUA_DISC,                  // 18
   MP_ConfDM_DISC,                  // 19

   MP_RespDM_SABM,                  // 20
   MP_RespUA_SABM,                  // 21
   MP_ConfDM_SABM,                  // 22
   MP_ConfUA_SABM,                  // 23
   MP_EndeT1_SABM,                  // 24

   MP_IndUIH,                       // 25
   MP_ReqUIH,                       // 26

   MP_IndTest,                      // 27
   MP_ReqTest,                      // 28
   MP_RespTest,                     // 29
   MP_ConfTest,                     // 30

   MP_IndMSC,                       // 31
   MP_ReqMSC,                       // 32
   MP_RespMSC,                      // 33
   MP_ConfMSC,                      // 34

   MP_EndeT2,                       // 35
   MP_EndeT3,                       // 36

   MP_IndCloseDown,                 // 37
   MP_ReqCloseDown,                 // 38
   MP_RespCloseDown,                // 39
   MP_ConfCloseDown,                // 40

   MP_IndNSC,                       // 41
// MP_ReqNSC,
   MP_RespNSC,                      // 42
   MP_ConfNSC,                      // 43

   MP_IndPSC,                       // 44
   MP_ReqPSC,                       // 45
   MP_RespPSC,                      // 46
   MP_ConfPSC,                      // 47
   MP_ReqWake,                      // 48
   MP_RespWake,                     // 49

   MP_IndRXQueue,                   // 50
   MP_IndTXQueue,                   // 51
   MP_IndDTR_Release,               // 52
   MP_IndDTR_ReleaseEnd,            // 53
   MP_IndRxDataAvail,               // 54
   MP_IndMSC_PCMUX_FC,              // 55 PC-MUX will send FC-BIT inMSC
   MP_IndMSC_PCMUX_ML,              // 56 PC-MUX will send DTR/RTS in MSC

   MP_IndRXTimer,                   // 57
   MP_IndTXTimer,                   // 58
   MP_IndPN_TimeOut,                // 59
   MP_Ind_already_sent,             // 60
   MP_IndRxGipsyLoop,               // 61
   MP_IndMemTest,                   // 62
   MP_IndEscDetected,               // 63
   MP_MuxVersTransm,                // 64
   MP_ChnEscape,                    // 65
   MP_IndV24Status,                 // 66

   NUMBER_OF_TRANSITIONS
}MP_EVENTS;

#define MP_EVENT_TYPE MP_EVENTS


/*---------------------------------------------------------------------------
   @type    typedef for multiplexer transition functions
--------------------------------------------------------------------------- */
typedef void (*tMPTransitionFun)( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *pstPrimitive );


/*---------------------------------------------------------------------------
   @type    typedef for multiplexer Event Transition Table
--------------------------------------------------------------------------- */
typedef struct {
  UINT16            uiEvent ;
  tMPTransitionFun  sTransition ;
} aMPEventTransition ;

/*---------------------------------------------------------------------------
   @type    define for size of Event Transition Table
--------------------------------------------------------------------------- */
#define N_EVTR_ENTRIES(x)   (sizeof(x) / sizeof(x[0]))


/*
    escape sequences used for java:
    to transmit additional information about the current data structure in
    each channel, escape sequences are used. By parameter negotiation a special
    "Cinterion" convergance layer \see WM_SPEZIAL_CONVEGENCE_TYPE is used.
    The escape sequences work as described now:

    - To transmit JAVA_STATE_ESC as a char, JAVA_STATE_ESC are transmitted twice
        --> 0x0b will be transmitted as 0x0b 0x0b

    - To transmit the different infos about the data structure, JAVA_STATE_ESC is
      transmitted first, followed by the info-element sign
        --> URC_BEGIN bill be transmitted as 0x0b 0x22

*/
#define CONF_S1_ESC_SIGN       0x0B
#define CONF_S1_RESONSE_BEGIN  0x20
#define CONF_S1_RESONSE_END    0x21
#define CONF_S1_URC_BEGIN      0x22
#define CONF_S1_URC_END        0x23
#define CONF_S1_DATA_BEGIN     0x24
#define CONF_S1_DATA_END       0x25


/* internal use */
#define MP_RXLOOP_HIGHPRIO     0x80



/* Parameter negotiation params

    Table 3: Parameter Negotiation: 8 Bytes (see 07.10)


    Value/  Bit 1   Bit 2   Bit 3   Bit 4   Bit 5   Bit 6   Bit 7 Bit 8
    Octet
    1       D1      D2      D3      D4      D5      D6      0       0
    2       I1      I2      I3      I4      CL1     CL2     CL3     CL4
    3       P1      P2      P3      P4      P5      P6      0       0
    4       T1      T2      T3      T4      T5      T6      T7      T8
    5       N1      N2      N3      N4      N5      N6      N7      N8
    6       N9      N10     N11     N12     N13     N14     N15     N16
    7       NA1     NA2     NA3     NA4     NA5     NA6     NA7     NA8
    8       K1      K2      K3      0       0       0       0       0


The D-bits define the DLCI that the other information refers to;
Bit D1 is the least significant.


The I-bits define the type of frames used for carrying information in the
particular DLC - See Table 4.


    Table 4: Meaning of I-bits

    Meaning             I1      I2      I3      I4
    Use UIH frames      0       0       0       0
    Use UI frames       1       0       0       0
    Use I frames        0       1       0       0




The CL-bits define the type of convergence layer to be used on the particular DLCI - see Table 5.

    Table 5: Meaning of CL-bits

    Meaning             CL1     CL2     CL3     CL4
    Type 1              0       0       0       0
    Type 2              1       0       0       0
    Type 3              0       1       0       0
    Type 4              1       1       0       0

Other values are reserved. Default value is 0000

The P-bits define the priority to be assigned to the particular DLC. The range
of values is 0 to 63 with 0 being the lowest priority. P1 is the least significant bit. Default
value for P-bits are given by the DLCI values. See subclause 5.6.

The T-bits define the value of the acknowledgement timer (T1) - see subclause 5.7.1. The units
are hundredths of a second and T1 is the least significant bit.

The N-bits define the maximum frame size (N1) - see subclause 5.7.2. The parameter is a
sixteen-bit number with N1 as the least significant bit.

The NA-bits define the maximum number of retransmissions (N2) - see subclause 5.7.3. The parameter
is an eight-bit number with NA1 as the least significant bit.

The K-bits define the window size for error recovery mode (k) - see subclause 5.7.4. The parameter
is a three-bit number with K1 as the least significant bit.


To use some special JAVA functions to tag data, we create a Cinterion WM specific convergence layer:
* I-bits  set to: Use I-Frames (0x02)
* CL-bits set to: Type Cinterion (0x08)


the whole byte is now:
*/

#define WM_SPECIAL_CONVERGENCE_TYPE 0x82


/*
To use UIH frames with Error Recovery Mode Option (clause 6 in gsm 07.10), another Cinterion WM
specific type has to be defined. Spec says: error recovery mode uses I frames to carry the data;
however, the module has not enough performance to handle hdlc framing w. chksum. Therefore, we
use UIH frames instead.
*/
#define WM_SPECIAL_UIH_WITH_WND 0x85

/*
for debug purposes, a checksum over the full context of a frame is interesting.
*/
#define WM_SPECIAL_UIH_WITH_WND_FULL_CHKSUM 0x86

#define PRIM_WRITE_MSC_ESC(x, y)   { (x)->ulParam &= 0x00ffffff; (x)->ulParam |= ((UINT32)y << 24); }
#define PRIM_WRITE_MSC_V24(x, y)   { (x)->ulParam &= 0xff00ffff; (x)->ulParam |= ((UINT32)y << 16); }
#define PRIM_WRITE_MSC_DLCI(x, y)  { (x)->ulParam &= 0xffff00ff; (x)->ulParam |= ((UINT32)y <<  8); }
#define PRIM_WRITE_PSC_MODE(x, y)  { (x)->ulParam &= 0xffff00ff; (x)->ulParam |= ((UINT32)y <<  8); }

#define PRIM_READ_MSC_ESC(x)       ((UINT8)((x)->ulParam >> 24))
#define PRIM_READ_MSC_V24(x)       ((UINT8)((x)->ulParam >> 16))
#define PRIM_READ_MSC_DLCI(x)      ((UINT8)((x)->ulParam >>  8))
#define PRIM_READ_PSC_MODE(x)      ((UINT8)((x)->ulParam >>  8))


//***** Prototypes ***********************************************************


/*---------------------------------------------------------------------------
    process start function
--------------------------------------------------------------------------- */
void MP_vProc (MUX_INSTANCE_t *pMux);


/*---------------------------------------------------------------------------
  searches the event function in a given event table
--------------------------------------------------------------------------- */
extern void MP_vProcessEvent ( MUX_INSTANCE_t            *pMux,
                               const aMPEventTransition  *pEvTransTable,
                               UINT16                     uiEntries,
                               MP_PRIMITIVE              *pstPrimitive );


/*---------------------------------------------------------------------------
  event functions
--------------------------------------------------------------------------- */
void MP_vCLOSEDDOWN_IndStartUp( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vCLOSEDDOWN_RequStartUp( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vWAIT4STARTUP_RespStartUpOkay( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vWAIT4STARTUP_RespStartUpError( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vWAIT4STARTUP_ConfStartUpOkay( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vWAIT4STARTUP_ConfStartUpError( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vDISCONNECTED_IndSABM_DLCI0( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTED_ReqSABM_DLCI0( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTED_IndSABM( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTED_ReqSABM( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTED_IndDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTED_ReqDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vDISCONNECTEDNEGOTIATION_IndDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDNEGOTIATION_ReqDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_SABM( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_SABM( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_SABM( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_SABM( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_EndeT1( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_RespUA_DISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_RespDM_DISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfUA_DISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_ConfDM_DISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_IndDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vDISCONNECTEDWAIT4UAFRAME_ReqDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );


void MP_vAllEvents_IndNegotiation( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqNegotiation( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ConfNegotiation( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_RespNegotiation( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );


void MP_vAllEvents_IndDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqDISC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_IndUIH( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqUIH( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_IndTest( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqTest( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_IndMSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqMSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vAllEvents_IndPSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqPSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_RespPSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ConfPSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqWake( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_RespWake( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vAllEvents_RespTest( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ConfTest( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_RespMSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ConfMSC( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_EndeT2( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vWAIT4CLOSEDOWN_RespCloseDown( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vWAIT4CLOSEDOWN_ConfCloseDown( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vWAIT4CLOSEDOWN_EndeT3( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vAllEvents_IndCloseDown( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );
void MP_vAllEvents_ReqCloseDown( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );

void MP_vAllEvents_EscapeDetected( MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive );



void MP_vResetMPDLCStruct(UINT8 DLCI);
void MP_vSetState( MUX_INSTANCE_t *pMux, MP_eState eNewState, UINT8 ucDLCI);
MP_eState MP_eGetState( MUX_INSTANCE_t *pMux, UINT8 ucDLCI);

BOOL MP_bDlciConnected(MUX_INSTANCE_t *pMux, UINT8 ucDlci);


void MP_Write_Negotiation( MUX_INSTANCE_t *pMux, MPFRAME *psFrame, UINT8 ucDlci, BOOL Resp );
void MP_WriteRequest_SendFrame(MUX_INSTANCE_t *pMux, MPFRAME *pFrame, UINT8 ucDLCI );
void MP_WriteRequest_V24Ind( MUX_INSTANCE_t *pMux, UINT8 ucDlci );
void MP_WriteRequest_SABM_DLCI0( MPFRAME *psFrame );
void MP_WriteRequest_SABM( MPFRAME *psFrame );
void MP_WriteRequest_DISC( MPFRAME *psFrame );
void MP_WriteRequest_UIH( MPFRAME *psFrame );
void MP_WriteRequest_Test( MUX_INSTANCE_t *pMux, MPFRAME *psFrame, UINT8 ucDLCI );
void MP_WriteRequest_MSC( MPFRAME *psFrame, MP_PRIMITIVE *psPrimitive );
void MP_WriteRequest_CloseDown( MPFRAME *psFrame );
void MP_WriteRequest_PSC( MPFRAME *psFrame, UINT8 ucMode );
void MP_WriteResponse_DM_SABM( MPFRAME *psFrame );
void MP_WriteResponse_UA_SABM( MPFRAME *psFrame );
void MP_WriteResponse_DM_DISC( MPFRAME *psFrame );
void MP_WriteResponse_UA_DISC( MPFRAME *psFrame );
void MP_WriteResponse_MSC( MPFRAME *psFrame, MP_PRIMITIVE *psPrimitive );
void MP_WriteResponse_CloseDown( MPFRAME *psFrame );
void MP_WriteResponse_RespNSC( MPFRAME *psFrame );
void MP_WriteResponse_PSC( MPFRAME *psFrame, UINT8 ucResult );
void MP_WriteResponse_Test( MPFRAME *psFrame, MPFRAME *psmuxFrame );
void MP_VersCompare( MUX_INSTANCE_t *pMux, MPFRAME *psmuxFrame );
void MP_PnCompare( MUX_INSTANCE_t *pMux, MPFRAME *psFrame, UINT8 ucDlci );
void MP_AdjustQueueSize( MUX_INSTANCE_t *pMux );


//***** global Functions ******************************************************
void MP_vSetFlowControl( MUX_INSTANCE_t *pMux, UINT8 ucDLCI);
void MP_vUpdateRxQE( MUX_INSTANCE_t *pMux, UINT8 ucDLCI);
void MP_vUpdateTxQE( MUX_INSTANCE_t *pMux );
void MP_vSetTXLoop( MUX_INSTANCE_t *pMux );
void MP_vSetRXLoop( MUX_INSTANCE_t *pMux, UINT8 ucDLCI);
UINT32 MP_uiToTxBufTransparent( MUX_INSTANCE_t *pMux, UINT8 *Data, UINT32 len, UINT8 ucDlci);


void MP_vMSC_FlowControlOff( MUX_INSTANCE_t *pMux, UINT8 ucDLCI);
void MP_vMSC_FlowControlOn( MUX_INSTANCE_t *pMux, UINT8 ucDLCI);
void MP_vMSC_SendMSC( MUX_INSTANCE_t *pMux, UINT8 ucDlci, UINT8 Escape);
void MP_vMSC_ReadMSC( MUX_INSTANCE_t *pMux, UINT8 ucDlci, UINT8 ucV24Signals);
void MP_vSendMSCInitialValues( MUX_INSTANCE_t *pMux, UINT8 ucDlci);


// ddmpmem.c
void MP_vMemInit( MUX_INSTANCE_t *pMux );
void MP_vFifoInit( MP_pTFifo fFifo );
void MP_vFifoPut( MP_pTFifo fFifo, pMPFRAME fQE );
pMPFRAME MP_pstFifoGet( MP_pTFifo fFifo );
pMPFRAME MP_pstFifoPeek( MP_pTFifo fFifo );


// ddmpfra.c
UINT8 MP_ucRxFCS( MUX_INSTANCE_t *pMux, UINT8 *pFCS, UINT32 FCSlen, UINT8 FCSCheckByte );
UINT8 ucMP_TxFCS( MUX_INSTANCE_t *pMux, UINT8 *p, UINT32 len);
void MP_vInitScanner( MUX_INSTANCE_t *pMux );
void MP_ResetResync( MUX_INSTANCE_t *pMux );


void MP_vStatemachineInit(  MUX_INSTANCE_t *pMux );

void MP_vAppCallback( MUX_INSTANCE_t *pMux, UINT8 ucDlci, UINT32 len);

void MP_vRespWake ( MUX_INSTANCE_t *pMux );

void MP_PostSimpleUserMessage( MUX_INSTANCE_t *pMux, MP_EVENT_TYPE evnt, UINT8 dlci);

void MP_CheckResyncTimeOut( MUX_INSTANCE_t  *pMux );

void MP_vUpdateTxQE_hdlc ( MUX_INSTANCE_t *pMux );



// ddmppkt.c
void MP_vInitPacketHandler ( MUX_INSTANCE_t *pMux, UINT8 ucDlci );
void MP_uiTxFreeQueue ( MUX_INSTANCE_t *pMux, UINT8 ucDlci );
BOOL MP_bHandleTxPackets ( MUX_INSTANCE_t  *pMux, UINT8 ucDlci );
BOOL MP_bTxDataAvail ( MUX_INSTANCE_t *pMux, UINT8 ucDlci );


#endif //__DDMPDLCH_H_
