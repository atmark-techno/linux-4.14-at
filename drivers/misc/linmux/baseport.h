/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Baseport.h
//
// This file contains the definition and some simple inline function
// implementations of the BasePort object which handles access to the
// module via the hardware base port, it's initialization and deintialization
// as well as the management of the opening and closing of virtual ports.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __BASEPORT_H
#define __BASEPORT_H

#include <linux/platform_device.h>

#include "global.h"
#include "os_wrap.h"
#include "linmuxcfg.h"
#include "ringbuffer.h"


//////////////////////////////////////////////////////////////////////////////

#define READ_BUFFER_SIZE    MAXFRAMESIZE_1K_ALIGNED
#define WRITE_BUFFER_SIZE   MAXFRAMESIZE_1K_ALIGNED
#define BP_WQUEUE_NAME     "BpChnWQ"

//////////////////////////////////////////////////////////////////////////////

typedef enum {
  BASEPORT_CLOSED = 0,
  BASEPORT_OPENING,
  BASEPORT_OPEN,
  BASEPORT_CLOSING,
  BASEPORT_KILLED,
  BASEPORT_SUSPENDED
} BASEPORT_STATE;

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  // port params
  DWORD                     m_dwVComs;
  PORTHANDLE                m_hPort;

  BOOL                      m_fMonitorDsr;

  BASEPORT_STATE            m_State;

  WORKQUEUE_HANDLE          m_hWorkQueue;
  WORKQUEUE_ITEM            m_WorkQueueShutDown;

  THREADHANDLE              m_hReadThread;
  THREADHANDLE              m_hWriteThread;
  THREADHANDLE              m_hIsAliveThread;
  THREADHANDLE              m_hMuxMsgThread;
  THREADHANDLE              m_hMuxTimebaseThread;
  THREADHANDLE              m_hWakeModuleThread;

  SEMHANDLE                 m_hMuxMsgSemaphore;

  EVTHANDLE                 m_hOpenEvent;
  EVTHANDLE                 m_hCloseEvent;
  EVTHANDLE                 m_hResumeEvent;
  EVTHANDLE                 m_hStartStopEvent;
  EVTHANDLE                 m_hWakeModuleEvent;

  DWORD                     m_dwStartResult;

  CRITICAL_SECTION          m_csLock;

  MUX_INSTANCE_t *          m_pMuxInstance;

  DWORD                     m_dwLenRestart;

  DWORD64                   m_dw64TimeLastOff;

  RingBuf_t                 m_WriteRingBuf;
  BYTE                      m_bWriteBuf[WRITE_BUFFER_SIZE];
  BYTE                      m_bReadBuf[READ_BUFFER_SIZE];
  DWORD                     m_dwLastRead;

  // Used by linmuxcfg's ReloadInstance to prevent reloading an instance while in use
  SPINLOCK                  m_slLockOpenCount;
  int                       m_iOpenCount;
  int                       m_iInstanceState;

  // Indication if the port is USB, related to module power management
  BOOL                      m_fUsbPort;

  // Port Configuration
  DWORD                     m_dwMaxChannels;
  DWORD                     m_dwBaudRate;
  DWORD                     m_dwMaxMuxVersion;
  DWORD                     m_dwHdlcWindowSize;
  DWORD                     m_dwHdlcFrameSize;
  DWORD                     m_dwStartDelay;
  BOOL                      m_fInitEnabled;
  BOOL                      m_fSwitchOffEnabled;
  char                      m_strPortName[MAX_PORTNAME_LEN];
  char                      m_strDeviceName[MAX_PORTNAME_LEN];
  char                      m_strInitCmds[MAX_INITCMD_NUM][MAX_INTCMD_LEN];
  char                      m_strDeinitCmds[MAX_INITCMD_NUM][MAX_INTCMD_LEN];

  // The driver reference
  struct tty_driver        *m_pMuxTtyDriver;

  // The platform device required to hook to power management api
  struct platform_device   *m_pPlatformDevice;
} T_BASEPORT, *PT_BASEPORT;

//////////////////////////////////////////////////////////////////////////////

extern PT_BASEPORT *pMuxBasePorts;

//////////////////////////////////////////////////////////////////////////////

PT_BASEPORT          bp_Create(int iInstance);
void                 bp_Destroy(PT_BASEPORT pBasePort);
MUX_INSTANCE_t *     bp_AddVCom(PT_BASEPORT pBasePort, DWORD dwDlci);
void                 bp_RemoveVCom(PT_BASEPORT pBasePort);
void                 bp_Shutdown(PT_BASEPORT pBasePort);
void                 bp_Suspend(PT_BASEPORT pBasePort);
void                 bp_Resume(PT_BASEPORT pBasePort);

