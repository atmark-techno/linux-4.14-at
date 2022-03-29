/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#ifndef __MUX_INTERFACE_H
#define __MUX_INTERFACE_H

//////////////////////////////////////////////////////////////////////////////
// return codes of the interface functions
//////////////////////////////////////////////////////////////////////////////
enum {
    MUX_OK = 0,
    MUX_ERR_INVALID_PARAMETER,
    MUX_ERR_WRONG_DLCI,
    MUX_ERR_DLCI_NOT_CONNECTED,
    MUX_ERR_WRONG_POWER_CMD,
    MUX_ERR_GET_MEMORY_FAILED,
    MUX_ERR_REVISION,
    MUX_ERR_CLIENT_NOT_ALLOWED
};


//////////////////////////////////////////////////////////////////////////////
// Mux Protocol Callback Interface
// These functions have to be provided by the framework
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// This function has to allocate memory which is needed for a multiplex
// protocol instance.
//
// Parameters:
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// dwSize           The size in bytes of the required memory block.
//
// Return:
// The pointer to the allocated space, NULL indicates failure.
//////////////////////////////////////////////////////////////////////////////
typedef PVOID   ( *MUX_GETMEM_cb )             ( DWORD dwUserData, DWORD dwSize );


//////////////////////////////////////////////////////////////////////////////
// This function has to release the memory which was allocated by a call to
// the function MUX_GETMEM_cb.
//
// Parameters:
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// pMem             The pointer to the buffer to be released.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_FREEMEM_cb )             ( DWORD dwUserData, PVOID pMem );


//////////////////////////////////////////////////////////////////////////////
// This callback function indicates the result of the multiplex protocol
// start initiated by a call of Mux_Start(). The function is not called by
// the multiplex protocol handling if there are no responses to the sent mux
// commands.
// If the multiplex protocol is running in client mode a call of this function
// indicates that the multiplex mode has been initiated by the host driver on
// the other side.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// dwResult         The result of the multiplex start,
//                  possible values: MUX_OK.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_STARTRESULT_cb )         ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwResult );


//////////////////////////////////////////////////////////////////////////////
// A call to this callback function indicates that the multiplex protocol
// handling needs internal information processing. This function indicates
// that messages processing is required. In consequence the framework must
// call the function Mux_MsgTick(). The call of Mux_MsgTick() must not take
// place in the context of this function.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_MSGAVAILABLE_cb )        ( DWORD hMuxInstance, DWORD dwUserData );


//////////////////////////////////////////////////////////////////////////////
// This callback function indicates that the multiplex protocol handling has
// been properly closed. The multiplex protocol handling calls this function
// after shutting down the last channel to indicate that the multiplex
// protocol handling has stopped and the physical port may be used now
// otherwise.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_RESET_cb )               ( DWORD hMuxInstance, DWORD dwUserData );


//////////////////////////////////////////////////////////////////////////////
// A call of this callback function indicates that the multiplex protocol
// handling wants to send data to the physical device. It is the interface
// from the multiplex protocol to the physical device. Multiplexed data is
// sent by calling that function. The multiplex protocol handling always tests
// first the free space of the physical device's transmit buffer by calling
// MUX_DEVGETFREETXBYTES_cb() which is essential for it. The multiplex
// protocol handling will never write more than one full packet at once and
// will take care not to have more than a specific amount of unsent bytes
// optimal for virtual flow control in the transmit buffer of the physical
// device. For a detailed description of the handshake mechanism for
// transmitting data on the physical device see description of the function
// Dev_SendDataContinue().
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// pData            A pointer to the data to be sent to the physical device.
// dwDataLen        The length in bytes of the data to be sent.
//
// Return:
// The number of bytes actually written. The function should always return
// the given length dwDataLen in the current implementation. If not, the
// multiplex protocol handling will fail to work properly.
//////////////////////////////////////////////////////////////////////////////
typedef DWORD   ( *MUX_DEVSENDDATA_cb )         ( DWORD hMuxInstance, DWORD dwUserData, PBYTE pData, DWORD dwDataLen );


//////////////////////////////////////////////////////////////////////////////
// The multiplex protocol handling informs the framework after which number of
// bytes left in the serial device's send buffer it has to be woken up again
// to continue sending of data. If the amount of data in the driver's transmit
// buffer (including all FIFO's) is less or equal than the number of bytes
// passed with the parameter dwLen, Dev_SendDataContinue() must be called to
// wake up the multiplex protocol handling again. For a detailed description
// of the handshake mechanism for transmitting data on the physical device see
// description of the function Dev_SendDataContinue ().
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// dwLen            The number of bytes left in the serial device's transmit
//                  buffer when the multiplex protocol has to be woken up
//                  again by calling Dev_SendDataContinue() to continue
//                  sending of data.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_DEVACTIVATE_cb )         ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwLen );


