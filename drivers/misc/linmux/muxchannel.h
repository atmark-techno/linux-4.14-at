/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Muxhhannel.h
//
// This file contains the definition of the MuxChannel object which provides
// the functionality of the virtual COM ports.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __MUXCHANNEL_H
#define __MUXCHANNEL_H

#include "global.h"
#include "os_wrap.h"

#include "baseport.h"

//////////////////////////////////////////////////////////////////////////////

#define MUX_WRITE_BUFFER_SIZE  MAXFRAMESIZE
#define MUX_WRITE_RINGBUF_SIZE 0x4000
#define MUX_WQUEUE_NAME        "MuxChnWQ"

//////////////////////////////////////////////////////////////////////////////

typedef enum {
  MUXCHN_CLOSED = 0,
  MUXCHN_OPENING,
  MUXCHN_OPEN,
  MUXCHN_CLOSING,
  MUXCHN_KILLED
} MUXCHAN_STATE;

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  WORKQUEUE_HANDLE          m_hWorkQueue;
  WORKQUEUE_ITEM            m_WorkQueueOpen;
  WORKQUEUE_ITEM            m_WorkQueueClose;
  WORKQUEUE_ITEM            m_WorkQueueShutDown;
  WORKQUEUE_ITEM            m_WorkQueueModemSignal;
  WORKQUEUE_ITEM            m_WorkQueueClearBuffers;
  WORKQUEUE_ITEM            m_WorkQueueContinueReceive;
  WORKQUEUE_ITEM            m_WorkQueueEsc;
  int                       m_iWorkRet;

  SPINLOCK                  m_slLock;

  THREADHANDLE              m_hWriteThread;
  RingBuf_t                 m_WriteRingBuf;
  BYTE                      m_bWriteBuf[WRITE_BUFFER_SIZE];

  DWORD                     m_dwOpenCount;

  MUXCHAN_STATE             m_State;

  DWORD                     m_dwDlci;

  pMUX_INSTANCE_t           m_pMuxInstance;

  EVTHANDLE                 m_hStartStopEvent;
  DWORD                     m_dwStartStopResult;

  MUX_V24STATUS_t           m_ModuleState;
  BOOL                      m_fRtsCtsFlowCtrl;

  EVTHANDLE                 m_hTxEmptyEvent;
  EVTHANDLE                 m_hTxSpaceEvent;
  EVTHANDLE                 m_hCommEvent;

  // ParseEscape
  DWORD64                   m_dw64TimeLastChar;
  DWORD64                   m_dw64TimeFirstPlus;
  DWORD                     m_dwPlusCnt;

  // Rx buffer information
  DWORD                     m_dwBytesInRx;
  DWORD                     m_dwSizeOfRx;
  
  // Clear buffers flags
  BOOL                      m_fRxClear;
  BOOL                      m_fTxClear;

  PT_BASEPORT               m_pBasePort;

  UPPERPORT                 m_UpperPort;
} T_MUXCHAN, *PT_MUXCHAN;

//////////////////////////////////////////////////////////////////////////////

PT_MUXCHAN mc_Create(PT_BASEPORT pBasePort, DWORD dwDlci, TTYHANDLE hTty);
void  mc_Destroy(PT_MUXCHAN pMuxChn);

int   mc_Open(PT_MUXCHAN pMuxChn);
BOOL  mc_Close(PT_MUXCHAN pMuxChn);

int   mc_GetErrCodeOnState(PT_MUXCHAN pMuxChn);

int   mc_WriteData(PT_MUXCHAN pMuxChn, PBYTE pData, DWORD dwLen);

int   mc_SetFlowCtrl(PT_MUXCHAN pMuxChn, BOOL fEnable);
int   mc_GetModemState(PT_MUXCHAN pMuxChn, PDWORD pdwState);
int   mc_SetModemState(PT_MUXCHAN pMuxChn, DWORD dwState);
int   mc_ClearModemState(PT_MUXCHAN pMuxChn, DWORD dwState);
int   mc_GetBufferInfo(PT_MUXCHAN pMuxChn, PDWORD pdwBytesInRx, PDWORD pdwBytesInTx, PDWORD pdwRxSize, PDWORD pdwTxSize);
int   mc_ClearBuffers(PT_MUXCHAN pMuxChn, BOOL fRx, BOOL fTx);
int   mc_ContinueReceive(PT_MUXCHAN pMuxChn);
int   mc_WaitForTxSpace(PT_MUXCHAN pMuxChn, DWORD dwTimeout);
int   mc_WaitForTxEmpty(PT_MUXCHAN pMuxChn, DWORD dwTimeout);
int   mc_WaitForCommEvent(PT_MUXCHAN pMuxChn, DWORD dwState);

#endif // __MUXCHANNEL_H

