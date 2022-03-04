/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Os_wrap.c
//
// This file contains the implementation of the operating system dependant
// interface of the linmux driver.
//
//////////////////////////////////////////////////////////////////////////////

#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#endif // KERNEL_VERSION

#include "muxdbg.h"
#include "os_wrap.h"

/****************************************************************************/
/* thread functions                                                         */
/****************************************************************************/

#define THREADNAME(h) (h ? STRPTR(h->pName) : "<NULL>")

bool StopThread(THREADHANDLE hThread);

//////////////////////////////////////////////////////////////////////////////
//
// The master thread function.
//
// Parameters:
// pData: User data passed to the thread function containing the thread data.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
static int oswrap_ThreadFunc(void *pData) {
  THREADHANDLE hThread = (THREADHANDLE)pData;

  DBG_ENTER(ZONE_FCT_OSWRAP_THREAD, "pData=%p", pData);

  allow_signal(SIGTERM);
  oswrap_SetEvent(&hThread->evStarted);

  while (!oswrap_ShouldStopThread()) {
    if (!(hThread->pFunc(hThread->pData))) {
      break;
    }
  }

  // Put thread asleep until it is stopped
  set_current_state(TASK_INTERRUPTIBLE);
  schedule();

  DBG_LEAVE(ZONE_FCT_OSWRAP_THREAD, "Ret=0");
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// Create and initialize a new thread (it wont be started here).
//
// Parameters:
// threadfn: The thread function. It will be repeatedly called in a loop
//           until it returns false or the thread is stopped.
// pData   : User data passed to the thread function.
// pName   : Name of the thread to be started.
//
// Return:
// A handle of the created thread, NULL in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
THREADHANDLE oswrap_CreateThread(bool (*threadfn)(void *), void *pData, const char *pName) {
  THREADHANDLE hThread;
  DBG_ENTER(ZONE_FCT_OSWRAP_THREAD, "Func=%p, pData=%p, Name=%s", threadfn, pData, STRPTR(pName));
  hThread = oswrap_GetMem(sizeof(THREADINFO));
  if (hThread) {
    mutex_init(&hThread->muLock);
    hThread->pTask    = NULL;
    hThread->pFunc    = threadfn;
    hThread->pData    = pData;
    hThread->pName    = pName;
    hThread->fStarted = false;
    oswrap_CreateEvent(&hThread->evStarted, true);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_THREAD, "Ret=%p, Name=%s", hThread, STRPTR(pName));
  return hThread;
}

//////////////////////////////////////////////////////////////////////////////
//
// Destroy a given thread.
//
// Parameters:
// hThread: The handle of the thread to be destroyed.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_DestroyThread(THREADHANDLE hThread) {
  DBG_ENTER(ZONE_FCT_OSWRAP_THREAD, "hThread=%p, Name=%s", hThread, THREADNAME(hThread));
  if (hThread) {
    StopThread(hThread);
    oswrap_FreeMem(hThread);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_THREAD, "hThread=%p", hThread);
}

//////////////////////////////////////////////////////////////////////////////
//
// Start a new thread.
//
// Parameters:
// hThread: The handle of the thread to be started.
//
// Return:
// true if the given thread was started, false otherwise.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_StartThread(THREADHANDLE hThread) {
  bool fRet = false;
  DBG_ENTER(ZONE_FCT_OSWRAP_THREAD, "hThread=%p, Name=%s", hThread, THREADNAME(hThread));
  if (hThread) {
    mutex_lock(&hThread->muLock);
    StopThread(hThread);
    oswrap_ResetEvent(&hThread->evStarted);
    hThread->fStarted = true;
    hThread->pTask = kthread_run(oswrap_ThreadFunc, hThread, hThread->pName);
    if (IS_ERR(hThread->pTask)) {
      DBGPRINT(ZONE_ERR, "Error kthread_run()");
      hThread->fStarted = false;
      hThread->pTask = NULL;
    } else {
      // Make sure that the thread is properly started
      if (oswrap_WaitForEvent(&hThread->evStarted, 1000)) {
        DBGPRINT(ZONE_ERR, "Failed to wait for thread start");
        hThread->fStarted = false;
        hThread->pTask = NULL;
      }
    }
    fRet = hThread->fStarted;
    mutex_unlock(&hThread->muLock);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_THREAD, "Ret=%s, hThread=%p, Name=%s", STRBOOL(fRet), hThread, THREADNAME(hThread));
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Stop a running thread unprotected for internal use.
//
// Parameters:
// hThread: The handle of the thread to be stopped.
//
// Return:
// true if the given thread was stopped, false otherwise.
//
//////////////////////////////////////////////////////////////////////////////
bool StopThread(THREADHANDLE hThread) {
  bool fRet = false;
  if (hThread->fStarted) {
    if (hThread->pTask) {
      send_sig(SIGTERM, hThread->pTask, 1);
      if (current != hThread->pTask) {
        kthread_stop(hThread->pTask);
      }
      fRet = true;
    }
    hThread->pTask = NULL;
    hThread->fStarted = false;
  }
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Stop a running thread.
//
// Parameters:
// hThread: The handle of the thread to be stopped.
//
// Return:
// true if the given thread was stopped, false otherwise.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_StopThread(THREADHANDLE hThread) {
  bool fRet = false;
  DBG_ENTER(ZONE_FCT_OSWRAP_THREAD, "hThread=%p, Name=%s", hThread, THREADNAME(hThread));
  if (hThread) {
    mutex_lock(&hThread->muLock);
    fRet = StopThread(hThread);
    mutex_unlock(&hThread->muLock);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_THREAD, "Ret=%s, hThread=%p, Name=%s", STRBOOL(fRet), hThread, THREADNAME(hThread));
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Check if the calling thread should be stopped.
//
// Parameters:
// None.
//
// Return:
// 0 if the calling thread should not be stopped, != 0 if it should be
// stopped.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_ShouldStopThread(void) {
  return kthread_should_stop();
}

/****************************************************************************/
/* workqueue functions                                                      */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// The work callback of a work queue item.
//
// Parameters:
// pItem: The corresponding work queue item. This is part of the
//        WORKQUEUE_ITEM structure.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void oswrap_WorkQueueWork(struct work_struct *pItem) {
  WORKQUEUE_ITEM *pWQueue = (WORKQUEUE_ITEM*)container_of(pItem, WORKQUEUE_ITEM, Work);
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Item=%p", pItem);
  if (pWQueue->pFunc) {
    pWQueue->pFunc(pWQueue->pData);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "");
}