//////////////////////////////////////////////////////////////////////////////
// The multiplex protocol will call this callback function to test how many
// free space is available in the serial device's transmit buffer. For a
// detailed description of the handshake mechanism for transmitting data on
// the physical device see description of the function Dev_SendDataContinue().
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
//
// Return:
// The number of free bytes in the transmit buffer of the serial device.
//////////////////////////////////////////////////////////////////////////////
typedef DWORD   ( *MUX_DEVGETFREETXBYTES_cb )   ( DWORD hMuxInstance, DWORD dwUserData );


//////////////////////////////////////////////////////////////////////////////
// The virtual multiplex flow control requires specific sizes of the in and
// out queue of the serial hardware device for proper functionality. The
// optimal queue sizes are dependant of the used multiplex frame size. The
// Mux_Start() function calls this callback with default values directly.
// During the multiplex start sequence the multiplex frame size is negotiated
// with the connected module. After the frame size negotiation has been
// completed the MUX_DEVSETQUEUESIZES_cb callback is called again to set the
// queue sizes to their optimal values.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// dwRxSize         The new size of the serial hardware devices in queue.
// dwTxSize         The new size of the serial hardware devices out queue.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_DEVSETQUEUESIZES_cb )   ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwRxSize, DWORD dwTxSize );


//////////////////////////////////////////////////////////////////////////////
// The multiplex protocol implementation is not re-entrant. For that reason
// all calls into the multiplex protocol have to be locked against each other.
// This is done by the multiplex protocol by calling this callback function
// before executing any interface function. The framework must provide an
// appropriate locking mechanism like a critical section in this function.
// Because all callbacks are called in the context of calls of Mux_MsgTick()
// which is locked also by that locking mechanism in fact all callbacks from
// the multiplex protocol implementation are locked by the same semaphore.
// That has to be kept in mind to avoid dead locks.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_LOCK_cb )                ( DWORD hMuxInstance, DWORD dwUserData );


//////////////////////////////////////////////////////////////////////////////
// This function is called by the multiplex protocol to unlock the interface
// previously locked by a call to MUX_LOCK_cb() again.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *MUX_UNLOCK_cb )              ( DWORD hMuxInstance, DWORD dwUserData );


//////////////////////////////////////////////////////////////////////////////
// This function is called by the multiplex protocol only if it is running in
// client mode to query the physical sate of the ring and dcd lines on the
// base port.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// pRing            Pointer to the flag receiving the ring line state.
// pDCD             Pointer to the flag receiving the dcd line state.
//
// Return:
// TRUE on success, FALSE in case of an error.
//////////////////////////////////////////////////////////////////////////////
typedef BOOL    ( *MUX_GETV24LINES_cb )        ( DWORD hMuxInstance, DWORD dwUserData, BOOL *pRing, BOOL *pDCD );


//////////////////////////////////////////////////////////////////////////////
// This function is called by the multiplex protocol only if it is running in
// client mode to set the physical state of the ring and dcd lines on the
// base port.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// bRing            The new ring line state.
// bDCD             The new dcd line state.
//
// Return:
// TRUE on success, FALSE in case of an error.
//////////////////////////////////////////////////////////////////////////////
typedef BOOL    ( *MUX_SETV24LINES_cb )        ( DWORD hMuxInstance, DWORD dwUserData, BOOL bRing, BOOL bDCD );


//////////////////////////////////////////////////////////////////////////////
// This function is called by the multiplex protocol only if it is running in
// client after a special power control multiplex frame has been received.
// After having received such frame the module running the client diver can
// take an appropriate action. See documentaion of Mux_SendPowerCmd() for an
// description of the usual power modes.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The user parameter passed to the initial call of
//                  Mux_Register().
// PwrCmd           The power command received.
//
// Return:
// TRUE if the new power setting could be handled properly, FALSE otherwise.
//////////////////////////////////////////////////////////////////////////////
typedef enum {
    MUX_POWER_MODE_DEFAULT                    = 0,
    MUX_POWER_MODE_FULL                       = 1,
    MUX_POWER_MODE_SLEEP                      = 2,
    MUX_POWER_MODE_CYCLIC_SLEEP_SHORT         = 3,
    MUX_POWER_MODE_CYCLIC_SLEEP_LONG          = 4,
    MUX_POWER_MODE_SWITCH_OFF                 = 5,
    MUX_POWER_MODE_RESET                      = 6,
    MUX_POWER_MODE_CYCLIC_SLEEP_SHORT_CONT    = 7,
    MUX_POWER_MODE_CYCLIC_SLEEP_LONG_CONT     = 8,
    MUX_POWER_MODE_CYCLIC_SLEEP_LONG_CONT2    = 9
} POWER_CMD_e;

typedef BOOL    ( *MUX_SETPOWERSTATE_cb )        ( DWORD hMuxInstance, DWORD dwUserData, POWER_CMD_e PwrState );


