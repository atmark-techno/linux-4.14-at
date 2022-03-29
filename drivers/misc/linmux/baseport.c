/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Baseport.c
//
// This file contains the implementation of the BasePort object handling
// access to the module via the hardware base port, it's initialization and
// deintialization as well as the management of the opening and closing of
// virtual ports.
//
//////////////////////////////////////////////////////////////////////////////

#include "muxdbg.h"
#include "muxdrv.h"

#include "baseport.h"

//////////////////////////////////////////////////////////////////////////////

PT_BASEPORT *pMuxBasePorts = NULL;

// from linmuxcfg.c
extern unsigned long gInstances;

//////////////////////////////////////////////////////////////////////////////

static BOOL bp_RestartMuxHandling(PT_BASEPORT pBasePort);
static BOOL bp_IsModulePowerOn(PT_BASEPORT pBasePort);
static BOOL bp_PowerModuleOff(PT_BASEPORT pBasePort);

/****************************************************************************/
/* mux interface                                                            */
/****************************************************************************/
static void* MuxGetMem(DWORD dwUserData, DWORD dwSize);
static void  MuxFreeMem(DWORD dwUserData, PVOID pMem);
static void  MuxStartResult(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwResult);
static void  MuxMsgAvailable(DWORD dwMuxInstance, DWORD dwUserData);
static void  MuxReset(DWORD dwMuxInstance, DWORD dwUserData);
static void  MuxLock(DWORD dwMuxInstance, DWORD dwUserData);
static void  MuxUnlock(DWORD dwMuxInstance, DWORD dwUserData);
static DWORD MuxDevSendData(DWORD dwMuxInstance, DWORD dwUserData, PBYTE pData, DWORD dwDataLen);
static void  MuxDevActivate(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwLen);
static DWORD MuxDevGetFreeTxBytes(DWORD dwMuxInstance, DWORD dwUserData);
static void  MuxDevSetQueueSizes(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwRxSize, DWORD dwTxSize);