//////////////////////////////////////////////////////////////////////////////
//
// The work callback of a delayed work queue item.
//
// Parameters:
// pItem: The corresponding work queue item. This is part of the
//        WORKQUEUE_ITEM structure  via the surrounding delayed_work
//        structure.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void oswrap_WorkQueueDelayedWork(struct work_struct *pItem) {
  struct delayed_work *pDelayedItem = (struct delayed_work*)container_of(pItem, struct delayed_work, work);
  WORKQUEUE_ITEM *pWQueue = (WORKQUEUE_ITEM*)container_of(pDelayedItem, WORKQUEUE_ITEM, DelayedWork);
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Item=%p", pItem);
  if (pWQueue->pFunc) {
    pWQueue->pFunc(pWQueue->pData);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "");
}

//////////////////////////////////////////////////////////////////////////////
//
// Create a work queue.
//
// Parameters:
// strName: Name of the work queue to be created.
//
// Return:
// The handle of the created work queue, NULL in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
WORKQUEUE_HANDLE oswrap_WorkQueueCreate(const char* strName) {
  WORKQUEUE_HANDLE hRet;
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Name=%s", strName);
  hRet = create_workqueue(strName);
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Ret=%p", hRet);
  return hRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Destroy the given work queue.
//
// Parameters:
// hWorkQueue: Handle of the work queue to be destroyed.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_WorkQueueDestroy(WORKQUEUE_HANDLE hWorkQueue) {
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Workqueue=%p", hWorkQueue);
  if (hWorkQueue) {
    destroy_workqueue(hWorkQueue);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Workqueue=%p", hWorkQueue);
}

//////////////////////////////////////////////////////////////////////////////
//
// Flush the given work queue.
//
// Parameters:
// hWorkQueue: Handle of the work queue to be flushed.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_WorkQueueFlush(WORKQUEUE_HANDLE hWorkQueue) {
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Workqueue=%p", hWorkQueue);
  flush_workqueue(hWorkQueue);
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Workqueue=%p", hWorkQueue);
}

//////////////////////////////////////////////////////////////////////////////
//
// Initialize a given work queue item.
//
// Parameters:
// pItem: The item to be intialized.
// pFunc: The work call back function.
// pData: User data to be passed to the callback function.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_WorkQueueInitItem(WORKQUEUE_ITEM *pItem, void (*pFunc)(void *), void *pData) {
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Item=%p, Func=%p, Data=%p", pItem, pFunc, pData);
  pItem->pFunc = pFunc;
  pItem->pData = pData;
  INIT_WORK(&pItem->Work, oswrap_WorkQueueWork);
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Item=%p", pItem);
}

//////////////////////////////////////////////////////////////////////////////
//
// Initialize a given delayed work queue item.
//
// Parameters:
// pItem: The item to be intialized.
// pFunc: The work call back function.
// pData: User data to be passed to the callback function.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_WorkQueueInitDelayedItem(WORKQUEUE_ITEM *pItem, void (*pFunc)(void *), void *pData) {
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Item=%p, Func=%p, Data=%p", pItem, pFunc, pData);
  pItem->pFunc = pFunc;
  pItem->pData = pData;
  INIT_DELAYED_WORK(&pItem->DelayedWork, oswrap_WorkQueueDelayedWork);
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Item=%p", pItem);
}

//////////////////////////////////////////////////////////////////////////////
//
// Queue the given item on the given work queue.
//
// Parameters:
// hWorkQueue: Handle of the work queue to be handled.
// pItem     : Item to be scheduled on the work queue.
//
// Return:
// true on success, false on failure.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_WorkQueueQueueItem(WORKQUEUE_HANDLE hWorkQueue, WORKQUEUE_ITEM *pItem) {
  bool fRet;
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Workqueue=%p, Item=%p", hWorkQueue, pItem);
  fRet = queue_work(hWorkQueue, &pItem->Work);
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Ret=%s, Workqueue=%p, Item=%p", STRBOOL(fRet), hWorkQueue, pItem);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Queue the given item on the given work queue with a given delay.
//
// Parameters:
// hWorkQueue: Handle of the work queue to be handled.
// pItem     : Item to be scheduled on the work queue.
// ilDelay   : Delay in milliseconds before the work queue item is started.
//
// Return:
// true on success, false on failure.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_WorkQueueQueueItemDelayed(WORKQUEUE_HANDLE hWorkQueue, WORKQUEUE_ITEM *pItem, unsigned long ulDelay) {
  bool fRet;
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Workqueue=%p, Item=%p, Delay=%d", hWorkQueue, pItem, ulDelay);
  fRet = queue_delayed_work(hWorkQueue, &pItem->DelayedWork, msecs_to_jiffies(ulDelay));
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Ret=%s, Workqueue=%p, Item=%p", STRBOOL(fRet), hWorkQueue, pItem);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Cancel the given delayed item from the work queue.
//
// Parameters:
// pItem: Item to be canceled.
//
// Return:
// true if the item was pending and has been canceled, false otherwise.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_WorkQueueCancelDelayedWork(WORKQUEUE_ITEM *pItem) {
  bool fRet;
  DBG_ENTER(ZONE_FCT_OSWRAP_WQUEUE, "Item=%p", pItem);
  fRet = cancel_delayed_work(&pItem->DelayedWork);
  DBG_LEAVE(ZONE_FCT_OSWRAP_WQUEUE, "Ret=%s, Item=%p", STRBOOL(fRet), pItem);
  return fRet;
}

/****************************************************************************/
/* tty functions for lower layer (base port)                                */
/****************************************************************************/

#define OPEN_FLAGS  (O_RDWR | O_NOCTTY)
// O_RDWR    : read and write
// O_NOCTTY  : no console
// O_NONBLOCK: do not block read and write calls
// O_SYNC    : wait in every write call until it is finished

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
  #define TTY_FROM_PORT(p) ((struct tty_struct*)((struct tty_file_private*)p->pFile->private_data)->tty)
#else // KERNEL_VERSION
  #define TTY_FROM_PORT(p) ((struct tty_struct*)p->pFile->private_data)
#endif // KERNEL_VERSION

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
  #define TTY_PARAMS(p)    TTY_FROM_PORT(p)
#else // KERNEL_VERSION
  #define TTY_PARAMS(p)    TTY_FROM_PORT(p), p->pFile