//////////////////////////////////////////////////////////////////////////////
// DLCI Callback Interface
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// This function will be called when a call of Dlci_Establish() has finished.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The DLCI specific user parameter passed to the initial
//                  call of Dlci_Establish().
// dwDLCI           The number of the corresponding DLCI.
// dwResult         The result of the DLCI connection,
//                  currently always MUX_OK.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *DLCI_ESTABLISHRESULT_cb )    ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwDLCI, DWORD dwResult );


//////////////////////////////////////////////////////////////////////////////
// This function will be called when a call of Dlci_Release() has finished.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The DLCI specific user parameter passed to the initial
//                  call of Dlci_Establish().
// dwDLCI           The number of the corresponding DLCI.
// dwResult         The result of the DLCI disconnection,
//                  currently always MUX_OK.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *DLCI_RELEASERESULT_cb )      ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwDLCI, DWORD dwResult );


//////////////////////////////////////////////////////////////////////////////
// A call of this callback function indicates that the framework can continue
// to send data after it had been stalled (see "Dlci_SendData" for further
// information).
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The DLCI specific user parameter passed to the initial
//                  call of Dlci_Establish().
// dwDLCI           The number of the corresponding DLCI.
// dwLen            Not used yet.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *DLCI_SENDDATACONTINUE_cb )   ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwDLCI, DWORD dwLen );


//////////////////////////////////////////////////////////////////////////////
// This function is the interface from the multiplex protocol to the connected
// framework to deliver received and decoded data to the corresponding
// multiplex channel. The framework must return the number of read bytes. If
// it is not able to process all delivered bytes (e.g. because internal
// buffers are full) the framework may return a value smaller than the given
// data size. In that case the multiplex protocol will not continue the
// delivering of received data until the framework has indicated that it is
// able to receive data again by a call to Dlci_ReceiveDataContinue(). If
// receiving of data is stalled automatic virtual flow control will be handled
// by the multiplex protocol if necessary without the need for the framework
// to care about that issue.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The DLCI specific user parameter passed to the initial
//                  call of Dlci_Establish().
// dwDLCI           The number of the corresponding DLCI.
// pData            The pointer to the received data.
// dwDataLen        The length in bytes of the received data to be handled.
// dwBytesInBuffer  The overall number of bytes currently in the dlci's
//                  receive buffer. This parameter is not required for the
//                  data handling and just passed for informational reasons.
//
// Return:
// The function has to return the number of read data from framework (a return
// value of 0 is possible).
//////////////////////////////////////////////////////////////////////////////
typedef DWORD   ( *DLCI_RECEIVEDATA_cb )        ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwDLCI, PBYTE pData, DWORD dwDataLen, DWORD dwBytesInBuffer );


//////////////////////////////////////////////////////////////////////////////
// A call of this callback function indicates that a state change of the
// incoming virtual modem signals CTS, RING, CD or DSR has been received.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The DLCI specific user parameter passed to the initial
//                  call of Dlci_Establish().
// dwDLCI           The number of the corresponding DLCI.
// MuxV24Status     The V24 virtual status signals.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef struct _mux_v24status
{
/*0*/  unsigned int DTR            : 1; // Status of the DTR signal
/*1*/  unsigned int RTS            : 1; // Status of the RTS signal
/*2*/  unsigned int OutFlowActive  : 1; // Status of the  outgoing flow control (1 means flow control active)
/*3*/  unsigned int Reserved_0     : 1;
/*4*/  unsigned int BreakInd       : 1; // Signals if the interface receives a break indication
/*5*/  unsigned int Reserved_1     : 1;
/*6*/  unsigned int Reserved_2     : 1;
/*7*/  unsigned int Reserved_3     : 1;
/*8*/  unsigned int CTS            : 1; // Status of the DTE signal
/*9*/  unsigned int DSR            : 1; // Status of the DSR signal
/*a*/  unsigned int RI             : 1; // Indicates a incoming call
/*b*/  unsigned int DCD            : 1; // Status of the DCD signal
/*c*/  unsigned int InFlowActive   : 1; // Status of the incoming flow control (1 means flow control active)
/*d*/  unsigned int BreakReq       : 1; // Break Request
/*e*/  unsigned int Reserved_4     : 1;
/*f*/  unsigned int TxEmpty        : 1; // Indicates that the send buffer is empty
/*0*/  unsigned int TxFull         : 1; // Indicates that the send buffer is full
} MUX_V24STATUS_t;

typedef void    ( *DLCI_RECEIVEV24STATUS_cb )   ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwDLCI, MUX_V24STATUS_t MuxV24Status );


//////////////////////////////////////////////////////////////////////////////
// A call of this callback function signals abnormal abortion of the channel
// activity in case of a multiplex reset. The framework can use this
// information to reset the internal channel states and put the channel
// implementation into the initial state.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The DLCI specific user parameter passed to the initial
//                  call of Dlci_Establish().
// dwDLCI           The number of the corresponding DLCI.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *DLCI_SHUTDOWN_cb )     ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwDLCI );


//////////////////////////////////////////////////////////////////////////////
// A call of this callback function signals that an virtual esc signal has
// been received if the mux protocol is running in client mode.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwUserData       The DLCI specific user parameter passed to the initial
//                  call of Dlci_Establish().
// dwDLCI           The number of the corresponding DLCI.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
typedef void    ( *DLCI_ESCRECEIVED_cb )  ( DWORD hMuxInstance, DWORD dwUserData, DWORD dwDLCI );


//////////////////////////////////////////////////////////////////////////////
// Mux Protocol Interface
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// This function has to be called before any multiplex activity to initialize
// the multiplex protocol instance. It computes and allocates the necessary
// memory, stores the provided callback functions and supplies a handle to the
// corresponding protocol instance. This handle created by a call to this
// function has to be passed to all calls into the multiplex protocol
// afterwards to identify the correct multiplex instance.
//
// Parameters:
// dwUserData     That parameter is user specific data and has no
//                functionality inside the multiplex protocol. It is simply
//                stored and passed to all mux callback functions (e.g. to
//                provide the possibility to access a C++ class instance
//                inside the C callback functions).
// pMuxInterface  Pointer to the structure of callback functions.
// dwMaxDlci      The maximum number of channels available in the created
//                multiplex protocol instance required (range of values
//                1-15). If the passed number is greater than the number of
//                multiplex channels provided by the connected module the
//                required memory will be allocated but acces to the
//                unavailable channels will fail.
// dwMpRevision   The multiplex protocol version is negotiated with the
//                connected module during multiplex startup. It is used to
//                ensure compatibility between different multiplex driver
//                and module generations. If for some reason an older
//                multiplex protocol shall be used it can be controlled with
//                that parameter. If a lower number than the lowest
//                supported version is passed then automatically the lowest
//                supported protocol version is used. The lowest supported
//                multiplex protocol version is 3. If a higher number than
//                the maximal supported protocol version is passed then
//                automatically the highest supported version number is
//                used. So for using the latest multiplex protocol
//                automatically simply 0xFFFFFFFF can be passed in that
//                parameter. For further information about the multiplex
//                protocol versions refer to the "Multiplexer User's Guide".
// phMuxInstance  The pointer to a variable receiving the multiplex protocol
//                instance handle identifying the corresponding multiplex
//                protocol instance if the function is successful (out).
//
// Return:
// MUX_OK (phMuxInstance contains a valid value.)
// MUX_ERR_INVALID_PARAMETER
// MUX_ERR_GET_MEMORY_FAILED
//////////////////////////////////////////////////////////////////////////////
typedef struct _mux_interface {
    MUX_GETMEM_cb                   pMuxGetMem;
    MUX_FREEMEM_cb                  pMuxFreeMem;
    MUX_STARTRESULT_cb              pMuxStartResult;
    MUX_MSGAVAILABLE_cb             pMuxMsgAvailable;
    MUX_RESET_cb                    pMuxReset;
    MUX_LOCK_cb                     pMuxLock;
    MUX_UNLOCK_cb                   pMuxUnlock;
    MUX_DEVSENDDATA_cb              pMuxDevSendData;
    MUX_DEVACTIVATE_cb              pMuxDevActivate;
    MUX_DEVGETFREETXBYTES_cb        pMuxDevGetFreeTxBytes;
    MUX_DEVSETQUEUESIZES_cb         pMuxDevSetQueueSizes;
    MUX_GETV24LINES_cb              pMuxDevGetV24Lines;
    MUX_SETV24LINES_cb              pMuxDevSetV24Lines;
    MUX_SETPOWERSTATE_cb            pMuxDevSetPowerState;
} MUX_INTERFACE_t, *pMUX_INTERFACE_t;

DWORD Mux_Register
(
    DWORD            dwUserData,
    pMUX_INTERFACE_t pMuxInterface,
    DWORD            dwMaxDlci,
    DWORD            dwMpRevision,
    PDWORD           phMuxInstance
);


//////////////////////////////////////////////////////////////////////////////
// This function destroys the corresponding multiplex protocol instance. It
// releases all allocated resources of the protocol instance. After the call
// of this function the handle hMuxInstance is invalid, it must not be used
// for further calls of the multiplex protocol anymore.
//
// Parameters:
// hMuxInstance     The handle of the multiplex protocol instance returned by
//                  the initial call of Mux_Register() which shall be
//                  destroyed.
//
// Return:
// MUX_OK (phMuxInstance contents valid handle)
// MUX_ERR_INVALID_PARAMETER
//////////////////////////////////////////////////////////////////////////////
DWORD Mux_Deregister
(
    DWORD           hMuxInstance
);


