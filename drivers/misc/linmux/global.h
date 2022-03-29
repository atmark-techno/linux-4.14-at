/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#ifndef __GLOBAL_H
#define __GLOBAL_H

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/tty.h>
#include <linux/serial.h>


/* wrap types for linux */
typedef unsigned char       UINT8;
typedef unsigned char       BYTE;
typedef unsigned short      UINT16;
typedef unsigned long       UINT32;
typedef unsigned int        UINT;

typedef unsigned long       DWORD;
typedef unsigned long *     PDWORD;
typedef unsigned long long  DWORD64;
typedef void *              PVOID;
typedef unsigned char *     PBYTE;
typedef bool                BOOL;

#define FALSE               false
#define TRUE                true

#define atoi(v)             simple_strtol(v, NULL, 0)

#define MAXDWORD            ULONG_MAX


#define EV_DSR              TIOCM_DSR
#define EV_CTS              TIOCM_CTS
#define EV_RING             TIOCM_RI
#define EV_RLSD             TIOCM_CD
#define EV_DTR              TIOCM_DTR
#define EV_RTS              TIOCM_RTS
//EV_BREAK
//EV_TXEMPTY


#define MS_DSR_ON           TIOCM_DSR
#define MS_CTS_ON           TIOCM_CTS
#define MS_RING_ON          TIOCM_RI
#define MS_RLSD_ON          TIOCM_CD
#define MS_DTR_ON           TIOCM_DTR
#define MS_RTS_ON           TIOCM_RTS


/************************/

//#include <tchar.h>
//#include <stdio.h>
//#include <stdlib.h>
#include "muxdbg.h"
#include "mux_interface.h"

typedef unsigned char  BIT;


// Multiplexer will be compiled with this Mux version.
#define MP_PROTOCOL_VERSION_MAX     MP_REVISION_04
#define MP_PROTOCOL_VERSION_MIN     MP_REVISION_03


// maximum packet size (with header) for a mux packet
#define DEF_MAXFRAMESIZE            98
#if defined(CLIENT_MODE) && CLIENT_MODE
#define MAXFRAMESIZE                (0x800 - 8)
#else
#define MAXFRAMESIZE                (0x1000 - 7)
#endif
#define MAXFRAMESIZE_1K_ALIGNED     ((((MAXFRAMESIZE / 0x400) * 0x400) < MAXFRAMESIZE) ? (((MAXFRAMESIZE + 0x400) / 0x400) * 0x400) : MAXFRAMESIZE)

// Hdlc window size to be used
#define HDLC_DEF_WINDOW_SIZE        4


// RingbufferSizes: must have a size of 2^x
#if defined(CLIENT_MODE) && CLIENT_MODE
#define __MP_TX_RINGBUF_SIZE        0x8000
#define __MP_RX_RINGBUF_SIZE        0x8000
#else
#define __MP_TX_RINGBUF_SIZE        0x10000
#define __MP_RX_RINGBUF_SIZE        0x10000
#endif


// FIFO-Elements for processing mux packets except UIH data packets
#define MP_FIFOELEMENTS             30


// mux_msgqueue.c
#define MAX_MUX_MSG_MEMEORY         30


// The buffer level in bytes on which stopped data sending will be restarted
#define RESTART_SEND_LEVEL          200


/*
    Applications connected to active multiplexer
    0: control channel
    1: data -> xrc0
    2: xrc1
    3: xrc2
    4: ...
*/
#define MP_APP_CTRL     0
#define MP_APP_DATA     1
#define MP_APP_XRC1     2
#define MP_APP_XRC2     3
#define MP_APP_XRC3     4

#define MP_APPCOUNT     5


#define MAX_SYNC_FLAGS      10          // maximum number of 0xF9(flag),
                                        // to be sent during the resynchronisation of MUX protocol

#define TIMEBASE_INTERVAL   1000        // time interval for timing dependant checks


typedef enum
{
    DLCI0,
    DLCI1
}MP_eDLCI;



// power control modes
typedef enum {
    PSC_MODE_DEFAULT                    = 0,
    PSC_MODE_FULL                       = 1,
    PSC_MODE_SLEEP                      = 2,
    PSC_MODE_CYCLIC_SLEEP_SHORT         = 3,
    PSC_MODE_CYCLIC_SLEEP_LONG          = 4,
    PSC_MODE_SWITCH_OFF                 = 5,
    PSC_MODE_RESET                      = 6,
    PSC_MODE_CYCLIC_SLEEP_SHORT_CONT    = 7,
    PSC_MODE_CYCLIC_SLEEP_LONG_CONT     = 8,
    PSC_MODE_CYCLIC_SLEEP_LONG_CONT2    = 9
}MP_ePSCMode;