#endif // KERNEL_VERSION

//////////////////////////////////////////////////////////////////////////////
//
// Open an existing tty port.
//
// Parameters:
// pTtyPathName: The device name of the port to be opened.
//
// Return:
// A handle of the opened port, NULL in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
PORTHANDLE oswrap_TtyOpen(const char * pTtyPathName) {
  PORTHANDLE    pTtyFile = NULL;
  bool          bExistsInterface = true;

  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "Name=%s", STRPTR(pTtyPathName));

  if (pTtyPathName) {
    pTtyFile = oswrap_GetMem(sizeof(_PORTHANDLE));
    pTtyFile->pFile = NULL;

    sprintf(pTtyFile->strPortName, "%s", pTtyPathName);

    pTtyFile->pFile = filp_open(pTtyPathName, OPEN_FLAGS, 0);
    if (IS_ERR(pTtyFile->pFile)) {
      DBGPRINT(ZONE_ERR, "error opening file. errno=%d", -PTR_ERR(pTtyFile->pFile));
      oswrap_FreeMem(pTtyFile);
      pTtyFile = NULL;
    } else {
      DBGPRINT(ZONE_GENERAL_INFO, "file (%s) successfully opened", pTtyPathName);
      // test interface function pointer
      if (pTtyFile->pFile->f_op->read == NULL) {
        DBGPRINT(ZONE_ERR, "file has no read operation registered!");
        bExistsInterface = false;
      }
      if (pTtyFile->pFile->f_op->unlocked_ioctl == NULL) {
        DBGPRINT(ZONE_ERR, "file has no unlocked_ioctl operation registered!");
        bExistsInterface = false;
      }
      if (pTtyFile->pFile->f_op->poll == NULL) {
        DBGPRINT(ZONE_ERR, "file has no poll operation registered!");
        bExistsInterface = false;
      }
      if (!bExistsInterface) {
        // close tty port
        oswrap_TtyClose(pTtyFile);
        pTtyFile = NULL;
      }
    }
  }

  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%p, Name=%s", pTtyFile, STRPTR(pTtyPathName));
  return pTtyFile;
}

