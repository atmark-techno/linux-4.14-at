/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#include "mux_msgqueue.h"


/*****                           Macros                                  *****/

/*****                          Typedefs                                 *****/

/*****       (enums, structs and unions shall always be typedef'd)       *****/


/*****                      Public Variables                             *****/


//============================================================================
// private functions of mux_msgqueue
//============================================================================
static void CallMsgAvailable(
    MUX_INSTANCE_t  *pMux
)
{
    if (NULL != pMux->MuxInterface.pMuxMsgAvailable)
    {
        pMux->MuxInterface.pMuxMsgAvailable ((DWORD)pMux, pMux->dwMuxUserData);
    }
}


//============================================================================
// public interface of mux_msgqueue
//============================================================================
void MuxMsg_Init (
    MUX_INSTANCE_t      *pMux
)
{
    UINT32  i;

    // init MuxMsgQueue (ring buffer)
    pMux->MuxMsgQueue.pMuxMsgRead     = NULL;
    pMux->MuxMsgQueue.pMuxMsgWrite    = &pMux->MuxMsgMem[0];

    for (i = 0; i < MAX_MUX_MSG_MEMEORY - 1; i++)
    {
        (&pMux->MuxMsgMem[i])->pNext = &pMux->MuxMsgMem[i + 1];
        MEMCLR(&(&pMux->MuxMsgMem[i])->MuxMsg, sizeof(MP_PRIMITIVE));
    }
    (&pMux->MuxMsgMem[i])->pNext = &pMux->MuxMsgMem[0];
}


DWORD MuxMsg_Get (
    MUX_INSTANCE_t      *pMux,
    MP_PRIMITIVE        *pMuxMsg
)
{
    MSG_QUEUE_t     *MuxMsgQueue = &pMux->MuxMsgQueue;

    if (NULL == MuxMsgQueue->pMuxMsgRead)
    {
        return MUXMSG_QUEUE_EMPTY;
    }

    // copy message
    MEMCPY(pMuxMsg, &MuxMsgQueue->pMuxMsgRead->MuxMsg, sizeof(MP_PRIMITIVE));

    // move MuxMsgQueue.pMuxMsgRead to the next message
    MuxMsgQueue->pMuxMsgRead = MuxMsgQueue->pMuxMsgRead->pNext;

    if (MuxMsgQueue->pMuxMsgRead == MuxMsgQueue->pMuxMsgWrite)
    {
        MuxMsgQueue->pMuxMsgRead = NULL;
    }

    return MUXMSG_OK;
}


DWORD MuxMsg_Put (
    MUX_INSTANCE_t      *pMux,
    MP_PRIMITIVE        *pMuxMsg
)
{
    MSG_QUEUE_t     *MuxMsgQueue = &pMux->MuxMsgQueue;

    if (MuxMsgQueue->pMuxMsgWrite == MuxMsgQueue->pMuxMsgRead)
    {
        MUXDBG(ZONE_MUX_FRAME_ERROR, TEXT("Mux message queue full, message lost!"));
        return MUXMSG_QUEUE_FULL;
    }

    // copy message
    MEMCPY(&MuxMsgQueue->pMuxMsgWrite->MuxMsg, pMuxMsg, sizeof(MP_PRIMITIVE));

    // move MuxMsgQueue.pMuxMsgWrite to the next buffer
    if (MuxMsgQueue->pMuxMsgRead == NULL)
    {
        MuxMsgQueue->pMuxMsgRead = MuxMsgQueue->pMuxMsgWrite;
    }

    MuxMsgQueue->pMuxMsgWrite = MuxMsgQueue->pMuxMsgWrite->pNext;

    // send signal
    CallMsgAvailable(pMux);

    return MUXMSG_OK;
}


BOOL MuxMsg_Exist(
    MUX_INSTANCE_t      *pMux
)
{
    return (pMux->MuxMsgQueue.pMuxMsgRead == NULL) ? FALSE : TRUE;
}

