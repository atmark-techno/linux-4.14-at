/******************************************************************************

 (c) 2018 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// ringbuffer.h
//
// This file contains the definition of the ringbuffer object used as internal
// transfer buffer for write requests to the base tty port.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SRC_DRIVER_RINGBUFFER_H_
#define SRC_DRIVER_RINGBUFFER_H_

#include "global.h"

//////////////////////////////////////////////////////////////////////////////

#define RINGBUF_DEFAULT_SIZE 256

//////////////////////////////////////////////////////////////////////////////

typedef struct RingBuf_t {
  CRITICAL_SECTION csLock;
  SPINLOCK         slLock;
  SEMHANDLE        DataSema;
  BOOL             fStatic;
  UINT32           bufSize;
  UINT32           count;
  UINT32           readIndex;
  UINT32           writeIndex;
  UINT8*           buf;
} RingBuf_t;

//////////////////////////////////////////////////////////////////////////////

BOOL ringbuf_Init(RingBuf_t* ringbuf);
BOOL ringbuf_StaticInit(RingBuf_t* ringbuf, UINT32 size);
void ringbuf_Destroy(RingBuf_t* ringbuf);
void ringbuf_Clear(RingBuf_t* ringbuf);
BOOL ringbuf_WaitForData(RingBuf_t* ringbuf);
UINT32 ringbuf_GetSize(RingBuf_t* ringbuf);
UINT32 ringbuf_GetNumWaitingBytes(RingBuf_t* ringbuf);
UINT32 ringbuf_GetNumFreeBytes(RingBuf_t* ringbuf);
UINT32 ringbuf_PutBytes(RingBuf_t* ringbuf, UINT8* data, UINT32 len);
UINT32 ringbuf_GetBytes(RingBuf_t* ringbuf, UINT8* outData, UINT32 maxLen);
UINT32 ringbuf_AdjustSize(RingBuf_t* ringbuf, UINT32 requestedSize);

#endif /* SRC_DRIVER_RINGBUFFER_H_ */