//////////////////////////////////////////////////////////////////////////////
// This function initializes the parameters required for the multiplex
// handling.
//
// Parameters:
// hMuxInstance   The handle returned by the initial call of Mux_Register()
//                identifying the corresponding multiplex protocol instance.
// dwWindowSize   Modules with multiplex protocol version 4 and above use a
//                kind of HDLC framing to secure the data transmission on
//                the virtual ports and avoid data loss on high transfer
//                rates and much data traffic. That parameter provides the
//                possibility to adjust the size of the HDLC window which
//                defines the maximal number of outstanding HDLC packets.
//                It can be between 1 and seven.
//                Please note that this parameter defines only the max.
//                window size to be used. The real size is negotiated
//                with the connected module and can be smaller, depending
//                on the modules capabilities.
// dwFrameSize    That parameter provides the possibility to adjust the
//                frame size of the HDLC frames used with multiplex
//                protocol version 4. It can be between 1 and 0x4000.
//                Please note that this parameter defines only the max.
//                frame size to be used. The real size is negotiated
//                with the connected module and can be smaller, depending
//                on the modules capabilities.
// dwBaudRate     The mux protocol handling requires the baud rate the base
//                port is running with for calculating useful queue sizes.
//                If the base port is a virtual usb cdc/acm port 0 (zero)
//                must be passed.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
void Mux_Init
(
    DWORD           hMuxInstance,
    DWORD           dwWindowSize,
    DWORD           dwFrameSize,
    DWORD           dwBaudRate
);


//////////////////////////////////////////////////////////////////////////////
// This function sends an initial message to the multiplex protocol and
// establishes the control channel. If this function is called the multiplex
// protocol handling will run in host (or master mode). The process of
// establishing the control channel is asynchronous, the function returns
// after having initiated the multiplex start. After this process is finished
// the multiplex protocol handling calls the registered callback function
// MUX_STARTRESULT_cb().
// In client mode that callback is called after the control channel has been
// established by the host driver on the other side.
//
// Parameters:
// hMuxInstance   The handle returned by the initial call of Mux_Register()
//                identifying the corresponding multiplex protocol instance.
//
// Return:
// MUX_OK (action in progress)
//////////////////////////////////////////////////////////////////////////////
DWORD Mux_Start
(
    DWORD           hMuxInstance
);


//////////////////////////////////////////////////////////////////////////////
// This function starts the multiplex shutdown sequence to gracefully stop the
// multiplex protocol handling. That process is asynchronous, the function
// returns after having initiated the multiplex shutdown sequence. The
// multiplex handling sends a multiplex shutdown message to the module and
// waits for the confirmation. When all logical channels are properly closed
// the multiplex handling calls the function MUX_RESET_cb() to indicate that
// the multiplex shutdown has finished correctly.
// The multiplex protocol can be stopped only from host side. So if it is
// running in client mode this function has no effect.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
void Mux_Stop
(
    DWORD           hMuxInstance
);


//////////////////////////////////////////////////////////////////////////////
// This function stops and resets the multiplex handling without trying a
// graceful shutdown and sending further multiplex frames. It has to be called
// in any unexpected module failure (e.g. if the module has been switched off
// externally) to avoid further module access by the multiplex protocol
// implementation.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
void Mux_Shutdown
(
    DWORD           hMuxInstance
);


//////////////////////////////////////////////////////////////////////////////
// This function sends a special power control multiplex frame to the module
// containing a power command with the following meaning:
// ---------------------------------------------------------------------------
// Value  |  Description
// ---------------------------------------------------------------------------
//     0  |  Switches to the same mode as without a value octet
//     1  |  Switches into full functionality mode, like AT+CFUN=1
//     2  |  Switches into NON-CYCLIC SLEEP mode, like AT+CFUN=0
//     3  |  Switches into CYCLIC SLEEP mode, like AT+CFUN=5
//     4  |  Switches into CYCLIC SLEEP mode, like AT+CFUN=6
//     5  |  Switches off, like AT^SMSO
//     6  |  Resets, like AT+CFUN=1,1
//     7  |  Switches into CYCLIC SLEEP mode, like AT+CFUN=7
//     8  |  Switches into CYCLIC SLEEP mode, like AT+CFUN=8
//     9  |  Switches into CYCLIC SLEEP mode, like AT+CFUN=9
// ---------------------------------------------------------------------------
// All power commands depend on the connected module. The supported commands
// are described in the appropriate AT command documentation of the module
// (see AT+CFUN command).
// A power control command can only be sent by a host driver. In client mode
// this function has no effect. Instead the callback MUX_SETPOWERSTATE_cb is
// called if a corresponding mux message has been received in client mode.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// PwrCmd           The power command to be executed.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_POWER_CMD
// MUX_ERR_WRONG_DLCI
// MUX_ERR_CLIENT_NOT_ALLOWED
//////////////////////////////////////////////////////////////////////////////
DWORD Mux_SendPowerCmd (
    DWORD           hMuxInstance,
    POWER_CMD_e     PwrCmd
);