/****************************************************************************/
/* baseport thread and work queue functions                                 */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Read data from port and feed it into the mux protocol handling.
//
// Parameters:
// data: Pointer to the corresponding base port object.
//
// Return:
// true indicates that the thread should continue false that it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
static bool bp_ThreadReadData(void *data) {
  PT_BASEPORT  pBasePort = (PT_BASEPORT)data;
  int          iRead;
  bool         fRet = true;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);

  // Read all data from port until we're stopped
  iRead = oswrap_TtyRead(pBasePort->m_hPort, pBasePort->m_bReadBuf, READ_BUFFER_SIZE);
  if (iRead > 0) {
    DBGPRINT(ZONE_FCT_BASEPORT, "bp_ThreadReadData(): Read %d bytes", iRead);
    // Feed the read data into the mux protocol handling
    if (pBasePort->m_pMuxInstance) {
      DWORD dwPut = (DWORD)iRead;
      Dev_ReceiveData((DWORD)pBasePort->m_pMuxInstance, pBasePort->m_bReadBuf, &dwPut);
    }
    pBasePort->m_dwLastRead = (DWORD)iRead;
  } else if (iRead == 0) {
    // Nothing read. This has been seen on some systems when we're running on an usb port
    // and the module had been switched off during mux operation. Because we cannot be sure
    // we wait some milliseconds and let the module shutdown detection do its work.
    DBGPRINT(ZONE_GENERAL_INFO, "bp_ThreadReadData(): Read 0 bytes");
    oswrap_Sleep(10);
  } else if (iRead == -EAGAIN) {
    // We're asked to read again. We do this after a short delay.
    oswrap_Sleep(0);
  } else if (iRead == -ERESTARTSYS) {
    // This happens during system suspend or when the thread has been stopped.
    // Let the thread handling check this.
  } else {
    // iRead < 0
    DBGPRINT(ZONE_GENERAL_INFO, "bp_ThreadReadData(): Error from oswrap_TtyRead(): %d", iRead);
    DBGPRINT(ZONE_MODULE_EXIT, "bp_ThreadReadData(): Baseport error, module went off!");
    oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
    if (pBasePort->m_dwLastRead) {
      DBGPRINT(ZONE_MODULE_EXIT, "bp_ThreadReadData(): Last received %d bytes:", pBasePort->m_dwLastRead);
      MUXDBGHEX(ZONE_MODULE_EXIT, pBasePort->m_bReadBuf, pBasePort->m_dwLastRead);
    }
    pBasePort->m_State = BASEPORT_KILLED;
    oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
    oswrap_WorkQueueQueueItem(pBasePort->m_hWorkQueue, &pBasePort->m_WorkQueueShutDown);
    fRet = false;
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Thread which will be pumping data from Linmux instance's tx buffer into
// the underlying serial port.
//
// Parameters:
// data: Pointer to the corresponding base port object.
//
// Return:
// true indicates that the thread should continue false that it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
static bool bp_ThreadWriteData(void *data) {
  PT_BASEPORT pBasePort = (PT_BASEPORT)data;
  bool fRet = true;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);

  // Wait for data in the ringbuffer
  if (ringbuf_WaitForData(&pBasePort->m_WriteRingBuf)) {
    // Loop to read out all the data from m_WriteRingBuf through m_bWriteBuf
    int iRead = 1;
    while (fRet && (iRead > 0) && !oswrap_ShouldStopThread()) {
      iRead = ringbuf_GetBytes(&pBasePort->m_WriteRingBuf, pBasePort->m_bWriteBuf, WRITE_BUFFER_SIZE);
      DBGPRINT(ZONE_FCT_BASEPORT, "bp_ThreadWriteData(): %d bytes read from m_WriteRingBuf", iRead);

      bp_RestartMuxHandling(pBasePort);

      if (iRead > 0) {
        // Inner loop to write data from m_bWriteBuf to the baseport
        int iErrCnt = 0;
        int iWritten = 0;
        while (fRet && (iWritten < iRead) && !oswrap_ShouldStopThread()) {
          int iResult = oswrap_TtyWrite(pBasePort->m_hPort, pBasePort->m_bWriteBuf + iWritten, iRead - iWritten);
          if (iResult > 0) {
            // Write success, some data written to the baseport
            DBGPRINT(ZONE_FCT_BASEPORT, "bp_ThreadWriteData(): Written %d bytes", iResult);
            iWritten += iResult;
            iErrCnt = 0;
          } else if (iResult == 0) {
            DBGPRINT(ZONE_FCT_BASEPORT, "bp_ThreadWriteData(): Written 0 bytes");
            // Tty write buffer is full -> Sleep a short interval to allow some data to be sent out to the device
            oswrap_Sleep(1);
            // This is not necessarily an error, we're increasing the error counter nevertheless to detect a stalled baseport
            iErrCnt++;
          } else {
            DBGPRINT(ZONE_ERR, "bp_ThreadWriteData(): Error from oswrap_TtyWrite(): %d", iResult);
            if ((iResult == -ENODEV) || (iResult == -EINVAL)) {
              // Treat these as "hard" errors
              fRet = false;
            } else {
              // And the rest as "soft" errors
              oswrap_Sleep(1);
              iErrCnt++;
            }
          }

          if (iErrCnt > 1000) {
            // Do not allow more than 1000 soft errors. With the sleep of 1ms in case of having written no data to
            // the baseport this means that the thread is not spinning around longer than 1s in case of problems.
            DBGPRINT(ZONE_ERR, "bp_ThreadWriteData(): Too many baseport soft errors!");
            fRet = false;
          }
          if (fRet == false) {
            DBGPRINT(ZONE_ERR, "bp_ThreadWriteData(): Baseport non-recoverable error!");
            DBGPRINT(ZONE_MODULE_EXIT, "bp_ThreadWriteData(): Baseport error, module went off!");
            oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
            pBasePort->m_State = BASEPORT_KILLED;
            oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
            oswrap_WorkQueueQueueItem(pBasePort->m_hWorkQueue, &pBasePort->m_WorkQueueShutDown);
          }
        }
      }
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Keep the message loop from the mux protocol handling alive.
//
// Parameters:
// data: Pointer to the corresponding base port object.
//
// Return:
// true indicates that the thread should continue false that it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
static bool bp_ThreadMuxMsg(void *data) {
  PT_BASEPORT pBasePort = (PT_BASEPORT)data;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (oswrap_WaitForSemaphore(&pBasePort->m_hMuxMsgSemaphore, SEM_WAIT_INFINITE) == EVT_SIGNALED) {
    if (pBasePort->m_pMuxInstance) {
      Mux_MsgTick((DWORD)pBasePort->m_pMuxInstance);
    }
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=true, Port=%p", pBasePort);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Provide timebase for multiplex protocol handling.
//
// Parameters:
// data: Pointer to the corresponding base port object.
//
// Return:
// true indicates that the thread should continue false that it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
static bool bp_ThreadMuxTimebase(void *data) {
  PT_BASEPORT pBasePort = (PT_BASEPORT)data;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (pBasePort->m_pMuxInstance) {
    Mux_TimerTick((DWORD)pBasePort->m_pMuxInstance);
    oswrap_Sleep(TIMEBASE_INTERVAL);
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=true, Port=%p", pBasePort);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Monitor the DSR signal to detect unexpected module shutdown.
//
// Parameters:
// lpParameter: Pointer to the corresponding base port object.
//
// Return:
// true indicates that the thread should continue false that it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
static bool bp_ThreadIsAlive(void *data) {
  PT_BASEPORT  pBasePort = (PT_BASEPORT)data;
  bool         fRet = true;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (oswrap_TtyWaitChangeModemLine(pBasePort->m_hPort, EV_DSR) == 0) {
    if (!oswrap_ShouldStopThread()) {
      // Check if we lost the DSR signal and together with it the module
      if (!bp_IsModulePowerOn(pBasePort)) {
        // We lost the DSR signal which can only mean that the module went off!
        oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
        pBasePort->m_State = BASEPORT_KILLED;
        oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
        DBGPRINT(ZONE_MODULE_EXIT, "Unexpected module shutdown detected!");
        // Shutdown mux protocol handling
        oswrap_WorkQueueQueueItem(pBasePort->m_hWorkQueue, &pBasePort->m_WorkQueueShutDown);
        fRet = false;
      }
    }
  } else {
    if (!oswrap_ShouldStopThread()) {
      // TIOCMIWAIT doesn't seem to be implemented in the hw driver -> we have to poll
      oswrap_Sleep(250);
      if (!oswrap_ShouldStopThread()) {
        // Check if we lost the DSR signal and together with it the module
        if (!bp_IsModulePowerOn(pBasePort)) {
          // We lost the DSR signal which can only mean that the module went off!
          oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
          pBasePort->m_State = BASEPORT_KILLED;
          oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
          DBGPRINT(ZONE_MODULE_EXIT, "Unexpected module shutdown detected!");
          // Shutdown mux protocol handling
          oswrap_WorkQueueQueueItem(pBasePort->m_hWorkQueue, &pBasePort->m_WorkQueueShutDown);
          fRet = false;
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Wake up a possibly sleeping module if module power saving is used and
// the multiplexer is running on a serial interface.
//
// Parameters:
// data: Pointer to the corresponding base port object.
//
// Return:
// true indicates that the thread should continue false that it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
static bool bp_ThreadWakeModule(void *data) {
  PT_BASEPORT pBasePort = (PT_BASEPORT)data;
  BOOL fRet = pBasePort->m_fUsbPort ? FALSE : TRUE;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (fRet) {
    if (!oswrap_WaitForEvent(&pBasePort->m_hWakeModuleEvent, SEM_WAIT_INFINITE)) {
      DWORD dwMask;
      if (oswrap_TtyGetModemLines(pBasePort->m_hPort, &dwMask) == 0) {
        if (!(dwMask & MS_CTS_ON)) {
          DBGPRINT(ZONE_FCT_POWER_MGM, "Wake up possibly sleeping module by RTS & DTR toggle");
          oswrap_TtyToggleRtsDtr(pBasePort->m_hPort, 1);
        }
      }
      oswrap_Sleep(100);
    }
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

/****************************************************************************/
/* internal functions base port                                             */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Stop the running threads.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void bp_StopThreads(PT_BASEPORT pBasePort) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  oswrap_StopThread(pBasePort->m_hWakeModuleThread);
  oswrap_StopThread(pBasePort->m_hMuxTimebaseThread);
  oswrap_StopThread(pBasePort->m_hMuxMsgThread);
  oswrap_StopThread(pBasePort->m_hIsAliveThread);
  oswrap_StopThread(pBasePort->m_hWriteThread);
  oswrap_StopThread(pBasePort->m_hReadThread);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
}

//////////////////////////////////////////////////////////////////////////////
//
// Start the required threads.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE on success, FALSE otherwise.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_StartThreads(PT_BASEPORT pBasePort) {
  BOOL fRet = TRUE;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (!oswrap_StartThread(pBasePort->m_hReadThread)           ||
      !oswrap_StartThread(pBasePort->m_hWriteThread)          ||
      !oswrap_StartThread(pBasePort->m_hIsAliveThread)        ||
      !oswrap_StartThread(pBasePort->m_hMuxTimebaseThread)    ||
      !oswrap_StartThread(pBasePort->m_hMuxMsgThread)         ||
      !oswrap_StartThread(pBasePort->m_hWakeModuleThread)) {
    DBGPRINT(ZONE_ERR, "Failed to start required threads");
    bp_StopThreads(pBasePort);
    fRet = FALSE;
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Restart the multiplex protocol handling if required based on the given low
// watermark for the write ringbuffer.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE if the multiplex protocol handling was restarted, FALSE otherwise.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_RestartMuxHandling(PT_BASEPORT pBasePort) {
  BOOL   fRet = FALSE;
  DWORD  dwLenRestart;
  UINT32 uiBytesInBuffer;

  // Check if the mux message handling must be restarted
  oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
  uiBytesInBuffer = ringbuf_GetNumWaitingBytes(&pBasePort->m_WriteRingBuf);
  dwLenRestart = pBasePort->m_dwLenRestart;
  if (dwLenRestart >= uiBytesInBuffer) {
    pBasePort->m_dwLenRestart = -1;
  }
  oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
  if ((dwLenRestart != -1) && (dwLenRestart >= uiBytesInBuffer)) {
    // Data in ringbuffer has reached the restart level -> restart the mux handling
    DBGPRINT(ZONE_FCT_BASEPORT, "bp_ThreadWriteData(): Tx queue has reached %lu of %lu bytes -> restart mux handling", uiBytesInBuffer, dwLenRestart);
    Dev_SendDataContinue((DWORD)pBasePort->m_pMuxInstance, uiBytesInBuffer);
    fRet = TRUE;
  }
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Close the hardware base port. This must be protected by the critical
// section of the multiplex protocol to avoid port access via the mux
// interface after or while the port is closing.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void bp_ClosePort(PT_BASEPORT pBasePort) {
  PORTHANDLE hPort;
  oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
  hPort = pBasePort->m_hPort;
  pBasePort->m_hPort = NULL;
  oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
  oswrap_TtyClose(hPort);
}

//////////////////////////////////////////////////////////////////////////////
//
// Emergency shutdown of multiplex handling. This must be done as a work
// queue call to avoid deadlocks.
//
// Parameters:
// pData: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void bp_ShutDownDeferred(void *pData) {
  PT_BASEPORT pBasePort = (PT_BASEPORT)pData;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  Mux_Shutdown((DWORD)pBasePort->m_pMuxInstance);
  bp_StopThreads(pBasePort);
  bp_ClosePort(pBasePort);
  bp_PowerModuleOff(pBasePort);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
}

//////////////////////////////////////////////////////////////////////////////
//
// Check if a given module response is part of a given response array.
//
// Parameters:
// strRsp : The response to be checked.
// strRsps: Array with the expected responses. This array must be finished
//          with an empty string.
//
// Return:
// TRUE if the given response was found, FALSE otherwise.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_CheckATRsp(const char* strRsp, const char** strRsps) {
  DWORD i;
  for (i = 0; strlen(strRsps[i]); i++) {
    if (strstr(strRsp, strRsps[i])) {
      return TRUE;
    }
  }
  return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
// Send an AT command to the COM port and check response for OK or ERROR.
//
// Parameters:
// pBasePort   : The reference to the corresponding base port object.
// pszATCommand: The AT command to send.
// dwTimeout   : Timeout for the command in ms.
// dwRetries   : Number of retries.
// szRsp       : External response buffer, only required if the response
//               is needed outside this function.
// dwRspLen    : Length of external response buffer.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_SendATCmd(PT_BASEPORT  pBasePort,
                         char        *szATCommand,
                         DWORD        dwTimeout,
                         DWORD        dwRetries,
                         char        *szRsp,
                         DWORD        dwRspLen) {
  BOOL         fRet = FALSE;
  BOOL         fAbort = FALSE;
  const char*  szOkRsps[] = { "OK\r\n", "" };
  const char*  szErrorRsps[] = { "ERROR\r\n", "+CME ERROR: 21\r\n", "" };
  char         szResponse[128];
  char*        pszResponse;
  char*        pRsp;
  int          iRead, iWritten;
  DWORD64      dw64StartTime;
  DWORD        dwTimeLeft = dwTimeout;
  DWORD        dwRspLength;
  DWORD        i;

  DBG_ENTER(ZONE_FCT_BASEPORT, "port=%p, Cmd=%s, Timeout=%d, Retries=%d, Rsp=%s, RspLen=%d", pBasePort, szATCommand, dwTimeout, dwRetries, STRPTR(szRsp), dwRspLen);

  if (!strlen(szATCommand)) {
    // Nothing to send
    fRet = TRUE;
  } else {
    for (i = 0; (i <= dwRetries) && !fAbort; i++) {
      if (i) {
        oswrap_Sleep(100);
      }

      // Check if module is still alive
      if (!bp_IsModulePowerOn(pBasePort)) {
        DBGPRINT(ZONE_GENERAL_INFO, "Module shutdown during initialization detected!");
        break;
      }

      MUXDBGCRLF(ZONE_AT_CMDS, "Sending AT command: ", szATCommand);

      if (!pBasePort->m_fUsbPort) {
        // Wake module from possible sleep mode
        oswrap_TtyToggleRtsDtr(pBasePort->m_hPort, 1);
      }

      iWritten = oswrap_TtyWrite(pBasePort->m_hPort, szATCommand, strlen(szATCommand));
      if (iWritten < strlen(szATCommand)) {
        DBGPRINT(ZONE_AT_CMDS, "oswrap_TtyWrite() returned %d, %d bytes should be written!", iWritten, strlen(szATCommand));
        break;
      }

      // Init and clear the response buffer
      if (szRsp && dwRspLen) {
        dwRspLength = dwRspLen - 1;
        pszResponse = szRsp;
      } else {
        dwRspLength = (sizeof(szResponse) / sizeof(char)) - 1;
        pszResponse = szResponse;
      }
      pRsp = pszResponse;
      MEMCLR(pszResponse, (dwRspLength + 1) * sizeof(char));

      dw64StartTime = oswrap_GetTickCount();
      while (!fAbort) {
        // Check if module is still alive
        if (!bp_IsModulePowerOn(pBasePort)) {
          DBGPRINT(ZONE_GENERAL_INFO, "Module shutdown during initialization detected!");
          fAbort = TRUE;
          break;
        }

        // Wait for some data
        iRead = oswrap_TtyReadTimeout(pBasePort->m_hPort, pszResponse, dwRspLength, dwTimeLeft);
        if (iRead < 0) {
          DBGPRINT(ZONE_AT_CMDS, "oswrap_TtyReadTimeout() returned %d", iRead);
          break;
        }

        // Check if we have received some data
        if (iRead > 0) {
          // Scan for possible responses
          if (bp_CheckATRsp(pRsp, szOkRsps)) {
            MUXDBGCRLF(ZONE_AT_CMDS, "Received OK response: ", pRsp);
            fRet = TRUE;
            fAbort = TRUE;
            break;
          } else if (bp_CheckATRsp(pRsp, szErrorRsps)) {
            MUXDBGCRLF(ZONE_AT_CMDS, "Received ERROR response: ", pRsp);
            fAbort = TRUE;
            break;
          }
          pszResponse += iRead;
          dwRspLength -= iRead;
          if (!dwRspLength) {
            DBGPRINT(ZONE_AT_CMDS, "Response buffer overflow!");
            break;
          }
        }
        oswrap_Sleep(100);

        dwTimeLeft = (DWORD)(oswrap_GetTickCount() - dw64StartTime);
        if (dwTimeLeft >= dwTimeout) {
          DBGPRINT(ZONE_AT_CMDS, "AT command timeout");
          break;
        }
        dwTimeLeft = dwTimeout - dwTimeLeft;
      }
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Synchronize the port's baud rate setting with the module.
//
// Params:
// pBasePort : The reference to the corresponding base port object.
// dwBaudRate: The baud rate to use.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_SyncBaudRate(PT_BASEPORT pBasePort, DWORD dwBaudRate) {
  DWORD  dwBaudRates[] = BAUDRATES_TO_SCAN;
  DWORD  dwNumOfBaudRates = sizeof(dwBaudRates) / sizeof(dwBaudRates[0]);
  DWORD  i, k, r;
  char   szBaudCmd[32];
  BOOL   fRet = FALSE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p, Baudrate=%d", pBasePort, dwBaudRate);

  // First try to set the new baud rate. If that fails we must exit.
  // Otherwise the module would be set to a baudrate the system
  // doesn't support and we might be lost forever.
  if (!oswrap_TtySetParams(pBasePort->m_hPort, dwBaudRate)) {
    DBGPRINT(ZONE_ERR, "Baudrate %d not supported by the system", dwBaudRate);
  } else {
    // Fetch the required baud rate
    dwBaudRates[0] = dwBaudRate;

    // Construct the command
    sprintf(szBaudCmd, "AT+IPR=%lu\r", dwBaudRate);

    // Scan through the list of baud rates and attempt to fix the modem to the required rate
    i = k = 0;
    r = 1;
    do {
      if ((i == 0) || (dwBaudRates[i] != dwBaudRates[0])) {
        // Try the new baud rate
        if (oswrap_TtySetParams(pBasePort->m_hPort, dwBaudRates[i])) {
          // Send the command to fix the baudrate
          if (bp_SendATCmd(pBasePort, szBaudCmd, TIMEOUT_MODULE_STD, r, NULL, 0)) {
            // If we get an intelligable response we can leave the loop as we have managed to set the baudrate
            break;
          }
        }
      }

      // Prepare next baud rate
      i++;
      if (i >= dwNumOfBaudRates) {
        // If we're at the end of our baud rate list restart the scan
        i = 0;
        k++;
      }
      r = 0;
    } while(k < BAUDRATES_RETRIES);

    if (k < BAUDRATES_RETRIES) {
      fRet = TRUE;

      // Set the new baud rate
      oswrap_TtySetParams(pBasePort->m_hPort, dwBaudRate);

      // Give system some time for the baud rate switch
      if (dwBaudRates[i] != dwBaudRate) {
        oswrap_Sleep(1000);
      }
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);

  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Switch module on and wait until it has started.
// On DSB development boards the module's ignition pin is connected
// with the COM DTR signal and there is no specific circuit to physically
// switch the power supply on or off.
// If your hardware wiring requires special function calls for powering the
// module on the code in this function has to be expanded or replaced with
// the hardware dependant code.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_PowerModuleOn(PT_BASEPORT pBasePort) {
  DWORD dwMask;
  BOOL  fRet = TRUE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  // First check if the module isn't already running
  oswrap_TtyGetModemLines(pBasePort->m_hPort, &dwMask);
  if (!(dwMask & MS_DSR_ON)) {
    DWORD64 dw64StartTime;
    // On DSB boards iginition is connected with DTR, toggling switches on the module
    oswrap_TtySetModemLines(pBasePort->m_hPort, dwMask & ~MS_DTR_ON);
    oswrap_Sleep(200);
    oswrap_TtySetModemLines(pBasePort->m_hPort, dwMask | MS_DTR_ON);

    // Now wait for the module to start
    dw64StartTime = oswrap_GetTickCount();

    // Initial delay to give signals some time to settle down.
    // This is usually 500ms for most modules. Although there are some slow
    // booting modules which require up to 9 seconds to settle their modem signals.
    // If such slow booting module isn't used this delay can be reduced to 500ms
    // to speed up mux initialization significantly.
    if (!pBasePort->m_fUsbPort) {
      oswrap_Sleep(pBasePort->m_dwStartDelay);
    }

    // Now wait for the CTS signal to become signaled
    fRet = FALSE;
    while (TRUE) {
      if (oswrap_TtyGetModemLines(pBasePort->m_hPort, &dwMask)) {
        break;
      }
      if (dwMask & MS_CTS_ON) {
        // Active CTS signal detected -> module is on now
        fRet = TRUE;
        // Final delay to give module last chance to settle things
        oswrap_Sleep(200);
        break;
      }
      if ((oswrap_GetTickCount() - dw64StartTime) >= TIMEOUT_MODULE_START) {
        // We timed out
        break;
      }
      oswrap_Sleep(1);
    }
    if (!pBasePort->m_fUsbPort) {
      // Ensure that DTR is disabled on tty port
      oswrap_TtySetModemLines(pBasePort->m_hPort, dwMask & ~MS_DTR_ON);
    }
  } else {
    // Ensure that DTR is disabled on tty port and enabled on USB port
    if (!pBasePort->m_fUsbPort) {
      oswrap_TtySetModemLines(pBasePort->m_hPort, dwMask & ~MS_DTR_ON);
    } else {
      oswrap_TtySetModemLines(pBasePort->m_hPort, dwMask | MS_DTR_ON);
    }
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Switch off module power.
// If your hardware contains special circuits to physically switch off the
// module's power supply this hardware dependant code has to be added to this
// function.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_PowerModuleOff(PT_BASEPORT pBasePort) {
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
//
// Switch off module and wait until it has shut down.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_SwitchModuleOff(PT_BASEPORT pBasePort) {
  BOOL fRet = TRUE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);

  if (bp_IsModulePowerOn(pBasePort)) {
    // Send the switch off command
    if (!bp_SendATCmd(pBasePort, ATCMD_SHUTDOWN, 3000, 1, NULL, 0)) {
      fRet = FALSE;
    } else {
      // Now wait for the CTS signal to become low
      DWORD64 dw64StartTime = oswrap_GetTickCount();
      while (TRUE) {
        DWORD dwMask;
        // We use oswrap_TtyIoctl() instead of oswrap_TtyGetModemLines() because the
        // ioctl call returns an error state in case an usb device has vanished in
        // difference to a direct driver call in oswrap_TtyGetModemLines().
        if (oswrap_TtyIoctl(pBasePort->m_hPort, TIOCMGET, (unsigned long)&dwMask)) {
          // If we fail to read the modem lines the USB device has vanished which
          // means that the module properly went off.
          oswrap_Sleep(100); // Give OS some time to tidy up USB stuff
          break;
        }
        if (!(dwMask & MS_CTS_ON)) {
          // Inactive CTS signal detected -> module is off now
          oswrap_Sleep(pBasePort->m_dwStartDelay); // Give module enough time to finish shutdown cycle
          break;
        }
        if ((oswrap_GetTickCount() - dw64StartTime) >= TIMEOUT_MODULE_STOP) {
          // We timed out
          fRet = FALSE;
          break;
        }
        oswrap_Sleep(50);
      }
    }
  }

  // Physically switch power off
  if (!bp_PowerModuleOff(pBasePort)) {
    fRet = FALSE;
  }

  pBasePort->m_dw64TimeLastOff = oswrap_GetTickCount();

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);

  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Check if the connected module is powered on.
// Currently the DSR signal on the hardware port is used to check if the
// module is powered on or off. If the DSR signal cannot be used for that
// module life check for any reason (e.g. because the hardware DSR signal
// isn't connected) you need to modify that function to reliably detect if
// the module is on or off. Additionally you might need to add a thread
// detecting a module power loss and notifying the rest of the driver as it
// done in "CBasePort::ThreadReadData()".
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE if the connected module is powerd on, FALSE otherwise.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_IsModulePowerOn(PT_BASEPORT pBasePort) {
  // Check if we have been switched off externally
  oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
  if (pBasePort->m_State == BASEPORT_KILLED) {
    oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
    DBGPRINT(ZONE_MODULE_EXIT, "bp_IsModulePowerOn() -> External shutdown");
    return FALSE;
  }
  oswrap_CriticalSectionLeave(&pBasePort->m_csLock);

  // Check if DSR monitoring is enabled
  if (pBasePort->m_fMonitorDsr) {
    // Check if module is still alive
    DWORD dwStatus = 0;
    // We use oswrap_TtyIoctl() instead of oswrap_TtyGetModemLines() because the
    // ioctl call returns an error state in case an usb device has vanished in
    // difference to a direct driver call in oswrap_TtyGetModemLines().
    int iRet = oswrap_TtyIoctl(pBasePort->m_hPort, TIOCMGET, (unsigned long)&dwStatus);
    if ((iRet != 0) && (iRet != -EAGAIN)) {
      DBGPRINT(ZONE_MODULE_EXIT, "bp_IsModulePowerOn() -> Module shutdown detected, oswrap_TtyIoctl() failed: %d", iRet);
      return FALSE;
    } else if (!pBasePort->m_fUsbPort && !(dwStatus & MS_DSR_ON)) {
      DBGPRINT(ZONE_MODULE_EXIT, "bp_IsModulePowerOn() -> Module shutdown detected, State: 0x%x", dwStatus);
      return FALSE;
    }
  }

  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
//
// Stop the multiplex mode.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_StopMuxMode(PT_BASEPORT pBasePort) {
  BOOL fRet = TRUE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (bp_IsModulePowerOn(pBasePort)) {
    // Close the mux protocol
    oswrap_ResetEvent(&pBasePort->m_hStartStopEvent);
    Mux_Stop((DWORD)pBasePort->m_pMuxInstance);

    // Wait for the mux protocol to be stopped
    if (oswrap_WaitForEvent(&pBasePort->m_hStartStopEvent, 20000) != 0) {
      // We timed out
      fRet = FALSE;
    }
    // From here we cannot use DSR to monitor if module is alive because we don't know
    // if somebody has sent AT&S1 on the first mux channel which is used on the hardware
    // port as well after closing mux mode.
    pBasePort->m_fMonitorDsr = FALSE;
  }

  // Finally stop the running threads
  bp_StopThreads(pBasePort);

  // Deregister from mux protocol handling
  Mux_Deregister((DWORD)pBasePort->m_pMuxInstance);
  pBasePort->m_pMuxInstance = NULL;

  oswrap_TtyFlushBuffers(pBasePort->m_hPort, TRUE, TRUE);

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Send the AT command for starting the multiplex mode.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_SendMuxStartCmd(PT_BASEPORT pBasePort) {
  BOOL fRet = FALSE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (!bp_SendATCmd(pBasePort, ATCMD_START_MUX, 3000, 0, NULL, 0)) {
    // On some modules the mux start fails immediatly after module start.
    // For these cases we try again after a sufficient delay.
    oswrap_Sleep(2000);
    if (bp_SendATCmd(pBasePort, ATCMD_START_MUX, 3000, 0, NULL, 0)) {
      fRet = TRUE;
    }
  } else {
    fRet = TRUE;
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Start the multiplex mode.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_StartMuxMode(PT_BASEPORT pBasePort) {
  BOOL fRet = FALSE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);

  // Init the mux protocol handling
  if (!pBasePort->m_pMuxInstance) {
    MUX_INTERFACE_t CallBacks;
    MEMCLR(&CallBacks, sizeof(MUX_INTERFACE_t));
    CallBacks.pMuxGetMem             = MuxGetMem;
    CallBacks.pMuxFreeMem            = MuxFreeMem;
    CallBacks.pMuxStartResult        = MuxStartResult;
    CallBacks.pMuxMsgAvailable       = MuxMsgAvailable;
    CallBacks.pMuxReset              = MuxReset;
    CallBacks.pMuxLock               = MuxLock;
    CallBacks.pMuxUnlock             = MuxUnlock;
    CallBacks.pMuxDevSendData        = MuxDevSendData;
    CallBacks.pMuxDevActivate        = MuxDevActivate;
    CallBacks.pMuxDevGetFreeTxBytes  = MuxDevGetFreeTxBytes;
    CallBacks.pMuxDevSetQueueSizes   = MuxDevSetQueueSizes;
    if (Mux_Register((DWORD)pBasePort, &CallBacks, pBasePort->m_dwMaxChannels, pBasePort->m_dwMaxMuxVersion, (DWORD *)&pBasePort->m_pMuxInstance) != MUX_OK) {
      // Something went wrong
      Mux_Deregister((DWORD)pBasePort->m_pMuxInstance);
      pBasePort->m_pMuxInstance = 0;
    }
  }

  if (pBasePort->m_pMuxInstance) {
    // Empty write transfer buffer
    ringbuf_Clear(&pBasePort->m_WriteRingBuf);
    // reset restart length
    pBasePort->m_dwLenRestart = -1;
    if (!bp_SendMuxStartCmd(pBasePort)) {
      Mux_Deregister((DWORD)pBasePort->m_pMuxInstance);
      pBasePort->m_pMuxInstance = 0;
    } else {
      // Reset the semaphores
      oswrap_ResetSemaphore(&pBasePort->m_hMuxMsgSemaphore);
      // Start the required threads
      if (!bp_StartThreads(pBasePort)) {
        Mux_Deregister((DWORD)pBasePort->m_pMuxInstance);
        pBasePort->m_pMuxInstance = 0;
      } else {
        // Give module some time to prepare the mux mode
        oswrap_Sleep(25);
        // Init and start the mux protocol handling
        oswrap_ResetEvent(&pBasePort->m_hStartStopEvent);
        pBasePort->m_dwStartResult = 0;
        Mux_Init((DWORD)pBasePort->m_pMuxInstance, pBasePort->m_dwHdlcWindowSize, pBasePort->m_dwHdlcFrameSize, pBasePort->m_fUsbPort ? 0 : pBasePort->m_dwBaudRate);
        if (Mux_Start((DWORD)pBasePort->m_pMuxInstance)) {
          // Something went wrong
          bp_StopThreads(pBasePort);
          Mux_Deregister((DWORD)pBasePort->m_pMuxInstance);
          pBasePort->m_pMuxInstance = 0;
        } else {
          // Wait for the mux protocol to be started (20 sec.)
          if (oswrap_WaitForEvent(&pBasePort->m_hStartStopEvent, 20000)) {
            // We timed out
            bp_StopThreads(pBasePort);
            Mux_Deregister((DWORD)pBasePort->m_pMuxInstance);
            pBasePort->m_pMuxInstance = 0;
          } else {
            // Check if everything was ok
            if (pBasePort->m_dwStartResult) {
              bp_StopMuxMode(pBasePort);
            } else {
              // We need a short delay here to allow mux version negotiation to finish to avoid
              // that too early opened mux channels are using the wrong version number.
              oswrap_Sleep(100);
              fRet = TRUE;
            }
          }
        }
      }
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);

  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Initialize the module we're connected to.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_InitModule(PT_BASEPORT pBasePort) {
  BOOL  fRet = TRUE;
  char *pMandatoryInitCmd[] = ATCMD_MANDATORY_INIT;
  int   i;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);

  if (pBasePort->m_fInitEnabled) {
    if (!bp_PowerModuleOn(pBasePort)) {
      fRet = FALSE;
    } else {
      if (!pBasePort->m_fUsbPort) {
        if (!bp_SyncBaudRate(pBasePort, pBasePort->m_dwBaudRate)) {
          fRet = FALSE;
        }
      }
    }
  }

  if (fRet) {
    char szCmd[MAX_INTCMD_LEN + 1];

    // Send the mandatory module initialization setting
    for (i = 0; (i < ARRAYSIZE(pMandatoryInitCmd)) && fRet; i++) {
      if (!bp_SendATCmd(pBasePort, pMandatoryInitCmd[i], TIMEOUT_MODULE_STD, 0, NULL, 0)) {
        // In emulator with DSB board the module switches on by simply opening the port.
        // If the customer init phase is disabled the module might have just been powered
        // on by opening the port which would cause the first AT command to fail.
        // The following retry simplifies testing with emulator and DSB board by avoiding
        // a false power off detection.
        oswrap_Sleep(2000);
        if (!bp_SendATCmd(pBasePort, pMandatoryInitCmd[i], TIMEOUT_MODULE_STD, 0, NULL, 0)) {
          fRet = FALSE;
          break;
        }
      }
      // Give modem signals some time to settle
      oswrap_Sleep(200);
    }

    // From here we can use DSR to monitor if module is alive because we've just sent AT&S0
    pBasePort->m_fMonitorDsr = TRUE;

    // For mux mode we must not be in autobauding mode
    if (!pBasePort->m_fInitEnabled) {
      sprintf(szCmd, "AT+IPR=%lu\r", pBasePort->m_dwBaudRate);
      if (!bp_SendATCmd(pBasePort, szCmd, TIMEOUT_MODULE_STD, 0, NULL, 0)) {
        fRet = FALSE;
      }
    }

    // Send the module initialization commands
    for (i = 0; (i < MAX_INITCMD_NUM) && fRet; i++) {
      if (strlen(pBasePort->m_strInitCmds[i])) {
        sprintf(szCmd, "%s\r", pBasePort->m_strInitCmds[i]);
        if (!bp_SendATCmd(pBasePort, szCmd, 3000, 0, NULL, 0)) {
          fRet = FALSE;
        }
      }
    }
  } else {
    pBasePort->m_fMonitorDsr = TRUE;
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);

  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Uninitialize the module we're connected to.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_DeinitModule(PT_BASEPORT pBasePort) {
  DWORD  i;
  BOOL   fRet = TRUE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (bp_IsModulePowerOn(pBasePort)) {
    // Send the module deinitialization commands
    for (i = 0; i < MAX_INITCMD_NUM; i++) {
      if (strlen(pBasePort->m_strDeinitCmds[i])) {
        char szCmd[MAX_INTCMD_LEN + 1];
        sprintf(szCmd, "%s\r", pBasePort->m_strDeinitCmds[i]);
        if (!bp_SendATCmd(pBasePort, szCmd, 3000, 1, NULL, 0)) {
          fRet = FALSE;
        }
      }
    }
  }

  // Check registry setting enabling module shutdwon
  if (pBasePort->m_fSwitchOffEnabled) {
    // Shutdown module
    if (!bp_SwitchModuleOff(pBasePort)) {
      fRet = FALSE;
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Open the hardware base port, initialize the module and start the mux mode.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static BOOL bp_Open(PT_BASEPORT pBasePort) {
  DWORD64  dw64Sleep;
  BOOL     fRet = FALSE;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);

  dw64Sleep = oswrap_GetTickCount() - pBasePort->m_dw64TimeLastOff;
  if (dw64Sleep < TIME_MODULE_OFF_MIN) {
    oswrap_Sleep(TIME_MODULE_OFF_MIN - dw64Sleep);
  }

  pBasePort->m_hPort = oswrap_TtyOpen(pBasePort->m_strPortName);
  if (pBasePort->m_hPort) {
    BOOL fInitialized = FALSE;
    pBasePort->m_fMonitorDsr = FALSE;
    pBasePort->m_fUsbPort = oswrap_IsUsbPort(pBasePort->m_hPort);
    if (oswrap_TtySetParams(pBasePort->m_hPort, pBasePort->m_dwBaudRate)) {
      if (!oswrap_TtyFlushBuffers(pBasePort->m_hPort, TRUE, TRUE)) {
        if (!oswrap_TtyStartDataFlow(pBasePort->m_hPort)) {
          // Now initialize the module if requested
          fInitialized = TRUE;
          if (bp_InitModule(pBasePort)) {
            // And finally start the mux mode
            if (bp_StartMuxMode(pBasePort)) {
              fRet = TRUE;
            }
          }
        }
      }
    }
    if (!fRet) {
      if (fInitialized) {
        bp_DeinitModule(pBasePort);
      }
      bp_ClosePort(pBasePort);
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%s, Port=%p", STRBOOL(fRet), pBasePort);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Stop the multiplex mode, deinitialize the module and close the hardware
// base port.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void bp_Close(PT_BASEPORT pBasePort) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  // Shutdown mux mode
  if (!bp_StopMuxMode(pBasePort)) {
    DBGPRINT(ZONE_ERR, "Failed to stop mux mode!");
  }
  // Now deinitialize the module if requested
  if (!bp_DeinitModule(pBasePort)) {
    DBGPRINT(ZONE_ERR, "Failed to shutdown module!");
  }
  bp_ClosePort(pBasePort);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
}

/****************************************************************************/
/* interface base port                                                      */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Create a base port object, allocate all corresponding resources.
//
// Parameters:
// iInstance: The number of the corresponding driver instance.
//
// Return:
// The reference to the corresponding base port object or NULL in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
PT_BASEPORT bp_Create(int iInstance) {
  PT_BASEPORT pBasePort = NULL;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%i", iInstance);

  pBasePort = oswrap_GetMem(sizeof(T_BASEPORT));
  if (pBasePort) {
    // Reset baseport struct
    MEMCLR(pBasePort, sizeof(T_BASEPORT));

    oswrap_CriticalSectionInitialize(&pBasePort->m_csLock);
    oswrap_SpinLockInitialize(&pBasePort->m_slLockOpenCount);

    oswrap_CreateEvent(&pBasePort->m_hStartStopEvent, FALSE);
    oswrap_CreateEvent(&pBasePort->m_hOpenEvent, TRUE);
    oswrap_CreateEvent(&pBasePort->m_hCloseEvent, TRUE);
    oswrap_CreateEvent(&pBasePort->m_hResumeEvent, TRUE);
    oswrap_CreateEvent(&pBasePort->m_hWakeModuleEvent, FALSE);
    oswrap_SetEvent(&pBasePort->m_hCloseEvent);
    oswrap_SetEvent(&pBasePort->m_hResumeEvent);

    oswrap_CreateSemaphore(&pBasePort->m_hMuxMsgSemaphore, 0);

    // Create the work queue
    pBasePort->m_hWorkQueue = oswrap_WorkQueueCreate(BP_WQUEUE_NAME);
    oswrap_WorkQueueInitItem(&pBasePort->m_WorkQueueShutDown, bp_ShutDownDeferred, pBasePort);

    // Create the required threads
    pBasePort->m_hReadThread = oswrap_CreateThread(bp_ThreadReadData, pBasePort, "MuxReadData");
    pBasePort->m_hWriteThread = oswrap_CreateThread(bp_ThreadWriteData, pBasePort, "MuxWriteData");
    pBasePort->m_hIsAliveThread = oswrap_CreateThread(bp_ThreadIsAlive, pBasePort, "IsAlive");
    pBasePort->m_hMuxMsgThread = oswrap_CreateThread(bp_ThreadMuxMsg, pBasePort, "MuxMsg");
    pBasePort->m_hMuxTimebaseThread = oswrap_CreateThread(bp_ThreadMuxTimebase, pBasePort, "MuxTimebase");
    pBasePort->m_hWakeModuleThread = oswrap_CreateThread(bp_ThreadWakeModule, pBasePort, "WakeModule");

    // Create ringbuffer for write handling
    ringbuf_Init(&pBasePort->m_WriteRingBuf);

    pBasePort->m_fInitEnabled = TRUE;
    pBasePort->m_fSwitchOffEnabled = TRUE;
    pBasePort->m_dwMaxMuxVersion = MP_PROTOCOL_VERSION_MAX;
    pBasePort->m_dwHdlcWindowSize = HDLC_DEF_WINDOW_SIZE;
    pBasePort->m_dwHdlcFrameSize = MAXFRAMESIZE;
    pBasePort->m_dwStartDelay = DEFAULT_START_DELAY;

    // init port names
    strcpy(pBasePort->m_strDeviceName, DEFAULT_TTYPORT_NAME);
    if (gInstances != 1) {
      pBasePort->m_strDeviceName[strlen(pBasePort->m_strDeviceName)] = 'A' + iInstance;
    }
    sprintf(pBasePort->m_strPortName, DEFAULT_BASEPORT_NAME"%d", iInstance);

    pBasePort->m_dwMaxChannels = gPorts;
    pBasePort->m_dwBaudRate = DEFAULT_BAUD_RATE;
  } else {
    DBGPRINT(ZONE_ERR, "no memory!");
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%p", pBasePort);
  return pBasePort;
}

//////////////////////////////////////////////////////////////////////////////
//
// Destroy a base port object, free all corresponding resources.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void bp_Destroy(PT_BASEPORT pBasePort) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (pBasePort) {
    // Destroy the used threads
    oswrap_DestroyThread(pBasePort->m_hReadThread);
    oswrap_DestroyThread(pBasePort->m_hWriteThread);
    oswrap_DestroyThread(pBasePort->m_hIsAliveThread);
    oswrap_DestroyThread(pBasePort->m_hMuxMsgThread);
    oswrap_DestroyThread(pBasePort->m_hMuxTimebaseThread);
    oswrap_DestroyThread(pBasePort->m_hWakeModuleThread);

    // Destroy work queue
    oswrap_WorkQueueDestroy(pBasePort->m_hWorkQueue);

    // Destroy ringbuffer for write handling
    ringbuf_Destroy(&pBasePort->m_WriteRingBuf);

    pBasePort->m_dwVComs = 1;
    bp_RemoveVCom(pBasePort);

    oswrap_FreeMem(pBasePort);
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
}

//////////////////////////////////////////////////////////////////////////////
//
// Increment the number of registered virtual ports. If the first virtual
// port is registered the base port is opened and initialized and the mux
// mode is started.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
// dwDlci   : The number of the channel to be added.
//
// Return:
// The handle of the connected mux instance, 0 in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
MUX_INSTANCE_t *bp_AddVCom(PT_BASEPORT pBasePort, DWORD dwDlci) {
  MUX_INSTANCE_t * pMuxInstance = NULL;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p, Dlci=%d", pBasePort, dwDlci);

  if (pBasePort) {
    if (dwDlci < 1) {
      DBGPRINT(ZONE_ERR, "wrong DLCI (%d)!", dwDlci);
    } else {
      oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
      // Check the number of the channel
      if (dwDlci > pBasePort->m_dwMaxChannels) {
        DBGPRINT(ZONE_ERR, "Dlci > max mux channels!");
      } else {
        BOOL fRepeat = TRUE;
        while (fRepeat) {
          // Check if we are not already open or opening
          switch (pBasePort->m_State) {
            case BASEPORT_CLOSED:
            case BASEPORT_KILLED:
              if (!pBasePort->m_dwVComs) {
                BOOL fOk;
                pBasePort->m_State = BASEPORT_OPENING;
                oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
                fOk = bp_Open(pBasePort);
                oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
                if (fOk) {
                  if (pBasePort->m_State == BASEPORT_OPENING) {
                    pBasePort->m_State = BASEPORT_OPEN;
                    pMuxInstance = pBasePort->m_pMuxInstance;
                    pBasePort->m_dwVComs++;
                    oswrap_ResetEvent(&pBasePort->m_hCloseEvent);
                    oswrap_SetEvent(&pBasePort->m_hOpenEvent);
                  }
                } else {
                  if (pBasePort->m_State != BASEPORT_KILLED) {
                    pBasePort->m_State = BASEPORT_CLOSED;
                  }
                }
              } else {
                if (pBasePort->m_State != BASEPORT_KILLED) {
                  pMuxInstance = pBasePort->m_pMuxInstance;
                  pBasePort->m_dwVComs++;
                }
              }
              fRepeat = FALSE;
              break;
            case BASEPORT_OPEN:
              pMuxInstance = pBasePort->m_pMuxInstance;
              pBasePort->m_dwVComs++;
              fRepeat = FALSE;
              break;
            case BASEPORT_OPENING:
              // We need to wait until the base port has finished to open
              oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
              if (oswrap_WaitForEvent(&pBasePort->m_hOpenEvent, TIMEOUT_STATE_CHANGE)) {
                DBGPRINT(ZONE_ERR, "Failed to wait for port being opened");
                fRepeat = FALSE;
              }
              oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
              break;
            case BASEPORT_CLOSING:
              // We need to wait until the base port has finished to close
              oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
              if (oswrap_WaitForEvent(&pBasePort->m_hCloseEvent, TIMEOUT_STATE_CHANGE)) {
                DBGPRINT(ZONE_ERR, "Failed to wait for port being closed");
                fRepeat = FALSE;
              }
              oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
              break;
            case BASEPORT_SUSPENDED:
              // We need to wait until we're resumed again
              oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
              if (oswrap_WaitForEvent(&pBasePort->m_hResumeEvent, TIMEOUT_STATE_CHANGE)) {
                DBGPRINT(ZONE_ERR, "Failed to wait until we're resumed again");
                fRepeat = FALSE;
              }
              oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
              break;
          }
        }
      }
      oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret= %p, Port=%p", pMuxInstance, pBasePort);

  return pMuxInstance;
}

//////////////////////////////////////////////////////////////////////////////
//
// Remove a virtual mux port, shutdown multiplexer and tidy up if it was the
// last one.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void bp_RemoveVCom(PT_BASEPORT pBasePort) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (pBasePort) {
    BOOL fRepeat = TRUE;
    oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
    while (fRepeat) {
      switch (pBasePort->m_State) {
        case BASEPORT_OPEN:
          if (pBasePort->m_dwVComs) {
            pBasePort->m_dwVComs--;
          }
          if (!pBasePort->m_dwVComs) {
            pBasePort->m_State = BASEPORT_CLOSING;
            oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
            // Stop mux protocol handling and tidy everything up
            bp_Close(pBasePort);
            oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
            pBasePort->m_State = BASEPORT_CLOSED;
            oswrap_ResetEvent(&pBasePort->m_hOpenEvent);
            oswrap_SetEvent(&pBasePort->m_hCloseEvent);
          }
          fRepeat = FALSE;
          break;
        case BASEPORT_OPENING:
          // We need to wait until the base port has finished to open
          oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
          if (oswrap_WaitForEvent(&pBasePort->m_hOpenEvent, TIMEOUT_STATE_CHANGE)) {
            DBGPRINT(ZONE_ERR, "Failed to wait for port being opened");
            fRepeat = FALSE;
          }
          oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
          break;
        case BASEPORT_CLOSING:
          // We need to wait until the base port has finished to close
          oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
          if (oswrap_WaitForEvent(&pBasePort->m_hCloseEvent, TIMEOUT_STATE_CHANGE)) {
            DBGPRINT(ZONE_ERR, "Failed to wait for port being closed");
            fRepeat = FALSE;
          }
          oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
          break;
        case BASEPORT_SUSPENDED:
          // We need to wait until we're resumed again
          oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
          if (oswrap_WaitForEvent(&pBasePort->m_hResumeEvent, TIMEOUT_STATE_CHANGE)) {
            DBGPRINT(ZONE_ERR, "Failed to wait until we're resumed again");
            fRepeat = FALSE;
          }
          oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
          break;
        default:
          // We aren't open, nothing to do than updating our counter.
          if (pBasePort->m_dwVComs) {
            pBasePort->m_dwVComs--;
          }
          fRepeat = FALSE;
          break;
      }
    }
    oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
}

//////////////////////////////////////////////////////////////////////////////
//
// Called when the system is shutting down.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void bp_Shutdown(PT_BASEPORT pBasePort) {
#if defined(PWRMGMT_ENABLED) && PWRMGMT_ENABLED
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  if (pBasePort) {
    bp_ShutDownDeferred(pBasePort);
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
#endif // PWRMGMT_ENABLED
}

//////////////////////////////////////////////////////////////////////////////
//
// Called when the system is suspending. We stop data flow and close the
// base port to avoid problems together with the suspending base port
// driver.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void bp_Suspend(PT_BASEPORT pBasePort) {
#if defined(PWRMGMT_ENABLED) && PWRMGMT_ENABLED
  BOOL fRepeat = TRUE;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
  while (fRepeat) {
    switch (pBasePort->m_State) {
      case BASEPORT_CLOSED:
      case BASEPORT_KILLED:
        // Nothing to do
        fRepeat = FALSE;
        break;
      case BASEPORT_OPEN:
        // Here we go...
        pBasePort->m_State = BASEPORT_SUSPENDED;
        oswrap_ResetEvent(&pBasePort->m_hResumeEvent);
        oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
        // Stop running threads to avoid further port access
        bp_StopThreads(pBasePort);
        // And put the base port to sleep
        if (!oswrap_TtySuspend(pBasePort->m_hPort)) {
          DBGPRINT(ZONE_ERR, "Failed to suspend base port");
        }
        oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
        fRepeat = FALSE;
        break;
      case BASEPORT_OPENING:
        // We need to wait until the base port has finished to open
        oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
        if (oswrap_WaitForEvent(&pBasePort->m_hOpenEvent, TIMEOUT_STATE_CHANGE)) {
          DBGPRINT(ZONE_ERR, "Failed to wait for port being opened");
          fRepeat = FALSE;
        }
        oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
        break;
      case BASEPORT_CLOSING:
        // We need to wait until the base port has finished to close
        oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
        if (oswrap_WaitForEvent(&pBasePort->m_hCloseEvent, TIMEOUT_STATE_CHANGE)) {
          DBGPRINT(ZONE_ERR, "Failed to wait for port being closed");
          fRepeat = FALSE;
        }
        oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
        break;
      case BASEPORT_SUSPENDED:
        // Something went utterly wrong
        DBGPRINT(ZONE_ERR, "We are already suspended");
        fRepeat = FALSE;
        break;
    }
  }
  oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
#endif // PWRMGMT_ENABLED
}

//////////////////////////////////////////////////////////////////////////////
//
// Called when the system is resuming. We reopen the base port which has been
// closed during suspend and restart the data flow.
//
// Parameters:
// pBasePort: The reference to the corresponding base port object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void bp_Resume(PT_BASEPORT pBasePort) {
#if defined(PWRMGMT_ENABLED) && PWRMGMT_ENABLED
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
  oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
  switch (pBasePort->m_State) {
    case BASEPORT_CLOSED:
    case BASEPORT_KILLED:
      // Nothing to do
      break;
    case BASEPORT_SUSPENDED:
      // Update the state
      pBasePort->m_State = BASEPORT_OPEN;
      oswrap_SetEvent(&pBasePort->m_hResumeEvent);
      oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
      // Resume the base port
      if (!oswrap_TtyResume(pBasePort->m_hPort)) {
        DBGPRINT(ZONE_ERR, "Failed to resume base port");
        oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
        pBasePort->m_State = BASEPORT_KILLED;
        // Shutdown mux protocol handling
        oswrap_WorkQueueQueueItem(pBasePort->m_hWorkQueue, &pBasePort->m_WorkQueueShutDown);
      } else {
        if (!bp_IsModulePowerOn(pBasePort)) {
          // We lost the DSR signal which can only mean that the module went off while the system was suspended!
          oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
          pBasePort->m_State = BASEPORT_KILLED;
          DBGPRINT(ZONE_MODULE_EXIT, "Unexpected module shutdown during suspend detected!");
          // Shutdown mux protocol handling
          oswrap_WorkQueueQueueItem(pBasePort->m_hWorkQueue, &pBasePort->m_WorkQueueShutDown);
        } else {
          // Restart the required threads
          bp_StartThreads(pBasePort);
          // Ensure that everything is kicked up again
          Dev_SendDataContinue((DWORD)pBasePort->m_pMuxInstance, 0);
          ringbuf_PutBytes(&pBasePort->m_WriteRingBuf, NULL, 0);
          oswrap_ReleaseSemaphore(&pBasePort->m_hMuxMsgSemaphore);
          oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
        }
      }
      break;
    default:
      // Something went utterly wrong
      DBGPRINT(ZONE_ERR, "We're not suspended");
      break;
  }
  oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", pBasePort);
#endif // PWRMGMT_ENABLED
}

/****************************************************************************/
/* Callbacks from the mux protocol implementation                           */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to allocate a block of memory.
//
// Parameters:
// dwUserData: User parameter used as reference to our base class object.
// dwSize    : The size of the memory block to be allocated.
//
// Return:
// Pointer to the allocated block of memory, NULL in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static void* MuxGetMem(DWORD dwUserData, DWORD dwSize) {
  void *pRet;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p", dwUserData, dwSize);
  pRet = oswrap_GetMem(dwSize);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%p, Port=%p", pRet, dwUserData);
  return pRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to free a previously allocated block of memory.
//
// Parameters:
// dwUserData: User parameter used as reference to our base class object.
// pMem      : The memory block to be freed.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxFreeMem(DWORD dwUserData, PVOID pMem) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Port=%p, Mem=%p", dwUserData, pMem);
  oswrap_FreeMem(pMem);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Port=%p", dwUserData);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to indicate that there is a message inside the mux protocol
// waiting to be handled.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxMsgAvailable(DWORD dwMuxInstance, DWORD dwUserData) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
  oswrap_ReleaseSemaphore(&((T_BASEPORT*)dwUserData)->m_hMuxMsgSemaphore);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to indicate that the mux protocol handling has been properly closed.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxReset(DWORD dwMuxInstance, DWORD dwUserData) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
  oswrap_SetEvent(&((PT_BASEPORT)dwUserData)->m_hStartStopEvent);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to thread safe lock the mux protocol handling.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxLock(DWORD dwMuxInstance, DWORD dwUserData) {
  oswrap_CriticalSectionEnter(&(((PT_BASEPORT)dwUserData)->m_csLock));
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to unlock the previously locked mux protocol handling.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxUnlock(DWORD dwMuxInstance, DWORD dwUserData) {
  oswrap_CriticalSectionLeave(&(((PT_BASEPORT)dwUserData)->m_csLock));
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called if the mux start has finished.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
// dwResult     : The result of the mux start operation.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxStartResult(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwResult) {
  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p, Result=%d", dwMuxInstance, dwUserData, dwResult);
  ((PT_BASEPORT)dwUserData)->m_dwStartResult = dwResult;
  oswrap_SetEvent(&((PT_BASEPORT)dwUserData)->m_hStartStopEvent);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Called to send data out to the hardware port.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
// pData        : Pointer to the data to write.
// dwDataLen    : The number of bytes to write.
//
// Return:
// The number of bytes written.
//
//////////////////////////////////////////////////////////////////////////////
static DWORD MuxDevSendData(DWORD dwMuxInstance, DWORD dwUserData, PBYTE pData, DWORD dwDataLen) {
  int          iWritten = 0;
  PT_BASEPORT  pBasePort = (PT_BASEPORT)dwUserData;

  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p, pData=%p, Len=%d", dwMuxInstance, dwUserData, pData, dwDataLen);

  if ((pBasePort->m_State != BASEPORT_KILLED) && pBasePort->m_hPort) {
#if defined(PWRMGMT_ENABLED) && PWRMGMT_ENABLED
    if (pBasePort->m_State == BASEPORT_SUSPENDED) {
      // We need to wait until we're resumed again
      oswrap_CriticalSectionLeave(&pBasePort->m_csLock);
      DBGPRINT(ZONE_FCT_POWER_MGM, "Trying to send data to base port &p while suspended", pBasePort);
      if (oswrap_WaitForEvent(&pBasePort->m_hResumeEvent, TIMEOUT_STATE_CHANGE)) {
        DBGPRINT(ZONE_ERR, "Failed to wait until we're resumed again");
        DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=0, Instance=%p, Port=%p", dwMuxInstance, dwUserData);
        oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
        return 0;
      }
      oswrap_CriticalSectionEnter(&pBasePort->m_csLock);
    }
#endif // PWRMGMT_ENABLED

    // Wake module from possible sleep mode
    if (!pBasePort->m_fUsbPort) {
      oswrap_SetEvent(&(pBasePort->m_hWakeModuleEvent));
    }

    iWritten = ringbuf_PutBytes(&pBasePort->m_WriteRingBuf, pData, dwDataLen);
    if (iWritten < dwDataLen) {
      DBGPRINT(ZONE_ERR, "Only %d of %d bytes written to ringbuffer", iWritten, dwDataLen);
    }
  }

  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret=%d, Instance=%p, Port=%p", iWritten, dwMuxInstance, dwUserData);

  return (DWORD)iWritten;
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Activate the mux message handling after the amount of data in the hardware
// ports tx queue has fallen below the given border.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
// dwLen        : Number of bytes remaining in the tx queue when the mux
//                message handling has to be reactivated.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxDevActivate(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwLen) {
  PT_BASEPORT pBasePort = (PT_BASEPORT)dwUserData;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p, Len=%d", dwMuxInstance, dwUserData, dwLen);
  if (pBasePort->m_State != BASEPORT_KILLED) {
    pBasePort->m_dwLenRestart = dwLen;
    // The data can have been written out already!
    // -> Activate the write thread to ensure that the mux protocol is started again
    ringbuf_PutBytes(&pBasePort->m_WriteRingBuf, NULL, 0);
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Return the data available in the write ringbuffer.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
//
// Return:
// The number of free bytes in the tx buffer.
//
//////////////////////////////////////////////////////////////////////////////
static DWORD MuxDevGetFreeTxBytes(DWORD dwMuxInstance, DWORD dwUserData) {
  PT_BASEPORT pBasePort = (PT_BASEPORT)dwUserData;
  DWORD       dwRet = 0;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
  if ((pBasePort->m_State != BASEPORT_KILLED) && (pBasePort->m_State != BASEPORT_SUSPENDED) && pBasePort->m_hPort) {
    dwRet = ringbuf_GetNumFreeBytes(&pBasePort->m_WriteRingBuf);
  }
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Ret= %d, Instance=%p, Port=%p", dwRet, dwMuxInstance, dwUserData);
  return dwRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Callback function from the multiplex protocol implementation.
// Set the queue sizes of the hardware port.
//
// Parameters:
// dwMuxInstance: Handle identifying the multiplex instance.
// dwUserData   : User parameter used as reference to our base class object.
// dwRxSize     : New size of the serial hardware devices in queue.
// dwTxSize     : New size of the serial hardware devices out queue.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void MuxDevSetQueueSizes(DWORD dwMuxInstance, DWORD dwUserData, DWORD dwRxSize, DWORD dwTxSize) {
  DWORD actualTxSize;
  DBG_ENTER(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p, RxSize=%d, TxSize=%d", dwMuxInstance, dwUserData, dwRxSize, dwTxSize);
  actualTxSize = ringbuf_AdjustSize(&((PT_BASEPORT)dwUserData)->m_WriteRingBuf, dwTxSize);
  DBGPRINT(ZONE_GENERAL_INFO, "MuxDevSetQueueSizes(Rx: %lu, Tx: %lu, actual Tx: %lu)", dwRxSize, dwTxSize, actualTxSize);
  DBG_LEAVE(ZONE_FCT_BASEPORT, "Instance=%p, Port=%p", dwMuxInstance, dwUserData);
}