// power control results
typedef enum {
    PSC_RES_FAILURE,
    PSC_RES_SUCCESS,
    PSC_RES_DEFAULT
}MP_ePSCResult;


typedef enum
{
    eMP_TX_BUF,
    eMP_RX_BUF
} T_MPBuf;



/*---------------------------------------------------------------------------
  @enum     MP_eState | states of DLC statemachine
  @devnote    attention:  the sequence of state functions must correspond to
                           the sequence of state enums !!!
--------------------------------------------------------------------------- */
typedef enum
{
    DLC_CLOSEDDOWN,
    DLC_WAIT4STARTUP,
    DLC_DISCONNECTED,
    DLC_DISCONNECTEDNEGOTIATION,
    DLC_DISCONNECTEDWAIT4UAFRAME,
    DLC_CONNECTED,
    DLC_CONNECTEDWAIT4RESPONSE,
    DLC_WAIT4CLOSEDOWN,
    NUMBER_OF_MP_STATES
} MP_eState;

typedef enum
{
    eLINK_ACTIVE,
    eLINK_SHUTTING_DOWN,
    eLINK_WAKING_UP,
    eLINK_DOWN
} MP_eLinkState;


typedef enum
{
    SCAN4STARTFLAG,
    SCAN4ADDRESS,
    SCAN4CONTROL,
    SCAN4LENGTH,
    SCAN4DATA,
    SCAN4FCS,
    SCAN4ENDFLAG,
    SCANRESYNC
}  eMP_RXSTATESCANNER;


typedef struct V24STATUS
{
/*0*/  unsigned int DTR            : 1; // Status of the DTR signal
/*1*/  unsigned int RTS            : 1; // Status of the RTS signal
/*2*/  unsigned int OutFlowActive  : 1; // Status of the  outgoing flow control (1 means flow control active)
/*3*/  unsigned int Reserved_0     : 1;
/*4*/  unsigned int BreakInd       : 1; // Signals if the interface receives a break indication
/*5*/  unsigned int Reserved_1     : 1;
/*6*/  unsigned int Reserved_2     : 1;
/*7*/  unsigned int Reserved_3     : 1;
/*8*/  unsigned int CTS            : 1; // Status of the DTE signal
/*9*/  unsigned int DSR            : 1; // Status of the DSR signal
/*a*/  unsigned int RI             : 1; // Indicates a incoming call
/*b*/  unsigned int DCD            : 1; // Status of the DCD signal
/*c*/  unsigned int InFlowActive   : 1; // Status of the incoming flow control (1 means flow control active)
/*d*/  unsigned int BreakReq       : 1; // Break Request
/*e*/  unsigned int Reserved_4     : 1;
/*f*/  unsigned int TxEmpty        : 1; // Indicates that the send buffer is empty
/*0*/  unsigned int TxFull         : 1; // Indicates that the send buffer is full
} V24STATUS;


typedef struct
{
    MP_eState   State;
    UINT8       bFlowControl;
    UINT8       ucLastControl;  // save the last command
    UINT8       ucFrameType;    // special channel framing
    UINT16      uiFrameSize;    // used frame size of incoming data
    V24STATUS   sV24Status;     // logical status of v24 status lines
} MPVAR;


enum
{
    MP_REVISION_ZERO,   // do not use this revision
    MP_REVISION_01,     // first valid revision
    MP_REVISION_02,     //
    MP_REVISION_03,     //
    MP_REVISION_04,     // new HDLC flow control
    MP_REVISION_LAST    // valid revision must be SMALLER than that
                        // AND MUST be smaller than 999 decimal
};

typedef struct
{
    UINT32  ChanNotActivePackets;
    UINT32  LFCActive;
    UINT32  RFCActive;
    UINT32  BytesRead;
    UINT32  BytesWritten;
    UINT32  DevDownLostBytes;
    UINT32  DevUpLostBytes;
    UINT32  AppUpFCActive;
    UINT32  RxBufMax;

    UINT32  SentREJ;
    UINT32  RecREJ;
    UINT32  SentRNR;
    UINT32  RecRNR;
    UINT32  OwnBusy;
    UINT32  ReTrans;
} MP_INFOBLOCK;

