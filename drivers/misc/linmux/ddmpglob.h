/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#ifndef __DDMPGLOB_H
#define __DDMPGLOB_H



typedef void (*CallbackTXBufFun) (MUX_INSTANCE_t *pMux);


//***** global variabled ******************************************************





// **************** FUNCTION TO CALL FROM FRAMEWORK ***************************

/*
        Functions called from the framework to access the mux
*/


/******************************************************************************/
/*!\brief Mux receives data from physical device
*
* This function is the entrance from the physical device into the multiplex
* protocol. <br>
* Muxed data are received by that function. The Framework has to call this
* function with the received data as soon as possible (logical flow control).
*
*
* \note
*      The function has a return value, but in the current implementation
*      this is alway the input len. Normally the logical flow control should
*      always prevent from data loss, so no physical flow control on the
*      hardware port is available. If there are too much data on the input
*      stream (flow control on, but still too many data received), packets
*      would lost.
*
* \param *Data A pointer to the data to be decoded from the multiplexer
*
* \param len length of data
*
* \return
*      The function will always return the given len in the current
*      implementation
*
* \see MP_vDevDataSendCallback()
*****************************************************************************/
UINT32 MP_uiDevDataReceive( MUX_INSTANCE_t *pMux, UINT8 *Data, UINT32 len );


/******************************************************************************/
/*!\brief Mux gets a wakeup from the physical device to write again
*
* There is a wakeup mechanism between the multiplex protocol and the physical
* device: if the transmit buffer from the physical device becomes a certain
* level (>=MP_MAX_PENDING_DATA_UART, this are the data which are already sent
* out of the multiplex protocol but not yet transmitted over the physical device),
* the multiplex protocol has to setup the physical driver to call this function
* when the buffer is near emtpy (<20 Bytes).
*
* \attention
*      The multiplex protocol WILL NOT LOOP AND POLL THE PHYSICAL DEVICE, but
*      wait for this function call. Not to call this function will result in
*      a fully deadlocked multiplex protocol. Calling this function is essential
*      for a proper function.
*
* \param len length of data; not used
*
* \see MP_uiDevDataReceive()
*****************************************************************************/
void MP_vDevDataSendCallback( MUX_INSTANCE_t *pMux, UINT32 len );


/*****************************************************************************/
/*!\brief Mux receives data from connected application
*
* This function is the entrance from the connected internal application
* (running on the same host as the mux) into the the multiplex protocol. All
* applications has to use a different channel IDs (param ucDlci). <br>
* Unframed data are received by that function. The multiplex protocol will
* create packets, frame and send them using the physical device to the
* opposite side.
*
* \note
*      The function has a return value, this is the size of data read by the
*      function. If this value is smaller then the given length, the application
*      should not call this function (for example with a timer) again and again
*      but sleep until the multiplex protocol will indicate free buffer with
*      calling MP_vAppReceiveDataCallback().
*
* \param *Data A pointer to the data to be transmitted using the multiplexer
*
* \param len length of data
*
* \param ucDlci channel which is connected with the selected application
*
* \return
*      The function will return the size read by the multiplex protocol. All
*      values between 0 and len are valid. If this size is smaller than the given
*      len, the multiplex protocol will wakeup the application by calling
*      MP_vAppReceiveDataCallback(ucDlci) when free buffer are available again
*      in the mux.
*
* \see MP_vAppReceiveDataCallback()
*****************************************************************************/
UINT32 MP_uiAppDataReceive( MUX_INSTANCE_t *pMux, UINT8 *Data, UINT32 len, UINT8 ucDlci );


/******************************************************************************/
/*!\brief Mux gets a wakeup from an application to send data again
*
* There is a wakeup mechanism between the multiplex protocoll and the application
* connected on the channel ucDlci: if the receive buffer from the appliocation
* becomes free again, it has to call this function to wakeup the mux again.
* The multiplex protocol will send now the next received data to the application.
*
* \attention
*      The multiplex protocol WILL NOT LOOP AND POLL THE APPLICATION, but
*      wait for this function call. Not to call this function will result in
*      a fully deadlocked multiplex protocol on the selected channel . Calling
*      this function is essential for a proper function.
*
* \param ucDlci selected channel to wakeup
*
* \see MP_vAppDataSend()
* \see MP_vDevDataSendCallback()
*****************************************************************************/
void MP_vAppDataSendCallback( MUX_INSTANCE_t *pMux, UINT8 ucDlci );


