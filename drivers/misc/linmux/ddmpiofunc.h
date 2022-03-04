/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

/****************** FUNCTION TO IMPLEMENT FROM FRAMEWORK **********************

    The following functions are called from the mux to have a conection
    to

    (1) a physical device to send muxed data
    (2) the connected applications to send them demuxed data

    The framework has to implement this functions.

******************************************************************************/

#ifndef __DDMPIOFUNC_H__

#define __DDMPIOFUNC_H__



/*****************************************************************************/
/*!\brief Mux sends an detected Escape (usually +++) to connected application
*
* <b> This function has to be provided by the framework </b> <br>
* This function is called if the mux has received a MSC indication with the
* optional BREAK signal != 0.
* This function is only called from within the module, not in the costumer
* application. If this is called in the custumers application, an error occured!
*
* \param ucDlci channel which is connected with the selected application
*****************************************************************************/
void MP_vAppSendEsc( MUX_INSTANCE_t *pMux, UINT8 ucDlci);


/*****************************************************************************/
/*!\brief init for system dependend adaption layer(s)
*
* <b> This function has to be provided by the framework </b> <br>
* This function is called from within MP_Init() to setup additional
* framework adaption layer. Normally this functions is empty. This function
* is only called the first time after system startup.
*
* \see MP_Init()
*****************************************************************************/
void MP_vInitAdaptation( MUX_INSTANCE_t *pMux );


/*****************************************************************************/
/*!\brief Post a complete PS_PRIMITIVE Message to the multiplex main process
*
* <b> This function has to be provided by the framework </b> <br>
* This function is called from within the mux process. The multiplex protocol
* needs his own tasks to run. With this function a message is posted to the
* first. The messaging system should work as a normal message queue on task
* based frameworks.
*
* \note
*       The multiplex protocol consists of 2 processes: one high prio process
*       (the main process, should have a higher priority as the connected
*       applications) and one low prio process (helper process; should have a
*       lower priority as the connected applications). This function has to
*       post to the main high prio process. The associated working function
*      is MP_vProc()
*
* \see MP_vProc()
* \see MP_ucGetMessage()
*****************************************************************************/
void MP_PostUserMessage(MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive);


/*****************************************************************************/
/*!\brief reads a complete PS_PRIMITIVE Message posted by MP_PostUserMessage
*
* <b> This function has to be provided by the framework </b> <br>
* This function is called from within the mux process. The multiplex protocol
* needs his own tasks to run. With this function a message posted by
* MP_PostUserMessage() or MP_PostUserMessageLowPrio() is read The messaging
* system should work as a normal message queue on task based frameworks.
*
* \note
*       The multiplex protocol consists of 2 processes: one high prio process
*       (the main process, should have a higher priority as the connected
*       applications) and one low prio process (helper process; should have a
*       lower priority as the connected applications). This function is called
*       from both processes and has to read from the accociated messages queues.
*
* \see MP_vProc()
* \see MP_PostUserMessage()
*****************************************************************************/
UINT8 MP_ucGetMessage (MUX_INSTANCE_t *pMux, MP_PRIMITIVE *psPrimitive, UINT8 ucMsgSize);


/*
    functions to access the physical device
*/
/******************************************************************************/
/*!\brief Mux sends encoded data to physical device
*
* <b> This function has to be provided by the framework </b> <br>
* This function is the exit from the multiplex protocol to the physical device.
* <br>
* Muxed data are sent by that function. The muxer always tests first the buffer
* from the physical device with MP_bDevMemAvailable() which is essential for it.
* The mux will never write more than one full packet at once with this function
* and wil take care not to have more than MP_MAX_PENDING_DATA_UART bytes in
* the transmit buffer from the physical device.
*
*
* \param *Data A pointer to the data to be sent from the multiplexer to the
*              physical device
*
* \param len length of data
*
* \return
*      The function should always return the given len in the current
*      implementation. If not, the function MP_bDevMemAvailable() would
*      not work.
*
* \see MP_bDevMemAvailable()
* \see MP_vDevDataSendCallback()
*****************************************************************************/
UINT32 MP_uiDevDataSend( MUX_INSTANCE_t *pMux, UINT8 *Data, UINT32 len );



/******************************************************************************/
/*!\brief get current free bytes in physical device transmit queue
*
* <b> This function has to be provided by the framework </b> <br>
* The mux will test with this function, and does not send more data as given here.
* If more data are avail to send out, the mux will call MP_DevActivateCallback()
* to setup a callback within the physical port driver adaption.
*
* \attention
*      The whole internal flow control is implemented with packets and depends on
*      this measurement of the data not yet written out on the physical port.
*
* \return
*      The function has to return the number of free bytes in the physical device
*      transmit queue; this depends from flow control, _NOT_ from hardware. The
*      implementation has to make shure that not more than a defined number of
*      mux packets are sent out _before_ a flow control packet (MSC) from RX path
*      may have read to stop transfer.
*
*      The current implementation limits the number of free bytes to 180 Bytes,
*      if 115,2kbps is used, and scaled with the baudrate.
*      This means that not more than 180 bytes are send i one step without
*      reading a new flow control packet from RX direction
*
* \see MP_DevActivateCallback()
*****************************************************************************/
UINT32 MP_DevGetFreeBytesInPhysTXBuf( MUX_INSTANCE_t *pMux );