typedef struct
{
    UINT32  DevFlowFound;
    UINT32  DevFlowReset;
    UINT32  FlagEndError;
    UINT32  CRCErrors;
    UINT32  CriticalDropdowns;
    UINT32  DLCItoobig;
    UINT32  Lentoolong;
    UINT32  ParserStateError;
    UINT32  OnlyOneF9;
    UINT32  BytesRead;
    UINT32  BytesWritten;
    UINT32  OverallPacketsRead;
    UINT32  BytesOutsidePacketsRead;
    UINT32  SplitUpLoadChunks;
} MP_PARSERINFO;


typedef struct packet
{
    UINT8   ucDlci;
    UINT8   Address; // ( DLCI<<2 ) & ( MPVAR->ucCommandResponse & DLC_FRAME_EA_BIT_SET)
    UINT8   Control;
    UINT16  Len;
    UINT8   BufHeader[4];
    UINT8   Buf[DEF_MAXFRAMESIZE];
    struct packet * next;
} MPFRAME, * pMPFRAME;


typedef struct
{
    UINT8   ucDlci;
    UINT8   Address;
    UINT8   Control;
    UINT16  Len;
    UINT8   BufHeader[4];
    UINT8   Buf[MAXFRAMESIZE];
} REC_MPFRAME;


// max possible packet number = 0 .. 7;
// 3 Bits within I frame header for each direction
#define MP_TX_PACKET_CNT    8


// every packet is in the Tx Ringbuffer . To find it, we use an additional
// pointer system
typedef struct
{
    UINT32 StartOffset;         // start of packet as offset into Tx Ringbuf to the first byte
    UINT32 Len;                 // length of packet
} stMP_TxPacket, *pstMP_TxPacket;


// each multiplex channel has its own packet handling, MP_TX_PACKET_CNT for each
typedef struct
{
    /** packet structure within Tx Rinbuffer */
    stMP_TxPacket TxPacket[MP_TX_PACKET_CNT];
//
//  to prevent from searching through all packets, the _last_ byte + 1 of newest packet
//  is saved here. This is needed to see quickly, if new data arrived from a
//  connected application. This is TxRingbuf->WriteIndex - MaxWrOffset. If
//  no new data available: TxRingbuf->WriteIndex == MaxWrOffset
//
    UINT32 MaxWrOffset;
} stMP_P;


typedef struct
{
    UINT32   BufSize;
    UINT32   Count;
    UINT32   ReadIndex;
    UINT32   WriteIndex;
    UINT8    *Buf;

    stMP_P   Packets;           // packet handling
} MP_TTxRingBuf, *MP_pTxRingBuf;


typedef struct
{
    UINT32   BufSize;
    UINT32   Count;
    UINT32   ReadIndex;
    UINT32   WriteIndex;
    UINT8    *Buf;
} MP_TRxRingBuf, *MP_pRxRingBuf;


typedef struct
{
    pMPFRAME  first;
    pMPFRAME  last;
    UINT8     count;
} MP_TFifo, * MP_pTFifo;


typedef struct // MP_PRIMITIVE_tag
{
    UINT16          Event;      // used event
    UINT8           ucDLCI;     // associated mux channel
    UINT32          ulParam;    // message param 1 --> do not write it directly, only use macros above
    UINT8           *pucPtr;    // message param 2
} MP_PRIMITIVE;


// mux_msgqueue.c

typedef struct _msg_element
{
    struct _msg_element     *pNext;
    MP_PRIMITIVE            MuxMsg;
} MSG_ELEMENT_t;


typedef struct _msg_queue
{
    MSG_ELEMENT_t           *pMuxMsgRead;
    MSG_ELEMENT_t           *pMuxMsgWrite;
} MSG_QUEUE_t;



typedef enum
{
    eHDLC_Invalid,
    eHDLC_I,
    eHDLC_RR,
    eHDLC_REJ,
    eHDLC_RNR,
    eHDLC_T1,
    eHDLC_T_RejResp
} eMP_HdlcEvent;


typedef enum
{
    eReceiver_Ready = 0,
    eReceiver_Busy
} eMP_StateReceiver;


typedef enum
{
    eReject_Reset = 0,
    eReject_Set
} eMP_StateReject;

typedef enum
{
    eAck_NotPending = 0,
    eAck_Pending
} eMP_StateAcknowledge;


#define HDLC_TO_T1              2               // acknowledgement timer, in sec
#define HDLC_TO_CLEAR_REJ       2               // reject response timer, in sec