//////////////////////////////////////////////////////////////////////////////
// This function is needed for the internal message processing. Each call
// triggers the handling of a single internal multiplex message. Each time
// handling of a multiplex message is required the multiplex protocol
// implementation calls the callback function MUX_MSGAVAILABLE_cb(). To keep
// the multiplex handling alive the framework must therefore call
// Mux_MsgTick() once for each call of MUX_MSGAVAILABLE_cb(). Although
// Mux_MsgTick() returns TRUE if there are more messages waiting to be
// processed it is recommended to call Mux_MsgTick() only once for each
// MUX_MSGAVAILABLE_cb() callback. To avoid consecutive calls Mux_MsgTick()
// must not be called under no circumstances inside MUX_MSGAVAILABLE_cb().
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
//
// Return:
// TRUE if there are more messages to be processed, FALSE if the mux message
// queue is empty.
//////////////////////////////////////////////////////////////////////////////
BOOL Mux_MsgTick (
    DWORD           hMuxInstance
);


//////////////////////////////////////////////////////////////////////////////
// For several internal time dependant evaluations and timeout detections the
// multiplex protocol implementation requires a reliable time base. That time
// base has to be provided by the framework via the cyclic call of the
// interface function Mux_TimerTick().The time interval for this cyclic
// function call is defined in the file "global.h" with the define
// "TIMEBASE_INTERVAL".
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
void Mux_TimerTick (
    DWORD           hMuxInstance
);


//////////////////////////////////////////////////////////////////////////////
// DLCI Interface
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// This function has to be called before any activity on the corresponding
// multiplex channel. It stores the provided callback functions.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI. The possible range
//                  of values depends on the parameter dwMaxDlci passed to
//                  the function Mux_Register(). It must not be 0 because
//                  channel 0 is a reserved multiplex channel used for
//                  multiplex control messages.
// dwUserData       That parameter is user specific data and has no
//                  functionality inside the multiplex protocol. It is simply
//                  stored and passed to all DLCI callback functions (e.g. to
//                  provide the possibility to access a C++ class instance
//                  inside the C callback functions).
// pDLCIInterface   Pointer to the structure of DLCI specific callback
//                  functions.
//
// Return:
// MUX_OK
// MUX_ERR_INVALID_PARAMETER
// MUX_ERR_WRONG_DLCI
//////////////////////////////////////////////////////////////////////////////
typedef struct _dlci_interface {
    DLCI_ESTABLISHRESULT_cb   pDlciEstablishResult;
    DLCI_RELEASERESULT_cb     pDlciReleaseResult;
    DLCI_SENDDATACONTINUE_cb  pDlciSendDataContinue;
    DLCI_RECEIVEDATA_cb       pDlciReceiveData;
    DLCI_RECEIVEV24STATUS_cb  pDlciReceiveV24Status;
    DLCI_SHUTDOWN_cb          pDlciShutdown;
    DLCI_ESCRECEIVED_cb       pDlciEscReceived;
} DLCI_INTERFACE_t, *pDLCI_INTERFACE_t;

DWORD Dlci_Register (
    DWORD                hMuxInstance,
    DWORD                dwDLCI,
    DWORD                dwUserData,
    pDLCI_INTERFACE_t    pDLCIInterface
);


//////////////////////////////////////////////////////////////////////////////
// This function destroys the corresponding multiplex channel. It releases
// all allocated resources of the multiplex channel. After the call of this
// function it is guaranteed that no callback function will be called
// anymore.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_Deregister
(
  DWORD                hMuxInstance,
  DWORD                dwDLCI
);


//////////////////////////////////////////////////////////////////////////////
// This function initiates the creation of a virtual multiplex data channel
// called DLCI (data link connection identification). If the multiplex
// protocol is running in host mode the commands for connecting the channel
// are issued as well. This process is asynchronous, the function returns
// after having initiated the multiplex channel start. After the connection
// has been successfully established, the callback function
// DLCI_ESTABLISHRESULT_cb() will be called.
// If the multiplex protocol is running in client mode the callback will be
// called after the channel has been connected by the host driver on the other
// side.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI. The possible range
//                  of values depends on the parameter dwMaxDlci passed to
//                  the function Mux_Register(). It must not be 0 because
//                  channel 0 is a reserved multiplex channel used for
//                  multiplex control messages.
//
// Return:
// MUX_OK
// MUX_ERR_INVALID_PARAMETER
// MUX_ERR_WRONG_DLCI
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_Establish (
    DWORD                hMuxInstance,
    DWORD                dwDLCI
);


