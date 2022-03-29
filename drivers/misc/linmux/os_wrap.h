/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Os_wrap.h
//
// This file contains the definition of the operating system dependant
// interface of the linmux driver.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __OS_WRAP_H
#define __OS_WRAP_H

#define ARRAYSIZE(a)          (sizeof(a) / sizeof(a[0]))

#define SEM_WAIT_INFINITE     MAX_SCHEDULE_TIMEOUT
#define EVT_WAIT_INFINITE     MAX_SCHEDULE_TIMEOUT
#define EVT_SIGNALED          0

//////////////////////////////////////////////////////////////////////////////

typedef struct mutex              MUTEX;
typedef struct semaphore          SEMHANDLE;
typedef struct tty_struct        *TTYHANDLE;
typedef struct workqueue_struct  *WORKQUEUE_HANDLE;

typedef struct {
  wait_queue_head_t    hEvent;
  bool                 fManual;
  bool                 fRelease;
} EVTHANDLE;

typedef struct {
  spinlock_t           slLock;
  unsigned long        ulFlags;
} SPINLOCK;

typedef struct {
  struct file         *pFile;
  struct termios       settings;
  unsigned long        ulLines;
  char                 strPortName[32];
} _PORTHANDLE, *PORTHANDLE;

typedef struct {
  struct task_struct  *pTask;
  const char          *pName;
  bool               (*pFunc)(void *);
  void                *pData;
  bool                 fStarted;
  EVTHANDLE            evStarted;
  struct mutex         muLock;
} THREADINFO, *THREADHANDLE;

typedef struct {
  void               (*pFunc)(void *);
  void                *pData;
  struct work_struct   Work;
  struct delayed_work  DelayedWork;
} WORKQUEUE_ITEM;

typedef struct {
  TTYHANDLE            hTty;
} UPPERPORT, *UPPERPORTHANDLE;

//////////////////////////////////////////////////////////////////////////////

WORKQUEUE_HANDLE oswrap_WorkQueueCreate(const char* strName);
void oswrap_WorkQueueDestroy(WORKQUEUE_HANDLE hWorkQueue);
void oswrap_WorkQueueFlush(WORKQUEUE_HANDLE hWorkQueue);
void oswrap_WorkQueueInitItem(WORKQUEUE_ITEM *pItem, void (*pFunc)(void *), void *pData);
void oswrap_WorkQueueInitDelayedItem(WORKQUEUE_ITEM *pItem, void (*pFunc)(void *), void *pData);
bool oswrap_WorkQueueQueueItem(WORKQUEUE_HANDLE hWorkQueue, WORKQUEUE_ITEM *pItem);
bool oswrap_WorkQueueQueueItemDelayed(WORKQUEUE_HANDLE hWorkQueue, WORKQUEUE_ITEM *pItem, unsigned long ulDelay);
bool oswrap_WorkQueueCancelDelayedWork(WORKQUEUE_ITEM *pItem);

THREADHANDLE oswrap_CreateThread(bool (*threadfn)(void *), void *pData, const char *pName);
void oswrap_DestroyThread(THREADHANDLE hThread);
bool oswrap_StartThread(THREADHANDLE hThread);
bool oswrap_StopThread(THREADHANDLE hThread);
int oswrap_ShouldStopThread(void);