typedef struct _dlci_hdlc
{
    eMP_HdlcEvent           eEvent;             // current HDLC protocol event

    UINT8                   WindowSize;         // window size
    UINT8                   V_A;                // acknowledge state variable
    UINT8                   V_S;                // send state variable
    UINT8                   V_R;                // receive state variable

    UINT8                   N_R;                // current receive sequence number
    UINT8                   N_S;                // current send sequence number
    UINT8                   P_F;                // current P/F bit

    UINT8                   N_RNR;              // Counter for correct frames after receiving a RNR

    eMP_StateReceiver       eReceiver;          // own receiver state (ready/busy)

    eMP_StateReceiver       ePeerReceiver;      // peer receiver state (ready/busy)
    eMP_StateReject         eReject;            // reject state (set/reset)

    eMP_StateAcknowledge    eAck;               // set/clear acknowledge pending for received I-Frames

    BOOL                    fRetransActive;

    UINT32                  T1_Timer;
    UINT32                  T_ClearRej;
} DLCI_HDLC_t;

typedef struct _dlci_instance
{
    MPVAR               sMP_DLCI;

    MP_INFOBLOCK        sMP_Info;
    MP_TTxRingBuf       MP_TxRingBuf;
    MP_TRxRingBuf       MP_RxRingBuf;

    DLCI_HDLC_t         sHDLC;

    // from mux_interface.h
    DWORD               dwUserData;
    DLCI_INTERFACE_t    DLCIInterface;
} DLCI_INSTANCE_t, *pDLCI_INSTANCE_t;

// to have a compact memory we use one big structure
typedef struct
{
    UINT8 *RxBuf;
    UINT8 *TxBuf;
} MP_RINGBUFFERSTRUCT;


typedef struct _mux_instance
{
    pDLCI_INSTANCE_t    pDLCIArray;
    DWORD               dwMaxNumberOfDLCI;

    MPFRAME             sMP_Frames[MP_FIFOELEMENTS];
    MP_TFifo            MP_freelist;
    MP_TFifo            MP_TxFifo;

    // Bitmask, for which DLCI a RX/TX-Queue-Msg is actually not looped
    // by the loop-process, but pending
    UINT16              MP_IndRXQueueLoopActive;

    // because there is a round robin for all available tx data, there is
    // only 1 message to lock
    BIT                 MP_IndTXQueueLoopActive;

    // Bitmask for "send V24 ind" msg pending
    UINT16              MP_V24IndLoopActive;

    // all instances not connected to internal applications but active
    // will loop all incoming data back to the user
    UINT16              MP_DataLoopOnInstance;

    BIT                 DLCI0_Established;

    MP_PARSERINFO       sMP_ParserInfo;

    UINT16              MP_uiUsedProtVersion;
    UINT16              MP_uiExternalVersion;
    UINT16              MP_uiInternalVersion;

    MP_eLinkState       MP_LinkState;

    MP_RINGBUFFERSTRUCT RingBufferStatic;

    // variables for RX-parsing
    eMP_RXSTATESCANNER  MP_RxStateScanner;
    UINT32              u32ResyncTickCount;         // tick counter for control of the resynchronisation of the MUX protocol
    UINT32              Datalen;
    UINT32              F9Count;
    UINT8               ucLastSendDLCI;

    UINT8               ucMP_RxDLCI;
    REC_MPFRAME         RxFrame;
    UINT8               fcs8_RX;
    UINT8               fcs8_TX;

    // mux_msgqueue.c
    MSG_ELEMENT_t       MuxMsgMem[MAX_MUX_MSG_MEMEORY];
    MSG_QUEUE_t         MuxMsgQueue;

    MUX_INTERFACE_t     MuxInterface;
    DWORD               dwMuxUserData;

    DWORD               dwMpWindowSize;             // window size (MP_REVISION_04)
    DWORD               dwMpFrameSize;              // frame size (MP_REVISION_04)
    DWORD               dwMpRevision;               // desired min protocol revision
    DWORD               dwMpBaudRate;               // baud rate ot the base port (0 on usb)

    UINT8               ucInitiator;                // module is always slave (value = 0), customer app always master (value = 1)

    UINT8               SendBuf[MAXFRAMESIZE + 7];
} MUX_INSTANCE_t, *pMUX_INSTANCE_t;

#define NULLP                  0

#define MEMCPY                 memcpy
#define MEMCMP                 memcmp
#define MEMSET                 memset
#define MEMCLR(p, s)           memset(p, 0, s)

#define RX_LWM(pMux, ucDlci)   (pMux->pDLCIArray[ucDlci].MP_RxRingBuf.BufSize / 8)
#define RX_HWM(pMux, ucDlci)   (pMux->pDLCIArray[ucDlci].MP_RxRingBuf.BufSize / 2)

#define MINMAX(VAL, MIN, MAX)  min(max(VAL, MIN), MAX)

#endif // __GLOBAL_H