/******************************************************************************/
/*!\brief register a callback function to the physical driver adaptation
*
* <b> This function has to be provided by the framework </b> <br>

* If there are too many bytes in the phys device transmit buffer, the mux will
* go into sleep mode after calling this function. This function regsters a callback
* function to the phys. driver adaption. The driver has to call the function when
* the data amount in the transmit queue is smaller or equal the given size Len.
*
*
* \attention
*      This callback registration prevents the mux from polling the transmit queue
*      of the phys driver adaptation. If the callback function is not used by the
*      phys driver adaptation after falling under the given level, a deadlock
*      will occur and data will not be transmitted again at runtime.
*
* \param len trigger (amount of data in transmit buffer) to the callback
*
* \param CallbackFun callback function
*
* \see MP_DevGetBytesInPhysTXBuf()
*****************************************************************************/
void MP_DevActivateCallback( MUX_INSTANCE_t *pMux, UINT32 Len, CallbackTXBufFun CallbackFun );


/*
    functions to access the connected applications
*/
/******************************************************************************/
/*!\brief Mux sends decoded data to a connected application
*
* <b> This function has to be provided by the framework </b> <br>
* This function is the exit from the multiplex protocol to a connected application.
* <br>
* Decoded data are sent by that function to transmit data to the application adressed
* by ucDlci. The applications confirms the read data with the return value. If the
* returned value from it is smaller than the given data size, the mux will not repeat
* sending the data (wil not poll the application) but waits until the function
* MP_vAppDataSendCallback() is called from the framework.
*
* \attention
*      The multiplex protocol WILL NOT LOOP AND POLL THE APPLICATION, but
*      wait for the function call MP_vAppDataSendCallback(). Not to call this
*      function will result in a fully deadlocked multiplex protocol on the selected
*      channel. Calling that function is essential for a proper function.
*
* \param ucDlci selected channel to wakeup
*
* \param *pucBuffer
*              A pointer to the data to be sent from the multiplexer to the
*              physical device
*
* \param uiLen length of data
* \param uiBytesInBuffer
*              Number of overall bytes currently in the dlci's rx buffer
*
* \return
*      The function return the number of read data. All values from 0 to uiLen
*      are valid.
*
* \see MP_uiAppDataReceive()
* \see MP_vAppDataSendCallback()
*****************************************************************************/
UINT32 MP_vSendDataMP_to_App( MUX_INSTANCE_t *pMux, UINT8 ucDlci, UINT8 *pucBuffer, UINT32 uiLen, UINT32 uiBytesInBuffer );


/******************************************************************************/
/*!\brief Mux send a wakeup to an application to send data again
*
* <b> This function has to be provided by the framework. </b> <br>
* There is a wakeup mechanism between the multiplex protocoll and the application
* connected on the channel ucDlci: if the mux was able to send data from the connected
* application over thge physical port to the opposite, it will call this function
* to indicate, that the application may send now more data. This is useful if the
* application was is sleep (MP_uiAppDataReceive() returned a smaller value).
*
* \note
*      With this function the connected application has the change not to
*      poll the multiplex protocol but to get an indication to write again.
*
* \param len
*       not used yet
* \param ucDlci
*       selected channel to wakeup
*
* \see MP_vAppReceiveData()
*****************************************************************************/
void MP_vAppReceiveDataCallback( MUX_INSTANCE_t *pMux, UINT32 len, UINT8 ucDlci );



/*****************************************************************************/
/*!\brief Mux sends V24 status informations to a connected application
*
* This function is the exit from the multiplex protocol to the connected internal
* application (running on the same host as the mux) to indicate V24 status
* information received from the opposite.<br>
* If the mux has received an MSC indication with status lines, it will call this
* function now, which has to send the sMP_DLCI[ucDlci].sV24Status structure to
* the application.
*
* \note
*      sMP_DLCI[ucDlci].sV24Status always holds the complete set of v24 status
*      lines values. Do not edit these values directly.
*
* \param ucDlci
*      channel which is connected with the selected application
*
*****************************************************************************/
void MP_vAppSendV24Status( MUX_INSTANCE_t *pMux, UINT8 ucDlci, V24STATUS v24Status );



#endif // #ifndef __DDMPIOFUNC_H__