//////////////////////////////////////////////////////////////////////////////
// This function destroys the corresponding DLCI channel. If the multiplex
// protocol is running in host mode the channel is disconnected as well. That
// process is asynchronous, the function returns after having initiated the
// multiplex channel disconnection. After the channel has been properly
// disconnected the callback function DLCI_RELEASERESULT_cb() will be called.
// In client mode the callback function is called after the channel has been
// disconnected by the host driver on the other side. It must not be called
// before the channel has been disconnected properly.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI.
//
// Return:
// MUX_OK (phMuxInstance contains valid handle)
// MUX_ERR_WRONG_DLCI
// MUX_ERR_CLIENT_NOT_ALLOWED
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_Release (
    DWORD           hMuxInstance,
    DWORD           dwDLCI
);


//////////////////////////////////////////////////////////////////////////////
// This function provides the connection state of the virtual data channel
// (DLCI).
//
// Parameters:
// hMuxInstance  The handle returned by the initial call of Mux_Register()
//               identifying the corresponding multiplex protocol instance.
// dwDLCI        The number of the corresponding DLCI.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
// MUX_ERR_DLCI_NOT_CONNECTED
//////////////////////////////////////////////////////////////////////////////
DWORD  Dlci_IsConnected(
    DWORD           hMuxInstance,
    DWORD           dwDLCI
);


//////////////////////////////////////////////////////////////////////////////
// By calls of this function, data is sent on the given multiplex channel
// (DLCI). The function will return number of bytes sent by the multiplex
// protocol. The multiplex protocol will return values smaller than the given
// length if it is not able to process all data at once because the internal
// buffer ran full. In that case the sending of data has to be stalled until
// the multiplex protocol calls the callback function
// DLCI_SENDDATACONTINUE_cb() to indicate that it is able again to process
// more transmit data.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI.
// pData            The pointer to the data to be transmitted.
// pdwDataLen       The length in bytes of the data to be transmitted when
//                  entering the function, the number of bytes transmitted
//                  on return.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
// MUX_ERR_DLCI_NOT_CONNECTED
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_SendData (
    DWORD           hMuxInstance,
    DWORD           dwDLCI,
    PBYTE           pData,
    PDWORD          pdwDataLen
);


//////////////////////////////////////////////////////////////////////////////
// There is a handshake mechanism for received data implemented between the
// multiplex protocol handling and the framework. The multiplex protocol
// delivers data received on a multiplex channel to the framework by calling
// the callback function DLCI_RECEIVEDATA_cb(). If the framework is not able
// to process all received data it has to indicate this with the return value
// of DLCI_RECEIVEDATA_cb() and the multiplex protocol handling stops
// delivering more data to the framework on that channel. After the framework
// is able again to process more receive data it has to indicate this by a
// call of this function Dlci_ReceiveDataContinue(). Only after that function
// call the multiplex protocol will continue to deliver more data on the given
// DLCI to the framework.
// The multiplex protocol will not loop and poll the framework, but wait for
// this function call. Neglecting to call this function after a failure of
// processing all receive data in DLCI_RECEIVEDATA_cb() will result in a fully
// deadlocked multiplex protocol on the selected channel. Calling this
// function is essential for proper functionality.
//
// The multiplex protocol will not loop and poll the framework, but
// wait for this function call. Not to call this function after a failure to
// process all receive data in DLCI_RECEIVEDATA_cb() will result in a fully
// deadlocked multiplex protocol on the selected channel. Calling this
// function is essential for proper functionality.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
// MUX_ERR_DLCI_NOT_CONNECTED
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_ReceiveDataContinue (
    DWORD           hMuxInstance,
    DWORD           dwDLCI
);


//////////////////////////////////////////////////////////////////////////////
// The function sends the multiplex equivalent of an escape sequence (usually
// +++) to the module. A "+++" sequence with one second pause before and after
// the "+++" is usually used to switch a connected AT command interpreter from
// data mode to command mode. Because a virtual data channel shares a single
// physical interface with other virtual channels the timing depends on the
// data traffic on the other virtual channels. Therefore the connected module
// is not able to parse these time dependant escape sequences properly.
// Instead the framework is responsible for the detection of escape sequences.
// A detected escape sequence has then to be passed to the corresponding DLCI
// by a call of this function Dlci_SendEsc().
// This function can only be used if the mutliplex protocol is running in host
// mode.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
// MUX_ERR_DLCI_NOT_CONNECTED
// MUX_ERR_CLIENT_NOT_ALLOWED
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_SendEsc (
    DWORD           hMuxInstance,
    DWORD           dwDLCI
);


//////////////////////////////////////////////////////////////////////////////
// This function provides the possibility to access the outgoing virtual V24
// status signals RTS and DTR. The V24 status information will be checked
// against the last transmitted one. If status lines have changed, the
// multiplex protocol will send them as special multiplex control frames for
// the corresponding DLCI to the module.
// This function has to be called always with a complete set of valid status
// information! For example; if only the RTS line shall be accessed, the
// correct state of the DTR line has to be transmitted as well.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI.
// V24Status        The V24 virtual status singnals.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
// MUX_ERR_DLCI_NOT_CONNECTED
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_SetV24Status (
    DWORD           hMuxInstance,
    DWORD           dwDLCI,
    MUX_V24STATUS_t V24Status
);