/*****************************************************************************/
/*!\brief Mux receives V24 status informations from a connected application
*
* This function is the entrance from the connected internal application
* (running on the same host as the mux) into the the multiplex protocol to send
* V24 status information to the other side. All
* applications has to use a different channel IDs (param ucDlci). <br>
* The V24 status ionformation will checked against the last transmitted ones; if
* status lines has changed, the multiplex protocol will create MSC indications and
* send them using the physical device to the opposite side.
*
* \note
*      Call this functions always with a copmplete set of valid status informations!
*      For example; if you only want to set the RTS line, you also have to transmit
*      the correct state of the DTR line
*
*
* \param status
*      V24 status information structure
*
* \param ucDlci
*      channel which is connected with the selected application
*
* \see MP_vMSC_SendESC()
* \see MP_uiAppDataReceive()
*****************************************************************************/
void MP_vAppSetV24Status( MUX_INSTANCE_t *pMux, V24STATUS status, UINT8 ucDlci );


/*****************************************************************************/
/*!\brief Mux receives an Escape sequence (usualy +++ detected)
*
* This function sends a special message to the module, the module will do the
* same workflow the as it would receive this escape sequence on a physical port.
* due to the nature of the mux protocol (data are framed, more than one logical
* channel on the physical port) the module is not able to parse this sequence.
* The costumer application has to parse for escape alone and call this funciton.
*
* \param ucDlci channel which is connected with the selected application
*
*
* \see MP_vAppSetV24Status()
* \see MP_uiAppDataReceive()
*****************************************************************************/
void MP_vSendESC(MUX_INSTANCE_t *pMux, UINT8 ucDlci );


/*****************************************************************************/
/*!\brief Mux Main Init function
*
* This function will setup the multiplex protocol. It brings the mux memory
* up and sends an initial message into the mux protocol.
*
* \note
*      Call this function every time before the mux protocol should start.<br>
*      The framework memory system adaption for the mux protocol has to be
*      started before. <br>
*      This functions will call a adaption initalization function
*      MP_vInitAdaptation() to be provided by the framework. This function
*      is only called the first time after system startup.
*
* \see MP_vInitAdaptation()
*****************************************************************************/
void MP_Init( MUX_INSTANCE_t *pMux );


/*****************************************************************************/
/*!\brief Mux Main Shutdown function
*
* This function will start the shutdown sequence to stop the multiplex protocol.
* Call this function to have a normal stop. The mux will send a ShutDown-Msg to
* the opposite and wait for the confirmation. When the logical channels are down,
* the mux will call the function MP_ResetMuxInd().
*
* \note
*      The framework adaption top the mux (messaging system etc.) has to work
*      until the function MP_ResetMuxInd() is called.
*
* \see MP_ResetMuxInd()
*****************************************************************************/
void MP_StartShutdownProc( MUX_INSTANCE_t *pMux );


/*****************************************************************************/
/*!\brief direct access to mux memory buffers
*
* This is a modules internal function and not necessary for proper working. If
* the mux in the module is not running, the memory may be used by other parts
* of the software
*
* \param cnt memory size
*
* \return
*      a pointer to the muxers internal memory buffer if the buffer is smaller
*      or equal to param cnt
*
*****************************************************************************/
UINT8 *MP_GetMemPointer( MUX_INSTANCE_t *pMux, UINT32 cnt );


/*****************************************************************************/
/*!\brief  upper layer calls here, to request a given low power state
*
*  -> Mux sends PSC command
*
* \return  none
*
*****************************************************************************/
void MP_SleepReqEx( MUX_INSTANCE_t *pMux, MP_ePSCMode newMode );


#endif // __DDMPGLOB_H
