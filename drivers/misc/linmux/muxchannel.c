/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// MuxChannel.cpp
//
// This file contains the implementation of the class "CMuxChannel" providing
// the functionality of the virtual COM ports.
//
//////////////////////////////////////////////////////////////////////////////

#include "muxdbg.h"
#include "muxdrv.h"

#include "muxchannel.h"

//////////////////////////////////////////////////////////////////////////////

/****************************************************************************/
/* mux callback interface                                                   */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called when a call of "Mux_DLCIEstablish()" has finished.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used for access to our class instance.
// dwDlci       : The corresponding mux channel number.
// dwResult     : The result of the DLCI establish operation.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void MuxEstablishResult(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwDlci, DWORD dwResult) {
  DBG_ENTER(ZONE_FCT_MUXCHAN, "Instance=%p, MuxChn=%p, Result=%d, Dlci=%d", dwMuxInstance, dwUserData, dwResult, dwDlci);
  ((PT_MUXCHAN)dwUserData)->m_dwStartStopResult = dwResult;
  oswrap_SetEvent(&((PT_MUXCHAN)dwUserData)->m_hStartStopEvent);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", dwUserData, dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called when a call of "Mux_DLCIRelease()" has finished.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used for access to our class instance.
// dwDlci       : The corresponding mux channel number.
// dwResult     : The result of the DLCI release operation.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void MuxReleaseResult(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwDlci, DWORD dwResult) {
  DBG_ENTER(ZONE_FCT_MUXCHAN, "Instance=%p, MuxChn=%p, Result=%d, Dlci=%d", dwMuxInstance, dwUserData, dwResult, dwDlci);
  ((PT_MUXCHAN)dwUserData)->m_dwStartStopResult = dwResult;
  oswrap_SetEvent(&((PT_MUXCHAN)dwUserData)->m_hStartStopEvent);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", dwUserData, dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to tell us that we can continue to send data after it had been
// stalled.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used for access to our class instance.
// dwDlci       : The corresponding mux channel number.
// dwLen        : The number of free bytes.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void MuxSendDataContinue(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwDlci, DWORD dwLen) {
  DBG_ENTER(ZONE_FCT_MUXCHAN, "Instance=%p, MuxChn=%p, Len=%d, Dlci=%d", dwMuxInstance, dwUserData, dwLen, dwDlci);
  if (dwUserData) {
    oswrap_UpperPortContinueSending(&((PT_MUXCHAN)dwUserData)->m_UpperPort);
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", dwUserData, dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called with available received data.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used for access to our class instance.
// dwDlci       : The corresponding mux channel number.
// pData        : Pointer to received data.
// dwLen        : Number of received bytes.
// dwBytesInBuf : The number of bytes currently in the rx buffer.
//
// Return:
// The number of read bytes.
//
//////////////////////////////////////////////////////////////////////////////
DWORD MuxReceiveData(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwDlci, PBYTE pData, DWORD dwLen, DWORD dwBytesInBuf) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)dwUserData;
  DWORD      dwWritten;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "Instance=%p, MuxChn=%p, pData=%p, Len=%d, Rest=%d, Dlci=%d", dwMuxInstance, dwUserData, pData, dwLen, dwBytesInBuf, dwDlci);
  // Update internal buffer info
  oswrap_SpinLock(&pMuxChn->m_slLock);
  if (pMuxChn->m_fRxClear) {
    // There is an ongoing clear rx buffer operation -> we must discard the current data
    oswrap_SpinUnlock(&pMuxChn->m_slLock);
    DBGPRINT(ZONE_FCT_MUXCHAN, "Data received during ongoing clear rx buffer operation!");
    dwWritten = dwLen;
  } else {
    pMuxChn->m_dwBytesInRx = dwBytesInBuf;
    oswrap_SpinUnlock(&pMuxChn->m_slLock);
    dwWritten = oswrap_UpperPortPushData(&pMuxChn->m_UpperPort, pData, dwLen);
    if (dwLen != dwWritten) {
      DBGPRINT(ZONE_FCT_MUXCHAN, "Only %d of %d bytes sent to upper tty port!", dwWritten, dwLen);
    }
    // Update internal buffer info
    oswrap_SpinLock(&pMuxChn->m_slLock);
    pMuxChn->m_dwBytesInRx = (dwBytesInBuf > dwWritten) ? (dwBytesInBuf - dwWritten) : 0;
    oswrap_SpinUnlock(&pMuxChn->m_slLock);
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%d, MuxChn=%p, Dlci=%d", dwWritten, dwUserData, dwDlci);
  return dwWritten;
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called when a state change of the virtual modem signals has been received.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used for access to our class instance.
// dwDlci       : The corresponding mux channel number.
// V24Status    : The received virtual V24 state.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void MuxReceiveV24Status(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwDlci, MUX_V24STATUS_t V24Status) {
  PT_MUXCHAN      pMuxChn = (PT_MUXCHAN)dwUserData;
  MUX_V24STATUS_t ModuleState;
  DWORD           dwChangedMask;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "Instance=%p, MuxChn=%p, State=0x%x, Dlci=%d", dwMuxInstance, dwUserData, V24Status, dwDlci);

  oswrap_SpinLock(&pMuxChn->m_slLock);

  // Store the new module state and check against event mask
  dwChangedMask = 0;
  if (V24Status.CTS  != pMuxChn->m_ModuleState.CTS)  dwChangedMask |= EV_CTS;
  if (V24Status.DSR  != pMuxChn->m_ModuleState.DSR)  dwChangedMask |= EV_DSR;
  if (V24Status.RI   != pMuxChn->m_ModuleState.RI)   dwChangedMask |= EV_RING;
  if (V24Status.DCD  != pMuxChn->m_ModuleState.DCD)  dwChangedMask |= EV_RLSD;
  //if (V24Status.BreakInd)                            dwChangedMask |= EV_BREAK;
  if (V24Status.TxEmpty && !pMuxChn->m_ModuleState.TxEmpty) oswrap_SetEvent(&pMuxChn->m_hTxEmptyEvent);
  if (V24Status.TxFull  && !pMuxChn->m_ModuleState.TxFull)  oswrap_ResetEvent(&pMuxChn->m_hTxSpaceEvent);
  if (!V24Status.TxEmpty && pMuxChn->m_ModuleState.TxEmpty) oswrap_ResetEvent(&pMuxChn->m_hTxEmptyEvent);
  if (!V24Status.TxFull  && pMuxChn->m_ModuleState.TxFull)  oswrap_SetEvent(&pMuxChn->m_hTxSpaceEvent);
  pMuxChn->m_ModuleState = V24Status;

  // Consider Dcb flow control setting for incoming data
  ModuleState = pMuxChn->m_ModuleState;
  if (V24Status.InFlowActive) {
    if (pMuxChn->m_fRtsCtsFlowCtrl) pMuxChn->m_ModuleState.RTS = 0;
  } else {
    if (pMuxChn->m_fRtsCtsFlowCtrl) pMuxChn->m_ModuleState.RTS = 1;
  }

  // Check if module state has been changed
  if (MEMCMP(&ModuleState, &pMuxChn->m_ModuleState, sizeof(MUX_V24STATUS_t))) {
    // Send new state to module if it has changed because of RTS/DTR flow control setting
    // Use workqueue to avoid reentering critical section of mux protocol
    oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueModemSignal);
  }

  oswrap_SpinUnlock(&pMuxChn->m_slLock);

  // Release pending mc_WaitForCommEvent() calls corresponding to changed state
  if (dwChangedMask) {
    DBGPRINT(ZONE_MUX_VIRT_SIGNAL, "Detected virtual modem signal change 0x%x on DLCI %d - new state: 0x%x", dwChangedMask, pMuxChn->m_dwDlci, V24Status);
    oswrap_SetEvent(&pMuxChn->m_hCommEvent);
  }

  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", dwUserData, dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called when the multiplex protocol handling has been unexpectedly shut