//////////////////////////////////////////////////////////////////////////////
// This function provides the possibility to query the current number of
// pending bytes in a channel's receive and transmit buffers as well as the
// size of these buffers.
//
// Parameters:
// hMuxInstance         The handle returned by the initial call of
//                      Mux_Register() identifying the corresponding multiplex
//                      protocol instance.
// dwDLCI               The number of the corresponding DLCI.
// pdwBytesInQueueRx    Buffer receiving the number of bytes in the recieve
//                      queue (output). If that value is not required NULL
//                      can be passed.
// pdwBytesInQueueTx    Buffer receiving the number of bytes in the transmit
//                      queue (output). If that value is not required NULL
//                      can be passed.
// pdwSizeOfQueueRx     Buffer receiving the size of the receive queue in
//                      bytes (output). If that value is not required NULL
//                      can be passed.
// pdwSizeOfQueueTx     Buffer receiving the size of the transmit queue in
//                      bytes (output). If that value is not required NULL
//                      can be passed.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
// MUX_ERR_DLCI_NOT_CONNECTED
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_GetQueueInformation (
    DWORD           hMuxInstance,
    DWORD           dwDLCI,
    PDWORD          pdwBytesInQueueRx,
    PDWORD          pdwBytesInQueueTx,
    PDWORD          pdwSizeOfQueueRx,
    PDWORD          pdwSizeOfQueueTx
);


//////////////////////////////////////////////////////////////////////////////
// This function provides the possibility to delete the pending data from a
// channel's transmit and receive buffers.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwDLCI           The number of the corresponding DLCI.
// fEmptyRx         Flag indicating that the receive buffer shall be emptied.
// fEmptyTx         Flag indicating that the transmit buffer shall be emptied.
//
// Return:
// MUX_OK
// MUX_ERR_WRONG_DLCI
// MUX_ERR_DLCI_NOT_CONNECTED
//////////////////////////////////////////////////////////////////////////////
DWORD Dlci_EmptyQueues (
    DWORD           hMuxInstance,
    DWORD           dwDLCI,
    BOOL            fEmptyRx,
    BOOL            fEmptyTx
);


//////////////////////////////////////////////////////////////////////////////
// This function is the entrance from the framework from view of the physical
// device into the multiplex protocol to feed physically received data into
// the multiplex protocol handling. The framework has to call this function
// with the data received on the physical interface as soon as possible (that
// is important for virtual flow control).
// The returned number of handled bytes is always the input length dwDataLen.
// Normally the virtual flow control should always prevent data loss. But if
// there is too much data on the input stream (flow control on, but still too
// much data received), packets will be lost.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// pData            A pointer to the physically received data to be decoded
//                  from the multiplex protocol.
// pdwDataLen       The size of the receive buffer in bytes when entering the
//                  function, the number of bytes received on return.
//
// Return:
// MUX_OK
//////////////////////////////////////////////////////////////////////////////
DWORD Dev_ReceiveData (
    DWORD           hMuxInstance,
    PBYTE           pData,
    PDWORD          pdwDataLen
);


//////////////////////////////////////////////////////////////////////////////
// There is a handshake mechanism between the multiplex protocol handling and
// the framework for accessing the physical device. The multiplex protocol
// handling takes care that the physical device's transmit buffer doesn't
// become too full for receiving complete multiplex frames. So it is ensured
// that the framework can always send all bytes received from the multiplex
// protocol handling via the callback function MUX_DEVSENDDATA_cb() to the
// physical device. If the transmit buffer from the physical device reaches
// that certain fill level the multiplex protocol handling will stop to send
// more bytes to the physical device. Instead it will call the callback
// function MUX_DEVACTIVATE_cb() to indicate after which physical transmit
// buffer level it will be notified to continue sending data to the physical
// device and enter a stall state. That notification has to be done by the
// framework with a call of this function Dev_SendDataContinue().
// The multiplex protocol will not loop and poll the physical device, but wait
// for this function call. Neglecting to call this function will result in a
// fully deadlocked multiplex protocol handling. Calling this function is
// essential for proper functionality of the whole multiplex protocol
// implementation.
//
// Parameters:
// hMuxInstance     The handle returned by the initial call of Mux_Register()
//                  identifying the corresponding multiplex protocol instance.
// dwLen            Not used yet.
//
// Return:
// None
//////////////////////////////////////////////////////////////////////////////
void Dev_SendDataContinue (
    DWORD           hMuxInstance,
    DWORD           dwLen
);


#endif // __MUX_INTERFACE_H__
