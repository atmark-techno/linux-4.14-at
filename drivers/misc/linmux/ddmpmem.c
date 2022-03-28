/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#include "global.h"
#include "ddmpglob.h"
#include "ddmpdlch.h"


//***** Defines ***************************************************************


// ***** Types ****************************************************************


//***** Global variables ******************************************************


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   FIFO Management: Init FIFO
 *
 * ---------------------------------------------------------------------------
 */
void MP_vFifoInit
(
    MP_pTFifo   fFifo
)
{
  fFifo->first  = NULL;
  fFifo->last   = NULL;
  fFifo->count  = 0;
}




/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   FFIFO Management: Get FIFO Element from selected FIFO, Normal
 *
 * ---------------------------------------------------------------------------
 */
pMPFRAME MP_pstFifoGet
(
    MP_pTFifo   fFifo
)
{
  pMPFRAME lQE;

  if (fFifo->first == NULL)
  {
     lQE = NULL;
  }
  else
  {
     lQE = fFifo->first;
     fFifo->first = lQE->next;
     if (--fFifo->count == 0)
     {
        fFifo->last = NULL;
     }
  }

  return lQE;
}



/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   FIFO Management: Read FIFO Element from selected FIFO, but
 *                           not delete from FIFO
 *
 * ---------------------------------------------------------------------------
 */
pMPFRAME MP_pstFifoPeek
(
    MP_pTFifo   fFifo
)
{
  return (fFifo->first);
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   FIFO Management: Put FIFO Element into selected FIFO
 *
 * ---------------------------------------------------------------------------
 */
void MP_vFifoPut
(
    MP_pTFifo   fFifo,
    pMPFRAME    fQE
)
{
  if (fQE == NULLP) // exit if NULL pointer detected
  {
    MUX_EXIT(TEXT("Mux-Exit: fQE == NULLP"));
  }
  else
  {
    fQE->next = NULL;
    if (++fFifo->count == 1)
    {
      fFifo->first  = fQE;
      fFifo->last   = fQE;
    }
    else
    {
      fFifo->last->next  = fQE;
      fFifo->last        = fQE;
    }
  }
}


/*****************************************************************************/
/* ---------------------------------------------------------------------------
 * scope:
 * input:
 * returns:
 * ---------------------------------------------------------------------------
 * descr:   FIFO Management: Init global Memory for FIFO Elements
 *
 * ---------------------------------------------------------------------------
 */
void MP_vMemInit
(
    MUX_INSTANCE_t  *pMux
)
{
    UINT32      i;
    MPFRAME    *pFrame;

    MP_vFifoInit(&pMux->MP_freelist);

    // init tx fifo for dlci0 and all events except UIH
    MP_vFifoInit(&pMux->MP_TxFifo);

    pMux->pDLCIArray[0].MP_RxRingBuf.BufSize  = pMux->pDLCIArray[0].MP_TxRingBuf.BufSize  = 0;
    pMux->pDLCIArray[0].MP_RxRingBuf.Buf      = pMux->pDLCIArray[0].MP_TxRingBuf.Buf      = NULL;

    // init data ringbuffer for UIH frames in both directions
    for (i=1; i < pMux->dwMaxNumberOfDLCI; i++)
    {
        pMux->pDLCIArray[i].MP_RxRingBuf.BufSize     = __MP_RX_RINGBUF_SIZE;
        pMux->pDLCIArray[i].MP_RxRingBuf.Count       = 0;
        pMux->pDLCIArray[i].MP_RxRingBuf.ReadIndex   = 0;
        pMux->pDLCIArray[i].MP_RxRingBuf.WriteIndex  = 0;
        pMux->pDLCIArray[i].MP_RxRingBuf.Buf = &pMux->RingBufferStatic.RxBuf[__MP_RX_RINGBUF_SIZE * (i - 1)];


        pMux->pDLCIArray[i].MP_TxRingBuf.BufSize     = __MP_TX_RINGBUF_SIZE;
        pMux->pDLCIArray[i].MP_TxRingBuf.Count       = 0;
        pMux->pDLCIArray[i].MP_TxRingBuf.ReadIndex   = 0;
        pMux->pDLCIArray[i].MP_TxRingBuf.WriteIndex  = 0;
        pMux->pDLCIArray[i].MP_TxRingBuf.Buf = &pMux->RingBufferStatic.TxBuf[__MP_TX_RINGBUF_SIZE * (i - 1)];
    }

    // init memory for the only one tx fifo we still have
    for (i = 0; i < MP_FIFOELEMENTS; i++)
    {
        pFrame = &pMux->sMP_Frames[i];
        MP_vFifoPut(&pMux->MP_freelist, pFrame);
    }

    // init stat values
    MEMCLR(&pMux->sMP_ParserInfo, sizeof(MP_PARSERINFO));
}

/* EOF */