// down.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used for access to our class instance.
// dwDlci       : The corresponding mux channel number.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void MuxShutdown(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwDlci) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)dwUserData;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "Instance=%p, MuxChn=%p, Dlci=%d", dwMuxInstance, dwUserData, dwDlci);
  oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueShutDown);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", dwUserData, dwDlci);
}

/****************************************************************************/
/* internal functions mux channel                                           */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Thread which will be pumping data from MuxChn's tx buffer into the mux
// protocol handling.
//
// Parameters:
// data: Pointer to the corresponding mux channel object.
//
// Return:
// true indicates that the thread should continue false that it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
static bool mc_ThreadWriteData(void *data) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)data;
  bool fRet = true;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p", pMuxChn);
  // Wait for data in the ringbuffer
  if (ringbuf_WaitForData(&pMuxChn->m_WriteRingBuf)) {
    oswrap_SpinLock(&pMuxChn->m_slLock);
    if (pMuxChn->m_fTxClear) {
      // There is an ongoing clear tx buffer operation
      oswrap_SpinUnlock(&pMuxChn->m_slLock);
      // Wait a short interval to allow the mux core to clear its tx buffer
      oswrap_Sleep(1);
      // And ensure that our write loop is continuing
      ringbuf_PutBytes(&pMuxChn->m_WriteRingBuf, NULL, 0);
      DBGPRINT(ZONE_FCT_MUXCHAN, "Data in write queue during ongoing clear tx buffer operation!");
    } else {
      // Loop to read out all the data from m_WriteRingBuf through m_bWriteBuf
      DWORD dwRead = 1;
      oswrap_SpinUnlock(&pMuxChn->m_slLock);
      while (fRet && dwRead && !oswrap_ShouldStopThread()) {
        DWORD dwLen, dwWritten = 0;
        dwRead = ringbuf_GetBytes(&pMuxChn->m_WriteRingBuf, pMuxChn->m_bWriteBuf, MUX_WRITE_BUFFER_SIZE);
        DBGPRINT(ZONE_FCT_MUXCHAN, "mc_ThreadWriteData(): %d bytes read from m_WriteRingBuf", dwRead);
        // Inner loop to feed data from m_bWriteBuf to the mux core
        dwLen = dwRead;
        while (fRet && dwLen && !oswrap_ShouldStopThread()) {
          if (Dlci_SendData((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci, pMuxChn->m_bWriteBuf + dwWritten, &dwLen) == MUX_OK) {
            DBGPRINT(ZONE_FCT_MUXCHAN, "mc_ThreadWriteData(): Written %d bytes", dwLen);
            if (dwLen == 0) {
              // Mux buffer is full -> Sleep a short interval to allow some data to be processed by the mux core
              oswrap_Sleep(1);
            }
            dwWritten += dwLen;
            dwLen = dwRead - dwWritten;
          } else {
            DBGPRINT(ZONE_ERR, "mc_ThreadWriteData(): Error from Dlci_SendData()");
            fRet = false;
          }
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%s, MuxChn=%p", STRBOOL(fRet), pMuxChn);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Parse a given buffer for three plus characters. Helper function for
// "ParseEscape()".
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
// pData  : The buffer to be parsed.
// dwLen  : The number of bytes in the data buffer.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_ParsePlus(PT_MUXCHAN pMuxChn, PBYTE pData, DWORD dwLen) {
  DWORD i;

  for (i = 0; i < dwLen; i++) {
    if (pData[i] != '+') {
      pMuxChn->m_dwPlusCnt = 0;
      break;
    }
    pMuxChn->m_dwPlusCnt++;
    if (pMuxChn->m_dwPlusCnt == 1) {
      pMuxChn->m_dw64TimeFirstPlus = oswrap_GetTickCount();
    } else if (pMuxChn->m_dwPlusCnt > 3) {
      pMuxChn->m_dwPlusCnt = 0;
      break;
    }
  }

  if (pMuxChn->m_dwPlusCnt == 3) {
    // We found the 1st part of a valid +++ sequence
    oswrap_WorkQueueQueueItemDelayed(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueEsc, 1000);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Parse for a valid "+++" sequence in the data stream written to the port.
// The rule for a valid escape sequence is as follows:
// The escape sequence consists of 3 consecutive '+' characters. It must be
// preceded and followed by a pause of at least 1000 ms. The +++ characters
// must be entered in quick succession, all within 1000 ms.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
// pData  : The buffer containing the data written to the channel.
// dwLen  : The number of bytes in the data buffer.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_ParseEscape(PT_MUXCHAN pMuxChn, PBYTE pData, DWORD dwLen) {
  DWORD64 dw64CurrentTickCount = oswrap_GetTickCount();

  if (pMuxChn->m_State == MUXCHN_OPEN) {
    if ((dwLen + pMuxChn->m_dwPlusCnt) <= 3) {
      if (!pMuxChn->m_dwPlusCnt) {
        if ((dw64CurrentTickCount - pMuxChn->m_dw64TimeLastChar) >= 1000) {
          mc_ParsePlus(pMuxChn, pData, dwLen);
        }
      } else {
        if ((dw64CurrentTickCount - pMuxChn->m_dw64TimeFirstPlus) < 1000) {
          mc_ParsePlus(pMuxChn, pData, dwLen);
        } else {
          pMuxChn->m_dwPlusCnt = 0;
          if ((dw64CurrentTickCount - pMuxChn->m_dw64TimeLastChar) >= 1000) {
            mc_ParsePlus(pMuxChn, pData, dwLen);
          }
        }
      }
    } else {
      pMuxChn->m_dwPlusCnt = 0;
    }
    pMuxChn->m_dw64TimeLastChar = dw64CurrentTickCount;

    // cancel possible running detect escape procedure
    if (!pMuxChn->m_dwPlusCnt) {
      oswrap_WorkQueueCancelDelayedWork(&pMuxChn->m_WorkQueueEsc);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Establish a virtual mux channel.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL mc_DlciOpen(PT_MUXCHAN pMuxChn) {
  BOOL              fRet = FALSE;
  DLCI_INTERFACE_t  CallBacks;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", pMuxChn, pMuxChn->m_dwDlci);

  oswrap_SetEvent(&pMuxChn->m_hTxEmptyEvent);
  oswrap_SetEvent(&pMuxChn->m_hTxSpaceEvent);

  // Register the mux channel
  MEMCLR(&CallBacks, sizeof(DLCI_INTERFACE_t));
  CallBacks.pDlciEstablishResult   = MuxEstablishResult;
  CallBacks.pDlciReleaseResult     = MuxReleaseResult;
  CallBacks.pDlciSendDataContinue  = MuxSendDataContinue;
  CallBacks.pDlciReceiveData       = MuxReceiveData;
  CallBacks.pDlciReceiveV24Status  = MuxReceiveV24Status;
  CallBacks.pDlciShutdown          = MuxShutdown;
  if (Dlci_Register((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci, (DWORD)pMuxChn, &CallBacks)) {
    DBGPRINT(ZONE_ERR, "Dlci_Register() failed");
  } else {
    // Open the mux channel
    oswrap_ResetEvent(&pMuxChn->m_hStartStopEvent);
    pMuxChn->m_dwStartStopResult = 0;
    if (Dlci_Establish((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci)) {
      DBGPRINT(ZONE_ERR, "Dlci_Establish() failed");
    } else {
      // Wait for the mux protocol to be started
      if (oswrap_WaitForEvent(&pMuxChn->m_hStartStopEvent, TIMEOUT_MODULE_STD) != 0) {
        // In case the channel is still connected we try to release it
        DBGPRINT(ZONE_ERR, "Timeout waiting for Dlci_Establish() result -> trying to close the channel!");
        oswrap_ResetEvent(&pMuxChn->m_hStartStopEvent);
        pMuxChn->m_dwStartStopResult = 0;
        Dlci_Release((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci);
        // wait for release
        if (oswrap_WaitForEvent(&pMuxChn->m_hStartStopEvent, TIMEOUT_MODULE_STD) != 0) {
          // We timed out
          DBGPRINT(ZONE_ERR, "Dlci_Release() timed out!");
        } else {
          // ok: channel now released -> try new connection
          oswrap_ResetEvent(&pMuxChn->m_hStartStopEvent);
          pMuxChn->m_dwStartStopResult = 0;
          if (Dlci_Establish((DWORD)pMuxChn->m_pMuxInstance, (DWORD)pMuxChn)) {
            DBGPRINT(ZONE_ERR, "Dlci_Establish() failed again");
          } else {
            if (oswrap_WaitForEvent(&pMuxChn->m_hStartStopEvent, TIMEOUT_MODULE_STD) != 0) {
              // We timed out
              DBGPRINT(ZONE_ERR, "Dlci_Establish() timed out!");
            } else {
              // Check if everything was ok
              if (0 == pMuxChn->m_dwStartStopResult) {
                // We need a short delay here to allow mux frame size negotiation to finish to avoid
                // that too early opened mux channels are using the wrong frame size.
                oswrap_Sleep(100);
                fRet = TRUE;
              } else {
                DBGPRINT(ZONE_ERR, "Dlci_Establish() Result %d", pMuxChn->m_dwStartStopResult);
              }
            }
          }
        }
      } else {
        // Check if everything was ok
        if (0 == pMuxChn->m_dwStartStopResult) {
          // We need a short delay here to allow mux frame size negotiation to finish to avoid
          // that too early opened mux channels are using the wrong frame size.
          oswrap_Sleep(100);
          fRet = TRUE;
        } else {
          DBGPRINT(ZONE_ERR, "Dlci_Establish() Result %d", pMuxChn->m_dwStartStopResult);
        }
      }
    }
  }

  if (fRet != TRUE) {
    Dlci_Deregister((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci);
  }

  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%s, MuxChn=%p, Dlci=%d", STRBOOL(fRet), pMuxChn, pMuxChn->m_dwDlci);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Close a virtual mux channel.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL mc_DlciClose(PT_MUXCHAN pMuxChn) {
  BOOL fRet = FALSE;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", pMuxChn, pMuxChn->m_dwDlci);

  if (Dlci_IsConnected((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci) == MUX_OK) {
    // Close the mux channel
    oswrap_ResetEvent(&pMuxChn->m_hStartStopEvent);
    pMuxChn->m_dwStartStopResult = 0;
    if (Dlci_Release((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci)) {
      // Something went wrong
      fRet = FALSE;
    } else {
      // Wait for the mux protocol to be stopped
      if (oswrap_WaitForEvent(&pMuxChn->m_hStartStopEvent, 5000) !=  0) {
        // We timed out or have been closed
        DBGPRINT(ZONE_ERR, "Dlci_Release() timed out!");
      }
      // Check if everything was ok
      if (pMuxChn->m_dwStartStopResult) {
        DBGPRINT(ZONE_ERR, "Dlci_Release() Result %d", pMuxChn->m_dwStartStopResult);
      } else {
        fRet = TRUE;
      }
    }
  }

  Dlci_Deregister((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci);

  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%s, MuxChn=%p, Dlci=%d", STRBOOL(fRet), pMuxChn, pMuxChn->m_dwDlci);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Open a virtual port device as deferred work queue call.
//
// Parameters:
// pData: The corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_OpenDeferred(void *pData) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)pData;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  pMuxChn->m_pMuxInstance = bp_AddVCom(pMuxChn->m_pBasePort, pMuxChn->m_dwDlci);
  if (pMuxChn->m_pMuxInstance != NULL) {
    MEMCLR(&pMuxChn->m_ModuleState, sizeof(MUX_V24STATUS_t));
    if (pMuxChn->m_fRtsCtsFlowCtrl) pMuxChn->m_ModuleState.RTS = 1;
    pMuxChn->m_ModuleState.TxEmpty = 1;
    // open the virtual mux channel
    if (!mc_DlciOpen(pMuxChn)) {
      bp_RemoveVCom(pMuxChn->m_pBasePort);
    } else {
      // Get rx buffer size
      DWORD dwRxSize;
      Dlci_GetQueueInformation((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci, NULL, NULL, &dwRxSize, NULL);
      // Start write handling
      ringbuf_Clear(&pMuxChn->m_WriteRingBuf);
      oswrap_StartThread(pMuxChn->m_hWriteThread);
      oswrap_SpinLock(&pMuxChn->m_slLock);
      pMuxChn->m_iWorkRet = 0;
      pMuxChn->m_dwSizeOfRx = dwRxSize;
      oswrap_SpinUnlock(&pMuxChn->m_slLock);
    }
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Close a virtual port device as deferred work queue call.
//
// Parameters:
// pData: The corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_CloseDeferred(void *pData) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)pData;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  if (pMuxChn->m_pMuxInstance != NULL) {
    oswrap_WorkQueueCancelDelayedWork(&pMuxChn->m_WorkQueueEsc);
    // Close the virtual mux channel
    mc_DlciClose(pMuxChn);
    // Stop write thread
    oswrap_StopThread(pMuxChn->m_hWriteThread);
    // Unregister closed port
    bp_RemoveVCom(pMuxChn->m_pBasePort);
    pMuxChn->m_pMuxInstance = NULL;
    // Abort possibly blocked interface calls
    oswrap_SetEvent(&pMuxChn->m_hTxEmptyEvent);
    oswrap_SetEvent(&pMuxChn->m_hTxSpaceEvent);
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown a virtual port device as deferred work queue call.
//
// Parameters:
// pData: The corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_ShutDownDeferred(void *pData) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)pData;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  pMuxChn->m_State = MUXCHN_KILLED;
  oswrap_WorkQueueCancelDelayedWork(&pMuxChn->m_WorkQueueEsc);
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  // Stop write thread
  oswrap_StopThread(pMuxChn->m_hWriteThread);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Deferred work queue item started to detect the final 1 second without data
// of the 1sec > +++ > 1sec escape sequence after the +++ has been received.
//
// Parameters:
// pData: The corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_DetectEscDeferred(void *pData) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)pData;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", pMuxChn, pMuxChn ? pMuxChn->m_dwDlci : -1);
  if (pMuxChn) {
    bool fFound = false;
    oswrap_SpinLock(&pMuxChn->m_slLock);
    if (pMuxChn->m_State == MUXCHN_OPEN) {
      if (pMuxChn->m_dwPlusCnt == 3) {
        // Valid "+++" sequence detected
        DBGPRINT(ZONE_GENERAL_INFO, "MuxDrv: Escape sequence detected on DLCI %d", pMuxChn->m_dwDlci);
        pMuxChn->m_dwPlusCnt = 0;
        fFound = true;
      }
    }
    oswrap_SpinUnlock(&pMuxChn->m_slLock);
    if (fFound) {
      Dlci_SendEsc((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci);
    }
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=false, MuxChn=%p, Dlci=%d", pMuxChn, pMuxChn ? pMuxChn->m_dwDlci : -1);
}

//////////////////////////////////////////////////////////////////////////////
//
// Send the V24 state to the module as deferred work queue call.
//
// Parameters:
// pData: The corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_SetV24StateDeferred(void *pData) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)pData;
  MUX_V24STATUS_t ModuleState;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  ModuleState = pMuxChn->m_ModuleState;
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  Dlci_SetV24Status((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci, ModuleState);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Clear send and receive buffers as as a deferred procedure call.
//
// Parameters:
// pData: The corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_ClearBuffersDeferred(void *pData) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)pData;
  BOOL       fRxClear, fTxClear;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  fRxClear = pMuxChn->m_fRxClear;
  fTxClear = pMuxChn->m_fTxClear;
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  Dlci_EmptyQueues((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci, fRxClear, fTxClear);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  pMuxChn->m_fRxClear = FALSE;
  pMuxChn->m_fTxClear = FALSE;
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Tell mux protocol implementation that it can continue to deliver received
// data as a deferred procedure call.
//
// Parameters:
// pData: The corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mc_ContinueReceiveDeferred(void *pData) {
  PT_MUXCHAN pMuxChn = (PT_MUXCHAN)pData;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  Dlci_ReceiveDataContinue((DWORD)pMuxChn->m_pMuxInstance, pMuxChn->m_dwDlci);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
}

/****************************************************************************/
/* interface mux channel                                                    */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Constructor
//
// Parameters:
// pBasePort : base port of the virtual channel
// dwDlci    : DLCI of the virtual channel
// hTty      : Handle of the corresponding upper tty port
//
// Return:
// Pointer to the created mux channel object, NULL in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
PT_MUXCHAN mc_Create(PT_BASEPORT pBasePort, DWORD dwDlci, TTYHANDLE hTty) {
  PT_MUXCHAN pMuxChn = NULL;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "BasePort=%p, Dlci=%d", pBasePort, dwDlci);
  pMuxChn = oswrap_GetMem(sizeof(T_MUXCHAN));
  if (pMuxChn) {
    // reset mux channel struct
    MEMCLR(pMuxChn, sizeof(T_MUXCHAN));

    pMuxChn->m_hWorkQueue = oswrap_WorkQueueCreate(MUX_WQUEUE_NAME);
    if (!pMuxChn->m_hWorkQueue) {
      oswrap_FreeMem(pMuxChn);
      pMuxChn = NULL;
    } else {
      if (!oswrap_UpperPortCreate(&pMuxChn->m_UpperPort)) {
        oswrap_WorkQueueDestroy(pMuxChn->m_hWorkQueue);
        oswrap_FreeMem(pMuxChn);
        pMuxChn = NULL;
      } else {
        oswrap_UpperPortRegister(&pMuxChn->m_UpperPort, hTty);
        pMuxChn->m_dwDlci = dwDlci;
        pMuxChn->m_pBasePort = pBasePort;

        oswrap_WorkQueueInitItem(&pMuxChn->m_WorkQueueOpen, mc_OpenDeferred, pMuxChn);
        oswrap_WorkQueueInitItem(&pMuxChn->m_WorkQueueClose, mc_CloseDeferred, pMuxChn);
        oswrap_WorkQueueInitItem(&pMuxChn->m_WorkQueueShutDown, mc_ShutDownDeferred, pMuxChn);
        oswrap_WorkQueueInitItem(&pMuxChn->m_WorkQueueModemSignal, mc_SetV24StateDeferred, pMuxChn);
        oswrap_WorkQueueInitItem(&pMuxChn->m_WorkQueueClearBuffers, mc_ClearBuffersDeferred, pMuxChn);
        oswrap_WorkQueueInitItem(&pMuxChn->m_WorkQueueContinueReceive, mc_ContinueReceiveDeferred, pMuxChn);
        oswrap_WorkQueueInitDelayedItem(&pMuxChn->m_WorkQueueEsc, mc_DetectEscDeferred, pMuxChn);

        oswrap_CreateEvent(&pMuxChn->m_hStartStopEvent, FALSE);

        oswrap_CreateEvent(&pMuxChn->m_hTxEmptyEvent, TRUE);
        oswrap_CreateEvent(&pMuxChn->m_hTxSpaceEvent, TRUE);
        oswrap_CreateEvent(&pMuxChn->m_hCommEvent, FALSE);

        ringbuf_StaticInit(&pMuxChn->m_WriteRingBuf, MUX_WRITE_RINGBUF_SIZE);
        pMuxChn->m_hWriteThread = oswrap_CreateThread(mc_ThreadWriteData, pMuxChn, "MuxChnWriteData");

        oswrap_SpinLockInitialize(&pMuxChn->m_slLock);

        pMuxChn->m_fRtsCtsFlowCtrl = TRUE;
      }
    }
  } else {
    DBGPRINT(ZONE_ERR, "no memory!");
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%p", pMuxChn);
  return pMuxChn;
}

//////////////////////////////////////////////////////////////////////////////
//
// Destructor
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void mc_Destroy(PT_MUXCHAN pMuxChn) {
  DWORD dwDlci = -1;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", pMuxChn, pMuxChn ? pMuxChn->m_dwDlci : -1);
  if (pMuxChn) {
    dwDlci = pMuxChn->m_dwDlci;
    oswrap_WorkQueueDestroy(pMuxChn->m_hWorkQueue);
    oswrap_DestroyThread(pMuxChn->m_hWriteThread);
    oswrap_UpperPortDestroy(&pMuxChn->m_UpperPort);
    ringbuf_Destroy(&pMuxChn->m_WriteRingBuf);
    oswrap_FreeMem(pMuxChn);
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", pMuxChn, dwDlci);
}

//////////////////////////////////////////////////////////////////////////////
//
// Open a virtual port device. To avoid deadlocks in some kernel versions
// when calling the open function of another tty device inside our tty open
// procedure we do the open work in a deferred procedure call via a work
// queue.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_Open(PT_MUXCHAN pMuxChn) {
  int iRet = -ENODEV;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  if (pMuxChn) {
    BOOL fWait = FALSE;
    oswrap_SpinLock(&pMuxChn->m_slLock);
    pMuxChn->m_dwBytesInRx = 0;
    pMuxChn->m_dwSizeOfRx  = 0;
    pMuxChn->m_fRxClear = FALSE;
    pMuxChn->m_fTxClear = FALSE;
    // If we're currently opening we need to
    if (pMuxChn->m_State == MUXCHN_OPENING) {
      iRet = -EAGAIN;
    } else {
      // Check if we are not already open
      if (!pMuxChn->m_dwOpenCount) {
        // Use workqueue to avoid that we're interrupted by any signal
        fWait = TRUE;
        pMuxChn->m_iWorkRet = -ENODEV;
        pMuxChn->m_State = MUXCHN_OPENING;
        oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueOpen);
      } else {
        if (pMuxChn->m_State != MUXCHN_KILLED) {
          pMuxChn->m_dwOpenCount++;
          iRet = 0;
        }
      }
    }
    oswrap_SpinUnlock(&pMuxChn->m_slLock);
    if (fWait) {
      // We wait until the queued open has finished
      oswrap_WorkQueueFlush(pMuxChn->m_hWorkQueue);
      oswrap_SpinLock(&pMuxChn->m_slLock);
      iRet = pMuxChn->m_iWorkRet;
      if (iRet == 0) {
        if (pMuxChn->m_State == MUXCHN_KILLED) {
          iRet = -ENODEV;
        } else {
          pMuxChn->m_State = MUXCHN_OPEN;
          pMuxChn->m_dwOpenCount++;
          // Trigger initialization of the virtual modem signals
          oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueModemSignal);
        }
      } else {
        pMuxChn->m_State = MUXCHN_CLOSED;
      }
      oswrap_SpinUnlock(&pMuxChn->m_slLock);
    }
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%d, OpenCount=%d, MuxChn=%p, Dlci=%d", iRet, pMuxChn->m_dwOpenCount, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Close a virtual port device. To avoid deadlocks in some kernel versions
// when calling the close function of another tty device inside our tty close
// procedure we do the close work in a deferred procedure call via a work
// queue.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
BOOL mc_Close(PT_MUXCHAN pMuxChn) {
  BOOL fRet = TRUE;

  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, OpenCount=%d, Dlci=%d", pMuxChn, pMuxChn->m_dwOpenCount, pMuxChn->m_dwDlci);
  if (pMuxChn) {
    BOOL fWait = FALSE;
    oswrap_SpinLock(&pMuxChn->m_slLock);
    // Check if we are really open
    if (pMuxChn->m_dwOpenCount) {
      pMuxChn->m_dwOpenCount--;
      if (pMuxChn->m_dwOpenCount == 0) {
        // Use workqueue to avoid that we're interrupted by any signal
        if (pMuxChn->m_State != MUXCHN_KILLED) {
          pMuxChn->m_State = MUXCHN_CLOSING;
        }
        fWait = TRUE;
        oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueClose);
      }
    } else {
      DBGPRINT(ZONE_ERR, "No port open!");
      fRet = FALSE;
    }
    oswrap_SpinUnlock(&pMuxChn->m_slLock);
    if (fWait) {
      // We wait until the queued close has finished
      oswrap_WorkQueueFlush(pMuxChn->m_hWorkQueue);
      oswrap_SpinLock(&pMuxChn->m_slLock);
      pMuxChn->m_State = MUXCHN_CLOSED;
      oswrap_SpinUnlock(&pMuxChn->m_slLock);
    }
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%s, OpenCount=%d, MuxChn=%p, Dlci=%d", STRBOOL(fRet), pMuxChn->m_dwOpenCount, pMuxChn, pMuxChn->m_dwDlci);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get an error code corresponding to the internal state without the
// corresponding spinlock.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// The requested error code.
//
//////////////////////////////////////////////////////////////////////////////
int mc_GetErrCodeOnStateUnlocked(PT_MUXCHAN pMuxChn) {
  int iRet;
  switch (pMuxChn->m_State) {
    case MUXCHN_OPEN   : iRet = 0;       break;
    case MUXCHN_OPENING: iRet = -EAGAIN; break;
    default            : iRet = -ENODEV; break;
  }
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get an error code corresponding to the internal state.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// The requested error code.
//
//////////////////////////////////////////////////////////////////////////////
int mc_GetErrCodeOnState(PT_MUXCHAN pMuxChn) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", pMuxChn, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Write data to the virtual mux port.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
// pData  : The buffer containing the data.
// dwLen  : The max. number of bytes to write.
//
// Return:
// The number of actually written bytes, a negative value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_WriteData(PT_MUXCHAN pMuxChn, PBYTE pData, DWORD dwLen) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, pData=%p, Len=%d, Dlci=%d", pMuxChn, pData, dwLen, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    iRet = ringbuf_PutBytes(&pMuxChn->m_WriteRingBuf, pData, dwLen);
    mc_ParseEscape(pMuxChn, pData, iRet);
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Enable or disable the RTS/CTS hardware flow control for the mux port.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
// fEnable: Flag indicating if flow control shall be enabled (TRUE) or
//          disabled (FALSE)
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_SetFlowCtrl(PT_MUXCHAN pMuxChn, BOOL fEnable) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Enable=%s, Dlci=%d", pMuxChn, STRBOOL(fEnable), pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    MUX_V24STATUS_t OldModuleState = pMuxChn->m_ModuleState;
    pMuxChn->m_fRtsCtsFlowCtrl = fEnable;
    if (fEnable) {
      if (pMuxChn->m_ModuleState.InFlowActive) {
        pMuxChn->m_ModuleState.RTS = 0;
      } else {
        pMuxChn->m_ModuleState.RTS = 1;
      }
    }
    iRet = 0;
    if (MEMCMP(&pMuxChn->m_ModuleState, &OldModuleState, sizeof(MUX_V24STATUS_t))) {
      // Send new state to module via workqueue to avoid problems with
      // critical section of mux protocol handling
      oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueModemSignal);
    }
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get the current modem state with the modem control signals.
//
// Parameters:
// pMuxChn : Pointer to the corresponding mux channel object.
// pdwState: The buffer receiving the current modem state.
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_GetModemState(PT_MUXCHAN pMuxChn, PDWORD pdwState) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, pState=%p, Dlci=%d", pMuxChn, pdwState, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    *pdwState = 0;
    if (pMuxChn->m_ModuleState.CTS) *pdwState |= MS_CTS_ON;
    if (pMuxChn->m_ModuleState.DSR) *pdwState |= MS_DSR_ON;
    if (pMuxChn->m_ModuleState.RI)  *pdwState |= MS_RING_ON;
    if (pMuxChn->m_ModuleState.DCD) *pdwState |= MS_RLSD_ON;
    if (pMuxChn->m_ModuleState.DTR) *pdwState |= MS_DTR_ON;
    if (pMuxChn->m_ModuleState.RTS) *pdwState |= MS_RTS_ON;
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, State=0x%x, MuxChn=%p, Dlci=%d", iRet, *pdwState, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Set the current modem state with the modem control signals.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
// dwState: The new state to be set.
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_SetModemState(PT_MUXCHAN pMuxChn, DWORD dwState) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, State=0x%x, Dlci=%d", pMuxChn, dwState, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    MUX_V24STATUS_t OldModuleState;
    OldModuleState = pMuxChn->m_ModuleState;
    if (dwState & MS_CTS_ON)  pMuxChn->m_ModuleState.CTS = 1;
    if (dwState & MS_DSR_ON)  pMuxChn->m_ModuleState.DSR = 1;
    if (dwState & MS_RING_ON) pMuxChn->m_ModuleState.RI  = 1;
    if (dwState & MS_RLSD_ON) pMuxChn->m_ModuleState.DCD = 1;
    if (dwState & MS_DTR_ON)  pMuxChn->m_ModuleState.DTR = 1;
    if (dwState & MS_RTS_ON)  pMuxChn->m_ModuleState.RTS = 1;
    if (MEMCMP(&pMuxChn->m_ModuleState, &OldModuleState, sizeof(MUX_V24STATUS_t))) {
      // Send new state to module via workqueue to avoid problems with
      // critical section of mux protocol handling
      oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueModemSignal);
    }
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Clear the current modem state with the modem control signals.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
// dwState: The new state to be cleared.
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_ClearModemState(PT_MUXCHAN pMuxChn, DWORD dwState) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, State=0x%x, Dlci=%d", pMuxChn, dwState, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    MUX_V24STATUS_t OldModuleState;
    OldModuleState = pMuxChn->m_ModuleState;
    if (dwState & MS_CTS_ON)  pMuxChn->m_ModuleState.CTS = 0;
    if (dwState & MS_DSR_ON)  pMuxChn->m_ModuleState.DSR = 0;
    if (dwState & MS_RING_ON) pMuxChn->m_ModuleState.RI  = 0;
    if (dwState & MS_RLSD_ON) pMuxChn->m_ModuleState.DCD = 0;
    if (dwState & MS_DTR_ON)  pMuxChn->m_ModuleState.DTR = 0;
    if (dwState & MS_RTS_ON)  pMuxChn->m_ModuleState.RTS = 0;
    if (MEMCMP(&pMuxChn->m_ModuleState, &OldModuleState, sizeof(MUX_V24STATUS_t))) {
      // Send new state to module via workqueue to avoid problems with
      // critical section of mux protocol handling
      oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueModemSignal);
    }
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get the current number of bytes in and sizes of send and receive buffers.
//
// Parameters:
// pMuxChn     : Pointer to the corresponding mux channel object.
// pdwBytesInRx: Pointer to DWORD receiving number of bytes in receive
//               buffer (NULL if not required).
// pdwBytesInTx: Pointer to DWORD receiving number of bytes in transmit
//               buffer (NULL if not required).
// pdwRxSize   : Pointer to DWORD receiving size of receive buffer
//               (NULL if not required).
// pdwRxSize   : Pointer to DWORD receiving size of transmit buffer
//               (NULL if not required).
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_GetBufferInfo(PT_MUXCHAN pMuxChn, PDWORD pdwBytesInRx, PDWORD pdwBytesInTx, PDWORD pdwRxSize, PDWORD pdwTxSize) {
  int   iRet;
  DWORD dwRxBytes = 0, dwTxBytes = 0, dwRxSize = 0, dwTxSize = 0;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, pRxBytes=%p, pTxBytes=%p, pRxSize=%p, pTxSize=%p, Dlci=%d", pMuxChn, pdwBytesInRx, pdwBytesInTx, pdwRxSize, pdwTxSize, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    dwRxBytes = pMuxChn->m_dwBytesInRx;
    dwRxSize  = pMuxChn->m_dwSizeOfRx;
    dwTxBytes = ringbuf_GetNumWaitingBytes(&pMuxChn->m_WriteRingBuf);
    dwTxSize  = ringbuf_GetSize(&pMuxChn->m_WriteRingBuf);

    if (pdwRxSize)    *pdwRxSize    = dwRxSize;
    if (pdwBytesInRx) *pdwBytesInRx = dwRxBytes;
    if (pdwTxSize)    *pdwTxSize    = dwTxSize;
    if (pdwBytesInTx) *pdwBytesInTx = dwTxBytes;
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, RxBytes=%d, TxBytes=%d, RxSize=%d, TxSize=%d, MuxChn=%p, Dlci=%d", iRet, dwRxBytes, dwTxBytes, dwRxSize, dwTxSize, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Clear all unhandled data from the internal buffers.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
// fRx    : Clear receive buffer.
// fTx    : Clear transmit buffer.
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_ClearBuffers(PT_MUXCHAN pMuxChn, BOOL fRx, BOOL fTx) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Rx=%s, Tx=%s, Dlci=%d", pMuxChn, STRBOOL(fRx), STRBOOL(fTx), pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    pMuxChn->m_fRxClear = fRx;
    pMuxChn->m_fTxClear = fTx;
    if (fRx || fTx) {
      if (fRx) {
        pMuxChn->m_dwBytesInRx = 0;
      }
      if (fTx) {
        ringbuf_Clear(&pMuxChn->m_WriteRingBuf);
        oswrap_SetEvent(&pMuxChn->m_hTxEmptyEvent);
        oswrap_SetEvent(&pMuxChn->m_hTxSpaceEvent);
      }
      oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueClearBuffers);
    }
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Tell mux protocol implementation that it can continue to deliver received
// data.
//
// Parameters:
// pMuxChn: Pointer to the corresponding mux channel object.
//
// Return:
// 0 on success, a negative value in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
int mc_ContinueReceive(PT_MUXCHAN pMuxChn) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Dlci=%d", pMuxChn, pMuxChn->m_dwDlci);
  oswrap_SpinLock(&pMuxChn->m_slLock);
  iRet = mc_GetErrCodeOnStateUnlocked(pMuxChn);
  if (iRet == 0) {
    oswrap_WorkQueueQueueItem(pMuxChn->m_hWorkQueue, &pMuxChn->m_WorkQueueContinueReceive);
  }
  oswrap_SpinUnlock(&pMuxChn->m_slLock);
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Wait until there is space for at least 1 byte in the transmit buffer.
//
// Parameters:
// pMuxChn  : Pointer to the corresponding mux channel object.
// dwTimeout: The timeout in milliseconds.
//
// Return:
// 0 in case of succes, a negative value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int mc_WaitForTxSpace(PT_MUXCHAN pMuxChn, DWORD dwTimeout) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Timeout=%d, Dlci=%d", pMuxChn, dwTimeout, pMuxChn->m_dwDlci);
  iRet = oswrap_WaitForEvent(&pMuxChn->m_hTxSpaceEvent, dwTimeout);
  if (iRet == 0) {
    iRet = mc_GetErrCodeOnState(pMuxChn);
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Wait until the transmit buffer is empty.
//
// Parameters:
// pMuxChn  : Pointer to the corresponding mux channel object.
// dwTimeout: The timeout in milliseconds.
//
// Return:
// 0 in case of succes, a negative value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int mc_WaitForTxEmpty(PT_MUXCHAN pMuxChn, DWORD dwTimeout) {
  int iRet;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Timeout=%d, Dlci=%d", pMuxChn, dwTimeout, pMuxChn->m_dwDlci);
  iRet = oswrap_WaitForEvent(&pMuxChn->m_hTxEmptyEvent, dwTimeout);
  if (iRet == 0) {
    iRet = mc_GetErrCodeOnState(pMuxChn);
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Wait until a change of modem signals occur.
//
// Parameters:
// pMuxChn  : Pointer to the corresponding mux channel object.
// dwSignals: The signals we're waiting for to change.
//
// Return:
// 0 in case of succes, a negative value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int mc_WaitForCommEvent(PT_MUXCHAN pMuxChn, DWORD dwSignals) {
  int   iRet;
  DWORD dwOldState, dwNewState;
  DBG_ENTER(ZONE_FCT_MUXCHAN, "MuxChn=%p, Signals=0x%x, Dlci=%d", pMuxChn, dwSignals, pMuxChn->m_dwDlci);
  iRet = mc_GetModemState(pMuxChn, &dwOldState);
  if (iRet == 0) {
    do {
      iRet = oswrap_WaitForEvent(&pMuxChn->m_hCommEvent, SEM_WAIT_INFINITE);
      if (iRet == 0) {
        iRet = mc_GetModemState(pMuxChn, &dwNewState);
      }
    } while ((iRet == 0) && (((dwOldState ^ dwNewState) & dwSignals) == 0));
  }
  DBG_LEAVE(ZONE_FCT_MUXCHAN, "Ret=%i, MuxChn=%p, Dlci=%d", iRet, pMuxChn, pMuxChn->m_dwDlci);
  return iRet;
}
