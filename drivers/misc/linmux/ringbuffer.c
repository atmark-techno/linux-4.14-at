/******************************************************************************

 (c) 2018 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// ringbuffer.c
//
// This file contains the implementation of the ringbuffer object used as
// internal transfer buffer for write requests to the base tty port.
//
//////////////////////////////////////////////////////////////////////////////

#include "ringbuffer.h"

//////////////////////////////////////////////////////////////////////////////

#define RINGBUF_LOCK(rb)   rb->fStatic ? oswrap_SpinLock(&rb->slLock)   : oswrap_CriticalSectionEnter(&rb->csLock)
#define RINGBUF_UNLOCK(rb) rb->fStatic ? oswrap_SpinUnlock(&rb->slLock) : oswrap_CriticalSectionLeave(&rb->csLock)

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// Initialize the given ringbuffer object and allocate the required resources.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
BOOL ringbuf_Init(RingBuf_t* ringbuf) {
  oswrap_CriticalSectionInitialize(&ringbuf->csLock);
  oswrap_CreateSemaphore(&ringbuf->DataSema, 0);
  ringbuf->fStatic = FALSE;
  ringbuf->bufSize = 0;
  ringbuf->count = 0;
  ringbuf->readIndex = 0;
  ringbuf->writeIndex = 0;
  ringbuf->buf = (UINT8*)oswrap_GetMem(RINGBUF_DEFAULT_SIZE);
  if (ringbuf->buf != NULL) {
    ringbuf->bufSize = RINGBUF_DEFAULT_SIZE;
  }
  return ringbuf->buf ? TRUE : FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
// Initialize the given fixed sized ringbuffer object and allocate the
// required resources.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
// size   : The requested buffer size of the ringbuffer object.
//
// Return:
// TRUE indicates success. FALSE indicates failure.
//
//////////////////////////////////////////////////////////////////////////////
BOOL ringbuf_StaticInit(RingBuf_t* ringbuf, UINT32 size) {
  oswrap_SpinLockInitialize(&ringbuf->slLock);
  oswrap_CreateSemaphore(&ringbuf->DataSema, 0);
  ringbuf->fStatic = TRUE;
  ringbuf->bufSize = 0;
  ringbuf->count = 0;
  ringbuf->readIndex = 0;
  ringbuf->writeIndex = 0;
  ringbuf->buf = (UINT8*)oswrap_GetMem(size);
  if (ringbuf->buf != NULL) {
    ringbuf->bufSize = size;
  }
  return ringbuf->buf ? TRUE : FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
// Destroy the given ringbuffer object and free all related resources.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void ringbuf_Destroy(RingBuf_t* ringbuf) {
  UINT8* buf;
  // reset all with locked spinlock, in case someone tried to access
  RINGBUF_LOCK(ringbuf);
  ringbuf->bufSize = 0;
  ringbuf->count = 0;
  ringbuf->readIndex = 0;
  ringbuf->writeIndex = 0;
  buf = ringbuf->buf;
  ringbuf->buf = NULL;
  RINGBUF_UNLOCK(ringbuf);
  // free buf memory outside of the critical section
  if (buf != NULL) {
    oswrap_FreeMem(buf);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Remove all data from given ringbuffer object.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void ringbuf_Clear(RingBuf_t* ringbuf) {
  RINGBUF_LOCK(ringbuf);
  ringbuf->count = 0;
  ringbuf->readIndex = 0;
  ringbuf->writeIndex = 0;
  oswrap_ResetSemaphore(&ringbuf->DataSema);
  RINGBUF_UNLOCK(ringbuf);
}

//////////////////////////////////////////////////////////////////////////////
//
// Wait until data is available inside the ringbuffer.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
//
// Return:
// TRUE if data is available, FALSE if the corresponding semaphore was
// terminated.
//
//////////////////////////////////////////////////////////////////////////////
BOOL ringbuf_WaitForData(RingBuf_t* ringbuf) {
  return (oswrap_WaitForSemaphore(&ringbuf->DataSema, SEM_WAIT_INFINITE) == EVT_SIGNALED) ? TRUE : FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get the current size of the given ringbuffer object.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
//
// Return:
// The number of bytes currently stored in the buffer.
//
//////////////////////////////////////////////////////////////////////////////
UINT32 ringbuf_GetSize(RingBuf_t* ringbuf) {
  UINT32 result;
  RINGBUF_LOCK(ringbuf);
  result = ringbuf->bufSize;
  RINGBUF_UNLOCK(ringbuf);
  return result;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get the number of bytes stored in the given ringbuffer object.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
//
// Return:
// The number of bytes currently stored in the buffer.
//
//////////////////////////////////////////////////////////////////////////////
UINT32 ringbuf_GetNumWaitingBytes(RingBuf_t* ringbuf) {
  UINT32 result;
  RINGBUF_LOCK(ringbuf);
  result = ringbuf->count;
  RINGBUF_UNLOCK(ringbuf);
  return result;
}

//////////////////////////////////////////////////////////////////////////////
//
// Get the number of free bytes in the given ringbuffer object.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
//
// Return:
// The number of free bytes in the buffer.
//
//////////////////////////////////////////////////////////////////////////////
UINT32 ringbuf_GetNumFreeBytes(RingBuf_t* ringbuf) {
  UINT32 result;
  RINGBUF_LOCK(ringbuf);
  result = ringbuf->bufSize - ringbuf->count;
  RINGBUF_UNLOCK(ringbuf);
  return result;
}

//////////////////////////////////////////////////////////////////////////////
//
// Store data in the in the given ringbuffer object. If no data is passed only
// the semaphore signaling availabe data is released.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
// data   : Pointer to the data to be stored.
// len    : The number of bytes to be stored.
//
// Return:
// The number of actually stored bytes.
//
//////////////////////////////////////////////////////////////////////////////
UINT32 ringbuf_PutBytes(RingBuf_t* ringbuf, UINT8* data, UINT32 len) {
  UINT32 bytesWritten = 0;

  RINGBUF_LOCK(ringbuf);
  if (ringbuf->buf && data && len) {
    UINT32 bytesToWrite = min(len, ringbuf->bufSize - ringbuf->count);
    if (ringbuf->writeIndex + bytesToWrite >= ringbuf->bufSize) {
      UINT32 toCopy = ringbuf->bufSize - ringbuf->writeIndex;
      memcpy(ringbuf->buf + ringbuf->writeIndex, data, toCopy);
      ringbuf->writeIndex = 0;
      bytesToWrite -= toCopy;
      bytesWritten += toCopy;
    }
    // now, we can be sure that the rest of data will fit between ringbuf->writeIndex and buf end
    if (bytesToWrite > 0) {
      memcpy(ringbuf->buf + ringbuf->writeIndex, data + bytesWritten, bytesToWrite);
      ringbuf->writeIndex += bytesToWrite;
      bytesWritten += bytesToWrite;
      // bytesToWrite = 0; // not necessary
    }
    ringbuf->count += bytesWritten;
  }
  oswrap_ReleaseSemaphore(&ringbuf->DataSema);
  RINGBUF_UNLOCK(ringbuf);

  return bytesWritten;
}

//////////////////////////////////////////////////////////////////////////////
//
// Read data out of the in the given ringbuffer object.
//
// Parameters:
// ringbuf: The reference to the corresponding ringbuffer object.
// data   : Pointer to the buffer receiving the data.
// maxlen : The maximum number of bytes to be read.
//
// Return:
// The number of actually read bytes.
//
//////////////////////////////////////////////////////////////////////////////
UINT32 ringbuf_GetBytes(RingBuf_t* ringbuf, UINT8* outData, UINT32 maxLen) {
  UINT32 bytesRead = 0;

  RINGBUF_LOCK(ringbuf);
  if (ringbuf->buf) {
    UINT32 bytesToRead = min(ringbuf->count, maxLen);
    if (ringbuf->readIndex + bytesToRead >= ringbuf->bufSize) {
      UINT32 toCopy = ringbuf->bufSize - ringbuf->readIndex;
      memcpy(outData, ringbuf->buf + ringbuf->readIndex, toCopy);
      ringbuf->readIndex = 0;
      bytesToRead -= toCopy;
      bytesRead += toCopy;
    }
    // now, we can be sure that the rest of data to read is between ringbuf->readIndex and buf end
    if (bytesToRead > 0) {
      memcpy(outData + bytesRead, ringbuf->buf + ringbuf->readIndex, bytesToRead);
      ringbuf->readIndex += bytesToRead;
      bytesRead += bytesToRead;
      // bytesToRead = 0; // not necessary
    }
    ringbuf->count -= bytesRead;
  }
  RINGBUF_UNLOCK(ringbuf);

  return bytesRead;
}

//////////////////////////////////////////////////////////////////////////////
//
// Resize the given ringbuffer object keeping possibly existing data.
// Shrinking below the default size is not allowed. For a static ringbuffer
// the resizing is omitted.
//
// Parameters:
// ringbuf      : The reference to the corresponding ringbuffer object.
// requestedSize: The new size of the ringbuffer object.
//
// Return:
// The actually adjusted size.
//
//////////////////////////////////////////////////////////////////////////////
UINT32 ringbuf_AdjustSize(RingBuf_t* ringbuf, UINT32 requestedSize) {
  UINT32 newSize;

  RINGBUF_LOCK(ringbuf);
  // Do not shrink the buffer below the count of existing data to avoid data loss, omit resize for static buffer
  newSize = ringbuf->fStatic ? ringbuf->bufSize : max((UINT32)RINGBUF_DEFAULT_SIZE, max(requestedSize, ringbuf->count));
  // Check if there's anything to do at all
  if (newSize != ringbuf->bufSize) {
    UINT8* pNewBuf = (UINT8*)oswrap_GetMem(newSize);
    if (pNewBuf) {
      if (ringbuf->buf) {
        // Copy existing data to the new buffer
        UINT32 uiCntUpper = min(ringbuf->count, ringbuf->bufSize - ringbuf->readIndex);
        UINT32 uiCntLower = ringbuf->count - uiCntUpper;
        memcpy(pNewBuf, ringbuf->buf + ringbuf->readIndex, uiCntUpper);
        memcpy(pNewBuf + uiCntUpper, ringbuf->buf, uiCntLower);
        // Adjust indexes
        ringbuf->readIndex = 0;
        ringbuf->writeIndex = ringbuf->count;
        oswrap_FreeMem(ringbuf->buf);
      }
      // Save new buffer info
      ringbuf->buf = pNewBuf;
      ringbuf->bufSize = newSize;
    } else {
      // Allocation of the new buffer failed, keep everything as it is
      newSize = ringbuf->bufSize;
    }
  }
  RINGBUF_UNLOCK(ringbuf);

  return newSize;
}