PORTHANDLE oswrap_TtyOpen(const char * pTtyPathName);
int oswrap_TtyClose(PORTHANDLE pTtyFile);
bool oswrap_TtySuspend(PORTHANDLE pTtyFile);
bool oswrap_TtyResume(PORTHANDLE pTtyFile);
int oswrap_TtyRead(PORTHANDLE pTtyFile, unsigned char *pBuf, int iCount);
int oswrap_TtyReadTimeout(PORTHANDLE pTtyFile, unsigned char *pBuf, int iCount, unsigned long ulTimeout);
int oswrap_TtyWrite(PORTHANDLE pTtyFile, const unsigned char *pBuf, int iCount);
int oswrap_TtyIoctl(PORTHANDLE pTtyFile, unsigned int uiCmd, unsigned long ulArg);
bool oswrap_TtySetParams(PORTHANDLE pTtyFile, unsigned long ulBaudRate);
int oswrap_TtyGetTiming(PORTHANDLE pTtyFile, PBYTE pVMin, PBYTE pVTime);
int oswrap_TtySetTiming(PORTHANDLE pTtyFile, BYTE VMin, BYTE VTime);
int oswrap_TtyStopDataFlow(PORTHANDLE pTtyFile);
int oswrap_TtyStartDataFlow(PORTHANDLE pTtyFile);
int oswrap_TtyWaitChangeModemLine(PORTHANDLE pTtyFile, unsigned long ulLineMask);
int oswrap_TtyGetModemLines(PORTHANDLE pTtyFile, unsigned long *pulLines);
int oswrap_TtySetModemLines(PORTHANDLE pTtyFile, unsigned long ulLines);
int oswrap_TtyToggleRtsDtr(PORTHANDLE pTtyFile, unsigned long ulInterval);
int oswrap_TtyGetBytesInTxQueue(PORTHANDLE pTtyFile, unsigned long *pulLen);
int oswrap_TtyFlushBuffers(PORTHANDLE pTtyFile, bool fRx, bool fTx);

bool oswrap_UpperPortCreate(UPPERPORTHANDLE hPort);
void oswrap_UpperPortDestroy(UPPERPORTHANDLE hPort);
void oswrap_UpperPortRegister(UPPERPORTHANDLE hPort, TTYHANDLE hTty);
void oswrap_UpperPortUnregister(UPPERPORTHANDLE hPort);
int oswrap_UpperPortPushData(UPPERPORTHANDLE hPort, unsigned char *pBuf, unsigned int uiCount);
void oswrap_UpperPortContinueSending(UPPERPORTHANDLE hPort);

void *oswrap_GetMem(unsigned long ulSize);
void oswrap_FreeMem(const void *pAddr);

void oswrap_Sleep(int iTime);

unsigned long long oswrap_GetTickCount(void);

bool oswrap_IsUsbPort(PORTHANDLE hPort);

void oswrap_CreateSemaphore(SEMHANDLE *pSem, int iInit);
int oswrap_WaitForSemaphore(SEMHANDLE *pSem, unsigned long ulTimeout);
void oswrap_ReleaseSemaphore(SEMHANDLE *pSem);
void oswrap_ResetSemaphore(SEMHANDLE *pSem);

void oswrap_CreateEvent(EVTHANDLE *pEvent, bool fManual);
int oswrap_WaitForEvent(EVTHANDLE *pEvent, unsigned long ulTimeout);
void oswrap_SetEvent(EVTHANDLE *pEvent);
void oswrap_ResetEvent(EVTHANDLE *pEvent);

void oswrap_SpinLockInitialize(SPINLOCK *pSL);
void oswrap_SpinLock(SPINLOCK *pSL);
void oswrap_SpinUnlock(SPINLOCK *pSL);
void oswrap_MutexInitialize(MUTEX *pMu);
void oswrap_MutexLock(MUTEX *pMu);
void oswrap_MutexUnlock(MUTEX *pMu);

// The CRITICAL_SECTION macros are used to synchronize the multiplex protocol
// handling in the host driver and to synchronize the multiplex protocol
// handling and the virtual multiplex channels in the client driver.
// It is not recommended to change these macros from mutex to spinlock usage
// because they syncronize huge code blocks. If it is inevitable to do so the
// memory handling must be changed from vmalloc to unblocking kmalloc usage as
// well because memory allocation is done inside synchronized code blocks.
typedef MUTEX CRITICAL_SECTION;
#define oswrap_CriticalSectionInitialize(hCS)  oswrap_MutexInitialize(hCS)
#define oswrap_CriticalSectionEnter(hCS)       oswrap_MutexLock(hCS)
#define oswrap_CriticalSectionLeave(hCS)       oswrap_MutexUnlock(hCS)
// typedef SPINLOCK CRITICAL_SECTION;
// #define oswrap_CriticalSectionInitialize(hCS)  oswrap_SpinLockInitialize(hCS)
// #define oswrap_CriticalSectionEnter(hCS)       oswrap_SpinLock(hCS)
// #define oswrap_CriticalSectionLeave(hCS)       oswrap_SpinUnlock(hCS)

#endif // __OS_WRAP_H