//////////////////////////////////////////////////////////////////////////////
//
// Close a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port to be closed.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyClose(PORTHANDLE pTtyFile) {
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p", pTtyFile);
  if (pTtyFile) {
    if (pTtyFile->pFile) {
      filp_close(pTtyFile->pFile, NULL);
    }
    oswrap_FreeMem(pTtyFile);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=0, TtyFile=%p", pTtyFile);
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// This function is called during system suspend. We save the corresponding
// port settings and close it.
//
// Parameters:
// pTtyFile: The handle of the corresponding port.
//
// Return:
// true on success, false on failure.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_TtySuspend(PORTHANDLE pTtyFile) {
  bool fRet = false;
  DBG_ENTER(ZONE_FCT_POWER_MGM, "TtyFile=%p", pTtyFile);
  if (pTtyFile && pTtyFile->pFile) {
    fRet = true;
    // save termios settings
    if (oswrap_TtyIoctl(pTtyFile, TCGETS, (unsigned long)&pTtyFile->settings)) {
      DBGPRINT(ZONE_ERR, "Failed to save termios settings");
      fRet = false;
    }
    // Save the current modem signals
    if (oswrap_TtyGetModemLines(pTtyFile, &pTtyFile->ulLines)) {
      DBGPRINT(ZONE_ERR, "Failed to save current modem signals");
      fRet = false;
    }
    // Stop data flow
    if (oswrap_TtyStopDataFlow(pTtyFile)) {
      DBGPRINT(ZONE_ERR, "Failed to stop data flow");
      fRet = false;
    }
    // Close the port
    filp_close(pTtyFile->pFile, NULL);
    pTtyFile->pFile = NULL;
  }
  DBG_LEAVE(ZONE_FCT_POWER_MGM, "Ret=%d, TtyFile=%p", fRet, pTtyFile);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// This function is called during system resume. We reopen the corresponding
// port and restore the settings saved during suspend.
//
// Parameters:
// pTtyFile: The handle of the corresponding port.
//
// Return:
// true on success, false on failure.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_TtyResume(PORTHANDLE pTtyFile) {
  bool fRet = false;
  DBG_ENTER(ZONE_FCT_POWER_MGM, "TtyFile%p", pTtyFile);
  if (pTtyFile && !pTtyFile->pFile) {
    pTtyFile->pFile = filp_open(pTtyFile->strPortName, OPEN_FLAGS, 0);
    if (IS_ERR(pTtyFile->pFile)) {
      DBGPRINT(ZONE_ERR, "error opening file. errno=%d", -PTR_ERR(pTtyFile->pFile));
      pTtyFile->pFile = NULL;
    } else {
      DBGPRINT(ZONE_GENERAL_INFO, "file (%s) successfully opened", pTtyFile->strPortName);
      fRet = true;
      // Restore modem signals
      if (oswrap_TtySetModemLines(pTtyFile, pTtyFile->ulLines)) {
        DBGPRINT(ZONE_ERR, "Failed to restore modem signals");
        fRet = false;
      }
      // Start data flow again
      if (oswrap_TtyStartDataFlow(pTtyFile)) {
        DBGPRINT(ZONE_ERR, "Failed to start data flow");
        fRet = false;
      }
      // Restore termios settings
      if (oswrap_TtyIoctl(pTtyFile, TCSETS, (unsigned long)&pTtyFile->settings))  {
        DBGPRINT(ZONE_ERR, "Failed to restore termios settings");
        fRet = false;
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_POWER_MGM, "Ret=%s, TtyFile=%p", STRBOOL(fRet), pTtyFile);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Read data from a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port to be read from.
// pBuf    : The buffer receiving the data.
// iCount  : The size of the buffer (max. number of bytes to be read).
//
// Return:
// The number of bytes read, a negative value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyRead(PORTHANDLE pTtyFile, unsigned char *pBuf, int iCount) {
  int iResult = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Buf=%p, Count=%i", pTtyFile, pBuf, iCount);
  if (pTtyFile) {
    iResult = -EINVAL;
    if (pBuf && (iCount > 0)) {
      iResult = -EAGAIN;
      if (pTtyFile->pFile) {
        mm_segment_t oldfs = get_fs();
        set_fs(KERNEL_DS);
        pTtyFile->pFile->f_pos = 0;
        iResult = pTtyFile->pFile->f_op->read(pTtyFile->pFile, pBuf, iCount, &pTtyFile->pFile->f_pos);
        set_fs(oldfs);
        if (iResult < 0) {
          DBGPRINT(ZONE_ERR, "tty read errno %d", iResult);
        } else {
          DBGPRINT(ZONE_RAW_DATA, "%d of %d bytes read:", iResult, iCount);
          MUXDBGHEX(ZONE_RAW_DATA, pBuf, iResult);
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iResult, pTtyFile);
  return iResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// Read data with a timeout from a previoulsy opened tty port.
//
// Parameters:
// pTtyFile : The handle of the port to be read from.
// pBuf     : The buffer receiving the data.
// iCount   : The size of the buffer (max. number of bytes to be read).
// ulTimeout: The timeout in milli seconds after which the read aborts.
//
// Return:
// The number of bytes read, a negative value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyReadTimeout(PORTHANDLE pTtyFile, unsigned char *pBuf, int iCount, unsigned long ulTimeout) {
  int iResult = 0;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Buf=%p, Count=%i, Timeout=%d", pTtyFile, pBuf, iCount, ulTimeout);
  if (pTtyFile && pBuf && (iCount > 0)) {
    BYTE bVMin, bVTime;
    iResult = oswrap_TtyGetTiming(pTtyFile, &bVMin, &bVTime);
    if (iResult == 0) {
      iResult = oswrap_TtySetTiming(pTtyFile, 0, 1);
      if (iResult == 0) {
        u64 u64Start = get_jiffies_64();
        while (((iResult == 0) || (iResult == -EAGAIN)) && (jiffies_to_msecs(get_jiffies_64() - u64Start) < ulTimeout)) {
          iResult = oswrap_TtyRead(pTtyFile, pBuf, iCount);
        }
        oswrap_TtySetTiming(pTtyFile, bVMin, bVTime);
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iResult, pTtyFile);
  return iResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// Write data to a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port to be read from.
// pBuf    : The buffer containing the data to be written.
// iCount  : The number of bytes to write.
//
// Return:
// The number of bytes written, a negative value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyWrite(PORTHANDLE pTtyFile, const unsigned char *pBuf, int iCount) {
  int iResult = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Buf=%p, Count=%i", pTtyFile, pBuf, iCount);
  if (pTtyFile) {
    iResult = -EINVAL;
    if (pBuf && (iCount > 0)) {
      iResult = -EAGAIN;
      if (pTtyFile->pFile) {
        struct tty_struct *pTty = TTY_FROM_PORT(pTtyFile);
        if (pTty->ops->write) {
          iResult = pTty->ops->write(pTty, pBuf, iCount);
        } else {
          mm_segment_t oldfs = get_fs();
          set_fs(KERNEL_DS);
          pTtyFile->pFile->f_pos = 0;
          iResult = pTtyFile->pFile->f_op->write(pTtyFile->pFile, pBuf, iCount, &pTtyFile->pFile->f_pos);
          set_fs(oldfs);
        }
        if (iResult < 0) {
          DBGPRINT(ZONE_ERR, "tty write errno %d", iResult);
        } else {
          DBGPRINT(ZONE_RAW_DATA, "%d of %d bytes written:", iResult, iCount);
          MUXDBGHEX(ZONE_RAW_DATA, pBuf, iResult);
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iResult, pTtyFile);
  return iResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// Call the ioctl handler of a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port to be read from.
// uiCmd   : The ioctl to be send to the port.
// ulArg   : The argument of the given ioctl.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyIoctl(PORTHANDLE pTtyFile, unsigned int uiCmd, unsigned long ulArg) {
  int iResult = -ENOSYS;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Cmd=0x%x, Arg=%p", pTtyFile, uiCmd, ulArg);
  if (pTtyFile) {
    mm_segment_t oldfs = get_fs();
    set_fs(KERNEL_DS);
    iResult = pTtyFile->pFile->f_op->unlocked_ioctl(pTtyFile->pFile, uiCmd, ulArg);
    set_fs(oldfs);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iResult, pTtyFile);
  return iResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// Initialize a previoulsy opened tty port for usage with the multiplex
// protocol.
//
// Parameters:
// pTtyFile  : The handle of the port to be read from.
// ulBaudRate: The baud rate to be used.
//
// Return:
// true in case of success, false in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_TtySetParams(PORTHANDLE pTtyFile, unsigned long ulBaudRate) {
  bool fRet = false;

  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Baudrate=%d", pTtyFile, ulBaudRate);
  if (pTtyFile) {
    struct termios settings;
    MEMCLR(&settings, sizeof(settings));
    settings.c_cflag = CS8 | CREAD | CRTSCTS | CLOCAL | HUPCL;
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;

    switch(ulBaudRate) {
      case   2400: settings.c_cflag |=   B2400; break;
      case   4800: settings.c_cflag |=   B4800; break;
      case   9600: settings.c_cflag |=   B9600; break;
      case  19200: settings.c_cflag |=  B19200; break;
      case  38400: settings.c_cflag |=  B38400; break;
      case  57600: settings.c_cflag |=  B57600; break;
      case 115200: settings.c_cflag |= B115200; break;
      case 230400: settings.c_cflag |= B230400; break;
      case 460800: settings.c_cflag |= B460800; break;
      case 921600: settings.c_cflag |= B921600; break;
      default    : settings.c_cflag  =       0; break;
    }
    if (oswrap_TtyIoctl(pTtyFile, TCSETS, (unsigned long)(&settings)) == 0) {
      oswrap_TtyFlushBuffers(pTtyFile, true, true);
      fRet = true;
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%s, TtyFile=%p", STRBOOL(fRet), pTtyFile);
  return fRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get the read timing of a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port.
// pVMin   : Pointer to byte value receiving the VMIN value (output).
// pVTime  : Pointer to byte value receiving the VTIME value (output).
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyGetTiming(PORTHANDLE pTtyFile, PBYTE pVMin, PBYTE pVTime) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, pVMin=%p, pVTime=%p", pTtyFile, pVMin, pVTime);
  if (pTtyFile) {
    struct termios settings;
    iRet = oswrap_TtyIoctl(pTtyFile, TCGETS, (unsigned long)&settings);
    if (iRet == 0) {
      *pVMin = settings.c_cc[VMIN];
      *pVTime = settings.c_cc[VTIME];
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, VMin=%u, VTime=%u, TtyFile=%p", iRet, *pVMin, *pVTime, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Set the read timing of a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port.
// pVMin   : The VMIN value to be set.
// pVTime  : The VTIME value to be set.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtySetTiming(PORTHANDLE pTtyFile, BYTE VMin, BYTE VTime) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, VMin=%d, VTime=%d", pTtyFile, VMin, VTime);
  if (pTtyFile) {
    struct termios settings;
    iRet = oswrap_TtyIoctl(pTtyFile, TCGETS, (unsigned long)&settings);
    if (iRet == 0) {
      settings.c_cc[VMIN] = VMin;
      settings.c_cc[VTIME] = VTime;
      iRet = oswrap_TtyIoctl(pTtyFile, TCSETS, (unsigned long)&settings);
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iRet, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Stop flow of incoming data by clearing the RTS signal.
//
// Parameters:
// pTtyFile: The handle of the port.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyStopDataFlow(PORTHANDLE pTtyFile) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p", pTtyFile);
  if (pTtyFile) {
    struct termios settings;
    iRet = oswrap_TtyIoctl(pTtyFile, TCGETS, (unsigned long)(&settings));
    if (iRet == 0) {
      settings.c_cflag &= ~CRTSCTS;
      iRet = oswrap_TtyIoctl(pTtyFile, TCSETS, (unsigned long)(&settings));
      if (iRet == 0) {
        unsigned long ulLines;
        iRet = oswrap_TtyGetModemLines(pTtyFile, &ulLines);
        if (iRet == 0) {
          ulLines &= ~TIOCM_RTS;
          iRet = oswrap_TtySetModemLines(pTtyFile, ulLines);
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iRet, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Start flow of incoming data by setting the RTS signal.
//
// Parameters:
// pTtyFile: The handle of the port.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyStartDataFlow(PORTHANDLE pTtyFile) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p", pTtyFile);
  if (pTtyFile) {
    unsigned long ulLines;
    iRet = oswrap_TtyGetModemLines(pTtyFile, &ulLines);
    if (iRet == 0) {
      ulLines |= TIOCM_RTS;
      iRet = oswrap_TtySetModemLines(pTtyFile, ulLines);
      if (iRet == 0) {
        struct termios settings;
        iRet = oswrap_TtyIoctl(pTtyFile, TCGETS, (unsigned long)(&settings));
        if (iRet == 0) {
          settings.c_cflag |= CRTSCTS;
          iRet = oswrap_TtyIoctl(pTtyFile, TCSETS, (unsigned long)(&settings));
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iRet, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Wait for a change of the modem signals of a previoulsy opened tty port.
//
// Parameters:
// pTtyFile  : The handle of the port.
// ulLineMask: The mask of the signals to wait for.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyWaitChangeModemLine(PORTHANDLE pTtyFile, unsigned long ulLineMask) {
  int iRet;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Mask=0x%x", pTtyFile, ulLineMask);
  iRet = oswrap_TtyIoctl(pTtyFile, TIOCMIWAIT, ulLineMask);
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iRet, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get the state of the modem signals of a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port.
// pulLines: Pointer to a value receiving the mask of the modem signals
//           (output).
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyGetModemLines(PORTHANDLE pTtyFile, unsigned long *pulLines) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, pLines=%p", pTtyFile, pulLines);
  if (pTtyFile) {
    iRet = -EINVAL;
    if (pulLines) {
      iRet = -EAGAIN;
      if (pTtyFile->pFile) {
        struct tty_struct *pTty = TTY_FROM_PORT(pTtyFile);
        if (pTty->ops->tiocmget) {
          iRet = pTty->ops->tiocmget(TTY_PARAMS(pTtyFile));
          if (iRet >= 0) {
            *pulLines = iRet;
            iRet = 0;
          }
        } else {
          iRet = oswrap_TtyIoctl(pTtyFile, TIOCMGET, (unsigned long)pulLines);
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, Lines=0x%x, TtyFile=%p", iRet, *pulLines, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Set the state of the modem signals of a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port.
// ulLines : The mask of the signals to be set.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtySetModemLines(PORTHANDLE pTtyFile, unsigned long ulLines) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Lines=0x%x", pTtyFile, ulLines);
  if (pTtyFile) {
    iRet = -EAGAIN;
    if (pTtyFile->pFile) {
      struct tty_struct *pTty = TTY_FROM_PORT(pTtyFile);
      if (pTty->ops->tiocmset) {
        int iSet = ulLines;
        int iClr = ~ulLines;
#if defined(CLIENT_MODE) && CLIENT_MODE
        iSet &= TIOCM_CTS | TIOCM_DSR | TIOCM_RI | TIOCM_CD | TIOCM_OUT1 | TIOCM_OUT2 | TIOCM_LOOP;
        iClr &= TIOCM_CTS | TIOCM_DSR | TIOCM_RI | TIOCM_CD | TIOCM_OUT1 | TIOCM_OUT2 | TIOCM_LOOP;
#else
        iSet &= TIOCM_DTR | TIOCM_RTS | TIOCM_OUT1 | TIOCM_OUT2 | TIOCM_LOOP;
        iClr &= TIOCM_DTR | TIOCM_RTS | TIOCM_OUT1 | TIOCM_OUT2 | TIOCM_LOOP;
#endif
        iRet = pTty->ops->tiocmset(TTY_PARAMS(pTtyFile), iSet, iClr);
      } else {
        iRet = oswrap_TtyIoctl(pTtyFile, TIOCMSET, (unsigned long)&ulLines);
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iRet, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Toggle the rts and dtr line for the given interval.
//
// Parameters:
// pTtyFile  : The handle of the port.
// ulInterval: The interval for the pulse in milliseconds.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyToggleRtsDtr(PORTHANDLE pTtyFile, unsigned long ulInterval) {
  int iRet;
  unsigned long ulLines;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Interval=%d", pTtyFile, ulInterval);
  iRet = oswrap_TtyGetModemLines(pTtyFile, &ulLines);
  if (iRet == 0) {
    iRet = oswrap_TtySetModemLines(pTtyFile, ulLines & ~TIOCM_DTR & ~TIOCM_RTS);
    if (iRet == 0) {
      oswrap_Sleep(ulInterval);
      iRet = oswrap_TtySetModemLines(pTtyFile, ulLines | TIOCM_DTR | TIOCM_RTS);
      if (iRet == 0) {
        oswrap_Sleep(ulInterval);
        iRet = oswrap_TtySetModemLines(pTtyFile, ulLines);
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iRet, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get number of unsent bytes in the tx queue of a previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port.
// pulLen  : Pointer to a value receiving the number of bytes in the tx queue
//           (output).
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyGetBytesInTxQueue(PORTHANDLE pTtyFile, unsigned long *pulLen) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, pNum=%p", pTtyFile, pulLen);
  if (pTtyFile) {
    iRet = -EINVAL;
    if (pulLen) {
      iRet = -EAGAIN;
      if (pTtyFile->pFile) {
        struct tty_struct *pTty = TTY_FROM_PORT(pTtyFile);
        if (pTty->ops->chars_in_buffer) {
          iRet = pTty->ops->chars_in_buffer(pTty);
          if (iRet >= 0) {
            *pulLen = iRet;
            iRet = 0;
          }
        } else {
          iRet = oswrap_TtyIoctl(pTtyFile, TIOCOUTQ, (unsigned long)pulLen);
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, Num=%d, TtyFile=%p", iRet, *pulLen, pTtyFile);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Flush the tx and rx queue (delete all unsent and unread data) of a
// previoulsy opened tty port.
//
// Parameters:
// pTtyFile: The handle of the port.
// fRx     : Flag indicating if the rx queue should be emprtied.
// fRx     : Flag indicating if the tx queue should be emprtied.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_TtyFlushBuffers(PORTHANDLE pTtyFile, bool fRx, bool fTx) {
  int iRet = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "TtyFile=%p, Rx=%s, Tx=%s", pTtyFile, STRBOOL(fRx), STRBOOL(fTx));
  if (pTtyFile) {
    if (fRx && fTx) {
      iRet = oswrap_TtyIoctl(pTtyFile, TCFLSH, TCIOFLUSH);
    } else if (fTx) {
      iRet = oswrap_TtyIoctl(pTtyFile, TCFLSH, TCOFLUSH);
    } else if (fRx) {
      iRet = oswrap_TtyIoctl(pTtyFile, TCFLSH, TCIFLUSH);
    } else {
      iRet = 0;
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, TtyFile=%p", iRet, pTtyFile);
  return iRet;
}

/****************************************************************************/
/* tty functions for upper layer (mux channel)                              */
/****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
  #define UPPERPORT(hPort) hPort->hTty->port
#else // KERNEL_VERSION
  #define UPPERPORT(hPort) hPort->hTty
#endif // KERNEL_VERSION

//////////////////////////////////////////////////////////////////////////////
//
// Create an upper port device.
//
// Parameters:
// hPort: Handle identifying the upper port device.
//
// Return:
// true in case of success, false in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_UpperPortCreate(UPPERPORTHANDLE hPort) {
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "Port=%p", hPort);
  hPort->hTty = NULL;
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=true, Port=%p", hPort);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Destroy an upper port device.
//
// Parameters:
// hPort: Handle identifying the upper port device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_UpperPortDestroy(UPPERPORTHANDLE hPort) {
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "Port=%p", hPort);
  oswrap_UpperPortUnregister(hPort);
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Port=%p", hPort);
}

//////////////////////////////////////////////////////////////////////////////
//
// Register a tty device for the given upper port an upper port handle.
//
// Parameters:
// hPort: Handle identifying the upper port device.
// hTty : Handle identifying the corresponding tty object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_UpperPortRegister(UPPERPORTHANDLE hPort, TTYHANDLE hTty) {
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "Port=%p, Tty=%p, Index=%d", hPort, hTty, hTty->index);
  hPort->hTty = hTty;
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Port=%p, Tty=%p", hPort, hPort->hTty);
}

//////////////////////////////////////////////////////////////////////////////
//
// Unregister a tty device from the given upper port an upper port handle.
//
// Parameters:
// hPort: Handle identifying the upper port device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_UpperPortUnregister(UPPERPORTHANDLE hPort) {
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "Port=%p", hPort);
  if (hPort->hTty) {
    tty_ldisc_flush(hPort->hTty);
  }
  hPort->hTty = NULL;
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Port=%p", hPort);
}

//////////////////////////////////////////////////////////////////////////////
//
// Write data in the tty line devices read buffer.
//
// Parameters:
// hPort  : The handle identifying the upper port device.
// pBuf   : Buffer with data to be written into the read queue.
// uiCount: The number of bytes to be written into the read queue.
//
// Return:
// The number of bytes written into the read queue, a negative value
// indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_UpperPortPushData(UPPERPORTHANDLE hPort, unsigned char *pBuf, unsigned int uiCount) {
  int iResult = -ENODEV;
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "Port=%p, Tty=%p, Index=%d, Buf=%p, Count=%d", hPort, hPort->hTty, hPort->hTty ? hPort->hTty->index : -1, pBuf, uiCount);
  if (hPort->hTty) {
    iResult = tty_insert_flip_string(UPPERPORT(hPort), pBuf, uiCount);
    tty_flip_buffer_push(UPPERPORT(hPort));
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Ret=%i, Port=%p, Tty=%p", iResult, hPort, hPort->hTty);
  return iResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// Signal the tty line devices that it can continue to send data into the
// connected tty device (a multiplex channel in our case).
//
// Parameters:
// hPort  : The handle identifying the upper port device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_UpperPortContinueSending(UPPERPORTHANDLE hPort) {
  DBG_ENTER(ZONE_FCT_OSWRAP_TTY, "Port=%p, Tty=%p, Index=%d", hPort, hPort->hTty, hPort->hTty ? hPort->hTty->index : -1);
  if (hPort->hTty) {
    tty_wakeup(hPort->hTty);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_TTY, "Port=%p, Tty=%p", hPort, hPort->hTty);
}

/****************************************************************************/
/* utilities                                                                */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Allocate a block of memory.
//
// Parameters:
// ulSize: The size of the memory block in bytes.
//
// Return:
// A pointer to the allocated block of memory, NULL in case of failure.
//
//////////////////////////////////////////////////////////////////////////////
void *oswrap_GetMem(unsigned long ulSize) {
  void *pMem;
  DBG_ENTER(ZONE_FCT_OSWRAP_UTILITY, "Size=%d", ulSize);
  pMem = vmalloc(ulSize);
  if (pMem == NULL) {
    DBGPRINT(ZONE_ERR | ZONE_FCT_OSWRAP_UTILITY, "Out of memory!");
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_UTILITY, "Ret=%p", pMem);
  return pMem;
}

//////////////////////////////////////////////////////////////////////////////
//
// Free a previously allocated a block of memory.
//
// Parameters:
// pAddr: Pointer to the memory block to be freed.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_FreeMem(const void *pAddr) {
  DBG_ENTER(ZONE_FCT_OSWRAP_UTILITY, "Ptr=%p", pAddr);
  if (pAddr) {
    vfree(pAddr);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_UTILITY, "");
}

//////////////////////////////////////////////////////////////////////////////
//
// Delay the calling thread for the given number of milli seconds.
//
// Parameters:
// iTime: The delay in milli seconds.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_Sleep(int iTime) {
  DBG_ENTER(ZONE_FCT_OSWRAP_UTILITY, "Time=%d", iTime);
  if (iTime) {
    msleep_interruptible(iTime);
  } else {
    // If we're called with timeout zero we want to sleep for the shortest possible time
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
    // On kernel 2.6.36 and above we have the usleep_range() function which allows a
    // sufficient short delay without causing processor load.
    usleep_range(1, 2);
#else // KERNEL_VERSION
    // On kernels older than 2.6.36 we don't have an usleep_range()!
    msleep_interruptible(1);
#endif // KERNEL_VERSION
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_UTILITY, "");
}

//////////////////////////////////////////////////////////////////////////////
//
// Get a tick count value in milli seconds since system start.
//
// Parameters:
// None.
//
// Return:
// The tick count in milli seconds since system start.
//
//////////////////////////////////////////////////////////////////////////////
unsigned long long oswrap_GetTickCount(void) {
  u64 u64Ret;
  DBG_ENTER(ZONE_FCT_OSWRAP_UTILITY, "");
  u64Ret = jiffies_to_msecs(get_jiffies_64());
  DBG_LEAVE(ZONE_FCT_OSWRAP_UTILITY, "Ret=%llu", u64Ret);
  return u64Ret;
}

//////////////////////////////////////////////////////////////////////////////
//
// Check if a given port is a virtual USB port.
//
//
// Parameters:
// hPort: File handle of the opened port.
//
// Return:
// true if the port is an USB port, false otherwise.
//
//////////////////////////////////////////////////////////////////////////////
bool oswrap_IsUsbPort(PORTHANDLE hPort) {
  bool fRet = false;
  DBG_ENTER(ZONE_FCT_OSWRAP_UTILITY, "hPort=%p", hPort);
  if (hPort) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
    if (hPort->pFile) {
      struct tty_file_private *pPrivTty = hPort->pFile->private_data;
      if (pPrivTty && pPrivTty->tty) {
        struct tty_struct *pTty = pPrivTty->tty;
        if (pTty && pTty->dev) {
          struct device *pDev = pTty->dev;
          while (pDev){
            if (pDev->bus) {
              DBGPRINT(ZONE_FCT_OSWRAP_UTILITY, "Bus-Name=%s", pDev->bus->name);
              // The bus name of direct usb connections is "usb".
              // For Usb2Tty adapters, the bus name is "usb-serial" and must not be handled as usb connections.
              if (strstr(pDev->bus->name, "usb-serial")) {
                fRet = false;
                // For devices with "usb-serial" bus, there is a device with the "usb" bus up the device tree.
                // Therefore we must break now and not let fRet be overwritten when parent device is checked.
                break;
              } else if (strstr(pDev->bus->name, "usb")) {
                fRet = true;
                break;
              }
            }
            pDev = pDev->parent;
          }
        }
      }
    }
#else // KERNEL_VERSION
    // Because we don't have a back reference to the device inside tty_struct
    // in older kernels we're doing a simple string match of the device name
    // instead to keep things simple. This must be extended if required.
    if (strstr(hPort->strPortName, "USB") || strstr(hPort->strPortName, "ACM")) {
      fRet = true;
    }
#endif // KERNEL_VERSION
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_UTILITY, "Ret=%s", STRBOOL(fRet));
  return fRet;
}

/****************************************************************************/
/* semaphore                                                                */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Initialize a semaphore object.
//
// Parameters:
// pSem : Pointer to the semaphore object to be initialized.
// iInit: The initial count of the semaphore. The semaphore is locked while
//        the count is 0.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_CreateSemaphore(SEMHANDLE *pSem, int iInit) {
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Sem=%p, Count=%d", pSem, iInit);
  if (pSem) {
    sema_init(pSem, iInit);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Sem=%p", pSem);
}

//////////////////////////////////////////////////////////////////////////////
//
// Decrement the counter of the a semaphore object, if it is already 0 wait
// for the semaphore to be released
//
// Parameters:
// pSem     : Pointer to the semaphore object.
// ulTimeout: The timeout in milli seconds for the wait operation.
//
// Return:
// 0 indicates success, any other value indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_WaitForSemaphore(SEMHANDLE *pSem, unsigned long ulTimeout) {
  int iRet = 0;
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Sem=%p, Timeout=%u", pSem, ulTimeout);
  if (pSem) {
    if (ulTimeout != SEM_WAIT_INFINITE) {
      iRet = down_timeout(pSem, msecs_to_jiffies(ulTimeout));
    } else {
      iRet = down_interruptible(pSem);
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Ret=%d, Sem=%p", iRet, pSem);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Increment the counter of the a semaphore object.
//
// Parameters:
// pSem: Pointer to the semaphore object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_ReleaseSemaphore(SEMHANDLE *pSem) {
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Sem=%p", pSem);
  if (pSem) {
    up(pSem);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Sem=%p", pSem);
}

//////////////////////////////////////////////////////////////////////////////
//
// Reset the semaphore object back to counter 0.
//
// Parameters:
// pSem: Pointer to the semaphore object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_ResetSemaphore(SEMHANDLE *pSem) {
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Sem=%p", pSem);
  if (pSem) {
    while(down_trylock(pSem) == 0);
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Sem=%p", pSem);
}

/****************************************************************************/
/* event                                                                    */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Initialize an event object. An event object provides a signalization
// mechanism between different threads.
//
// Parameters:
// pEvent : Pointer to the event object to be initialized.
// fManual: Flag indicating if the event setting and resetting should be
//          manual (true) or automatic (false). In case of a manual event it
//          remains signaled until the oswrap_SetEvent() is called. In case
//          of an automatic event it is reseted after a oswrap_WaitForEvent()
//          call has been released automatically.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_CreateEvent(EVTHANDLE *pEvent, bool fManual) {
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Event=%p, Manual=%s", pEvent, STRBOOL(fManual));
  if (pEvent) {
    init_waitqueue_head(&(pEvent->hEvent));
    pEvent->fManual = fManual;
    pEvent->fRelease = false;
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Event=%p", pEvent);
}

//////////////////////////////////////////////////////////////////////////////
//
// Wait for the given event to become signaled. The calling thread will be
// set to sleeping state until the event becomes signaled or the timout
// expires.
//
// Parameters:
// pEvent   : Pointer to the event object to be waited for.
// ulTimeout: Timout in milli seconds for the wait operation.
//
// Return:
// = 0: The event has been signaled.
// = 1: A timeout has occurred.
// < 0: An error has occurred.
//
//////////////////////////////////////////////////////////////////////////////
int oswrap_WaitForEvent(EVTHANDLE *pEvent, unsigned long ulTimeout) {
  int iRet = 0;
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Event=%p, Timeout=%u", pEvent, ulTimeout);
  if (pEvent) {
    if (!pEvent->fManual) {
      pEvent->fRelease = false;
    }
    if (ulTimeout != SEM_WAIT_INFINITE) {
      if (ulTimeout != 0) {
        iRet = wait_event_interruptible_timeout(pEvent->hEvent, pEvent->fRelease, msecs_to_jiffies(ulTimeout));
        if (iRet == 0) {
          iRet = pEvent->fRelease ? 0 : 1;
        } else if (iRet > 0) {
          iRet = 0;
        }
      } else {
        iRet = pEvent->fRelease ? 0 : 1;
      }
    } else {
      iRet = wait_event_interruptible(pEvent->hEvent, pEvent->fRelease);
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Ret=%i, Event=%p", iRet, pEvent);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Set the event to signaled state. If the event is a manual event all
// waiting oswrap_WaitForEvent() will be released and all following calls of
// oswrap_WaitForEvent() won't block until oswrap_ResetEvent() is called.
// In case of an automatic event only one waiting oswrap_WaitForEvent() call
// will be released and the event is then set to unsignaled state
// automatically. If there is no waiting oswrap_WaitForEvent() call the next
// call of oswrap_WaitForEvent() will be continue execution immediately with
// the event state set to unsignaled then.
//
// Parameters:
// pEvent: Pointer to the event object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_SetEvent(EVTHANDLE *pEvent) {
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Event=%p", pEvent);
  if (pEvent) {
    pEvent->fRelease = true;
    if (pEvent->fManual) {
      wake_up_interruptible_all(&(pEvent->hEvent));
    } else {
      wake_up_interruptible(&(pEvent->hEvent));
    }
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Event=%p", pEvent);
}

//////////////////////////////////////////////////////////////////////////////
//
// Set the event to unsignaled state. All following calls of
// oswrap_WaitForEvent() will block independant of being an automatic or
// manual event.
//
// Parameters:
// pEvent: Pointer to the event object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_ResetEvent(EVTHANDLE *pEvent) {
  DBG_ENTER(ZONE_FCT_OSWRAP_SYNC, "Event=%p", pEvent);
  if (pEvent) {
    pEvent->fRelease = false;
  }
  DBG_LEAVE(ZONE_FCT_OSWRAP_SYNC, "Event=%p", pEvent);
}

/****************************************************************************/
/* Synchronization                                                          */
/****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Initialize a spin lock object. A spin lock provides the possibility to
// lock code blocks of being accessed from various threads simultaneously.
//
// Parameters:
// pSL: Pointer to the spin lock object to be initialized.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_SpinLockInitialize(SPINLOCK *pSL) {
  DBG_ENTER(ZONE_FCT_OSWRAP_CRITSECT, "SL=%p", pSL);
  spin_lock_init(&pSL->slLock);
  DBG_LEAVE(ZONE_FCT_OSWRAP_CRITSECT, "SL=%p", pSL);
}

//////////////////////////////////////////////////////////////////////////////
//
// Lock the spin lock.
//
// Parameters:
// pSL: Pointer to the spin lock object to be locked.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_SpinLock(SPINLOCK *pSL) {
  DBG_ENTER(ZONE_FCT_OSWRAP_CRITSECT, "SL=%p", pSL);
  spin_lock_irqsave(&pSL->slLock, pSL->ulFlags);
  DBG_LEAVE(ZONE_FCT_OSWRAP_CRITSECT, "SL=%p", pSL);
}

//////////////////////////////////////////////////////////////////////////////
//
// Unlock the spin lock.
//
// Parameters:
// pSL: Pointer to the spin lock object to be unlocked.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_SpinUnlock(SPINLOCK *pSL) {
  DBG_ENTER(ZONE_FCT_OSWRAP_CRITSECT, "SL=%p", pSL);
  spin_unlock_irqrestore(&pSL->slLock, pSL->ulFlags);
  DBG_LEAVE(ZONE_FCT_OSWRAP_CRITSECT, "SL=%p", pSL);
}

//////////////////////////////////////////////////////////////////////////////
//
// Initialize a mutex object. A mutex provides the possibility to lock code
// of being accessed from various threads simultaneously.
//
// Parameters:
// pMu: Pointer to the mutex object to be initialized.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_MutexInitialize(MUTEX *pMu) {
  DBG_ENTER(ZONE_FCT_OSWRAP_CRITSECT, "Mu=%p", pMu);
  mutex_init(pMu);
  DBG_LEAVE(ZONE_FCT_OSWRAP_CRITSECT, "Mu=%p", pMu);
}

//////////////////////////////////////////////////////////////////////////////
//
// Lock the mutex.
//
// Parameters:
// pMu: Pointer to the mutex object to be locked.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_MutexLock(MUTEX *pMu) {
  DBG_ENTER(ZONE_FCT_OSWRAP_CRITSECT, "Mu=%p", pMu);
  mutex_lock(pMu);
  DBG_LEAVE(ZONE_FCT_OSWRAP_CRITSECT, "Mu=%p", pMu);
}

//////////////////////////////////////////////////////////////////////////////
//
// Unlock the mutex.
//
// Parameters:
// pMu: Pointer to the mutex object to be unlocked.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void oswrap_MutexUnlock(MUTEX *pMu) {
  DBG_ENTER(ZONE_FCT_OSWRAP_CRITSECT, "Mu=%p", pMu);
  mutex_unlock(pMu);
  DBG_LEAVE(ZONE_FCT_OSWRAP_CRITSECT, "Mu=%p", pMu);
}