//////////////////////////////////////////////////////////////////////////////
//
// Getting and setting of init configuration values
//
//////////////////////////////////////////////////////////////////////////////
static inline DWORD  bp_GetMaxChannels(PT_BASEPORT pBasePort) { return pBasePort->m_dwMaxChannels; }
static inline void   bp_SetMaxChannels(PT_BASEPORT pBasePort, DWORD dwMaxChannels) { pBasePort->m_dwMaxChannels = dwMaxChannels; }
static inline DWORD  bp_GetBaudRate(PT_BASEPORT pBasePort) { return pBasePort->m_dwBaudRate; }
static inline void   bp_SetBaudRate(PT_BASEPORT pBasePort, DWORD dwBaudRate) { pBasePort->m_dwBaudRate = dwBaudRate; }
static inline char*  bp_GetBasePortName(PT_BASEPORT pBasePort) { return pBasePort->m_strPortName; }
static inline void   bp_SetBasePortName(PT_BASEPORT pBasePort, char *strDevice) { strcpy(pBasePort->m_strPortName, strDevice); }
static inline char*  bp_GetDeviceName(PT_BASEPORT pBasePort) { return pBasePort->m_strDeviceName; }
static inline void   bp_SetDeviceName(PT_BASEPORT pBasePort, char *strPort) { strcpy(pBasePort->m_strDeviceName, strPort); }
static inline char*  bp_GetInitCmd(PT_BASEPORT pBasePort, int iIndex) { return pBasePort->m_strInitCmds[iIndex]; }
static inline void   bp_SetInitCmd(PT_BASEPORT pBasePort, int iIndex, char *strCmd) { strcpy(pBasePort->m_strInitCmds[iIndex], strCmd); }
static inline char*  bp_GetDeinitCmd(PT_BASEPORT pBasePort, int iIndex) { return pBasePort->m_strDeinitCmds[iIndex]; }
static inline void   bp_SetDeinitCmd(PT_BASEPORT pBasePort, int iIndex, char *strCmd) { strcpy(pBasePort->m_strDeinitCmds[iIndex], strCmd); }
static inline DWORD  bp_GetMaxMuxVersion(PT_BASEPORT pBasePort) { return pBasePort->m_dwMaxMuxVersion; }
static inline void   bp_SetMaxMuxVersion(PT_BASEPORT pBasePort, DWORD dwMaxMuxVersion) { pBasePort->m_dwMaxMuxVersion = dwMaxMuxVersion; }
static inline DWORD  bp_GetHdlcWindowSize(PT_BASEPORT pBasePort) { return pBasePort->m_dwHdlcWindowSize; }
static inline void   bp_SetHdlcWindowSize(PT_BASEPORT pBasePort, DWORD dwHdlcWindowSize) { pBasePort->m_dwHdlcWindowSize = dwHdlcWindowSize; }
static inline DWORD  bp_GetHdlcFrameSize(PT_BASEPORT pBasePort) { return pBasePort->m_dwHdlcFrameSize; }
static inline void   bp_SetHdlcFrameSize(PT_BASEPORT pBasePort, DWORD dwHdlcFrameSize) { pBasePort->m_dwHdlcFrameSize = dwHdlcFrameSize; }
static inline BOOL   bp_GetInitEnabled(PT_BASEPORT pBasePort) { return pBasePort->m_fInitEnabled; }
static inline void   bp_SetInitEnabled(PT_BASEPORT pBasePort, BOOL fInitEnabled) { pBasePort->m_fInitEnabled = fInitEnabled; }
static inline BOOL   bp_GetSwitchOffEnabled(PT_BASEPORT pBasePort) { return pBasePort->m_fSwitchOffEnabled; }
static inline void   bp_SetSwitchOffEnabled(PT_BASEPORT pBasePort, BOOL fSwitchOffEnabled) { pBasePort->m_fSwitchOffEnabled = fSwitchOffEnabled; }
static inline DWORD  bp_GetStartDelay(PT_BASEPORT pBasePort) { return pBasePort->m_dwStartDelay; }
static inline void   bp_SetStartDelay(PT_BASEPORT pBasePort, DWORD dwStartDelay) { pBasePort->m_dwStartDelay = dwStartDelay; }

#endif // __BASEPORT_H
